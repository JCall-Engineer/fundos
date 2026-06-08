#pragma once
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QWidget>

/// A label that can be clicked to edit its text inline.
///
/// In view mode a QLabel and pencil button are shown.
/// In edit mode a QLineEdit replaces them.
/// Escape cancels and reverts to the last committed value.
class EditableLabel : public QWidget {
	Q_OBJECT

public:
	explicit EditableLabel(const QString& text, QWidget* parent = nullptr);

	/// @return The last committed text value.
	QString text() const;

	/// Sets the text without entering edit mode.
	void set_text(const QString& text);

signals:
	/// Emitted only when the committed value actually changes.
	/// @param text The new committed text.
	void value_changed(const QString& text);

protected:
	bool eventFilter(QObject* object, QEvent* event) override;

private slots:
	void enter_edit_mode();
	void commit();
	void cancel();

private:
	QString      value;
	QLabel*      label;
	QToolButton* edit_button;
	QLineEdit*   line_edit;
};
