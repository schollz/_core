# Standalone application releases

For the version-bump action and builds pinned to its exact commit, see the
[repository release guide](../../docs/releases.md). These entrypoints accept
paired `--release-tag` and `--source-commit` arguments to build that source and
upload to that named release. Without both arguments, the default behavior
below remains unchanged.

The release entrypoints build **_core sample manager** from a fresh clone of the
latest `main` commit of `schollz/_core`, using the latest published stable release
for the application version and upload destination. Windows runs manually in
GitHub Actions; the macOS and Linux scripts run on your machines. Each uploads
its platform's standalone archive, manifest and checksums to that same release.
Packaging reads the application name from the selected source's `PRODUCT_NAME`,
so older release tags retain their original app and executable names.
No installer, AU, VST3, firmware build or hardware flashing is involved.

## Release selection and source

The latest release must already exist, be mutable, and have a tag in
`vMAJOR.MINOR.PATCH` or `MAJOR.MINOR.PATCH` form. The tag supplies the application
version; each component must fit JUCE's 0–255 version fields. The release ID,
tag and version are recorded separately from the selected `refs/heads/main`
commit before cloning. Each invocation gets a new source checkout and build
directory, with no reuse of local application changes.

The scripts you invoke supply the release tooling. Application sources come
from the recorded `main` commit, which must contain `sample-manager/CMakeLists.txt`.
The release tag can predate Sample Manager entirely: no new tag or release is
needed to publish application updates from `main`. For example, with latest
release `v8.0.4`, a newer `main` build still reports version `8.0.4` and replaces
that release's `8.0.4` application assets. The exact source commit is recorded in
the manifest so successive builds with the same version remain distinguishable.

Immediately before upload, the scripts resolve latest again and require the
same release ID, tag and version. A changed or immutable release aborts
publication and retains the files; rerun to build with the new release version.
Advancing `main` during a run does not affect its pinned source or block upload.
Rerunning replaces only the exact asset names for that version/platform;
release metadata and other assets are left
alone. No tag or GitHub release is created.

GitHub CLI (`gh`) must be authenticated with access to read releases and upload
assets. A public HTTPS Git clone supplies the source. Local credentials are not
copied into the source checkout or sent to the Intel builder.

## Windows x64 — manual GitHub Actions workflow

Run **Release _core sample manager - Windows**, choosing `main` for the release
tooling. It has no version input:

```sh
gh workflow run release-sample-manager-windows.yml --repo schollz/_core --ref main
```

The workflow uses GitHub's hosted `windows-2022` runner, which includes Git,
GitHub CLI, Windows PowerShell and Visual Studio's C++ x64, CMake and Ninja tools.
It provisions Python 3.11, .NET 8 and Tape's SHA-256-checked portable PowerShell
7.4.13 in runner tool caches. It clones the selected `main` commit under
`RUNNER_TEMP`, builds a Release executable with the static MSVC runtime, and
skips application tests.

The pinned Azure Artifact Signing action signs the executable with SHA-256 and
an RFC 3161 timestamp, using the existing repository secrets:

- `AZURE_TENANT_ID`, `AZURE_CLIENT_ID`, `AZURE_CLIENT_SECRET`
- `AZURE_CODE_SIGNING_ENDPOINT`, `AZURE_CODE_SIGNING_ACCOUNT`, `AZURE_CERT_PROFILE_NAME`

`AZURE_CLIENT_SECRET_ID` is not the secret value and is not used. Packaging
requires a valid Authenticode signature and timestamp, records the publisher
and certificate thumbprints, and creates a portable ZIP with notices. This is
Windows signing, not Apple notarization.

The Windows job has read-only repository permissions. A separate GitHub-hosted
Linux job receives the finished package, verifies its hashes, rechecks the
selected release, and uploads with `contents: write`. Build, packaging and
publication logs are retained as Actions artifacts for 14 days. A failed
publication leaves the downloadable signed package in the workflow artifacts.

## macOS credentials and prerequisites

Run the macOS scripts on the signing Mac with Python 3.9+, Git, authenticated
GitHub CLI, Xcode command-line tools and modern `notarytool`. Native builds also
need CMake 3.24+ and Ninja. Both app architectures retain a macOS 11.0 deployment
minimum. Deployment metadata is checked; it is not evidence of runtime testing
on every supported OS.

