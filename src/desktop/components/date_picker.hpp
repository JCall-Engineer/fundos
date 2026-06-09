#pragma once
#include <QDate>
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

	QDate              picked_date;
	DateSectionWidget* section_widget;
	DatePickerPopup*   popup;
	QToolButton*       calendar_button;

	bool hovered = false;
	bool is_active = false;
	void set_active(bool value);

	bool eventFilter(QObject*, QEvent*) override;
	void paintEvent(QPaintEvent*) override;
	void enterEvent(QEnterEvent*) override;
	void leaveEvent(QEvent*) override;
	void open_popup();

public:
	explicit DatePicker(QDate value, QWidget* parent = nullptr);

	QDate date() const;
	void set_date(QDate value);

signals:
	void updated(QDate value);
};
