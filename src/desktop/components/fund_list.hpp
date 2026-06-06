#pragma once
#include "context.hpp"
#include <QWidget>

class FundList : public QWidget {
	Q_OBJECT

	std::shared_ptr<AppContext> context;
	std::vector<fundos::fund> funds;

public:
	explicit FundList(std::shared_ptr<AppContext> ctx, QWidget* parent = nullptr);

signals:
	void db_outcome(const fundos::db::outcome& outcome);
	void open_fund(std::shared_ptr<fundos::fund> opening);
	void go_home();
};
