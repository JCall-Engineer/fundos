#pragma once
#include "fundos.hpp"
#include <QWidget>

class FundList : public QWidget {
	Q_OBJECT

	std::shared_ptr<fundos::db> database;
	std::vector<fundos::fund> funds;

public:
	explicit FundList(std::shared_ptr<fundos::db> db, QWidget* parent = nullptr);

signals:
	void db_outcome(const fundos::db::outcome& outcome);
};
