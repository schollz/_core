# sudo minicom -b 115200 -o -D /dev/ttyACM0 | tee dev/sdcards/micro_center_10_64gb_xc.txt
# mogrify -resize 440x560! *.png

import glob
import math

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.offsetbox import OffsetImage, AnnotationBbox
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


def getImage(path):
    return OffsetImage(plt.imread(path), zoom=0.1)


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
images = []
file_labels = []

for fname in fnames:
    cpu, sd = get_numbers(fname)
    cpu_timings.append(cpu)
    sd_timings.append(sd)

    # Assuming the image has the same name but with a .jpg extension
    image_path = fname.replace(".txt", ".png")
    images.append(getImage(image_path))
    # Extracting the file name without the extension for labels
    file_label = fname.split("/")[-1].replace(".txt", "")
    file_labels.append(file_label)

# Create a separate figure for SD card timings
fig2, ax2 = plt.subplots(figsize=(12, 6))
ax2.boxplot(sd_timings)
ax2.set_title("SD Card Timings")
ax2.set_yscale("log")
ax2.set_ylabel("Read duration (sec/MB)")
ax2.set_ylim(0.005, 1000)


# Add images to x-axis for SD card timings
# Adjust the y_offset to position the images below the x-axis
y_offset = ax2.get_ylim()[0] * 1.5
for x, img in enumerate(images):
    ab = AnnotationBbox(img, (x + 1, y_offset), frameon=False, box_alignment=(0.5, 0))
    ax2.add_artist(ab)
# Hiding default x-axis labels
# Setting custom x-axis labels
plt.subplots_adjust(bottom=0.3)  # Adjust this value as needed
ax2.set_xticklabels(file_labels, rotation=45, ha="right")


# Create a rectangle to cover the area above 10.0
# x = 0.5 to start just before the first box, width = len(sd_timings) to cover all boxes
rect = patches.Rectangle(
    (0.5, 10),
    10000,
    10000,
    linewidth=0,
    edgecolor=None,
    facecolor="red",
    alpha=0.3,
)
ax2.add_patch(rect)

plt.savefig("dev/sdcards2/sdcard_timings2.png", dpi=300, transparent=True)
plt.show()