The local Keychain must contain the certificate and private key for:

```text
Developer ID Application: Zackary Scholl (KF253X8W3N)
```

Override the identity with `SIGN_IDENTITY`. Notarization uses either an existing
Keychain profile selected by `--notary-profile` / `NOTARY_PROFILE`, or all three
environment variables `APPLE_ID`, `TEAM_ID` and app-specific `APPLE_PASSWORD`.
The scripts do not read `signing.json` or import the repository's P12 secrets.
No Developer ID Installer certificate is needed because these releases are ZIPs.

The app is signed with hardened runtime and a timestamp. The script verifies
the identity and signature, requires an `Accepted` notarization response,
staples and validates the ticket, and checks Gatekeeper before making the final
ZIP. Signing and notarization failures stop publication; there is no unsigned
or signed-only release fallback. Credential values are redacted in retained
logs. Apple and GitHub credential environment variables are stripped from
clone/build/SSH/rsync subprocesses.

### Apple Silicon — build and sign here

From the repository root on this Apple Silicon Mac, using native ARM64 Python
rather than Rosetta:

```sh
python3 sample-manager/Release/release-macosarm.py --notary-profile tape-notary
# Makefile equivalent, with an existing profile:
NOTARY_PROFILE=tape-notary make sample-manager-release-macosarm
```

This clones the latest `main` commit locally, compiles the standalone ARM64 app
inside that clone, then signs, notarizes and uploads it from this Mac.

### Intel — build remotely, sign here

```sh
python3 sample-manager/Release/release-macos11.py --notary-profile tape-notary
# Override Tape's default Intel host:
python3 sample-manager/Release/release-macos11.py user@intel-mac --notary-profile tape-notary --jobs 3
# Makefile equivalent:
NOTARY_PROFILE=tape-notary make sample-manager-release-macos11 MACOS_RELEASE_HOST=user@intel-mac
```

The default host is `zns@192.168.0.44`. It must be an Intel Mac reachable through
noninteractive SSH, with Git, Xcode command-line tools, CMake 3.24+, Ninja,
rsync, and network access to GitHub for cloning and the pinned JUCE download.
SSH and rsync must also be installed on the signing Mac; SSH agent forwarding
is disabled.

The controller clones the selected `main` commit locally for packaging inputs.
The Intel Mac creates its own fresh clone in a unique temporary directory and verifies the
same pinned commit before compiling. Only the app, build metadata and JUCE
notice return from the builder; a `ditto` ZIP preserves bundle permissions and
symlinks. The controller checks the returned architecture, version, deployment
minimum and dependencies, then signs and notarizes locally. All Apple signing
credentials stay on the controller.

The remote directory is removed only after a successful run. Failures retain
it and print its path. `--keep-remote` also retains successful remote builds.

## Linux x86_64 — run directly on Linux

Use a Debian/Ubuntu x86_64 machine, preferably a conservative baseline such as
Ubuntu 22.04. Install Python 3.9+, Git, authenticated GitHub CLI and the native
build/packaging tools first. For example, the build dependencies include:

```sh
sudo apt-get install build-essential cmake ninja-build pkg-config binutils \
  libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev \
  libxrender-dev libfreetype6-dev libfontconfig1-dev libcurl4-openssl-dev
```

Use CMake 3.24+ if the distribution's default is older. Packaging also uses
`dpkg-query`, `ldconfig`, `ldd` and `getconf`. The script reports missing tools;
it does not install system packages.

```sh
python3 sample-manager/Release/release-linux.py
# Makefile equivalent:
make sample-manager-release-linux
```

This clones and builds the latest `main` commit locally, checks the ELF
architecture, and packages the executable with a relative launcher and its resolved non-glibc
runtime libraries. It includes installed Debian/Ubuntu library notices and
records their package ownership; missing notices fail packaging. The archive
includes libstdc++, libgcc when needed, ALSA, X11, FreeType, Fontconfig, libcurl
and their non-glibc dependencies. The loader and glibc remain host requirements,
and the manifest records the build host's glibc baseline. Display/audio services,
fonts and trusted CA certificates also remain host services.

The Linux package is uploaded without a platform code signature or notarization.
Launch the extracted app with `core-sample-manager`.

