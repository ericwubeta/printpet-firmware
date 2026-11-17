# Core 1.6.1 community installer

This branch packages the exact released Core 1.6.1 application for the FoloToy
community's single-file, address-zero installer. It does not change monitoring,
camera, button, power or network application code. The ordinary GitHub
installation/recovery ZIP and historical tags remain unchanged.

## Layout and data

The browser writes one 3,211,264-byte file covering `[0, 0x310000)`. The file
contains the bootloader, the original shared partition table and a compressed
application at `0x10000`. It ends on a sector boundary **before** the entire
vendor identity range `[0x310000, 0x370000)`, including cardid at `0x356000`.
An ordinary merged image reaching the application at `0x370000` is unsafe and
must not be substituted.

On first boot, a small hook verifies the compressed source, erases/writes only
the fixed application output at `0x370000`, verifies its full SHA256, then writes
a 16-byte completion record at `0x30f000`. Core has no external resource payload.
The progress display reuses the already device-checked Passport implementation
from the Spirit community installer. A 2 KiB decompression dictionary fits the
bootloader memory budget. Every Flash mutation checks an overflow-safe whitelist
and rejects any overlap with the vendor identity range.

The application is 1,996,512 bytes, SHA256
`2a9a6cfda021cc40e61530cdd29251ac687e8501f0d15804b8589bacf1bce17d`,
identical to the released/device-tested binary from Core 1.6.1. No Spirit
characters, audio, resources or personal device data are included.

**Community installation resets saved Wi-Fi/account/printer settings.** The
public file contains erased bytes in the NVS/PHY range. Set up again from the
phone after installation. Preserving current factory bytes does not reconstruct
identity already lost before installation. Never distribute someone else's
Flash backup or copy another device identity.

## Interruption and future upgrades

Keep power on until preparation finishes. An interrupted first-boot application
write can restart from the intact compressed source. A partially programmed
valid completion record can also resume. Invalid source hashes or marker bits
stop safely before attempting installation. Once complete, normal boot hands
off without revalidating/reinstalling the old compressed payload, so a compatible
later application is not silently downgraded.

If the browser write itself was interrupted, reinstall the same complete
community file. Do not use full-chip erase. This is a dedicated installer
contract: future firmware must not format the reserved marker/source area while
retaining this bootloader. To migrate to the standard sparse package, use its
guarded complete-install procedure; do not assume an app-only upgrade accepts
an unknown/community bootloader.

## Reproduction

Use ESP-IDF 5.5.5 and the Core release configuration. Prepare the pinned
application with `tools/community/prepare.py --application APP --output PAYLOAD`;
build `idf.py bootloader`; then run the same preparation command with
`--bootloader BOOT --partitions TABLE`. The packager rejects mismatched released
application bytes, versions, partition tables and bootloader descriptors, even
under Python optimization. All input credentials must remain placeholders.

`tools/community/test_qemu.py` runs the real ROM and installer against synthetic
8 MiB Flash. It checks first install, an observed interruption, partial-marker
recovery, corrupt-source/invalid-marker rejection, factory-range preservation,
and actual execution into `app_main` and display initialization. It accepts the
QEMU, nm, application ELF, bootloader ELF, firmware, manifest and output paths
through explicit arguments. No physical serial port is accessed.

The emulator does not complete this board's panel transactions: the unchanged
application with the stock bootloader reaches the same `spi_ll_usr_is_done`
wait. Reaching display initialization is not proof of visible output or complete
UI initialization. This community-specific Core installer has not been
physically flashed in this publication task. The unchanged application
previously passed the bounded USB, idle, printer-offline Core 1.6.1 network test;
live printing, camera load, other network conditions and battery-current
acceptance remain separate.

## Attribution

The decompressor and preparation pattern adapt the MIT-licensed PrintPet Spirit
community installer; the progress display is reused without changes. This Core
variant contains a single application, a Core-specific completion record and
fixed Core 1.6.1 hashes. Main source and releases:
https://github.com/ericwubeta/printpet-firmware.
