#include <array>
#include <format>
#include <functional>
#include <unordered_set>
#include "db.hpp"
#include "platform.hpp"
#include "schema.inc"

namespace fundos {

#pragma region Query Execution Layer

template<typename Enum, std::size_t N>
using enum_string_map = std::array<std::pair<Enum, std::string>, N>;

template<typename Enum, std::size_t N>
static inline std::optional<std::string> enum_to_string(const enum_string_map<Enum, N>& map, Enum value) {
	for (const auto &pair : map) {
		if (pair.first == value) { return pair.second; }
	}
	return std::nullopt;
}

template<typename Enum, std::size_t N>
static inline std::optional<Enum> string_to_enum(const enum_string_map<Enum, N>& map, std::string_view value) {
	for (const auto &pair : map) {
		if (pair.second == value) { return pair.first; }
	}
	return std::nullopt;
}

template<typename Enum, std::size_t N>
static inline std::optional<std::string> optional_enum_to_string(const enum_string_map<Enum, N>& map, const std::optional<Enum>& value) {
	if (!value) { return std::nullopt; }
	return enum_to_string(map, *value);
}

template<typename Enum, std::size_t N>
static inline std::optional<Enum> optional_string_to_enum(const enum_string_map<Enum, N>& map, const std::optional<std::string>& value) {
	if (!value) { return std::nullopt; }
	return string_to_enum(map, *value);
}

static inline std::string extract_text(sqlite3_stmt* stmt, int index) {
	return reinterpret_cast<const char*>(sqlite3_column_text(stmt, index));
}
static inline void bind_text(sqlite3_stmt* stmt, int index, const std::string& value) {
	sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT); // I wanted to use SQLITE_STATIC but implicit string copies has become complicated
}

static inline std::optional<std::string> extract_optional_text(sqlite3_stmt* stmt, int index) {
	if (sqlite3_column_type(stmt, index) == SQLITE_NULL) { return std::nullopt; }
	return reinterpret_cast<const char*>(sqlite3_column_text(stmt, index));
}
static inline void bind_optional_text(sqlite3_stmt* stmt, int index, const std::optional<std::string>& value) {
	if (value) {
		sqlite3_bind_text(stmt, index, value->c_str(), -1, SQLITE_TRANSIENT); // I wanted to use SQLITE_STATIC but implicit string copies has become complicated
	} else {
		sqlite3_bind_null(stmt, index);
	}
}

template<typename T>
static inline std::optional<int64_t> as_optional_int64(const std::optional<T>& value) {
	if (!value) { return std::nullopt; }
	return static_cast<int64_t>(*value);
}
template<typename T>
static inline std::optional<T> as_optional(const std::optional<int64_t>& value) {
	if (!value) { return std::nullopt; }
	return T{*value};
}

static inline std::optional<int64_t> extract_optional_int64(sqlite3_stmt* stmt, int index) {
	if (sqlite3_column_type(stmt, index) == SQLITE_NULL) { return std::nullopt; }
	return sqlite3_column_int64(stmt, index);
}
static inline void bind_optional_int64(sqlite3_stmt* stmt, int index, const std::optional<int64_t>& value) {
	if (value) {
		sqlite3_bind_int64(stmt, index, *value);
	} else {
		sqlite3_bind_null(stmt, index);
	}
}

db::error db::classify_sqlite_runtime_error(int rc) {
	switch (rc & 0xFF) {
		case SQLITE_FULL:
			return error::disk_full;
		case SQLITE_NOMEM:
			return error::out_of_memory;
		case SQLITE_BUSY:
		case SQLITE_LOCKED:
		// READONLY cannot be returned at step time
			return error::unavailable;
		case SQLITE_CORRUPT:
		// NOTADB cannot be returned at step time
		case SQLITE_IOERR:
			return error::corrupted;
		case SQLITE_CONSTRAINT:
			return error::constraint;
		case SQLITE_INTERRUPT: // can occur from db::interrupt
			return error::interrupted;
		default:
			FUNDOS_ASSERT(false, "unhandled sqlite3 step result code");
			return error::internal;
	}
}

db::outcome db::sql_count_check(const std::string& sql, size_t expected, executor bind, message on_failure) {
	if (!is_ready()) { return not_ready(); }
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(connection, sql.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK) { return sqlite_runtime_error(rc); }
	auto result = sql_fetch_one<int64_t>(
		stmt,
		bind, [](sqlite3_stmt* stmt) -> int64_t {
			return sqlite3_column_int64(stmt, 0);
		}
	);
	sqlite3_finalize(stmt);

	if (!result) { return result.status(); }
	if (result.value() < 0) { return outcome(error::internal, "Query returned negative rows?"); }
	if (static_cast<size_t>(result.value()) != expected) { return outcome(error::rejected, on_failure); }
	return success();
}

db::outcome db::sql_delete_except(const db::delete_except_params& params) {
	if (!is_ready()) { return not_ready(); }
	std::string sql = std::format("DELETE FROM {} WHERE {} = ?", params.table, params.filter_column);

	if (!params.preserve_ids.empty()) {
		sql += " AND id NOT IN(";
		for (size_t i = 0; i < params.preserve_ids.size(); ++i) {
			sql += i == 0 ? "?" : ", ?";
		}
		sql += ")";
	}

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(connection, sql.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK) { return sqlite_runtime_error(rc); }

	outcome result = sql_execute(stmt, [&](sqlite3_stmt* stmt) {
		sqlite3_bind_int64(stmt, 1, params.filter_value);
		for (size_t i = 0; i < params.preserve_ids.size(); ++i) {
			sqlite3_bind_int64(stmt, static_cast<int>(i) + 2, params.preserve_ids[i]);
		}
	});
	sqlite3_finalize(stmt);
	return result;
}

db::outcome db::sql_execute(sqlite3_stmt* stmt, executor bind) {
	if (!is_ready()) { return not_ready(); }
	sqlite3_reset(stmt);
	bind(stmt);
	int rc = sqlite3_step(stmt);
	if (SQLITE_DONE != rc) {
		return sqlite_runtime_error(rc);
	}
	return error::none;
}

template<typename T>
db::result<T> db::sql_fetch_one(sqlite3_stmt* stmt, executor bind, extractor<T> extract) {
	if (!is_ready()) { return not_ready(); }
	sqlite3_reset(stmt);
	bind(stmt);
	int rc = sqlite3_step(stmt);
	switch (rc) {
		case SQLITE_ROW: {
			return extract(stmt);
		}
		case SQLITE_DONE:
			return outcome(error::not_found);
		default:
			return sqlite_runtime_error(rc);
	}
}

template<typename T>
db::result<std::vector<T>> db::sql_fetch_many(sqlite3_stmt* stmt, executor bind, extractor<T> extract) {
	if (!is_ready()) { return not_ready(); }
	sqlite3_reset(stmt);
	bind(stmt);
	int rc;
	std::vector<T> rows;
	while (SQLITE_ROW == (rc = sqlite3_step(stmt))) {
		rows.push_back(extract(stmt));
	}
	if (SQLITE_DONE != rc) {
		return sqlite_runtime_error(rc);
	}
	return rows;
}

//--------------------------------------------------------------------------------------+
// work() must propagate all errors immediately — transaction() relies on               |
// error::corrupted being returned to skip COMMIT/ROLLBACK on a closed connection.      |
// Do not silently swallow errors from execute/fetch_one/fetch_many inside work().      |
//--------------------------------------------------------------------------------------+
db::outcome db::sql_transaction(std::function<outcome(std::vector<std::function<void()>>&)> work) {
	if (!is_ready()) { return not_ready(); }
	int rc = sqlite3_exec(connection, "BEGIN", nullptr, nullptr, nullptr);
	if (rc != SQLITE_OK) {
		return sqlite_runtime_error(rc);
	}
	std::vector<std::function<void()>> rollback;
	outcome result = work(rollback);
	if (!result) {
		// undo in reverse order for correctness (debatable if necessary)
		for (auto it = rollback.rbegin(); it != rollback.rend(); ++it) { (*it)(); }
	}
	switch (result.code) {
		case error::none:
			rc = sqlite3_exec(connection, "COMMIT", nullptr, nullptr, nullptr);
			if (rc != SQLITE_OK) {
				return sqlite_runtime_error(rc);
			}
			return success();
		case error::interrupted:
			return result;
		default: {
			rc = sqlite3_exec(connection, "ROLLBACK", nullptr, nullptr, nullptr);
			if (SQLITE_OK == rc || rc == SQLITE_INTERRUPT) {
				return result;
			}
			auto msg = sqlite_error_message();
			close(); // failed rollback means untrustworthy state regardless of cause
			return outcome(error::corrupted, msg);
		}
		case error::corrupted:
			return result;
	}
}

