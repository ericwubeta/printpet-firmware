# PrintPet firmware maintenance

Preserve historical tags and all unrelated local changes. Core1.5.2 and Spirit2.0.1 are separate product choices; neither controls printers. Build and host checks are separate from physical device acceptance.

This source uses a shared ESP32-C3 8MiB layout: app0x370000/4MiB, resources0x10000/3MiB, read-only cardid0x356000/0x4000. Never erase or write the vendor gap0x310000–0x36FFFF or copy another device identity. Complete sparse packages follow docs/INSTALL.md. The user explicitly authorized a community single-file adaptation on 2026-11-15: only the compressed first-boot installer described in docs/COMMUNITY-INSTALL.md may be offered as this alternative, with exact verified application/resource bytes and a file ending at0x310000. Never create a conventional merged image spanning the identity gap. Building/publication does not authorize device access or flashing.

On 2026-11-16 the user approved the derived Da Fei Yu avatar and explicitly requested a new firmware and flashing their connected AI Passport. This branch is Spirit2.0.2 with a new application/splash; do not claim its app matches v2.0.1. Pin selected app hashes/version in an explicit input manifest before packaging. Keep the character resource pack and protected partition geometry unchanged. Preserve original device settings and factory identity using only this same device's private backup; never include device data in public artifacts.

Never commit secrets.h, credentials, personal NVS/Flash backups or raw device logs. Use placeholder secrets for release builds. Reuse BSP drivers, preserve LVGL locking and nonblocking button callbacks, and verify Chinese glyph coverage. Keep software/toolchains/build outputs on E: on the original workstation.
