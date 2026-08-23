#include <gtest/gtest.h>
#include <chrono>
#include <format>
#include <string>
#include <string_view>

#include "data/db.hpp"
using namespace fundos;

sqlite3* mockDb() {
	sqlite3* connection;
	sqlite3_open(":memory:", &connection);
	return connection;
}

TEST(DbOpen, EmptyOrCurrentDb) {
	auto connection = mockDb();
	{
		auto database = std::make_shared<db>(connection);
		auto& status = database->get_status();
		EXPECT_EQ(database->is_ready(), true);
		EXPECT_EQ(database->is_connected(), true);
		EXPECT_EQ(status.result, db::status::code::ok);
		EXPECT_EQ(status.schema_status, db::schema_state::created);
		EXPECT_TRUE(static_cast<bool>(status.sqlite3_outcome));
		EXPECT_EQ(status.is_ok(), true);
		EXPECT_EQ(status.has_error(), false);
		EXPECT_EQ(status.needs_migration(), false);
	} // database destroyed here, connection should still exist

	int current_schema = 0;
	int rc = sqlite3_exec(connection, R"sql(
		SELECT value FROM meta WHERE key='schema_version'
	)sql", [](void* data, int, char** cols, char**) {
		*static_cast<int*>(data) = std::atoi(cols[0]);
		return 0;
	}, &current_schema, nullptr);
	EXPECT_EQ(rc, SQLITE_OK);
	EXPECT_EQ(current_schema, 1); // If this ever changes, write a test for older schema

	{
		auto database = std::make_shared<db>(connection, db::owns_connection{});
		auto& status = database->get_status();
		EXPECT_EQ(database->is_ready(), true);
		EXPECT_EQ(database->is_connected(), true);
		EXPECT_EQ(status.result, db::status::code::ok);
		EXPECT_EQ(status.schema_status, db::schema_state::current);
		EXPECT_TRUE(static_cast<bool>(status.sqlite3_outcome));
		EXPECT_EQ(status.is_ok(), true);
		EXPECT_EQ(status.has_error(), false);
		EXPECT_EQ(status.needs_migration(), false);
	}
}

TEST(DbOpen, NullDb) {
	sqlite3* connection = nullptr;
	auto database = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = database->get_status();
	EXPECT_EQ(database->is_ready(), false);
	EXPECT_EQ(database->is_connected(), false);
	EXPECT_EQ(status.result, db::status::code::null_db);
	EXPECT_EQ(status.schema_status, db::schema_state::none);
	EXPECT_TRUE(static_cast<bool>(status.sqlite3_outcome));
	EXPECT_EQ(status.is_ok(), false);
	EXPECT_EQ(status.has_error(), true);
	EXPECT_EQ(status.needs_migration(), false);
}

TEST(DbOpen, ForeignDbNoMeta) {
	auto connection = mockDb();
	int rc = sqlite3_exec(connection, R"sql(
		CREATE TABLE things(
			key   TEXT PRIMARY KEY,
			value TEXT NOT NULL
		);
	)sql", nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, SQLITE_OK);
	auto database = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = database->get_status();
	EXPECT_EQ(database->is_ready(), false);
	EXPECT_EQ(database->is_connected(), false);
	EXPECT_EQ(status.result, db::status::code::schema_error);
	EXPECT_EQ(status.schema_status, db::schema_state::app_mismatch);
	EXPECT_EQ(status.sqlite3_outcome.code, db::error::none);
	EXPECT_EQ(status.is_ok(), false);
	EXPECT_EQ(status.has_error(), true);
	EXPECT_EQ(status.needs_migration(), false);
}

TEST(DbOpen, ForeignDbBadMeta) {
	auto connection = mockDb();
	int rc = sqlite3_exec(connection, R"sql(
		CREATE TABLE meta(
			id  TEXT PRIMARY KEY,
			val TEXT NOT NULL
		);
	)sql", nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, SQLITE_OK);
	auto database = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = database->get_status();
	EXPECT_EQ(database->is_ready(), false);
	EXPECT_EQ(database->is_connected(), false);
	EXPECT_EQ(status.result, db::status::code::schema_error);
	EXPECT_EQ(status.schema_status, db::schema_state::app_mismatch);
	EXPECT_EQ(status.sqlite3_outcome.code, db::error::none);
	EXPECT_EQ(status.is_ok(), false);
	EXPECT_EQ(status.has_error(), true);
	EXPECT_EQ(status.needs_migration(), false);
}

TEST(DbOpen, ForeignDbNoApplication) {
	auto connection = mockDb();
	int rc = sqlite3_exec(connection, R"sql(
		CREATE TABLE meta(
			key   TEXT PRIMARY KEY,
			value TEXT NOT NULL
		);
	)sql", nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, SQLITE_OK);
	auto database = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = database->get_status();
	EXPECT_EQ(database->is_ready(), false);
	EXPECT_EQ(database->is_connected(), false);
	EXPECT_EQ(status.result, db::status::code::schema_error);
	EXPECT_EQ(status.schema_status, db::schema_state::app_mismatch);
	EXPECT_EQ(status.sqlite3_outcome.code, db::error::none);
	EXPECT_EQ(status.is_ok(), false);
	EXPECT_EQ(status.has_error(), true);
	EXPECT_EQ(status.needs_migration(), false);
}

TEST(DbOpen, ForeignDbWrongApplication) {
	auto connection = mockDb();
	int rc = sqlite3_exec(connection, R"sql(
		CREATE TABLE meta(
			key   TEXT PRIMARY KEY,
			value TEXT NOT NULL
		);
		INSERT INTO meta (key, value) VALUES ('application', 'other');
	)sql", nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, SQLITE_OK);
	auto database = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = database->get_status();
	EXPECT_EQ(database->is_ready(), false);
	EXPECT_EQ(database->is_connected(), false);
	EXPECT_EQ(status.result, db::status::code::schema_error);
	EXPECT_EQ(status.schema_status, db::schema_state::app_mismatch);
	EXPECT_EQ(status.sqlite3_outcome.code, db::error::none);
	EXPECT_EQ(status.is_ok(), false);
	EXPECT_EQ(status.has_error(), true);
	EXPECT_EQ(status.needs_migration(), false);
}

TEST(DbOpen, FundDbNoSchema) {
	auto connection = mockDb();
	int rc = sqlite3_exec(connection, R"sql(
		CREATE TABLE meta(
			key   TEXT PRIMARY KEY,
			value TEXT NOT NULL
		);
		INSERT INTO meta (key, value) VALUES ('application', 'fundos');
	)sql", nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, SQLITE_OK);
	auto database = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = database->get_status();
	EXPECT_EQ(database->is_ready(), false);
	EXPECT_EQ(database->is_connected(), false);
	EXPECT_EQ(status.result, db::status::code::schema_error);
	EXPECT_EQ(status.schema_status, db::schema_state::schema_mismatch);
	EXPECT_EQ(status.sqlite3_outcome.code, db::error::corrupted);
	EXPECT_EQ(status.is_ok(), false);
	EXPECT_EQ(status.has_error(), true);
	EXPECT_EQ(status.needs_migration(), false);
}

TEST(DbOpen, FundDbNanSchema) {
	auto connection = mockDb();
	int rc = sqlite3_exec(connection, R"sql(
		CREATE TABLE meta(
			key   TEXT PRIMARY KEY,
			value TEXT NOT NULL
		);
		INSERT INTO meta (key, value) VALUES ('application', 'fundos');
		INSERT INTO meta (key, value) VALUES ('schema_version', 'abc');
	)sql", nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, SQLITE_OK);
	auto database = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = database->get_status();
	EXPECT_EQ(database->is_ready(), false);
	EXPECT_EQ(database->is_connected(), false);
	EXPECT_EQ(status.result, db::status::code::schema_error);
	EXPECT_EQ(status.schema_status, db::schema_state::schema_mismatch);
	EXPECT_EQ(status.sqlite3_outcome.code, db::error::corrupted);
	EXPECT_EQ(status.is_ok(), false);
	EXPECT_EQ(status.has_error(), true);
	EXPECT_EQ(status.needs_migration(), false);
}

TEST(DbOpen, FundDbNegSchema) {
	auto connection = mockDb();
	int rc = sqlite3_exec(connection, R"sql(
		CREATE TABLE meta(
			key   TEXT PRIMARY KEY,
			value TEXT NOT NULL
		);
		INSERT INTO meta (key, value) VALUES ('application', 'fundos');
		INSERT INTO meta (key, value) VALUES ('schema_version', '-1');
	)sql", nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, SQLITE_OK);
	auto database = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = database->get_status();
	EXPECT_EQ(database->is_ready(), false);
	EXPECT_EQ(database->is_connected(), false);
	EXPECT_EQ(status.result, db::status::code::schema_error);
	EXPECT_EQ(status.schema_status, db::schema_state::schema_mismatch);
	EXPECT_EQ(status.sqlite3_outcome.code, db::error::corrupted);
	EXPECT_EQ(status.is_ok(), false);
	EXPECT_EQ(status.has_error(), true);
	EXPECT_EQ(status.needs_migration(), false);
}

TEST(DbOpen, FundDbSchema0) {
	auto connection = mockDb();
	int rc = sqlite3_exec(connection, R"sql(
		CREATE TABLE meta(
			key   TEXT PRIMARY KEY,
			value TEXT NOT NULL
		);
		INSERT INTO meta (key, value) VALUES ('application', 'fundos');
		INSERT INTO meta (key, value) VALUES ('schema_version', '0');
	)sql", nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, SQLITE_OK);
	auto database = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = database->get_status();
	EXPECT_EQ(database->is_ready(), false);
	EXPECT_EQ(database->is_connected(), false);
	EXPECT_EQ(status.result, db::status::code::schema_error);
	EXPECT_EQ(status.schema_status, db::schema_state::schema_mismatch);
	EXPECT_EQ(status.sqlite3_outcome.code, db::error::corrupted);
	EXPECT_EQ(status.is_ok(), false);
	EXPECT_EQ(status.has_error(), true);
	EXPECT_EQ(status.needs_migration(), false);
}

