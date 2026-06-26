#include "phase_widget.hpp"
#include "theme.hpp"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

static constexpr int column_handle    = 0;
static constexpr int column_fund      = 1;
static constexpr int column_amount    = 2;
static constexpr int column_cap_check = 3;
static constexpr int column_cap_edit  = 4;
static constexpr int column_overdraw  = 5;
static constexpr int column_remove    = 6;

PhaseWidget::PhaseWidget(
	fundos::budget_phase<fundos::fixed_target>* phase,
	AppCoordinator* coordinator,
	QScrollArea* scroll_area,
	DropIndicator* drop_indicator,
	QWidget* parent
) : QWidget(parent), fixed_phase(phase), app_coordinator(coordinator) {
	target_drag_controller = new TargetDragController(phase, scroll_area, drop_indicator, this);
	init(tr("Fixed phase"));
}

PhaseWidget::PhaseWidget(
	fundos::budget_phase<fundos::percentage_target>* phase,
	AppCoordinator* coordinator,
	QScrollArea* scroll_area,
	DropIndicator* drop_indicator,
	QWidget* parent
) : QWidget(parent), percentage_phase(phase), app_coordinator(coordinator) {
	target_drag_controller = new TargetDragController(phase, scroll_area, drop_indicator, this);
	init(tr("Percentage phase"));
}

void PhaseWidget::init(const QString& kind_label) {
	connect(target_drag_controller, &TargetDragController::reorder_complete, this, &PhaseWidget::rebuild_target_rows);

	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	setStyleSheet(QStringLiteral(
		"PhaseWidget { border: 1px solid %1; border-radius: 8px; background-color: %2; }"
	).arg(theme::separator.name(), theme::background.name()));
	setAttribute(Qt::WA_StyledBackground, true);

	build_header_row(layout, kind_label);

	grid_container = new QWidget(this);
	grid_layout = new QGridLayout(grid_container);
	grid_layout->setContentsMargins(0, 0, 0, 0);
	grid_layout->setHorizontalSpacing(4);
	grid_layout->setVerticalSpacing(0);
	grid_layout->setColumnStretch(column_fund,     3);
	grid_layout->setColumnStretch(column_amount,   2);
	grid_layout->setColumnStretch(column_cap_edit, 2);

	build_column_header_row();
	rebuild_target_rows();

	layout->addWidget(grid_container);
}

void PhaseWidget::build_header_row(QVBoxLayout* layout, const QString& kind_label) {
	auto* header = new QWidget(this);
	auto* header_layout = new QHBoxLayout(header);
	header_layout->setContentsMargins(8, 8, 8, 8);
	header_layout->setSpacing(8);

	phase_drag_handle = new DragHandle(header);

	auto* kind = new QLabel(kind_label, header);

	fund_combo = new QComboBox(header);
	for (const auto& fund : app_coordinator->context()->funds()) {
		fund_combo->addItem(QString::fromStdString(fund.name), static_cast<qlonglong>(fund.id()));
	}

	auto* add_button = new QPushButton(tr("Add target"), header);
	add_button->setIcon(theme::colored_svg_icon(":/icons/plus.svg", theme::text.name(), theme::toolbar_icon_size));
	connect(add_button, &QPushButton::clicked, this, &PhaseWidget::on_add_target);

	auto* remove_button = new QPushButton(header);
	remove_button->setIcon(theme::colored_svg_icon(":/icons/trash.svg", theme::text, theme::toolbar_icon_size));
	remove_button->setToolTip(tr("Remove phase"));
	connect(remove_button, &QPushButton::clicked, this, &PhaseWidget::phase_remove_requested);

	header_layout->addWidget(phase_drag_handle);
	header_layout->addWidget(kind);
	header_layout->addStretch();
	header_layout->addWidget(fund_combo);
	header_layout->addWidget(add_button);
	header_layout->addWidget(remove_button);

	layout->addWidget(header);
}

