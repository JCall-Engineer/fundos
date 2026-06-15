#pragma once
#include <cstdint>
#include "coordinator.hpp"
#include "components/date_picker.hpp"
#include "components/editable_label.hpp"
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

class AccountPage : public QWidget {
	Q_OBJECT

	struct Transaction {
		fundos::db::transaction_history::allocated_transaction record;
		QString      icon_path;
		QColor       background_color  = nullptr;
		QWidget*     background_widget = nullptr;
		QLabel*      icon              = nullptr;
		QLabel*      date              = nullptr;
		QLabel*      memo              = nullptr;
		QLabel*      amount            = nullptr;
		QLabel*      balance           = nullptr;
		QToolButton* details_button    = nullptr;
		QWidget*     details_widget    = nullptr;
	};

	AppCoordinator* app_coordinator;
	fundos::account record;
	EditableLabel* name_label;
	QPushButton* open_close_button;
	QWidget* history_panel;
	QVBoxLayout* history_layout;
	DatePicker* after_picker;
	DatePicker* before_picker;
	bool loading_preset_date_range = false;
	std::string previous_name;
	std::optional<fundos::datetime> previous_closed_at;

	std::vector<Transaction> transaction_widgets;

	void update_open_close_button();
	void clear_history();
	void update_backgrounds();

public:
	explicit AccountPage(AppCoordinator* coordinator, fundos::account opening, QWidget* parent = nullptr);

private slots:
	void rename(QString name);
	void new_transaction();
	void open_transaction(const fundos::db::transaction_history::allocated_transaction& opening);
	void on_toggle_open();
	void fetch_history();

	void on_account_saved(fundos::db::outcome saved);
	void on_history(fundos::db::result<fundos::db::transaction_history> received);

signals:
	void history_requested(int64_t account_id, fundos::datetime after, fundos::datetime before);
	void save_account_requested(fundos::account saving);
	void go_home();
	void import_ofx();
};
