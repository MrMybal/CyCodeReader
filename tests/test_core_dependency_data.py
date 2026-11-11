"""Resolver command-line contracts. No Git, download or native execution."""
import contextlib
import json
import tempfile
import importlib.util
import io
from pathlib import Path
import unittest
from unittest.mock import patch
spec=importlib.util.spec_from_file_location('core_dependency',Path(__file__).resolve().parents[1]/'editor/scripts/core_dependency.py')
dependency=importlib.util.module_from_spec(spec);spec.loader.exec_module(dependency)
class ResolverInput(unittest.TestCase):
    def test_empty_cmake_source_uses_the_pinned_dependency(self):
        with patch.object(dependency,'resolve',return_value=Path('/resolved/core')) as resolve,contextlib.redirect_stdout(io.StringIO()):
            dependency.main(['--source',''])
        resolve.assert_called_once_with(None)
    def test_explicit_source_is_retained(self):
        with patch.object(dependency,'resolve',return_value=Path('/resolved/core')) as resolve,contextlib.redirect_stdout(io.StringIO()):
            dependency.main(['--source','source with spaces'])
        resolve.assert_called_once_with('source with spaces')
    def fixture(self, directory):
        source=Path(directory)/'native source'
        (source/'common').mkdir(parents=True)
        (source/'common/CMakeLists.txt').write_text('# source fixture\n',encoding='utf-8')
        (source/'common/api-version.txt').write_text('1\n',encoding='utf-8')
        (source/'api-version.txt').write_text('1\n',encoding='utf-8')
        lock=Path(directory)/'lock.json'
        lock.write_text(json.dumps({'revision':'a'*40,'nativeSourceApi':1}),encoding='utf-8')
        return source,lock
    def test_explicit_checkout_requires_the_exact_revision(self):
        with tempfile.TemporaryDirectory() as directory:
            source,lock=self.fixture(directory)
            with patch.object(dependency,'LOCK',lock),patch.object(dependency.subprocess,'check_output',return_value='b'*40):
                with self.assertRaisesRegex(RuntimeError,'revision mismatch'):dependency.resolve(source)
    def test_explicit_dirty_checkout_is_preserved_and_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            source,lock=self.fixture(directory)
            with patch.object(dependency,'LOCK',lock),patch.object(dependency.subprocess,'check_output',side_effect=['a'*40,' M common/CMakeLists.txt\n']):
                with self.assertRaisesRegex(RuntimeError,'tracked local changes'):dependency.resolve(source)
    def test_clean_exact_checkout_is_accepted_without_fetch_or_checkout(self):
        with tempfile.TemporaryDirectory() as directory:
            source,lock=self.fixture(directory)
            with patch.object(dependency,'LOCK',lock),patch.object(dependency.subprocess,'check_output',side_effect=['a'*40,'']) as git:
                self.assertEqual(dependency.resolve(source),source.resolve())
                self.assertEqual(git.call_count,2)

if __name__=='__main__':unittest.main()
