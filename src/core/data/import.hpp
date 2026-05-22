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
	error err = error::none;
	pending_import data;
	std::array<int32_t, (size_t)warning::NUM_WARNINGS> warning_counts = {};

	result() = default;
	result(error e) { err = e; }
	void set_error(error e) { err = e; data.accounts.clear(); }
	void add_warning(warning w) { ++warning_counts[(size_t)w]; }

	bool ok() const { return err == error::none; }
};

result import_ofx(const std::string& filepath, const currency_locale::spec& locale);

} // fundos::import
