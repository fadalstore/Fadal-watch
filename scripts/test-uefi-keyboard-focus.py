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


def send_text(process: subprocess.Popen[str], text: str, serial_log: Path) -> None:
    key_names = {" ": "spc", ".": "dot", "/": "slash", "-": "minus"}
    offset = serial_log.stat().st_size if serial_log.exists() else 0
    expected = bytearray()
    for character in text:
        expected.extend(character.encode("ascii"))
        for attempt in range(3):
            send_monitor_command(process,
                                 f"sendkey {key_names.get(character, character)}",
                                 delay=0.25)
            deadline = time.monotonic() + 1.0
            while time.monotonic() < deadline:
                data = serial_log.read_bytes() if serial_log.exists() else b""
                if data[offset:].endswith(expected):
                    break
                time.sleep(0.02)
            else:
                continue
            break
        else:
            recent = serial_log.read_bytes()[offset:] if serial_log.exists() else b""
            raise RuntimeError(f"QEMU did not echo command prefix {expected!r}; observed {recent[-120:]!r}")
    for attempt in range(3):
        send_monitor_command(process, "sendkey ret", delay=0.25)
        deadline = time.monotonic() + 1.0
        while time.monotonic() < deadline:
            data = serial_log.read_bytes() if serial_log.exists() else b""
            if b"\r\n" in data[offset + len(expected):]:
                return
            time.sleep(0.02)
    raise RuntimeError(f"QEMU did not submit command {text!r}")


def wait_for_serial_text(path: Path, text: str, count: int = 1,
                         timeout: float = 5.0) -> str:
    deadline = time.monotonic() + timeout
    serial = ""
    while time.monotonic() < deadline:
        serial = path.read_text(errors="replace") if path.exists() else ""
        if serial.count(text) >= count:
            return serial
        time.sleep(0.1)
    raise RuntimeError(f"timed out waiting for serial output {text!r}; recent output: {serial[-1000:]}")


def wait_for_shell_prompt(path: Path, count: int, timeout: float = 5.0) -> str:
    serial = wait_for_serial_text(path, "fadal:/", count=count, timeout=timeout)
    time.sleep(0.25)
    return path.read_text(errors="replace") if path.exists() else serial


