#include <cstdint>
#include <unity.h>

#include "sensor_enable_policy.h"

void setUp() {}
void tearDown() {}

void test_disabled_channels() {
  TEST_ASSERT_FALSE(sensor_enable_policy::isEnabled(1));   // Flower 2
  TEST_ASSERT_FALSE(sensor_enable_policy::isEnabled(13));  // Flower 14
  TEST_ASSERT_FALSE(sensor_enable_policy::isEnabled(14));  // Flower 15
}

void test_all_other_channels_enabled() {
  for (int channel = 0; channel < sensor_enable_policy::kSensorCount; ++channel) {
    const bool expected = channel != 1 && channel != 13 && channel != 14;
    TEST_ASSERT_EQUAL_INT(expected, sensor_enable_policy::isEnabled(channel));
  }
}

void test_out_of_range_fail_closed() {
  TEST_ASSERT_FALSE(sensor_enable_policy::isEnabled(-1));
  TEST_ASSERT_FALSE(sensor_enable_policy::isEnabled(15));
  TEST_ASSERT_FALSE(sensor_enable_policy::isEnabled(255));
}

void test_active_count() {
  TEST_ASSERT_EQUAL_UINT8(15U, sensor_enable_policy::kSensorCount);
  TEST_ASSERT_EQUAL_UINT8(12U, sensor_enable_policy::kEnabledSensorCount);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_disabled_channels);
  RUN_TEST(test_all_other_channels_enabled);
  RUN_TEST(test_out_of_range_fail_closed);
  RUN_TEST(test_active_count);
  return UNITY_END();
}
