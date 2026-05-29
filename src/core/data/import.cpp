#include <cctype>
#include <chrono>
#include <charconv>
#include <algorithm>
#include <fstream>
#include "import.hpp"

namespace fundos::import {

#pragma region String Helpers

static std::optional<int> extract_number(std::string_view text, size_t offset, size_t length) {
	if (offset + length > text.size()) {
		return std::nullopt;
	}
	int value = 0;
	auto result = std::from_chars(text.data() + offset, text.data() + offset + length, value);
	if (result.ec != std::errc{}) {
		return std::nullopt;
	}
	return value;
}

std::optional<datetime> parse_ofx_datetime(const std::string& text) {
	if (text.size() < 8) { return std::nullopt; }

	auto year  = extract_number(text, 0, 4);
	auto month = extract_number(text, 4, 2);
	auto day   = extract_number(text, 6, 2);
	if (!year || !month || !day) { return std::nullopt; }

	int hour = 0, minute = 0, second = 0, millisecond = 0;
	if (text.size() >= 14) {
		auto parsed_hour   = extract_number(text, 8,  2);
		auto parsed_minute = extract_number(text, 10, 2);
		auto parsed_second = extract_number(text, 12, 2);
		if (!parsed_hour || !parsed_minute || !parsed_second) { return std::nullopt; }

		hour   = *parsed_hour;
		minute = *parsed_minute;
		second = *parsed_second;

		if (text.size() >= 18 && text[14] == '.') {
			auto parsed_ms = extract_number(text, 15, 3);
			if (!parsed_ms) { return std::nullopt; }
			millisecond = *parsed_ms;
		}
	}

	// OFX spec allows 60 for leap seconds
	if (hour > 23 || minute > 59 || second > 60 || millisecond > 999) { return std::nullopt; }

	// Parse optional GMT offset from [offset:name] or [offset]
	double gmt_offset_hours = 0.0;
	size_t bracket = text.find('[');
	if (bracket != std::string::npos) {
		size_t colon = text.find(':', bracket);
		size_t end   = colon != std::string::npos ? colon : text.find(']', bracket);
		if (end == std::string::npos) { return std::nullopt; }
		std::string offset_str(text.begin() + bracket + 1, text.begin() + end);
		try {
			gmt_offset_hours = std::stod(offset_str);
		} catch (...) {
			return std::nullopt;
		}
	}
	if (gmt_offset_hours < -12.0 || gmt_offset_hours > 12.0) { return std::nullopt; }

	std::chrono::year_month_day ymd{
		std::chrono::year{*year},
		std::chrono::month{static_cast<unsigned>(*month)},
		std::chrono::day{static_cast<unsigned>(*day)}
	};
	if (!ymd.ok()) { return std::nullopt; }
	std::chrono::sys_days date = std::chrono::sys_days{ymd};

	auto time_point = date
		+ std::chrono::hours{hour}
		+ std::chrono::minutes{minute}
		+ std::chrono::seconds{second}
		+ std::chrono::milliseconds{millisecond};

	// Subtract offset to convert to UTC (e.g. -5:EST means add 5 hours)
	auto offset_minutes = static_cast<int>(gmt_offset_hours * 60);
	time_point -= std::chrono::minutes{offset_minutes};

	int64_t ms = duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()).count();
	return datetime{ms};
}

std::string& upper(std::string& text) {
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character) {
		return static_cast<char>(std::toupper(character));
	});
	return text;
}

std::string& strip(std::string& text) {
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end()); // normalize windows line endings

	auto start = text.find_first_not_of(" \t\n");
	auto end = text.find_last_not_of(" \t\n");
	if (start == std::string::npos) { text.clear(); }
	else { text = text.substr(start, end - start + 1); }

	return text;
}

std::string cp1252_to_utf8(const std::string& input) {
	// Windows-1252 to Unicode codepoint mapping for 0x80-0x9F
	static const uint32_t table[32] = {
		0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
		0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
		0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
		0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
	};

	std::string output;
	output.reserve(input.size());
	for (unsigned char byte : input) {
		uint32_t codepoint;
		if (byte < 0x80) {
			output.push_back(byte);
			continue;
		}
		if (byte >= 0x80 && byte <= 0x9F) {
			codepoint = table[byte - 0x80];
		} else {
			codepoint = byte; // 0xA0-0xFF identical to ISO-8859-1
		}
		if (codepoint < 0x80) {
			output.push_back(static_cast<char>(codepoint));
		} else if (codepoint < 0x800) {
			output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
			output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
		} else {
			output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
			output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
			output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
		}
	}
	return output;
}

