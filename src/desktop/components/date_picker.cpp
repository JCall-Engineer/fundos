#include <optional>
#include "date_picker.hpp"
#include "theme.hpp"
#include <QCalendar>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

// This widget was designed collaboratively with an AI assistant.
// The interaction model, visual design, and architecture are intentional,
// but the Qt-specific implementation details are less internalized than
// the rest of the codebase. Tread carefully when modifying.

// ─── SpinnerColumn ───────────────────────────────────────────────────────

class SpinnerColumn : public QWidget {
	std::function<void(int)> on_scroll;

protected:
	void wheelEvent(QWheelEvent* event) override {
		int delta = event->angleDelta().y() > 0 ? -1 : 1;
		on_scroll(delta);
		event->accept();
	}

public:
	explicit SpinnerColumn(std::function<void(int)> callback, QWidget* parent = nullptr)
		: QWidget(parent)
		, on_scroll(std::move(callback))
	{
	}
};

// ─── DateSectionWidget ───────────────────────────────────────────────────────

class DateSectionWidget : public QWidget {
	enum class Section { year, month, day };

	QDate                  picked_date;
	std::optional<Section> focused_section;
	DatePicker*            owner;

	static constexpr int section_padding = 6;

	int year_width  = 0;
	int month_width = 0;
	int day_width   = 0;
	int sep_width   = 0;

	void compute_widths() {
		QFontMetrics metrics(font());
		year_width  = metrics.horizontalAdvance(QStringLiteral("0000")) + section_padding * 2;
		month_width = metrics.horizontalAdvance(QStringLiteral("00"))   + section_padding * 2;
		day_width   = metrics.horizontalAdvance(QStringLiteral("00"))   + section_padding * 2;
		sep_width   = metrics.horizontalAdvance(QStringLiteral(" / "));
	}

	QRect year_rect() const {
		return QRect(0, 0, year_width, height());
	}
	QRect month_rect() const {
		return QRect(year_width + sep_width, 0, month_width, height());
	}
	QRect day_rect() const {
		return QRect(year_width + sep_width + month_width + sep_width, 0, day_width, height());
	}

	Section section_at(QPoint position) const {
		if (year_rect().contains(position))  { return Section::year; }
		if (month_rect().contains(position)) { return Section::month; }
		return Section::day;
	}

	void increment_focused(int delta) {
		if (!focused_section.has_value()) { return; }
		switch (*focused_section) {
		case Section::year:
			picked_date = picked_date.addYears(delta);
			break;
		case Section::month:
			picked_date = picked_date.addMonths(delta);
			break;
		case Section::day:
			picked_date = picked_date.addDays(delta);
			break;
		}
		owner->set_date(picked_date);
		update();
	}

	void advance_section() {
		if (!focused_section.has_value()) { return; }
		switch (*focused_section) {
		case Section::year:
			focused_section = Section::month;
			break;
		case Section::month:
			focused_section = Section::day;
			break;
		case Section::day:
			break;
		}
		update();
	}

	void retreat_section() {
		if (!focused_section.has_value()) { return; }
		switch (*focused_section) {
		case Section::year:
			break;
		case Section::month:
			focused_section = Section::year;
			break;
		case Section::day:
			focused_section = Section::month;
			break;
		}
		update();
	}

	QString digit_buffer;