TEST(DbOpen, FundDbNewerSchema) {
	auto connection = mockDb();
	int rc = sqlite3_exec(connection, R"sql(
		CREATE TABLE meta(
			key   TEXT PRIMARY KEY,
			value TEXT NOT NULL
		);
		INSERT INTO meta (key, value) VALUES ('application', 'fundos');
		INSERT INTO meta (key, value) VALUES ('schema_version', '100');
	)sql", nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, SQLITE_OK);
	auto database = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = database->get_status();
	EXPECT_EQ(database->is_ready(), false);
	EXPECT_EQ(database->is_connected(), false);
	EXPECT_EQ(status.result, db::status::code::schema_error);
	EXPECT_EQ(status.schema_status, db::schema_state::newer_schema);
	EXPECT_EQ(status.sqlite3_outcome.code, db::error::none);
	EXPECT_EQ(status.is_ok(), false);
	EXPECT_EQ(status.has_error(), true);
	EXPECT_EQ(status.needs_migration(), false);
}

/// Previous iterations used a helper function instead of a macro but that loses the ability to assert in a test
#define FUNDOS_TEST_DB() \
	sqlite3* connection = mockDb(); \
	auto database = std::make_shared<db>(connection, db::owns_connection{}); \
	ASSERT_EQ(database->is_ready(), true);

static constexpr datetime date(const int& year, const std::chrono::month& month, const int& day) {
	return datetime{std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::sys_days{std::chrono::year{year} / month / day}
		.time_since_epoch()
	).count()};
}

static constexpr datetime CLOSED_AT = date(2024, std::chrono::June, 1);

