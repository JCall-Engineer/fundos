#include <gtest/gtest.h>
#include "db.hpp"
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
