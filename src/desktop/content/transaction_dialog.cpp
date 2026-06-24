#include <algorithm>
#include "transaction_dialog.hpp"
#include "theme.hpp"
#include <QDateTime>
#include <QHBoxLayout>
#include <QLocale>
#include <QMessageBox>

TransactionDialog::TransactionDialog(
	AppCoordinator*       coordinator,
	allocated_transaction opening,
	QWidget*              parent
) : QDialog(parent), app_coordinator(coordinator), current_allocated_transaction(std::move(opening)) {
	setWindowTitle(current_allocated_transaction.record.is_persisted()
		? tr("Edit transaction")
		: current_allocated_transaction.record.corrects_id
			? tr("Correct transaction")
			: tr("Create transaction")
	);

	const auto& locale = app_coordinator->context()->currency_locale().info();

	for (const auto& existing : current_allocated_transaction.allocations) {
		adjusted_balances[existing.fund_id] = fundos::currency{0};
		current_allocations.push_back(existing);
	}

	auto* outer = new QGridLayout(this);
	outer->setContentsMargins(8, 8, 8, 8);
	outer->setColumnStretch(0, 1);
	outer->setColumnStretch(1, 1);
	outer->setSizeConstraint(QLayout::SetFixedSize);

	int row = 0;

	date_recorded_picker = new DatePicker(QDateTime::fromMSecsSinceEpoch(current_allocated_transaction.record.date_recorded.milliseconds_since_epoch), this);

	const QString amount_text = QString::fromStdString(current_allocated_transaction.record.amount.to_string(locale));
	amount_field = new QLineEdit(amount_text, this);
	amount_field->installEventFilter(this);
	amount_field->setEnabled(!current_allocated_transaction.record.is_persisted());

	outer->addWidget(new QLabel(tr("Date recorded"), this), row, 0);
	outer->addWidget(new QLabel(tr("Amount"),        this), row, 1);
	++row;
	outer->addWidget(date_recorded_picker, row, 0);
	outer->addWidget(amount_field,         row, 1);
	++row;

	memo_field = new QLineEdit(QString::fromStdString(current_allocated_transaction.record.memo), this);
	outer->addWidget(new QLabel(tr("Memo"), this), row, 0, 1, 2);
	++row;
	outer->addWidget(memo_field, row, 0, 1, 2);
	++row;

	auto* reconciled_widget = new QWidget(this);
	auto* reconciled_layout = new QHBoxLayout(reconciled_widget);
	reconciled_layout->setContentsMargins(0, 0, 0, 0);

	reconciled_checkbox = new QCheckBox(this);
	date_reconciled_picker = new DatePicker(QDateTime::currentDateTime(), this);

	reconciled_layout->addWidget(reconciled_checkbox);
	reconciled_layout->addWidget(date_reconciled_picker);

	if (current_allocated_transaction.record.date_reconciled.has_value()) {
		reconciled_checkbox->setChecked(true);
		date_reconciled_picker->set_value(
			QDateTime::fromMSecsSinceEpoch(
				current_allocated_transaction.record.date_reconciled->milliseconds_since_epoch
			)
		);
	}

	const QString cleared_text = current_allocated_transaction.record.date_cleared.has_value()
		? QLocale::system().toString(
			QDateTime::fromMSecsSinceEpoch(current_allocated_transaction.record.date_cleared->milliseconds_since_epoch).date(),
			QLocale::ShortFormat
		)
		: tr("Not imported");

	date_cleared_label = new QLabel(cleared_text, this);

	outer->addWidget(new QLabel(tr("Reconciled"),   this), row, 0);
	outer->addWidget(new QLabel(tr("Date cleared"), this), row, 1);
	++row;
	outer->addWidget(reconciled_widget,  row, 0);
	outer->addWidget(date_cleared_label, row, 1);
	++row;

	outer->addWidget(new QLabel(tr("Justified by"), this), row, 0, 1, 2);
	++row;

	justified_by_combo = new QComboBox(this);
	justified_by_combo->setItemDelegate(new FundComboDelegate(locale, justified_by_combo));
	populate_justified_by_combo();
	outer->addWidget(justified_by_combo, row, 0, 1, 2);
	++row;

	allocation_table = new TableView(false, this);
	allocation_table->set_column_padding(8);
	allocation_table->set_header_vertical_padding(6);
	allocation_table->set_body_vertical_padding(8);
	allocation_table->set_row_spacing(6);
	allocation_table->add_header_label(0, tr("Fund"));
	allocation_table->add_header_label(1, tr("Amount"));
	allocation_table->add_header_label(2, QStringLiteral(""));
	allocation_table->body_layout()->setColumnStretch(0, 1);

	allocation_total_label = new QLabel(this);
	allocation_total_label->setAlignment(Qt::AlignRight);
	QFont total_font = allocation_total_label->font();
	total_font.setBold(true);
	allocation_total_label->setFont(total_font);
	allocation_table->set_footer(allocation_total_label);

	outer->addWidget(allocation_table, row, 0, 1, 2);
	++row;
	rebuild_allocation_table();

	auto* add_row_widget = new QWidget(this);
	auto* add_row_layout = new QHBoxLayout(add_row_widget);
	add_row_layout->setContentsMargins(0, 0, 0, 0);
	outer->addWidget(add_row_widget, row, 0, 1, 2);
	++row;

	add_fund_combo = new QComboBox(this);
	add_fund_combo->setItemDelegate(new FundComboDelegate(locale, add_fund_combo));
	add_fund_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	add_fund_button = new QPushButton(tr("Add row"), this);
	add_fund_button->setEnabled(false);
	populate_add_fund_combo();

	add_row_layout->addWidget(add_fund_combo, 1);
	add_row_layout->addWidget(add_fund_button);

	auto* button_row = new QWidget(this);
	auto* button_layout = new QHBoxLayout(button_row);
	outer->addWidget(button_row, row, 0, 1, 2);

	auto* cancel_button = new QPushButton(tr("Cancel"), this);
	save_button = new QPushButton(tr("Save"), this);

	button_layout->addStretch();
	button_layout->addWidget(cancel_button);
	button_layout->addWidget(save_button);

	auto* database = app_coordinator->database();
	connect(this,     &TransactionDialog::request_fund_balance, database, &AppDatabase::request_fund_balance);
	connect(this,     &TransactionDialog::save_requested,       database, &AppDatabase::save_transaction);
	connect(database, &AppDatabase::fund_balance_received,      this,     &TransactionDialog::on_balance_received);
	connect(database, &AppDatabase::transaction_saved,          this,     &TransactionDialog::on_save_completed);

	connect(amount_field, &QLineEdit::editingFinished, this, [this]() {
		const auto& locale = app_coordinator->context()->currency_locale().info();
		const auto parsed = fundos::currency::from_string(
			amount_field->text().toStdString(),
			locale
		);
		if (!parsed.has_value()) {
			amount_field->setText(QString::fromStdString(
				current_allocated_transaction.record.amount.to_string(locale)
			));
			return;
		}
		current_allocated_transaction.record.amount = *parsed;
		amount_field->setText(QString::fromStdString(parsed->to_string(locale)));
		apply_justification(justified_by_combo->currentIndex());
		update_allocation_total();
	});

	connect(justified_by_combo, &QComboBox::currentIndexChanged, this, &TransactionDialog::apply_justification);
	connect(add_fund_combo,     &QComboBox::currentIndexChanged, this, [this](int index) {
		add_fund_button->setEnabled(index > 0);
	});
	connect(add_fund_button, &QPushButton::clicked, this, &TransactionDialog::on_add_fund_clicked);
	connect(cancel_button,   &QPushButton::clicked, this, &QDialog::reject);
	connect(save_button,     &QPushButton::clicked, this, &TransactionDialog::on_save_clicked);

	connect(reconciled_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
		date_reconciled_picker->setEnabled(checked);
	});
	date_reconciled_picker->setEnabled(reconciled_checkbox->isChecked());

	for (const auto& fund : app_coordinator->context()->funds()) {
		emit request_fund_balance(fund.id());
	}
}

