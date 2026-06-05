#include "fund_page.hpp"

FundPage::FundPage(std::shared_ptr<fundos::db> db, fundos::fund opening, QWidget *parent) : QWidget(parent), database(std::move(db)), record(std::move(opening)) {

}
