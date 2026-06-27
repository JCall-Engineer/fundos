#pragma once
#include <cstdint>

namespace fundos {

/// Intentionally minimal: this is an interchange type (milliseconds since epoch) for arithmetic and comparisons, not a calendar implementation.
/// Calendar logic (leap years, formatting, localization) is deferred to whichever GUI framework's datetime type this gets converted to/from (e.g. Qt's QDateTime)
/// implementing that here would mean reimplementing a calendar, which is out of scope for this library.
/// No to_string()/from_string() by design, unlike currency and percentage: those have no equivalent external library to defer to, so they own their own formatting; datetime doesn't need to.

struct timedelta {
	int64_t milliseconds = 0;

	static constexpr timedelta seconds(int64_t seconds) { return { seconds * 1000 }; }
	static constexpr timedelta minutes(int64_t minutes) { return seconds(minutes * 60); }
	static constexpr timedelta hours  (int64_t hours)   { return minutes(hours * 60); }
	static constexpr timedelta days   (int64_t days)    { return hours(days * 24); }

	// Convert operator allows this to be used like a true primitive type
	constexpr explicit operator int64_t() const { return milliseconds; }

	/// Returns the absolute value as a timedelta
	/// (kept as timedelta rather than a raw int64_t so it can be compared directly against things like timedelta::days(7)).
	/// Negating INT64_MIN is signed overflow (UB) same as currency.cpp's std::abs(INT64_MIN) case;
	/// accepted here for the same reason: a timedelta that extreme is not a value this library expects to see.
	constexpr timedelta magnitude() const {
		if (milliseconds < 0) {
			return { -milliseconds };
		}
		return { milliseconds };
	}

	constexpr bool operator==(const timedelta& rhs) const { return milliseconds == rhs.milliseconds; }
	constexpr bool operator!=(const timedelta& rhs) const { return milliseconds != rhs.milliseconds; }
	constexpr bool operator< (const timedelta& rhs) const { return milliseconds <  rhs.milliseconds; }
	constexpr bool operator> (const timedelta& rhs) const { return milliseconds >  rhs.milliseconds; }
	constexpr bool operator<=(const timedelta& rhs) const { return milliseconds <= rhs.milliseconds; }
	constexpr bool operator>=(const timedelta& rhs) const { return milliseconds >= rhs.milliseconds; }
};

struct datetime {
	int64_t milliseconds_since_epoch = 0;

	// Convert operator allows this to be used like a true primitive type
	constexpr explicit operator int64_t() const { return milliseconds_since_epoch; }

	constexpr datetime operator+(const timedelta& delta) const { return { milliseconds_since_epoch + delta.milliseconds }; }
	constexpr datetime operator-(const timedelta& delta) const { return { milliseconds_since_epoch - delta.milliseconds }; }
	constexpr timedelta operator-(const datetime& rhs) const { return { milliseconds_since_epoch - rhs.milliseconds_since_epoch }; }

	constexpr bool operator==(const datetime& rhs) const { return milliseconds_since_epoch == rhs.milliseconds_since_epoch; }
	constexpr bool operator!=(const datetime& rhs) const { return milliseconds_since_epoch != rhs.milliseconds_since_epoch; }
	constexpr bool operator< (const datetime& rhs) const { return milliseconds_since_epoch <  rhs.milliseconds_since_epoch; }
	constexpr bool operator> (const datetime& rhs) const { return milliseconds_since_epoch >  rhs.milliseconds_since_epoch; }
	constexpr bool operator<=(const datetime& rhs) const { return milliseconds_since_epoch <= rhs.milliseconds_since_epoch; }
	constexpr bool operator>=(const datetime& rhs) const { return milliseconds_since_epoch >= rhs.milliseconds_since_epoch; }
};

} // namespace fundos
