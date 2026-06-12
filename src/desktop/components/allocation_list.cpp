#include "allocation_list.hpp"

AllocationList::AllocationList(
	std::shared_ptr<AppContext> ctx,
	int64_t fund_id, fundos::datetime after, fundos::datetime before,
	QWidget *parent
) : QWidget(parent), context(std::move(ctx)) {
	
}
