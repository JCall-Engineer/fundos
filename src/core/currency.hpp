#pragma once
#include <cstdint>
#include <string>

struct currency {
	int64_t cents = 0;
	static constexpr int64_t scale = 100;

	static currency from_string(const std::string& text); // ignores non-numerical symbols, treats - and ( as negative sign
	std::string to_string() const; // renders negative values in accounting notation e.g. ($1.50)

	constexpr currency operator+(const currency& rhs) const { return { cents + rhs.cents }; }
	constexpr currency operator-(const currency& rhs) const { return { cents - rhs.cents }; }
	constexpr currency operator*(int64_t rhs) const { return { cents * rhs }; }
	constexpr currency operator/(int64_t rhs) const { return { cents / rhs }; }

	constexpr currency& operator+=(const currency& rhs) { cents += rhs.cents; return *this; }
	constexpr currency& operator-=(const currency& rhs) { cents -= rhs.cents; return *this; }
	constexpr currency& operator*=(int64_t rhs) { cents *= rhs; return *this; }
	constexpr currency& operator/=(int64_t rhs) { cents /= rhs; return *this; }

	constexpr bool operator==(const currency& rhs) const { return cents == rhs.cents; }
	constexpr bool operator!=(const currency& rhs) const { return cents != rhs.cents; }
	constexpr bool operator< (const currency& rhs) const { return cents <  rhs.cents; }
	constexpr bool operator> (const currency& rhs) const { return cents >  rhs.cents; }
	constexpr bool operator<=(const currency& rhs) const { return cents <= rhs.cents; }
	constexpr bool operator>=(const currency& rhs) const { return cents >= rhs.cents; }
};
