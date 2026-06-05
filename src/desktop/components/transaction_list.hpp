#pragma once
#include "fundos.hpp"
#include <QWidget>

class TransactionList : public QWidget {
	Q_OBJECT

	std::shared_ptr<fundos::db> database;
	fundos::db::transaction_history history;

public:
	explicit TransactionList(std::shared_ptr<fundos::db> db, int64_t account_id, fundos::datetime after, fundos::datetime before, QWidget* parent = nullptr);

signals:
	void db_outcome(const fundos::db::outcome& outcome);
};
