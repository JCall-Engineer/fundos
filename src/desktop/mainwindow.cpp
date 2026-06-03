#include <QSettings>
#include <QStandardPaths>
#include "mainwindow.hpp"

void MainWindow::open_database() {
	database = nullptr; // Close previous db so it doesn't lock an attempt to create a new db instance
	database = fundos::db::open_file(db_path);
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
		switch(status.result) {
			case fundos::db::status::code::null_db: // impossible with how we use db, requires passing null sqlite3* to a constructor
				FUNDOS_UNREACHABLE();
				return;
			case fundos::db::status::code::schema_error:
			case fundos::db::status::code::sqlite3_error:
				break;
		}
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

	db_path = (QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/fundos.sqlite").toStdString();

	pages = new QStackedWidget(this);
	setCentralWidget(pages);

	open_database();
}

void MainWindow::closeEvent(QCloseEvent* event) {
	QSettings settings;
	settings.setValue("mainwindow/geometry", saveGeometry());
	settings.setValue("mainwindow/state", saveState(WINDOW_VERSION));
	QMainWindow::closeEvent(event);
}
