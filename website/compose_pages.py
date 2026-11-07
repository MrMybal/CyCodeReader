"""Compose the verified Reader showcase and historical CyCode reader for Pages."""
import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path
from build import inventory, MARKER
from verify import verify, Page

ROOT = Path(__file__).resolve().parents[1]


def compose(showcase, cycode, output):
    showcase, cycode, output = (path.resolve() for path in (showcase, cycode, output))
    if not any(output.is_relative_to(ROOT / folder) for folder in ('runtime', 'dist')):
        raise RuntimeError('Choose generated output inside runtime/ or dist/')
    if output.exists():
        raise RuntimeError('Use a fresh Pages output; preserve existing files')
    for source in (showcase, cycode):
        if source.is_symlink() or output.is_relative_to(source) or source.is_relative_to(output):
            raise RuntimeError('Inputs and output overlap or use linked paths')
    verify(showcase)
    subprocess.run([sys.executable, '-B', str(ROOT / '.CyDocBuild/compose.py'),
                    '--verify-manual', 'CyCode', str(cycode)], check=True, cwd=ROOT)
    if (showcase / 'cycode').exists():
        raise RuntimeError('The standalone showcase already owns the shared destination')
    baseline = inventory(cycode)
    shutil.copytree(showcase, output)
    shutil.copytree(cycode, output / 'cycode')
    page = Page()
    page.feed((showcase / 'index.html').read_text('utf-8'))
    reader = showcase / Path(json.loads(page.scripts['site-config'])['documentation']).parent
    # Retain runtime resource URLs used by older cached root reader pages.
    shutil.copytree(reader / 'DocData', output / 'DocData', ignore=shutil.ignore_patterns('export-files.json'))
    shutil.copyfile(reader / 'DocAI.md', output / 'DocAI.md')
    if inventory(output / 'cycode') != baseline:
        raise RuntimeError('The historical manual changed while copying')
    metadata = json.loads((output / MARKER).read_text('utf-8'))
    metadata['sharedManuals'] = ['cycode']
    metadata['files'] = {name: digest for name, digest in inventory(output).items() if name != MARKER}
    (output / MARKER).write_text(json.dumps(metadata, sort_keys=True, indent=2) + '\n', 'utf-8')
    verify(output, with_cycode=True)
    print('Pages artifact: Reader showcase at /, complete Reader manual and unchanged CyCode manual at /cycode/.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('showcase', 'cycode', 'output'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    compose(args.showcase, args.cycode, args.output)
