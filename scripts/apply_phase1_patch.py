from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly 1 match, found {count}")
    return text.replace(old, new, 1)


main_path = Path("src/main.cpp")
main = main_path.read_text(encoding="utf-8")

main = replace_once(
    main,
    '#include "secrets.h"\n',
    '#include "secrets.h"\n#include "command_safety.h"\n',
    "include command_safety",
)

main = replace_once(
    main,
    '''struct PendingCommand {\n  bool fromHA = false;\n  String payload;\n};\n\nPendingCommand commandQueue[COMMAND_QUEUE_SIZE];\n\nuint8_t commandQueueHead = 0;\nuint8_t commandQueueTail = 0;\nuint8_t commandQueueCount = 0;\n''',
    '''struct PendingCommand {\n  bool fromHA = false;\n  uint32_t stopEpoch = 0;\n  String payload;\n};\n\nPendingCommand commandQueue[COMMAND_QUEUE_SIZE];\n\nuint8_t commandQueueHead = 0;\nuint8_t commandQueueTail = 0;\nuint8_t commandQueueCount = 0;\n\n// Katrs urgent STOP/OFF palielina epoch. Parastās komandas saglabā\n// epoch, kurā tās tika saņemtas, lai pēc jaunāka STOP varētu\n// atmest tikai stale pump-start/extend komandas, nezaudējot statusa\n// vai diagnostikas komandas.\nuint32_t pumpStopEpoch = 0;\n\nbool urgentPumpStopPending = false;\nbool urgentPumpStopFromHA = false;\n''',
    "command queue state",
)

main = replace_once(
    main,
    '''bool startPump(uint32_t seconds) {\n\n  if (pumpRunning) {\n    return false;\n  }\n''',
    '''bool startPump(uint32_t seconds) {\n\n  // Ja STOP/OFF tikko saņemts, vispirms jāpabeidz tā stāvokļa\n  // reconciliācija. Tas nepieļauj pump startu vienā un tajā pašā\n  // loop logā pirms urgent stop apstrādes.\n  if (urgentPumpStopPending) {\n    return false;\n  }\n\n  if (pumpRunning) {\n    return false;\n  }\n''',
    "startPump urgent guard",
)

main = replace_once(
    main,
    '''void servicePump() {\n\n  if (!pumpRunning) {\n    return;\n  }\n''',
    '''void servicePump() {\n\n  if (!pumpRunning) {\n    return;\n  }\n''',
    "servicePump anchor",
)

service_pump_end = '''    stopPump(\n      "plānotais laiks beidzies"\n    );\n  }\n}\n\n// ============================================================\n// OTA\n// ============================================================\n'''
service_pump_new = '''    stopPump(\n      "plānotais laiks beidzies"\n    );\n  }\n}\n\n// ============================================================\n// URGENT SŪKŅA STOP/OFF\n// ============================================================\n\nvoid requestUrgentPumpStop(bool fromHA) {\n\n  // Drošības efekts notiek uzreiz callback kontekstā: tikai GPIO OFF.\n  // MQTT publish/logging šeit apzināti neveicam, lai neizraisītu\n  // PubSubClient re-entrancy.\n  digitalWrite(\n    RELAY_PIN,\n    RELAY_OFF\n  );\n\n  pumpStopEpoch++;\n  urgentPumpStopPending = true;\n  urgentPumpStopFromHA = fromHA;\n}\n\nvoid serviceUrgentPumpStop() {\n\n  if (!urgentPumpStopPending) {\n    return;\n  }\n\n  bool fromHA = urgentPumpStopFromHA;\n\n  urgentPumpStopPending = false;\n  urgentPumpStopFromHA = false;\n\n  if (pumpRunning) {\n\n    stopPump(\n      fromHA ?\n      "Home Assistant OFF (urgent)" :\n      "manuāla STOP komanda (urgent)"\n    );\n\n    return;\n  }\n\n  // Pat ja programmatūras stāvoklis jau bija OFF, uzturam fizisko\n  // fail-safe stāvokli un atjaunojam retained MQTT statusu.\n  digitalWrite(\n    RELAY_PIN,\n    RELAY_OFF\n  );\n\n  publishPumpStatus();\n\n  if (!fromHA) {\n    tgSend(\n      "Sūknis jau bija izslēgts."\n    );\n  }\n}\n\n// Garāku sensoru lasījumu laikā pieņemam MQTT inputu, bet\n// neuzsākam reconnect. Tas ļauj saņemt STOP/OFF starp sensoriem\n// un tūlīt pēc mqtt.loop() atgriešanās pabeigt stop reconciliāciju.\nvoid servicePumpCriticalNetworkInput() {\n\n  if (mqtt.connected()) {\n    mqtt.loop();\n  }\n\n  serviceUrgentPumpStop();\n  servicePump();\n  feedWatchdog();\n}\n\n// ============================================================\n// OTA\n// ============================================================\n'''
main = replace_once(main, service_pump_end, service_pump_new, "urgent stop functions")

