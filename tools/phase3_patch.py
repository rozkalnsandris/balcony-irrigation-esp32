from pathlib import Path

PATH = Path("src/main.cpp")
text = PATH.read_text(encoding="utf-8")


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"[STOP] {label}: expected exactly 1 match, got {count}")
    text = text.replace(old, new, 1)


replace_once(
    '#include "command_safety.h"\n#include "network_reconnect_policy.h"',
    '#include "command_safety.h"\n#include "command_payload_policy.h"\n#include "network_reconnect_policy.h"',
    "payload policy include",
)

replace_once(
    'uint8_t commandQueueCount = 0;\n\n// Katrs urgent STOP/OFF palielina epoch.',
    'uint8_t commandQueueCount = 0;\n\nuint32_t oversizedCommandDrops = 0;\n\n// Katrs urgent STOP/OFF palielina epoch.',
    "oversized command counter",
)

old_callback = '''void mqttCallback(
  char* topic,
  byte* payload,
  unsigned int length
) {

  String message;

  message.reserve(
    length
  );

  for (
      unsigned int i = 0;
      i < length;
      i++
  ) {
    message +=
        static_cast<char>(
          payload[i]
        );
  }

  message.trim();

  String topicString(
    topic
  );

  bool fromHA = false;
  bool recognizedTopic = false;

  if (
      topicString ==
      T_PUMP_CMD
  ) {

    fromHA = true;
    recognizedTopic = true;

  } else if (
      topicString ==
      T_CMD
  ) {

    recognizedTopic = true;
  }

  if (!recognizedTopic) {
    return;
  }
'''

new_callback = '''void mqttCallback(
  char* topic,
  byte* payload,
  unsigned int length
) {

  bool fromHA = false;
  bool recognizedTopic = false;

  if (
      strcmp(
        topic,
        T_PUMP_CMD
      ) == 0
  ) {

    fromHA = true;
    recognizedTopic = true;

  } else if (
      strcmp(
        topic,
        T_CMD
      ) == 0
  ) {

    recognizedTopic = true;
  }

  if (!recognizedTopic) {
    return;
  }

  // Komandu kontrakts ir ļoti mazs. Pārāk lielu payload atmetam
  // pirms dinamiska String allocation/copy, neizsaucot MQTT publish/log
  // callback kontekstā.
  if (
      !command_payload_policy::isCommandPayloadLengthAllowed(
        length
      )
  ) {

    oversizedCommandDrops++;

    Serial.printf(
      "BRĪDINĀJUMS: MQTT komanda par garu (%u > %u baiti)\\n",
      length,
      static_cast<unsigned int>(
        command_payload_policy::kMaxCommandPayloadBytes
      )
    );

    return;
  }

  String message;

  message.reserve(
    length
  );

  for (
      unsigned int i = 0;
      i < length;
      i++
  ) {
    message +=
        static_cast<char>(
          payload[i]
        );
  }

  message.trim();
'''

replace_once(old_callback, new_callback, "mqtt callback payload gate")

replace_once(
    '    status.reserve(\n      480\n    );',
    '    status.reserve(\n      640\n    );',
    "status reserve",
)

old_heap = '''    status +=
        "Brīvā atmiņa: " +
        String(
          ESP.getFreeHeap() /
          1024
        ) +
        " KB\\n";

    status +=
        "Laiks: " +
'''

new_heap = '''    status +=
        "Brīvā atmiņa: " +
        String(
          ESP.getFreeHeap() /
          1024
        ) +
        " KB\\n";

    status +=
        "Min. brīvā atmiņa: " +
        String(
          ESP.getMinFreeHeap() /
          1024
        ) +
        " KB\\n";

    status +=
        "Lielākais heap bloks: " +
        String(
          ESP.getMaxAllocHeap() /
          1024
        ) +
        " KB\\n";

    status +=
        "Loop steks min. brīvs: " +
        String(
          static_cast<unsigned long>(
            uxTaskGetStackHighWaterMark(
              nullptr
            )
          )
        ) +
        " B\\n";

    status +=
        "Pārāk garas komandas: " +
        String(
          oversizedCommandDrops
        ) +
        "\\n";

    status +=
        "Laiks: " +
'''

replace_once(old_heap, new_heap, "status runtime telemetry")

PATH.write_text(text, encoding="utf-8")
print("[OK] Phase 3 guarded patch applied to src/main.cpp")
