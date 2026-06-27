#include "currency.hpp"
#include <format>

namespace fundos {

std::optional<int64_t> parse_currency(const std::string& text, const currency_locale::spec& locale) {
	enum class state : uint8_t { sign, whole, remainder };

	uint8_t max_digits = 0;
	for (int16_t s = locale.scale; s > 1; s /= 10) { ++max_digits; } // relies on scale being a power of 10, see spec::scale

	int64_t sign = 1;
	int64_t whole = 0;
	int64_t remainder = 0;

	uint8_t digits = 0;
	state parse_state = state::sign;

	// Currency strings longer than 255 characters (even 32, really - see format_currency) are considered malformed.
	if (text.length() > 255) { return std::nullopt; }

	for (uint8_t i = 0, len = static_cast<uint8_t>(text.length()); i < len; ++i) {
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
				if (is_digit) {
					if (whole > (INT64_MAX - digit) / 10) { return std::nullopt; }
					whole = whole * 10 + digit;
				}
				else if (c == locale.decimal_separator) { parse_state = state::remainder; }
				break;
			case state::remainder:
				if (is_digit && digits < max_digits) {
					remainder = remainder * 10 + digit; // max_digits guarantees this doesn't overflow
					++digits;
				}
				break;
		}
	}
	while (digits < max_digits) { remainder *= 10; ++digits; } // max_digits guarantees this doesn't overflow

	// Second pass for trailing sign markers: the main loop's `sign` state only catches a leading '-' (or '(' / '<'), since it only runs before any digit is seen.
	// A trailing minus like "100-" would otherwise parse as positive, so this catches that case.
	// ')' and '>' ride along here too since they're only ever valid as trailing markers anyway.
	// Note: input with signs on both ends (e.g. "-100-") is not rejected; this loop unconditionally re-sets sign = -1 on the trailing marker,
	// which is a harmless no-op since the leading pass already set it. Consistent with this parser's best-effort policy on malformed input.
	for (int16_t i = static_cast<int16_t>(text.length()) - 1; i >= 0; --i) {
		char c = text[i];
		if (c >= '0' && c <= '9') { break; }
		if (c == '-' || c == ')' || c == '>') { sign = -1; break; }
	}

	if (whole > (INT64_MAX - remainder) / locale.scale) { return std::nullopt; }
	return { sign * (whole * locale.scale + remainder) };
}

inline // helper for format_currency writes the arbitrary size symbol in reverse byte order
void copy_symbol_reversed(char* buffer, size_t& buffer_n, const currency_locale::spec& locale) {
	// The 4 here must match the "4 bytes max for utf8 symbol" budget in format_currency's buffer comment.
	FUNDOS_ASSERT(locale.symbol.size() <= 4, "currency symbol exceeds 4-byte budget, will be truncated");

	size_t copied = 0;
	for (auto it = locale.symbol.rbegin(); it != locale.symbol.rend() && copied < 4; ++it, ++copied) {
		buffer[buffer_n++] = *it;
	}
}

std::string format_currency(int64_t minor_units, const currency_locale::spec& locale) {
	// Unfortunately, std::format is of little help here after we add thousands separators (locale-variable no less), so we (mostly) build the whole string manually
	// Built least-significant-digit first: % 10 and /= 10 naturally produce digits in that order,
	// so writing them out as they're extracted (then reversing at the end) follows the grain of the algorithm instead of fighting it.
	// Thousands separators and zero-padding minor_units fall out naturally too, since each digit's position is already known the moment it's pushed.

	// 19 digits max (combined between major_units and minor_units): max uint64_t = 2^63 - 1 = 9,223,372,036,854,775,807
	// 6 max thousands separators
	// 4 bytes max for utf8 symbol
	// 1 max decimal separator
	// 2 max negative symbol
	// = 32 bytes max total
	char buffer[32];
	size_t buffer_n = 0;

	bool    is_negative =          minor_units < 0;
	int64_t major_units = std::abs(minor_units / locale.scale); // std::abs(INT64_MIN) is UB, but only reachable here when scale=1 (e.g. JPY) and minor_units is near INT64_MIN
	        minor_units = std::abs(minor_units % locale.scale); // (values that extreme are considered malformed input anyway)

	if (locale.symbol_position == currency_locale::spec::symbol_placement::after) {
		copy_symbol_reversed(buffer, buffer_n, locale);
	}

	if (locale.scale > 1) {
		int16_t minor_scale = locale.scale / 10;
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

	if (locale.symbol_position == currency_locale::spec::symbol_placement::before) {
		copy_symbol_reversed(buffer, buffer_n, locale);
	}

	std::reverse(buffer, buffer + buffer_n);
	std::string_view output(buffer, buffer_n);

	// The most comprehensible part of this function lol
	if (!is_negative) { return std::string(output); }
	switch (locale.negative_format) {
		case currency_locale::spec::negative_notation::leading_minus:
			return std::format("-{}", output);
		case currency_locale::spec::negative_notation::trailing_minus:
			return std::format("{}-", output);
		case currency_locale::spec::negative_notation::parentheses:
			return std::format("({})", output);
		case currency_locale::spec::negative_notation::angle_brackets:
			return std::format("<{}>", output);
		default:
			FUNDOS_ASSERT(false, "unhandled currency_negative_format");
			return "";
	}
}

} // namespace fundos
