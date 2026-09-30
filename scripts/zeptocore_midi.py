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


def management_frame(operation):
    return bytes([0xf0]) + ('core_cmd=1,' + operation).encode('ascii') + bytes([0xf7])


def negotiate_management(send, receive, now=time.monotonic):
    """Read-only discovery; receive(timeout) returns complete MIDI frames."""
    started = now()
    send(management_frame('hello'))
    stage = 0
    legacy = False
    while now() - started < 2:
        elapsed = now() - started
        if stage == 0 and elapsed >= .5:
            stage = 1
            send(management_frame('hello'))
        if stage == 1 and elapsed >= 1:
            stage = 2
            send(bytes([0xb0, 1, 0]))
        for frame in receive(.05):
            if frame == b'\xf0core_caps=1\xf7':
                return 'sysex'
            if stage >= 2 and frame.startswith(b'\xf0version=') and frame.endswith(b'\xf7') and len(frame) > 10:
                legacy = True
        # Process all available replies first so modern acknowledgment wins.
        if legacy:
            return 'legacy'
    raise RuntimeError('Device management unavailable: no protocol response')


def reset_management(port):
    """Negotiate using amidi's input dump; send only the confirmed reset form."""
    import os
    import re
    import select

    def send(data):
        subprocess.run(['amidi', '-p', port, '-S', data.hex()], check=True,
                       capture_output=True, timeout=3)

    listener = subprocess.Popen(['amidi', '-p', port, '-d'], stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, bufsize=0)
    pending = b''
    frame = bytearray()
    def receive(timeout):
        nonlocal pending, frame
        if listener.poll() is not None:
            raise RuntimeError('Could not open MIDI input for protocol detection')
        if not select.select([listener.stdout], [], [], timeout)[0]:
            return []
        pending += os.read(listener.stdout.fileno(), 4096)
        # Dump bytes are whitespace-separated hex. Retain an unfinished token.
        tokens = re.split(rb'\s+', pending)
        pending = tokens.pop()
        frames = []
        for token in tokens:
            if not re.fullmatch(rb'[0-9a-fA-F]{2}', token):
                continue
            byte = int(token, 16)
            if byte >= 0xf8:
                continue
            if byte == 0xf0:
                frame = bytearray([byte])
            elif frame:
                frame.append(byte)
                if byte == 0xf7:
                    frames.append(bytes(frame)); frame.clear()
                elif byte >= 0x80 or len(frame) > 128:
                    frame.clear()
        return frames
    try:
        # Allow the read endpoint to open before sending the first probe.
        time.sleep(.05)
        protocol = negotiate_management(send, receive)
        send(management_frame('bootloader') if protocol == 'sysex' else bytes([0xb0, 0, 0]))
    finally:
        listener.terminate()
        try:
            listener.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            listener.kill(); listener.communicate()
