#pragma once
#include <QString>
#include <QWidget>

/// A small handle widget the user grabs to initiate a drag.
/// Emits drag_started, drag_moved, and drag_released with global cursor positions.
class DragHandle : public QWidget {
	Q_OBJECT

public:
	explicit DragHandle(QWidget* parent = nullptr);

signals:
	/// Emitted when the mouse is pressed on the handle.
	/// @param global_position The cursor position in global screen coordinates.
	void drag_started(QPoint global_position);

	/// Emitted on each mouse move after drag_started.
	/// @param global_position The cursor position in global screen coordinates.
	void drag_moved(QPoint global_position);

	/// Emitted when the mouse is released.
	/// @param global_position The cursor position in global screen coordinates.
	void drag_released(QPoint global_position);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

private:
	bool dragging = false;
};

/// A frameless, transparent-for-input widget that follows the cursor during a drag.
/// The caller is responsible for showing, moving, and hiding it.
class DragGhost : public QWidget {
	Q_OBJECT

public:
	explicit DragGhost(const QString& description, QWidget* parent = nullptr);

	/// Moves the ghost so the given local offset within it sits under global_position.
	void track(QPoint global_position, QPoint grab_offset);

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	QString description;
};

/// A transparent overlay widget that paints a single horizontal drop indicator line.
/// Must be a child of the scroll area viewport and sized to match it.
/// Caller updates indicator_y and calls update() to repaint.
class DropIndicator : public QWidget {
	Q_OBJECT

public:
	explicit DropIndicator(QWidget* parent = nullptr);

	/// Sets the Y coordinate (in this widget's local space) at which to draw the line.
	/// Pass -1 to hide the indicator without hiding the widget.
	void set_y(int y);

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	int indicator_y = -1;
};
