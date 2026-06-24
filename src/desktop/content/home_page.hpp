#pragma once
#include <functional>
#include "coordinator.hpp"
#include <QWidget>
#include <QScrollArea>
#include <QString>

class HomePage : public QWidget {
	Q_OBJECT

	AppCoordinator* app_coordinator;

	QScrollArea* scroll_area   = nullptr;
	QWidget*     account_panel = nullptr;
	QWidget*     fund_panel    = nullptr;
	QWidget*     budget_panel  = nullptr;
	QWidget*     account_list  = nullptr;
	QWidget*     fund_list     = nullptr;
	QWidget*     budget_list   = nullptr;
	bool show_closed_accounts  = false;
	bool show_closed_funds     = false;

	struct button_spec {
		QString tooltip;
		QString icon_path;
		std::function<void()> action;

		QString checked_icon_path;
		void (HomePage::*toggle_signal)(bool) = nullptr;
	};

	QWidget* make_panel(QWidget* list, const QString& title, std::vector<button_spec> buttons);
	void relayout();

	void make_accounts(const std::vector<fundos::account>& accounts);
	void make_funds   (const std::vector<fundos::fund>&    funds);
	void make_budgets (const std::vector<fundos::budget>&  budgets);

protected:
	void resizeEvent(QResizeEvent* event) override;

public:
	explicit HomePage(AppCoordinator* coordinator, QWidget* parent = nullptr);

public slots:
	void on_account_created(fundos::db::outcome saved);
	void on_fund_created   (fundos::db::outcome saved);

	void on_accounts(fundos::db::result<std::vector<fundos::account>> accounts);
	void on_funds   (fundos::db::result<std::vector<fundos::fund>>    funds);
	void on_budgets (fundos::db::result<std::vector<fundos::budget>> budgets);

signals:
	void toggle_closed_accounts(bool);
	void toggle_closed_funds(bool);

	void accounts_requested();
	void funds_requested();

	void account_balance_requested(int64_t account_id);
	void fund_balance_requested   (int64_t fund_id);

	void create_account(fundos::account saving);
	void create_fund   (fundos::fund    saving);

	void open_account(const fundos::account& opening);
	void open_fund(const fundos::fund& opening);
	void open_budget(const fundos::budget& opening);
};
