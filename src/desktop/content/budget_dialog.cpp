#include "budget_dialog.hpp"
#include "components/phase_widget.hpp"
#include "theme.hpp"
#include "components/editable_label.hpp"
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

BudgetDialog::BudgetDialog(AppCoordinator* coordinator, fundos::budget opening, QWidget* parent) : QDialog(parent), app_coordinator(coordinator), record(std::move(opening)) {
	auto* database = app_coordinator->database();
	connect(this,     &BudgetDialog::save_budget_requested,   database, &AppDatabase::save_budget);
	connect(this,     &BudgetDialog::delete_budget_requested, database, &AppDatabase::delete_budget);
	connect(database, &AppDatabase::budget_saved,             this,     &BudgetDialog::on_save);
	connect(database, &AppDatabase::budget_deleted,           this,     &BudgetDialog::on_save); // delete and save have the same outcome handling: accept on success, re-enable button_box on failure

	setWindowTitle(tr("Edit Budget"));
	setMinimumWidth(700);
	setMinimumHeight(500);

	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(0);

	// ── Toolbar row ───────────────────────────────────────────────────────────
	{
		auto* toolbar = new QWidget(this);
		toolbar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

		auto* toolbar_layout = new QHBoxLayout(toolbar);
		toolbar_layout->setContentsMargins(8, 8, 8, 8);
		toolbar_layout->setSpacing(8);

		auto* name_label = new EditableLabel(QString::fromStdString(record.name), toolbar);
		connect(name_label, &EditableLabel::value_changed, this, [this](QString name) {
			record.name = name.toStdString();
		});

		auto* delete_button = new QPushButton(tr("Delete budget"), toolbar);
		delete_button->setIcon(theme::colored_svg_icon(":/icons/trash.svg", theme::text, theme::toolbar_icon_size));
		connect(delete_button, &QPushButton::clicked, this, [this]() {
			button_box->setEnabled(false);
			emit delete_budget_requested(record.id());
		});

		toolbar_layout->addWidget(name_label);
		toolbar_layout->addStretch();
		toolbar_layout->addWidget(delete_button);

		layout->addWidget(toolbar);
	}

	// ── Overflow fund + add phase buttons ─────────────────────────────────────
	{
		auto* controls = new QWidget(this);
		controls->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

		auto* controls_layout = new QHBoxLayout(controls);
		controls_layout->setContentsMargins(8, 4, 8, 4);
		controls_layout->setSpacing(8);

		auto* overflow_label = new QLabel(tr("Overflow fund"), controls);

		overflow_combo = new QComboBox(controls);
		for (const auto& fund : app_coordinator->context()->funds()) {
			if (!fund.closed_at.has_value()) {
				overflow_combo->addItem(
					QString::fromStdString(fund.name),
					static_cast<qlonglong>(fund.id())
				);
				if (fund.id() == record.overflow_fund) {
					overflow_combo->setCurrentIndex(overflow_combo->count() - 1);
				}
			}
		}
		// New budgets have no overflow_fund set; initialize it to the combo's default selection.
		record.overflow_fund = overflow_combo->currentData().toLongLong();
		connect(overflow_combo, &QComboBox::currentIndexChanged, this, [this](int) {
			record.overflow_fund = overflow_combo->currentData().toLongLong();
		});

		auto* add_fixed_button = new QPushButton(tr("Fixed phase"), controls);
		auto* add_percentage_button = new QPushButton(tr("Percentage phase"), controls);
		add_fixed_button->setIcon(theme::colored_svg_icon(":/icons/plus.svg", theme::text.name(), theme::toolbar_icon_size));
		add_percentage_button->setIcon(theme::colored_svg_icon(":/icons/plus.svg", theme::text.name(), theme::toolbar_icon_size));
		connect(add_fixed_button,      &QPushButton::clicked, this, &BudgetDialog::on_add_fixed_phase);
		connect(add_percentage_button, &QPushButton::clicked, this, &BudgetDialog::on_add_percentage_phase);

		controls_layout->addWidget(overflow_label);
		controls_layout->addWidget(overflow_combo);
		controls_layout->addStretch();
		controls_layout->addWidget(add_fixed_button);
		controls_layout->addWidget(add_percentage_button);

		layout->addWidget(controls);
	}

	// ── Phases group box ──────────────────────────────────────────────────────────
	{
		auto* phases_group = new QGroupBox(tr("Phases"), this);
		auto* phases_layout = new QVBoxLayout(phases_group);
		phases_layout->setContentsMargins(8, 8, 8, 8);
		phases_layout->setSpacing(0);

		scroll_area = new QScrollArea(phases_group);
		scroll_area->setWidgetResizable(true);
		scroll_area->setFrameShape(QFrame::NoFrame);

		scroll_content = new QWidget(scroll_area);
		scroll_layout = new QVBoxLayout(scroll_content);
		scroll_layout->setContentsMargins(8, 8, 8, 8);
		scroll_layout->setSpacing(8);
		scroll_layout->addStretch(); // trailing stretch; rebuild_phases inserts phase widgets before it

		scroll_area->setWidget(scroll_content);

		phases_layout->addWidget(scroll_area);
		layout->addWidget(phases_group, 1);
	}

	// ── Drop indicator ────────────────────────────────────────────────────────
	drop_indicator = new DropIndicator(scroll_area->viewport());
	drop_indicator->resize(scroll_area->viewport()->size());

	// ── Phase drag controller ─────────────────────────────────────────────────
	phase_drag_controller = new PhaseDragController(
		&record,
		scroll_area,
		scroll_content,
		drop_indicator,
		this
	);
	connect(phase_drag_controller, &PhaseDragController::reorder_complete, this, &BudgetDialog::rebuild_phases);

	rebuild_phases();

	// ── Button box ────────────────────────────────────────────────────────────
	{
		button_box = new QDialogButtonBox(
			QDialogButtonBox::Save | QDialogButtonBox::Cancel,
			this
		);
		connect(button_box, &QDialogButtonBox::accepted, this, [this]() {
			button_box->setEnabled(false);
			emit save_budget_requested(record);
		});
		connect(button_box, &QDialogButtonBox::rejected, this, &BudgetDialog::reject);

		auto* button_row = new QWidget(this);
		auto* button_layout = new QHBoxLayout(button_row);
		button_layout->setContentsMargins(8, 4, 8, 8);
		button_layout->addWidget(button_box);
		layout->addWidget(button_row);
	}
}