db::result<std::string> db::get_meta(std::string key) {
	return sql_fetch_one<std::string>(
		prepared->named.get_meta.statement,
		[&](sqlite3_stmt* stmt) {
			bind_text(stmt, 1, key);
		},
		[](sqlite3_stmt* stmt) -> std::string {
			return extract_text(stmt, 0);
		}
	);
}
db::outcome db::set_meta(std::string key, std::string value) {
	return sql_execute(
		prepared->named.set_meta.statement,
		[&](sqlite3_stmt* stmt) {
			bind_text(stmt, 1, key);
			bind_text(stmt, 2, value);
		}
	);
}

//--------------------------------------------------------------------------------------+
// Extractors are intentionally duplicated per query function rather than shared.       |
// Column indices are determined by each SQL statement's SELECT order, which is         |
// not guaranteed to remain consistent across queries even for the same model type.     |
// Sharing extractors would couple unrelated queries and risk silent data corruption    |
// if any statement's column order ever diverges. Treat each extractor as local         |
// to its query and verify column indices against the SQL when making changes.          |
//--------------------------------------------------------------------------------------+
struct locale_register {
	const std::string percentage_locale_key = "percentage_locale";
	const std::string currency_locale_key   = "currency_locale";

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

static constexpr size_t num_symbol_placement = 2;
static const enum_string_map<percentage_locale::spec::symbol_placement, num_symbol_placement> percentage_symbol_placement_map = {{
	{ percentage_locale::spec::symbol_placement::before, "before" },
	{ percentage_locale::spec::symbol_placement::after,  "after"  },
}};
static const enum_string_map<currency_locale::spec::symbol_placement, num_symbol_placement> currency_symbol_placement_map = {{
	{ currency_locale::spec::symbol_placement::before, "before" },
	{ currency_locale::spec::symbol_placement::after,  "after"  },
}};

static constexpr size_t num_negative_format = 4;
static const enum_string_map<currency_locale::spec::negative_notation, num_negative_format> currency_negative_notation_map = {{
	{ currency_locale::spec::negative_notation::parentheses,    "parentheses"    },
	{ currency_locale::spec::negative_notation::angle_brackets, "angle_brackets" },
	{ currency_locale::spec::negative_notation::leading_minus,  "leading_minus"  },
	{ currency_locale::spec::negative_notation::trailing_minus, "trailing_minus" },
}};

db::result<currency_locale::selection> db::get_currency_locale() {
	static const std::unordered_map<std::string, int16_t> valid_scales = {
		{ "1", int16_t{1} }, { "10", int16_t{10} }, { "100", int16_t{100} }, { "1000", int16_t{1000} },
	};

	auto preset_result = get_meta(locale_meta.currency_locale_key);
	if (!preset_result) {
		auto& status = preset_result.status();
		if (status.code == error::not_found) {
			return outcome(error::not_found, "Currency Locale is not yet set");
		}
		return status;
	}

	std::string &preset_value = preset_result.value();
	if (preset_value == currency_locale::selection::custom_id) {
		// Extract scale from meta
		auto scale_result = get_meta(locale_meta.currency_locale_scale_key);
		if (!scale_result) { return scale_result.status(); }
		auto scale_it = valid_scales.find(scale_result.value());
		if (scale_it == valid_scales.end()) { return outcome(error::not_found, "Recorded currency scale is invalid"); }
		int16_t scale = scale_it->second;

		// Extract symbol from meta
		auto symbol_result = get_meta(locale_meta.currency_locale_symbol_key);
		if (!symbol_result) { return symbol_result.status(); }
		std::string& symbol = symbol_result.value();
		if (symbol.length() > 4) { return outcome(error::not_found, "Recorded currency symbol is too long"); } // This is an implicit assumption for currency::to_string

		// Extract thousands_separator from meta
		auto thousands_result = get_meta(locale_meta.currency_locale_thousands_separator_key);
		if (!thousands_result) { return thousands_result.status(); }
		if (thousands_result.value().empty()) { return outcome(error::not_found, "Recorded thousands separator is empty"); }
		char thousands_separator = thousands_result.value()[0];

		// Extract decimal_separator from meta
		auto decimal_result = get_meta(locale_meta.currency_locale_decimal_separator_key);
		if (!decimal_result) { return decimal_result.status(); }
		if (decimal_result.value().empty()) { return outcome(error::not_found, "Recorded decimal separator is empty"); }
		char decimal_separator = decimal_result.value()[0];

		// Extract symbol position from meta
		auto position_result = get_meta(locale_meta.currency_locale_symbol_position_key);
		if (!position_result) { return position_result.status(); }
		auto position_enum = string_to_enum(currency_symbol_placement_map, position_result.value());
		if (!position_enum.has_value()) { return outcome(error::not_found, "Recorded symbol position is not a recognized string"); }
		currency_locale::spec::symbol_placement symbol_position = position_enum.value();

		// Extract negative format from meta
		auto negative_result = get_meta(locale_meta.currency_locale_negative_format_key);
		if (!negative_result) { return negative_result.status(); }
		auto negative_enum = string_to_enum(currency_negative_notation_map, negative_result.value());
		if (!negative_enum.has_value()) { return outcome(error::not_found, "Recorded negative format is not a recognized string"); }
		currency_locale::spec::negative_notation negative_format = negative_enum.value();

		return currency_locale::selection(currency_locale::spec{
			.scale = scale,
			.symbol = symbol,
			.thousands_separator = thousands_separator,
			.decimal_separator = decimal_separator,
			.symbol_position = symbol_position,
			.negative_format = negative_format,
		});
	}
	auto locale = currency_locale::get_locale(preset_value);
	if (locale) {
		return currency_locale::selection(locale);
	}
	return outcome(error::not_found, "Recorded currency locale preset is not a recognized string");
}
db::outcome db::set_currency_locale(const currency_locale::selection& locale) {
	auto set_identifier = [&]() -> outcome {
		return set_meta(locale_meta.currency_locale_key, locale.identifier());
	};
	if (locale.is_preset()) {
		return set_identifier();
	}
	return sql_transaction([&](std::vector<std::function<void()>>&) -> outcome {
		outcome result = set_identifier();
		if (!result) { return result; }

		switch (locale.info().scale) {
			case 1: case 10: case 100: case 1000:
				result = set_meta(locale_meta.currency_locale_scale_key, std::to_string(locale.info().scale));
				if (!result) { return result; }
				break;
			default:
				return outcome(error::rejected, "Scale was not a recognized value (valid options: 1, 10, 100, 1000)");
		}

		if (locale.info().symbol.length() > 4) { return outcome(error::rejected, "Locale symbol is maximally 4 bytes"); }
		result = set_meta(locale_meta.currency_locale_symbol_key, locale.info().symbol);
		if (!result) { return result; }

		result = set_meta(locale_meta.currency_locale_thousands_separator_key, std::string(1, locale.info().thousands_separator));
		if (!result) { return result; }

		result = set_meta(locale_meta.currency_locale_decimal_separator_key, std::string(1, locale.info().decimal_separator));
		if (!result) { return result; }

		auto symbol_pos = enum_to_string(currency_symbol_placement_map, locale.info().symbol_position);
		if (!symbol_pos.has_value()) { return outcome(error::internal, "Unhandled symbol position"); }
		result = set_meta(locale_meta.currency_locale_symbol_position_key, symbol_pos.value());
		if (!result) { return result; }

		auto negative_format = enum_to_string(currency_negative_notation_map, locale.info().negative_format);
		if (!negative_format.has_value()) { return outcome(error::internal, "unhandled negative notation"); }
		result = set_meta(locale_meta.currency_locale_negative_format_key, negative_format.value());
		if (!result) { return result; }

		return success();
	});
}

db::result<percentage_locale::selection> db::get_percentage_locale() {
	auto preset_result = get_meta(locale_meta.percentage_locale_key);
	if (!preset_result) {
		auto& status = preset_result.status();
		if (status.code == error::not_found) {
			return outcome(error::not_found, "Percentage Locale is not yet set");
		}
		return status;
	}

	std::string &preset_value = preset_result.value();
	if (preset_value == percentage_locale::selection::custom_id) {
		// Extract decimal_separator from meta
		auto decimal_result = get_meta(locale_meta.percentage_locale_decimal_separator_key);
		if (!decimal_result) { return decimal_result.status(); }
		if (decimal_result.value().empty()) { return outcome(error::not_found, "Recorded decimal separator is empty"); }
		char decimal_separator = decimal_result.value()[0];

		// Extract has_space from meta
		auto space_result = get_meta(locale_meta.percentage_locale_has_space_key);
		if (!space_result) { return space_result.status(); }
		bool has_space_around_number = space_result.value() == "1";

		// Extract symbol position from meta
		auto position_result = get_meta(locale_meta.percentage_locale_symbol_position_key);
		if (!position_result) { return position_result.status(); }
		auto position_enum = string_to_enum(percentage_symbol_placement_map, position_result.value());
		if (!position_enum.has_value()) { return outcome(error::not_found, "Recorded symbol position is not a recognized string"); }
		percentage_locale::spec::symbol_placement symbol_position = position_enum.value();

		return percentage_locale::selection(percentage_locale::spec{
			.decimal_separator = decimal_separator,
			.has_space_around_number = has_space_around_number,
			.symbol_position = symbol_position,
		});
	}
	auto locale = percentage_locale::get_locale(preset_value);
	if (locale) {
		return percentage_locale::selection(locale);
	}
	return outcome(error::not_found, "Recorded percentage locale preset is not a recognized string");
}
db::outcome db::set_percentage_locale(const percentage_locale::selection& locale) {
	auto set_identifier = [&]() -> outcome {
		return set_meta(locale_meta.percentage_locale_key, locale.identifier());
	};
	if (locale.is_preset()) {
		return set_identifier();
	}
	return sql_transaction([&](std::vector<std::function<void()>>&) -> outcome {
		outcome result = set_identifier();
		if (!result) { return result; }

		result = set_meta(locale_meta.percentage_locale_decimal_separator_key, std::string(1, locale.info().decimal_separator));
		if (!result) { return result; }

		result = set_meta(locale_meta.percentage_locale_has_space_key, locale.info().has_space_around_number ? "1" : "0");
		if (!result) { return result; }

		auto symbol_pos = enum_to_string(percentage_symbol_placement_map, locale.info().symbol_position);
		if (!symbol_pos.has_value()) { return outcome(error::internal, "Unhandled symbol placement"); }
		result = set_meta(locale_meta.percentage_locale_symbol_position_key, symbol_pos.value());
		if (!result) { return result; }

		return success();
	});
}

db::result<std::vector<fund>> db::get_funds() {
	return sql_fetch_many<fund>(
		prepared->named.get_funds.statement,
		[](sqlite3_stmt*) {},
		[](sqlite3_stmt* stmt) -> fund {
			fund row;
			row.id_       =                       sqlite3_column_int64  (stmt, 0);
			row.name      =                       extract_text          (stmt, 1);
			row.closed_at = as_optional<datetime>(extract_optional_int64(stmt, 2));
			return row;
		}
	);
}
db::result<currency> db::get_fund_balance(int64_t fund_id) {
	return sql_fetch_one<currency>(
		prepared->named.get_fund_balance.statement,
		[&](sqlite3_stmt* stmt) -> void {
			sqlite3_bind_int64(stmt, 1, fund_id);
		},
		[](sqlite3_stmt* stmt) -> currency {
			return currency{sqlite3_column_int64(stmt, 0)};
		}
	);
}
db::outcome db::save_fund(fund& saving) {
	if (!saving.is_persisted()) {
		outcome insert = sql_execute(
			prepared->named.insert_fund.statement,
			[&](sqlite3_stmt* stmt) {
				bind_text(stmt, 1, saving.name);
			}
		);
		if (!insert) { return insert; }
		saving.id_= sqlite3_last_insert_rowid(connection);
	} else {
		outcome update = sql_execute(
			prepared->named.update_fund.statement,
			[&](sqlite3_stmt* stmt) {
				bind_text          (stmt, 1, saving.name);
				bind_optional_int64(stmt, 2, as_optional_int64(saving.closed_at));
				sqlite3_bind_int64 (stmt, 3, saving.id_);
			}
		);
		if (!update) { return update; }
		if (sqlite3_changes(connection) != 1) {
			return outcome(error::not_found, "Cannot update fund which does not exist");
		}
	}
	return success();
}

db::result<std::vector<account>> db::get_accounts() {
	return sql_fetch_many<account>(
		prepared->named.get_accounts.statement,
		[](sqlite3_stmt*) {},
		[](sqlite3_stmt* stmt) -> account {
			account row;
			row.id_             =                       sqlite3_column_int64  (stmt, 0);
			row.name            =                       extract_text          (stmt, 1);
			row.closed_at       = as_optional<datetime>(extract_optional_int64(stmt, 2));
			row.bank_account_id =                       extract_optional_text (stmt, 3);
			return row;
		}
	);
}
db::result<currency> db::get_account_balance(int64_t account_id) {
	return sql_fetch_one<currency>(
		prepared->named.get_account_balance.statement,
		[&](sqlite3_stmt* stmt) -> void {
			sqlite3_bind_int64(stmt, 1, account_id);
		},
		[](sqlite3_stmt* stmt) -> currency {
			return currency{sqlite3_column_int64(stmt, 0)};
		}
	);
}
db::outcome db::save_account(account& saving) {
	if (!saving.is_persisted()) {
		outcome insert = sql_execute(
			prepared->named.insert_account.statement,
			[&](sqlite3_stmt* stmt) {
				bind_text         (stmt, 1, saving.name);
				bind_optional_text(stmt, 2, saving.bank_account_id);
			}
		);
		if (!insert) { return insert; }
		saving.id_= sqlite3_last_insert_rowid(connection);
	} else {
		outcome update = sql_execute(
			prepared->named.update_account.statement,
			[&](sqlite3_stmt* stmt) {
				bind_text          (stmt, 1, saving.name);
				bind_optional_int64(stmt, 2, as_optional_int64(saving.closed_at));
				bind_optional_text (stmt, 3, saving.bank_account_id);
				sqlite3_bind_int64 (stmt, 4, saving.id_);
			}
		);
		if (!update) { return update; }
		if (sqlite3_changes(connection) != 1) {
			return outcome(error::not_found, "Cannot update account which does not exist");
		}
	}
	return success();
}

static const std::string fixed_phase_identifier = "fixed";
static const std::string percentage_phase_identifier = "percentage";
db::result<std::vector<budget>> db::get_budgets() {
	auto budget_result = sql_fetch_many<budget>(
		prepared->named.get_budgets.statement,
		[](sqlite3_stmt*) {},
		[](sqlite3_stmt* stmt) -> budget {
			budget out;
			out.id_           = sqlite3_column_int64(stmt, 0);
			out.name          = extract_text        (stmt, 1);
			out.overflow_fund = sqlite3_column_int64(stmt, 2);
			return out;
		}
	);
	if (!budget_result) { return budget_result.status(); }

	std::vector<budget> &out = budget_result.value(); // fetch_many guarantees a value if no error
	for (auto &budget : out) {
		auto phase_result = sql_fetch_many<any_budget_phase>(
			prepared->named.get_phases.statement,
			[&budget](sqlite3_stmt* stmt) {
				sqlite3_bind_int64(stmt, 1, budget.id_);
			},
			[](sqlite3_stmt* stmt) -> any_budget_phase {
				std::string kind = extract_text(stmt, 1);
				if (kind == fixed_phase_identifier) {
					budget_phase<fixed_target> phase;
					phase.id_ = sqlite3_column_int64(stmt, 0);
					return phase;
				}
				FUNDOS_ASSERT(kind == percentage_phase_identifier, "unexpected phase kind");
				budget_phase<percentage_target> phase;
				phase.id_ = sqlite3_column_int64(stmt, 0);
				return phase;
			}
		);
		if (!phase_result) { return phase_result.status(); }
		outcome fetch_targets_result = error::none;
		for (auto &phase : phase_result.value()) {
			std::visit([&](auto &typed_phase) {
				using TargetType = typename std::decay_t<decltype(typed_phase.targets)>::value_type;
				auto target_result = sql_fetch_many<TargetType>(
					prepared->named.get_targets.statement,
					[&typed_phase](sqlite3_stmt* stmt) {
						sqlite3_bind_int64(stmt, 1, typed_phase.id_);
					},
					[](sqlite3_stmt* stmt) -> TargetType {
						TargetType target;
						target.id_     = sqlite3_column_int64(stmt, 0);
						target.fund_id = sqlite3_column_int64(stmt, 1);
						if constexpr (std::is_same_v<TargetType, fixed_target>) {
							target.amount = currency{sqlite3_column_int64(stmt, 2)};
						} else {
							target.amount = percentage{(int32_t)sqlite3_column_int(stmt, 2)};
						}
						target.cap = as_optional<currency>(extract_optional_int64(stmt, 3));
						target.allow_overdraw = sqlite3_column_int(stmt, 4);
						return target;
					}
				);
				if (!target_result) {
					fetch_targets_result = target_result.status();
					return;
				}
				typed_phase.targets = std::list<TargetType>(
					std::make_move_iterator(target_result.value().begin()),
					std::make_move_iterator(target_result.value().end())
				);
				budget.phases.push_back(std::move(typed_phase));
			}, phase);
			if (!fetch_targets_result) { return fetch_targets_result; }
		}
	}
	return out;
}
db::outcome db::save_budget(budget& saving) {
	return sql_transaction([&](std::vector<std::function<void()>>& rollback) -> outcome {
		outcome result;

		// Update budget
		if (!saving.is_persisted()) {
			result = sql_execute(
				prepared->named.insert_budget.statement,
				[&](sqlite3_stmt* stmt) {
					bind_text(stmt, 1, saving.name);
					sqlite3_bind_int64(stmt, 2, saving.overflow_fund);
				}
			);
			if (!result) { return result; }
			saving.id_= sqlite3_last_insert_rowid(connection);
			rollback.push_back([&saving]() { saving.id_ = 0; });
		} else {
			result = sql_execute(
				prepared->named.update_budget.statement,
				[&](sqlite3_stmt* stmt) {
					bind_text(stmt, 1, saving.name);
					sqlite3_bind_int64(stmt, 2, saving.overflow_fund);
					sqlite3_bind_int64(stmt, 3, saving.id_);
				}
			);
			if (!result) { return result; }
			if (sqlite3_changes(connection) != 1) {
				return outcome(error::not_found, "Cannot update budget which does not exist");
			}
		}

		// Delete phases not found in budget anymore
		{
			std::vector<int64_t> preserve_ids;
			saving.each_phase([&preserve_ids](int, any_budget_phase* phase) -> bool {
				std::visit([&](auto &typed_phase) {
					if (typed_phase.is_persisted()) {
						preserve_ids.push_back(typed_phase.id_);
					}
				}, *phase);
				return false;
			});

			result = sql_delete_except({
				.table = "budget_phases",
				.filter_column = "budget_id",
				.filter_value = saving.id_,
				.preserve_ids = preserve_ids,
			});
			if (!result) { return result; }
		}

		// Deletes targets not found in a phase anymore
		auto delete_orphaned_targets = [&](int64_t phase_id, const std::vector<int64_t>& preserve_ids) -> outcome {
			return sql_delete_except({
				.table = "phase_targets",
				.filter_column = "phase_id",
				.filter_value = phase_id,
				.preserve_ids = preserve_ids,
			});
		};

		// Return value is for the budget::find api, return true on error
		auto upsert_phase = [&](int pos, db_managed* phase_managed, const std::string& kind) -> bool {
			if (phase_managed->is_persisted()) {
				result = sql_execute(prepared->named.update_phase.statement, [&](sqlite3_stmt* stmt) {
					sqlite3_bind_int(stmt, 1, pos);
					sqlite3_bind_int64(stmt, 2, phase_managed->id_);
					sqlite3_bind_int64(stmt, 3, saving.id_);
				});
				if (!result) { return true; }
				if (sqlite3_changes(connection) != 1) {
					result = outcome(error::not_found, "Cannot update phase which does not exist");
					return true;
				}
			} else {
				result = sql_execute(prepared->named.insert_phase.statement, [&](sqlite3_stmt* stmt) {
					sqlite3_bind_int64(stmt, 1, saving.id_);
					sqlite3_bind_int  (stmt, 2, pos);
					bind_text         (stmt, 3, kind);
				});
				if (!result) { return true; }
				phase_managed->id_ = sqlite3_last_insert_rowid(connection);
				rollback.push_back([phase_managed]() { phase_managed->id_ = 0; });
			}
			return false;
		};

		// TIL: since c++14 lambdas with auto parameters are templated functions
		static auto collect_target_ids = [](auto* phase) -> std::vector<int64_t> {
			std::vector<int64_t> preserve_ids;
			phase->each_target([&preserve_ids](int, auto* target) {
				if (target->is_persisted()) {
					preserve_ids.push_back(target->id_);
				}
			});
			return preserve_ids;
		};

		// Since auto* target makes this templated we can access common properties
		auto upsert_target = [&](int pos, auto* target, int64_t phase_id, int64_t amount) -> bool {
			if (target->is_persisted()) {
				result = sql_execute(prepared->named.update_target.statement, [&](sqlite3_stmt* stmt) {
					sqlite3_bind_int   (stmt, 1, pos);
					sqlite3_bind_int64 (stmt, 2, target->fund_id);
					sqlite3_bind_int64 (stmt, 3, amount);
					bind_optional_int64(stmt, 4, as_optional_int64(target->cap));
					sqlite3_bind_int   (stmt, 5, target->allow_overdraw);
					sqlite3_bind_int64 (stmt, 6, target->id_);
					sqlite3_bind_int64 (stmt, 7, phase_id);
				});
				if (!result) { return true; }
				if (sqlite3_changes(connection) != 1) {
					result = outcome(error::not_found, "Cannot update target which does not exist");
					return true;
				}
			} else {
				result = sql_execute(prepared->named.insert_target.statement, [&](sqlite3_stmt* stmt) {
					sqlite3_bind_int64 (stmt, 1, phase_id);
					sqlite3_bind_int   (stmt, 2, pos);
					sqlite3_bind_int64 (stmt, 3, target->fund_id);
					sqlite3_bind_int64 (stmt, 4, amount);
					bind_optional_int64(stmt, 5, as_optional_int64(target->cap));
					sqlite3_bind_int   (stmt, 6, target->allow_overdraw);
				});
				if (!result) { return true; }
				target->id_ = sqlite3_last_insert_rowid(connection);
				rollback.push_back([target]() { target->id_ = 0; });
			}
			return false;
		};

		// We are "finding" errors as we save phases
		auto phase_err = saving.find_phase(
			[&](int pos, budget_phase<fixed_target>* phase) -> bool {
				if (upsert_phase(pos, phase, fixed_phase_identifier)) { return true; }

				result = delete_orphaned_targets(phase->id_, collect_target_ids(phase));
				if (!result) { return true; }

				// "finding" errors as we save targets
				auto target_err = phase->find_target([&](int pos, fixed_target* target) -> bool {
					return upsert_target(pos, target, phase->id_, target->amount.minor_units);
				});
				// Exit phase iterator on error
				if (target_err != nullptr) { return true; }
				return false;
			},
			[&](int pos, budget_phase<percentage_target>* phase) -> bool {
				if (upsert_phase(pos, phase, percentage_phase_identifier)) { return true; }

				result = delete_orphaned_targets(phase->id_, collect_target_ids(phase));
				if (!result) { return true; }

				// "finding" errors as we save targets
				auto target_err = phase->find_target([&](int pos, percentage_target* target) -> bool {
					return upsert_target(pos, target, phase->id_, target->amount.basis_points);
				});
				// Exit phase iterator on error
				if (target_err != nullptr) { return true; }
				return false;
			}
		);
		// Exit transaction on error
		if (phase_err != nullptr) { return result; }
		return success();
	});
}

db::outcome db::delete_budget(int64_t budget_id) {
	return sql_execute(
		prepared->named.delete_budget.statement,
		[&](sqlite3_stmt* stmt) {
			sqlite3_bind_int64(stmt, 1, budget_id);
		}
	);
}

static constexpr size_t num_correct_actions = 2;
static const enum_string_map<transaction::correction_type, num_correct_actions> correction_map = {{
	{ transaction::correction_type::deletes,   "delete" },
	{ transaction::correction_type::replaces,  "replace" },
}};

db::outcome db::resolve_corrections() {
	static const char* update_corrections_sql = R"sql(
UPDATE transactions
SET superseded_by = (
	SELECT corrections.id
	FROM transactions AS corrections
	WHERE corrections.corrects_fitid = transactions.fitid
	AND corrections.account_id = transactions.account_id
)
WHERE fitid IS NOT NULL
AND superseded_by IS NULL;

UPDATE transactions
SET corrects_id = (
	SELECT originals.id
	FROM transactions AS originals
	WHERE originals.fitid = transactions.corrects_fitid
	AND originals.account_id = transactions.account_id
)
WHERE corrects_fitid IS NOT NULL
AND corrects_id IS NULL;
)sql";
	int rc = sqlite3_exec(connection, update_corrections_sql, nullptr, nullptr, nullptr);
	if (SQLITE_OK != rc) {
		return sqlite_runtime_error(rc);
	}
	return success();
}