TEST(DbQuery, ReadEntities) {
	FUNDOS_TEST_DB();
	sqlite3_exec(connection, std::format(R"sql(
		INSERT INTO funds (id, name, closed_at) VALUES (10, 'Emergency', NULL);
		INSERT INTO funds (id, name, closed_at) VALUES (11, 'Vacation', {});
		INSERT INTO accounts (id, name, closed_at, bank_account_id) VALUES (20, 'Checking', NULL, 'ref1');
		INSERT INTO accounts (id, name, closed_at, bank_account_id) VALUES (21, 'Savings', {}, NULL);
	)sql", CLOSED_AT.milliseconds_since_epoch, CLOSED_AT.milliseconds_since_epoch).c_str(), nullptr, nullptr, nullptr);

	{
		auto result = database->get_funds();
		ASSERT_TRUE(static_cast<bool>(result));
		ASSERT_EQ(result.value().size(), 2);
		auto emergency = result.value()[0];
		EXPECT_EQ(emergency.id(), 10);
		EXPECT_EQ(emergency.name, "Emergency");
		EXPECT_EQ(emergency.closed_at, std::nullopt);
		auto vacation = result.value()[1];
		EXPECT_EQ(vacation.id(), 11);
		EXPECT_EQ(vacation.name, "Vacation");
		EXPECT_EQ(vacation.closed_at, CLOSED_AT);
	}

	{
		auto result = database->get_accounts();
		ASSERT_TRUE(static_cast<bool>(result));
		ASSERT_EQ(result.value().size(), 2);
		auto checking = result.value()[0];
		EXPECT_EQ(checking.id(), 20);
		EXPECT_EQ(checking.name, "Checking");
		EXPECT_EQ(checking.closed_at, std::nullopt);
		EXPECT_EQ(checking.bank_account_id, "ref1");
		auto savings = result.value()[1];
		EXPECT_EQ(savings.id(), 21);
		EXPECT_EQ(savings.name, "Savings");
		EXPECT_EQ(savings.closed_at, CLOSED_AT);
		EXPECT_EQ(savings.bank_account_id, std::nullopt);
	}
}

TEST(DbQuery, SaveEntities) {
	FUNDOS_TEST_DB();

	fund emergency;
	emergency.name = "Emergency";
	ASSERT_TRUE(static_cast<bool>(database->save_fund(emergency)));
	EXPECT_NE(emergency.id(), 0);

	account checking;
	checking.name = "Checking";
	ASSERT_TRUE(static_cast<bool>(database->save_account(checking)));
	EXPECT_NE(checking.id(), 0);

	{
		auto result = database->get_funds();
		ASSERT_TRUE(static_cast<bool>(result));
		ASSERT_EQ(result.value().size(), 1);
		auto& row = result.value()[0];
		EXPECT_EQ(row.id(), emergency.id());
		EXPECT_EQ(row.name, emergency.name);
		EXPECT_EQ(row.closed_at, emergency.closed_at);
	}
	{
		auto result = database->get_accounts();
		ASSERT_TRUE(static_cast<bool>(result));
		ASSERT_EQ(result.value().size(), 1);
		auto& row = result.value()[0];
		EXPECT_EQ(row.id(), checking.id());
		EXPECT_EQ(row.name, checking.name);
		EXPECT_EQ(row.closed_at, checking.closed_at);
		EXPECT_EQ(row.bank_account_id, checking.bank_account_id);
	}

	emergency.name = "Rainy Day";
	emergency.closed_at = CLOSED_AT;
	ASSERT_TRUE(static_cast<bool>(database->save_fund(emergency)));
	{
		auto result = database->get_funds();
		ASSERT_TRUE(static_cast<bool>(result));
		ASSERT_EQ(result.value().size(), 1);
		auto& row = result.value()[0];
		EXPECT_EQ(row.id(), emergency.id());
		EXPECT_EQ(row.name, "Rainy Day");
		EXPECT_EQ(row.closed_at, CLOSED_AT);
	}

	checking.name = "Debit Card";
	checking.bank_account_id = "ref1";
	checking.closed_at = CLOSED_AT;
	ASSERT_TRUE(static_cast<bool>(database->save_account(checking)));
	{
		auto result = database->get_accounts();
		ASSERT_TRUE(static_cast<bool>(result));
		ASSERT_EQ(result.value().size(), 1);
		auto& row = result.value()[0];
		EXPECT_EQ(row.id(), checking.id());
		EXPECT_EQ(row.name, "Debit Card");
		EXPECT_EQ(row.bank_account_id, "ref1");
		EXPECT_EQ(row.closed_at, CLOSED_AT);
	}
}

TEST(DbQuery, MetaLocales) {
	FUNDOS_TEST_DB();
	{
		auto saved_currency   = database->get_currency_locale();
		auto saved_percentage = database->get_percentage_locale();

		ASSERT_FALSE(saved_currency);
		ASSERT_FALSE(saved_percentage);
		EXPECT_EQ(saved_currency.status().code, db::error::not_found);
		EXPECT_EQ(saved_percentage.status().code, db::error::not_found);
	}
	{
		auto currency_result   = database->set_currency_locale(&currency_locale::locales.named.USD);
		auto percentage_result = database->set_percentage_locale(&percentage_locale::locales.named.en);

		ASSERT_TRUE(static_cast<bool>(currency_result));
		ASSERT_TRUE(static_cast<bool>(percentage_result));

		auto saved_currency   = database->get_currency_locale();
		auto saved_percentage = database->get_percentage_locale();

		ASSERT_TRUE(static_cast<bool>(saved_currency));
		ASSERT_TRUE(static_cast<bool>(saved_percentage));

		auto c_locale = saved_currency.value().info();
		auto p_locale = saved_percentage.value().info();

		EXPECT_EQ(c_locale.scale,               currency_locale::locales.named.USD.info.scale);
		EXPECT_EQ(c_locale.symbol,              currency_locale::locales.named.USD.info.symbol);
		EXPECT_EQ(c_locale.thousands_separator, currency_locale::locales.named.USD.info.thousands_separator);
		EXPECT_EQ(c_locale.decimal_separator,   currency_locale::locales.named.USD.info.decimal_separator);
		EXPECT_EQ(c_locale.symbol_position,     currency_locale::locales.named.USD.info.symbol_position);
		EXPECT_EQ(c_locale.negative_format,     currency_locale::locales.named.USD.info.negative_format);

		EXPECT_EQ(p_locale.decimal_separator,       percentage_locale::locales.named.en.info.decimal_separator);
		EXPECT_EQ(p_locale.has_space_around_number, percentage_locale::locales.named.en.info.has_space_around_number);
		EXPECT_EQ(p_locale.symbol_position,         percentage_locale::locales.named.en.info.symbol_position);
	}
	{
		currency_locale::spec custom_currency = {
			.scale = 1000,
			.symbol = "L",
			.thousands_separator = ';',
			.decimal_separator = ':',
			.symbol_position = currency_locale::spec::symbol_placement::before,
			.negative_format = currency_locale::spec::negative_notation::leading_minus,
		};
		percentage_locale::spec custom_percentage = {
			.decimal_separator = ':',
			.has_space_around_number = true,
			.symbol_position = percentage_locale::spec::symbol_placement::before,
		};
		auto currency_result   = database->set_currency_locale(custom_currency);
		auto percentage_result = database->set_percentage_locale(custom_percentage);

		ASSERT_TRUE(static_cast<bool>(currency_result));
		ASSERT_TRUE(static_cast<bool>(percentage_result));

		auto saved_currency   = database->get_currency_locale();
		auto saved_percentage = database->get_percentage_locale();

		ASSERT_TRUE(static_cast<bool>(saved_currency));
		ASSERT_TRUE(static_cast<bool>(saved_percentage));

		auto c_locale = saved_currency.value().info();
		auto p_locale = saved_percentage.value().info();

		EXPECT_EQ(c_locale.scale,               custom_currency.scale);
		EXPECT_EQ(c_locale.symbol,              custom_currency.symbol);
		EXPECT_EQ(c_locale.thousands_separator, custom_currency.thousands_separator);
		EXPECT_EQ(c_locale.decimal_separator,   custom_currency.decimal_separator);
		EXPECT_EQ(c_locale.symbol_position,     custom_currency.symbol_position);
		EXPECT_EQ(c_locale.negative_format,     custom_currency.negative_format);

		EXPECT_EQ(p_locale.decimal_separator,       custom_percentage.decimal_separator);
		EXPECT_EQ(p_locale.has_space_around_number, custom_percentage.has_space_around_number);
		EXPECT_EQ(p_locale.symbol_position,         custom_percentage.symbol_position);
	}
}

// Helper: count rows in a table, optionally filtered
static int count_rows(sqlite3* connection, const char* query) {
	int count = 0;
	sqlite3_exec(connection, query, [](void* data, int, char** cols, char**) {
		*static_cast<int*>(data) = std::atoi(cols[0]);
		return 0;
	}, &count, nullptr);
	return count;
}

TEST(DbQuery, SaveBudget_ReadBack) {
	FUNDOS_TEST_DB();

	// Create funds via save_fund
	fund emergency_savings; emergency_savings.name = "Emergency Savings";
	fund food;              food.name              = "Food";
	fund investments;       investments.name       = "Investments";
	fund flex_spending;     flex_spending.name     = "Flex Spending";
	fund rent;              rent.name              = "Rent";
	ASSERT_TRUE(static_cast<bool>(database->save_fund(emergency_savings)));
	ASSERT_TRUE(static_cast<bool>(database->save_fund(food)));
	ASSERT_TRUE(static_cast<bool>(database->save_fund(investments)));
	ASSERT_TRUE(static_cast<bool>(database->save_fund(flex_spending)));
	ASSERT_TRUE(static_cast<bool>(database->save_fund(rent)));

	// Manually build initial budget state in SQL
	// budget: "Default", overflow -> emergency_savings
	// phase 0: percentage
	//   target 0: flex_spending, 2500 bp, cap 100000, no overdraw
	//   target 1: investments,   3000 bp, no cap,     no overdraw
	// phase 1: fixed
	//   target 0: food, 30000, cap 50000, allow overdraw
	//   target 1: rent, 100000, cap 250000, no overdraw
	sqlite3_exec(connection, R"sql(
		INSERT INTO budgets (id, name, overflow_fund) VALUES (1, 'Default', )sql"
		// Can't embed a variable in a raw string literal, so use exec with bind below
	, nullptr, nullptr, nullptr);

	// Use parameterized inserts for the budget row since overflow_fund is a variable id
	{
		sqlite3_stmt* stmt = nullptr;
		sqlite3_prepare_v2(connection,
			"INSERT INTO budgets (id, name, overflow_fund) VALUES (1, 'Default', ?)",
			-1, &stmt, nullptr);
		sqlite3_bind_int64(stmt, 1, emergency_savings.id());
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	sqlite3_exec(connection, R"sql(
		INSERT INTO budget_phases (id, budget_id, position, kind) VALUES (1, 1, 0, 'percentage');
		INSERT INTO budget_phases (id, budget_id, position, kind) VALUES (2, 1, 1, 'fixed');
	)sql", nullptr, nullptr, nullptr);
	{
		sqlite3_stmt* stmt = nullptr;
		sqlite3_prepare_v2(connection, R"sql(
			INSERT INTO phase_targets (id, phase_id, position, fund_id, amount, cap, allow_overdraw)
			VALUES
				(1, 1, 0, ?, 2500,   100000, 0),
				(2, 1, 1, ?, 3000,   NULL,   0),
				(3, 2, 0, ?, 30000,  50000,  1),
				(4, 2, 1, ?, 100000, 250000, 0)
		)sql", -1, &stmt, nullptr);
		sqlite3_bind_int64(stmt, 1, flex_spending.id());
		sqlite3_bind_int64(stmt, 2, investments.id());
		sqlite3_bind_int64(stmt, 3, food.id());
		sqlite3_bind_int64(stmt, 4, rent.id());
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	// Verify get_budgets round-trip before any save
	{
		auto result = database->get_budgets();
		ASSERT_TRUE(static_cast<bool>(result));
		ASSERT_EQ(result.value().size(), 1);

		auto& budget = result.value()[0];
		EXPECT_EQ(budget.id(),           1);
		EXPECT_EQ(budget.name,           "Default");
		EXPECT_EQ(budget.overflow_fund,  emergency_savings.id());
		ASSERT_EQ(budget.phases.size(),  2);

		auto phase_it = budget.phases.begin();

		// phase 0: percentage
		{
			auto& phase_variant = *phase_it++;
			auto* phase = std::get_if<budget_phase<percentage_target>>(&phase_variant);
			ASSERT_NE(phase, nullptr);
			EXPECT_EQ(phase->id(), 1);
			ASSERT_EQ(phase->targets.size(), 2);

			auto target_it = phase->targets.begin();
			{
				auto& target = *target_it++;
				EXPECT_EQ(target.id(),           1);
				EXPECT_EQ(target.fund_id,        flex_spending.id());
				EXPECT_EQ(target.amount.basis_points, 2500);
				ASSERT_TRUE(target.cap.has_value());
				EXPECT_EQ(target.cap->minor_units, 100000);
				EXPECT_EQ(target.allow_overdraw, false);
			}
			{
				auto& target = *target_it++;
				EXPECT_EQ(target.id(),           2);
				EXPECT_EQ(target.fund_id,        investments.id());
				EXPECT_EQ(target.amount.basis_points, 3000);
				EXPECT_FALSE(target.cap.has_value());
				EXPECT_EQ(target.allow_overdraw, false);
			}
		}

		// phase 1: fixed
		{
			auto& phase_variant = *phase_it++;
			auto* phase = std::get_if<budget_phase<fixed_target>>(&phase_variant);
			ASSERT_NE(phase, nullptr);
			EXPECT_EQ(phase->id(), 2);
			ASSERT_EQ(phase->targets.size(), 2);

			auto target_it = phase->targets.begin();
			{
				auto& target = *target_it++;
				EXPECT_EQ(target.id(),              3);
				EXPECT_EQ(target.fund_id,           food.id());
				EXPECT_EQ(target.amount.minor_units, 30000);
				ASSERT_TRUE(target.cap.has_value());
				EXPECT_EQ(target.cap->minor_units,  50000);
				EXPECT_EQ(target.allow_overdraw,    true);
			}
			{
				auto& target = *target_it++;
				EXPECT_EQ(target.id(),              4);
				EXPECT_EQ(target.fund_id,           rent.id());
				EXPECT_EQ(target.amount.minor_units, 100000);
				ASSERT_TRUE(target.cap.has_value());
				EXPECT_EQ(target.cap->minor_units,  250000);
				EXPECT_EQ(target.allow_overdraw,    false);
			}
		}
	}
}

TEST(DbQuery, SaveBudget_PartialUpdate) {
	FUNDOS_TEST_DB();

	fund emergency_savings; emergency_savings.name = "Emergency Savings";
	fund food;              food.name              = "Food";
	fund investments;       investments.name       = "Investments";
	fund flex_spending;     flex_spending.name     = "Flex Spending";
	fund rent;              rent.name              = "Rent";
	ASSERT_TRUE(static_cast<bool>(database->save_fund(emergency_savings)));
	ASSERT_TRUE(static_cast<bool>(database->save_fund(food)));
	ASSERT_TRUE(static_cast<bool>(database->save_fund(investments)));
	ASSERT_TRUE(static_cast<bool>(database->save_fund(flex_spending)));
	ASSERT_TRUE(static_cast<bool>(database->save_fund(rent)));

	{
		sqlite3_stmt* stmt = nullptr;
		sqlite3_prepare_v2(connection,
			"INSERT INTO budgets (id, name, overflow_fund) VALUES (1, 'Default', ?)",
			-1, &stmt, nullptr);
		sqlite3_bind_int64(stmt, 1, emergency_savings.id());
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	sqlite3_exec(connection, R"sql(
		INSERT INTO budget_phases (id, budget_id, position, kind) VALUES (1, 1, 0, 'percentage');
		INSERT INTO budget_phases (id, budget_id, position, kind) VALUES (2, 1, 1, 'fixed');
	)sql", nullptr, nullptr, nullptr);
	{
		sqlite3_stmt* stmt = nullptr;
		sqlite3_prepare_v2(connection, R"sql(
			INSERT INTO phase_targets (id, phase_id, position, fund_id, amount, cap, allow_overdraw)
			VALUES
				(1, 1, 0, ?, 2500,   100000, 0),
				(2, 1, 1, ?, 3000,   NULL,   0),
				(3, 2, 0, ?, 30000,  50000,  1),
				(4, 2, 1, ?, 100000, 250000, 0)
		)sql", -1, &stmt, nullptr);
		sqlite3_bind_int64(stmt, 1, flex_spending.id());
		sqlite3_bind_int64(stmt, 2, investments.id());
		sqlite3_bind_int64(stmt, 3, food.id());
		sqlite3_bind_int64(stmt, 4, rent.id());
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	auto get_result = database->get_budgets();
	ASSERT_TRUE(static_cast<bool>(get_result));
	ASSERT_EQ(get_result.value().size(), 1);
	budget default_budget = get_result.value()[0];

	int64_t original_budget_id  = default_budget.id();
	int64_t original_pct_phase_id = 0;
	int64_t original_flex_target_id = 0;

	// Drop the fixed phase entirely, replace with fresh one at same position
	{
		auto& phase_variant = *default_budget.phases.begin();
		auto* pct_phase = std::get_if<budget_phase<percentage_target>>(&phase_variant);
		ASSERT_NE(pct_phase, nullptr);
		original_pct_phase_id = pct_phase->id();

		// Drop investments target (position 1), keep flex (position 0)
		original_flex_target_id = pct_phase->targets.begin()->id();
		pct_phase->targets.erase(std::next(pct_phase->targets.begin()));

		percentage_target new_investments;
		new_investments.fund_id        = investments.id();
		new_investments.amount         = percentage{3500};
		new_investments.cap            = std::nullopt;
		new_investments.allow_overdraw = false;
		pct_phase->targets.push_back(new_investments);
	}

	default_budget.phases.erase(std::next(default_budget.phases.begin())); // drop fixed phase

	budget_phase<fixed_target> new_fixed_phase;

	fixed_target new_food_target;
	new_food_target.fund_id        = food.id();
	new_food_target.amount         = currency{35000};
	new_food_target.cap            = currency{55000};
	new_food_target.allow_overdraw = true;
	new_fixed_phase.targets.push_back(new_food_target);

	fixed_target new_rent_target;
	new_rent_target.fund_id        = rent.id();
	new_rent_target.amount         = currency{110000};
	new_rent_target.cap            = currency{260000};
	new_rent_target.allow_overdraw = false;
	new_fixed_phase.targets.push_back(new_rent_target);

	default_budget.phases.push_back(new_fixed_phase);

	ASSERT_TRUE(static_cast<bool>(database->save_budget(default_budget)));

	// Verify id stability for surviving rows
	EXPECT_EQ(default_budget.id(), original_budget_id);
	{
		auto phase_it = default_budget.phases.begin();
		auto* pct_phase = std::get_if<budget_phase<percentage_target>>(&*phase_it++);
		ASSERT_NE(pct_phase, nullptr);
		EXPECT_EQ(pct_phase->id(), original_pct_phase_id);
		EXPECT_EQ(pct_phase->targets.begin()->id(), original_flex_target_id);

		// New investments target: fresh non-zero id
		auto& new_investments = *std::next(pct_phase->targets.begin());
		EXPECT_NE(new_investments.id(), 0);

		auto* fixed_phase = std::get_if<budget_phase<fixed_target>>(&*phase_it++);
		ASSERT_NE(fixed_phase, nullptr);
		EXPECT_NE(fixed_phase->id(), 0);

		for (auto& target : fixed_phase->targets) {
			EXPECT_NE(target.id(), 0);
		}
	}

	// Direct SQL: old rows gone, counts correct
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM budgets"),       1);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM budget_phases"), 2);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM phase_targets"), 4);

	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM budget_phases WHERE position = 0 AND kind = 'percentage'"), 1);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM budget_phases WHERE position = 1 AND kind = 'fixed'"), 1);

	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM phase_targets WHERE amount = 2500  AND cap = 100000 AND allow_overdraw = 0"), 1);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM phase_targets WHERE amount = 3500  AND cap IS NULL  AND allow_overdraw = 0"), 1);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM phase_targets WHERE amount = 35000 AND cap = 55000  AND allow_overdraw = 1"), 1);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM phase_targets WHERE amount = 110000 AND cap = 260000 AND allow_overdraw = 0"), 1);

	// get_budgets round-trip
	{
		auto result = database->get_budgets();
		ASSERT_TRUE(static_cast<bool>(result));
		ASSERT_EQ(result.value().size(), 1);

		auto& budget = result.value()[0];
		EXPECT_EQ(budget.id(),          original_budget_id);
		EXPECT_EQ(budget.name,          "Default");
		EXPECT_EQ(budget.overflow_fund, emergency_savings.id());
		ASSERT_EQ(budget.phases.size(), 2);

		auto phase_it = budget.phases.begin();
		{
			auto* phase = std::get_if<budget_phase<percentage_target>>(&*phase_it++);
			ASSERT_NE(phase, nullptr);
			EXPECT_EQ(phase->id(), original_pct_phase_id);
			ASSERT_EQ(phase->targets.size(), 2);

			auto target_it = phase->targets.begin();
			EXPECT_EQ(target_it->id(),                   original_flex_target_id);
			EXPECT_EQ(target_it->fund_id,                flex_spending.id());
			EXPECT_EQ(target_it->amount.basis_points,    2500);
			ASSERT_TRUE(target_it->cap.has_value());
			EXPECT_EQ(target_it->cap->minor_units,       100000);
			EXPECT_EQ(target_it->allow_overdraw,         false);
			++target_it;
			EXPECT_EQ(target_it->fund_id,                investments.id());
			EXPECT_EQ(target_it->amount.basis_points,    3500);
			EXPECT_FALSE(target_it->cap.has_value());
			EXPECT_EQ(target_it->allow_overdraw,         false);
		}
		{
			auto* phase = std::get_if<budget_phase<fixed_target>>(&*phase_it++);
			ASSERT_NE(phase, nullptr);
			ASSERT_EQ(phase->targets.size(), 2);

			auto target_it = phase->targets.begin();
			EXPECT_EQ(target_it->fund_id,           food.id());
			EXPECT_EQ(target_it->amount.minor_units, 35000);
			ASSERT_TRUE(target_it->cap.has_value());
			EXPECT_EQ(target_it->cap->minor_units,  55000);
			EXPECT_EQ(target_it->allow_overdraw,    true);
			++target_it;
			EXPECT_EQ(target_it->fund_id,            rent.id());
			EXPECT_EQ(target_it->amount.minor_units, 110000);
			ASSERT_TRUE(target_it->cap.has_value());
			EXPECT_EQ(target_it->cap->minor_units,   260000);
			EXPECT_EQ(target_it->allow_overdraw,     false);
		}
	}
}

