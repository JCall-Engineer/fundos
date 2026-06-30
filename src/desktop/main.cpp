#include "theme.hpp"
#include "shell/main_window.hpp"
#include <QApplication>
#include <QStyle>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
	QApplication application(argc, argv);
	application.setOrganizationName("JCall.Engineer");
	application.setApplicationName("FundOS");
	application.setWindowIcon(QIcon(":/icon.svg"));
	QStyle* fusion_style = QStyleFactory::create("Fusion");
	FUNDOS_ASSERT(fusion_style != nullptr, "fusion style not properly generating"); // noop if fails
	application.setStyle(fusion_style);
	application.setPalette(theme::make_palette());

	MainWindow window;
	window.show();

	return application.exec();
}