void PhaseWidget::build_column_header_row() {
	auto* fund_label     = new QLabel(tr("Fund"),     grid_container);
	auto* amount_label   = new QLabel(tr("Amount"),   grid_container);
	auto* cap_label      = new QLabel(tr("Cap"),      grid_container);
	auto* overdraw_label = new QLabel(tr("Overdraw"), grid_container);

	fund_label->setContentsMargins(4, 6, 4, 6);
	amount_label->setContentsMargins(4, 6, 4, 6);
	cap_label->setContentsMargins(4, 6, 4, 6);
	overdraw_label->setContentsMargins(4, 6, 4, 6);

	if (percentage_phase) {
		amount_label->setText(tr("Percentage"));
	}

	auto* header_background = new QWidget(grid_container);
	header_background->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	header_background->setMinimumSize(0, 0);
	header_background->setStyleSheet(QStringLiteral(
		"border: 1px solid %1; background-color: %2;"
	).arg(theme::separator.name(), theme::background.name()));
	header_background->lower();

	grid_layout->addWidget(header_background, 0, column_handle,    1, column_remove - column_handle + 1);
	grid_layout->addWidget(fund_label,        0, column_fund,      1, 1);
	grid_layout->addWidget(amount_label,      0, column_amount,    1, 1);
	grid_layout->addWidget(cap_label,         0, column_cap_check, 1, 2);
	grid_layout->addWidget(overdraw_label,    0, column_overdraw,  1, 1);
}

bool PhaseWidget::eventFilter(QObject* object, QEvent* event) {
	if (event->type() == QEvent::FocusIn) {
		auto* field = qobject_cast<QLineEdit*>(object);
		if (field != nullptr) {
			// selectAll must be queued; calling it directly during FocusIn is overridden by Qt's focus handling.
			QMetaObject::invokeMethod(field, "selectAll", Qt::QueuedConnection);
		}
	}
	return QWidget::eventFilter(object, event);
}

template<typename TargetType>
void PhaseWidget::build_target_row(TargetType* target, int row) {
	auto* handle = new DragHandle(grid_container);
	const auto& currency_locale   = app_coordinator->context()->currency_locale().info();
	const auto& percentage_locale = app_coordinator->context()->percentage_locale().info();

	QString fund_name;
	auto* fund = app_coordinator->context()->fund(target->fund_id);
	if (fund != nullptr) {
		fund_name = QString::fromStdString(fund->name);
	}

	auto* row_background = new QWidget(grid_container);
	row_background->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	row_background->setMinimumSize(0, 0);
	row_background->setStyleSheet(QStringLiteral(
		"border: 1px solid %1; background-color: %2;"
	).arg(theme::separator.name(), theme::surface.name()));
	row_background->lower();
	grid_layout->addWidget(row_background, row, column_handle, 1, column_remove - column_handle + 1);

	auto* fund_label = new QLabel(fund_name, grid_container);

	auto* amount_edit = new QLineEdit(grid_container);
	if constexpr (std::is_same_v<TargetType, fundos::fixed_target>) {
		amount_edit->setText(QString::fromStdString(target->amount.to_string(currency_locale)));
		connect(amount_edit, &QLineEdit::editingFinished, this, [this, amount_edit, target, currency_locale]() {
			const auto parsed = fundos::currency::from_string(amount_edit->text().toStdString(), currency_locale);
			target->amount = parsed.value_or(target->amount);
			amount_edit->setText(QString::fromStdString(target->amount.to_string(currency_locale)));
		});
	} else {
		amount_edit->setText(QString::fromStdString(target->amount.to_string(percentage_locale)));
		connect(amount_edit, &QLineEdit::editingFinished, this, [this, amount_edit, target, percentage_locale]() {
			const auto parsed = fundos::percentage::from_string(amount_edit->text().toStdString());
			target->amount = parsed.value_or(target->amount);
			amount_edit->setText(QString::fromStdString(target->amount.to_string(percentage_locale)));
		});
	}
	amount_edit->installEventFilter(this);

	auto* cap_checkbox = new QCheckBox(grid_container);
	cap_checkbox->setChecked(target->cap.has_value());

	auto* cap_edit = new QLineEdit(grid_container);
	cap_edit->setEnabled(target->cap.has_value());
	cap_edit->setPlaceholderText(tr("none"));
	if (target->cap) {
		cap_edit->setText(QString::fromStdString(target->cap->to_string(currency_locale)));
	}
	connect(cap_edit, &QLineEdit::editingFinished, this, [this, cap_edit, target, currency_locale]() {
		if (!target->cap.has_value()) { return; }
		const auto parsed = fundos::currency::from_string(cap_edit->text().toStdString(), currency_locale);
		target->cap = parsed.value_or(*target->cap);
		cap_edit->setText(QString::fromStdString(target->cap->to_string(currency_locale)));
	});
	cap_edit->installEventFilter(this);

	connect(cap_checkbox, &QCheckBox::toggled, cap_edit, [cap_edit, target, currency_locale](bool checked) {
		cap_edit->setEnabled(checked);
		if (!checked) {
			target->cap = std::nullopt;
			cap_edit->clear();
		} else {
			target->cap = fundos::currency{0};
			cap_edit->setText(QString::fromStdString(target->cap->to_string(currency_locale)));
		}
	});

	auto* overdraw_checkbox = new QCheckBox(grid_container);
	overdraw_checkbox->setChecked(target->allow_overdraw);
	connect(overdraw_checkbox, &QCheckBox::toggled, this, [target](bool checked) {
		target->allow_overdraw = checked;
	});

	auto* remove_button = new QPushButton(grid_container);
	remove_button->setIcon(theme::colored_svg_icon(":/icons/trash.svg", theme::text, theme::toolbar_icon_size));
	remove_button->setToolTip(tr("Remove target"));

	if constexpr (std::is_same_v<TargetType, fundos::fixed_target>) {
		connect(remove_button, &QPushButton::clicked, this, [this, target]() {
			on_remove_fixed_target(target);
		});
	} else {
		connect(remove_button, &QPushButton::clicked, this, [this, target]() {
			on_remove_percentage_target(target);
		});
	}

	grid_layout->addWidget(handle,            row, column_handle);
	grid_layout->addWidget(fund_label,        row, column_fund);
	grid_layout->addWidget(amount_edit,       row, column_amount);
	grid_layout->addWidget(cap_checkbox,      row, column_cap_check);
	grid_layout->addWidget(cap_edit,          row, column_cap_edit);
	grid_layout->addWidget(overdraw_checkbox, row, column_overdraw);
	grid_layout->addWidget(remove_button,     row, column_remove);

	target_drag_controller->watch(handle, row_background, target, fund_name);
}