TEST(DbQuery, SaveBudget_RollbackOnError) {
	FUNDOS_TEST_DB();

	fund emergency_savings; emergency_savings.name = "Emergency Savings";
	fund flex_spending;     flex_spending.name     = "Flex Spending";
	fund investments;       investments.name       = "Investments";
	ASSERT_TRUE(static_cast<bool>(database->save_fund(emergency_savings)));
	ASSERT_TRUE(static_cast<bool>(database->save_fund(flex_spending)));
	ASSERT_TRUE(static_cast<bool>(database->save_fund(investments)));

	// Build a fresh budget with one percentage phase, three targets,
	// last target references a nonexistent fund to trigger FK violation
	budget bad_budget;
	bad_budget.name          = "Bad Budget";
	bad_budget.overflow_fund = emergency_savings.id();

	budget_phase<percentage_target> phase;

	percentage_target flex_target;
	flex_target.fund_id        = flex_spending.id();
	flex_target.amount         = percentage{2500};
	flex_target.cap            = currency{100000};
	flex_target.allow_overdraw = false;
	phase.targets.push_back(flex_target);

	percentage_target investments_target;
	investments_target.fund_id        = investments.id();
	investments_target.amount         = percentage{3000};
	investments_target.cap            = std::nullopt;
	investments_target.allow_overdraw = false;
	phase.targets.push_back(investments_target);

	percentage_target bad_target;
	bad_target.fund_id        = 99999; // nonexistent fund
	bad_target.amount         = percentage{500};
	bad_target.cap            = std::nullopt;
	bad_target.allow_overdraw = false;
	phase.targets.push_back(bad_target);

	bad_budget.phases.push_back(phase);

	// Capture pre-save ids (all zero) for rollback verification
	auto result = database->save_budget(bad_budget);
	EXPECT_FALSE(static_cast<bool>(result));

	// All ids must be rolled back to zero
	EXPECT_EQ(bad_budget.id(), 0);
	{
		auto& phase_variant = *bad_budget.phases.begin();
		auto* saved_phase = std::get_if<budget_phase<percentage_target>>(&phase_variant);
		ASSERT_NE(saved_phase, nullptr);
		EXPECT_EQ(saved_phase->id(), 0);

		auto target_it = saved_phase->targets.begin();
		EXPECT_EQ(target_it->id(), 0); ++target_it;
		EXPECT_EQ(target_it->id(), 0); ++target_it;
		EXPECT_EQ(target_it->id(), 0);
	}

	// No partial rows in any table
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM budgets"),       0);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM budget_phases"), 0);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM phase_targets"), 0);

	// Fix the bad target and verify save succeeds
	{
		auto& phase_variant = *bad_budget.phases.begin();
		auto* fixed_phase = std::get_if<budget_phase<percentage_target>>(&phase_variant);
		fixed_phase->targets.back().fund_id = investments.id();
	}

	ASSERT_TRUE(static_cast<bool>(database->save_budget(bad_budget)));
	EXPECT_NE(bad_budget.id(), 0);
	{
		auto& phase_variant = *bad_budget.phases.begin();
		auto* saved_phase = std::get_if<budget_phase<percentage_target>>(&phase_variant);
		ASSERT_NE(saved_phase, nullptr);
		EXPECT_NE(saved_phase->id(), 0);

		for (auto& target : saved_phase->targets) {
			EXPECT_NE(target.id(), 0);
		}
	}

	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM budgets"),       1);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM budget_phases"), 1);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM phase_targets"), 3);
}

