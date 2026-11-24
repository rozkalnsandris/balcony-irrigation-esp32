#!/usr/bin/env python3
"""Check candidate cutoff wiring and report actual build support (never run firmware)."""

import argparse
import hashlib
from pathlib import Path
import re
import subprocess


def require(condition, message):
    if not condition:
        raise SystemExit("HA cutoff contract: FAIL: " + message)


def source_contract():
    source = Path("src/main.cpp").read_text(encoding="utf-8")
    runtime = Path("include/ha_cutoff_runtime.h").read_text(encoding="utf-8")
    start = source.split("bool startPump(uint32_t seconds) {", 1)[1].split(
        "uint32_t extendPump", 1)[0]
    order = ["boundedCutoff.sampleCount", "uint32_t now = millis()",
             "boundedGate.remainingMs(now)", "boundedCutoff.arm(",
             "boundedGate.claimFresh(millis())", "boundedCutoff.start()",
             "pumpRunning = true"]
    pos = -1
    for item in order:
        pos = start.find(item, pos + 1)
        require(pos >= 0, "missing ordered start step: " + item)
    require(re.search(r"#else\s+digitalWrite\(\s*RELAY_PIN,\s*RELAY_ON\s*\);\s*#endif",
                      start), "legacy ON must not be candidate ON")
    require(len(re.findall(r"digitalWrite\(\s*RELAY_PIN,\s*RELAY_ON", source)) == 1,
            "unexpected additional ON path")
    for a, z in [("void stopPump(", "void servicePump()"),
                 ("void requestUrgentPumpStop(", "void serviceUrgentPumpStop()"),
                 ("void serviceUrgentPumpStop()", "void servicePumpCriticalNetworkInput()"),
                 ("void forcePumpOffForOtaSafety()", "void setupOTA()")]:
        block = source.split(a, 1)[1].split(z, 1)[0]
        require("pumpRelayOff();" in block, a + " lacks synchronized OFF")
    require("if (!boundedCutoff.ready()) return;" in source,
            "unavailable cutoff must inhibit offer")
    require("if (!boundedCutoff.running())" in source, "ON ACK must check latch")
    require("if (!kCacheSafeSdk) return false;" in runtime, "SDK fail-closed gate absent")
    require("alarm.flags.auto_reload_on_alarm = false;" in runtime, "not a one-shot")
    require("count > armedFromCount_" in runtime, "counter progression not checked")
    require("claimBudgetMs_" in runtime, "claim expiry not checked at final gate")
    implementation = Path("src/ha_cutoff_runtime.cpp").read_text(encoding="utf-8")
    require("static bool onAlarm(" in runtime and "static bool IRAM_ATTR onAlarm(" not in runtime,
            "callback must not be an inline class definition")
    callback = implementation.split("bool IRAM_ATTR Guard::onAlarm(", 1)[1].split(
        "}  // namespace ha_cutoff", 1)[0]
    require("REG_WRITE(GPIO_OUT_W1TS_REG" in callback and
            "self->state_.alarm();" in callback and
            "portENTER_CRITICAL_ISR" in callback, "ISR does not force OFF and latch")
    require(not any(x in callback for x in ("mqtt", "Serial", "digitalWrite", "gptimer_stop",
                                            "new ", "malloc", "delay(")),
            "unexpected ISR dependency")
    print("HA_CUTOFF_SOURCE_CONTRACT=PASS")


