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
# plot with three subplots
# subplot 1 is the main figure
fig, axs = plt.subplots(3, 1, figsize=(12, 18))


axs[0].plot(downsampled_time, downsampled_data, color="black")
axs[0].set_title("Waveform of {}".format(file_path))
axs[0].label_outer()
axs[0].set_ylabel("Amplitude")
axs[0].grid()


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
            x = sample / 44100
            # draw a vertical bar
            axs[0].axvline(x=x, color=color, linestyle="--")


# plot bombo.wav
filename_noext = os.path.splitext(dataj["Filename"])[0]
fnames = ["bombo.wav", "redoblante.wav"]
names = ["kicks", "snares"]
for i, fname in enumerate(fnames):
    file_path = (
        "drum_separation_model_output/modelo_final/" + filename_noext + "/" + fname
    )
    print(file_path)

    # If filename ends in .flac, convert to wav using sox
    if file_path.endswith("flac"):
        os.system(f"sox {file_path} {file_path}.wav")
        file_path += ".wav"

    # Read the .wav file
    sample_rate, data = wavfile.read(file_path)

    # Only use the first channel
    if len(data.shape) > 1:
        data = data[:, 0]

    # Normalize to 1
    data = data / np.max(data)

    # Downsample the data for plotting
    downsampled_data = data[::downsample_factor]
    downsampled_time = np.arange(len(downsampled_data)) / (
        sample_rate / downsample_factor
    )

    # Plot in axs[1]
    axs[i + 1].plot(downsampled_time, downsampled_data, color="black")
    axs[i + 1].set_title(names[i])
    axs[i + 1].set_ylabel("Amplitude")
    axs[i + 1].grid()
    # only show x-label on last
    if i == 1:
        axs[i + 1].set_xlabel("Time [s]")
    else:
        axs[i + 1].label_outer()

    # plot the envelope in red
    env_data = json.load(open(file_path + ".envelope", "r"))
    time_data = json.load(open(file_path + ".timeaxis", "r"))
    axs[i + 1].plot(time_data, env_data, color="red")

    # load peaks and plot them as blue vertical lines
    peaks = json.load(open(file_path + ".peaks", "r"))
    for peak in peaks:
        axs[i + 1].axvline(x=peak, color="blue", linestyle="--")


plt.show()
