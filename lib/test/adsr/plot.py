import sys

import matplotlib.pyplot as plt
import numpy as np


def read_points():
    xs = []
    ys = []
    for line in sys.stdin:
        if line.strip():
            try:
                xs.append(float(line.split()[0]))
                ys.append(float(line.split()[1]))
            except ValueError:
                print("Invalid input. Please enter points in the format: x y")
        else:
            break
    return xs, ys


def plot_points(xs, ys):
    # make the figure wide
    plt.figure(figsize=(18, 5))
    plt.plot(xs, ys, marker=None)  # Line graph with points marked
    plt.xlabel("seconds")
    plt.ylabel("output")
    plt.grid(True)
    plt.show()


def main():
    xs, ys = read_points()
    plot_points(xs, ys)


if __name__ == "__main__":
    main()
