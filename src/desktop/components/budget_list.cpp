#include "budget_list.hpp"

BudgetList::BudgetList(std::shared_ptr<AppContext> ctx, QWidget *parent) : QWidget(parent), context(std::move(ctx)) {
	auto result = context->database->get_budgets();
	if (!result) {
		emit db_outcome(result.status());
		return;
	}
	emit db_outcome({ fundos::db::error::none, "Budgets Query Successful"});
	budgets = std::move(result.value());
}
