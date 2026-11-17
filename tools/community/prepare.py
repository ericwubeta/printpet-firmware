"""Wrap the exact released Core 1.6.1 app without crossing factory identity.

The application is unchanged. The one-file browser installer ends at 0x310000;
its first boot expands the pinned app at 0x370000. Never use an ordinary merge.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[2]
LIMIT, DONE, SOURCE, DESTINATION = 0x310000, 0x30F000, 0x10000, 0x370000
APP_SHA = '2a9a6cfda021cc40e61530cdd29251ac687e8501f0d15804b8589bacf1bce17d'
APP_BYTES = 1996512
TABLE_SHA = '24c0c3087457f81be878fd30144e806df84c4047e3b0dde894bbe2caf55c5492'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--application', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--bootloader', type=Path)
    p.add_argument('--partitions', type=Path)
    a = p.parse_args()
    raw = a.application.read_bytes()
    require(len(raw) == APP_BYTES and sha(raw) == APP_SHA, 'Wrong released Core 1.6.1 application')
    require(raw[0] == 0xE9 and struct.unpack_from('<H', raw, 12)[0] == 5,
            'Not an ESP32-C3 image')
    desc = raw[:1024].find(struct.pack('<I', 0xABCD5432))
    require(desc >= 0, 'Missing application descriptor')
    require(raw[desc + 16:desc + 48].split(b'\0')[0] == b'1.6.1', 'Wrong application version')
    compressor = zlib.compressobj(level=9, wbits=11)
    packed = compressor.compress(raw) + compressor.flush()
    require(zlib.decompress(packed) == raw, 'Compression roundtrip failed')
    require(SOURCE + len(packed) <= DONE and len(raw) <= 0x400000,
            'Payload exceeds safe fixed ranges')
    part = dict(name='application', source=SOURCE, destination=DESTINATION,
                packedBytes=len(packed), rawBytes=len(raw),
                packedSha256=sha(packed), rawSha256=sha(raw))
    serialized = struct.pack('<IIII', SOURCE, DESTINATION, len(packed), len(raw)) + \
        bytes.fromhex(part['packedSha256']) + bytes.fromhex(part['rawSha256'])
    def c_bytes(digest):
        return '{' + ','.join('0x' + digest[i:i + 2] for i in range(0, 64, 2)) + '}'
    header = '\n'.join([
        '/* Generated only from the pinned released Core 1.6.1 application. */',
        '#pragma once', '#define PP_WINDOW 2048', '#define PP_DONE_OFFSET 0x30f000',
        '#define PP_IMAGE_COUNT 1', 'static const pp_image_t images[] = {',
        '    {0x10000,0x370000,%d,%d,%s,%s},' % (
            len(packed), len(raw), c_bytes(part['packedSha256']), c_bytes(part['rawSha256'])),
        '};', ''])
    target = ROOT / 'bootloader_components/printpet_install/payload.h'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(header, encoding='utf-8')
    a.output.mkdir(parents=True, exist_ok=True)
    (a.output / 'application.zlib').write_bytes(packed)
    manifest = dict(format='printpet-core-community-compressed-v1', version='1.6.1',
                    applicationSourceCommit='26adfa591f15d03594c9542404046b778ccedf68',
                    windowBits=11, imageBytes=LIMIT, markerOffset=DONE,
                    markerHex=b'PC161-installed\0'.hex(), protectedStart=LIMIT,
                    protectedEnd=DESTINATION, parts=[part],
                    networkSettingsResetOnCommunityInstall=True,
                    physicalInstallerValidation='not_run', applicationUnchanged=True)
    if a.bootloader or a.partitions:
        require(a.bootloader and a.partitions, 'Supply both bootloader and partition table')
        boot, table = a.bootloader.read_bytes(), a.partitions.read_bytes()
        require(0 < len(boot) <= 0x8000 and boot[0] == 0xE9, 'Invalid bootloader')
        require(sha(table) == TABLE_SHA and len(table) == 3072, 'Wrong shared partition table')
        require(serialized in boot, 'Rebuild bootloader: compiled payload descriptor differs')
        require(b'PC161-installed\0' in boot, 'Wrong completion record in bootloader')
        image = bytearray(b'\xff' * LIMIT)
        image[:len(boot)] = boot
        image[0x8000:0x8000 + len(table)] = table
        image[SOURCE:SOURCE + len(packed)] = packed
        require(image[DONE:] == b'\xff' * 4096, 'Completion sector must start erased')
        require(image[0x9000:0x10000] == b'\xff' * 0x7000,
                'Public image must not contain personal settings')
        path = a.output / 'PrintPet-Core-1.6.1-community.bin'
        path.write_bytes(image)
        manifest.update(imageSha256=sha(image), bootloaderBytes=len(boot),
                        bootloaderSha256=sha(boot), partitionsSha256=sha(table))
    (a.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
