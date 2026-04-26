#pragma once
#include <cstdint>
#include <optional>
#include "assert.hpp"
#include "currency_locale.hpp"

namespace fundos {

std::optional<int64_t> parse_currency(const std::string& text, const currency_locale::info& locale);
std::string format_currency(int64_t minor_units, const currency_locale::info& locale);

struct currency {
	int64_t minor_units = 0; // cents, pence, yen, etc

	static std::optional<currency> from_string(const std::string& text, const currency_locale::info& locale) {
		auto parsed = parse_currency(text, locale);
		if (!parsed) { return std::nullopt; }
		return currency{*parsed};
	}
	std::string to_string(const currency_locale::info& locale) const { return format_currency(minor_units, locale); }

	// Convert operator allows this to be used like a true primitive type
	constexpr explicit operator int64_t() const { return minor_units; }

	constexpr currency operator+(const currency& rhs) const { return { minor_units + rhs.minor_units }; }
	constexpr currency operator-(const currency& rhs) const { return { minor_units - rhs.minor_units }; }
	constexpr currency operator*(int64_t rhs) const {                                                   return { minor_units * rhs }; }
	constexpr currency operator%(int64_t rhs) const { FUNDOS_ASSERT(rhs != 0, "cannot modulo by zero"); return { minor_units % rhs }; }
	constexpr currency operator/(int64_t rhs) const { FUNDOS_ASSERT(rhs != 0, "cannot divide by zero"); return { minor_units / rhs }; }

	constexpr currency& operator+=(const currency& rhs) { minor_units += rhs.minor_units; return *this; }
	constexpr currency& operator-=(const currency& rhs) { minor_units -= rhs.minor_units; return *this; }
	constexpr currency& operator*=(int64_t rhs) {                                                   minor_units *= rhs; return *this; }
	constexpr currency& operator%=(int64_t rhs) { FUNDOS_ASSERT(rhs != 0, "cannot modulo by zero"); minor_units %= rhs; return *this; }
	constexpr currency& operator/=(int64_t rhs) { FUNDOS_ASSERT(rhs != 0, "cannot divide by zero"); minor_units /= rhs; return *this; }

	constexpr bool operator==(const currency& rhs) const { return minor_units == rhs.minor_units; }
	constexpr bool operator!=(const currency& rhs) const { return minor_units != rhs.minor_units; }
	constexpr bool operator< (const currency& rhs) const { return minor_units <  rhs.minor_units; }
	constexpr bool operator> (const currency& rhs) const { return minor_units >  rhs.minor_units; }
	constexpr bool operator<=(const currency& rhs) const { return minor_units <= rhs.minor_units; }
	constexpr bool operator>=(const currency& rhs) const { return minor_units >= rhs.minor_units; }
};

} // fundos
