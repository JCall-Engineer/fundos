#pragma once
#include <cstdint>
#include <string>

namespace fundos {

struct currency_locale_info {
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
	std::string_view symbol;
	char thousands_separator;
	char decimal_separator;
	symbol_placement symbol_position;
	negative_notation negative_format;
};

template<typename T>
concept CurrencyLocale = requires {
	{ T::info } -> std::convertible_to<currency_locale_info>;
};

namespace currency_locale {

struct USD {
	static constexpr currency_locale_info info = {
		.scale                = 100,
		.symbol               = "$",
		.thousands_separator  = ',',
		.decimal_separator    = '.',
		.symbol_position      = currency_locale_info::symbol_placement::before,
		.negative_format      = currency_locale_info::negative_notation::parentheses,
	};
};
struct CAD {
	static constexpr currency_locale_info info = {
		.scale                = 100,
		.symbol               = "CA$",
		.thousands_separator  = ',',
		.decimal_separator    = '.',
		.symbol_position      = currency_locale_info::symbol_placement::before,
		.negative_format      = currency_locale_info::negative_notation::parentheses,
	};
};
struct GBP {
	static constexpr currency_locale_info info = {
		.scale                = 100,
		.symbol               = "\xc2\xa3", // £ in UTF-8
		.thousands_separator  = ',',
		.decimal_separator    = '.',
		.symbol_position      = currency_locale_info::symbol_placement::before,
		.negative_format      = currency_locale_info::negative_notation::leading_minus,
	};
};
struct EUR {
	static constexpr currency_locale_info info = {
		.scale                = 100,
		.symbol               = "\xe2\x82\xac", // € in UTF-8
		.thousands_separator  = '.',
		.decimal_separator    = ',',
		.symbol_position      = currency_locale_info::symbol_placement::after,
		.negative_format      = currency_locale_info::negative_notation::leading_minus,
	};
};
struct AUD {
	static constexpr currency_locale_info info = {
		.scale                = 100,
		.symbol               = "A$",
		.thousands_separator  = ',',
		.decimal_separator    = '.',
		.symbol_position      = currency_locale_info::symbol_placement::before,
		.negative_format      = currency_locale_info::negative_notation::parentheses,
	};
};
struct JPY {
	static constexpr currency_locale_info info = {
		.scale                = 1, // No minor unit
		.symbol               = "\xc2\xa5", // ¥ in UTF-8
		.thousands_separator  = ',',
		.decimal_separator    = '.', // Unused given scale=1
		.symbol_position      = currency_locale_info::symbol_placement::before,
		.negative_format      = currency_locale_info::negative_notation::leading_minus,
	};
};
struct CNY {
	static constexpr currency_locale_info info = {
		.scale                = 100,
		.symbol               = "CN\xc2\xa5", // CN¥ in UTF-8
		.thousands_separator  = ',',
		.decimal_separator    = '.',
		.symbol_position      = currency_locale_info::symbol_placement::before,
		.negative_format      = currency_locale_info::negative_notation::leading_minus,
	};
};

} // currency_locale

} // fundos
