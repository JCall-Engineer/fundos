#include "percentage.hpp"

namespace fundos {

std::optional<int32_t> parse_percentage(const std::string& text) {
	enum class state : uint8_t { sign, whole, remainder };

	constexpr uint8_t max_decimals = 2;

	int32_t sign = 1;
	int32_t whole = 0;
	int32_t remainder = 0;

	uint8_t decimals = 0;
	state parse_state = state::sign;

	// percentage strings longer than 255 characters (even 9, really - see format_percentage) are considered malformed.
	if (text.length() > 255) { return std::nullopt; }

	for (uint8_t i = 0, len = text.length(); i < len; ++i) {
		char c = text[i];
		bool is_decimal_separator = c == '.' || c == ',';
		bool is_digit = c >= '0' && c <= '9';
		int32_t digit = c - '0';

		switch (parse_state) {
			case state::sign:
				if (is_digit) { whole = digit; parse_state = state::whole; }
				else if (is_decimal_separator) { parse_state = state::remainder; }
				else if (c == '-') { sign = -1; }
				break;
			case state::whole:
				if (is_digit) {
					whole = whole * 10 + digit;
					if (whole > 100) { return std::nullopt; }
				}
				else if (is_decimal_separator) { parse_state = state::remainder; }
				break;
			case state::remainder:
				if (is_digit && decimals < max_decimals) {
					remainder = remainder * 10 + digit; // max_decimals guarantees this doesn't overflow
					++decimals;
				}
				break;
		}
	}
	while (decimals < max_decimals) { remainder *= 10; ++decimals; } // max_decimals guarantees this doesn't overflow
	if (whole == 100 && remainder > 0) { return std::nullopt; }

	return { sign * (whole * 100 + remainder) };
}

std::string format_percentage(int32_t basis_points, const percentage_locale_info& locale) {
	// Unfortunately, std::format is of little help here after we add locale-variables, so we build the whole string manually

	// 5 digits max (3 whole, 2 decimal for 100%)
	// 1 optional decimal separator character
	// 1 % symbol
	// 1 optional space between symbol and number depending on locale
	// 1 optional - sign
	// = 9 bytes max total
	char buffer[9];
	size_t buffer_n = 0;

	bool    is_negative   =          basis_points < 0;
	int32_t whole_percent = std::abs(basis_points / 100);
	        basis_points  = std::abs(basis_points % 100);

	if (locale.symbol_position == percentage_locale_info::symbol_placement::before) {
		buffer[buffer_n++] = '%';
		if (locale.has_space_around_number) {
			buffer[buffer_n++] = ' ';
		}
	}

	if (is_negative) {
		buffer[buffer_n++] = '-';
	}

	if (whole_percent == 100) {
		buffer[buffer_n++] = '1';
	}
	buffer[buffer_n++] = '0' + (whole_percent / 10) % 10;
	buffer[buffer_n++] = '0' +  whole_percent % 10;

	buffer[buffer_n++] = locale.decimal_separator;

	buffer[buffer_n++] = '0' + basis_points / 10;
	buffer[buffer_n++] = '0' + basis_points % 10;

	if (locale.symbol_position == percentage_locale_info::symbol_placement::after) {
		if (locale.has_space_around_number) {
			buffer[buffer_n++] = ' ';
		}
		buffer[buffer_n++] = '%';
	}

	return std::string(buffer, buffer_n);
}

} // fundos
