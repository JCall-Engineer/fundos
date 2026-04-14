#include "db.hpp"
#include <cassert>

#pragma region SQL Queries

static constexpr std::string_view schema_migrations[] = {
R"sql(
CREATE TABLE meta (
	key   TEXT PRIMARY KEY,
	value TEXT NOT NULL
);
INSERT INTO meta (key, value) VALUES ('application', 'fundos');
INSERT INTO meta (key, value) VALUES ('schema_version', '1');

CREATE TABLE users (
	id   INTEGER PRIMARY KEY,
	name TEXT NOT NULL
);

CREATE TABLE funds (
	id        INTEGER PRIMARY KEY,
	name      TEXT NOT NULL,
	closed_at TEXT
);

CREATE TABLE fund_members (
	fund_id INTEGER NOT NULL REFERENCES funds(id) ON DELETE CASCADE,
	user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
	PRIMARY KEY (fund_id, user_id)
);
CREATE INDEX idx_fund_members_fund_id ON fund_members(fund_id);
CREATE INDEX idx_fund_members_user_id ON fund_members(user_id);

CREATE TABLE accounts (
	id            INTEGER PRIMARY KEY,
	name          TEXT NOT NULL,
	closed_at     TEXT,
	bank_ref      TEXT,
	import_source TEXT,
	UNIQUE(bank_ref, import_source)
);

CREATE TABLE account_members (
	account_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
	user_id    INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
	PRIMARY KEY (account_id, user_id)
);
CREATE INDEX idx_account_members_account_id ON account_members(account_id);
CREATE INDEX idx_account_members_user_id ON account_members(user_id);

CREATE TABLE budgets (
	id              INTEGER PRIMARY KEY,
	name            TEXT NOT NULL,
	overflow_fund   INTEGER NOT NULL REFERENCES funds(id) ON DELETE RESTRICT
);
CREATE INDEX idx_budgets_overflow_fund ON budgets(overflow_fund);

CREATE TABLE budget_phases (
	id        INTEGER PRIMARY KEY,
	budget_id INTEGER NOT NULL REFERENCES budgets(id) ON DELETE CASCADE,
	position  INTEGER NOT NULL,
	kind      TEXT NOT NULL CHECK(kind IN ('fixed', 'percentage')),
	UNIQUE(budget_id, position)
);
CREATE INDEX idx_budget_phases_budget_id ON budget_phases(budget_id);

CREATE TABLE phase_targets (
	id                INTEGER PRIMARY KEY,
	phase_id          INTEGER NOT NULL REFERENCES budget_phases(id) ON DELETE CASCADE,
	fund_id           INTEGER NOT NULL REFERENCES funds(id),
	amount            INTEGER NOT NULL,
	cap               INTEGER DEFAULT NULL,
	allow_overdraw    INTEGER NOT NULL DEFAULT 0 CHECK(allow_overdraw IN (0, 1))
);
CREATE INDEX idx_phase_targets_phase_id ON phase_targets(phase_id);
CREATE INDEX idx_phase_targets_fund_id ON phase_targets(fund_id);

CREATE TABLE transactions (
	id              INTEGER PRIMARY KEY,
	account_id      INTEGER NOT NULL REFERENCES accounts(id) ON DELETE RESTRICT,
	amount          INTEGER NOT NULL,
	date            TEXT NOT NULL,
	memo            TEXT,
	bank_ref        TEXT,
	import_source   TEXT,
	UNIQUE(account_id, bank_ref, import_source)
);
CREATE INDEX idx_transactions_account_id ON transactions(account_id);

CREATE TABLE allocations (
	id             INTEGER PRIMARY KEY,
	transaction_id INTEGER NOT NULL REFERENCES transactions(id) ON DELETE RESTRICT,
	fund_id        INTEGER NOT NULL REFERENCES funds(id) ON DELETE RESTRICT,
	amount         INTEGER NOT NULL
);
CREATE INDEX idx_allocations_transaction_id ON allocations(transaction_id);
CREATE INDEX idx_allocations_fund_id ON allocations(fund_id);
)sql", // version 1
};
static constexpr std::size_t schema_latest_version = std::size(schema_migrations);

namespace fundos {

struct statement_slot {
	sqlite3_stmt* statement = nullptr;
	const char* sql;
};

struct statements {
	statement_slot get_users { .sql = R"sql(
		SELECT * FROM users
	)sql" };
	statement_slot insert_user { .sql = R"sql(
		INSERT INTO users (name) VALUES (?)
	)sql" };
	statement_slot delete_user { .sql = R"sql(
		DELETE FROM users WHERE id = ?
	)sql" };

