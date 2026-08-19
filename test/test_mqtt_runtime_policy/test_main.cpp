#include <cstddef>
#include <cstdint>
#include <unity.h>

#include "mqtt_runtime_policy.h"

void setUp() {}
void tearDown() {}

void test_valid_qos0_command_envelope_is_allowed() {
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(mqtt_runtime_policy::RejectReason::none),
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::validateEnvelope(4U, 0U, false)));
}

void test_valid_qos1_command_envelope_is_allowed() {
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(mqtt_runtime_policy::RejectReason::none),
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::validateEnvelope(32U, 1U, false)));
}

void test_retained_command_is_rejected_fail_closed() {
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(mqtt_runtime_policy::RejectReason::retained),
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::validateEnvelope(4U, 0U, true)));
}

void test_oversized_command_is_rejected_before_copy() {
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(mqtt_runtime_policy::RejectReason::oversized),
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::validateEnvelope(33U, 0U, false)));
}

void test_qos2_command_is_outside_runtime_contract() {
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(mqtt_runtime_policy::RejectReason::unsupportedQos),
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::validateEnvelope(4U, 2U, false)));
}

void test_contiguous_chunks_are_accepted() {
  TEST_ASSERT_TRUE(
      mqtt_runtime_policy::isChunkSequenceValid(0U, 10U, 0U, 4U, 10U));
  TEST_ASSERT_TRUE(
      mqtt_runtime_policy::isChunkSequenceValid(4U, 10U, 4U, 6U, 10U));
}

void test_gap_or_total_change_is_rejected() {
  TEST_ASSERT_FALSE(
      mqtt_runtime_policy::isChunkSequenceValid(4U, 10U, 5U, 5U, 10U));
  TEST_ASSERT_FALSE(
      mqtt_runtime_policy::isChunkSequenceValid(4U, 10U, 4U, 5U, 9U));
}

void test_chunk_overrun_is_rejected() {
  TEST_ASSERT_FALSE(
      mqtt_runtime_policy::isChunkSequenceValid(8U, 10U, 8U, 3U, 10U));
}

void test_transitional_connection_is_aborted_only_when_pump_runs() {
  TEST_ASSERT_TRUE(
      mqtt_runtime_policy::shouldAbortTransitionalConnection(
          true, false, false));
  TEST_ASSERT_FALSE(
      mqtt_runtime_policy::shouldAbortTransitionalConnection(
          false, false, false));
  TEST_ASSERT_FALSE(
      mqtt_runtime_policy::shouldAbortTransitionalConnection(
          true, true, false));
  TEST_ASSERT_FALSE(
      mqtt_runtime_policy::shouldAbortTransitionalConnection(
          true, false, true));
}

void test_timeout_and_buffer_contracts_are_explicit() {
  TEST_ASSERT_EQUAL_UINT32(1000U, mqtt_runtime_policy::kTcpConnectionTimeoutMs);
  TEST_ASSERT_EQUAL_UINT16(2U, mqtt_runtime_policy::kMqttAckTimeoutSeconds);
  TEST_ASSERT_EQUAL_UINT16(30U, mqtt_runtime_policy::kKeepAliveSeconds);
  TEST_ASSERT_EQUAL_UINT32(1024U, mqtt_runtime_policy::kRxBufferBytes);
  TEST_ASSERT_EQUAL_UINT32(1024U, mqtt_runtime_policy::kTxBufferBytes);
  TEST_ASSERT_EQUAL_UINT32(3500U, mqtt_runtime_policy::kConnectAttemptBudgetMs);
  TEST_ASSERT_EQUAL_UINT32(250U, mqtt_runtime_policy::kDisconnectCleanupBudgetMs);
}

void test_elapsed_check_is_wrap_safe() {
  TEST_ASSERT_FALSE(mqtt_runtime_policy::hasElapsed(1499U, 1000U, 500U));
  TEST_ASSERT_TRUE(mqtt_runtime_policy::hasElapsed(1500U, 1000U, 500U));

  const std::uint32_t startedAt = 0xFFFFFFF0U;
  TEST_ASSERT_FALSE(mqtt_runtime_policy::hasElapsed(0x0000000EU, startedAt, 31U));
  TEST_ASSERT_TRUE(mqtt_runtime_policy::hasElapsed(0x0000000FU, startedAt, 31U));
}

void test_best_effort_queue_is_bounded() {
  TEST_ASSERT_TRUE(mqtt_runtime_policy::canQueueBestEffort(0U));
  TEST_ASSERT_TRUE(mqtt_runtime_policy::canQueueBestEffort(3U));
  TEST_ASSERT_FALSE(mqtt_runtime_policy::canQueueBestEffort(4U));
  TEST_ASSERT_FALSE(mqtt_runtime_policy::canQueueBestEffort(100U));
}

void test_tracked_publish_storage_is_explicit() {
  TEST_ASSERT_EQUAL_UINT32(64U, mqtt_runtime_policy::kTrackedTopicBytes);
  TEST_ASSERT_EQUAL_UINT32(1024U, mqtt_runtime_policy::kTrackedPayloadBytes);
}

void test_duplicate_metadata_is_representable_without_dedup_policy() {
  const mqtt_runtime_policy::InboundMetadata metadata{1U, true, false, 77U};
  TEST_ASSERT_TRUE(metadata.duplicate);
  TEST_ASSERT_EQUAL_UINT16(77U, metadata.packetId);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_valid_qos0_command_envelope_is_allowed);
  RUN_TEST(test_valid_qos1_command_envelope_is_allowed);
  RUN_TEST(test_retained_command_is_rejected_fail_closed);
  RUN_TEST(test_oversized_command_is_rejected_before_copy);
  RUN_TEST(test_qos2_command_is_outside_runtime_contract);
  RUN_TEST(test_contiguous_chunks_are_accepted);
  RUN_TEST(test_gap_or_total_change_is_rejected);
  RUN_TEST(test_chunk_overrun_is_rejected);
  RUN_TEST(test_transitional_connection_is_aborted_only_when_pump_runs);
  RUN_TEST(test_timeout_and_buffer_contracts_are_explicit);
  RUN_TEST(test_elapsed_check_is_wrap_safe);
  RUN_TEST(test_best_effort_queue_is_bounded);
  RUN_TEST(test_tracked_publish_storage_is_explicit);
  RUN_TEST(test_duplicate_metadata_is_representable_without_dedup_policy);
  return UNITY_END();
}
