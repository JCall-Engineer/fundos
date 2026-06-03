#pragma once
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSvgRenderer>

namespace theme {
	static constexpr QColor background  = QColor(0x1e, 0x20, 0x24);
	static constexpr QColor surface     = QColor(0x2a, 0x2d, 0x33);
	static constexpr QColor text        = QColor(0xff, 0xff, 0xff);
	static constexpr QColor text_muted  = QColor(0x9a, 0x9e, 0xa8);
	static constexpr QColor highlight   = QColor(0xd4, 0xa0, 0x17);
	static constexpr QColor success     = QColor(0x4c, 0xaf, 0x50);
	static constexpr QColor warning     = QColor(0xff, 0xc1, 0x07);
	static constexpr QColor error       = QColor(0xf4, 0x43, 0x36);

	static inline QIcon colored_icon(const QString& path, QColor color) {
		QPixmap pixmap(24, 24);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing);
		QSvgRenderer renderer(path);
		renderer.render(&painter);
		painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
		painter.fillRect(pixmap.rect(), color);
		return QIcon(pixmap);
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
}
