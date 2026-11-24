#!/usr/bin/env python3

import base64
import subprocess
import sys
import time


def find_cdc_device():
    try:
        import serial.tools.list_ports

        return any(
            port.vid == 0x0483 and port.pid == 0x5740
            for port in serial.tools.list_ports.comports()
        )
    except Exception:
        return False


def main():
    if len(sys.argv) != 2:
        return 2

    try:
        command = base64.b64decode(sys.argv[1]).decode("utf-8")
    except (ValueError, UnicodeDecodeError):
        return 2

    process = subprocess.Popen(
        command,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="replace",
    )

    output_lines = []
    assert process.stdout is not None
    for line in process.stdout:
        print(line, end="")
        output_lines.append(line)

    status = process.wait()

    if status != 74:
        return status

    output = "".join(output_lines)
    download_completed = (
        "Download done." in output
        or "File downloaded successfully" in output
    )
    if not download_completed:
        return status

    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if find_cdc_device():
            return 0
        time.sleep(0.1)

    return status


if __name__ == "__main__":
    raise SystemExit(main())
