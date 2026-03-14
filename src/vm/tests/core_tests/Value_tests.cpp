#include "StringPool.h"
#include "Value.h"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>

using VM::Core::StringPool;
using VM::Core::StringPtr;
using VM::Core::Value;
using VM::Core::ValueHelper;

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

TEST(ValueTest, AsConversionPrimitiveToPrimitive)
{
	Value v = 42.0;
	EXPECT_EQ(ValueHelper::As<double>(v), 42.0);
	EXPECT_EQ(ValueHelper::As<int64_t>(v), 42);

	Value boolValue = true;
	EXPECT_EQ(ValueHelper::As<bool>(boolValue), true);
	EXPECT_EQ(ValueHelper::As<int64_t>(boolValue), 1);
}

TEST(ValueTest, AsConversionThrowsOnInvalid)
{
	Value v = std::monostate{};
	EXPECT_THROW(ValueHelper::As<double>(v), std::bad_variant_access);
}

TEST(ValueTest, AddTwoDoubles)
{
	Value a = 10.0;
	Value b = 5.0;
	Value result = ValueHelper::Add(a, b);

	EXPECT_TRUE(ValueHelper::IsType<double>(result));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(result), 15.0);
}

TEST(ValueTest, AddIntAndDoublePromotesToDouble)
{
	Value a = int64_t{ 10 };
	Value b = 5.5;
	Value result = ValueHelper::Add(a, b);

	EXPECT_TRUE(ValueHelper::IsType<double>(result));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(result), 15.5);
}

TEST(ValueTest, SubtractBasic)
{
	Value a = 20.0;
	Value b = 8.0;
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(ValueHelper::Subtract(a, b)), 12.0);
}

TEST(ValueTest, MultiplyBasic)
{
	Value a = 6.0;
	Value b = 7.0;
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(ValueHelper::Multiply(a, b)), 42.0);
}

TEST(ValueTest, DivideBasic)
{
	Value a = 100.0;
	Value b = 4.0;
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(ValueHelper::Divide(a, b)), 25.0);
}

TEST(ValueTest, DivideByZeroThrows)
{
	Value a = 10.0;
	Value b = 0.0;
	EXPECT_THROW(ValueHelper::Divide(a, b), std::runtime_error);
}

TEST(ValueTest, NegatePositiveToNegative)
{
	Value v = 42.0;
	Value result = ValueHelper::Negate(v);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(result), -42.0);
}

TEST(ValueTest, NegateNegativeToPositive)
{
	Value v = -15.0;
	Value result = ValueHelper::Negate(v);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(result), 15.0);
}

TEST(ValueTest, PerformBinaryLogicAND)
{
	Value a = true;
	Value b = false;
	Value result = ValueHelper::PerformBinaryLogic(a, b, std::logical_and<bool>{});
	EXPECT_FALSE(ValueHelper::As<bool>(result));
}

TEST(ValueTest, PerformBinaryLogicOR)
{
	Value a = false;
	Value b = true;
	Value result = ValueHelper::PerformBinaryLogic(a, b, std::logical_or<bool>{});
	EXPECT_TRUE(ValueHelper::As<bool>(result));
}

TEST(ValueTest, PerformUnaryLogicNOT)
{
	Value v = true;
	Value result = ValueHelper::PerformUnaryLogic(v);
	EXPECT_FALSE(ValueHelper::As<bool>(result));
}

TEST(ValueTest, CompareEqual)
{
	Value a = 42.0;
	Value b = 42.0;
	Value result = ValueHelper::PerformBinaryComparison(a, b, std::equal_to<double>{});
	EXPECT_TRUE(ValueHelper::As<bool>(result));
}

TEST(ValueTest, CompareNotEqual)
{
	Value a = 10.0;
	Value b = 20.0;
	Value result = ValueHelper::PerformBinaryComparison(a, b, std::not_equal_to<double>{});
	EXPECT_TRUE(ValueHelper::As<bool>(result));
}

TEST(ValueTest, CompareLessThan)
{
	Value a = 5.0;
	Value b = 10.0;
	EXPECT_TRUE(ValueHelper::As<bool>(
		ValueHelper::PerformBinaryComparison(a, b, std::less<double>{})));
}

TEST(ValueTest, CompareGreaterThan)
{
	Value a = 15.0;
	Value b = 10.0;
	EXPECT_TRUE(ValueHelper::As<bool>(
		ValueHelper::PerformBinaryComparison(a, b, std::greater<double>{})));
}

TEST(ValueTest, PrintValueNull)
{
	Value v = std::monostate{};
	std::ostringstream oss;
	ValueHelper::PrintValue(v, oss);
	EXPECT_EQ(oss.str(), "null");
}

TEST(ValueTest, PrintValueBool)
{
	std::ostringstream oss;
	ValueHelper::PrintValue(true, oss);
	EXPECT_EQ(oss.str(), "true");

	oss.str("");
	ValueHelper::PrintValue(false, oss);
	EXPECT_EQ(oss.str(), "false");
}

TEST(ValueTest, PrintValueNumber)
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

