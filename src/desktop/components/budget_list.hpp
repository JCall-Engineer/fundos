#pragma once
#include "context.hpp"
#include <QWidget>

class BudgetList : public QWidget {
	Q_OBJECT

	std::shared_ptr<AppContext> context;
	std::vector<fundos::budget> budgets;

public:
	explicit BudgetList(std::shared_ptr<AppContext> ctx, QWidget* parent = nullptr);

signals:
	void db_outcome(const fundos::db::outcome& outcome);
	void open_budget(std::shared_ptr<fundos::budget> opening);
};
