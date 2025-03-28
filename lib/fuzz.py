import math


def dbamp(db):
    return math.pow(10, db / 20)


def fuzz(num_points, max_value):
    a = 5.4
    samples = []
    for n in range(int(num_points)):
        x = n / num_points
        y = (a * x) / (1 + abs(a * x)) * max_value
        samples.append(y)
    return samples


samples = fuzz(math.pow(2, 15), (math.pow(2, 16) - 1) * dbamp(-6))

samples_plot = []
for i in range(len(samples) - 1, 0, -1):
    samples_plot.append(-1 * samples[i])
samples_plot += samples
# # plot samples
# from matplotlib import pyplot as plt
# plt.plot(range(-32768, 32767), samples_plot)
# plt.xlabel("in (int16_t)")
# plt.ylabel("out (int16_t)")
# plt.show()

print("#ifndef LIB_FUZZ")
print("#define LIB_FUZZ 1")
print("#include <stdint.h>\n")
print(f"const int16_t __in_flash() fuzz_samples[{len(samples)}] = {{")
for sample in samples:
    print(f"\t{int(sample)},")
print("};")
print(
    """void Fuzz_process(int16_t *values, uint16_t num_values, uint8_t pre_amp,
                  uint8_t post_amp) {
  for (uint16_t i = 0; i < num_values; i++) {
    values[i] = util_clamp((values[i] * linlin(pre_amp, 0, 255, 16, 256)) / 16, -32767, 32767);
    if (values[i] >= 0) {
      values[i] = fuzz_samples[values[i]];
    } else {
      values[i] = -1 * fuzz_samples[-values[i]];
    }
    values[i] = util_clamp((values[i] * linlin(post_amp, 0, 255, 2, 256)) / 256, -32767, 32767);
  }
}"""
)
print("#endif")
