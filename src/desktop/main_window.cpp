#include "main_window.hpp"
#include "pages/database_error.hpp"
#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>

void MainWindow::open_database() {
	if (database != nullptr) {
		// Close previous db so it doesn't lock an attempt to create a new db instance
		database->close();
	}
	database = fundos::db::open_file(db_path);
	status_bar->set_database(database);

	if (database->is_ready()) {
		// open main page
		return;
	}

	auto& status = database->get_status();
	DatabaseErrorPage* error_page = new DatabaseErrorPage(status, this);
	connect(error_page, &DatabaseErrorPage::retry_requested,      this, &MainWindow::handle_retry);
	connect(error_page, &DatabaseErrorPage::migrate_requested,    this, &MainWindow::handle_migrate);
	connect(error_page, &DatabaseErrorPage::backup_requested,     this, &MainWindow::handle_backup);
	connect(error_page, &DatabaseErrorPage::create_new_requested, this, &MainWindow::handle_create_new);
	connect(error_page, &DatabaseErrorPage::restore_requested,    this, &MainWindow::handle_restore);
	connect(error_page, &DatabaseErrorPage::quit_requested,       this, &MainWindow::handle_quit);
	setCentralWidget(error_page);
}

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

void MainWindow::handle_retry() {
	QMessageBox::information(this, tr("Title"), tr("Retry Called"));
	open_database();
}
void MainWindow::handle_quit() {
	QApplication::quit();
}
void MainWindow::handle_migrate() {
	QMessageBox::information(this, tr("Title"), tr("Migrate Called"));
	//database->migrate();
}
void MainWindow::handle_backup() {
	QMessageBox::information(this, tr("Title"), tr("Backup Called"));
}
void MainWindow::handle_create_new() {
	QMessageBox::information(this, tr("Title"), tr("Create new called"));
}
void MainWindow::handle_restore() {
	QMessageBox::information(this, tr("Title"), tr("Restore Called"));
}
void MainWindow::handle_manage_locale() {
	QMessageBox::information(this, tr("Title"), tr("Manage locale Called"));
}
