#!/usr/bin/env python3
"""Exercise UEFI keyboard focus and optional terminal input under QEMU/OVMF."""

import argparse
import os
from pathlib import Path
import secrets
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


def send_keys(process: subprocess.Popen[str], keys: tuple[str, ...],
              delay: float = 0.18) -> None:
    for key in keys:
        send_monitor_command(process, f"sendkey {key}", delay=delay)


def send_text(process: subprocess.Popen[str], text: str, serial_log: Path,
              submit: bool = True, delay: float = 0.45) -> None:
    key_names = {" ": "spc", ".": "dot", "/": "slash", "-": "minus"}
    offset = serial_log.stat().st_size if serial_log.exists() else 0
    expected = bytearray()
    for character in text:
        expected.extend(character.encode("ascii"))
        for attempt in range(3):
            send_monitor_command(process,
                                 f"sendkey {key_names.get(character, character)}",
                                 delay=delay)
            deadline = time.monotonic() + 1.5
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
    if not submit:
        return
    for attempt in range(3):
        send_monitor_command(process, "sendkey ret", delay=delay)
        deadline = time.monotonic() + 1.5
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


def run_terminal_selection_checks(process: subprocess.Popen[str], serial_log: Path,
                                  screenshot: Path, timeout: float) -> None:
    serial = serial_log.read_text(errors="replace")
    prompt_count = serial.count("fadal:/")
    help_count = serial.count("commands: help clear pwd")
    send_text(process, "clear", serial_log)
    prompt_count += 1
    wait_for_shell_prompt(serial_log, prompt_count, timeout)
    send_text(process, "help", serial_log)
    wait_for_serial_text(serial_log, "commands: help clear pwd", help_count + 1, timeout)
    help_count += 1
    prompt_count += 1
    wait_for_shell_prompt(serial_log, prompt_count, timeout)

    mouse = [640, 400]
    # In the cleared 1280x800 desktop, row 1 contains "commands: help ...".
    drag_mouse(process, mouse, 355, 167, 373, 167)
    capture_frame(process, screenshot)
    selected_color = (61, 90, 128)
    assert_pixel(screenshot, 357, 163, selected_color, "selected terminal text")
    send_monitor_command(process, "sendkey ctrl-c", delay=0.4)

    focus_capture = screenshot.with_name("terminal-selection-focus-clear.ppm")
    click_mouse(process, mouse, 80, 300)
    capture_frame(process, focus_capture)
    if ppm_pixel(focus_capture, 357, 163) == selected_color:
        raise RuntimeError("terminal selection highlight remained after focus left the terminal")
    send_monitor_command(process, "sendkey tab", delay=0.45)

    drag_mouse(process, mouse, 355, 167, 373, 167)
    drag_mouse(process, mouse, 1226, 400, 1180, 400)
    drag_mouse(process, mouse, 1180, 400, 1226, 400)
    resize_capture = screenshot.with_name("terminal-selection-resize-clear.ppm")
    capture_frame(process, resize_capture)
    if ppm_pixel(resize_capture, 357, 163) == selected_color:
        raise RuntimeError("terminal selection highlight remained after resize")

    paste_offset = serial_log.stat().st_size if serial_log.exists() else 0
    send_monitor_command(process, "sendkey ctrl-v", delay=0.45)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        recent = serial_log.read_bytes()[paste_offset:] if serial_log.exists() else b""
        if recent.endswith(b"help"):
            break
        time.sleep(0.05)
    else:
        raise RuntimeError("Ctrl+V did not paste the copied word from FadalOS's guest buffer")
    send_monitor_command(process, "sendkey ret", delay=0.45)
    wait_for_serial_text(serial_log, "commands: help clear pwd", help_count + 1, timeout)
    wait_for_shell_prompt(serial_log, prompt_count + 1, timeout)
    print("PASS: mouse drag selects terminal text and paints the selection highlight.")
    print("PASS: Ctrl+C/Ctrl+V use the bounded guest copy buffer; focus loss and resize clear the highlight.")
    print(f"Text-selection capture: {screenshot.resolve()}")


def capture_frame(process: subprocess.Popen[str], path: Path) -> None:
    send_monitor_command(process, f"screendump {path}", delay=0.3)
    if not path.is_file() or path.stat().st_size < 1024:
        raise RuntimeError(f"QEMU did not produce framebuffer capture: {path}")


