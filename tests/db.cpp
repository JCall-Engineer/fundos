#include <gtest/gtest.h>

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
		auto file = std::make_shared<db>(connection);
		auto& status = file->get_status();
		EXPECT_EQ(file->is_ready(), true);
		EXPECT_EQ(file->is_connected(), true);
		EXPECT_EQ(status.result, db::status::code::ok);
		EXPECT_EQ(status.schema_status, db::schema_state::created);
		EXPECT_EQ(status.sqlite3_error, db::error::none);
		EXPECT_EQ(status.is_ok(), true);
		EXPECT_EQ(status.has_error(), false);
		EXPECT_EQ(status.needs_migration(), false);
	} // file destroyed here, connection should still exist

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
		auto file = std::make_shared<db>(connection, db::owns_connection{});
		auto& status = file->get_status();
		EXPECT_EQ(file->is_ready(), true);
		EXPECT_EQ(file->is_connected(), true);
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
	auto file = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = file->get_status();
	EXPECT_EQ(file->is_ready(), false);
	EXPECT_EQ(file->is_connected(), false);
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
	auto file = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = file->get_status();
	EXPECT_EQ(file->is_ready(), false);
	EXPECT_EQ(file->is_connected(), false);
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
	auto file = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = file->get_status();
	EXPECT_EQ(file->is_ready(), false);
	EXPECT_EQ(file->is_connected(), false);
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
	auto file = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = file->get_status();
	EXPECT_EQ(file->is_ready(), false);
	EXPECT_EQ(file->is_connected(), false);
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
	auto file = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = file->get_status();
	EXPECT_EQ(file->is_ready(), false);
	EXPECT_EQ(file->is_connected(), false);
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
	auto file = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = file->get_status();
	EXPECT_EQ(file->is_ready(), false);
	EXPECT_EQ(file->is_connected(), false);
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
	auto file = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = file->get_status();
	EXPECT_EQ(file->is_ready(), false);
	EXPECT_EQ(file->is_connected(), false);
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
	auto file = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = file->get_status();
	EXPECT_EQ(file->is_ready(), false);
	EXPECT_EQ(file->is_connected(), false);
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
	auto file = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = file->get_status();
	EXPECT_EQ(file->is_ready(), false);
	EXPECT_EQ(file->is_connected(), false);
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
	auto file = std::make_shared<db>(connection, db::owns_connection{});
	auto& status = file->get_status();
	EXPECT_EQ(file->is_ready(), false);
	EXPECT_EQ(file->is_connected(), false);
	EXPECT_EQ(status.result, db::status::code::schema_error);
	EXPECT_EQ(status.schema_status, db::schema_state::newer_schema);
	EXPECT_EQ(status.sqlite3_error, db::error::none);
	EXPECT_EQ(status.is_ok(), false);
	EXPECT_EQ(status.has_error(), true);
	EXPECT_EQ(status.needs_migration(), false);
}

sqlite3* mockSchema() {
	sqlite3* connection = mockDb();
	auto file = std::make_shared<db>(connection); // ctor handles migration from fresh file to current schema
	auto& status = file->get_status();
	EXPECT_EQ(file->is_ready(), true);
	return connection; // file is destroyed, connection still exists
}

