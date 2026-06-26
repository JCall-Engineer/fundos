#include "target_drag_controller.hpp"
#include <QScrollBar>

static constexpr int auto_scroll_margin      = 40;
static constexpr int auto_scroll_speed       = 8;
static constexpr int auto_scroll_interval_ms = 16;

TargetDragController::TargetDragController(
	fundos::budget_phase<fundos::fixed_target>* phase,
	QScrollArea* scroll_area,
	DropIndicator* drop_indicator,
	QObject* parent
) : QObject(parent), fixed_phase(phase), scroll_area(scroll_area), drop_indicator(drop_indicator) {
	auto_scroll_timer = new QTimer(this);
	auto_scroll_timer->setInterval(auto_scroll_interval_ms);
	connect(auto_scroll_timer, &QTimer::timeout, this, &TargetDragController::on_auto_scroll);
}

TargetDragController::TargetDragController(
	fundos::budget_phase<fundos::percentage_target>* phase,
	QScrollArea* scroll_area,
	DropIndicator* drop_indicator,
	QObject* parent
) : QObject(parent), percentage_phase(phase), scroll_area(scroll_area), drop_indicator(drop_indicator) {
	auto_scroll_timer = new QTimer(this);
	auto_scroll_timer->setInterval(auto_scroll_interval_ms);
	connect(auto_scroll_timer, &QTimer::timeout, this, &TargetDragController::on_auto_scroll);
}

void TargetDragController::watch(DragHandle* handle, QWidget* target_widget, fundos::fixed_target* target, const QString& description) {
	if (fixed_phase == nullptr) {
		FUNDOS_ASSERT(false, "watch(fixed_target*) called on a percentage phase controller");
		return;
	}

	fixed_entries.append({ target_widget, target, description });
	// target_widget and target must outlive this registration;
	// nothing here guards against either being deleted while still registered.
	// clear() must be called before the corresponding widgets/data are destroyed.

	connect(handle, &DragHandle::drag_started,  this, &TargetDragController::on_drag_started);
	connect(handle, &DragHandle::drag_moved,    this, &TargetDragController::on_drag_moved);
	connect(handle, &DragHandle::drag_released, this, &TargetDragController::on_drag_released);

	// Properties used to recover the associated widget and phase from sender() in on_drag_started,
	// since signal connections don't carry per-registration context.
	handle->setProperty("target_widget", QVariant::fromValue(static_cast<QWidget*>(target_widget)));
	handle->setProperty("fixed_target_ptr", QVariant::fromValue(static_cast<void*>(target)));
}

void TargetDragController::watch(DragHandle* handle, QWidget* target_widget, fundos::percentage_target* target, const QString& description) {
	if (percentage_phase == nullptr) {
		FUNDOS_ASSERT(false, "watch(percentage_target*) called on a fixed phase controller");
		return;
	}

	percentage_entries.append({ target_widget, target, description });
	connect(handle, &DragHandle::drag_started,  this, &TargetDragController::on_drag_started);
	connect(handle, &DragHandle::drag_moved,    this, &TargetDragController::on_drag_moved);
	connect(handle, &DragHandle::drag_released, this, &TargetDragController::on_drag_released);
	handle->setProperty("target_widget",           QVariant::fromValue(static_cast<QWidget*>(target_widget)));
	handle->setProperty("percentage_target_ptr",   QVariant::fromValue(static_cast<void*>(target)));
}

void TargetDragController::clear() {
	fixed_entries.clear();
	percentage_entries.clear();
}

void TargetDragController::on_drag_started(QPoint global_position) {
	auto* handle = qobject_cast<DragHandle*>(sender());
	if (!handle) { return; }

	dragged_widget = qvariant_cast<QWidget*>(handle->property("target_widget"));
	grab_offset    = dragged_widget->mapFromGlobal(global_position);

	if (fixed_phase) {
		dragged_fixed_target = static_cast<fundos::fixed_target*>(
			qvariant_cast<void*>(handle->property("fixed_target_ptr"))
		);
		for (const auto& entry : fixed_entries) {
			if (entry.widget == dragged_widget) {
				ghost = new DragGhost(entry.description, nullptr);
				break;
			}
		}
		int viewport_y = scroll_area->viewport()->mapFromGlobal(global_position).y();
		before_fixed_target = insertion_point_at(viewport_y, fixed_entries, dragged_widget);
		drop_indicator->set_y(indicator_y_for(before_fixed_target, fixed_entries, dragged_widget));
	} else {
		dragged_percentage_target = static_cast<fundos::percentage_target*>(
			qvariant_cast<void*>(handle->property("percentage_target_ptr"))
		);
		for (const auto& entry : percentage_entries) {
			if (entry.widget == dragged_widget) {
				ghost = new DragGhost(entry.description, nullptr);
				break;
			}
		}
		int viewport_y = scroll_area->viewport()->mapFromGlobal(global_position).y();
		before_percentage_target = insertion_point_at(viewport_y, percentage_entries, dragged_widget);
		drop_indicator->set_y(indicator_y_for(before_percentage_target, percentage_entries, dragged_widget));
	}

	ghost->show();
	ghost->track(global_position, grab_offset);
	drop_indicator->resize(scroll_area->viewport()->size());
	drop_indicator->raise();
}

