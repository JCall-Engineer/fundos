#include "fund_list.hpp"
#include "components/navigable_row.hpp"
#include "theme.hpp"
#include <QVBoxLayout>

FundList::FundList(std::shared_ptr<AppContext> ctx, QWidget* parent) : QWidget(parent), context(std::move(ctx)) {
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->setAlignment(Qt::AlignTop);
}

void FundList::initialize() {
	// Clear existing rows before repopulating
	auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
	QLayoutItem* item;
	while ((item = layout->takeAt(0)) != nullptr) {
		delete item->widget();
		delete item;
	}

	auto fetched = context->database->get_funds();
	if (!fetched) {
		emit db_outcome(fetched.status());
		return;
	}
	emit db_outcome({ fundos::db::error::none, "Accounts Query Successful" });
	funds = std::move(fetched.value());

	for (int i = 0; i < static_cast<int>(funds.size()); i++) {
		auto balance = context->database->get_fund_balance(funds[i].id());
		QLabel* amount = nullptr;
		if (balance) {
			amount = theme::currency_label(balance.value(), context->currency_locale.info());
		} else {
			emit db_outcome(balance.status());
		}

		auto* row = new NavigableRow(i, QString::fromStdString(funds[i].name), amount, this);
		connect(row, &NavigableRow::clicked, this, [this](int index) {
			emit open_fund(std::make_shared<fundos::fund>(funds[index]));
		});
		layout->addWidget(row);
	}
}
