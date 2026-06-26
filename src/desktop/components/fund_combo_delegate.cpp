#include "fund_combo_delegate.hpp"
#include "theme.hpp"
#include <QPainter>
#include <QStandardItemModel>

// This widget was designed collaboratively with an AI assistant.
// The interaction model, visual design, and architecture are intentional,
// but the Qt-specific implementation details are less internalized than
// the rest of the codebase. Tread carefully when modifying.

void FundComboDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
	const auto* model = static_cast<const QStandardItemModel*>(index.model());
	const auto* standard_item = model->itemFromIndex(index);

	if (const auto* header_item = dynamic_cast<const FundComboHeader*>(standard_item)) {
		painter->save();
		QFont bold_font = option.font;
		bold_font.setBold(true);
		bold_font.setItalic(true);
		painter->setFont(bold_font);
		painter->setPen(option.palette.color(QPalette::Text));
		painter->drawText(
			option.rect.adjusted(4, 0, 0, 0),
			Qt::AlignVCenter | Qt::AlignLeft,
			header_item->text()
		);
		painter->restore();
		return;
	}

	QStyledItemDelegate::paint(painter, option, index); // draws background, selection highlight, and fund name

	// Draw balance on top, right-aligned, colored by sign
	const auto* fund_item = dynamic_cast<const FundComboItem*>(standard_item);
	if (fund_item == nullptr || !fund_item->balance.has_value()) {
		return;
	}

	const fundos::currency balance = fund_item->balance.value();
	const QString balance_text = QString::fromStdString(balance.to_string(locale));

	painter->save();
	painter->setPen(balance.minor_units < 0 ? theme::error_foreground : theme::success_foreground);
	painter->drawText(
		option.rect.adjusted(0, 0, -4, 0),
		Qt::AlignVCenter | Qt::AlignRight,
		balance_text
	);
	painter->restore();
}

QSize FundComboDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
	QSize size = QStyledItemDelegate::sizeHint(option, index);
	const auto* model = static_cast<const QStandardItemModel*>(index.model());
	if (dynamic_cast<const FundComboHeader*>(model->itemFromIndex(index))) {
		size.setHeight(size.height() + 4);
	}
	return size;
}
