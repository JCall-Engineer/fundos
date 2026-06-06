#include "budget_page.hpp"

BudgetPage::BudgetPage(std::shared_ptr<AppContext> ctx, fundos::budget opening, QWidget *parent) : QWidget(parent), context(std::move(ctx)), record(std::move(opening)) {

}
