#pragma once
#include <cstdint>
#include "context.hpp"
#include "components/editable_label.hpp"
#include <QDateTimeEdit>
#include <QPushButton>
#include <QWidget>

class AccountPage : public QWidget {
	Q_OBJECT

	std::shared_ptr<AppContext> context;
	fundos::account record;
	EditableLabel* name_label;
	QPushButton* close_button;
	QDateTimeEdit* after_picker;
	QDateTimeEdit* before_picker;

	void update_close_button();

public:
	explicit AccountPage(std::shared_ptr<AppContext> ctx, fundos::account opening, QWidget* parent = nullptr);

private slots:
	void rename(QString name);
	void new_transaction();
	void on_toggle_open();
	void refresh();

signals:
	void db_outcome(const fundos::db::outcome& outcome);
	void go_home();
	void import_ofx();
};
