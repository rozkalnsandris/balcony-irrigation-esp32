#include <unity.h>
#include "ha_fresh_offer_policy.h"

using namespace ha_fresh_offer;

constexpr char request[] = "0123456789abcdef";
constexpr char ticket[] = "fedcba987654321000000001";
constexpr char revision[] = "0123456789abcdef0123456789abcdef01234567";
constexpr int channels[] = {0, 2, 3, 4, 5, 6, 7, 8, 9, 11};
constexpr const char* categories[] = {
    "sauss", "mitrs", "videjs", "", "mitrs", "sauss",
    "mitrs", "sauss", "mitrs", ""};

void setUp() {}
void tearDown() {}

Scan completeScan() {
  Scan scan;
  for (std::size_t i = 0; i < kSampleCount; ++i) {
    TEST_ASSERT_TRUE(scan.append(channels[i], categories[i]));
  }
  return scan;
}

void test_exact_atomic_offer() {
  const Scan scan = completeScan();
  TEST_ASSERT_TRUE(scan.complete());
  TEST_ASSERT_EQUAL_STRING("DWMUWDWDWU", scan.codes());
  char reply[kOfferChars + 1U] = {};
  TEST_ASSERT_TRUE(formatOffer(reply, sizeof(reply), request, ticket, revision, scan));
  TEST_ASSERT_EQUAL_UINT32(kOfferChars, std::strlen(reply));
  TEST_ASSERT_EQUAL_STRING(
      "offer2:0123456789abcdef:fedcba987654321000000001:"
      "0123456789abcdef0123456789abcdef01234567:DWMUWDWDWU:off", reply);
  TEST_ASSERT_FALSE(formatOffer(reply, sizeof(reply)-1U, request, ticket, revision, scan));
  TEST_ASSERT_EQUAL_STRING("", reply);
}

void test_no_partial_duplicate_or_retired_scan() {
  Scan incomplete;
  TEST_ASSERT_TRUE(incomplete.append(0, "sauss"));
  char reply[kOfferChars + 1U] = {};
  TEST_ASSERT_FALSE(formatOffer(reply, sizeof(reply), request, ticket, revision, incomplete));
  TEST_ASSERT_FALSE(incomplete.append(0, "mitrs"));
  TEST_ASSERT_FALSE(incomplete.append(2, "mitrs"));
  Scan retired;
  TEST_ASSERT_TRUE(retired.append(0, "mitrs"));
  TEST_ASSERT_FALSE(retired.append(1, "sauss"));
  Scan missing;
  TEST_ASSERT_FALSE(missing.append(2, "sauss"));
  Scan invalid;
  TEST_ASSERT_FALSE(invalid.append(0, "unexpected"));
  TEST_ASSERT_FALSE(invalid.complete());
  Scan extra = completeScan();
  TEST_ASSERT_FALSE(extra.append(12, ""));
  TEST_ASSERT_FALSE(extra.complete());
}

void test_identity_strictness() {
  const Scan scan = completeScan();
  char reply[kOfferChars + 1U] = {};
  TEST_ASSERT_FALSE(formatOffer(reply, sizeof(reply), request, ticket, "unknown", scan));
  TEST_ASSERT_FALSE(formatOffer(reply, sizeof(reply), request, ticket,
      "0123456789abcdef0123456789abcdef01234567-dirty", scan));
  TEST_ASSERT_FALSE(formatOffer(reply, sizeof(reply), "0123456789abcdeF", ticket, revision, scan));
  TEST_ASSERT_FALSE(formatOffer(reply, sizeof(reply), request, "short", revision, scan));
  TEST_ASSERT_FALSE(formatOffer(reply, sizeof(reply), nullptr, ticket, revision, scan));
  TEST_ASSERT_FALSE(formatOffer(nullptr, sizeof(reply), request, ticket, revision, scan));
}

void test_stop_session_and_elapsed() {
  TEST_ASSERT_TRUE(scanCurrent(100U, 100U, 7U, 7U, true));
  TEST_ASSERT_TRUE(scanCurrent(2099U, 100U, 7U, 7U, true));
  TEST_ASSERT_FALSE(scanCurrent(2100U, 100U, 7U, 7U, true));
  TEST_ASSERT_FALSE(scanCurrent(101U, 100U, 7U, 8U, true));
  TEST_ASSERT_FALSE(scanCurrent(101U, 100U, 7U, 7U, false));
  constexpr std::uint32_t start = 0xfffffff0U;
  TEST_ASSERT_TRUE(scanCurrent(start + 1999U, start, 7U, 7U, true));
  TEST_ASSERT_FALSE(scanCurrent(start + 2000U, start, 7U, 7U, true));
}

void test_scan_age_counts_against_ticket_and_cycle() {
  ha_bounded_watering::TicketGate gate;
  // Scan starts at 100; offer may be published at 2000.
  TEST_ASSERT_TRUE(gate.issue(ticket, 100U));
  TEST_ASSERT_TRUE(gate.claim(ticket, 2000U));
  TEST_ASSERT_EQUAL_UINT32(43100U, gate.remainingMs(2000U));
  gate.invalidate();
  TEST_ASSERT_FALSE(gate.claim(ticket, 2001U));
  ha_bounded_watering::TicketGate late;
  TEST_ASSERT_TRUE(late.issue(ticket, 100U));
  TEST_ASSERT_FALSE(late.claim(ticket, 10100U));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_exact_atomic_offer);
  RUN_TEST(test_no_partial_duplicate_or_retired_scan);
  RUN_TEST(test_identity_strictness);
  RUN_TEST(test_stop_session_and_elapsed);
  RUN_TEST(test_scan_age_counts_against_ticket_and_cycle);
  return UNITY_END();
}
