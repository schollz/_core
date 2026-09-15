"""Wait for ALSA to finish USB MIDI enumeration after a firmware reset."""
from pathlib import Path
import subprocess
import time


def wait_available(port, timeout=10):
    deadline = time.monotonic()+timeout
    while True:
        listing = subprocess.run(["amidi", "-l"], capture_output=True, text=True, check=True).stdout
        candidate = port
        parts = port.removeprefix("hw:").split(",")
        if len(parts) == 3 and not parts[0].isdigit():
            for identity in Path("/proc/asound").glob("card[0-9]*/id"):
                try:
                    if identity.read_text().strip() == parts[0]:
                        candidate = f"hw:{identity.parent.name[4:]},{parts[1]},{parts[2]}"
                except FileNotFoundError:
                    pass  # device disconnected during enumeration
        if any(len(fields := line.split()) > 1 and fields[1] == candidate
               for line in listing.splitlines()):
            return
        if time.monotonic()>deadline:
            raise RuntimeError(f"MIDI port {port} did not enumerate: {listing.strip()}")
        time.sleep(.1)