bool TransactionDialog::eventFilter(QObject* object, QEvent* event) {
	if (event->type() != QEvent::FocusIn) {
		return QDialog::eventFilter(object, event);
	}
	auto* field = qobject_cast<QLineEdit*>(object);
	if (field != nullptr && field != memo_field) {
		QMetaObject::invokeMethod(field, "selectAll", Qt::QueuedConnection);
	}
	return false;
}

void TransactionDialog::populate_justified_by_combo() {
	auto* model = new QStandardItemModel(justified_by_combo);

	auto* custom_item = new QStandardItem(tr("Custom"));
	model->appendRow(custom_item);

	auto* fund_header = new FundComboHeader(tr("By fund"));
	model->appendRow(fund_header);

	for (const auto& fund : app_coordinator->context()->funds()) {
		auto* item = new FundComboItem(QString::fromStdString(fund.name), fund.id());
		if (adjusted_balances.count(fund.id())) {
			item->balance = adjusted_balances.at(fund.id());
		}
		model->appendRow(item);
	}

	auto* budget_header = new FundComboHeader(tr("With budget"));
	model->appendRow(budget_header);

	for (const auto& budget : app_coordinator->context()->budgets()) {
		auto* item = new QStandardItem(QString::fromStdString(budget.name));
		item->setData(QVariant::fromValue(budget.id()), Qt::UserRole);
		model->appendRow(item);
	}

	justified_by_combo->setModel(model);
}

