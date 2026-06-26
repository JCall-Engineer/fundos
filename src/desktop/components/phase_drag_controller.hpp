#pragma once
#include "data/models.hpp"
#include "drag_drop.hpp"
#include <QObject>
#include <QPoint>
#include <QScrollArea>
#include <QTimer>
#include <QWidget>

/// Manages drag-to-reorder for phase widgets within the budget page scroll area.
/// Call watch() for each PhaseWidget after constructing it.
/// On drop, calls budget::reorder_phase then emits reorder_complete.
class PhaseDragController : public QObject {
	Q_OBJECT

public:
	/// @param budget The budget whose phases are being reordered.
	/// @param scroll_area The scroll area containing the phase widgets.
	/// @param scroll_content The direct parent widget of all phase widgets.
	/// @param drop_indicator The shared drop indicator overlay.
	explicit PhaseDragController(
		fundos::budget* budget,
		QScrollArea* scroll_area,
		QWidget* scroll_content,
		DropIndicator* drop_indicator,
		QObject* parent = nullptr
	);

	/// Registers a phase widget and its drag handle with this controller.
	/// @param handle The DragHandle belonging to phase_widget.
	/// @param phase_widget The PhaseWidget being made draggable.
	/// @param phase The core data pointer corresponding to phase_widget.
	void watch(DragHandle* handle, QWidget* phase_widget, fundos::any_budget_phase* phase);

	/// Clears all registered phase entries. Call before re-registering after a rebuild.
	void clear();

signals:
	/// Emitted after reorder_phase has been called and the drop is complete.
	/// BudgetPage should rebuild its phase list in response.
	void reorder_complete();

private slots:
	void on_drag_started(QPoint global_position);
	void on_drag_moved(QPoint global_position);
	void on_drag_released(QPoint global_position);
	void on_auto_scroll();

private:
	struct PhaseEntry {
		QWidget*                  widget;
		fundos::any_budget_phase* phase;
	};

	/// Finds the insertion point for the given Y coordinate in viewport space.
	/// @return Pointer to the phase that the dragged item should be inserted before, or nullptr for end.
	fundos::any_budget_phase* insertion_point_at(int viewport_y) const;

	/// Returns the Y coordinate in viewport space of the drop indicator for the given insertion point.
	int indicator_y_for(fundos::any_budget_phase* before_phase) const;

	fundos::budget*    budget;
	QScrollArea*       scroll_area;
	QWidget*           scroll_content;
	DropIndicator*     drop_indicator;

	QList<PhaseEntry>  entries;

	// Active drag state — all null/invalid when no drag is in progress
	QWidget*                  dragged_widget  = nullptr;
	fundos::any_budget_phase* dragged_phase   = nullptr;
	fundos::any_budget_phase* before_phase    = nullptr;
	QPoint                    grab_offset;
	DragGhost*                ghost           = nullptr;
	QTimer*                   auto_scroll_timer;
	int                       auto_scroll_direction = 0;
};
