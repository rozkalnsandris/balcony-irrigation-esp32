# HA candidate #75 — isolated bench and independent cutoff preflight

SOURCE-ONLY plan. No test execution, flash, broker publish, wiring change, HA change or pump activation is authorized. This plan does not establish a deploy-ready artifact. Keep the HA reference hard-inhibited. Issue #2 remains open.

## Evidence baseline and remaining gap

At main `f8d7c54694a22c77bfeb93c3714d65e28c648a74`, PR #77 merged, exact-main firmware CI run 45569724050 and policy run 45569724384 passed. Job 139053208149 reports the rebuilt cache-safe SDK and selected ISR helpers in IRAM, guard in DRAM; ELF SHA256 `8678f15dddf20af1b19d8d2d49dbed9ccb72b4ef7c498d5cd31531b02e4badbc`. This is a historical build-only receipt, not a downloadable or deploy-approved image. Revalidate the actual future bench revision/artifact.

Focused disassembly: callback writes GPIO26 OFF and returns false; driver calls HAL counter capture and critical-section helpers, then the registered callback. FreeRTOS critical sections call esp_cpu_compare_and_set, use ROM _xtos_set_intlevel and internal nesting state; assertion paths reference Flash rodata and __assert_func. HAL capture waits for peripheral completion; spinlock acquisition can wait. Selected placement does not prove every path or bounded worst-case IRQ latency. Do not disable assertions or alter SDK safety options to manufacture a pass. A future exact-artifact review must resolve interrupt dispatch, indirect targets, literal/data paths, and classify abnormal paths as controller faults covered by independent protection, not normal cache-safe proof.

## 1. Required owner-supplied evidence before any powered bench action

- Separate spare ESP32 Dev Module, physically disconnected from all irrigation wiring, with a unique verified board/serial-port identity. No production ESP32, relay/pump, valve, or production power path connected. If no spare board is available, STOP; do not repurpose the live controller.
- Photo or local inspection confirming motor supply disconnected, isolated current-limited low-voltage test supply, suitable 3.3 V logic analyzer or scope with common reference and known timebase accuracy, non-actuating indicator/dummy load if needed. No mains work. Human present, accessible physical power disconnect.
- Isolated bench network and test-only broker/account; no routing or bridge to production, no production credentials, no HA/Telegram automation. Verify isolation independently before connecting the candidate. The CI secrets example alone is not evidence of network isolation. No retained commands.
- Exact clean source revision, resolved pinned toolchain/packages, reviewed local bench configuration (no secret values in receipts), disposable checkout and separate PlatformIO core per existing candidate CI recipe. Candidate environment only, no OTA. Build once, record ELF and each required flash-image digest, address/partition manifest, device identity and matching SDK/link checks. Verify that artifact, not a later rebuild. CI's example-credential image is not automatically a usable bench image.
- A reviewed bounded test procedure with exact artifact/target and separate STRICT owner authorization before flash, MQTT test command, powered fault injection or test-device mutation. No generic upload, no production credentials, no auto retry. No fault-injection hooks in production firmware. If a controlled fault requires an instrumented harness, review it separately and mark its results non-equivalent to the unmodified candidate.

## 2. Bench acceptance matrix (plan only; NOT RUN)

Measure independently of ESP32 logs. Capture active-low GPIO26 edges with a calibrated timebase, starting before reset; record raw traces, uncertainties, exact run ID, firmware digest and observed outcomes. Use test-only protocol traffic, synthetic sensor fixtures and bounded attempts. Any ambiguous or failed test ends the authorized batch: remove bench power by the agreed physical disconnect, preserve evidence, no automated repeat/reflash.

| Case | Required observation |
| --- | --- |
| Boot/reset, loss/restoration of bench power | No unintended low/ON pulse; reset itself cannot grant admission. Test without motor or production relay. |
| Normal fresh request and start | Exactly one low interval; high by scan-start deadline. No legacy ON/extension accepted. |
| Expired, duplicate, malformed or canceled admission; alarm before start | No low pulse. Repeated requests do not extend a started interval. |
| STOP before/during ON and repeated STOP | OFF, no late ON; old alarm/callback reconciled before new admission. |
| Network loss, delayed/lost reply, application-loop stall near deadline | No extension; observe GPIO cutoff even with application task stalled if interrupts remain operational. Distinguish ordinary cooperative OFF from timer-driven OFF. |
| Flash-cache-disabled interval across timer deadline | Observe OFF during the independently evidenced cache-disabled interval; no Flash-dependent logging used as proof. Use only a separately reviewed isolated test mechanism; a normal short run is not this test. |
| Interrupt masking/core halt/reset or driver uncertainty | Do not claim GPTimer protects against these; verify separately independent cutoff on dummy load. No intentional controller fault on installed irrigation equipment. |

