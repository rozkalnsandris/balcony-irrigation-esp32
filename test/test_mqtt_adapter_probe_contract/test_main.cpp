#include <cstddef>
#include <cstdint>
#include <unity.h>

#include "mqtt_adapter_probe_contract.h"

using mqtt_adapter_probe_contract::InboundMetadata;

void setUp() {}
void tearDown() {}

void test_zero_length_qos0_nonretained_command_is_allowed() {
  const InboundMetadata metadata{0U, false, false, 0U, 0U};
  TEST_ASSERT_TRUE(
      mqtt_adapter_probe_contract::isCommandEnvelopeAllowed(metadata));
}

void test_maximum_length_qos1_nonretained_command_is_allowed() {
  const InboundMetadata metadata{1U, false, false, 42U, 32U};
  TEST_ASSERT_TRUE(
      mqtt_adapter_probe_contract::isCommandEnvelopeAllowed(metadata));
}

void test_oversized_command_is_rejected_before_payload_copy() {
  const InboundMetadata metadata{0U, false, false, 0U, 33U};
  TEST_ASSERT_FALSE(
      mqtt_adapter_probe_contract::isCommandEnvelopeAllowed(metadata));
}

void test_retained_command_is_rejected_fail_closed() {
  const InboundMetadata metadata{0U, false, true, 0U, 4U};
  TEST_ASSERT_FALSE(
      mqtt_adapter_probe_contract::isCommandEnvelopeAllowed(metadata));
}

void test_qos2_command_is_outside_current_contract() {
  const InboundMetadata metadata{2U, false, false, 11U, 4U};
  TEST_ASSERT_FALSE(
      mqtt_adapter_probe_contract::isCommandEnvelopeAllowed(metadata));
}

void test_duplicate_metadata_is_surfaced_not_silently_rejected() {
  const InboundMetadata metadata{1U, true, false, 77U, 4U};
  TEST_ASSERT_TRUE(metadata.duplicate);
  TEST_ASSERT_EQUAL_UINT16(77U, metadata.packetId);
  TEST_ASSERT_TRUE(
      mqtt_adapter_probe_contract::isCommandEnvelopeAllowed(metadata));
}

void test_probe_client_id_is_mqtt311_mandatory_compatible() {
  TEST_ASSERT_TRUE(mqtt_adapter_probe_contract::isProbeClientIdCompatible());
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(
      mqtt_adapter_probe_contract::kMqtt311MandatoryClientIdMaxBytes,
      sizeof(mqtt_adapter_probe_contract::kProbeClientId) - 1U);
}

void test_probe_timeouts_are_explicit_and_bounded() {
  TEST_ASSERT_EQUAL_UINT16(
      1U,
      mqtt_adapter_probe_contract::kOperationTimeoutSeconds);
  TEST_ASSERT_EQUAL_UINT32(
      1000U,
      mqtt_adapter_probe_contract::kTcpConnectionTimeoutMs);
  TEST_ASSERT_EQUAL_UINT16(
      30U,
      mqtt_adapter_probe_contract::kKeepAliveSeconds);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_zero_length_qos0_nonretained_command_is_allowed);
  RUN_TEST(test_maximum_length_qos1_nonretained_command_is_allowed);
  RUN_TEST(test_oversized_command_is_rejected_before_payload_copy);
  RUN_TEST(test_retained_command_is_rejected_fail_closed);
  RUN_TEST(test_qos2_command_is_outside_current_contract);
  RUN_TEST(test_duplicate_metadata_is_surfaced_not_silently_rejected);
  RUN_TEST(test_probe_client_id_is_mqtt311_mandatory_compatible);
  RUN_TEST(test_probe_timeouts_are_explicit_and_bounded);
  return UNITY_END();
}
