#pragma once
#include "currency_locale.hpp"
#include <cassert>
#include <optional>

namespace fundos {

std::optional<int64_t> parse_currency(const std::string& text, const currency_locale_info& locale);
std::string format_currency(int64_t minor_units, const currency_locale_info& locale);

template<CurrencyLocale Locale>
struct currency {
	static constexpr currency_locale_info locale = Locale::info;
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

} // fundos
