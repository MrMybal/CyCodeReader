"""Packaging policy checks using source data only; no native executable runs."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from package_windows import validate_build


class DistributionBuildTests(unittest.TestCase):
    def setUp(self):
        self.build = dict(version='0.1.4', platform='Windows', architecture='x64', publicUpdates='ON')

    def test_public_release_requires_update_checks(self):
        validate_build(self.build, '0.1.4', 'Release', True)
        for setting in ('OFF', '', None):
            with self.subTest(setting=setting), self.assertRaises(RuntimeError):
                validate_build(dict(self.build, publicUpdates=setting), '0.1.4', 'Release', True)

    def test_preparation_can_disable_online_checks(self):
        validate_build(dict(self.build, publicUpdates='OFF'), '0.1.4', 'Release')

    def test_version_platform_and_configuration_must_match(self):
        for name, value in [('version', '0.1.3'), ('platform', 'Linux'), ('architecture', 'ARM64')]:
            with self.subTest(name=name), self.assertRaises(RuntimeError):
                validate_build(dict(self.build, **{name: value}), '0.1.4', 'Release', True)
        with self.assertRaises(RuntimeError):
            validate_build(self.build, '0.1.4', 'Debug', True)


if __name__ == '__main__':
    unittest.main()
