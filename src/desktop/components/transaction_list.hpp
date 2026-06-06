#pragma once
#include "context.hpp"
#include <QWidget>

class TransactionList : public QWidget {
	Q_OBJECT

	std::shared_ptr<AppContext> context;
	fundos::db::transaction_history history;

public:
	explicit TransactionList(std::shared_ptr<AppContext> ctx, int64_t account_id, fundos::datetime after, fundos::datetime before, QWidget* parent = nullptr);

signals:
	void db_outcome(const fundos::db::outcome& outcome);
};