TEST(DbQuery, SaveBudget_ReorderPreservedAcrossSave) {
	FUNDOS_TEST_DB();

	fund emergency_savings; emergency_savings.name = "Emergency Savings";
	fund fund_one;          fund_one.name          = "Fund One";
	fund fund_two;          fund_two.name          = "Fund Two";
	fund fund_three;        fund_three.name        = "Fund Three";
	fund fund_four;         fund_four.name         = "Fund Four";
	ASSERT_TRUE(static_cast<bool>(database->save_fund(emergency_savings)));
	ASSERT_TRUE(static_cast<bool>(database->save_fund(fund_one)));
	ASSERT_TRUE(static_cast<bool>(database->save_fund(fund_two)));
	ASSERT_TRUE(static_cast<bool>(database->save_fund(fund_three)));
	ASSERT_TRUE(static_cast<bool>(database->save_fund(fund_four)));

	budget new_budget;
	new_budget.name          = "Reorder Budget";
	new_budget.overflow_fund = emergency_savings.id();

	budget_phase<fixed_target> fixed_phase;

	fixed_target fixed_target_one;
	fixed_target_one.fund_id        = fund_one.id();
	fixed_target_one.amount         = currency{1};
	fixed_target_one.cap            = std::nullopt;
	fixed_target_one.allow_overdraw = false;
	fixed_phase.targets.push_back(fixed_target_one);

	fixed_target fixed_target_two;
	fixed_target_two.fund_id        = fund_two.id();
	fixed_target_two.amount         = currency{2};
	fixed_target_two.cap            = std::nullopt;
	fixed_target_two.allow_overdraw = false;
	fixed_phase.targets.push_back(fixed_target_two);

	budget_phase<percentage_target> percentage_phase;

	percentage_target percentage_target_one;
	percentage_target_one.fund_id        = fund_three.id();
	percentage_target_one.amount         = percentage{1};
	percentage_target_one.cap            = std::nullopt;
	percentage_target_one.allow_overdraw = false;
	percentage_phase.targets.push_back(percentage_target_one);

	percentage_target percentage_target_two;
	percentage_target_two.fund_id        = fund_four.id();
	percentage_target_two.amount         = percentage{2};
	percentage_target_two.cap            = std::nullopt;
	percentage_target_two.allow_overdraw = false;
	percentage_phase.targets.push_back(percentage_target_two);

	new_budget.phases.push_back(fixed_phase);
	new_budget.phases.push_back(percentage_phase);

	ASSERT_TRUE(static_cast<bool>(database->save_budget(new_budget)));

	any_budget_phase* fixed_phase_pointer      = nullptr;
	any_budget_phase* percentage_phase_pointer = nullptr;

	new_budget.each_phase([&](int, any_budget_phase* phase) {
		std::visit([&](auto& typed_phase) {
			using PhaseType = std::decay_t<decltype(typed_phase)>;
			if constexpr (std::is_same_v<PhaseType, budget_phase<fixed_target>>) {
				fixed_phase_pointer = phase;
			} else if constexpr (std::is_same_v<PhaseType, budget_phase<percentage_target>>) {
				percentage_phase_pointer = phase;
			}
		}, *phase);
	});

	ASSERT_NE(fixed_phase_pointer, nullptr);
	ASSERT_NE(percentage_phase_pointer, nullptr);

	new_budget.reorder_phase(fixed_phase_pointer, nullptr);

	new_budget.find_phase([&](int, budget_phase<fixed_target>* phase) {
		fixed_target* first_target  = &*phase->targets.begin();
		phase->reorder_target(first_target, nullptr);
		return true;
	}, [](int, budget_phase<percentage_target>*) {
		return false;
	});

	new_budget.find_phase([](int, budget_phase<fixed_target>*) {
		return false;
	}, [&](int, budget_phase<percentage_target>* phase) {
		percentage_target* first_target = &*phase->targets.begin();
		phase->reorder_target(first_target, nullptr);
		return true;
	});

	ASSERT_TRUE(static_cast<bool>(database->save_budget(new_budget)));

	auto result = database->get_budgets();
	ASSERT_TRUE(static_cast<bool>(result));
	ASSERT_EQ(result.value().size(), 1);

	budget& loaded_budget = result.value()[0];
	ASSERT_EQ(loaded_budget.phases.size(), 2);

	auto phase_iterator = loaded_budget.phases.begin();

	auto* loaded_percentage_phase = std::get_if<budget_phase<percentage_target>>(&*phase_iterator++);
	ASSERT_NE(loaded_percentage_phase, nullptr);
	ASSERT_EQ(loaded_percentage_phase->targets.size(), 2);
	{
		auto target_iterator = loaded_percentage_phase->targets.begin();
		EXPECT_EQ(target_iterator->amount.basis_points, 2); ++target_iterator;
		EXPECT_EQ(target_iterator->amount.basis_points, 1);
	}

	auto* loaded_fixed_phase = std::get_if<budget_phase<fixed_target>>(&*phase_iterator++);
	ASSERT_NE(loaded_fixed_phase, nullptr);
	ASSERT_EQ(loaded_fixed_phase->targets.size(), 2);
	{
		auto target_iterator = loaded_fixed_phase->targets.begin();
		EXPECT_EQ(target_iterator->amount.minor_units, 2); ++target_iterator;
		EXPECT_EQ(target_iterator->amount.minor_units, 1);
	}
}

TEST(DbQuery, AccountHistory_ClearedAndPending) {
	fundos::datetime
		  tx1_recorded = date(2026, std::chrono::January,   1) + timedelta::hours(11) + timedelta::minutes(12) + timedelta::seconds(13)
		, tx1_cleared  = date(2026, std::chrono::January,   4) + timedelta::hours(12) + timedelta::minutes(00) + timedelta::seconds(00)
		, tx2_recorded = date(2026, std::chrono::January,  15) + timedelta::hours(12) + timedelta::minutes(34) + timedelta::seconds(24)
		, tx3_recorded = date(2026, std::chrono::February,  1) + timedelta::hours(13) + timedelta::minutes(56) + timedelta::seconds(35)
		, tx3_cleared  = date(2026, std::chrono::February,  3) + timedelta::hours(12) + timedelta::minutes(00) + timedelta::seconds(00)
		, tx4_recorded = date(2026, std::chrono::February, 15) + timedelta::hours(14) + timedelta::minutes(18) + timedelta::seconds(46)
		, tx5_recorded = date(2026, std::chrono::March,     1) + timedelta::hours(15) + timedelta::minutes(29) + timedelta::seconds(57)
		, tx5_cleared  = date(2026, std::chrono::March,     3) + timedelta::hours(12) + timedelta::minutes(00) + timedelta::seconds(00)
		;

	FUNDOS_TEST_DB();
	int rc = sqlite3_exec(connection, std::format(R"sql(
		INSERT INTO accounts (id, name) VALUES (1, 'Checking');
		INSERT INTO transactions (id, account_id, amount, date_recorded, memo, fitid, date_cleared)
			VALUES
				(1, 1, 100, {}, 'First Transaction', 'fitid1', {}),
				(2, 1,  11, {}, 'Second Transaction', NULL,    NULL),
				(3, 1,  22, {}, 'Third Transaction', 'fitid3', {}),
				(4, 1,  33, {}, 'Fourth Transaction', NULL,    NULL),
				(5, 1,  44, {}, 'Fifth Transaction', 'fitid5', {});
	)sql",
		  tx1_recorded.milliseconds_since_epoch
		, tx1_cleared.milliseconds_since_epoch
		, tx2_recorded.milliseconds_since_epoch
		, tx3_recorded.milliseconds_since_epoch
		, tx3_cleared.milliseconds_since_epoch
		, tx4_recorded.milliseconds_since_epoch
		, tx5_recorded.milliseconds_since_epoch
		, tx5_cleared.milliseconds_since_epoch
	).c_str(), nullptr, nullptr, nullptr);
	ASSERT_EQ(rc, SQLITE_OK);

	auto history = database->account_history(1, date(2026, std::chrono::February, 1), date(2026, std::chrono::March, 1));
	ASSERT_TRUE(static_cast<bool>(history));

	ASSERT_EQ(history.value().ledger_balances.size(), 0);
	ASSERT_EQ(history.value().transactions.size(), 2);

	{ // transaction 4
		EXPECT_EQ(history.value().transactions[0].record.id(), 4);
		EXPECT_EQ(history.value().transactions[0].record.account_id, 1);
		EXPECT_EQ(history.value().transactions[0].record.date_recorded, tx4_recorded);
		EXPECT_EQ(history.value().transactions[0].record.memo, "Fourth Transaction");
		EXPECT_EQ(history.value().transactions[0].record.amount, currency{33});
		EXPECT_EQ(history.value().transactions[0].record.date_reconciled, std::nullopt);
		EXPECT_EQ(history.value().transactions[0].record.fitid, std::nullopt);
		EXPECT_EQ(history.value().transactions[0].record.date_cleared, std::nullopt);
		EXPECT_EQ(history.value().transactions[0].record.correct_action, std::nullopt);
		EXPECT_EQ(history.value().transactions[0].record.corrects_fitid, std::nullopt);
		EXPECT_EQ(history.value().transactions[0].record.corrects_id, std::nullopt);
		EXPECT_EQ(history.value().transactions[0].record.superseded_by, std::nullopt);
		EXPECT_EQ(history.value().transactions[0].account_balance, currency{210});
		EXPECT_EQ(history.value().transactions[0].effective_date, tx4_recorded);
		EXPECT_TRUE(history.value().transactions[0].allocations.empty());
	}

	{ // transaction 3
		EXPECT_EQ(history.value().transactions[1].record.id(), 3);
		EXPECT_EQ(history.value().transactions[1].record.account_id, 1);
		EXPECT_EQ(history.value().transactions[1].record.date_recorded, tx3_recorded);
		EXPECT_EQ(history.value().transactions[1].record.memo, "Third Transaction");
		EXPECT_EQ(history.value().transactions[1].record.amount, currency{22});
		EXPECT_EQ(history.value().transactions[1].record.date_reconciled, std::nullopt);
		EXPECT_EQ(history.value().transactions[1].record.fitid, "fitid3");
		EXPECT_EQ(history.value().transactions[1].record.date_cleared, tx3_cleared);
		EXPECT_EQ(history.value().transactions[1].record.correct_action, std::nullopt);
		EXPECT_EQ(history.value().transactions[1].record.corrects_fitid, std::nullopt);
		EXPECT_EQ(history.value().transactions[1].record.corrects_id, std::nullopt);
		EXPECT_EQ(history.value().transactions[1].record.superseded_by, std::nullopt);
		EXPECT_EQ(history.value().transactions[1].account_balance, currency{122});
		EXPECT_EQ(history.value().transactions[1].effective_date, tx3_cleared);
		EXPECT_TRUE(history.value().transactions[1].allocations.empty());
	}
}

