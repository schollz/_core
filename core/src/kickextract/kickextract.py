import subprocess
import re
import os
import math
import json
import glob
import traceback

from icecream import ic
import numpy as np
import librosa
from tqdm import tqdm
import click

doplot = True
if not doplot:
    ic.disable()


def gather2(fname):
    ic(fname)
    data = subprocess.check_output(
        ["sox", fname, "-n", "stat", "-freq"], stderr=subprocess.STDOUT
    )
    vals = dict()
    for i in range(435):
        vals[i] = []
    for line in data.split(b"\n"):
        foo = line.decode().split()
        if len(foo) != 2:
            continue
        try:
            freq = int(round(math.log10(float(foo[0])) * 100))
            power = float(foo[1])
            vals[freq].append(power)
        except:
            pass

    data = []
    for i in range(435):
        if len(vals[i]) == 0:
            data.append(0)
        else:
            data.append(np.mean(vals[i]))
    return data


def analyze(fname):
    ic(fname)
    duration = librosa.get_duration(filename=fname)
    slice_start = []
    slice_stop = []
    for i in range(16):
        slice_start.append(duration / 16 * i)
        slice_stop.append(duration / 16 * (i + 1))

    for i in range(16):
        ic(slice_start[i], slice_stop[i])
        os.system(
            f"sox {fname} output{i+1:03}.wav trim {slice_start[i]} {slice_stop[i]-slice_start[i]} 2>/dev/null"
        )

    if doplot:
        import matplotlib.pyplot as plt

    legend = []
    ys = []
    for i in range(len(slice_start)):
        legend.append(str(i + 1))
        y = gather2(f"output{i+1:03}.wav")
        x = np.arange(len(y))
        # ys.append(np.sum(y[:204]))
        ys.append(np.sum(y[250:300]))
        ic(ys)
        if doplot:
            plt.plot(x, y)

    max_val = np.amax(ys)
    kick = []
    for v in ys:
        if v > max_val / 2:
            p = v / max_val * 0.5
            kick.append(p)
        else:
            kick.append(0)
    kick = librosa.amplitude_to_db(kick)
    with open(fname + ".json", "w") as f:
        f.write(json.dumps(list(kick)))

    print(ys)

    if doplot:
        plt.legend(legend, loc="upper right")
        plt.savefig("test.png")
        plt.close("all")
        plt.plot(np.arange(len(ys)), ys)
        plt.savefig("test2.png")


@click.command()
@click.option("--folder", help="folder to analyze", required=True)
def hello(folder):
    for fname in tqdm(list(glob.glob(folder + "/*.flac"))):
        try:
            analyze(fname)
        except Exception as e:
            traceback.print_exc()
            os.remove(fname)


if __name__ == "__main__":
    hello()
