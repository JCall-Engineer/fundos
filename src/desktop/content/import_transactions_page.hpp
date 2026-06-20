#pragma once
#include "coordinator.hpp"
#include <QWidget>

class ImportTransactionsPage : public QWidget {
	Q_OBJECT

	AppCoordinator* app_coordinator;
	std::shared_ptr<fundos::import::pending_import> data;

public:
	explicit ImportTransactionsPage(AppCoordinator* coordinator, std::shared_ptr<fundos::import::pending_import> data, QWidget* parent = nullptr);

signals:
};
