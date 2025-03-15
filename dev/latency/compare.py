#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = [
#     "numpy",
#     "soundfile",
#     "scipy",
#     "plotly",
#     "pandas",
# ]
# ///

import numpy as np
import pandas as pd
import plotly.express as px
import soundfile as sf  # Replaces librosa
from scipy.signal import find_peaks

file_paths = ["v6.3.5", "v6.3.6_lowlatency", "v6.3.7"]
df_data = []

for file_path in file_paths:
    audio_data, sr = sf.read(file_path + ".ogg")  # Read with SoundFile

    # Ensure the audio is stereo
    if audio_data.ndim != 2 or audio_data.shape[1] != 2:
        raise ValueError("Audio file must be stereo (2 channels).")

    # Separate left and right channels
    left_channel = np.abs(audio_data[:, 0])
    right_channel = np.abs(audio_data[:, 1])

    # Time array
    time = np.arange(len(left_channel)) / sr

    # Find peaks with a minimum distance of 0.1 seconds and minimum amplitude of 0.25
    min_distance = int(0.1 * sr)
    min_amplitude = 0.25  # Minimum required amplitude to be a valid peak

    # Detect peaks
    peaks_left, _ = find_peaks(
        left_channel, distance=min_distance, height=min_amplitude
    )
    peaks_right, _ = find_peaks(
        right_channel, distance=min_distance, height=min_amplitude
    )

    # Find closest peak in left channel for each right channel peak
    time_left_peaks = time[peaks_left]
    time_right_peaks = time[peaks_right]

    for right_peak in time_right_peaks:
        closest_left_peak = min(time_left_peaks, key=lambda t: abs(t - right_peak))
        time_diff_ms = (right_peak - closest_left_peak) * 1000
        # remove peaks that are 2 std from mean
        df_data.append({"File": file_path, "Time Difference (ms)": time_diff_ms})


# Convert to DataFrame
df = pd.DataFrame(df_data)

# for each file, remove outliers
for file_path in file_paths:
    file_df = df[df["File"] == file_path]
    mean = file_df["Time Difference (ms)"].mean()
    std = file_df["Time Difference (ms)"].std()
    df = df[
        ~((df["File"] == file_path) & (df["Time Difference (ms)"] > mean + 1 * std))
    ]
    df = df[
        ~((df["File"] == file_path) & (df["Time Difference (ms)"] < mean - 1 * std))
    ]

# Create box plot
fig = px.box(
    df,
    x="File",
    y="Time Difference (ms)",
    title="Ectocore latency when externally clocking (140 bpm)",
    color_discrete_sequence=["black"],
    labels={
        "Time Difference (ms)": "Latency (ms)",
        "File": "Ectocore firmware version",
    },
)

# Add a line at 0 and label it "zero latency"
fig.add_shape(
    type="line",
    x0=-0.5,  # Extend slightly before the first box
    x1=len(file_paths) - 0.5,  # Extend slightly after the last box
    y0=0,
    y1=0,
    line=dict(color="red", dash="dash"),
)
fig.add_annotation(
    x=0,  # Place near the last box
    y=0,
    text="Zero Latency",
    showarrow=False,
    font=dict(color="red"),
    xanchor="left",
    yanchor="bottom",
)

fig.show()