void PhaseWidget::rebuild_target_rows() {
	// Remove all rows except the column header (row 0)
	const QList<QWidget*> children = grid_container->findChildren<QWidget*>(Qt::FindDirectChildrenOnly);
	for (auto* child : children) {
		auto* item = grid_layout->itemAt(grid_layout->indexOf(child));
		if (!item) { continue; }
		int row, col, row_span, col_span;
		grid_layout->getItemPosition(grid_layout->indexOf(child), &row, &col, &row_span, &col_span);
		if (row >= 1) {
			grid_layout->removeWidget(child);
			delete child;
		}
	}

	target_drag_controller->clear();

	int row = 1;
	if (fixed_phase) {
		for (auto& target : fixed_phase->targets) {
			build_target_row(&target, row++);
		}
	} else {
		for (auto& target : percentage_phase->targets) {
			build_target_row(&target, row++);
		}
	}
}

void PhaseWidget::on_add_target() {
	int64_t fund_id = fund_combo->currentData().toLongLong();
	if (fixed_phase) {
		fundos::fixed_target target;
		target.fund_id = fund_id;
		fixed_phase->targets.push_back(target);
	} else {
		fundos::percentage_target target;
		target.fund_id = fund_id;
		percentage_phase->targets.push_back(target);
	}
	rebuild_target_rows();
}

void PhaseWidget::on_remove_fixed_target(fundos::fixed_target* target) {
	if (fixed_phase == nullptr) {
		FUNDOS_ASSERT(false, "on_remove_fixed_target called on a percentage phase widget");
		return;
	}
	fixed_phase->targets.remove_if([target](const fundos::fixed_target& element) {
		return &element == target;
	});
	rebuild_target_rows();
}

void PhaseWidget::on_remove_percentage_target(fundos::percentage_target* target) {
	if (percentage_phase == nullptr) {
		FUNDOS_ASSERT(false, "on_remove_percentage_target called on a fixed phase widget");
		return;
	}
	percentage_phase->targets.remove_if([target](const fundos::percentage_target& element) {
		return &element == target;
	});
	rebuild_target_rows();
}
