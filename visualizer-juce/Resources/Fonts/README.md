# Bundled fonts

All fonts are embedded; no font installation, conversion tool or network access is needed to build or run.

- IBM Plex Mono Medium and Bold: lossless WOFF2-to-TrueType conversion of the original visualizer assets using fontTools.
- IBM Plex Mono Regular: upstream TrueType font from IBM/plex revision `bf260093582f04622aacc1e9f9ca604d7ccd0c42`, also used by the tape JUCE app. The original visualizer's Regular WOFF2 failed Brotli decoding during conversion.
- Font Awesome Free solid 6.2.0: upstream `https://raw.githubusercontent.com/FortAwesome/Font-Awesome/6.2.0/webfonts/fa-solid-900.ttf`. Uses the same 19 codepoints and 16 effect-bit assignments as the original visualizer. Its existing WOFF2 also failed decoding, so the matching native release is bundled.

IBM Plex and Font Awesome fonts retain their accompanying SIL Open Font License notices. `FONT-LICENSE.md` is preserved from the source visualizer as provenance.
