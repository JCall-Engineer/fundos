#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "sqlite3.h"
#include "models.hpp"

namespace fundos {

/// Not exposed publicly
namespace schema {
union db_prepared_statements;
}

class db {
public:
	/// Tag type: pass to db(sqlite3*, owns_connection) to transfer connection ownership to db.
	struct owns_connection {};

	/// Describes the schema relationship discovered when opening a database.
	/// States that imply a closed connection are noted.
	/// query methods will return error::not_ready for anything other than current, created, or migrated.
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
		unavailable,     // busy or locked, try again
		inaccessible,    // can't open the database
		readonly,        // filesystem permission check necessary
		out_of_memory,   // potentially transient error
		disk_full,       // potentially transient error
		constraint,      // either a FOREIGN KEY or UNIQUE violation
		not_found,       // query did not yield resulting data
		bad_request,     // incorrect API usage
		rejected,        // data does not satisfy preconditions
		interrupted,     // sqlite3_interrupt was called, most likely from fundos::db::interrupt
		internal,        // an unexpected situation that would abort a debug build
	};

	/// Carries a human-readable error description without unnecessary allocation.
	/// Constructed from either:
	/// - a string literal (no heap allocation)
	/// - a shared sqlite3 error string (shared ownership, one allocation)
	/// @warning do not construct from a raw pointer unless it is a string literal
	struct message {
		std::variant<const char*, std::shared_ptr<std::string>> content;

		/// implicit construction from string literals — no allocation
		message(const char* literal) : content(literal) {}

		/// from existing shared sqlite error string
		message(std::shared_ptr<std::string> dynamic) : content(std::move(dynamic)) {}

		std::string_view view() const {
			return std::visit([](auto& value) -> std::string_view {
				using ValueType = std::decay_t<decltype(value)>;
				if constexpr (std::is_same_v<ValueType, const char*>) {
					return value ? std::string_view(value) : std::string_view{};
				} else {
					return value ? std::string_view(*value) : std::string_view{};
				}
			}, content);
		}
	};

	/// Carries an error code and an optional human-readable description.
	/// Used as the error carrier in result<T> and the return type for operations that produce no value on success.
	/// Defaults to error::none; boolean conversion returns true on success.
	struct outcome {
		error code = error::none;
		std::optional<message> msg;
		operator bool() const { return code == error::none; }

		outcome() = default;
		outcome(error e, std::optional<message> m = std::nullopt) : code{e}, msg(std::move(m)) {}
	};

	/// Describes the result of opening and initializing a database connection.
	/// Populated by open() and updated by prepare() and migrate().
	///
	/// result gives the coarse outcome: ok, needs_migration, or an error category.
	/// schema_status gives the fine-grained schema relationship, including which error states imply a closed connection (see schema_state).
	/// sqlite3_outcome carries the underlying sqlite3 error when result indicates a sqlite3_error, and is also set on corrupted schema states.
	///
	/// ok and needs_migration are the only workable states; all error codes imply the connection has been closed and queries will return not_ready.
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
		outcome      sqlite3_outcome;
		std::string  journal_mode; // cheap diagnostic for if wal mode can be set

		bool is_ok()           const { return result == code::ok; }
		bool has_error()       const { return result >= first_error; }
		bool needs_migration() const { return result == code::needs_migration; }
	};

private:
	bool managed;
	sqlite3* connection;
	schema::db_prepared_statements* prepared;
	mutable std::shared_mutex connection_mutex;

	int64_t schema = 0;
	status open_result = {};

	inline std::optional<message> sqlite_error_message() { return sqlite_error_message(connection); }
	static inline std::optional<message> sqlite_error_message(sqlite3* connection) {
		const char* msg = sqlite3_errmsg(connection);
		if (msg == nullptr || std::string_view(msg) == "not an error") {
			return std::nullopt;
		}
		return std::make_shared<std::string>(msg);
	}

	inline outcome not_ready() {
		if (connection == nullptr) {
			return outcome(error::not_ready, "The database connection is closed");
		}
		FUNDOS_ASSERT(!open_result.is_ok(), "not_ready() was called when the database is ready");
		if (open_result.needs_migration()) {
			return outcome(error::not_ready, "Database must be migrated before it can be used");
		}
		FUNDOS_UNREACHABLE();
		return outcome(error::not_ready, "Database is not ready, see db::get_status() for more details");
	}

	static inline outcome success() { return outcome(error::none); }

	static error classify_sqlite_open_error(int rc);
	inline outcome sqlite_open_error(int rc) { return outcome(classify_sqlite_open_error(rc), sqlite_error_message()); }

	error classify_sqlite_runtime_error(int rc);
	inline outcome sqlite_runtime_error(int rc) {
		auto error = classify_sqlite_runtime_error(rc);
		auto message = sqlite_error_message(); // captured before any close
		if (error == error::corrupted || error == error::internal) {
			close();
		}
		return outcome(error, std::move(message));
	}

