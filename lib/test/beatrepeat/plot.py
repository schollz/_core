import sys

import matplotlib.pyplot as plt
import numpy as np


def read_points():
    points = []
    for line in sys.stdin:
        if line.strip():
            try:
                points.append(float(line.strip()))
            except ValueError:
                print("Invalid input. Please enter points in the format: x y")
        else:
            break
    return points


def plot_points(points):
    if not points:
        print("No points to plot.")
        return

    x_values = [i for i in range(len(points))]
    x_values = np.multiply(x_values, 1 / 44100)
    y_values = points
    # make the figure wide
    plt.figure(figsize=(18, 5))
    plt.plot(x_values, y_values, marker=None)  # Line graph with points marked
    plt.xlabel("seconds")
    plt.ylabel("output")
    plt.grid(True)
    plt.show()


def main():
    print("Enter your points (x y). Press Enter on an empty line to finish:")
    points = read_points()
    plot_points(points)


if __name__ == "__main__":
    main()
