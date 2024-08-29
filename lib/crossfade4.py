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
    cos_out = np.cos(np.pi / 2 * x)
    cos_in = np.cos((1 - x) * np.pi / 2)
    sqrt_in = np.sqrt(np.sqrt(x))
    sqrt_out = np.sqrt(np.sqrt(1 - x))
    exp_out = np.exp(-x * np.exp(1.5))
    exp_in = np.exp(x * np.exp(1.5)) / np.exp(np.exp(1.5))
    line_out = 1 - x

    # fade out the last 10% of the exponential
    for i in range(int(block_size * 0.9), block_size):
        exp_out[i] = exp_out[i] * (1 - (i - int(block_size * 0.9)) / (block_size * 0.1))
    print(
        """#ifndef CROSSFADE3_LIB
#define CROSSFADE3_COS 0      
#define CROSSFADE3_SQRT 1      
#define CROSSFADE3_EXP 2      
#define CROSSFADE3_LINE 3
"""
    )
    print("#define CROSSFADE3_LIMIT %d" % block_size)
    print(f"static int32_t crossfade3_cos_out[{block_size}]={{")
    s = ""
    for _, v in enumerate(cos_out):
        s += f"{q16_16_float_to_fp(v)},"
    print(s)
    print("};")
    print(f"static int32_t crossfade3_cos_in[{block_size}]={{")
    s = ""
    for _, v in enumerate(cos_in):
        s += f"{q16_16_float_to_fp(v)},"
    print(s)
    print("};")

    print(f"static int32_t crossfade3_sqrt_out[{block_size}]={{")
    s = ""
    for _, v in enumerate(sqrt_out):
        s += f"{q16_16_float_to_fp(v)},"
    print(s)
    print("};")

    print(f"static int32_t crossfade3_sqrt_in[{block_size}]={{")
    s = ""
    for _, v in enumerate(sqrt_in):
        s += f"{q16_16_float_to_fp(v)},"
    print(s)
    print("};")

    print(f"static int32_t crossfade3_exp_out[{block_size}]={{")
    s = ""
    for _, v in enumerate(exp_out):
        s += f"{q16_16_float_to_fp(v)},"
    print(s)
    print("};")

    print(f"static int32_t crossfade3_exp_in[{block_size}]={{")
    s = ""
    for _, v in enumerate(exp_in):
        s += f"{q16_16_float_to_fp(v)},"
    print(s)
    print("};")

    print(f"static int32_t crossfade3_line[{block_size}]={{")
    s = ""
    for _, v in enumerate(line_out):
        s += f"{q16_16_float_to_fp(v)},"
    print(s)
    print("};")

    print(
        """
          
int16_t crossfade3_out(int16_t val, uint16_t i, uint8_t crossfade_type) {
          if (crossfade_type==CROSSFADE3_COS) {
            return (crossfade3_cos_out[i] * (int32_t)val)>>16;
          } else if (crossfade_type==CROSSFADE3_SQRT) {
            return (crossfade3_sqrt_out[i] * (int32_t)val)>>16;
          } else if (crossfade_type==CROSSFADE3_EXP) {
            return (crossfade3_exp_out[i] * (int32_t)val)>>16;
          } else {
            return 
          ((crossfade3_line[i])*(int32_t)val)>>16;
          }
}          
int16_t crossfade3_in(int16_t val, uint16_t i, uint8_t crossfade_type) {
          if (crossfade_type==CROSSFADE3_COS) {
            return (crossfade3_cos_in[i] * (int32_t)val)>>16;
          } else if (crossfade_type==CROSSFADE3_SQRT) {
            return (crossfade3_sqrt_in[i] * (int32_t)val)>>16;
          } else if (crossfade_type==CROSSFADE3_EXP) {
            return (crossfade3_exp_in[i] * (int32_t)val)>>16;
          } else {
            return 
          ((65536-crossfade3_line[i])*(int32_t)val)>>16;
          }
}
#define CROSSFADE3_LIB 1
#endif
"""
    )

    # # Plot the sine wave
    # plt.plot(x, cos_out, label="Cos", color="r")
    # plt.plot(x, sqrt_out, label="Sqrt", color="b")
    # plt.plot(x, line_out, label="Line", color="g")
    # plt.plot(x, exp_out, label="Exp", color="y")
    # plt.plot(x, cos_in, color="r")
    # plt.plot(x, sqrt_in, color="b")
    # plt.plot(x, 1 - line_out, color="g")
    # plt.plot(x, exp_in, color="y")
    # plt.legend()
    # plt.xlabel("X")
    # plt.ylabel("Y")
    # plt.title("Curves")
    # plt.show()


if __name__ == "__main__":
    run()