	void handle_digit(const QString& digit) {
		if (!focused_section.has_value()) { return; }
		digit_buffer += digit;
		switch (*focused_section) {
		case Section::year: {
			if (digit_buffer.length() == 4) {
				int year = digit_buffer.toInt();
				picked_date = QDate(year, picked_date.month(), picked_date.day());
				owner->set_date(picked_date);
				advance_section();
				digit_buffer.clear();
			}
			break;
		}
		case Section::month: {
			if (digit_buffer.length() == 2) {
				int month = digit_buffer.toInt();
				if (month >= 1 && month <= 12) {
					picked_date = QDate(picked_date.year(), month, picked_date.day());
					owner->set_date(picked_date);
					advance_section();
				}
				digit_buffer.clear();
			} else {
				int first = digit_buffer[0].digitValue();
				if (first > 1) {
					int month = digit_buffer.toInt();
					if (month >= 1 && month <= 12) {
						picked_date = QDate(picked_date.year(), month, picked_date.day());
						owner->set_date(picked_date);
						advance_section();
					}
					digit_buffer.clear();
				}
			}
			break;
		}
		case Section::day: {
			if (digit_buffer.length() == 2) {
				int day = digit_buffer.toInt();
				int days_in_month = picked_date.daysInMonth();
				if (day >= 1 && day <= days_in_month) {
					picked_date = QDate(picked_date.year(), picked_date.month(), day);
					owner->set_date(picked_date);
				}
				digit_buffer.clear();
			} else {
				int first = digit_buffer[0].digitValue();
				if (first > 3) {
					int day = digit_buffer.toInt();
					int days_in_month = picked_date.daysInMonth();
					if (day >= 1 && day <= days_in_month) {
						picked_date = QDate(picked_date.year(), picked_date.month(), day);
						owner->set_date(picked_date);
					}
					digit_buffer.clear();
				}
			}
			break;
		}
		}
		update();
	}

	void draw_section(
		QPainter&      painter,
		const QRect&   rect,
		const QString& text,
		bool           is_focused
	) {
		if (is_focused) {
			painter.fillRect(rect, theme::info_background);
		}
		painter.setPen(theme::text);
		painter.drawText(rect, Qt::AlignCenter, text);
	}

protected:
	void paintEvent(QPaintEvent*) override {
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);

		draw_section(
			painter,
			year_rect(),
			QString::number(picked_date.year()).rightJustified(4, u'0'),
			focused_section == Section::year
		);

		painter.setPen(theme::text_muted);
		painter.drawText(
			QRect(year_width, 0, sep_width, height()),
			Qt::AlignCenter,
			QStringLiteral(" / ")
		);

		draw_section(
			painter,
			month_rect(),
			QString::number(picked_date.month()).rightJustified(2, u'0'),
			focused_section == Section::month
		);

		painter.setPen(theme::text_muted);
		painter.drawText(
			QRect(year_width + sep_width + month_width, 0, sep_width, height()),
			Qt::AlignCenter,
			QStringLiteral(" / ")
		);

		draw_section(
			painter,
			day_rect(),
			QString::number(picked_date.day()).rightJustified(2, u'0'),
			focused_section == Section::day
		);
	}

	void keyPressEvent(QKeyEvent* event) override {
		if (event->key() >= Qt::Key_0 && event->key() <= Qt::Key_9) {
			handle_digit(event->text());
			return;
		}
		switch (event->key()) {
		case Qt::Key_Up:    increment_focused(1);  break;
		case Qt::Key_Down:  increment_focused(-1); break;
		case Qt::Key_Left:  retreat_section();     break;
		case Qt::Key_Right: advance_section();     break;
		default:            QWidget::keyPressEvent(event); break;
		}
	}

	void mousePressEvent(QMouseEvent* event) override {
		setFocus();
		focused_section = section_at(event->pos());
		digit_buffer.clear();
		update();
	}

	void focusInEvent(QFocusEvent*) override {
		focused_section = Section::year;
		owner->set_active(true);
		update();
	}

	void focusOutEvent(QFocusEvent*) override {
		focused_section = std::nullopt;
		digit_buffer.clear();
		owner->set_active(false);
		update();
	}

	void wheelEvent(QWheelEvent* event) override {
		int delta = event->angleDelta().y() > 0 ? 1 : -1;
		increment_focused(delta);
		event->accept();
	}

	QSize sizeHint() const override {
		QFontMetrics metrics(font());
		int total_width = year_width + sep_width + month_width + sep_width + day_width;
		return QSize(total_width, metrics.height() + section_padding * 2);
	}

public:
	explicit DateSectionWidget(QDate value, DatePicker* owner, QWidget* parent = nullptr)
		: QWidget(parent)
		, picked_date(value)
		, owner(owner)
	{
		setFocusPolicy(Qt::StrongFocus);
		setAttribute(Qt::WA_Hover);
		compute_widths();
	}

	void update_date(QDate value) {
		picked_date = value;
		update();
	}
};

