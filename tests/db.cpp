#include <gtest/gtest.h>
#include <chrono>
#include <format>

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
		EXPECT_EQ(status.sqlite3_error, db::error::none);
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
		EXPECT_EQ(status.sqlite3_error, db::error::none);
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
	EXPECT_EQ(status.sqlite3_error, db::error::none);
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
	EXPECT_EQ(status.sqlite3_error, db::error::none);
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
	EXPECT_EQ(status.sqlite3_error, db::error::none);
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
	EXPECT_EQ(status.sqlite3_error, db::error::none);
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
	EXPECT_EQ(status.sqlite3_error, db::error::none);
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
	EXPECT_EQ(status.sqlite3_error, db::error::corrupted);
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
	EXPECT_EQ(status.sqlite3_error, db::error::corrupted);
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
	EXPECT_EQ(status.sqlite3_error, db::error::corrupted);
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
	EXPECT_EQ(status.sqlite3_error, db::error::corrupted);
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
	EXPECT_EQ(status.sqlite3_error, db::error::none);
	EXPECT_EQ(status.is_ok(), false);
	EXPECT_EQ(status.has_error(), true);
	EXPECT_EQ(status.needs_migration(), false);
}

/// Previous iterations used a helper function instead of a macro but that loses the ability to assert in a test
#define FUNDOS_TEST_DB() \
	sqlite3* connection = mockDb(); \
	auto database = std::make_shared<db>(connection, db::owns_connection{}); \
	ASSERT_EQ(database->is_ready(), true);

static constexpr datetime CLOSED_AT = {
	std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::sys_days{std::chrono::year{2024} / std::chrono::June / 1}
		.time_since_epoch()
	).count()
};

