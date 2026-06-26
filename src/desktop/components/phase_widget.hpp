#pragma once
#include "data/models.hpp"
#include "coordinator.hpp"
#include "components/drag_drop.hpp"
#include "components/target_drag_controller.hpp"
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QObject>
#include <QScrollArea>
#include <QWidget>

/// Displays and edits a single budget phase and its target rows.
/// Constructed with either a fixed or percentage phase; the kind is fixed at construction time.
/// Handles target add and remove internally; emits phase_remove_requested for the parent to handle.
class PhaseWidget : public QWidget {
	Q_OBJECT

	void init(const QString& kind_label);

public:
	explicit PhaseWidget(
		fundos::budget_phase<fundos::fixed_target>* phase,
		AppCoordinator* coordinator,
		QScrollArea* scroll_area,
		DropIndicator* drop_indicator,
		QWidget* parent = nullptr
	);

	explicit PhaseWidget(
		fundos::budget_phase<fundos::percentage_target>* phase,
		AppCoordinator* coordinator,
		QScrollArea* scroll_area,
		DropIndicator* drop_indicator,
		QWidget* parent = nullptr
	);

protected:
	bool eventFilter(QObject* object, QEvent* event) override;

signals:
	/// Emitted when the user clicks the remove button for this phase.
	void phase_remove_requested();

private slots:
	void on_add_target();
	void on_remove_fixed_target(fundos::fixed_target* target);
	void on_remove_percentage_target(fundos::percentage_target* target);
	void rebuild_target_rows();

private:
	void build_header_row(QVBoxLayout* layout, const QString& kind_label);
	void build_column_header_row();

	template<typename TargetType>
	void build_target_row(TargetType* target, int row);

	fundos::budget_phase<fundos::fixed_target>*      fixed_phase      = nullptr;
	fundos::budget_phase<fundos::percentage_target>* percentage_phase = nullptr;

	AppCoordinator*        app_coordinator;
	TargetDragController*  target_drag_controller;
	QComboBox*             fund_combo;
	QWidget*               grid_container;
	QGridLayout*           grid_layout;

public:
	/// Public so BudgetDialog can register it with PhaseDragController after construction.
	DragHandle*            phase_drag_handle = nullptr;
};