enum class codec {
	utf8,
	cp1252,
};

#pragma endregion
#pragma region OFX Parser Helpers

struct parse_context {
	std::ifstream& file;
	codec encoding;
	const currency_locale::spec& locale;
	result& output;
};

struct ofx_token {
	enum class type : uint8_t {
		eof,
		leaf_value,
		opening_tag,
		closing_tag,
	};
	type type = type::leaf_value;
	std::string text;
};

ofx_token extract_token(parse_context& context) {
	char character;
	ofx_token out;

	do {
		if (context.file.peek() == EOF) {
			out.type = ofx_token::type::eof;
			return out;
		}

		if (context.file.peek() == '<') {
			context.file.get(character); // consume '<'
			out.type = ofx_token::type::opening_tag;

			if (context.file.peek() == '/') {
				out.type = ofx_token::type::closing_tag;
				context.file.get(character); // consume '/'
			}

			std::getline(context.file, out.text, '>'); // read until '>'
			upper(out.text);
			return out;
		}

		std::getline(context.file, out.text, '<'); // read until next tag
		context.file.unget(); // un-consume the <

		strip(out.text);
		if (context.encoding == codec::cp1252) {
			out.text = cp1252_to_utf8(out.text);
		}
	} while(out.text.length() == 0); // consume empty lines
	return out;
}

#pragma endregion
#pragma region Recursive Descent Parser

static const std::string_view CORRECT_FITID_TAG  = "CORRECTFITID";
static const std::string_view CORRECT_ACTION_TAG = "CORRECTACTION";
static const std::string_view CORRECT_REPLACE    = "REPLACE";
static const std::string_view CORRECT_DELETE     = "DELETE";

static const std::string_view DATE_TAG   = "DTPOSTED";
static const std::string_view AMOUNT_TAG = "TRNAMT";
static const std::string_view FITID_TAG  = "FITID";
static const std::string_view NAME_TAG   = "NAME";
static const std::string_view MEMO_TAG   = "MEMO";
void import_transaction(parse_context& context, std::vector<imported_transaction>& transactions, std::string_view close_on) {
	transaction transaction;
	std::optional<currency> pending_amount;
	ofx_token in = extract_token(context);
	while (in.type != ofx_token::type::eof) {
		switch (in.type) {
			case ofx_token::type::opening_tag: {
				ofx_token value = extract_token(context);
				if (value.type != ofx_token::type::leaf_value) {
					context.output.set_error(error::malformed);
					return;
				}
				if (in.text == MEMO_TAG || (in.text == NAME_TAG && transaction.memo.empty())) {
					transaction.memo = value.text;
				} else if (in.text == FITID_TAG) {
					transaction.fitid = value.text;
				} else if (in.text == AMOUNT_TAG) {
					auto parsed = currency::from_string(value.text, context.locale);
					if (!parsed) {
						context.output.add_warning(warning::bad_amount);
						context.output.add_warning(warning::skipped_transaction);
						return;
					}
					pending_amount = *parsed;
				} else if (in.text == DATE_TAG) {
					auto parsed = parse_ofx_datetime(value.text);
					if (!parsed) {
						context.output.add_warning(warning::bad_date);
						context.output.add_warning(warning::skipped_transaction);
						return;
					}
					transaction.date_recorded = *parsed;
					transaction.date_cleared = *parsed;
				} else if (in.text == CORRECT_FITID_TAG) {
					transaction.corrects_fitid = value.text;
				} else if (in.text == CORRECT_ACTION_TAG) {
					upper(strip(value.text));
					if (value.text == CORRECT_REPLACE) {
						transaction.correct_action = transaction::correction_type::replaces;
					} else if (value.text == CORRECT_DELETE) {
						transaction.correct_action = transaction::correction_type::deletes;
					} else {
						context.output.add_warning(warning::bad_correction);
						context.output.add_warning(warning::skipped_transaction);
						return;
					}
				}
				break;
			}
			case ofx_token::type::closing_tag:
				if (in.text != close_on) {
					context.output.set_error(error::malformed);
					return;
				}
				if (transaction.fitid == std::nullopt) {
					context.output.add_warning(warning::missing_fitid);
					context.output.add_warning(warning::skipped_transaction);
					return;
				}
				if (transaction.corrects_fitid.has_value() != transaction.correct_action.has_value()) {
					context.output.add_warning(warning::bad_correction);
					context.output.add_warning(warning::skipped_transaction);
					return;
				}
				if (transaction.date_recorded.milliseconds_since_epoch == 0) {
					context.output.add_warning(warning::missing_date);
					context.output.add_warning(warning::skipped_transaction);
					return;
				}
				bool is_delete = transaction.correct_action == transaction::correction_type::deletes;
				if (!pending_amount && !is_delete) {
					context.output.add_warning(warning::missing_amount);
					context.output.add_warning(warning::skipped_transaction);
					return;
				}
				if (pending_amount) {
					transaction.amount = *pending_amount;
				}
				imported_transaction imported;
				imported.importing = std::move(transaction);
				transactions.push_back(std::move(imported));
				return;
		}
		in = extract_token(context);
	}
	context.output.set_error(error::malformed);
}

