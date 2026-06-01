#include <QSettings>
#include "mainwindow.hpp"

MainWindow::MainWindow() {
	setWindowTitle("FundOS");
	resize(1280, 720);

	QSettings settings;
	restoreGeometry(settings.value("mainwindow/geometry", QByteArray()).toByteArray());
	restoreState(settings.value("mainwindow/state", QByteArray()).toByteArray());

	show();
}

void MainWindow::closeEvent(QCloseEvent* event) {
	QSettings settings;
	settings.setValue("mainwindow/geometry", saveGeometry());
	settings.setValue("mainwindow/state", saveState());
	QMainWindow::closeEvent(event);
}
