#include "account_list.hpp"
#include "components/navigable_row.hpp"
#include "theme.hpp"
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QToolButton>
#include <QVBoxLayout>

AccountList::AccountList(std::shared_ptr<AppContext> ctx, QWidget *parent) : QWidget(parent), context(std::move(ctx)) {
	auto* root_layout = new QVBoxLayout(this);
	auto* header = new QHBoxLayout();

	auto* header_label = theme::header_label("ACCOUNTS", this);
	const auto header_height = header_label->sizeHint().height();
	const QSize header_button_size = QSize(header_height, header_height);

	auto* import_button = new QToolButton(this);
	import_button->setIcon(theme::colored_svg_icon(":/icons/upload.svg", theme::text, header_button_size));
	import_button->setAutoRaise(true);

	auto* add_button = new QToolButton(this);
	add_button->setIcon(theme::colored_svg_icon(":/icons/plus.svg", theme::text, header_button_size));
	add_button->setAutoRaise(true);

	header->addWidget(header_label);
	header->addStretch();
	header->addWidget(import_button);
	header->addWidget(add_button);

	auto* list_container = new QWidget(this);
	auto* list_layout = new QVBoxLayout(list_container);
	list_layout->setContentsMargins(0, 0, 0, 0);
	list_layout->setSpacing(0);
	list_layout->addStretch();

	root_layout->addLayout(header);
	root_layout->addWidget(list_container);

	connect(import_button, &QToolButton::clicked, this, &AccountList::import_ofx);
	connect(add_button, &QToolButton::clicked, this, [this]() {
		bool accepted = false;
		QString name = QInputDialog::getText(this, "New Account", "Account name:", QLineEdit::Normal, "", &accepted);
		if (!accepted) { return; }
		name = name.trimmed();
		if (name.isEmpty()) { return; }

		fundos::account creating = { .name = name.toStdString() };
		auto saved = context->database->save_account(creating);
		if (saved) {
			emit go_home();
		} else {
			emit db_outcome(saved);
		}
	});

	auto fetched = context->database->get_accounts();
	if (!fetched) {
		emit db_outcome(fetched.status());
		return;
	}
	emit db_outcome({ fundos::db::error::none, "Accounts Query Successful" });
	accounts = std::move(fetched.value());

	for (int i = 0; i < static_cast<int>(accounts.size()); i++) {
		auto balance = context->database->get_account_balance(accounts[i].id());
		QLabel* amount = nullptr;
		if (balance) {
			amount = theme::currency_label(balance.value(), context->currency_locale.info());
		} else {
			emit db_outcome(balance.status());
		}

		auto* row = new NavigableRow(i, QString::fromStdString(accounts[i].name), amount, this);
		connect(row, &NavigableRow::clicked, this, [this](int index) {
			emit open_account(std::make_shared<fundos::account>(accounts[index]));
		});
		list_layout->addWidget(row);
	}
}
