#!/usr/bin/env python3
"""Commit a version bump and atomically push main and its new release tag."""
import argparse
import json
import os
import subprocess

from release_target import version_parts
from update_version import ROOT, planned_updates


def git(root, *args):
    return subprocess.check_output(['git', *args], cwd=root, text=True).strip()


def release_absent(repository, tag):
    # The list API distinguishes a genuinely absent release from auth/API errors.
    output = subprocess.check_output([
        'gh', 'api', '--paginate', f'repos/{repository}/releases',
        '--jq', '.[].tag_name',
    ], text=True)
    if tag in output.splitlines():
        raise ValueError('Target release already exists: ' + tag)


def prepare(root, version, repository, check_release=release_absent, before_push=None):
    version_parts(version)
    tag = 'v' + version
    if git(root, 'branch', '--show-current') != 'main':
        raise ValueError('Release preparation must run on main')
    if git(root, 'status', '--porcelain'):
        raise ValueError('Release preparation requires a clean checkout')
    git(root, 'fetch', 'origin', 'main')
    if git(root, 'rev-parse', 'HEAD') != git(root, 'rev-parse', 'origin/main'):
        raise ValueError('Checkout must match latest origin/main; start a fresh run')
    if git(root, 'ls-remote', '--tags', 'origin', 'refs/tags/' + tag):
        raise ValueError('Target tag already exists: ' + tag)
    if git(root, 'tag', '--list', tag):
        raise ValueError('Target tag already exists locally: ' + tag)
    check_release(repository, tag)
    updates = planned_updates(root, version)
    for path, text in updates.items():
        (root / path).write_text(text)
    git(root, 'add', '--', *updates)
    git(root, '-c', 'commit.gpgsign=false', 'commit', '-m', 'chore: release ' + tag)
    commit = git(root, 'rev-parse', 'HEAD')
    git(root, 'tag', tag, commit)
    if before_push:
        before_push()
    # A non-fast-forward main or an existing tag rejects BOTH refs.
    git(root, 'push', '--atomic', 'origin', 'HEAD:refs/heads/main', 'refs/tags/' + tag)
    return {'version': version, 'release_tag': tag, 'source_sha': commit}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('version')
    parser.add_argument('--repository', default=os.environ.get('GITHUB_REPOSITORY', 'schollz/_core'))
    args = parser.parse_args()
    try:
        result = prepare(ROOT, args.version, args.repository)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'], 'a') as handle:
            handle.write(''.join(f'{key}={value}\n' for key, value in result.items()))
    print(json.dumps(result))


if __name__ == '__main__':
    main()
