"""Offline update packages made by Python's independent ZIP implementation."""
import json
from pathlib import Path
import sys
import zipfile

destination = Path(sys.argv[1])
destination.mkdir(parents=True, exist_ok=True)
required = {'CyCodeReader.exe': b'new application', 'CyCodeReaderUpdater.exe': b'new updater',
            'portable.flag': b'portable', 'locales/ui.json': b'{"new":true}',
            'plugins/new.json': b'{"plugin":"new"}'}


def archive(name, changes=None, compression=zipfile.ZIP_STORED, manifest=None):
    files = dict(required)
    files.update(changes or {})
    files['package-files.json'] = json.dumps(
        {'version': '0.2.0', 'files': manifest or sorted([*files, 'package-files.json'])}).encode()
    with zipfile.ZipFile(destination / name, 'w', compression=compression) as output:
        for path, data in files.items():
            output.writestr(path, data)


archive('valid.zip')
archive('traversal.zip', {'../outside.txt': b'unsafe'})
archive('data.zip', {'data/reader/preferences.json': b'unsafe'})
archive('device.zip', {'plugins/CON.json': b'unsafe'})
archive('case.zip', {'LOCALes/ui.json': b'unsafe'})
archive('compressed.zip', compression=zipfile.ZIP_DEFLATED)
archive('manifest.zip', manifest=['CyCodeReader.exe'])
corrupt = bytearray((destination / 'valid.zip').read_bytes())
at = corrupt.index(b'new application')
corrupt[at] ^= 1
(destination / 'corrupt.zip').write_bytes(corrupt)
