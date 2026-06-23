#!/usr/bin/env python3
import argparse
import os
import re
import subprocess
import sys
import termios
import time
from pathlib import Path


MATCH_WORDS = (
    "raspberry",
    "rp2040",
    "rp2350",
    "pico",
    "zeptocore",
    "zeptoboard",
    "ectocore",
)
SKIP_WORDS = (
    "debug probe",
    "cmsis-dap",
)
SKIP_USB_IDS = {
    ("2e8a", "000c"),  # Raspberry Pi Debug Probe
}


def usb_info_for_tty(tty_name):
    device = Path("/sys/class/tty") / tty_name / "device"
    if not device.exists():
        return {}

    current = device.resolve()
    for path in (current, *current.parents):
        vendor_path = path / "idVendor"
        if vendor_path.exists():
            return {
                "vendor": read_text(vendor_path).lower(),
                "product_id": read_text(path / "idProduct").lower(),
                "manufacturer": read_text(path / "manufacturer"),
                "product": read_text(path / "product"),
                "serial": read_text(path / "serial"),
            }
    return {}


def read_text(path):
    try:
        return Path(path).read_text(encoding="utf-8", errors="replace").strip()
    except OSError:
        return ""


def serial_by_id_devices():
    by_id = Path("/dev/serial/by-id")
    if not by_id.exists():
        return []

    devices = []
    for link in sorted(by_id.iterdir()):
        try:
            target = link.resolve()
        except OSError:
            continue
        if target.name.startswith(("ttyACM", "ttyUSB")):
            devices.append((str(target), link.name))
    return devices


def sysfs_tty_devices():
    devices = []
    for pattern in ("ttyACM*", "ttyUSB*"):
        devices.extend(sorted(Path("/sys/class/tty").glob(pattern)))
    return [(f"/dev/{path.name}", "") for path in devices]


def is_candidate(device, alias):
    info = usb_info_for_tty(Path(device).name)
    text = " ".join(
        [
            alias,
            info.get("manufacturer", ""),
            info.get("product", ""),
            info.get("serial", ""),
            info.get("vendor", ""),
            info.get("product_id", ""),
        ]
    ).lower()

    usb_id = (info.get("vendor", "").lower(), info.get("product_id", "").lower())
    if usb_id in SKIP_USB_IDS or any(word in text for word in SKIP_WORDS):
        return False
    return info.get("vendor", "").lower() == "2e8a" or any(
        word in text for word in MATCH_WORDS
    )


def find_port(explicit_port=None):
    if explicit_port:
        return explicit_port

    seen = set()
    for device, alias in serial_by_id_devices() + sysfs_tty_devices():
        if device in seen:
            continue
        seen.add(device)
        if Path(device).exists() and is_candidate(device, alias):
            return device
    return None


def find_midi_ports():
    try:
        result = subprocess.run(
            ["amidi", "-l"], check=False, text=True, capture_output=True
        )
    except FileNotFoundError:
        return []

    ports = []
    for line in result.stdout.splitlines():
        if not re.search(r"zeptocore|zeptoboard|ectocore", line, re.IGNORECASE):
            continue
        match = re.search(r"\b(hw:[^ ]+)", line)
        if match:
            ports.append(match.group(1))
    return ports


def reset_via_midi(wait, dry_run=False):
    ports = find_midi_ports()
    if not ports:
        print("No Zeptocore MIDI device found for BOOTSEL reset fallback; continuing")
        return False

    port = ports[0]
    print(f"{'Would send' if dry_run else 'Sending'} MIDI reset command to {port} to request BOOTSEL")
    if dry_run:
        return True

    for _ in range(3):
        subprocess.run(["amidi", "-p", port, "-S", "B00000"], check=False)
        time.sleep(0.1)
    time.sleep(wait)
    return True


def touch_1200(port, baud):
    baud_const_name = f"B{baud}"
    if not hasattr(termios, baud_const_name):
        raise ValueError(f"unsupported baud rate: {baud}")

    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        attrs = termios.tcgetattr(fd)
        attrs[2] |= termios.HUPCL
        baud_const = getattr(termios, baud_const_name)
        attrs[4] = baud_const
        attrs[5] = baud_const
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        time.sleep(0.1)
    finally:
        os.close(fd)


def main():
    parser = argparse.ArgumentParser(
        description="Touch an RP-series USB serial device at 1200 baud to request BOOTSEL."
    )
    parser.add_argument("--baud", type=int, default=1200)
    parser.add_argument("--wait", type=float, default=2.0)
    parser.add_argument("--port", default=os.environ.get("UPLOAD_TOUCH_PORT", ""))
    parser.add_argument("--midi-fallback", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    port = find_port(args.port or None)
    if not port:
        print("No RP serial device found for 1200-baud BOOTSEL touch")
        if args.midi_fallback:
            reset_via_midi(args.wait, args.dry_run)
        else:
            print("MIDI reset fallback disabled; continuing")
        return 0

    print(f"Touching {port} at {args.baud} baud to request BOOTSEL")
    if args.dry_run:
        return 0

    try:
        touch_1200(port, args.baud)
    except OSError as exc:
        print(f"Could not touch {port}: {exc}; continuing", file=sys.stderr)
        return 0

    time.sleep(args.wait)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
