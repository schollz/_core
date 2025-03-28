import math

# https://www.wolframalpha.com/input?i=%5B%281-Sin%284*pi*%28x%2F4%29%2Bpi%2F2%2Bpi%29%29%2F2+%2C%281-Sin%284*pi*%28x%2F4%29%2Bpi%2F2%29%29%2F2%5D+from+0+to+1
# [(1-Sin(4*pi*(x/4)+pi/2+pi))/2 ,(1-Sin(4*pi*(x/4)+pi/2))/2] from 0 to 1

samples_per_block = 32
blocks = 15

print("#ifndef CROSSFADE_LIB")
print(f"static uint crossfade_raw[101][{blocks}] = {{")
for _, scaler in enumerate(range(101)):
    print("{", end="")
    for v in range(blocks):
        x = float(v) / blocks
        x = round(
            float(scaler)
            * ((1 - math.sin(4 * math.pi * (x / 4) + math.pi / 2 + math.pi)) / 2)
        )
        print("{},".format(x), end="")
    print("},\n", end="")
print("};\n")
print(f"static uint32_t CROSSFADE_UPDATE_SAMPLES = {samples_per_block};\n")
print(f"static uint32_t CROSSFADE_MAX = {blocks * samples_per_block};\n")

print(
    """uint crossfade_vol(uint current_vol, uint32_t phase_since) {
    if (current_vol==0 || phase_since >= CROSSFADE_MAX) {
        return 0;
    }
    """
)
print(f"phase_since = phase_since / {samples_per_block};")
print("return crossfade_raw[current_vol][phase_since];")
print("\n}")
print(
    """
#define CROSSFADE_LIB 1
#endif
"""
)