db::result<transaction> db::fetch_transaction(int64_t id) {
	return sql_fetch_one<fundos::transaction>(
		prepared->named.find_transaction_by_id.statement,
		[&](sqlite3_stmt* stmt) -> void {
			sqlite3_bind_int64(stmt, 1, id);
		},
		[&](sqlite3_stmt* stmt) -> fundos::transaction {
			fundos::transaction out;
			out.id_            = id;
			out.account_id     =                                         sqlite3_column_int64  (stmt, 0);
			out.amount         =                                currency{sqlite3_column_int64  (stmt, 1)};
			out.date_cleared   =                   as_optional<datetime>(extract_optional_int64(stmt, 2));
			out.fitid          =                                         extract_optional_text (stmt, 3);
			out.corrects_fitid =                                         extract_optional_text (stmt, 4);
			out.correct_action = optional_string_to_enum(correction_map, extract_optional_text (stmt, 5));
			out.corrects_id    =                                         extract_optional_int64(stmt, 6);
			out.superseded_by  =                                         extract_optional_int64(stmt, 7);
			return out;
		}
	);
}

static inline bool safe_match(const transaction& lhs, const transaction& rhs) {
	return (
		   lhs.account_id     == rhs.account_id
		&& lhs.amount         == rhs.amount
		&& lhs.date_cleared   == rhs.date_cleared
		&& lhs.fitid          == rhs.fitid
		&& lhs.corrects_fitid == rhs.corrects_fitid
		&& lhs.correct_action == rhs.correct_action
		&& lhs.corrects_id    == rhs.corrects_id
		&& lhs.superseded_by  == rhs.superseded_by
	);
}

