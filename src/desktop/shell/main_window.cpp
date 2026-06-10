#include "main_window.hpp"
#include "content/home_page.hpp"
#include "content/locale_page.hpp"
#include "content/account_page.hpp"
#include "content/fund_page.hpp"
#include "content/budget_page.hpp"
#include <QApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileDialog>
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

	QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
	db_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/fundos.sqlite";

	status_bar = new StatusBar(this);
	setStatusBar(status_bar);
	connect(status_bar, &StatusBar::backup_requested,        this, &MainWindow::db_backup);
	connect(status_bar, &StatusBar::restore_requested,       this, &MainWindow::db_restore);
	connect(status_bar, &StatusBar::create_new_requested,    this, &MainWindow::db_create_new);
	connect(status_bar, &StatusBar::manage_locale_requested, this, &MainWindow::db_manage_locale);

	open_database();
}

void MainWindow::closeEvent(QCloseEvent* event) {
	QSettings settings;
	settings.setValue("mainwindow/geometry", saveGeometry());
	settings.setValue("mainwindow/state", saveState(WINDOW_VERSION));
	QMainWindow::closeEvent(event);
}

void MainWindow::open_database() {
	if (database != nullptr) {
		// Close previous db so it doesn't lock an attempt to create a new db instance
		database->close();
		context = nullptr;
	}
	std::string path = db_path.toStdString();
	database = fundos::db::open_file(path.c_str());
	status_bar->set_database(database);

	if (database->is_ready()) {
		return create_context();
	}

	auto& status = database->get_status();
	load_error_page(new ErrorPage(status, this));
}

void MainWindow::create_context() {
	FUNDOS_ASSERT(database->is_ready(), "create_context called when database is not ready");
	context = nullptr;

	AppContext::try_create(database, this, {
		.on_success = [this](std::shared_ptr<AppContext> ctx) {
			on_context_refreshed(ctx);
			go_home();
		},
		.on_needs_locale = [this](std::shared_ptr<AppContext> ctx) {
			open_locale_page(ctx->currency, ctx->percentage);
		},
		.on_fatal = [this](const fundos::db::outcome& failure) {
			load_error_page(new ErrorPage(failure, this));
		},
	});
}

void MainWindow::on_context_refreshed(std::shared_ptr<AppContext> new_context) {
	if (context != nullptr) {
		disconnect(context.get(), &AppContext::refreshed, this, &MainWindow::on_context_refreshed);
	}
	context = new_context;
	connect(context.get(), &AppContext::refreshed, this, &MainWindow::on_context_refreshed);
}

void MainWindow::load_error_page(ErrorPage* page) {
	connect(page, &ErrorPage::retry_requested,      this, &MainWindow::db_retry);
	connect(page, &ErrorPage::migrate_requested,    this, &MainWindow::db_migrate);
	connect(page, &ErrorPage::backup_requested,     this, &MainWindow::db_backup);
	connect(page, &ErrorPage::create_new_requested, this, &MainWindow::db_create_new);
	connect(page, &ErrorPage::restore_requested,    this, &MainWindow::db_restore);
	connect(page, &ErrorPage::quit_requested,       this, &MainWindow::quit);
	setCentralWidget(page);
}

void MainWindow::open_locale_page(std::optional<fundos::currency_locale::selection> currency_locale, std::optional<fundos::percentage_locale::selection> percentage_locale) {
	FUNDOS_ASSERT(database->is_ready(), "open_locale_page called when database is not ready");

	auto* locale_page = new LocalePage(database, currency_locale, percentage_locale, this);
	connect(locale_page, &LocalePage::db_outcome, this, &MainWindow::on_result);
	connect(locale_page, &LocalePage::done,       this, [this]() { create_context(); });
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
	FUNDOS_ASSERT(database->is_ready(), "go_home called when database is not ready");
	FUNDOS_ASSERT(context != nullptr, "go_home called before context is created");

	auto* home_page = new HomePage(context, this);
	connect(home_page, &HomePage::db_outcome,   this, &MainWindow::on_result);
	connect(home_page, &HomePage::open_account, this, &MainWindow::open_account);
	connect(home_page, &HomePage::open_fund,    this, &MainWindow::open_fund);
	connect(home_page, &HomePage::open_budget,  this, &MainWindow::open_budget);
	connect(home_page, &HomePage::import_ofx,   this, &MainWindow::import_ofx);
	connect(home_page, &HomePage::refresh,      this, &MainWindow::go_home);
	home_page->initialize();
	setCentralWidget(home_page);
}
void MainWindow::quit() {
	QApplication::quit();
}

void MainWindow::db_retry() {
	open_database();
}
void MainWindow::db_migrate() {
	if (database == nullptr || !database->is_connected()) {
		FUNDOS_ASSERT(false, "Migrate was called on a non-existent or closed db");
		return;
	}
	auto migrated = database->migrate();
	on_result(migrated);
	if (migrated) {
		create_context();
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
	QFile::remove(destination); // Delete the existing file

	if (database && database->is_connected()) {
		// Shows a message box on failure, loads an error page if the database is closed as a result
		on_result(database->backup(destination.toStdString()));
	} else {
		if (!QFile::copy(db_path, destination)) {
			QMessageBox::critical(this, tr("Backup Error"), tr("Could not export database file."));
		}
	}
}
void MainWindow::db_create_new() {
	if (QMessageBox::Ok != confirm_destruction(this)) { return; }
	if (database && database->is_connected()) {
		database->close();
	}
	if (!QFile::remove(db_path)) {
		QMessageBox::critical(this, tr("Error"), tr("Could not delete the database file. Ensure it is not open in another program and that you have write permissions to the file."));
		return;
	}
	open_database();
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

	bool was_open = false;
	if (database && database->is_connected()) {
		database->close();
		was_open = true;
	}
	QString temp_path = db_path + ".tmp";
	if (!QFile::rename(db_path, temp_path)) {
		QMessageBox::critical(this, tr("Restore Error"), tr("Could not move existing database file."));
		return;
	}
	if (!QFile::copy(source, db_path)) {
		if (QFile::rename(temp_path, db_path)) { // best effort recovery
			if (was_open) { open_database(); }
		} else {
			QMessageBox::critical(this, tr("Restore Error"), tr("Could not recover original database file. Your data may be at risk."));
		}
		QMessageBox::critical(this, tr("Restore Error"), tr("Could not import database file."));
		return;
	}
	QFile::remove(temp_path);
	open_database();
}

void MainWindow::db_manage_locale() {
	if (context == nullptr) { return; }
	open_locale_page(context->currency, context->percentage);
}

void MainWindow::import_ofx() {

}

void MainWindow::open_account(const fundos::account& opening) {
	auto account_page = new AccountPage(context, opening, this);
	connect(account_page, &AccountPage::db_outcome, this, &MainWindow::on_result);
	connect(account_page, &AccountPage::go_home,    this, &MainWindow::go_home);
	connect(account_page, &AccountPage::import_ofx, this, &MainWindow::import_ofx);
	setCentralWidget(account_page);
}
void MainWindow::open_fund(const fundos::fund& opening) {

}
void MainWindow::open_budget(const fundos::budget& opening) {

}
