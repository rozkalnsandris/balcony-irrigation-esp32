#include <cstddef>
#include <cstdint>
#include <limits>
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
  TEST_ASSERT_EQUAL_UINT32(
      6500U,
      mqtt_runtime_policy::kCommandSubscriptionAckBudgetMs);
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

void test_tracked_publish_storage_is_explicit_and_tx_bounded() {
  TEST_ASSERT_EQUAL_UINT32(64U, mqtt_runtime_policy::kTrackedTopicBytes);
  TEST_ASSERT_EQUAL_UINT32(896U, mqtt_runtime_policy::kTrackedPayloadBytes);
  TEST_ASSERT_EQUAL_UINT32(63U, mqtt_runtime_policy::kTrackedTopicTextMaxBytes);
  TEST_ASSERT_EQUAL_UINT32(895U, mqtt_runtime_policy::kTrackedPayloadTextMaxBytes);
  TEST_ASSERT_TRUE(
      mqtt_runtime_policy::kTrackedPublishWorstCaseBytes <=
      mqtt_runtime_policy::kTxBufferBytes);
  TEST_ASSERT_EQUAL_UINT32(
      969U,
      mqtt_runtime_policy::kTrackedPublishWorstCaseBytes);
}

void test_duplicate_metadata_is_representable_without_dedup_policy() {
  const mqtt_runtime_policy::InboundMetadata metadata{1U, true, false, 77U};
  TEST_ASSERT_TRUE(metadata.duplicate);
  TEST_ASSERT_EQUAL_UINT16(77U, metadata.packetId);
}

void test_tracked_topic_capacity_accepts_63_and_rejects_64() {
  TEST_ASSERT_TRUE(mqtt_runtime_policy::canStoreTrackedTopic(0U));
  TEST_ASSERT_TRUE(mqtt_runtime_policy::canStoreTrackedTopic(63U));
  TEST_ASSERT_FALSE(mqtt_runtime_policy::canStoreTrackedTopic(64U));
  TEST_ASSERT_FALSE(mqtt_runtime_policy::canStoreTrackedTopic(
      std::numeric_limits<std::size_t>::max()));
}

void test_tracked_payload_capacity_accepts_895_and_rejects_896() {
  TEST_ASSERT_TRUE(mqtt_runtime_policy::canStoreTrackedPayload(0U));
  TEST_ASSERT_TRUE(mqtt_runtime_policy::canStoreTrackedPayload(895U));
  TEST_ASSERT_FALSE(mqtt_runtime_policy::canStoreTrackedPayload(896U));
  TEST_ASSERT_FALSE(mqtt_runtime_policy::canStoreTrackedPayload(
      std::numeric_limits<std::size_t>::max()));
}

void test_suback_exact_qos1_is_accepted() {
  const std::uint8_t returnCodes[] = {0x01U};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::SubscriptionAckResult::acceptedQos1),
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::classifySingleSubscriptionAck(
              returnCodes,
              1U)));
}

void test_suback_qos0_downgrade_is_rejected() {
  const std::uint8_t returnCodes[] = {0x00U};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::SubscriptionAckResult::rejectedQos0),
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::classifySingleSubscriptionAck(
              returnCodes,
              1U)));
}

void test_suback_qos2_anomaly_is_rejected() {
  const std::uint8_t returnCodes[] = {0x02U};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::SubscriptionAckResult::rejectedQos2),
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::classifySingleSubscriptionAck(
              returnCodes,
              1U)));
}

void test_suback_broker_failure_is_rejected() {
  const std::uint8_t returnCodes[] = {0x80U};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::SubscriptionAckResult::brokerRejected),
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::classifySingleSubscriptionAck(
              returnCodes,
              1U)));
}

void test_suback_unknown_code_is_malformed() {
  const std::uint8_t returnCodes[] = {0x7FU};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::SubscriptionAckResult::malformed),
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::classifySingleSubscriptionAck(
              returnCodes,
              1U)));
}

void test_suback_malformed_shapes_fail_closed() {
  const std::uint8_t returnCodes[] = {0x01U, 0x01U};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::SubscriptionAckResult::malformed),
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::classifySingleSubscriptionAck(nullptr, 1U)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::SubscriptionAckResult::malformed),
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::classifySingleSubscriptionAck(
              returnCodes,
              0U)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::SubscriptionAckResult::malformed),
      static_cast<std::uint8_t>(
          mqtt_runtime_policy::classifySingleSubscriptionAck(
              returnCodes,
              2U)));
}

void test_suback_budget_is_6499_pending_and_6500_timed_out() {
  TEST_ASSERT_FALSE(mqtt_runtime_policy::isSubscriptionAckTimedOut(6499U, 0U));
  TEST_ASSERT_TRUE(mqtt_runtime_policy::isSubscriptionAckTimedOut(6500U, 0U));
}

