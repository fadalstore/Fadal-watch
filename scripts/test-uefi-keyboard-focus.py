#!/usr/bin/env python3
"""Exercise UEFI keyboard focus and optional terminal input under QEMU/OVMF."""

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


def send_keys(process: subprocess.Popen[str], keys: tuple[str, ...]) -> None:
    for key in keys:
        send_monitor_command(process, f"sendkey {key}")


def ppm_pixels(path: Path) -> bytes:
    image = path.read_bytes()
    header = image.split(b"\n", 3)
    if len(header) != 4 or header[0] != b"P6" or header[2] != b"255":
        raise RuntimeError(f"invalid PPM framebuffer capture: {path}")
    return header[3]


def ppm_pixel(path: Path, x: int, y: int) -> tuple[int, int, int]:
    image = path.read_bytes()
    header = image.split(b"\n", 3)
    if len(header) != 4 or header[0] != b"P6" or header[2] != b"255":
        raise RuntimeError(f"invalid PPM framebuffer capture: {path}")
    width, height = (int(value) for value in header[1].split())
    if not (0 <= x < width and 0 <= y < height):
        raise RuntimeError(f"pixel coordinate {(x, y)} is outside {width}x{height}")
    offset = (y * width + x) * 3
    return tuple(header[3][offset:offset + 3])  # type: ignore[return-value]


def move_mouse_to(process: subprocess.Popen[str], current: list[int],
                  x: int, y: int) -> None:
    while current[0] != x or current[1] != y:
        dx = max(-80, min(80, x - current[0]))
        dy = max(-80, min(80, y - current[1]))
        send_monitor_command(process, f"mouse_move {dx} {dy}", delay=0.12)
        current[0] += dx
        current[1] += dy


def click_mouse(process: subprocess.Popen[str], current: list[int],
                x: int, y: int) -> None:
    move_mouse_to(process, current, x, y)
    send_monitor_command(process, "mouse_button 1", delay=0.2)
    send_monitor_command(process, "mouse_button 0", delay=0.22)


def capture_frame(process: subprocess.Popen[str], path: Path) -> None:
    send_monitor_command(process, f"screendump {path}", delay=0.3)
    if not path.is_file() or path.stat().st_size < 1024:
        raise RuntimeError(f"QEMU did not produce framebuffer capture: {path}")


def assert_pixel(path: Path, x: int, y: int, expected: tuple[int, int, int],
                 state: str) -> None:
    actual = ppm_pixel(path, x, y)
    if actual != expected:
        raise RuntimeError(f"{state}: pixel {(x, y)} was {actual}, expected {expected}")


def run_window_control_checks(process: subprocess.Popen[str], temp: Path,
                              screenshot: Path) -> None:
    mouse = [640, 400]
    state = temp / "window-state.ppm"

    # The initial OVMF GOP is 1280x800; these positions target the titlebar controls.
    click_mouse(process, mouse, 1181, 115)
    capture_frame(process, state)
    assert_pixel(state, 25, 100, (13, 21, 40), "maximize")
    print("PASS: the maximize control expands the terminal to the desktop work area.")

    click_mouse(process, mouse, 1215, 61)
    capture_frame(process, state)
    assert_pixel(state, 25, 100, (24, 39, 69), "restore")
    print("PASS: the maximize control restores the previous window geometry.")

    click_mouse(process, mouse, 1151, 115)
    capture_frame(process, state)
    assert_pixel(state, 300, 200, (28, 47, 82), "minimize")
    click_mouse(process, mouse, 210, 773)
    capture_frame(process, state)
    assert_pixel(state, 300, 200, (10, 16, 30), "taskbar restore")
    print("PASS: minimize hides the terminal and its taskbar button restores it.")

    click_mouse(process, mouse, 1211, 115)
    capture_frame(process, state)
    assert_pixel(state, 300, 200, (28, 47, 82), "close")
    click_mouse(process, mouse, 70, 773)
    capture_frame(process, state)
    assert_pixel(state, 30, 550, (26, 49, 85), "Start menu reopen")
    click_mouse(process, mouse, 160, 570)
    capture_frame(process, screenshot)
    assert_pixel(screenshot, 300, 200, (10, 16, 30), "Start-menu terminal reopen")
    if ppm_pixel(screenshot, 300, 110) != (45, 95, 155):
        raise RuntimeError("Start-menu reopen: terminal did not regain keyboard focus")
    print("PASS: close hides the terminal, and Start > Terminal reopens the same session.")


