"""Build a verified sparse release ZIP from an ESP-IDF build directory."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,re,shutil,zipfile

def main():
    root=Path(__file__).resolve().parents[1]
    p=argparse.ArgumentParser();p.add_argument('--build',type=Path,default=root/'build');p.add_argument('--output',type=Path,default=root/'dist');p.add_argument('--stock-bootloader',type=Path);a=p.parse_args()
    product=re.search(r'#define APP_PRODUCT_LINE "(Core|Spirit)"',(root/'main/app_identity.h').read_text(encoding='utf-8'))[1]
    version=re.search(r'project\(passport_printer VERSION ([\d.]+)\)',(root/'CMakeLists.txt').read_text(encoding='utf-8'))[1]
    dest=a.output/f'PrintPet-{product}-{version}';dest.mkdir(parents=True,exist_ok=False);(dest/'images').mkdir()
    spec=importlib.util.spec_from_file_location('guard',root/'tools/installer/printpet_flash.py');guard=importlib.util.module_from_spec(spec);spec.loader.exec_module(guard)
    sha=lambda q:hashlib.sha256(q.read_bytes()).hexdigest()
    if (root/'bootloader_components/printpet_install').exists() and not a.stock_bootloader:
        raise RuntimeError('Sparse packages require --stock-bootloader; do not use the compressed installer bootloader')
    stock=a.stock_bootloader or a.build/'bootloader/bootloader.bin'
    if a.stock_bootloader:
        if sha(stock)!='d3666c37877a2383e00e6dd9aadff9e4e7edca0b768ff1c7d69e73e81dae3c92':
            raise RuntimeError('Wrong selected ESP-IDF5.5 stock ESP32-C3 bootloader')
    items=[('bootloader',stock),('partitions',a.build/'partition_table/partition-table.bin'),('application',a.build/'passport_printer.bin')]
    if product=='Spirit':
        resource_version='2.0.1' if version in {'2.0.2', '2.0.3', '2.0.6'} else version
        items.append(('resources',root/f'resources/spirit-resources-{resource_version}.bin'))
    parts=[]
    for role,src in items:
        offset=guard.ADDRESSES[role];name=f'images/{role}-at-{hex(offset)}.bin';target=dest/name;shutil.copy2(src,target);parts.append({'role':role,'file':name,'offset':offset,'bytes':target.stat().st_size,'sha256':sha(target)})
    m={'format':'printpet-sparse-v1','product':product,'version':version,'chip':'ESP32-C3','flashBytes':0x800000,'parts':parts,'deviceTested':False}
    (dest/'manifest.json').write_text(json.dumps(m,indent=2),encoding='utf-8')
    for src in (root/'tools/installer').glob('*'):
        if src.is_file():shutil.copy2(src,dest/src.name)
    shutil.copy2(root/'docs/INSTALL.md',dest/'INSTALL.md')
    for name in ('LICENSE', 'LICENSING.md', 'LICENSES/MIT.txt'):
        target = dest / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / name, target)
    release_notes = root / 'docs' / f'RELEASE_{product.upper()}_{version}.md'
    if release_notes.is_file():
        shutil.copy2(release_notes, dest / release_notes.name)
    guard.validate(dest)
    (dest/'SHA256SUMS.txt').write_text(''.join(f'{sha(q)}  {q.relative_to(dest).as_posix()}\n' for q in sorted(dest.rglob('*')) if q.is_file()),encoding='utf-8')
    archive=a.output/f'PrintPet-{product}-{version}-install-recovery.zip'
    with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
        for q in sorted(dest.rglob('*')):
            if q.is_file():z.write(q,q.relative_to(dest))
    print(json.dumps({'archive':str(archive),'sha256':sha(archive),'deviceTested':False}))

if __name__=='__main__':main()
