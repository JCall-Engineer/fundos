#include "table_view.hpp"
#include "theme.hpp"
#include <QScrollArea>
#include <QTimer>

TableView::TableView(bool scrollable, QWidget* parent) : QWidget(parent) {
	setAttribute(Qt::WA_StyledBackground, true);
	setObjectName(QStringLiteral("table_view"));
	setStyleSheet(QStringLiteral(
		"#table_view { border: 1px solid %1; border-radius: 4px; background-color: %2; }"
	).arg(theme::text_muted.name(), theme::surface.name()));

	QVBoxLayout* root = new QVBoxLayout(this);
	root->setSpacing(0);
	root->setContentsMargins(1, 1, 1, 1);

	header = new QWidget(this);
	header->setAttribute(Qt::WA_StyledBackground, true);
	header->setStyleSheet(QStringLiteral(
		"background-color: %1; border-radius: 4px 4px 0 0;"
	).arg(theme::surface.lighter(110).name()));
	header_layout = new QGridLayout(header);
	header_layout->setContentsMargins(8, 8, 8, 8);

	auto* header_separator = new QWidget(this);
	header_separator->setFixedHeight(1);
	header_separator->setStyleSheet(QStringLiteral(
		"background-color: %1;"
	).arg(theme::text_muted.name()));

	root->addWidget(header);
	root->addWidget(header_separator);

	body_widget = new QWidget();
	body_widget->installEventFilter(this);
	body_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	body_grid = new QGridLayout(body_widget);
	body_grid->setContentsMargins(8, 8, 8, 8);
	body_grid->setAlignment(Qt::AlignTop);

	if (scrollable) {
		auto* body_scroll = new QScrollArea(this);
		body_scroll->setWidget(body_widget);
		body_scroll->setWidgetResizable(true);
		body_scroll->setFrameShape(QFrame::NoFrame);
		body_scroll->viewport()->setContentsMargins(0, 0, 0, 0);
		body_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		body_scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
		body_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		root->addWidget(body_scroll);
	} else {
		root->addWidget(body_widget);
	}

	footer_separator = new QWidget(this);
	footer_separator->setFixedHeight(1);
	footer_separator->setStyleSheet(QStringLiteral(
		"background-color: %1;"
	).arg(theme::text_muted.name()));

	footer = new QWidget(this);
	footer->setAttribute(Qt::WA_StyledBackground, true);
	footer->setStyleSheet(QStringLiteral(
		"background-color: %1; border-radius: 0 0 4px 4px;"
	).arg(theme::surface.lighter(110).name()));
	footer_layout = new QHBoxLayout(footer);
	footer_layout->setContentsMargins(8, 8, 8, 8);
	footer->setVisible(false);
	footer_separator->setVisible(false);

	root->addWidget(footer_separator);
	root->addWidget(footer);
}

QGridLayout* TableView::body_layout() const {
	return body_grid;
}
QWidget* TableView::body_container() const {
	return body_widget;
}

void TableView::add_header_label(int column, const QString &text) {
	QLabel* ghost = new QLabel(text, body_widget);
	ghost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	ghost->setFixedHeight(0);
	ghost->setStyleSheet(QStringLiteral("background: transparent; color: transparent;"));
	body_grid->addWidget(ghost, 0, column);

	QLabel* real = new QLabel(text, header);
	QFont header_font = font();
	header_font.setBold(true);
	real->setFont(header_font);
	header_layout->addWidget(real, 0, column);

	ghost_labels.push_back(ghost);
	header_labels.push_back(real);
}

void TableView::set_footer(QWidget *widget) {
	footer_layout->addWidget(widget);
	footer->setVisible(true);
	footer_separator->setVisible(true);
}

void TableView::sync_header() {
	for (size_t i = 0; i < ghost_labels.size(); ++i) {
		int width = ghost_labels[i]->geometry().width();
		header_labels[i]->setFixedWidth(width);
	}
}

void TableView::showEvent(QShowEvent* event) {
	QWidget::showEvent(event);
	QTimer::singleShot(0, this, &TableView::sync_header);
}

void TableView::resizeEvent(QResizeEvent* event) {
	QWidget::resizeEvent(event);
	QTimer::singleShot(0, this, &TableView::sync_header);
}

bool TableView::eventFilter(QObject* object, QEvent* event) {
	if (object == body_widget && event->type() == QEvent::LayoutRequest) {
		QTimer::singleShot(0, this, &TableView::sync_header);
	}
	return QWidget::eventFilter(object, event);
}
