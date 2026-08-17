#include <cstdint>
#include <limits>
#include <unity.h>

#include "pump_timing_policy.h"

void setUp() {}
void tearDown() {}

void test_zero_request_uses_default_duration() {
  TEST_ASSERT_EQUAL_UINT32(
      30U,
      pump_timing_policy::normalizeRequestedSeconds(0U, 30U, 180U));
}

void test_default_duration_is_capped_to_hard_limit() {
  TEST_ASSERT_EQUAL_UINT32(
      60U,
      pump_timing_policy::normalizeRequestedSeconds(0U, 90U, 60U));
}

void test_requested_duration_below_limit_is_preserved() {
  TEST_ASSERT_EQUAL_UINT32(
      45U,
      pump_timing_policy::normalizeRequestedSeconds(45U, 30U, 180U));
}

void test_requested_duration_above_limit_is_capped() {
  TEST_ASSERT_EQUAL_UINT32(
      180U,
      pump_timing_policy::normalizeRequestedSeconds(999U, 30U, 180U));
}

void test_extension_below_limit_preserves_requested_addition() {
  const auto result =
      pump_timing_policy::extendPlannedDuration(30000U, 30U, 180U);

  TEST_ASSERT_EQUAL_UINT32(60000U, result.plannedDurationMs);
  TEST_ASSERT_EQUAL_UINT32(30000U, result.addedMs);
}

void test_extension_is_capped_at_hard_limit() {
  const auto result =
      pump_timing_policy::extendPlannedDuration(170000U, 30U, 180U);

  TEST_ASSERT_EQUAL_UINT32(180000U, result.plannedDurationMs);
  TEST_ASSERT_EQUAL_UINT32(10000U, result.addedMs);
}

void test_extension_at_hard_limit_adds_nothing() {
  const auto result =
      pump_timing_policy::extendPlannedDuration(180000U, 30U, 180U);

  TEST_ASSERT_EQUAL_UINT32(180000U, result.plannedDurationMs);
  TEST_ASSERT_EQUAL_UINT32(0U, result.addedMs);
}

void test_huge_extension_cannot_overflow_past_hard_limit() {
  const auto result = pump_timing_policy::extendPlannedDuration(
      1000U,
      std::numeric_limits<std::uint32_t>::max(),
      180U);

  TEST_ASSERT_EQUAL_UINT32(180000U, result.plannedDurationMs);
  TEST_ASSERT_EQUAL_UINT32(179000U, result.addedMs);
}

void test_duration_boundary_is_elapsed_exactly_on_time() {
  TEST_ASSERT_TRUE(pump_timing_policy::hasElapsed(
      31000U,
      1000U,
      30000U));
}

void test_before_duration_boundary_is_not_elapsed() {
  TEST_ASSERT_FALSE(pump_timing_policy::hasElapsed(
      30999U,
      1000U,
      30000U));
}

void test_hard_limit_boundary_is_testable_independently() {
  TEST_ASSERT_TRUE(pump_timing_policy::hasElapsed(
      181000U,
      1000U,
      180000U));
}

void test_elapsed_math_is_wrap_safe() {
  constexpr std::uint32_t start = 0xFFFFFFF0U;
  constexpr std::uint32_t now = 0x00000020U;

  TEST_ASSERT_EQUAL_UINT32(
      48U,
      pump_timing_policy::elapsedMs(now, start));

  TEST_ASSERT_TRUE(pump_timing_policy::hasElapsed(
      now,
      start,
      48U));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_zero_request_uses_default_duration);
  RUN_TEST(test_default_duration_is_capped_to_hard_limit);
  RUN_TEST(test_requested_duration_below_limit_is_preserved);
  RUN_TEST(test_requested_duration_above_limit_is_capped);
  RUN_TEST(test_extension_below_limit_preserves_requested_addition);
  RUN_TEST(test_extension_is_capped_at_hard_limit);
  RUN_TEST(test_extension_at_hard_limit_adds_nothing);
  RUN_TEST(test_huge_extension_cannot_overflow_past_hard_limit);
  RUN_TEST(test_duration_boundary_is_elapsed_exactly_on_time);
  RUN_TEST(test_before_duration_boundary_is_not_elapsed);
  RUN_TEST(test_hard_limit_boundary_is_testable_independently);
  RUN_TEST(test_elapsed_math_is_wrap_safe);
  return UNITY_END();
}
