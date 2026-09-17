#!/usr/bin/env python3
"""Report audio callback wall-time occupancy from validated diagnostic captures.

This includes time spent waiting inside the callback and any preemption. It is
not an instruction-cycle measurement or whole-device/core-0 CPU utilization.
"""
import argparse
import json
from pathlib import Path

from zeptocore_debug import summarize


def report(directory):
    snapshots = [json.loads(line) for line in (directory / 'snapshots.jsonl').read_text().splitlines()]
    summary = summarize(snapshots)
    if not summary['valid']:
        raise ValueError(f'{directory}: {summary.get("reason", "invalid capture")}')
    # Domain timestamps are uint32 microseconds. Sum bounded adjacent deltas
    # to handle rollover without substituting host polling/scheduling time.
    intervals = [(b['audio']['timestamp_us'] - a['audio']['timestamp_us']) & 0xffffffff
                 for a, b in zip(snapshots, snapshots[1:])]
    if any(delta == 0 or delta >= 0x80000000 for delta in intervals):
        raise ValueError(f'{directory}: stalled or ambiguous device timestamps')
    elapsed = sum(intervals)
    first, last = snapshots[0], snapshots[-1]
    rendered = (last['audio']['counters']['rendered_frames'] -
                first['audio']['counters']['rendered_frames'])
    if rendered <= 0:
        raise ValueError(f'{directory}: no rendered audio')
    callback = summary['domains']['audio']['metrics']['callback']
    return {
        'capture': str(directory), 'clock_hz': first['device']['clock_hz'],
        'build_id': first['device'].get('build_id'),
        'producer_frames': first['device']['frames'],
        'actual_sample_rate_millihz': first['device']['actual_sample_rate_millihz'],
        'device_elapsed_us': elapsed, 'callback_count': callback['count'],
        'callback_total_us': callback['total_us'],
        'audio_callback_occupancy_percent': 100 * callback['total_us'] / elapsed,
        'callback_mean_us': callback['mean_us'],
        'callback_p95_us_range': callback['p95_us_range'],
        'callback_p99_us_range': callback['p99_us_range'],
        'rendered_frames': rendered,
        'callback_us_per_rendered_frame': callback['total_us'] / rendered,
        'starvation_count': summary['starvation_count'],
        'scope': 'Callback wall time on audio core, including waits/preemption; '
                 'excludes other core work and time outside the callback. '
                 'Callback timing publication lags by one call.',
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path, nargs='+', help='Directories containing snapshots.jsonl')
    args = parser.parse_args()
    print(json.dumps([report(path) for path in args.capture], indent=2))
