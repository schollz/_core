+++
title = 'Zeptocore documentation has moved'
description = 'The canonical Zeptocore manual now lives on the Infinite Digits store product pages.'
+++

# Zeptocore has a new home

The complete Zeptocore manual now lives beside the products it supports. The assembled and DIY pages share one version-controlled guide, so specifications, controls, firmware instructions, schematics, sample preparation, MIDI, troubleshooting, and extension projects cannot drift apart.

<div class="migration-page__actions">
  <a class="migration-page__primary" href="https://shop.infinitedigits.co/collections/zeptocore/#zeptocore-guide">Open the complete Zeptocore guide</a>
  <a href="https://shop.infinitedigits.co/collections/zeptocore/">Zeptocore assembled</a>
  <a href="https://shop.infinitedigits.co/collections/zeptocore-diy/">Zeptocore DIY kit</a>
  <a href="https://tool.zeptocore.com/">Open the sample tool</a>
</div>

The firmware and hardware source remain in the [_core repository](https://github.com/schollz/_core), and existing static asset URLs remain available for old links.

## Current development firmware controls

The current firmware remaps **A + knobs 1/2/3** to volume, low-pass filter cutoff,
and realtime stretch, and **B + knobs 1/2/3** to random slice sequence, playback
rate, and tempo. Knobs 1/2/3 correspond to X/Y/Z. See the
[firmware control reference](https://github.com/schollz/_core/blob/main/docs/zeptocore-controls.md)
for ranges and MIDI CC mappings. Older firmware retains its previous controls.