main = replace_once(
    main,
    '''  // Ja rinda pilna, izmetam vecāko komandu.\n  // Tas dod priekšroku jaunākajām komandām,\n  // piemēram STOP.\n''',
    '''  // Ja rinda pilna, izmetam vecāko PARASTO komandu.\n  // STOP/OFF šo FIFO vispār neizmanto.\n''',
    "queue full comment",
)

main = replace_once(
    main,
    '''  commandQueue[\n    commandQueueTail\n  ].fromHA = fromHA;\n\n  commandQueue[\n    commandQueueTail\n  ].payload = payload;\n''',
    '''  commandQueue[\n    commandQueueTail\n  ].fromHA = fromHA;\n\n  commandQueue[\n    commandQueueTail\n  ].stopEpoch = pumpStopEpoch;\n\n  commandQueue[\n    commandQueueTail\n  ].payload = payload;\n''',
    "enqueue stop epoch",
)

main = replace_once(
    main,
    '''  out.fromHA =\n      commandQueue[\n        commandQueueHead\n      ].fromHA;\n\n  out.payload =\n      commandQueue[\n        commandQueueHead\n      ].payload;\n''',
    '''  out.fromHA =\n      commandQueue[\n        commandQueueHead\n      ].fromHA;\n\n  out.stopEpoch =\n      commandQueue[\n        commandQueueHead\n      ].stopEpoch;\n\n  out.payload =\n      commandQueue[\n        commandQueueHead\n      ].payload;\n''',
    "dequeue stop epoch",
)

old_callback = '''  String topicString(\n    topic\n  );\n\n  if (\n      topicString ==\n      T_PUMP_CMD\n  ) {\n\n    enqueueCommand(\n      true,\n      message\n    );\n\n  } else if (\n      topicString ==\n      T_CMD\n  ) {\n\n    enqueueCommand(\n      false,\n      message\n    );\n  }\n'''
new_callback = '''  message.trim();\n\n  String topicString(\n    topic\n  );\n\n  bool fromHA = false;\n  bool recognizedTopic = false;\n\n  if (\n      topicString ==\n      T_PUMP_CMD\n  ) {\n\n    fromHA = true;\n    recognizedTopic = true;\n\n  } else if (\n      topicString ==\n      T_CMD\n  ) {\n\n    recognizedTopic = true;\n  }\n\n  if (!recognizedTopic) {\n    return;\n  }\n\n  if (\n      command_safety::isUrgentStop(\n        fromHA,\n        message.c_str()\n      )\n  ) {\n\n    requestUrgentPumpStop(\n      fromHA\n    );\n\n    return;\n  }\n\n  enqueueCommand(\n    fromHA,\n    message\n  );\n'''
main = replace_once(main, old_callback, new_callback, "mqtt urgent callback")

# Replace the three long-scan watchdog/servicePump pairs with a critical input service.
main = replace_once(
    main,
    '''    feedWatchdog();\n\n    String category =\n''',
    '''    servicePumpCriticalNetworkInput();\n\n    String category =\n''',
    "publish moisture critical input",
)

main = replace_once(
    main,
    '''      feedWatchdog();\n\n      servicePump();\n\n      String category =\n''',
    '''      servicePumpCriticalNetworkInput();\n\n      String category =\n''',
    "mitrums critical input",
)

main = replace_once(
    main,
    '''      feedWatchdog();\n\n      servicePump();\n\n      result +=\n''',
    '''      servicePumpCriticalNetworkInput();\n\n      result +=\n''',
    "raw critical input",
)

