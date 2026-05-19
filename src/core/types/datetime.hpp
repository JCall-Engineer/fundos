#pragma once
#include <cstdint>

namespace fundos {

struct timedelta {
	int64_t milliseconds = 0;

	static constexpr timedelta seconds(int64_t seconds) { return { seconds * 1000 }; }
	static constexpr timedelta minutes(int64_t minutes) { return seconds(minutes * 60); }
	static constexpr timedelta hours  (int64_t hours)   { return minutes(hours * 60); }
	static constexpr timedelta days   (int64_t days)    { return hours(days * 24); }

	// Convert operator allows this to be used like a true primitive type
	constexpr explicit operator int64_t() const { return milliseconds; }

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

} // fundos
