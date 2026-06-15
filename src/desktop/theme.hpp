#pragma once
#include "types/currency.hpp"
#include <QColor>
#include <QLabel>
#include <QFontMetrics>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSize>
#include <QSvgRenderer>

namespace theme {

static constexpr QColor background         = QColor(0x1e, 0x20, 0x24);
static constexpr QColor surface            = QColor(0x2a, 0x2d, 0x33);
static constexpr QColor text               = QColor(0xff, 0xff, 0xff);
static constexpr QColor text_muted         = QColor(0x9a, 0x9e, 0xa8);
static constexpr QColor highlight          = QColor(0xd4, 0xa0, 0x17);
static constexpr QColor info_background    = QColor(0x0d, 0x22, 0x33);
static constexpr QColor info_foreground    = QColor(0x29, 0xb6, 0xf6);
static constexpr QColor success_background = QColor(0x1a, 0x33, 0x1e);
static constexpr QColor success_foreground = QColor(0x4c, 0xd9, 0x6e);
static constexpr QColor warning_background = QColor(0x33, 0x28, 0x0d);
static constexpr QColor warning_foreground = QColor(0xff, 0xc1, 0x07);
static constexpr QColor error_background   = QColor(0x33, 0x11, 0x11);
static constexpr QColor error_foreground   = QColor(0xf4, 0x43, 0x36);

static constexpr QSize toolbar_icon_size = QSize(32, 32);

static inline QSize default_icon_size() {
	QFont font;
	QFontMetrics metrics(font);
	int side = metrics.height();
	return QSize(side, side);
}

static inline QSize label_icon_size(const QWidget* label) {
	int side = label->sizeHint().height();
	return QSize(side, side);
}

static inline QPixmap colored_svg(const QString& path, QColor color, QSize size) {
	QPixmap pixmap(size);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	QSvgRenderer renderer(path);
	renderer.render(&painter);
	painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
	painter.fillRect(pixmap.rect(), color);
	return pixmap;
}

static inline QIcon colored_svg_icon(const QString& path, QColor color, QSize size) {
	return QIcon(colored_svg(path, color, size));
}

static inline QPalette make_palette() {
	QPalette palette;
	palette.setColor(QPalette::Window,          background);
	palette.setColor(QPalette::Base,            surface);
	palette.setColor(QPalette::AlternateBase,   surface.lighter(110));
	palette.setColor(QPalette::WindowText,      text);
	palette.setColor(QPalette::Text,            text);
	palette.setColor(QPalette::ButtonText,      text);
	palette.setColor(QPalette::Button,          surface);
	palette.setColor(QPalette::Highlight,       highlight);
	palette.setColor(QPalette::HighlightedText, background);
	palette.setColor(QPalette::ToolTipBase,     surface);
	palette.setColor(QPalette::ToolTipText,     text);
	palette.setColor(QPalette::PlaceholderText, text_muted);
	return palette;
}

static inline QLabel* currency_label(fundos::currency amount, const fundos::currency_locale::spec& locale, QWidget* parent = nullptr) {
	auto* label = new QLabel(QString::fromStdString(amount.to_string(locale)), parent);
	QPalette palette = label->palette();
	palette.setColor(QPalette::WindowText, amount.minor_units < 0 ? theme::error_foreground : theme::success_foreground);
	label->setPalette(palette);
	return label;
}

static inline QLabel* header_label(const QString& content, QWidget* parent = nullptr) {
	auto* label = new QLabel(content, parent);
	QFont font = label->font();
	font.setPointSize(font.pointSize() + 2);
	font.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
	label->setFont(font);
	QPalette palette = label->palette();
	palette.setColor(QPalette::WindowText, theme::text_muted);
	label->setPalette(palette);
	return label;
}

} // namespace theme