def prepare_files_scroll_fixture(image: Path, temp: Path) -> None:
    required = ("mdir", "mmd", "mcopy")
    missing = [tool for tool in required if shutil.which(tool) is None]
    if missing:
        raise RuntimeError("Files scrolling regression requires mtools: " + ", ".join(missing))
    mtools_image = f"{image}@@1048576"
    listing = subprocess.run(
        ["mdir", "-i", mtools_image, "::/FADALHOME"],
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True,
    )
    if listing.returncode != 0:
        subprocess.run(
            ["mmd", "-i", mtools_image, "::/FADALHOME"],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True,
        )

    fixture_dir = temp / "files-scroll-fixtures"
    fixture_dir.mkdir()
    prefix = "Z" + secrets.token_hex(2).upper()
    fixtures = []
    for index in range(20):
        fixture = fixture_dir / f"{prefix}{index:02d}.TXT"
        fixture.write_text(f"SCROLL TEST ITEM {index:02d}\nSECOND LINE\n")
        fixtures.append(str(fixture))
    subprocess.run(
        ["mcopy", "-o", "-i", mtools_image, *fixtures, "::/FADALHOME/"],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True,
    )


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
    files_root_location = ppm_region(state, 284, 190, 900, 215)
    send_monitor_command(process, "sendkey down")
    capture_frame(process, state)
    if ppm_pixel(state, 500, 280) != (36, 63, 109):
        raise RuntimeError("Files app Down key did not move its visible selection")
    send_monitor_command(process, "sendkey ret")
    capture_frame(process, state)
    if bytes((126, 214, 167)) not in ppm_region(state, 286, 594, 550, 632):
        raise RuntimeError("Files app Enter did not preview the selected home file")
    print("PASS: Start > Files lists ESP entries and keyboard selection previews a file.")

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
    send_keys(process, ("h", "t", "t", "p", "s", "ret"), delay=0.35)
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
    send_monitor_command(process, "sendkey backspace")
    capture_frame(process, state)
    if ppm_region(state, 284, 190, 900, 215) != files_root_location:
        raise RuntimeError("Files Backspace did not return from Downloads to /home/root")
    files_first_page = ppm_region(state, 276, 260, 1004, 577)
    for _ in range(15):
        send_monitor_command(process, "sendkey down", delay=0.25)
    capture_frame(process, state)
    if ppm_region(state, 276, 260, 1004, 577) == files_first_page:
        raise RuntimeError("Files Down did not scroll the list past the first page")
    assert_pixel(state, 500, 566, (36, 63, 109), "Files Down selection after crossing the first page")
    for _ in range(15):
        send_monitor_command(process, "sendkey up", delay=0.25)
    capture_frame(process, state)
    if ppm_region(state, 276, 260, 1004, 577) != files_first_page:
        raise RuntimeError("Files Up did not scroll back to the first page")
    print("PASS: repeated Down/Up scroll across the 15-row boundary in both directions.")
    send_monitor_command(process, "sendkey end", delay=0.35)
    capture_frame(process, state)
    if ppm_region(state, 276, 260, 1004, 577) == files_first_page:
        raise RuntimeError("Files End did not scroll the entry list past the first 15 rows")
    assert_pixel(state, 500, 566, (36, 63, 109), "Files End selection on the last visible row")
    send_monitor_command(process, "sendkey up")
    capture_frame(process, state)
    assert_pixel(state, 500, 544, (36, 63, 109), "Files Up selection near the list tail")
    send_monitor_command(process, "sendkey ret")
    capture_frame(process, state)
    if bytes((126, 214, 167)) not in ppm_region(state, 286, 594, 550, 632):
        raise RuntimeError("Files Enter did not open the selected entry after scrolling")
    send_monitor_command(process, "sendkey home")
    capture_frame(process, state)
    if ppm_region(state, 276, 260, 1004, 577) != files_first_page:
        raise RuntimeError("Files Home did not restore the first page of entries")
    print("PASS: Files scrolls beyond 15 entries; End/Up reach the tail and Home restores the first page.")
    print("PASS: Browser Downloads opens in Files and Backspace returns to /home/root.")


