# Native Rack plugin releases

These scripts follow the sample manager's native release layout: Apple Silicon
builds here, Intel builds over SSH on the Intel Mac, and Linux builds directly on
Linux x86_64. Windows remains in the manual `release-rack-windows.yml` workflow.

Each run selects the latest published stable GitHub release and pins the current
`main` commit of `schollz/_core`. A fresh HTTPS clone supplies all build inputs,
including the SHA-256-checked Rack 2.6.6 SDK downloader and source packager.
Local uncommitted changes and existing build/SDK directories are not used.
The scripts you invoke supply release orchestration and ZIP verification.

The plugin version comes from `rack/plugin.json` (currently `2.0.0`), independently
of the GitHub release tag (for example `v8.0.3`), just like the Windows workflow.
Rack 2 plugins must keep their `2.x` compatibility version. The scripts create
neither tags nor releases. Immediately before upload they require the selected
release to remain latest, stable, and mutable; a change stops publication and
retains the package. Advancing `main` during a build does not change its source.

## Prerequisites

Use Python 3.10+, Git, authenticated GitHub CLI (`gh`), and zstd on the machine
running the script. Build machines also need NumPy for that Python, GNU Make,
Clang with JSON AST support, jq, and a native C/C++17 compiler. Prerequisites are
checked; the scripts do not install packages. HTTPS access to GitHub and the
official VCV SDK download is required.

On macOS, install Xcode command-line tools, jq, zstd and NumPy. The scripts use
Clang, `codesign`, `otool`, `install_name_tool`, and rsync. macOS builds retain
the Makefile's 11.0 deployment minimum. The Rack SDK signs the packaged dylib
ad hoc and fixes its library reference to `/tmp/Rack2/libRack.dylib`; the release
code verifies the resulting signature and library path. These are Rack plugin
archives, with no Developer ID signing, notarization, or stapling. Apple
credentials and notary profiles are not needed.

On Debian/Ubuntu Linux x86_64, a conservative build baseline such as Ubuntu
22.04 is recommended. For example:

```sh
sudo apt-get install build-essential clang make jq zstd binutils python3 python3-numpy git
# Install/authenticate gh separately if it is not already available.
```

The Linux SDK links the C++ runtime statically and uses Rack's installed runtime
library. Rack and the host's glibc remain runtime requirements. This is a plugin,
so the sample manager's standalone launcher and library-bundling logic do not apply.

## Apple Silicon

Run from the repository root on an Apple Silicon Mac with native ARM64 Python:

```sh
python3 rack/Release/release-macosarm.py
# Equivalent:
make rack-release-macosarm
```

## Intel Mac

Run from the controller Mac. The default SSH host matches the sample manager:

```sh
python3 rack/Release/release-macos11.py
python3 rack/Release/release-macos11.py user@intel-mac --jobs 3
# Equivalent:
make rack-release-macos11 MACOS_RELEASE_HOST=user@intel-mac
```

The Intel host defaults to `zns@192.168.0.44`. It needs noninteractive SSH, native
x86_64 Python 3.10+ with NumPy, and the macOS build prerequisites above available
under `/usr/local/bin`, `/opt/homebrew/bin`, or the system paths. The controller
also needs SSH and rsync. SSH agent forwarding is disabled, and local Apple and
GitHub credential environment variables are stripped from build/transport
subprocesses.

The controller and Intel host independently clone and verify the same pinned
commit. The Intel host generates its own target-specific DSP sources, builds
and packages the dylib, and returns only the `.vcvplugin` and source archive.
The controller checks the returned x86_64 binary and signature, resources,
manifest, and source hashes against its own clone before publishing with its
local `gh` authentication. Remote temporary directories are removed only after
success; failures print and retain their paths. Use `--keep-remote` to retain a
successful remote build too.

## Linux x86_64

Run directly on Linux x86_64:

```sh
python3 rack/Release/release-linux.py
# Equivalent:
make rack-release-linux
```

## Options and results

All entrypoints accept:

- `--jobs N`: compiler parallelism, default 6 (1–64).
- `--output PATH`: parent for a new, unique run directory; default
  `rack/dist/releases/` next to the invoking tooling.
- `--no-upload`: build and fully package locally without publishing.

The same targets work inside `rack/` without the `rack-` prefix. They do not
require an existing local SDK. Pass options through Make with `RELEASE_ARGS`:

```sh
make rack-release-macosarm RELEASE_ARGS='--jobs 4 --no-upload'
```

Each platform uploads exactly two assets, matching the Windows naming scheme:

```text
InfiniteDigits-2.0.0-PLATFORM.zip
InfiniteDigits-2.0.0-PLATFORM-SHA256SUMS.txt
```

Platforms are `mac-arm64`, `mac-x64`, and `lin-x64`. Each ZIP contains the
`.vcvplugin`, matching source tarball (with `SOURCE_SHA256.json`), `INSTALL.txt`,
`BUILD.json`, and internal `SHA256SUMS.txt`. Build metadata records the plugin
version, platform, source commit, build host, signing mode, and selected release.
The plugin contains its resources and license notices.

Reruns replace only those two exact asset names for the selected version and
platform. Existing firmware, sample manager, Windows, and older named assets
are preserved. Published files are downloaded again and compared by SHA-256;
only then is local `assets/complete.json` written. A partial/failed upload may
leave some assets online; rerun to replace the pair. No completion marker is
written for failed runs or `--no-upload`.

The run directory retains `selection.json`, `release.log`, the source checkout,
build output (or `remote-dist/`), verified binary, ZIP/checksum under `assets/`,
and downloaded publication checks under `published-verify/`. Default output is
ignored by Git. Each run is independent and previous runs remain available.

Release scripts do not launch Rack, install the plugin, run engine/runtime
tests, or submit to the VCV Library. Architecture, archive, source, resource,
signature, and publication checks do not establish runtime compatibility on
every supported OS. Offline release regression tests use synthetic packages
and simulated build/upload commands; they do not run any release entrypoint:

```sh
python3 -m unittest discover -s test/rack -p 'test_release.py' -v
```
