import math

PI_FLOAT = math.pi

RESONANT_FILTER_Q_BITS = 16
RESONANT_FILTER_FRACTIONAL_BITS = 1 << RESONANT_FILTER_Q_BITS


def float_to_fixed_point(value):
    return int(value * RESONANT_FILTER_FRACTIONAL_BITS)


def midi_to_freq(midi_note):
    return 440 * pow(2, (midi_note - 69) / 12)


def design_filter(fc, fs, db, q, filter_type):
    w0 = 2 * PI_FLOAT * (fc / fs)
    cosW = math.cos(w0)
    sinW = math.sin(w0)
    A = pow(10, db / 40)
    alpha = sinW / (2 * q)
    beta = pow(A, 0.5) / q
    b0, b1, b2, a0, a1, a2 = 0, 0, 0, 0, 0, 0

    if filter_type == "FILTER_HIGHPASS":
        b0 = (1 + cosW) / 2
        b1 = -(1 + cosW)
        b2 = b0
        a0 = 1 + alpha
        a1 = -2 * cosW
        a2 = 1 - alpha
    else:
        # Low pass
        b1 = 1 - cosW
        b0 = b1 / 2
        b2 = b0
        a0 = 1 + alpha
        a1 = -2 * cosW
        a2 = 1 - alpha

    b0 = b0 / a0
    b1 = b1 / a0
    b2 = b2 / a0
    a1 = a1 / a0
    a2 = a2 / a0

    # convert each to fixed point
    b0 = float_to_fixed_point(b0)
    b1 = float_to_fixed_point(b1)
    b2 = float_to_fixed_point(b2)
    a0 = float_to_fixed_point(a0)
    a1 = float_to_fixed_point(a1)
    a2 = float_to_fixed_point(a2)

    return b0, b1, b2, a0, a1, a2


# generate midi notes between 100 hz and 20000 hz
# for each note, generate a filter
# produce a multidimensional array, indexed by q and frequency
# for C-code generation
qs = [1.5]
notes = list(range(44, 130))
filter_types = ["FILTER_LOWPASS", "FILTER_HIGHPASS"]
# filter_types = ["FILTER_LOWPASS"]
print(f"const uint8_t resonantfilter_q_max = {len(qs)};")
print(f"const uint8_t resonantfilter_fc_max = {len(notes)};")
print(f"const uint8_t resonantfilter_filter_max = {len(filter_types)};")

print(
    f"const int32_t resonantfilter_data[{len(filter_types)}][{len(qs)}][{len(notes)}][5] = {{"
)

for filter_type in filter_types:
    print(f"// {filter_type}")
    print("{")
    for i, q in enumerate(qs):
        if filter_type == "FILTER_HIGHPASS":
            q = 1.05
        print(f"// q = {q}")
        print("{")
        for j, note in enumerate(notes):
            freq = midi_to_freq(note)
            print(f"// note = {note}, frequency = {freq}")
            # print("#", j, note, i, q, freq)
            if freq > 100 and freq < 20000:
                b0, b1, b2, a0, a1, a2 = design_filter(freq, 44100, 0, q, filter_type)
                print("{ %d, %d, %d,  %d, %d }," % (b0, b1, b2, a1, a2))
        print("},")
    print("},")
print("};")
