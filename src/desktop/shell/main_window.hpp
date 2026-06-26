#pragma once
#include "coordinator.hpp"
#include "shell/status_bar.hpp"
#include "content/error_page.hpp"
#include <QMainWindow>
#include <QMessageBox>
#include <QString>

class MainWindow : public QMainWindow {
	Q_OBJECT

	AppDatabase* database;
	AppCoordinator* coordinator;
	StatusBar* status_bar = nullptr;

	void create_context();
	void load_error_page(ErrorPage* page);
	void open_locale_page(std::optional<fundos::currency_locale::selection> currency_locale, std::optional<fundos::percentage_locale::selection> percentage_locale);

	QMessageBox::StandardButton confirm_destruction();
public:
	MainWindow();

protected:
	void closeEvent(QCloseEvent* event) override;

signals:
	void db_open_requested();
	void db_migrate_requested();
	void context_requested();

	void db_backup_requested(QString destination);
	void db_create_new_requested();
	void db_restore_requested(AppDatabase::RestoreContext context);

private slots:
	void on_db_open(fundos::db::status open_result);
	void on_migrate(fundos::db::outcome status);

	void on_context_failure(fundos::db::outcome status);

	void on_result(const fundos::db::outcome& result);
	void go_home();
	void on_quit();

	void db_backup();
	void on_backup_result(fundos::db::outcome status);
	void on_backup_copy_failed();

	void db_create_new();
	void on_create_new(bool succeeded);

	void db_restore();
	void on_restore(AppDatabase::RestoreContext context);

	void db_manage_locale();

	void open_account(const fundos::account& opening);
	void open_account_with_transaction(const fundos::account& opening, std::optional<fundos::transaction> requested);
	void open_fund(const fundos::fund& opening);
};
