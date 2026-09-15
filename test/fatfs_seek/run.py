#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as directory:
    executable = str(Path(directory)/'fatfs')
    command = ['cc', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined',
               '-Ilib', '-Ilib/sdio/include', '-Ilib/sdio/ff15/source', '-Itest/fatfs_seek',
               '-DSEEK_MAP_HOST_TEST=1', 'lib/seek_hash.c', 'lib/audio_seek_map.c',
               'test/fatfs_seek/disk.c', 'test/fatfs_seek/test_bundled.c',
               'lib/sdio/ff15/source/ff.c', 'lib/sdio/ff15/source/ffsystem.c',
               'lib/sdio/ff15/source/ffunicode.c', '-Wl,--wrap=calloc', '-lm', '-o', executable]
    subprocess.run(command, cwd=root, check=True)
    subprocess.run([executable], cwd=root, check=True)
    comparison = command.copy()
    comparison.insert(1, '-DSEEK_MAP_ATTACH=0')
    comparison.remove('-Wl,--wrap=calloc')
    comparison[comparison.index('test/fatfs_seek/test_bundled.c')] = 'test/fatfs_seek/test_comparison.c'
    subprocess.run(comparison, cwd=root, check=True)
    subprocess.run([executable], cwd=root, check=True)
    benchmark = comparison.copy()
    benchmark.insert(1, '-DSEEK_TEST_FRAGMENT_BENCH=1')
    benchmark[benchmark.index('test/fatfs_seek/test_comparison.c')] = 'test/fatfs_seek/test_benchmark.c'
    benchmark[benchmark.index('lib/audio_seek_map.c')] = 'lib/seek_fragment_benchmark.c'
    benchmark.insert(1, 'lib/seek_diagnostics.c')
    subprocess.run(benchmark, cwd=root, check=True)
    subprocess.run([executable], cwd=root, check=True)
    fixture = command.copy()
    fixture.insert(1, '-DSEEK_TEST_AUDIO_FIXTURE=1')
    fixture.insert(1, 'lib/seek_audio_fixture.c')
    fixture.remove('-Wl,--wrap=calloc')
    fixture[fixture.index('test/fatfs_seek/test_bundled.c')] = 'test/fatfs_seek/test_audio_fixture.c'
    subprocess.run(fixture, cwd=root, check=True)
    subprocess.run([executable], cwd=root, check=True)
    command = ['cc', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined',
               '-DAUDIO_MEDIA_HOST_TEST=1', '-Ilib', '-pthread',
               'lib/audio_media_owner.c', 'test/fatfs_seek/test_owner.c',
               '-o', executable]
    subprocess.run(command, cwd=root, check=True)
    subprocess.run([executable], cwd=root, check=True)
