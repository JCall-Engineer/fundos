#pragma once
#include "context.hpp"
#include "components/account_list.hpp"
#include "components/fund_list.hpp"
#include "components/budget_list.hpp"
#include <QWidget>
#include <QScrollArea>

class HomePage : public QWidget {
	Q_OBJECT

	std::shared_ptr<AppContext> context;

	QScrollArea* scroll_area  = nullptr;
	AccountList* account_list = nullptr;
	FundList*    fund_list    = nullptr;
	BudgetList*  budget_list  = nullptr;

	void relayout();

protected:
	void resizeEvent(QResizeEvent* event) override;

public:
	explicit HomePage(std::shared_ptr<AppContext> ctx, QWidget* parent = nullptr);

	///  Must be called after HonePage's signals are connected
	void initialize();

signals:
	void db_outcome(const fundos::db::outcome& outcome);
	void open_account(std::shared_ptr<fundos::account> opening);
	void open_fund(std::shared_ptr<fundos::fund> opening);
	void open_budget(std::shared_ptr<fundos::budget> opening);
	void import_ofx();
	void go_home();
};
