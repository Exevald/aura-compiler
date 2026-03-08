#include "Value.h"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>

namespace VM::Core::Tests
{

TEST(ValueTest, CanHoldPrimitiveTypes)
{
	Value intValue = int64_t{ 42 };
	Value doubleValue = double{ 3.14 };
	Value boolValue = true;
	Value nullValue = std::monostate{};

	EXPECT_TRUE(ValueHelper::IsType<int64_t>(intValue));
	EXPECT_TRUE(ValueHelper::IsType<double>(doubleValue));
	EXPECT_TRUE(ValueHelper::IsType<bool>(boolValue));
	EXPECT_TRUE(ValueHelper::IsType<std::monostate>(nullValue));
}

TEST(ValueTest, GetTypeNames)
{
	EXPECT_EQ(ValueHelper::GetTypeName(std::monostate{}), "void");
	EXPECT_EQ(ValueHelper::GetTypeName(true), "bool");
	EXPECT_EQ(ValueHelper::GetTypeName(int64_t{ 100 }), "int64");
	EXPECT_EQ(ValueHelper::GetTypeName(2.718), "float64");
}

TEST(ValueTest, AsConversion_PrimitiveToPrimitive)
{
	Value v = 42.0;
	EXPECT_EQ(ValueHelper::As<double>(v), 42.0);
	EXPECT_EQ(ValueHelper::As<int64_t>(v), 42);

	Value boolValue = true;
	EXPECT_EQ(ValueHelper::As<bool>(boolValue), true);
	EXPECT_EQ(ValueHelper::As<int64_t>(boolValue), 1);
}

TEST(ValueTest, AsConversion_ThrowsOnInvalid)
{
	Value v = std::monostate{};
	EXPECT_THROW(ValueHelper::As<double>(v), std::bad_variant_access);
}

TEST(ValueTest, Add_TwoDoubles)
{
	Value a = 10.0;
	Value b = 5.0;
	Value result = ValueHelper::Add(a, b);

	EXPECT_TRUE(ValueHelper::IsType<double>(result));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(result), 15.0);
}

TEST(ValueTest, Add_IntAndDouble_PromotesToDouble)
{
	Value a = int64_t{ 10 };
	Value b = 5.5;
	Value result = ValueHelper::Add(a, b);

	EXPECT_TRUE(ValueHelper::IsType<double>(result));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(result), 15.5);
}

TEST(ValueTest, Subtract_Basic)
{
	Value a = 20.0;
	Value b = 8.0;
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(ValueHelper::Subtract(a, b)), 12.0);
}

TEST(ValueTest, Multiply_Basic)
{
	Value a = 6.0;
	Value b = 7.0;
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(ValueHelper::Multiply(a, b)), 42.0);
}

TEST(ValueTest, Divide_Basic)
{
	Value a = 100.0;
	Value b = 4.0;
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(ValueHelper::Divide(a, b)), 25.0);
}

TEST(ValueTest, Divide_ByZero_Throws)
{
	Value a = 10.0;
	Value b = 0.0;
	EXPECT_THROW(ValueHelper::Divide(a, b), std::runtime_error);
}

TEST(ValueTest, Negate_PositiveToNegative)
{
	Value v = 42.0;
	Value result = ValueHelper::Negate(v);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(result), -42.0);
}

TEST(ValueTest, Negate_NegativeToPositive)
{
	Value v = -15.0;
	Value result = ValueHelper::Negate(v);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(result), 15.0);
}

TEST(ValueTest, PerformBinaryLogic_AND)
{
	Value a = true;
	Value b = false;
	Value result = ValueHelper::PerformBinaryLogic(a, b, std::logical_and<bool>{});
	EXPECT_FALSE(ValueHelper::As<bool>(result));
}

TEST(ValueTest, PerformBinaryLogic_OR)
{
	Value a = false;
	Value b = true;
	Value result = ValueHelper::PerformBinaryLogic(a, b, std::logical_or<bool>{});
	EXPECT_TRUE(ValueHelper::As<bool>(result));
}

TEST(ValueTest, PerformUnaryLogic_NOT)
{
	Value v = true;
	Value result = ValueHelper::PerformUnaryLogic(v);
	EXPECT_FALSE(ValueHelper::As<bool>(result));
}

TEST(ValueTest, Compare_Equal)
{
	Value a = 42.0;
	Value b = 42.0;
	Value result = ValueHelper::PerformBinaryComparison(a, b, std::equal_to<double>{});
	EXPECT_TRUE(ValueHelper::As<bool>(result));
}

TEST(ValueTest, Compare_NotEqual)
{
	Value a = 10.0;
	Value b = 20.0;
	Value result = ValueHelper::PerformBinaryComparison(a, b, std::not_equal_to<double>{});
	EXPECT_TRUE(ValueHelper::As<bool>(result));
}

TEST(ValueTest, Compare_LessThan)
{
	Value a = 5.0;
	Value b = 10.0;
	EXPECT_TRUE(ValueHelper::As<bool>(
		ValueHelper::PerformBinaryComparison(a, b, std::less<double>{})));
}

TEST(ValueTest, Compare_GreaterThan)
{
	Value a = 15.0;
	Value b = 10.0;
	EXPECT_TRUE(ValueHelper::As<bool>(
		ValueHelper::PerformBinaryComparison(a, b, std::greater<double>{})));
}

TEST(ValueTest, PrintValue_Null)
{
	Value v = std::monostate{};
	std::ostringstream oss;
	ValueHelper::PrintValue(v, oss);
	EXPECT_EQ(oss.str(), "null");
}

TEST(ValueTest, PrintValue_Bool)
{
	std::ostringstream oss;
	ValueHelper::PrintValue(true, oss);
	EXPECT_EQ(oss.str(), "true");

	oss.str("");
	ValueHelper::PrintValue(false, oss);
	EXPECT_EQ(oss.str(), "false");
}

TEST(ValueTest, PrintValue_Number)
{
	std::ostringstream oss;
	ValueHelper::PrintValue(42.0, oss);
	EXPECT_EQ(oss.str(), "42");
}

TEST(ValueTest, ToString)
{
	EXPECT_EQ(ValueHelper::ToString(3.14), "3.14");
	EXPECT_EQ(ValueHelper::ToString(true), "true");
	EXPECT_EQ(ValueHelper::ToString(std::monostate{}), "null");
}

TEST(ValueTest, Arithmetic_WithLargeNumbers)
{
	Value a = std::numeric_limits<double>::max() / 2;
	Value b = std::numeric_limits<double>::max() / 2;
	EXPECT_NO_THROW(ValueHelper::Add(a, b));
}

TEST(ValueTest, Arithmetic_WithNaN)
{
	Value a = std::numeric_limits<double>::quiet_NaN();
	Value b = 1.0;
	auto result = ValueHelper::Add(a, b);
	EXPECT_TRUE(std::isnan(ValueHelper::As<double>(result)));
}

TEST(ValueTest, Division_Precision)
{
	Value a = 1.0;
	Value b = 3.0;
	auto result = ValueHelper::Divide(a, b);
	EXPECT_NEAR(ValueHelper::As<double>(result), 0.3333333333333333, 1e-10);
}

} // namespace VM::Core::Tests