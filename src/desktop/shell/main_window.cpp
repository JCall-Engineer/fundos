#include "main_window.hpp"
#include "content/home_page.hpp"
#include "content/locale_page.hpp"
#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>

static constexpr int WINDOW_VERSION = 1;
MainWindow::MainWindow() {
	setWindowTitle("FundOS");
	resize(1280, 720);
	setMinimumWidth(600);

	QSettings settings;
	restoreGeometry(settings.value("mainwindow/geometry", QByteArray()).toByteArray());
	restoreState(settings.value("mainwindow/state", QByteArray()).toByteArray(), WINDOW_VERSION);

	QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
	db_path = (QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/fundos.sqlite").toStdString();

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
	}
	database = fundos::db::open_file(db_path);
	status_bar->set_database(database);

	if (database->is_ready()) {
		return create_context();
	}

	auto& status = database->get_status();
	load_error_page(new ErrorPage(status, this));
}

bool MainWindow::try_get_locales(std::optional<fundos::currency_locale::selection>& currency_locale, std::optional<fundos::percentage_locale::selection>& percentage_locale) {
	FUNDOS_ASSERT(database->is_ready(), "get_locales called when database is not ready");

	currency_locale = std::nullopt;
	percentage_locale = std::nullopt;

	auto currency_locale_fetch = database->get_currency_locale();
	while (!currency_locale_fetch) {
		if (currency_locale_fetch.status().code == fundos::db::error::not_found) {
			break;
		}
		if (!database->is_ready()) {
			load_error_page(new ErrorPage(currency_locale_fetch.status(), this));
			return false;
		}
		QString detail = tr("Error Code:") + " " + QString::number(static_cast<int>(currency_locale_fetch.status().code));
		if (currency_locale_fetch.status().msg.has_value()) {
			const auto& view = currency_locale_fetch.status().msg->view();
			detail += "\n\n" + tr("Error Message:") + " " + QString::fromUtf8(view.data(), view.size());
		}
		int choice = QMessageBox::critical(
			this,
			tr("Settings Error"),
			tr("FundOS could not read its settings from the database and cannot continue.") + "\n\n" + detail,
			QMessageBox::StandardButton::Retry,
			QMessageBox::StandardButton::Abort
		);
		if (choice == QMessageBox::Abort) {
			QApplication::quit();
			return false;
		}
		currency_locale_fetch = database->get_currency_locale();
	}
	if (currency_locale_fetch) { currency_locale = currency_locale_fetch.value(); }

	auto percentage_locale_fetch = database->get_percentage_locale();
	while (!percentage_locale_fetch) {
		if (percentage_locale_fetch.status().code == fundos::db::error::not_found) {
			break;
		}
		if (!database->is_ready()) {
			load_error_page(new ErrorPage(percentage_locale_fetch.status(), this));
			return false;
		}
		QString detail = tr("Error Code:") + " " + QString::number(static_cast<int>(percentage_locale_fetch.status().code));
		if (percentage_locale_fetch.status().msg.has_value()) {
			const auto& view = percentage_locale_fetch.status().msg->view();
			detail += "\n\n" + tr("Error Message:") + " " + QString::fromUtf8(view.data(), view.size());
		}
		int choice = QMessageBox::critical(
			this,
			tr("Settings Error"),
			tr("FundOS could not read its settings from the database and cannot continue.") + "\n\n" + detail,
			QMessageBox::StandardButton::Retry,
			QMessageBox::StandardButton::Abort
		);
		if (choice == QMessageBox::Abort) {
			QApplication::quit();
			return false;
		}
		percentage_locale_fetch = database->get_percentage_locale();
	}
	if (percentage_locale_fetch) { percentage_locale = percentage_locale_fetch.value(); }
	return true;
}

void MainWindow::create_context() {
	FUNDOS_ASSERT(database->is_ready(), "create_context called when database is not ready");
	context = nullptr;

	std::optional<fundos::currency_locale::selection> currency_locale;
	std::optional<fundos::percentage_locale::selection> percentage_locale;
	if (!try_get_locales(currency_locale, percentage_locale)) {
		return;
	}

	if (!currency_locale || !percentage_locale) {
		return open_locale_page(currency_locale, percentage_locale);
	}
	context = std::make_shared<AppContext>(database, *currency_locale, *percentage_locale);
	go_home();
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

void MainWindow::open_locale_page() {
	FUNDOS_ASSERT(database->is_ready(), "open_locale_page called when database is not ready");

	std::optional<fundos::currency_locale::selection> currency_locale;
	std::optional<fundos::percentage_locale::selection> percentage_locale;
	if (!try_get_locales(currency_locale, percentage_locale)) {
		return;
	}

	open_locale_page(currency_locale, percentage_locale);
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
	connect(home_page, &HomePage::go_home,      this, &MainWindow::go_home);
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
void MainWindow::db_backup() {
	QMessageBox::information(this, tr("Title"), tr("Backup Called"));
}
void MainWindow::db_create_new() {
	QMessageBox::information(this, tr("Title"), tr("Create new called"));
}
void MainWindow::db_restore() {
	QMessageBox::information(this, tr("Title"), tr("Restore Called"));
}
void MainWindow::db_manage_locale() {
	open_locale_page();
}

void MainWindow::import_ofx() {

}

void MainWindow::create_account() {

}
void MainWindow::open_account(std::shared_ptr<fundos::account> opening) {

}
void MainWindow::save_account(std::shared_ptr<fundos::account> saving) {

}

void MainWindow::create_fund() {

}
void MainWindow::open_fund(std::shared_ptr<fundos::fund> opening) {

}
void MainWindow::save_fund(std::shared_ptr<fundos::fund> saving) {

}

// create_budget is open_budget with a default constructed budget
void MainWindow::open_budget(std::shared_ptr<fundos::budget> opening) {

}
void MainWindow::save_budget(std::shared_ptr<fundos::budget> saving) {

}

// Create transaction is open_transaction with minimal properties set
void MainWindow::open_transaction(std::shared_ptr<fundos::transaction> opening) {

}
void MainWindow::save_transaction(std::shared_ptr<fundos::transaction> saving) {

}
