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
    block_size = int(sys.argv[1])
    if block_size == None:
        print("oops")
        return
    # Create a numpy array of x-values from 0 to 1
    x = np.linspace(0, 1, block_size)

    # Calculate the sine of the x-values
    y = (1 - np.sin(4 * np.pi * (x / 4) + np.pi / 2 + np.pi)) / 2
    y2 = np.exp(-5.4 * x)
    y3 = 1 - np.log(1.7 * x + 1)
    y4 = 1 - x

    print(
        """#ifndef CROSSFADE3_LIB
#define CROSSFADE3_SINE 0      
#define CROSSFADE3_EXP 1      
#define CROSSFADE3_LOG 2      
#define CROSSFADE3_LINE 3
"""
    )
    print(f"static int32_t crossfade3_sine[{block_size}]={{")
    s = ""
    for _, v in enumerate(y):
        s += f"{q16_16_float_to_fp(v)},"
    print(s)
    print("};")

    print(f"static int32_t crossfade3_exp[{block_size}]={{")
    s = ""
    for _, v in enumerate(y2):
        s += f"{q16_16_float_to_fp(v)},"
    print(s)
    print("};")

    print(f"static int32_t crossfade3_log[{block_size}]={{")
    s = ""
    for _, v in enumerate(y3):
        s += f"{q16_16_float_to_fp(v)},"
    print(s)
    print("};")

    print(
        """
          
int16_t crossfade3_out(int16_t val, uint16_t i, uint8_t crossfade_type) {
          if (crossfade_type==CROSSFADE3_SINE) {
            return q16_16_fp_to_int16(
          q16_16_multiply(
          q16_16_int16_to_fp(val),
          crossfade3_sine[i]
          )
            );
          } else if (crossfade_type==CROSSFADE3_EXP) {
            return q16_16_fp_to_int16(
          q16_16_multiply(
          q16_16_int16_to_fp(val),
          crossfade3_exp[i]
          )
            );
          } else {
            return q16_16_fp_to_int16(
          q16_16_multiply(
          q16_16_int16_to_fp(val),
          crossfade3_log[i]
          )
            );          
          }
}          
int16_t crossfade3_in(int16_t val, uint16_t i, uint8_t crossfade_type) {
          if (crossfade_type==CROSSFADE3_SINE) {
            return q16_16_fp_to_int16(
          q16_16_multiply(
          q16_16_int16_to_fp(val),
          65536-crossfade3_sine[i]
          )
            );
          } else if (crossfade_type==CROSSFADE3_EXP) {
            return q16_16_fp_to_int16(
          q16_16_multiply(
          q16_16_int16_to_fp(val),
          65536-crossfade3_exp[i]
          )
            );
          } else {
            return q16_16_fp_to_int16(
          q16_16_multiply(
          q16_16_int16_to_fp(val),
          65536-crossfade3_log[i]
          )
            );          
          }
}
#define CROSSFADE3_LIB 1
#endif
"""
    )

    # Plot the sine wave
    plt.plot(x, y, label="Sine")
    plt.plot(x, y2, label="Exp")
    plt.plot(x, y3, label="Log")
    plt.plot(x, y4, label="Line")
    plt.plot(x, 1 - y, label="Sine")
    plt.plot(x, 1 - y2, label="Exp")
    plt.plot(x, 1 - y3, label="Log")
    plt.legend()
    plt.xlabel("X")
    plt.ylabel("Y")
    plt.title("Curves")
    plt.show()


if __name__ == "__main__":
    run()
