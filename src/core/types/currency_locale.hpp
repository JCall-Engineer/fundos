#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace fundos::currency_locale {

struct info {
	enum class symbol_placement : uint8_t {
		before,
		after
	};
	enum class negative_notation : uint8_t {
		leading_minus,
		trailing_minus,
		parentheses,
		angle_brackets,
	};

	int16_t scale;
	std::string symbol;
	char thousands_separator;
	char decimal_separator;
	symbol_placement symbol_position;
	negative_notation negative_format;
};

struct slot {
	const char* identifier;
	info info;
};

struct data {
	slot USD {
		"USD", {
			.scale                = 100,
			.symbol               = "$",
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = info::symbol_placement::before,
			.negative_format      = info::negative_notation::parentheses,
		}
	};
	slot CAD {
		"CAD", {
			.scale                = 100,
			.symbol               = "CA$",
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = info::symbol_placement::before,
			.negative_format      = info::negative_notation::parentheses,
		}
	};
	slot GBP {
		"GBP", {
			.scale                = 100,
			.symbol               = "\xc2\xa3", // £ in UTF-8
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = info::symbol_placement::before,
			.negative_format      = info::negative_notation::leading_minus,
		}
	};
	slot EUR {
		"EUR", {
			.scale                = 100,
			.symbol               = "\xe2\x82\xac", // € in UTF-8
			.thousands_separator  = '.',
			.decimal_separator    = ',',
			.symbol_position      = info::symbol_placement::after,
			.negative_format      = info::negative_notation::leading_minus,
		}
	};
	slot AUD {
		"AUD", {
			.scale                = 100,
			.symbol               = "A$",
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = info::symbol_placement::before,
			.negative_format      = info::negative_notation::parentheses,
		}
	};
	slot JPY {
		"JPY", {
			.scale                = 1, // No minor unit
			.symbol               = "\xc2\xa5", // ¥ in UTF-8
			.thousands_separator  = ',',
			.decimal_separator    = '.', // Unused given scale=1
			.symbol_position      = info::symbol_placement::before,
			.negative_format      = info::negative_notation::leading_minus,
		}
	};
	slot CNY {
		"CNY", {
			.scale                = 100,
			.symbol               = "CN\xc2\xa5", // CN¥ in UTF-8
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = info::symbol_placement::before,
			.negative_format      = info::negative_notation::leading_minus,
		}
	};
};
static constexpr std::size_t num_locales = sizeof(data) / sizeof(slot);
static_assert(sizeof(data) % sizeof(slot) == 0, "slots must contain only slot members with no padding");

union registry {
	slot slots[num_locales];
	data named;
	~registry() { named.~data(); }
};
extern const registry locales;

static inline std::optional<info> get_locale(std::string_view identifier) {
	for (std::size_t i = 0; i < num_locales; i++) {
		if (locales.slots[i].identifier == identifier)
			return locales.slots[i].info;
	}
	return std::nullopt;
}

} // fundos::currency_locale
