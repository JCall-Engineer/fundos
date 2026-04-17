#include <gtest/gtest.h>
#include "types.hpp"
using namespace fundos;

TEST(PercentageStringConversions, WellformedStrings) {
	EXPECT_EQ(percentage::from_string("0.00%").value().basis_points, 0);
	EXPECT_EQ(percentage::from_string("100%").value().basis_points, 10000);
	EXPECT_EQ(percentage::from_string("5%").value().basis_points, 500);
	EXPECT_EQ(percentage::from_string("50.25%").value().basis_points, 5025);
}

TEST(PercentageStringConversions, MalformedStrings) {
	EXPECT_EQ(percentage::from_string("x-1-2,.345").value().basis_points, 1234);
	EXPECT_EQ(percentage::from_string("abc").value().basis_points, 0);
	EXPECT_EQ(percentage::from_string("").value().basis_points, 0);

	// std::nullopt checks
	EXPECT_FALSE(percentage::from_string(std::string(256, '1')));
	EXPECT_FALSE(percentage::from_string("100.01%"));
}

TEST(PercentageStringConversions, ToString) {
	percentage_locale_info eur = {
		.decimal_separator = ',',
		.has_space_around_number = true,
		.symbol_position = percentage_locale_info::symbol_placement::before,
	};
	EXPECT_EQ(percentage{0}.to_string(), "0%");
	EXPECT_EQ(percentage{25}.to_string(), "0.25%");
	EXPECT_EQ(percentage{100}.to_string(), "1%");
	EXPECT_EQ(percentage{1050}.to_string(), "10.5%");
	EXPECT_EQ(percentage{10000}.to_string(), "100%");
	EXPECT_EQ(percentage{9876}.to_string(), "98.76%");

	EXPECT_EQ(percentage{0}.to_string(eur), "% 0");
	EXPECT_EQ(percentage{25}.to_string(eur), "% 0,25");
	EXPECT_EQ(percentage{100}.to_string(eur), "% 1");
	EXPECT_EQ(percentage{1050}.to_string(eur), "% 10,5");
	EXPECT_EQ(percentage{10000}.to_string(eur), "% 100");
	EXPECT_EQ(percentage{9876}.to_string(eur), "% 98,76");
}

TEST(TypeArithmetic, PercentBasic) { // necessary? debatable
	percentage a{21}, b{4300}, c, d;
	int64_t scalar1 = 2, scalar2 = 10;

	c = a + b;
	d = a; d += b;
	EXPECT_EQ(c.basis_points, 4321);
	EXPECT_EQ(d.basis_points, 4321);

	c = b + a;
	d = b; d += a;
	EXPECT_EQ(c.basis_points, 4321);
	EXPECT_EQ(d.basis_points, 4321);

	c = b - a;
	d = b; d -= a;
	EXPECT_EQ(c.basis_points, 4279);
	EXPECT_EQ(d.basis_points, 4279);

	c = a - b;
	d = a; d -= b;
	EXPECT_EQ(c.basis_points, -4279);
	EXPECT_EQ(d.basis_points, -4279);

	c = a * scalar1;
	d = a; d *= scalar1;
	EXPECT_EQ(c.basis_points, 42);
	EXPECT_EQ(d.basis_points, 42);

	c = b / scalar2;
	d = b; d /= scalar2;
	EXPECT_EQ(c.basis_points, 430);
	EXPECT_EQ(d.basis_points, 430);

	// Actually useful
	c = percentage::whole();
	d = percentage{5000}; // 50%
	c -= d;
	EXPECT_EQ(c, d);
}

TEST(TypeArithmetic, CurrencyScaledByPercentage) {
	currency<currency_locale::USD> a{10000}, b; // $100
	percentage r{2500}; // 25%

	b = a * r;
	EXPECT_EQ(b, currency<currency_locale::USD>{2500}); // $25

	b = r * (a * 2);
	EXPECT_EQ(b, currency<currency_locale::USD>{5000}); // $50

	// Validate upper limit for overflow risk
	a.minor_units = (INT64_MAX / 100) - 1; // 92_233_720_368_547_757 or $922,337,203,685,477 or $922t
	r.basis_points = 9999; // 99.99%
	b = r * a;
	EXPECT_EQ(b.minor_units, 92'224'496'996'510'902);
}

TEST(TypeArithmetic, CurrencyBasic) { // necessary? debatable
	currency<currency_locale::USD> a{21}, b{4300}, c, d;
	int64_t scalar1 = 2, scalar2 = 10;

	c = a + b;
	d = a; d += b;
	EXPECT_EQ(c.minor_units, 4321);
	EXPECT_EQ(d.minor_units, 4321);

	c = b + a;
	d = b; d += a;
	EXPECT_EQ(c.minor_units, 4321);
	EXPECT_EQ(d.minor_units, 4321);

	c = b - a;
	d = b; d -= a;
	EXPECT_EQ(c.minor_units, 4279);
	EXPECT_EQ(d.minor_units, 4279);

	c = a - b;
	d = a; d -= b;
	EXPECT_EQ(c.minor_units, -4279);
	EXPECT_EQ(d.minor_units, -4279);

	c = a * scalar1;
	d = a; d *= scalar1;
	EXPECT_EQ(c.minor_units, 42);
	EXPECT_EQ(d.minor_units, 42);

	c = b / scalar2;
	d = b; d /= scalar2;
	EXPECT_EQ(c.minor_units, 430);
	EXPECT_EQ(d.minor_units, 430);
}

TEST(CurrencyStringConversions, WellformedStrings) {
	// Zero
	EXPECT_EQ(currency<currency_locale::USD>::from_string("$0.00").value().minor_units, 0);

	// Positive
	EXPECT_EQ(currency<currency_locale::USD>::from_string("$1,234.50").value().minor_units, 123450);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("$1.50").value().minor_units, 150);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("1.5").value().minor_units, 150);

	// Negative
	EXPECT_EQ(currency<currency_locale::USD>::from_string("($1,234.50)").value().minor_units, -123450);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("($1.50)").value().minor_units, -150);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("<$1.50>").value().minor_units, -150);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("1.50-").value().minor_units, -150);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("-1.5").value().minor_units, -150);
}

