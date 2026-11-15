# Spirit 2.0.1 community single-file installer

This branch adds a first-boot installer to the already released Spirit 2.0.1. The
application and character resource bytes are identical to tag `v2.0.1`; historical
tags and ZIP release assets are unchanged. The community artifact is a new
installation format with its own bootloader and validation boundary.

The community uploader/flasher accepts one merged binary starting at zero. A
normal merged binary spanning the application would overwrite factory identity.
Instead this file ends exactly at **0x310000**, excluding the entire factory
reservation **0x310000–0x36FFFF**. Do not pad it to 8 MiB or erase the chip.

## First startup

Keep power connected while the bootloader expands and verifies the application
and resources. Once finished, setup opens automatically. Community installation
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
python tools/community/prepare.py --application RELEASE_APP.bin --resources resources/spirit-resources-2.0.1.bin --output OUTPUT
idf.py bootloader
python tools/community/prepare.py --application RELEASE_APP.bin --resources resources/spirit-resources-2.0.1.bin --output OUTPUT --bootloader build/bootloader/bootloader.bin --partitions RELEASE_PARTITIONS.bin
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

The original application SHA256 is
`8ad4f4e69623e2ef236ba96bca1001ad3c84ac4d0e3ec0dfdbe857a5d3cfd277`;
resources SHA256 is
`b07a354f3fd0edcfe90c21381c00f5d341ddc831aaf08a604c74d8d5a801fd5d`.
