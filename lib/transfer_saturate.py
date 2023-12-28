import math

print(
    """
const uint16_t __in_flash() transfer_doublesine_raw[] = {
"""
)
points = []
for i, v in enumerate(range(-32767, 32768)):
    x = float(v) / 32767
    e = ""
    if i % 8 == 0:
        e = "\n"
    x = round(0.75 * 32767 * math.sin(2 * math.pi * x))
    points.append(x)
    print("{},".format(x), end=e)
    if i % 8 == 0:
        print("\t", end="")

print("\n};")

print(
    """
int16_t transfer_doublesine(int32_t v) {
    v = v + 32767;
    if (v<= 0 || v>=65533) {
        return 0;
    }
    return transfer_doublesine_raw[v];
}"""
)

# plot points
import matplotlib.pyplot as plt

plt.plot(points)
plt.show()
