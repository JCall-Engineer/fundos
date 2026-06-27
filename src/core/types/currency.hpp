#pragma once
#include <cstdint>
#include <optional>
#include "platform.hpp"
#include "currency_locale.hpp"

namespace fundos {

std::optional<int64_t> parse_currency(const std::string& text, const currency_locale::spec& locale);
std::string format_currency(int64_t minor_units, const currency_locale::spec& locale);

struct currency {
	/// The amount in minor units: cents, pence, yen, etc.
	int64_t minor_units = 0;

	static std::optional<currency> from_string(const std::string& text, const currency_locale::spec& locale) {
		auto parsed = parse_currency(text, locale);
		if (!parsed) { return std::nullopt; }
		return currency{*parsed};
	}
	std::string to_string(const currency_locale::spec& locale) const { return format_currency(minor_units, locale); }

	// Convert operator allows this to be used like a true primitive type
	constexpr explicit operator int64_t() const { return minor_units; }
	constexpr currency operator-() const { return { -minor_units }; }

	constexpr currency operator+(const currency& rhs) const { return { minor_units + rhs.minor_units }; }
	constexpr currency operator-(const currency& rhs) const { return { minor_units - rhs.minor_units }; }
	constexpr currency operator*(int64_t rhs) const {                                                                             return { minor_units * rhs }; }
	constexpr currency operator%(int64_t rhs) const { FUNDOS_REQUIRE_OR_FALLBACK(rhs != 0, "cannot modulo by zero", currency{0}); return { minor_units % rhs }; }
	constexpr currency operator/(int64_t rhs) const { FUNDOS_REQUIRE_OR_FALLBACK(rhs != 0, "cannot divide by zero", currency{0}); return { minor_units / rhs }; }

	constexpr currency& operator+=(const currency& rhs) { minor_units += rhs.minor_units; return *this; }
	constexpr currency& operator-=(const currency& rhs) { minor_units -= rhs.minor_units; return *this; }
	constexpr currency& operator*=(int64_t rhs) {                                                                       minor_units *= rhs; return *this; }
	constexpr currency& operator%=(int64_t rhs) { FUNDOS_REQUIRE_OR_FALLBACK(rhs != 0, "cannot modulo by zero", *this); minor_units %= rhs; return *this; }
	constexpr currency& operator/=(int64_t rhs) { FUNDOS_REQUIRE_OR_FALLBACK(rhs != 0, "cannot divide by zero", *this); minor_units /= rhs; return *this; }

	constexpr bool operator==(const currency& rhs) const { return minor_units == rhs.minor_units; }
	constexpr bool operator!=(const currency& rhs) const { return minor_units != rhs.minor_units; }
	constexpr bool operator< (const currency& rhs) const { return minor_units <  rhs.minor_units; }
	constexpr bool operator> (const currency& rhs) const { return minor_units >  rhs.minor_units; }
	constexpr bool operator<=(const currency& rhs) const { return minor_units <= rhs.minor_units; }
	constexpr bool operator>=(const currency& rhs) const { return minor_units >= rhs.minor_units; }
};

} // namespace fundos
