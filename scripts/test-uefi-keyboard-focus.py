#!/usr/bin/env python3
"""Exercise UEFI desktop keyboard focus routing under QEMU/OVMF."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


def send_monitor_command(process: subprocess.Popen[str], command: str,
                         delay: float = 0.18) -> None:
    if process.stdin is None:
        raise RuntimeError("QEMU monitor input is unavailable")
    process.stdin.write(command + "\n")
    process.stdin.flush()
    time.sleep(delay)


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path,
                        default=root / "kernel/out/fadal-uefi.img")
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--ovmf-code", type=Path,
                        default=Path(os.environ.get("OVMF_CODE", "/usr/share/OVMF/OVMF_CODE_4M.fd")))
    parser.add_argument("--ovmf-vars", type=Path,
                        default=Path(os.environ.get("OVMF_VARS", "/usr/share/OVMF/OVMF_VARS_4M.fd")))
    parser.add_argument("--screenshot", type=Path,
                        default=root / "kernel/uefi/out/keyboard-focus.ppm")
    parser.add_argument("--timeout", type=float, default=20.0)
    args = parser.parse_args()

    for path, label in ((args.image, "UEFI image"),
                        (args.ovmf_code, "OVMF code image"),
                        (args.ovmf_vars, "OVMF variables image")):
        if not path.is_file():
            parser.error(f"{label} not found: {path}")
    if shutil.which(args.qemu) is None:
        parser.error(f"QEMU executable not found: {args.qemu}")

    args.screenshot.parent.mkdir(parents=True, exist_ok=True)
    args.screenshot.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix="fadal-focus-") as temporary:
        temp = Path(temporary)
        serial_log = temp / "serial.log"
        debug_log = temp / "debugcon.log"
        vars_copy = temp / "OVMF_VARS.fd"
        shutil.copyfile(args.ovmf_vars, vars_copy)
        command = [
            args.qemu, "-machine", "q35", "-m", "512M",
            "-drive", f"if=pflash,format=raw,readonly=on,file={args.ovmf_code}",
            "-drive", f"if=pflash,format=raw,file={vars_copy}",
            "-drive", f"format=raw,file={args.image}",
            "-display", "none", "-monitor", "stdio",
            "-serial", f"file:{serial_log}",
            "-debugcon", f"file:{debug_log}",
            "-global", "isa-debugcon.iobase=0x402",
            "-no-reboot", "-no-shutdown",
        ]
        process = subprocess.Popen(
            command, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT, text=True,
        )
        try:
            deadline = time.monotonic() + args.timeout
            while time.monotonic() < deadline:
                if debug_log.exists() and b"FADALOS_CONSOLE_ENTRY" in debug_log.read_bytes():
                    break
                if process.poll() is not None:
                    raise RuntimeError(f"QEMU exited before payload entry ({process.returncode})")
                time.sleep(0.2)
            else:
                raise RuntimeError("timed out waiting for the UEFI payload entry marker")

            time.sleep(0.8)
            send_monitor_command(process, "sendkey tab")
            time.sleep(0.35)
            for _ in range(4):
                send_monitor_command(process, "sendkey z")
            time.sleep(0.5)
            send_monitor_command(process, f"screendump {args.screenshot}")
            time.sleep(0.35)

            serial = serial_log.read_text(errors="replace") if serial_log.exists() else ""
            if "zzzz" in serial:
                raise RuntimeError("keyboard input leaked to the terminal while desktop had focus")
            if not args.screenshot.is_file() or args.screenshot.stat().st_size < 1024:
                raise RuntimeError("QEMU did not produce the requested desktop-focus screenshot")

            for key in ("tab", "h", "e", "l", "p", "ret"):
                send_monitor_command(process, f"sendkey {key}")
            time.sleep(0.6)
            serial = serial_log.read_text(errors="replace") if serial_log.exists() else ""
            if "commands: help clear pwd" not in serial:
                raise RuntimeError("terminal did not receive and execute input after focus returned")

            print("PASS: Tab moved focus to the desktop and terminal input was suppressed.")
            print("PASS: Tab restored terminal focus and the help command executed.")
            print(f"Framebuffer screenshot: {args.screenshot.resolve()}")
            return 0
        finally:
            if process.poll() is None:
                try:
                    send_monitor_command(process, "quit", delay=0)
                    process.wait(timeout=3)
                except (BrokenPipeError, OSError, subprocess.TimeoutExpired):
                    process.terminate()
                    try:
                        process.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=2)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
