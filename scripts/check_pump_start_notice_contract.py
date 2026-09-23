#!/usr/bin/env python3
"""Fail-closed source contract for pump-start notification ordering."""

from pathlib import Path

SOURCE = Path("src/main.cpp")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"pump-start notice contract: FAIL: {message}")


def main() -> None:
    source = SOURCE.read_text(encoding="utf-8")

    start = source.find("bool startPump(uint32_t seconds)")
    end = source.find("uint32_t extendPump", start)
    require(start >= 0 and end > start, "could not isolate startPump()")
    block = source[start:end]

    notice_text = '"💧 Laistīšana sākas! ("'
    notice = block.find(notice_text)
    status_on = block.find("publishPumpStatusValue(true)")
    relay_on = block.find("RELAY_ON")
    best_effort = block.find("mqtt.publishBestEffort", notice)

    require(notice >= 0, "truthful pre-start notice is missing")
    require("Laistīšana sākta!" not in block, "post-activation start wording returned")
    require(best_effort > notice, "start notice is not sent through best-effort MQTT")
    require(status_on > best_effort, "retained pump status must follow the pre-start notice")
    require(relay_on > status_on, "relay ON must remain after all pre-start network work")
    require("tgSend(" not in block, "startPump must not queue a delayed tracked Telegram notice")
    require(
        "startNoticeAccepted && mqtt.queueSize() != 0U" in block,
        "accepted pre-start notice must fail closed if the local outbox does not drain",
    )
    require(
        "urgentPumpStopPending" in block[best_effort:status_on],
        "urgent STOP must be rechecked after the pre-start network attempt",
    )

    service_start = source.find("void serviceTelegramDelivery()")
    service_end = source.find("// ============================================================", service_start + 1)
    require(service_start >= 0 and service_end > service_start, "could not isolate serviceTelegramDelivery()")
    service = source[service_start:service_end]
    require(
        "pumpRunning || !mqttSessionReady()" in service,
        "application Telegram delivery must remain suppressed while the pump runs",
    )

    print("Pump-start notice contract: PASS")


if __name__ == "__main__":
    main()
