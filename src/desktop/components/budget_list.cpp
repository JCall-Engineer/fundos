#include "budget_list.hpp"

BudgetList::BudgetList(std::shared_ptr<fundos::db> db, QWidget *parent) : QWidget(parent), database(std::move(db)) {
	auto result = database->get_budgets();
	if (!result) {
		emit db_outcome(result.status());
		return;
	}
	emit db_outcome({ fundos::db::error::none, "Budgets Query Successful"});
	budgets = std::move(result.value());
}
