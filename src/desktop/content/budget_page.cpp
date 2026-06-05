#include "budget_page.hpp"

BudgetPage::BudgetPage(std::shared_ptr<fundos::db> db, fundos::budget opening, QWidget *parent) : QWidget(parent), database(std::move(db)), record(std::move(opening)) {

}
