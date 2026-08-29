#include <unity.h>

#include "ota_error_policy.h"

void setUp() {}
void tearDown() {}

void test_pre_transfer_error_does_not_request_restart() {
  TEST_ASSERT_FALSE(ota_error_policy::shouldRestartAfterFailure(false));
}

void test_post_start_error_requests_clean_restart() {
  TEST_ASSERT_TRUE(ota_error_policy::shouldRestartAfterFailure(true));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pre_transfer_error_does_not_request_restart);
  RUN_TEST(test_post_start_error_requests_clean_restart);
  return UNITY_END();
}