TEST(DbQuery, ReadEntities) {
	FUNDOS_TEST_DB();
	sqlite3_exec(connection, std::format(R"sql(
		INSERT INTO users (id, name) VALUES (1, 'Alice');
		INSERT INTO users (id, name) VALUES (2, 'Bob');
		INSERT INTO funds (id, name, closed_at) VALUES (10, 'Emergency', NULL);
		INSERT INTO funds (id, name, closed_at) VALUES (11, 'Vacation', {});
		INSERT INTO accounts (id, name, closed_at, bank_account_id) VALUES (20, 'Checking', NULL, 'ref1');
		INSERT INTO accounts (id, name, closed_at, bank_account_id) VALUES (21, 'Savings', {}, NULL);
		INSERT INTO fund_members (fund_id, user_id) VALUES (10, 1);
		INSERT INTO fund_members (fund_id, user_id) VALUES (11, 1);
		INSERT INTO account_members (account_id, user_id) VALUES (20, 1);
	)sql", CLOSED_AT.milliseconds_since_epoch, CLOSED_AT.milliseconds_since_epoch).c_str(), nullptr, nullptr, nullptr);

	{
		auto result = database->get_users();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 2);
		auto alice = (*result.val)[0];
		EXPECT_EQ(alice.id(), 1);
		EXPECT_EQ(alice.name, "Alice");
		auto bob = (*result.val)[1];
		EXPECT_EQ(bob.id(), 2);
		EXPECT_EQ(bob.name, "Bob");
	}

	{
		auto result = database->get_funds();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 2);
		auto emergency = (*result.val)[0];
		EXPECT_EQ(emergency.id(), 10);
		EXPECT_EQ(emergency.name, "Emergency");
		EXPECT_EQ(emergency.closed_at, std::nullopt);
		auto vacation = (*result.val)[1];
		EXPECT_EQ(vacation.id(), 11);
		EXPECT_EQ(vacation.name, "Vacation");
		EXPECT_EQ(vacation.closed_at, CLOSED_AT);
	}

	{
		auto result = database->get_accounts();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 2);
		auto checking = (*result.val)[0];
		EXPECT_EQ(checking.id(), 20);
		EXPECT_EQ(checking.name, "Checking");
		EXPECT_EQ(checking.closed_at, std::nullopt);
		EXPECT_EQ(checking.bank_account_id, "ref1");
		auto savings = (*result.val)[1];
		EXPECT_EQ(savings.id(), 21);
		EXPECT_EQ(savings.name, "Savings");
		EXPECT_EQ(savings.closed_at, CLOSED_AT);
		EXPECT_EQ(savings.bank_account_id, std::nullopt);
	}

	{
		// Alice is member of Emergency (open), not Vacation (closed) — memberships excludes closed
		auto memberships = database->get_fund_memberships(1);
		ASSERT_TRUE(memberships.ok());
		ASSERT_EQ(memberships.val->size(), 1);
		EXPECT_EQ((*memberships.val)[0].id(), 10);
		EXPECT_EQ((*memberships.val)[0].name, "Emergency");

		// nonmemberships excludes closed funds too, so no rows return for Alice
		auto nonmemberships = database->get_fund_nonmemberships(1);
		ASSERT_TRUE(nonmemberships.ok());
		ASSERT_EQ(nonmemberships.val->size(), 0);

		// Bob is not a member of the only open fund open fund
		auto bob_nonmemberships = database->get_fund_nonmemberships(2);
		ASSERT_TRUE(bob_nonmemberships.ok());
		ASSERT_EQ(bob_nonmemberships.val->size(), 1);
		EXPECT_EQ((*bob_nonmemberships.val)[0].id(), 10);

		auto members = database->get_fund_members(10);
		ASSERT_TRUE(members.ok());
		ASSERT_EQ(members.val->size(), 1);
		EXPECT_EQ((*members.val)[0].id(), 1);
		EXPECT_EQ((*members.val)[0].name, "Alice");

		auto nonmembers = database->get_fund_nonmembers(10);
		ASSERT_TRUE(nonmembers.ok());
		ASSERT_EQ(nonmembers.val->size(), 1);
		EXPECT_EQ((*nonmembers.val)[0].id(), 2);
		EXPECT_EQ((*nonmembers.val)[0].name, "Bob");
	}

	{
		auto memberships = database->get_account_memberships(1);
		ASSERT_TRUE(memberships.ok());
		ASSERT_EQ(memberships.val->size(), 1);
		EXPECT_EQ((*memberships.val)[0].id(), 20);
		EXPECT_EQ((*memberships.val)[0].name, "Checking");
		EXPECT_EQ((*memberships.val)[0].bank_account_id, "ref1");

		auto nonmemberships = database->get_account_nonmemberships(1);
		ASSERT_TRUE(nonmemberships.ok());
		ASSERT_EQ(nonmemberships.val->size(), 0);

		auto members = database->get_account_members(20);
		ASSERT_TRUE(members.ok());
		ASSERT_EQ(members.val->size(), 1);
		EXPECT_EQ((*members.val)[0].id(), 1);
		EXPECT_EQ((*members.val)[0].name, "Alice");

		auto nonmembers = database->get_account_nonmembers(20);
		ASSERT_TRUE(nonmembers.ok());
		ASSERT_EQ(nonmembers.val->size(), 1);
		EXPECT_EQ((*nonmembers.val)[0].id(), 2);
		EXPECT_EQ((*nonmembers.val)[0].name, "Bob");
	}
}

