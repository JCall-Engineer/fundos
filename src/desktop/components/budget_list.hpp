#pragma once
#include "fundos.hpp"
#include <QWidget>

class BudgetList : public QWidget {
	Q_OBJECT

	std::shared_ptr<fundos::db> database;
	std::vector<fundos::budget> budgets;

public:
	explicit BudgetList(std::shared_ptr<fundos::db> db, QWidget* parent = nullptr);

signals:
	void db_outcome(const fundos::db::outcome& outcome);
};
