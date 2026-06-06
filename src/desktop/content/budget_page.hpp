#pragma once
#include "context.hpp"
#include <QWidget>

class BudgetPage : public QWidget {
	Q_OBJECT

	std::shared_ptr<AppContext> context;
	fundos::budget record;

public:
	explicit BudgetPage(std::shared_ptr<AppContext> ctx, fundos::budget opening, QWidget* parent = nullptr);

signals:
};
