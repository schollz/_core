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
# plot samples
from matplotlib import pyplot as plt

plt.plot(samples)
plt.show()

print("#ifndef LIB_FUZZ")
print("#define LIB_FUZZ 1")
print("#include <stdint.h>\n")
print(f"const int16_t fuzz_samples[{len(samples)}] = {{")
for sample in samples:
    print(f"\t{int(sample)},")
print("};")
print("void Fuzz_process(int16_t *values, uint16_t num_values) {")
print("\tfor (uint16_t i = 0; i < num_values; i++) {")
print("\t\tif (values[i] >= 0) {")
print("\tvalues[i] = fuzz_samples[values[i]];")
print("\t\t} else {")
print("\t\tvalues[i] = -1*fuzz_samples[-values[i]];")
print("\t}")
print("}")
print("}")
print("#endif")
