#include "data/import.hpp"
#include "main_window.hpp"
#include "content/locale_page.hpp"
#include "content/home_page.hpp"
#include "content/account_page.hpp"
#include "content/fund_page.hpp"
#include "content/budget_page.hpp"
#include "content/import_accounts_page.hpp"
#include "content/import_transactions_page.hpp"
#include <QApplication>
#include <QtConcurrent>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFuture>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>

static constexpr int WINDOW_VERSION = 1;
MainWindow::MainWindow() {
	setWindowTitle("FundOS");
	resize(1280, 720);
	setMinimumWidth(700);

	QSettings settings;
	restoreGeometry(settings.value("mainwindow/geometry", QByteArray()).toByteArray());
	restoreState(settings.value("mainwindow/state", QByteArray()).toByteArray(), WINDOW_VERSION);

	database = new AppDatabase(this);
	connect(this,     &MainWindow::db_open_requested,       database, &AppDatabase::open);
	connect(this,     &MainWindow::db_backup_requested,     database, &AppDatabase::backup);
	connect(this,     &MainWindow::db_restore_requested,    database, &AppDatabase::restore);
	connect(this,     &MainWindow::db_create_new_requested, database, &AppDatabase::create_new);
	connect(database, &AppDatabase::db_outcome,             this,     &MainWindow::on_result);
	connect(database, &AppDatabase::connection_opened,      this,     &MainWindow::on_db_open);
	connect(database, &AppDatabase::connection_migrated,    this,     &MainWindow::on_migrate);
	connect(database, &AppDatabase::backup_complete,        this,     &MainWindow::on_backup_result);
	connect(database, &AppDatabase::backup_copy_failed,     this,     &MainWindow::on_backup_copy_failed);
	connect(database, &AppDatabase::restore_complete,       this,     &MainWindow::on_restore);
	connect(database, &AppDatabase::create_new_complete,    this,     &MainWindow::on_create_new);

	coordinator = new AppCoordinator(database, this);
	connect(this,        &MainWindow::context_requested, coordinator, &AppCoordinator::on_database_open);
	connect(coordinator, &AppCoordinator::creation_failure, this,        &MainWindow::on_context_failure);
	connect(coordinator, &AppCoordinator::needs_locale,     this,        &MainWindow::db_manage_locale);
	connect(coordinator, &AppCoordinator::created,          this,        &MainWindow::go_home);

	status_bar = new StatusBar(this);
	setStatusBar(status_bar);
	connect(status_bar, &StatusBar::db_info_requested,       database,   &AppDatabase::request_db_info);
	connect(database,   &AppDatabase::db_info_received,      status_bar, &StatusBar::on_db_info);
	connect(status_bar, &StatusBar::backup_requested,        this,       &MainWindow::db_backup);
	connect(status_bar, &StatusBar::restore_requested,       this,       &MainWindow::db_restore);
	connect(status_bar, &StatusBar::create_new_requested,    this,       &MainWindow::db_create_new);
	connect(status_bar, &StatusBar::manage_locale_requested, this,       &MainWindow::db_manage_locale);

	emit db_open_requested();
}

void MainWindow::closeEvent(QCloseEvent* event) {
	QSettings settings;
	settings.setValue("mainwindow/geometry", saveGeometry());
	settings.setValue("mainwindow/state", saveState(WINDOW_VERSION));
	QMainWindow::closeEvent(event);
}

void MainWindow::on_db_open(fundos::db::status open_result) {
	status_bar->on_db_open(open_result);
	if (open_result.is_ok()) {
		emit context_requested();
	} else {
		load_error_page(new ErrorPage(open_result, this));
	}
}

void MainWindow::on_context_failure(fundos::db::outcome status) {
	load_error_page(new ErrorPage(status, this));
}

void MainWindow::load_error_page(ErrorPage* page) {
	connect(page, &ErrorPage::retry_requested,      database, &AppDatabase::open);
	connect(page, &ErrorPage::migrate_requested,    database, &AppDatabase::migrate);
	connect(page, &ErrorPage::backup_requested,     this,     &MainWindow::db_backup);
	connect(page, &ErrorPage::create_new_requested, this,     &MainWindow::db_create_new);
	connect(page, &ErrorPage::restore_requested,    this,     &MainWindow::db_restore);
	connect(page, &ErrorPage::quit_requested,       this,     &MainWindow::on_quit);
	setCentralWidget(page);
}

