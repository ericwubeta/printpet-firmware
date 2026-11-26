# Spirit 2.0.3 community single-file installer

This branch builds Spirit 2.0.3 with the SMS fix for reopened setup. The app is
bound to its exact SHA and version by an explicit input manifest. The Da Fei Yu
terminal startup and character resource bytes are unchanged from 2.0.2.
Historical tags and release files are unchanged. The community artifact uses its
own first-boot installer; the separate sparse recovery ZIP uses a standard loader.

The community uploader/flasher accepts one merged binary starting at zero. A
normal merged binary spanning the application would overwrite factory identity.
Instead this file ends exactly at **0x310000**, excluding the entire factory
reservation **0x310000–0x36FFFF**. Do not pad it to 8 MiB or erase the chip.

## First startup

The progress-screen revision on this branch addresses the silent wait during
first-start unpacking. It shows PREPARING, a progress bar and KEEP POWER ON before
the application starts. A failed installation shows SETUP STOPPED / REINSTALL.
Completed installs skip this screen on subsequent boots. See
[current validation](COMMUNITY-VALIDATION.json) for this exact artifact's tests.

Keep power connected while the progress screen advances and the bootloader
expands and verifies the application and resources. Once finished, setup opens automatically. Community installation
resets old Wi-Fi and Bambu account settings; enter them again. It preserves the
factory identity reservation, not all user configuration. Identity lost before
this installation cannot be recreated by this file.

After the browser has finished writing, an interruption during first-start
unpacking can be recovered by reconnecting power and letting installation resume.
If browser flashing itself is interrupted, reinstall the same complete file.
If the package is
damaged, the installer stops without starting a partially installed application;
reinstall the same complete community file. Do not use erase-all.

## Fixed layout and restart protocol

- Original partition table: unchanged. Application at `0x370000`, resources at
  `0x10000`, read-only cardid at `0x356000`.
- Community file: bootloader at zero; partition table at `0x8000`; compressed
  application at `0x10000`; compressed resources at `0x268000`; completion record
  at `0x30f000`. The whole final sector is initially FF so reinstall clears an
  old completion record.
- A low-level ROM inflater uses a 2 KiB dictionary and a 128-byte input buffer.
  No network request, account, API key or per-device identity is embedded.
- Target application SHA is checked first. If absent, the compressed source SHA
  must match before any application erase. Erase/write calls use fixed output
  whitelists and reject the entire identity gap.
- Only after application readback passes does installation touch resources.
  Resources overwrite part of the compressed application, which is no longer
  needed. The compressed resources are outside their own destination and remain
  available through resource-stage interruptions.
- Only after both final hashes pass is the completion record programmed and read
  back. A partial compatible record can be completed after a restart. Later boots
  with the complete record use the normal bootloader, allowing compatible
  application-only upgrades instead of reinstalling this version.

The spiffs suffix containing compressed resources and the completion record is
reserved by this installer. A future firmware that formats or rewrites that
suffix must replace this bootloader with a compatible one. Do not promise that
arbitrary future app-only images are compatible. The normal sparse package's
full install restores the original stock PrintPet bootloader; existing shared
layout application upgrades that leave the suffix intact remain possible.

## Reproduce

Use ESP-IDF 5.5 for ESP32-C3 with the release's configuration and placeholder
`main/secrets.h.example` values. Generate the pinned payload header from the
published application and resource files:

```text
python tools/community/prepare.py --application RELEASE_APP.bin --resources resources/spirit-resources-2.0.1.bin --inputs-manifest SELECTED-BUILD.json --output OUTPUT
idf.py bootloader
python tools/community/prepare.py --application RELEASE_APP.bin --resources resources/spirit-resources-2.0.1.bin --inputs-manifest SELECTED-BUILD.json --output OUTPUT --bootloader build/bootloader/bootloader.bin --partitions RELEASE_PARTITIONS.bin
```

The generator rejects other application/resource hashes, overlapping payloads or
oversized bootloaders. It does not access a device. The normal sparse ZIP builder
continues to build the traditional full installation package; do not replace
historical ZIP files with the community binary.

## Validation boundary

Build and linker layout, native 32-bit inflation, byte-level interruption models
and ESP32-C3 QEMU execution have separate evidence. See the accompanying community
validation report for the exact artifact hashes and actual results. QEMU and
models do not establish physical flash brownout behavior or physical-device
acceptance. No device connection or flash operation is authorized by this file.

The selected 2.0.3 application SHA256 is
`d571d9ad249cf53cc06a8db3db07be89d2afaed2abfc6d3b11ba8c78d5fc6f88`;
resources SHA256 is
`b07a354f3fd0edcfe90c21381c00f5d341ddc831aaf08a604c74d8d5a801fd5d`.