TEST(DbQuery, SaveEntities) {
	FUNDOS_TEST_DB();

	user alice;
	alice.name = "Alice";
	ASSERT_EQ(database->save_user(alice), db::error::none);
	EXPECT_NE(alice.id(), 0);

	fund emergency;
	emergency.name = "Emergency";
	ASSERT_EQ(database->save_fund(emergency), db::error::none);
	EXPECT_NE(emergency.id(), 0);

	account checking;
	checking.name = "Checking";
	ASSERT_EQ(database->save_account(checking), db::error::none);
	EXPECT_NE(checking.id(), 0);

	{
		auto result = database->get_users();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), alice.id());
		EXPECT_EQ(row.name, alice.name);
	}
	{
		auto result = database->get_funds();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), emergency.id());
		EXPECT_EQ(row.name, emergency.name);
		EXPECT_EQ(row.closed_at, emergency.closed_at);
	}
	{
		auto result = database->get_accounts();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), checking.id());
		EXPECT_EQ(row.name, checking.name);
		EXPECT_EQ(row.closed_at, checking.closed_at);
		EXPECT_EQ(row.bank_account_id, checking.bank_account_id);
	}

	// Verify membership additions and removals
	ASSERT_EQ(database->add_user_to_account(checking.id(), alice.id()), db::error::none);
	{
		auto result = database->get_account_members(checking.id());
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), alice.id());
		EXPECT_EQ(row.name, alice.name);
	}
	{
		auto result = database->get_account_memberships(alice.id());
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), checking.id());
		EXPECT_EQ(row.name, checking.name);
		EXPECT_EQ(row.closed_at, checking.closed_at);
		EXPECT_EQ(row.bank_account_id, checking.bank_account_id);
	}
	ASSERT_EQ(database->remove_user_from_account(checking.id(), alice.id()), db::error::none);
	{
		auto result = database->get_account_members(checking.id());
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 0);
	}
	{
		auto result = database->get_account_memberships(alice.id());
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 0);
	}

	ASSERT_EQ(database->add_user_to_fund(emergency.id(), alice.id()), db::error::none);
	{
		auto result = database->get_fund_members(emergency.id());
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), alice.id());
		EXPECT_EQ(row.name, alice.name);
	}
	{
		auto result = database->get_fund_memberships(alice.id());
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), emergency.id());
		EXPECT_EQ(row.name, emergency.name);
		EXPECT_EQ(row.closed_at, emergency.closed_at);
	}
	ASSERT_EQ(database->remove_user_from_fund(emergency.id(), alice.id()), db::error::none);
	{
		auto result = database->get_fund_members(emergency.id());
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 0);
	}
	{
		auto result = database->get_fund_memberships(alice.id());
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 0);
	}

	// Verify update path
	alice.name = "Alicia";
	ASSERT_EQ(database->save_user(alice), db::error::none);

	{
		auto result = database->get_users();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), alice.id());
		EXPECT_EQ(row.name, "Alicia");
	}

	emergency.name = "Rainy Day";
	emergency.closed_at = CLOSED_AT;
	ASSERT_EQ(database->save_fund(emergency), db::error::none);
	{
		auto result = database->get_funds();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), emergency.id());
		EXPECT_EQ(row.name, "Rainy Day");
		EXPECT_EQ(row.closed_at, CLOSED_AT);
	}

	checking.name = "Debit Card";
	checking.bank_account_id = "ref1";
	checking.closed_at = CLOSED_AT;
	ASSERT_EQ(database->save_account(checking), db::error::none);
	{
		auto result = database->get_accounts();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
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

		EXPECT_EQ(saved_currency.not_found(),   true);
		EXPECT_EQ(saved_percentage.not_found(), true);
	}
	{
		auto currency_result   = database->set_currency_locale_preset(currency_locale::locales.named.USD);
		auto percentage_result = database->set_percentage_locale_preset(percentage_locale::locales.named.en);

		EXPECT_EQ(currency_result, db::error::none);
		EXPECT_EQ(percentage_result, db::error::none);

		auto saved_currency   = database->get_currency_locale();
		auto saved_percentage = database->get_percentage_locale();

		EXPECT_EQ(saved_currency.ok(),   true);
		EXPECT_EQ(saved_percentage.ok(), true);

		auto c_locale = saved_currency.val.value();
		auto p_locale = saved_percentage.val.value();

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
		currency_locale::info custom_currency = {
			.scale = 1000,
			.symbol = "L",
			.thousands_separator = ';',
			.decimal_separator = ':',
			.symbol_position = currency_locale::info::symbol_placement::before,
			.negative_format = currency_locale::info::negative_notation::leading_minus,
		};
		percentage_locale::info custom_percentage = {
			.decimal_separator = ':',
			.has_space_around_number = true,
			.symbol_position = percentage_locale::info::symbol_placement::before,
		};
		auto currency_result   = database->set_currency_locale(custom_currency);
		auto percentage_result = database->set_percentage_locale(custom_percentage);

		EXPECT_EQ(currency_result, db::error::none);
		EXPECT_EQ(percentage_result, db::error::none);

		auto saved_currency   = database->get_currency_locale();
		auto saved_percentage = database->get_percentage_locale();

		EXPECT_EQ(saved_currency.ok(),   true);
		EXPECT_EQ(saved_percentage.ok(), true);

		auto c_locale = saved_currency.val.value();
		auto p_locale = saved_percentage.val.value();

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
	ASSERT_EQ(database->save_fund(emergency_savings), db::error::none);
	ASSERT_EQ(database->save_fund(food),              db::error::none);
	ASSERT_EQ(database->save_fund(investments),       db::error::none);
	ASSERT_EQ(database->save_fund(flex_spending),     db::error::none);
	ASSERT_EQ(database->save_fund(rent),              db::error::none);

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
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);

		auto& budget = (*result.val)[0];
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
	ASSERT_EQ(database->save_fund(emergency_savings), db::error::none);
	ASSERT_EQ(database->save_fund(food),              db::error::none);
	ASSERT_EQ(database->save_fund(investments),       db::error::none);
	ASSERT_EQ(database->save_fund(flex_spending),     db::error::none);
	ASSERT_EQ(database->save_fund(rent),              db::error::none);

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
	ASSERT_TRUE(get_result.ok());
	ASSERT_EQ(get_result.val->size(), 1);
	budget default_budget = (*get_result.val)[0];

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

	ASSERT_EQ(database->save_budget(default_budget), db::error::none);

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
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);

		auto& budget = (*result.val)[0];
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
	ASSERT_EQ(database->save_fund(emergency_savings), db::error::none);
	ASSERT_EQ(database->save_fund(flex_spending),     db::error::none);
	ASSERT_EQ(database->save_fund(investments),       db::error::none);

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
	EXPECT_NE(result, db::error::none);

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

	ASSERT_EQ(database->save_budget(bad_budget), db::error::none);
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

