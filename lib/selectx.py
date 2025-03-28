import math

# https://www.wolframalpha.com/input?i=%5B%281-Sin%284*pi*%28x%2F4%29%2Bpi%2F2%2Bpi%29%29%2F2+%2C%281-Sin%284*pi*%28x%2F4%29%2Bpi%2F2%29%29%2F2%5D+from+0+to+1
# [(1-Sin(4*pi*(x/4)+pi/2+pi))/2 ,(1-Sin(4*pi*(x/4)+pi/2))/2] from 0 to 1

scaler = 128

print(
    """
const uint8_t __in_flash() selectx_raw[] = {"""
)
for i, v in enumerate(range(0, scaler+1)):
    x = float(v) / scaler
    e = ""
    if i % 8 == 0:
        e = "\n"
    x = round(
        float(scaler)
        * ((1 - math.sin(4 * math.pi * (x / 4) + math.pi / 2 + math.pi)) / 2)
    )
    print("{},".format(x), end=e)
    if i % 8 == 0:
        print("\t", end="")

print("\n};")

print(
    """
// selectx takes x, range: [0,"""
    + "{}".format(scaler)
    + """] and mixes
// y1 and y2. x == 0 : full y1. x == """
    + "{}".format(scaler)
    + """: full y2.
int16_t selectx(int8_t x, int16_t y1, int16_t y2) {
    if (x<=0) {
        return y1;
    } else if (x>="""
    + "{}".format(scaler)
    + """) {
        return y2;
    }
    int32_t y = y1 * selectx_raw[x];
    y = y + ("""
    + "{}".format(scaler)
    + """-selectx_raw[x]) * y2;
    return y / """
    + "{}".format(scaler)
    + """;
}"""
)
