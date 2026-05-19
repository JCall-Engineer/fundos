#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "sqlite3.h"
#include "models.hpp"

namespace fundos {

union db_prepared_statements;

class db {
public:
	struct owns_connection {};

	enum class schema_state : uint8_t {
		none,            // empty or nonexistent db
		current,         // existing db, schema current
		created,         // fresh db, schema applied
		migrated,        // schema updated this session
		older_schema,    // needs migration
		newer_schema,    // app outdated,           connection closed
		schema_mismatch, // ours but untrustworthy, connection closed
		app_mismatch,    // not our db,             connection closed
	};
	enum class error : uint8_t {
		none,            // happy path
		not_ready,       // called a query function before migration or after a closed connection
		corrupted,       // unrecoverable fs error, connection closed
		unavailable,     // busy or locked
		out_of_memory,   // potentially transient error
		disk_full,       // potentially transient error
		constraint,      // either a FOREIGN KEY or UNIQUE violation
		bad_request,     // incorrect API usage
		rejected,        // data does not satisfy preconditions
		internal,        // an unexpected situation that would abort a debug build
	};

	struct status {
		enum class code : uint8_t {
			// Workable
			ok,
			needs_migration,

			// Errors
			null_db,
			schema_error,
			sqlite3_error,
		};
		static constexpr code first_error = code::null_db;

		code         result        = code::ok;
		schema_state schema_status = schema_state::none;
		error        sqlite3_error = error::none;

		bool is_ok()           const { return result == code::ok; }
		bool has_error()       const { return result >= first_error; }
		bool needs_migration() const { return result == code::needs_migration; }
	};

private:
	bool managed;
	sqlite3* connection;
	db_prepared_statements* prepared;

	uint64_t schema = 0;
	status open_result = {};
	std::shared_ptr<std::string> errmsg = nullptr;

	inline void get_sqlite3_error() {
		const char* msg = sqlite3_errmsg(connection);
		errmsg =
			msg == nullptr || std::string_view(msg) == "not an error"
			? nullptr
			: std::make_shared<std::string>(msg);
	}

public:
	/// Opens or creates a database file at the given path.
	static std::shared_ptr<db> open_file(std::string);

	/// Creates a private in-memory database.
	static std::shared_ptr<db> open_memory();

	/// Opens in a permanently errored state; used by factory functions to return a failed-but-valid db object.
	/// Prefer open_file() or open_memory() over this.
	explicit db(error);

	/// Borrows an existing sqlite3 connection; caller responsible for lifetime, no sqlite3_close on destruction.
	explicit db(sqlite3*);

	/// Takes ownership of an existing connection; sqlite3_close is called on destruction.
	explicit db(sqlite3*, owns_connection);

	/// Cleans up prepared statements. Calls sqlite3_close iff owns_connection
	~db();

	// no copy, no move — connection lifetime is explicit
	db(const db&) = delete;
	db& operator=(const db&) = delete;

	std::shared_ptr<std::string> get_errmsg()   const { return errmsg; }
	const status&                get_status()   const { return open_result; }
	bool                         is_connected() const { return connection != nullptr; }
	bool                         is_ready()     const { return open_result.is_ok() && is_connected(); }

#pragma region Query methods

	/// Three-state return type for query operations.
	/// - ok() + value: record found.
	/// - ok() + no value (not_found()): query succeeded but no matching record exists.
	/// - !ok(): a db::error is set; value is empty.
	template<typename T>
	struct result {
		error err = error::none;
		std::optional<T> val;
		operator bool()  const { return val.has_value(); }
		bool ok()        const { return err == error::none; }
		bool not_found() const { return err == error::none && !val.has_value(); }
	};

private:
	error classify_sqlite_runtime_error(int rc);

	using executor = std::function<void(sqlite3_stmt*)>;

	template<typename T>
	using extractor = std::function<T(sqlite3_stmt*)>;

	// delete where id not in
	struct delete_except_params {
		std::string_view table;
		std::string_view filter_column;
		int64_t filter_value;
		std::span<const int64_t> preserve_ids;
	};
	error sql_delete_except(const delete_except_params&);

	// "select COUNT(*) where id IN" checks
	error sql_count_check(const std::string& sql, size_t expected, executor bind);

	// insert/update/delete
	error sql_execute(sqlite3_stmt* stmt, executor bind);

	// single row or no result
	template<typename T>
	result<T> sql_fetch_one(sqlite3_stmt* stmt, executor bind, extractor<T> extract);

	// multiple rows
	template<typename T>
	result<std::vector<T>> sql_fetch_many(sqlite3_stmt* stmt, executor bind, extractor<T> extract);

	error sql_transaction(std::function<error(std::vector<std::function<void()>>&)> work);

	result<std::string> get_meta(std::string key);
	error               set_meta(std::string key, std::string value);

public:
	result<currency_locale::info>   get_currency_locale();
	error                           set_currency_locale_preset(const currency_locale::slot&);
	error                           set_currency_locale(const currency_locale::info&);

	result<percentage_locale::info> get_percentage_locale();
	error                           set_percentage_locale_preset(const percentage_locale::slot&);
	error                           set_percentage_locale(const percentage_locale::info&);

	result<std::vector<user>>    get_users();

	/// Inserts or updates the given user.
	/// If id is zero, inserts and sets id on the object.
	/// If id is nonzero, updates the existing record.
	error                        save_user(user&);
	error                        delete_user(int64_t user_id);

	result<std::vector<account>> get_accounts();

	/// Inserts or updates the given account.
	/// If id is zero, inserts and sets id on the object.
	/// If id is nonzero, updates the existing record.
	error                        save_account(account&);

