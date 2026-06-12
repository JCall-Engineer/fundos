#include "account_page.hpp"
#include "theme.hpp"
#include "components/loading_spinner.hpp"
#include <QDateTime>
#include <QHboxLayout>
#include <QScrollArea>
#include <QSize>

AccountPage::AccountPage(
	AppCoordinator* coordinator,
	fundos::account opening,
	QWidget *parent
) : QWidget(parent), app_coordinator(std::move(coordinator)), record(std::move(opening)) {
	auto* database = app_coordinator->database();
	connect(this,     &AccountPage::save_account_requested,   database, &AppDatabase::save_account);
	connect(this,     &AccountPage::history_requested,        database, &AppDatabase::request_account_history);
	connect(database, &AppDatabase::account_saved,            this,     &AccountPage::on_account_saved);
	connect(database, &AppDatabase::account_history_received, this,     &AccountPage::on_history);

	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	{
		auto* header_row = new QWidget(this);
		header_row->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

		auto* header_layout = new QHBoxLayout(header_row);
		header_layout->setContentsMargins(8, 8, 8, 8);
		header_layout->setSpacing(8);

		name_label = new EditableLabel(QString::fromStdString(record.name), this);
		connect(name_label, &EditableLabel::value_changed, this, &AccountPage::rename);
		QSize button_size = QSize(name_label->sizeHint().height(), name_label->sizeHint().height());

		auto* home_button = new QPushButton(this);
		home_button->setIcon(theme::colored_svg_icon(":/icons/home.svg", theme::text, button_size));
		home_button->setIconSize(button_size);
		home_button->setToolTip(tr("Home"));
		connect(home_button, &QPushButton::clicked, this, &AccountPage::go_home);

		auto* import_button = new QPushButton(tr("Import OFX"), this);
		import_button->setIcon(theme::colored_svg_icon(":/icons/upload.svg", theme::text, button_size));
		connect(import_button, &QPushButton::clicked, this, &AccountPage::import_ofx);

		auto* new_transaction_button = new QPushButton(tr("New Transaction"), this);
		new_transaction_button->setIcon(theme::colored_svg_icon(":/icons/plus.svg", theme::text, button_size));
		connect(new_transaction_button, &QPushButton::clicked, this, &AccountPage::new_transaction);

		open_close_button = new QPushButton(this);
		update_open_close_button();
		connect(open_close_button, &QPushButton::clicked, this, &AccountPage::on_toggle_open);

		header_layout->addWidget(home_button);
		header_layout->addWidget(name_label);
		header_layout->addStretch();
		header_layout->addWidget(import_button);
		header_layout->addWidget(new_transaction_button);
		header_layout->addWidget(open_close_button);

		layout->addWidget(header_row);
	}
	{
		auto* filter_row = new QWidget(this);
		filter_row->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

		auto* filter_layout = new QHBoxLayout(filter_row);
		filter_layout->setContentsMargins(8, 8, 8, 8);
		filter_layout->setSpacing(8);

		auto today = QDate::currentDate();
		auto* after_label = new QLabel(tr("From"), this);
		after_picker = new DatePicker(today.addMonths(-1), this);

		auto* before_label = new QLabel(tr("Until"), this);
		before_picker = new DatePicker(today, this);

		QSize button_size = QSize(after_label->sizeHint().height(), after_label->sizeHint().height());

		auto* month_button = new QPushButton(tr("Last Month"), this);
		connect(month_button, &QPushButton::clicked, this, [this]() {
			loading_preset_date_range = true;
			auto today = QDate::currentDate();
			after_picker->set_date(today.addMonths(-1));
			before_picker->set_date(today);
			loading_preset_date_range = false;
			fetch_history();
		});

		auto* quarter_button = new QPushButton(tr("3 Months"), this);
		connect(quarter_button, &QPushButton::clicked, this, [this]() {
			loading_preset_date_range = true;
			auto today = QDate::currentDate();
			after_picker->set_date(today.addMonths(-3));
			before_picker->set_date(today);
			loading_preset_date_range = false;
			fetch_history();
		});

		auto* ytd_button = new QPushButton(tr("YTD"), this);
		connect(ytd_button, &QPushButton::clicked, this, [this]() {
			loading_preset_date_range = true;
			auto today = QDate::currentDate();
			after_picker->set_date(QDate(today.year(), 1, 1));
			before_picker->set_date(today);
			loading_preset_date_range = false;
			fetch_history();
		});

		filter_layout->addWidget(after_label);
		filter_layout->addWidget(after_picker);
		filter_layout->addSpacing(8);
		filter_layout->addWidget(before_label);
		filter_layout->addWidget(before_picker);
		filter_layout->addSpacing(8);
		filter_layout->addWidget(month_button);
		filter_layout->addWidget(quarter_button);
		filter_layout->addWidget(ytd_button);
		filter_layout->addStretch();

		layout->addWidget(filter_row);
	}
	{
		auto* history_scroll = new QScrollArea(this);
		history_scroll->setWidgetResizable(true);

		history_panel = new QWidget();
		history_scroll->setWidget(history_panel);

		history_layout = new QVBoxLayout(history_panel);
		history_layout->setContentsMargins(0, 0, 0, 0);
		history_layout->setSpacing(0);
		fetch_history();

		layout->addWidget(history_scroll, 1);
	}
}

