#include <gtest/gtest.h>
#include "currency.hpp"
using namespace fundos;

TEST(FromString, ZeroValue) {
	EXPECT_EQ(currency<currency_locale::USD>::from_string("$0.00").value().minor_units, 0);
}

TEST(FromString, PositiveValues) {
	EXPECT_EQ(currency<currency_locale::USD>::from_string("$1,234.50").value().minor_units, 123450);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("$1.50").value().minor_units, 150);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("1.5").value().minor_units, 150);
}

TEST(FromString, NegativeValues) {
	EXPECT_EQ(currency<currency_locale::USD>::from_string("($1,234.50)").value().minor_units, -123450);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("($1.50)").value().minor_units, -150);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("<$1.50>").value().minor_units, -150);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("1.50-").value().minor_units, -150);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("-1.5").value().minor_units, -150);
}

TEST(FromString, MalformedStrings) {
	EXPECT_EQ(currency<currency_locale::USD>::from_string("$x1a,b2c3d4e.f5-g0h.i012").value().minor_units, 123450);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("abc").value().minor_units, 0);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("").value().minor_units, 0);

	// std::nullopt checks
	EXPECT_FALSE(currency<currency_locale::USD>::from_string(std::string(256, '1')));
	EXPECT_FALSE(currency<currency_locale::USD>::from_string("99999999999999999999.00"));
	EXPECT_FALSE(currency<currency_locale::USD>::from_string(std::to_string(INT64_MAX / 100 + 1)));
}

TEST(ToString, BasicDollarAmount) {
	EXPECT_EQ(currency<currency_locale::USD>{0}.to_string(), "$0.00");
	EXPECT_EQ(currency<currency_locale::USD>{150}.to_string(), "$1.50");
	EXPECT_EQ(currency<currency_locale::USD>{-150}.to_string(), "($1.50)");
	EXPECT_EQ(currency<currency_locale::USD>{123450}.to_string(), "$1,234.50");
	EXPECT_EQ(currency<currency_locale::USD>{-123450}.to_string(), "($1,234.50)");
	EXPECT_EQ(currency<currency_locale::USD>{123456789}.to_string(), "$1,234,567.89");
	EXPECT_EQ(currency<currency_locale::USD>{-123456789}.to_string(), "($1,234,567.89)");
}

TEST(ToString, EUR) {
	EXPECT_EQ(currency<currency_locale::EUR>{0}.to_string(),        "0,00€");
	EXPECT_EQ(currency<currency_locale::EUR>{150}.to_string(),      "1,50€");
	EXPECT_EQ(currency<currency_locale::EUR>{-150}.to_string(),     "-1,50€");
	EXPECT_EQ(currency<currency_locale::EUR>{123450}.to_string(),   "1.234,50€");
	EXPECT_EQ(currency<currency_locale::EUR>{-123450}.to_string(),  "-1.234,50€");
}

TEST(ToString, GBP) {
	EXPECT_EQ(currency<currency_locale::GBP>{0}.to_string(),        "£0.00");
	EXPECT_EQ(currency<currency_locale::GBP>{150}.to_string(),      "£1.50");
	EXPECT_EQ(currency<currency_locale::GBP>{-150}.to_string(),     "-£1.50");
	EXPECT_EQ(currency<currency_locale::GBP>{123450}.to_string(),   "£1,234.50");
	EXPECT_EQ(currency<currency_locale::GBP>{-123450}.to_string(),  "-£1,234.50");
}

TEST(ToString, CAD) {
	EXPECT_EQ(currency<currency_locale::CAD>{0}.to_string(),        "CA$0.00");
	EXPECT_EQ(currency<currency_locale::CAD>{150}.to_string(),      "CA$1.50");
	EXPECT_EQ(currency<currency_locale::CAD>{-150}.to_string(),     "(CA$1.50)");
	EXPECT_EQ(currency<currency_locale::CAD>{123450}.to_string(),   "CA$1,234.50");
	EXPECT_EQ(currency<currency_locale::CAD>{-123450}.to_string(),  "(CA$1,234.50)");
}

TEST(ToString, AUD) {
	EXPECT_EQ(currency<currency_locale::AUD>{0}.to_string(),        "A$0.00");
	EXPECT_EQ(currency<currency_locale::AUD>{150}.to_string(),      "A$1.50");
	EXPECT_EQ(currency<currency_locale::AUD>{-150}.to_string(),     "(A$1.50)");
	EXPECT_EQ(currency<currency_locale::AUD>{123450}.to_string(),   "A$1,234.50");
	EXPECT_EQ(currency<currency_locale::AUD>{-123450}.to_string(),  "(A$1,234.50)");
}

TEST(ToString, JPY) {
	EXPECT_EQ(currency<currency_locale::JPY>{0}.to_string(),        "¥0");
	EXPECT_EQ(currency<currency_locale::JPY>{150}.to_string(),      "¥150");
	EXPECT_EQ(currency<currency_locale::JPY>{-150}.to_string(),     "-¥150");
	EXPECT_EQ(currency<currency_locale::JPY>{123450}.to_string(),   "¥123,450");
	EXPECT_EQ(currency<currency_locale::JPY>{-123450}.to_string(),  "-¥123,450");
}

TEST(ToString, CNY) {
	EXPECT_EQ(currency<currency_locale::CNY>{0}.to_string(),        "CN¥0.00");
	EXPECT_EQ(currency<currency_locale::CNY>{150}.to_string(),      "CN¥1.50");
	EXPECT_EQ(currency<currency_locale::CNY>{-150}.to_string(),     "-CN¥1.50");
	EXPECT_EQ(currency<currency_locale::CNY>{123450}.to_string(),   "CN¥1,234.50");
	EXPECT_EQ(currency<currency_locale::CNY>{-123450}.to_string(),  "-CN¥1,234.50");
}