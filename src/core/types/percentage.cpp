#include "percentage.hpp"

namespace fundos {

std::optional<int32_t> parse_percentage(const std::string& text) {
	enum class state : uint8_t { whole, remainder };

	constexpr uint8_t max_decimals = 2;

	// Unlike parse_currency, there is no sign handling: this parser is best-effort and simply skips any
	// character it doesn't recognize, including '-'. For percentages this is a deliberate design choice,
	// not a gap: it makes negative values unrepresentable to the end user, and the stripped sign gives
	// immediate visual feedback (e.g. "-50" displays back as "50") that the input was rejected.
	// percentage::scale() relies on this to assume non-negative basis_points from user input; see its comment.

	int32_t whole = 0;
	int32_t remainder = 0;

	uint8_t decimals = 0;
	state parse_state = state::whole;

	// percentage strings longer than 255 characters (even 9, really - see format_percentage) are considered malformed.
	if (text.length() > 255) { return std::nullopt; }

	for (uint8_t i = 0, len = static_cast<uint8_t>(text.length()); i < len; ++i) {
		char c = text[i];
		bool is_decimal_separator = c == '.' || c == ',';
		bool is_digit = c >= '0' && c <= '9';
		int32_t digit = c - '0';

		switch (parse_state) {
			case state::whole:
				if (is_digit) {
					whole = whole * 10 + digit;
					if (whole > 100) { return std::nullopt; } // percentages are capped at 100% for this app's domain; see note at the 100.xx check below
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

	// Together with the `whole > 100` check above, this caps parsed percentages at exactly 100.00%.
	// This is a parsing-layer policy, not a constraint of the percentage type itself: percentage supports values outside [0, 1] (see scale()'s docstring).
	// The cap exists because values above 100% are not meaningful in this app's domain (e.g. allocations can't exceed the whole);
	// parse_percentage should not be assumed to enforce this if reused in a domain where >100% is valid.
	if (whole == 100 && remainder > 0) { return std::nullopt; }

	return { whole * 100 + remainder };
}

std::string format_percentage(int32_t basis_points, const percentage_locale::spec& locale) {
	// Unfortunately, std::format is of little help here after we add locale-variables, so we build the whole string manually

	// 5 digits max (3 whole, 2 decimal for 100%)
	// 1 optional decimal separator character
	// 1 % symbol
	// 1 optional space between symbol and number depending on locale
	// 1 optional - sign
	// = 9 bytes max total
	char buffer[9];
	size_t buffer_n = 0;

	bool is_negative = basis_points < 0;
	if (is_negative) { basis_points = -basis_points; }

	// Grab each digit
	struct { uint8_t hundreds, tens, ones, tenths, hundredths; } digit = {
		(uint8_t)( basis_points / 10000),
		(uint8_t)((basis_points % 10000) / 1000),
		(uint8_t)((basis_points % 1000)  / 100),
		(uint8_t)((basis_points % 100)   / 10),
		(uint8_t)( basis_points % 10),
	};

	auto digit_char = [](uint8_t digit) { return static_cast<char>('0' + digit); };
	auto push = [&buffer, &buffer_n](char character) {
		buffer[buffer_n++] = character;
	};

	if (locale.symbol_position == percentage_locale::spec::symbol_placement::before) {
		push('%');
		if (locale.has_space_around_number) { push(' '); }
	}
	if (is_negative)                      { push('-'); }
	if (digit.hundreds)                   { push(digit_char(digit.hundreds)); }
	if (digit.hundreds || digit.tens)     { push(digit_char(digit.tens)); }
	                                        push(digit_char(digit.ones));
	if (digit.hundredths || digit.tenths) { push(locale.decimal_separator);
	                                        push(digit_char(digit.tenths)); }
	if (digit.hundredths)                 { push(digit_char(digit.hundredths)); }
	if (locale.symbol_position == percentage_locale::spec::symbol_placement::after) {
		if (locale.has_space_around_number) { push(' '); }
		push('%');
	}

	return std::string(buffer, buffer_n);
}

} // namespace fundos
