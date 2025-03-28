#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = [
#     "plotly",
#     "numpy",
# ]
# ///

import numpy as np

import plotly.graph_objects as go

datasets = [
    {
        "name": "Pams 2x at 120 BPM",
        "beats": [0, 16, 120, 480, 720, 960, 1920, 2400],
        "lag": [0, 0, 0.006, 0.032, 0.046, 0.072, 0.13, 0.162],
    },
    {
        "name": "Ectocore clocked by Pams 2x at 120 BPM",
        "beats": [0, 480, 720, 960, 3600, 1920, 2880, 1440],
        "lag": [0, 0.026, 0.038, 0.064, 0.226, 0.116, 0.178, 0.076],
    },
    {
        "name": "Ectocore clocked internally at 171 bpm",
        "beats": [0, 128, 160, 352, 640, 1280, 1376],
        "lag": [0, -0.05495, -0.06085, -0.1447, -0.2662, -0.5495, -0.6014],
    },
    {
        "name": "Ectocore clocked internally at 171 bpm (no overclock)",
        "beats": [
            0,
            512,
            1024,
            2048,
            3072,
        ],
        "lag": [0, -0.0001571, -0.00033415, -0.00091625, -0.00137295],
    },
]

# Create a plotly figure plotting all the datasets and a trendline for each (trendline same color as dataset)
# on the same figure with labels

fig = go.Figure()

colors = [
    "red",
    "blue",
    "green",
    "purple",
    "orange",
    "yellow",
    "black",
    "pink",
    "brown",
    "gray",
]
for i, dataset in enumerate(datasets):

    fig.add_trace(
        go.Scatter(
            x=dataset["beats"],
            y=dataset["lag"],
            mode="markers",
            name=dataset["name"],
            marker=dict(color=colors[i]),
        )
    )
    m, b = np.polyfit(dataset["beats"], dataset["lag"], 1)
    fig.add_trace(
        go.Scatter(
            x=dataset["beats"],
            y=m * np.array(dataset["beats"]) + b,
            mode="lines",
            name=f"{dataset['name']} trendline",
            line=dict(color=colors[i], dash="dash"),
        )
    )

# y-label is lag time in BEATS
fig.update_yaxes(title_text="Lag time (beats)")
# x-label is beat number
fig.update_xaxes(title_text="Beat number")
fig.update_layout(title="Ectocore clocking lag")
fig.show()