old_process_queue = '''  if (item.fromHA) {\n\n    processHACommand(\n      item.payload\n    );\n\n  } else {\n\n    processCommand(\n      item.payload\n    );\n  }\n'''
new_process_queue = '''  if (\n      command_safety::shouldSuppressQueuedPumpStart(\n        item.stopEpoch,\n        pumpStopEpoch,\n        item.fromHA,\n        item.payload.c_str()\n      )\n  ) {\n\n    logEvent(\n      "Drošība: stale pump-start komanda atmesta pēc jaunāka STOP/OFF"\n    );\n\n    return;\n  }\n\n  if (item.fromHA) {\n\n    processHACommand(\n      item.payload\n    );\n\n  } else {\n\n    processCommand(\n      item.payload\n    );\n  }\n'''
main = replace_once(main, old_process_queue, new_process_queue, "stale queue suppression")

main = replace_once(
    main,
    '''  feedWatchdog();\n\n  // Sūkņa drošību pārbaudām pašā loop sākumā.\n  servicePump();\n''',
    '''  feedWatchdog();\n\n  // Ja urgent STOP palicis no iepriekšējā tīkla callback,\n  // tas vienmēr dominē pirms jebkura cita loop darba.\n  serviceUrgentPumpStop();\n\n  // Sūkņa drošību pārbaudām pašā loop sākumā.\n  servicePump();\n''',
    "loop urgent start",
)

main = replace_once(
    main,
    '''  serviceMQTT();\n\n  // Pēc iespējami bloķējoša tīkla mēģinājuma\n  // vēlreiz pārbaudām sūkņa laiku.\n  servicePump();\n''',
    '''  serviceMQTT();\n\n  // mqtt.loop() varēja pieņemt urgent STOP/OFF. Relejs callbackā\n  // jau tika fiziski izslēgts; šeit nekavējoties sakārtojam state,\n  // statistiku, logus un retained statusu pirms parastās FIFO.\n  serviceUrgentPumpStop();\n\n  // Pēc iespējami bloķējoša tīkla mēģinājuma\n  // vēlreiz pārbaudām sūkņa laiku.\n  servicePump();\n''',
    "loop urgent after mqtt",
)

main_path.write_text(main, encoding="utf-8")

Path("include/command_safety.h").write_text(
    '''#pragma once\n\n#include <cstdint>\n#include <cstring>\n\nnamespace command_safety {\n\ninline bool isUrgentStop(bool fromHA, const char* payload) {\n  if (payload == nullptr) {\n    return false;\n  }\n\n  if (fromHA) {\n    return std::strcmp(payload, "OFF") == 0;\n  }\n\n  return std::strcmp(payload, "stop") == 0;\n}\n\ninline bool isPumpStartCommand(bool fromHA, const char* payload) {\n  if (payload == nullptr) {\n    return false;\n  }\n\n  if (fromHA) {\n    return std::strcmp(payload, "ON") == 0;\n  }\n\n  if (std::strcmp(payload, "laist") == 0) {\n    return true;\n  }\n\n  return std::strncmp(payload, "laist_", 6) == 0;\n}\n\ninline bool shouldSuppressQueuedPumpStart(\n    std::uint32_t queuedStopEpoch,\n    std::uint32_t currentStopEpoch,\n    bool fromHA,\n    const char* payload) {\n  return queuedStopEpoch != currentStopEpoch &&\n         isPumpStartCommand(fromHA, payload);\n}\n\n}  // namespace command_safety\n''',
    encoding="utf-8",
)

