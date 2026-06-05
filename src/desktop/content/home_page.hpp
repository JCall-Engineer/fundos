#pragma once
#include "fundos.hpp"
#include "components/account_list.hpp"
#include "components/fund_list.hpp"
#include "components/budget_list.hpp"
#include <QWidget>
#include <QScrollArea>

class HomePage : public QWidget {
	Q_OBJECT

	std::shared_ptr<fundos::db> database;

	QScrollArea* scroll_area;
	AccountList* account_list;
	FundList* fund_list;
	BudgetList* budget_list;

	void relayout();

protected:
	void resizeEvent(QResizeEvent* event) override;

public:
	explicit HomePage(std::shared_ptr<fundos::db> db, QWidget* parent = nullptr);

signals:
	void db_outcome(const fundos::db::outcome& outcome);
	void open_account(std::shared_ptr<fundos::account> opening);
	void open_fund(std::shared_ptr<fundos::fund> opening);
	void open_budget(std::shared_ptr<fundos::budget> opening);
	void import_ofx();
	void go_home();
};
