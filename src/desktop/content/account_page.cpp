#include "account_page.hpp"
#include "theme.hpp"
#include "content/import_dialog.hpp"
#include "content/transaction_dialog.hpp"
#include "components/loading_spinner.hpp"
#include "components/table_view.hpp"
#include <QLatin1StringView>
#include <QMessageBox>
#include <QDateTime>
#include <QDialog>
#include <QHBoxLayout>

AccountPage::AccountPage(
	AppCoordinator* coordinator,
	fundos::account opening,
	std::optional<fundos::transaction> requested,
	QWidget *parent
) : QWidget(parent), app_coordinator(std::move(coordinator)), record(std::move(opening)), requested_transaction(std::move(requested)) {
	auto* database = app_coordinator->database();
	connect(this,     &AccountPage::save_account_requested,   database, &AppDatabase::save_account);
	connect(this,     &AccountPage::delete_requested,         database, &AppDatabase::save_transaction);
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

		auto* home_button = new QPushButton(this);
		home_button->setIcon(theme::colored_svg_icon(":/icons/home.svg", theme::text, theme::toolbar_icon_size));
		home_button->setIconSize(theme::label_icon_size(name_label));
		home_button->setToolTip(tr("Home"));
		connect(home_button, &QPushButton::clicked, this, &AccountPage::go_home);

		auto* import_button = new QPushButton(tr("Import OFX"), this);
		import_button->setIcon(theme::colored_svg_icon(":/icons/upload.svg", theme::text, theme::toolbar_icon_size));
		connect(import_button, &QPushButton::clicked, this, [this]() {
			auto* dialog = new ImportDialog(app_coordinator, this);
			dialog->setAttribute(Qt::WA_DeleteOnClose);
			connect(dialog, &QDialog::accepted, this, &AccountPage::fetch_history);
			dialog->show();
		});

		auto* new_transaction_button = new QPushButton(tr("New Transaction"), this);
		new_transaction_button->setIcon(theme::colored_svg_icon(":/icons/plus.svg", theme::text, theme::toolbar_icon_size));
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

		auto today = QDateTime::currentDateTime();
		if (requested_transaction) {
			auto effective_date = requested_transaction->date_recorded;
			if (requested_transaction->date_reconciled) {
				effective_date = *requested_transaction->date_reconciled;
			}
			if (requested_transaction->date_cleared) {
				effective_date = *requested_transaction->date_cleared;
			}
			today = QDateTime::fromMSecsSinceEpoch(effective_date.milliseconds_since_epoch);
		}

		auto* after_label = new QLabel(tr("From"), this);
		after_picker = new DatePicker(today.addMonths(-1), this);
		connect(after_picker, &DatePicker::updated, this, [this](){
			if (loading_preset_date_range) { return; }
			fetch_history();
		});

		auto* before_label = new QLabel(tr("Until"), this);
		before_picker = new DatePicker(today, this);
		connect(before_picker, &DatePicker::updated, this, [this](){
			if (loading_preset_date_range) { return; }
			fetch_history();
		});

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
		history_panel = new QWidget();
		history_layout = new QVBoxLayout(history_panel);
		history_layout->setContentsMargins(0, 0, 0, 0);
		history_layout->setSpacing(0);
		layout->addWidget(history_panel, 1);

		fetch_history();
	}
}

void AccountPage::rename(QString name) {
	previous_name = record.name;
	previous_closed_at = record.closed_at;
	record.name = name.toStdString();
	emit save_account_requested(record);
}

