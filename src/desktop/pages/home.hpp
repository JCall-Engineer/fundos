#pragma once
#include "fundos.hpp"
#include <QWidget>
#include <QScrollArea>

class HomePage : public QWidget {
	Q_OBJECT

	QScrollArea* make_panel(QWidget* content);

	std::shared_ptr<fundos::db> database;

public:
	explicit HomePage(std::shared_ptr<fundos::db> db, QWidget* parent = nullptr);

signals:
	void db_outcome(const fundos::db::outcome& outcome);
	void create_account();
	void create_fund();
	void open_budget(fundos::budget opening);
	void import_ofx();
};
