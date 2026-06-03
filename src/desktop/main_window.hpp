#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include "fundos.hpp"

class MainWindow : public QMainWindow {
	Q_OBJECT

	std::string db_path;
	std::shared_ptr<fundos::db> database = nullptr;
	QStackedWidget* pages = nullptr;

	void open_database();

public:
	MainWindow();

protected:
	void closeEvent(QCloseEvent* event) override;
};
