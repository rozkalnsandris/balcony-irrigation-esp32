#include <cstdint>
#include <unity.h>

#include "command_parse_policy.h"

void setUp() {}
void tearDown() {}

void test_valid_positive_decimal_within_bound() {
  std::uint32_t value = 0U;

  TEST_ASSERT_TRUE(command_parse_policy::parsePositiveDecimal("1", 3U, value));
  TEST_ASSERT_EQUAL_UINT32(1U, value);

  TEST_ASSERT_TRUE(command_parse_policy::parsePositiveDecimal("03", 3U, value));
  TEST_ASSERT_EQUAL_UINT32(3U, value);

  TEST_ASSERT_TRUE(command_parse_policy::parsePositiveDecimal("180", 180U, value));
  TEST_ASSERT_EQUAL_UINT32(180U, value);
}

void test_zero_empty_and_null_are_rejected() {
  std::uint32_t value = 99U;

  TEST_ASSERT_FALSE(command_parse_policy::parsePositiveDecimal(nullptr, 3U, value));
  TEST_ASSERT_EQUAL_UINT32(0U, value);

  TEST_ASSERT_FALSE(command_parse_policy::parsePositiveDecimal("", 3U, value));
  TEST_ASSERT_FALSE(command_parse_policy::parsePositiveDecimal("0", 3U, value));
  TEST_ASSERT_FALSE(command_parse_policy::parsePositiveDecimal("000", 3U, value));
}

void test_non_decimal_syntax_is_rejected() {
  std::uint32_t value = 0U;

  TEST_ASSERT_FALSE(command_parse_policy::parsePositiveDecimal("+1", 3U, value));
  TEST_ASSERT_FALSE(command_parse_policy::parsePositiveDecimal("-1", 3U, value));
  TEST_ASSERT_FALSE(command_parse_policy::parsePositiveDecimal(" 1", 3U, value));
  TEST_ASSERT_FALSE(command_parse_policy::parsePositiveDecimal("1 ", 3U, value));
  TEST_ASSERT_FALSE(command_parse_policy::parsePositiveDecimal("1abc", 3U, value));
  TEST_ASSERT_FALSE(command_parse_policy::parsePositiveDecimal("1.0", 3U, value));
}

void test_bound_and_overflow_are_rejected() {
  std::uint32_t value = 0U;

  TEST_ASSERT_FALSE(command_parse_policy::parsePositiveDecimal("4", 3U, value));
  TEST_ASSERT_FALSE(command_parse_policy::parsePositiveDecimal("40", 3U, value));
  TEST_ASSERT_FALSE(
      command_parse_policy::parsePositiveDecimal(
          "4294967296",
          0xFFFFFFFFU,
          value));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_valid_positive_decimal_within_bound);
  RUN_TEST(test_zero_empty_and_null_are_rejected);
  RUN_TEST(test_non_decimal_syntax_is_rejected);
  RUN_TEST(test_bound_and_overflow_are_rejected);
  return UNITY_END();
}
