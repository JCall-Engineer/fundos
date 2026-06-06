#include "fund_page.hpp"

FundPage::FundPage(std::shared_ptr<AppContext> ctx, fundos::fund opening, QWidget *parent) : QWidget(parent), context(std::move(ctx)), record(std::move(opening)) {

}
