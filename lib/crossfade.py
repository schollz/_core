import math

# https://www.wolframalpha.com/input?i=%5B%281-Sin%284*pi*%28x%2F4%29%2Bpi%2F2%2Bpi%29%29%2F2+%2C%281-Sin%284*pi*%28x%2F4%29%2Bpi%2F2%29%29%2F2%5D+from+0+to+1
# [(1-Sin(4*pi*(x/4)+pi/2+pi))/2 ,(1-Sin(4*pi*(x/4)+pi/2))/2] from 0 to 1

samples_per_block = 22
blocks = 30

print(
    """
#ifndef CROSSFADE_LIB
const uint8_t __in_flash() crossfade_raw[] = {"""
)
for _, scaler in enumerate(range(101)):
    if scaler == 0:
        continue
    for v in range(blocks):
        x = float(v) / blocks
        x = round(
            float(scaler)
            * ((1 - math.sin(4 * math.pi * (x / 4) + math.pi / 2 + math.pi)) / 2)
        )
        print("{},".format(x), end="")
    print("\n\t", end="")

print("\n};")

print(
    """
#define CROSSFADE_UPDATE_SAMPLES """
    + str(samples_per_block)
    + """
#define CROSSFADE_MAX """
    + str(blocks * samples_per_block)
    + """ 
uint8_t crossfade_vol(uint8_t current_vol, uint16_t phase_since) {
    if (current_vol==0 || phase_since >= CROSSFADE_MAX) {
        return 0;
    }
    return crossfade_raw[(current_vol-1)*"""
    + str(blocks)
    + """+(phase_since/CROSSFADE_UPDATE_SAMPLES)];
}

#define CROSSFADE_LIB 1
#endif
"""
)
