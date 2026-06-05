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
	void open_account(std::shared_ptr<fundos::account> opening);
	void open_fund(std::shared_ptr<fundos::fund> opening);
	void open_budget(std::shared_ptr<fundos::budget> opening);
	void import_ofx();
	void go_home();
};
