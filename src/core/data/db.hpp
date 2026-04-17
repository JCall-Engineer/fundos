#pragma once
#include "sqlite3.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
		corrupted,       // unrecoverable fs error, connection closed
		unavailable,     // busy or locked
		out_of_memory,   // potentially transient error
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

	template<typename T>
	struct result {
		error err = error::none;
		std::optional<T> val;
		operator bool() const { return err == error::none; }
	};

	// Query methods
	//result<std::vector<user>> get_users();

private:
	void open();
	void close();
	void prepare();

public:
	error migrate();
};

} // fundos
