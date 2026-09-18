"""Build opt-in firmware in isolation; never programs a device or edits device definitions."""
from pathlib import Path
import argparse
import os
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=root / 'artifacts/visualizer-build')
parser.add_argument('--jobs', type=int, default=min(os.cpu_count() or 2, 8))
args = parser.parse_args()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
env = {**os.environ, 'PICO_SDK_PATH': str(root / 'pico-sdk'), 'PICO_EXTRAS_PATH': str(root / 'pico-extras')}


def run(command, log, success=True):
    result = subprocess.run(command, cwd=root, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log.write_text(result.stdout)
    if (result.returncode == 0) != success:
        raise RuntimeError(f'Unexpected exit {result.returncode}; see {log}\n{result.stdout[-4000:]}')
    return result.stdout


for variant, definitions in [('441', 'zeptocore_compile_definitions.cmake'),
                             ('256', 'zeptocore_compile_definitions_256.cmake')]:
    build = output / variant
    base = ['cmake', '-S', str(root), '-B', str(build),
            f'-DCORE_COMPILE_DEFINITIONS={root / "lib/cmake" / definitions}']
    # -U removes a previous cache value to exercise the default as well as explicit OFF.
    for mode, options in [('default', ['-UZEPTOCORE_VISUALIZER']),
                          ('on', ['-DZEPTOCORE_VISUALIZER=ON']),
                          ('off', ['-DZEPTOCORE_VISUALIZER=OFF'])]:
        print(f'Building {variant} / {mode}', flush=True)
        run(base + options, output / f'{variant}-{mode}-configure.log')
        run(['cmake', '--build', str(build), '--parallel', str(args.jobs)], output / f'{variant}-{mode}-build.log')
        symbols = run(['arm-none-eabi-nm', '-a', str(build / '_core.elf')], output / f'{variant}-{mode}-symbols.log')
        enabled = mode == 'on'
        assert (' zv_publish' in symbols) == enabled
        assert (' zv_service' in symbols) == enabled
        if not enabled:
            assert not any(' zv_' in line for line in symbols.splitlines())
            assert 'visualizer_telemetry.c' not in (build / '_core.elf.map').read_text()
        binary = (build / '_core.bin').read_bytes()
        assert (b'view=2,' in binary) == enabled
        assert f'ZEPTOCORE_VISUALIZER:BOOL={"ON" if enabled else "OFF"}' in (build / 'CMakeCache.txt').read_text()
        size = run(['arm-none-eabi-size', str(build / '_core.elf')], output / f'{variant}-{mode}-size.log')
        print(size.strip(), flush=True)

# Invalid configurations must fail at configuration, not later during linking.
no_midi = output / 'zeptocore-no-midi.cmake'
no_midi.write_text(f'include("{root / "lib/cmake/zeptocore_compile_definitions.cmake"}")\n'
                   'get_target_property(defs ${PROJECT_NAME} COMPILE_DEFINITIONS)\n'
                   'list(FILTER defs EXCLUDE REGEX "^INCLUDE_MIDI(=|$)")\n'
                   'set_property(TARGET ${PROJECT_NAME} PROPERTY COMPILE_DEFINITIONS ${defs})\n')
for name, definitions in [('other-device', root / 'lib/cmake/ezeptocore_compile_definitions.cmake'), ('no-midi', no_midi)]:
    result = run(['cmake', '-S', str(root), '-B', str(output / name),
                  f'-DCORE_COMPILE_DEFINITIONS={definitions}', '-DZEPTOCORE_VISUALIZER=ON'],
                 output / f'{name}.log', success=False)
    assert 'ZEPTOCORE_VISUALIZER requires' in result
# Exercise the Make entry points as well: every invocation must pass an explicit
# value and opt-in output must never overwrite the standard UF2 filename.
for target in ['zeptocore', 'zeptocore_256', 'zeptocore_nooverclock']:
    for mode in ['ON', 'OFF', None]:
        command = ['make', '-n', target] + ([f'ZEPTOCORE_VISUALIZER={mode}'] if mode else [])
        recipe = run(command, output / f'make-{target}-{mode or "default"}.log')
        assert f'-DZEPTOCORE_VISUALIZER={mode or "OFF"}' in recipe
        filename = 'zeptocore_nooverclock' if target.endswith('nooverclock') else 'zeptocore'
        filename += '_visualizer.uf2' if mode == 'ON' else '.uf2'
        assert f'cp build/_core.uf2 {filename}' in recipe
print('Visualizer build checks passed.', flush=True)