def wait_for_titlebar_color(process: subprocess.Popen[str], screenshot: Path,
                            expected: bytes, timeout: float = 3.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        send_monitor_command(process, f"screendump {screenshot}", delay=0.12)
        pixels = ppm_pixels(screenshot)
        width = int(screenshot.read_bytes().split(b"\n", 3)[1].split()[0])
        offset = (100 * width + 500) * 3
        if pixels[offset:offset + 3] == expected:
            return
    raise RuntimeError("terminal title-bar did not reach the requested focus state")


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
    parser.add_argument("--terminal-input", action="store_true",
                        help="also test cursor editing and PageUp/PageDown scrollback")
    parser.add_argument("--window-controls", action="store_true",
                        help="also test terminal minimize/maximize/restore/close with the PS/2 mouse")
    parser.add_argument("--window-screenshot", type=Path,
                        default=root / "kernel/uefi/out/window-controls.ppm")
    parser.add_argument("--scroll-screenshot", type=Path,
                        default=root / "kernel/uefi/out/terminal-scrollback.ppm")
    parser.add_argument("--tail-screenshot", type=Path,
                        default=root / "kernel/uefi/out/terminal-tail.ppm")
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
    if args.terminal_input:
        args.scroll_screenshot.parent.mkdir(parents=True, exist_ok=True)
        args.tail_screenshot.parent.mkdir(parents=True, exist_ok=True)
        args.scroll_screenshot.unlink(missing_ok=True)
        args.tail_screenshot.unlink(missing_ok=True)
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
            wait_for_titlebar_color(process, temp / "focus-state.ppm", bytes((28, 49, 84)))
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

            send_monitor_command(process, "sendkey tab")
            wait_for_titlebar_color(process, temp / "focus-state.ppm", bytes((45, 95, 155)))
            send_keys(process, ("h", "e", "l", "p", "ret"))
            time.sleep(0.6)
            serial = serial_log.read_text(errors="replace") if serial_log.exists() else ""
            if "commands: help clear pwd" not in serial:
                raise RuntimeError("terminal did not receive and execute input after focus returned")

            print("PASS: Tab moved focus to the desktop and terminal input was suppressed.")
            print("PASS: Tab restored terminal focus and the help command executed.")
            if args.terminal_input:
                send_keys(process, ("h", "e", "p", "left", "l", "ret"))
                time.sleep(0.6)
                serial = serial_log.read_text(errors="replace") if serial_log.exists() else ""
                if serial.count("commands: help clear pwd") < 2:
                    raise RuntimeError("left-arrow insertion did not execute the reconstructed help command")

                send_keys(process, ("h", "e", "p", "l", "home", "right", "right",
                                    "delete", "end", "p", "ret"))
                time.sleep(0.6)
                serial = serial_log.read_text(errors="replace") if serial_log.exists() else ""
                if serial.count("commands: help clear pwd") < 3:
                    raise RuntimeError("Home/Right/End/Delete editing did not execute the reconstructed help command")

                send_keys(process, ("h", "e", "l", "p", "left", "backspace", "l", "ret"))
                time.sleep(0.6)
                serial = serial_log.read_text(errors="replace") if serial_log.exists() else ""
                if serial.count("commands: help clear pwd") < 4:
                    raise RuntimeError("mid-line Backspace editing did not execute the reconstructed help command")

                for _ in range(30):
                    send_keys(process, ("l", "s", "ret"))
                send_monitor_command(process, "sendkey pgup", delay=0.45)
                send_monitor_command(process, f"screendump {args.scroll_screenshot}", delay=0.35)
                send_monitor_command(process, "sendkey pgdn", delay=0.45)
                send_monitor_command(process, f"screendump {args.tail_screenshot}", delay=0.35)
                if not args.scroll_screenshot.is_file() or not args.tail_screenshot.is_file():
                    raise RuntimeError("QEMU did not produce both scrollback framebuffer captures")
                if ppm_pixels(args.scroll_screenshot) == ppm_pixels(args.tail_screenshot):
                    raise RuntimeError("PageUp/PageDown did not change the terminal scrollback view")
                print("PASS: Left/Right, Home/End, Delete, and Backspace edited commands in place.")
                print("PASS: PageUp/PageDown changed and restored the terminal scrollback view.")
                print(f"Scrollback capture: {args.scroll_screenshot.resolve()}")
                print(f"Tail capture: {args.tail_screenshot.resolve()}")
            if args.window_controls:
                args.window_screenshot.parent.mkdir(parents=True, exist_ok=True)
                args.window_screenshot.unlink(missing_ok=True)
                run_window_control_checks(process, temp, args.window_screenshot)
                print(f"Window-controls capture: {args.window_screenshot.resolve()}")
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
