#pragma once
#include <QDateTime>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QToolButton>
#include <QHBoxLayout>
#include <QWidget>

class DateSectionWidget;
class DatePickerPopup;

class DatePicker : public QWidget {
	friend class DateSectionWidget;
	Q_OBJECT

	QDateTime          picked;
	DateSectionWidget* section_widget;
	DatePickerPopup*   popup;
	QToolButton*       calendar_button;

	bool hovered = false;
	bool is_active = false;
	bool enabled = true;
	void set_active(bool value);

	bool eventFilter(QObject*, QEvent*) override;
	void paintEvent(QPaintEvent*) override;
	void enterEvent(QEnterEvent*) override;
	void leaveEvent(QEvent*) override;
	void open_popup();

public:
	explicit DatePicker(QDateTime value, QWidget* parent = nullptr);

	QDate get_date() const;
	QDateTime get_value() const;
	void set_date(QDate value);
	void set_value(QDateTime value);
	void set_enabled(bool value);

signals:
	void updated(QDateTime value);
};
