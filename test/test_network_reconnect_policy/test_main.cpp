#include <cstdint>
#include <unity.h>

#include "network_reconnect_policy.h"

void setUp() {}
void tearDown() {}

void test_reconnect_requires_wifi_and_disconnected_mqtt() {
  constexpr std::uint32_t now = 10000U;
  constexpr std::uint32_t lastAttempt = 5000U;
  constexpr std::uint32_t interval = 5000U;

  TEST_ASSERT_FALSE(network_policy::shouldAttemptMqttReconnect(
      false, false, false, now, lastAttempt, interval));
  TEST_ASSERT_FALSE(network_policy::shouldAttemptMqttReconnect(
      true, true, false, now, lastAttempt, interval));
  TEST_ASSERT_TRUE(network_policy::shouldAttemptMqttReconnect(
      true, false, false, now, lastAttempt, interval));
}

void test_reconnect_is_suppressed_while_pump_runs() {
  TEST_ASSERT_FALSE(network_policy::shouldAttemptMqttReconnect(
      true, false, true, 10000U, 0U, 5000U));
}

void test_reconnect_waits_for_interval_boundary() {
  TEST_ASSERT_FALSE(network_policy::shouldAttemptMqttReconnect(
      true, false, false, 9999U, 5000U, 5000U));
  TEST_ASSERT_TRUE(network_policy::shouldAttemptMqttReconnect(
      true, false, false, 10000U, 5000U, 5000U));
}

void test_reconnect_interval_is_wrap_safe() {
  constexpr std::uint32_t lastAttempt = 0xFFFFFF00U;

  TEST_ASSERT_FALSE(network_policy::reconnectIntervalElapsed(
      4743U, lastAttempt, 5000U));
  TEST_ASSERT_TRUE(network_policy::reconnectIntervalElapsed(
      4744U, lastAttempt, 5000U));
  TEST_ASSERT_TRUE(network_policy::shouldAttemptMqttReconnect(
      true, false, false, 4744U, lastAttempt, 5000U));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_reconnect_requires_wifi_and_disconnected_mqtt);
  RUN_TEST(test_reconnect_is_suppressed_while_pump_runs);
  RUN_TEST(test_reconnect_waits_for_interval_boundary);
  RUN_TEST(test_reconnect_interval_is_wrap_safe);
  return UNITY_END();
}
