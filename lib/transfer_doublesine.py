import math
import sys

import numpy as np
import matplotlib.pyplot as plt

print(
    """
const int16_t transfer_doublesine_raw[] = {
"""
)

# Create a numpy array of x-values from 0 to 1
x = np.linspace(0, 65535, 65536)

doublesine = np.sin(2 * 2 * np.pi * x / 65535)
doublesine = np.multiply(doublesine, 32767)
doublesine = np.round(doublesine)

for i, v in enumerate(doublesine):
    e = ""
    if i % 8 == 0:
        e = "\n"
    print("{},".format(int(v)), end=e)
    if i % 8 == 0:
        print("\t", end="")

print("\n};")

print(
    """
int16_t transfer_doublesine(int16_t v) {
    return transfer_doublesine_raw[v+32768];
}"""
)


# plt.plot(x, doublesine)
# plt.show()
# print(len(doublesine))
# print(np.max(doublesine))
# print(np.min(doublesine))
