#!/usr/bin/env -S uv --quiet run --script
# /// script
# requires-python = ">=3.13"
# dependencies = [
#     "matplotlib",
#     "numpy",
#     "scikit-image",
#     "plotly",
#     "kaleido",
# ]
# ///

# sudo minicom -b 115200 -o -D /dev/ttyACM0 | tee dev/sdcards/micro_center_10_64gb_xc.txt
# mogrify -resize 440x560! *.png

import glob
import math

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.offsetbox import OffsetImage, AnnotationBbox
import matplotlib.patches as patches
import matplotlib
from skimage.transform import resize


def get_numbers(fname):
    cpu = []
    sd = []
    current_sd = 0
    last_sd = 0
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
                current_sd = float(words[1]) / float(words[2])
                if last_sd < 10:
                    sd.append(current_sd)
                print(last_sd)
                last_sd = current_sd
            except:
                continue
    cpu = np.array(cpu)
    sd = np.array(sd)
    # cpu = cpu[np.where(cpu < 100)]
    return cpu, sd


def getImage(path):
    img = plt.imread(path)
    pixel_width = 400
    img = resize(img, (int(pixel_width * img.shape[0] / img.shape[1]), pixel_width))
    return OffsetImage(img, zoom=0.1)


# glob get all *.txt files in dev/sdcards
fnames = glob.glob("dev/sdcards4/*.txt")

medians = []
for fname in fnames:
    _, sd = get_numbers(fname)
    medians.append(np.median(sd))

# # Sort filenames based on the median values
fnames = [x for _, x in sorted(zip(medians, fnames))]

# sort filenames based on filename
# fnames = sorted(fnames)

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
    # check if image_path exists
    if not glob.glob(image_path):
        image_path = fname.split(".")[0] + ".png"

    images.append(getImage(image_path))
    # Extracting the file name without the extension for labels
    file_label = fname.split("/")[-1].replace(".txt", "")
    # find proprotion of sd timings > 10
    proportion = len(sd[np.where(sd > 10)]) / (len(sd))
    proportion_per_min = proportion / 0.5 * 60
    if proportion_per_min > 0:
        file_label = f"{file_label}\n{1/proportion_per_min:.2f}"
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

plt.savefig("dev/sdcards4/sdcard_timings4.png", dpi=300, transparent=True)
plt.show()
