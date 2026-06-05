#include "navigable_row.hpp"
#include "theme.hpp"
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

NavigableRow::NavigableRow(int index, const QString& name, QLabel* amount_label, QWidget* parent) : QWidget(parent) {
	auto* layout = new QHBoxLayout(this);

	auto* name_label = new QLabel(name, this);
	auto* open_button = new QToolButton(this);
	open_button->setIcon(theme::colored_svg_icon(":/icons/chevron-right.svg", theme::text, QSize(12, 12)));
	open_button->setAutoRaise(true); // Hides border until hover

	connect(open_button, &QToolButton::clicked, this, [this, index]() {
		emit clicked(index);
	});

	layout->addWidget(name_label);
	layout->addStretch();
	if (amount_label) {
		layout->addWidget(amount_label);
	}
	layout->addWidget(open_button);
}
