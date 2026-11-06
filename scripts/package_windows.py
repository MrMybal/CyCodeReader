"""Prepare public-release Windows x64 portable ZIP and per-user Inno installer.

No publication, credential lookup or application launch is performed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile
from verify_documentation import verify as verify_documentation

ROOT = Path(__file__).resolve().parents[1]
MAX_SIZE = 256 * 1024 * 1024


def version():
    return re.search(r'project\(CyCodeReader VERSION (\d+\.\d+\.\d+)',
                     (ROOT / 'CMakeLists.txt').read_text('utf-8')).group(1)


def find_cmake(build):
    candidate = shutil.which('cmake')
    if candidate:
        return candidate
    cache = (build / 'CMakeCache.txt').read_text('utf-8')
    return re.search(r'^CMAKE_COMMAND:INTERNAL=(.+)$', cache, re.M).group(1).strip()


def allowed(name):
    parts = name.split('/')
    if not name or len(name) > 240 or any(not p or p in {'.', '..', '.CyDocBuild'} or p[-1] in '. ' for p in parts):
        return False
    for part in parts:
        if any(ord(c) < 32 or ord(c) >= 127 or c in '\\:<>"|?*' for c in part):
            return False
        stem = part.split('.')[0].upper()
        if stem in {'CON', 'PRN', 'AUX', 'NUL'} or re.fullmatch(r'(COM|LPT)[0-9]', stem):
            return False
    return (name in {'CyCodeReader.exe', 'CyCodeReaderUpdater.exe', 'portable.flag',
                     'installed.flag', 'package-files.json', 'LICENSE', 'COPYRIGHT',
                     'THIRD_PARTY_NOTICES.md'} or
            (len(parts) == 1 and name.endswith('.dll')) or
            (len(parts) > 1 and parts[0] in {'locales', 'licenses', 'plugins', 'app', 'help', 'Documentation'}) or
            name in {'icons/CyCodeReader.png', 'icons/CyCodeReader.ico',
                     'icons/CyCodeReader-document.png', 'icons/CyCodeReader-document.ico'})


def files_in(stage):
    files = []
    for file in sorted(stage.rglob('*')):
        if file.is_symlink():
            raise RuntimeError('Package must not contain symlinks')
        if file.is_dir():
            continue
        name = file.relative_to(stage).as_posix()
        if not allowed(name):
            raise RuntimeError(f'Unexpected package file: {name}')
        files.append(name)
    if len(files) > 4096 or sum((stage / name).stat().st_size for name in files) > MAX_SIZE:
        raise RuntimeError('Package exceeds native updater limits')
    if len({name.lower() for name in files}) != len(files):
        raise RuntimeError('Case-insensitive package path collision')
    return sorted(files)


def write_manifest(stage, app_version):
    verify_documentation(stage / 'Documentation')
    files = sorted(set(files_in(stage)) | {'package-files.json'})
    (stage / 'package-files.json').write_text(
        json.dumps({'version': app_version, 'files': files}, indent=2) + '\n', 'utf-8')
    return files_in(stage)


def portable_zip(stage, destination, app_version):
    (stage / 'portable.flag').write_text('CyCodeReader portable\n', 'utf-8')
    names = write_manifest(stage, app_version)
    # Stored ZIP is standard/extractable everywhere and lets the native helper
    # use a small, bounded parser without another runtime or archive library.
    with zipfile.ZipFile(destination, 'w', compression=zipfile.ZIP_STORED, allowZip64=False) as archive:
        for name in names:
            entry = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_STORED
            entry.create_system = 0
            entry.external_attr = 0x20
            archive.writestr(entry, (stage / name).read_bytes())
    if destination.stat().st_size > MAX_SIZE:
        raise RuntimeError('Portable archive exceeds native updater limit')
    with zipfile.ZipFile(destination) as archive:
        if archive.testzip() is not None or sorted(archive.namelist()) != names:
            raise RuntimeError('Portable archive verification failed')


def find_iscc(explicit):
    candidates = [explicit, shutil.which('ISCC')]
    for variable in ('ProgramFiles', 'ProgramFiles(x86)', 'LOCALAPPDATA'):
        base = Path(os.environ.get(variable, '.'))
        for suffix in ('Inno Setup 7/ISCC.exe', 'Inno Setup 6/ISCC.exe',
                       'Programs/Inno Setup 7/ISCC.exe', 'Programs/Inno Setup 6/ISCC.exe'):
            candidates.append(base / suffix)
    return next((Path(p) for p in candidates if p and Path(p).is_file()), None)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'runtime/build-vs')
    parser.add_argument('--configuration', default='Release')
    parser.add_argument('--output', type=Path, default=ROOT / 'dist')
    parser.add_argument('--iscc', type=Path)
    parser.add_argument('--portable-only', action='store_true')
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    info = json.loads((build / 'distribution-build.json').read_text('utf-8'))
    app_version = version()
    if info['version'] != app_version or info['platform'] != 'Windows' or info['architecture'] != 'x64':
        raise RuntimeError('Package requires a matching Windows x64 build')
    if args.configuration != 'Release':
        raise RuntimeError('Only Release builds may be packaged')
    iscc = find_iscc(args.iscc)
    if not args.portable_only and not iscc:
        raise RuntimeError('Inno Setup 6.3+ ISCC.exe is required; select --iscc or --portable-only')
    output.mkdir(parents=True, exist_ok=True)
    prefix = f'CyCodeReader-{app_version}-windows-x64-'
    artifacts = [output / (prefix + 'portable.zip')]
    with tempfile.TemporaryDirectory(prefix='reader-package-', dir=output) as temporary:
        stage = Path(temporary) / 'files'
        subprocess.run([find_cmake(build), '--install', str(build), '--config',
                        args.configuration, '--prefix', str(stage)], check=True)
        for name in ('CyCodeReader.exe', 'CyCodeReaderUpdater.exe', 'locales/ui.json',
                     'LICENSE', 'COPYRIGHT', 'msvcp140.dll', 'vcruntime140.dll',
                     'icons/CyCodeReader.png', 'icons/CyCodeReader.ico',
                     'icons/CyCodeReader-document.png', 'icons/CyCodeReader-document.ico',
                     'plugins/git.json', 'plugins/reader_git_plugin.dll'):
            if not (stage / name).is_file():
                raise RuntimeError(f'Incomplete standalone package: {name}')
        portable_zip(stage, artifacts[0], app_version)
        if not args.portable_only:
            (stage / 'portable.flag').unlink()
            (stage / 'installed.flag').write_text('CyCodeReader per-user installation\n', 'utf-8')
            write_manifest(stage, app_version)
            subprocess.run([str(iscc), f'/DAppVersion={app_version}', f'/DSourceDir={stage}',
                            f'/DOutputDir={output}', str(ROOT / 'packaging/windows.iss')], check=True)
            artifacts.append(output / (prefix + 'setup.exe'))
    for file in artifacts:
        if not file.is_file() or file.stat().st_size > MAX_SIZE:
            raise RuntimeError(f'Invalid release artifact: {file.name}')
    sums = ''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in artifacts)
    (output / 'SHA256SUMS.txt').write_text(sums, 'ascii')
    print(json.dumps({'version': app_version, 'publicUpdates': info['publicUpdates'],
                      'artifacts': [str(p) for p in artifacts]}, indent=2))


if __name__ == '__main__':
    main()
