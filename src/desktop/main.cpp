#include <QApplication>
#include <QMainWindow>

int main(int argc, char* argv[]) {
	QApplication application(argc, argv);
	application.setWindowIcon(QIcon(":/res/icon.svg"));

	QMainWindow window;
	window.setWindowTitle("FundOS");
	window.resize(1280, 720);
	window.show();

	return application.exec();
}
