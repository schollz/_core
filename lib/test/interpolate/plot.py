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
    """Plots the data, y against x.

    Args:
      data: A numpy array containing the data, with the first column being the x-values and the second column being the y-values.
    """

    c = ["gray", "red", "blue", "green", "yellow"]
    interp = ["original", "linear", "quadratic", "linear2", "hermite"]
    for i, data in enumerate(datas):
        x = data[:, 0]
        y = data[:, 1]
        if i == 0:
            plt.plot(x, y, marker=".", color=c[i], label=interp[i], linewidth=2)
        else:
            plt.plot(x, y, color=c[i], label=interp[i], linewidth=2)

    plt.xlabel("x")
    plt.ylabel("y")
    plt.title("Interpolation")
    plt.legend()
    plt.show()


if __name__ == "__main__":
    filename = "data.txt"

    datas = read_data_from_stdin()
    plot_datas(datas)
