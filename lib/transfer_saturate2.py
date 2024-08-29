import math

print(
    """
const int16_t transfer_doublesine_raw[] = {
"""
)
points = []
for i, v in enumerate(range(0, int(32768 / 4))):
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
    v = (v + 32768);
    if (v < 8192) {
        return transfer_doublesine_raw[v];
    } else if (v < 16384) {
        return transfer_doublesine_raw[8191 - (v-8192)];
    } else if (v < 24576) {
        return -1* transfer_doublesine_raw[(v-16384)];
    } else if (v < 32768) {
        return -1* transfer_doublesine_raw[8191 - (v-24576)];
    } else if (v < 40960) {
        return transfer_doublesine_raw[v - 32768];    
    } else if (v < 49152) {
        return transfer_doublesine_raw[8191 - (v-40960)];
    } else if (v < 57344) {
        return -1* transfer_doublesine_raw[(v-49152)];
    } else if (v < 65536) {
        return -1* transfer_doublesine_raw[8191 - (v-57344)];
    }
    return 0;
}"""
)

# plot points
# import matplotlib.pyplot as plt
# plt.plot(points)
# plt.show()
