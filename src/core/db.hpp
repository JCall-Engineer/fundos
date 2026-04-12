#pragma once
#include "sqlite3.h"
#include <memory>
#include <string>

namespace fundos {

class db {
public:
	struct owns_connection {};
	enum class state : uint8_t {
		ok,
		created,
		older_schema,
		migrated,
		corrupted,
		newer_schema,
		app_mismatch,
		closed
	};

private:
	bool managed;
	state status;
	uint64_t schema;
	sqlite3* connection;
	void* prepared;
	std::shared_ptr<std::string> errmsg;

public:
	static std::shared_ptr<db> open_file(std::string);
	static std::shared_ptr<db> open_memory();
	explicit db(sqlite3*);                   // borrows, no close
	explicit db(sqlite3*, owns_connection);  // owns, closes on dtor
	~db();

	// no copy, no move — connection lifetime is explicit
	db(const db&) = delete;
	db& operator=(const db&) = delete;

	std::shared_ptr<std::string> get_errmsg() const { return errmsg; }
	state get_status() const { return status; }
	bool is_valid() const { return status == state::ok || status == state::created || status == state::migrated; }

private:
	void open();
	void close();
	void prepare();

public:
	void migrate();
};

} // fundos
