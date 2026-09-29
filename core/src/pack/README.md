# Sample filename metadata

Full website packs include `bankN/S.name.json` for each numbered sample. Copying
bank folders preserves these files automatically. Individual sample copies must
include the sidecar as well as the numbered WAV and `.info` files; renumber the
sidecar too when changing slots. Settings-only packs contain no sample sidecars.

Schema 1 uses UTF-8 JSON with `schema`, `name`, `originalFilename`, and
`audioSha256`. Both names initially contain the website's source basename,
including its extension. No source directory is stored. The SHA-256 covers the
entire copied primary `S.0.wav`, including its header and padding. One sidecar
covers all companion variants of that slot. Metadata must fit within 64 KiB.
Failure to write required metadata fails the full export.

The native sample manager recovers these names without `.core-manager`, using
only metadata in the same slot with a matching primary audio hash. Older cards
without recorded names retain generic labels. The firmware continues using its
existing numbered WAV and binary `.info` filenames and formats.

`pack_test.go` prepares a full ZIP/extraction contract against the same Unicode
sidecar fixture used by native adoption. These new checks have not been executed.
