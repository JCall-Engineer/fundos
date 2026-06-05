#include "account_list.hpp"

AccountList::AccountList(std::shared_ptr<fundos::db> db, QWidget *parent) : QWidget(parent), database(std::move(db)) {
	auto result = database->get_accounts();
	if (!result) {
		emit db_outcome(result.status());
		return;
	}
	emit db_outcome({ fundos::db::error::none, "Accounts Query Successful"});
	accounts = std::move(result.value());
}
