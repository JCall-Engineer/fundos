#pragma once
#include "fundos.hpp"
#include <QMainWindow>

class StatusBar;
class ErrorPage;
class MainWindow : public QMainWindow {
	Q_OBJECT

	std::string db_path;
	std::shared_ptr<fundos::db> database = nullptr;
	StatusBar* status_bar = nullptr;

	void open_database();
	void load_error_page(ErrorPage* page);

public:
	MainWindow();

protected:
	void closeEvent(QCloseEvent* event) override;

private slots:
	void on_result(const fundos::db::outcome& result);
	void go_home();
	void quit();

	void db_retry();
	void db_migrate();
	void db_backup();
	void db_create_new();
	void db_restore();
	void db_manage_locale();

	void import_ofx();

	void create_account();
	void open_account(fundos::account opening);
	void save_account(fundos::account saving);

	void create_fund();
	void open_fund(fundos::fund opening);
	void save_fund(fundos::fund saving);

	void open_budget(fundos::budget opening);
	void save_budget(fundos::budget saving);

	void open_transaction(fundos::transaction opening);
	void save_transaction(fundos::transaction saving);
};
