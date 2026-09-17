# zeptocore visualizer

Live waveform, spectrum, and pad visualizations. Requires Node.js 22.12+, Chrome
or Chromium, and a zeptocore with visualizer-enabled firmware.

```sh
cd visualizer
make serve REFERENCE=/path/to/sd-card-copy
```

Open [localhost:4173](http://localhost:4173/), click **CONNECT**, and allow MIDI/SysEx.

**Raspberry Pi kiosk:** with Labwc running, an SD-card copy in `reference/`, and
logged in as `zns`, run without sudo:

```sh
cd /home/zns/_core/visualizer
./scripts/setup-kiosk.sh
sudo reboot
```

See [AGENTS.md](AGENTS.md) for firmware, kiosk setup, troubleshooting, and development details.