void MainWindow::open_locale_page(
	std::optional<fundos::currency_locale::selection> currency_locale,
	std::optional<fundos::percentage_locale::selection> percentage_locale
) {
	auto* locale_page = new LocalePage(currency_locale, percentage_locale, this);
	connect(locale_page, &LocalePage::save_requested, database,    &AppDatabase::set_locales);
	connect(database,    &AppDatabase::locales_saved, locale_page, &LocalePage::on_save_result);
	connect(locale_page, &LocalePage::cancelled,      this,        &MainWindow::go_home);
	connect(locale_page, &LocalePage::saved,          this, [this](fundos::currency_locale::selection currency, fundos::percentage_locale::selection percentage) {
		coordinator->update_locales(currency, percentage);
	});
	setCentralWidget(locale_page);
}

void MainWindow::on_result(const fundos::db::outcome& result) {
	using error = fundos::db::error;
	status_bar->set_status(result);
	if (!result) {
		QString msg;
		if (result.msg.has_value()) {
			const auto& view = result.msg->view();
			msg = "\n\n" + tr("Error Message:") + " \"" + QString::fromUtf8(view.data(), view.size()) + "\"";
		}
		switch (result.code) {
			case error::none:         // Conflicts with the if (!result) guard
			case error::not_ready:    // not_ready should cause an error page on db open instead of home, making this function unreachable
			case error::inaccessible: // would prevent the db from opening, making this function unreachable
			case error::readonly:     // would prevent the db from opening, making this function unreachable
				FUNDOS_UNREACHABLE();
				return;

			case error::interrupted:
				QMessageBox::information(this, tr("Interrupted"), tr("A database operation was interrupted.") + msg);
				return;

			case error::corrupted:
				QMessageBox::critical(this, tr("Database Corrupted"), tr("This operation has corrupted the database.") + msg);
				return load_error_page(new ErrorPage(result, this));

			case error::internal:
				QMessageBox::critical(this, tr("Internal Error"), tr("This operation resulted in an unexpected error.") + " " + tr("Please report this issue.") + msg);
				return load_error_page(new ErrorPage(result, this));

			case error::unavailable:
				QMessageBox::information(this, tr("Database Unavailable"), tr("Something has the database busy. Try again later.") + msg);
				return;

			case error::out_of_memory:
				QMessageBox::critical(this, tr("System out of Memory"), tr("You may need to close some programs or restart.") + msg);
				return;

			case error::disk_full:
				QMessageBox::critical(this, tr("System out of Storage"), tr("You may need to clear some space on your disk.") + msg);
				return;

			case error::constraint:
				QMessageBox::critical(this, tr("Unexpected Error"), tr("An operation violated a database constraint.") + " " + tr("Please report this issue.") + msg);
				return;

			case error::not_found:
				QMessageBox::warning(this, tr("Object not Found"), tr("This operation attempted to access an unknown record from the database.") + msg);
				return;

			case error::bad_request:
				QMessageBox::critical(this, tr("Unexpected Error"), tr("An operation was attempted in an unexpected state.") + " " + tr("Please report this issue.") + msg);
				return;

			case error::rejected:
				QMessageBox::warning(this, tr("Operation Not Allowed"), tr("This operation could not be completed because the data did not meet the required conditions.") + msg);
				return;
		}
	}
}
void MainWindow::go_home() {
	auto* home_page = new HomePage(coordinator, this);
	connect(home_page, &HomePage::open_account, this, &MainWindow::open_account);
	connect(home_page, &HomePage::open_fund,    this, &MainWindow::open_fund);
	connect(home_page, &HomePage::open_budget,  this, &MainWindow::open_budget);
	connect(home_page, &HomePage::import_ofx,   this, &MainWindow::import_ofx);
	setCentralWidget(home_page);
}
void MainWindow::on_quit() {
	QApplication::quit();
}

