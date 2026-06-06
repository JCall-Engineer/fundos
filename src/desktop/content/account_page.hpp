#pragma once
#include "context.hpp"
#include <QWidget>

class AccountPage : public QWidget {
	Q_OBJECT

	std::shared_ptr<AppContext> context;
	fundos::account record;

public:
	explicit AccountPage(std::shared_ptr<AppContext> ctx, fundos::account opening, QWidget* parent = nullptr);

signals:
};
