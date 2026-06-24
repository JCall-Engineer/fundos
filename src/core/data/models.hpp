#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <list>
#include <optional>
#include <string>
#include <unordered_set>
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

struct fund : db_managed {
	std::string name;
	std::optional<datetime> closed_at;
};

struct account : db_managed {
	std::string name;
	std::optional<datetime> closed_at;
	std::optional<std::string> bank_account_id;
};

struct transaction : db_managed {
	enum class correction_type {
		replaces,
		deletes,
	};

	int64_t account_id = 0;
	currency amount;
	datetime date_recorded;
	std::string memo;
	std::optional<datetime> date_reconciled;
	std::optional<std::string> fitid;
	std::optional<datetime> date_cleared;
	std::optional<std::string> corrects_fitid;
	std::optional<correction_type> correct_action;
	std::optional<int64_t> corrects_id;
	std::optional<int64_t> superseded_by;
};

struct import_ledger_balance : db_managed {
	int64_t account_id = 0;
	currency amount;
	datetime date_as_of;
};

namespace import {

/// A transaction parsed from an OFX file, staged for user review before committing.
struct imported_transaction {
	enum class memo_choice : uint8_t {
		prefer_existing,
		prefer_importing,
	};

	/// Controls which memo is committed; only meaningful when a match is set.
	memo_choice memo = memo_choice::prefer_existing;

	/// Populated by the importer
	/// @note importer must ensure fitid and cleared are populated
	transaction record;

	/// Returns true if match was found by fitid — the match is definitive and cannot be changed.
	bool is_definitive_match() const { return match_ != nullptr && match_->fitid == record.fitid; }

	/// Match suggestions are initialized by db::prepare_import, can be adjusted by the user if not definitive.
	bool set_match(const transaction* candidate) {
		if (!is_definitive_match()) {
			match_ = candidate;
			return true;
		}
		return false;
	}

	const transaction* get_match() const { return match_; }

private:
	const transaction* match_ = nullptr;
};

/// An account and its transactions as parsed from an OFX file or fetched from a bank API.
/// candidates is the pool of existing db transactions available for matching.
/// The importer creates each `imported_transaction`, populating only the `importing` field.
/// db::prepare_import resolves `account_id` from `acct_id` and populates `candidates`.
struct bank_account {
	int64_t account_id = 0;
	std::string acct_id;
	currency balance;
	datetime as_of;
	std::vector<imported_transaction> transactions;
	std::vector<transaction> candidates;

	/// Returns pointers to candidates valid for matching with the given imported transaction.
	/// A valid candidate is unclaimed, has the same amount, and is not a corrects_id record if the importing transaction has a correct_action.
	/// Pointers are valid for the lifetime of this bank_account.
	std::vector<const transaction*> valid_candidates_for(const imported_transaction& subject) const {
		std::unordered_set<const transaction*> claimed;
		for (const auto& imported : transactions) {
			if (imported.get_match() != nullptr && imported.get_match() != subject.get_match()) {
				claimed.insert(imported.get_match());
			}
		}
		std::vector<const transaction*> view;
		for (const auto& candidate : candidates) {
			if (claimed.contains(&candidate)) { continue; }
			if (candidate.fitid) { continue; } // Should be redundant with the previous statement but for safety
			if (candidate.amount != subject.record.amount) { continue; }
			if (subject.record.correct_action.has_value() && candidate.corrects_id.has_value()) { continue; }
			view.push_back(&candidate);
		}
		return view;
	}

	/// Surfaces if manual matching is possible
	bool has_any_candidates(const fundos::import::imported_transaction& txn) const {
		if (txn.is_definitive_match()) { return false; }
		return std::any_of(
			candidates.begin(),
			candidates.end(),
			[&txn](const fundos::transaction& candidate) {
				return !candidate.fitid && candidate.amount == txn.record.amount;
			}
		);
	}
};

/// The result of parsing an OFX file or using a bank API, staged for user review before committing.
struct pending_import {
	std::vector<bank_account> accounts;
};

} // namespace fundos::import

struct allocation : db_managed {
	int64_t transaction_id = 0;
	int64_t fund_id = 0;
	currency amount;
};

struct fixed_target : db_managed {
	int64_t fund_id = 0;
	currency amount;
	std::optional<currency> cap;
	bool allow_overdraw = false;
};

struct percentage_target : db_managed {
	int64_t fund_id = 0;
	percentage amount;
	std::optional<currency> cap;
	bool allow_overdraw = false;
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

#pragma region Const Accessors

	///-------------------------------------------------------------------------------------------------------------------+
	/// Finds the first target satisfying the predicate.                                                                  |
	/// @param on_target Called with (position, target*); return true to stop iteration early.                            |
	/// @return Pointer to the first target for which on_target returned true, or nullptr.                                |
	///-------------------------------------------------------------------------------------------------------------------+
	inline const TargetType* find_target(std::function<bool(int, const TargetType*)> on_target) const {
		int position = 0;
		for (const auto& target : targets) {
			if (on_target(position, &target)) { return &target; }
			++position;
		}
		return nullptr;
	}