void AccountPage::rename(QString name) {
	previous_name = record.name;
	previous_closed_at = record.closed_at;
	record.name = name.toStdString();
	emit save_account_requested(record);
}

void AccountPage::new_transaction() {

}

void AccountPage::update_open_close_button() {
	QSize button_size = QSize(name_label->sizeHint().height(), name_label->sizeHint().height());
	if (record.closed_at.has_value()) {
		open_close_button->setText(tr("Open Account"));
		open_close_button->setIcon(theme::colored_svg_icon(":/icons/lock-open.svg", theme::success_foreground, button_size));
		open_close_button->setStyleSheet(QString(
			"QPushButton {"
			"  color: %1;"
			"  border: 1px solid %1;"
			"  border-radius: 4px;"
			"  padding: 4px 8px;"
			"}"
			"QPushButton:hover {"
			"  color: %2;"
			"  border-color: %2;"
			"}"
		).arg(
			theme::success_foreground.name(),
			theme::success_foreground.lighter(120).name()
		));
	} else {
		open_close_button->setText(tr("Close Account"));
		open_close_button->setIcon(theme::colored_svg_icon(":/icons/lock.svg", theme::error_foreground, button_size));
		open_close_button->setStyleSheet(QString(
			"QPushButton {"
			"  color: %1;"
			"  border: 1px solid %1;"
			"  border-radius: 4px;"
			"  padding: 4px 8px;"
			"}"
			"QPushButton:hover {"
			"  color: %2;"
			"  border-color: %2;"
			"}"
		).arg(
			theme::error_foreground.name(),
			theme::error_foreground.lighter(120).name()
		));
	}
}
void AccountPage::on_toggle_open() {
	previous_name = record.name;
	previous_closed_at = record.closed_at;
	if (previous_closed_at.has_value()) {
		record.closed_at = std::nullopt;
	} else {
		record.closed_at = fundos::datetime{QDateTime::currentMSecsSinceEpoch()};
	}
	emit save_account_requested(record);
}

void AccountPage::on_account_saved(fundos::db::outcome saved) {
	if (!saved) {
		record.name = previous_name;
		name_label->set_text(QString::fromStdString(previous_name));
		record.closed_at = previous_closed_at;
	} else {
		app_coordinator->update_account(record);
	}
	update_open_close_button();
}

void AccountPage::clear_history() {
	while (QLayoutItem* item = history_layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}
}

void AccountPage::fetch_history() {
	if (loading_preset_date_range) { return; }
	clear_history();

	auto* info_row = new QWidget(history_panel);
	history_layout->addWidget(info_row);

	auto* info_layout = new QHBoxLayout(info_row);

	auto* spinner = new LoadingSpinner(this);
	info_layout->addWidget(spinner);

	fundos::datetime after  = {after_picker->date().startOfDay().toMSecsSinceEpoch()};
	fundos::datetime before = {before_picker->date().endOfDay().toMSecsSinceEpoch()};
	emit history_requested(record.id(), after, before);
}

void AccountPage::on_history(fundos::db::result<fundos::db::transaction_history> received) {
	clear_history();
	if (!received) {
		auto* info_row = new QWidget(this);
		history_layout->addWidget(info_row);

		auto* info_layout = new QHBoxLayout(info_row);
		info_layout->addWidget(theme::header_label(tr("Error getting transaction history."), history_panel), 0, Qt::AlignCenter);
		return;
	}
	auto& history = received.value();
	if (history.transactions.empty() && history.ledger_balances.empty()) {
		auto* info_row = new QWidget(this);
		history_layout->addWidget(info_row);

		auto* info_layout = new QHBoxLayout(info_row);
		info_layout->addWidget(theme::header_label(tr("No transactions recorded during the selected period."), history_panel), 0, Qt::AlignCenter);
		return;
	}
	std::optional<fundos::currency> balance_checker;
	size_t tx_index = 0;
	size_t lb_index = 0;
	auto add_transaction = [this, &balance_checker](fundos::db::transaction_history::allocated_transaction& transaction) -> void {
		// todo
	};
	auto add_ledger_balance = [this, &balance_checker](fundos::import_ledger_balance& ledger_balance) -> void {
		// todo
	};

	while (tx_index < history.transactions.size() || lb_index < history.ledger_balances.size()) {
		if (tx_index >= history.transactions.size()) {
			add_ledger_balance(history.ledger_balances[lb_index++]);
			continue;
		}
		if (lb_index >= history.ledger_balances.size()) {
			add_transaction(history.transactions[tx_index++]);
			continue;
		}
		auto& transaction = history.transactions[tx_index];
		auto& ledger_balance = history.ledger_balances[lb_index];

		// TODO: decide tie-breaking order when ledger_balance.date_as_of == transaction.effective_date
		if (ledger_balance.date_as_of < transaction.effective_date) {
			add_ledger_balance(history.ledger_balances[lb_index++]);
		} else {
			add_transaction(history.transactions[tx_index++]);
		}
	}
	history_layout->addStretch();
}