TEST(DbQuery, FundHistory_Basic) {
	fundos::datetime
		  tx1_recorded = date(2026, std::chrono::January,  1) + timedelta::hours(12)
		, tx2_recorded = date(2026, std::chrono::February, 1) + timedelta::hours(12)
		, tx3_recorded = date(2026, std::chrono::March,    1) + timedelta::hours(12)
		, tx4_recorded = date(2026, std::chrono::March,   15) + timedelta::hours(12)
		, tx5_recorded = date(2026, std::chrono::March,   15) + timedelta::hours(13)
		;
	FUNDOS_TEST_DB();
	int rc = sqlite3_exec(connection, std::format(R"sql(
		INSERT INTO accounts (id, name) VALUES (1, 'Checking');
		INSERT INTO funds (id, name) VALUES (1, 'Groceries');
		INSERT INTO transactions (id, account_id, amount, date_recorded, memo)
			VALUES
				(1, 1, 100, {}, 'First Transaction'),
				(2, 1,  50, {}, 'Second Transaction'),
				(3, 1,  25, {}, 'Third Transaction'),
				(4, 1,  75, {}, 'Fourth Transaction - Superseded'),
				(5, 1,  99, {}, 'Fifth Transaction - Supersedes Fourth');
		UPDATE transactions SET superseded_by = 5 WHERE id = 4;
		UPDATE transactions SET corrects_id = 4, correct_action = 'replace' WHERE id = 5;
		INSERT INTO allocations (id, transaction_id, fund_id, amount)
			VALUES
				(1, 1, 1, 100),
				(2, 2, 1,  50),
				(3, 3, 1,  25),
				(4, 4, 1,  75),
				(5, 5, 1,  99);
	)sql",
		  tx1_recorded.milliseconds_since_epoch
		, tx2_recorded.milliseconds_since_epoch
		, tx3_recorded.milliseconds_since_epoch
		, tx4_recorded.milliseconds_since_epoch
		, tx5_recorded.milliseconds_since_epoch
	).c_str(), nullptr, nullptr, nullptr);
	ASSERT_EQ(rc, SQLITE_OK);

	auto history = database->fund_history(1, date(2026, std::chrono::February, 1), date(2026, std::chrono::March, 31));
	ASSERT_TRUE(static_cast<bool>(history));
	ASSERT_EQ(history.value().transactions.size(), 3);

	{ // transaction 4 superseded, transaction 5 replaces it
		EXPECT_EQ(history.value().transactions[0].record.id(), 5);
		EXPECT_EQ(history.value().transactions[0].record.account_id, 1);
		EXPECT_EQ(history.value().transactions[0].record.date_recorded, tx5_recorded);
		EXPECT_EQ(history.value().transactions[0].record.memo, "Fifth Transaction - Supersedes Fourth");
		EXPECT_EQ(history.value().transactions[0].record.amount, currency{99});
		EXPECT_EQ(history.value().transactions[0].record.date_reconciled, std::nullopt);
		EXPECT_EQ(history.value().transactions[0].record.fitid, std::nullopt);
		EXPECT_EQ(history.value().transactions[0].record.date_cleared, std::nullopt);
		EXPECT_EQ(history.value().transactions[0].record.correct_action, fundos::transaction::correction_type::replaces);
		EXPECT_EQ(history.value().transactions[0].record.corrects_fitid, std::nullopt);
		EXPECT_EQ(history.value().transactions[0].record.corrects_id, 4);
		EXPECT_EQ(history.value().transactions[0].record.superseded_by, std::nullopt);
		EXPECT_EQ(history.value().transactions[0].allocated.amount, currency{99});
		EXPECT_EQ(history.value().transactions[0].fund_balance, currency{274});
	}
 
	{ // transaction 3
		EXPECT_EQ(history.value().transactions[1].record.id(), 3);
		EXPECT_EQ(history.value().transactions[1].record.account_id, 1);
		EXPECT_EQ(history.value().transactions[1].record.date_recorded, tx3_recorded);
		EXPECT_EQ(history.value().transactions[1].record.memo, "Third Transaction");
		EXPECT_EQ(history.value().transactions[1].record.amount, currency{25});
		EXPECT_EQ(history.value().transactions[1].record.date_reconciled, std::nullopt);
		EXPECT_EQ(history.value().transactions[1].record.fitid, std::nullopt);
		EXPECT_EQ(history.value().transactions[1].record.date_cleared, std::nullopt);
		EXPECT_EQ(history.value().transactions[1].record.correct_action, std::nullopt);
		EXPECT_EQ(history.value().transactions[1].record.corrects_fitid, std::nullopt);
		EXPECT_EQ(history.value().transactions[1].record.corrects_id, std::nullopt);
		EXPECT_EQ(history.value().transactions[1].record.superseded_by, std::nullopt);
		EXPECT_EQ(history.value().transactions[1].allocated.amount, currency{25});
		EXPECT_EQ(history.value().transactions[1].fund_balance, currency{175});
	}

	{ // transaction 2
		EXPECT_EQ(history.value().transactions[2].record.id(), 2);
		EXPECT_EQ(history.value().transactions[2].record.account_id, 1);
		EXPECT_EQ(history.value().transactions[2].record.date_recorded, tx2_recorded);
		EXPECT_EQ(history.value().transactions[2].record.memo, "Second Transaction");
		EXPECT_EQ(history.value().transactions[2].record.amount, currency{50});
		EXPECT_EQ(history.value().transactions[2].record.date_reconciled, std::nullopt);
		EXPECT_EQ(history.value().transactions[2].record.fitid, std::nullopt);
		EXPECT_EQ(history.value().transactions[2].record.date_cleared, std::nullopt);
		EXPECT_EQ(history.value().transactions[2].record.correct_action, std::nullopt);
		EXPECT_EQ(history.value().transactions[2].record.corrects_fitid, std::nullopt);
		EXPECT_EQ(history.value().transactions[2].record.corrects_id, std::nullopt);
		EXPECT_EQ(history.value().transactions[2].record.superseded_by, std::nullopt);
		EXPECT_EQ(history.value().transactions[2].allocated.amount, currency{50});
		EXPECT_EQ(history.value().transactions[2].fund_balance, currency{150});
	}

	// transaction 1 outside date range but contributes to fund_balance
}

/// Previous iterations used a helper function instead of a macro but that loses the ability to assert in a test
#define FUNDOS_SEED() \
	fund groceries; \
	groceries.name = "Groceries"; \
	ASSERT_TRUE(static_cast<bool>(database->save_fund(groceries))); \
	account checking; \
	checking.name = "Checking"; \
	checking.bank_account_id = std::string{"checking-123"}; \
	ASSERT_TRUE(static_cast<bool>(database->save_account(checking))); \
	transaction txn; \
	txn.account_id    = checking.id(); \
	txn.amount        = currency{10000}; \
	txn.date_recorded = datetime{0}; \
	txn.memo          = "Test"; \
	std::vector<allocation> allocations; \
	ASSERT_TRUE(static_cast<bool>(database->save_transaction(txn, allocations)));

static int64_t fetch_int64(sqlite3* connection, const char* sql) {
	int64_t value{};
	sqlite3_exec(connection, sql,
		[](void* data, int, char** cols, char**) {
			*static_cast<int64_t*>(data) = cols[0] ? static_cast<int64_t>(std::atoll(cols[0])) : int64_t{};
			return 0;
		}, &value, nullptr);
	return value;
}

static std::string fetch_string(sqlite3* connection, const char* sql) {
	std::string value;
	sqlite3_exec(connection, sql,
		[](void* data, int, char** cols, char**) {
			if (cols[0]) { *static_cast<std::string*>(data) = cols[0]; }
			return 0;
		}, &value, nullptr);
	return value;
}

static std::optional<int64_t> fetch_optional_int64(sqlite3* connection, const char* sql) {
	std::optional<int64_t> value;
	sqlite3_exec(connection, sql,
		[](void* data, int, char** cols, char**) {
			auto* out = static_cast<std::optional<int64_t>*>(data);
			*out = cols[0] ? std::optional<int64_t>{std::atoll(cols[0])} : std::nullopt;
			return 0;
		}, &value, nullptr);
	return value;
}

bool transaction_exists(sqlite3* connection, const fundos::transaction& transaction, int64_t id = 0) {
	auto correction_string = [](fundos::transaction::correction_type correct_action) -> std::string_view {
		switch (correct_action) {
			case fundos::transaction::correction_type::replaces: return "replace";
			case fundos::transaction::correction_type::deletes:  return "delete";
		}
		FUNDOS_UNREACHABLE();
	};
	std::string sql = std::format(
		"SELECT 1 FROM transactions"
		" WHERE account_id    =  {}"
		" AND amount          =  {}"
		" AND date_recorded   =  {}"
		" AND memo            = '{}'"
		" AND date_reconciled IS {}"
		" AND fitid           IS {}"
		" AND date_cleared    IS {}"
		" AND corrects_fitid  IS {}"
		" AND correct_action  IS {}"
		" AND corrects_id     IS {}"
		" AND superseded_by   IS {}",
		transaction.account_id,
		transaction.amount.minor_units,
		transaction.date_recorded.milliseconds_since_epoch,
		transaction.memo,
		transaction.date_reconciled ? std::format( "{}",   transaction.date_reconciled->milliseconds_since_epoch) : "NULL",
		transaction.fitid           ? std::format("'{}'", *transaction.fitid)                                     : "NULL",
		transaction.date_cleared    ? std::format( "{}",   transaction.date_cleared->milliseconds_since_epoch)    : "NULL",
		transaction.corrects_fitid  ? std::format("'{}'", *transaction.corrects_fitid)                            : "NULL",
		transaction.correct_action  ? std::format("'{}'", correction_string(*transaction.correct_action))         : "NULL",
		transaction.corrects_id     ? std::format( "{}",  *transaction.corrects_id)                               : "NULL",
		transaction.superseded_by   ? std::format( "{}",  *transaction.superseded_by)                             : "NULL"
	);
	int64_t pinned_id = id != 0 ? id : transaction.id();
	if (pinned_id != 0) {
		sql += std::format(" AND id = {}", pinned_id);
	}
	bool found = false;
	sqlite3_exec(connection, sql.c_str(),
		[](void* data, int, char**, char**) {
			*static_cast<bool*>(data) = true;
			return 0;
		}, &found, nullptr
	);
	return found;
}

