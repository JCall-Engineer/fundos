#pragma once
#include "fundos.hpp"
#include <QWidget>

class AllocationList : public QWidget {
	Q_OBJECT

	std::shared_ptr<fundos::db> database;
	fundos::db::allocation_history history;

public:
	explicit AllocationList(std::shared_ptr<fundos::db> db, int64_t fund_id, fundos::datetime after, fundos::datetime before, QWidget* parent = nullptr);

signals:
	void db_outcome(const fundos::db::outcome& outcome);
};
