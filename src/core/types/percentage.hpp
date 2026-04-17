#pragma once
#include <cassert>
#include <cstdint>
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
	static constexpr percentage whole() { return percentage{10000}; }

	static std::optional<percentage> from_string(const std::string& text) {
		auto parsed = parse_percentage(text);
		if (!parsed) { return std::nullopt; }
		return percentage{*parsed};
	}
	std::string to_string()                                     const { return format_percentage(basis_points, percentage_locale_info{}); }
	std::string to_string(const percentage_locale_info& locale) const { return format_percentage(basis_points, locale); }

	constexpr percentage operator+(const percentage& rhs) const { return { basis_points + rhs.basis_points }; }
	constexpr percentage operator-(const percentage& rhs) const { return { basis_points - rhs.basis_points }; }
	constexpr percentage operator*(int32_t rhs) const {                   return { basis_points * rhs }; }
	constexpr percentage operator/(int32_t rhs) const { assert(rhs != 0); return { basis_points / rhs }; }

	constexpr percentage& operator+=(const percentage& rhs) { basis_points += rhs.basis_points; return *this; }
	constexpr percentage& operator-=(const percentage& rhs) { basis_points -= rhs.basis_points; return *this; }
	constexpr percentage& operator*=(int32_t rhs) {                   basis_points *= rhs; return *this; }
	constexpr percentage& operator/=(int32_t rhs) { assert(rhs != 0); basis_points /= rhs; return *this; }

	constexpr bool operator==(const percentage& rhs) const { return basis_points == rhs.basis_points; }
	constexpr bool operator!=(const percentage& rhs) const { return basis_points != rhs.basis_points; }
	constexpr bool operator< (const percentage& rhs) const { return basis_points <  rhs.basis_points; }
	constexpr bool operator> (const percentage& rhs) const { return basis_points >  rhs.basis_points; }
	constexpr bool operator<=(const percentage& rhs) const { return basis_points <= rhs.basis_points; }
	constexpr bool operator>=(const percentage& rhs) const { return basis_points >= rhs.basis_points; }
};

} // fundos

constexpr fundos::percentage operator""_percent(unsigned long long percent) {
	return fundos::percentage{static_cast<int32_t>(percent) * 100};
}

constexpr fundos::percentage operator""_percent(long double percent) {
	return fundos::percentage{static_cast<int32_t>(percent * 100.0L)};
}

#include "types/currency.hpp"

// Split on percentage::whole() keeps high * basis_points within int64_t for ratio <= percentage::whole()
template<fundos::CurrencyLocale Locale>
constexpr fundos::currency<Locale> operator*(const fundos::currency<Locale>& monetary, const fundos::percentage& ratio) {
	assert(ratio.basis_points >= 0 && "cannot multiply currency by negative percentage");

	constexpr int64_t split = fundos::percentage::whole().basis_points;
	int64_t high = monetary.minor_units / split;
	int64_t low  = monetary.minor_units % split;
	return { high * ratio.basis_points + (low * ratio.basis_points / split) };
}

template<fundos::CurrencyLocale Locale>
constexpr fundos::currency<Locale> operator*(const fundos::percentage& ratio, const fundos::currency<Locale>& monetary) {
	return monetary * ratio;
}
