#include <QApplication>
#include <QMainWindow>

int main(int argc, char* argv[]) {
	QApplication application(argc, argv);

	QMainWindow window;
	window.setWindowTitle("FundOS");
	window.resize(1280, 720);
	window.show();

	return application.exec();
}