TEST(DbQuery, ReadEntities) {
	auto connection = mockSchema();
	sqlite3_exec(connection, R"sql(
		INSERT INTO users (id, name) VALUES (1, 'Alice');
		INSERT INTO users (id, name) VALUES (2, 'Bob');
		INSERT INTO funds (id, name, closed_at) VALUES (10, 'Emergency', NULL);
		INSERT INTO funds (id, name, closed_at) VALUES (11, 'Vacation', '2024-01-01');
		INSERT INTO accounts (id, name, closed_at, bank_ref, import_source) VALUES (20, 'Checking', NULL, 'ref1', 'bank');
		INSERT INTO accounts (id, name, closed_at, bank_ref, import_source) VALUES (21, 'Savings', '2024-06-01', NULL, NULL);
		INSERT INTO fund_members (fund_id, user_id) VALUES (10, 1);
		INSERT INTO fund_members (fund_id, user_id) VALUES (11, 1);
		INSERT INTO account_members (account_id, user_id) VALUES (20, 1);
	)sql", nullptr, nullptr, nullptr);

	auto file = std::make_shared<db>(connection, db::owns_connection{});
	ASSERT_EQ(file->is_ready(), true);

	{
		auto result = file->get_users();
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
		auto result = file->get_funds();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 2);
		auto emergency = (*result.val)[0];
		EXPECT_EQ(emergency.id(), 10);
		EXPECT_EQ(emergency.name, "Emergency");
		EXPECT_EQ(emergency.closed_at, std::nullopt);
		auto vacation = (*result.val)[1];
		EXPECT_EQ(vacation.id(), 11);
		EXPECT_EQ(vacation.name, "Vacation");
		EXPECT_EQ(vacation.closed_at, "2024-01-01");
	}

	{
		auto result = file->get_accounts();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 2);
		auto checking = (*result.val)[0];
		EXPECT_EQ(checking.id(), 20);
		EXPECT_EQ(checking.name, "Checking");
		EXPECT_EQ(checking.closed_at, std::nullopt);
		EXPECT_EQ(checking.bank_ref, "ref1");
		EXPECT_EQ(checking.import_source, "bank");
		auto savings = (*result.val)[1];
		EXPECT_EQ(savings.id(), 21);
		EXPECT_EQ(savings.name, "Savings");
		EXPECT_EQ(savings.closed_at, "2024-06-01");
		EXPECT_EQ(savings.bank_ref, std::nullopt);
		EXPECT_EQ(savings.import_source, std::nullopt);
	}

	{
		// Alice is member of Emergency (open), not Vacation (closed) — memberships excludes closed
		auto memberships = file->get_fund_memberships(1);
		ASSERT_TRUE(memberships.ok());
		ASSERT_EQ(memberships.val->size(), 1);
		EXPECT_EQ((*memberships.val)[0].id(), 10);
		EXPECT_EQ((*memberships.val)[0].name, "Emergency");

		// nonmemberships excludes closed funds too, so no rows return for Alice
		auto nonmemberships = file->get_fund_nonmemberships(1);
		ASSERT_TRUE(nonmemberships.ok());
		ASSERT_EQ(nonmemberships.val->size(), 0);

		// Bob is not a member of the only open fund open fund
		auto bob_nonmemberships = file->get_fund_nonmemberships(2);
		ASSERT_TRUE(bob_nonmemberships.ok());
		ASSERT_EQ(bob_nonmemberships.val->size(), 1);
		EXPECT_EQ((*bob_nonmemberships.val)[0].id(), 10);

		auto members = file->get_fund_members(10);
		ASSERT_TRUE(members.ok());
		ASSERT_EQ(members.val->size(), 1);
		EXPECT_EQ((*members.val)[0].id(), 1);
		EXPECT_EQ((*members.val)[0].name, "Alice");

		auto nonmembers = file->get_fund_nonmembers(10);
		ASSERT_TRUE(nonmembers.ok());
		ASSERT_EQ(nonmembers.val->size(), 1);
		EXPECT_EQ((*nonmembers.val)[0].id(), 2);
		EXPECT_EQ((*nonmembers.val)[0].name, "Bob");
	}

	{
		auto memberships = file->get_account_memberships(1);
		ASSERT_TRUE(memberships.ok());
		ASSERT_EQ(memberships.val->size(), 1);
		EXPECT_EQ((*memberships.val)[0].id(), 20);
		EXPECT_EQ((*memberships.val)[0].name, "Checking");
		EXPECT_EQ((*memberships.val)[0].bank_ref, "ref1");
		EXPECT_EQ((*memberships.val)[0].import_source, "bank");

		auto nonmemberships = file->get_account_nonmemberships(1);
		ASSERT_TRUE(nonmemberships.ok());
		ASSERT_EQ(nonmemberships.val->size(), 0);

		auto members = file->get_account_members(20);
		ASSERT_TRUE(members.ok());
		ASSERT_EQ(members.val->size(), 1);
		EXPECT_EQ((*members.val)[0].id(), 1);
		EXPECT_EQ((*members.val)[0].name, "Alice");

		auto nonmembers = file->get_account_nonmembers(20);
		ASSERT_TRUE(nonmembers.ok());
		ASSERT_EQ(nonmembers.val->size(), 1);
		EXPECT_EQ((*nonmembers.val)[0].id(), 2);
		EXPECT_EQ((*nonmembers.val)[0].name, "Bob");
	}
}

TEST(DbQuery, SaveEntities) {
	auto connection = mockSchema();
	auto file = std::make_shared<db>(connection, db::owns_connection{});
	ASSERT_EQ(file->is_ready(), true);

	user alice;
	alice.name = "Alice";
	ASSERT_EQ(file->save_user(alice), db::error::none);
	EXPECT_NE(alice.id(), 0);

	fund emergency;
	emergency.name = "Emergency";
	ASSERT_EQ(file->save_fund(emergency), db::error::none);
	EXPECT_NE(emergency.id(), 0);

	account checking;
	checking.name = "Checking";
	ASSERT_EQ(file->save_account(checking), db::error::none);
	EXPECT_NE(checking.id(), 0);

	{
		auto result = file->get_users();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), alice.id());
		EXPECT_EQ(row.name, alice.name);
	}

	{
		auto result = file->get_funds();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), emergency.id());
		EXPECT_EQ(row.name, emergency.name);
		EXPECT_EQ(row.closed_at, emergency.closed_at);
	}

	{
		auto result = file->get_accounts();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), checking.id());
		EXPECT_EQ(row.name, checking.name);
		EXPECT_EQ(row.closed_at, checking.closed_at);
		EXPECT_EQ(row.bank_ref, checking.bank_ref);
		EXPECT_EQ(row.import_source, checking.import_source);
	}

	// Verify update path
	alice.name = "Alicia";
	ASSERT_EQ(file->save_user(alice), db::error::none);

	{
		auto result = file->get_users();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), alice.id());
		EXPECT_EQ(row.name, "Alicia");
	}

	emergency.name = "Rainy Day";
	emergency.closed_at = "2024-06-01";
	ASSERT_EQ(file->save_fund(emergency), db::error::none);
	{
		auto result = file->get_funds();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), emergency.id());
		EXPECT_EQ(row.name, "Rainy Day");
		EXPECT_EQ(row.closed_at, "2024-06-01");
	}

	checking.name = "Debit Card";
	checking.bank_ref = "ref1";
	checking.import_source = "bank";
	checking.closed_at = "2025-06-01";
	ASSERT_EQ(file->save_account(checking), db::error::none);
	{
		auto result = file->get_accounts();
		ASSERT_TRUE(result.ok());
		ASSERT_EQ(result.val->size(), 1);
		auto& row = (*result.val)[0];
		EXPECT_EQ(row.id(), emergency.id());
		EXPECT_EQ(row.name, "Debit Card");
		EXPECT_EQ(row.bank_ref, "ref1");
		EXPECT_EQ(row.import_source, "bank");
		EXPECT_EQ(row.closed_at, "2025-06-01");
	}
}
