from scipy.io import wavfile
import numpy as np
import matplotlib.pyplot as plt

import os
import json

jsonData = open("transients.json").read()
dataj = json.loads(jsonData)

file_path = dataj["Filename"]
downsample_factor = 80

# if filename ends in .flac, convert to wav using sox
if file_path.endswith("flac"):
    os.system(f"sox {file_path} {file_path}.wav")
    file_path += ".wav"

# Read the .wav file
sample_rate, data = wavfile.read(file_path)

# only use the first channel
if len(data.shape) > 1:
    data = data[:, 0]

# normalize to 1
data = data / np.max(data)

# Downsample the data for plotting
downsampled_data = data[::downsample_factor]
downsampled_time = np.arange(len(downsampled_data)) / (sample_rate / downsample_factor)

# Plot the downsampled data
plt.figure(figsize=(12, 6))
plt.plot(downsampled_time, downsampled_data, color="black")
plt.title("Waveform of {}".format(file_path))
plt.xlabel("Time [s]")
plt.ylabel("Amplitude")
plt.grid()


# plot all the AllTransients
do_plot = [
    ("AllTransients", "gray"),
    ("SnareTransients", "blue"),
    ("KickTransients", "red"),
    # ("OtherTransients", "green"),
]

for key, color in do_plot:
    for sample in dataj[key]:
        if sample > 0 and sample < len(data):
            x = sample / sample_rate
            # draw a vertical bar
            plt.axvline(x=x, color=color, linestyle="--")

plt.show()
