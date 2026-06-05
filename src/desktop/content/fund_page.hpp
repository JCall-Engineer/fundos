#pragma once
#include "fundos.hpp"
#include <QWidget>

class FundPage : public QWidget {
	Q_OBJECT

	std::shared_ptr<fundos::db> database;
	fundos::fund record;

public:
	explicit FundPage(std::shared_ptr<fundos::db> db, fundos::fund opening, QWidget* parent = nullptr);

signals:
};