void MainWindow::on_migrate(fundos::db::outcome status) {
	status_bar->set_status(status);
	if (!status) {
		QMessageBox::information(this, tr("Error"), tr("Migration failed."));
	} else {
		emit context_requested();
	}
}

static QMessageBox::StandardButton confirm_destruction(QWidget* parent) {
	return QMessageBox::question(
		parent,
		QObject::tr("Destructive Operation"),
		QObject::tr("This operation deletes the old database. You should perform a backup first. Continue?"),
		QMessageBox::Ok | QMessageBox::Cancel,
		QMessageBox::Cancel
	);
}

void MainWindow::db_backup() {
	QString defaultName = QString("FundOS Backup %1.sqlite")
		.arg(QDate::currentDate().toString("yyyy-MM-dd"));

	QString destination = QFileDialog::getSaveFileName(
		this,
		tr("Backup Database"),
		QDir::homePath() + "/" + defaultName,
		tr("SQLite Database (*.sqlite)")
	);
	if (destination.isEmpty()) { return; }
	emit db_backup_requested(destination);
}

void MainWindow::on_backup_result(fundos::db::outcome status) {
	if (status) {
		QMessageBox::information(this, tr("Success"), "Backup Created");
	} else {
		QMessageBox::information(this, tr("Error"), "Backup Failed");
	}
}
void MainWindow::on_backup_copy_failed() {
	QMessageBox::critical(this, tr("Error"), "Backup could not be created");
}

void MainWindow::db_create_new() {
	if (QMessageBox::Ok != confirm_destruction(this)) { return; }
	emit db_create_new_requested();
}
void MainWindow::on_create_new(bool succeeded) {
	if (succeeded) {
		QMessageBox::information(this, tr("Success"), tr("Created new database."));
	} else {
		QMessageBox::critical(this, tr("Error"), tr("Could not delete the database file. Ensure it is not open in another program and that you have write permissions to the file."));
	}
}
void MainWindow::db_restore() {
	if (QMessageBox::Ok != confirm_destruction(this)) { return; }

	QString source = QFileDialog::getOpenFileName(
		this,
		tr("Restore Database"),
		QDir::homePath(),
		tr("SQLite Database (*.sqlite *.db)")
	);
	if (source.isEmpty()) { return; }
	emit db_restore_requested(source);
}
void MainWindow::on_restore(AppDatabase::RestoreResult result) {
	switch (result) {
		case AppDatabase::RestoreResult::success:
			QMessageBox::information(this, tr("Restore Success"), tr("Database Restoration Successful"));
			return;
		case AppDatabase::RestoreResult::failed_to_move:
			QMessageBox::critical(this, tr("Restore Error"), tr("Could not move existing database file."));
			return;
		case AppDatabase::RestoreResult::failed_to_copy:
			QMessageBox::critical(this, tr("Restore Error"), tr("Could not import database file."));
			return;
		case AppDatabase::RestoreResult::data_at_risk:
			QMessageBox::critical(this, tr("Restore Error"), tr("Could not recover original database file. Your data may be at risk."));
			return;
	}
}

void MainWindow::db_manage_locale() {
	auto context = coordinator->context();
	open_locale_page(context->optional_currency_locale(), context->optional_percentage_locale());
}

static inline QString warning_message(fundos::import::warning type, int32_t count) {
	using warning = fundos::import::warning;
	switch (type) {
		case warning::missing_acctid:
			return QObject::tr("%n transaction(s) were missing an account ID and could not be matched to an account.", "", count);
		case warning::skipped_transaction:
			return QObject::tr("%n transaction(s) could not be parsed and were skipped.", "", count);
		case warning::missing_fitid:
			return QObject::tr("%n transaction(s) were missing a unique identifier and may be imported as duplicates.", "", count);
		case warning::missing_date:
			return QObject::tr("%n transaction(s) were missing a date.", "", count);
		case warning::missing_amount:
			return QObject::tr("%n transaction(s) were missing an amount.", "", count);
		case warning::bad_date:
			return QObject::tr("%n transaction(s) had a date that could not be parsed.", "", count);
		case warning::bad_amount:
			return QObject::tr("%n transaction(s) had an amount that could not be parsed.", "", count);
		case warning::bad_correction:
			return QObject::tr("%n transaction(s) referenced a correction that could not be resolved.", "", count);
		case warning::NUM_WARNINGS:
			return QString();
	}
	FUNDOS_UNREACHABLE();
}

