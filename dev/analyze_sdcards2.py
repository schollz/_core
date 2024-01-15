# sudo minicom -b 115200 -o -D /dev/ttyACM0 | tee dev/sdcards/micro_center_10_64gb_xc.txt
# mogrify -resize 440x560! *.png

import glob
import math

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as patches


def get_numbers(fname):
    cpu = []
    sd = []
    with open(fname, "r") as f:
        # Read each line in the file
        for line in f:
            # Split the line into words
            line = line.split("sdcard")
            if len(line) < 2:
                continue
            words = line[1].split()
            if len(words) != 5:
                continue

            try:
                cpu.append(float(words[0]))
                sd.append(float(words[1]) / float(words[2]))
            except:
                continue
    cpu = np.array(cpu)
    sd = np.array(sd)
    # cpu = cpu[np.where(cpu < 100)]
    return cpu, sd


# glob get all *.txt files in dev/sdcards
fnames = glob.glob("dev/sdcards2/*.txt")

medians = []
for fname in fnames:
    _, sd = get_numbers(fname)
    medians.append(np.median(sd))

# Sort filenames based on the median values
fnames = [x for _, x in sorted(zip(medians, fnames))]

cpu_timings = []
sd_timings = []
file_labels = []

for fname in fnames:
    cpu, sd = get_numbers(fname)
    cpu_timings.append(cpu)
    sd_timings.append(sd)
    # Extracting the file name without the extension for labels
    file_label = fname.split("/")[-1].split(".")[0]
    file_labels.append(f"{file_label}\ncpu ~ {np.average(cpu):2.1f} %")

# Create a separate figure for SD card timings
fig2, ax2 = plt.subplots(figsize=(12, 6))
ax2.boxplot(sd_timings)
ax2.set_title("SD Card Timings")
ax2.set_yscale("log")
ax2.set_ylabel("Read duration (sec/MB)")
ax2.set_ylim(0.1, 20)

ax2.set_xticklabels(file_labels, rotation=45, ha="right")
# create more room at the bottom for the label
plt.subplots_adjust(bottom=0.3)

# Create a rectangle to cover the area above 10.0
# x = 0.5 to start just before the first box, width = len(sd_timings) to cover all boxes
rect = patches.Rectangle(
    (0.5, 10),
    100,
    100,
    linewidth=0,
    edgecolor=None,
    facecolor="red",
    alpha=0.3,
)
ax2.add_patch(rect)

plt.show()
