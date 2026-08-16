# Source baseline

Initial GitHub scaffold prepared from the uploaded `main.cpp`.

- SHA-256: `f13db85f3808701573d724663c463534455b23795a2a534fa3403610484008e9`
- Lines: 2375
- Import date: 2026-08-16

This hash is evidence for the exact firmware file used to bootstrap the repository.

## VS Code / PlatformIO archive verification

On 2026-08-16 the original VS Code/PlatformIO project archive was independently inspected. Its `src/main.cpp` has the same SHA-256 (`f13db85f...008e9`) as the initially uploaded source, confirming that the GitHub bootstrap was based on the actual project source rather than a reconstructed copy.

The archive also contained the original `platformio.ini`, a local `include/secrets.h`, generated `.vscode` metadata and `.pio` build artefacts. The local secrets file and generated build artefacts are intentionally excluded from Git because compiled firmware/debug artefacts can contain embedded Wi-Fi, MQTT and OTA credentials.

The old PlatformIO configuration used the previous pioarduino release and a static LAN OTA address. Those values are historical evidence only; the audited repository configuration supersedes them.

## Bootstrap audit note

The repository import intentionally applies a documented source-only fix to time synchronization (`configTzTime`) after this baseline was captured. The hash above therefore identifies the user-uploaded and VS Code-verified pre-audit source, not the post-audit repository file.