	///-------------------------------------------------------------------------------------------------------------------+
	/// Visits every target in their defined order.                                                                       |
	/// @param on_target Called with (position, target*) for every target.                                                |
	///-------------------------------------------------------------------------------------------------------------------+
	inline void each_target(std::function<void(int, const TargetType*)> on_target) const {
		find_target([&](int position, const TargetType* target) {
			on_target(position, target);
			return false;
		});
	}

#pragma endregion
#pragma region NonConst Accessors

	///-------------------------------------------------------------------------------------------------------------------+
	/// Finds the first target satisfying the predicate.                                                                  |
	/// @param on_target Called with (position, target*); return true to stop iteration early.                            |
	/// @return Pointer to the first target for which on_target returned true, or nullptr.                                |
	///-------------------------------------------------------------------------------------------------------------------+
	inline TargetType* find_target(std::function<bool(int, TargetType*)> on_target) {
		const auto* self = this;
		const TargetType* result = self->find_target([&](int position, const TargetType* target) {
			return on_target(position, const_cast<TargetType*>(target));
		});
		return const_cast<TargetType*>(result);
	}

	///-------------------------------------------------------------------------------------------------------------------+
	/// Visits every target in their defined order.                                                                       |
	/// @param on_target Called with (position, target*) for every target.                                                |
	///-------------------------------------------------------------------------------------------------------------------+
	inline void each_target(std::function<void(int, TargetType*)> on_target) {
		const auto* self = this;
		self->each_target([&](int position, const TargetType* target) {
			on_target(position, const_cast<TargetType*>(target));
		});
	}

#pragma endregion

	///-------------------------------------------------------------------------------------------------------------------+
	/// Moves target to immediately before before_target in the defined order.                                            |
	/// @param target The target to move; must belong to this phase.                                                      |
	/// @param before_target The target to insert before, or nullptr for end; must belong to this phase if non-null.      |
	///-------------------------------------------------------------------------------------------------------------------+
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
	int64_t overflow_fund = 0;
	std::list<any_budget_phase> phases;

#pragma region Const Accessors

	///-------------------------------------------------------------------------------------------------------------------+
	/// Iterates phases in their defined order, passing each as a const any_budget_phase* to on_phase.                    |
	/// @param on_phase Called with (position, phase*); return true to stop iteration early.                              |
	/// @return Pointer to the first phase for which on_phase returned true, or nullptr.                                  |
	///-------------------------------------------------------------------------------------------------------------------+
	inline const any_budget_phase* find_phase(std::function<bool(int, const any_budget_phase*)> on_phase) const {
		int position = 0;
		for (const auto& phase : phases) {
			if (on_phase(position, &phase)) { return &phase; }
			++position;
		}
		return nullptr;
	}

