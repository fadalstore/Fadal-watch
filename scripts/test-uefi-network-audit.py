#!/usr/bin/env python3
"""Audit UEFI network protocols exposed to Fadal64 under QEMU/OVMF."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def monitor(process: subprocess.Popen[str], command: str, delay: float = 0.25) -> None:
    if process.stdin is None:
        raise RuntimeError("QEMU monitor input is unavailable")
    process.stdin.write(command + "\n")
    process.stdin.flush()
    time.sleep(delay)


def wait_for_text(path: Path, text: str, timeout: float = 20.0) -> str:
    deadline = time.monotonic() + timeout
    output = ""
    while time.monotonic() < deadline:
        output = path.read_text(errors="replace") if path.exists() else ""
        if text in output:
            return output
        time.sleep(0.1)
    raise RuntimeError(f"timed out waiting for {text!r}; recent serial: {output[-1200:]}")


def move_mouse(process: subprocess.Popen[str], position: list[int], x: int, y: int) -> None:
    while position[0] != x or position[1] != y:
        dx = max(-80, min(80, x - position[0]))
        dy = max(-80, min(80, y - position[1]))
        monitor(process, f"mouse_move {dx} {dy}", delay=0.12)
        position[0] += dx
        position[1] += dy


def click_mouse(process: subprocess.Popen[str], position: list[int], x: int, y: int) -> None:
    move_mouse(process, position, x, y)
    monitor(process, "mouse_button 1", delay=0.2)
    monitor(process, "mouse_button 0", delay=0.25)


def send_net_command(process: subprocess.Popen[str]) -> None:
    for key in ("n", "e", "t", "ret"):
        monitor(process, f"sendkey {key}", delay=0.25)


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=root / "kernel/uefi/out/network-audit.img")
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--ovmf-code", type=Path,
                        default=Path(os.environ.get("OVMF_CODE", "/usr/share/OVMF/OVMF_CODE_4M.fd")))
    parser.add_argument("--ovmf-vars", type=Path,
                        default=Path(os.environ.get("OVMF_VARS", "/usr/share/OVMF/OVMF_VARS_4M.fd")))
    parser.add_argument("--screenshot", type=Path,
                        default=root / "kernel/uefi/out/network-audit-system.ppm")
    parser.add_argument("--timeout", type=float, default=25.0)
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
    with tempfile.TemporaryDirectory(prefix="fadal-uefi-net-audit-") as temporary:
        temp = Path(temporary)
        image = temp / "network-audit.img"
        shutil.copyfile(args.image, image)
        vars_copy = temp / "OVMF_VARS.fd"
        shutil.copyfile(args.ovmf_vars, vars_copy)
        serial_log = temp / "serial.log"
        debug_log = temp / "debugcon.log"
        command = [
            args.qemu, "-machine", "q35", "-m", "512M",
            "-drive", f"if=pflash,format=raw,readonly=on,file={args.ovmf_code}",
            "-drive", f"if=pflash,format=raw,file={vars_copy}",
            "-drive", f"format=raw,file={image}",
            "-netdev", "user,id=net0,restrict=on",
            "-device", "e1000,netdev=net0",
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
            wait_for_text(serial_log, "UEFI network audit: SNP interfaces=", args.timeout)
            wait_for_text(serial_log, "fadal:/home/root#", args.timeout)
            time.sleep(0.75)
            serial = serial_log.read_text(errors="replace")
            required = (
                "UEFI network audit: HTTP service binding=",
                "UEFI network audit: EFI_HTTP protocol=",
                "hostname lookup delegated to EFI_HTTP when available",
            )
            missing = [text for text in required if text not in serial]
            if missing:
                raise RuntimeError("missing UEFI network audit lines: " + ", ".join(missing))

            count_text = serial.split("UEFI network audit: SNP interfaces=", 1)[1].splitlines()[0]
            try:
                interface_count = int(count_text)
            except ValueError as error:
                raise RuntimeError(f"invalid SNP interface count: {count_text!r}") from error
            http_available = "UEFI network audit: EFI_HTTP protocol=available" in serial
            binding_available = "UEFI network audit: HTTP service binding=available" in serial

            send_net_command(process)
            serial = wait_for_text(serial_log,
                                   "network: hostname lookup delegated to EFI_HTTP when available; native DNS absent",
                                   args.timeout)
            expected_snp = ("network: UEFI SNP interface exposed" if interface_count != 0
                            else "network: UEFI SNP interface not exposed")
            expected_http = ("network: UEFI HTTP protocol available" if http_available
                             else "network: UEFI HTTP protocol unavailable")
            expected_binding = ("network: HTTP service binding available" if binding_available
                                else "network: HTTP service binding unavailable")
            for expected in (expected_snp, expected_http, expected_binding):
                if expected not in serial:
                    raise RuntimeError(f"net command disagrees with loader audit; missing {expected!r}")

            mouse = [640, 400]
            click_mouse(process, mouse, 70, 773)
            click_mouse(process, mouse, 160, 678)
            monitor(process, f"screendump {args.screenshot}", delay=0.5)
            deadline = time.monotonic() + 3.0
            while time.monotonic() < deadline and not args.screenshot.is_file():
                time.sleep(0.1)
            if not args.screenshot.is_file() or args.screenshot.stat().st_size < 1024:
                raise RuntimeError("QEMU did not capture the System network-capability panel")
            header = args.screenshot.read_bytes().split(b"\n", 3)
            if len(header) != 4 or header[0] != b"P6":
                raise RuntimeError("QEMU produced an invalid System panel PPM capture")

            print(f"UEFI network audit: SNP interfaces={interface_count}, "
                  f"HTTP binding={'available' if binding_available else 'unavailable'}, "
                  f"EFI_HTTP={'available' if http_available else 'unavailable'}")
            print("PASS: loader audit and the `net` command report matching firmware capabilities.")
            print(f"System panel capture: {args.screenshot.resolve()}")
            print("NOTE: this is a capability probe; it does not claim a live HTTP/DNS request.")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=3)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