	statement_slot get_user_accounts { .sql = R"sql(
		SELECT accounts.* FROM accounts
		JOIN account_members ON account_members.account_id = accounts.id
		WHERE account_members.user_id = ?
	)sql" };
	statement_slot get_account_users { .sql = R"sql(
		SELECT users.* FROM users
		JOIN account_members ON account_members.user_id = users.id
		WHERE account_members.account_id = ?
	)sql" };
	statement_slot add_user_to_account { .sql = R"sql(
		INSERT INTO account_members (account_id, user_id)
		VALUES (?, ?)
	)sql" };
	statement_slot remove_user_from_account { .sql = R"sql(
		DELETE FROM account_members
		WHERE account_id = ? AND user_id = ?
	)sql" };

	statement_slot get_user_funds { .sql = R"sql(
		SELECT funds.* FROM funds
		JOIN fund_members ON fund_members.fund_id = funds.id
		WHERE fund_members.user_id = ?
	)sql" };
	statement_slot get_fund_users { .sql = R"sql(
		SELECT users.* FROM users
		JOIN fund_members ON fund_members.user_id = users.id
		WHERE fund_members.fund_id = ?
	)sql" };
	statement_slot add_user_to_fund { .sql = R"sql(
		INSERT INTO fund_members (fund_id, user_id)
		VALUES (?, ?)
	)sql" };
	statement_slot remove_user_from_fund { .sql = R"sql(
		DELETE FROM fund_members
		WHERE fund_id = ? AND user_id = ?
	)sql" };

	statement_slot get_funds { .sql = R"sql(
		SELECT * FROM funds
	)sql" };
	statement_slot get_fund_balance { .sql = R"sql(
		SELECT COALESCE(SUM(allocations.amount), 0) FROM allocations
		WHERE allocations.fund_id = ?
	)sql" };
	statement_slot insert_fund { .sql = R"sql(
		INSERT INTO funds (name) VALUES (?)
	)sql" };
	statement_slot update_fund { .sql = R"sql(
		UPDATE funds
		SET name = ?, closed_at = ?
		WHERE id = ?
	)sql" };

	statement_slot get_accounts { .sql = R"sql(
		SELECT * FROM accounts
	)sql" };
	statement_slot get_account_balance { .sql = R"sql(
		SELECT COALESCE(SUM(amount), 0) FROM transactions
		WHERE account_id = ?
	)sql" };
	statement_slot insert_account { .sql = R"sql(
		INSERT INTO accounts (name, bank_ref, import_source)
		VALUES (?, ?, ?)
	)sql" };
	statement_slot update_account { .sql = R"sql(
		UPDATE accounts
		SET name = ?, closed_at = ?, bank_ref = ?, import_source = ?
		WHERE id = ?
	)sql" };

	statement_slot get_budgets { .sql = R"sql(
		SELECT * FROM budgets
	)sql" };
	statement_slot insert_budget { .sql = R"sql(
		INSERT INTO budgets (name, overflow_fund)
		VALUES (?, ?)
	)sql" };
	statement_slot update_budget { .sql = R"sql(
		UPDATE budgets
		SET name = ?, overflow_fund = ?
		WHERE id = ?
	)sql" };
	statement_slot delete_budget { .sql = R"sql(
		DELETE FROM budgets WHERE id = ?
	)sql" };

	statement_slot get_phases { .sql = R"sql(
		SELECT *
		FROM budget_phases
		WHERE budget_id = ?
		ORDER BY position
	)sql" };
	statement_slot insert_phase { .sql = R"sql(
		INSERT INTO budget_phases (budget_id, position, kind)
		VALUES (?, ?, ?)
	)sql" };
	statement_slot update_phase { .sql = R"sql(
		UPDATE budget_phases
		SET position = ?, kind = ?
		WHERE id = ?
	)sql" };
	statement_slot delete_phase { .sql = R"sql(
		DELETE FROM budget_phases WHERE id = ?
	)sql" };

