#include <array>
#include <functional>
#include "db.hpp"
#include "platform.hpp"

namespace fundos {

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
	memo            TEXT NOT NULL,
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

struct statement_slot {
	sqlite3_stmt* statement = nullptr;
	const char* sql;
};

struct statements {
	statement_slot get_meta { .sql = R"sql(
		SELECT value FROM meta WHERE key = ?
	)sql" };
	statement_slot set_meta { .sql = R"sql(
		INSERT INTO meta (key, value) VALUES (?, ?)
		ON CONFLICT (key) DO UPDATE SET value = excluded.value
	)sql" };

	statement_slot get_users { .sql = R"sql(
		SELECT id, name FROM users
	)sql" };
	statement_slot insert_user { .sql = R"sql(
		INSERT INTO users (name) VALUES (?)
	)sql" };
	statement_slot update_user { .sql = R"sql(
		UPDATE users SET name = ? WHERE id = ?
	)sql" };
	statement_slot delete_user { .sql = R"sql(
		DELETE FROM users WHERE id = ?
	)sql" };

	statement_slot get_account_memberships { .sql = R"sql(
		SELECT accounts.id, accounts.name, accounts.closed_at, accounts.bank_ref, accounts.import_source
		FROM accounts JOIN account_members ON account_members.account_id = accounts.id
		WHERE account_members.user_id = ? AND accounts.closed_at IS NULL
	)sql" };
	statement_slot get_account_nonmemberships { .sql = R"sql(
		SELECT accounts.id, accounts.name, accounts.closed_at, accounts.bank_ref, accounts.import_source
		FROM accounts LEFT JOIN account_members ON account_members.account_id = accounts.id AND account_members.user_id = ?
		WHERE account_members.account_id IS NULL AND accounts.closed_at IS NULL
	)sql" };
	statement_slot get_account_members { .sql = R"sql(
		SELECT users.id, users.name
		FROM users JOIN account_members ON account_members.user_id = users.id
		WHERE account_members.account_id = ?
	)sql" };
	statement_slot get_account_nonmembers { .sql = R"sql(
		SELECT users.id, users.name
		FROM users LEFT JOIN account_members ON account_members.user_id = users.id AND account_members.account_id = ?
		WHERE account_members.user_id IS NULL
	)sql" };
	statement_slot add_user_to_account { .sql = R"sql(
		INSERT INTO account_members (account_id, user_id)
		VALUES (?, ?)
	)sql" };
	statement_slot remove_user_from_account { .sql = R"sql(
		DELETE FROM account_members
		WHERE account_id = ? AND user_id = ?
	)sql" };