public:
	/// Opens or creates a database file at the given path.
	static std::shared_ptr<db> open_file(const char* path);

	/// Creates a private in-memory database.
	static std::shared_ptr<db> open_memory();

	/// Opens in a permanently errored state; used by factory functions to return a failed-but-valid db object.
	/// Prefer open_file() or open_memory() over this.
	explicit db(outcome);

	/// Borrows an existing sqlite3 connection; caller responsible for lifetime, no sqlite3_close on destruction.
	explicit db(sqlite3*);

	/// Takes ownership of an existing connection; sqlite3_close is called on destruction.
	explicit db(sqlite3*, owns_connection);

	/// Cleans up prepared statements. Calls sqlite3_close iff owns_connection
	~db();

	// no default construct, no copy, no move — connection lifetime is explicit
	db() = delete;
	db(const db&) = delete;
	db& operator=(const db&) = delete;

	const status&                get_status()     { return open_result; }
	bool                         is_connected()   { return connection != nullptr; }
	bool                         is_ready()       { return open_result.is_ok() && is_connected(); }

	/// Meant to be called from another thread in order to cancel a current request
	void interrupt();

	/// @return 0 on an errored or uninitialized db
	int64_t                      schema_version() { return schema; }

	/// @return 0 on an errored or closed db
	int64_t                      size_on_disk();

	outcome backup(const std::string& path);

	void close();
private:
	void open();
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
	outcome migrate();

#pragma region Query methods

	/// @brief Lightweight C++20 alternative to std::expected for database operations.
	///
	/// Represents either a successful result containing a value of type T, or a failure represented by db::outcome.
	///
	/// This is a strict discriminated union where exactly one of (T, outcome) is active at any time.
	/// No empty or double-value states exist.
	///
	/// @warning This type does not enforce safe access at runtime.
	/// Calling value() or status() in the wrong state triggers std::bad_variant_access.
	/// The caller is responsible for checking the state before access.
	///
	/// Success is determined via the boolean conversion operator; if(result) indicates that a value is present.
	template<typename T>
	struct result {
		std::variant<T, outcome> data;

		result() = delete;
		result(T v) : data(std::move(v)) {}
		result(outcome o) : data(std::move(o)) {}

		explicit operator bool() const { return std::holds_alternative<T>(data); }

		// All accessors are lvalue-qualified; none make sense on a temporary result.
		// Callers must verify via operator bool before calling value() or status().

		      T&       value()        & { return std::get<T>(data); }
		const T&       value()  const & { return std::get<T>(data); }
		const outcome& status() const & { return std::get<outcome>(data); }
	};

private:
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
	outcome sql_delete_except(const delete_except_params&);

	// "select COUNT(*) where id IN" checks
	outcome sql_count_check(const std::string& sql, size_t expected, executor bind, message on_failure);

	// insert/update/delete
	outcome sql_execute(sqlite3_stmt* stmt, executor bind);

	// single row or no result
	template<typename T>
	result<T> sql_fetch_one(sqlite3_stmt* stmt, executor bind, extractor<T> extract);

	// multiple rows
	template<typename T>
	result<std::vector<T>> sql_fetch_many(sqlite3_stmt* stmt, executor bind, extractor<T> extract);

	outcome sql_transaction(std::function<outcome(std::vector<std::function<void()>>&)> work);

	result<std::string> get_meta(std::string key);
	outcome             set_meta(std::string key, std::string value);

public:
	result<currency_locale::selection>   get_currency_locale();
	outcome                              set_currency_locale(const currency_locale::selection& locale);

	result<percentage_locale::selection> get_percentage_locale();
	outcome                              set_percentage_locale(const percentage_locale::selection& locale);

	result<std::vector<account>> get_accounts();
	result<currency>             get_account_balance(int64_t account_id);

	/// Inserts or updates the given account.
	/// If id is zero, inserts and sets id on the object.
	/// If id is nonzero, updates the existing record.
	outcome                      save_account(account& saving);

	result<std::vector<fund>>    get_funds();
	result<currency>             get_fund_balance(int64_t fund_id);

	/// Inserts or updates the given fund.
	/// If id is zero, inserts and sets id on the object.
	/// If id is nonzero, updates the existing record.
	outcome                      save_fund(fund& saving);

	result<std::vector<budget>>  get_budgets();

	/// Inserts or updates the budget and performs a deep save of its phases and targets.
	/// If id is zero, inserts and sets id on the object.
	/// If id is nonzero, updates the existing record.
	outcome                      save_budget(budget& saving);
	outcome                      delete_budget(int64_t budget_id);