void MainWindow::import_ofx() {
	QString source = QFileDialog::getOpenFileName(
		this,
		tr("Import from Bank"),
		QDir::homePath(),
		tr("OFX File (*.ofx)")
	);
	if (source.isEmpty()) { return; }
	std::string filepath = source.toStdString();
	fundos::currency_locale::spec locale = coordinator->context()->currency_locale().info();

	QFutureWatcher<fundos::import::result>* watcher = new QFutureWatcher<fundos::import::result>(this);
	connect(watcher, &QFutureWatcher<fundos::import::result>::finished, this, [this, watcher]() {
		fundos::import::result imported = watcher->result();
		watcher->deleteLater();
		using error = fundos::import::error;
		if (!imported.ok()) {
			switch (imported.err) {
				case error::bad_format:
					QMessageBox::critical(this, tr("Import Error"), tr("The selected file is not a recognized OFX format."));
					return;
				case error::io_error:
					QMessageBox::critical(this, tr("Import Error"), tr("There was an error reading the selected file."));
					return;
				case error::malformed:
					QMessageBox::critical(this, tr("Import Error"), tr("The selected file is damaged or incomplete and could not be read."));
					return;
				case error::none:
					FUNDOS_UNREACHABLE();
					return;
			}
		}
		bool has_warnings = false;
		QString warnings = tr("Importing generated the following warnings:");
		using warning = fundos::import::warning;
		for (int32_t index = 0; index < (int32_t)warning::NUM_WARNINGS; ++index) {
			int32_t count = imported.warning_counts[index];
			if (count > 0) {
				warnings += "\n" + warning_message((warning)index, count);
				has_warnings = true;
			}
		}
		warnings += "\n" + tr("Continue?");
		if (has_warnings) {
			if (QMessageBox::Cancel == QMessageBox::question(
				this,
				tr("Import Warnings"),
				warnings,
				QMessageBox::Ok | QMessageBox::Cancel,
				QMessageBox::Cancel)
			) { return; }
		}
		auto import_data = std::make_shared<fundos::import::pending_import>(std::move(imported.data));
		auto iterator = std::make_shared<ImportAccountsPage::Iterator>(import_data);
		if (iterator->advance_to_unmatched(coordinator->context()->accounts()) != nullptr) {
			auto* page = new ImportAccountsPage(coordinator, iterator, this);
			connect(page, &ImportAccountsPage::ready_for_merge, this, &MainWindow::import_handle_merge);
			setCentralWidget(page);
		} else {
			import_handle_merge(import_data);
		}
	});
	QFuture<fundos::import::result> future = QtConcurrent::run([filepath, locale]() {
		return fundos::import::import_ofx(filepath, locale);
	});
	watcher->setFuture(future);
}

void MainWindow::import_handle_merge(std::shared_ptr<fundos::import::pending_import> import_data) {
	auto* page = new ImportTransactionsPage(coordinator, import_data, this);
	setCentralWidget(page);
}

void MainWindow::open_account(const fundos::account& opening) {
	open_account_with_transaction(opening, std::nullopt);
}
void MainWindow::open_account_with_transaction(const fundos::account& opening, std::optional<fundos::transaction> requested) {
	auto account_page = new AccountPage(coordinator, opening, requested, this);
	connect(account_page, &AccountPage::go_home,    this, &MainWindow::go_home);
	connect(account_page, &AccountPage::import_ofx, this, &MainWindow::import_ofx);
	setCentralWidget(account_page);
}
void MainWindow::open_fund(const fundos::fund& opening) {
	auto fund_page = new FundPage(coordinator, opening, this);
	connect(fund_page, &FundPage::go_home,           this, &MainWindow::go_home);
	connect(fund_page, &FundPage::account_requested, this, &MainWindow::open_account_with_transaction);
	setCentralWidget(fund_page);
}
void MainWindow::open_budget(const fundos::budget& opening) {

}
