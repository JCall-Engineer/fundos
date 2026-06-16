#pragma once
#include <optional>
#include <unordered_map>
#include <vector>
#include "fundos.hpp"
#include "coordinator.hpp"
#include "database.hpp"
#include "components/date_picker.hpp"
#include "components/fund_combo_delegate.hpp"
#include "components/table_view.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

class TransactionDialog : public QDialog {
	Q_OBJECT

	using allocated_transaction = fundos::db::transaction_history::allocated_transaction;

	enum class justification : uint8_t {
		custom,
		by_fund,
		by_budget,
	};

	struct allocation_row {
		int64_t          fund_id;
		fundos::currency amount;
	};

	AppCoordinator*    app_coordinator;
	allocated_transaction transaction;
	std::unordered_map<int64_t, fundos::currency> adjusted_balances;

	justification                current_justification = justification::custom;
	std::vector<allocation_row>  current_allocations;

	QLineEdit*   amount_field;
	QLineEdit*   memo_field;
	DatePicker*  date_recorded_picker;
	QCheckBox*   reconciled_checkbox;
	DatePicker*  date_reconciled_picker;
	QLabel*      date_cleared_label;
	QComboBox*   justified_by_combo;
	QComboBox*   add_fund_combo;
	QPushButton* add_fund_button;
	QPushButton* save_button;
	TableView*   allocation_table;
	QLabel*      allocation_total_label;

	void populate_justified_by_combo();
	void populate_add_fund_combo();
	void rebuild_allocation_table();
	void update_allocation_total();
	void apply_justification(int combo_index);
	void add_allocation_row(allocation_row row, bool editable, int grid_row);
	void on_add_fund_clicked();
	void on_save_clicked();

	bool eventFilter(QObject* object, QEvent* event) override;

signals:
	void request_fund_balance(int64_t fund_id);
	void save_requested(fundos::transaction transaction, std::vector<fundos::allocation> allocations);

public slots:
	void on_balance_received(int64_t fund_id, fundos::db::result<fundos::currency> result);
	void on_save_completed(fundos::db::outcome outcome);

public:
	explicit TransactionDialog(
		AppCoordinator*       coordinator,
		allocated_transaction opening,
		QWidget*              parent = nullptr
	);
};