def run_persistent_home_checks(process: subprocess.Popen[str], serial_log: Path,
                               debug_log: Path,
                               timeout: float) -> None:
    serial = serial_log.read_text(errors="replace")
    prompt = "fadal:/"
    prompt_count = serial.count(prompt)
    if prompt_count == 0:
        raise RuntimeError("persistent-home test started without a shell prompt")

    send_text(process, "ls", serial_log)
    serial = wait_for_serial_text(serial_log, ".profile")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    if not all(name in serial for name in ("README", "motd", ".profile")):
        raise RuntimeError("persistent home did not list the first-boot default files")
    send_text(process, "cd /etc", serial_log)
    wait_for_serial_text(serial_log, "directory changed")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    if "fadal:/etc#" not in serial:
        raise RuntimeError("cd did not update the shell's current directory")
    send_text(process, "pwd", serial_log)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    normalized_serial = serial.replace("\r\n", "\n").replace("\r", "")
    if "/etc\nfadal:/etc#" not in normalized_serial:
        raise RuntimeError("pwd did not report the current /etc directory")
    send_text(process, "cat motd", serial_log)
    wait_for_serial_text(serial_log, "FadalOS: small, inspectable, and user-aware.")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "cd ../home/root", serial_log)
    wait_for_serial_text(serial_log, "directory changed", count=2)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    if "fadal:/home/root#" not in serial:
        raise RuntimeError("relative cd did not normalize back to /home/root")
    send_text(process, "cat readme", serial_log)
    serial = wait_for_serial_text(serial_log, "This is the FadalOS Phase 1 console.")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    if "This is the FadalOS Phase 1 console." not in serial:
        raise RuntimeError("cat did not read the seeded persistent README; guest output: " +
                           serial[-1200:])
    send_text(process, "write persist diskdatalong", serial_log)
    serial = wait_for_serial_text(serial_log, "write: saved in /home/root")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "write persist ok", serial_log)
    serial = wait_for_serial_text(serial_log, "write: saved in /home/root", count=2)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "write scratch removeme", serial_log)
    wait_for_serial_text(serial_log, "write: saved in /home/root", count=3)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "rm scratch", serial_log)
    wait_for_serial_text(serial_log, "rm: removed from /home/root")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "mkdir guarddir", serial_log)
    wait_for_serial_text(serial_log, "mkdir: created in /home/root")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "mkdir guarddir", serial_log)
    wait_for_serial_text(serial_log, "mkdir: already exists")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "cd guarddir", serial_log)
    wait_for_serial_text(serial_log, "directory changed", count=3)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    if "fadal:/home/root/guarddir#" not in serial:
        raise RuntimeError("cd did not enter the persistent home subdirectory")
    send_text(process, "mkdir child", serial_log)
    wait_for_serial_text(serial_log, "mkdir: created in /home/root", count=2)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "cd child", serial_log)
    wait_for_serial_text(serial_log, "directory changed", count=4)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    if "fadal:/home/root/guarddir/child#" not in serial:
        raise RuntimeError("cd did not enter a second-level home directory")
    send_text(process, "pwd", serial_log)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    normalized_serial = serial.replace("\r\n", "\n").replace("\r", "")
    if "/home/root/guarddir/child\nfadal:/home/root/guarddir/child#" not in normalized_serial:
        raise RuntimeError("pwd did not report the nested home directory")
    send_text(process, "write note nestedvalue", serial_log)
    wait_for_serial_text(serial_log, "write: saved in /home/root", count=4)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "cat note", serial_log)
    wait_for_serial_text(serial_log, "nestedvalue", count=2)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "ls", serial_log)
    wait_for_serial_text(serial_log, "note\n")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "cd ..", serial_log)
    wait_for_serial_text(serial_log, "directory changed", count=5)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "ls", serial_log)
    wait_for_serial_text(serial_log, "child/\n")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "cd ..", serial_log)
    wait_for_serial_text(serial_log, "directory changed", count=6)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "rm guarddir", serial_log)
    wait_for_serial_text(serial_log, "rm: cannot remove a directory")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "write guarddir nope", serial_log)
    wait_for_serial_text(serial_log, "write: cannot replace a directory")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "write ../escape nope", serial_log)
    wait_for_serial_text(serial_log, "write: only /home/root is writable")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "rm ../escape", serial_log)
    wait_for_serial_text(serial_log, "rm: only /home/root files can be removed")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)

    send_text(process, "restart", serial_log)
    deadline = time.monotonic() + timeout
    marker = b"FADALOS_CONSOLE_ENTRY"
    while time.monotonic() < deadline:
        if debug_log.exists() and debug_log.read_bytes().count(marker) >= 2:
            break
        if process.poll() is not None:
            raise RuntimeError(f"QEMU exited during persistent-home reboot ({process.returncode})")
        time.sleep(0.2)
    else:
        raise RuntimeError("guest did not boot a second time after the restart command")

    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count, timeout=timeout)
    listing_offset = serial_log.stat().st_size
    send_text(process, "ls", serial_log)
    wait_for_serial_text(serial_log, "persist\n")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    listing_output = serial_log.read_bytes()[listing_offset:]
    if b"persist\n" not in listing_output:
        raise RuntimeError("the user file was not listed after reboot")
    if b"scratch\n" in listing_output:
        raise RuntimeError("the deleted file reappeared after reboot")
    if b"guarddir/" not in listing_output.lower():
        raise RuntimeError("the home subdirectory did not persist after reboot")
    send_text(process, "cat persist", serial_log)
    serial = wait_for_serial_text(serial_log, "ok\n")
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    if "ok\n" not in serial:
        raise RuntimeError("the user file contents did not survive reboot; guest output: " +
                           serial[-1200:])
    send_text(process, "cd guarddir/child", serial_log)
    wait_for_serial_text(serial_log, "directory changed", count=7)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    if "fadal:/home/root/guarddir/child#" not in serial:
        raise RuntimeError("the second-level home directory could not be entered after reboot")
    nested_listing_offset = serial_log.stat().st_size
    send_text(process, "ls", serial_log)
    wait_for_serial_text(serial_log, "note\n", count=2)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    if b"note\n" not in serial_log.read_bytes()[nested_listing_offset:]:
        raise RuntimeError("the nested file was not listed after reboot")
    send_text(process, "cat note", serial_log)
    wait_for_serial_text(serial_log, "nestedvalue", count=3)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "cd ../..", serial_log)
    wait_for_serial_text(serial_log, "directory changed", count=8)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    send_text(process, "rm guarddir/child/note", serial_log)
    wait_for_serial_text(serial_log, "rm: removed from /home/root", count=2)
    prompt_count += 1
    serial = wait_for_shell_prompt(serial_log, prompt_count)
    print("PASS: persistent home supports mkdir, cd, ls, cat, write, and rm on nested paths.")
    print("PASS: /home/root confines writes/deletes and refuses directory removal.")
    print("PASS: deleted home entries stay absent after a full QEMU/OVMF reboot.")
    print("PASS: file contents survive a full QEMU/OVMF guest reboot.")


