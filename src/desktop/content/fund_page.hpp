#pragma once
#include "context.hpp"
#include <QWidget>

class FundPage : public QWidget {
	Q_OBJECT

	std::shared_ptr<AppContext> context;
	fundos::fund record;

public:
	explicit FundPage(std::shared_ptr<AppContext> ctx, fundos::fund opening, QWidget* parent = nullptr);

signals:
};
