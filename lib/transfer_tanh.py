import math

print(
    """
const uint16_t __in_flash() transfer_tanh_raw[] = {
"""
)
for i, v in enumerate(range(-32767, 32768)):
    x = float(v) / 32767
    e = ""
    if i % 8 == 0:
        e = "\n"
    x = round(32768 * ((3 * x) / (1 + abs(3 * x))))
    print("{},".format(x), end=e)
    if i % 8 == 0:
        print("\t", end="")

print("\n};")

print(
    """
int16_t transfer_tanh(int32_t v) {
    v = v + 32767;
    if (v<0) {
        return -32767;
    }
    if (v>65533) {
        return 32767;
    }
    return transfer_tanh_raw[v];
}"""
)
