"""Resolver command-line contracts. No Git, download or native execution."""
import contextlib
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
if __name__=='__main__':unittest.main()