def build_contract(log_path, elf_path, packages):
    log = log_path.read_text(encoding="utf-8", errors="replace")
    flags = set(re.findall(r"HA_CUTOFF_SDK_CACHE_SAFE=(YES|NO)", log))
    require(len(flags) == 1, "missing or contradictory actual SDK compile flag")
    require(elf_path.is_file(), "candidate ELF missing")
    support = flags.pop()
    print("HA_CUTOFF_ACTUAL_SDK_CACHE_SAFE=" + support)
    require(support == "YES", "candidate SDK still lacks cache-safe GPTimer")
    require("*** Copied compiled esp32 IDF libraries to Arduino framework ***" in log,
            "fresh HybridCompile library rebuild not observed")
    configs = sorted((packages / "framework-arduinoespressif32-libs" / "esp32").glob(
        "*/include/sdkconfig.h"))
    require(bool(configs), "rebuilt SDK configuration header missing")
    # The pinned IDF Kconfig selects OBJ_CACHE_SAFE via HANDLER_IN_IRAM.
    # The driver uses this option for internal-memory group allocation.
    keys = ("CONFIG_GPTIMER_ISR_CACHE_SAFE", "CONFIG_GPTIMER_ISR_HANDLER_IN_IRAM",
            "CONFIG_GPTIMER_OBJ_CACHE_SAFE")
    enabled = [path for path in configs if all(
        re.search(r"^#define\s+" + key + r"\s+1\s*$",
                  path.read_text(encoding="utf-8"), re.MULTILINE) for key in keys)]
    require(bool(enabled), "rebuilt SDK headers lack required options")
    print("HA_CUTOFF_REBUILT_SDK_HEADERS=PASS")
    binaries = sorted((packages / "toolchain-xtensa-esp-elf" / "bin").glob(
        "xtensa*elf-objdump"))
    require(bool(binaries), "objdump unavailable")
    symbols = subprocess.check_output([str(binaries[0]), "-t", "-C", str(elf_path)],
                                      text=True)

    def section(name, fragment):
        rows = [line for line in symbols.splitlines() if name in line]
        require(bool(rows) and all(fragment in line for line in rows),
                "missing or misplaced symbol: " + name)
        print("HA_CUTOFF_LINK_PLACEMENT=" + name + ":" + fragment)

    section("ha_cutoff::Guard::onAlarm(", ".iram")
    section("gptimer_default_isr", ".iram")
    section("boundedCutoff", ".dram")

    # Pinned ESP-IDF 5.5.5 gptimer_default_isr calls the HAL capture
    # helper and the FreeRTOS critical section pair before our callback.
    # Check exact C symbols, not substring matches (e.g. Compliance).
    raw_symbols = subprocess.check_output([str(binaries[0]), "-t", str(elf_path)],
                                          text=True)
    helpers = ("timer_hal_capture_and_get_counter_value",
               "xPortEnterCriticalTimeout", "vPortExitCritical")
    for name in helpers:
        rows = [line.split() for line in raw_symbols.splitlines()
                if line.split() and line.split()[-1] == name]
        require(len(rows) == 1 and any(
            field.startswith(".iram") for field in rows[0][1:-1]),
            "missing or misplaced ISR helper: " + name)
        print("HA_CUTOFF_LINK_HELPER=" + name + ":IRAM")

    # Record focused machine-code evidence for review of direct calls, literal
    # loads, the driver's indirect callback and critical-section internals.
    # A human still has to resolve targets/data and the interrupt entry path.
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    print("HA_CUTOFF_ELF_SOURCE_REV=" + revision)
    print("HA_CUTOFF_ELF_SHA256=" + hashlib.sha256(elf_path.read_bytes()).hexdigest())
    callback = "_ZN9ha_cutoff5Guard7onAlarmEP9gptimer_tPK26gptimer_alarm_event_data_tPv"
    for name in (callback, "gptimer_default_isr", *helpers):
        disassembly = subprocess.check_output(
            [str(binaries[0]), "-d", "--disassemble=" + name, str(elf_path)], text=True)
        require("<" + name + ">:" in disassembly, "missing ISR disassembly: " + name)
        print("HA_CUTOFF_DISASSEMBLY_BEGIN=" + name)
        print(disassembly.rstrip())
        print("HA_CUTOFF_DISASSEMBLY_END=" + name)
    print("HA_CUTOFF_LINK_PROOF=SELECTED_HELPER_AND_ENTRY_DATA_PLACEMENT_ONLY")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-log", type=Path)
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--packages-root", type=Path)
    args = parser.parse_args()
    source_contract()
    if args.build_log:
        require(args.elf is not None and args.packages_root is not None,
                "build evidence requires ELF and package root")
        build_contract(args.build_log, args.elf, args.packages_root)
