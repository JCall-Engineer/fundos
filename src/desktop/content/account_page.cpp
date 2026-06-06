#include "account_page.hpp"

AccountPage::AccountPage(std::shared_ptr<AppContext> ctx, fundos::account opening, QWidget *parent) : QWidget(parent), context(std::move(ctx)), record(std::move(opening)) {

}
