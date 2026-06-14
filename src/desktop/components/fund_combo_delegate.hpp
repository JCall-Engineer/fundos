#pragma once
#include <optional>
#include "fundos.hpp"
#include <QStandardItem>
#include <QStyledItemDelegate>

class FundComboItem : public QStandardItem {
public:
	int64_t fund_id = 0;
	std::optional<fundos::currency> balance;

	explicit FundComboItem(const QString& name, int64_t fund_id) : QStandardItem(name), fund_id(fund_id) {}
};

class FundComboHeader : public QStandardItem {
public:
	explicit FundComboHeader(const QString& name) : QStandardItem(name) {
		setFlags(Qt::NoItemFlags);
	}
};

class FundComboDelegate : public QStyledItemDelegate {
	Q_OBJECT

	const fundos::currency_locale::spec& locale;

public:
	explicit FundComboDelegate(const fundos::currency_locale::spec& locale, QObject* parent) : QStyledItemDelegate(parent), locale(locale) {}
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
