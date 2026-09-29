"""Independent specification fixtures; stdlib only, never required at app runtime.
Layout is taken from core/src/zeptocore/zeptocorebinary.go and lib/sampleinfo.h.
"""
import pathlib
import struct
root = pathlib.Path(__file__).parent / 'fixtures'
root.mkdir(exist_ok=True)
for channels in (1, 2):
    for rate in (44100, 88200):
        size = rate * channels * 2
        flags = 172 | (3 << 9) | (1 << 13) | ((rate == 88200) << 14) | ((channels - 1) << 15) | (1 << 16)
        blob = struct.pack('<IIHB', size, flags, 96, 2)
        blob += struct.pack('<4i2b', 0, size // 2 // 4 * 4, size // 2 // 4 * 4, size // 4 * 4, 1, 0)
        blob += struct.pack('<3H3H', 1, 1, 1, 100, 200, 300)
        (root / f'website-{channels}-{rate}.info').write_bytes(blob)
