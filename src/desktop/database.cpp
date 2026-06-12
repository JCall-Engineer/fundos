#include "database.hpp"
#include <QFile>

template<typename T>
void AppDatabase::update_status(const fundos::db::result<T>& result) {
	if (result) {
		emit db_outcome({}); // defaults to error::none
	} else {
		update_status(result.status());
	}
}

void AppDatabase::update_status(const fundos::db::outcome& status) {
	if (database->is_connected()) {
		emit db_outcome(status);
	} else {
		emit connection_closed(status);
	}
}

void AppDatabase::close() {
	if (!database || !database->is_connected()) { return; }
	database->close();
	emit connection_closed({}); // defaults to error::none
}

void AppDatabase::migrate() {
	if (!database || !database->is_connected()) { return; }
	emit operation_started(tr("migrating database..."));
	auto migrated = database->migrate();
	emit connection_migrated(migrated);
	emit operation_finished();
	update_status(migrated);
}

void AppDatabase::open() {
	const bool was_open = database && database->is_connected();
	if (was_open) { database->close(); }
	// Do not emit operation_started/finished as there is no way to reasonably interrupt this process

	std::string path_str = db_path.toStdString();
	database = fundos::db::open_file(path_str.c_str());

	emit connection_opened(database->get_status());
}

void AppDatabase::backup(QString destination) {
	QFile::remove(destination); // Delete the existing file
	// Do not emit operation_started/finished as there is no way to reasonably interrupt this process
	if (database && database->is_connected()) {
		auto saved = database->backup(destination.toStdString());
		emit backup_complete(saved);
		update_status(saved);
	} else {
		// Don't update_status, no connection to update on
		if (QFile::copy(db_path, destination)) {
			emit backup_complete({});
		} else {
			emit backup_copy_failed();
		}
	}
}

void AppDatabase::restore(QString source) {
	const bool was_open = database && database->is_connected();
	if (was_open) { database->close(); }
	// Do not emit operation_started/finished as there is no way to reasonably interrupt this process
	QString temp_path = db_path + ".tmp";
	if (!QFile::rename(db_path, temp_path)) {
		emit restore_complete(RestoreResult::failed_to_move);
		return;
	}
	if (!QFile::copy(source, db_path)) {
		if (QFile::rename(temp_path, db_path)) { // best effort recovery
			if (was_open) { open(); }
			emit restore_complete(RestoreResult::failed_to_copy);
		} else {
			emit restore_complete(RestoreResult::data_at_risk);
		}
		return;
	}
	QFile::remove(temp_path);
	open();
	emit restore_complete(RestoreResult::success);
}

void AppDatabase::create_new() {
	const bool was_open = database && database->is_connected();
	if (was_open) { database->close(); }
	const bool deleted = QFile::remove(db_path);
	emit create_new_complete(deleted);
	if (deleted || was_open) { open(); }
}

void AppDatabase::set_locales(fundos::currency_locale::selection currency, fundos::percentage_locale::selection percentage) {
	if (!database) { return; }
	emit operation_started(tr("saving locales..."));
	auto saved_currency = database->set_currency_locale(currency);
	if (!saved_currency) {
		emit locales_saved(saved_currency);
		update_status(saved_currency);
		emit operation_finished();
		return;
	}
	auto saved_percentage = database->set_percentage_locale(percentage);
	emit locales_saved(saved_percentage);
	emit operation_finished();
	update_status(saved_percentage);
}

void AppDatabase::request_db_info() {
	if (!database) { return; }
	// Do not emit operation_started/finished as there is no way to reasonably interrupt this process
	emit db_info_received(DatabaseInfo{
		.size_on_disk = database->size_on_disk(),
		.journal_mode = database->get_status().journal_mode,
		.schema_version = database->schema_version(),
	});
}

void AppDatabase::request_locales() {
	if (!database) { return; }
	emit operation_started(tr("fetching locale information..."));
	auto currency = database->get_currency_locale();
	auto percentage = database->get_percentage_locale();
	emit locales_received(currency, percentage);
	emit operation_finished();
	if (currency) {
		update_status(percentage);
	} else {
		update_status(currency);
	}
}

void AppDatabase::request_accounts() {
	if (!database) { return; }
	emit operation_started(tr("fetching accounts..."));
	auto accounts = database->get_accounts();
	emit accounts_received(accounts);
	emit operation_finished();
	update_status(accounts);
}

void AppDatabase::request_funds() {
	if (!database) { return; }
	emit operation_started(tr("fetching funds..."));
	auto funds = database->get_funds();
	emit funds_received(funds);
	emit operation_finished();
	update_status(funds);
}

void AppDatabase::request_budgets() {
	if (!database) { return; }
	emit operation_started(tr("fetching budgets..."));
	auto budgets = database->get_budgets();
	emit budgets_received(budgets);
	emit operation_finished();
	update_status(budgets);
}

void AppDatabase::request_account_balance(int64_t account_id) {
	if (!database) { return; }
	emit operation_started(tr("fetching account balance..."));
	auto balance = database->get_account_balance(account_id);
	emit account_balance_received(account_id, balance);
	emit operation_finished();
	update_status(balance);
}

void AppDatabase::request_fund_balance(int64_t fund_id) {
	if (!database) { return; }
	emit operation_started(tr("fetching fund balance..."));
	auto balance = database->get_fund_balance(fund_id);
	emit fund_balance_received(fund_id, balance);
	emit operation_finished();
	update_status(balance);
}

void AppDatabase::save_account(fundos::account account) {
	if (!database) { return; }
	emit operation_started(tr("saving account..."));
	auto saved = database->save_account(account);
	emit account_saved(saved);
	emit operation_finished();
	update_status(saved);
}

void AppDatabase::save_fund(fundos::fund fund) {
	if (!database) { return; }
	emit operation_started(tr("saving fund..."));
	auto saved = database->save_fund(fund);
	emit fund_saved(saved);
	emit operation_finished();
	update_status(saved);
}

void AppDatabase::request_account_history(int64_t account_id, fundos::datetime after, fundos::datetime before) {
	if (!database) { return; }
	emit operation_started(tr("fetching account history..."));
	auto history = database->account_history(account_id, after, before);
	emit account_history_received(history);
	emit operation_finished();
	update_status(history);
}
