#!/usr/bin/env bash
# Installed into ~/.local/bin and started by Labwc as the desktop user.
set -euo pipefail

config="$HOME/.config/zeptocore-visualizer/kiosk.conf"
if [[ ! -r "$config" ]]; then
    echo "Missing $config; run scripts/setup-kiosk.sh first." >&2
    exit 1
fi
# shellcheck source=/dev/null
source "$config"
: "${KIOSK_BROWSER:?}" "${KIOSK_URL:?}" "${KIOSK_PROFILE:?}" "${KIOSK_STATE:?}"
mkdir -p "$KIOSK_STATE" "$KIOSK_PROFILE"
exec 9>"$KIOSK_STATE/kiosk.lock"
flock -n 9 || exit 0
if [[ -z "${WAYLAND_DISPLAY:-}" && -z "${DISPLAY:-}" ]]; then
    echo "Launch from the desktop session, not a plain SSH shell." >&2
    exit 1
fi

# Keep the latest two session logs; Chromium can be noisy on long-running kiosks.
rotate_log() {
    [[ ! -f "$KIOSK_STATE/kiosk.log" ]] || mv -f "$KIOSK_STATE/kiosk.log" "$KIOSK_STATE/kiosk.log.previous"
    exec >"$KIOSK_STATE/kiosk.log" 2>&1
}
rotate_log
echo "$$" >"$KIOSK_STATE/kiosk.pid"
child=
cleanup() {
    trap - EXIT TERM INT HUP
    if [[ -n "$child" ]]; then
        kill "$child" 2>/dev/null || true
        wait "$child" 2>/dev/null || true
    fi
    rm -f "$KIOSK_STATE/kiosk.pid"
}
trap cleanup EXIT
trap 'exit 0' TERM INT HUP

echo "Starting visualizer kiosk at $KIOSK_URL"
# Wayland blanking is disabled at setup through raspi-config. xset only applies
# to an actual X11 session; setting it on XWayland cannot keep Labwc awake.
if [[ -z "${WAYLAND_DISPLAY:-}" ]] && command -v xset >/dev/null; then
    xset s off || true
    xset s noblank || true
    xset -dpms || true
fi
browser_args=(--kiosk --noerrdialogs --disable-infobars --no-first-run
    --disable-session-crashed-bubble --user-data-dir="$KIOSK_PROFILE")
if [[ -n "${WAYLAND_DISPLAY:-}" ]]; then
    browser_args+=(--ozone-platform=wayland)
fi

while true; do
    echo "Waiting for the reference library and HTTP server…"
    until curl --noproxy '*' --fail --silent --output /dev/null --max-time 3 "$KIOSK_URL"; do
        sleep 2 &
        child=$!
        wait "$child" || true
        child=
    done
    echo "Server ready; launching Chromium."
    "$KIOSK_BROWSER" "${browser_args[@]}" "$KIOSK_URL" 9>&- &
    child=$!
    wait "$child" || true
    child=
    echo "Chromium exited; restarting in two seconds."
    sleep 2 &
    child=$!
    wait "$child" || true
    child=
    if [[ $(wc -c <"$KIOSK_STATE/kiosk.log") -gt 10485760 ]]; then
        rotate_log
    fi
done
