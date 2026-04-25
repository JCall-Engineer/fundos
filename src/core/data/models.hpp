#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include "types/currency.hpp"
#include "types/percentage.hpp"

namespace fundos {

class db;
struct db_managed {
	friend class db;
private:
	int64_t id_ = 0;
public:
	int64_t id() const { return id_; }
	bool is_persisted() const { return id_ != 0; }
};

struct user : db_managed {
	std::string name;
};

struct fund : db_managed {
	std::string name;
	std::optional<std::string> closed_at;
};

struct account : db_managed {
	std::string name;
	std::optional<std::string> closed_at;
	std::optional<std::string> bank_ref;
	std::optional<std::string> import_source;
};

struct budget : db_managed {
	std::string name;
	int64_t overflow_fund;
};

enum class phase_kind : uint8_t {
	fixed,
	percentage,
};

struct budget_phase : db_managed {
	int64_t budget_id;
	int32_t position;
	phase_kind kind;
};

template<CurrencyLocale Locale>
struct fixed_target : db_managed {
	int64_t phase_id;
	int64_t fund_id;
	currency<Locale> amount;
	std::optional<currency<Locale>> cap;
	bool allow_overdraw;
};

template<CurrencyLocale Locale>
struct percentage_target : db_managed {
	int64_t phase_id;
	int64_t fund_id;
	percentage amount;
	std::optional<currency<Locale>> cap;
	bool allow_overdraw;
};

template<CurrencyLocale Locale>
struct transaction : db_managed {
	int64_t account_id;
	currency<Locale> amount;
	std::string date;
	std::string memo;
	std::optional<std::string> bank_ref;
	std::optional<std::string> import_source;
};

template<CurrencyLocale Locale>
struct allocation : db_managed {
	int64_t transaction_id;
	int64_t fund_id;
	currency<Locale> amount;
};

}; // fundos
