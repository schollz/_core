"""Build opt-in firmware in isolation; never programs a device or edits device definitions."""
from pathlib import Path
import argparse
import json
import os
import shlex
import subprocess
import sys
import time

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


subprocess.run([sys.executable, str(root / 'test/visualizer_build/test_make.py')], check=True)

for variant, definitions in [('441', 'zeptocore_compile_definitions.cmake'),
                             ('256', 'zeptocore_compile_definitions_256.cmake'),
                             ('ezeptocore', 'ezeptocore_compile_definitions.cmake'),
                             ('ectocore', 'ectocore_compile_definitions.cmake')]:
    build = output / variant
    base = ['cmake', '-S', str(root), '-B', str(build),
            '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
            f'-DCORE_COMPILE_DEFINITIONS={root / "lib/cmake" / definitions}']
    # -U removes a previous cache value to exercise the default as well as explicit OFF.
    for mode, options in [('default', ['-UZEPTOCORE_VISUALIZER']),
                          ('on', ['-DZEPTOCORE_VISUALIZER=ON']),
                          ('off', ['-DZEPTOCORE_VISUALIZER=OFF'])]:
        print(f'Building {variant} / {mode}', flush=True)
        # macOS Make 3.81 compares whole-second mtimes. Ensure changed flags
        # are newer than the last object rather than racing the previous link.
        if mode != 'default':
            time.sleep(1)
        run(base + options, output / f'{variant}-{mode}-configure.log')
        run(['cmake', '--build', str(build), '--parallel', str(args.jobs)], output / f'{variant}-{mode}-build.log')
        symbols = run(['arm-none-eabi-nm', '-a', str(build / '_core.elf')], output / f'{variant}-{mode}-symbols.log')
        enabled = mode == 'on'
        commands = json.loads((build / 'compile_commands.json').read_text())
        assert any(c['file'].endswith('/lib/visualizer_telemetry.c') for c in commands) == enabled
        main_command = next(c['command'] for c in commands if c['file'] == str(root / 'main.c'))
        flags = shlex.split(main_command)
        if variant in ('ezeptocore', 'ectocore'):
            assert '-DSAMPLES_PER_BUFFER=441' in flags and '-DDO_OVERCLOCK=1' in flags
            amen, brk = (3, 0) if variant == 'ezeptocore' else (0, 3)
            assert f'-DMCP_KNOB_AMEN={amen}' in flags and f'-DMCP_KNOB_BREAK={brk}' in flags
            assert ('-DINCLUDE_MIDI=1' in flags) == enabled
            assert ('pico_stdio_usb/include/tusb_config.h' in main_command) == (not enabled)
            assert (' tud_midi_n_packet_write' in symbols) == enabled
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
for name, definitions in [('other-device', root / 'lib/cmake/zeptoboard_compile_definitions.cmake'), ('no-midi', no_midi)]:
    result = run(['cmake', '-S', str(root), '-B', str(output / name),
                  f'-DCORE_COMPILE_DEFINITIONS={definitions}', '-DZEPTOCORE_VISUALIZER=ON'],
                 output / f'{name}.log', success=False)
    assert 'ZEPTOCORE_VISUALIZER requires' in result
# Exercise the Make entry points as well: every invocation must pass an explicit
# value and opt-in output must never overwrite the standard UF2 filename.
for target in ['zeptocore', 'zeptocore_256', 'zeptocore_nooverclock', 'ezeptocore', 'ectocore']:
    for mode in ['ON', 'OFF', None]:
        command = ['make', '-n', target] + ([f'ZEPTOCORE_VISUALIZER={mode}'] if mode else [])
        recipe = run(command, output / f'make-{target}-{mode or "default"}.log')
        assert f'-DZEPTOCORE_VISUALIZER={mode or "OFF"}' in recipe
        filename = 'zeptocore' if target == 'zeptocore_256' else target
        filename += '_visualizer.uf2' if mode == 'ON' else '.uf2'
        assert f'cp build/_core.uf2 {filename}' in recipe
print('Visualizer build checks passed.', flush=True)