// ─── DatePickerPopup ─────────────────────────────────────────────────────────

class DatePickerPopup : public QWidget {
	static constexpr int spinner_visible_rows = 5;

	enum class View { calendar, spinner };

	QDate           current_date;
	DatePicker*     owner;
	QStackedWidget* stacked;

	QToolButton* prev_month_button;
	QToolButton* next_month_button;
	QPushButton* month_year_label;

	QPushButton* day_buttons[6][7];
	QPushButton* today_button;

	QToolButton* month_up_button;
	QToolButton* month_down_button;
	QToolButton* year_up_button;
	QToolButton* year_down_button;
	QPushButton* month_labels[5];
	QPushButton* year_labels[5];

	int spinner_month;
	int spinner_year;

	void switch_view(View view) {
		stacked->setCurrentIndex(static_cast<int>(view));
		bool is_calendar = view == View::calendar;
		prev_month_button->setVisible(is_calendar);
		next_month_button->setVisible(is_calendar);
		month_year_label->setText(QStringLiteral("%1 %2")
			.arg(current_date.toString(QStringLiteral("MMMM yyyy")))
			.arg(is_calendar ? u'▾' : u'▴')
		);
	}

	void refresh_calendar() {
		QDate today         = QDate::currentDate();
		QLocale locale      = QLocale::system();
		Qt::DayOfWeek first_day = locale.firstDayOfWeek();

		bool is_calendar = stacked->currentIndex() == static_cast<int>(View::calendar);
		month_year_label->setText(QStringLiteral("%1 %2")
			.arg(current_date.toString(QStringLiteral("MMMM yyyy")))
			.arg(is_calendar ? u'▾' : u'▴')
		);

		QDate first_of_month(current_date.year(), current_date.month(), 1);
		int first_weekday = first_of_month.dayOfWeek();
		int offset        = (first_weekday - static_cast<int>(first_day) + 7) % 7;
		QDate cell_date   = first_of_month.addDays(-offset);

		for (int row = 0; row < 6; ++row) {
			for (int column = 0; column < 7; ++column) {
				QPushButton* button  = day_buttons[row][column];
				button->setText(QString::number(cell_date.day()));

				bool is_today    = cell_date == today;
				bool is_selected = cell_date == owner->date();
				bool is_current  = cell_date.month() == current_date.month();

				QString text_color =
					is_today   ? theme::warning_foreground.name() :
					is_current ? theme::text.name()               :
					theme::text_muted.name();

				QString background_color =
					is_selected ? theme::info_background.name() :
					QStringLiteral("transparent");

				button->setStyleSheet(QStringLiteral(
					"QPushButton { color: %1; background: %2; border: none; border-radius: 4px; }"
					"QPushButton:hover { background: %3; }"
				).arg(
					text_color,
					background_color,
					theme::surface.name()
				));

				QDate captured = cell_date;
				disconnect(button, &QPushButton::clicked, nullptr, nullptr);
				connect(button, &QPushButton::clicked, this, [this, captured]() {
					owner->set_date(captured);
					close();
				});

				cell_date = cell_date.addDays(1);
			}
		}
	}

