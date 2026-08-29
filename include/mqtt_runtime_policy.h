#pragma once

#include <cstddef>
#include <cstdint>

#include "command_payload_policy.h"

namespace mqtt_runtime_policy {

constexpr std::uint32_t kTcpConnectionTimeoutMs = 1000U;
constexpr std::uint16_t kMqttAckTimeoutSeconds = 2U;
constexpr std::uint16_t kKeepAliveSeconds = 30U;
constexpr std::size_t kRxBufferBytes = 1024U;
constexpr std::size_t kTxBufferBytes = 1024U;

constexpr std::uint32_t kConnectAttemptBudgetMs =
    kTcpConnectionTimeoutMs +
    static_cast<std::uint32_t>(kMqttAckTimeoutSeconds) * 1000U +
    500U;
constexpr std::uint32_t kDisconnectCleanupBudgetMs = 250U;
constexpr std::uint32_t kCommandSubscriptionAckBudgetMs = 6500U;
constexpr std::size_t kMaxBestEffortQueuePackets = 4U;
constexpr std::size_t kTrackedTopicBytes = 64U;
constexpr std::size_t kTrackedPayloadBytes = 896U;
constexpr std::size_t kTrackedTopicTextMaxBytes = kTrackedTopicBytes - 1U;
constexpr std::size_t kTrackedPayloadTextMaxBytes = kTrackedPayloadBytes - 1U;

constexpr std::size_t kMqttFixedHeaderBytes = 1U;
constexpr std::size_t kMqttRemainingLengthMaxBytes = 4U;
constexpr std::size_t kMqttTopicLengthFieldBytes = 2U;
constexpr std::size_t kMqttPacketIdBytes = 2U;
constexpr std::size_t kTrackedPublishWorstCaseBytes =
    kMqttFixedHeaderBytes +
    kMqttRemainingLengthMaxBytes +
    kMqttTopicLengthFieldBytes +
    kTrackedTopicBytes +
    kMqttPacketIdBytes +
    kTrackedPayloadBytes;

static_assert(
    kTrackedPublishWorstCaseBytes <= kTxBufferBytes,
    "tracked QoS1 publish must fit the explicit MQTT TX buffer");

enum class RejectReason : std::uint8_t {
  none = 0,
  oversized = 1,
  retained = 2,
  unsupportedQos = 3,
  malformedChunks = 4,
};

enum class SubscriptionAckResult : std::uint8_t {
  none = 0,
  acceptedQos1 = 1,
  rejectedQos0 = 2,
  rejectedQos2 = 3,
  brokerRejected = 4,
  malformed = 5,
  timedOut = 6,
};

enum class TrackedSubscriptionState : std::uint8_t {
  idle = 0,
  awaitingAck = 1,
  accepted = 2,
  rejected = 3,
};

enum class TrackedPublishPhase : std::uint8_t {
  empty = 0,
  staged = 1,
  inFlight = 2,
};

struct InboundMetadata {
  std::uint8_t qos;
  bool duplicate;
  bool retained;
  std::uint16_t packetId;
};

struct TrackedPublishState {
  TrackedPublishPhase phase;
  std::uint16_t packetId;
};

struct TrackedSubscriptionTracker {
  TrackedSubscriptionState state;
  SubscriptionAckResult result;
  std::uint16_t packetId;
  std::uint32_t startedAt;
};

inline RejectReason validateEnvelope(
    std::size_t totalBytes,
    std::uint8_t qos,
    bool retained) {
  if (retained) {
    return RejectReason::retained;
  }

  if (qos > 1U) {
    return RejectReason::unsupportedQos;
  }

  if (!command_payload_policy::isCommandPayloadLengthAllowed(totalBytes)) {
    return RejectReason::oversized;
  }

  return RejectReason::none;
}

inline bool isChunkSequenceValid(
    std::size_t expectedIndex,
    std::size_t expectedTotal,
    std::size_t index,
    std::size_t len,
    std::size_t total) {
  if (total != expectedTotal || index != expectedIndex) {
    return false;
  }

  if (index > total || len > total - index) {
    return false;
  }

  return true;
}

inline bool shouldAbortTransitionalConnection(
    bool pumpRunning,
    bool connected,
    bool disconnected) {
  return pumpRunning && !connected && !disconnected;
}

inline bool hasElapsed(
    std::uint32_t now,
    std::uint32_t startedAt,
    std::uint32_t durationMs) {
  return static_cast<std::uint32_t>(now - startedAt) >= durationMs;
}

inline bool canQueueBestEffort(std::size_t currentQueuePackets) {
  return currentQueuePackets < kMaxBestEffortQueuePackets;
}

inline bool canStoreTrackedTopic(std::size_t length) {
  return length <= kTrackedTopicTextMaxBytes;
}

inline bool canStoreTrackedPayload(std::size_t length) {
  return length <= kTrackedPayloadTextMaxBytes;
}

inline SubscriptionAckResult classifySingleSubscriptionAck(
    const std::uint8_t* returnCodes,
    std::size_t count) {
  if (returnCodes == nullptr || count != 1U) {
    return SubscriptionAckResult::malformed;
  }

  switch (returnCodes[0]) {
    case 0x01U:
      return SubscriptionAckResult::acceptedQos1;
    case 0x00U:
      return SubscriptionAckResult::rejectedQos0;
    case 0x02U:
      return SubscriptionAckResult::rejectedQos2;
    case 0x80U:
      return SubscriptionAckResult::brokerRejected;
    default:
      return SubscriptionAckResult::malformed;
  }
}

inline bool isSubscriptionAckTimedOut(
    std::uint32_t now,
    std::uint32_t startedAt) {
  return hasElapsed(now, startedAt, kCommandSubscriptionAckBudgetMs);
}

inline TrackedSubscriptionTracker resetTrackedSubscriptionTracker() {
  return {
      TrackedSubscriptionState::idle,
      SubscriptionAckResult::none,
      0U,
      0U,
  };
}

inline bool canArmTrackedSubscription(
    const TrackedSubscriptionTracker& tracker,
    std::uint16_t packetId) {
  return tracker.state == TrackedSubscriptionState::idle && packetId != 0U;
}

inline TrackedSubscriptionTracker armTrackedSubscription(
    const TrackedSubscriptionTracker& tracker,
    std::uint16_t packetId,
    std::uint32_t startedAt) {
  if (!canArmTrackedSubscription(tracker, packetId)) {
    return tracker;
  }

  return {
      TrackedSubscriptionState::awaitingAck,
      SubscriptionAckResult::none,
      packetId,
      startedAt,
  };
}

inline TrackedSubscriptionTracker latchTrackedSubscriptionTimeout(
    TrackedSubscriptionTracker tracker,
    std::uint32_t now) {
  if (tracker.state == TrackedSubscriptionState::awaitingAck &&
      isSubscriptionAckTimedOut(now, tracker.startedAt)) {
    tracker.state = TrackedSubscriptionState::rejected;
    tracker.result = SubscriptionAckResult::timedOut;
  }

  return tracker;
}

inline TrackedSubscriptionTracker applyTrackedSubscriptionAck(
    TrackedSubscriptionTracker tracker,
    std::uint16_t packetId,
    const std::uint8_t* returnCodes,
    std::size_t count,
    std::uint32_t now) {
  tracker = latchTrackedSubscriptionTimeout(tracker, now);

  if (tracker.state != TrackedSubscriptionState::awaitingAck ||
      packetId != tracker.packetId) {
    return tracker;
  }

  tracker.result = classifySingleSubscriptionAck(returnCodes, count);
  tracker.state = tracker.result == SubscriptionAckResult::acceptedQos1
                      ? TrackedSubscriptionState::accepted
                      : TrackedSubscriptionState::rejected;
  return tracker;
}

inline bool canEnqueueTrackedPublish(
    TrackedPublishPhase phase,
    bool connected,
    std::size_t currentQueuePackets) {
  return phase == TrackedPublishPhase::staged && connected &&
         canQueueBestEffort(currentQueuePackets);
}

inline bool trackedPublishAckMatches(
    TrackedPublishPhase phase,
    std::uint16_t trackedPacketId,
    std::uint16_t ackPacketId) {
  return phase == TrackedPublishPhase::inFlight &&
         trackedPacketId != 0U && trackedPacketId == ackPacketId;
}

inline TrackedPublishState stageTrackedPublishState() {
  return {TrackedPublishPhase::staged, 0U};
}

inline TrackedPublishState recordTrackedPublishEnqueue(
    TrackedPublishState state,
    std::uint16_t packetId) {
  if (state.phase == TrackedPublishPhase::staged && packetId != 0U) {
    state.phase = TrackedPublishPhase::inFlight;
    state.packetId = packetId;
  }

  return state;
}

inline TrackedPublishState preserveTrackedPublishOnDisconnect(
    TrackedPublishState state) {
  return state;
}

inline TrackedPublishState applyTrackedPublishAck(
    TrackedPublishState state,
    std::uint16_t ackPacketId) {
  if (trackedPublishAckMatches(state.phase, state.packetId, ackPacketId)) {
    return {TrackedPublishPhase::empty, 0U};
  }

  return state;
}

}  // namespace mqtt_runtime_policy