db::outcome db::prepare_import(import::pending_import& pending) {
	for (auto &account : pending.accounts) {
		auto account_query = sql_fetch_one<int64_t>(
			prepared->named.find_account_by_bank_id.statement,
			[&] (sqlite3_stmt* stmt) -> void {
				bind_text(stmt, 1, account.acct_id);
			},
			[&] (sqlite3_stmt* stmt) -> int64_t {
				return sqlite3_column_int64(stmt, 0);
			}
		);
		if (!account_query) {
			if (account_query.status().code == error::not_found) {
				return outcome(error::rejected, "Cannot import from an unrecognized bank account");
			}
			return account_query.status();
		}
		account.account_id = account_query.value();

		{ // Collect candidates
			auto candidate_query = sql_fetch_many<transaction>(
				prepared->named.find_transaction_candidates.statement,
				[&](sqlite3_stmt* stmt) {
					sqlite3_bind_int64(stmt, 1, account.account_id);
				},
				[&](sqlite3_stmt* stmt) -> transaction {
					transaction out;
					out.account_id    =          account.account_id;
					out.id_           =          sqlite3_column_int64(stmt, 0);
					out.amount        = currency{sqlite3_column_int64(stmt, 1)};
					out.date_recorded = datetime{sqlite3_column_int64(stmt, 2)};
					out.memo          =          extract_text        (stmt, 3);
					return out;
				}
			);
			if (!candidate_query) {
				if (candidate_query.status().code != error::not_found) {
					return candidate_query.status();
				}
			} else {
				account.candidates = std::move(candidate_query.value());
			}
		}

		// Worst case: Every transaction being imported has a matching fitid, reserve it now so references are stable
		account.candidates.reserve(account.candidates.size() + account.transactions.size());

		// Search for matching records for each imported transaction
		for (auto &txn : account.transactions) {
			if (!txn.importing.fitid || !txn.importing.date_cleared) {
				return outcome(error::bad_request, "Importer must set both fitid and date_cleared");
			}
			auto match = [&txn](const transaction* candidate) {
				txn.set_match(candidate);
				txn.saving.date_recorded = candidate->date_recorded;
				txn.saving.memo = candidate->memo;
			};
			auto fitid_query = sql_fetch_one<transaction>(
				prepared->named.find_transaction_by_fitid.statement,
				[&](sqlite3_stmt* stmt) {
					bind_text         (stmt, 1, *txn.importing.fitid);
					sqlite3_bind_int64(stmt, 2, account.account_id);
				},
				[&](sqlite3_stmt* stmt) -> transaction {
					transaction out;
					out.id_            =                                         sqlite3_column_int64  (stmt, 0);
					out.account_id     =                                         account.account_id;
					out.amount         =                               currency {sqlite3_column_int64  (stmt, 1)};
					out.date_recorded  =                               datetime {sqlite3_column_int64  (stmt, 2)};
					out.date_cleared   =                   as_optional<datetime>(extract_optional_int64(stmt, 3));
					out.memo           =                                         extract_text          (stmt, 4);
					out.fitid          =                                         extract_optional_text (stmt, 5);
					out.corrects_fitid =                                         extract_optional_text (stmt, 6);
					out.correct_action = optional_string_to_enum(correction_map, extract_optional_text (stmt, 7));
					out.corrects_id    =                                         extract_optional_int64(stmt, 8);
					out.superseded_by  =                                         extract_optional_int64(stmt, 9);
					return out;
				}
			);
			if (!fitid_query && fitid_query.status().code != error::not_found) { return fitid_query.status(); }
			txn.saving.date_recorded = txn.importing.date_recorded;
			txn.saving.memo = txn.importing.memo;
			if (!fitid_query && fitid_query.status().code == error::not_found) {
				auto view = account.unclaimed_candidates();
				for (auto* candidate : view) {
					if (candidate->amount == txn.importing.amount) {
						if ((candidate->date_recorded - *txn.importing.date_cleared).magnitude() < timedelta::days(7)) {
							match(candidate);
							break;
						}
					}
				}
			} else {
				auto &matched = account.candidates.emplace_back(std::move(fitid_query.value()));
				match(&matched);
			}
		}
	}

	return success();
}