TEST(ValueTest, ArithmeticWithLargeNumbers)
{
	Value a = std::numeric_limits<double>::max() / 2;
	Value b = std::numeric_limits<double>::max() / 2;
	EXPECT_NO_THROW(ValueHelper::Add(a, b));
}

TEST(ValueTest, ArithmeticWithNaN)
{
	Value a = std::numeric_limits<double>::quiet_NaN();
	Value b = 1.0;
	auto result = ValueHelper::Add(a, b);
	EXPECT_TRUE(std::isnan(ValueHelper::As<double>(result)));
}

TEST(ValueTest, DivisionPrecision)
{
	Value a = 1.0;
	Value b = 3.0;
	auto result = ValueHelper::Divide(a, b);
	EXPECT_NEAR(ValueHelper::As<double>(result), 0.3333333333333333, 1e-10);
}

TEST(ValueTest, StringStorageAndType)
{
	StringPool pool;
	auto s1 = pool.Intern("hello");
	const Value v = s1;

	EXPECT_TRUE(ValueHelper::IsString(v));
	EXPECT_EQ(ValueHelper::GetTypeName(v), "string");

	const auto retrieved = ValueHelper::As<StringPtr>(v);
	EXPECT_EQ(*retrieved, "hello");
}

TEST(ValueTest, StringInterningLogic)
{
	StringPool pool;
	const std::string str1 = "aura";
	const std::string str2 = "aura";

	const auto p1 = pool.Intern(str1);
	const auto p2 = pool.Intern(str2);

	EXPECT_EQ(p1.get(), p2.get());

	const auto p3 = pool.Intern("other string");
	EXPECT_NE(p1.get(), p3.get());
}

TEST(ValueTest, StringAddConcatenation)
{
	StringPool pool;
	Value s1 = pool.Intern("Hello");
	Value s2 = pool.Intern(" World");

	Value res1 = ValueHelper::Add(s1, s2);
	EXPECT_EQ(ValueHelper::ToString(res1), "Hello World");

	Value n = 42;
	Value res2 = ValueHelper::Add(s1, n);
	EXPECT_EQ(ValueHelper::ToString(res2), "Hello42");

	Value res3 = ValueHelper::Add(n, s1);
	EXPECT_EQ(ValueHelper::ToString(res3), "42Hello");
}

TEST(ValueTest, StringComparison)
{
	StringPool pool;
	Value v1 = pool.Intern("apple");
	Value v2 = pool.Intern("apple");
	Value v3 = pool.Intern("orange");

	auto eq = [](const auto& a, const auto& b) { return a == b; };

	EXPECT_TRUE(ValueHelper::As<bool>(ValueHelper::PerformBinaryComparison(v1, v2, eq)));
	EXPECT_FALSE(ValueHelper::As<bool>(ValueHelper::PerformBinaryComparison(v1, v3, eq)));
}

TEST(ValueTest, StringPrintAndToString)
{
	StringPool pool;
	const Value v = pool.Intern("asd");

	EXPECT_EQ(ValueHelper::ToString(v), "asd");

	std::ostringstream oss;
	ValueHelper::PrintValue(v, oss);
	EXPECT_EQ(oss.str(), "asd");
}

TEST(ValueTest, StringAsBool)
{
	StringPool pool;
	Value v = pool.Intern("anything");
	EXPECT_TRUE(ValueHelper::As<bool>(v));

	Value n = std::monostate{};
	EXPECT_THROW(ValueHelper::As<StringPtr>(n), std::bad_variant_access);
}

TEST(ValueTest, EqualityDeepCheck)
{
	StringPool pool;
	Value s1 = pool.Intern("apple");
	Value s2 = std::make_shared<const std::string>("apple");
	Value s3 = pool.Intern("orange");

	EXPECT_TRUE(ValueHelper::Equal(s1, s2));
	EXPECT_FALSE(ValueHelper::Equal(s1, s3));

	EXPECT_TRUE(ValueHelper::Equal(int64_t{ 42 }, 42.0));
	EXPECT_TRUE(ValueHelper::Equal(42.0, int64_t{ 42 }));
	EXPECT_FALSE(ValueHelper::Equal(int64_t{ 42 }, 42.1));

	EXPECT_FALSE(ValueHelper::Equal(42.0, s1));
	EXPECT_TRUE(ValueHelper::Equal(true, 1));
}

TEST(ValueTest, NumericComparisons)
{
	Value low = 10.0;
	Value high = 20.0;
	Value loInt = int64_t{ 10 };

	EXPECT_TRUE(ValueHelper::As<bool>(ValueHelper::Less(low, high)));
	EXPECT_TRUE(ValueHelper::As<bool>(ValueHelper::Less(loInt, high)));
	EXPECT_FALSE(ValueHelper::As<bool>(ValueHelper::Less(high, low)));

	EXPECT_TRUE(ValueHelper::As<bool>(ValueHelper::Greater(high, low)));
	EXPECT_FALSE(ValueHelper::As<bool>(ValueHelper::Greater(low, high)));
}