#pragma once
#include "fundos.hpp"

#include "status_bar.hpp"
#include <QMainWindow>

class MainWindow : public QMainWindow {
	Q_OBJECT

	std::string db_path;
	std::shared_ptr<fundos::db> database = nullptr;
	StatusBar* status_bar = nullptr;

	void open_database();

public:
	MainWindow();

protected:
	void closeEvent(QCloseEvent* event) override;

private slots:
	void handle_retry();
	void handle_migrate();
	void handle_backup();
	void handle_create_new();
	void handle_restore();
	void handle_quit();
	void handle_manage_locale();
};
