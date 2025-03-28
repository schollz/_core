import sys

import numpy as np
import matplotlib.pyplot as plt

Q16_16_Q_BITS = 16
Q16_16_FRACTIONAL_BITS = 1 << Q16_16_Q_BITS


def q16_16_fp_to_int(fixedValue):
    """Converts a Q16.16 fixed-point value to an int."""
    return int(fixedValue >> Q16_16_Q_BITS)


def q16_16_fp_to_float(value):
    """Converts a Q16.16 fixed-point value to a float."""
    return float(value) / Q16_16_FRACTIONAL_BITS


def q16_16_float_to_fp(value):
    """Converts a float to a Q16.16 fixed-point value."""
    return int(value * Q16_16_FRACTIONAL_BITS)


def q16_16_int_to_fp(value):
    """Converts an int to a Q16.16 fixed-point value."""
    return int(value) << Q16_16_Q_BITS


def q16_16_multiply(a, b):
    """Multiplies two Q16.16 fixed-point values."""
    return int(((int64(a) * b) >> Q16_16_Q_BITS))


def run():
    block_size = 255

    # Create a numpy array of x-values from 0 to 1
    x = np.linspace(0, 1, block_size)

    # Calculate the sine of the x-values
    cos_out = np.cos(np.pi / 2 * x)
    cos_in = np.cos((1 - x) * np.pi / 2)

    print(f"static int32_t selectx2_cos_out[{block_size}]={{")
    for i, v in enumerate(cos_out):
        e = ""
        if i % 8 == 0:
            e = "\n"
        print(f"{q16_16_float_to_fp(v)},", end=e)
        if i % 8 == 0:
            print("\t", end="")
    print("\n};")

    print(f"static int32_t selectx2_cos_in[{block_size}]={{")
    for i, v in enumerate(cos_in):
        e = ""
        if i % 8 == 0:
            e = "\n"
        print(f"{q16_16_float_to_fp(v)},", end=e)
        if i % 8 == 0:
            print("\t", end="")
    print("\n};")

    print(
        """
// selectx2 takes x, range: [0,255] and mixes
int32_t selectx2_mix(uint8_t x, int32_t y1, int32_t y2) {
    if (x<=0) {
        return y1;
    } else if (x>=255) {
        return y2;
    }
    int64_t z = ((int64_t)y1 * selectx2_cos_out[x]) + ((int64_t)selectx2_cos_out[x] * y2);
    return (int32_t)(z >> 16);
}
"""
    )
    # Plot the sine wave
    # plt.plot(x, cos_out, label="Cos", color="r")
    # plt.plot(x, cos_in, color="r")
    # plt.legend()
    # plt.xlabel("X")
    # plt.ylabel("Y")
    # plt.title("Curves")
    # plt.show()


if __name__ == "__main__":
    run()
