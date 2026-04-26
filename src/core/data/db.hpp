#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
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
	static std::shared_ptr<db> open_file(std::string);
	static std::shared_ptr<db> open_memory();
	explicit db(error);                      // nothing opens
	explicit db(sqlite3*);                   // borrows, no close
	explicit db(sqlite3*, owns_connection);  // owns, closes on dtor
	~db();

	// no copy, no move — connection lifetime is explicit
	db(const db&) = delete;
	db& operator=(const db&) = delete;

	std::shared_ptr<std::string> get_errmsg()   const { return errmsg; }
	const status&                get_status()   const { return open_result; }
	bool                         is_connected() const { return connection != nullptr; }
	bool                         is_ready()     const { return open_result.is_ok() && is_connected(); }

#pragma region Query methods

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

	typedef std::function<void(sqlite3_stmt*)> executor;

	template<typename T>
	using extractor = std::function<T(sqlite3_stmt*)>;

	// insert/update/delete
	error execute(sqlite3_stmt* stmt, executor bind);

	// single row or no result
	template<typename T>
	result<T> fetch_one(sqlite3_stmt* stmt, executor bind, extractor<T> extract);

	// multiple rows
	template<typename T>
	result<std::vector<T>> fetch_many(sqlite3_stmt* stmt, executor bind, extractor<T> extract);

	error transaction(std::function<error()> work);

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
	error                        save_user(user&);
	error                        delete_user(int64_t user_id);

	result<std::vector<account>> get_accounts();
	error                        save_account(account&);

	result<std::vector<fund>>    get_funds();
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

#pragma endregion

private:
	void open();
	void close();
	void prepare();

public:
	error migrate();
};

} // fundos