/// Previous iterations used a helper function instead of a macro but that loses the ability to assert in a test
#define FUNDOS_SEED() \
	fund groceries; \
	groceries.name = "Groceries"; \
	ASSERT_EQ(database->save_fund(groceries), db::error::none); \
	account checking; \
	checking.name = "Checking"; \
	checking.bank_account_id = std::string{"checking-123"}; \
	ASSERT_EQ(database->save_account(checking), db::error::none); \
	transaction txn; \
	txn.account_id = checking.id(); \
	txn.amount = currency{10000}; \
	txn.date = datetime{0}; \
	txn.memo = "Test"; \
	ASSERT_EQ(database->save_transaction(txn), db::error::none);

template<typename T>
static T fetch_field(sqlite3* connection, const char* sql) {
	T value{};
	sqlite3_exec(connection, sql,
		[](void* data, int, char** cols, char**) {
			*static_cast<T*>(data) = cols[0] ? static_cast<T>(std::atoll(cols[0])) : T{};
			return 0;
		}, &value, nullptr);
	return value;
}

template<>
static std::string fetch_field<std::string>(sqlite3* connection, const char* sql) {
	std::string value;
	sqlite3_exec(connection, sql,
		[](void* data, int, char** cols, char**) {
			if (cols[0]) { *static_cast<std::string*>(data) = cols[0]; }
			return 0;
		}, &value, nullptr);
	return value;
}

template<>
static std::optional<std::string> fetch_field<std::optional<std::string>>(sqlite3* connection, const char* sql) {
	std::optional<std::string> value;
	sqlite3_exec(connection, sql,
		[](void* data, int, char** cols, char**) {
			auto* out = static_cast<std::optional<std::string>*>(data);
			*out = cols[0] ? std::optional<std::string>{cols[0]} : std::nullopt;
			return 0;
		}, &value, nullptr);
	return value;
}

template<>
static std::optional<int64_t> fetch_field<std::optional<int64_t>>(sqlite3* connection, const char* sql) {
	std::optional<int64_t> value;
	sqlite3_exec(connection, sql,
		[](void* data, int, char** cols, char**) {
			auto* out = static_cast<std::optional<int64_t>*>(data);
			*out = cols[0] ? std::optional<int64_t>{std::atoll(cols[0])} : std::nullopt;
			return 0;
		}, &value, nullptr);
	return value;
}

TEST(DbQuery, SaveTransaction_Insert) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	EXPECT_NE(txn.id(), 0);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM transactions"), 1);
}