def run_window_resize_checks(process: subprocess.Popen[str], temp: Path,
                              screenshot: Path) -> None:
    mouse = [640, 400]
    state = temp / "window-resize-state.ppm"
    serial_log = temp / "serial.log"
    wide = screenshot.with_name("window-reflow-wide.ppm")
    narrow = screenshot.with_name("window-reflow-narrow.ppm")
    workspace = (28, 47, 82)
    terminal = (10, 16, 30)

    send_text(process, "help", serial_log)
    serial = wait_for_serial_text(serial_log, "commands: help clear pwd", count=2)
    if serial.count("commands: help clear pwd") < 2:
        raise RuntimeError("could not seed a long terminal line for the reflow regression")
    send_text(process, "hel", serial_log, submit=False)
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
    send_text(process, "p", serial_log)
    serial = wait_for_serial_text(serial_log, "commands: help clear pwd", count=3)
    if serial.count("commands: help clear pwd") < 3:
        raise RuntimeError("the active command editor did not survive repeated resize cycles")
    print("PASS: long output reflows and the active command editor survives repeated resize cycles.")

    send_text(process, "x" * 45, serial_log, submit=False, delay=0.18)
    editor_wide = screenshot.with_name("window-reflow-editor-wide.ppm")
    editor_narrow = screenshot.with_name("window-reflow-editor-narrow.ppm")
    editor_restored = screenshot.with_name("window-reflow-editor-restored.ppm")
    move_mouse_to(process, mouse, 60, 80)
    capture_frame(process, editor_wide)
    editor_region = ppm_region(editor_wide, 292, 154, 1208, 660)
    drag_mouse(process, mouse, 1226, 400, 640, 400)
    move_mouse_to(process, mouse, 60, 80)
    capture_frame(process, editor_narrow)
    # The payload clamps the right edge to x=690 (window left 270 + 420 minimum).
    drag_mouse(process, mouse, 688, 400, 1226, 400)
    move_mouse_to(process, mouse, 60, 80)
    capture_frame(process, editor_restored)
    restored_editor_region = ppm_region(editor_restored, 292, 154, 1208, 660)
    if restored_editor_region != editor_region:
        mismatch = next(index for index, (actual, expected) in
                        enumerate(zip(restored_editor_region, editor_region))
                        if actual != expected)
        pixel = mismatch // 3
        location = (292 + pixel % 916, 154 + pixel // 916)
        raise RuntimeError("long active command was lost or changed after narrow/wide resize; "
                           f"first difference at {location}; captures: {editor_wide}, "
                           f"{editor_narrow}, {editor_restored}")
    serial = serial_log.read_text(errors="replace")
    missing_command_count = serial.count("command not found")
    prompt_count = serial.count("fadal:/")
    send_text(process, "", serial_log)
    wait_for_serial_text(serial_log, "command not found", count=missing_command_count + 1)
    wait_for_shell_prompt(serial_log, prompt_count + 1)
    print("PASS: the full active command is preserved and horizontally scrolled across minimum-width resize.")

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
    parser.add_argument("--terminal-selection", action="store_true",
                        help="test mouse selection and the in-guest copy/paste buffer")
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
    parser.add_argument("--selection-screenshot", type=Path,
                        default=root / "kernel/uefi/out/terminal-selection.ppm")
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
    if args.terminal_selection:
        args.selection_screenshot.parent.mkdir(parents=True, exist_ok=True)
        args.selection_screenshot.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix="fadal-focus-") as temporary:
        temp = Path(temporary)
        test_image = temp / "fadal-uefi-test.img"
        shutil.copyfile(args.image, test_image)
        if args.window_controls:
            prepare_files_scroll_fixture(test_image, temp)
        serial_log = temp / "serial.log"
        debug_log = temp / "debugcon.log"
        vars_copy = temp / "OVMF_VARS.fd"
        shutil.copyfile(args.ovmf_vars, vars_copy)
        command = [
            args.qemu, "-machine", "q35", "-m", "512M",
            "-drive", f"if=pflash,format=raw,readonly=on,file={args.ovmf_code}",
            "-drive", f"if=pflash,format=raw,file={vars_copy}",
            "-drive", f"format=raw,file={test_image}",
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
            send_monitor_command(process, "sendkey tab", delay=0.4)
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

            send_monitor_command(process, "sendkey tab", delay=0.4)
            wait_for_titlebar_color(process, temp / "focus-state.ppm", bytes((45, 95, 155)))
            send_text(process, "help", serial_log)
            serial = wait_for_serial_text(serial_log, "commands: help clear pwd",
                                          timeout=args.timeout)
            if "commands: help clear pwd" not in serial:
                raise RuntimeError("terminal did not receive and execute input after focus returned")

            print("PASS: Tab moved focus to the desktop and terminal input was suppressed.")
            print("PASS: Tab restored terminal focus and the help command executed.")
            if args.persistent_home:
                run_persistent_home_checks(process, serial_log, debug_log, args.timeout)
            if args.terminal_input:
                help_count = serial_log.read_text(errors="replace").count("commands: help clear pwd")
                prompt_count = serial_log.read_text(errors="replace").count("fadal:/")
                send_keys(process, ("h", "e", "p", "left", "l", "ret"), delay=0.45)
                serial = wait_for_serial_text(serial_log, "commands: help clear pwd",
                                              count=help_count + 1, timeout=args.timeout)
                help_count += 1
                prompt_count += 1
                serial = wait_for_shell_prompt(serial_log, prompt_count, args.timeout)

                send_keys(process, ("h", "e", "p", "l", "home", "right", "right",
                                    "delete", "end", "p", "ret"), delay=0.45)
                serial = wait_for_serial_text(serial_log, "commands: help clear pwd",
                                              count=help_count + 1, timeout=args.timeout)
                help_count += 1
                prompt_count += 1
                serial = wait_for_shell_prompt(serial_log, prompt_count, args.timeout)

                send_keys(process, ("h", "e", "l", "p", "left", "backspace", "l", "ret"),
                          delay=0.45)
                serial = wait_for_serial_text(serial_log, "commands: help clear pwd",
                                              count=help_count + 1, timeout=args.timeout)
                help_count += 1
                prompt_count += 1
                wait_for_shell_prompt(serial_log, prompt_count, args.timeout)

                for _ in range(30):
                    send_text(process, "ls", serial_log, delay=0.18)
                    prompt_count += 1
                    wait_for_shell_prompt(serial_log, prompt_count, args.timeout)
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
            if args.terminal_selection:
                args.selection_screenshot.parent.mkdir(parents=True, exist_ok=True)
                args.selection_screenshot.unlink(missing_ok=True)
                run_terminal_selection_checks(process, serial_log,
                                              args.selection_screenshot, args.timeout)
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
