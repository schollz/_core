# Lucide button icons

Vendored unmodified from [Lucide 1.49.0](https://github.com/lucide-icons/lucide/releases/tag/1.49.0),
commit `5a92b9ba262de5bf10e864219883267672c05db8`.
Each SVG comes from the upstream `icons/` directory. `SHA256SUMS` records the exact
vendored bytes, including the upstream license copied as `Lucide-LICENSE.txt`.

Only the action icons used by the sample manager are included. CMake embeds the
SVGs and license into `CoreIcons`; no icon download or web font is needed at
runtime. `IconButton` preserves the 24-unit SVG geometry and round strokes,
resolves `currentColor` for JUCE, and tints the drawing with the button's theme
text color, including toggled and disabled states. The SVG source files are not
modified for recoloring.

Lucide uses the ISC license, with MIT notices for icons derived from Feather.
The complete upstream notices are retained in `Lucide-LICENSE.txt` and also
included in the macOS app's resources.