void TransactionDialog::populate_add_fund_combo() {
	auto* model = new QStandardItemModel(add_fund_combo);

	auto* placeholder = new QStandardItem(tr("Select a fund to add..."));
	placeholder->setFlags(Qt::NoItemFlags);
	model->appendRow(placeholder);

	for (const auto& fund : app_coordinator->context()->funds()) {
		const bool already_added = std::any_of(
			current_allocations.begin(),
			current_allocations.end(),
			[&](const fundos::allocation row) { return row.fund_id == fund.id(); }
		);
		if (already_added) {
			continue;
		}
		auto* item = new FundComboItem(QString::fromStdString(fund.name), fund.id());
		if (adjusted_balances.count(fund.id())) {
			item->balance = adjusted_balances.at(fund.id());
		}
		model->appendRow(item);
	}

	add_fund_combo->setModel(model);
	add_fund_combo->setCurrentIndex(0);
	add_fund_button->setEnabled(false);
}

void TransactionDialog::rebuild_allocation_table() {
	for (int grid_row = allocation_table->body_layout()->rowCount() - 1; grid_row >= 1; --grid_row) {
		for (int column = 0; column < 3; ++column) {
			QLayoutItem* item = allocation_table->body_layout()->itemAtPosition(grid_row, column);
			if (item == nullptr) {
				continue;
			}
			QWidget* widget = item->widget();
			allocation_table->body_layout()->removeWidget(widget);
			delete widget;
		}
	}

	const bool editable = current_justification == justification::custom;
	int grid_row = 1;
	for (const auto& row : current_allocations) {
		add_allocation_row(row, editable, grid_row);
		++grid_row;
	}
	update_allocation_total();
	adjustSize();
}

void TransactionDialog::update_allocation_total() {
	const auto& locale = app_coordinator->context()->currency_locale().info();

	fundos::currency total{0};
	for (const auto& row : current_allocations) {
		total += row.amount;
	}

	const fundos::currency remaining = current_allocated_transaction.record.amount - total;
	const QString color = remaining.minor_units == 0
		? theme::success_foreground.name()
		: theme::error_foreground.name();

	allocation_total_label->setText(QStringLiteral(
		"<span>%1 / %2</span> <i style='color: %3'>(%4 remaining)</i>"
	).arg(
		QString::fromStdString(total.to_string(locale)),
		QString::fromStdString(current_allocated_transaction.record.amount.to_string(locale)),
		color,
		QString::fromStdString(remaining.to_string(locale))
	));
	allocation_total_label->setTextFormat(Qt::RichText);
}