db::outcome db::perform_import(import::pending_import& pending) {
	for (auto &account : pending.accounts) {
		auto account_query = sql_fetch_one<int64_t>(
			prepared->named.find_account_by_bank_id.statement,
			[&] (sqlite3_stmt* stmt) -> void {
				bind_text(stmt, 1, account.acct_id);
			},
			[&] (sqlite3_stmt* stmt) -> int64_t {
				return sqlite3_column_int64(stmt, 0);
			}
		);
		if (!account_query) {
			if (account_query.status().code == error::not_found) {
				return outcome(error::bad_request, "Imported acct_id not recognized");
			}
			return account_query.status();
		}
		if (account_query.value() != account.account_id) { return outcome(error::bad_request, "Imported account_id does not match database"); }

		for (auto &transaction : account.transactions) {
			if (!transaction.importing.date_cleared) { return outcome(error::bad_request, "Imported transaction does not report a date_cleared date"); }
			transaction.saving.id_ = 0;
			transaction.saving.account_id = account.account_id;
			transaction.saving.amount = transaction.importing.amount;
			transaction.saving.date_cleared = transaction.importing.date_cleared;
			transaction.saving.fitid = transaction.importing.fitid;
			transaction.saving.corrects_fitid = transaction.importing.corrects_fitid;
			transaction.saving.correct_action = transaction.importing.correct_action;

			if (transaction.get_match() != nullptr) {
				auto match = fetch_transaction(transaction.get_match()->id_);
				if (!match) {
					if (match.status().code == error::not_found) {
						return outcome(error::bad_request, "Matched transaction does not exist");
					}
					return match.status();
				}
				if (!safe_match(*transaction.get_match(), match.value())) { return outcome(error::bad_request, "Matched transaction has deviated from the database"); }

				transaction.saving.id_ = match.value().id_;
				if (match.value().account_id != account.account_id) { return outcome(error::bad_request, "Matched transaction belongs to a different account"); }
				if (match.value().fitid && match.value().fitid != transaction.importing.fitid) { return outcome(error::bad_request, "Matched transaction has a different fitid"); }
				if (match.value().fitid) {
					if (match.value().correct_action != transaction.importing.correct_action) { return outcome(error::bad_request, "Matched transaction has a different correct action"); }
					if (match.value().corrects_fitid != transaction.importing.corrects_fitid) { return outcome(error::bad_request, "Matched transaction corrects a different fitid"); }
				} else {
					if (match.value().correct_action || match.value().corrects_id || match.value().superseded_by) { return outcome(error::bad_request, "Matched transaction is a manual correction transaction"); }
				}
			}
		}
	}

	return sql_transaction([&](std::vector<std::function<void()>>& rollback) -> outcome {
		for (auto &account : pending.accounts) {
			outcome insert_ledgerbal = sql_execute(
				prepared->named.insert_import_ledger_balance.statement,
				[&](sqlite3_stmt* stmt) {
					sqlite3_bind_int64(stmt, 1, account.account_id);
					sqlite3_bind_int64(stmt, 2, account.balance.minor_units);
					sqlite3_bind_int64(stmt, 3, account.as_of.milliseconds_since_epoch);
				}
			);
			if (!insert_ledgerbal) { return insert_ledgerbal; }

			for (auto &importing : account.transactions) {
				transaction& saving = importing.saving;
				if (!saving.is_persisted()) {
					outcome insert_transaction = sql_execute(
						prepared->named.insert_transaction_import.statement,
						[&](sqlite3_stmt* stmt) {
							sqlite3_bind_int64 (stmt, 1, saving.account_id);
							sqlite3_bind_int64 (stmt, 2, saving.amount.minor_units);
							sqlite3_bind_int64 (stmt, 3, saving.date_recorded.milliseconds_since_epoch);
							bind_optional_int64(stmt, 4, as_optional_int64(saving.date_cleared));
							bind_text          (stmt, 5, saving.memo);
							bind_optional_text (stmt, 6, saving.fitid);
							bind_optional_text (stmt, 7, saving.corrects_fitid);
							bind_optional_text (stmt, 8, optional_enum_to_string<fundos::transaction::correction_type>(correction_map, saving.correct_action));
						}
					);
					if (!insert_transaction) { return insert_transaction; }
					saving.id_= sqlite3_last_insert_rowid(connection);
					rollback.push_back([&saving]() { saving.id_ = 0; });
				} else {
					outcome update_transaction = sql_execute(
						prepared->named.update_transaction_import.statement,
						[&](sqlite3_stmt* stmt) {
							sqlite3_bind_int64 (stmt, 1, saving.amount.minor_units);
							sqlite3_bind_int64 (stmt, 2, saving.date_recorded.milliseconds_since_epoch);
							bind_optional_int64(stmt, 3, as_optional_int64(saving.date_cleared));
							bind_text          (stmt, 4, saving.memo);
							bind_optional_text (stmt, 5, saving.fitid);
							bind_optional_text (stmt, 6, saving.corrects_fitid);
							bind_optional_text (stmt, 7, optional_enum_to_string<fundos::transaction::correction_type>(correction_map, saving.correct_action));
							sqlite3_bind_int64 (stmt, 8, saving.id_);
						}
					);
					if (!update_transaction) { return update_transaction; }
					if (sqlite3_changes(connection) != 1) {
						return outcome(error::not_found, "Cannot update transaction which does not exist");
					}
				}
			}
		}
		return resolve_corrections();
	});
}

