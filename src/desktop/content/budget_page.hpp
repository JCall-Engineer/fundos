#pragma once
#include "fundos.hpp"
#include <QWidget>

class BudgetPage : public QWidget {
	Q_OBJECT

	std::shared_ptr<fundos::db> database;
	fundos::budget record;

public:
	explicit BudgetPage(std::shared_ptr<fundos::db> db, fundos::budget opening, QWidget* parent = nullptr);

signals:
};
