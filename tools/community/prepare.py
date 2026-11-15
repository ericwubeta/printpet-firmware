"""Reproduce a factory-identity-preserving, compressed community installer.

The application and resources remain byte-identical to the v2.0.1 release.
Only the bootloader and temporary low-flash installation payload differ.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[2]
LIMIT = 0x310000
DONE = LIMIT - 0x1000
ALIGN = lambda n, a=4: (n + a - 1) // a * a
SHA = lambda data: hashlib.sha256(data).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--application', type=Path, required=True)
    p.add_argument('--resources', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--bootloader', type=Path)
    p.add_argument('--partitions', type=Path)
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=True)
    descriptors = []
    cursor = 0x10000
    expected = {
        'application': '8ad4f4e69623e2ef236ba96bca1001ad3c84ac4d0e3ec0dfdbe857a5d3cfd277',
        'resources': 'b07a354f3fd0edcfe90c21381c00f5d341ddc831aaf08a604c74d8d5a801fd5d',
    }
    for name, file, dest in [('application', a.application, 0x370000),
                             ('resources', a.resources, 0x10000)]:
        raw = file.read_bytes()
        require(SHA(raw) == expected[name], f'Wrong original {name}')
        c = zlib.compressobj(level=9, wbits=11)
        packed = c.compress(raw) + c.flush()
        require(zlib.decompress(packed) == raw, 'Compression roundtrip failed')
        d = dict(name=name, source=cursor, destination=dest,
                 rawBytes=len(raw), packedBytes=len(packed),
                 rawSha256=SHA(raw), packedSha256=SHA(packed))
        (a.output / f'{name}.zlib').write_bytes(packed)
        descriptors.append(d)
        cursor = ALIGN(cursor + len(packed), 0x1000)
    require(cursor <= DONE, 'Compressed payload overlaps the completion marker')
    app, res = descriptors
    require(res['source'] >= ALIGN(res['destination'] + res['rawBytes'], 0x1000),
            'Resource destination overlaps its compressed source')
    manifest = dict(format='printpet-community-compressed-v1', version='2.0.1',
                    windowBits=11, imageBytes=LIMIT, markerOffset=DONE,
                    protectedStart=LIMIT, protectedEnd=0x370000,
                    parts=descriptors, physicalValidation='not_run')
    (a.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    def bytes_c(hexdigest):
        return '{' + ','.join('0x' + hexdigest[i:i+2] for i in range(0, 64, 2)) + '}'
    header = ['/* Generated from verified v2.0.1 bytes by tools/community/prepare.py. */',
              '#pragma once', '#define PP_WINDOW 2048', '#define PP_DONE_OFFSET 0x30f000',
              'static const pp_image_t images[] = {']
    for d in descriptors:
        header.append('    {%s,%s,%d,%d,%s,%s},' %
                      (hex(d['source']), hex(d['destination']), d['packedBytes'],
                       d['rawBytes'], bytes_c(d['packedSha256']), bytes_c(d['rawSha256'])))
    header.append('};\n')
    target = ROOT / 'bootloader_components/printpet_install/payload.h'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text('\n'.join(header), encoding='utf-8')
    if a.bootloader or a.partitions:
        require(a.bootloader and a.partitions, 'Supply both bootloader and partition table')
        boot, table = a.bootloader.read_bytes(), a.partitions.read_bytes()
        require(0 < len(boot) <= 0x8000 and boot[0] == 0xe9, 'Invalid bootloader')
        require(SHA(table) == '24c0c3087457f81be878fd30144e806df84c4047e3b0dde894bbe2caf55c5492',
                'Wrong original partition table')
        serialized = b''.join(struct.pack('<IIII', d['source'], d['destination'],
                                         d['packedBytes'], d['rawBytes']) +
                              bytes.fromhex(d['packedSha256']) + bytes.fromhex(d['rawSha256'])
                              for d in descriptors)
        require(serialized in boot, 'Rebuild bootloader: compiled payload descriptors differ')
        image = bytearray(b'\xff' * LIMIT)
        image[:len(boot)] = boot
        image[0x8000:0x8000+len(table)] = table
        for d in descriptors:
            data = (a.output / (d['name'] + '.zlib')).read_bytes()
            image[d['source']:d['source']+len(data)] = data
        require(image[DONE:] == b'\xff' * 0x1000, 'Completion sector must start erased')
        path = a.output / 'PrintPet-Spirit-2.0.1-community.bin'
        path.write_bytes(image)
        manifest.update(imageSha256=SHA(image), bootloaderBytes=len(boot),
                        bootloaderSha256=SHA(boot), partitionsSha256=SHA(table))
        (a.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
