#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace fundos::currency_locale {

struct spec {
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

	/// Number of minor units per major unit (e.g. 100 for cents, 1 for currencies with no minor unit).
	int16_t scale;
	std::string symbol;
	char thousands_separator;
	char decimal_separator;
	symbol_placement symbol_position;
	negative_notation negative_format;
};

struct slot {
	const char* identifier;
	spec info;
};

struct data {
	slot USD {
		"USD", {
			.scale                = 100,
			.symbol               = "$",
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = spec::symbol_placement::before,
			.negative_format      = spec::negative_notation::parentheses,
		}
	};
	slot CAD {
		"CAD", {
			.scale                = 100,
			.symbol               = "CA$",
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = spec::symbol_placement::before,
			.negative_format      = spec::negative_notation::parentheses,
		}
	};
	slot GBP {
		"GBP", {
			.scale                = 100,
			.symbol               = "\xc2\xa3", // £ in UTF-8
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = spec::symbol_placement::before,
			.negative_format      = spec::negative_notation::leading_minus,
		}
	};
	slot EUR {
		"EUR", {
			.scale                = 100,
			.symbol               = "\xe2\x82\xac", // € in UTF-8
			.thousands_separator  = '.',
			.decimal_separator    = ',',
			.symbol_position      = spec::symbol_placement::after,
			.negative_format      = spec::negative_notation::leading_minus,
		}
	};
	slot AUD {
		"AUD", {
			.scale                = 100,
			.symbol               = "A$",
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = spec::symbol_placement::before,
			.negative_format      = spec::negative_notation::parentheses,
		}
	};
	slot JPY {
		"JPY", {
			.scale                = 1, // No minor unit
			.symbol               = "\xc2\xa5", // ¥ in UTF-8
			.thousands_separator  = ',',
			.decimal_separator    = '.', // Unused given scale=1
			.symbol_position      = spec::symbol_placement::before,
			.negative_format      = spec::negative_notation::leading_minus,
		}
	};
	slot CNY {
		"CNY", {
			.scale                = 100,
			.symbol               = "CN\xc2\xa5", // CN¥ in UTF-8
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = spec::symbol_placement::before,
			.negative_format      = spec::negative_notation::leading_minus,
		}
	};
};
static constexpr std::size_t num_locales = sizeof(data) / sizeof(slot);
static_assert(sizeof(data) % sizeof(slot) == 0, "slots must contain only slot members with no padding");

/// Union of named and indexed access to the locale slots.
/// Explicit destructor required because info contains a non-trivial member (symbol), so the active union member must be explicitly destroyed.
union registry {
	slot slots[num_locales];
	data named;
	~registry() { named.~data(); }
};
extern const registry locales;

static inline std::optional<spec> get_locale(std::string_view identifier) {
	for (std::size_t i = 0; i < num_locales; i++) {
		if (locales.slots[i].identifier == identifier)
			return locales.slots[i].info;
	}
	return std::nullopt;
}

} // fundos::currency_locale
