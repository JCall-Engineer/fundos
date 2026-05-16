#pragma once
#include <cstdint>

namespace fundos {

struct datetime {
	int64_t milliseconds_since_epoch = 0;

	// Convert operator allows this to be used like a true primitive type
	constexpr explicit operator int64_t() const { return milliseconds_since_epoch; }

	constexpr bool operator==(const datetime& rhs) const { return milliseconds_since_epoch == rhs.milliseconds_since_epoch; }
	constexpr bool operator!=(const datetime& rhs) const { return milliseconds_since_epoch != rhs.milliseconds_since_epoch; }
	constexpr bool operator< (const datetime& rhs) const { return milliseconds_since_epoch <  rhs.milliseconds_since_epoch; }
	constexpr bool operator> (const datetime& rhs) const { return milliseconds_since_epoch >  rhs.milliseconds_since_epoch; }
	constexpr bool operator<=(const datetime& rhs) const { return milliseconds_since_epoch <= rhs.milliseconds_since_epoch; }
	constexpr bool operator>=(const datetime& rhs) const { return milliseconds_since_epoch >= rhs.milliseconds_since_epoch; }
};

}; // fundos
