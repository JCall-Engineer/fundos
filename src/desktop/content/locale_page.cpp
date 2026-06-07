#include "fundos.hpp"
#include "locale_page.hpp"
#include "theme.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>

using currency_spec     = fundos::currency_locale::spec;
using currency_locale   = fundos::currency_locale::selection;
using percentage_spec   = fundos::percentage_locale::spec;
using percentage_locale = fundos::percentage_locale::selection;

void LocalePage::setup_layout(bool can_cancel) {
	// Actions
	auto* actions        = new QWidget(this);
	auto* actions_layout = new QHBoxLayout(actions);
	actions_layout->setContentsMargins(8, 8, 8, 8);
	actions_layout->addStretch();

	if (can_cancel) {
		auto* cancel_button = new QPushButton(tr("&Cancel"), actions);
		connect(cancel_button, &QPushButton::clicked, this, &LocalePage::done);
		actions_layout->addWidget(cancel_button);
	}

	auto* confirm_button = new QPushButton(tr("&Save"), actions);
	connect(confirm_button, &QPushButton::clicked, this, &LocalePage::on_confirm);
	actions_layout->addWidget(confirm_button);

	// Form columns (currency + percentage side by side)
	auto* columns        = new QWidget(this);
	auto* columns_layout = new QHBoxLayout(columns);
	columns_layout->setContentsMargins(16, 16, 16, 16);
	columns_layout->setSpacing(24);
	columns_layout->setAlignment(Qt::AlignTop);

	// Currency column
	auto* currency_column        = new QWidget(columns);
	auto* currency_column_layout = new QVBoxLayout(currency_column);
	currency_column_layout->setContentsMargins(0, 0, 0, 0);
	currency_column_layout->setAlignment(Qt::AlignTop);

	currency_column_layout->addWidget(theme::header_label(tr("CURRENCY"), currency_column));

	currency_combo = new QComboBox(currency_column);
	currency_column_layout->addWidget(currency_combo);

	// Scrollable currency custom fields
	currency_custom_fields       = new QWidget();
	auto* currency_custom_layout = new QVBoxLayout(currency_custom_fields);
	currency_custom_layout->setContentsMargins(0, 8, 0, 8);
	currency_custom_layout->setSpacing(8);
	currency_custom_layout->setAlignment(Qt::AlignTop);

	currency_custom_layout->addWidget(theme::header_label(tr("SCALE"), currency_custom_fields));
	currency_scale = new QComboBox(currency_custom_fields);
	currency_scale->addItem("1",    QVariant::fromValue(1));
	currency_scale->addItem("10",   QVariant::fromValue(10));
	currency_scale->addItem("100",  QVariant::fromValue(100));
	currency_scale->addItem("1000", QVariant::fromValue(1000));
	currency_custom_layout->addWidget(currency_scale);

	currency_custom_layout->addWidget(theme::header_label(tr("SYMBOL"), currency_custom_fields));
	currency_symbol = new QLineEdit(currency_custom_fields);
	currency_symbol->setMaxLength(4);
	currency_custom_layout->addWidget(currency_symbol);

	currency_custom_layout->addWidget(theme::header_label(tr("THOUSANDS SEPARATOR"), currency_custom_fields));
	currency_thousands_separator = new QLineEdit(currency_custom_fields);
	currency_thousands_separator->setMaxLength(1);
	currency_custom_layout->addWidget(currency_thousands_separator);

	currency_custom_layout->addWidget(theme::header_label(tr("DECIMAL SEPARATOR"), currency_custom_fields));
	currency_decimal_separator = new QLineEdit(currency_custom_fields);
	currency_decimal_separator->setMaxLength(1);
	currency_custom_layout->addWidget(currency_decimal_separator);

	currency_custom_layout->addWidget(theme::header_label(tr("SYMBOL POSITION"), currency_custom_fields));
	currency_symbol_placement_group = new QButtonGroup(currency_custom_fields);
	auto* placement_before = new QRadioButton(tr("Before"), currency_custom_fields);
	auto* placement_after  = new QRadioButton(tr("After"),  currency_custom_fields);
	currency_symbol_placement_group->addButton(placement_before, static_cast<int>(currency_spec::symbol_placement::before));
	currency_symbol_placement_group->addButton(placement_after,  static_cast<int>(currency_spec::symbol_placement::after));
	currency_custom_layout->addWidget(placement_before);
	currency_custom_layout->addWidget(placement_after);

	currency_custom_layout->addWidget(theme::header_label(tr("NEGATIVE FORMAT"), currency_custom_fields));
	currency_negative_notation_group = new QButtonGroup(currency_custom_fields);
	auto* negative_leading_minus  = new QRadioButton(tr("Leading minus"),  currency_custom_fields);
	auto* negative_trailing_minus = new QRadioButton(tr("Trailing minus"), currency_custom_fields);
	auto* negative_parentheses    = new QRadioButton(tr("Parentheses"),    currency_custom_fields);
	auto* negative_angle_brackets = new QRadioButton(tr("Angle brackets"), currency_custom_fields);
	currency_negative_notation_group->addButton(negative_leading_minus,  static_cast<int>(currency_spec::negative_notation::leading_minus));
	currency_negative_notation_group->addButton(negative_trailing_minus, static_cast<int>(currency_spec::negative_notation::trailing_minus));
	currency_negative_notation_group->addButton(negative_parentheses,    static_cast<int>(currency_spec::negative_notation::parentheses));
	currency_negative_notation_group->addButton(negative_angle_brackets, static_cast<int>(currency_spec::negative_notation::angle_brackets));
	currency_custom_layout->addWidget(negative_leading_minus);
	currency_custom_layout->addWidget(negative_trailing_minus);
	currency_custom_layout->addWidget(negative_parentheses);
	currency_custom_layout->addWidget(negative_angle_brackets);

	auto* currency_scroll = new QScrollArea(currency_column);
	currency_scroll->setWidget(currency_custom_fields);
	currency_scroll->setWidgetResizable(true);
	currency_scroll->setFrameShape(QFrame::NoFrame);
	currency_column_layout->addWidget(currency_scroll, 1);

	currency_column_layout->addWidget(theme::header_label(tr("PREVIEW"), currency_column));
	currency_preview = new QLabel(currency_column);
	currency_column_layout->addWidget(currency_preview);

	columns_layout->addWidget(currency_column);

	// Percentage column
	auto* percentage_column        = new QWidget(columns);
	auto* percentage_column_layout = new QVBoxLayout(percentage_column);
	percentage_column_layout->setContentsMargins(0, 0, 0, 0);
	percentage_column_layout->setAlignment(Qt::AlignTop);

	percentage_column_layout->addWidget(theme::header_label(tr("PERCENTAGE"), percentage_column));

	percentage_combo = new QComboBox(percentage_column);
	percentage_column_layout->addWidget(percentage_combo);

	// Scrollable percentage custom fields
	percentage_custom_fields       = new QWidget();
	auto* percentage_custom_layout = new QVBoxLayout(percentage_custom_fields);
	percentage_custom_layout->setContentsMargins(0, 8, 0, 8);
	percentage_custom_layout->setSpacing(8);
	percentage_custom_layout->setAlignment(Qt::AlignTop);

	percentage_custom_layout->addWidget(theme::header_label(tr("DECIMAL SEPARATOR"), percentage_custom_fields));
	percentage_decimal_separator = new QLineEdit(percentage_custom_fields);
	percentage_decimal_separator->setMaxLength(1);
	percentage_custom_layout->addWidget(percentage_decimal_separator);

	percentage_has_space = new QCheckBox(tr("Space around number"), percentage_custom_fields);
	percentage_custom_layout->addWidget(percentage_has_space);

	percentage_custom_layout->addWidget(theme::header_label(tr("SYMBOL POSITION"), percentage_custom_fields));
	percentage_symbol_placement_group = new QButtonGroup(percentage_custom_fields);
	auto* percentage_placement_before = new QRadioButton(tr("Before"), percentage_custom_fields);
	auto* percentage_placement_after  = new QRadioButton(tr("After"),  percentage_custom_fields);
	percentage_symbol_placement_group->addButton(percentage_placement_before, static_cast<int>(percentage_spec::symbol_placement::before));
	percentage_symbol_placement_group->addButton(percentage_placement_after,  static_cast<int>(percentage_spec::symbol_placement::after));
	percentage_custom_layout->addWidget(percentage_placement_before);
	percentage_custom_layout->addWidget(percentage_placement_after);

	auto* percentage_scroll = new QScrollArea(percentage_column);
	percentage_scroll->setWidget(percentage_custom_fields);
	percentage_scroll->setWidgetResizable(true);
	percentage_scroll->setFrameShape(QFrame::NoFrame);
	percentage_column_layout->addWidget(percentage_scroll, 1);

	percentage_column_layout->addWidget(theme::header_label(tr("PREVIEW"), percentage_column));
	percentage_preview = new QLabel(percentage_column);
	percentage_column_layout->addWidget(percentage_preview);

	columns_layout->addWidget(percentage_column);

	// Outer layout
	auto* outer_layout = new QVBoxLayout(this);
	outer_layout->setContentsMargins(0, 0, 0, 0);
	outer_layout->addWidget(columns, 1);
	outer_layout->addWidget(actions);
}

