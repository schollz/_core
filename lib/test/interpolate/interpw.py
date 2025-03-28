import matplotlib.pyplot as plt
import numpy as np
from icecream import ic

xpoints = np.array([0,0, 1, 2, 3, 4, 5, 5])
y = np.array([ 3,3, 10, 30, 90, 89, 40, 40])
plt.plot(xpoints, y, "-o")


for i in range(5):
    c0 = y[i + 1]
    c1 = (y[i + 2] - y[i]) / 2
    c2 = y[i] - 5 * y[i + 1] / 2.0 + 2 * y[i + 2] - y[i + 3] / 2
    c3 = (y[i + 3] - y[i]) / 2 + 3 * (y[i + 1] - y[i + 2]) / 2.0
    yp = []
    xp = []
    for j in range(4):
        x = j / 4
        xp.append(x + i)
        yp.append(round(((c3 * x + c2) * x + c1) * x + c0))
    plt.plot(xp, yp, "-r")

plt.show()
