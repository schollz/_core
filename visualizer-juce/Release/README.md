# Standalone releases

These three independent release pipelines package **only the standalone app**.
They do not build plugins, run application tests, launch the app or install it.
They follow Tape's native macOS/Linux scripts and self-hosted Windows action.
All platforms upload to GitHub's **latest existing release**, regardless of its
tag or title. The app version is independent of that release's tag. No release or
tag is created, and existing release metadata and other assets are preserved.

Use the same source revision and version on each machine. The scripts build the
current source; macOS/Linux include source hashes and dirty state in the manifest.
Publication replaces only matching platform asset names and fails if no latest
release exists. Authenticate `gh` against `schollz/_core` for local uploads
(or use `GH_TOKEN`/`GITHUB_TOKEN`). No sibling checkout is needed.

## macOS: run on each Mac

```sh
# Apple Silicon, native arm64 Python (not Rosetta); deployment minimum macOS 12:
python3 scripts/release_visualizer_macos_arm.py --version 1.0.0

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
python3 scripts/release_visualizer_linux.py --version 1.0.0
```

Requires x86_64 Linux, Python 3.9+, Git, CMake 3.22+, a C++17 compiler, Make or
Ninja, and JUCE's Linux development libraries (ALSA, X11, FreeType, Fontconfig).
The executable is unsigned. The ZIP preserves its executable permissions and
contains notices and launch instructions. System libraries are not bundled;
build on the oldest distribution you intend to support.

Native scripts accept `--no-upload` to package locally, `--jobs N`, and
`--repo owner/repository`. They resolve the latest release at startup and abort
upload if it changes during the build, keeping the artifacts locally. They use a fresh build in
`visualizer-juce/release-output/<platform>/<version>-<unique>/`, retain incomplete
output after failure, and write `complete.json` only on success. Each platform
produces a `-Standalone.zip`, `-manifest.json` and `-SHA256SUMS.txt` with unique
platform/architecture names. No test execution is implied by completion.

## Windows: separate manual GitHub Action

Select **Release Visualizer Standalone - Windows** in Actions, choose the source
branch, enter the app version, and click **Run workflow**. After building and
signing, it automatically uploads to whichever release is latest at publication
time. The signed ZIP, manifest and hashes are also retained as an Actions artifact.
`workflow_dispatch` is its only trigger: no tag, push, PR or release event starts
this action automatically. No separate publish checkbox is required.

The runner uses `[self-hosted, Windows, X64]`. It uses
Visual Studio's x64 MSVC/CMake/Ninja tools, static MSVC runtime, and the same
pinned Azure signing action and secrets:

- `AZURE_TENANT_ID`, `AZURE_CLIENT_ID`, `AZURE_CLIENT_SECRET`
- `AZURE_CODE_SIGNING_ENDPOINT`, `AZURE_CODE_SIGNING_ACCOUNT`
- `AZURE_CERT_PROFILE_NAME`

The Azure action Authenticode-signs the executable with SHA-256 and applies an
RFC 3161 SHA-256 timestamp. Packaging fails unless Windows validates both the
signer and timestamp certificates, and the manifest records their subjects,
thumbprints and the signed executable hash. Uploads use the workflow's
`GITHUB_TOKEN` with `contents: write` in a separate hosted publish job. There is
no installer or plugin payload. Builds use fresh output directories on the
persistent runner.

These new release paths have not been executed or tested.
