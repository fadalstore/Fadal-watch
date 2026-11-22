#!/usr/bin/env python3
"""Single-request HTTP fixture for the isolated UEFI network regression."""

import os
import sys

BODY = b"FADAL_UEFI_HTTP_FIXTURE_OK\n"
REQUEST_LIMIT = 2048
HEADER_LIMIT = 8192


def log_event(event: bytes) -> None:
    log_path = os.environ.get("FADAL_UEFI_HTTP_FIXTURE_LOG")
    if log_path:
        with open(log_path, "ab") as log_file:
            log_file.write(event + b"\n")


def main() -> int:
    log_event(b"fixture-started")
    request_line = sys.stdin.buffer.readline(REQUEST_LIMIT + 1)
    log_event(b"request-line:" + request_line.rstrip(b"\r\n"))
    if len(request_line) > REQUEST_LIMIT or not request_line.endswith(b"\r\n"):
        return 1

    parts = request_line[:-2].split(b" ")
    valid_request = len(parts) == 3 and parts[0] == b"GET" and parts[1] == b"/fixture.txt"
    consumed = len(request_line)
    while consumed <= HEADER_LIMIT:
        line = sys.stdin.buffer.readline(REQUEST_LIMIT + 1)
        if not line or len(line) > REQUEST_LIMIT:
            return 1
        consumed += len(line)
        if line == b"\r\n":
            break
    else:
        return 1

    status = b"200 OK" if valid_request else b"404 Not Found"
    body = BODY if valid_request else b"fixture path not found\n"
    response = (
        b"HTTP/1.1 " + status + b"\r\n"
        b"Content-Type: text/plain\r\n"
        b"Content-Length: " + str(len(body)).encode("ascii") + b"\r\n"
        b"Connection: close\r\n\r\n" + body
    )
    sys.stdout.buffer.write(response)
    sys.stdout.buffer.flush()
    log_event(b"response-sent:" + status)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
