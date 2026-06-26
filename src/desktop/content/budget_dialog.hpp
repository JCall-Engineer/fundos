#pragma once
#include "data/models.hpp"
#include "coordinator.hpp"
#include "components/drag_drop.hpp"
#include "components/phase_drag_controller.hpp"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

class PhaseWidget;

class BudgetDialog : public QDialog {
	Q_OBJECT

	AppCoordinator*       app_coordinator;
	fundos::budget        record;
	PhaseDragController*  phase_drag_controller;
	DropIndicator*        drop_indicator;
	QScrollArea*          scroll_area;
	QWidget*              scroll_content;
	QVBoxLayout*          scroll_layout;
	QComboBox*            overflow_combo;
	QDialogButtonBox*     button_box;

public:
	explicit BudgetDialog(AppCoordinator* coordinator, fundos::budget opening, QWidget* parent = nullptr);

signals:
	void save_budget_requested(fundos::budget budget);
	void delete_budget_requested(int64_t budget_id);

private slots:
	void on_add_fixed_phase();
	void on_add_percentage_phase();
	void on_phase_remove_requested(fundos::any_budget_phase* phase);
	void on_save(fundos::db::outcome result);
	void rebuild_phases();
};
