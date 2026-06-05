#include "fund_list.hpp"

FundList::FundList(std::shared_ptr<fundos::db> db, const fundos::currency_locale::spec& locale, QWidget *parent) : QWidget(parent), database(std::move(db)) {
	auto result = database->get_funds();
	if (!result) {
		emit db_outcome(result.status());
		return;
	}
	emit db_outcome({ fundos::db::error::none, "Funds Query Successful"});
	funds = std::move(result.value());
}
