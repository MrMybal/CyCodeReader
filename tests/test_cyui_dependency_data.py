"""CyUI source-lock/data policy checks. Never runs Git, a build, or a native fixture."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('cyui_dependency',ROOT/'editor/scripts/cyui_dependency.py')
dependency=importlib.util.module_from_spec(spec);spec.loader.exec_module(dependency)
class LockedCyUI(unittest.TestCase):
    def test_revision_really_contains_zig_and_zon(self):
        lock=json.loads((ROOT/'editor/cyui-source.lock.json').read_text())
        self.assertEqual(lock['revision'],'e8ccec0a96ed820fc46e86059e010efafcef687e')
        source=ROOT/'runtime/deps'/('cyui-'+lock['revision'][:12])
        if not source.exists():self.skipTest('Run the locked dependency resolver first')
        catalogue=json.loads((source/'data/code-languages.json').read_text(encoding='utf-8'))
        by_id={item['id']:item for item in catalogue}
        self.assertEqual(len(catalogue),104)
        self.assertIn('zig',by_id['zig']['extensions']);self.assertIn('zon',by_id['zon']['extensions'])
        text=(source/'include/cyui/detail/CodeLanguageList.inc').read_text()
        self.assertIn('zig =',text);self.assertIn('zon =',text)
        self.assertTrue((source/'third_party/lexilla/lexers/LexZig.cxx').is_file())
        self.assertIn('SCE_ZIG_', (source/'src/modules/code/CodeSyntax.cpp').read_text())
    def test_clean_exact_source_does_not_fetch_or_checkout(self):
        lock=json.loads((ROOT/'editor/cyui-source.lock.json').read_text())
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)
            with patch.object(dependency,'git',side_effect=[lock['revision'],'']) as git:
                self.assertEqual(dependency.resolve(source,source),source.resolve())
                self.assertEqual(git.call_count,2)
    def test_dirty_checkout_is_refused_and_preserved(self):
        lock=json.loads((ROOT/'editor/cyui-source.lock.json').read_text())
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory);marker=source/'local.txt';marker.write_text('preserve')
            with patch.object(dependency,'git',side_effect=[lock['revision'],' M src/modules/code/CodeSyntax.cpp']):
                with self.assertRaisesRegex(RuntimeError,'local changes'):dependency.resolve(source,source)
            self.assertEqual(marker.read_text(),'preserve')
    def test_previous_revision_is_not_advertised_as_zig(self):
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)
            with patch.object(dependency,'git',return_value='6ef01cfcdb197f3a89d64618ce5ef8818c704728') as git:
                with self.assertRaisesRegex(RuntimeError,'revision mismatch'):dependency.resolve(source,source)
                self.assertEqual(git.call_count,1)
if __name__=='__main__':unittest.main()
