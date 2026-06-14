#pragma once
#include <vector>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWidget>

class TableView : public QWidget {
	Q_OBJECT

public:
	explicit TableView(bool scrollable, QWidget* parent = nullptr);

	QGridLayout* body_layout() const;
	QWidget* body_container() const;

	void add_header_label(int column, const QString &text);
	void set_footer(QWidget* widget);

	void sync_header();

protected:
	void showEvent(QShowEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	bool eventFilter(QObject* object, QEvent* event) override;

private:
	QWidget*     header;
	QGridLayout* header_layout;
	QWidget*     body_widget;
	QGridLayout* body_grid;
	QWidget*     footer;
	QWidget*     footer_separator;
	QHBoxLayout* footer_layout;

	std::vector<QLabel*> ghost_labels;
	std::vector<QLabel*> header_labels;
};