	///-------------------------------------------------------------------------------------------------------------------+
	/// Iterates phases in their defined order, dispatching to on_fixed or on_percentage based on each phase's type.      |
	/// @param on_fixed Called with (position, phase*) for fixed phases; return true to stop iteration early.             |
	/// @param on_percentage Called with (position, phase*) for percentage phases; return true to stop iteration early.   |
	/// @return Pointer to the first phase for which either visitor returned true, or nullptr.                            |
	///-------------------------------------------------------------------------------------------------------------------+
	inline const any_budget_phase* find_phase(
		std::function<bool(int, const budget_phase<fixed_target>*)> on_fixed,
		std::function<bool(int, const budget_phase<percentage_target>*)> on_percentage
	) const {
		return find_phase([&](int position, const any_budget_phase* phase) -> bool {
			bool stop = false;
			std::visit([&](const auto& typed_phase) {
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

	///-------------------------------------------------------------------------------------------------------------------+
	/// Visits every phase in their defined order, passing each as a const any_budget_phase* to on_phase.                 |
	/// @param on_phase Called with (position, phase*) for every phase.                                                   |
	///-------------------------------------------------------------------------------------------------------------------+
	inline void each_phase(std::function<void(int, const any_budget_phase*)> on_phase) const {
		find_phase([&](int position, const any_budget_phase* phase) {
			on_phase(position, phase);
			return false;
		});
	}

	///-------------------------------------------------------------------------------------------------------------------+
	/// Visits every phase in their defined order, dispatching to on_fixed or on_percentage based on each phase's type.   |
	/// @param on_fixed Called with (position, phase*) for every fixed phase.                                             |
	/// @param on_percentage Called with (position, phase*) for every percentage phase.                                   |
	///-------------------------------------------------------------------------------------------------------------------+
	inline void each_phase(
		std::function<void(int, const budget_phase<fixed_target>*)> on_fixed,
		std::function<void(int, const budget_phase<percentage_target>*)> on_percentage
	) const {
		find_phase([&](int position, const budget_phase<fixed_target>* phase) {
			on_fixed(position, phase);
			return false;
		}, [&](int position, const budget_phase<percentage_target>* phase) {
			on_percentage(position, phase);
			return false;
		});
	}

#pragma endregion
#pragma region NonConst Accessors

	///-------------------------------------------------------------------------------------------------------------------+
	/// Iterates phases in their defined order, passing each as an any_budget_phase* to on_phase.                         |
	/// @param on_phase Called with (position, phase*); return true to stop iteration early.                              |
	/// @return Pointer to the first phase for which on_phase returned true, or nullptr.                                  |
	///-------------------------------------------------------------------------------------------------------------------+
	inline any_budget_phase* find_phase(std::function<bool(int, any_budget_phase*)> on_phase) {
		const budget* self = this;
		const any_budget_phase* result = self->find_phase([&](int position, const any_budget_phase* phase) {
			return on_phase(position, const_cast<any_budget_phase*>(phase));
		});
		return const_cast<any_budget_phase*>(result);
	}

	///-------------------------------------------------------------------------------------------------------------------+
	/// Iterates phases in their defined order, dispatching to on_fixed or on_percentage based on each phase's type.      |
	/// @param on_fixed Called with (position, phase*) for fixed phases; return true to stop iteration early.             |
	/// @param on_percentage Called with (position, phase*) for percentage phases; return true to stop iteration early.   |
	/// @return Pointer to the first phase for which either visitor returned true, or nullptr.                            |
	///-------------------------------------------------------------------------------------------------------------------+
	inline any_budget_phase* find_phase(
		std::function<bool(int, budget_phase<fixed_target>*)> on_fixed,
		std::function<bool(int, budget_phase<percentage_target>*)> on_percentage
	) {
		const budget* self = this;
		const any_budget_phase* result = self->find_phase(
			[&](int position, const budget_phase<fixed_target>* phase) {
				return on_fixed(position, const_cast<budget_phase<fixed_target>*>(phase));
			},
			[&](int position, const budget_phase<percentage_target>* phase) {
				return on_percentage(position, const_cast<budget_phase<percentage_target>*>(phase));
			}
		);
		return const_cast<any_budget_phase*>(result);
	}

	///-------------------------------------------------------------------------------------------------------------------+
	/// Visits every phase in their defined order, passing each as an any_budget_phase* to on_phase.                      |
	/// @param on_phase Called with (position, phase*) for every phase.                                                   |
	///-------------------------------------------------------------------------------------------------------------------+
	inline void each_phase(std::function<void(int, any_budget_phase*)> on_phase) {
		const budget* self = this;
		self->each_phase([&](int position, const any_budget_phase* phase) {
			on_phase(position, const_cast<any_budget_phase*>(phase));
		});
	}

	///-------------------------------------------------------------------------------------------------------------------+
	/// Visits every phase in their defined order, dispatching to on_fixed or on_percentage based on each phase's type.   |
	/// @param on_fixed Called with (position, phase*) for every fixed phase.                                             |
	/// @param on_percentage Called with (position, phase*) for every percentage phase.                                   |
	///-------------------------------------------------------------------------------------------------------------------+
	inline void each_phase(
		std::function<void(int, budget_phase<fixed_target>*)> on_fixed,
		std::function<void(int, budget_phase<percentage_target>*)> on_percentage
	) {
		const budget* self = this;
		self->each_phase(
			[&](int position, const budget_phase<fixed_target>* phase) {
				on_fixed(position, const_cast<budget_phase<fixed_target>*>(phase));
			},
			[&](int position, const budget_phase<percentage_target>* phase) {
				on_percentage(position, const_cast<budget_phase<percentage_target>*>(phase));
			}
		);
	}

#pragma endregion

	///-------------------------------------------------------------------------------------------------------------------+
	/// Moves phase to immediately before before_phase in the defined order.                                              |
	/// Pass nullptr for before_phase to move phase to the end.                                                           |
	/// @param phase The phase to move; must belong to this budget.                                                       |
	/// @param before_phase The phase to insert before, or nullptr for end; must belong to this budget if non-null.       |
	///-------------------------------------------------------------------------------------------------------------------+
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

	///-------------------------------------------------------------------------------------------------------------------+
	/// This is **THE** FundOS function. The heart of it all. The magic.                                                  |
	/// Distributes an income transaction amount across funds by processing phases in their defined order.                |
	/// - fixed phases claim a set amount from the remainder.                                                             |
	/// - percentage phases claim a percentage of the remainder at the point they run.                                    |
	/// Any unclaimed remainder (or negative remainder) goes to the overflow fund.                                        |
	/// @param transaction The transaction to allocate; must already be persisted.                                        |
	/// @param current_balances Current balance per fund_id, used to enforce caps.                                        |
	/// @return One allocation per fund that received a nonzero amount.                                                   |
	///-------------------------------------------------------------------------------------------------------------------+
	inline std::vector<allocation> apply(const transaction& transaction, const std::unordered_map<int64_t, currency>& current_balances) const {
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

		// phase ordering is user-defined; fixed and percentage phases may be interleaved
		each_phase([&](int, const budget_phase<fixed_target>* phase) -> void {
			phase->each_target([&](int, const fixed_target* target) -> void {
				allocate(target, target->amount);
			});
		}, [&](int, const budget_phase<percentage_target>* phase) -> void {
			currency phase_balance = remainder.minor_units < 0 ? currency{0} : remainder;
			phase->each_target([&](int, const percentage_target* target) -> void {
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

} // namespace fundos