	void refresh_spinner() {
		QLocale locale = QLocale::system();

		for (int index = 0; index < spinner_visible_rows; ++index) {
			int month_offset = spinner_month - 2 + index;
			int month        = ((month_offset - 1 + 12) % 12) + 1;
			int year         = spinner_year - 2 + index;
			bool is_center   = index == 2;

			month_labels[index]->setText(locale.standaloneMonthName(month, QLocale::ShortFormat));
			month_labels[index]->setStyleSheet(QStringLiteral(
				"QPushButton { color: %1; font-weight: %2; border: none; background: transparent; }"
				"QPushButton:hover { color: %3; }"
			).arg(
				is_center ? theme::text.name() : theme::text_muted.name(),
				is_center ? QStringLiteral("500") : QStringLiteral("400"),
				theme::text.name()
			));
			disconnect(month_labels[index], &QPushButton::clicked, nullptr, nullptr);
			connect(month_labels[index], &QPushButton::clicked, this, [this, month]() {
				spinner_month = month;
				refresh_spinner();
			});

			year_labels[index]->setText(QString::number(year));
			year_labels[index]->setStyleSheet(QStringLiteral(
				"QPushButton { color: %1; font-weight: %2; border: none; background: transparent; }"
				"QPushButton:hover { color: %3; }"
			).arg(
				is_center ? theme::text.name() : theme::text_muted.name(),
				is_center ? QStringLiteral("500") : QStringLiteral("400"),
				theme::text.name()
			));
			disconnect(year_labels[index], &QPushButton::clicked, nullptr, nullptr);
			connect(year_labels[index], &QPushButton::clicked, this, [this, year]() {
				spinner_year = year;
				refresh_spinner();
			});

			month_year_label->setText(QStringLiteral("%1 %2")
				.arg(QDate(spinner_year, spinner_month, 1).toString(QStringLiteral("MMMM yyyy")))
				.arg(u'▴')
			);
		}

		month_year_label->setText(QStringLiteral("%1 %2")
			.arg(QDate(spinner_year, spinner_month, 1).toString(QStringLiteral("MMMM yyyy")))
			.arg(u'▴')
		);
	}

	void commit_spinner() {
		int safe_day = qMin(owner->date().day(), QDate(spinner_year, spinner_month, 1).daysInMonth());
		current_date = QDate(spinner_year, spinner_month, safe_day);
		refresh_calendar();
		switch_view(View::calendar);
	}

