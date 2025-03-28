import math
from icecream import ic


# https://stackoverflow.com/questions/52547218/how-to-caculate-biquad-filter-coefficient
def coefficients(FC, FS, Q, dB, lowpass=True, ROUNDER=20):
    w0 = 2 * math.pi * (FC / FS)
    cosW = math.cos(w0)
    sinW = math.sin(w0)
    A = math.pow(10, (dB / 40))
    alpha = sinW / (2 * Q)
    beta = math.pow(A, 0.5) / Q
    b0 = (1 - cosW) / 2
    b1 = 1 - cosW
    b2 = (1 - cosW) / 2
    a0 = 1 + alpha
    a1 = -2 * cosW
    a2 = 1 - alpha
    if not lowpass:
        b0 = (1 + cosW) / 2
        b1 = -(1 + cosW)
        b2 = (1 + cosW) / 2
        a0 = 1 + alpha
        a1 = -2 * cosW
        a2 = 1 - alpha
    b0 = b0 / a0
    b1 = b1 / a0
    b2 = b2 / a0
    a1 = a1 / a0
    a2 = a2 / a0

    b0 = round((1 << ROUNDER) * b0)
    b1 = round((1 << ROUNDER) * b1)
    b2 = round((1 << ROUNDER) * b2)
    a1 = round((1 << ROUNDER) * a1)
    a2 = round((1 << ROUNDER) * a2)

    # ic(FC, FS, Q, dB)
    # ic(a1, a2, b0, b1, b2)
    return (a1, a2, b0, b1, b2)


def midi2freq(note):
    return 440 * math.pow(2, (note - 69) / 12)


# lopwass
ROUNDER = 20
q = 0.707 * 2.5
notes = list(range(76, 122))
print(f"#define LPF_MAX {len(notes)-1}")
print("long x1_f, x2_f, y1_f, y2_f;")
print("int32_t filter_lpf(long x, long f_, uint8_t q) {")
print("  long y;")
print(
    """long f = f_;
"""
)
print("  if (f>LPF_MAX) f=LPF_MAX;")
for i, note in enumerate(notes):
    if i == 0:
        print(f"  if (f=={i}) " + "{")
    elif i == len(notes) - 1:
        print("  else {")
    else:
        print(f"  else if (f=={i}) " + "{")
    freq = midi2freq(note)
    (a1, a2, b0, b1, b2) = coefficients(freq, 44100, q, 0, ROUNDER)
    print(
        f"      y = {b0} * x + {b1} * x1_f + {b2} * x2_f - {a1} * y1_f - {a2} * y2_f; "
    )

    print("    }")
print(f" y=y>>{ROUNDER};")
print(
    """
  x2_f = x1_f;
  x1_f = x;
  y2_f = y1_f;
  y1_f = y;
  return (int32_t)(y);
"""
)
print("}")


# # highpass
# q = 0.707 * 1
# ROUNDER = 20
# notes = list(range(80, 130))
# print(f"#define HPF_MAX {len(notes)-1}")
# print("int32_t xh1_f, xh2_f, yh1_f, yh2_f;")
# print("uint8_t filter_hpf(int32_t x, uint8_t f, uint8_t q) {")
# print("  int32_t y;")
# print("  if (f>HPF_MAX) f=HPF_MAX;")
# for i, note in enumerate(notes):
#     if i == 0:
#         print(f"  if (f=={i}) " + "{")
#     elif i == len(notes) - 1:
#         print("  else {")
#     else:
#         print(f"  else if (f=={i}) " + "{")
#     freq = midi2freq(note)
#     (a1, a2, b0, b1, b2) = coefficients(freq, 33000, q, 16, False, ROUNDER)
#     print(
#         f"      y = {b0} * x + {b1} * xh1_f + {b2} * xh2_f - {a1} * yh1_f - {a2} * yh2_f; "
#     )

#     print("    }")
# print(f" y=y>>{ROUNDER};")
# print(
#     """
#   xh2_f = xh1_f;
#   xh1_f = x;
#   yh2_f = yh1_f;
#   yh1_f = y;
#   return (int16_t)(y);
# """
# )
# print("}")
