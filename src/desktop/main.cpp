#include <QApplication>
#include "main_window.hpp"

int main(int argc, char* argv[]) {
	QApplication application(argc, argv);
	application.setOrganizationName("fundos");
	application.setApplicationName("fundos");
	application.setWindowIcon(QIcon(":/icon.svg"));

	MainWindow window;
	window.show();

	return application.exec();
}
