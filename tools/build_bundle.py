"""Build a verified sparse release ZIP from an ESP-IDF build directory."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,re,shutil,zipfile

def main():
    root=Path(__file__).resolve().parents[1]
    p=argparse.ArgumentParser();p.add_argument('--build',type=Path,default=root/'build');p.add_argument('--output',type=Path,default=root/'dist')
    p.add_argument('--device-tested',action='store_true');p.add_argument('--device-test-scope')
    a=p.parse_args()
    if bool(a.device_tested) != bool(a.device_test_scope):p.error('--device-tested and --device-test-scope must be supplied together')
    product=re.search(r'#define APP_PRODUCT_LINE "(Core|Spirit)"',(root/'main/app_identity.h').read_text(encoding="utf-8"))[1]
    version=re.search(r'project\(passport_printer VERSION ([\d.]+)\)',(root/'CMakeLists.txt').read_text(encoding="utf-8"))[1]
    dest=a.output/f'PrintPet-{product}-{version}';dest.mkdir(parents=True,exist_ok=False);(dest/'images').mkdir()
    spec=importlib.util.spec_from_file_location('guard',root/'tools/installer/printpet_flash.py');guard=importlib.util.module_from_spec(spec);spec.loader.exec_module(guard)
    sha=lambda q:hashlib.sha256(q.read_bytes()).hexdigest()
    items=[('bootloader',a.build/'bootloader/bootloader.bin'),('partitions',a.build/'partition_table/partition-table.bin'),('application',a.build/'passport_printer.bin')]
    if product=='Spirit':items.append(('resources',root/f'resources/spirit-resources-{version}.bin'))
    parts=[]
    for role,src in items:
        offset=guard.ADDRESSES[role];name=f'images/{role}-at-{hex(offset)}.bin';target=dest/name;shutil.copy2(src,target);parts.append({'role':role,'file':name,'offset':offset,'bytes':target.stat().st_size,'sha256':sha(target)})
    m={'format':'printpet-sparse-v1','product':product,'version':version,'chip':'ESP32-C3','flashBytes':0x800000,'parts':parts,'deviceTested':a.device_tested}
    if a.device_tested:m['deviceTestScope']=a.device_test_scope
    (dest/'manifest.json').write_text(json.dumps(m,indent=2),encoding='utf-8')
    for src in (root/'tools/installer').glob('*'):
        if src.is_file():shutil.copy2(src,dest/src.name)
    shutil.copy2(root/'docs/INSTALL.md',dest/'INSTALL.md')
    for name in ('LICENSE', 'LICENSING.md'):
        shutil.copy2(root / name, dest / name)
    shutil.copytree(root / 'LICENSES', dest / 'LICENSES')
    release_notes=root/'docs'/f'RELEASE_{product.upper()}_{version}.md'
    if release_notes.is_file():shutil.copy2(release_notes,dest/release_notes.name)
    guard.validate(dest)
    (dest/'SHA256SUMS.txt').write_text(''.join(f'{sha(q)}  {q.relative_to(dest).as_posix()}\n' for q in sorted(dest.rglob('*')) if q.is_file()),encoding='utf-8')
    archive=a.output/f'PrintPet-{product}-{version}-install-recovery.zip'
    with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
        for q in sorted(dest.rglob('*')):
            if q.is_file():z.write(q,q.relative_to(dest))
    print(json.dumps({'archive':str(archive),'sha256':sha(archive),'deviceTested':a.device_tested}))

if __name__=='__main__':main()