For normal and loop-stall/cache tests, correlate the scan-start reference, actual timer deadline, GPIO edge and ACK observations. If exact scan-start/alarm instrumentation is unavailable, report only measured GPIO low duration; do not infer scan-start deadline or ISR latency. A trace of OFF at the ordinary cooperative timeout cannot by itself prove the independent interrupt path. Require worst observed time plus measurement uncertainty within the applicable bound, and retain the distinction between finite sample testing and a documented worst-case guarantee. Record failure/reset traces, not just successes.

## 3. Independent physical cutoff — selection requirements, no chosen hardware

Preferred minimal architecture: a separately timed normally-open series power-interrupting element independent of ESP32 GPIO/software, in addition to the existing NO relay. This is a functional block description, not a wiring diagram. Its own supply/control failure must default OFF. It must interrupt the actual motor supply even if the ESP32 output remains ON or the existing relay contacts weld. A timer that merely sends MQTT OFF or drives the same welded contact is insufficient.

Require a one-shot maximum ON window with no extension or retrigger during the window. After timeout/fault, inhibit re-energization on held input, input chatter, ESP32 reboot or loss/restoration of timer supply. Rearm must be independently controlled and demonstrated safe; a software-controlled reset or cyclic interval mode alone is insufficient. No automatic rearm policy is selected here. The additional contact can also fail; a single ordinary timer relay is not a certified safety system or proof against arbitrary simultaneous failures.

Before choosing a timer or switching element, record the actual motor/supply voltage, steady current and documented or safely established startup/stall current, existing relay model and DC inductive ratings, suppression placement, independent supply/trigger behavior, environment and enclosure. R385/12 V is historical; it is not a verified load rating. Use manufacturer DC motor/inductive load and inrush ratings, suitable protection and release-time data; an AC or resistive-current label alone is insufficient. An approved circuit must also account for stored energy and suppression-related release delay. Require competent electrical review of the exact low-voltage circuit; no wiring or purchasing decision from this plan.

Timing acceptance must include maximum positive timing error over the full selected range, supply/temperature tolerance, trigger and switching/release delays, residual motor energy/flow as applicable, and measurement uncertainty. For a 45 s physical ON-duration bound, the sum of nominal interval and all worst-case positive delays/errors must be <= 45 s. A nominal 45 s dial setting is not acceptable evidence.

The candidate's software deadline is stricter in origin: 45 s from scan start, with a 10 s ticket TTL and provisional 1 s margin. A separate timer started at physical ON does not automatically enforce the scan-start deadline. If the physical requirement also covers scan-start timing, either independently anchor the cutoff at/before scan start, or subtract a proven maximum scan-to-ON delay (including clock error) from the external budget. Nominally that leaves at most 35 s before further tolerance/latency deductions; do not treat 35 s as an approved setting or silently change watering duration. Owner must review the resulting duration/independent rearm tradeoff after actual ratings and timing evidence.

## 4. Release gates and evidence receipt

1. #2 pump-OFF photo checklist: COM/NO/NC, motor path, suppression and current supply/relay ratings. No energized rewiring.
2. Independent cutoff model, datasheet and reviewed topology, timing budget and reset/rearm proof; isolated dummy-load tests with held trigger, repeated input, supply loss/restoration and simulated stuck existing contact on the bench only. No live pump.
3. Exact candidate build and isolated traces from section 2; resolve normal ISR call/data paths and explicitly account for abnormal paths. Finite trace tests alone cannot prove a universal bound.
4. Separately review actual motor-power integration and commissioning with exact authorization, person present and physical disconnect. Until then, no production flash, HA pin change, arm enable, scheduler or MQTT/pump actuation. RPi5_main #1561 inhibits remain required.

Receipt fields: source SHA, artifact hashes, board alias, isolation verified, measurement instrument/accuracy, test case, trace ID, observed maximum and uncertainty, result PASS/FAIL/NOT_RUN/UNKNOWN, physical cutoff model/rating/timing/rearm evidence, mutation receipt. Record only public-safe values. Current status: **PLAN_ONLY / BLOCKED_MISSING_PHYSICAL_AND_BENCH_EVIDENCE**. No board, instrument, cutoff part or installed state verified.

## References (manufacturer documentation)

- ESP-IDF v5.5.5 GPTimer, one-shot, power management and cache safety: https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32/api-reference/peripherals/gptimer.html
- ESP32 reset, pin and power design: https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/schematic-checklist.html
- OMRON interval vs other timer modes: https://www.ia.omron.com/support/guide/13/classifications.html
- OMRON timing tolerance, load ratings and suppression cautions: https://www.ia.omron.com/product/cautions/13/precautions_for_correct_use.html
- OMRON environment, contact and inductive-load precautions: https://www.ia.omron.com/product/cautions/13/precautions_for_safety_use.html

References establish design constraints, not suitability of an unselected part or verified physical safety.
