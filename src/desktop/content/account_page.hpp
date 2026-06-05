#pragma once
#include "fundos.hpp"
#include <QWidget>

class AccountPage : public QWidget {
	Q_OBJECT

	std::shared_ptr<fundos::db> database;
	fundos::account record;

public:
	explicit AccountPage(std::shared_ptr<fundos::db> db, fundos::account opening, QWidget* parent = nullptr);

signals:
};
