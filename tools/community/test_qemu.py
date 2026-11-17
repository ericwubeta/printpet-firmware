"""Execute the real Core installer/ROM in QEMU with synthetic Flash only.

No serial ports, physical devices or user settings are accessed. Installation,
interruption, corrupt input, partial marker, and native application handoff are
checked separately; this is not physical display or printer acceptance.
"""
import argparse
import hashlib
import json
import random
import socket
import subprocess
import threading
import time
from pathlib import Path

from qemu_common import run
from gdb_remote import Remote


def need(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def symbols(nm, elf):
    result = {}
    for line in subprocess.check_output([str(nm), str(elf)], text=True).splitlines():
        columns = line.split()
        if len(columns) == 3:
            result[columns[2]] = int(columns[0], 16)
    return result


def milestones(qemu, flash, folder, marks, timeout=45):
    folder.mkdir(parents=True, exist_ok=True)
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    cmd = [str(qemu), '-display', 'none', '-monitor', 'none', '-serial',
           'file:' + str((folder / 'serial.log').resolve()), '-qmp', 'stdio',
           '-icount', '3', '-machine', 'esp32c3', '-drive',
           f'file={flash.resolve()},if=mtd,format=raw', '-S', '-gdb', f'tcp:127.0.0.1:{port}']
    qmp = []
    hits = []
    with (folder / 'stderr.log').open('w') as err:
        proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=err, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
        thread = threading.Thread(target=lambda: qmp.extend(proc.stdout), daemon=True)
        thread.start()
        remote = None
        try:
            for _ in range(50):
                try:
                    remote = Remote(port)
                    break
                except OSError:
                    time.sleep(.1)
            need(remote is not None, 'QEMU GDB did not start')
            proc.stdin.write('{"execute":"qmp_capabilities"}\n')
            proc.stdin.flush()
            remote.send('?')
            remote.send('qSupported')
            for label, addr in marks:
                need(remote.send(f'Z1,{addr:x},2') == 'OK', 'Breakpoint rejected')
                began = time.monotonic()
                reply = remote.run(timeout)
                regs = remote.regs()
                hit = int(regs['pc'], 16) == addr
                hits.append(dict(name=label, address=hex(addr), hit=hit, registers=regs,
                                 reply=reply, elapsedSeconds=round(time.monotonic() - began, 3)))
                remote.send(f'z1,{addr:x},2')
                if not hit:
                    break
        finally:
            if remote:
                remote.s.close()
            if proc.poll() is None:
                proc.stdin.write('{"execute":"quit"}\n')
                proc.stdin.flush()
                try:
                    proc.wait(5)
                except subprocess.TimeoutExpired:
                    proc.terminate()
                    proc.wait(3)
            thread.join(timeout=2)
    (folder / 'qmp.log').write_text(''.join(qmp), encoding='utf-8')
    return dict(command=cmd, milestones=hits, passed=len(hits) == len(marks) and all(x['hit'] for x in hits))


def main():
    p = argparse.ArgumentParser()
    for name in ('qemu', 'nm', 'application-elf', 'bootloader-elf', 'firmware', 'manifest', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=True)
    image = a.firmware.read_bytes()
    m = json.loads(a.manifest.read_text(encoding='utf-8'))
    need(sha(image) == m['imageSha256'] and len(image) == 0x310000, 'Wrong test firmware')
    part = m['parts'][0]
    need(len(m['parts']) == 1, 'Core must have one application')
    start, end, done = m['protectedStart'], m['protectedEnd'], m['markerOffset']
    marker = bytes.fromhex(m['markerHex'])
    need(len(marker) == 16 and start == 0x310000 and end == 0x370000, 'Wrong protected geometry')
    original = random.Random(161).randbytes(0x800000)
    fresh = bytearray(original)
    fresh[:len(image)] = image
    vendor = original[start:end]
    def piece(data):
        return data[part['destination']:part['destination'] + part['rawBytes']]
    def complete(data):
        return data[done:done + 16] == marker and sha(piece(data)) == part['rawSha256']
    cases = []
    def verify_case(name, flash, extra=True):
        data = flash.read_bytes()
        ok = complete(data) and data[start:end] == vendor and extra
        cases.append(dict(name=name, passed=ok, applicationMatches=sha(piece(data)) == part['rawSha256'],
                          vendorUnchanged=data[start:end] == vendor,
                          markerMatches=data[done:done + 16] == marker))
        need(ok, name + ' failed')
        print(name + ': PASS', flush=True)
        return data
    first = a.output / 'first-install.bin'
    first.write_bytes(fresh)
    first_run = run(a.qemu, first, a.output / 'first-install', complete, timeout=50)
    installed = verify_case('first_install', first, first_run['matched'])

    resume = a.output / 'interrupted-install.bin'
    resume.write_bytes(fresh)
    initial = piece(fresh)
    def incomplete(data):
        return piece(data) != initial and sha(piece(data)) != part['rawSha256']
    interrupted = run(a.qemu, resume, a.output / 'interrupted', incomplete, timeout=45)
    partial = resume.read_bytes()
    need(interrupted['matched'] and incomplete(partial) and partial[start:end] == vendor,
         'Did not capture a real incomplete install; cannot claim interruption coverage')
    resumed = run(a.qemu, resume, a.output / 'resumed', complete, timeout=50)
    verify_case('resume_actual_interruption', resume, resumed['matched'])

    partial_marker = bytearray(installed)
    partial_marker[done:done + 16] = marker[:8] + b'\xff' * 8
    marker_flash = a.output / 'partial-marker.bin'
    marker_flash.write_bytes(partial_marker)
    marker_run = run(a.qemu, marker_flash, a.output / 'partial-marker', complete, timeout=30)
    verify_case('resume_partial_marker', marker_flash, marker_run['matched'])

    boot_symbols = symbols(a.nm, a.bootloader_elf)
    for label, offset in [('corrupt_source', part['source']), ('invalid_marker', done)]:
        bad = bytearray(fresh)
        bad[offset] = 0 if label == 'invalid_marker' else bad[offset] ^ 0x80
        path = a.output / (label + '.bin')
        path.write_bytes(bad)
        trace = milestones(a.qemu, path, a.output / label, [('safe_stop', boot_symbols['stop'])])
        after = path.read_bytes()
        ok = trace['passed'] and after[start:end] == vendor and piece(after) == piece(bad) and after == bad
        cases.append(dict(name=label, passed=ok, reachedSafeStop=trace['passed'],
                          noFlashMutation=after == bad, execution=trace))
        need(ok, label + ' did not fail closed')
        print(label + ': PASS', flush=True)

    # A valid marker must hand off even if the old compressed source is damaged.
    native = bytearray(installed)
    native[part['source']] ^= 0x80
    native_path = a.output / 'native-handoff.bin'
    native_path.write_bytes(native)
    app_symbols = symbols(a.nm, a.application_elf)
    # QEMU does not complete this board's physical panel transactions. The
    # stock bootloader reaches the same display wait, recorded independently.
    # Check actual app handoff and display-init entry, not a simulated screen.
    marks = [(name, app_symbols[name]) for name in ('app_main', 'bsp_display_init')]
    trace = milestones(a.qemu, native_path, a.output / 'native-handoff', marks)
    verify_case('native_application_and_display_init_entry', native_path, trace['passed'])
    cases[-1]['execution'] = trace
    report = dict(passed=all(x['passed'] for x in cases), firmwareSha256=sha(image),
                  applicationSha256=part['rawSha256'], cases=cases, actualDeviceUsed=False,
                  physicalDisplayValidated=False, physicalPrinterValidated=False,
                  qemuVersion=subprocess.check_output([str(a.qemu), '--version'], text=True).strip())
    (a.output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'passed':report['passed'], 'cases':len(cases), 'actualDeviceUsed':False}), flush=True)


if __name__ == '__main__':
    main()