db::outcome db::save_allocations(transaction& saving, std::vector<allocation>& allocations, std::vector<std::function<void()>>& rollback) {
	std::vector<int64_t> preserve_ids;

	currency sum = {0};
	std::unordered_set<int64_t> seen_funds;
	std::string allocation_finder_sql = R"sql(
		SELECT COUNT(*)
		FROM allocations
		WHERE transaction_id = ?
		AND id IN (
	)sql";
	std::string fund_finder_sql = R"sql(
		SELECT COUNT(*)
		FROM funds
		WHERE closed_at IS NULL
		AND id IN (
	)sql";

	for (auto& entry : allocations) {
		if (entry.transaction_id == 0) {
			auto old = entry.transaction_id;
			entry.transaction_id = saving.id();
			rollback.push_back([&entry, old]() {
				entry.transaction_id = old;
			});
		}
		if (entry.transaction_id != saving.id()) {
			return outcome(error::bad_request, "Allocation has non-matching transaction_id");
		}

		if (entry.fund_id == 0) {
			return outcome(error::bad_request, "Allocation must include a fund");
		}

		// Gather ids to preserve in upsert and build sql to validate allocations belong the correct transaction
		if (entry.is_persisted()) {
			allocation_finder_sql += preserve_ids.empty() ? "?" : ", ?";
			preserve_ids.push_back(entry.id_);
		}

		// Build sql to validate that funds exist and are not closed
		fund_finder_sql += seen_funds.empty() ? "?" : ", ?";

		if (seen_funds.contains(entry.fund_id)) {
			return outcome(error::bad_request, "Cannot have duplicate fund allocations in the same set");
		}
		seen_funds.insert(entry.fund_id);

		sum += entry.amount;
	}
	if (!allocations.empty() && sum != saving.amount) {
		return outcome(error::rejected, "Cannot partially allocate a transaction");
	}
	fund_finder_sql += ")";
	allocation_finder_sql += ")";

	// Make sure funds exist and are not closed
	outcome find_funds = sql_count_check(
		fund_finder_sql,
		allocations.size(),
		[&](sqlite3_stmt* stmt) {
			for (size_t i = 0; i < allocations.size(); ++i) {
				sqlite3_bind_int64(stmt, static_cast<int>(i) + 1, allocations[i].fund_id);
			}
		},
		"Attempted to allocate to one or more funds which do not exist or are closed"
	);
	if (!find_funds) { return find_funds; }

	// Make sure persisted allocations exist and do not belong to a different transaction
	if (!preserve_ids.empty()) {
		outcome existing_allocations = sql_count_check(
			allocation_finder_sql,
			preserve_ids.size(),
			[&](sqlite3_stmt* stmt) {
				sqlite3_bind_int64(stmt, 1, saving.id());
				for (size_t i = 0; i < preserve_ids.size(); ++i) {
					sqlite3_bind_int64(stmt, static_cast<int>(i) + 2, preserve_ids[i]);
				}
			},
			"Attempted to modify an allocation which does not exist"
		);
		if (!existing_allocations) { return existing_allocations; }
	}

	outcome delete_extra_allocations = sql_delete_except({
		.table = "allocations",
		.filter_column = "transaction_id",
		.filter_value = saving.id(),
		.preserve_ids = preserve_ids,
	});
	if (!delete_extra_allocations) { return delete_extra_allocations; }

	for (auto &entry : allocations) {
		if (!entry.is_persisted()) {
			outcome insert_allocation = sql_execute(prepared->named.insert_allocation.statement, [&](sqlite3_stmt* stmt) {
				sqlite3_bind_int64 (stmt, 1, saving.id());
				sqlite3_bind_int64 (stmt, 2, entry.fund_id);
				sqlite3_bind_int64 (stmt, 3, entry.amount.minor_units);
			});
			if (!insert_allocation) { return insert_allocation; }
			entry.id_ = sqlite3_last_insert_rowid(connection);
			auto* ptr = &entry; // We need a persistent reference that outlives the transaction call
			rollback.push_back([ptr]() { ptr->id_ = 0; });
		} else {
			outcome update_allocation = sql_execute(prepared->named.update_allocation.statement, [&](sqlite3_stmt* stmt) {
				sqlite3_bind_int64 (stmt, 1, entry.fund_id);
				sqlite3_bind_int64 (stmt, 2, entry.amount.minor_units);
				sqlite3_bind_int64 (stmt, 3, entry.id_);
			});
			if (!update_allocation) { return update_allocation; }
			if (sqlite3_changes(connection) != 1) {
				return outcome(error::internal, "Cannot update allocation which does not exist");
			}
		}
	}

	return success();
}

db::outcome db::create_transaction(transaction& saving, std::vector<allocation>& allocations) {
	if (saving.corrects_id.has_value() != saving.correct_action.has_value()) {
		return outcome(error::bad_request, "correct_action and corrects_id must be set together");
	}
	std::optional<result<fundos::transaction>> corrects;
	if (saving.corrects_id) {
		corrects = fetch_transaction(*saving.corrects_id);
		if (!*corrects) {
			if (corrects->status().code == error::not_found) {
				return outcome(error::bad_request, "Transaction corrects a record that doesn't exist");
			}
			return corrects->status();
		}
		if (corrects->value().account_id != saving.account_id) { return outcome(error::bad_request, "Transaction corrects a record from a different account"); }
		if (corrects->value().fitid.has_value()) { return outcome(error::rejected, "Manual transaction corrects a record that is reported by the bank"); }
		if (corrects->value().superseded_by.has_value()) { return outcome(error::rejected, "Transaction corrects a record that is already superseded"); }
	}

	return sql_transaction([&](std::vector<std::function<void()>>& rollback) -> outcome {
		outcome insert_transaction = sql_execute(
			prepared->named.insert_transaction_user.statement,
			[&](sqlite3_stmt* stmt) {
				sqlite3_bind_int64 (stmt, 1, saving.account_id);
				sqlite3_bind_int64 (stmt, 2, saving.amount.minor_units);
				sqlite3_bind_int64 (stmt, 3, saving.date_recorded.milliseconds_since_epoch);
				bind_text          (stmt, 4, saving.memo);
				bind_optional_int64(stmt, 5, as_optional_int64(saving.date_reconciled));
				bind_optional_int64(stmt, 6, saving.corrects_id);
				bind_optional_text (stmt, 7, optional_enum_to_string<fundos::transaction::correction_type>(correction_map, saving.correct_action));
			}
		);
		if (!insert_transaction) { return insert_transaction; }
		saving.id_= sqlite3_last_insert_rowid(connection);
		rollback.push_back([&saving]() { saving.id_ = 0; });

		if (corrects) {
			outcome update = sql_execute(
				prepared->named.update_transaction_correction.statement,
				[&](sqlite3_stmt* stmt) {
					sqlite3_bind_int64(stmt, 1, saving.id_);
					sqlite3_bind_int64(stmt, 2, corrects->value().id_);
				}
			);
			if (!update) { return update; }
			if (sqlite3_changes(connection) != 1) {
				return outcome(error::not_found, "Cannot correct a transaction which does not exist or is already superseded");
			}
		}

		return save_allocations(saving, allocations, rollback);
	});
}