private:
	/// Resolves correction links between transactions after an OFX import.
	/// - Sets superseded_by on original transactions that have been corrected.
	/// - Sets corrects_id on correction transactions that reference a known fitid.
	/// Should be called after each OFX import.
	outcome                      resolve_corrections();

	/// Used internally to pull all fields from a transaction needed for validating safe updates to the db
	result<transaction>          fetch_transaction(int64_t id);

public:
	/// Populates candidates and initializes match and saving on each imported_transaction.
	/// Resolves each bank_account's acct_id to an account_id via bank_account_id.
	/// @return rejected if any acct_id does not match a known account.
	/// @return bad_request if any imported transaction is missing fitid or cleared.
	outcome                      prepare_import(import::pending_import& pending);

	/// Commits each imported_transaction in the pending import.
	/// The committed record is assembled from multiple sources rather than saving verbatim:
	///   - id — taken from match if not null (nonnull match = update, null = insert)
	///   - account_id — taken from the enclosing bank_account (resolved by prepare_import)
	///   - fitid, corrects_fitid, correct_action, cleared, amount — taken from importing
	///   - date, memo — taken from saving
	/// Runs resolve_corrections after all transactions are committed.
	/// @return bad_request if a match was set without going through prepare_import and valid_candidates_for.
	outcome                      perform_import(import::pending_import& pending);

private:
	outcome                      save_allocations(transaction& saving, std::vector<allocation>& allocations, std::vector<std::function<void()>>& rollback);
	outcome                      create_transaction(transaction& saving, std::vector<allocation>& allocations);
	outcome                      update_transaction(transaction& saving, std::vector<allocation>& allocations);

public:
	/// Saves a transaction and atomically replaces its allocations.
	///
	/// On insert (id == 0), persists account_id, amount, date, memo, corrects_id, and correct_action, and sets id on the transaction.
	/// If corrects_id is set, marks the target transaction as superseded_by this record.
	/// On update (id != 0), persists date and memo only; all other transaction fields must match the persisted record.
	///
	/// In both cases, allocations are replaced wholesale: existing allocations not present in the vector are deleted, persisted ones are updated, and new ones are inserted.
	/// Sets id on inserted allocations; clears id on rollback.
	/// An empty vector clears all existing allocations, leaving the transaction unallocated.
	///
	/// @param saving      The transaction to insert or update.
	/// @param allocations The complete intended allocation set, or empty to leave the transaction unallocated.
	///                     transaction_id need not be set; it is filled in automatically.
	///
	/// @return bad_request if:
	///   - The record does not exist on update.
	///   - corrects_id and correct_action are not in parity, or corrects_id points at an invalid target.
	///   - transaction_id on an allocation is non-zero and does not match saving.id, or fund ids are duplicated or zero.
	/// @return rejected if:
	///   - account_id, amount, cleared, fitid, corrects_fitid, correct_action, corrects_id, or superseded_by differ from the persisted record on update.
	///   - allocations is non-empty and amounts do not sum exactly to the transaction amount.
	///   - The correction target is already superseded or has a fitid.
	/// @return constraint, unavailable, or other db::error on storage failure.
	outcome                      save_transaction(transaction& saving, std::vector<allocation>& allocations);

	/// Result type for account-level transaction views over a specified date range.
	/// Each transaction carries its resulting account balance and all of its fund allocations.
	/// Ledger balances allow the UI to verify that local records are in sync with the bank's reported statements.
	struct transaction_history {
		struct allocated_transaction {
			transaction record;
			datetime effective_date;
			currency account_balance;
			std::vector<allocation> allocations;
		};

		std::vector<allocated_transaction> transactions;
		std::vector<import_ledger_balance> ledger_balances;
	};

	/// Result type for fund-level transaction views over a specified date range.
	/// Each transaction carries its resulting fund balance and the allocation amount specific to the fund.
	struct allocation_history {
		struct allocated_transaction {
			transaction record;
			allocation allocated;
			currency fund_balance;
		};

		std::vector<allocated_transaction> transactions;
	};

	result<transaction_history>  account_history(int64_t account_id, datetime after, datetime before);
	result<allocation_history>   fund_history(int64_t fund_id, datetime after, datetime before);

#pragma endregion

}; // class db

} // namespace fundos
