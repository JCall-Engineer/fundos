#include "main_window.hpp"
#include "content/home_page.hpp"
#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>

static constexpr int WINDOW_VERSION = 1;
MainWindow::MainWindow() {
	setWindowTitle("FundOS");
	resize(1280, 720);

	QSettings settings;
	restoreGeometry(settings.value("mainwindow/geometry", QByteArray()).toByteArray());
	restoreState(settings.value("mainwindow/state", QByteArray()).toByteArray(), WINDOW_VERSION);

	QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
	db_path = (QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/fundos.sqlite").toStdString();

	status_bar = new StatusBar(this);
	setStatusBar(status_bar);

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
		return go_home();
	}

	auto& status = database->get_status();
	load_error_page(new ErrorPage(status, this));
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
	auto home_page = new HomePage(database, this);
	connect(home_page, &HomePage::db_outcome,     this, &MainWindow::on_result);
	connect(home_page, &HomePage::create_account, this, &MainWindow::create_account);
	connect(home_page, &HomePage::create_fund,    this, &MainWindow::create_fund);
	connect(home_page, &HomePage::open_budget,    this, &MainWindow::open_budget);
	connect(home_page, &HomePage::import_ofx,     this, &MainWindow::import_ofx);
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
	database->migrate();
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
	QMessageBox::information(this, tr("Title"), tr("Manage locale Called"));
}

void MainWindow::import_ofx() {

}

void MainWindow::create_account() {

}
void MainWindow::open_account(fundos::account opening) {

}
void MainWindow::save_account(fundos::account saving) {

}

void MainWindow::create_fund() {

}
void MainWindow::open_fund(fundos::fund opening) {

}
void MainWindow::save_fund(fundos::fund saving) {

}

// create_budget is open_budget with a default constructed budget
void MainWindow::open_budget(fundos::budget opening) {

}
void MainWindow::save_budget(fundos::budget saving) {

}

// Create transaction is open_transaction with minimal properties set
void MainWindow::open_transaction(fundos::transaction opening) {

}
void MainWindow::save_transaction(fundos::transaction saving) {

}
