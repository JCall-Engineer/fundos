#pragma once
#include "context.hpp"
#include <QWidget>

class AccountList : public QWidget {
	Q_OBJECT

	std::shared_ptr<AppContext> context;
	std::vector<fundos::account> accounts;

public:
	explicit AccountList(std::shared_ptr<AppContext> ctx, QWidget* parent = nullptr);

signals:
	void db_outcome(const fundos::db::outcome& outcome);
	void open_account(std::shared_ptr<fundos::account> opening);
	void import_ofx();
	void go_home();
};