LocalePage::LocalePage(
	std::shared_ptr<fundos::db>      db,
	std::optional<currency_locale>   current_currency,
	std::optional<percentage_locale> current_percentage,
	QWidget* parent
) : QWidget(parent), database(std::move(db)) {
	setup_layout(current_currency.has_value() && current_percentage.has_value());
	if (!current_currency.has_value()) {
		current_currency = currency_locale(&fundos::currency_locale::locales.named.USD);
	}
	if (!current_percentage.has_value()) {
		current_percentage = percentage_locale(&fundos::percentage_locale::locales.named.en);
	}
// Populate currency combo
	for (std::size_t i = 0; i < fundos::currency_locale::num_locales; i++) {
		const auto& entry = fundos::currency_locale::locales.entries[i];
		currency_combo->addItem(QString::fromUtf8(entry.identifier), QVariant::fromValue(static_cast<int>(i)));
	}
	currency_combo->addItem(tr("Custom"), QVariant::fromValue(-1));

	// Populate percentage combo
	for (std::size_t i = 0; i < fundos::percentage_locale::num_locales; i++) {
		const auto& entry = fundos::percentage_locale::locales.entries[i];
		percentage_combo->addItem(QString::fromUtf8(entry.identifier), QVariant::fromValue(static_cast<int>(i)));
	}
	percentage_combo->addItem(tr("Custom"), QVariant::fromValue(-1));

	// Connect combos
	connect(currency_combo,                    &QComboBox::currentIndexChanged, this, &LocalePage::on_currency_preset);
	connect(percentage_combo,                  &QComboBox::currentIndexChanged, this, &LocalePage::on_percentage_preset);

	// Connect custom currency controls to preview
	connect(currency_scale,                    &QComboBox::currentIndexChanged, this, &LocalePage::on_currency_preview);
	connect(currency_symbol,                   &QLineEdit::textChanged,         this, &LocalePage::on_currency_preview);
	connect(currency_thousands_separator,      &QLineEdit::textChanged,         this, &LocalePage::on_currency_preview);
	connect(currency_decimal_separator,        &QLineEdit::textChanged,         this, &LocalePage::on_currency_preview);
	connect(currency_symbol_placement_group,   &QButtonGroup::idClicked,        this, [this](int) { on_currency_preview(); });
	connect(currency_negative_notation_group,  &QButtonGroup::idClicked,        this, [this](int) { on_currency_preview(); });

	// Connect custom percentage controls to preview
	connect(percentage_decimal_separator,      &QLineEdit::textChanged,         this, &LocalePage::on_percentage_preview);
	connect(percentage_has_space,              &QCheckBox::checkStateChanged,   this, [this](auto) { on_percentage_preview(); });
	connect(percentage_symbol_placement_group, &QButtonGroup::idClicked,        this, [this](int)  { on_percentage_preview(); });

	// Apply initial currency selection
	if (current_currency->is_preset()) {
		const char* identifier = current_currency->identifier();
		for (int i = 0; i < currency_combo->count(); i++) {
			if (currency_combo->itemData(i).toInt() >= 0) {
				const auto& entry = fundos::currency_locale::locales.entries[currency_combo->itemData(i).toInt()];
				if (std::string_view(entry.identifier) == identifier) {
					currency_combo->setCurrentIndex(i);
					break;
				}
			}
		}
	} else {
		currency_combo->setCurrentIndex(currency_combo->count() - 1);
		const auto& spec = current_currency->info();
		currency_symbol->setText(QString::fromStdString(spec.symbol));
		currency_thousands_separator->setText(QString(spec.thousands_separator));
		currency_decimal_separator->setText(QString(spec.decimal_separator));
		for (int i = 0; i < currency_scale->count(); i++) {
			if (currency_scale->itemData(i).toInt() == spec.scale) {
				currency_scale->setCurrentIndex(i);
				break;
			}
		}
		auto* placement_button = currency_symbol_placement_group->button(static_cast<int>(spec.symbol_position));
		if (placement_button) { placement_button->setChecked(true); }
		auto* notation_button = currency_negative_notation_group->button(static_cast<int>(spec.negative_format));
		if (notation_button) { notation_button->setChecked(true); }
	}
	on_currency_preset();

	// Apply initial percentage selection
	if (current_percentage->is_preset()) {
		const char* identifier = current_percentage->identifier();
		for (int i = 0; i < percentage_combo->count(); i++) {
			if (percentage_combo->itemData(i).toInt() >= 0) {
				const auto& entry = fundos::percentage_locale::locales.entries[percentage_combo->itemData(i).toInt()];
				if (std::string_view(entry.identifier) == identifier) {
					percentage_combo->setCurrentIndex(i);
					break;
				}
			}
		}
	} else {
		percentage_combo->setCurrentIndex(percentage_combo->count() - 1);
		const auto& spec = current_percentage->info();
		percentage_decimal_separator->setText(QString(spec.decimal_separator));
		percentage_has_space->setChecked(spec.has_space_around_number);
		auto* placement_button = percentage_symbol_placement_group->button(static_cast<int>(spec.symbol_position));
		if (placement_button) { placement_button->setChecked(true); }
	}
	on_percentage_preset();
}

