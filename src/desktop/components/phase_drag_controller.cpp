#include "phase_drag_controller.hpp"
#include <QScrollBar>

static constexpr int auto_scroll_margin      = 40;
static constexpr int auto_scroll_speed       = 8;
static constexpr int auto_scroll_interval_ms = 16;

PhaseDragController::PhaseDragController(
	fundos::budget* budget,
	QScrollArea* scroll_area,
	QWidget* scroll_content,
	DropIndicator* drop_indicator,
	QObject* parent
) : QObject(parent), budget(budget), scroll_area(scroll_area), scroll_content(scroll_content), drop_indicator(drop_indicator) {
	auto_scroll_timer = new QTimer(this);
	auto_scroll_timer->setInterval(auto_scroll_interval_ms);
	connect(auto_scroll_timer, &QTimer::timeout, this, &PhaseDragController::on_auto_scroll);
}

void PhaseDragController::watch(DragHandle* handle, QWidget* phase_widget, fundos::any_budget_phase* phase) {
	entries.append({ phase_widget, phase });
	connect(handle, &DragHandle::drag_started,  this, &PhaseDragController::on_drag_started);
	connect(handle, &DragHandle::drag_moved,    this, &PhaseDragController::on_drag_moved);
	connect(handle, &DragHandle::drag_released, this, &PhaseDragController::on_drag_released);

	// Properties used to recover the associated widget and phase from sender() in on_drag_started,
	// since signal connections don't carry per-registration context.
	handle->setProperty("phase_widget", QVariant::fromValue(static_cast<QWidget*>(phase_widget)));
	handle->setProperty("phase_ptr",    QVariant::fromValue(static_cast<void*>(phase)));
}

void PhaseDragController::clear() {
	entries.clear();
}

void PhaseDragController::on_drag_started(QPoint global_position) {
	auto* handle = qobject_cast<DragHandle*>(sender());
	if (!handle) { return; }

	dragged_widget = qvariant_cast<QWidget*>(handle->property("phase_widget"));
	dragged_phase  = static_cast<fundos::any_budget_phase*>(
		qvariant_cast<void*>(handle->property("phase_ptr"))
	);

	grab_offset = dragged_widget->mapFromGlobal(global_position);

	QString description = std::visit([](const auto& phase) -> QString {
		using T = std::decay_t<decltype(phase)>;
		if constexpr (std::is_same_v<T, fundos::budget_phase<fundos::fixed_target>>) {
			return QString("Fixed phase · %1 targets").arg(phase.targets.size());
		} else {
			return QString("Percentage phase · %1 targets").arg(phase.targets.size());
		}
	}, *dragged_phase);

	ghost = new DragGhost(description, nullptr);
	ghost->show();
	ghost->track(global_position, grab_offset);

	before_phase = insertion_point_at(
		scroll_area->viewport()->mapFromGlobal(global_position).y()
	);
	drop_indicator->set_y(indicator_y_for(before_phase));
	drop_indicator->resize(scroll_area->viewport()->size());
	drop_indicator->raise();
}

void PhaseDragController::on_drag_moved(QPoint global_position) {
	if (!dragged_widget) { return; }

	ghost->track(global_position, grab_offset);

	int viewport_y = scroll_area->viewport()->mapFromGlobal(global_position).y();

	before_phase = insertion_point_at(viewport_y);
	drop_indicator->set_y(indicator_y_for(before_phase));

	// Auto-scroll when cursor is near the top or bottom edge of the viewport
	int viewport_height = scroll_area->viewport()->height();
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

void PhaseDragController::on_drag_released(QPoint) {
	if (!dragged_widget) { return; }

	auto_scroll_timer->stop();
	auto_scroll_direction = 0;

	delete ghost;
	ghost = nullptr;

	drop_indicator->set_y(-1);

	budget->reorder_phase(dragged_phase, before_phase);

	dragged_widget = nullptr;
	dragged_phase  = nullptr;
	before_phase   = nullptr;

	emit reorder_complete();
}

void PhaseDragController::on_auto_scroll() {
	auto* bar = scroll_area->verticalScrollBar();
	bar->setValue(bar->value() + auto_scroll_direction * auto_scroll_speed);

	// Recalculate insertion point after scroll shifts geometry
	int viewport_y = scroll_area->viewport()->mapFromGlobal(QCursor::pos()).y();
	before_phase = insertion_point_at(viewport_y);
	drop_indicator->set_y(indicator_y_for(before_phase));
}

fundos::any_budget_phase* PhaseDragController::insertion_point_at(int viewport_y) const {
	// Phase widgets live in scroll_content's coordinate space, which scrolls independently of the viewport the cursor position is reported in.
	// Round-trip through global screen coordinates to convert between them.
	QPoint content_pos = scroll_content->mapFromGlobal(
		scroll_area->viewport()->mapToGlobal(QPoint(0, viewport_y))
	);
	int content_y = content_pos.y();

	for (const PhaseEntry& entry : entries) {
		if (entry.phase == dragged_phase) { continue; }
		QRect geometry = entry.widget->geometry();
		if (content_y < geometry.top() + geometry.height() / 2) {
			return entry.phase;
		}
	}
	return nullptr;
}

int PhaseDragController::indicator_y_for(fundos::any_budget_phase* before) const {
	int content_y = -1;

	if (before == nullptr) {
		// End of list — bottom edge of the last non-dragged widget
		for (const PhaseEntry& entry : entries) {
			if (entry.phase == dragged_phase) { continue; }
			content_y = entry.widget->geometry().bottom();
		}
	} else {
		// Top edge of the before widget
		for (const PhaseEntry& entry : entries) {
			if (entry.phase != before) { continue; }
			content_y = entry.widget->geometry().top();
			break;
		}
	}

	if (content_y < 0) { return -1; }

	QPoint viewport_pos = scroll_area->viewport()->mapFromGlobal(
		scroll_content->mapToGlobal(QPoint(0, content_y))
	);
	return viewport_pos.y();
}