Path("test/test_command_safety").mkdir(parents=True, exist_ok=True)
Path("test/test_command_safety/test_main.cpp").write_text(
    '''#include <unity.h>\n\n#include "command_safety.h"\n\nvoid setUp() {}\nvoid tearDown() {}\n\nvoid test_urgent_stop_contract() {\n  TEST_ASSERT_TRUE(command_safety::isUrgentStop(true, "OFF"));\n  TEST_ASSERT_TRUE(command_safety::isUrgentStop(false, "stop"));\n  TEST_ASSERT_FALSE(command_safety::isUrgentStop(true, "ON"));\n  TEST_ASSERT_FALSE(command_safety::isUrgentStop(false, "laist"));\n}\n\nvoid test_pump_start_classifier() {\n  TEST_ASSERT_TRUE(command_safety::isPumpStartCommand(true, "ON"));\n  TEST_ASSERT_TRUE(command_safety::isPumpStartCommand(false, "laist"));\n  TEST_ASSERT_TRUE(command_safety::isPumpStartCommand(false, "laist_1"));\n  TEST_ASSERT_TRUE(command_safety::isPumpStartCommand(false, "laist_3"));\n  TEST_ASSERT_FALSE(command_safety::isPumpStartCommand(true, "OFF"));\n  TEST_ASSERT_FALSE(command_safety::isPumpStartCommand(false, "statuss"));\n}\n\nvoid test_newer_stop_suppresses_only_stale_pump_starts() {\n  TEST_ASSERT_TRUE(command_safety::shouldSuppressQueuedPumpStart(4, 5, true, "ON"));\n  TEST_ASSERT_TRUE(command_safety::shouldSuppressQueuedPumpStart(4, 5, false, "laist"));\n  TEST_ASSERT_TRUE(command_safety::shouldSuppressQueuedPumpStart(4, 5, false, "laist_2"));\n\n  TEST_ASSERT_FALSE(command_safety::shouldSuppressQueuedPumpStart(4, 5, false, "statuss"));\n  TEST_ASSERT_FALSE(command_safety::shouldSuppressQueuedPumpStart(4, 5, false, "mitrums"));\n  TEST_ASSERT_FALSE(command_safety::shouldSuppressQueuedPumpStart(5, 5, true, "ON"));\n  TEST_ASSERT_FALSE(command_safety::shouldSuppressQueuedPumpStart(5, 5, false, "laist"));\n}\n\nvoid test_null_payload_is_never_actionable() {\n  TEST_ASSERT_FALSE(command_safety::isUrgentStop(false, nullptr));\n  TEST_ASSERT_FALSE(command_safety::isPumpStartCommand(false, nullptr));\n  TEST_ASSERT_FALSE(command_safety::shouldSuppressQueuedPumpStart(1, 2, false, nullptr));\n}\n\nint main() {\n  UNITY_BEGIN();\n  RUN_TEST(test_urgent_stop_contract);\n  RUN_TEST(test_pump_start_classifier);\n  RUN_TEST(test_newer_stop_suppresses_only_stale_pump_starts);\n  RUN_TEST(test_null_payload_is_never_actionable);\n  return UNITY_END();\n}\n''',
    encoding="utf-8",
)

Path("platformio.ini").write_text(
    '''[platformio]\ndefault_envs = esp32_ci\n\n[esp32_base]\nplatform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip\nboard = esp32dev\nframework = arduino\nmonitor_speed = 115200\nextra_scripts = pre:scripts/git_rev_macro.py\n\nlib_deps =\n    knolleary/PubSubClient @ 2.8.0\n\n[env:esp32_ci]\nextends = esp32_base\n; Build-only environment. No OTA target and no production credentials required.\n\n[env:esp32_ota]\nextends = esp32_base\nupload_protocol = espota\nupload_port = balkons-esp32.local\n; Set the OTA authentication flag only for an intentional upload, e.g.\n; PLATFORMIO_UPLOAD_FLAGS='--auth=...' pio run -e esp32_ota -t upload\n\n[env:native]\nplatform = native\ntest_framework = unity\ntest_build_src = no\nbuild_flags = -std=c++17\n''',
    encoding="utf-8",
)

workflow_path = Path(".github/workflows/firmware-ci.yml")
workflow = workflow_path.read_text(encoding="utf-8")
workflow = replace_once(
    workflow,
    '''      - name: Install PlatformIO\n        run: python -m pip install --disable-pip-version-check 'platformio==6.1.19'\n\n      - name: Create non-secret CI configuration\n''',
    '''      - name: Install PlatformIO\n        run: python -m pip install --disable-pip-version-check 'platformio==6.1.19'\n\n      - name: Run native command-safety tests\n        run: pio test -e native\n\n      - name: Create non-secret CI configuration\n''',
    "CI native test step",
)
workflow_path.write_text(workflow, encoding="utf-8")

print("Phase 1 patch applied successfully")
