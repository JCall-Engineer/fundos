#pragma once
#include "fundos.hpp"
#include <QDir>
#include <QObject>
#include <QStandardPaths>
#include <QString>
#include <QThread>

/// Manages database access on a dedicated worker thread.
/// @warning All public slots execute on an internal worker thread, not the calling thread.
/// Never call slots directly — always connect via QObject::connect(), which defaults to Qt::QueuedConnection across threads.
///
/// @note `database` is null only before the first open() call ever succeeds or fails; after that it is always non-null but may be disconnected.
class AppDatabase : public QObject {
	Q_OBJECT

	QString db_path;
	QString temp_path;
	QThread worker_thread;
	std::shared_ptr<fundos::db> database;

	/// @overload
	/// Unwraps the result and forwards the contained outcome to the primary overload.
	template<typename T>
	void update_status(const fundos::db::result<T>& result);

	/// Routes an operations outcome to the appropriate signal based on current connection state.
	/// If the database is connected db_outcome is emitted, otherwise connection_closed is emitted.
	void update_status(const fundos::db::outcome& status);

public:
	explicit AppDatabase(QObject* parent) : QObject(parent) {
		QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
		db_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/fundos.sqlite";
		temp_path = db_path + ".tmp";

		moveToThread(&worker_thread);
		worker_thread.start();
	}
	~AppDatabase() {
		worker_thread.quit();
		worker_thread.wait();
	}

	struct DatabaseInfo {
		int64_t     size_on_disk;
		std::string journal_mode;
		int64_t     schema_version;
	};

	enum class RestoreStep : uint8_t {
		start,   // initial restore attempt
		recover, // recover orphaned temp
		clean,   // discard orphaned temp and retry
	};

	enum class RestoreResult : uint8_t {
		// terminal success states
		success,
		orphaned_backup_restored, // previous failed recovery was recovered

		// terminal failure states
		failed_to_backup,         // original untouched at db_path; restore never started
		failed_to_import,         // could not restore external db; original restored successfully to db_path
		failed_to_restore,        // recovering the temp backup failed; original may be lost
		failed_to_clean_orphaned, // deleting the temp backup failed; cannot proceed with import
		live_prevents_orphaned,   // could not remove live database before recovering temp backup

		// non-terminal: requires caller input before proceeding
		orphaned_backup_detected, // previous failed recovery exists, defer to manual resolution
	};

	struct RestoreContext {
		QString source;

		// Set during RestoreStep::start; gates whether open() is called if the original is restored after a failed import.
		bool reopen_on_failure = false;

		RestoreStep step = RestoreStep::start;
		RestoreResult result = RestoreResult::success;
	};

	QString live_path()     { return db_path; }
	QString recovery_path() { return temp_path; }

public:
	/// Called from the main thread to interrupt a long-running database operation.
	/// Unlike slots, this is the one public method intentionally called cross-thread;
	/// sqlite3_interrupt() is thread-safe, so no queued connection is needed.
	void interrupt() { if (database) { database->interrupt(); } }

public slots:
	void close();
	void migrate();
	void open();

	void backup(QString destination);

	/// Restores the database from `source`, replacing the current file at db_path.
	/// Uses a rename-to-temp safety net so the original isn't lost on a failed copy: db_path -> db_path.tmp, then attempt to copy source -> db_path.
	/// If that copy fails, attempt to rename .tmp back to db_path (best-effort recovery).
	/// RestoreResult::failed_to_restore means even that recovery rename failed, so the original database may no longer be at db_path;
	/// this is the one case callers should treat as urgent/destructive.
	void restore(RestoreContext context);
	void create_new();

	void set_locales(fundos::currency_locale::selection currency, fundos::percentage_locale::selection percentage);

	void request_db_info();
	void request_locales();
	void request_accounts();
	void request_funds();
	void request_budgets();

	void request_account_balance(int64_t account_id);
	void request_fund_balance(int64_t fund_id);

	void save_account(fundos::account account);
	void save_fund(fundos::fund fund);
	void save_budget(fundos::budget budget);
	void delete_budget(int64_t budget_id);

	void save_transaction(fundos::transaction transaction, std::vector<fundos::allocation> allocations);

	void request_account_history(int64_t account_id, fundos::datetime after, fundos::datetime before);
	void request_fund_history(int64_t fund_id, fundos::datetime after, fundos::datetime before);

	void prepare_import(std::shared_ptr<fundos::import::pending_import> pending);
	void perform_import(std::shared_ptr<fundos::import::pending_import> pending);

signals:
	/// Informs the client that an operation is in progress so that a modal can be displayed to potentially interrupt the operation.
	void operation_started(QString description);

	// Informs the client that the operation is no longer interruptable.
	void operation_finished();

	/// Informs the client of every outcome so the status bar can be kept up to date and error messages can be centrally displayed.
	void db_outcome(fundos::db::outcome status);

	void connection_closed(fundos::db::outcome status);
	void connection_migrated(fundos::db::outcome status);
	void connection_opened(fundos::db::status open_result);

	void backup_complete(fundos::db::outcome status); // db was open, outcome from db::backup
	void backup_copy_failed();                        // db was closed, QFile::copy failed
	void restore_complete(RestoreContext context);
	void create_new_complete(bool succeeded);

	void locales_saved(fundos::db::outcome status);

	void db_info_received(DatabaseInfo info);
	void locales_received(fundos::db::result<fundos::currency_locale::selection> currency, fundos::db::result<fundos::percentage_locale::selection> percentage);
	void accounts_received(fundos::db::result<std::vector<fundos::account>> accounts);
	void funds_received   (fundos::db::result<std::vector<fundos::fund>>    funds);
	void budgets_received (fundos::db::result<std::vector<fundos::budget>> budgets);

	void account_balance_received(int64_t account_id, fundos::db::result<fundos::currency> balance);
	void fund_balance_received   (int64_t fund_id,    fundos::db::result<fundos::currency> balance);

	void account_saved(fundos::db::outcome status);
	void fund_saved(fundos::db::outcome status);
	void budget_saved(fundos::db::outcome status);
	void budget_deleted(fundos::db::outcome status);

	void transaction_saved(fundos::db::outcome status);

	void account_history_received(fundos::db::result<fundos::db::transaction_history> history);
	void fund_history_received(fundos::db::result<fundos::db::allocation_history> history);

	void import_prepared(fundos::db::outcome result);
	void import_performed(fundos::db::outcome result);
};
