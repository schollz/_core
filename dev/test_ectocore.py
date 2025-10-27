#!/usr/bin/env -S uv --quiet run --script
# /// script
# requires-python = ">=3.13"
# dependencies = [
#     "pyserial",
#     "plotly",
#     "scipy",
#     "colorlog",
# ]
# ///

import time, numpy as np, plotly.graph_objects as go
from plotly.subplots import make_subplots
from scipy.stats import linregress
import serial, serial.tools.list_ports, threading, re, logging, colorlog

handler = colorlog.StreamHandler()
handler.setFormatter(
    colorlog.ColoredFormatter(
        "%(log_color)s[%(levelname)s]%(reset)s %(message)s",
        log_colors={
            "DEBUG": "cyan",
            "INFO": "green",
            "WARNING": "yellow",
            "ERROR": "red",
            "CRITICAL": "red,bg_white",
        },
    )
)
log = logging.getLogger("crow_ecto")
log.setLevel(logging.DEBUG)
log.addHandler(handler)


class Crow:
    def __init__(self):
        self.serial = None

    def connect(self):
        for p in serial.tools.list_ports.comports():
            if "crow" in p.description.lower() or "monome" in p.description.lower():
                self.serial = serial.Serial(p.device, 115200, timeout=1)
                log.info(f"Connected to Crow at {self.serial.port}")
                break
        if not self.serial:
            log.error("Could not find Crow device")
            return
        time.sleep(2)

    def send(self, cmd):
        if self.serial:
            log.debug(f"→ Crow: {cmd}")
            self.serial.write((cmd + "\r\n").encode())

    def set(self, v1, v2, v3, v4):
        for i, v in enumerate([v1, v2, v3, v4], 1):
            self.send(f"output[{i}].volts = {v}")

    def close(self):
        if self.serial:
            log.info("Closed Crow connection")
            self.serial.close()


class Ecto:
    def __init__(self):
        self.serial = None
        self.cv = [0, 0, 0, 0]
        self.running = False

    def connect(self):
        for p in serial.tools.list_ports.comports():
            if (
                "pico" in p.description.lower()
                or "ecto" in p.description.lower()
                or "rp2040" in p.description.lower()
            ):
                self.serial = serial.Serial(p.device, 115200, timeout=1)
                log.info(f"Connected to Ectocore at {self.serial.port}")
                break
        if not self.serial:
            log.error("Could not find Ectocore device")
            return
        time.sleep(2)
        self.running = True
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        pat = re.compile(r"cv(\d)=([-+]?\d*\.?\d+)")
        while self.running:
            if self.serial is None or self.serial.fd is None:
                break
            try:
                line = self.serial.readline().decode(errors="ignore").strip()
            except (OSError, TypeError):
                break
            if not line:
                continue
            for m in pat.finditer(line):
                i, val = int(m[1]) - 1, float(m[2])
                self.cv[i] = val
            log.debug(f"← Ecto: {line}")

    def close(self):
        self.running = False
        if self.serial:
            log.info("Closed Ecto connection")
            self.serial.close()


def main():
    crow = Crow()
    ecto = Ecto()
    crow.connect()
    log.warning("Make sure to press MODE + BANK to initiate diagnostic mode")
    input("Press Enter to continue...")
    ecto.connect()

    # Verify CV monitoring is active
    log.info("Checking if CV monitoring is active...")
    time.sleep(0.5)
    if all(cv == 0 for cv in ecto.cv):
        log.error("No CV data received from Ectocore!")
        log.error("Did you press MODE + BANK buttons to enable CV monitoring?")
        crow.close()
        ecto.close()
        return
    log.info("CV monitoring active, starting calibration...")

    xs = np.linspace(-5, 5, 51)
    ys = [[], [], [], []]
    for v in xs:
        crow.set(v, v, v, v)
        log.info(f"Set all Crow outputs to {v:+.2f}V")
        time.sleep(0.2)  # Wait 200ms for readings to stabilize
        for i in range(4):
            ys[i].append(ecto.cv[i])
        log.info(f"Ectocore readback: {[round(c,3) for c in ecto.cv]}")
    crow.close()
    ecto.close()

    # Calculate min/max for each channel for subplot titles
    subplot_titles = [
        f"CV{i+1} (range: {min(ys[i]):.1f} to {max(ys[i]):.1f})" for i in range(4)
    ]

    fig = make_subplots(rows=2, cols=2, subplot_titles=subplot_titles)
    for i in range(4):
        r, c = divmod(i, 2)
        fig.add_scatter(
            x=xs, y=ys[i], mode="markers", name="Data", row=r + 1, col=c + 1
        )
        slope, intercept, r_value, *_ = linregress(xs, ys[i])
        r_squared = r_value**2
        fit = slope * xs + intercept
        fig.add_scatter(
            x=xs,
            y=fit,
            mode="lines",
            line=dict(color="red", dash="dot"),
            name=f"Fit: R²={r_squared:.4f}, slope={slope:.2f}",
            row=r + 1,
            col=c + 1,
        )
        # Update axes labels for each subplot
        fig.update_xaxes(title_text="Crow (Volts)", row=r + 1, col=c + 1)
        fig.update_yaxes(title_text="Ectocore reading", row=r + 1, col=c + 1)

    fig.update_layout(title="Crow → Ectocore Calibration", showlegend=True, height=800)
    fig.show()


if __name__ == "__main__":
    main()
