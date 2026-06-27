#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

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
	/// Invariant: must be a power of 10 (1, 10, 100, 1000, ...). Enforced by the database and GUI layers;
	/// parse_currency/format_currency assume it and will misbehave silently if violated.
	int16_t scale;
	std::string symbol;
	char thousands_separator;
	char decimal_separator;
	symbol_placement symbol_position;
	negative_notation negative_format;
};

struct currency_locale_entry {
	const char* identifier;
	spec info;
};

/// The unit of exchange between the database and GUI for locale data:
/// the DB persists a selection and returns a selection, which the GUI consumes directly without needing to know which case it's in.
/// Wraps either a pointer to a known locale (so registry updates are reflected automatically) or a one-off custom spec.
/// identifier() doubles as the DB's serialization discriminator: a preset's identifier (e.g. "USD") means look it up by name;
/// "Custom" means reconstruct the spec field-by-field from individual meta settings instead.
struct selection {
	std::variant<const currency_locale_entry*, spec> raw;

	selection(const currency_locale_entry* entry) : raw(entry)  {}
	selection(const spec& custom)                 : raw(custom) {}

	static constexpr const char* custom_id = "Custom";
	const char* identifier() const {
		if (std::holds_alternative<const currency_locale_entry*>(raw)) {
			return std::get<const currency_locale_entry*>(raw)->identifier;
		}
		return custom_id;
	 }
	 const spec& info() const {
		if (std::holds_alternative<const currency_locale_entry*>(raw)) {
			return std::get<const currency_locale_entry*>(raw)->info;
		}
		return std::get<spec>(raw);
	 }

	 bool is_preset() const { return std::holds_alternative<const currency_locale_entry*>(raw); }
	 bool is_custom() const { return !is_preset(); }
};

struct data {
	currency_locale_entry USD {
		"USD", {
			.scale                = 100,
			.symbol               = "$",
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = spec::symbol_placement::before,
			.negative_format      = spec::negative_notation::parentheses,
		}
	};
	currency_locale_entry CAD {
		"CAD", {
			.scale                = 100,
			.symbol               = "CA$",
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = spec::symbol_placement::before,
			.negative_format      = spec::negative_notation::parentheses,
		}
	};
	currency_locale_entry GBP {
		"GBP", {
			.scale                = 100,
			.symbol               = "\xc2\xa3", // £ in UTF-8
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = spec::symbol_placement::before,
			.negative_format      = spec::negative_notation::leading_minus,
		}
	};
	currency_locale_entry EUR {
		"EUR", {
			.scale                = 100,
			.symbol               = "\xe2\x82\xac", // € in UTF-8
			.thousands_separator  = '.',
			.decimal_separator    = ',',
			.symbol_position      = spec::symbol_placement::after,
			.negative_format      = spec::negative_notation::leading_minus,
		}
	};
	currency_locale_entry AUD {
		"AUD", {
			.scale                = 100,
			.symbol               = "A$",
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = spec::symbol_placement::before,
			.negative_format      = spec::negative_notation::parentheses,
		}
	};
	currency_locale_entry JPY {
		"JPY", {
			.scale                = 1, // No minor unit
			.symbol               = "\xc2\xa5", // ¥ in UTF-8
			.thousands_separator  = ',',
			.decimal_separator    = '.', // Unused given scale=1
			.symbol_position      = spec::symbol_placement::before,
			.negative_format      = spec::negative_notation::leading_minus,
		}
	};
	currency_locale_entry CNY {
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
static constexpr std::size_t num_locales = sizeof(data) / sizeof(currency_locale_entry);
static_assert(sizeof(data) % sizeof(currency_locale_entry) == 0, "currency locale data must contain only entry members with no padding");

/// Allows both named (locales.USD) and indexed (locales.entries[i]) access to the same locale data without duplicating it.
/// Relies on `data`'s members being identically-typed entries with no padding between them (enforced by the static_assert above)
/// if that assumption breaks, entries[] silently reads garbage or misaligned data instead of failing to compile.
/// Explicit destructor required because info contains a non-trivial member (symbol), so the active union member must be explicitly destroyed.
union registry {
	currency_locale_entry entries[num_locales];
	data named;
	~registry() { named.~data(); } // non-trivial because spec::symbol is a std::string; see locale.cpp for the full tradeoff this forces
};
extern const registry locales;

static inline const currency_locale_entry* get_locale(std::string_view identifier) {
	for (std::size_t i = 0; i < num_locales; i++) {
		if (locales.entries[i].identifier == identifier)
			return &locales.entries[i];
	}
	return nullptr;
}

} // namespace fundos::currency_locale
