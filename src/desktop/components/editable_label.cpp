#include "editable_label.hpp"
#include "theme.hpp"
#include <QFont>
#include <QHBoxLayout>
#include <QKeyEvent>

static QFont make_label_font(const QFont& base) {
	QFont font = base;
	font.setPointSize(font.pointSize() + 2);
	font.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
	return font;
}

EditableLabel::EditableLabel(const QString& text, QWidget* parent)
	: QWidget(parent)
	, value(text)
	, label(new QLabel(text, this))
	, edit_button(new QToolButton(this))
	, line_edit(new QLineEdit(text, this))
{
	auto* layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(6);
	layout->addWidget(label);
	layout->addWidget(edit_button);
	layout->addWidget(line_edit);
	layout->addStretch();

	const QFont font = make_label_font(label->font());
	label->setFont(font);
	line_edit->setFont(font);
	line_edit->setFrame(false);
	line_edit->setVisible(false);
	line_edit->installEventFilter(this);

	const int icon_size = label->sizeHint().height();
	const QSize button_size(icon_size, icon_size);
	edit_button->setIcon(theme::colored_svg_icon(
		QStringLiteral(":/icons/pencil.svg"),
		theme::text_muted,
		button_size
	));
	edit_button->setIconSize(button_size);
	edit_button->setFixedSize(button_size);
	edit_button->setAutoRaise(true);

	line_edit->setStyleSheet(QStringLiteral(
		"QLineEdit {"
		"  background: transparent;"
		"  border: none;"
		"  border-bottom: 1px solid %1;"
		"}"
	).arg(theme::highlight.name()));

	connect(edit_button, &QToolButton::clicked,      this, &EditableLabel::enter_edit_mode);
	connect(line_edit,   &QLineEdit::editingFinished, this, &EditableLabel::commit);
}

QString EditableLabel::text() const {
	return value;
}

void EditableLabel::set_text(const QString& text) {
	value = text;
	label->setText(text);
	line_edit->setText(text);
}

bool EditableLabel::eventFilter(QObject* object, QEvent* event) {
	if (object == line_edit && event->type() == QEvent::KeyPress) {
		auto* key_event = static_cast<QKeyEvent*>(event);
		if (key_event->key() == Qt::Key_Escape) {
			cancel();
			return true;
		}
	}
	return QWidget::eventFilter(object, event);
}

void EditableLabel::enter_edit_mode() {
	label->setVisible(false);
	edit_button->setVisible(false);
	line_edit->setVisible(true);
	line_edit->selectAll();
	line_edit->setFocus();
}

void EditableLabel::commit() {
	if (line_edit->text() == value) {
		cancel();
		return;
	}
	value = line_edit->text();
	label->setText(value);
	line_edit->setVisible(false);
	label->setVisible(true);
	edit_button->setVisible(true);
	emit value_changed(value);
}

void EditableLabel::cancel() {
	line_edit->setText(value);
	line_edit->setVisible(false);
	label->setVisible(true);
	edit_button->setVisible(true);
}
