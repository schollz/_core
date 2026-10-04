"""Validate an explicit stable release tag and its exact source commit."""
import re

VERSION_PATTERN = r'(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)'


def version_parts(version):
    if not re.fullmatch(VERSION_PATTERN, version):
        raise ValueError('Expected MAJOR.MINOR.PATCH without a prefix or leading zeros')
    parts = tuple(map(int, version.split('.')))
    if any(part > 255 for part in parts):
        raise ValueError('Version components must be at most 255 for JUCE')
    return parts


def validate_target(release_tag, source_commit):
    if bool(release_tag) != bool(source_commit):
        raise ValueError('--release-tag and --source-commit must be supplied together')
    if not release_tag:
        return
    if not release_tag.startswith('v'):
        raise ValueError('Explicit release tag must start with v')
    version_parts(release_tag[1:])
    if not re.fullmatch(r'[0-9a-f]{40}', source_commit):
        raise ValueError('Source commit must be a full lowercase 40-character Git SHA')


def verify_tag(runner, repository_url, release_tag, source_commit):
    validate_target(release_tag, source_commit)
    if not release_tag:
        raise ValueError('An explicit release tag and source commit are required')
    ref = 'refs/tags/' + release_tag
    output = runner.run('git', 'ls-remote', '--exit-code', repository_url, ref, ref + '^{}')
    refs = dict(line.split()[::-1] for line in output.splitlines() if len(line.split()) == 2)
    if refs.get(ref + '^{}', refs.get(ref)) != source_commit:
        raise ValueError('Release tag does not point to the supplied source commit')


def target_arguments(parser):
    parser.add_argument('--release-tag', default='', help='Published vMAJOR.MINOR.PATCH release to upload to')
    parser.add_argument('--source-commit', default='', help='Exact full Git SHA pointed to by --release-tag')
