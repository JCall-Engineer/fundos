#include <gtest/gtest.h>
#include <format>

#include "data/models.hpp"
using namespace fundos;

static inline void AssertAllocationsMatch(const std::vector<allocation>& actual, std::vector<std::pair<int64_t, currency>> expected) {
	ASSERT_EQ(actual.size(), expected.size());
	
	std::unordered_map<int64_t, currency> actual_map;
	for (const auto& alloc : actual) {
		actual_map[alloc.fund_id] = alloc.amount;
	}
	
	for (const auto& [fund_id, amount] : expected) {
		ASSERT_TRUE(actual_map.contains(fund_id));
		EXPECT_EQ(actual_map[fund_id].minor_units, amount.minor_units);
	}
}

constexpr int64_t FUND_GROCERIES = 1;
constexpr int64_t FUND_RENT = 2;
constexpr int64_t FUND_SAVINGS = 3;
constexpr int64_t FUND_LONGTERM_SPENDING = 4;
constexpr int64_t FUND_OVERFLOW = 99;

static const std::unordered_map<int64_t, currency> NO_BALANCE = {};

static inline budget SingleFixedPhase(bool allow_overdraw) {
	budget budget;
	budget.overflow_fund = FUND_OVERFLOW;
	budget_phase<fixed_target> phase;

	fixed_target target;
	target.fund_id = FUND_GROCERIES;
	target.amount = currency{50000}; // $500
	target.allow_overdraw = allow_overdraw;
	phase.targets.push_back(target);

	budget.phases.push_back(phase);
	return budget;
}

TEST(BudgetApply, SingleFixedPhase_NotEnough_NoOverdraw) {
	budget budget = SingleFixedPhase(false);
	transaction transaction;
	transaction.amount = currency{30000}; // $300
	auto allocations = budget.apply(transaction, NO_BALANCE);
	AssertAllocationsMatch(allocations, {
		{FUND_GROCERIES, currency{30000}}
	});
}

TEST(BudgetApply, SingleFixedPhase_NotEnough_WithOverdraw) {
	budget budget = SingleFixedPhase(true);
	transaction transaction;
	transaction.amount = currency{30000}; // $300
	auto allocations = budget.apply(transaction, NO_BALANCE);
	AssertAllocationsMatch(allocations, {
		{FUND_GROCERIES, currency{50000}},
		{FUND_OVERFLOW, currency{-20000}}
	});
}

TEST(BudgetApply, SingleFixedPhase_MoreThanEnough) {
	budget budget = SingleFixedPhase(false);
	transaction transaction;
	transaction.amount = currency{60000}; // $600
	auto allocations = budget.apply(transaction, NO_BALANCE);
	AssertAllocationsMatch(allocations, {
		{FUND_GROCERIES, currency{50000}},
		{FUND_OVERFLOW, currency{10000}}
	});
}

TEST(BudgetApply, SinglePercentagePhase_Under100) {
	budget budget;
	budget.overflow_fund = FUND_OVERFLOW;
	budget_phase<percentage_target> phase;

	percentage_target savings_target;
	savings_target.fund_id = FUND_SAVINGS;
	savings_target.amount = percentage{2500}; // 25%
	savings_target.allow_overdraw = false;
	phase.targets.push_back(savings_target);

	percentage_target lts_target;
	lts_target.fund_id = FUND_LONGTERM_SPENDING;
	lts_target.amount = percentage{3333}; // 33.33%
	lts_target.allow_overdraw = false;
	phase.targets.push_back(lts_target);

	budget.phases.push_back(phase);
	transaction transaction;
	transaction.amount = currency{10000}; // $100
	auto allocations = budget.apply(transaction, NO_BALANCE);
	AssertAllocationsMatch(allocations, {
		{FUND_SAVINGS, currency{2500}},
		{FUND_LONGTERM_SPENDING, currency{3333}},
		{FUND_OVERFLOW, currency{4167}}
	});
}

TEST(BudgetApply, SinglePercentagePhase_Over100_NoOverdraw) {
	budget budget;
	budget.overflow_fund = FUND_OVERFLOW;
	budget_phase<percentage_target> phase;

	percentage_target savings_target;
	savings_target.fund_id = FUND_SAVINGS;
	savings_target.amount = percentage{7500}; // 75%
	savings_target.allow_overdraw = true;
	phase.targets.push_back(savings_target);

	percentage_target lts_target;
	lts_target.fund_id = FUND_LONGTERM_SPENDING;
	lts_target.amount = percentage{3333}; // 33.33%
	lts_target.allow_overdraw = false;
	phase.targets.push_back(lts_target);

	budget.phases.push_back(phase);
	transaction transaction;
	transaction.amount = currency{10000}; // $100
	auto allocations = budget.apply(transaction, NO_BALANCE);
	AssertAllocationsMatch(allocations, {
		{FUND_SAVINGS, currency{7500}},
		{FUND_LONGTERM_SPENDING, currency{2500}}
	});
}