def ppm_pixels(path: Path) -> bytes:
    image = path.read_bytes()
    header = image.split(b"\n", 3)
    if len(header) != 4 or header[0] != b"P6" or header[2] != b"255":
        raise RuntimeError(f"invalid PPM framebuffer capture: {path}")
    return header[3]


def ppm_region(path: Path, left: int, top: int, right: int, bottom: int) -> bytes:
    image = path.read_bytes()
    header = image.split(b"\n", 3)
    if len(header) != 4 or header[0] != b"P6" or header[2] != b"255":
        raise RuntimeError(f"invalid PPM framebuffer capture: {path}")
    width, height = (int(value) for value in header[1].split())
    if not (0 <= left < right <= width and 0 <= top < bottom <= height):
        raise RuntimeError(f"invalid framebuffer comparison region: {(left, top, right, bottom)}")
    pixels = header[3]
    row_bytes = (right - left) * 3
    rows = []
    for y in range(top, bottom):
        start = (y * width + left) * 3
        rows.append(pixels[start:start + row_bytes])
    return b"".join(rows)


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


def drag_mouse(process: subprocess.Popen[str], current: list[int],
               start_x: int, start_y: int, end_x: int, end_y: int) -> None:
    move_mouse_to(process, current, start_x, start_y)
    send_monitor_command(process, "mouse_button 1", delay=0.2)
    move_mouse_to(process, current, end_x, end_y)
    send_monitor_command(process, "mouse_button 0", delay=0.3)


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

    move_mouse_to(process, mouse, 70, 773)
    send_monitor_command(process, "mouse_button 1", delay=0.2)
    move_mouse_to(process, mouse, 82, 773)
    send_monitor_command(process, "mouse_button 0", delay=0.22)
    capture_frame(process, state)
    assert_pixel(state, 30, 550, (26, 49, 85), "held pointer on Start")
    print("PASS: a held mouse/touch contact does not repeat click actions or toggle the menu.")

    click_mouse(process, mouse, 160, 624)
    capture_frame(process, state)
    assert_pixel(state, 270, 125, (45, 95, 155), "Files app titlebar")
    if bytes((216, 234, 255)) not in ppm_region(state, 284, 250, 1000, 560):
        raise RuntimeError("Files app opened without rendering persistent home entries")
    click_mouse(process, mouse, 350, 261)
    capture_frame(process, state)
    if bytes((126, 214, 167)) not in ppm_region(state, 286, 594, 550, 632):
        raise RuntimeError("Files app did not render a preview after selecting a home file")
    print("PASS: Start > Files lists persistent ESP entries and previews a selected file.")

    click_mouse(process, mouse, 995, 137)
    click_mouse(process, mouse, 70, 773)
    click_mouse(process, mouse, 160, 678)
    capture_frame(process, state)
    assert_pixel(state, 270, 125, (36, 63, 109), "System app titlebar")
    if bytes((216, 234, 255)) not in ppm_region(state, 288, 190, 990, 430):
        raise RuntimeError("System app opened without rendering system information")
    print("PASS: Start > System opens the information panel with UEFI/runtime status.")

    click_mouse(process, mouse, 995, 137)
    click_mouse(process, mouse, 70, 773)
    click_mouse(process, mouse, 160, 728)
    capture_frame(process, state)
    assert_pixel(state, 270, 125, (45, 95, 155), "Browser app titlebar")
    if bytes((126, 214, 167)) not in ppm_region(state, 280, 265, 1000, 295):
        raise RuntimeError("Browser app did not render its address/status controls")
    print("PASS: Start > Browser opens the native HTTP text-viewer panel.")

    idle_status = ppm_region(state, 280, 265, 1000, 295)
    click_mouse(process, mouse, 500, 202)
    send_keys(process, ("h", "t", "t", "p", "s", "ret"))
    capture_frame(process, state)
    if ppm_region(state, 280, 265, 1000, 295) == idle_status:
        raise RuntimeError("Browser URL keyboard editing did not update its visible status")
    print("PASS: Browser accepts keyboard URL input and handles Enter without leaking to the shell.")

    click_mouse(process, mouse, 420, 240)
    capture_frame(process, state)
    if bytes((126, 214, 167)) not in ppm_region(state, 280, 265, 1000, 295):
        raise RuntimeError("Browser download control did not return a visible status")
    click_mouse(process, mouse, 550, 240)
    capture_frame(process, state)
    assert_pixel(state, 270, 125, (45, 95, 155), "Browser Downloads shortcut")
    if bytes((216, 234, 255)) not in ppm_region(state, 284, 185, 900, 215):
        raise RuntimeError("Browser Downloads shortcut did not open the persistent Downloads folder")
    print("PASS: Browser download control reports status and Downloads opens in Files.")


