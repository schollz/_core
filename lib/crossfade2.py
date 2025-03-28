import math

# https://www.wolframalpha.com/input?i=%5B%281-Sin%284*pi*%28x%2F4%29%2Bpi%2F2%2Bpi%29%29%2F2+%2C%281-Sin%284*pi*%28x%2F4%29%2Bpi%2F2%29%29%2F2%5D+from+0+to+1
# [(1-Sin(4*pi*(x/4)+pi/2+pi))/2 ,(1-Sin(4*pi*(x/4)+pi/2))/2] from 0 to 1

samples = 441
scalar = 128

print("#ifndef CROSSFADE2_LIB")
print(f"static int16_t crossfade2_raw[{samples}] = {{")
for v in range(samples):
    x = float(v) / float(samples)
    x = round(
        float(scalar)
        * ((1 - math.sin(4 * math.pi * (x / 4) + math.pi / 2 + math.pi)) / 2)
    )
    print("{},".format(x), end="")
print("};\n")

print(
    """
#define CROSSFADE2_LIB 1
#endif
"""
)