db::outcome db::update_transaction(transaction& saving, std::vector<allocation>& allocations) {
	auto existing = fetch_transaction(saving.id_);
	if (!existing) {
		if (existing.status().code == error::not_found) {
			return outcome(error::bad_request, "Cannot update a transaction that no longer exists");
		}
		return existing.status();
	}
	if (!safe_match(saving, existing.value())) { return outcome(error::rejected, "End user cannot manually alter protected fields on a transaction"); }

	return sql_transaction([&](std::vector<std::function<void()>>& rollback) -> outcome {
		outcome update = sql_execute(
			prepared->named.update_transaction_user.statement,
			[&](sqlite3_stmt* stmt) {
				sqlite3_bind_int64 (stmt, 1, saving.date_recorded.milliseconds_since_epoch);
				bind_text          (stmt, 2, saving.memo);
				bind_optional_int64(stmt, 3, as_optional_int64(saving.date_reconciled));
				sqlite3_bind_int64 (stmt, 4, saving.id_);
			}
		);
		if (!update) { return update; }
		if (sqlite3_changes(connection) != 1) {
			return outcome(error::not_found, "Cannot update transaction which does not exist");
		}

		return save_allocations(saving, allocations, rollback);
	});
}

db::outcome db::save_transaction(transaction& saving, std::vector<allocation>& allocations) {
	if (!saving.is_persisted()) {
		return create_transaction(saving, allocations);
	} else {
		return update_transaction(saving, allocations);
	}
}

db::result<db::transaction_history> db::account_history(int64_t account_id, datetime after, datetime before) {
	using transaction = transaction_history::allocated_transaction;
	auto fetched_transactions = sql_fetch_many<transaction>(
		prepared->named.filter_transactions.statement,
		[&](sqlite3_stmt* stmt) -> void {
			sqlite3_bind_int64(stmt, 1, account_id);
			sqlite3_bind_int64(stmt, 2, after.milliseconds_since_epoch);
			sqlite3_bind_int64(stmt, 3, before.milliseconds_since_epoch);
		},
		[&](sqlite3_stmt* stmt) -> transaction {
			transaction out;
			out.record.account_id = account_id;
			out.record.id_             =                                         sqlite3_column_int64  (stmt, 0);
			out.record.amount          =                               currency {sqlite3_column_int64  (stmt, 1)};
			out.record.date_recorded   =                               datetime {sqlite3_column_int64  (stmt, 2)};
			out.record.memo            =                                         extract_text          (stmt, 3);
			out.record.date_reconciled =                   as_optional<datetime>(extract_optional_int64(stmt, 4));
			out.record.fitid           =                                         extract_optional_text (stmt, 5);
			out.record.date_cleared    =                   as_optional<datetime>(extract_optional_int64(stmt, 6));
			out.record.corrects_fitid  =                                         extract_optional_text (stmt, 7);
			out.record.correct_action  = optional_string_to_enum(correction_map, extract_optional_text (stmt, 8));
			out.record.corrects_id     =                                         extract_optional_int64(stmt, 9);
			out.record.superseded_by   =                                         extract_optional_int64(stmt, 10);
			// is_pending can be derived from !record.date_cleared && !record.date_reconciled          (stmt, 11)
			out.effective_date         =                               datetime {sqlite3_column_int64  (stmt, 12)};
			out.account_balance        =                               currency {sqlite3_column_int64  (stmt, 13)};
			return out;
		}
	);
	if (!fetched_transactions) { return fetched_transactions.status(); }
	for (auto &fetched_transaction : fetched_transactions.value()) {
		auto fetched_allocations = sql_fetch_many<allocation>(
			prepared->named.get_transaction_allocations.statement,
			[&](sqlite3_stmt* stmt) -> void {
				sqlite3_bind_int64(stmt, 1, fetched_transaction.record.id_);
			},
			[&](sqlite3_stmt* stmt) -> allocation {
				allocation out;
				out.transaction_id = fetched_transaction.record.id_;
				out.id_     =          sqlite3_column_int64(stmt, 0);
				out.fund_id =          sqlite3_column_int64(stmt, 1);
				out.amount  = currency{sqlite3_column_int64(stmt, 2)};
				return out;
			}
		);
		if (!fetched_allocations) { return fetched_allocations.status(); }
		fetched_transaction.allocations = std::move(fetched_allocations.value());
	}

	auto fetched_ledgers = sql_fetch_many<import_ledger_balance>(
		prepared->named.filter_ledger_balances.statement,
		[&](sqlite3_stmt* stmt) -> void {
			sqlite3_bind_int64(stmt, 1, account_id);
			sqlite3_bind_int64(stmt, 2, after.milliseconds_since_epoch);
			sqlite3_bind_int64(stmt, 3, before.milliseconds_since_epoch);
		},
		[&](sqlite3_stmt* stmt) -> import_ledger_balance {
			import_ledger_balance out;
			out.account_id = account_id;
			out.id_        =          sqlite3_column_int64(stmt, 0);
			out.amount     = currency{sqlite3_column_int64(stmt, 1)};
			out.date_as_of = datetime{sqlite3_column_int64(stmt, 2)};
			return out;
		}
	);
	if (!fetched_ledgers) { return fetched_ledgers.status(); }

	return transaction_history{
		.transactions = std::move(fetched_transactions.value()),
		.ledger_balances = std::move(fetched_ledgers.value()),
	};
}

db::result<db::allocation_history> db::fund_history(int64_t fund_id, datetime after, datetime before) {
	using transaction = allocation_history::allocated_transaction;
	auto history = sql_fetch_many<transaction>(
		prepared->named.filter_allocations.statement,
		[&](sqlite3_stmt* stmt) -> void {
			sqlite3_bind_int64(stmt, 1, fund_id);
			sqlite3_bind_int64(stmt, 2, after.milliseconds_since_epoch);
			sqlite3_bind_int64(stmt, 3, before.milliseconds_since_epoch);
		},
		[&](sqlite3_stmt* stmt) -> transaction {
			transaction out;
			out.allocated.fund_id      = fund_id;
			out.record.id_             =                                         sqlite3_column_int64  (stmt, 0);
			out.record.account_id      =                                         sqlite3_column_int64  (stmt, 1);
			out.record.amount          =                               currency {sqlite3_column_int64  (stmt, 2)};
			out.record.date_recorded   =                               datetime {sqlite3_column_int64  (stmt, 3)};
			out.record.memo            =                                         extract_text          (stmt, 4);
			out.record.date_reconciled =                   as_optional<datetime>(extract_optional_int64(stmt, 5));
			out.record.fitid           =                                         extract_optional_text (stmt, 6);
			out.record.date_cleared    =                   as_optional<datetime>(extract_optional_int64(stmt, 7));
			out.record.corrects_fitid  =                                         extract_optional_text (stmt, 8);
			out.record.correct_action  = optional_string_to_enum(correction_map, extract_optional_text (stmt, 9));
			out.record.corrects_id     =                                         extract_optional_int64(stmt, 10);
			out.record.superseded_by   =                                         extract_optional_int64(stmt, 11);
			out.allocated.id_          =                                         sqlite3_column_int64  (stmt, 12);
			out.allocated.amount       =                               currency {sqlite3_column_int64  (stmt, 13)};
			out.fund_balance           =                               currency {sqlite3_column_int64  (stmt, 14)};
			return out;
		}
	);
	if (!history) { return history.status(); }
	return allocation_history{
		.transactions = std::move(history.value())
	};
}

#pragma endregion

#pragma region Lifecycle

db::error db::classify_sqlite_open_error(int rc) {
	switch (rc) {
		case SQLITE_FULL:
			return error::disk_full;
		case SQLITE_NOMEM:
			return error::out_of_memory;
		case SQLITE_BUSY:
		case SQLITE_LOCKED:
			return error::unavailable;
		case SQLITE_READONLY:
			return error::readonly;
		case SQLITE_CANTOPEN:
			return error::inaccessible;
		default:
			FUNDOS_ASSERT(false, "unhandled sqlite3 result code"); // In production fall through to corrupted
			[[fallthrough]];
		case SQLITE_CORRUPT:
		case SQLITE_NOTADB:
		case SQLITE_IOERR:
			return error::corrupted;
	}
}

