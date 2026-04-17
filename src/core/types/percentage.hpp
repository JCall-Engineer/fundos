#pragma once
#include <cassert>
#include <optional>
#include <string>

namespace fundos {

struct percentage_locale_info {
	enum class symbol_placement : uint8_t {
		before,
		after
	};

	char decimal_separator = '.';
	bool has_space_around_number = false;
	symbol_placement symbol_position = symbol_placement::after;
};

std::optional<int32_t> parse_percentage(const std::string& text);
std::string format_percentage(int32_t basis_points, const percentage_locale_info& locale);

// In domain: values range from [0, 1], but this struct doesn't inherently limit to those values except for string conversions and multiplication operator provided
struct percentage {
	int32_t basis_points = 0; // 0.01%
	static constexpr uint32_t scale = 10000;

	static std::optional<percentage> from_string(const std::string& text) {
		auto parsed = parse_percentage(text);
		if (!parsed) { return std::nullopt; }
		return percentage{*parsed};
	}
	std::string to_string()                                     const { return format_percentage(basis_points, percentage_locale_info{}); }
	std::string to_string(const percentage_locale_info& locale) const { return format_percentage(basis_points, locale); }

	constexpr bool operator==(const percentage& rhs) const { return basis_points == rhs.basis_points; }
	constexpr bool operator!=(const percentage& rhs) const { return basis_points != rhs.basis_points; }
	constexpr bool operator< (const percentage& rhs) const { return basis_points <  rhs.basis_points; }
	constexpr bool operator> (const percentage& rhs) const { return basis_points >  rhs.basis_points; }
	constexpr bool operator<=(const percentage& rhs) const { return basis_points <= rhs.basis_points; }
	constexpr bool operator>=(const percentage& rhs) const { return basis_points >= rhs.basis_points; }
};

} // fundos

#include "types/currency.hpp"

// Overflows for currencies with abs(minor_units) > INT64_MAX / 100
template<fundos::CurrencyLocale Locale>
constexpr fundos::currency<Locale> operator*(const fundos::currency<Locale>& monetary, const fundos::percentage& ratio) {
	assert(ratio.basis_points >= 0 && "cannot multiply currency by negative percentage");

	int32_t whole     = ratio.basis_points / 100;
	int32_t remainder = ratio.basis_points % 100;

	int64_t s1 = monetary.minor_units * whole     / 100;
	int64_t s2 = monetary.minor_units * remainder / 10000;

	return { s1 + s2 };
}

template<fundos::CurrencyLocale Locale>
constexpr fundos::currency<Locale> operator*(const fundos::percentage& ratio, const fundos::currency<Locale>& monetary) {
	return monetary * ratio;
}