	statement_slot get_fund_memberships { .sql = R"sql(
		SELECT funds.id, funds.name, funds.closed_at
		FROM funds JOIN fund_members ON fund_members.fund_id = funds.id
		WHERE fund_members.user_id = ? AND funds.closed_at IS NULL
	)sql" };
	statement_slot get_fund_nonmemberships { .sql = R"sql(
		SELECT funds.id, funds.name, funds.closed_at
		FROM funds LEFT JOIN fund_members ON fund_members.fund_id = funds.id AND fund_members.user_id = ?
		WHERE fund_members.fund_id IS NULL AND funds.closed_at IS NULL
	)sql" };
	statement_slot get_fund_members { .sql = R"sql(
		SELECT users.id, users.name
		FROM users JOIN fund_members ON fund_members.user_id = users.id
		WHERE fund_members.fund_id = ?
	)sql" };
	statement_slot get_fund_nonmembers { .sql = R"sql(
		SELECT users.id, users.name
		FROM users LEFT JOIN fund_members ON fund_members.user_id = users.id AND fund_members.fund_id = ?
		WHERE fund_members.user_id IS NULL
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
		SELECT id, name, closed_at FROM funds
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
		SELECT id, name, closed_at, bank_ref, import_source FROM accounts
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
	//statement_slot name { .sql = R"sql()sql" };
};
static constexpr std::size_t num_prepared = sizeof(statements) / sizeof(statement_slot);
static_assert(sizeof(statements) % sizeof(statement_slot) == 0, "db_prepared_statements must contain only statement_slot members with no padding");

// I begroan that this layout isn't cache-efficient but prepared statements are not accessed in order so it doesn't matter and this gives us safety
union db_prepared_statements {
	statement_slot slots[num_prepared];
	statements named = {};
};

#pragma endregion

#pragma region Query Execution Layer

static inline std::string extract_text(sqlite3_stmt* stmt, int index) {
	return reinterpret_cast<const char*>(sqlite3_column_text(stmt, index));
}

static inline void bind_text(sqlite3_stmt* stmt, int index, const std::string& value) {
	sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_STATIC);
}

static inline std::optional<std::string> extract_optional_text(sqlite3_stmt* stmt, int index) {
	if (sqlite3_column_type(stmt, index) == SQLITE_NULL) { return std::nullopt; }
	return reinterpret_cast<const char*>(sqlite3_column_text(stmt, index));
}

static inline void bind_optional_text(sqlite3_stmt* stmt, int index, const std::optional<std::string>& value) {
	if (value) {
		sqlite3_bind_text(stmt, index, value->c_str(), -1, SQLITE_STATIC);
	} else {
		sqlite3_bind_null(stmt, index);
	}
}

db::error db::classify_sqlite_runtime_error(int rc) {
	switch (rc & 0xFF) {
		case SQLITE_FULL:
			return db::error::disk_full;
		case SQLITE_NOMEM:
			return db::error::out_of_memory;
		case SQLITE_BUSY:
		case SQLITE_LOCKED:
		// READONLY cannot be returned at step time
			return db::error::unavailable;
		case SQLITE_CORRUPT:
		// NOTADB cannot be returned at step time
		case SQLITE_IOERR:
			close();
			return db::error::corrupted;
		case SQLITE_CONSTRAINT:
			return db::error::constraint;
		// SQLITE_INTERRUPT: not expected, would indicate external sqlite3_interrupt() call
		default:
			FUNDOS_ASSERT(false, "unhandled sqlite3 step result code");
			return db::error::internal;
	}
}

db::error db::execute(sqlite3_stmt* stmt, executor bind) {
	if (!is_ready()) { return db::error::not_ready; }
	bind(stmt);
	int rc = sqlite3_step(stmt);
	sqlite3_reset(stmt);
	if (SQLITE_DONE != rc) {
		get_sqlite3_error();
		return classify_sqlite_runtime_error(rc);
	}
	return db::error::none;
}

template<typename T>
db::result<T> db::fetch_one(sqlite3_stmt* stmt, executor bind, extractor<T> extract) {
	if (!is_ready()) { return result<T> { .err = db::error::not_ready }; }
	bind(stmt);
	int rc = sqlite3_step(stmt);
	switch (rc) {
		case SQLITE_ROW: {
			T value = extract(stmt);
			sqlite3_reset(stmt);
			return result<T> { .val = value };
		}
		case SQLITE_DONE:
			sqlite3_reset(stmt);
			return result<T> {}; // not_found: err==none, val==nullopt
		default:
			get_sqlite3_error();
			sqlite3_reset(stmt);
			return result<T> { .err = classify_sqlite_runtime_error(rc) };

	}
}

template<typename T>
db::result<std::vector<T>> db::fetch_many(sqlite3_stmt* stmt, executor bind, extractor<T> extract) {
	if (!is_ready()) { return result<std::vector<T>> { .err = db::error::not_ready }; }
	bind(stmt);
	int rc;
	std::vector<T> rows;
	while (SQLITE_ROW == (rc = sqlite3_step(stmt))) {
		rows.push_back(extract(stmt));
	}
	sqlite3_reset(stmt);
	if (SQLITE_DONE != rc) {
		get_sqlite3_error();
		return result<std::vector<T>> { .err = classify_sqlite_runtime_error(rc) };
	}
	return result<std::vector<T>> { .val = rows };
}

//-------------------------------------------------------------------------------------+
// work() must propagate all errors immediately — transaction() relies on              |
// error::corrupted being returned to skip COMMIT/ROLLBACK on a closed connection.     |
// Do not silently swallow errors from execute/fetch_one/fetch_many inside work().     |
//-------------------------------------------------------------------------------------+
db::error db::transaction(std::function<error()> work) {
	if (!is_ready()) { return error::not_ready; }
	int rc = sqlite3_exec(connection, "BEGIN", nullptr, nullptr, nullptr);
	if (rc != SQLITE_OK) {
		get_sqlite3_error();
		return classify_sqlite_runtime_error(rc);
	}
	error result = work();
	switch (result) {
		case error::none:
			rc = sqlite3_exec(connection, "COMMIT", nullptr, nullptr, nullptr);
			if (rc != SQLITE_OK) {
				get_sqlite3_error();
				return classify_sqlite_runtime_error(rc);
			}
			return error::none;
		default:
			rc = sqlite3_exec(connection, "ROLLBACK", nullptr, nullptr, nullptr);
			if (SQLITE_OK == rc) {
				return result;
			}
			get_sqlite3_error();
			close(); // failed rollback means untrustworthy state regardless of cause
			[[fallthrough]];
		case error::corrupted:
			return error::corrupted;
	}
}

db::result<std::string> db::get_meta(std::string key) {
	return fetch_one<std::string>(
		prepared->named.get_meta.statement,
		[&](sqlite3_stmt* stmt) {
			bind_text(stmt, 1, key);
		},
		[](sqlite3_stmt* stmt) -> std::string {
			return extract_text(stmt, 0);
		}
	);
}

db::error db::set_meta(std::string key, std::string value) {
	return execute(
		prepared->named.set_meta.statement,
		[&](sqlite3_stmt* stmt) {
			bind_text(stmt, 1, key);
			bind_text(stmt, 2, value);
		}
	);
}

//-------------------------------------------------------------------------------------+
// Extractors are intentionally duplicated per query function rather than shared.      |
// Column indices are determined by each SQL statement's SELECT order, which is        |
// not guaranteed to remain consistent across queries even for the same model type.    |
// Sharing extractors would couple unrelated queries and risk silent data corruption   |
// if any statement's column order ever diverges. Treat each extractor as local        |
// to its query and verify column indices against the SQL when making changes.         |
//-------------------------------------------------------------------------------------+
struct locale_register {
	const std::string percentage_locale_key = "percentage_locale";
	const std::string currency_locale_key   = "currency_locale";
	const std::string custom_sentinel_val   = "custom";

