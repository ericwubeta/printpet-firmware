"""Observe and interrupt two real QEMU installation phases, then reboot."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import queue
import random
import shutil
import subprocess
import threading
import time


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def run(qemu: Path, flash: Path, output: Path, predicate, timeout: float = 45) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    serial = (output / "serial.log").resolve()
    serial.write_bytes(b"")
    command = [str(qemu.resolve()), "-display", "none", "-monitor", "none", "-serial", "file:" + str(serial),
               "-qmp", "stdio", "-icount", "3", "-machine", "esp32c3", "-drive", f"file={flash.resolve()},if=mtd,format=raw"]
    messages: queue.Queue[str] = queue.Queue()
    began = time.monotonic()
    matched = None
    with (output / "qmp.log").open("w", encoding="utf-8") as log, (output / "stderr.log").open("w", encoding="utf-8") as err:
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=err, text=True,
                                   creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)

        def reader() -> None:
            assert process.stdout is not None
            for line in process.stdout:
                log.write(line)
                log.flush()
                messages.put(line)

        thread = threading.Thread(target=reader, daemon=True)
        thread.start()
        capabilities = False
        while time.monotonic() - began < timeout:
            while not messages.empty():
                line = messages.get_nowait()
                if '"QMP"' in line and not capabilities:
                    assert process.stdin is not None
                    process.stdin.write('{"execute":"qmp_capabilities"}\n')
                    process.stdin.flush()
                    capabilities = True
            if capabilities:
                matched = predicate(flash.read_bytes())
                if matched:
                    break
            if process.poll() is not None:
                break
            time.sleep(0.02)
        if process.poll() is None:
            try:
                assert process.stdin is not None
                process.stdin.write('{"execute":"quit"}\n')
                process.stdin.flush()
                process.wait(timeout=5)
            except (OSError, BrokenPipeError, subprocess.TimeoutExpired):
                process.terminate()
                process.wait(timeout=3)
        thread.join(timeout=2)
    return {"matched": matched, "elapsedSeconds": round(time.monotonic() - began, 3), "exitCode": process.returncode,
            "command": command, "interruptionMethod": "QMP quit; simulated CPU stop, not a physical power test"}
