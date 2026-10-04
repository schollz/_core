#!/usr/bin/env python3
"""Check an explicit workflow release target before building or uploading."""
import argparse
import json
import subprocess

from release_target import verify_tag
from update_version import ROOT


class Runner:
    def run(self, *args):
        return subprocess.check_output(args, text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('release_tag')
    parser.add_argument('source_sha')
    parser.add_argument('--published', action='store_true')
    args = parser.parse_args()
    runner = Runner()
    try:
        verify_tag(runner, 'https://github.com/schollz/_core.git', args.release_tag, args.source_sha)
        if runner.run('git', '-C', str(ROOT), 'rev-parse', 'HEAD') != args.source_sha:
            raise ValueError('Checkout does not match the supplied source commit')
        if (ROOT / 'VERSION').read_text().strip() != args.release_tag[1:]:
            raise ValueError('VERSION does not match the supplied release tag')
        if args.published:
            release = json.loads(runner.run('gh', 'api', 'repos/schollz/_core/releases/tags/' + args.release_tag))
            if (release['tag_name'] != args.release_tag or release.get('draft')
                    or release.get('prerelease') or release.get('immutable')):
                raise ValueError('Target release must be published, stable and mutable')
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