std::shared_ptr<db> db::open_file(const char* path) {
	sqlite3* connection;
	int rc = sqlite3_open(path, &connection);
	if (rc != SQLITE_OK) {
		auto msg = db::sqlite_error_message(connection);
		sqlite3_close(connection); // must still close even on failure
		return std::make_shared<db>(outcome(classify_sqlite_open_error(rc), msg));
	}
	sqlite3_busy_timeout(connection, 5000);
	return std::make_shared<db>(connection, owns_connection{});
}

std::shared_ptr<db> db::open_memory() {
	return open_file(":memory:");
}

db::outcome db::backup(const std::string& path) {
	if (!is_ready()) { return not_ready(); }

	sqlite3* destination;
	int rc = sqlite3_open(path.c_str(), &destination);
	if (rc != SQLITE_OK) {
		auto msg = sqlite_error_message(destination);
		sqlite3_close(destination); // must still close even on open failure
		return outcome(classify_sqlite_open_error(rc), msg);
	}

	sqlite3_backup* backup = sqlite3_backup_init(destination, "main", connection, "main");
	if (backup == nullptr) {
		rc = sqlite3_errcode(destination);
		auto msg = sqlite_error_message(destination);
		sqlite3_close(destination);
		return outcome(classify_sqlite_open_error(rc), msg);
	}

	auto classify = [this](int rc) -> error {
		switch (rc & 0xFF) {
			case SQLITE_FULL:
				return error::disk_full;
			case SQLITE_NOMEM:
				return error::out_of_memory;
			case SQLITE_BUSY:
			case SQLITE_LOCKED:
				return error::unavailable;
			case SQLITE_READONLY:
				return error::readonly;
			case SQLITE_CORRUPT: // Definitely an error on the source
				close();
				[[fallthrough]];
			case SQLITE_IOERR: // could be an error on source or destination
				return error::corrupted;
			default:
				FUNDOS_ASSERT(false, "unhandled sqlite3 backup result code");
				return error::internal;
		}
	};

	rc = sqlite3_backup_step(backup, -1); // -1 = copy all pages at once
	rc = sqlite3_backup_finish(backup); // must be called even if step fails
	if (SQLITE_OK != rc) {
		auto msg = sqlite_error_message(destination);
		sqlite3_close(destination);
		return outcome(classify(rc), msg);
	}

	sqlite3_close(destination);
	return outcome(error::none);
}

void db::prepare() {
	for (size_t i = 0; i < schema::num_prepared; ++i) {
		int rc = sqlite3_prepare_v3(
			connection,
			prepared->slots[i].sql,
			-1,                              // length, -1 = read to null terminator
			SQLITE_PREPARE_PERSISTENT,       // prepFlags hint: reused frequently, keep associated cache resources warm
			&(prepared->slots[i].statement), // out: stmt
			nullptr                          // out: tail pointer, unused
		);
		if (SQLITE_OK == rc) { continue; }

		auto msg = sqlite_error_message();
		close(); // prepare is only called during initialization; closing unconditionally is safe since there is no path for the caller to retry
		if (SQLITE_ERROR == rc) { // SQL referenced a table/column that doesn't exist — schema drift
			open_result.result = status::code::schema_error;
			open_result.schema_status = schema_state::schema_mismatch;
		} else {
			open_result.result = status::code::sqlite3_error;
			open_result.sqlite3_outcome = outcome(classify_sqlite_open_error(rc), msg);
		}
		return;
	}
}

db::outcome db::migrate() {
	if (!is_connected()) { return not_ready(); }
	switch (open_result.schema_status) {
		case schema_state::created:
		case schema_state::older_schema:
			break;
		default:
			return success();
	}
	for (; schema < schema::schema_latest_version; ++schema) {
		const char* migration = schema::schema_migrations[schema].data();
		int rc = sqlite3_exec(connection, migration, nullptr, nullptr, nullptr);
		if (SQLITE_OK == rc) { continue; }

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
			open_result.sqlite3_outcome = outcome(error::corrupted, "Database is in an inconsistent state during migration");
			close();
		}
		return outcome(out, sqlite_error_message());
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
		: open_result.sqlite3_outcome;
}

void db::open() {
	if (connection == nullptr) {
		open_result.result = status::code::null_db;
		delete prepared;
		prepared = nullptr;
		return;
	}

	int rc = sqlite3_exec(connection, "PRAGMA locking_mode = EXCLUSIVE;", nullptr, nullptr, nullptr); // Adds an extra assurance that the db isn't going to change while we have it open
	if (SQLITE_OK != rc) {
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_outcome = sqlite_open_error(rc);
		close();
		return;
	}

	auto journal_mode_callback = [](void* result, int count, char** values, char**) -> int {
		if (count > 0 && values[0]) {
			*static_cast<std::string*>(result) = values[0];
		}
		return 0;
	};
	rc = sqlite3_exec(connection, "PRAGMA journal_mode = WAL;", journal_mode_callback, &open_result.journal_mode, nullptr); // Protects against write failures on system crashes
	if (SQLITE_OK != rc) {
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_outcome = sqlite_open_error(rc);
		close();
		return;
	}

	rc = sqlite3_exec(connection, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr); // required for cascade delete
	if (SQLITE_OK != rc) {
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_outcome = sqlite_open_error(rc);
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
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_outcome = sqlite_open_error(rc);
		close();
		return;
	}

	if (schema_objects == 0) {
		open_result.schema_status = schema_state::created;
		outcome from_migration = migrate();
		switch (from_migration.code) {
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
		switch (rc) {
			case SQLITE_ERROR: // meta table exists but doesn't have key column or value column
			case SQLITE_ABORT: // meta table has application key but its value is null
				open_result.result = status::code::schema_error;
				open_result.schema_status = schema_state::app_mismatch;
				break;
			default:
				open_result.result = status::code::sqlite3_error;
				open_result.sqlite3_outcome = sqlite_open_error(rc);
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
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_outcome = sqlite_open_error(rc);
		close();
		return;
	}

	rc = sqlite3_step(schema_statement);
	if (rc == SQLITE_DONE) { // No row returned — meta table exists but schema_version key is missing
		sqlite3_finalize(schema_statement);
		open_result.result = status::code::schema_error;
		open_result.schema_status = schema_state::schema_mismatch;
		open_result.sqlite3_outcome = outcome(error::corrupted, "Database is missing schema identifier");
		close();
		return;
	}
	if (rc != SQLITE_ROW) { // Unexpected sql error
		sqlite3_finalize(schema_statement);
		open_result.result = status::code::sqlite3_error;
		open_result.sqlite3_outcome = sqlite_open_error(rc);
		close();
		return;
	}

	int64_t version = sqlite3_column_int64(schema_statement, 0);
	sqlite3_finalize(schema_statement);

	if (version <= 0) { // db was affected by 3rd party in unpredictable way
		open_result.result = status::code::schema_error;
		open_result.schema_status = schema_state::schema_mismatch;
		open_result.sqlite3_outcome = outcome(error::corrupted, "Database reports negative schema");
		close();
		return;
	}

	schema = version;
	if (schema < schema::schema_latest_version) {
		open_result.schema_status = schema_state::older_schema;
		open_result.result = status::code::needs_migration;
		return;
	} else if (schema > schema::schema_latest_version) {
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
	std::unique_lock lock(connection_mutex);
	if (connection == nullptr) { return; }  // prepared and connection have parity and are managed as one resource
	for (size_t i = 0; i < schema::num_prepared; ++i) {
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

void db::interrupt() {
	std::shared_lock lock(connection_mutex);
	sqlite3_interrupt(connection);
}

int64_t db::size_on_disk() {
	if (!is_connected()) { return 0; }
	auto extract_int = [](void* data, int, char** cols, char**) {
		*static_cast<int64_t*>(data) = std::atoll(cols[0]);
		return 0;
	};
	int64_t page_count = 0;
	int64_t page_size  = 0;
	sqlite3_exec(connection, "PRAGMA page_count", extract_int, &page_count, nullptr);
	sqlite3_exec(connection, "PRAGMA page_size",  extract_int, &page_size,  nullptr);
	return page_count * page_size;
}

db::db(outcome err) : connection(nullptr), managed(false), prepared(nullptr) {
	open_result.result = status::code::sqlite3_error;
	open_result.sqlite3_outcome = std::move(err);
}
db::db(sqlite3* c)                  : connection(c), managed(false), prepared(new schema::db_prepared_statements()) { open(); }
db::db(sqlite3* c, owns_connection) : connection(c), managed(true),  prepared(new schema::db_prepared_statements()) { open(); }
db::~db() { close(); }

#pragma endregion

} // namespace fundos