## Options, artifacts and logs

The three native entrypoints accept:

- `--jobs N`: compiler parallelism, default 6.
- `--output PATH`: parent of a unique run directory; defaults to
  `sample-manager/dist/releases/` beside the invoking tooling.
- `--no-upload`: still clone `main`, use the latest release version and perform
  the full normal build/package/sign/notarize path, but retain the assets locally.

Use `RELEASE_ARGS` with the Makefile shortcuts, for example:

```sh
NOTARY_PROFILE=tape-notary make sample-manager-release-macosarm RELEASE_ARGS='--jobs 4 --no-upload'
```

Each platform uploads three files:

```text
_core-sample-manager-VERSION-PLATFORM.zip      # macos-arm64, macos-x86_64, windows-x64
_core-sample-manager-VERSION-linux-x86_64.tar.gz
_core-sample-manager-VERSION-PLATFORM-manifest.json
_core-sample-manager-VERSION-PLATFORM-SHA256SUMS.txt
```

The native run directory retains `source/`, the build inside it, `payload/`,
`selection.json`, `release.log`, `assets/`, and macOS notarization evidence.
The manifest records the release ID/tag/version under `release` and the built
repository/ref/commit under `source`, as well as signed payload hashes,
platform and version, archive size/hash, signature/notarization evidence where
applicable, and `applicationTestsRun: false`. Output is ignored by Git under the
default destination. Previous runs are preserved.

After publication, the script checks GitHub's reported asset sizes and SHA-256
digests, then writes local `assets/complete.json`. This marker is not uploaded;
Windows retains it in the publication-result Actions artifact. `--no-upload`
and failed/partial uploads do not receive a publication completion marker.
An interrupted multi-file upload may leave some assets on GitHub; rerun the
release to replace that platform's full set.

## Execution and acceptance

These new release paths do **not** run CTest, embedded self-tests, MIDI tests,
application launches, or hardware tests. They do perform source/version,
architecture, signature, notarization, packaging and upload-integrity checks.
Source-selection and publication regression tests use temporary Git repositories
and simulated build/signing/upload commands:

```sh
python3 -m unittest discover -s sample-manager/Tests -p 'test_release.py' -v
```

These tests do not validate native compilation, real signing/notarization or
live GitHub uploads.

When exercising them yourself, check:

1. Each of the four paths builds the selected `main` commit using the latest
   release version, and attaches only that platform's archive, manifest and
   checksum file to that release.
2. ARM64 and Intel packages have accepted notarization and stapled tickets;
   Windows has valid timestamped Authenticode; Linux starts through its launcher.
3. An older latest tag without Sample Manager still works when `main` contains it.
4. Missing tools/credentials, an unavailable Intel host, or rejected signing or
   notarization produces retained diagnostics and no upload/completion marker.
5. Publishing a newer release during a build aborts upload and retains the
   finished package. Advancing `main` does not change the source being built or
   block upload to the unchanged latest release.
6. `--no-upload` produces the normal signed/notarized macOS package and all
   metadata locally, without publishing or marking publication complete.

## Existing local-only packaging

The existing commands still build/package the current source checkout without
uploading. Their version remains an independent explicit value, default `0.1.0`:

```sh
python3 sample-manager/Release/package.py --platform macos-arm64 --version 0.1.0
python3 sample-manager/Release/package.py --platform macos-x86_64 --version 0.1.0
python3 sample-manager/Release/package.py --platform linux-x86_64 --version 0.1.0
```

These local commands retain their native CTest step unless `--skip-build` is
used. macOS defaults to ad-hoc signing; `--sign-identity` plus `--notary-profile`
opts into Developer ID signing/notarization. Their local `complete.json` marks
package completion only, not publication.

On Windows:

```powershell
./sample-manager/Release/build-windows.ps1 -Version 0.1.0
./sample-manager/Release/package-windows.ps1 -Version 0.1.0
```

Both PowerShell helpers accept `-SourceRoot` pointing at a sample-manager
project and `-BuildDirectory`. The builder accepts `-SkipTests`; the packager
accepts `-OutputDirectory` and `-RequireSignature`. The new release workflow
supplies the fresh `main` project explicitly, skips application tests, and
requires signing. Ordinary local commands preserve their build/test and
unsigned-packaging defaults.
