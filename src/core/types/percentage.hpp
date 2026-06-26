#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include "platform.hpp"
#include "percentage_locale.hpp"

namespace fundos {

std::optional<int32_t> parse_percentage(const std::string& text);
std::string format_percentage(int32_t basis_points, const percentage_locale::spec& locale);

template<typename Scalar>
concept SignedScalar = requires(Scalar value, int64_t factor) {
	{ value * -1 } -> std::convertible_to<Scalar>; // no mixing signed and unsigned math
	{ static_cast<int64_t>(value) };               // allow working with a raw int type
	{ Scalar{factor} };                            // allow initialization from a raw int type
};

/// Represents a percentage as basis points (1 basis point = 0.01%).
/// Values are not inherently limited to [0, 1]; string conversions and scale() assume that range.
struct percentage {
	/// 1 basis point = 0.01%
	int32_t basis_points = 0;

	/// Returns 100% as a percentage (10000 basis points).
	static constexpr percentage whole() { return percentage{10000}; }

	static std::optional<percentage> from_string(const std::string& text) {
		auto parsed = parse_percentage(text);
		if (!parsed) { return std::nullopt; }
		return percentage{*parsed};
	}
	std::string to_string(const percentage_locale::spec& locale) const { return format_percentage(basis_points, locale); }

	// Convert operator allows this to be used like a true primitive type
	constexpr explicit operator int64_t() const { return basis_points; }
	constexpr percentage operator-() const { return { -basis_points }; }

	constexpr percentage operator+(const percentage& rhs) const { return { basis_points + rhs.basis_points }; }
	constexpr percentage operator-(const percentage& rhs) const { return { basis_points - rhs.basis_points }; }
	// Operator * % and / are ambiguous for percentage: should they return the rhs type or a percentage?

	constexpr percentage& operator+=(const percentage& rhs) { basis_points += rhs.basis_points; return *this; }
	constexpr percentage& operator-=(const percentage& rhs) { basis_points -= rhs.basis_points; return *this; }
	// Therefore consumers must rely on the more explicit scale() or extracting/mutating basis_points directly

	/// Multiplies value by this percentage without overflow, provided the percentage is within [0, 1].
	/// @note Accepts by const& as S is not guaranteed by SignedScalar to be trivially copyable.
	/// @param value The value to scale; asserts this percentage is within [0, 1] to prevent integer overflow.
	/// @return value scaled by this percentage.
	template<SignedScalar S>
	constexpr S scale(const S& value) const {
		// Split on percentage::whole() keeps high * basis_points within int64_t for ratio <= percentage::whole()
		constexpr int64_t split = whole().basis_points;
		FUNDOS_ASSERT(basis_points >= 0 && basis_points <= split, "scale() risks overflow for percentages outside [0, 1]");

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

} // namespace fundos

/// Constructs a percentage from a whole number (e.g. 50_percent = 50%).
constexpr fundos::percentage operator""_percent(unsigned long long percent) {
	return fundos::percentage{static_cast<int32_t>(percent) * 100};
}

/// Constructs a percentage from a decimal (e.g. 0.5_percent = 0.5%).
constexpr fundos::percentage operator""_percent(long double percent) {
	return fundos::percentage{static_cast<int32_t>(percent * 100.0L)};
}

#include "types/currency.hpp"

constexpr fundos::currency operator*(const fundos::currency& monetary, const fundos::percentage& ratio) {
	return { ratio.scale(monetary.minor_units) };
}

constexpr fundos::currency operator*(const fundos::percentage& ratio, const fundos::currency& monetary) {
	return { ratio.scale(monetary.minor_units) };
}
