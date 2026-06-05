#include "account_page.hpp"

AccountPage::AccountPage(std::shared_ptr<fundos::db> db, fundos::account opening, QWidget *parent) : QWidget(parent), database(std::move(db)), record(std::move(opening)) {

}