void LocalePage::on_confirm() {
	if (currency_fields.symbol.size() > 4) {
		QMessageBox::information(this, tr("Invalid Input"), tr("Currency symbol is limited to 4 characters (some international characters count as 2 or more)"));
		return;
	}
	int currency_index = currency_combo->currentData().toInt();
	if (currency_index == -1) {
		auto custom = currency_locale(currency_fields);
		auto saved = database->set_currency_locale(custom);
		emit db_outcome(saved);
		if (!saved) { return; }
	} else {
		auto preset = currency_locale(&fundos::currency_locale::locales.entries[currency_index]);
		auto saved = database->set_currency_locale(preset);
		emit db_outcome(saved);
		if (!saved) { return; }
	}

	int percentage_index = percentage_combo->currentData().toInt();
	if (percentage_index == -1) {
		auto custom = percentage_locale(percentage_fields);
		auto saved = database->set_percentage_locale(custom);
		emit db_outcome(saved);
		if (!saved) { return; }
	} else {
		auto preset = percentage_locale(&fundos::percentage_locale::locales.entries[percentage_index]);
		auto saved = database->set_percentage_locale(preset);
		emit db_outcome(saved);
		if (!saved) { return; }
	}

	emit done();
}

void LocalePage::on_currency_preset() {
	int index    = currency_combo->currentData().toInt();
	bool custom  = index == -1;
	currency_custom_fields->setEnabled(custom);
	if (!custom) {
		const auto& entry = fundos::currency_locale::locales.entries[index];
		const auto& info  = entry.info;
		for (int i = 0; i < currency_scale->count(); i++) {
			if (currency_scale->itemData(i).toInt() == info.scale) {
				currency_scale->setCurrentIndex(i);
				break;
			}
		}
		currency_symbol->setText(QString::fromStdString(info.symbol));
		currency_thousands_separator->setText(QString(info.thousands_separator));
		currency_decimal_separator->setText(QString(info.decimal_separator));
		auto* placement_button = currency_symbol_placement_group->button(static_cast<int>(info.symbol_position));
		if (placement_button) { placement_button->setChecked(true); }
		auto* notation_button = currency_negative_notation_group->button(static_cast<int>(info.negative_format));
		if (notation_button) { notation_button->setChecked(true); }
	}
	on_currency_preview();
}

