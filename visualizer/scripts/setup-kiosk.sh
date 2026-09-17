#!/usr/bin/env bash
# Run as the Raspberry Pi desktop user. Only system changes use sudo.
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: ./scripts/setup-kiosk.sh [options]
  --reference PATH  SD-card copy (default: reference under this application)
  --port PORT       HTTP port (default: 4173)
  --skip-build      Use the existing dist/ instead of npm ci && npm run build
  --skip-packages   Do not install Chromium/curl/Python/X11 utilities
  --no-autologin    Keep the existing desktop login configuration
  --help           Show this help

Run as zns (without sudo) in a terminal or over SSH to the Pi.
Requires Raspberry Pi OS Desktop with a running Labwc session and Node >=22.12.
Configures the server service, desktop autologin, display blanking, and Chromium.
Reboot once when setup finishes; the script never reboots automatically.
EOF
}
die() { echo "Error: $*" >&2; exit 1; }
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
app_dir=$(cd -- "$script_dir/.." && pwd)
reference="$app_dir/reference"
port=4173
build=1
packages=1
autologin=1
while [[ $# -gt 0 ]]; do
    case "$1" in
        --reference|--port)
            [[ $# -ge 2 ]] || die "$1 needs a value"
            if [[ "$1" == --reference ]]; then reference=$2; else port=$2; fi
            shift 2 ;;
        --skip-build) build=0; shift ;;
        --skip-packages) packages=0; shift ;;
        --no-autologin) autologin=0; shift ;;
        --help|-h) usage; exit 0 ;;
        *) die "Unknown option: $1 (see --help)" ;;
    esac
done
[[ $(uname -s) == Linux ]] || die "Run this installer on the Raspberry Pi, not this computer."
[[ $EUID -ne 0 ]] || die "Run as the desktop user without sudo (so nvm and HOME are correct)."
[[ "$port" =~ ^[0-9]{1,5}$ ]] || die "Port must be an integer from 1 to 65535"
port=$((10#$port))
((port >= 1 && port <= 65535)) || die "Port must be from 1 to 65535"
[[ -d "$reference" ]] || die "Missing SD-card copy: $reference (use --reference PATH)"
reference=$(cd -- "$reference" && pwd)
for tool in node sudo systemctl raspi-config labwc pgrep; do
    command -v "$tool" >/dev/null || die "Missing $tool; see the Raspberry Pi kiosk section of AGENTS.md."
done
pgrep -u "$EUID" -x labwc >/dev/null || die "Log into the Labwc desktop once first; setup may then run over SSH."
node_bin=$(command -v node)
[[ "$node_bin" == /* && -x "$node_bin" ]] || die "node must resolve to an executable absolute path"
"$node_bin" -e 'const [major, minor] = process.versions.node.split(".").map(Number); process.exit(major > 22 || (major === 22 && minor >= 12) ? 0 : 1)' \
    || die "Node 22.12+ is required; activate your nvm version and rerun without sudo."
if ((build)); then
    command -v npm >/dev/null || die "npm is missing; activate your Node installation or use --skip-build."
    (cd "$app_dir" && npm ci && npm run build)
fi
[[ -f "$app_dir/dist/server.mjs" && -f "$app_dir/dist/index.html" && -d "$app_dir/dist/assets" ]] \
    || die "Missing built app; run npm ci && npm run build, or omit --skip-build."

sudo -v
if ((packages)); then
    sudo apt-get update
    sudo apt-get install -y chromium curl python3 x11-xserver-utils util-linux
fi
for tool in python3 curl flock systemd-analyze; do
    command -v "$tool" >/dev/null || die "Missing $tool; rerun without --skip-packages."
done
browser=$(command -v chromium || command -v chromium-browser || true)
[[ -n "$browser" ]] || die "Install Chromium or rerun without --skip-packages."
desktop_user=$(id -un)
config_dir="$HOME/.config/zeptocore-visualizer"
state_dir="$HOME/.local/state/zeptocore-visualizer"
autostart="$HOME/.config/labwc/autostart"
mkdir -p "$config_dir" "$state_dir/setup-backups" "$HOME/.local/bin" "$(dirname "$autostart")"
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
render() {
    python3 "$script_dir/kiosk-config.py" --app "$app_dir" --reference "$reference" \
        --node "$node_bin" --user "$desktop_user" --home "$HOME" --browser "$browser" \
        --port "$port" --autostart "$autostart" --output "$scratch"
}
render
systemd-analyze verify "$scratch/visualizer.service"
backup=$(mktemp -d "$state_dir/setup-backups/$(date +%Y%m%d-%H%M%S).XXXXXX")
[[ ! -f "$autostart" ]] || cp -p "$autostart" "$backup/autostart"
if sudo test -f /etc/systemd/system/visualizer.service; then
    sudo cat /etc/systemd/system/visualizer.service >"$backup/visualizer.service"
fi
[[ ! -f "$config_dir/kiosk.conf" ]] || cp -p "$config_dir/kiosk.conf" "$backup/kiosk.conf"

# The current desktop user is sudo's SUDO_USER, which raspi-config uses.
if ((autologin)); then sudo raspi-config nonint do_boot_behaviour B4; fi
# 1 means DISABLE. On Labwc this removes swayidle startup entries; xset alone
# would only affect X11/XWayland. A reboot ends any already-running idle daemon.
sudo raspi-config nonint do_blanking 1
# Read again because raspi-config may have edited this same autostart file.
render
install -m 755 "$script_dir/kiosk-launch.sh" "$HOME/.local/bin/zeptocore-visualizer-kiosk"
install -m 600 "$scratch/kiosk.conf" "$config_dir/kiosk.conf"
install -m 644 "$scratch/autostart" "$autostart"
sudo install -m 644 "$scratch/visualizer.service" /etc/systemd/system/visualizer.service
sudo systemctl daemon-reload
sudo systemctl enable visualizer.service
sudo systemctl restart visualizer.service

cat <<EOF

Kiosk installed for $desktop_user.
Node:      $node_bin
Reference: $reference
URL:       http://localhost:$port/
Backups:   $backup

The server is starting; the first reference analysis can take several minutes.
Watch progress: journalctl -u visualizer.service -f
Reboot once:    sudo reboot

On first launch, click CONNECT and allow MIDI including SysEx.
Chromium remembers that permission in its dedicated kiosk profile.
EOF
