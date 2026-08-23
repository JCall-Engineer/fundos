#pragma once
#include <memory>
#include "coordinator.hpp"
#include "data/import.hpp"
#include <QDialog>
#include <QFutureWatcher>
#include <QVBoxLayout>

class ImportDialog : public QDialog {
	Q_OBJECT

	/// Advances through bank accounts in a pending import, skipping those already resolved to a known account.
	class AccountIterator {
		/// Stateful: position advances across calls so each invocation resumes after the last matched account.
		size_t position = 0;
	public:
		/// @return the next unmatched bank_account, or nullptr if all accounts are resolved.
		fundos::import::bank_account* advance_to_unmatched(
			fundos::import::pending_import& pending,
			const std::vector<fundos::account>& accounts
		);
	};

	AppCoordinator*                                 app_coordinator;
	std::shared_ptr<fundos::import::pending_import> importing;
	AccountIterator                                 account_iterator;
	QVBoxLayout*                                    layout;
	QFutureWatcher<fundos::import::result>*         parse_watcher = nullptr;

	static QString warning_message(fundos::import::warning type, int32_t count);

	void show_file_selection_page();
	void show_spinner_page(const QString& description);
	void show_parse_error_page(fundos::import::error error);
	void show_warnings_page(const fundos::import::result& result);
	void show_account_page(fundos::import::bank_account* bank_account);
	void show_transaction_page();
	void show_success_page(int32_t imported, int32_t merged);

private slots:
	void on_parse_finished();
	void on_account_saved(fundos::db::outcome status);
	void on_accounts_updated();
	void on_import_prepared(fundos::db::outcome result);
	void on_import_performed(fundos::db::outcome result);

signals:
	void save_account_requested(fundos::account saving);
	void refresh_accounts_requested();
	void prepare_import_requested(std::shared_ptr<fundos::import::pending_import> pending);
	void perform_import_requested(std::shared_ptr<fundos::import::pending_import> pending);

public:
	explicit ImportDialog(AppCoordinator* coordinator, QWidget* parent = nullptr);

public slots:
	void done(int result) override;
	void adjust_height() {
		resize(width(), sizeHint().height());
	}
};