void BudgetDialog::rebuild_phases() {
	// Remove all phase widgets from the layout, preserving the trailing stretch
	while (scroll_layout->count() > 1) {
		auto* item = scroll_layout->takeAt(0);
		if (item->widget()) { delete item->widget(); }
		delete item;
	}

	phase_drag_controller->clear();

	record.each_phase([&](int, fundos::budget_phase<fundos::fixed_target>* phase) {
		auto* widget = new PhaseWidget(phase, app_coordinator, scroll_area, drop_indicator, scroll_content);
		connect(widget, &PhaseWidget::phase_remove_requested, this, [this, phase]() {
			// phase is a budget_phase<fixed_target>*;
			// on_phase_remove_requested needs the any_budget_phase* wrapper instead,
			// so find_phase translates between the two by matching on the same underlying pointer via get_if.
			on_phase_remove_requested(
				record.find_phase([phase](int, fundos::any_budget_phase* any) {
					return std::get_if<fundos::budget_phase<fundos::fixed_target>>(any) == phase;
				})
			);
		});
		scroll_layout->insertWidget(scroll_layout->count() - 1, widget);
		phase_drag_controller->watch(widget->phase_drag_handle, widget, record.find_phase(
			[phase](int, const fundos::budget_phase<fundos::fixed_target>* p) { return p == phase; },
			[](int, const fundos::budget_phase<fundos::percentage_target>*)  { return false; }
		));
	}, [&](int, fundos::budget_phase<fundos::percentage_target>* phase) {
		auto* widget = new PhaseWidget(phase, app_coordinator, scroll_area, drop_indicator, scroll_content);
		connect(widget, &PhaseWidget::phase_remove_requested, this, [this, phase]() {
			on_phase_remove_requested(
				record.find_phase([phase](int, fundos::any_budget_phase* any) {
					return std::get_if<fundos::budget_phase<fundos::percentage_target>>(any) == phase;
				})
			);
		});
		scroll_layout->insertWidget(scroll_layout->count() - 1, widget);
		phase_drag_controller->watch(widget->phase_drag_handle, widget, record.find_phase(
			// find_phase needs both a fixed- and percentage-phase predicate (it's a std::visit under the hood);
			// since this branch is building a fixed phase widget, the percentage predicate always returns false by design.
			[](int, const fundos::budget_phase<fundos::fixed_target>*)             { return false; },
			[phase](int, const fundos::budget_phase<fundos::percentage_target>* p) { return p == phase; }
		));
	});
}

void BudgetDialog::on_add_fixed_phase() {
	record.phases.push_back(fundos::budget_phase<fundos::fixed_target>{});
	rebuild_phases();
}

void BudgetDialog::on_add_percentage_phase() {
	record.phases.push_back(fundos::budget_phase<fundos::percentage_target>{});
	rebuild_phases();
}

void BudgetDialog::on_phase_remove_requested(fundos::any_budget_phase* phase) {
	record.phases.remove_if([phase](const fundos::any_budget_phase& element) {
		return &element == phase;
	});
	rebuild_phases();
}

void BudgetDialog::on_save(fundos::db::outcome result) {
	if (result) {
		accept();
	} else {
		button_box->setEnabled(true);
	}
}