TEST(DbQuery, SaveTransaction_Update) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	txn.date = datetime{86400000}; // 1 day later
	txn.memo = "Updated";
	ASSERT_EQ(database->save_transaction(txn), db::error::none);
	EXPECT_EQ(86400000, fetch_field<int64_t>(connection, "SELECT date FROM transactions LIMIT 1"));
	EXPECT_EQ("Updated", fetch_field<std::string>(connection, "SELECT memo FROM transactions LIMIT 1"));
}

TEST(DbQuery, SaveTransaction_Update_ImmutableFieldChanged) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	txn.amount = currency{99999};
	EXPECT_EQ(database->save_transaction(txn), db::error::rejected);
}

TEST(DbQuery, SaveTransaction_Update_NonexistentId) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	sqlite3_exec(connection, "DELETE FROM transactions", nullptr, nullptr, nullptr);
	EXPECT_EQ(database->save_transaction(txn), db::error::bad_request);
}

TEST(DbQuery, SaveTransaction_InsertCorrection) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	transaction correction;
	correction.account_id   = checking.id();
	correction.amount       = currency{5000};
	correction.date         = datetime{0};
	correction.memo         = "Correction";
	correction.corrects_id  = txn.id();
	correction.correct_action = transaction::correction_type::replaces;
	ASSERT_EQ(database->save_transaction(correction), db::error::none);
	ASSERT_NE(correction.id(), 0);
	auto superseded_by = fetch_field<std::optional<int64_t>>(connection,
		std::format("SELECT superseded_by FROM transactions WHERE id = {}", txn.id()).c_str()
	);
	EXPECT_EQ(superseded_by, correction.id());
}

TEST(DbQuery, SaveTransaction_InsertCorrection_ParityMissing_CorrectAction) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	transaction correction;
	correction.account_id  = checking.id();
	correction.amount      = currency{5000};
	correction.date        = datetime{0};
	correction.memo        = "Bad";
	correction.corrects_id = txn.id();
	// correct_action intentionally omitted
	EXPECT_EQ(database->save_transaction(correction), db::error::bad_request);
}

TEST(DbQuery, SaveTransaction_InsertCorrection_ParityMissing_CorrectsId) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	transaction correction;
	correction.account_id     = checking.id();
	correction.amount         = currency{5000};
	correction.date           = datetime{0};
	correction.memo           = "Bad";
	correction.correct_action = transaction::correction_type::replaces;
	// corrects_id intentionally omitted
	EXPECT_EQ(database->save_transaction(correction), db::error::bad_request);
}

TEST(DbQuery, SaveTransaction_InsertCorrection_TargetHasFitid) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	std::string update = std::format(
		"UPDATE transactions SET fitid = 'imported-fitid' WHERE id = {}", txn.id()
	);
	ASSERT_EQ(sqlite3_exec(connection, update.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);
	transaction correction;
	correction.account_id     = checking.id();
	correction.amount         = currency{5000};
	correction.date           = datetime{0};
	correction.memo           = "Correction";
	correction.corrects_id    = txn.id();
	correction.correct_action = transaction::correction_type::replaces;
	EXPECT_EQ(database->save_transaction(correction), db::error::rejected);
}

TEST(DbQuery, SaveTransaction_InsertCorrection_TargetAlreadySuperseded) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	transaction superseder;
	superseder.account_id = checking.id();
	superseder.amount     = currency{5000};
	superseder.date       = datetime{0};
	superseder.memo       = "Superseder";
	ASSERT_EQ(database->save_transaction(superseder), db::error::none);
	std::string update = std::format(
		"UPDATE transactions SET superseded_by = {} WHERE id = {}", superseder.id(), txn.id()
	);
	ASSERT_EQ(sqlite3_exec(connection, update.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);
	transaction correction;
	correction.account_id     = checking.id();
	correction.amount         = currency{5000};
	correction.date           = datetime{0};
	correction.memo           = "Correction";
	correction.corrects_id    = txn.id();
	correction.correct_action = transaction::correction_type::replaces;
	EXPECT_EQ(database->save_transaction(correction), db::error::rejected);
}

