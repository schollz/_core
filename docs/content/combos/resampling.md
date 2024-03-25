+++
title = 'Resampling'
date = 2024-02-01T12:33:06-08:00
short = 'Toggle between linear and quadratic resampling modes'
buttons = ['9','12','10','11']
weight = 97
mode = 'any'
icon = 'superscript'
+++

There are two types of resampling in Zeptocore - *linear* and *quadratic*. By default, resampling is conducted linearly, which may introduce artifacts that sound "grainy" or "lofi". An alternative mode is quadratic resampling, which offers slightly better audio quality but at a higher CPU cost. In cases where the CPU load surpasses a threshold (which can happen with multiple effects), quadratic resampling may lead to more frequent intermittent glitches.

When in *linear* resampling mode, it will display `LINEAR`, and when in *quadratic* mode, it will show `QUAD`.