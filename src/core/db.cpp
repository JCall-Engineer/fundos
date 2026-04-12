#include "db.hpp"

static constexpr std::string_view schema_sql = R"sql(
CREATE TABLE IF NOT EXISTS meta (
	key   TEXT PRIMARY KEY,
	value TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS users (
	id   INTEGER PRIMARY KEY,
	name TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS funds (
	id      INTEGER PRIMARY KEY,
	user_id INTEGER NOT NULL REFERENCES users(id),
	name    TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS accounts (
	id          INTEGER PRIMARY KEY,
	user_id     INTEGER NOT NULL REFERENCES users(id),
	name        TEXT NOT NULL,
	bank_ref      TEXT,
	import_source TEXT,
	UNIQUE(bank_ref, import_source)
);

CREATE TABLE IF NOT EXISTS transactions (
	id              INTEGER PRIMARY KEY,
	account_id      INTEGER NOT NULL REFERENCES accounts(id),
	amount          INTEGER NOT NULL,
	date            TEXT NOT NULL,
	memo            TEXT,
	bank_ref        TEXT,
	import_source   TEXT,
	UNIQUE(account_id, bank_ref, import_source)
);

CREATE TABLE IF NOT EXISTS allocations (
	id             INTEGER PRIMARY KEY,
	transaction_id INTEGER NOT NULL REFERENCES transactions(id),
	fund_id        INTEGER NOT NULL REFERENCES funds(id),
	amount         INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS budgets (
	id              INTEGER PRIMARY KEY,
	user_id         INTEGER NOT NULL REFERENCES users(id),
	name            TEXT NOT NULL,
	overflow_fund   INTEGER NOT NULL REFERENCES funds(id)
);

CREATE TABLE IF NOT EXISTS budget_phases (
	id        INTEGER PRIMARY KEY,
	budget_id INTEGER NOT NULL REFERENCES budgets(id),
	position  INTEGER NOT NULL,
	kind      TEXT NOT NULL CHECK(kind IN ('fixed', 'percentage')),
	UNIQUE(budget_id, position)
);

CREATE TABLE IF NOT EXISTS phase_targets (
	id                INTEGER PRIMARY KEY,
	phase_id          INTEGER NOT NULL REFERENCES budget_phases(id),
	fund_id           INTEGER NOT NULL REFERENCES funds(id),
	amount            INTEGER NOT NULL,
	cap               INTEGER DEFAULT NULL,
	allow_overdraw    INTEGER NOT NULL DEFAULT 0 CHECK(allow_overdraw IN (0, 1))
);
)sql";

namespace fundos {

enum class statement : uint8_t {
	get_users,
	COUNT
};

struct db_prepared_statements {
	sqlite3_stmt* statements[static_cast<uint8_t>(statement::COUNT)] = {};
};

std::shared_ptr<db> db::open_file(std::string path) {
	sqlite3* connection;
	sqlite3_open(path.c_str(), &connection);
	return std::make_shared<db>(connection);
}

std::shared_ptr<db> db::open_memory() {
	sqlite3* connection;
	sqlite3_open(":memory:", &connection);
	return std::make_shared<db>(connection);
}

db::db(sqlite3* c) : connection(c), prepared(new db_prepared_statements()) {
	sqlite3_exec(connection, schema_sql.data(), nullptr, nullptr, nullptr);
}
db::~db() {
	for (auto& statement : prepared->statements) { 
		sqlite3_finalize(statement);
	}
	delete prepared;
	sqlite3_close(connection);
}

} // fundos