static const std::string_view TX_WRAPPER = "STMTTRN";
void import_transactions(parse_context& context, bank_account& account, std::string_view close_on) {
	ofx_token in = extract_token(context);
	while (in.type != ofx_token::type::eof) {
		switch (in.type) {
			case ofx_token::type::opening_tag:
				if (in.text == TX_WRAPPER) {
					import_transaction(context, account.transactions, TX_WRAPPER);
					if (!context.output.ok()) { return; }
				}
				break;
			case ofx_token::type::closing_tag:
				if (in.text == close_on) { return; }
		}
		in = extract_token(context);
	}
	context.output.set_error(error::malformed);
}

static const std::string_view BALANCE_VALUE  = "BALAMT";
static const std::string_view AS_OF_VALUE    = "DTASOF";
void import_ledger(parse_context& context, bank_account& account, std::string_view close_on) {
	ofx_token in = extract_token(context);
	std::optional<currency> parsed_amount;
	std::optional<datetime> parsed_as_of;
	while (in.type != ofx_token::type::eof) {
		switch (in.type) {
			case ofx_token::type::opening_tag: {
				ofx_token value = extract_token(context);
				if (value.type != ofx_token::type::leaf_value) {
					context.output.set_error(error::malformed);
					return;
				}
				if (in.text == BALANCE_VALUE) {
					parsed_amount = currency::from_string(value.text, context.locale);
					if (!parsed_amount) {
						context.output.add_warning(warning::bad_amount);
						break;
					}
					account.balance = *parsed_amount;
				} else if (in.text == AS_OF_VALUE) {
					parsed_as_of = parse_ofx_datetime(value.text);
					if (!parsed_as_of) {
						context.output.add_warning(warning::bad_date);
						break;
					}
					account.as_of = *parsed_as_of;
				}
				break;
			}
			case ofx_token::type::closing_tag:
				if (in.text != close_on) { break; }
				if (!parsed_amount) {
					context.output.add_warning(warning::missing_amount);
				}
				if (!parsed_as_of) {
					context.output.add_warning(warning::missing_date);
				}
				return;
		}
		in = extract_token(context);
	}
	context.output.set_error(error::malformed);
}

static const std::string_view ACCTID_TAG     = "ACCTID";
static const std::string_view LEDGER_TAG     = "LEDGERBAL";
static const std::string_view TXLIST_WRAPPER = "BANKTRANLIST";
void import_bank(parse_context& context, std::string_view close_on) {
	ofx_token in = extract_token(context);
	bank_account account;
	while (in.type != ofx_token::type::eof) {
		switch (in.type) {
			case ofx_token::type::opening_tag:
				if (in.text == ACCTID_TAG) {
					in = extract_token(context);
					if (in.type != ofx_token::type::leaf_value) {
						context.output.set_error(error::malformed);
						return;
					}
					account.acct_id = in.text;
				} else if (in.text == TXLIST_WRAPPER) {
					import_transactions(context, account, TXLIST_WRAPPER);
					if (!context.output.ok()) { return; }
				} else if (in.text == LEDGER_TAG) {
					import_ledger(context, account, LEDGER_TAG);
					if (!context.output.ok()) { return; }
				}
				break;
			case ofx_token::type::closing_tag:
				if (in.text == close_on) {
					if (account.acct_id.empty()) {
						context.output.add_warning(warning::missing_acctid);
					} else {
						context.output.data.accounts.push_back(std::move(account));
					}
					return;
				}
				break;
		}
		in = extract_token(context);
	}
	context.output.set_error(error::malformed);
}