void TransactionDialog::add_allocation_row(const fundos::allocation& row, bool editable, int grid_row) {
	const auto& locale = app_coordinator->context()->currency_locale().info();

	const auto found = app_coordinator->context()->fund(row.fund_id);
	const QString fund_name = found
		? QString::fromStdString(found->name)
		: tr("Unknown fund");

	auto* name_label = new QLabel(allocation_table->body_container());

	if (adjusted_balances.count(row.fund_id)) {
		const fundos::currency balance = adjusted_balances.at(row.fund_id);
		const QString balance_text = QString::fromStdString(balance.to_string(locale));
		const QString color = balance.minor_units < 0
			? QStringLiteral("color: %1").arg(theme::error_foreground.name())
			: QStringLiteral("color: %1").arg(theme::success_foreground.name());
		name_label->setText(QStringLiteral("%1 <i style='%2'>%3</i>").arg(fund_name, color, balance_text));
		name_label->setTextFormat(Qt::RichText);
	} else {
		name_label->setText(fund_name);
	}

	auto* allocation_amount_field = new QLineEdit(
		QString::fromStdString(row.amount.to_string(locale)),
		allocation_table->body_container()
	);
	allocation_amount_field->setEnabled(editable);
	allocation_amount_field->installEventFilter(this);

	connect(allocation_amount_field, &QLineEdit::editingFinished, this, [this, allocation_amount_field, row]() mutable {
		const auto& locale = app_coordinator->context()->currency_locale().info();
		const auto parsed = fundos::currency::from_string(
			allocation_amount_field->text().toStdString(),
			locale
		);
		auto amount = row.amount;
		if (parsed.has_value()) { amount = *parsed; }
		allocation_amount_field->setText(QString::fromStdString(amount.to_string(locale)));
		for (auto& allocation : current_allocations) {
			if (allocation.fund_id == row.fund_id) {
				allocation.amount = amount;
				break;
			}
		}
		update_allocation_total();
	});

	auto* remove_button = new QPushButton(allocation_table->body_container());
	remove_button->setIcon(theme::colored_svg_icon(":/icons/trash.svg", theme::text, theme::toolbar_icon_size));
	remove_button->setVisible(editable);
	remove_button->setFixedWidth(28);

	allocation_table->body_layout()->addWidget(name_label,              grid_row, 0);
	allocation_table->body_layout()->addWidget(allocation_amount_field, grid_row, 1);
	allocation_table->body_layout()->addWidget(remove_button,           grid_row, 2);

	const int64_t fund_id = row.fund_id;
	connect(remove_button, &QPushButton::clicked, this, [this, fund_id] {
		current_allocations.erase(
			std::remove_if(
				current_allocations.begin(),
				current_allocations.end(),
				[fund_id](const fundos::allocation& record) { return record.fund_id == fund_id; }
			),
			current_allocations.end()
		);
		rebuild_allocation_table();
		populate_add_fund_combo();
	});

	connect(allocation_amount_field, &QLineEdit::textChanged, this, [this, fund_id](const QString& text) {
		const auto& locale = app_coordinator->context()->currency_locale().info();
		const auto parsed = fundos::currency::from_string(text.toStdString(), locale);
		if (!parsed.has_value()) {
			return;
		}
		for (auto& allocation : current_allocations) {
			if (allocation.fund_id == fund_id) {
				allocation.amount = *parsed;
				break;
			}
		}
	});

	adjustSize();
}