	result<std::vector<fund>>    get_funds();

	/// Inserts or updates the given fund.
	/// If id is zero, inserts and sets id on the object.
	/// If id is nonzero, updates the existing record.
	error                        save_fund(fund&);

	result<std::vector<account>> get_account_memberships(int64_t user_id);
	result<std::vector<account>> get_account_nonmemberships(int64_t user_id);
	result<std::vector<user>>    get_account_members(int64_t account_id);
	result<std::vector<user>>    get_account_nonmembers(int64_t account_id);
	error                        add_user_to_account(int64_t account_id, int64_t user_id);
	error                        remove_user_from_account(int64_t account_id, int64_t user_id);

	result<std::vector<fund>>    get_fund_memberships(int64_t user_id);
	result<std::vector<fund>>    get_fund_nonmemberships(int64_t user_id);
	result<std::vector<user>>    get_fund_members(int64_t fund_id);
	result<std::vector<user>>    get_fund_nonmembers(int64_t fund_id);
	error                        add_user_to_fund(int64_t fund_id, int64_t user_id);
	error                        remove_user_from_fund(int64_t fund_id, int64_t user_id);

	result<std::vector<budget>>  get_budgets();

	/// Inserts or updates the budget and performs a deep save of its phases and targets.
	/// If id is zero, inserts and sets id on the object.
	/// If id is nonzero, updates the existing record.
	error                        save_budget(budget&);

private:
	/// Resolves correction links between transactions after an OFX import.
	/// - Sets superseded_by on original transactions that have been corrected.
	/// - Sets corrects_id on correction transactions that reference a known fitid.
	/// Should be called after each OFX import.
	error                        resolve_corrections();

public:
	/// Populates candidates and initializes match and saving on each imported_transaction.
	/// Resolves each bank_account's acct_id to an account_id via bank_account_id.
	/// @return rejected if any acct_id does not match a known account.
	/// @return bad_request if any imported transaction is missing fitid or cleared.
	error                        prepare_import(import::pending_import& pending);

	/// Commits each imported_transaction in the pending import.
	/// The committed record is assembled from multiple sources rather than saving verbatim:
	///   - id — taken from  match if not null
	///   - fitid, corrects_fitid, correct_action, cleared, amount — taken from importing
	///   - date, memo — taken from saving
	/// @note Callers must not modify fitid, corrects_fitid, correct_action, cleared, or amount on saving.
	/// @note Callers must not bypass set_match() to alter definitive matches.
	error                        perform_import(import::pending_import& pending);

	/// Saves a user-created or user-edited transaction.
	/// Insert (id == 0): persists account_id, amount, date, memo, corrects_id, correct_action.
	///   Sets id on the object. If corrects_id is set, marks the target as superseded_by this record.
	///   Returns rejected if the correction target is already superseded or has a fitid.
	///   Returns bad_request if corrects_id and correct_action are not in parity or corrects_id points at an invalid target.
	/// Update (id != 0): persists date and memo only.
	///   Returns rejected if account_id, amount, cleared, fitid, corrects_fitid, correct_action, corrects_id, or superseded_by differ from the persisted record.
	///   Returns bad_request if the record does not exist.
	error                        save_transaction(transaction& transaction);

	/// Replaces the allocations for a transaction atomically.
	/// - Existing allocations not present in the vector are deleted.
	/// - Persisted allocations are updated.
	/// - New allocations are inserted.
	/// Sets id on inserted allocations; clears id on rollback.
	/// @note Allocation amounts must sum exactly to the transaction amount.
	/// @note All allocations must reference the same transaction_id.
	/// @note Fund ids must be unique within the vector; funds must exist and not be closed.
	/// @note Persisted allocations must belong to the given transaction.
	/// @param allocations The complete intended allocation set for the transaction; must be non-empty.
	/// @return bad_request if empty, transaction_id is zero, fund_ids are duplicated or zero, or allocations span multiple transactions.
	/// @return rejected if amounts do not sum to the transaction amount, or the transaction does not exist.
	/// @return constraint, unavailable, or other db::error on storage failure.
	error                        allocate_transaction(std::vector<allocation>& allocations);

	/// Result type for account-level transaction views.
	/// Account balance and transactions for a date range.
	/// Each transaction carries its complete allocation set.
	/// Checkpoints allow the UI to identify imported transaction ranges and detect gaps where bank data was never imported.
	/// A transaction absent from all checkpoint sets was not part of any OFX import.
	struct transaction_history {
		using allocated_transaction = std::pair<transaction, std::vector<allocation>>;

		currency starting_balance;
		std::vector<allocated_transaction> transactions;
		std::vector<balance_checkpoint> checkpoints;
	};

	/// Fund balance and allocations for a date range.
	/// Each allocation is paired with its parent transaction for date, memo, or other context.
	struct allocation_history {
		currency starting_balance;
		std::vector<std::pair<transaction, allocation>> allocations;
	};

	result<transaction_history>  account_history(int64_t account_id, datetime after, datetime before);
	result<allocation_history>   fund_history(int64_t fund_id, datetime after, datetime before);

#pragma endregion

private:
	void open();
	void close();
	void prepare();

public:
	/// Applies any pending schema migrations and prepares query statements.
	/// Must be called after open_file() or open_memory() if get_status().needs_migration() is true;
	/// query methods return not_ready until migration succeeds.
	/// Safe to call when the schema is already current; returns none immediately.
	/// On success, sets schema_status to migrated and the db becomes ready for queries.
	/// @return none if already current or migration succeeded.
	/// @return corrupted if schema drift or a constraint violation is detected; connection is closed.
	/// @return other db::error on storage failure.
	error migrate();
};

} // fundos
