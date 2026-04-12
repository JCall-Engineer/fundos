#include "currency.hpp"
#include <format>
#include <cassert>

namespace fundos {

int64_t parse_currency(const std::string& text, const currency_locale_info& locale) {
	enum class state : uint8_t { sign, whole, remainder };

	uint8_t max_digits = 0;
	for (int16_t s = locale.scale; s > 1; s /= 10) { ++max_digits; }

	int64_t sign = (!text.empty() && text.back() == '-') ? -1 : 1; // handle the trailing minus case, other formats for sign are handled by the state machine
	int64_t whole = 0;
	int64_t remainder = 0;

	uint8_t digits = 0;
	state parse_state = state::sign;

	// Currency strings longer than 255 characters (even 32, really - see format_currency) are considered malformed. Garbage in: garbage out
	assert(text.length() <= 255);

	for (uint8_t i = 0, len = text.length(); i < len; ++i) {
		char c = text[i];
		bool is_digit = c >= '0' && c <= '9';
		int64_t digit = c - '0';

		switch (parse_state) {
			case state::sign:
				if (is_digit) { whole = digit; parse_state = state::whole; }
				else if (c == locale.decimal_separator) { parse_state = state::remainder; }
				else if (c == '-' || c == '(' || c == '<') { sign = -1; }
				break;
			case state::whole:
				if (is_digit) { whole = whole * 10 + digit; }
				else if (c == locale.decimal_separator) { parse_state = state::remainder; }
				break;
			case state::remainder:
				if (is_digit && digits < max_digits) {
					remainder = remainder * 10 + digit;
					++digits;
				}
				break;
		}
	}
	while (digits < max_digits) { remainder *= 10; ++digits; }

	return { sign * (whole * locale.scale + remainder) };
}

inline // helper for format_currency writes the arbitrary size symbol in reverse byte order
void copy_symbol_reversed(char* buffer, size_t& buffer_n, const currency_locale_info& locale) {
	for (auto it = locale.symbol.rbegin(); it != locale.symbol.rend(); ++it) {
		buffer[buffer_n++] = *it;
	}
}

std::string format_currency(int64_t minor_units, const currency_locale_info& locale) {
	// Unfortunately, std::format is of little help here after we add thousands separators (locale-variable no less), so we (mostly) build the whole string manually

	// 19 digits max (combined between major_units and minor_units): max uint64_t = 2^63 - 1 = 9,223,372,036,854,775,807
	// 6 max thousands separators
	// 4 bytes max for utf8 symbol
	// 1 max decimal separator
	// 2 max negative symbol
	// = 32 bytes max total
	char buffer[32];
	size_t buffer_n = 0;

	bool is_negative = minor_units < 0;
	int64_t major_units = std::abs(minor_units / locale.scale); // Note: INT64_MIN with scale=1 (e.g. JPY) overflows std::abs
	minor_units         = std::abs(minor_units % locale.scale); // Values that extreme are considered malformed input, consistent with the rest of this library.

	// In order to 0 pad the minor_units and thousands-separate the major_units it is beneficial to build the string in reverse order
	if (locale.symbol_position == currency_symbol_position::after) {
		copy_symbol_reversed(buffer, buffer_n, locale);
	}

	if (locale.scale > 1) {
		uint8_t minor_scale = locale.scale / 10;
		while (minor_scale > 0) {
			buffer[buffer_n++] = '0' + (minor_units % 10);
			minor_units /= 10;
			minor_scale /= 10;
		}

		buffer[buffer_n++] = locale.decimal_separator;
	}

	if (major_units == 0) {
		buffer[buffer_n++] = '0';
	} else {
		size_t digits = 0;
		while (major_units > 0) {
			buffer[buffer_n++] = '0' + (major_units % 10);
			major_units /= 10;
			++digits;

			// Prevent a leading thousands separator by ensuring there are still digits left
			if (major_units > 0 && digits % 3 == 0) {
				buffer[buffer_n++] = locale.thousands_separator;
			}
		}
	}

	if (locale.symbol_position == currency_symbol_position::before) {
		copy_symbol_reversed(buffer, buffer_n, locale);
	}

	std::reverse(buffer, buffer + buffer_n);
	std::string_view output(buffer, buffer_n);

	// The most comprehensible part of this function lol
	if (!is_negative) { return std::string(output); }
	switch (locale.negative_format) {
		case currency_negative_format::leading_minus:
			return std::format("-{}", output);
		case currency_negative_format::trailing_minus:
			return std::format("{}-", output);
		case currency_negative_format::parentheses:
			return std::format("({})", output);
		case currency_negative_format::angle_brackets:
			return std::format("<{}>", output);
		default:
			assert(false && "unhandled currency_negative_format");
			return "";
	}
}

} // fundos