def run_window_resize_checks(process: subprocess.Popen[str], temp: Path,
                              screenshot: Path) -> None:
    mouse = [640, 400]
    state = temp / "window-resize-state.ppm"
    wide = screenshot.with_name("window-reflow-wide.ppm")
    narrow = screenshot.with_name("window-reflow-narrow.ppm")
    workspace = (28, 47, 82)
    terminal = (10, 16, 30)

    send_keys(process, ("h", "e", "l", "p", "ret"))
    time.sleep(0.5)
    serial = (temp / "serial.log").read_text(errors="replace")
    if serial.count("commands: help clear pwd") < 2:
        raise RuntimeError("could not seed a long terminal line for the reflow regression")
    send_keys(process, ("h", "e", "l"))
    move_mouse_to(process, mouse, 1226, 400)
    capture_frame(process, wide)
    wide_region = ppm_region(wide, 292, 154, 1208, 660)
    for narrow_right in (780, 920):
        drag_mouse(process, mouse, 1226, 400, narrow_right, 400)
        capture_frame(process, narrow)
        if ppm_region(narrow, 292, 154, 1208, 660) == wide_region:
            raise RuntimeError("narrowing the terminal did not reflow the long output line")
        drag_mouse(process, mouse, narrow_right, 400, 1226, 400)
        capture_frame(process, state)
        restored_region = ppm_region(state, 292, 154, 1208, 660)
        if restored_region != wide_region:
            mismatch = next(index for index, (actual, expected) in
                            enumerate(zip(restored_region, wide_region)) if actual != expected)
            pixel = mismatch // 3
            location = (292 + pixel % 916, 154 + pixel // 916)
            raise RuntimeError(f"terminal text was not restored after resize at {location}; "
                               f"captures: {wide}, {narrow}, {state}")
    send_keys(process, ("p", "ret"))
    time.sleep(0.5)
    serial = (temp / "serial.log").read_text(errors="replace")
    if serial.count("commands: help clear pwd") < 3:
        raise RuntimeError("the active command editor did not survive repeated resize cycles")
    print("PASS: long output reflows and the active command editor survives repeated resize cycles.")

    drag_mouse(process, mouse, 800, 686, 800, 358)
    for _ in range(5):
        send_keys(process, ("l", "s", "ret"))
    send_monitor_command(process, "sendkey pgup", delay=0.45)
    scroll_wide = screenshot.with_name("window-reflow-scrollback-wide.ppm")
    scroll_narrow = screenshot.with_name("window-reflow-scrollback-narrow.ppm")
    capture_frame(process, scroll_wide)
    scroll_region = ppm_region(scroll_wide, 292, 154, 1208, 340)
    drag_mouse(process, mouse, 1226, 300, 780, 300)
    capture_frame(process, scroll_narrow)
    drag_mouse(process, mouse, 780, 300, 1226, 300)
    capture_frame(process, state)
    if ppm_region(state, 292, 154, 1208, 340) != scroll_region:
        raise RuntimeError("the visible scrollback anchor changed across repeated resize")
    send_monitor_command(process, "sendkey pgdn", delay=0.45)
    capture_frame(process, state)
    if ppm_region(state, 292, 154, 1208, 340) == scroll_region:
        raise RuntimeError("PageDown did not return from the resized scrollback view")
    drag_mouse(process, mouse, 800, 356, 800, 684)
    send_keys(process, ("c", "l", "e", "a", "r", "ret"))
    time.sleep(0.5)
    print("PASS: PageUp history remains anchored through a width reflow and PageDown returns to live output.")

    drag_mouse(process, mouse, 1226, 400, 1096, 400)
    capture_frame(process, state)
    assert_pixel(state, 1120, 400, workspace, "right-edge resize inward")
    drag_mouse(process, mouse, 1096, 400, 1226, 400)
    capture_frame(process, state)
    assert_pixel(state, 1200, 500, terminal, "right-edge resize outward")
    print("PASS: the right edge resizes inward and outward.")

    drag_mouse(process, mouse, 800, 686, 800, 566)
    capture_frame(process, state)
    assert_pixel(state, 800, 580, workspace, "bottom-edge resize inward")
    drag_mouse(process, mouse, 800, 566, 800, 686)
    capture_frame(process, state)
    assert_pixel(state, 800, 650, terminal, "bottom-edge resize outward")
    print("PASS: the bottom edge resizes inward and outward.")

    drag_mouse(process, mouse, 271, 400, 351, 400)
    capture_frame(process, state)
    assert_pixel(state, 300, 400, workspace, "left-edge resize")
    assert_pixel(state, 400, 500, terminal, "left-edge content")
    drag_mouse(process, mouse, 351, 400, 271, 400)
    capture_frame(process, state)
    assert_pixel(state, 300, 400, terminal, "left-edge restore")
    print("PASS: the left edge resizes while keeping the opposite edge anchored.")

    drag_mouse(process, mouse, 1226, 686, 1086, 586)
    capture_frame(process, state)
    assert_pixel(state, 1100, 500, workspace, "bottom-right corner resize")
    assert_pixel(state, 1000, 610, workspace, "bottom-right height resize")
    print("PASS: the bottom-right corner resizes width and height together.")

    drag_mouse(process, mouse, 1086, 586, 0, 0)
    capture_frame(process, state)
    assert_pixel(state, 700, 200, workspace, "minimum-width clamp")
    assert_pixel(state, 600, 200, terminal, "minimum-width interior")
    assert_pixel(state, 600, 370, workspace, "minimum-height clamp")
    drag_mouse(process, mouse, 688, 356, 1226, 686)
    capture_frame(process, screenshot)
    assert_pixel(screenshot, 1200, 500, terminal, "restore after minimum-size clamp")
    print("PASS: minimum width/height are enforced and the original bounds can be restored.")

    drag_mouse(process, mouse, 800, 99, 800, 169)
    capture_frame(process, screenshot)
    assert_pixel(screenshot, 800, 130, workspace, "top-edge resize")
    assert_pixel(screenshot, 800, 650, terminal, "top-edge keeps the bottom anchored")
    print("PASS: the top edge resizes while keeping the bottom edge anchored.")


def wait_for_titlebar_color(process: subprocess.Popen[str], screenshot: Path,
                            expected: bytes, timeout: float = 3.0) -> None:
    deadline = time.monotonic() + timeout
    actual = b""
    while time.monotonic() < deadline:
        send_monitor_command(process, f"screendump {screenshot}", delay=0.12)
        pixels = ppm_pixels(screenshot)
        width = int(screenshot.read_bytes().split(b"\n", 3)[1].split()[0])
        offset = (100 * width + 500) * 3
        actual = pixels[offset:offset + 3]
        if actual == expected:
            return
    raise RuntimeError(f"terminal title-bar color {tuple(actual)} did not reach {tuple(expected)}")


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
    parser.add_argument("--window-resize", action="store_true",
                        help="also test terminal edge/corner resizing and minimum bounds")
    parser.add_argument("--persistent-home", action="store_true",
                        help="test ESP-backed /home/root files across a guest reboot")
    parser.add_argument("--window-screenshot", type=Path,
                        default=root / "kernel/uefi/out/window-controls.ppm")
    parser.add_argument("--resize-screenshot", type=Path,
                        default=root / "kernel/uefi/out/window-resize.ppm")
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
            "-no-shutdown",
        ]
        if not args.persistent_home:
            command.append("-no-reboot")
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

            wait_for_serial_text(serial_log, "browser: HTML-to-text self-test passed",
                                 timeout=args.timeout)
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
            serial = wait_for_serial_text(serial_log, "commands: help clear pwd",
                                          timeout=args.timeout)
            if "commands: help clear pwd" not in serial:
                raise RuntimeError("terminal did not receive and execute input after focus returned")

            print("PASS: Tab moved focus to the desktop and terminal input was suppressed.")
            print("PASS: Tab restored terminal focus and the help command executed.")
            if args.persistent_home:
                run_persistent_home_checks(process, serial_log, debug_log, args.timeout)
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
                    raise RuntimeError("Home/Right/End/Delete editing did not execute the reconstructed help command; "
                                       "recent output: " + serial[-1200:])

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
            if args.window_resize:
                args.resize_screenshot.parent.mkdir(parents=True, exist_ok=True)
                args.resize_screenshot.unlink(missing_ok=True)
                run_window_resize_checks(process, temp, args.resize_screenshot)
                print(f"Window-resize capture: {args.resize_screenshot.resolve()}")
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