static const std::string_view BANK_WRAPPER = "BANKMSGSRSV1";
static const std::string_view CC_WRAPPER   = "CREDITCARDMSGSRSV1";
void import_ofx(parse_context& context) {
	ofx_token in = extract_token(context);
	while (in.type != ofx_token::type::eof) {
		if (in.type == ofx_token::type::opening_tag) {
			if (in.text == BANK_WRAPPER || in.text == CC_WRAPPER) {
				import_bank(context, in.text);
				if (!context.output.ok()) { return; }
			}
		}

		in = extract_token(context);
	}
}

#pragma endregion
#pragma region Dispatch Helpers

static result dispatch(std::ifstream& file, const currency_locale::spec& locale, codec encoding) {
	result output;
	parse_context context { file, encoding, locale, output };
	import_ofx(context);
	return output;
}

static std::optional<codec> resolve_xml_header(const std::string& first_line) {
	auto extract = [&first_line](const std::string& key) -> std::string {
		std::string search = key + "=\"";
		size_t start = first_line.find(search);
		if (start == std::string::npos) { return ""; }
		start += search.size();
		size_t end = first_line.find('"', start);
		if (end == std::string::npos) { return ""; }
		return first_line.substr(start, end - start);
	};

	if (!first_line.ends_with("?>")) { return std::nullopt; }
	if (extract("VERSION") != "1.0") { return std::nullopt; }

	std::string encoding_str = extract("ENCODING");
	if (encoding_str == "UTF-8" || encoding_str == "") { return codec::utf8; }
	if (encoding_str == "ISO-8859-1" || encoding_str == "WINDOWS-1252") { return codec::cp1252; }
	return std::nullopt;
}

static std::optional<codec> resolve_legacy_header(std::ifstream& file) {
	std::string line, header_encoding, header_charset;
	while (std::getline(file, line)) {
		if (!line.empty() && line.back() == '\r') { line.pop_back(); }
		if (line.empty()) { break; }
		upper(line);

		size_t colon = line.find(":");
		if (colon == std::string::npos) { return std::nullopt; }
		std::string key = line.substr(0, colon);
		std::string value = line.substr(colon + 1);
		if (key == "DATA" && value != "OFXSGML") { return std::nullopt; }
		if (key == "SECURITY" && value != "NONE") { return std::nullopt; }
		if (key == "COMPRESSION" && value != "NONE") { return std::nullopt; }
		if (key == "ENCODING") { header_encoding = value; }
		if (key == "CHARSET") { header_charset = value; }
	}

	if (header_encoding == "UTF-8" || header_charset == "UTF-8")         { return codec::utf8; }
	if (header_encoding.empty() && header_charset.empty())               { return codec::cp1252; }
	if (header_encoding == "USASCII" || header_encoding == "ISO-8859-1") { return codec::cp1252; }
	if (header_charset == "ISO-8859-1" || header_charset == "1252")      { return codec::cp1252; }
	return std::nullopt;
}

#pragma endregion

static const std::string legacy_header = "OFXHEADER:";
result import_ofx(const std::string& filepath, const currency_locale::spec& locale) {
	std::ifstream file(filepath);
	if (!file.is_open()) { return result(error::io_error); }

	std::string first_line;
	std::getline(file, first_line);
	upper(strip(first_line));

	if (first_line.starts_with("<?XML")) {
		auto encoding = resolve_xml_header(first_line);
		if (!encoding) { return result(error::bad_format); }
		return dispatch(file, locale, *encoding);
	}

	if (first_line.starts_with(legacy_header)) {
		strip(first_line.erase(0, legacy_header.length()));
		if (first_line != "100") { return result(error::bad_format); }

		auto encoding = resolve_legacy_header(file);
		if (!encoding) { return result(error::bad_format); }
		return dispatch(file, locale, *encoding);
	}

	return result(error::bad_format);
}

} // namespace fundos::import