	static QToolButton* make_chevron_button(const QString& icon_path, QWidget* parent) {
		QToolButton* button = new QToolButton(parent);
		button->setIcon(theme::colored_svg_icon(icon_path, theme::text_muted, QSize(16, 16)));
		button->setStyleSheet(QStringLiteral(
			"QToolButton { border: none; background: transparent; }"
		));
		button->setAttribute(Qt::WA_Hover);
		return button;
	}

public:
	explicit DatePickerPopup(QDate value, DatePicker* owner_widget, QWidget* parent = nullptr)
		: QWidget(parent, Qt::Popup)
		, current_date(value)
		, owner(owner_widget)
		, spinner_month(value.month())
		, spinner_year(value.year())
	{
		setStyleSheet(QStringLiteral("background: %1;").arg(theme::surface.name()));

		QVBoxLayout* main_layout = new QVBoxLayout(this);

		// ── nav header ────────────────────────────────────────────────────────

		QHBoxLayout* nav_layout = new QHBoxLayout;

		prev_month_button = make_chevron_button(QStringLiteral(":/icons/chevron-left.svg"),  this);
		next_month_button = make_chevron_button(QStringLiteral(":/icons/chevron-right.svg"), this);

		month_year_label = new QPushButton;
		month_year_label->setFlat(true);
		month_year_label->setStyleSheet(QStringLiteral(
			"QPushButton { color: %1; background: transparent; border: none; font-weight: 500; }"
			"QPushButton:hover { color: %2; }"
		).arg(theme::text.name(), theme::highlight.name()));

		nav_layout->addWidget(prev_month_button);
		nav_layout->addWidget(month_year_label, 1, Qt::AlignCenter);
		nav_layout->addWidget(next_month_button);

		main_layout->addLayout(nav_layout);

		// ── stacked middle content ────────────────────────────────────────────

		stacked = new QStackedWidget(this);

		// calendar page

		QWidget*     calendar_page = new QWidget;
		QVBoxLayout* cal_layout    = new QVBoxLayout(calendar_page);
		cal_layout->setContentsMargins(0, 0, 0, 0);

		QLocale      locale    = QLocale::system();
		Qt::DayOfWeek first_day = locale.firstDayOfWeek();

		QGridLayout* grid = new QGridLayout;
		grid->setSpacing(2);

		for (int column = 0; column < 7; ++column) {
			int day_index    = (static_cast<int>(first_day) - 1 + column) % 7 + 1;
			QString day_name = locale.dayName(day_index, QLocale::ShortFormat);
			QLabel* header   = new QLabel(day_name);
			header->setAlignment(Qt::AlignCenter);
			header->setStyleSheet(QStringLiteral(
				"color: %1; font-weight: 500;"
			).arg(theme::text_muted.name()));
			grid->addWidget(header, 0, column);
		}

		for (int row = 0; row < 6; ++row) {
			for (int column = 0; column < 7; ++column) {
				QPushButton* button      = new QPushButton;
				day_buttons[row][column] = button;
				button->setFixedSize(32, 32);
				button->setStyleSheet(QStringLiteral(
					"QPushButton { border: none; border-radius: 4px; }"
				));
				grid->addWidget(button, row + 1, column);
			}
		}

		cal_layout->addLayout(grid);
		stacked->addWidget(calendar_page);

		// spinner page

		QWidget*     spinner_page    = new QWidget;
		QHBoxLayout* spinner_columns = new QHBoxLayout(spinner_page);

		SpinnerColumn* month_column_widget = new SpinnerColumn([this](int delta) {
			spinner_month = ((spinner_month - 1 + delta + 12) % 12) + 1;
			refresh_spinner();
		}, spinner_page);
		QVBoxLayout* month_column = new QVBoxLayout(month_column_widget);
		month_up_button           = make_chevron_button(QStringLiteral(":/icons/chevron-up.svg"),   this);
		month_down_button         = make_chevron_button(QStringLiteral(":/icons/chevron-down.svg"), this);

		month_column->addWidget(month_up_button, 0, Qt::AlignHCenter);
		for (int index = 0; index < spinner_visible_rows; ++index) {
			month_labels[index] = new QPushButton;
			month_labels[index]->setFlat(true);
			month_labels[index]->setStyleSheet(QStringLiteral("border: none; background: transparent;"));
			month_column->addWidget(month_labels[index]);
		}
		month_column->addWidget(month_down_button, 0, Qt::AlignHCenter);

		SpinnerColumn* year_column_widget = new SpinnerColumn([this](int delta) {
			spinner_year += delta;
			refresh_spinner();
		}, spinner_page);
		QVBoxLayout* year_column = new QVBoxLayout(year_column_widget);
		year_up_button           = make_chevron_button(QStringLiteral(":/icons/chevron-up.svg"),   this);
		year_down_button         = make_chevron_button(QStringLiteral(":/icons/chevron-down.svg"), this);

		year_column->addWidget(year_up_button, 0, Qt::AlignHCenter);
		for (int index = 0; index < spinner_visible_rows; ++index) {
			year_labels[index] = new QPushButton;
			year_labels[index]->setFlat(true);
			year_labels[index]->setStyleSheet(QStringLiteral("border: none; background: transparent;"));
			year_column->addWidget(year_labels[index]);
		}
		year_column->addWidget(year_down_button, 0, Qt::AlignHCenter);

		spinner_columns->addWidget(month_column_widget);
		spinner_columns->addWidget(year_column_widget);
		stacked->addWidget(spinner_page);

		main_layout->addWidget(stacked);

		// ── today button ──────────────────────────────────────────────────────

		today_button = new QPushButton(tr("Today"));
		today_button->setStyleSheet(QStringLiteral(
			"QPushButton { color: %1; background: transparent; border: none; }"
			"QPushButton:hover { color: %2; }"
		).arg(theme::text_muted.name(), theme::text.name()));
		main_layout->addWidget(today_button, 0, Qt::AlignCenter);

		// ── connections ───────────────────────────────────────────────────────

		connect(prev_month_button, &QToolButton::clicked, this, [this]() {
			current_date = current_date.addMonths(-1);
			refresh_calendar();
		});
		connect(next_month_button, &QToolButton::clicked, this, [this]() {
			current_date = current_date.addMonths(1);
			refresh_calendar();
		});
		connect(month_year_label, &QPushButton::clicked, this, [this]() {
			bool going_to_spinner = stacked->currentIndex() == static_cast<int>(View::calendar);
			if (going_to_spinner) {
				spinner_month = current_date.month();
				spinner_year  = current_date.year();
				refresh_spinner();
				switch_view(View::spinner);
			} else {
				commit_spinner();
			}
		});
		connect(month_up_button,   &QToolButton::clicked, this, [this]() {
			spinner_month = (spinner_month % 12) + 1;
			refresh_spinner();
		});
		connect(month_down_button, &QToolButton::clicked, this, [this]() {
			spinner_month = ((spinner_month - 2 + 12) % 12) + 1;
			refresh_spinner();
		});
		connect(year_up_button,    &QToolButton::clicked, this, [this]() {
			++spinner_year;
			refresh_spinner();
		});
		connect(year_down_button,  &QToolButton::clicked, this, [this]() {
			spinner_year--;
			refresh_spinner();
		});
		connect(today_button, &QPushButton::clicked, this, [this]() {
			current_date = QDate::currentDate();
			switch_view(View::calendar);
			refresh_calendar();
		});

		refresh_calendar();
		refresh_spinner();
		switch_view(View::calendar);
	}

