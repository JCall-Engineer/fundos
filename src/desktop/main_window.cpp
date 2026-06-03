#include "main_window.hpp"
#include <QDir>
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
	if (status.needs_migration()) {
		// open migration page
		return;
	}

	if (status.has_error()) {
		
		// show error page
	}
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

	pages = new QStackedWidget(this);
	setCentralWidget(pages);

	status_bar = new StatusBar(this);
	setStatusBar(status_bar);
	//connect(status_bar, &StatusBar::db_menu_requested, this, &MainWindow::show_db_menu);

	open_database();
}

void MainWindow::closeEvent(QCloseEvent* event) {
	QSettings settings;
	settings.setValue("mainwindow/geometry", saveGeometry());
	settings.setValue("mainwindow/state", saveState(WINDOW_VERSION));
	QMainWindow::closeEvent(event);
}
