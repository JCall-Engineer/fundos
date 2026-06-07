#include "transaction_list.hpp"

TransactionList::TransactionList(std::shared_ptr<AppContext> ctx, int64_t account_id, fundos::datetime after, fundos::datetime before, QWidget *parent) : QWidget(parent), context(std::move(ctx)) {
	auto result = context->db()->account_history(account_id, after, before);
	if (!result) {
		emit db_outcome(result.status());
		return;
	}
	emit db_outcome({ fundos::db::error::none, "Account History Query Successful"});
	history = std::move(result.value());
	
}
