# Standalone releases

These three independent release pipelines package **only the standalone app**.
They do not build plugins, run application tests, launch the app or install it.
They follow Tape's native macOS/Linux scripts and self-hosted Windows action.
All platforms share the dedicated `zeptocore-visualizer-vVERSION` GitHub release;
firmware releases are untouched, and this release is not marked latest.

Use the same source revision and version on each machine. The scripts build the
current source; macOS/Linux include source hashes and dirty state in the manifest.
Publication creates the dedicated release if necessary or replaces only matching
platform asset names. Authenticate `gh` against `schollz/_core` for local uploads
(or use `GH_TOKEN`/`GITHUB_TOKEN`). No sibling checkout is needed.

## macOS: run on each Mac

```sh
# Apple Silicon, native arm64 Python (not Rosetta); deployment minimum macOS 12:
make -C visualizer-juce release-macosarm RELEASE_ARGS='--version 1.0.0'

# Intel Mac; deployment minimum macOS 11.6:
make -C visualizer-juce release-macos11 RELEASE_ARGS='--version 1.0.0'
```

Each Mac builds, signs, notarizes and packages locally. There is no SSH build or
hosted macOS runner. Both need Python 3.9+, Git, CMake 3.22+, Make or Ninja,
Xcode command-line tools, `gh`, and a working `xcrun notarytool` installation.
Use a sufficiently recent signing Mac/toolchain even when targeting older macOS.

Credentials match Tape: the local Keychain's
`Developer ID Application: Zackary Scholl (KF253X8W3N)` certificate/private key,
overridable with `SIGN_IDENTITY`, plus `NOTARY_PROFILE` (or `--notary-profile`),
or `APPLE_ID`, `TEAM_ID`, `APPLE_PASSWORD` (app-specific password). No certificates
are imported, no secrets are copied, and no unsigned fallback is used. The
standalone needs no microphone entitlement. No installer certificate is needed
because this release distributes a ZIP containing the signed, notarized, stapled
app and notices, rather than a PKG.

## Linux: run on the Linux machine

```sh
make -C visualizer-juce release-linux RELEASE_ARGS='--version 1.0.0'
```

Requires x86_64 Linux, Python 3.9+, Git, CMake 3.22+, a C++17 compiler, Make or
Ninja, and JUCE's Linux development libraries (ALSA, X11, FreeType, Fontconfig).
The executable is unsigned. The ZIP preserves its executable permissions and
contains notices and launch instructions. System libraries are not bundled;
build on the oldest distribution you intend to support.

Native scripts accept `--no-upload` to package locally, `--jobs N`, and
`--repo owner/repository`. They use a fresh build in
`visualizer-juce/release-output/<platform>/<version>-<unique>/`, retain incomplete
output after failure, and write `complete.json` only on success. Each platform
produces a `-Standalone.zip`, `-manifest.json` and `-SHA256SUMS.txt` with unique
platform/architecture names. No test execution is implied by completion.

## Windows: separate manual GitHub Action

Select **Release Visualizer Standalone - Windows** in Actions, choose the source
branch, enter the same version, and enable `publish` to attach the assets. With
`publish` disabled the signed ZIP, manifest and hashes are retained as an Actions
artifact. This workflow does not trigger on firmware tags or ordinary pushes.

The runner labels match Tape: `[self-hosted, Windows, X64, tape-gpu]`. It uses
Visual Studio's x64 MSVC/CMake/Ninja tools, static MSVC runtime, and the same
pinned Azure signing action and secrets:

- `AZURE_TENANT_ID`, `AZURE_CLIENT_ID`, `AZURE_CLIENT_SECRET`
- `AZURE_CODE_SIGNING_ENDPOINT`, `AZURE_CODE_SIGNING_ACCOUNT`
- `AZURE_CERT_PROFILE_NAME`

Signing is required before packaging. Uploads use the workflow's `GITHUB_TOKEN`
with `contents: write` in a separate hosted publish job. There is no installer or
plugin payload. Builds use fresh output directories on the persistent runner.

These new release paths have not been executed or tested.
