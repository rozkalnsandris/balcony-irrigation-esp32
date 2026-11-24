#include <unity.h>
#include "ha_bounded_watering_policy.h"

using namespace ha_bounded_watering;
constexpr char a[] = "0123456789abcdef00000001";
constexpr char b[] = "0123456789abcdef00000002";
constexpr char c[] = "fedcba987654321000000001";

void setUp() {}
void tearDown() {}

void test_default_disabled_and_strict_wire() {
  TEST_ASSERT_FALSE(kEnabled);
  TEST_ASSERT_EQUAL_UINT32(45U, kCycleSeconds);
  constexpr char offer[] = "ticket45:0123456789abcdef";
  constexpr char start[] = "start45:0123456789abcdef00000001";
  TEST_ASSERT_TRUE(decode(true, false, offer, sizeof(offer)-1U) == Kind::ticket);
  TEST_ASSERT_TRUE(decode(true, false, start, sizeof(start)-1U) == Kind::start);
  TEST_ASSERT_TRUE(decode(false, false, start, sizeof(start)-1U) == Kind::invalid);
  TEST_ASSERT_TRUE(decode(true, true, start, sizeof(start)-1U) == Kind::invalid);
  TEST_ASSERT_TRUE(decode(true, false, start, sizeof(start)) == Kind::invalid);
  TEST_ASSERT_TRUE(decode(true, false, start, sizeof(start)-2U) == Kind::invalid);
  TEST_ASSERT_TRUE(decode(true, false, "ON", 2U) == Kind::invalid);
  TEST_ASSERT_TRUE(decode(true, false, "laist", 5U) == Kind::invalid);
  char malformed[sizeof(start)] = {};
  std::memcpy(malformed, start, sizeof(start));
  malformed[12] = '\0';
  TEST_ASSERT_TRUE(decode(true, false, malformed, sizeof(start)-1U) == Kind::invalid);
  malformed[12] = 'A';
  TEST_ASSERT_TRUE(decode(true, false, malformed, sizeof(start)-1U) == Kind::invalid);
}

void test_single_use_and_no_extension() {
  TicketGate gate;
  TEST_ASSERT_FALSE(gate.claim(a, 1U));
  TEST_ASSERT_TRUE(gate.issue(a, 100U));
  TEST_ASSERT_FALSE(gate.claim(b, 101U));
  TEST_ASSERT_TRUE(gate.claim(a, 102U));
  TEST_ASSERT_FALSE(gate.claim(a, 103U));
  TEST_ASSERT_FALSE(gate.issue(b, 104U));
  TEST_ASSERT_EQUAL_UINT32(44998U, gate.remainingMs(102U));
  TEST_ASSERT_EQUAL_UINT32(40000U, gate.remainingMs(5100U));
  gate.invalidate();
  TEST_ASSERT_FALSE(gate.claim(a, 5101U));
  TEST_ASSERT_FALSE(gate.issue(a, 5102U));
  TEST_ASSERT_TRUE(gate.issue(b, 5103U));
  TEST_ASSERT_FALSE(gate.claim(a, 5104U));
  TEST_ASSERT_TRUE(gate.claim(b, 5105U));
}

void test_stop_reconnect_ota_or_failed_start_invalidate() {
  TicketGate gate;
  TEST_ASSERT_TRUE(gate.issue(a, 1U));
  gate.invalidate();
  TEST_ASSERT_FALSE(gate.claim(a, 2U));
  TEST_ASSERT_TRUE(gate.issue(b, 3U));
  TEST_ASSERT_TRUE(gate.claim(b, 4U));
  gate.invalidate();  // STOP received during pre-relay network work
  TEST_ASSERT_FALSE(gate.claimFresh(5U));
  TEST_ASSERT_EQUAL_UINT32(0U, gate.remainingMs(5U));
  TEST_ASSERT_FALSE(gate.claim(b, 6U));
  TicketGate rebooted;
  TEST_ASSERT_TRUE(rebooted.issue(c, 1U));
  TEST_ASSERT_FALSE(rebooted.claim(a, 2U));
  TEST_ASSERT_FALSE(rebooted.claim(b, 2U));
  TEST_ASSERT_TRUE(rebooted.claim(c, 3U));
}

void test_expiry_and_wrap() {
  TicketGate gate;
  constexpr std::uint32_t issued = 0xfffffff0U;
  TEST_ASSERT_TRUE(gate.issue(a, issued));
  TEST_ASSERT_TRUE(gate.claim(a, issued + 9999U));
  TEST_ASSERT_EQUAL_UINT32(35001U, gate.remainingMs(issued + 9999U));
  TEST_ASSERT_FALSE(gate.claimFresh(issued + 10000U));
  TEST_ASSERT_EQUAL_UINT32(0U, gate.remainingMs(issued + 10000U));
  gate.expire(issued + 10000U);
  TEST_ASSERT_FALSE(gate.claim(a, issued + 10001U));
  TEST_ASSERT_TRUE(gate.issue(b, 100U));
  TEST_ASSERT_FALSE(gate.claim(b, 10100U));
}

void test_queued_stop_epoch_age_and_replacement() {
  TEST_ASSERT_TRUE(queueFresh(20U, 10U, 1U, 1U));
  TEST_ASSERT_FALSE(queueFresh(20U, 10U, 1U, 2U));
  TEST_ASSERT_FALSE(queueFresh(5010U, 10U, 1U, 1U));
  TEST_ASSERT_TRUE(queueFresh(4U, 0xfffffffeU, 1U, 1U));
  TicketGate gate;
  TEST_ASSERT_TRUE(gate.issue(a, 1U));
  TEST_ASSERT_TRUE(gate.issue(b, 2U));
  TEST_ASSERT_FALSE(gate.claim(a, 3U));
  TEST_ASSERT_TRUE(gate.claim(b, 4U));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_default_disabled_and_strict_wire);
  RUN_TEST(test_single_use_and_no_extension);
  RUN_TEST(test_stop_reconnect_ota_or_failed_start_invalidate);
  RUN_TEST(test_expiry_and_wrap);
  RUN_TEST(test_queued_stop_epoch_age_and_replacement);
  return UNITY_END();
}
