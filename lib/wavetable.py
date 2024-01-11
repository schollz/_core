import math


def dbamp(db):
    return math.pow(10, db / 20)


# midi to frequency
def mtof(m):
    return 440.0 * math.pow(2, (m - 69.0) / 12.0)


# wavetable generator
def wavetable(f, sr):
    # f: frequency
    # sr: sampling rate
    # returns a list of samples
    # one full cycle
    dur = 1.0 / float(f)
    samples = []
    num_samples = round(sr * dur)
    for n in range(num_samples):
        samples.append(math.sin(2 * math.pi * f * n / sr))
    return samples


# plot a sine wave
def plot_wavetable(f, sr):
    import matplotlib.pyplot as plt

    # title with the number of points
    plt.title("Sine wave: %d points" % (sr / f))
    plt.plot(wavetable(f, sr))
    plt.show()


def lcm(a, b):
    return abs(a * b) // math.gcd(a, b)


total_bytes = 0
wavetable_max = 36
print("#ifndef LIB_WAVETABLE_DATA_H")
print("#define LIB_WAVETABLE_DATA_H 1")
print("#define WAVETABLE_MAX " + str(wavetable_max))
wavetable_len = []
for i in range(wavetable_max):
    note = i + 24
    s = wavetable(mtof(note), 44100)
    # if i == 0:
    #     plot_wavetable(mtof(note), 44100)
    wavetable_len.append(len(s))
    print("const int32_t wavetable%d[%d] = {" % (i, len(s)))
    for j in range(len(s)):
        print("  %d," % round(s[j] * 1073741823))  # 32767 2147483647
        total_bytes += 4
    print("};")
print("uint16_t wavetable_len(uint8_t wave) {")
print("  switch (wave) {")
for i in range(wavetable_max):
    print("    case %d: return %d;" % (i, wavetable_len[i]))
print("    default: return 0;")
print("  }")
print("}")
print("int32_t wavetable_sample(uint8_t wave, uint16_t index) {")
print("  switch (wave) {")
for i in range(wavetable_max):
    print("    case %d:" % i)
    print(f"return wavetable{i}[index];")
    # print(f"if (index<{wavetable_len[i]}) {{return wavetable{i}[index];}}")
    # print(f"else if (index<{wavetable_len[i]}) {{return wavetable{i}[index];}}")
    # print(
    #     f"else if (index<{2*wavetable_len[i]}) {{return wavetable{i}[{wavetable_len[i]-1}-(index-{wavetable_len[i]})];}}"
    # )
    # print(
    #     f"else if (index<{3*wavetable_len[i]}) {{return -1*wavetable{i}[(index-{wavetable_len[i]*2})];}}"
    # )
    # print(
    #     f"else {{return -1*wavetable{i}[{wavetable_len[i]-1}-(index-{wavetable_len[i]*3})];}}"
    # )
    print("break;")
print("    default: return 0;")
print("  }")
print("}")

# print("#define WAVETABLE_TOTAL_BYTES " + str(total_bytes))

# print(
#     """
# int32_t *wavetable_samples;
# void init_wavetables() {
#     wavetable_samples = (int32_t *)malloc(WAVETABLE_TOTAL_BYTES);
#     """
# )
# j = 0
# for i in range(wavetable_max):
#     print(
#         "    memcpy(wavetable_samples + %d, wavetable%d, wavetable_len(%d) * sizeof(int32_t));"
#         % (j, i, i)
#     )
#     j += wavetable_len[i]
# print(
#     """
# }
# """
# )
# print("static int32_t wavetable_sample2(uint8_t wave, uint16_t index) {")
# print("  switch (wave) {")
# j = 0
# for i in range(wavetable_max):
#     print("    case %d: return wavetable_samples[index + %d];" % (i, j))
#     j += wavetable_len[i]
# print("    default: return 0;")
# print("  }")
# print("}")
print("#endif")
