#include <cstddef>
#include <unity.h>

#include "command_payload_policy.h"

void setUp() {}
void tearDown() {}

void test_payload_below_limit_is_allowed() {
  TEST_ASSERT_TRUE(command_payload_policy::isCommandPayloadLengthAllowed(31U));
}

void test_payload_at_limit_is_allowed() {
  TEST_ASSERT_TRUE(command_payload_policy::isCommandPayloadLengthAllowed(
      command_payload_policy::kMaxCommandPayloadBytes));
}

void test_payload_above_limit_is_rejected() {
  TEST_ASSERT_FALSE(command_payload_policy::isCommandPayloadLengthAllowed(33U));
}

void test_empty_payload_preserves_existing_unknown_command_behavior() {
  TEST_ASSERT_TRUE(command_payload_policy::isCommandPayloadLengthAllowed(0U));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_payload_below_limit_is_allowed);
  RUN_TEST(test_payload_at_limit_is_allowed);
  RUN_TEST(test_payload_above_limit_is_rejected);
  RUN_TEST(test_empty_payload_preserves_existing_unknown_command_behavior);
  return UNITY_END();
}