	void show_for_date(QDate value) {
		current_date  = value;
		spinner_month = value.month();
		spinner_year  = value.year();
		refresh_calendar();
		refresh_spinner();
		switch_view(View::calendar);
	}
};

// ─── DatePicker ──────────────────────────────────────────────────────────────

DatePicker::DatePicker(QDate value, QWidget* parent)
	: QWidget(parent)
	, picked_date(value)
{
	setAttribute(Qt::WA_Hover);
	setFocusPolicy(Qt::StrongFocus);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	QHBoxLayout* layout = new QHBoxLayout(this);
	layout->setContentsMargins(4, 2, 4, 2);
	layout->setSpacing(4);

	section_widget = new DateSectionWidget(value, this, this);
	layout->addWidget(section_widget, 1);

	calendar_button = new QToolButton(this);
	calendar_button->setIcon(theme::colored_svg_icon(QStringLiteral(":/icons/calendar.svg"), theme::text_muted, QSize(20, 20)));
	calendar_button->setStyleSheet(QStringLiteral(
		"QToolButton { border: none; background: transparent; }"
	));
	calendar_button->setAttribute(Qt::WA_Hover);
	layout->addWidget(calendar_button);

	popup = new DatePickerPopup(value, this, this);
	popup->installEventFilter(this);
	popup->hide();

	qApp->installEventFilter(this);

	connect(calendar_button, &QToolButton::clicked, this, &DatePicker::open_popup);
}

void DatePicker::set_active(bool value) {
	is_active = value;
	update();
}

QDate DatePicker::date() const {
	return picked_date;
}

void DatePicker::set_date(QDate value) {
	picked_date = value;
	section_widget->update_date(value);
	update();
	emit updated(value);
}

void DatePicker::set_enabled(bool value) {
	enabled = value;
	section_widget->setEnabled(value);
	calendar_button->setEnabled(value);
	if (!value && popup->isVisible()) {
		popup->hide();
	}
	update();
}

void DatePicker::open_popup() {
	popup->show_for_date(picked_date);
	popup->move(mapToGlobal(rect().bottomLeft()));
	popup->show();
	is_active = true;
	update();
}

bool DatePicker::eventFilter(QObject* object, QEvent* event) {
	if (object == popup && event->type() == QEvent::Hide) {
		is_active = false;
		update();
	}
	if (event->type() == QEvent::MouseButtonPress) {
		QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
		bool outside_self  = !geometry().contains(mapFromGlobal(mouse_event->globalPos()));
		bool outside_popup = !popup->geometry().contains(mouse_event->globalPos());
		if (outside_self && outside_popup && section_widget->hasFocus()) {
			section_widget->clearFocus();
		}
	}
	return QWidget::eventFilter(object, event);
}

void DatePicker::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	QColor border_color = (hovered || is_active) ? theme::text_muted : theme::surface;
	painter.setPen(QPen(border_color, 1));
	painter.setBrush(theme::surface);
	painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);

	if (!enabled) {
		painter.fillRect(rect(), QColor(0, 0, 0, 60));
	}
}

void DatePicker::enterEvent(QEnterEvent*) {
	hovered = true;
	update();
}

void DatePicker::leaveEvent(QEvent*) {
	hovered = false;
	update();
}
