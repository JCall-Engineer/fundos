#include "import_accounts_page.hpp"
#include <QComboBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>

ImportAccountsPage::ImportAccountsPage(
	AppCoordinator* coordinator,
	std::shared_ptr<ImportAccountsPage::Iterator> missing,
	QWidget* parent
) : QWidget(parent), app_coordinator(coordinator), current(missing) {
	auto* database = app_coordinator->database();
	connect(this,            &ImportAccountsPage::refresh_accounts_requested, database, &AppDatabase::request_accounts);
	connect(this,            &ImportAccountsPage::save_account_requested,     database, &AppDatabase::save_account);
	connect(database,        &AppDatabase::account_saved,                     this,     &ImportAccountsPage::on_account_saved);
	connect(app_coordinator, &AppCoordinator::refreshed,                      this,     &ImportAccountsPage::show_current);

	layout = new QVBoxLayout(this);
	layout->setAlignment(Qt::AlignTop);
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(8);
	show_current();
}

void ImportAccountsPage::show_current() {
	confirm_button = nullptr;
	while (QLayoutItem* item = layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}

	const auto* bank_account = current->advance_to_unmatched(app_coordinator->context()->accounts());
	if (bank_account == nullptr) {
		emit ready_for_merge(current->get());
		return;
	}

	auto* identity_label = new QLabel(tr("Balance: %1 as of %2")
		.arg(QString::fromStdString(bank_account->balance.to_string(app_coordinator->context()->currency_locale().info())))
		.arg(QDateTime::fromMSecsSinceEpoch(bank_account->as_of.milliseconds_since_epoch).toString(QLocale::system().dateFormat(QLocale::ShortFormat))),
		this);
	layout->addWidget(identity_label);

	auto* preview_list = new QListWidget(this);
	preview_list->setMaximumHeight(150);
	preview_list->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	int32_t preview_count = 0;
	for (const auto& imported : bank_account->transactions) {
		QString line = tr("%1 — %2")
			.arg(QString::fromStdString(imported.importing.memo))
			.arg(QString::fromStdString(imported.importing.amount.to_string(app_coordinator->context()->currency_locale().info())));
		preview_list->addItem(line);
		++preview_count;
	}
	layout->addWidget(preview_list);

	layout->addSpacing(16);

	auto* prompt = new QLabel(tr("Assign bank account \"%1\" to:").arg(QString::fromStdString(bank_account->acct_id)), this);
	layout->addWidget(prompt);

	auto* picker = new QComboBox(this);
	picker->addItem(tr("Create New Account"), QVariant());
	for (const auto& account : app_coordinator->context()->accounts()) {
		if (account.bank_account_id.has_value()) { continue; }
		picker->addItem(QString::fromStdString(account.name), QVariant::fromValue(account.id()));
	}
	layout->addWidget(picker);

	confirm_button = new QPushButton(tr("Continue"), this);
	layout->addWidget(confirm_button);
	connect(confirm_button, &QPushButton::clicked, this, [this, picker, bank_account]() {
		if (!picker->currentData().isValid()) {
			bool accepted = false;
			QString name = QInputDialog::getText(this, tr("New Account"), tr("Account name:"), QLineEdit::Normal, "", &accepted);
			if (!accepted) { return; }
			name = name.trimmed();
			if (name.isEmpty()) { return; }

			fundos::account creating = {
				.name             = name.toStdString(),
				.bank_account_id  = bank_account->acct_id,
			};
			confirm_button->setEnabled(false);
			emit save_account_requested(creating);
		} else {
			int64_t account_id = picker->currentData().value<int64_t>();
			auto* existing = app_coordinator->context()->account(account_id);
			if (existing == nullptr) { return; }

			fundos::account updating = *existing;
			updating.bank_account_id = bank_account->acct_id;
			confirm_button->setEnabled(false);
			emit save_account_requested(updating);
		}
	});
}

void ImportAccountsPage::on_account_saved(fundos::db::outcome status) {
	if (!status) {
		if (confirm_button) {
			confirm_button->setEnabled(true);
		}
		return;
	}
	emit refresh_accounts_requested();
}

void ImportAccountsPage::on_accounts_refreshed(fundos::db::result<fundos::account> results) {
	if (!results) {
		if (confirm_button) {
			confirm_button->setEnabled(true);
		}
		return;
	}
	show_current();
}
