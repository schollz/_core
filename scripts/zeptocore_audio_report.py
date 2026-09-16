#!/usr/bin/env python3
"""Report recorded signal levels and candidate gaps; silence is not proof of an underrun."""
import argparse
import array
import json
import math
from pathlib import Path
import sys
import wave


def report(path):
    with wave.open(str(path), "rb") as source:
        if source.getsampwidth() != 4 or source.getnchannels() != 2:
            raise ValueError("expected stereo signed 32-bit PCM WAV")
        rate, count = source.getframerate(), source.getnframes()
        samples = array.array("i", source.readframes(count))
        if sys.byteorder != "little":
            samples.byteswap()
    channels = []
    for channel in range(2):
        values = samples[channel::2]
        energy, peak, clipped, quiet, longest = 0.0, 0, 0, 0, 0
        # -90 dBFS identifies near-zero runs, which can also exist in the source.
        threshold = int(2147483648 * 10**(-90/20))
        for sample in values:
            absolute = abs(sample)
            peak = max(peak, absolute)
            energy += (sample / 2147483648)**2
            clipped += absolute >= 0x7FFF0000
            quiet = quiet + 1 if absolute <= threshold else 0
            longest = max(longest, quiet)
        rms = math.sqrt(energy / max(1, len(values)))
        channels.append({"channel": channel+1,
                         "peak_dbfs": 20*math.log10(max(peak/2147483648, 1e-15)),
                         "rms_dbfs": 20*math.log10(max(rms, 1e-15)),
                         "clipped_samples": clipped,
                         "longest_near_zero_ms": 1000*longest/rate})
    return {"file": str(path), "rate": rate, "frames": count, "seconds": count/rate,
            "channels": channels, "interpretation": "Candidate gaps require comparison with source and DMA counters."}


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("wav", type=Path, nargs="+")
    args = p.parse_args()
    print(json.dumps([report(path) for path in args.wav], indent=2))