TEST(BudgetApply, SinglePercentagePhase_Over100_WithOverdraw) {
	budget budget;
	budget.overflow_fund = FUND_OVERFLOW;
	budget_phase<percentage_target> phase;

	percentage_target lts_target;
	lts_target.fund_id = FUND_LONGTERM_SPENDING;
	lts_target.amount = percentage{3333}; // 33.33%
	lts_target.allow_overdraw = false;
	phase.targets.push_back(lts_target);

	percentage_target savings_target;
	savings_target.fund_id = FUND_SAVINGS;
	savings_target.amount = percentage{7500}; // 75%
	savings_target.allow_overdraw = true;
	phase.targets.push_back(savings_target);

	budget.phases.push_back(phase);
	transaction transaction;
	transaction.amount = currency{10000}; // $100
	auto allocations = budget.apply(transaction, NO_BALANCE);
	AssertAllocationsMatch(allocations, {
		{FUND_SAVINGS, currency{7500}},
		{FUND_LONGTERM_SPENDING, currency{3333}},
		{FUND_OVERFLOW, currency{-833}}
	});
}

budget MultiPhase() {
	budget budget;
	budget.overflow_fund = FUND_OVERFLOW;

	{
		budget_phase<fixed_target> first_phase;

		fixed_target groceries_target;
		groceries_target.fund_id = FUND_GROCERIES;
		groceries_target.amount = currency{50000}; // $500
		groceries_target.cap = currency{100000}; // $1k
		groceries_target.allow_overdraw = true;
		first_phase.targets.push_back(groceries_target);

		budget.phases.push_back(first_phase);
	}

	{
		budget_phase<percentage_target> second_phase;

		percentage_target savings_target;
		savings_target.fund_id = FUND_SAVINGS;
		savings_target.amount = percentage{3333}; // 33.33%
		savings_target.allow_overdraw = false;
		second_phase.targets.push_back(savings_target);

		percentage_target lts_target;
		lts_target.fund_id = FUND_LONGTERM_SPENDING;
		lts_target.amount = percentage{2500}; // 25%
		lts_target.cap = currency{1000000}; // $10k
		lts_target.allow_overdraw = false;
		second_phase.targets.push_back(lts_target);

		budget.phases.push_back(second_phase);
	}

	{
		budget_phase<fixed_target> third_phase;

		fixed_target rent_target;
		rent_target.fund_id = FUND_RENT;
		rent_target.amount = currency{150000}; // $1500
		rent_target.cap = currency{300000}; // $3k
		rent_target.allow_overdraw = true;
		third_phase.targets.push_back(rent_target);

		budget.phases.push_back(third_phase);
	}

	return budget;
}

TEST(BudgetApply, MultiPhase_Underallocated) {
	budget budget = MultiPhase();
	transaction transaction;
	transaction.amount = currency{200000}; // $2k
	auto allocations = budget.apply(transaction, NO_BALANCE);
	AssertAllocationsMatch(allocations, {
		{FUND_GROCERIES, currency{50000}},
		{FUND_SAVINGS, currency{49995}},
		{FUND_LONGTERM_SPENDING, currency{37500}},
		{FUND_RENT, currency{150000}},
		{FUND_OVERFLOW, currency{-87495}}
	});
}

TEST(BudgetApply, MultiPhase_Overallocated) {
	budget budget = MultiPhase();
	transaction transaction;
	transaction.amount = currency{500000}; // $5k
	auto allocations = budget.apply(transaction, NO_BALANCE);
	AssertAllocationsMatch(allocations, {
		{FUND_GROCERIES, currency{50000}},
		{FUND_SAVINGS, currency{149985}},
		{FUND_LONGTERM_SPENDING, currency{112500}},
		{FUND_RENT, currency{150000}},
		{FUND_OVERFLOW, currency{37515}}
	});
}

static const std::unordered_map<int64_t, currency> NEAR_CAP_BALANCE = {
	{FUND_GROCERIES, currency{75000}},
	{FUND_LONGTERM_SPENDING, currency{950000}},
	{FUND_RENT, currency{190000}},
};

TEST(BudgetApply, MultiPhase_OverCaps) {
	budget budget = MultiPhase();
	transaction transaction;
	transaction.amount = currency{500000}; // $5k
	auto allocations = budget.apply(transaction, NEAR_CAP_BALANCE);
	AssertAllocationsMatch(allocations, {
		{FUND_GROCERIES, currency{25000}},
		{FUND_SAVINGS, currency{158317}},
		{FUND_LONGTERM_SPENDING, currency{50000}},
		{FUND_RENT, currency{110000}},
		{FUND_OVERFLOW, currency{156683}}
	});
}
