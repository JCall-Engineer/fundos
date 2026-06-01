#include <QApplication>
#include "mainwindow.hpp"

int main(int argc, char* argv[]) {
	QApplication application(argc, argv);
	application.setOrganizationName("fundos");
	application.setApplicationName("fundos");
	application.setWindowIcon(QIcon(":/res/icon.svg"));

	MainWindow window;
	return application.exec();
}