void LocalePage::on_percentage_preset() {
	int index   = percentage_combo->currentData().toInt();
	bool custom = index == -1;
	percentage_custom_fields->setEnabled(custom);
	if (!custom) {
		const auto& entry = fundos::percentage_locale::locales.entries[index];
		const auto& info  = entry.info;
		percentage_decimal_separator->setText(QString(info.decimal_separator));
		percentage_has_space->setChecked(info.has_space_around_number);
		auto* placement_button = percentage_symbol_placement_group->button(static_cast<int>(info.symbol_position));
		if (placement_button) { placement_button->setChecked(true); }
	}
	on_percentage_preview();
}

void LocalePage::on_currency_preview() {
	int     scale_value          = currency_scale->currentData().toInt();
	QString symbol               = currency_symbol->text();
	QString thousands            = currency_thousands_separator->text();
	QString decimal              = currency_decimal_separator->text();
	int     placement_id         = currency_symbol_placement_group->checkedId();
	int     notation_id          = currency_negative_notation_group->checkedId();

	if (thousands.isEmpty() || decimal.isEmpty() || placement_id == -1 || notation_id == -1) {
		currency_preview->setText(tr("(incomplete)"));
		return;
	}

	currency_fields = currency_spec{
		.scale               = static_cast<int16_t>(scale_value),
		.symbol              = symbol.toStdString(),
		.thousands_separator = thousands[0].toLatin1(),
		.decimal_separator   = decimal[0].toLatin1(),
		.symbol_position     = static_cast<currency_spec::symbol_placement>(placement_id),
		.negative_format     = static_cast<currency_spec::negative_notation>(notation_id),
	};

	currency_preview->setText(QString::fromStdString(
		fundos::format_currency(-123654789, currency_fields)
	));
}

void LocalePage::on_percentage_preview() {
	QString decimal      = percentage_decimal_separator->text();
	bool    has_space    = percentage_has_space->isChecked();
	int     placement_id = percentage_symbol_placement_group->checkedId();

	if (decimal.isEmpty() || placement_id == -1) {
		percentage_preview->setText(tr("(incomplete)"));
		return;
	}

	percentage_fields = percentage_spec{
		.decimal_separator       = decimal[0].toLatin1(),
		.has_space_around_number = has_space,
		.symbol_position         = static_cast<percentage_spec::symbol_placement>(placement_id),
	};

	percentage_preview->setText(QString::fromStdString(
		fundos::format_percentage(-4230, percentage_fields)
	));
}
