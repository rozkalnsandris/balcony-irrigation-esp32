#include <unity.h>
#include <string>
#include <cstdio>
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

void test_dequeued_record_preserves_actual_receive_age() {
  struct Command {
    bool fromHA = false;
    std::uint32_t stopEpoch = 0;
    std::uint32_t receivedAt = 0;
    std::string payload;
  };
  Command slot{true, 7U, 120000U, "ticket45:0123456789abcdef"};
  Command item;
  takeQueuedCommand(item, slot); // same transfer as firmware dequeueCommand
  TEST_ASSERT_TRUE(item.fromHA);
  TEST_ASSERT_EQUAL_UINT32(7U, item.stopEpoch);
  TEST_ASSERT_EQUAL_UINT32(120000U, item.receivedAt);
  TEST_ASSERT_EQUAL_STRING("ticket45:0123456789abcdef", item.payload.c_str());
  TEST_ASSERT_TRUE(slot.payload.empty());
  TEST_ASSERT_TRUE(queueFresh(124999U, item.receivedAt, item.stopEpoch, 7U));
  TEST_ASSERT_FALSE(queueFresh(125000U, item.receivedAt, item.stopEpoch, 7U));
  TEST_ASSERT_FALSE(queueFresh(120001U, item.receivedAt, item.stopEpoch, 8U));
  slot = Command{false, 8U, 0xfffffff0U, "statuss"};
  takeQueuedCommand(item, slot);
  TEST_ASSERT_FALSE(item.fromHA);
  TEST_ASSERT_EQUAL_STRING("statuss", item.payload.c_str());
  TEST_ASSERT_TRUE(queueFresh(0xfffffff0U + 4999U, item.receivedAt, item.stopEpoch, 8U));
  TEST_ASSERT_FALSE(queueFresh(0xfffffff0U + 5000U, item.receivedAt, item.stopEpoch, 8U));
}

void test_recent_request_does_not_refresh_or_resurrect_ticket() {
  RecentRequests recent;
  TicketGate gate;
  constexpr char first[] = "0123456789abcdef";
  constexpr char second[] = "fedcba9876543210";
  TEST_ASSERT_TRUE(recent.admit(first));
  TEST_ASSERT_TRUE(gate.issue(a, 100U));
  TEST_ASSERT_FALSE(recent.admit(first));
  TEST_ASSERT_TRUE(gate.claim(a, 10099U));
  TEST_ASSERT_EQUAL_UINT32(35001U, gate.remainingMs(10099U));
  gate.invalidate(); // STOP, session loss, failed start, or cancellation
  TEST_ASSERT_FALSE(recent.admit(first));
  TEST_ASSERT_FALSE(gate.claim(a, 10100U));
  TEST_ASSERT_TRUE(recent.admit(second));
  TEST_ASSERT_TRUE(gate.issue(b, 10200U));
  TEST_ASSERT_FALSE(recent.admit(first)); // non-consecutive redelivery
  TEST_ASSERT_FALSE(recent.admit(second));
  gate.expire(20200U);
  TEST_ASSERT_FALSE(recent.admit(second));
  TEST_ASSERT_FALSE(gate.claim(b, 20201U));
  // Even an admitted request that never produces an offer cannot be retried.
  TEST_ASSERT_TRUE(recent.admit("0000000000000001"));
  TEST_ASSERT_FALSE(recent.admit("0000000000000001"));
}

void test_request_history_has_explicit_bounded_lifetime() {
  RecentRequests recent;
  TEST_ASSERT_FALSE(recent.admit(nullptr));
  TEST_ASSERT_FALSE(recent.admit(""));
  TEST_ASSERT_FALSE(recent.admit("0123456789abcdeF"));
  TEST_ASSERT_FALSE(recent.admit("0123456789abcdef0"));
  constexpr char first[] = "0123456789abcdef";
  TEST_ASSERT_TRUE(recent.admit(first));
  for (std::size_t i = 1U; i < RecentRequests::kCapacity; ++i) {
    char id[kRequestChars + 1U] = {};
    std::snprintf(id, sizeof(id), "%016lx", static_cast<unsigned long>(i));
    TEST_ASSERT_TRUE(recent.admit(id));
    TEST_ASSERT_FALSE(recent.admit(first));
  }
  TEST_ASSERT_TRUE(recent.admit("ffffffffffffffff")); // evicts oldest
  TEST_ASSERT_TRUE(recent.admit(first)); // not global replay protection
  RecentRequests rebooted;
  TEST_ASSERT_TRUE(rebooted.admit(first)); // RAM history only
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_default_disabled_and_strict_wire);
  RUN_TEST(test_single_use_and_no_extension);
  RUN_TEST(test_stop_reconnect_ota_or_failed_start_invalidate);
  RUN_TEST(test_expiry_and_wrap);
  RUN_TEST(test_queued_stop_epoch_age_and_replacement);
  RUN_TEST(test_dequeued_record_preserves_actual_receive_age);
  RUN_TEST(test_recent_request_does_not_refresh_or_resurrect_ticket);
  RUN_TEST(test_request_history_has_explicit_bounded_lifetime);
  return UNITY_END();
}