TEST(DbQuery, SaveTransaction_InsertCorrection_TargetWrongAccount) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	account savings;
	savings.name = "Savings";
	ASSERT_EQ(database->save_account(savings), db::error::none);
	transaction correction;
	correction.account_id     = savings.id();
	correction.amount         = currency{5000};
	correction.date           = datetime{0};
	correction.memo           = "Correction";
	correction.corrects_id    = txn.id();
	correction.correct_action = transaction::correction_type::replaces;
	EXPECT_EQ(database->save_transaction(correction), db::error::bad_request);
}

#define FUNDOS_SEED_IMPORT() \
	FUNDOS_SEED(); \
	transaction previous_import; \
	previous_import.account_id = checking.id(); \
	previous_import.amount     = currency{5000}; \
	previous_import.date       = CLOSED_AT - timedelta::days(3); \
	previous_import.cleared    = CLOSED_AT; \
	previous_import.memo       = "Previous Import"; \
	previous_import.fitid      = std::string{"fitid-existing"}; \
	std::string previous_import_sql = std::format( \
		"INSERT INTO transactions (account_id, amount, date, cleared, memo, fitid) " \
		"VALUES ({}, {}, {}, {}, '{}', '{}')", \
		previous_import.account_id, \
		previous_import.amount.minor_units, \
		previous_import.date.milliseconds_since_epoch, \
		previous_import.cleared->milliseconds_since_epoch, \
		previous_import.memo, \
		*previous_import.fitid \
	); \
	ASSERT_EQ(sqlite3_exec(connection, previous_import_sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK); \
	int64_t previous_import_id = sqlite3_last_insert_rowid(connection); \
	import::imported_transaction matched_import; \
	matched_import.importing.fitid   = previous_import.fitid; \
	matched_import.importing.cleared = previous_import.cleared; \
	matched_import.importing.amount  = previous_import.amount; \
	matched_import.importing.date    = previous_import.date + timedelta::days(3); \
	matched_import.importing.memo    = "New Memo"; \
	import::imported_transaction fresh_import; \
	fresh_import.importing.fitid    = std::string{"fitid-new"}; \
	fresh_import.importing.cleared  = CLOSED_AT + timedelta::days(21); \
	fresh_import.importing.amount   = currency{2000}; \
	fresh_import.importing.date     = datetime{0}; \
	fresh_import.importing.memo     = "Fresh"; \
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
	ASSERT_EQ(database->prepare_import(pending), db::error::none);
	auto& matched = pending.accounts[0].transactions[0];
	EXPECT_TRUE(matched.is_definitive_match());
	EXPECT_EQ(matched.get_match()->id(), previous_import_id);
	EXPECT_EQ(matched.saving.date, previous_import.date);
	EXPECT_EQ(matched.saving.memo, previous_import.memo);
}

TEST(DbQuery, PrepareImport_NoMatch) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_EQ(database->prepare_import(pending), db::error::none);
	auto& fresh = pending.accounts[0].transactions[1];
	EXPECT_EQ(fresh.get_match(), nullptr);
	EXPECT_EQ(fresh.saving.date, fresh.importing.date);
	EXPECT_EQ(fresh.saving.memo, fresh.importing.memo);
}

TEST(DbQuery, PrepareImport_FuzzyMatch) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	// Insert a candidate with no fitid, matching amount, date within 7 days
	std::string sql = std::format(
		"INSERT INTO transactions (account_id, amount, date, memo) "
		"VALUES ({}, {}, {}, 'Fuzzy Candidate')",
		checking.id(),
		fresh_import.importing.amount.minor_units,
		fresh_import.importing.cleared->milliseconds_since_epoch - timedelta::days(3).milliseconds
	);
	ASSERT_EQ(sqlite3_exec(connection, sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);
	ASSERT_EQ(database->prepare_import(pending), db::error::none);
	auto& fresh = pending.accounts[0].transactions[1];
	EXPECT_NE(fresh.get_match(), nullptr);
	EXPECT_FALSE(fresh.is_definitive_match());
}

TEST(DbQuery, PrepareImport_MissingFitid) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	pending.accounts[0].transactions[0].importing.fitid = std::nullopt;
	EXPECT_EQ(database->prepare_import(pending), db::error::bad_request);
}

