#pragma once
#include "context.hpp"
#include "shell/status_bar.hpp"
#include "content/error_page.hpp"
#include <QMainWindow>

class MainWindow : public QMainWindow {
	Q_OBJECT

	std::string db_path;
	std::shared_ptr<fundos::db> database;
	std::shared_ptr<AppContext> context;
	StatusBar* status_bar = nullptr;

	void open_database();
	void create_context();
	void load_error_page(ErrorPage* page);
	void open_locale_page();
	void open_locale_page(std::optional<fundos::currency_locale::selection> currency_locale, std::optional<fundos::percentage_locale::selection> percentage_locale);
	bool try_get_locales(std::optional<fundos::currency_locale::selection>& currency_locale, std::optional<fundos::percentage_locale::selection>& percentage_locale);

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
	void open_account(std::shared_ptr<fundos::account> opening);
	void save_account(std::shared_ptr<fundos::account> saving);

	void create_fund();
	void open_fund(std::shared_ptr<fundos::fund> opening);
	void save_fund(std::shared_ptr<fundos::fund> saving);

	void open_budget(std::shared_ptr<fundos::budget> opening);
	void save_budget(std::shared_ptr<fundos::budget> saving);

	void open_transaction(std::shared_ptr<fundos::transaction> opening);
	void save_transaction(std::shared_ptr<fundos::transaction> saving);
};
