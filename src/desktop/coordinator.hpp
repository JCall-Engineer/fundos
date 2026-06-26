#pragma once
#include <memory>
#include "fundos.hpp"
#include "database.hpp"
#include "context.hpp"
#include <QObject>
#include <QWidget>

class AppCoordinator : public QObject {
	Q_OBJECT

	QWidget* parent_widget;
	AppDatabase* app_database;
	std::shared_ptr<AppContext> current_context;

	bool show_retry_dialog(const fundos::db::outcome& outcome);

public:
	explicit AppCoordinator(AppDatabase* database, QWidget* parent);

	AppDatabase* database() { return app_database; }
	std::shared_ptr<AppContext> context() { return current_context; }

	/// True once locales, accounts, funds, and budgets have all been received at least once.
	/// Gates whether a mutation should fire created() (the first time everything is present) vs refreshed() (already had a full context, this is just an update).
	/// See on_*_received below.
	const bool is_ready() const {
		return (current_context
			&& current_context->currency
			&& current_context->percentage
			&& current_context->account_list
			&& current_context->fund_list
			&& current_context->budget_list
		);
	}

	void update_locales(const fundos::currency_locale::selection& currency, const fundos::percentage_locale::selection& percentage);

	void update_account(const fundos::account& updating);
	void update_fund   (const fundos::fund&    updating);
	void update_budget (const fundos::budget&  updating);

public slots:
	void on_database_open();

	void on_locales_received (fundos::db::result<fundos::currency_locale::selection> currency, fundos::db::result<fundos::percentage_locale::selection> percentage);
	void on_accounts_received(fundos::db::result<std::vector<fundos::account>> accounts);
	void on_funds_received   (fundos::db::result<std::vector<fundos::fund>>    funds);
	void on_budgets_received (fundos::db::result<std::vector<fundos::budget>>  budgets);

signals:
	void refreshed();
	void created();
	void needs_locale();
	void creation_failure(fundos::db::outcome);

	void locales_requested();
	void accounts_requested();
	void funds_requested();
	void budgets_requested();
};
