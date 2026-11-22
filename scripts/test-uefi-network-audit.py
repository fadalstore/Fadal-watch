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


def wait_for_any_text(path: Path, texts: tuple[str, ...], start: int,
                      timeout: float) -> str | None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        output = path.read_text(errors="replace") if path.exists() else ""
        recent = output[start:]
        for text in texts:
            if text in recent:
                return text
        time.sleep(0.1)
    return None


def diagnostic_value(serial: str, prefix: str) -> str:
    for line in serial.splitlines():
        if prefix in line:
            return line.split(prefix, 1)[1].strip()
    return "not reported"


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


def send_text(process: subprocess.Popen[str], text: str, serial_log: Path,
              delay: float = 0.35) -> None:
    key_names = {" ": "spc", ".": "dot", "/": "slash", "-": "minus",
                 ":": "shift-semicolon"}
    offset = serial_log.stat().st_size if serial_log.exists() else 0
    expected = bytearray()
    for character in text:
        expected.extend(character.encode("ascii"))
        for _ in range(3):
            monitor(process, f"sendkey {key_names.get(character, character)}", delay=delay)
            deadline = time.monotonic() + 1.5
            while time.monotonic() < deadline:
                data = serial_log.read_bytes() if serial_log.exists() else b""
                if data[offset:].endswith(expected):
                    if character == ":":
                        monitor(process, "sendkey shift", delay=0.1)
                    break
                time.sleep(0.02)
            else:
                continue
            break
        else:
            recent = serial_log.read_bytes()[offset:] if serial_log.exists() else b""
            raise RuntimeError(f"QEMU did not echo command prefix {expected!r}; "
                               f"observed {recent[-120:]!r}")
    for _ in range(3):
        monitor(process, "sendkey ret", delay=delay)
        deadline = time.monotonic() + 1.5
        while time.monotonic() < deadline:
            data = serial_log.read_bytes() if serial_log.exists() else b""
            if b"\r\n" in data[offset + len(expected):]:
                return
            time.sleep(0.02)
    raise RuntimeError(f"QEMU did not submit command {text!r}")


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=root / "kernel/uefi/out/network-audit.img")
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--ovmf-code", type=Path,
                        default=Path(os.environ.get("OVMF_CODE", "/usr/share/OVMF/OVMF_CODE_4M.fd")))
    parser.add_argument("--ovmf-vars", type=Path,
                        default=Path(os.environ.get("OVMF_VARS", "/usr/share/OVMF/OVMF_VARS_4M.fd")))
    parser.add_argument("--http-fixture", type=Path,
                        default=root / "scripts/uefi-http-fixture.py")
    parser.add_argument("--network-device", choices=("e1000", "virtio-net-pci"),
                        default="e1000")
    parser.add_argument("--virtio-rng", action="store_true")
    parser.add_argument("--disable-option-rom", action="store_true")
    parser.add_argument("--hostname-url", default="",
                        help="optional hostname URL for a separate fixture probe; failures remain unverified")
    parser.add_argument("--screenshot", type=Path,
                        default=root / "kernel/uefi/out/network-audit-system.ppm")
    parser.add_argument("--timeout", type=float, default=25.0)
    args = parser.parse_args()

    for path, label in ((args.image, "UEFI image"),
                        (args.ovmf_code, "OVMF code image"),
                        (args.ovmf_vars, "OVMF variables image"),
                        (args.http_fixture, "HTTP fixture script")):
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
        fixture_log = temp / "http-fixture.log"
        network_backend = (
            "user,id=net0,restrict=on,"
            "guestfwd=tcp:10.0.2.100:80-cmd:/usr/bin/python3 "
            f"{args.http_fixture.resolve()}"
        )
        command = [
            args.qemu, "-machine", "q35", "-m", "512M", "-k", "en-us",
            "-drive", f"if=pflash,format=raw,readonly=on,file={args.ovmf_code}",
            "-drive", f"if=pflash,format=raw,file={vars_copy}",
            "-drive", f"format=raw,file={image}",
            "-netdev", network_backend,
            "-device", f"{args.network_device},netdev=net0",
            "-display", "none", "-monitor", "stdio",
            "-serial", f"file:{serial_log}",
            "-debugcon", f"file:{debug_log}",
            "-global", "isa-debugcon.iobase=0x402",
            "-no-reboot", "-no-shutdown",
        ]
        if args.virtio_rng:
            command.extend(["-device", "virtio-rng-pci"])
        if args.disable_option_rom:
            command.extend(["-global", f"{args.network_device}.romfile="])
        environment = os.environ.copy()
        environment["FADAL_UEFI_HTTP_FIXTURE_LOG"] = str(fixture_log)
        process = subprocess.Popen(
            command, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT, text=True, env=environment,
        )
        try:
            wait_for_text(serial_log, "UEFI network audit: SNP interfaces=", args.timeout)
            wait_for_text(serial_log, "fadal:/home/root#", args.timeout)
            time.sleep(0.75)
            serial = serial_log.read_text(errors="replace")
            required = (
                "UEFI network audit: HTTP service binding=",
                "UEFI network audit: EFI_RNG protocol=",
                "UEFI network audit: EFI_HTTP LocateProtocol status=",
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
            rng_available = "UEFI network audit: EFI_RNG protocol=available" in serial
            http_locate_status = diagnostic_value(
                serial, "UEFI network audit: EFI_HTTP LocateProtocol status=")
            http_child_status = diagnostic_value(
                serial, "UEFI network audit: EFI_HTTP CreateChild status=")
            http_open_status = diagnostic_value(
                serial, "UEFI network audit: EFI_HTTP OpenProtocol status=")
            if binding_available and not http_available and http_child_status == "not reported":
                raise RuntimeError("HTTP Service Binding was present but CreateChild status was not reported")

            mouse = [640, 400]
            click_mouse(process, mouse, 640, 400)
            send_text(process, "net", serial_log)
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

            download_name = "fadal-http-fixture.txt"
            send_text(process,
                      f"download http://10.0.2.100/fixture.txt {download_name}",
                      serial_log)
            try:
                wait_for_text(serial_log, "download: saved under /home/root", args.timeout)
            except RuntimeError as error:
                fixture_state = (fixture_log.read_text(errors="replace")
                                 if fixture_log.exists() else "fixture process not invoked")
                raise RuntimeError(f"{error}\nHTTP fixture log: {fixture_state}") from error
            send_text(process, f"cat {download_name}", serial_log)
            wait_for_text(serial_log, "FADAL_UEFI_HTTP_FIXTURE_OK", args.timeout)

            hostname_probe_passed = False
            if args.hostname_url:
                if not args.hostname_url.startswith("http://") or " " in args.hostname_url:
                    raise RuntimeError("--hostname-url must be a single plaintext http:// URL")
                fixture_before = (fixture_log.read_bytes().count(b"request-line:")
                                  if fixture_log.exists() else 0)
                probe_offset = serial_log.stat().st_size if serial_log.exists() else 0
                send_text(process,
                          f"download {args.hostname_url} fadal-hostname-fixture.txt",
                          serial_log)
                probe_result = wait_for_any_text(
                    serial_log,
                    ("download: saved under /home/root", "download: failed"),
                    probe_offset, args.timeout)
                fixture_after = (fixture_log.read_bytes().count(b"request-line:")
                                 if fixture_log.exists() else fixture_before)
                if (probe_result == "download: saved under /home/root" and
                        fixture_after > fixture_before):
                    marker_offset = serial_log.stat().st_size if serial_log.exists() else 0
                    send_text(process, "cat fadal-hostname-fixture.txt", serial_log)
                    marker_result = wait_for_any_text(
                        serial_log, ("FADAL_UEFI_HTTP_FIXTURE_OK",),
                        marker_offset, args.timeout)
                    hostname_probe_passed = marker_result is not None
                    if hostname_probe_passed:
                        print("PASS: hostname-based HTTP GET reached the local fixture and returned its marker.")
                    else:
                        print("NOTE: hostname GET was saved but its marker was not observed on readback; DNS/hostname remains UNVERIFIED.")
                else:
                    print(f"NOTE: hostname probe result={probe_result or 'no completion observed'}; "
                          f"new fixture requests={fixture_after - fixture_before}.")
                    print("NOTE: the failed probe does not distinguish DNS failure from connection failure; DNS/hostname remains UNVERIFIED.")
                    if probe_result is None:
                        print("NOTE: no completed download result was observed before the probe timeout.")

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
            print(f"UEFI network detail: EFI_RNG={'available' if rng_available else 'unavailable'}, "
                  f"EFI_HTTP LocateProtocol={http_locate_status}, "
                  f"CreateChild={http_child_status}, OpenProtocol={http_open_status}")
            final_serial = serial_log.read_text(errors="replace")
            station_line = next((line.strip() for line in final_serial.splitlines()
                                 if "UEFI IPv4 DHCP station=" in line), "not reported")
            print(f"UEFI IPv4 configuration: {station_line}")
            print("PASS: loader audit and the `net` command report matching firmware capabilities.")
            print("PASS: bounded plaintext HTTP GET returned and saved the local fixture body.")
            if args.hostname_url:
                print(f"Hostname probe URL: {args.hostname_url}")
                if hostname_probe_passed:
                    print("NOTE: only this hostname-to-local-fixture path was confirmed; general DNS, Internet access, and HTTPS/TLS remain unverified.")
                else:
                    print("NOTE: hostname resolution/reachability remains unverified; Internet access and HTTPS/TLS are also unverified.")
            else:
                print("NOTE: the GET used a numeric address; DNS, Internet access, and HTTPS/TLS remain unverified.")
            print(f"System panel capture: {args.screenshot.resolve()}")
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
