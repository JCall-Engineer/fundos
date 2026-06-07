#include "navigable_row.hpp"
#include "theme.hpp"
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

NavigableRow::NavigableRow(
	const props& data,
	const QString& name,
	QLabel* amount_label,
	QWidget* parent
) : QWidget(parent), properties(data) {
	auto* layout = new QHBoxLayout(this);

	auto* name_label = new QLabel(name, this);
	if (properties.is_closed) {
		QPalette palette = name_label->palette();
		palette.setColor(QPalette::WindowText, theme::text_muted);
		name_label->setPalette(palette);
	}
	auto* open_button = new QToolButton(this);
	open_button->setIcon(theme::colored_svg_icon(":/icons/chevron-right.svg", theme::text, QSize(12, 12)));
	open_button->setAutoRaise(true); // Hides border until hover

	connect(open_button, &QToolButton::clicked, this, [this]() {
		emit clicked(properties.index);
	});

	layout->addWidget(name_label);
	layout->addStretch();
	if (amount_label) {
		layout->addWidget(amount_label);
	}
	layout->addWidget(open_button);
}

void NavigableRow::on_toggle(bool show_closed) {
	setVisible(!properties.is_closed || show_closed || properties.has_amount);
}
