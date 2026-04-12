#pragma once
#include "sqlite3.h"
#include <memory>
#include <string>

namespace fundos {

struct db_prepared_statements;

class db {
public:
	static std::shared_ptr<db> open_file(std::string);
	static std::shared_ptr<db> open_memory();
	explicit db(sqlite3*);
	~db();

	// no copy, no move — connection lifetime is explicit
	db(const db&) = delete;
	db& operator=(const db&) = delete;

private:
	sqlite3* connection;
	db_prepared_statements* prepared;
};

} // fundos
