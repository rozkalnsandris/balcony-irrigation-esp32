#include <unity.h>
#include <initializer_list>
#include "ha_cutoff_policy.h"

using namespace ha_cutoff;

void setUp() {}
void tearDown() {}

void test_budget_is_shortened_never_extended() {
  TEST_ASSERT_EQUAL_UINT64(44000000ULL, alarmTicks(45000U));
  TEST_ASSERT_EQUAL_UINT64(34000000ULL, alarmTicks(35000U));
  TEST_ASSERT_EQUAL_UINT64(0ULL, alarmTicks(45001U));
  TEST_ASSERT_EQUAL_UINT64(0ULL, alarmTicks(1100U));
  TEST_ASSERT_EQUAL_UINT64(101000ULL, alarmTicks(1101U));
  TEST_ASSERT_EQUAL_UINT64(0ULL, alarmTicks(0U));
  ha_bounded_watering::TicketGate ticket;
  TEST_ASSERT_TRUE(ticket.issue("0123456789abcdef00000001", 0xfffffff0U));
  TEST_ASSERT_TRUE(ticket.claim("0123456789abcdef00000001", 0xfffffff0U + 9000U));
  TEST_ASSERT_EQUAL_UINT64(35000000ULL,
      alarmTicks(ticket.remainingMs(0xfffffff0U + 9000U)));
}

void test_late_arm_or_start_window() {
  TEST_ASSERT_TRUE(startWindow(1000U, 1000000U, 10001U, 10000U));
  TEST_ASSERT_FALSE(startWindow(1000U, 1000000U, 10100U, 10000U));
  TEST_ASSERT_FALSE(startWindow(900000U, 1000000U, 10001U, 10000U));
  TEST_ASSERT_FALSE(startWindow(1000000U, 1000000U, 10001U, 10000U));
  TEST_ASSERT_FALSE(startWindow(1000001U, 1000000U, 10001U, 10000U));
  TEST_ASSERT_TRUE(startWindow(1000U, 1000000U, 0x20U, 0xfffffff0U));
  TEST_ASSERT_FALSE(startWindow(1000U, 1000000U, 0xfffffff0U + 100U, 0xfffffff0U));
}

void test_alarm_before_on_blocks_late_on() {
  State state;
  TEST_ASSERT_TRUE(state.arm());
  TEST_ASSERT_FALSE(state.arm());
  state.alarm();
  TEST_ASSERT_TRUE(state.tripped());
  TEST_ASSERT_FALSE(state.start(true));
  TEST_ASSERT_FALSE(state.arm());
  state.reconcile();
  TEST_ASSERT_TRUE(state.idle());
}

void test_alarm_while_on_and_no_extension() {
  State state;
  TEST_ASSERT_TRUE(state.arm());
  TEST_ASSERT_TRUE(state.start(true));
  TEST_ASSERT_TRUE(state.running());
  TEST_ASSERT_FALSE(state.arm());
  TEST_ASSERT_FALSE(state.start(true));
  state.alarm();
  TEST_ASSERT_FALSE(state.running());
  TEST_ASSERT_TRUE(state.tripped());
  TEST_ASSERT_FALSE(state.start(true));
  state.cancel();
  TEST_ASSERT_TRUE(state.tripped());
  state.reconcile();
  TEST_ASSERT_TRUE(state.idle());
}

void test_stop_before_or_during_on_holds_alarm_until_callback() {
  for (bool running : {false, true}) {
    State state;
    TEST_ASSERT_TRUE(state.arm());
    if (running) TEST_ASSERT_TRUE(state.start(true));
    state.cancel();
    TEST_ASSERT_FALSE(state.running());
    state.reconcile();
    TEST_ASSERT_FALSE(state.idle());
    TEST_ASSERT_FALSE(state.start(true));
    TEST_ASSERT_FALSE(state.arm());
    state.alarm();
    TEST_ASSERT_TRUE(state.tripped());
    state.reconcile();
    TEST_ASSERT_TRUE(state.idle());
  }
}

void test_fault_and_unexpected_irq_never_rearm() {
  State state;
  TEST_ASSERT_TRUE(state.arm());
  TEST_ASSERT_FALSE(state.start(false));
  state.fault(); // failed driver arm or raw-count read
  state.alarm(); // uncertain/late callback
  state.reconcile();
  TEST_ASSERT_TRUE(state.faulted());
  TEST_ASSERT_FALSE(state.arm());
  TEST_ASSERT_FALSE(state.start(true));
  State unexpected;
  unexpected.alarm();
  TEST_ASSERT_TRUE(unexpected.faulted());
  TEST_ASSERT_FALSE(unexpected.arm());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_budget_is_shortened_never_extended);
  RUN_TEST(test_late_arm_or_start_window);
  RUN_TEST(test_alarm_before_on_blocks_late_on);
  RUN_TEST(test_alarm_while_on_and_no_extension);
  RUN_TEST(test_stop_before_or_during_on_holds_alarm_until_callback);
  RUN_TEST(test_fault_and_unexpected_irq_never_rearm);
  return UNITY_END();
}
