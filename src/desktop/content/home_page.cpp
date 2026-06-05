#include "home_page.hpp"
#include <QHBoxLayout>
#include <QScrollArea>

QScrollArea* HomePage::make_panel(QWidget* content) {
	auto* area = new QScrollArea(this);
	area->setWidget(content);
	area->setWidgetResizable(true);
	area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	return area;
}

HomePage::HomePage(std::shared_ptr<fundos::db> db, QWidget* parent) : QWidget(parent), database(std::move(db)) {
	auto* root_layout = new QHBoxLayout(this);
	root_layout->setContentsMargins(0, 0, 0, 0);
	root_layout->setSpacing(0);

	auto* accounts_panel = new QWidget();
	auto* funds_panel    = new QWidget();
	auto* budgets_panel  = new QWidget();

	accounts_panel->setMinimumWidth(200);
	funds_panel->setMinimumWidth(200);
	budgets_panel->setMinimumWidth(200);

	accounts_panel->setStyleSheet(QString("background: red;"));
	funds_panel->setStyleSheet(QString("background: green;"));
	budgets_panel->setStyleSheet(QString("background: blue;"));

	root_layout->addWidget(make_panel(accounts_panel), 1);
	root_layout->addWidget(make_panel(funds_panel),    1);
	root_layout->addWidget(make_panel(budgets_panel),  1);
}
