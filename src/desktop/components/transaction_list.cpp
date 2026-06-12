#include "transaction_list.hpp"

TransactionList::TransactionList(
	std::shared_ptr<AppContext> ctx,
	int64_t account_id, fundos::datetime after, fundos::datetime before,
	QWidget *parent
) : QWidget(parent), context(std::move(ctx)) {
	
}
