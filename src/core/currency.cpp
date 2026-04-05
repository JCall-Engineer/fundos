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
	return cents < 0
		? std::format("(${}.{:02})", -cents / scale, -cents % scale)
		: std::format( "${}.{:02}",   cents / scale,  cents % scale);
}
