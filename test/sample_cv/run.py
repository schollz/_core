"""Host checks for sample CV mapping, settings, and the production control path."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as directory:
    output = Path(directory)
    # Compile the production branches, not a second implementation of their
    # selection rules. Hardware and unrelated controls are stubbed by the test.
    source = (root / 'lib/ectocore.h').read_text()

    def body(marker, start=0):
        opening = source.index(marker, start) + len(marker)
        depth = 1
        for end in range(opening, len(source)):
            depth += (source[end] == '{') - (source[end] == '}')
            if depth == 0:
                return source[opening:end], end
        raise AssertionError(f'Unterminated block: {marker}')

    start = source.index('    bool sample_cv_1voct_active =')
    end = source.index('    for (uint8_t i = 0; i < 3; i++)', start)
    (output / 'active.h').write_text(source[start:end])
    for marker, filename in [('} else if (i == CV_SAMPLE) {', 'sample_control.h'),
                             ('if (knob_gpio[i] == MCP_KNOB_SAMPLE) {', 'sample_knob.h')]:
        content, _ = body(marker)
        (output / filename).write_text(content)
    first_bank = source.index('if (sel_bank_next_new != sel_bank_cur) {')
    _, end = body('if (sel_bank_next_new != sel_bank_cur) {', first_bank)
    content, _ = body('if (sel_bank_next_new != sel_bank_cur) {', end)
    (output / 'button_bank.h').write_text(content)

    for name in ('sample_cv', 'settings', 'controls'):
        exe = output / f'test-{name}'
        subprocess.run([
            os.environ.get('CC', 'clang'), '-std=c11', '-O1', '-g',
            '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
            '-Ilib', '-Itest/sample_cv', f'-I{output}', f'test/sample_cv/test_{name}.c',
            '-lm', '-o', str(exe),
        ], cwd=root, check=True)
        subprocess.run([str(exe)], cwd=output, check=True)
