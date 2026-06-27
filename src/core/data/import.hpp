#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "models.hpp"

namespace fundos::import {

enum class error {
	// File-level failures: the import cannot proceed at all.
	none,
	io_error,
	bad_format,
	malformed,
};

enum class warning : uint8_t {
	// Row-level issues: the bad row is skipped/defaulted and counted here, but the import continues.
	missing_acctid,
	skipped_transaction,
	missing_fitid,
	missing_date,
	missing_amount,
	bad_date,
	bad_amount,
	bad_correction,
	NUM_WARNINGS  // sentinel: must stay last, new warnings should be added above this line
};

struct result {
	error err = error::none;
	pending_import data;
	std::array<int32_t, (size_t)warning::NUM_WARNINGS> warning_counts = {};

	result() = default;
	result(error e) { err = e; }
	void set_error(error e) { err = e; data.accounts.clear(); } // errors are file-level; a failed parse should not yield partial data
	void add_warning(warning w) { ++warning_counts[(size_t)w]; }

	bool ok() const { return err == error::none; }
};

result import_ofx(const std::string& filepath, const currency_locale::spec& locale);

} // namespace fundos::import
