#include <unity.h>

#include "command_safety.h"

void setUp() {}
void tearDown() {}

void test_urgent_stop_contract() {
  TEST_ASSERT_TRUE(command_safety::isUrgentStop(true, "OFF"));
  TEST_ASSERT_TRUE(command_safety::isUrgentStop(false, "stop"));
  TEST_ASSERT_FALSE(command_safety::isUrgentStop(true, "ON"));
  TEST_ASSERT_FALSE(command_safety::isUrgentStop(false, "laist"));
}

void test_pump_start_classifier() {
  TEST_ASSERT_TRUE(command_safety::isPumpStartCommand(true, "ON"));
  TEST_ASSERT_TRUE(command_safety::isPumpStartCommand(false, "laist"));
  TEST_ASSERT_TRUE(command_safety::isPumpStartCommand(false, "laist_1"));
  TEST_ASSERT_TRUE(command_safety::isPumpStartCommand(false, "laist_3"));
  TEST_ASSERT_FALSE(command_safety::isPumpStartCommand(true, "OFF"));
  TEST_ASSERT_FALSE(command_safety::isPumpStartCommand(false, "statuss"));
}

void test_newer_stop_suppresses_only_stale_pump_starts() {
  TEST_ASSERT_TRUE(command_safety::shouldSuppressQueuedPumpStart(4, 5, true, "ON"));
  TEST_ASSERT_TRUE(command_safety::shouldSuppressQueuedPumpStart(4, 5, false, "laist"));
  TEST_ASSERT_TRUE(command_safety::shouldSuppressQueuedPumpStart(4, 5, false, "laist_2"));

  TEST_ASSERT_FALSE(command_safety::shouldSuppressQueuedPumpStart(4, 5, false, "statuss"));
  TEST_ASSERT_FALSE(command_safety::shouldSuppressQueuedPumpStart(4, 5, false, "mitrums"));
  TEST_ASSERT_FALSE(command_safety::shouldSuppressQueuedPumpStart(5, 5, true, "ON"));
  TEST_ASSERT_FALSE(command_safety::shouldSuppressQueuedPumpStart(5, 5, false, "laist"));
}

void test_null_payload_is_never_actionable() {
  TEST_ASSERT_FALSE(command_safety::isUrgentStop(false, nullptr));
  TEST_ASSERT_FALSE(command_safety::isPumpStartCommand(false, nullptr));
  TEST_ASSERT_FALSE(command_safety::shouldSuppressQueuedPumpStart(1, 2, false, nullptr));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_urgent_stop_contract);
  RUN_TEST(test_pump_start_classifier);
  RUN_TEST(test_newer_stop_suppresses_only_stale_pump_starts);
  RUN_TEST(test_null_payload_is_never_actionable);
  return UNITY_END();
}
