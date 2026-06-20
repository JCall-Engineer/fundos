#include "data/import.hpp"
#include "import_transactions_page.hpp"

ImportTransactionsPage::ImportTransactionsPage(
	AppCoordinator* coordinator,
	std::shared_ptr<fundos::import::pending_import> data,
	QWidget *parent
) : QWidget(std::move(parent)), data(std::move(data)), app_coordinator(std::move(coordinator)) {
	
}
