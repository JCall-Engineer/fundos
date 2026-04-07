#include "currency.hpp"
#include <format>

currency currency::from_string(const std::string& text) {
	enum class state : uint8_t { sign, whole, remainder };

	int64_t sign = 1;
	int64_t whole = 0;
	int64_t remainder = 0;

	uint8_t digits = 0;
	state parse_state = state::sign;

	for (uint8_t i = 0, len = text.length(); i < len; ++i) {
		char c = text[i];
		bool is_digit = c >= '0' && c <= '9';
		int64_t digit = c - '0';

		switch (parse_state) {
			case state::sign:
				if (is_digit) { whole = digit; parse_state = state::whole; }
				else if (c == '.') { parse_state = state::remainder; }
				else if (c == '-' || c == '(') { sign = -1; }
				break;
			case state::whole:
				if (is_digit) { whole = whole * 10 + digit; }
				else if (c == '.') { parse_state = state::remainder; }
				break;
			case state::remainder:
				if (is_digit && digits < 2) {
					remainder = remainder * 10 + digit;
					++digits;
				}
				break;
		}
	}
	if (digits == 1) { remainder *= 10; }

	return { sign * (whole * scale + remainder) };
}

std::string currency::to_string() const {
	// All this ceremony is because std::format does not have a comma separator format flag...and it makes me angry

	// 2^63 - 1 = 9,223,372,036,854,775,807 (19 digits: take away 2 digits for the cents)
	// 5 commas mas => 22 characters is the maximum length for dollar amounts in a string
	char dollar_buffer[22];
	size_t buffer_n = 0;

	int64_t dollars = std::abs(cents / scale); // dividing first prevents overflow on std::abs(INT64_MIN)
	if (dollars == 0) {
		dollar_buffer[buffer_n++] = '0';
	} else {
		size_t digits = 0;
		while (dollars > 0) {
			char digit = '0' + (dollars % 10);
			dollar_buffer[buffer_n++] = digit;
			dollars /= 10;
			++digits;

			// Prevent a leading comma by ensuring there are still digits left
			if (dollars > 0 && digits % 3 == 0) {
				dollar_buffer[buffer_n++] = ',';
			}
		}
	}
	std::reverse(dollar_buffer, dollar_buffer + buffer_n);
	std::string_view dollar_str(dollar_buffer, buffer_n);

	return cents < 0
		? std::format("(${}.{:02})", dollar_str, -cents % scale)
		: std::format( "${}.{:02}",  dollar_str,  cents % scale);
}