TEST(CurrencyStringConversions, MalformedStrings) {
	EXPECT_EQ(currency<currency_locale::USD>::from_string("$x1a,b2c3d4e.f5-g0h.i012").value().minor_units, 123450);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("abc").value().minor_units, 0);
	EXPECT_EQ(currency<currency_locale::USD>::from_string("").value().minor_units, 0);

	// std::nullopt checks
	EXPECT_FALSE(currency<currency_locale::USD>::from_string(std::string(256, '1')));
	EXPECT_FALSE(currency<currency_locale::USD>::from_string("99999999999999999999.00"));
	EXPECT_FALSE(currency<currency_locale::USD>::from_string(std::to_string(INT64_MAX / 100 + 1)));
}

TEST(CurrencyStringConversions, USD) {
	EXPECT_EQ(currency<currency_locale::USD>{0}.to_string(), "$0.00");
	EXPECT_EQ(currency<currency_locale::USD>{150}.to_string(), "$1.50");
	EXPECT_EQ(currency<currency_locale::USD>{-150}.to_string(), "($1.50)");
	EXPECT_EQ(currency<currency_locale::USD>{123450}.to_string(), "$1,234.50");
	EXPECT_EQ(currency<currency_locale::USD>{-123450}.to_string(), "($1,234.50)");
	EXPECT_EQ(currency<currency_locale::USD>{123456789}.to_string(), "$1,234,567.89");
	EXPECT_EQ(currency<currency_locale::USD>{-123456789}.to_string(), "($1,234,567.89)");
}

TEST(CurrencyStringConversions, EUR) {
	EXPECT_EQ(currency<currency_locale::EUR>{0}.to_string(),        "0,00€");
	EXPECT_EQ(currency<currency_locale::EUR>{150}.to_string(),      "1,50€");
	EXPECT_EQ(currency<currency_locale::EUR>{-150}.to_string(),     "-1,50€");
	EXPECT_EQ(currency<currency_locale::EUR>{123450}.to_string(),   "1.234,50€");
	EXPECT_EQ(currency<currency_locale::EUR>{-123450}.to_string(),  "-1.234,50€");
}

TEST(CurrencyStringConversions, GBP) {
	EXPECT_EQ(currency<currency_locale::GBP>{0}.to_string(),        "£0.00");
	EXPECT_EQ(currency<currency_locale::GBP>{150}.to_string(),      "£1.50");
	EXPECT_EQ(currency<currency_locale::GBP>{-150}.to_string(),     "-£1.50");
	EXPECT_EQ(currency<currency_locale::GBP>{123450}.to_string(),   "£1,234.50");
	EXPECT_EQ(currency<currency_locale::GBP>{-123450}.to_string(),  "-£1,234.50");
}

TEST(CurrencyStringConversions, CAD) {
	EXPECT_EQ(currency<currency_locale::CAD>{0}.to_string(),        "CA$0.00");
	EXPECT_EQ(currency<currency_locale::CAD>{150}.to_string(),      "CA$1.50");
	EXPECT_EQ(currency<currency_locale::CAD>{-150}.to_string(),     "(CA$1.50)");
	EXPECT_EQ(currency<currency_locale::CAD>{123450}.to_string(),   "CA$1,234.50");
	EXPECT_EQ(currency<currency_locale::CAD>{-123450}.to_string(),  "(CA$1,234.50)");
}

TEST(CurrencyStringConversions, AUD) {
	EXPECT_EQ(currency<currency_locale::AUD>{0}.to_string(),        "A$0.00");
	EXPECT_EQ(currency<currency_locale::AUD>{150}.to_string(),      "A$1.50");
	EXPECT_EQ(currency<currency_locale::AUD>{-150}.to_string(),     "(A$1.50)");
	EXPECT_EQ(currency<currency_locale::AUD>{123450}.to_string(),   "A$1,234.50");
	EXPECT_EQ(currency<currency_locale::AUD>{-123450}.to_string(),  "(A$1,234.50)");
}

TEST(CurrencyStringConversions, JPY) {
	EXPECT_EQ(currency<currency_locale::JPY>{0}.to_string(),        "¥0");
	EXPECT_EQ(currency<currency_locale::JPY>{150}.to_string(),      "¥150");
	EXPECT_EQ(currency<currency_locale::JPY>{-150}.to_string(),     "-¥150");
	EXPECT_EQ(currency<currency_locale::JPY>{123450}.to_string(),   "¥123,450");
	EXPECT_EQ(currency<currency_locale::JPY>{-123450}.to_string(),  "-¥123,450");
}

TEST(CurrencyStringConversions, CNY) {
	EXPECT_EQ(currency<currency_locale::CNY>{0}.to_string(),        "CN¥0.00");
	EXPECT_EQ(currency<currency_locale::CNY>{150}.to_string(),      "CN¥1.50");
	EXPECT_EQ(currency<currency_locale::CNY>{-150}.to_string(),     "-CN¥1.50");
	EXPECT_EQ(currency<currency_locale::CNY>{123450}.to_string(),   "CN¥1,234.50");
	EXPECT_EQ(currency<currency_locale::CNY>{-123450}.to_string(),  "-CN¥1,234.50");
}
