#include "drag_drop.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QApplication>

// ─── DragHandle ──────────────────────────────────────────────────────────────

DragHandle::DragHandle(QWidget* parent) : QWidget(parent) {
	setCursor(Qt::SizeVerCursor);
	setFixedSize(16, 24);
	setAttribute(Qt::WA_Hover);
}

void DragHandle::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	QColor dot_color = underMouse()
		? palette().color(QPalette::Text)
		: palette().color(QPalette::PlaceholderText);

	painter.setBrush(dot_color);
	painter.setPen(Qt::NoPen);

	// 2×3 grid of dots centred in the widget
	constexpr int dot_radius  = 2;
	constexpr int dot_spacing = 5;
	int left   = (width()  - dot_spacing) / 2;
	int top    = (height() - dot_spacing * 2) / 2;

	for (int column = 0; column < 2; ++column) {
		for (int row = 0; row < 3; ++row) {
			QPoint centre(left + column * dot_spacing, top + row * dot_spacing);
			painter.drawEllipse(centre, dot_radius, dot_radius);
		}
	}
}

void DragHandle::mousePressEvent(QMouseEvent* event) {
	if (event->button() != Qt::LeftButton) { return; }
	dragging = true;
	emit drag_started(event->globalPosition().toPoint());
}

void DragHandle::mouseMoveEvent(QMouseEvent* event) {
	if (!dragging) { return; }
	emit drag_moved(event->globalPosition().toPoint());
}

void DragHandle::mouseReleaseEvent(QMouseEvent* event) {
	if (event->button() != Qt::LeftButton || !dragging) { return; }
	dragging = false;
	emit drag_released(event->globalPosition().toPoint());
}

// ─── DragGhost ───────────────────────────────────────────────────────────────

static constexpr Qt::WindowFlags flags = Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowTransparentForInput;
DragGhost::DragGhost(const QString& description, QWidget* parent) : QWidget(parent, flags), description(description) {
	setAttribute(Qt::WA_TranslucentBackground);
	setFixedSize(320, 28);
}

void DragGhost::track(QPoint global_position, QPoint grab_offset) {
	move(global_position - grab_offset);
}

void DragGhost::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	QColor background = palette().color(QPalette::Base);
	background.setAlpha(200);

	QPainterPath path;
	path.addRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);

	painter.fillPath(path, background);

	painter.setPen(palette().color(QPalette::Text));
	painter.drawText(rect().adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, description);
}

// ─── DropIndicator ───────────────────────────────────────────────────────────

DropIndicator::DropIndicator(QWidget* parent) : QWidget(parent) {
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setAttribute(Qt::WA_TranslucentBackground);
	// Sits on top of all siblings inside the viewport
	raise();
}

void DropIndicator::set_y(int y) {
	indicator_y = y;
	update();
}

void DropIndicator::paintEvent(QPaintEvent*) {
	if (indicator_y < 0) { return; }

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	QColor line_color = palette().color(QPalette::Highlight);

	// End caps
	constexpr int cap_radius = 4;
	painter.setBrush(line_color);
	painter.setPen(Qt::NoPen);
	painter.drawEllipse(QPoint(cap_radius, indicator_y), cap_radius, cap_radius);
	painter.drawEllipse(QPoint(width() - cap_radius, indicator_y), cap_radius, cap_radius);

	// Line between caps
	painter.setPen(QPen(line_color, 2));
	painter.drawLine(cap_radius * 2, indicator_y, width() - cap_radius * 2, indicator_y);
}
