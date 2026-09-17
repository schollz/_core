#!/usr/bin/env python3
"""Render kiosk configuration; kept separate so setup can be tested off-device."""

import argparse
from pathlib import Path
import re
import shlex

BEGIN = "# BEGIN zeptocore visualizer kiosk"
END = "# END zeptocore visualizer kiosk"


def unit_quote(value, *, executable_argument=False):
    if any(ord(c) < 32 for c in value):
        raise ValueError("Configuration paths must not contain control characters")
    value = value.replace("%", "%%").replace("\\", "\\\\").replace('"', '\\"')
    if executable_argument:
        value = value.replace("$", "$$")
    return '"' + value + '"'


def service(app, reference, node, user, home, port):
    # WorkingDirectory is a scalar path, not an ExecStart word list: systemd
    # preserves spaces here but would interpret surrounding quotes literally.
    if any(ord(c) < 32 for c in app) or app != app.strip() or app.endswith("\\"):
        raise ValueError("Unsupported whitespace or trailing backslash in application path")
    args = [node, str(Path(app) / "dist/server.mjs"), reference, "--port", str(port)]
    return f"""[Unit]
Description=Visualizer Node Server
After=network.target
StartLimitIntervalSec=0

[Service]
Type=simple
User={user}
WorkingDirectory={app.replace('%', '%%')}
Environment={unit_quote('HOME=' + home)}
ExecStart={' '.join(unit_quote(arg, executable_argument=True) for arg in args)}
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
"""


def autostart(original, launcher, port):
    """Replace our block and migrate the simple Chromium launch documented in AGENTS.md."""
    if original.count(BEGIN) != original.count(END) or original.count(BEGIN) > 1:
        raise ValueError("Malformed kiosk markers in Labwc autostart; repair them first")
    if BEGIN in original:
        if original.index(END) < original.index(BEGIN):
            raise ValueError("Reversed kiosk markers in Labwc autostart")
        original = re.sub(re.escape(BEGIN) + r".*?" + re.escape(END) + r"\n?",
                          "", original, flags=re.S)
    lines = original.splitlines(keepends=True)
    result = []
    index = 0
    while index < len(lines):
        logical = lines[index]
        index += 1
        while logical.rstrip("\n").endswith("\\") and index < len(lines):
            logical += lines[index]
            index += 1
        flat = logical.replace("\\\n", " ")
        # Other desktop startup code can contain multiline quoted strings; only
        # parse commands that could be the old visualizer kiosk invocation.
        if "--kiosk" not in flat or not re.search(r"http://(?:localhost|127\.0\.0\.1):", flat):
            result.append(logical)
            continue
        lexer = shlex.shlex(flat, posix=True, punctuation_chars=True)
        lexer.whitespace_split = True
        tokens = list(lexer)
        urls = {f"http://{host}:{p}{suffix}"
                for host in ("localhost", "127.0.0.1")
                for p in (4173, port) for suffix in ("", "/")}
        if "--kiosk" in tokens and urls.intersection(tokens):
            command = tokens[:]
            if (len(command) >= 3 and command[0] == "sleep"
                    and re.fullmatch(r"[0-9.]+", command[1]) and command[2] == "&&"):
                command = command[3:]
            if command and command[-1] == "&":
                command.pop()
            # Only migrate a standalone launch, never an arbitrary shell compound.
            if (not command or Path(command[0]).name not in ("chromium", "chromium-browser")
                    or re.search(r"[;&|<>`$]", " ".join(command))):
                raise ValueError("Custom kiosk command in Labwc autostart: remove it manually first")
            result.append("# Previous Chromium kiosk launch replaced by managed block below.\n")
        else:
            result.append(logical)
    return "".join(result).rstrip() + f"\n\n{BEGIN}\n{shlex.quote(launcher)} &\n{END}\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("app", "reference", "node", "user", "home", "browser", "output", "autostart"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--port", type=int, required=True)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535 or not re.fullmatch(r"[a-z_][a-z0-9_-]*[$]?", args.user):
        parser.error("Invalid port or user")
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    home = Path(args.home)
    launcher = str(home / ".local/bin/zeptocore-visualizer-kiosk")
    source = Path(args.autostart)
    updated = autostart(source.read_text() if source.exists() else "", launcher, args.port)
    (output / "visualizer.service").write_text(service(
        args.app, args.reference, args.node, args.user, args.home, args.port))
    (output / "autostart").write_text(updated)
    config = {
        "KIOSK_BROWSER": args.browser,
        "KIOSK_URL": f"http://localhost:{args.port}/",
        "KIOSK_PROFILE": str(home / ".config/zeptocore-visualizer/chromium"),
        "KIOSK_STATE": str(home / ".local/state/zeptocore-visualizer"),
    }
    (output / "kiosk.conf").write_text("# Generated by setup-kiosk.sh\n" + "".join(
        f"{key}={shlex.quote(value)}\n" for key, value in config.items()))


if __name__ == "__main__":
    main()