TEST(DbQuery, PrepareImport_MissingCleared) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	pending.accounts[0].transactions[0].importing.cleared = std::nullopt;
	EXPECT_EQ(database->prepare_import(pending), db::error::bad_request);
}

TEST(DbQuery, PrepareImport_UnknownAcctId) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	pending.accounts[0].acct_id = "does-not-exist";
	EXPECT_EQ(database->prepare_import(pending), db::error::rejected);
}

TEST(DbQuery, PrepareImport_DefinitiveMatchInCandidates) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_EQ(database->prepare_import(pending), db::error::none);
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
	ASSERT_EQ(database->prepare_import(pending), db::error::none);
	auto& account = pending.accounts[0];
	auto it = std::find_if(account.candidates.begin(), account.candidates.end(),
		[&](const transaction& candidate) {
			return candidate.id() == previous_import_id;
		});
	EXPECT_NE(it, account.candidates.end());
	EXPECT_EQ(it->memo, previous_import.memo);
	EXPECT_EQ(it->date, previous_import.date);
}

TEST(DbQuery, PerformImport_InsertsNewTransaction) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_EQ(database->prepare_import(pending), db::error::none);
	ASSERT_EQ(database->perform_import(pending), db::error::none);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM transactions"), 3); // txn + previous_import + fresh_import, matched_import updates previous_import
}

TEST(DbQuery, PerformImport_UpdatesMatchedTransaction) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_EQ(database->prepare_import(pending), db::error::none);
	pending.accounts[0].transactions[0].saving.memo = "Updated Memo";
	ASSERT_EQ(database->perform_import(pending), db::error::none);
	auto memo = fetch_field<std::string>(connection,
		std::format("SELECT memo FROM transactions WHERE id = {}", previous_import_id).c_str()
	);
	EXPECT_EQ(memo, "Updated Memo");
}

TEST(DbQuery, PerformImport_CreatesCheckpoint) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_EQ(database->prepare_import(pending), db::error::none);
	ASSERT_EQ(database->perform_import(pending), db::error::none);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM balance_checkpoints"), 1);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM balance_checkpoint_transactions"), 2);
}

TEST(DbQuery, PerformImport_WithoutPrepare) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	// account_id is 0, acct_id won't match what prepare would have set
	EXPECT_EQ(database->perform_import(pending), db::error::bad_request);
}

TEST(DbQuery, PerformImport_StaleMatch) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	ASSERT_EQ(database->prepare_import(pending), db::error::none);
	// Mutate the db between prepare and perform
	std::string sql = std::format(
		"UPDATE transactions SET amount = 99999 WHERE id = {}", previous_import_id
	);
	ASSERT_EQ(sqlite3_exec(connection, sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);
	EXPECT_EQ(database->perform_import(pending), db::error::bad_request);
}

TEST(DbQuery, PerformImport_ResolvesCorrections) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED_IMPORT();
	pending.accounts[0].transactions[1].importing.corrects_fitid = std::string{"fitid-existing"};
	pending.accounts[0].transactions[1].importing.correct_action = transaction::correction_type::replaces;
	ASSERT_EQ(database->prepare_import(pending), db::error::none);
	ASSERT_EQ(database->perform_import(pending), db::error::none);
	auto corrects_id = fetch_field<std::optional<int64_t>>(connection,
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

	std::vector<allocation> allocations = { alloc };
	ASSERT_EQ(database->allocate_transaction(allocations), db::error::none);
	EXPECT_NE(allocations[0].id(), 0);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM allocations"), 1);
}

TEST(DbQuery, AllocateTransaction_MultipleAllocations) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	fund rent;
	rent.name = "Rent";
	ASSERT_EQ(database->save_fund(rent), db::error::none);

	allocation alloc1;
	alloc1.transaction_id = txn.id();
	alloc1.fund_id = groceries.id();
	alloc1.amount = currency{6000};

	allocation alloc2;
	alloc2.transaction_id = txn.id();
	alloc2.fund_id = rent.id();
	alloc2.amount = currency{4000};

	std::vector<allocation> allocations = { alloc1, alloc2 };
	ASSERT_EQ(database->allocate_transaction(allocations), db::error::none);
	EXPECT_NE(allocations[0].id(), 0);
	EXPECT_NE(allocations[1].id(), 0);
	EXPECT_EQ(count_rows(connection, "SELECT COUNT(*) FROM allocations"), 2);
}