	const std::string percentage_locale_decimal_separator_key = "percentage_locale_decimal_separator";
	const std::string percentage_locale_has_space_key         = "percentage_locale_has_space";
	const std::string percentage_locale_symbol_position_key   = "percentage_locale_symbol_position";

	const std::string currency_locale_scale_key               = "currency_locale_scale";
	const std::string currency_locale_symbol_key              = "currency_locale_symbol";
	const std::string currency_locale_thousands_separator_key = "currency_locale_thousands_separator";
	const std::string currency_locale_decimal_separator_key   = "currency_locale_decimal_separator";
	const std::string currency_locale_symbol_position_key     = "currency_locale_symbol_position";
	const std::string currency_locale_negative_format_key     = "currency_locale_negative_format";
};
static locale_register locale_meta = {};

template<typename Enum, std::size_t N>
using enum_string_map = std::array<std::pair<Enum, std::string>, N>;

static constexpr size_t num_symbol_placement = 2;
static const enum_string_map<percentage_locale::info::symbol_placement, num_symbol_placement> percentage_symbol_placement_map = {{
	{ percentage_locale::info::symbol_placement::before, "before" },
	{ percentage_locale::info::symbol_placement::after,  "after"  },
}};
static const enum_string_map<currency_locale::info::symbol_placement, num_symbol_placement> currency_symbol_placement_map = {{
	{ currency_locale::info::symbol_placement::before, "before" },
	{ currency_locale::info::symbol_placement::after,  "after"  },
}};

static constexpr size_t num_negative_format = 4;
static const enum_string_map<currency_locale::info::negative_notation, num_negative_format> currency_negative_notation_map = {{
	{ currency_locale::info::negative_notation::parentheses,    "parentheses"    },
	{ currency_locale::info::negative_notation::angle_brackets, "angle_brackets" },
	{ currency_locale::info::negative_notation::leading_minus,  "leading_minus"  },
	{ currency_locale::info::negative_notation::trailing_minus, "trailing_minus" },
}};

template<typename Enum, std::size_t N>
static inline std::optional<std::string> enum_to_string(const enum_string_map<Enum, N>& map, Enum value) {
	for (const auto& pair : map) {
		if (pair.first == value) { return pair.second; }
	}
	return std::nullopt;
}

template<typename Enum, std::size_t N>
static inline std::optional<Enum> string_to_enum(const enum_string_map<Enum, N>& map, std::string_view value) {
	for (const auto& pair : map) {
		if (pair.second == value) { return pair.first; }
	}
	return std::nullopt;
}

db::result<currency_locale::info> db::get_currency_locale() {
	static auto NOT_FOUND = result<currency_locale::info>{}; // not_found: err==none, val==nullopt
	static auto ERROR = [](error err) { return result<currency_locale::info>{ .err = err }; };
	static const std::unordered_map<std::string, int16_t> valid_scales = {
		{ "1", 1 }, { "10", 10 }, { "100", 100 }, { "1000", 1000 },
	};

	auto preset_result = get_meta(locale_meta.currency_locale_key);
	if (preset_result.err != error::none) { return ERROR(preset_result.err); }
	if (preset_result.not_found()) { return NOT_FOUND; }

	std::string preset_value = preset_result.val.value();
	// It looks ugly, I know, but it's the best I could do for functionality
	if (preset_value == locale_meta.custom_sentinel_val) {
		// Extract scale from meta
		auto scale_result = get_meta(locale_meta.currency_locale_scale_key);
		if (scale_result.err != error::none) { return ERROR(scale_result.err); }
		if (scale_result.not_found()) { return NOT_FOUND; }
		auto scale_it = valid_scales.find(scale_result.val.value());
		if (scale_it == valid_scales.end()) { return NOT_FOUND; }
		int16_t scale = scale_it->second;

		// Extract symbol from meta
		auto symbol_result = get_meta(locale_meta.currency_locale_symbol_key);
		if (symbol_result.err != error::none) { return ERROR(symbol_result.err); }
		if (symbol_result.not_found()) { return NOT_FOUND; }
		std::string symbol = symbol_result.val.value();
		if (symbol.length() > 4) { return NOT_FOUND; } // This is an implicit assumption for currency::to_string

		// Extract thousands_separator from meta
		auto thousands_result = get_meta(locale_meta.currency_locale_thousands_separator_key);
		if (thousands_result.err != error::none) { return ERROR(thousands_result.err); }
		if (thousands_result.not_found()) { return NOT_FOUND; }
		if (thousands_result.val.value().empty()) { return NOT_FOUND; }
		char thousands_separator = thousands_result.val.value()[0];

		// Extract decimal_separator from meta
		auto decimal_result = get_meta(locale_meta.currency_locale_decimal_separator_key);
		if (decimal_result.err != error::none) { return ERROR(decimal_result.err); }
		if (decimal_result.not_found()) { return NOT_FOUND; }
		if (decimal_result.val.value().empty()) { return NOT_FOUND; }
		char decimal_separator = decimal_result.val.value()[0];

		// Extract symbol position from meta
		auto position_result = get_meta(locale_meta.currency_locale_symbol_position_key);
		if (position_result.err != error::none) { return ERROR(position_result.err); }
		if (position_result.not_found()) { return NOT_FOUND; }
		auto position_enum = string_to_enum(currency_symbol_placement_map, position_result.val.value());
		if (!position_enum.has_value()) { return NOT_FOUND; }
		currency_locale::info::symbol_placement symbol_position = position_enum.value();

		// Extract negative format from meta
		auto negative_result = get_meta(locale_meta.currency_locale_negative_format_key);
		if (negative_result.err != error::none) { return ERROR(negative_result.err); }
		if (negative_result.not_found()) { return NOT_FOUND; }
		auto negative_enum = string_to_enum(currency_negative_notation_map, negative_result.val.value());
		if (!negative_enum.has_value()) { return NOT_FOUND; }
		currency_locale::info::negative_notation negative_format = negative_enum.value();

		return result<currency_locale::info>{ .val = currency_locale::info{
			.scale = scale,
			.symbol = symbol,
			.thousands_separator = thousands_separator,
			.decimal_separator = decimal_separator,
			.symbol_position = symbol_position,
			.negative_format = negative_format,
		}};
	}
	auto locale = currency_locale::get_locale(preset_value);
	if (locale.has_value()) {
		return result<currency_locale::info>{ .val = locale.value() };
	}
	return NOT_FOUND;
}
db::error db::set_currency_locale_preset(const currency_locale::slot& slot) {
	return set_meta(locale_meta.currency_locale_key, slot.identifier);
}
db::error db::set_currency_locale(const currency_locale::info& locale) {
	return transaction([&]() -> error {
		error result = set_meta(locale_meta.currency_locale_key, locale_meta.custom_sentinel_val);
		if (result != error::none) { return result; }

		switch (locale.scale) {
			case 1: case 10: case 100: case 1000:
				result = set_meta(locale_meta.currency_locale_scale_key, std::to_string(locale.scale));
				if (result != error::none) { return result; }
				break;
			default:
				return error::internal;
		}

		if (locale.symbol.length() > 4) { return error::internal; }
		result = set_meta(locale_meta.currency_locale_symbol_key, locale.symbol);
		if (result != error::none) { return result; }

		result = set_meta(locale_meta.currency_locale_thousands_separator_key, std::string(1, locale.thousands_separator));
		if (result != error::none) { return result; }

		result = set_meta(locale_meta.currency_locale_decimal_separator_key, std::string(1, locale.decimal_separator));
		if (result != error::none) { return result; }

		auto symbol_pos = enum_to_string(currency_symbol_placement_map, locale.symbol_position);
		if (!symbol_pos.has_value()) { return error::internal; }
		result = set_meta(locale_meta.currency_locale_symbol_position_key, symbol_pos.value());
		if (result != error::none) { return result; }

		auto negative_format = enum_to_string(currency_negative_notation_map, locale.negative_format);
		if (!negative_format.has_value()) { return error::internal; }
		result = set_meta(locale_meta.currency_locale_negative_format_key, negative_format.value());
		if (result != error::none) { return result; }

		return error::none;
	});
}

db::result<percentage_locale::info> db::get_percentage_locale() {
	static auto NOT_FOUND = result<percentage_locale::info>{}; // not_found: err==none, val==nullopt
	static auto ERROR = [](error err) { return result<percentage_locale::info>{ .err = err }; };

	auto preset_result = get_meta(locale_meta.percentage_locale_key);
	if (preset_result.err != error::none) { return ERROR(preset_result.err); }
	if (preset_result.not_found()) { return NOT_FOUND; }

	std::string preset_value = preset_result.val.value();
	if (preset_value == locale_meta.custom_sentinel_val) {
		// Extract decimal_separator from meta
		auto decimal_result = get_meta(locale_meta.percentage_locale_decimal_separator_key);
		if (decimal_result.err != error::none) { return ERROR(decimal_result.err); }
		if (decimal_result.not_found()) { return NOT_FOUND; }
		if (decimal_result.val.value().empty()) { return NOT_FOUND; }
		char decimal_separator = decimal_result.val.value()[0];

		// Extract has_space from meta
		auto space_result = get_meta(locale_meta.percentage_locale_has_space_key);
		if (space_result.err != error::none) { return ERROR(space_result.err); }
		if (space_result.not_found()) { return NOT_FOUND; }
		bool has_space_around_number = space_result.val.value() == "1";

		// Extract symbol position from meta
		auto position_result = get_meta(locale_meta.percentage_locale_symbol_position_key);
		if (position_result.err != error::none) { return ERROR(position_result.err); }
		if (position_result.not_found()) { return NOT_FOUND; }
		auto position_enum = string_to_enum(percentage_symbol_placement_map, position_result.val.value());
		if (!position_enum.has_value()) { return NOT_FOUND; }
		percentage_locale::info::symbol_placement symbol_position = position_enum.value();

		return result<percentage_locale::info>{ .val = percentage_locale::info{
			.decimal_separator = decimal_separator,
			.has_space_around_number = has_space_around_number,
			.symbol_position = symbol_position,
		}};
	}
	auto locale = percentage_locale::get_locale(preset_value);
	if (locale.has_value()) {
		return result<percentage_locale::info>{ .val = locale.value() };
	}
	return NOT_FOUND;
}
db::error db::set_percentage_locale_preset(const percentage_locale::slot& slot) {
	return set_meta(locale_meta.percentage_locale_key, slot.identifier);
}
db::error db::set_percentage_locale(const percentage_locale::info& locale) {
	return transaction([&]() -> error {
		error result = set_meta(locale_meta.percentage_locale_key, locale_meta.custom_sentinel_val);
		if (result != error::none) { return result; }

		result = set_meta(locale_meta.percentage_locale_decimal_separator_key, std::string(1, locale.decimal_separator));
		if (result != error::none) { return result; }

		result = set_meta(locale_meta.percentage_locale_has_space_key, locale.has_space_around_number ? "1" : "0");
		if (result != error::none) { return result; }

		auto symbol_pos = enum_to_string(percentage_symbol_placement_map, locale.symbol_position);
		if (!symbol_pos.has_value()) { return error::internal; }
		result = set_meta(locale_meta.percentage_locale_symbol_position_key, symbol_pos.value());
		if (result != error::none) { return result; }

		return error::none;
	});
}

db::result<std::vector<user>> db::get_users() {
	return fetch_many<user>(
		prepared->named.get_users.statement,
		[](sqlite3_stmt*) {},
		[](sqlite3_stmt* stmt) -> user {
			user row;
			row.id_   = sqlite3_column_int64(stmt, 0);
			row.name  = extract_text        (stmt, 1);
			return row;
		}
	);
}

db::error db::save_user(user& user) {
	if (!user.is_persisted()) {
		error err = execute(
			prepared->named.insert_user.statement,
			[&](sqlite3_stmt* stmt) {
				bind_text(stmt, 1, user.name);
			}
		);
		if (err != error::none) { return err; }
		user.id_= sqlite3_last_insert_rowid(connection);
		return error::none;
	} else {
		return execute(
			prepared->named.update_user.statement,
			[&](sqlite3_stmt* stmt) {
				bind_text         (stmt, 1, user.name);
				sqlite3_bind_int64(stmt, 2, user.id_);
			}
		);
	}
}

db::error db::delete_user(int64_t user_id) {
	return execute(
		prepared->named.delete_user.statement,
		[&](sqlite3_stmt* stmt) {
			sqlite3_bind_int64(stmt, 1, user_id);
		}
	);
}

db::result<std::vector<account>> db::get_account_memberships(int64_t user_id) {
	return fetch_many<account>(
		prepared->named.get_account_memberships.statement,
		[&](sqlite3_stmt* stmt) {
			sqlite3_bind_int64(stmt, 1, user_id);
		},
		[](sqlite3_stmt* stmt) -> account {
			account row;
			row.id_           = sqlite3_column_int64 (stmt, 0);
			row.name          = extract_text         (stmt, 1);
			row.closed_at     = extract_optional_text(stmt, 2);
			row.bank_ref      = extract_optional_text(stmt, 3);
			row.import_source = extract_optional_text(stmt, 4);
			return row;
		}
	);
}

db::result<std::vector<account>> db::get_account_nonmemberships(int64_t user_id) {
	return fetch_many<account>(
		prepared->named.get_account_nonmemberships.statement,
		[&](sqlite3_stmt* stmt) {
			sqlite3_bind_int64(stmt, 1, user_id);
		},
		[](sqlite3_stmt* stmt) -> account {
			account row;
			row.id_           = sqlite3_column_int64 (stmt, 0);
			row.name          = extract_text         (stmt, 1);
			row.closed_at     = extract_optional_text(stmt, 2);
			row.bank_ref      = extract_optional_text(stmt, 3);
			row.import_source = extract_optional_text(stmt, 4);
			return row;
		}
	);
}

db::result<std::vector<user>> db::get_account_members(int64_t account_id) {
	return fetch_many<user>(
		prepared->named.get_account_members.statement,
		[&](sqlite3_stmt* stmt) {
			sqlite3_bind_int64(stmt, 1, account_id);
		},
		[](sqlite3_stmt* stmt) -> user {
			user row;
			row.id_   = sqlite3_column_int64(stmt, 0);
			row.name  = extract_text        (stmt, 1);
			return row;
		}
	);
}

db::result<std::vector<user>> db::get_account_nonmembers(int64_t account_id) {
	return fetch_many<user>(
		prepared->named.get_account_nonmembers.statement,
		[&](sqlite3_stmt* stmt) {
			sqlite3_bind_int64(stmt, 1, account_id);
		},
		[](sqlite3_stmt* stmt) -> user {
			user row;
			row.id_   = sqlite3_column_int64(stmt, 0);
			row.name  = extract_text        (stmt, 1);
			return row;
		}
	);
}

db::error db::add_user_to_account(int64_t account_id, int64_t user_id) {
	return execute(
		prepared->named.add_user_to_account.statement,
		[&](sqlite3_stmt* stmt) {
			sqlite3_bind_int64(stmt, 1, account_id);
			sqlite3_bind_int64(stmt, 2, user_id);
		}
	);
}

db::error db::remove_user_from_account(int64_t account_id, int64_t user_id) {
	return execute(
		prepared->named.remove_user_from_account.statement,
		[&](sqlite3_stmt* stmt) {
			sqlite3_bind_int64(stmt, 1, account_id);
			sqlite3_bind_int64(stmt, 2, user_id);
		}
	);
}

db::result<std::vector<fund>> db::get_fund_memberships(int64_t user_id) {
	return fetch_many<fund>(
		prepared->named.get_fund_memberships.statement,
		[&](sqlite3_stmt* stmt) {
			sqlite3_bind_int64(stmt, 1, user_id);
		},
		[](sqlite3_stmt* stmt) -> fund {
			fund row;
			row.id_       = sqlite3_column_int64 (stmt, 0);
			row.name      = extract_text         (stmt, 1);
			row.closed_at = extract_optional_text(stmt, 2);
			return row;
		}
	);
}

db::result<std::vector<fund>> db::get_fund_nonmemberships(int64_t user_id) {
	return fetch_many<fund>(
		prepared->named.get_fund_nonmemberships.statement,
		[&](sqlite3_stmt* stmt) {
			sqlite3_bind_int64(stmt, 1, user_id);
		},
		[](sqlite3_stmt* stmt) -> fund {
			fund row;
			row.id_       = sqlite3_column_int64 (stmt, 0);
			row.name      = extract_text         (stmt, 1);
			row.closed_at = extract_optional_text(stmt, 2);
			return row;
		}
	);
}

db::result<std::vector<user>> db::get_fund_members(int64_t fund_id) {
	return fetch_many<user>(
		prepared->named.get_fund_members.statement,
		[&](sqlite3_stmt* stmt) {
			sqlite3_bind_int64(stmt, 1, fund_id);
		},
		[](sqlite3_stmt* stmt) -> user {
			user row;
			row.id_   = sqlite3_column_int64(stmt, 0);
			row.name  = extract_text        (stmt, 1);
			return row;
		}
	);
}

db::result<std::vector<user>> db::get_fund_nonmembers(int64_t fund_id) {
	return fetch_many<user>(
		prepared->named.get_fund_nonmembers.statement,
		[&](sqlite3_stmt* stmt) {
			sqlite3_bind_int64(stmt, 1, fund_id);
		},
		[](sqlite3_stmt* stmt) -> user {
			user row;
			row.id_   = sqlite3_column_int64(stmt, 0);
			row.name  = extract_text        (stmt, 1);
			return row;
		}
	);
}

db::error db::add_user_to_fund(int64_t fund_id, int64_t user_id) {
	return execute(
		prepared->named.add_user_to_fund.statement,
		[&](sqlite3_stmt* stmt) {
			sqlite3_bind_int64(stmt, 1, fund_id);
			sqlite3_bind_int64(stmt, 2, user_id);
		}
	);
}

db::error db::remove_user_from_fund(int64_t fund_id, int64_t user_id) {
	return execute(
		prepared->named.remove_user_from_fund.statement,
		[&](sqlite3_stmt* stmt) {
			sqlite3_bind_int64(stmt, 1, fund_id);
			sqlite3_bind_int64(stmt, 2, user_id);
		}
	);
}

db::result<std::vector<fund>> db::get_funds() {
	return fetch_many<fund>(
		prepared->named.get_funds.statement,
		[](sqlite3_stmt*) {},
		[](sqlite3_stmt* stmt) -> fund {
			fund row;
			row.id_       = sqlite3_column_int64 (stmt, 0);
			row.name      = extract_text         (stmt, 1);
			row.closed_at = extract_optional_text(stmt, 2);
			return row;
		}
	);
}

db::error db::save_fund(fund& fund) {
	if (!fund.is_persisted()) {
		error err = execute(
			prepared->named.insert_fund.statement,
			[&](sqlite3_stmt* stmt) {
				bind_text(stmt, 1, fund.name);
			}
		);
		if (err != error::none) { return err; }
		fund.id_= sqlite3_last_insert_rowid(connection);
		return error::none;
	} else {
		return execute(
			prepared->named.update_fund.statement,
			[&](sqlite3_stmt* stmt) {
				bind_text         (stmt, 1, fund.name);
				bind_optional_text(stmt, 2, fund.closed_at);
				sqlite3_bind_int64(stmt, 3, fund.id_);
			}
		);
	}
}

db::result<std::vector<account>> db::get_accounts() {
	return fetch_many<account>(
		prepared->named.get_accounts.statement,
		[](sqlite3_stmt*) {},
		[](sqlite3_stmt* stmt) -> account {
			account row;
			row.id_           = sqlite3_column_int64 (stmt, 0);
			row.name          = extract_text         (stmt, 1);
			row.closed_at     = extract_optional_text(stmt, 2);
			row.bank_ref      = extract_optional_text(stmt, 3);
			row.import_source = extract_optional_text(stmt, 4);
			return row;
		}
	);
}

db::error db::save_account(account& account) {
	if (!account.is_persisted()) {
		error err = execute(
			prepared->named.insert_account.statement,
			[&](sqlite3_stmt* stmt) {
				bind_text         (stmt, 1, account.name);
				bind_optional_text(stmt, 2, account.bank_ref);
				bind_optional_text(stmt, 3, account.import_source);
			}
		);
		if (err != error::none) { return err; }
		account.id_= sqlite3_last_insert_rowid(connection);
		return error::none;
	} else {
		return execute(
			prepared->named.update_account.statement,
			[&](sqlite3_stmt* stmt) {
				bind_text         (stmt, 1, account.name);
				bind_optional_text(stmt, 2, account.closed_at);
				bind_optional_text(stmt, 3, account.bank_ref);
				bind_optional_text(stmt, 4, account.import_source);
				sqlite3_bind_int64(stmt, 5, account.id_);
			}
		);
	}
}

#pragma endregion

#pragma region Lifecycle

std::shared_ptr<db> db::open_file(std::string path) {
	sqlite3* connection;
	int rc = sqlite3_open(path.c_str(), &connection);
	if (rc != SQLITE_OK) {
		sqlite3_close(connection); // must still close even on failure
		return std::make_shared<db>(classify_sqlite_open_error(rc));
	}
	sqlite3_busy_timeout(connection, 5000);
	return std::make_shared<db>(connection, owns_connection{});
}

std::shared_ptr<db> db::open_memory() {
	sqlite3* connection;
	int rc = sqlite3_open(":memory:", &connection);
	if (rc != SQLITE_OK) {
		sqlite3_close(connection); // must still close even on failure
		return std::make_shared<db>(classify_sqlite_open_error(rc));
	}
	return std::make_shared<db>(connection, owns_connection{});
}

static inline db::error classify_sqlite_open_error(int rc) {
	switch (rc) {
		case SQLITE_FULL:
			return db::error::disk_full;
		case SQLITE_NOMEM:
			return db::error::out_of_memory;
		case SQLITE_BUSY:
		case SQLITE_LOCKED:
		case SQLITE_READONLY:
			return db::error::unavailable;
		default:
			FUNDOS_ASSERT(false, "unhandled sqlite3 result code"); // In production fall through to corrupted
			[[fallthrough]];
		case SQLITE_CORRUPT:
		case SQLITE_NOTADB:
		case SQLITE_IOERR:
			return db::error::corrupted;
	}
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

		get_sqlite3_error();
		close(); // prepare is only called during initialization; closing unconditionally is safe since there is no path for the caller to retry
		if (SQLITE_ERROR == rc) { // SQL referenced a table/column that doesn't exist — schema drift
			open_result.result = status::code::schema_error;
			open_result.schema_status = schema_state::schema_mismatch;
		} else {
			open_result.result = status::code::sqlite3_error;
			open_result.sqlite3_error = classify_sqlite_open_error(rc);
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

		get_sqlite3_error();
		error out = error::none;
		switch (rc) {
			case SQLITE_ERROR: // Treat the db as corrupted if there is schema drift during migration
			case SQLITE_CONSTRAINT: // A migration violated a constraint, treat as corrupted
				out = error::corrupted;
				break;
			default:
				out = classify_sqlite_open_error(rc);
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
	if (open_result.result == status::code::needs_migration) {
		open_result.result = status::code::ok;
	}
	// prepare cannot happen for older_schema as migrate must be called manually, so we call it here *or* when opening a file with a current schema
	prepare();
	return open_result.result == status::code::schema_error
		? error::corrupted
		: open_result.sqlite3_error;
}

void db::open() {
	if (connection == nullptr) {
		open_result.result = status::code::null_db;
		delete prepared;
		prepared = nullptr;
		return;
	}

	int rc = sqlite3_exec(connection, "PRAGMA locking_mode = EXCLUSIVE;", nullptr, nullptr, nullptr); // Just adds an extra assurance that the db isn't going to change while we have it open
	if (SQLITE_OK != rc) {
		get_sqlite3_error();
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_error = classify_sqlite_open_error(rc);
		close();
		return;
	}

	rc = sqlite3_exec(connection, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr); // required for cascade delete
	if (SQLITE_OK != rc) {
		get_sqlite3_error();
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_error = classify_sqlite_open_error(rc);
		close();
		return;
	}

	int schema_objects = 0;
	rc = sqlite3_exec(connection,
		"SELECT COUNT(*) FROM sqlite_schema",
		[](void* data, int, char** cols, char**) {
			// The failure mode for a query against sqlite_schema is practically negligible, atoi is fine
			*static_cast<int*>(data) = std::atoi(cols[0]);
			return 0;
		},
		&schema_objects, nullptr);
	if (SQLITE_OK != rc) {
		get_sqlite3_error();
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_error = classify_sqlite_open_error(rc);
		close();
		return;
	}

	if (schema_objects == 0) {
		open_result.schema_status = schema_state::created;
		error err = migrate();
		switch (err) {
			default:
				FUNDOS_ASSERT(false, "unhandled migration error");
				[[fallthrough]];
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

	// non-empty db: ensure it's ours by checking for existence of meta table and the application identifier
	std::string app;
	rc = sqlite3_exec(connection,
		"SELECT value FROM meta WHERE key='application'"
		" AND EXISTS (SELECT 1 FROM sqlite_master WHERE type='table' AND name='meta')",
		[](void* data, int, char** cols, char**) {
			if (cols[0] == nullptr) { return 1; } // value could be null if this is a foreign db with a meta table but no application key
			*static_cast<std::string*>(data) = cols[0]; // string = null is undefined behavior, thus the prior check
			return 0;
		},
		&app, nullptr);
	if (SQLITE_OK != rc) {
		get_sqlite3_error();
		switch (rc) {
			case SQLITE_ERROR: // meta table exists but doesn't have key column or value column
			case SQLITE_ABORT: // meta table has application key but its value is null
				open_result.result = status::code::schema_error;
				open_result.schema_status = schema_state::app_mismatch;
				break;
			default:
				open_result.result = status::code::sqlite3_error;
				open_result.sqlite3_error = classify_sqlite_open_error(rc);
		}
		close();
		return;
	}

	if (app != "fundos") { // if meta table does not have application key this will be an empty string (0 rows returned)
		open_result.schema_status = schema_state::app_mismatch;
		open_result.result = status::code::schema_error;
		close();
		return;
	}

	// app matches: read version
	sqlite3_stmt* schema_statement;
	rc = sqlite3_prepare_v2(connection,
		"SELECT value FROM meta WHERE key='schema_version'", -1, // length, -1 = read to null terminator
		&schema_statement, nullptr); // pzTail is only used for multi-statement strings
	if (SQLITE_OK != rc) { // Previous queries have ruled out sql errors, let classify_sqlite_error default if they occur
		get_sqlite3_error();
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_error = classify_sqlite_open_error(rc);
		close();
		return;
	}

	rc = sqlite3_step(schema_statement);
	if (rc == SQLITE_DONE) { // No row returned — meta table exists but schema_version key is missing
		sqlite3_finalize(schema_statement);
		open_result.result = status::code::schema_error;
		open_result.schema_status = schema_state::schema_mismatch;
		open_result.sqlite3_error = error::corrupted;
		close();
		return;
	}
	if (rc != SQLITE_ROW) { // Unexpected sql error
		get_sqlite3_error();
		sqlite3_finalize(schema_statement);
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_error = classify_sqlite_open_error(rc);
		close();
		return;
	}

	int64_t version = sqlite3_column_int64(schema_statement, 0);
	sqlite3_finalize(schema_statement);

	if (version <= 0) { // db was affected by 3rd party in unpredictable way
		open_result.result = status::code::schema_error;
		open_result.schema_status = schema_state::schema_mismatch;
		open_result.sqlite3_error = error::corrupted;
		close();
		return;
	}

	schema = static_cast<uint64_t>(version);
	if (schema < schema_latest_version) {
		open_result.schema_status = schema_state::older_schema;
		open_result.result = status::code::needs_migration;
		return;
	} else if (schema > schema_latest_version) {
		open_result.schema_status = schema_state::newer_schema;
		open_result.result = status::code::schema_error;
		close();
		return;
	} else {
		open_result.schema_status = schema_state::current;
	}
	prepare(); // if prepare succeeds we will "trust" this db
}
void db::close() {
	if (connection == nullptr) { return; }  // prepared and connection have parity and are managed as one resource
	for (size_t i = 0; i < num_prepared; ++i) {
		sqlite3_finalize(prepared->slots[i].statement); // passing a nullptr is a noop
		prepared->slots[i].statement = nullptr;
	}
	if (managed) {
		sqlite3_close(connection);
	}
	delete prepared;
	prepared = nullptr;
	connection = nullptr;
}

db::db(error err) : connection(nullptr), managed(false), prepared(nullptr) {
	open_result.result = status::code::sqlite3_error;
	open_result.sqlite3_error = err;
}
db::db(sqlite3* c)                  : connection(c), managed(false), prepared(new db_prepared_statements()) { open(); }
db::db(sqlite3* c, owns_connection) : connection(c), managed(true),  prepared(new db_prepared_statements()) { open(); }
db::~db() { close(); }

#pragma endregion

} // fundos
