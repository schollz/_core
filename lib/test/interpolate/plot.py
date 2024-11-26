import sys
import matplotlib.pyplot as plt
import numpy as np


def read_data_from_stdin():
    """Reads data from stdin in the format "x,y" where each "x,y" pair is on a new line.

    Returns:
      A numpy array containing the data, with the first column being the x-values and the second column being the y-values.
    """

    datas = []
    data = []
    last_x = 0.0
    for line in sys.stdin:
        x, y = line.strip().split(",")
        x = float(x)
        if x < last_x:
            print("ok", x, last_x)
            datas.append(np.array(data))
            data = []
        last_x = x
        data.append([float(x), float(y)])
    datas.append(np.array(data))

    return datas


def plot_datas(datas):
    """Plots the data, y against x, with two separate subplots.

    Args:
      datas: A list of numpy arrays containing the data,
             where each array has the first column as x-values and the second as y-values.
    """

    # Create two subplots
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8, 10))

    c = ["gray", "red", "blue", "green", "yellow"]
    interp = ["original", "linear(lookahead)", "linear(old)", "linear2", "hermite"]
    marker_size = 4
    xdatas = []
    ydatas = []

    # Plot each dataset in the first subplot
    for i, data in enumerate(datas):
        x = data[:, 0]
        y = data[:, 1]
        # Normalize x to [0, 1]
        x = (x - x.min()) / (x.max() - x.min())
        # Interpolate to 500 points
        x500 = np.linspace(0, 1, 500)
        y500 = np.interp(x500, x, y)
        xdatas.append(x500)
        ydatas.append(y500)
        ax1.plot(x, y, label=interp[i], color=c[i], marker="o", markersize=marker_size)

    # Plot the difference between the first and second datasets on the second subplot
    if len(datas) > 1:
        ax2.plot(
            xdatas[0],
            ydatas[0] - ydatas[1],
            label="diff(0, 1)",
            color="red",
            marker="o",
            markersize=marker_size,
        )
        ax2.plot(
            xdatas[0],
            ydatas[0] - ydatas[2],
            label="diff(0, 1)",
            color="blue",
            marker="o",
            markersize=marker_size,
        )

    # Customize first subplot
    ax1.set_xlabel("x")
    ax1.set_ylabel("y")
    ax1.set_title("Interpolation")
    ax1.legend()

    # Customize second subplot
    ax2.set_xlabel("x")
    ax2.set_ylabel("Difference")
    ax2.set_title("Difference Between First Two Datasets")
    ax2.legend()

    # Adjust layout and show the plot
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    filename = "data.txt"

    datas = read_data_from_stdin()
    plot_datas(datas)
