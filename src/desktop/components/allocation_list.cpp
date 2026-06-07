#include "allocation_list.hpp"

AllocationList::AllocationList(std::shared_ptr<AppContext> ctx, int64_t fund_id, fundos::datetime after, fundos::datetime before, QWidget *parent) : QWidget(parent), context(std::move(ctx)) {
	auto result = context->db()->fund_history(fund_id, after, before);
	if (!result) {
		emit db_outcome(result.status());
		return;
	}
	emit db_outcome({ fundos::db::error::none, "Fund History Query Successful"});
	history = std::move(result.value());
	
}
