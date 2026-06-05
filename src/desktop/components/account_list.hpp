#pragma once
#include "fundos.hpp"
#include <QWidget>

class AccountList : public QWidget {
	Q_OBJECT

	std::shared_ptr<fundos::db> database;
	std::vector<fundos::account> accounts;

public:
	explicit AccountList(std::shared_ptr<fundos::db> db, QWidget* parent = nullptr);

signals:
	void db_outcome(const fundos::db::outcome& outcome);
	void open_account(std::shared_ptr<fundos::account> opening);
	void import_ofx();
	void go_home();
};
