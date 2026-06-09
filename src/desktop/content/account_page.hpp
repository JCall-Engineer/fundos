#pragma once
#include <cstdint>
#include "context.hpp"
#include "components/date_picker.hpp"
#include "components/editable_label.hpp"
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class AccountPage : public QWidget {
	Q_OBJECT

	std::shared_ptr<AppContext> context;
	fundos::account record;
	EditableLabel* name_label;
	QPushButton* open_close_button;
	QVBoxLayout* history_layout;
	DatePicker* after_picker;
	DatePicker* before_picker;
	bool loading_preset = false;

	void update_open_close_button();

public:
	explicit AccountPage(std::shared_ptr<AppContext> ctx, fundos::account opening, QWidget* parent = nullptr);

private slots:
	void rename(QString name);
	void new_transaction();
	void on_toggle_open();
	void fetch_history();

signals:
	void db_outcome(const fundos::db::outcome& outcome);
	void go_home();
	void import_ofx();
};
