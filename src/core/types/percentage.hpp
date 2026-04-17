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

template<typename Scalar>
concept SignedScalar = requires(Scalar value, int64_t factor) {
	{ value * -1 } -> std::convertible_to<Scalar>; // no mixing signed and unsigned math
	{ static_cast<int64_t>(value) };               // allow working with a raw int type
	{ Scalar{factor} };                            // allow initialization from a raw int type
};

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

	// Convert operator allows this to be used like a true primitive type
	constexpr explicit operator int64_t() const { return basis_points; }

	constexpr percentage operator+(const percentage& rhs) const { return { basis_points + rhs.basis_points }; }
	constexpr percentage operator-(const percentage& rhs) const { return { basis_points - rhs.basis_points }; }
	// Operator * % and / are ambiguous for percentage: should they return the rhs type or a percentage?

	constexpr percentage& operator+=(const percentage& rhs) { basis_points += rhs.basis_points; return *this; }
	constexpr percentage& operator-=(const percentage& rhs) { basis_points -= rhs.basis_points; return *this; }
	// Therefore consumers must rely on the more explicit scale() or extracting/mutating basis_points directly

	// Split on percentage::whole() keeps high * basis_points within int64_t for ratio <= percentage::whole()
	template<SignedScalar S>
	inline S scale(const S& value) const { // S may not be trivially copyable (it most likely is but it's not guaranteed by SignedScalar)
		constexpr int64_t split = whole().basis_points;
		assert(basis_points >= 0 && basis_points <= split && "scale() risks overflow for percentages outside [0, 1]");

		const int64_t raw = static_cast<int64_t>(value);
		int64_t high = raw / split;
		int64_t low  = raw % split;
		return { high * basis_points + (low * basis_points / split) };
	}

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

template<fundos::CurrencyLocale Locale>
constexpr fundos::currency<Locale> operator*(const fundos::currency<Locale>& monetary, const fundos::percentage& ratio) {
	return { ratio.scale(monetary.minor_units) };
}

template<fundos::CurrencyLocale Locale>
constexpr fundos::currency<Locale> operator*(const fundos::percentage& ratio, const fundos::currency<Locale>& monetary) {
	return { ratio.scale(monetary.minor_units) };
}
