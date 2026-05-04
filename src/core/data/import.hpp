#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>
#include "models.hpp"

namespace fundos::import {

enum class error {
	none,
	io_error,
	bad_format,
	malformed,
};

enum class warning : uint8_t {
	missing_acctid,
	skipped_transaction,
	missing_fitid,
	missing_date,
	missing_amount,
	bad_date,
	bad_amount,
	bad_correction,
	NUM_WARNINGS
};

struct result {
	struct bank_account {
		std::string ref;
		std::vector<transaction> transactions;
	};
	error err = error::none;
	std::vector<bank_account> accounts;
	std::array<int32_t, (size_t)warning::NUM_WARNINGS> warning_counts = {};

	result() = default;
	result(error error) { err = error; }
	void error(error e) { err = e; accounts.clear(); }
	void warn(warning w) { ++warning_counts[(size_t)w]; }

	bool ok() const { return err == error::none; }
};

result import_ofx(const std::string& filepath, const currency_locale::info& locale);

}; // fundos::import
