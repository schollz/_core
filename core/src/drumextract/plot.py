import sys

import matplotlib.pyplot as plt


def plot_data(data):
    # parse the data
    lines = data.split("\n")
    points = []
    peaks = []
    get_peaks = False
    for _, line in enumerate(lines):
        line = line.strip()
        if line == "peaks":
            get_peaks = True
            continue
        if get_peaks:
            try:
                peaks.append(int(line))
            except:
                continue
        else:
            try:
                points.append(float(line))
            except:
                continue

    print(points, peaks)
    # plot the points as a line graph
    fig, ax = plt.subplots()
    ax.plot(points)
    for peak in peaks:
        ax.axvline(x=peak, color="r", linestyle="--")
    plt.show()


# get data from stdin
data = ""
for line in sys.stdin:
    data += line

print(data)
plot_data(data)