	statement_slot get_targets { .sql = R"sql(
		SELECT *
		FROM phase_targets
		WHERE phase_id = ?
	)sql" };
	statement_slot insert_target { .sql = R"sql(
		INSERT INTO phase_targets (phase_id, fund_id, amount, cap, allow_overdraw)
		VALUES (?, ?, ?, ?, ?)
	)sql" };
	statement_slot update_target { .sql = R"sql(
		UPDATE phase_targets
		SET fund_id = ?, amount = ?, cap = ?, allow_overdraw = ?
		WHERE id = ?
	)sql" };
	statement_slot delete_target { .sql = R"sql(
		DELETE FROM phase_targets WHERE id = ?
	)sql" };

	statement_slot filter_transactions { .sql = R"sql(
		SELECT * FROM transactions
		WHERE account_id = ?
		AND date BETWEEN ? AND ?
	)sql" };
	statement_slot insert_transaction { .sql = R"sql(
		INSERT INTO transactions (account_id, amount, date, memo, bank_ref, import_source)
		VALUES (?, ?, ?, ?, ?, ?)
	)sql" };
	statement_slot update_transaction { .sql = R"sql(
		UPDATE transactions
		SET bank_ref = ?, import_source = ?
		WHERE id = ?
	)sql" };

	statement_slot filter_allocations { .sql = R"sql(
		SELECT allocations.*, transactions.date FROM allocations
		JOIN transactions ON transactions.id = allocations.transaction_id
		WHERE allocations.fund_id = ?
		AND transactions.date BETWEEN ? AND ?
	)sql" };
	statement_slot insert_allocation { .sql = R"sql(
		INSERT INTO allocations (transaction_id, fund_id, amount)
		VALUES (?, ?, ?)
	)sql" };
	//statement name { .sql = R"sql()sql" };
};
static constexpr std::size_t num_prepared = sizeof(statements) / sizeof(statement_slot);

// I begroan that this layout isn't cache-efficient but prepared statements are not accessed in order so it doesn't matter and this gives us safety
union db_prepared_statements {
	statement_slot slots[num_prepared];
	statements named = {};
};

#pragma endregion

static inline db::error classify_sqlite_error(int rc) {
	switch (rc) {
		case SQLITE_NOMEM:
			return db::error::out_of_memory;
		case SQLITE_BUSY:
		case SQLITE_LOCKED:
		case SQLITE_READONLY:
			return db::error::unavailable;
		default:
			assert(false && "unhandled sqlite3 result code");
		case SQLITE_CORRUPT:
		case SQLITE_NOTADB:
		case SQLITE_IOERR:
			return db::error::corrupted;
	}
}

static inline std::shared_ptr<std::string> get_sqlite3_error(sqlite3* connection) {
	const char* msg = sqlite3_errmsg(connection);
	if (msg == nullptr || std::string_view(msg) == "not an error") { return nullptr; }
	return std::make_shared<std::string>(msg);
}

std::shared_ptr<db> db::open_file(std::string path) {
	sqlite3* connection;
	sqlite3_open(path.c_str(), &connection);
	sqlite3_busy_timeout(connection, 5000);
	return std::make_shared<db>(connection, owns_connection{});
}

std::shared_ptr<db> db::open_memory() {
	sqlite3* connection;
	sqlite3_open(":memory:", &connection);
	return std::make_shared<db>(connection, owns_connection{});
}

void db::prepare() {
	for (size_t i = 0; i < num_prepared; ++i) {
		int rc = sqlite3_prepare_v3(
			connection,
			prepared->slots[i].sql,
			-1,                              // length, -1 = read to null terminator
			SQLITE_PREPARE_PERSISTENT,       // prepFlags hint: reused frequently, keep associated cache resources warm
			&(prepared->slots[i].statement), // out: stmt
			nullptr                          // out: tail pointer, unused
		);
		if (SQLITE_OK == rc) { continue; }

		errmsg = get_sqlite3_error(connection);
		close(); // prepare is only called during initialization; closing unconditionally is safe since there is no path for the caller to retry
		if (SQLITE_ERROR == rc) { // SQL referenced a table/column that doesn't exist — schema drift
			open_result.result = status::code::schema_error;
			open_result.schema_status = schema_state::schema_mismatch;
		} else {
			open_result.result = status::code::sqlite3_error;
			open_result.sqlite3_error = classify_sqlite_error(rc);
		}
		return;
	}
}

db::error db::migrate() {
	switch (open_result.schema_status) {
		case schema_state::created:
		case schema_state::older_schema:
			break;
		default:
			return error::none;
	}
	for (; schema < schema_latest_version; ++schema) {
		const char* migration = schema_migrations[schema].data();
		int rc = sqlite3_exec(connection, migration, nullptr, nullptr, nullptr);
		if (SQLITE_OK == rc) { continue; }

		errmsg = get_sqlite3_error(connection);
		error out = error::none;
		switch (rc) {
			case SQLITE_ERROR: // Treat the db as corrupted if there is schema drift during migration
			case SQLITE_CONSTRAINT: // A migration violated a constraint, treat as corrupted
				out = error::corrupted;
				break;
			default:
				out = classify_sqlite_error(rc);
		}
		if (out == error::corrupted) {
			open_result.result = status::code::sqlite3_error;
			open_result.sqlite3_error = error::corrupted;
			close();
		}
		return out;
	}
	if (open_result.schema_status == schema_state::older_schema) {
		open_result.schema_status = schema_state::migrated;
	}
	// prepare cannot happen for older_schema as migrate must be called manually, so we call it here *or* when opening a file with a current schema
	prepare();
	return open_result.result == status::code::schema_error
		? error::corrupted
		: open_result.sqlite3_error;
}

void db::open() {
	int rc = sqlite3_exec(connection, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr); // required for cascade delete
	if (SQLITE_OK != rc) {
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_error = classify_sqlite_error(rc);
		close();
		return;
	}

	uint64_t schema_objects = 0;
	rc = sqlite3_exec(connection,
		"SELECT COUNT(*) FROM sqlite_schema",
		[](void* data, int, char** cols, char**) {
			*static_cast<uint64_t*>(data) = std::atoi(cols[0]);
			return 0;
		},
		&schema_objects, nullptr);
	if (SQLITE_OK != rc) {
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_error = classify_sqlite_error(rc);
		close();
		return;
	}

	if (schema_objects == 0) {
		open_result.schema_status = schema_state::created;
		error err = migrate();
		switch (err) {
			default:
				assert(false && "unhandled migration error");
			case error::none: // nothing to do
			case error::corrupted: // open_result already set by migrate()
				break;
			case error::out_of_memory:
			case error::unavailable:
				open_result.schema_status = schema_state::older_schema;
				open_result.result = status::code::needs_migration;
		}
		return;
	}

	// non-empty db: ensure it's ours
	std::string app;
	rc = sqlite3_exec(connection,
		"SELECT value FROM meta WHERE key='application'"
		" AND EXISTS (SELECT 1 FROM sqlite_master WHERE type='table' AND name='meta')",
		[](void* data, int, char** cols, char**) {
			*static_cast<std::string*>(data) = cols[0];
			return 0;
		},
		&app, nullptr);
	if (SQLITE_OK != rc) {
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_error = classify_sqlite_error(rc);
		close();
		return;
	}

	if (app != "fundos") {
		close();
		open_result.schema_status = schema_state::app_mismatch;
		open_result.result = status::code::schema_error;
		return;
	}

	// app matches: read version
	rc = sqlite3_exec(connection,
		"SELECT value FROM meta WHERE key='schema_version'",
		[](void* data, int, char** cols, char**) {
			*static_cast<uint64_t*>(data) = std::strtoull(cols[0], nullptr, 10);
			return 0;
		},
		&schema, nullptr);
	if (SQLITE_OK != rc) {
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_error = classify_sqlite_error(rc);
		close();
		return;
	}

	if (schema < schema_latest_version) {
		open_result.schema_status = schema_state::older_schema;
		open_result.result = status::code::needs_migration;
		return;
	} else if (schema > schema_latest_version) {
		close();
		open_result.schema_status = schema_state::newer_schema;
		open_result.result = status::code::schema_error;
		return;
	}
	prepare();
}
void db::close() {
	if (connection == nullptr) { return; }  // prepared and connection have parity and are managed as one resource
	for (size_t i = 0; i < num_prepared; ++i) {
		sqlite3_finalize(prepared->slots[i].statement);
		prepared->slots[i].statement = nullptr;
	}
	if (managed) {
		sqlite3_close(connection);
	}
	delete prepared;
	prepared = nullptr;
	connection = nullptr;
}

db::db(sqlite3* c)                  : connection(c), managed(false), prepared(new db_prepared_statements()) { open(); }
db::db(sqlite3* c, owns_connection) : connection(c), managed(true),  prepared(new db_prepared_statements()) { open(); }
db::~db() { close(); }

} // fundos
