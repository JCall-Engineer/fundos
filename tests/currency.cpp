#include <gtest/gtest.h>
#include "currency.hpp"

TEST(FromString, ZeroValue) {
	EXPECT_EQ(currency::from_string("$0.00").cents, 0);
}

TEST(FromString, PositiveValues) {
	EXPECT_EQ(currency::from_string("$1,234.50").cents, 123450);
	EXPECT_EQ(currency::from_string("$1.50").cents, 150);
	EXPECT_EQ(currency::from_string("1.5").cents, 150);
}

TEST(FromString, NegativeValues) {
	EXPECT_EQ(currency::from_string("($1,234.50)").cents, -123450);
	EXPECT_EQ(currency::from_string("($1.50)").cents, -150);
	EXPECT_EQ(currency::from_string("-1.5").cents, -150);
}

TEST(FromString, MalformedStrings) {
	EXPECT_EQ(currency::from_string("$x1a,b2c3d4e.f5-g0h.i012").cents, 123450);
	EXPECT_EQ(currency::from_string("abc").cents, 0);
	EXPECT_EQ(currency::from_string("").cents, 0);
}

TEST(ToString, BasicDollarAmount) {
	EXPECT_EQ(currency{0}.to_string(), "$0.00");
	EXPECT_EQ(currency{150}.to_string(), "$1.50");
	EXPECT_EQ(currency{-150}.to_string(), "($1.50)");
	EXPECT_EQ(currency{123450}.to_string(), "$1,234.50");
	EXPECT_EQ(currency{-123450}.to_string(), "($1,234.50)");
	EXPECT_EQ(currency{123456789}.to_string(), "$1,234,567.89");
	EXPECT_EQ(currency{-123456789}.to_string(), "($1,234,567.89)");
}
