#!/usr/bin/env -S uv --quiet run --script
# /// script
# requires-python = ">=3.13"
# dependencies = [
#     "numpy",
#     "plotly",
#     "click",
# ]
# ///
import glob
import os
import click
import numpy as np
import plotly.graph_objects as go
import plotly.io as pio

pio.renderers.default = "firefox"


def parse_line(line):
    if b"sdcard" not in line:
        return None
    parts = line.split(b"sdcard", 1)
    fields = parts[1].split()
    if len(fields) != 5:
        return None
    try:
        return float(fields[0])
    except:
        return None


def parse_file(fname):
    vals = []
    with open(fname, "rb") as f:
        for line in f:
            c = parse_line(line)
            if c is not None:
                vals.append(c)
    return np.array(vals)


def extract_label(fname):
    base = os.path.basename(fname)
    return base[:-4] if base.endswith(".txt") else base


@click.command()
@click.argument("folder", type=click.Path(exists=True))
def main(folder):
    folder = os.path.abspath(folder)
    fnames = sorted(glob.glob(os.path.join(folder, "*.txt")))
    data = {}

    for fname in fnames:
        arr = parse_file(fname)
        if len(arr):
            data[extract_label(fname)] = arr

    if not data:
        return

    labels = list(data.keys())
    xs = []
    ys = []

    for label in labels:
        arr = data[label]
        xs.extend([label] * len(arr))
        ys.extend(arr)

    fig = go.Figure()
    fig.add_trace(
        go.Box(
            x=xs,
            y=ys,
            boxpoints="all",
            jitter=0.3,
            pointpos=-1.8,
            marker=dict(size=3),
        )
    )

    fig.update_layout(
        title="CPU Utilization",
        yaxis=dict(title="cpu"),
        xaxis=dict(title="File", tickangle=-45),
        showlegend=False,
        height=600,
    )

    fig.show()


if __name__ == "__main__":
    main()