TEST(DbQuery, AllocateTransaction_UpdateExisting) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	fund rent;
	rent.name = "Rent";
	ASSERT_EQ(database->save_fund(rent), db::error::none);

	allocation alloc;
	alloc.transaction_id = txn.id();
	alloc.fund_id = groceries.id();
	alloc.amount = currency{10000};
	std::vector<allocation> allocations = { alloc };
	ASSERT_EQ(database->allocate_transaction(allocations), db::error::none);
	ASSERT_NE(allocations[0].id(), 0);
	int64_t original_id = allocations[0].id();

	// Update amount on the existing allocation
	allocations[0].fund_id = rent.id(); // same sum, different fund
	ASSERT_EQ(database->allocate_transaction(allocations), db::error::none);
	EXPECT_EQ(allocations[0].id(), original_id);
	std::string query = std::format("SELECT COUNT(*) FROM allocations WHERE fund_id = {}", rent.id());
	EXPECT_EQ(count_rows(connection, query.c_str()), 1);
}

TEST(DbQuery, AllocateTransaction_Empty) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();

	std::vector<allocation> allocations;
	EXPECT_EQ(database->allocate_transaction(allocations), db::error::bad_request);
}

TEST(DbQuery, AllocateTransaction_ZeroTransactionId) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();

	allocation alloc;
	alloc.fund_id = groceries.id();
	alloc.amount = currency{10000};
	std::vector<allocation> allocations = { alloc };
	EXPECT_EQ(database->allocate_transaction(allocations), db::error::bad_request);
}

TEST(DbQuery, AllocateTransaction_MismatchedTransactionIds) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	fund rent;
	rent.name = "Rent";
	ASSERT_EQ(database->save_fund(rent), db::error::none);

	allocation alloc1;
	alloc1.transaction_id = txn.id();
	alloc1.fund_id = groceries.id();
	alloc1.amount = currency{6000};

	allocation alloc2;
	alloc2.transaction_id = txn.id() + 1;
	alloc2.fund_id = rent.id();
	alloc2.amount = currency{4000};

	std::vector<allocation> allocations = { alloc1, alloc2 };
	EXPECT_EQ(database->allocate_transaction(allocations), db::error::bad_request);
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

	std::vector<allocation> allocations = { alloc1, alloc2 };
	EXPECT_EQ(database->allocate_transaction(allocations), db::error::bad_request);
}

TEST(DbQuery, AllocateTransaction_ZeroFundId) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();

	allocation alloc;
	alloc.transaction_id = txn.id();
	alloc.amount = currency{10000};
	std::vector<allocation> allocations = { alloc };
	EXPECT_EQ(database->allocate_transaction(allocations), db::error::bad_request);
}

TEST(DbQuery, AllocateTransaction_WrongSum) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();

	allocation alloc;
	alloc.transaction_id = txn.id();
	alloc.fund_id = groceries.id();
	alloc.amount = currency{9999};
	std::vector<allocation> allocations = { alloc };
	EXPECT_EQ(database->allocate_transaction(allocations), db::error::rejected);
}

TEST(DbQuery, AllocateTransaction_NonexistentTransaction) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();

	allocation alloc;
	alloc.transaction_id = 99999;
	alloc.fund_id = groceries.id();
	alloc.amount = currency{10000};
	std::vector<allocation> allocations = { alloc };
	EXPECT_EQ(database->allocate_transaction(allocations), db::error::rejected);
}

TEST(DbQuery, AllocateTransaction_ClosedFund) {
	FUNDOS_TEST_DB();
	FUNDOS_SEED();
	groceries.closed_at = CLOSED_AT;
	ASSERT_EQ(database->save_fund(groceries), db::error::none);

	allocation alloc;
	alloc.transaction_id = txn.id();
	alloc.fund_id = groceries.id();
	alloc.amount = currency{10000};
	std::vector<allocation> allocations = { alloc };
	EXPECT_EQ(database->allocate_transaction(allocations), db::error::rejected);
}
