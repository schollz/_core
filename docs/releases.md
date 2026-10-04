# Release a new version

Open **Actions → Release new version → Run workflow**, choose `main`, and enter
the exact new version without `v`. You can also use GitHub CLI:

```sh
gh workflow run release.yml --repo schollz/_core --ref main -f version=NEW_VERSION
```

The action updates the current version references and download links, commits
directly to `main`, and atomically pushes that commit and its `vNEW_VERSION` tag.
It also increments the last component of the Rack plugin version once per new
release (for example, `2.0.0` becomes `2.0.1`) and updates all four Rack download
links in that same commit. It publishes firmware first, then builds and uploads
the Windows sample manager and Rack plugin in parallel. The run summary records
the tag, exact source SHA, release link, and each build's result. Copy the tag and SHA for your manual builds.

`VERSION` holds the current firmware/sample-manager release version. Versions
must increase, use three integer components without leading zeros, and keep
each component at most 255. Rack keeps its own `2.x` compatibility version:
the updater increments its revision in the plugin manifest without changing
the major or minor component, then regenerates the README Rack links from it.
Historical fixtures and independently versioned dependencies remain separate.
If another tracked file still contains the old current version, the update fails
and names the file; classify it in the updater before retrying.

The workflow uses `GITHUB_TOKEN` with `contents: write` for preparation and uploads.
Repository policy must permit that token to push directly to `main`. No personal
access token is needed. Reusable workflows explicitly run the builds, because
token-authenticated pushes do not trigger the usual push workflows. Windows
sample-manager signing uses the existing Azure signing secrets.

## Manual macOS and Linux packages

You build and upload Apple Silicon, Intel macOS, and Linux for both applications.
No self-hosted runners or changes to your signing/SSH setup are required. Set
these values from the release summary:

```sh
tag=vNEW_VERSION
commit=FULL_SOURCE_SHA_FROM_SUMMARY
```

On your signing Mac, using the existing `tape-notary` Keychain profile:

```sh
python3 sample-manager/Release/release-macosarm.py --notary-profile tape-notary --release-tag "$tag" --source-commit "$commit"
python3 sample-manager/Release/release-macos11.py --notary-profile tape-notary --release-tag "$tag" --source-commit "$commit"
python3 rack/Release/release-macosarm.py --release-tag "$tag" --source-commit "$commit"
python3 rack/Release/release-macos11.py --release-tag "$tag" --source-commit "$commit"
```

The Intel entrypoints retain the default SSH host `zns@192.168.0.44`; pass a
different host as the positional argument if needed. On your Linux x86_64 machine:

```sh
python3 sample-manager/Release/release-linux.py --release-tag "$tag" --source-commit "$commit"
python3 rack/Release/release-linux.py --release-tag "$tag" --source-commit "$commit"
```

Supply both arguments together. The scripts verify that the release tag points
to the source commit, build that commit even if `main` has advanced, and upload to
that named published release. They recheck the selected release and tag before
uploading. A newer release becoming latest does not redirect or block these
explicit builds. Omitting both arguments preserves the existing fresh-`main` and
latest-release behavior. `--no-upload` still signs/packages and keeps assets locally.

All four platforms have README links. macOS/Linux links may be unavailable until
you upload their packages. Rack ZIP names use the plugin version, while their URL
uses the firmware release tag.

## Retry a failed build

If preparation fails, no remote version commit or tag is pushed. Resolve the
reported issue and start a new run. A concurrent change to `main` or creation of
the target tag rejects the atomic push without overwriting either remote ref.

Once preparation succeeds, the version commit and tag stay in place even if a
later build fails. For firmware failures, use **Re-run failed jobs** on the
original release run; this reuses the successful preparation job's outputs. Do
not use **Re-run all jobs** or start another version-bump run for the same version:
the existing tag is deliberately rejected.

If Windows fails, firmware remains published. Rerun failed jobs or invoke either
Windows workflow independently with the recorded tag and SHA:

```sh
gh workflow run release-sample-manager-windows.yml --repo schollz/_core --ref main -f release_tag="$tag" -f source_sha="$commit"
gh workflow run release-rack-windows.yml --repo schollz/_core --ref main -f release_tag="$tag" -f source_sha="$commit"
```

The Windows Run workflow forms offer the same optional inputs. Leaving both
empty retains their standalone defaults. A retry replaces only that platform's
matching assets; it creates no version commit or tag and never moves a tag.
Retries and manual package builds reuse the committed Rack version without
incrementing it again.
Sample-manager diagnostics and packaged Windows assets remain in workflow
artifacts for 14 days. Download/upload verification must pass before a job reports
success. See the [sample-manager release guide](../sample-manager/Release/README.md)
and [Rack release guide](../rack/Release/README.md) for local requirements and logs.

## Verify changes to release tooling

```sh
python3 -m unittest discover -s test/release -v
python3 -m unittest discover -s sample-manager/Tests -p 'test_release.py' -v
python3 -m unittest discover -s test/rack -p 'test_release.py' -v
python3 scripts/update_version.py NEW_VERSION --check
actionlint
```

The automated suites use temporary repositories and simulated packaging/upload
commands. Real firmware compilation, Windows signing, notarization, and hardware
behavior require their respective build environments. A first live release
should confirm firmware publication precedes the Windows uploads and that both
packages record the source SHA from the release summary.