TEST(DbQuery, SaveTransaction_Insert) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	EXPECT_NE(txn.id(), 0);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM transactions"), 1);
	EXPECT_TRUE(transaction_exists(connection, txn));
}

TEST(DbQuery, SaveTransaction_Update) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	EXPECT_NE(txn.id(), 0);
	txn.date_recorded = datetime{86400000}; // 1 day later
	txn.memo = "Updated";
	ASSERT_TRUE(static_cast<bool>(database->save_transaction(txn, allocations)));
	EXPECT_EQ(86400000, fetch_int64(connection, "SELECT date_recorded FROM transactions LIMIT 1"));
	EXPECT_EQ("Updated", fetch_string(connection, "SELECT memo FROM transactions LIMIT 1"));
	EXPECT_TRUE(transaction_exists(connection, txn));
}

TEST(DbQuery, SaveTransaction_Update_ImmutableFieldChanged) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	EXPECT_NE(txn.id(), 0);
	txn.amount = currency{99999};
	EXPECT_EQ(database->save_transaction(txn, allocations).code, db::error::rejected);
	txn.amount = currency{10000};
	EXPECT_TRUE(transaction_exists(connection, txn));
}

TEST(DbQuery, SaveTransaction_Update_NonexistentId) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	EXPECT_NE(txn.id(), 0);
	sqlite3_exec(connection, "DELETE FROM transactions", nullptr, nullptr, nullptr);
	EXPECT_EQ(database->save_transaction(txn, allocations).code, db::error::bad_request);
}

TEST(DbQuery, SaveTransaction_InsertCorrection) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	EXPECT_NE(txn.id(), 0);
	transaction correction;
	correction.account_id    = checking.id();
	correction.amount        = currency{5000};
	correction.date_recorded = datetime{0};
	correction.memo          = "Correction";
	correction.corrects_id   = txn.id();
	correction.correct_action = transaction::correction_type::replaces;
	ASSERT_TRUE(static_cast<bool>(database->save_transaction(correction, allocations)));
	ASSERT_NE(correction.id(), 0);
	txn.superseded_by = fetch_optional_int64(connection,
		std::format("SELECT superseded_by FROM transactions WHERE id = {}", txn.id()).c_str()
	);
	EXPECT_EQ(txn.superseded_by, correction.id());
	EXPECT_EQ(correction.corrects_id, txn.id());
	EXPECT_TRUE(transaction_exists(connection, txn));
	EXPECT_TRUE(transaction_exists(connection, correction));
}

TEST(DbQuery, SaveTransaction_InsertCorrection_ParityMissing_CorrectAction) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	transaction correction;
	correction.account_id    = checking.id();
	correction.amount        = currency{5000};
	correction.date_recorded = datetime{0};
	correction.memo          = "Bad";
	correction.corrects_id   = txn.id();
	// correct_action intentionally omitted
	EXPECT_EQ(database->save_transaction(correction, allocations).code, db::error::bad_request);
	EXPECT_TRUE(transaction_exists(connection, txn));
}

TEST(DbQuery, SaveTransaction_InsertCorrection_ParityMissing_CorrectsId) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	transaction correction;
	correction.account_id     = checking.id();
	correction.amount         = currency{5000};
	correction.date_recorded  = datetime{0};
	correction.memo           = "Bad";
	correction.correct_action = transaction::correction_type::replaces;
	// corrects_id intentionally omitted
	EXPECT_EQ(database->save_transaction(correction, allocations).code, db::error::bad_request);
	EXPECT_TRUE(transaction_exists(connection, txn));
}

TEST(DbQuery, SaveTransaction_InsertCorrection_TargetHasFitid) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	std::string update = std::format(
		"UPDATE transactions SET fitid = 'imported-fitid', date_cleared = 1 WHERE id = {}", txn.id()
	);
	ASSERT_EQ(sqlite3_exec(connection, update.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);
	txn.fitid        = std::string{"imported-fitid"};
	txn.date_cleared = datetime{1};
	transaction correction;
	correction.account_id     = checking.id();
	correction.amount         = currency{5000};
	correction.date_recorded  = datetime{0};
	correction.memo           = "Correction";
	correction.corrects_id    = txn.id();
	correction.correct_action = transaction::correction_type::replaces;
	EXPECT_EQ(database->save_transaction(correction, allocations).code, db::error::rejected);
	EXPECT_TRUE(transaction_exists(connection, txn));
}

TEST(DbQuery, SaveTransaction_InsertCorrection_TargetAlreadySuperseded) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	EXPECT_NE(txn.id(), 0);
	transaction superseder;
	superseder.account_id     = checking.id();
	superseder.amount         = currency{5000};
	superseder.date_recorded  = datetime{0};
	superseder.memo           = "Superseder";
	superseder.corrects_id    = txn.id();
	superseder.correct_action = transaction::correction_type::replaces;
	ASSERT_TRUE(static_cast<bool>(database->save_transaction(superseder, allocations)));
	ASSERT_NE(superseder.id(), 0);
	txn.superseded_by = superseder.id();
	transaction correction;
	correction.account_id     = checking.id();
	correction.amount         = currency{5000};
	correction.date_recorded  = datetime{0};
	correction.memo           = "Correction";
	correction.corrects_id    = txn.id();
	correction.correct_action = transaction::correction_type::replaces;
	EXPECT_EQ(database->save_transaction(correction, allocations).code, db::error::rejected);
	EXPECT_TRUE(transaction_exists(connection, txn));
	EXPECT_TRUE(transaction_exists(connection, superseder));
}

TEST(DbQuery, SaveTransaction_InsertCorrection_TargetWrongAccount) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	EXPECT_NE(txn.id(), 0);
	account savings;
	savings.name = "Savings";
	ASSERT_TRUE(static_cast<bool>(database->save_account(savings)));
	transaction correction;
	correction.account_id     = savings.id();
	correction.amount         = currency{5000};
	correction.date_recorded  = datetime{0};
	correction.memo           = "Correction";
	correction.corrects_id    = txn.id();
	correction.correct_action = transaction::correction_type::replaces;
	EXPECT_EQ(database->save_transaction(correction, allocations).code, db::error::bad_request);
	EXPECT_TRUE(transaction_exists(connection, txn));
}

#define FUNDOS_SEED_IMPORT() \
	FUNDOS_SEED(); \
	transaction previous_import; \
	previous_import.account_id    = checking.id(); \
	previous_import.amount        = currency{5000}; \
	previous_import.date_recorded = CLOSED_AT - timedelta::days(3); \
	previous_import.memo          = "Previous Import"; \
	previous_import.fitid         = std::string{"fitid-existing"}; \
	previous_import.date_cleared  = CLOSED_AT; \
	std::string previous_import_sql = std::format( \
		"INSERT INTO transactions (account_id, amount, date_recorded, date_cleared, memo, fitid) " \
		"VALUES ({}, {}, {}, {}, '{}', '{}')", \
		previous_import.account_id, \
		previous_import.amount.minor_units, \
		previous_import.date_recorded.milliseconds_since_epoch, \
		previous_import.date_cleared->milliseconds_since_epoch, \
		previous_import.memo, \
		*previous_import.fitid \
	); \
	ASSERT_EQ(sqlite3_exec(connection, previous_import_sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK); \
	int64_t previous_import_id = sqlite3_last_insert_rowid(connection); \
	(void)previous_import_id; \
	import::imported_transaction matched_import; \
	matched_import.fitid         = *previous_import.fitid; \
	matched_import.date_cleared  = *previous_import.date_cleared; \
	matched_import.amount        =  previous_import.amount; \
	matched_import.memo    = "New Memo"; \
	import::imported_transaction fresh_import; \
	fresh_import.fitid         = std::string{"fitid-new"}; \
	fresh_import.date_cleared  = CLOSED_AT + timedelta::days(21); \
	fresh_import.amount        = currency{2000}; \
	fresh_import.memo          = "Fresh"; \
	import::pending_import pending; \
	import::bank_account bank; \
	bank.acct_id = "checking-123"; \
	bank.balance = currency{15000}; \
	bank.as_of   = datetime{0}; \
	bank.transactions.push_back(matched_import); \
	bank.transactions.push_back(fresh_import); \
	pending.accounts.push_back(std::move(bank));

TEST(DbQuery, PrepareImport_DefinitiveMatch) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_TRUE(static_cast<bool>(database->prepare_import(pending)));
	auto& matched = pending.accounts[0].transactions[0];
	EXPECT_TRUE(matched.is_definitive_match());
	EXPECT_EQ(matched.get_match()->id(), previous_import_id);
}

TEST(DbQuery, PrepareImport_NoMatch) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_TRUE(static_cast<bool>(database->prepare_import(pending)));
	auto& fresh = pending.accounts[0].transactions[1];
	EXPECT_EQ(fresh.get_match(), nullptr);
}

TEST(DbQuery, PrepareImport_FuzzyMatch) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	// Insert a candidate with no fitid, matching amount, date within 7 days
	std::string sql = std::format(
		"INSERT INTO transactions (account_id, amount, date_recorded, memo) "
		"VALUES ({}, {}, {}, 'Fuzzy Candidate')",
		checking.id(),
		fresh_import.amount.minor_units,
		fresh_import.date_cleared.milliseconds_since_epoch - timedelta::days(3).milliseconds
	);
	ASSERT_EQ(sqlite3_exec(connection, sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);
	ASSERT_TRUE(static_cast<bool>(database->prepare_import(pending)));
	auto& fresh = pending.accounts[0].transactions[1];
	EXPECT_NE(fresh.get_match(), nullptr);
	EXPECT_FALSE(fresh.is_definitive_match());
}

TEST(DbQuery, PrepareImport_MissingFitid) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	pending.accounts[0].transactions[0].fitid = std::string();
	EXPECT_EQ(database->prepare_import(pending).code, db::error::bad_request);
}

TEST(DbQuery, PrepareImport_MissingCleared) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	pending.accounts[0].transactions[0].date_cleared = {0};
	EXPECT_EQ(database->prepare_import(pending).code, db::error::bad_request);
}

