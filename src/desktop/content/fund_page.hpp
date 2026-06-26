#pragma once
#include <cstdint>
#include "coordinator.hpp"
#include "components/date_picker.hpp"
#include "components/editable_label.hpp"
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class FundPage : public QWidget {
	Q_OBJECT

	AppCoordinator* app_coordinator;
	fundos::fund record;
	EditableLabel* name_label;
	QPushButton* open_close_button;
	QWidget* history_panel;
	QVBoxLayout* history_layout;
	DatePicker* after_picker;
	DatePicker* before_picker;

	/// Suppresses the automatic fetch_history() that DatePicker::updated would otherwise trigger for each picker individually.
	/// The preset buttons change both pickers in sequence and want exactly one fetch afterward, not two (one per picker).
	bool loading_preset_date_range = false;

	// Single-slot rollback buffer for the most recent unconfirmed edit.
	// Set immediately before emitting save_fund_requested, read back in on_fund_saved to revert `record` if the save failed.
	// Only one edit can be in flight at a time this way:
	// a second save before the first's result arrives would overwrite these and lose the ability to roll back the first edit correctly.
	std::string previous_name;
	std::optional<fundos::datetime> previous_closed_at;

	void update_open_close_button();
	void clear_history();

public:
	explicit FundPage(AppCoordinator* coordinator, fundos::fund opening, QWidget* parent = nullptr);

private slots:
	void rename(QString name);
	void on_toggle_open();
	void fetch_history();

	void on_fund_saved(fundos::db::outcome saved);
	void on_history(fundos::db::result<fundos::db::allocation_history> received);

signals:
	void account_requested(fundos::account account, std::optional<fundos::transaction> transaction);
	void history_requested(int64_t fund_id, fundos::datetime after, fundos::datetime before);
	void save_fund_requested(fundos::fund saving);
	void go_home();
};
