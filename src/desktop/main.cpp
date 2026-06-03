#include "main_window.hpp"
#include "theme.hpp"
#include <QApplication>

int main(int argc, char* argv[]) {
	QApplication application(argc, argv);
	application.setOrganizationName("fundos");
	application.setApplicationName("fundos");
	application.setWindowIcon(QIcon(":/icon.svg"));
	application.setPalette(theme::make_palette());

	MainWindow window;
	window.show();

	return application.exec();
}
