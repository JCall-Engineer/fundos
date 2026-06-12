#pragma once
#include <cstdint>
#include "coordinator.hpp"
#include "components/date_picker.hpp"
#include "components/editable_label.hpp"
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class AccountPage : public QWidget {
	Q_OBJECT

	AppCoordinator* app_coordinator;
	fundos::account record;
	EditableLabel* name_label;
	QPushButton* open_close_button;
	QVBoxLayout* history_layout;
	DatePicker* after_picker;
	DatePicker* before_picker;
	bool loading_preset_date_range = false;
	std::string previous_name;
	std::optional<fundos::datetime> previous_closed_at;

	void update_open_close_button();

public:
	explicit AccountPage(AppCoordinator* coordinator, fundos::account opening, QWidget* parent = nullptr);

private slots:
	void rename(QString name);
	void new_transaction();
	void on_toggle_open();
	void fetch_history();

	void on_account_saved(fundos::db::outcome saved);

signals:
	void save_account_requested(fundos::account saving);
	void go_home();
	void import_ofx();
};
