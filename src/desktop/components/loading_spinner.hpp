#pragma once
#include <QPainter>
#include <QPen>
#include <QTimer>
#include <QWidget>

class LoadingSpinner : public QWidget {
	Q_OBJECT

	QTimer timer;
	int    angle = 0;

public:
	explicit LoadingSpinner(QWidget* parent = nullptr) : QWidget(parent) {
		setFixedSize(24, 24);
		timer.setInterval(16);
		connect(&timer, &QTimer::timeout, this, [this]() {
			angle = (angle + 3) % 360;
			update();
		});
		timer.start();
	}

protected:
	void paintEvent(QPaintEvent*) override {
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(Qt::NoPen);

		int side = qMin(width(), height());
		double center_x = width()  / 2.0;
		double center_y = height() / 2.0;
		double dot_size = side / 8.0;
		double radius = (side / 2.0) - 4 - (dot_size / 2.0);

		constexpr int dot_count = 12;
		QColor color = palette().text().color();

		for (int i = 0; i < dot_count; ++i) {
			double dot_angle  = (angle + i * (360 / dot_count)) * M_PI / 180.0;
			double opacity    = (i + 1.0) / dot_count;
			double dot_x      = center_x + radius * std::cos(dot_angle) - dot_size / 2.0;
			double dot_y      = center_y + radius * std::sin(dot_angle) - dot_size / 2.0;

			color.setAlphaF(opacity);
			painter.setBrush(color);
			painter.drawEllipse(QRectF(dot_x, dot_y, dot_size, dot_size));
		}
	}
};
