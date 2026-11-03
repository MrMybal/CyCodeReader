"""Exercise the actual native Git plugin with local, disposable repositories."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

LIBRARY = Path(sys.argv.pop(1)).resolve()
GIT = sys.argv.pop(1)
EXECUTE = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p)


class PluginABI(ctypes.Structure):
    _fields_ = [('abi', ctypes.c_uint32), ('size', ctypes.c_uint32), ('execute', EXECUTE)]


class GitPluginTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.library = ctypes.CDLL(str(LIBRARY))
        cls.library.cycodereader_plugin_v1.restype = ctypes.POINTER(PluginABI)
        cls.api = cls.library.cycodereader_plugin_v1().contents
        assert cls.api.abi == 1 and cls.api.size >= ctypes.sizeof(PluginABI)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='reader-git-offline-')
        self.root = Path(self.temporary.name)
        self.repo = self.root / 'dépôt avec espaces'
        self.repo.mkdir()
        self.env = dict(os.environ, GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL=os.devnull,
                        GIT_TERMINAL_PROMPT='0', GIT_AUTHOR_NAME='Offline fixture',
                        GIT_AUTHOR_EMAIL='fixture@example.invalid',
                        GIT_COMMITTER_NAME='Offline fixture', GIT_COMMITTER_EMAIL='fixture@example.invalid')
        for key in ('GIT_DIR', 'GIT_WORK_TREE', 'GIT_INDEX_FILE', 'GIT_CONFIG_COUNT', 'GIT_CONFIG_PARAMETERS'):
            self.env.pop(key, None)
        self.git('init')
        self.file = self.repo / 'été [one] --file.cpp'
        self.base = 'base 🛠\nsecond line\n'
        self.file.write_text(self.base, encoding='utf-8', newline='')
        self.git('add', '--', self.file.name)
        self.git('commit', '-m', 'Owned offline fixture')

    def tearDown(self):
        self.temporary.cleanup()

    def git(self, *args, cwd=None, success=True):
        result = subprocess.run([GIT, '-c', 'core.autocrlf=false', '-C', str(cwd or self.repo), *args],
                                env=self.env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr.decode('utf-8', errors='replace'))
        return result

    def invoke(self, command='context', path=None, text='unsaved buffer 🛠\n', encoding='utf-8', **extra):
        payload = json.dumps({'path': str(path or self.file), 'text': text,
                              'encoding': encoding, 'revision': 7, 'options': {}, **extra}).encode('utf-8')
        result = self.api.execute(command.encode('ascii'), payload)
        return json.loads(ctypes.string_at(result))

    def test_history_follows_renames_and_compares_revisions_without_writing(self):
        original = self.file.name
        first = self.git('rev-parse', 'HEAD').stdout.decode().strip()
        renamed = self.repo / 'nouveau 🛠.cpp'
        self.git('mv', '--', original, renamed.name)
        self.git('commit', '-m', 'Rename only')
        rename_commit = self.git('rev-parse', 'HEAD').stdout.decode().strip()
        self.file = renamed
        self.file.write_text('base 🛠\nchanged line\n', encoding='utf-8', newline='')
        self.git('commit', '-am', 'Change second line')
        index = (self.repo / '.git/index').read_bytes()
        disk = self.file.read_bytes()
        result = self.invoke('history')
        self.assertNotIn('error', result)
        commits = result['history']['commits']
        self.assertEqual(len(commits), 3)
        self.assertEqual([c['subject'] for c in commits], ['Change second line', 'Rename only', 'Owned offline fixture'])
        self.assertEqual(commits[1]['previousPath'], original)
        self.assertEqual(commits[2]['path'], original)
        revision = self.invoke('revision', beforeRevision=first, beforePath=original, text='live buffer\n')
        self.assertEqual(revision['comparison']['before'], self.base)
        self.assertEqual(revision['comparison']['after'], 'live buffer\n')
        pair = self.invoke('revision', beforeRevision=first, beforePath=original,
                           afterRevision=rename_commit, afterPath=renamed.name)
        self.assertEqual(pair['comparison']['before'], pair['comparison']['after'])
        root = self.invoke('revision', beforeRevision='', afterRevision=first, afterPath=original)
        self.assertEqual(root['comparison']['before'], '')
        self.assertEqual(root['comparison']['after'], self.base)
        resolved = self.invoke('revision', beforeRevision=first)
        self.assertEqual(resolved['comparison']['before'], self.base)
        self.assertEqual((self.repo / '.git/index').read_bytes(), index)
        self.assertEqual(self.file.read_bytes(), disk)

    def test_line_history_maps_unchanged_unsaved_lines_and_rejects_changed_ranges(self):
        first = self.git('rev-parse', 'HEAD').stdout.decode().strip()
        self.file.write_text('base 🛠\nchanged line\n', encoding='utf-8', newline='')
        self.git('commit', '-am', 'Unrelated second line change')
        committed = self.file.read_text(encoding='utf-8')
        result = self.invoke('selectionHistory', text=committed, startLine=1, endLine=1)
        self.assertNotIn('error', result)
        self.assertEqual([c['id'] for c in result['history']['commits']], [first])
        mapped = self.invoke('selectionHistory', text='unsaved inserted line\n' + committed, startLine=2, endLine=2)
        self.assertEqual([c['id'] for c in mapped['history']['commits']], [first])
        changed = self.invoke('selectionHistory', text='edited first line\nchanged line\n', startLine=1, endLine=1)
        self.assertIn('error', changed)
        self.assertIn('error', self.invoke('selectionHistory', text=committed, startLine=0, endLine=1))

    def test_history_pages_continue_without_duplicates_and_validate_revision_paths(self):
        branch = self.git('symbolic-ref', '--short', 'HEAD').stdout.decode().strip()
        stream = bytearray()
        for i in range(104):
            message = f'Page fixture {i}'.encode()
            body = f'line {i}\n'.encode()
            stream += f'commit refs/heads/paged\ncommitter Fixture <fixture@example.invalid> {1700000000+i} +0000\ndata {len(message)}\n'.encode() + message + b'\n'
            stream += f'M 100644 inline paged.txt\ndata {len(body)}\n'.encode() + body + b'\n'
        stream += b'done\n'
        result = subprocess.run([GIT, '-C', str(self.repo), 'fast-import', '--quiet'], input=stream,
                                env=self.env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.git('checkout', 'paged')
        path = self.repo / 'paged.txt'
        first = self.invoke('history', path=path)['history']
        self.assertEqual(len(first['commits']), 100)
        self.assertTrue(first['more'])
        second = self.invoke('history', path=path, cursor=first['cursor'])['history']
        self.assertEqual(len(second['commits']), 4)
        self.assertFalse(second['more'])
        self.assertEqual(len({c['id'] for c in first['commits']+second['commits']}), 104)
        revision = first['commits'][0]['id']
        self.assertIn('error', self.invoke('revision', path=path, beforeRevision='--help'))
        self.assertIn('error', self.invoke('revision', path=path, beforeRevision=revision, beforePath='../outside'))

    def test_head_index_staged_and_unsaved_buffer_are_distinct_and_read_only(self):
        staged = 'staged 🛠\nsecond line\n'
        disk = 'disk 🛠\nsecond line\n'
        buffer = 'live 🛠\nsecond line\n'
        self.file.write_text(staged, encoding='utf-8', newline='')
        self.git('add', '--', self.file.name)
        self.file.write_text(disk, encoding='utf-8', newline='')
        index = (self.repo / '.git/index').read_bytes()
        metadata = self.invoke()
        self.assertTrue(metadata['available'] and metadata['tracked'] and metadata['staged'])
        self.assertTrue(Path(metadata['root']).samefile(self.repo))
        for command, before, after in [('head', self.base, buffer), ('index', staged, buffer), ('staged', self.base, staged)]:
            result = self.invoke(command, text=buffer)
            self.assertNotIn('error', result)
            self.assertEqual(result['comparison']['before'], before)
            self.assertEqual(result['comparison']['after'], after)
        self.assertEqual(self.file.read_bytes(), disk.encode('utf-8'))
        self.assertEqual((self.repo / '.git/index').read_bytes(), index)

    def test_literal_paths_untracked_and_deleted_versions(self):
        other = self.repo / 'été o --file.cpp'
        other.write_text('wrong file\n', encoding='utf-8')
        self.git('add', '--', other.name)
        self.assertEqual(self.invoke('head')['comparison']['before'], self.base)
        untracked = self.repo / '--untracked [x].txt'
        untracked.write_text('new\n', encoding='utf-8')
        self.assertFalse(self.invoke(path=untracked)['tracked'])
        self.assertEqual(self.invoke('head', untracked)['comparison']['before'], '')
        self.assertEqual(self.invoke('index', untracked)['comparison']['before'], '')
        self.git('rm', '--cached', '--', self.file.name)
        self.assertEqual(self.invoke('staged')['comparison']['after'], '')

    def test_unborn_repository_and_outside_worktree(self):
        unborn = self.root / 'unborn'
        unborn.mkdir()
        self.git('init', cwd=unborn)
        path = unborn / 'new.txt'
        path.write_text('new\n', encoding='utf-8')
        self.git('add', '--', path.name, cwd=unborn)
        result = self.invoke('staged', path)
        self.assertEqual(result['comparison']['before'], '')
        self.assertEqual(result['comparison']['after'], 'new\n')
        outside = self.root / 'outside.txt'
        outside.write_text('outside', encoding='utf-8')
        self.assertFalse(self.invoke(path=outside)['available'])
        self.assertIn('error', self.invoke('head', outside))

    def test_worktrees_nested_repositories_and_detached_head(self):
        worktree = self.root / 'other worktree'
        self.git('worktree', 'add', '-b', 'fixture-worktree', str(worktree))
        result = self.invoke(path=worktree / self.file.name)
        self.assertTrue(Path(result['root']).samefile(worktree))
        self.assertEqual(result['branch'], 'fixture-worktree')
        nested = self.repo / 'nested'
        nested.mkdir()
        self.git('init', cwd=nested)
        path = nested / 'nested.txt'
        path.write_text('nested', encoding='utf-8')
        self.assertTrue(Path(self.invoke(path=path)['root']).samefile(nested))
        self.git('checkout', '--detach')
        self.assertEqual(self.invoke()['branch'], 'Detached HEAD')

    def test_conflicted_index_disables_ambiguous_comparisons(self):
        original_branch = self.git('symbolic-ref', '--short', 'HEAD').stdout.decode().strip()
        self.git('checkout', '-b', 'fixture-side')
        self.file.write_text('side\n', encoding='utf-8')
        self.git('commit', '-am', 'Side fixture')
        self.git('checkout', original_branch)
        self.file.write_text('main\n', encoding='utf-8')
        self.git('commit', '-am', 'Main fixture')
        self.assertNotEqual(self.git('merge', '--no-edit', 'fixture-side', success=False).returncode, 0)
        result = self.invoke()
        self.assertTrue(result['conflicted'])
        self.assertTrue(result['enabled']['head'])
        self.assertFalse(result['enabled']['index'] or result['enabled']['staged'])
        self.assertIn('error', self.invoke('index'))
        self.assertIn('comparison', self.invoke('head'))

    def test_binary_large_blobs_and_encoding(self):
        binary = self.repo / 'binary.txt'
        binary.write_bytes(b'ab\x00cd')
        large = self.repo / 'large.txt'
        large.write_bytes(b'x' * (4 * 1024 * 1024 + 1))
        utf16 = self.repo / 'utf16.txt'
        utf16.write_bytes('é 🛠\r\n'.encode('utf-16'))
        legacy = self.repo / 'legacy.txt'
        legacy.write_bytes('été\r\n'.encode('cp1252'))
        self.git('add', '--', binary.name, large.name, utf16.name, legacy.name)
        self.git('commit', '-m', 'Encoding fixtures')
        self.assertIn('error', self.invoke('head', binary))
        self.assertIn('error', self.invoke('head', large))
        self.assertEqual(self.invoke('head', utf16)['comparison']['before'], 'é 🛠\n')
        self.assertEqual(self.invoke('head', legacy, encoding='windows-1252')['comparison']['before'], 'été\n')

    def test_ambient_git_environment_cannot_redirect_the_document(self):
        changes = {'GIT_DIR': str(self.root / 'wrong.git'), 'GIT_WORK_TREE': str(self.root),
                   'GIT_INDEX_FILE': str(self.root / 'wrong-index')}
        previous = {name: os.environ.get(name) for name in changes}
        os.environ.update(changes)
        try:
            self.assertEqual(self.invoke('head')['comparison']['before'], self.base)
        finally:
            for name, value in previous.items():
                if value is None:
                    os.environ.pop(name, None)
                else:
                    os.environ[name] = value

    def test_missing_git_and_optional_filters_are_not_executed(self):
        previous = os.environ.get('PATH')
        os.environ['PATH'] = ''
        try:
            self.assertIn('error', self.invoke())
        finally:
            if previous is None:
                os.environ.pop('PATH', None)
            else:
                os.environ['PATH'] = previous
        marker = self.root / 'unexpected-command'
        self.git('config', 'core.fsmonitor', f'echo unsafe > "{marker}"')
        self.git('config', 'diff.external', f'echo unsafe > "{marker}"')
        self.assertEqual(self.invoke('head')['comparison']['before'], self.base)
        self.assertFalse(marker.exists())


if __name__ == '__main__':
    unittest.main()
