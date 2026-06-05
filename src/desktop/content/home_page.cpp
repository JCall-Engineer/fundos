#include "home_page.hpp"
#include <QHBoxLayout>
#include <QScrollArea>

static constexpr int HORIZONTAL_BREAKPOINT = 600;
void HomePage::relayout() {
	bool use_horizontal = width() >= HORIZONTAL_BREAKPOINT;

	auto* container = new QWidget();
	auto direction = use_horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom;
	auto* container_layout = new QBoxLayout(direction, container);
	container_layout->setContentsMargins(0, 0, 0, 0);
	container_layout->setSpacing(0);

	container_layout->addWidget(account_list);
	container_layout->addWidget(fund_list);
	container_layout->addWidget(budget_list);

	// setWidget replaces the old container and deletes it,
	// but our list widgets are parented to HomePage so they survive
	scroll_area->setWidget(container);
}

HomePage::HomePage(std::shared_ptr<fundos::db> db, QWidget* parent) : QWidget(parent), database(std::move(db)) {
	auto* root_layout = new QVBoxLayout(this);
	root_layout->setContentsMargins(0, 0, 0, 0);

	scroll_area = new QScrollArea(this);
	scroll_area->setWidgetResizable(true);
	scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	root_layout->addWidget(scroll_area);

	auto locale = database->get_currency_locale();
	if (!locale) {
		emit db_outcome(locale.status());
		return;
	}

	account_list = new AccountList(database, locale.value(), this);
	fund_list    = new FundList(database, locale.value(), this);
	budget_list  = new BudgetList(database, this);

	connect(account_list, &AccountList::db_outcome,   this, &HomePage::db_outcome);
	connect(fund_list,    &FundList::db_outcome,      this, &HomePage::db_outcome);
	connect(budget_list,  &BudgetList::db_outcome,    this, &HomePage::db_outcome);
	connect(account_list, &AccountList::open_account, this, &HomePage::open_account);
	connect(fund_list,    &FundList::open_fund,       this, &HomePage::open_fund);
	connect(budget_list,  &BudgetList::open_budget,   this, &HomePage::open_budget);
	connect(account_list, &AccountList::import_ofx,   this, &HomePage::import_ofx);
	connect(account_list, &AccountList::go_home,      this, &HomePage::go_home);
	connect(fund_list,    &FundList::go_home,         this, &HomePage::go_home);

	relayout();
}

void HomePage::resizeEvent(QResizeEvent* event) {
	QWidget::resizeEvent(event);
	relayout();
}
