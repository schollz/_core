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
#
# sudo minicom -b 115200 -o -D /dev/ttyACM0 | tee dev/sdcards/micro_center_10_64gb_xc.txt
# mogrify -resize 440x560! *.png
import glob
import numpy as np
import plotly.graph_objects as go
from skimage.transform import resize
import plotly.io as pio

pio.renderers.default = "browser"


def get_numbers(fname):
    cpu = []
    sd = []
    current_sd = 0
    last_sd = 0
    with open(fname, "r") as f:
        for line in f:
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
                last_sd = current_sd
            except:
                continue
    return np.array(cpu), np.array(sd)


fnames = glob.glob("dev/sdcards5/*.txt")
medians = [np.median(get_numbers(fname)[1]) for fname in fnames]
fnames = [x for _, x in sorted(zip(medians, fnames))]

cpu_timings = []
sd_timings = []
file_labels = []

for fname in fnames:
    cpu, sd = get_numbers(fname)
    cpu_timings.append(cpu)
    sd_timings.append(sd)
    file_label = fname.split("/")[-1].replace(".txt", "")
    if len(sd) > 0:
        proportion = len(sd[np.where(sd > 10)]) / len(sd)
        proportion_per_min = proportion / 0.5 * 60
    else:
        proportion_per_min = 0
    if proportion_per_min > 0:
        file_label = f"{file_label}<br>{1/proportion_per_min:.2f}"
    file_labels.append(file_label)

fig = go.Figure()
fig.add_trace(
    go.Box(
        y=[item for sublist in sd_timings for item in sublist],
        x=[label for label, sublist in zip(file_labels, sd_timings) for _ in sublist],
        boxpoints="all",
        jitter=0.3,
        pointpos=-1.8,
        marker=dict(size=3),
        name="SD Card Timings",
    )
)

fig.update_layout(
    title="SD Card Timings",
    yaxis=dict(type="log", title="Read duration (sec/MB)", range=[-2.3, 3.0]),
    xaxis=dict(title="File", tickangle=-45),
    showlegend=False,
    height=600,
)

fig.add_shape(
    type="rect",
    x0=-0.5,
    x1=len(sd_timings),
    y0=10,
    y1=10000,
    line=dict(width=0),
    fillcolor="red",
    opacity=0.3,
)

fig.write_image("dev/sdcards5/sdcard_timings5.png")
fig.show()
