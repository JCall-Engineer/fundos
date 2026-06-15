// Internal implementation detail — not part of the public API.
// Include only from db.cpp and test translation units.
#pragma once
#include <string_view>
#include "sqlite3.h"

namespace fundos::schema {

static constexpr std::string_view schema_migrations[] = {
R"sql(
CREATE TABLE meta (
	key   TEXT PRIMARY KEY,
	value TEXT NOT NULL
);
INSERT INTO meta (key, value) VALUES ('application', 'fundos');
INSERT INTO meta (key, value) VALUES ('schema_version', '1');

CREATE TABLE funds (
	id        INTEGER PRIMARY KEY,
	name      TEXT NOT NULL,
	closed_at INTEGER,
	UNIQUE(name COLLATE NOCASE)
);

CREATE TABLE accounts (
	id              INTEGER PRIMARY KEY,
	name            TEXT NOT NULL,
	closed_at       INTEGER,
	bank_account_id TEXT,
	UNIQUE(bank_account_id),
	UNIQUE(name COLLATE NOCASE)
);

CREATE TABLE budgets (
	id              INTEGER PRIMARY KEY,
	name            TEXT NOT NULL,
	overflow_fund   INTEGER NOT NULL REFERENCES funds(id) ON DELETE RESTRICT,
	UNIQUE(name COLLATE NOCASE)
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
	position          INTEGER NOT NULL,
	fund_id           INTEGER NOT NULL REFERENCES funds(id),
	amount            INTEGER NOT NULL,
	cap               INTEGER DEFAULT NULL,
	allow_overdraw    INTEGER NOT NULL DEFAULT 0 CHECK(allow_overdraw IN (0, 1)),
	UNIQUE(phase_id, position)
);
CREATE INDEX idx_phase_targets_phase_id ON phase_targets(phase_id);
CREATE INDEX idx_phase_targets_fund_id ON phase_targets(fund_id);

CREATE TABLE transactions (
	id              INTEGER PRIMARY KEY,
	account_id      INTEGER NOT NULL REFERENCES accounts(id) ON DELETE RESTRICT,
	amount          INTEGER NOT NULL,
	date_recorded   INTEGER NOT NULL,
	memo            TEXT NOT NULL,
	date_reconciled INTEGER,
	fitid           TEXT,
	date_cleared    INTEGER,
	corrects_fitid  TEXT,
	correct_action  TEXT CHECK(correct_action IN ('replace', 'delete')),
	corrects_id     INTEGER REFERENCES transactions(id) ON DELETE RESTRICT,
	superseded_by   INTEGER REFERENCES transactions(id) ON DELETE RESTRICT,
	CHECK((date_cleared IS NULL) = (fitid IS NULL)),                                    -- date_cleared signals a bank authoritative date
	CHECK((date_cleared IS NULL) OR (date_reconciled IS NULL)),                         -- date_reconciled signals a user defined date that reconciles OFX imports
	CHECK((correct_action IS NULL) = (corrects_fitid IS NULL AND corrects_id IS NULL)), -- correct_action requires a transaction being corrected, and corrections require an action
	UNIQUE(corrects_id),
	UNIQUE(superseded_by),
	UNIQUE(account_id, fitid)
);
CREATE INDEX idx_transactions_account_id ON transactions(account_id);
CREATE INDEX idx_transactions_corrects ON transactions(corrects_id) WHERE corrects_id IS NOT NULL;
CREATE INDEX idx_transactions_superseded ON transactions(superseded_by) WHERE superseded_by IS NOT NULL;

CREATE TABLE import_ledger_balances (
	id                INTEGER PRIMARY KEY,
	account_id        INTEGER NOT NULL REFERENCES accounts(id) ON DELETE RESTRICT,
	amount            INTEGER NOT NULL,
	date_as_of        INTEGER NOT NULL
);
CREATE INDEX idx_import_account_id ON import_ledger_balances(account_id);

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

/// Each prepared statement is a statement_slot: a sqlite3_stmt* paired with the SQL string used to prepare it.
/// All slots are named members of `statements`, giving call sites readable access by name.
/// db_prepared_statements is a union of `statements` and a flat statement_slot[] array.
/// The array view lets prepare() call sqlite3_prepare_v3 on every slot by index, without naming each one.
/// The static_assert enforces that no padding exists between slots, which is what makes the array view safe.
///
/// Extractor lambdas are intentionally duplicated at each call site rather than factored into shared helpers.
/// Queries that happen to select the same columns are still independent queries.
/// A shared extractor would couple them through column order — a silent contract that breaks when either query changes.
/// The duplication is the most honest expression of that independence.
struct statements {
	statement_slot get_meta { .sql = R"sql(
		SELECT value FROM meta WHERE key = ?
	)sql" };
	statement_slot set_meta { .sql = R"sql(
		INSERT INTO meta (key, value) VALUES (?, ?)
		ON CONFLICT (key) DO UPDATE SET value = excluded.value
	)sql" };

	statement_slot get_funds { .sql = R"sql(
		SELECT id, name, closed_at FROM funds ORDER BY name
	)sql" };
	statement_slot insert_fund { .sql = R"sql(
		INSERT INTO funds (name) VALUES (?)
	)sql" };
	statement_slot update_fund { .sql = R"sql(
		UPDATE funds
		SET name = ?, closed_at = ?
		WHERE id = ?
	)sql" };
	statement_slot get_fund_balance { .sql = R"sql(
		SELECT COALESCE(SUM(allocations.amount), 0)
		FROM allocations JOIN transactions ON allocations.transaction_id = transactions.id
		WHERE allocations.fund_id = ? AND transactions.superseded_by IS NULL
	)sql" };

	statement_slot get_accounts { .sql = R"sql(
		SELECT id, name, closed_at, bank_account_id FROM accounts ORDER BY name
	)sql" };
	statement_slot insert_account { .sql = R"sql(
		INSERT INTO accounts (name, bank_account_id)
		VALUES (?, ?)
	)sql" };
	statement_slot update_account { .sql = R"sql(
		UPDATE accounts
		SET name = ?, closed_at = ?, bank_account_id = ?
		WHERE id = ?
	)sql" };
	statement_slot get_account_balance { .sql = R"sql(
		SELECT COALESCE(SUM(amount), 0)
		FROM transactions
		WHERE account_id = ? AND superseded_by IS NULL
	)sql" };

	statement_slot get_budgets { .sql = R"sql(
		SELECT id, name, overflow_fund FROM budgets ORDER BY name
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
		SELECT id, kind
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
		SET position = ?
		WHERE id = ? AND budget_id = ?
	)sql" };

	statement_slot get_targets { .sql = R"sql(
		SELECT id, fund_id, amount, cap, allow_overdraw
		FROM phase_targets
		WHERE phase_id = ?
		ORDER BY position
	)sql" };
	statement_slot insert_target { .sql = R"sql(
		INSERT INTO phase_targets (phase_id, position, fund_id, amount, cap, allow_overdraw)
		VALUES (?, ?, ?, ?, ?, ?)
	)sql" };
	statement_slot update_target { .sql = R"sql(
		UPDATE phase_targets
		SET position = ?, fund_id = ?, amount = ?, cap = ?, allow_overdraw = ?
		WHERE id = ? AND phase_id = ?
	)sql" };

	/// Used to resolve imported bank ids
	statement_slot find_account_by_bank_id { .sql = R"sql(
		SELECT id
		FROM accounts
		WHERE bank_account_id = ?
	)sql" };

	/// Used to find manual transactions that may need to be updated by an import.
	statement_slot find_transaction_candidates { .sql = R"sql(
		SELECT id, amount, date_recorded, memo
		FROM transactions
		WHERE
			account_id = ?
			AND date_reconciled IS NULL -- Reconciliation transactions are user assertions and should be corrected rather than matched to an OFX import
			AND fitid IS NULL           -- Excludes previously imported transactions; also excludes bank corrections via schema constraint (corrects_fitid IS NULL implied, corrects_id for bank corrections handled)
			AND correct_action IS NULL  -- Excludes all corrections; manual corrections with corrects_id set are caught here since fitid IS NULL only handles the bank correction case
			AND superseded_by IS NULL   -- Excludes voided manual transactions
	)sql" };

	/// Used to find records of previously imported transactions.
	statement_slot find_transaction_by_fitid { .sql = R"sql(
		SELECT id, amount, date_recorded, date_cleared, memo, fitid, corrects_fitid, correct_action, corrects_id, superseded_by
		FROM transactions
		WHERE fitid = ? AND account_id = ?
	)sql" };

	/// Used to validate transactions aren't modified illegally before saving
	statement_slot find_transaction_by_id { .sql = R"sql(
		SELECT account_id, amount, date_cleared, fitid, corrects_fitid, correct_action, corrects_id, superseded_by
		FROM transactions
		WHERE id = ?
	)sql" };

	statement_slot insert_transaction_import { .sql = R"sql(
		INSERT INTO transactions (account_id, amount, date_recorded, date_cleared, memo, fitid, corrects_fitid, correct_action)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?)
	)sql" };
	statement_slot update_transaction_import { .sql = R"sql(
		UPDATE transactions
		SET amount = ?, date_recorded = ?, date_cleared = ?, memo = ?, fitid = ?, corrects_fitid = ?, correct_action = ?
		WHERE id = ?
	)sql" };

	statement_slot insert_transaction_user { .sql = R"sql(
		INSERT INTO transactions (account_id, amount, date_recorded, memo, date_reconciled, corrects_id, correct_action)
		VALUES (?, ?, ?, ?, ?, ?, ?)
	)sql" };
	statement_slot update_transaction_user { .sql = R"sql(
		UPDATE transactions
		SET date_recorded = ?, memo = ?, date_reconciled = ?
		WHERE id = ?
	)sql" };

	/// Used to allow manual corrections
	statement_slot update_transaction_correction { .sql = R"sql(
		UPDATE transactions
		SET superseded_by = ?
		WHERE id = ? AND superseded_by IS NULL
	)sql" };

	statement_slot insert_import_ledger_balance { .sql = R"sql(
		INSERT INTO import_ledger_balances (account_id, amount, date_as_of)
		VALUES (?, ?, ?)
	)sql" };

	statement_slot filter_transactions { .sql = R"sql(
		WITH params AS (
			SELECT
				? AS account_id,
				? AS range_start,
				? AS range_end
		),
		cleared_total AS (
			SELECT COALESCE(SUM(amount), 0) AS total
			FROM transactions, params
			WHERE transactions.account_id = params.account_id
				AND COALESCE(date_reconciled, date_cleared) IS NOT NULL
				AND superseded_by IS NULL
				AND correct_action IS NOT 'delete'
		),
		all_transactions AS (
			SELECT
				id,
				amount,
				date_recorded,
				memo,
				date_reconciled,
				fitid,
				date_cleared,
				corrects_fitid,
				correct_action,
				corrects_id,
				superseded_by,
				CASE
					WHEN COALESCE(date_reconciled, date_cleared) IS NOT NULL THEN 0
					ELSE 1
				END AS is_pending,
				COALESCE(date_reconciled, date_cleared, date_recorded) AS effective_date
			FROM transactions, params
			WHERE transactions.account_id = params.account_id
				AND superseded_by IS NULL
				AND correct_action IS NOT 'delete'
		),
		with_balance AS (
			SELECT
				all_transactions.*,
				CASE
					WHEN is_pending = 0 THEN
						SUM(amount) OVER (
							PARTITION BY is_pending
							ORDER BY effective_date
							ROWS UNBOUNDED PRECEDING
						)
					ELSE
						(SELECT total FROM cleared_total) +
						SUM(amount) OVER (
							PARTITION BY is_pending
							ORDER BY date_recorded
							ROWS UNBOUNDED PRECEDING
						)
				END AS account_balance
			FROM all_transactions
		)
		SELECT * FROM with_balance
		WHERE
			(is_pending = 0 AND effective_date BETWEEN (SELECT range_start FROM params) AND (SELECT range_end FROM params)) OR
			(is_pending = 1 AND date_recorded BETWEEN (SELECT range_start FROM params) AND (SELECT range_end FROM params))
		ORDER BY effective_date DESC, is_pending
	)sql" };

	statement_slot filter_allocations { .sql = R"sql(
		WITH all_allocations AS (
			SELECT
				transactions.id,
				transactions.account_id,
				transactions.amount,
				transactions.date_recorded,
				transactions.memo,
				transactions.date_reconciled,
				transactions.fitid,
				transactions.date_cleared,
				transactions.corrects_fitid,
				transactions.correct_action,
				transactions.corrects_id,
				transactions.superseded_by,
				allocations.id AS allocation_id,
				allocations.amount AS allocation_amount,
				SUM(allocations.amount) OVER (ORDER BY transactions.date_recorded ROWS UNBOUNDED PRECEDING) AS fund_balance
			FROM allocations
			JOIN transactions ON transactions.id = allocations.transaction_id
			WHERE allocations.fund_id = ? AND transactions.superseded_by IS NULL AND transactions.correct_action IS NOT 'delete'
		)
		SELECT * FROM all_allocations
		WHERE date_recorded BETWEEN ? AND ?
		ORDER BY date_recorded DESC
	)sql" };

	statement_slot filter_ledger_balances { .sql = R"sql(
		SELECT id, amount, date_as_of
		FROM import_ledger_balances
		WHERE account_id = ? AND date_as_of BETWEEN ? AND ?
		ORDER BY date_as_of
	)sql" };

	statement_slot get_transaction_allocations { .sql = R"sql(
		SELECT id, fund_id, amount FROM allocations WHERE transaction_id = ?
	)sql" };

	/// Used to validate allocations
	statement_slot get_transaction_amount { .sql = R"sql(
		SELECT amount FROM transactions WHERE id = ?
	)sql" };

	statement_slot insert_allocation { .sql = R"sql(
		INSERT INTO allocations (transaction_id, fund_id, amount)
		VALUES (?, ?, ?)
	)sql" };
	statement_slot update_allocation { .sql = R"sql(
		UPDATE allocations
		SET fund_id = ?, amount = ?
		WHERE id = ?
	)sql" };
	//statement_slot name { .sql = R"sql()sql" };
};
static constexpr std::size_t num_prepared = sizeof(statements) / sizeof(statement_slot);
static_assert(sizeof(statements) % sizeof(statement_slot) == 0, "db_prepared_statements must contain only statement_slot members with no padding");

// I begroan that this layout isn't cache-efficient but prepared statements are only accessed in order once so it doesn't matter and this gives us safety
union db_prepared_statements {
	statement_slot slots[num_prepared];
	statements named = {};
};

} // namespace fundos::schema
