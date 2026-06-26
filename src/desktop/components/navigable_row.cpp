#include "navigable_row.hpp"
#include "theme.hpp"
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

NavigableRow::NavigableRow(
	const props& data,
	const QString& name,
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
	open_button->setIcon(theme::colored_svg_icon(":/icons/chevron-right.svg", theme::text, theme::toolbar_icon_size));
	open_button->setAutoRaise(true); // Hides border until hover

	connect(open_button, &QToolButton::clicked, this, [this]() {
		emit clicked(properties.index);
	});

	layout->addWidget(name_label);
	layout->addStretch();
	layout->addWidget(open_button);
}

void NavigableRow::set_amount(QLabel* amount_label, bool has_amount) {
	amount_label->setParent(this);
	// Index 2 = after name_label (0) and stretch (1), before open_button (last).
	static_cast<QHBoxLayout*>(layout())->insertWidget(2, amount_label);
	properties.has_amount = has_amount;
}

void NavigableRow::on_toggle(bool show_closed) {
	// Closed rows with a balance remain visible regardless of show_closed, so the user doesn't lose sight of accounts/funds that still hold money.
	setVisible(!properties.is_closed || show_closed || properties.has_amount);
}
