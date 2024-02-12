+++
title = 'Resampling'
date = 2024-02-01T12:33:06-08:00
short = 'Toggle between linear and quadratic resampling modes'
buttons = ['9','12','10','11']
weight = 97
mode = 'any'
icon = 'superscript'
+++

There are two types of resampling in the zeptocore - *linear* and *quadratic*. By default, the resampling is done linearly, which can have artifacts that sound "grainy" or "lofi". There is an alternative mode of quadratic resampling which sounds slightly better but has a higher CPU cost and can cause more frequent intermittent glitches if the CPU ever exceeds a threshold (which can occur with many effects).

Resampling will display `LINEAR` if in *linear* and will display `QUAD` when set to *quadratic*.

