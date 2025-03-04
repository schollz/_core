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
    count_block_drops = 0
    block_size = fname.split("/")[-1].replace(".txt", "")
    block_size = float(block_size.split("_")[-1].replace("gb", ""))
    ms_limit = 1000.0 * block_size / 44100.0
    with open(fname, "rb") as f:
        for line in f:
            if b"flag: 3" in line:
                count_block_drops += 1
            if b"sdcard" not in line:
                continue
            line = line.split(b"sdcard")
            if len(line) < 2:
                continue
            words = line[1].split()
            if len(words) != 5:
                continue
            try:
                cpu.append(float(words[0]))
                current_sd = float(words[1]) / 1000.0
                sd.append(current_sd / ms_limit * 100)
                last_sd = current_sd
            except:
                continue
    print(f"{fname}: {count_block_drops} block drops")
    return np.array(cpu), np.array(sd)


fnames = glob.glob("dev/sdcards5/*.txt")
# sort fnames by the number (after removing .txt)
fnames = sorted(
    fnames,
    key=lambda x: int(
        x.split("/")[-1].replace(".txt", "").split("_")[-1].replace("gb", "")
    ),
)
medians = [np.median(get_numbers(fname)[1]) for fname in fnames]

cpu_timings = []
sd_timings = []
file_labels = []

for fname in fnames:
    cpu, sd = get_numbers(fname)
    cpu_timings.append(cpu)
    sd_timings.append(sd)
    # calculate the percentage above 100%
    above_100 = np.sum(sd > 100) / len(sd)
    time_between_block_drops = 0
    block_size = fname.split("/")[-1].replace(".txt", "")
    block_size = float(block_size.split("_")[-1].replace("gb", ""))
    if above_100 > 0:
        time_between_block_drops = (1.0 / above_100) * block_size / 44100.0
    file_label = fname.split("/")[-1].replace(".txt", "")
    file_label = f"{file_label}<br>{time_between_block_drops:.1f}"
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
        name="Timings for Default SD Card (b83355)",
    )
)

fig.update_layout(
    title="SD Card Timings",
    yaxis=dict(title="SD reading (% of total block time)"),
    xaxis=dict(title="Block size", tickangle=-45),
    showlegend=False,
    height=600,
)

# draw a line as 100%
fig.add_shape(
    type="line",
    x0=-1,
    y0=100,
    x1=5.5,
    y1=100,
    line=dict(color="red", width=2),
)
# label the line as block drop
fig.add_annotation(
    x=3.5,
    y=108,
    xref="x",
    yref="y",
    text="Above this line, blocks are dropped",
    showarrow=False,
    font=dict(size=12, color="red"),
)

fig.write_image("dev/sdcards5/sdcard_timings5.png")
fig.show()
