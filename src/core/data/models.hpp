#pragma once
#include <cstdint>
#include <functional>
#include <list>
#include <optional>
#include <string>
#include <variant>
#include "platform.hpp"
#include "types/datetime.hpp"
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
	std::optional<std::string> bank_account_id;
};

struct transaction : db_managed {
	enum class correction_type {
		replaces,
		deletes,
	};

	int64_t account_id;
	currency amount;
	datetime date;
	std::string memo;
	std::optional<std::string> fitid;
	std::optional<std::string> corrects_fitid;
	std::optional<correction_type> correct_action;
	std::optional<int64_t> corrects_id;
	std::optional<int64_t> superseded_by;
};

struct allocation : db_managed {
	int64_t transaction_id;
	int64_t fund_id;
	currency amount;
};

struct fixed_target : db_managed {
	int64_t fund_id;
	currency amount;
	std::optional<currency> cap;
	bool allow_overdraw;
};

struct percentage_target : db_managed {
	int64_t fund_id;
	percentage amount;
	std::optional<currency> cap;
	bool allow_overdraw;
};

template<typename T>
concept PhaseTarget = std::is_same_v<T, fixed_target> || std::is_same_v<T, percentage_target>;

enum class phase_kind : uint8_t {
	fixed,
	percentage,
};

template<PhaseTarget TargetType>
struct budget_phase : db_managed {
	std::list<TargetType> targets;

	phase_kind kind() const {
		if constexpr (std::is_same_v<TargetType, fixed_target>)      { return phase_kind::fixed; }
		if constexpr (std::is_same_v<TargetType, percentage_target>) { return phase_kind::percentage; }
		FUNDOS_UNREACHABLE();
	}

	inline TargetType* find(std::function<bool(int, TargetType*)> on_target) {
		int position = 0;
		for (auto& target : targets) {
			if (on_target(position, &target)) { return &target; }
			++position;
		}
		return nullptr;
	}

	inline void each(std::function<void(int, TargetType*)> on_target) {
		find([&](int position, TargetType* target) {
			on_target(position, target);
			return false;
		});
	}

	inline void reorder_target(TargetType* target, TargetType* before) {
		auto target_it = std::find_if(targets.begin(), targets.end(),
			[target](const TargetType& element) { return &element == target; });

		if (target_it == targets.end()) {
			FUNDOS_ASSERT(false, "Attempted to reorder element not in list");
			return;
		}

		auto before_it = before == nullptr
			? targets.end()
			: std::find_if(targets.begin(), targets.end(),
				[before](const TargetType& element) { return &element == before; });

		if (before != nullptr && before_it == targets.end()) {
			FUNDOS_ASSERT(false, "Attempted to reorder element before other not in list");
			return;
		}

		targets.splice(before_it, targets, target_it);
	}
};

using any_budget_phase = std::variant<
	budget_phase<fixed_target>,
	budget_phase<percentage_target>
>;

struct budget : db_managed {
	std::string name;
	int64_t overflow_fund;
	std::list<any_budget_phase> phases;

	inline any_budget_phase* find(std::function<bool(int, any_budget_phase*)> on_phase) {
		int position = 0;
		for (auto& phase : phases) {
			if (on_phase(position, &phase)) { return &phase; }
			++position;
		}
		return nullptr;
	}
	inline any_budget_phase* find(
		std::function<bool(int, budget_phase<fixed_target>*)> on_fixed,
		std::function<bool(int, budget_phase<percentage_target>*)> on_percentage
	) {
		return find([&](int position, any_budget_phase* phase) -> bool {
			bool stop = false;
			std::visit([&](auto& typed_phase) {
				using T = std::decay_t<decltype(typed_phase)>;
				if constexpr (std::is_same_v<T, budget_phase<fixed_target>>) {
					stop = on_fixed(position, &typed_phase);
				} else if constexpr (std::is_same_v<T, budget_phase<percentage_target>>) {
					stop = on_percentage(position, &typed_phase);
				} else {
					FUNDOS_UNREACHABLE();
				}
			}, *phase);
			return stop;
		});
	}

	inline void each(std::function<void(int, any_budget_phase*)> on_phase) {
		find([&](int position, any_budget_phase* phase) {
			on_phase(position, phase);
			return false;
		});
	}
	inline void each(
		std::function<void(int, budget_phase<fixed_target>*)> on_fixed,
		std::function<void(int, budget_phase<percentage_target>*)> on_percentage
	) {
		find([&](int position, budget_phase<fixed_target>* phase) {
			on_fixed(position, phase);
			return false;
		}, [&](int position, budget_phase<percentage_target>* phase) {
			on_percentage(position, phase);
			return false;
		});
	}

	inline void reorder_phase(any_budget_phase* phase, any_budget_phase* before) {
		auto phase_it = std::find_if(phases.begin(), phases.end(),
			[phase](const any_budget_phase& element) { return &element == phase; });

		if (phase_it == phases.end()) {
			FUNDOS_ASSERT(false, "Attempted to reorder element not in list");
			return;
		}

		auto before_it = before == nullptr
			? phases.end()
			: std::find_if(phases.begin(), phases.end(),
				[before](const any_budget_phase& element) { return &element == before; });

		if (before != nullptr && before_it == phases.end()) {
			FUNDOS_ASSERT(false, "Attempted to reorder element before other not in list");
			return;
		}

		phases.splice(before_it, phases, phase_it);
	}

	//--------------------------------------------------------------------------------------+
	// This is **THE** FundOS function. The heart of it all. The magic.                     |
	// I would like to mark this const, but I don't because that require me to duplicate    |
	// find/each either here or in a const version of their respective functions            |
	// apply() is still guaranteed to not modify the budget                                 |
	//--------------------------------------------------------------------------------------+
	inline std::vector<allocation> apply(const transaction& transaction, const std::unordered_map<int64_t, currency>& current_balances) {
		// unordered_map operator[] default constructs currency{} on non-existent keys
		std::unordered_map<int64_t, currency> allocations;
		currency remainder = transaction.amount;
		auto allocate = [&](const auto& target, currency allocated) -> void {
			if (target->cap) {
				currency current = current_balances.contains(target->fund_id)
					? current_balances.at(target->fund_id)
					: currency{0};
				current += allocations.contains(target->fund_id)
					? allocations.at(target->fund_id)
					: currency{0};

				currency room = std::max(currency{0}, *target->cap - current);
				allocated = std::min(allocated, room);
			}

			if (remainder >= allocated || target->allow_overdraw) {
				allocations[target->fund_id] += allocated;
				remainder -= allocated;
			} else if (remainder.minor_units > 0) {
				allocations[target->fund_id] += remainder;
				remainder.minor_units = 0;
			}
		};

		each([&](int, budget_phase<fixed_target>* phase) -> void {
			phase->each([&](int, fixed_target* target) -> void {
				allocate(target, target->amount);
			});
		}, [&](int, budget_phase<percentage_target>* phase) -> void {
			currency phase_balance = remainder.minor_units < 0 ? currency{0} : remainder;
			phase->each([&](int, percentage_target* target) -> void {
				allocate(target, target->amount.scale(phase_balance));
			});
		});

		if (remainder.minor_units != 0) {
			allocations[overflow_fund] += remainder;
		}

		std::vector<allocation> result;
		result.reserve(allocations.size());
		for (const auto& [fund_id, amount] : allocations) {
			allocation allocation;
			allocation.transaction_id = transaction.id();
			allocation.fund_id = fund_id;
			allocation.amount = amount;
			result.push_back(allocation);
		}
		return result;
	}
};

}; // fundos
