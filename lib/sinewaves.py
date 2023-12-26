import math


def dbamp(db):
    return math.pow(10, db / 20)


# midi to frequency
def mtof(m):
    return 440.0 * math.pow(2, (m - 69.0) / 12.0)


# sinewave generator
def sinewave(f, sr):
    # f: frequency
    # sr: sampling rate
    # returns a list of samples
    # one full cycle
    dur = 1.0 / float(f)
    samples = []
    for n in range(int(sr * dur)):
        samples.append(math.sin(2 * math.pi * f * n / sr))
    return samples


# plot a sine wave
def plot_sinewave(f, sr):
    import matplotlib.pyplot as plt

    # title with the number of points
    plt.title("Sine wave: %d points" % (sr / f))
    plt.plot(sinewave(f, sr))
    plt.show()


def lcm(a, b):
    return abs(a * b) // math.gcd(a, b)


total_bytes = 0
sinewave_max = 36
print("#ifndef LIB_SINEWAVE")
print("#define LIB_SINEWAVE 1")
print("#define SINEWAVE_MAX " + str(sinewave_max))
sinewave_len = []
for i in range(sinewave_max):
    note = i + 24
    s = sinewave(mtof(note), 44100)
    sinewave_len.append(len(s))
    print("const int32_t sinewave%d[%d] = {" % (i, len(s)))
    for j in range(len(s)):
        print("  %d," % round(s[j] * 2147483647))  # 32767 2147483647
        total_bytes += 4
    print("};")
print("uint16_t sinewave_len(uint8_t wave) {")
print("  switch (wave) {")
for i in range(sinewave_max):
    print("    case %d: return %d;" % (i, sinewave_len[i]))
print("    default: return 0;")
print("  }")
print("}")
print("int32_t sinewave_sample(uint8_t wave, uint16_t index) {")
print("  switch (wave) {")
for i in range(sinewave_max):
    print("    case %d: return sinewave%d[index];" % (i, i))
print("    default: return 0;")
print("  }")
print("}")

print("#define SINEWAVE_TOTAL_BYTES " + str(total_bytes))

print(
    """
int32_t *sinewave_samples;
void init_sinewaves() {
    sinewave_samples = (int32_t *)malloc(SINEWAVE_TOTAL_BYTES);
    """
)
j = 0
for i in range(sinewave_max):
    print(
        "    memcpy(sinewave_samples + %d, sinewave%d, sinewave_len(%d) * sizeof(int32_t));"
        % (j, i, i)
    )
    j += sinewave_len[i]
print(
    """
}
"""
)
print("static int32_t sinewave_sample2(uint8_t wave, uint16_t index) {")
print("  switch (wave) {")
j = 0
for i in range(sinewave_max):
    print("    case %d: return sinewave_samples[index + %d];" % (i, j))
    j += sinewave_len[i]
print("    default: return 0;")
print("  }")
print("}")
print("#endif")