void test_suback_timeout_is_wrap_safe() {
  const std::uint32_t startedAt = 0xFFFFF000U;
  const std::uint32_t beforeDeadline = startedAt + 6499U;
  const std::uint32_t deadline = startedAt + 6500U;
  TEST_ASSERT_FALSE(
      mqtt_runtime_policy::isSubscriptionAckTimedOut(
          beforeDeadline,
          startedAt));
  TEST_ASSERT_TRUE(
      mqtt_runtime_policy::isSubscriptionAckTimedOut(
          deadline,
          startedAt));
}

void test_tracked_publish_enqueue_requires_staged_connected_and_queue_room() {
  TEST_ASSERT_TRUE(mqtt_runtime_policy::canEnqueueTrackedPublish(
      mqtt_runtime_policy::TrackedPublishPhase::staged,
      true,
      0U));
  TEST_ASSERT_TRUE(mqtt_runtime_policy::canEnqueueTrackedPublish(
      mqtt_runtime_policy::TrackedPublishPhase::staged,
      true,
      3U));
  TEST_ASSERT_FALSE(mqtt_runtime_policy::canEnqueueTrackedPublish(
      mqtt_runtime_policy::TrackedPublishPhase::empty,
      true,
      0U));
  TEST_ASSERT_FALSE(mqtt_runtime_policy::canEnqueueTrackedPublish(
      mqtt_runtime_policy::TrackedPublishPhase::inFlight,
      true,
      0U));
  TEST_ASSERT_FALSE(mqtt_runtime_policy::canEnqueueTrackedPublish(
      mqtt_runtime_policy::TrackedPublishPhase::staged,
      false,
      0U));
  TEST_ASSERT_FALSE(mqtt_runtime_policy::canEnqueueTrackedPublish(
      mqtt_runtime_policy::TrackedPublishPhase::staged,
      true,
      4U));
}

void test_tracked_publish_ack_matches_only_exact_inflight_packet() {
  TEST_ASSERT_TRUE(mqtt_runtime_policy::trackedPublishAckMatches(
      mqtt_runtime_policy::TrackedPublishPhase::inFlight,
      77U,
      77U));
  TEST_ASSERT_FALSE(mqtt_runtime_policy::trackedPublishAckMatches(
      mqtt_runtime_policy::TrackedPublishPhase::inFlight,
      77U,
      78U));
  TEST_ASSERT_FALSE(mqtt_runtime_policy::trackedPublishAckMatches(
      mqtt_runtime_policy::TrackedPublishPhase::staged,
      77U,
      77U));
  TEST_ASSERT_FALSE(mqtt_runtime_policy::trackedPublishAckMatches(
      mqtt_runtime_policy::TrackedPublishPhase::empty,
      77U,
      77U));
  TEST_ASSERT_FALSE(mqtt_runtime_policy::trackedPublishAckMatches(
      mqtt_runtime_policy::TrackedPublishPhase::inFlight,
      0U,
      0U));
}

void test_tracked_publish_state_represents_empty_staged_and_inflight() {
  const mqtt_runtime_policy::TrackedPublishState empty{
      mqtt_runtime_policy::TrackedPublishPhase::empty,
      0U};
  const mqtt_runtime_policy::TrackedPublishState staged{
      mqtt_runtime_policy::TrackedPublishPhase::staged,
      0U};
  const mqtt_runtime_policy::TrackedPublishState inFlight{
      mqtt_runtime_policy::TrackedPublishPhase::inFlight,
      42U};

  TEST_ASSERT_NOT_EQUAL(
      static_cast<std::uint8_t>(empty.phase),
      static_cast<std::uint8_t>(staged.phase));
  TEST_ASSERT_NOT_EQUAL(
      static_cast<std::uint8_t>(staged.phase),
      static_cast<std::uint8_t>(inFlight.phase));
  TEST_ASSERT_EQUAL_UINT16(42U, inFlight.packetId);
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
  RUN_TEST(test_tracked_publish_storage_is_explicit_and_tx_bounded);
  RUN_TEST(test_duplicate_metadata_is_representable_without_dedup_policy);
  RUN_TEST(test_tracked_topic_capacity_accepts_63_and_rejects_64);
  RUN_TEST(test_tracked_payload_capacity_accepts_895_and_rejects_896);
  RUN_TEST(test_suback_exact_qos1_is_accepted);
  RUN_TEST(test_suback_qos0_downgrade_is_rejected);
  RUN_TEST(test_suback_qos2_anomaly_is_rejected);
  RUN_TEST(test_suback_broker_failure_is_rejected);
  RUN_TEST(test_suback_unknown_code_is_malformed);
  RUN_TEST(test_suback_malformed_shapes_fail_closed);
  RUN_TEST(test_suback_budget_is_6499_pending_and_6500_timed_out);
  RUN_TEST(test_suback_timeout_is_wrap_safe);
  RUN_TEST(test_tracked_publish_enqueue_requires_staged_connected_and_queue_room);
  RUN_TEST(test_tracked_publish_ack_matches_only_exact_inflight_packet);
  RUN_TEST(test_tracked_publish_state_represents_empty_staged_and_inflight);
  return UNITY_END();
}