void TransactionDialog::apply_justification(int combo_index) {
	const auto* model = static_cast<const QStandardItemModel*>(justified_by_combo->model());
	const auto* standard_item = model->item(combo_index);

	if (combo_index == 0) {
		current_justification = justification::custom;
		rebuild_allocation_table();
		add_fund_combo->setVisible(true);
		add_fund_button->setVisible(true);
		return;
	}

	if (dynamic_cast<const FundComboHeader*>(standard_item) != nullptr) {
		return;
	}

	if (const auto* fund_item = dynamic_cast<const FundComboItem*>(standard_item)) {
		current_justification = justification::by_fund;
		current_allocations.clear();
		fundos::allocation allocation;
		allocation.fund_id = fund_item->fund_id;
		allocation.amount  = current_allocated_transaction.record.amount;
		current_allocations.push_back(allocation);
		rebuild_allocation_table();
		add_fund_combo->setVisible(false);
		add_fund_button->setVisible(false);
		return;
	}

	const int64_t budget_id = standard_item->data(Qt::UserRole).toLongLong();
	const auto budget = app_coordinator->context()->budget(budget_id);
	if (!budget) { return; }

	current_justification = justification::by_budget;
	current_allocations = budget->apply(current_allocated_transaction.record, adjusted_balances);
	rebuild_allocation_table();
	add_fund_combo->setVisible(false);
	add_fund_button->setVisible(false);
}

void TransactionDialog::on_add_fund_clicked() {
	const auto* model = static_cast<const QStandardItemModel*>(add_fund_combo->model());
	const int index = add_fund_combo->currentIndex();
	const auto* fund_item = dynamic_cast<const FundComboItem*>(model->item(index));
	if (fund_item == nullptr) {
		return;
	}

	fundos::allocation allocation;
	allocation.fund_id = fund_item->fund_id;
	allocation.amount  = fundos::currency{0};
	current_allocations.push_back(allocation);
	rebuild_allocation_table();
	populate_add_fund_combo();
}

void TransactionDialog::on_balance_received(int64_t fund_id, fundos::db::result<fundos::currency> result) {
	if (!result) { return; }
	auto balance = result.value();
	fundos::currency adjusted = balance;
	for (const auto& existing : current_allocated_transaction.allocations) {
		if (existing.fund_id == fund_id) {
			adjusted += existing.amount;
			break;
		}
	}
	adjusted_balances[fund_id] = adjusted;

	populate_justified_by_combo();
	populate_add_fund_combo();
	rebuild_allocation_table();
}

void TransactionDialog::on_save_clicked() {
	const auto& locale = app_coordinator->context()->currency_locale().info();

	fundos::transaction saving;
	saving = current_allocated_transaction.record;
	saving.memo = memo_field->text().toStdString();
	saving.date_recorded = fundos::datetime{
		date_recorded_picker->get_value().toMSecsSinceEpoch()
	};

	if (!current_allocated_transaction.record.is_persisted()) {
		const auto parsed_amount = fundos::currency::from_string(amount_field->text().toStdString(), locale);
		if (!parsed_amount.has_value()) { return; }
		saving.amount = *parsed_amount;
	}

	if (reconciled_checkbox->isChecked()) {
		saving.date_reconciled = fundos::datetime{
			date_reconciled_picker->get_value().toMSecsSinceEpoch()
		};
	} else {
		saving.date_reconciled = std::nullopt;
	}

	fundos::currency allocated_total{0};
	for (const auto& allocation : current_allocations) {
		allocated_total += allocation.amount;
	}

	if (!current_allocations.empty() && allocated_total != saving.amount) {
		QMessageBox::warning(
			this,
			tr("Allocation mismatch"),
			tr("The allocated amount must equal the transaction amount before saving.")
		);
		return;
	}

	save_button->setEnabled(false);
	emit save_requested(saving, current_allocations);
}

void TransactionDialog::on_save_completed(fundos::db::outcome outcome) {
	save_button->setEnabled(true);
	if (outcome) {
		accept();
	}
}
