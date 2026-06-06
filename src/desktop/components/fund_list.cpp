#include "fund_list.hpp"

FundList::FundList(std::shared_ptr<AppContext> ctx, QWidget *parent) : QWidget(parent), context(std::move(ctx)) {
	auto result = context->database->get_funds();
	if (!result) {
		emit db_outcome(result.status());
		return;
	}
	emit db_outcome({ fundos::db::error::none, "Funds Query Successful"});
	funds = std::move(result.value());
}
