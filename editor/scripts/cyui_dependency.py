"""Resolve the exact CyUI source; never update a developer checkout or push."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

EDITOR = Path(__file__).resolve().parents[1]


def git(source, *args):
    env = dict(os.environ, GIT_TERMINAL_PROMPT='0', GCM_INTERACTIVE='Never')
    return subprocess.check_output(
        ['git', '-c', f'safe.directory={source.as_posix()}', '-C', str(source), *args],
        env=env, text=True, stderr=subprocess.PIPE).strip()


def resolve(destination, source=None, allow_development=False):
    lock = json.loads((EDITOR/'cyui-source.lock.json').read_text('utf-8'))
    path = (source or destination).resolve()
    if not source and not path.exists():
        path.mkdir(parents=True)
        git(path, 'init', '--quiet')
        git(path, 'remote', 'add', 'origin', lock['repository'])
    try:
        revision = git(path, 'rev-parse', 'HEAD')
    except subprocess.CalledProcessError:
        if source:
            raise RuntimeError('CYUI_DIR must be a Git checkout at the locked revision')
        git(path, 'fetch', '--quiet', '--depth=1', 'origin', lock['revision'])
        git(path, 'checkout', '--quiet', '--detach', lock['revision'])
        revision = git(path, 'rev-parse', 'HEAD')
    if allow_development:
        if not source: raise RuntimeError('Development CyUI requires an explicit --source checkout')
        if not (path/'include/cyui/detail/CodeLanguageList.inc').is_file(): raise RuntimeError('Development CyUI lacks the shared syntax catalogue')
        return path
    if revision != lock['revision']:
        raise RuntimeError(f'CyUI revision mismatch: expected {lock["revision"]}, got {revision}. '
                           'Use a separate matching checkout or the default dependency cache.')
    if git(path, 'status', '--porcelain', '--untracked-files=no'):
        raise RuntimeError('CyUI has tracked local changes; use a clean pinned checkout')
    return path


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, required=True)
    parser.add_argument('--source', default='')
    parser.add_argument('--allow-development',choices=('ON','OFF'),default='OFF')
    args = parser.parse_args()
    # CMake passes an empty --source when the default reproducible cache is wanted.
    source = Path(args.source) if args.source else None
    try:
        print(resolve(args.destination, source, args.allow_development=='ON'))
    except (RuntimeError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
