# ESP32 source and flash backup / recovery

## What is already recovered

The original VS Code/PlatformIO project archive has been inspected and its `src/main.cpp` matches the repository source baseline exactly by SHA-256. The original project has no additional custom `lib/` or `test/` implementation files; those directories only contained PlatformIO template README files.

For source-code recovery, the VS Code project is therefore more authoritative and useful than reading flash from the ESP32. A flash dump contains compiled machine code and embedded configuration, not the original C++ comments/project structure.

## When a physical ESP32 flash snapshot is useful

A read-only flash snapshot is useful to:

- preserve the exact bytes currently installed on the device;
- compare a running device with known build artefacts later;
- inspect the partition layout/firmware if source provenance becomes uncertain.

It is **not** a replacement for the source repository.

## Security warning

A full flash dump can contain Wi-Fi, MQTT and OTA credentials because those values are compiled into the firmware. Treat the dump as a secret backup.

- Never commit a flash dump to GitHub.
- `*.bin` is ignored by this repository.
- Store the dump only locally or in an appropriately protected backup location.

The same rule applies to PlatformIO `.pio/build` artefacts such as `firmware.bin`, `firmware.factory.bin`, `.elf` and object files.

## Read-only backup from the VS Code terminal

Connect the ESP32 to the computer by USB.

1. List serial devices:

```bash
pio device list
```

2. Optionally verify that esptool can identify the attached flash without changing it:

```bash
esptool -p PORT flash-id
```

Replace `PORT` with the device shown by PlatformIO, for example `/dev/ttyUSB0` or `/dev/ttyACM0` on Linux.

3. Read the complete flash using automatic flash-size detection:

```bash
esptool -p PORT -b 460800 read-flash 0 ALL esp32-full-flash.bin
```

This is a **read operation**. Do not use `erase-flash`, `erase-region`, `write-flash` or other mutation commands when the goal is only to take a backup.

4. Record the checksum locally:

```bash
sha256sum esp32-full-flash.bin
```

Keep both the binary and checksum outside the Git repository.

## PlatformIO OTA remains separate

The repository's `esp32_ota` environment is for an intentional firmware upload and is not part of the backup procedure. A source/PR audit or normal Git merge must never automatically perform OTA or energize the pump.

## Source of command syntax

The backup command follows Espressif esptool's current `read-flash` interface, including support for `ALL` flash-size autodetection. PlatformIO's `pio device list` is the supported way to enumerate serial ports.
