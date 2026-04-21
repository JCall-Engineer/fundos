#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include "types/currency.hpp"
#include "types/percentage.hpp"

namespace fundos {

struct user {
	int64_t id;
	std::string name;
};

struct fund {
	int64_t id;
	std::string name;
	std::optional<std::string> closed_at;
};

struct account {
	int64_t id;
	std::string name;
	std::optional<std::string> closed_at;
	std::optional<std::string> bank_ref;
	std::optional<std::string> import_source;
};

struct budget {
	int64_t id;
	std::string name;
	int64_t overflow_fund;
};

enum class phase_kind : uint8_t {
	fixed,
	percentage,
};

struct budget_phase {
	int64_t id;
	int64_t budget_id;
	int32_t position;
	phase_kind kind;
};

template<CurrencyLocale Locale>
struct fixed_target {
	int64_t id;
	int64_t phase_id;
	int64_t fund_id;
	currency<Locale> amount;
	std::optional<currency<Locale>> cap;
	bool allow_overdraw;
};

template<CurrencyLocale Locale>
struct percentage_target {
	int64_t id;
	int64_t phase_id;
	int64_t fund_id;
	percentage amount;
	std::optional<currency<Locale>> cap;
	bool allow_overdraw;
};

template<CurrencyLocale Locale>
struct transaction {
	int64_t id;
	int64_t account_id;
	currency<Locale> amount;
	std::string date;
	std::string memo;
	std::optional<std::string> bank_ref;
	std::optional<std::string> import_source;
};

template<CurrencyLocale Locale>
struct allocation {
	int64_t id;
	int64_t transaction_id;
	int64_t fund_id;
	currency<Locale> amount;
};

}; // fundos
