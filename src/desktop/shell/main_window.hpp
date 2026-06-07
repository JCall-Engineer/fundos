#pragma once
#include "context.hpp"
#include "shell/status_bar.hpp"
#include "content/error_page.hpp"
#include <QMainWindow>
#include <QString>

class MainWindow : public QMainWindow {
	Q_OBJECT

	QString db_path;
	std::shared_ptr<fundos::db> database;
	std::shared_ptr<AppContext> context;
	StatusBar* status_bar = nullptr;

	void open_database();
	void create_context();
	void load_error_page(ErrorPage* page);
	void open_locale_page(std::optional<fundos::currency_locale::selection> currency_locale, std::optional<fundos::percentage_locale::selection> percentage_locale);

public:
	MainWindow();

protected:
	void closeEvent(QCloseEvent* event) override;

private slots:
	void on_context_refreshed(std::shared_ptr<AppContext> new_context);

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

	void open_account(const fundos::account& opening);
	void open_fund(const fundos::fund& opening);
	void open_budget(const fundos::budget& opening);
};
