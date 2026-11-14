# PrintPet Core 1.6.0

English | [简体中文](README.zh_CN.md)

Read-only Bambu printer monitoring on third-party FoloToy AI Passport hardware: ESP32-C3, 8MiB flash, no PSRAM. No official FoloToy or Bambu Lab affiliation.

## Choose the firmware you prefer

- [Core 1.6.0](https://github.com/ericwubeta/printpet-firmware/releases/tag/v1.6.0): monitoring with optional LAN camera previews, persistent printer/page selection and automatic power-off. See the [release notes](docs/RELEASE_CORE_1.6.0.md).
- [Core 1.5.2](https://github.com/ericwubeta/printpet-firmware/releases/tag/v1.5.2): the retained compact 1.5.x monitor, without the 1.6.0 camera features.
- [Spirit 2.0.1](https://github.com/ericwubeta/printpet-firmware/releases/tag/v2.0.1): monitoring plus six characters, state animation and short voice prompts. This renumbers the local 2.2.0 feature set; it does not restore early experimental code.

These complete releases share the identity-protected layout. Historical tags remain unchanged. The Windows desktop application is separate. No firmware controls printers, includes desktop AI chat, or synchronizes with the desktop app. Core 1.6.0 camera features are not part of the existing Spirit 2.0.1 release.

## Camera monitoring and power

The second detail page shows the supported printer's LAN JPEG preview with measured FPS and print status. Configure the printer IP/hostname and LAN access code in provisioning. The earlier hardware tests covered X2D at about 0.7–1.0 FPS; other models, A1/P1 streams and this release binary still need device validation. This is a JPEG preview, not RTSPS/H.264 video.

The device restores the last printer and page on startup. After 10 minutes with the screen off, it enters deep sleep if USB power is undetected and no known print is active. Network monitoring stops in deep sleep; a physical key restarts the device. USB host detection cannot reliably identify an ordinary charger/power bank. See [camera controls and compatibility](docs/CAMERA_UI.md) and [automatic power-off](docs/AUTO_POWEROFF.md).

## Complete installation / recovery packages

The old 2.2.0 archive was an app/resource-only upgrade for a prior PrintPet layout, not a first-install or recovery image. An application flashed at 0x0 replaces boot information. Its old 4MiB layout also conflicts with the vendor Arduino identity reservation.

Use the complete ZIP and [installation guide](docs/INSTALL.md). Only the bootloader belongs at 0x0. The application belongs at **0x370000**, resources at 0x10000. The cardid partition at 0x356000 (16KiB) is read-only; the entire 0x310000–0x36FFFF gap is preserved. Do not erase the chip, fill the gap in a merged image, or reuse the old app offset. The tool defaults to offline validation; explicit install/upgrade, port and confirm-write flags are required for serial access. It preserves a per-device backup and never retries writes automatically.

Already overwritten identity or account data cannot be recreated from generic firmware. Use the same device's original backup or the vendor; never copy another device's full dump.

## Build

Use ESP-IDF 5.5.3–5.5.x for esp32c3. Clean checkouts use main/secrets.h.example automatically; never embed personal credentials in a distribution.

```text
idf.py build
python tools/build_bundle.py --build build --output dist
```

Use the validated package, not the default flash command for an unknown layout. Build: PASS (ESP-IDF 5.5.5, LVGL 9.5.0). The 69 camera/protocol/state regressions and both MQTT/LVGL read-only host checks passed. Package validation is offline. Device tests: NOT RUN for this binary/new layout; previous-version acceptance does not establish new-layout or all-printer-state acceptance.

## Third-party media

Furina and Hu Tao are unofficial fan depictions from Genshin Impact / miHoYo. Associated prompts are noncommercial AI-cloned synthetic speech, not new actor recordings; original Chinese performances: Qian Chen and Tao Dian. No endorsement is implied, and the software license grants no third-party character, trademark or voice rights.