TEST(DbQuery, PrepareImport_UnknownAcctId) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	pending.accounts[0].acct_id = "does-not-exist";
	EXPECT_EQ(database->prepare_import(pending).code, db::error::rejected);
}

TEST(DbQuery, PrepareImport_DefinitiveMatchInCandidates) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_TRUE(static_cast<bool>(database->prepare_import(pending)));
	auto& account = pending.accounts[0];
	auto it = std::find_if(account.candidates.begin(), account.candidates.end(),
		[&](const transaction& candidate) {
			return candidate.id() == previous_import_id;
		});
	EXPECT_NE(it, account.candidates.end());
}

TEST(DbQuery, PrepareImport_PrefersMatchInformation) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_TRUE(static_cast<bool>(database->prepare_import(pending)));
	auto& account = pending.accounts[0];
	auto it = std::find_if(account.candidates.begin(), account.candidates.end(),
		[&](const transaction& candidate) {
			return candidate.id() == previous_import_id;
		});
	EXPECT_NE(it, account.candidates.end());
	EXPECT_EQ(it->memo, previous_import.memo);
	EXPECT_EQ(it->date_recorded, previous_import.date_recorded);
}

TEST(DbQuery, PerformImport_InsertsNewTransaction) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_TRUE(static_cast<bool>(database->prepare_import(pending)));
	ASSERT_TRUE(static_cast<bool>(database->perform_import(pending)));
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM transactions"), 3); // txn + previous_import + fresh_import, matched_import updates previous_import
	EXPECT_TRUE(transaction_exists(connection, txn, txn.id()));
	EXPECT_TRUE(transaction_exists(connection, previous_import, previous_import_id));
	
	fundos::transaction expected_fresh;
	expected_fresh.account_id     = checking.id();             // the import process sets account_id from the target account
	expected_fresh.amount         = fresh_import.amount;
	expected_fresh.date_recorded  = fresh_import.date_cleared; // the import process takes date_recorded from date_cleared for fresh imports
	expected_fresh.date_cleared   = fresh_import.date_cleared;
	expected_fresh.fitid          = fresh_import.fitid;
	expected_fresh.memo           = fresh_import.name;         // unmatched transaction: memo resolution falls back to name
	expected_fresh.corrects_fitid = fresh_import.corrects_fitid;
	expected_fresh.correct_action = fresh_import.correct_action;
	EXPECT_TRUE(transaction_exists(connection, expected_fresh));
}

TEST(DbQuery, PerformImport_PreservesExistingMemo) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_TRUE(static_cast<bool>(database->prepare_import(pending)));
	ASSERT_TRUE(static_cast<bool>(database->perform_import(pending)));
	EXPECT_TRUE(transaction_exists(connection, previous_import, previous_import_id));
}

TEST(DbQuery, PerformImport_UpdatesMatchedTransaction) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_TRUE(static_cast<bool>(database->prepare_import(pending)));
	pending.accounts[0].transactions[0].choice = fundos::import::imported_transaction::memo_choice::prefer_memo;
	ASSERT_TRUE(static_cast<bool>(database->perform_import(pending)));
	previous_import.memo = matched_import.memo; // prefer_memo replaces memo with the imported value
	EXPECT_TRUE(transaction_exists(connection, previous_import, previous_import_id));
}

TEST(DbQuery, PerformImport_CreatesCheckpoint) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_TRUE(static_cast<bool>(database->prepare_import(pending)));
	ASSERT_TRUE(static_cast<bool>(database->perform_import(pending)));
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM import_ledger_balances"), 1);
}

TEST(DbQuery, PerformImport_WithoutPrepare) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	// account_id is 0, acct_id won't match what prepare would have set
	EXPECT_EQ(database->perform_import(pending).code, db::error::bad_request);
}

TEST(DbQuery, PerformImport_StaleMatch) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_TRUE(static_cast<bool>(database->prepare_import(pending)));
	// Mutate the db between prepare and perform
	std::string sql = std::format(
		"UPDATE transactions SET amount = 99999 WHERE id = {}", previous_import_id
	);
	ASSERT_EQ(sqlite3_exec(connection, sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);
	EXPECT_EQ(database->perform_import(pending).code, db::error::bad_request);
}

TEST(DbQuery, PerformImport_ResolvesCorrections) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	pending.accounts[0].transactions[1].corrects_fitid = std::string{"fitid-existing"};
	pending.accounts[0].transactions[1].correct_action = transaction::correction_type::replaces;
	ASSERT_TRUE(static_cast<bool>(database->prepare_import(pending)));
	ASSERT_TRUE(static_cast<bool>(database->perform_import(pending)));
	auto corrects_id = fetch_optional_int64(connection,
		std::format(
			"SELECT corrects_id FROM transactions WHERE fitid = 'fitid-new'"
		).c_str()
	);
	EXPECT_EQ(corrects_id, previous_import_id);
}

TEST(DbQuery, AllocateTransaction_SingleAllocation) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();

	allocation alloc;
	alloc.transaction_id = txn.id();
	alloc.fund_id = groceries.id();
	alloc.amount = currency{10000};

	allocations.push_back(alloc);
	ASSERT_TRUE(static_cast<bool>(database->save_transaction(txn, allocations)));
	EXPECT_NE(allocations[0].id(), 0);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM allocations"), 1);
}

TEST(DbQuery, AllocateTransaction_MultipleAllocations) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	fund rent;
	rent.name = "Rent";
	ASSERT_TRUE(static_cast<bool>(database->save_fund(rent)));

	allocation alloc1;
	alloc1.transaction_id = txn.id();
	alloc1.fund_id = groceries.id();
	alloc1.amount = currency{6000};

	allocation alloc2;
	alloc2.transaction_id = txn.id();
	alloc2.fund_id = rent.id();
	alloc2.amount = currency{4000};

	allocations.push_back(alloc1);
	allocations.push_back(alloc2);
	ASSERT_TRUE(static_cast<bool>(database->save_transaction(txn, allocations)));
	EXPECT_NE(allocations[0].id(), 0);
	EXPECT_NE(allocations[1].id(), 0);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM allocations"), 2);
}

TEST(DbQuery, AllocateTransaction_UpdateExisting) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	fund rent;
	rent.name = "Rent";
	ASSERT_TRUE(static_cast<bool>(database->save_fund(rent)));

	allocation alloc;
	alloc.transaction_id = txn.id();
	alloc.fund_id = groceries.id();
	alloc.amount = currency{10000};
	allocations.push_back(alloc);
	ASSERT_TRUE(static_cast<bool>(database->save_transaction(txn, allocations)));
	ASSERT_NE(allocations[0].id(), 0);
	int64_t original_id = allocations[0].id();

	// Update amount on the existing allocation
	allocations[0].fund_id = rent.id(); // same sum, different fund
	ASSERT_TRUE(static_cast<bool>(database->save_transaction(txn, allocations)));
	EXPECT_EQ(allocations[0].id(), original_id);
	std::string query = std::format("SELECT COUNT(*) FROM allocations WHERE fund_id = {}", rent.id());
	EXPECT_EQ(count_rows(connection, query.c_str()), 1);
}

TEST(DbQuery, AllocationTransactionIdFilledAutomatically) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();

	allocation alloc;
	alloc.fund_id = groceries.id();
	alloc.amount = currency{10000};
	allocations.push_back(alloc);
	EXPECT_EQ(database->save_transaction(txn, allocations).code, db::error::none);
}

TEST(DbQuery, AllocateTransaction_MismatchedTransactionIds) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	fund rent;
	rent.name = "Rent";
	ASSERT_TRUE(static_cast<bool>(database->save_fund(rent)));

	allocation alloc1;
	alloc1.transaction_id = txn.id();
	alloc1.fund_id = groceries.id();
	alloc1.amount = currency{6000};

	allocation alloc2;
	alloc2.transaction_id = txn.id() + 1;
	alloc2.fund_id = rent.id();
	alloc2.amount = currency{4000};

	allocations.push_back(alloc1);
	allocations.push_back(alloc2);
	EXPECT_EQ(database->save_transaction(txn, allocations).code, db::error::bad_request);
}

TEST(DbQuery, AllocateTransaction_DuplicateFund) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();

	allocation alloc1;
	alloc1.transaction_id = txn.id();
	alloc1.fund_id = groceries.id();
	alloc1.amount = currency{6000};

	allocation alloc2;
	alloc2.transaction_id = txn.id();
	alloc2.fund_id = groceries.id();
	alloc2.amount = currency{4000};

	allocations.push_back(alloc1);
	allocations.push_back(alloc2);
	EXPECT_EQ(database->save_transaction(txn, allocations).code, db::error::bad_request);
}

TEST(DbQuery, AllocateTransaction_ZeroFundId) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();

	allocation alloc;
	alloc.transaction_id = txn.id();
	alloc.amount = currency{10000};
	allocations.push_back(alloc);
	EXPECT_EQ(database->save_transaction(txn, allocations).code, db::error::bad_request);
}

TEST(DbQuery, AllocateTransaction_WrongSum) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();

	allocation alloc;
	alloc.transaction_id = txn.id();
	alloc.fund_id = groceries.id();
	alloc.amount = currency{9999};
	allocations.push_back(alloc);
	EXPECT_EQ(database->save_transaction(txn, allocations).code, db::error::rejected);
}

TEST(DbQuery, AllocateTransaction_ClosedFund) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	groceries.closed_at = CLOSED_AT;
	ASSERT_TRUE(static_cast<bool>(database->save_fund(groceries)));

	allocation alloc;
	alloc.transaction_id = txn.id();
	alloc.fund_id = groceries.id();
	alloc.amount = currency{10000};
	allocations.push_back(alloc);

	// closed_at is a visual anchor only (hides the fund from selection UI);
	// the database layer does not restrict allocations against closed funds. See 43c1844.
	EXPECT_EQ(database->save_transaction(txn, allocations).code, db::error::none);
}