void AccountPage::new_transaction() {
	open_transaction({
		.record = {
			.account_id = record.id(),
			.date_recorded = fundos::datetime{QDateTime::currentDateTime().toMSecsSinceEpoch()}
		}
	});
}
void AccountPage::open_transaction(const fundos::db::transaction_history::allocated_transaction& opening) {
	auto* dialog = new TransactionDialog(app_coordinator, opening, this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	connect(dialog, &QDialog::accepted, this, &AccountPage::fetch_history);
	connect(dialog, &QDialog::rejected, this, &AccountPage::update_backgrounds);
	dialog->show();
}

void AccountPage::update_open_close_button() {
	if (record.closed_at.has_value()) {
		open_close_button->setText(tr("Open Account"));
		open_close_button->setIcon(theme::colored_svg_icon(":/icons/lock-open.svg", theme::success_foreground, theme::toolbar_icon_size));
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
		open_close_button->setIcon(theme::colored_svg_icon(":/icons/lock.svg", theme::error_foreground, theme::toolbar_icon_size));
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

void AccountPage::on_transaction_deleted(fundos::db::outcome saved) {
	disconnect(app_coordinator->database(), &AppDatabase::transaction_saved, this, &AccountPage::on_transaction_deleted);
	if (saved) {
		fetch_history();
	} else {
		update_backgrounds();
	}
}

void AccountPage::clear_history() {
	transaction_widgets.clear();
	while (QLayoutItem* item = history_layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}
}

void AccountPage::update_backgrounds() {
	for (auto& widget : transaction_widgets) {
		widget.background_widget->setStyleSheet(QStringLiteral(
			"background-color: %1; border: 1px solid %2"
		).arg(widget.background_color.name(), theme::separator.name()));
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

	fundos::datetime after  = {after_picker->get_date().startOfDay().toMSecsSinceEpoch()};
	fundos::datetime before = {before_picker->get_date().endOfDay().toMSecsSinceEpoch()};
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
	transaction_widgets.reserve(history.transactions.size()); // Make sure references are stable while constructing the transaction list

	auto* table = new TableView(true, this);
	table->set_header_vertical_padding(8);
	table->add_header_label(0, QStringLiteral(""));
	table->add_header_label(1, tr("Date"));
	table->add_header_label(2, tr("Memo"));
	table->add_header_label(3, tr("Amount"));
	table->add_header_label(4, tr("Balance"));
	table->add_header_label(5, QStringLiteral(""));

	table->body_layout()->setColumnStretch(0, 0);
	table->body_layout()->setColumnStretch(1, 1);
	table->body_layout()->setColumnStretch(2, 5);
	table->body_layout()->setColumnStretch(3, 1);
	table->body_layout()->setColumnStretch(4, 1);
	table->body_layout()->setColumnStretch(5, 0);

	history_layout->addWidget(table);

	auto* footer = new QWidget(this);
	auto* footer_layout = new QHBoxLayout(footer);
	footer_layout->setContentsMargins(0, 0, 0, 0);

	auto add_legend_item = [&](const QString& icon_path, const QString& label_text) {
		auto* icon_label = new QLabel(footer);
		icon_label->setPixmap(theme::colored_svg(icon_path, theme::text, theme::default_icon_size()));

		auto* text_label = new QLabel(label_text, footer);

		footer_layout->addWidget(icon_label);
		footer_layout->addWidget(text_label);
		footer_layout->addSpacing(12);
	};

	add_legend_item(":/icons/building-bank.svg", tr("Bank Cleared"));
	add_legend_item(":/icons/writing.svg",       tr("Reconciled"));
	add_legend_item(":/icons/clock.svg",         tr("Pending"));

	footer_layout->addStretch();
	table->set_footer(footer);

	int row = 1;
	auto add_transaction = [&](fundos::db::transaction_history::allocated_transaction& transaction) -> void {
		transaction_widgets.push_back(Transaction{});
		auto* widget = &transaction_widgets.back();
		widget->record = transaction;

		widget->background_color = widget->record.allocations.empty() ? theme::warning_background : theme::surface;
		if (requested_transaction && widget->record.record.id() == requested_transaction->id()) {
			widget->background_color = theme::info_background;
		}
		widget->background_widget = new QWidget(table);
		widget->background_widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
		widget->background_widget->setMinimumSize(0, 0);
		widget->background_widget->lower();

		widget->icon_path = ":/icons/clock.svg";
		if (widget->record.record.date_reconciled) { widget->icon_path = ":/icons/writing.svg"; }
		if (widget->record.record.date_cleared)    { widget->icon_path = ":/icons/building-bank.svg"; }

		auto* icon_container = new QWidget(table);
		auto* icon_container_layout = new QHBoxLayout(icon_container);
		icon_container_layout->setContentsMargins(8, 8, 8, 8);
		icon_container_layout->setAlignment(Qt::AlignCenter);

		widget->icon = new QLabel(icon_container);
		widget->icon->setPixmap(theme::colored_svg(widget->icon_path, theme::text, theme::default_icon_size()));
		icon_container_layout->addWidget(widget->icon);

		widget->date = new QLabel(
			QLocale::system().toString(
				QDateTime::fromMSecsSinceEpoch(widget->record.effective_date.milliseconds_since_epoch).date(),
				QLocale::ShortFormat
			),
			table
		);

		widget->memo = new QLabel(QString::fromStdString(widget->record.record.memo), table);

		widget->amount = theme::currency_label(widget->record.record.amount, app_coordinator->context()->currency_locale().info(), table);

		widget->balance = theme::currency_label(widget->record.account_balance, app_coordinator->context()->currency_locale().info(), table);

		static constexpr QLatin1StringView unchecked_icon_path{":/icons/chevron-down.svg"};
		static constexpr QLatin1StringView checked_icon_path{":/icons/chevron-up.svg"};

		widget->details_button = new QToolButton(table);
		widget->details_button->setStyleSheet("QToolButton:checked { background: transparent; border: none; }");
		widget->details_button->setCheckable(true);
		widget->details_button->setChecked(false);
		widget->details_button->setIcon(theme::colored_svg_icon(unchecked_icon_path, theme::text, theme::toolbar_icon_size));
		widget->details_button->setIconSize(theme::label_icon_size(widget->memo));
		widget->details_button->setAutoRaise(true);
		widget->details_button->setFixedSize(widget->details_button->sizeHint());

		connect(widget->details_button, &QToolButton::toggled, this, [this, widget, table](bool checked) {
			const auto& path = checked ? checked_icon_path : unchecked_icon_path;
			widget->details_button->setIcon(theme::colored_svg_icon(path, theme::text, theme::toolbar_icon_size));
			if (widget->details_widget) {
				widget->details_widget->setVisible(checked);
			} else {
				widget->details_widget = new QWidget(table);
				auto* details_layout = new QVBoxLayout(widget->details_widget);

				auto* allocations_widget = new QWidget(widget->details_widget);
				auto* allocations_grid = new QGridLayout(allocations_widget);
				allocations_grid->setContentsMargins(0, 0, 0, 0);
				allocations_grid->setSpacing(0);
				allocations_grid->setColumnStretch(0, 0);
				allocations_grid->setColumnMinimumWidth(0, 25);
				allocations_grid->setColumnStretch(1, 0);
				allocations_grid->setColumnStretch(2, 0);
				allocations_grid->setColumnMinimumWidth(2, 50);
				allocations_grid->setColumnStretch(3, 0);
				allocations_grid->setColumnStretch(4, 1);

				int allocation_row = 0;
				for (auto& allocation : widget->record.allocations) {
					auto* background = new QWidget(allocations_widget);
					background->setStyleSheet(QStringLiteral(
						"background-color: %1; border: 1px solid %2"
					).arg(allocation_row % 2 ? theme::background.name() : theme::background.lighter(110).name(), theme::separator.name()));
					background->lower();

					auto* fund = app_coordinator->context()->fund(allocation.fund_id);
					QString fund_name = tr("Fund id: %1").arg(QString::number(allocation.fund_id));
					if (fund != nullptr) { fund_name = QString::fromStdString(fund->name); }

					auto* fund_label = new QLabel(fund_name, allocations_widget);
					auto* fund_amount = theme::currency_label(allocation.amount, app_coordinator->context()->currency_locale().info(), allocations_widget);

					fund_amount->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
					fund_label->setContentsMargins(8, 4, 0, 4);
					fund_amount->setContentsMargins(0, 4, 8, 4);

					allocations_grid->addWidget(background,  allocation_row, 1, 1, 3);
					allocations_grid->addWidget(fund_label,  allocation_row, 1, 1, 1);
					allocations_grid->addWidget(fund_amount, allocation_row, 3, 1, 1);
					++allocation_row;
				}
				if (allocation_row == 0) {
					auto* empty_label = new QLabel(tr("No allocations yet"), allocations_widget);
					auto empty_label_font = empty_label->font();
					empty_label_font.setItalic(true);
					empty_label->setFont(empty_label_font);
					allocations_grid->addWidget(empty_label, allocation_row, 1, 1, 1);
				}

				details_layout->addWidget(allocations_widget);

				auto* details_actions = new QWidget(widget->details_widget);
				auto* details_actions_layout = new QHBoxLayout(details_actions);
				details_actions_layout->setAlignment(Qt::AlignLeft);

				auto* edit_action = new QPushButton(tr("Edit"), details_actions);
				edit_action->setIcon(theme::colored_svg_icon(":/icons/edit.svg", theme::text, theme::toolbar_icon_size));
				details_actions_layout->addWidget(edit_action);
				connect(edit_action, &QPushButton::clicked, this, [this, widget]() {
					widget->background_widget->setStyleSheet(QStringLiteral(
						"background-color: %1; border: 1px solid %2"
					).arg(theme::info_background.name(), theme::separator.name()));
					open_transaction(widget->record);
				});

				if (!widget->record.record.fitid) {
					auto* correct_action = new QPushButton(tr("Make Correction"), details_actions);
					correct_action->setIcon(theme::colored_svg_icon(":/icons/replace.svg", theme::text, theme::toolbar_icon_size));
					details_actions_layout->addWidget(correct_action);
					connect(correct_action, &QPushButton::clicked, this, [this, widget]() {
						widget->background_widget->setStyleSheet(QStringLiteral(
							"background-color: %1; border: 1px solid %2"
						).arg(theme::info_background.name(), theme::separator.name()));
						fundos::db::transaction_history::allocated_transaction correction;
						correction.record.corrects_id = widget->record.record.id();
						correction.record.correct_action = fundos::transaction::correction_type::replaces;
						correction.record.account_id = widget->record.record.account_id;
						correction.record.date_recorded = widget->record.record.date_recorded;
						correction.record.memo = widget->record.record.memo;
						correction.record.amount = widget->record.record.amount;

						open_transaction(correction);
					});

					auto* delete_action = new QPushButton(tr("Delete Transaction"), details_actions);
					delete_action->setIcon(theme::colored_svg_icon(":/icons/trash.svg", theme::text, theme::toolbar_icon_size));
					details_actions_layout->addWidget(delete_action);
					connect(delete_action, &QPushButton::clicked, this, [this, widget]() {
						widget->background_widget->setStyleSheet(QStringLiteral(
							"background-color: %1; border: 1px solid %2"
						).arg(theme::info_background.name(), theme::separator.name()));

						QMessageBox dialog(this);
						dialog.setWindowTitle(tr("Delete Transaction"));
						dialog.setText(tr("Delete \"%1\"?").arg(widget->record.record.memo));
						dialog.setInformativeText(tr("This transaction will be permanently removed from your register."));
						dialog.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
						dialog.setDefaultButton(QMessageBox::Cancel);
						dialog.button(QMessageBox::Ok)->setText(tr("Delete"));

						if (dialog.exec() != QMessageBox::Ok) {
							update_backgrounds();
							return;
						}

						fundos::transaction correction;
						correction.corrects_id = widget->record.record.id();
						correction.correct_action = fundos::transaction::correction_type::deletes;
						correction.account_id = widget->record.record.account_id;
						correction.date_recorded = widget->record.record.date_recorded;
						correction.memo = widget->record.record.memo;
						std::vector<fundos::allocation> allocations;

						connect(app_coordinator->database(), &AppDatabase::transaction_saved, this, &AccountPage::on_transaction_deleted);
						emit delete_requested(correction, allocations);
					});
				}

				details_layout->addWidget(details_actions);
				table->body_layout()->addWidget(widget->details_widget, widget->details_row, 0, 1, 6);
			}
		});

		table->body_layout()->addWidget(widget->background_widget, row, 0, 1, 6);
		table->body_layout()->addWidget(icon_container,            row, 0, 1, 1);
		table->body_layout()->addWidget(widget->date,              row, 1, 1, 1);
		table->body_layout()->addWidget(widget->memo,              row, 2, 1, 1);
		table->body_layout()->addWidget(widget->amount,            row, 3, 1, 1);
		table->body_layout()->addWidget(widget->balance,           row, 4, 1, 1);
		table->body_layout()->addWidget(widget->details_button,    row, 5, 1, 1);
		widget->details_row = ++row;
		++row;
	};
	auto add_ledger_balance = [&](fundos::import_ledger_balance& ledger_balance, std::optional<fundos::currency> balance_checker) -> void {
		const bool ledger_contradicted = balance_checker && *balance_checker != ledger_balance.amount;

		const auto background_color = ledger_contradicted ? theme::error_background : theme::success_background;
		auto* background_widget = new QWidget(table);
		background_widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
		background_widget->setMinimumSize(0, 0);
		background_widget->lower();
		background_widget->setStyleSheet(QStringLiteral(
			"background-color: %1; border: 1px solid %2"
		).arg(background_color.name(), theme::separator.name()));

		auto* icon_container = new QWidget(table);
		auto* icon_container_layout = new QHBoxLayout(icon_container);
		icon_container_layout->setContentsMargins(8, 8, 8, 8);
		icon_container_layout->setAlignment(Qt::AlignCenter);

		auto* icon = new QLabel(icon_container);
		icon_container_layout->addWidget(icon);

		auto* date = new QLabel(
			QLocale::system().toString(
				QDateTime::fromMSecsSinceEpoch(ledger_balance.date_as_of.milliseconds_since_epoch).date(),
				QLocale::ShortFormat
			),
			table
		);

		auto* memo = new QLabel(tr("Bank Balance"), table);

		auto* amount = ledger_contradicted
			? theme::currency_label(ledger_balance.amount - *balance_checker, app_coordinator->context()->currency_locale().info(), table)
			: new QLabel("", table);

		if (ledger_contradicted) {
			memo->setText(tr("Bank Balance — Discrepancy"));
			amount->setToolTip(tr(
				"Discrepancy between your last reconciled balance and the balance reported by your bank.\n"
				"A negative value means the bank's figure is lower than your register.\n"
				"Reconcile transactions to resolve this discrepancy."
			));
		}

		auto* balance = theme::currency_label(ledger_balance.amount, app_coordinator->context()->currency_locale().info(), table);
		balance->setToolTip(tr("Balance reported by your bank at the time of this import."));

		table->body_layout()->addWidget(background_widget, row, 0, 1, 6);
		table->body_layout()->addWidget(icon_container,    row, 0, 1, 1);
		table->body_layout()->addWidget(date,              row, 1, 1, 1);
		table->body_layout()->addWidget(memo,              row, 2, 1, 1);
		table->body_layout()->addWidget(amount,            row, 3, 1, 1);
		table->body_layout()->addWidget(balance,           row, 4, 1, 1);
		++row;
	};

	size_t tx_index = 0;
	size_t lb_index = 0;
	table->setUpdatesEnabled(false);
	while (tx_index < history.transactions.size() || lb_index < history.ledger_balances.size()) {
		if (tx_index >= history.transactions.size()) {
			add_ledger_balance(history.ledger_balances[lb_index++], std::nullopt);
			continue;
		}
		if (lb_index >= history.ledger_balances.size()) {
			add_transaction(history.transactions[tx_index++]);
			continue;
		}
		auto& transaction = history.transactions[tx_index];
		auto& ledger_balance = history.ledger_balances[lb_index];

		// TODO: decide tie-breaking order when ledger_balance.date_as_of == transaction.effective_date
		if (ledger_balance.date_as_of > transaction.effective_date) {
			std::optional<fundos::currency> next_balance = std::nullopt;
			size_t search = tx_index;
			while (search < history.transactions.size()) {
				auto& found = history.transactions[search];
				if (found.record.date_cleared || found.record.date_reconciled) {
					next_balance = found.account_balance;
					break;
				}
				++search;
			}
			add_ledger_balance(history.ledger_balances[lb_index++], next_balance);
		} else {
			add_transaction(history.transactions[tx_index++]);
		}
	}
	table->setUpdatesEnabled(true);
	update_backgrounds();
}
