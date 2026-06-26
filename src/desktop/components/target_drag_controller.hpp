#pragma once
#include "data/models.hpp"
#include "drag_drop.hpp"
#include <QList>
#include <QObject>
#include <QPoint>
#include <QScrollArea>
#include <QString>
#include <QTimer>
#include <QWidget>

/// Manages drag-to-reorder for target rows within a single PhaseWidget.
/// Constructed with either a fixed or percentage phase; watch() overloads enforce the match.
/// On drop, calls budget_phase::reorder_target then emits reorder_complete.
class TargetDragController : public QObject {
	Q_OBJECT

public:
	explicit TargetDragController(
		fundos::budget_phase<fundos::fixed_target>* phase,
		QScrollArea* scroll_area,
		DropIndicator* drop_indicator,
		QObject* parent = nullptr
	);

	explicit TargetDragController(
		fundos::budget_phase<fundos::percentage_target>* phase,
		QScrollArea* scroll_area,
		DropIndicator* drop_indicator,
		QObject* parent = nullptr
	);

	/// Registers a fixed target row and its drag handle with this controller.
	/// Asserts that this controller was constructed with a fixed phase.
	/// @param handle The DragHandle belonging to target_widget.
	/// @param target_widget The widget used for geometry hit boxes.
	/// @param target The core data pointer corresponding to target_widget.
	/// @param description The text to show while dragging.
	void watch(DragHandle* handle, QWidget* target_widget, fundos::fixed_target* target, const QString& description);

	/// Registers a percentage target row and its drag handle with this controller.
	/// Asserts that this controller was constructed with a percentage phase.
	/// @param handle The DragHandle belonging to target_widget.
	/// @param target_widget The widget used for geometry hit boxes.
	/// @param target The core data pointer corresponding to target_widget.
	/// @param description The text to show while dragging.
	void watch(DragHandle* handle, QWidget* target_widget, fundos::percentage_target* target, const QString& description);

	/// Clears all registered phase entries. Call before re-registering after a rebuild.
	/// Does not disconnect drag handles; Qt cleans up their connections when the widgets are deleted.
	void clear();

signals:
	/// Emitted after reorder_target has been called and the drop is complete.
	/// PhaseWidget should rebuild its target rows in response.
	void reorder_complete();

private slots:
	void on_drag_started(QPoint global_position);
	void on_drag_moved(QPoint global_position);
	void on_drag_released(QPoint global_position);
	void on_auto_scroll();

private:
	template<typename TargetType>
	struct TargetEntry {
		QWidget*    widget;
		TargetType* target;
		QString     description;
	};

	/// Finds the insertion point for the given Y coordinate in viewport space.
	/// @return Pointer to the target to insert before, or nullptr for end.
	template<typename TargetType>
	TargetType* insertion_point_at(int viewport_y, const QList<TargetEntry<TargetType>>& entry_list, QWidget* dragged) const;

	/// Returns the Y coordinate in viewport space of the drop indicator for the given insertion point.
	template<typename TargetType>
	int indicator_y_for(TargetType* before, const QList<TargetEntry<TargetType>>& entry_list, QWidget* dragged) const;

	fundos::budget_phase<fundos::fixed_target>*      fixed_phase      = nullptr;
	fundos::budget_phase<fundos::percentage_target>* percentage_phase = nullptr;

	QList<TargetEntry<fundos::fixed_target>>      fixed_entries;
	QList<TargetEntry<fundos::percentage_target>> percentage_entries;

	QScrollArea*   scroll_area;
	DropIndicator* drop_indicator;

	// Active drag state — all null/invalid when no drag is in progress
	QWidget*   dragged_widget = nullptr;

	fundos::fixed_target*      dragged_fixed_target      = nullptr;
	fundos::fixed_target*      before_fixed_target       = nullptr;
	fundos::percentage_target* dragged_percentage_target = nullptr;
	fundos::percentage_target* before_percentage_target  = nullptr;

	QPoint     grab_offset;
	DragGhost* ghost                 = nullptr;
	QTimer*    auto_scroll_timer;
	int        auto_scroll_direction = 0;
};
