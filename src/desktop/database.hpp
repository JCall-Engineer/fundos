#pragma once
#include "fundos.hpp"
#include <QDir>
#include <QObject>
#include <QStandardPaths>
#include <QString>
#include <QThread>

class AppDatabase : public QObject {
	Q_OBJECT

	QString db_path;
	QThread worker_thread;
	std::shared_ptr<fundos::db> database;

	template<typename T>
	void update_status(const fundos::db::result<T>& result);
	void update_status(const fundos::db::outcome& status);

public:
	explicit AppDatabase(QObject* parent) : QObject(parent) {
		QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
		db_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/fundos.sqlite";

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

	enum class RestoreResult : uint8_t {
		success,
		failed_to_move,
		failed_to_copy,
		data_at_risk,
	};

public:
	/// Called from the main thread to interrupt a long-running database operation.
	/// sqlite3_interrupt() is thread-safe; safe to call even when not connected.
	void interrupt() { if (database) { database->interrupt(); } }

public slots:
	void close();
	void migrate();
	void open();

	void backup(QString destination);
	void restore(QString source);
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

	void request_account_history(int64_t account_id, fundos::datetime after, fundos::datetime before);

signals:
	void operation_started(QString description);
	void operation_finished();

	void db_outcome(fundos::db::outcome status);

	void connection_closed(fundos::db::outcome status);
	void connection_migrated(fundos::db::outcome status);
	void connection_opened(fundos::db::status open_result);

	void backup_complete(fundos::db::outcome status); // db was open, outcome from db::backup
	void backup_copy_failed();                        // db was closed, QFile::copy failed
	void restore_complete(RestoreResult result);
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

	void account_history_received(fundos::db::result<fundos::db::transaction_history> history);
};
