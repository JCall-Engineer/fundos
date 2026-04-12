#pragma once
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>

namespace fundos {

enum class currency_negative_format : uint8_t {
	leading_minus,
	trailing_minus,
	parentheses,
	angle_brackets,
};

enum class currency_symbol_position : uint8_t {
	before,
	after
};

struct currency_locale_info {
	int16_t scale;
	std::string_view symbol;
	char thousands_separator;
	char decimal_separator;
	currency_symbol_position symbol_position;
	currency_negative_format negative_format;
};

template<typename T>
concept Locale = requires {
	{ T::info } -> std::convertible_to<currency_locale_info>;
};

std::optional<int64_t> parse_currency(const std::string& text, const currency_locale_info& locale);
std::string format_currency(int64_t minor_units, const currency_locale_info& locale);

template<Locale L>
struct currency {
	static constexpr currency_locale_info locale = L::info;
	int64_t minor_units = 0; // cents, pence, yen, etc

	static std::optional<currency> from_string(const std::string& text) {
		auto parsed = parse_currency(text, locale);
		if (!parsed) { return std::nullopt; }
		return currency{*parsed};
	}
	std::string to_string() const { return format_currency(minor_units, locale); }

	constexpr currency operator+(const currency& rhs) const { return { minor_units + rhs.minor_units }; }
	constexpr currency operator-(const currency& rhs) const { return { minor_units - rhs.minor_units }; }
	constexpr currency operator*(int64_t rhs) const {                   return { minor_units * rhs }; }
	constexpr currency operator/(int64_t rhs) const { assert(rhs != 0); return { minor_units / rhs }; }

	constexpr currency& operator+=(const currency& rhs) { minor_units += rhs.minor_units; return *this; }
	constexpr currency& operator-=(const currency& rhs) { minor_units -= rhs.minor_units; return *this; }
	constexpr currency& operator*=(int64_t rhs) {                   minor_units *= rhs; return *this; }
	constexpr currency& operator/=(int64_t rhs) { assert(rhs != 0); minor_units /= rhs; return *this; }

	constexpr bool operator==(const currency& rhs) const { return minor_units == rhs.minor_units; }
	constexpr bool operator!=(const currency& rhs) const { return minor_units != rhs.minor_units; }
	constexpr bool operator< (const currency& rhs) const { return minor_units <  rhs.minor_units; }
	constexpr bool operator> (const currency& rhs) const { return minor_units >  rhs.minor_units; }
	constexpr bool operator<=(const currency& rhs) const { return minor_units <= rhs.minor_units; }
	constexpr bool operator>=(const currency& rhs) const { return minor_units >= rhs.minor_units; }
};

namespace currency_locale {
	struct USD {
		static constexpr currency_locale_info info = {
			.scale                = 100,
			.symbol               = "$",
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = currency_symbol_position::before,
			.negative_format      = currency_negative_format::parentheses,
		};
	};
	struct CAD {
		static constexpr currency_locale_info info = {
			.scale                = 100,
			.symbol               = "CA$",
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = currency_symbol_position::before,
			.negative_format      = currency_negative_format::parentheses,
		};
	};
	struct GBP {
		static constexpr currency_locale_info info = {
			.scale                = 100,
			.symbol               = "\xc2\xa3", // £ in UTF-8
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = currency_symbol_position::before,
			.negative_format      = currency_negative_format::leading_minus,
		};
	};
	struct EUR {
		static constexpr currency_locale_info info = {
			.scale                = 100,
			.symbol               = "\xe2\x82\xac", // € in UTF-8
			.thousands_separator  = '.',
			.decimal_separator    = ',',
			.symbol_position      = currency_symbol_position::after,
			.negative_format      = currency_negative_format::leading_minus,
		};
	};
	struct AUD {
		static constexpr currency_locale_info info = {
			.scale                = 100,
			.symbol               = "A$",
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = currency_symbol_position::before,
			.negative_format      = currency_negative_format::parentheses,
		};
	};
	struct JPY {
		static constexpr currency_locale_info info = {
			.scale                = 1, // No minor unit
			.symbol               = "\xc2\xa5", // ¥ in UTF-8
			.thousands_separator  = ',',
			.decimal_separator    = '.', // Unused given scale=1
			.symbol_position      = currency_symbol_position::before,
			.negative_format      = currency_negative_format::leading_minus,
		};
	};
	struct CNY {
		static constexpr currency_locale_info info = {
			.scale                = 100,
			.symbol               = "CN\xc2\xa5", // CN¥ in UTF-8
			.thousands_separator  = ',',
			.decimal_separator    = '.',
			.symbol_position      = currency_symbol_position::before,
			.negative_format      = currency_negative_format::leading_minus,
		};
	};
} // currency_locale

} // fundos
