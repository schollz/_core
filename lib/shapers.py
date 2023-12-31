import math
import numpy as np


def compress_curve(mu):
    y = []
    for i in range(int(math.pow(2, resolution))):
        x = float(i) / math.pow(2, resolution)
        y.append(np.sign(x) * math.log(1 + mu * abs(x)) / math.log(1 + mu))
    return np.multiply(y, math.pow(2, 15) - 1)


def expand_curve(mu):
    y = []
    for i in range(int(math.pow(2, resolution))):
        x = float(i) / math.pow(2, resolution)
        y.append(np.sign(x) * (math.pow(1 + mu, abs(x)) - 1) / mu)
    return np.multiply(y, math.pow(2, 15) - 1)


resolution = 15
mu = 255 * 2
cc = compress_curve(mu)
ce = expand_curve(mu)

# from matplotlib import pyplot as plt
# plt.plot(cc, label="compress curve")
# plt.plot(ce, label="expand curve")
# plt.legend()
# plt.xlabel("in")
# plt.ylabel("out")
# plt.show()


print("#ifndef LIB_SHAPER")
print("#define LIB_SHAPER 1")
print("#define SHAPER_RESOLUTION " + str(resolution))
print("#define SHAPER_REDUCE " + str(15 - resolution))
print(f"const int16_t __in_flash() compress_curve_data[{len(cc)}] = {{")
for v in cc:
    print(f"\t{int(v)},")
print("};")

print(f"const int16_t __in_flash() expand_curve_data[{len(ce)}] = {{")
for v in ce:
    print(f"\t{int(v)},")
print("};")

print("#endif")