void TargetDragController::on_drag_moved(QPoint global_position) {
	if (!dragged_widget) { return; }

	ghost->track(global_position, grab_offset);

	int viewport_y      = scroll_area->viewport()->mapFromGlobal(global_position).y();
	int viewport_height = scroll_area->viewport()->height();

	if (fixed_phase) {
		before_fixed_target = insertion_point_at(viewport_y, fixed_entries, dragged_widget);
		drop_indicator->set_y(indicator_y_for(before_fixed_target, fixed_entries, dragged_widget));
	} else {
		before_percentage_target = insertion_point_at(viewport_y, percentage_entries, dragged_widget);
		drop_indicator->set_y(indicator_y_for(before_percentage_target, percentage_entries, dragged_widget));
	}

	if (viewport_y < auto_scroll_margin) {
		auto_scroll_direction = -1;
		if (!auto_scroll_timer->isActive()) { auto_scroll_timer->start(); }
	} else if (viewport_y > viewport_height - auto_scroll_margin) {
		auto_scroll_direction = 1;
		if (!auto_scroll_timer->isActive()) { auto_scroll_timer->start(); }
	} else {
		auto_scroll_direction = 0;
		auto_scroll_timer->stop();
	}
}

void TargetDragController::on_drag_released(QPoint) {
	if (!dragged_widget) { return; }

	auto_scroll_timer->stop();
	auto_scroll_direction = 0;

	delete ghost;
	ghost = nullptr;

	drop_indicator->set_y(-1);

	if (fixed_phase) {
		fixed_phase->reorder_target(dragged_fixed_target, before_fixed_target);
		dragged_fixed_target = nullptr;
		before_fixed_target  = nullptr;
	} else {
		percentage_phase->reorder_target(dragged_percentage_target, before_percentage_target);
		dragged_percentage_target = nullptr;
		before_percentage_target  = nullptr;
	}

	dragged_widget = nullptr;
	emit reorder_complete();
}

void TargetDragController::on_auto_scroll() {
	auto* bar = scroll_area->verticalScrollBar();
	bar->setValue(bar->value() + auto_scroll_direction * auto_scroll_speed);

	int viewport_y = scroll_area->viewport()->mapFromGlobal(QCursor::pos()).y();

	if (fixed_phase) {
		before_fixed_target = insertion_point_at(viewport_y, fixed_entries, dragged_widget);
		drop_indicator->set_y(indicator_y_for(before_fixed_target, fixed_entries, dragged_widget));
	} else {
		before_percentage_target = insertion_point_at(viewport_y, percentage_entries, dragged_widget);
		drop_indicator->set_y(indicator_y_for(before_percentage_target, percentage_entries, dragged_widget));
	}
}

template<typename TargetType>
TargetType* TargetDragController::insertion_point_at(int viewport_y, const QList<TargetEntry<TargetType>>& entry_list, QWidget* dragged) const {
	for (const TargetEntry<TargetType>& entry : entry_list) {
		if (entry.widget == dragged) { continue; }
		int top = scroll_area->viewport()->mapFromGlobal(entry.widget->mapToGlobal(QPoint(0, 0))).y();
		int mid = top + entry.widget->height() / 2;
		if (viewport_y < mid) {
			return entry.target;
		}
	}
	return nullptr;
}

template<typename TargetType>
int TargetDragController::indicator_y_for(TargetType* before, const QList<TargetEntry<TargetType>>& entry_list, QWidget* dragged) const {
	if (before == nullptr) {
		int last_bottom = -1;
		for (const TargetEntry<TargetType>& entry : entry_list) {
			if (entry.widget == dragged) { continue; }
			last_bottom = scroll_area->viewport()->mapFromGlobal(
				entry.widget->mapToGlobal(QPoint(0, entry.widget->height()))
			).y();
		}
		return last_bottom;
	}

	for (const TargetEntry<TargetType>& entry : entry_list) {
		if (entry.target != before) { continue; }
		return scroll_area->viewport()->mapFromGlobal(
			entry.widget->mapToGlobal(QPoint(0, 0))
		).y();
	}
	return -1;
}
