"""Compose only verified documentation exports for the shared Pages site."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path
from urllib.parse import unquote, urlsplit
from build import publish
from verify import References


def verify_manual(site, name):
    current = (site / 'DocData/export-files.json').is_file()
    data = site / 'DocData' if current else site
    marker = 'DocData/export-files.json' if current else 'cydoc-export.json'
    owned = json.loads((site / marker).read_text('utf-8'))['files']
    actual = set()
    for path in site.rglob('*'):
        if path.is_symlink(): raise RuntimeError('Linked documentation path')
        if path.is_file(): actual.add(path.relative_to(site).as_posix())
    if actual != set(owned) | {marker}: raise RuntimeError('Unknown or missing documentation files')
    def local(relative, base=data):
        url = urlsplit(relative)
        path = (base / unquote(url.path)).resolve()
        if url.scheme or url.netloc or not path.is_relative_to(site) or not path.is_file():
            raise RuntimeError('Missing, remote or unsafe documentation resource')
        return path
    for relative, digest in owned.items():
        path = local(relative, site)
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest: raise RuntimeError('Changed documentation file')
        if path.stat().st_size >= 64*1024*1024: raise RuntimeError('Oversized documentation resource')
        if any(part in {'.CyDocBuild','internal','runtime','node_modules'} for part in Path(relative).parts):
            raise RuntimeError('Private build path in documentation')
    if sum((site / p).stat().st_size for p in owned) > 256*1024*1024: raise RuntimeError('Oversized documentation export')
    manifest = json.loads((data / 'manifest.json').read_text('utf-8'))
    if manifest['name'] != name or manifest['defaultLocale'] != 'en': raise RuntimeError('Wrong documentation location/default language')
    if {entry['code'] for entry in manifest['locales']} != {'en','fr'}: raise RuntimeError('Unexpected locales')
    if {p.name for p in data.glob('media-*.js')} != {'media-image.js','media-gallery.js'}: raise RuntimeError('Unexpected modules')
    if current:
        if not (site / 'Documentation.html').is_file() or not (site / 'DocAI.md').is_file(): raise RuntimeError('Incomplete reader')
        report = json.loads((data / 'translation-report.json').read_text('utf-8'))
        if any(report.get(key) for key in ('missing','outdated','untracked')): raise RuntimeError('Incomplete translations')
        if any(p.get('kind')=='source' or p.get('sourcePageId') or p.get('source') for p in manifest['pages']):
            raise RuntimeError('Implementation browser in human manual')
    pages={(p['locale'],p['id']):p for p in manifest['pages']}
    if current and {p[1] for p in pages if p[0]=='en'} != {p[1] for p in pages if p[0]=='fr'}: raise RuntimeError('Missing translated pages')
    for page in pages.values():
        for resource in page.get('assets', []): local(resource)
        for resource in References(page['html']).assets: local(resource)
    shell=site / ('Documentation.html' if current else 'index.html')
    for resource in References(shell.read_text('utf-8')).assets: local(resource, site)
    return marker


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-manual', nargs=2, metavar=('NAME','SITE'), help='Validate one unchanged manual export without composing an artifact')
    for name in ('reader','cycode','output'): parser.add_argument(name,type=Path,nargs='?')
    args=parser.parse_args()
    if args.verify_manual:
        if any((args.reader,args.cycode,args.output)): parser.error('Do not combine manual verification and composition')
        name,site=args.verify_manual
        verify_manual(Path(site).resolve(),name)
        print(name + ': complete manual export verified.')
        return
    if not all((args.reader,args.cycode,args.output)): parser.error('reader, cycode and output are required')
    reader,cycode,output=[p.resolve() for p in (args.reader,args.cycode,args.output)]
    if output.exists(): raise RuntimeError('Use a fresh Pages output; preserve existing files')
    for site,name in ((reader,'CyCodeReader'),(cycode,'CyCode')):
        verify_manual(site,name)
        if output.is_relative_to(site) or site.is_relative_to(output): raise RuntimeError('Inputs and output overlap')
    publish(reader,output)
    shutil.copytree(cycode,output/'cycode')
    marker='DocData/export-files.json' if (output/'DocData/export-files.json').is_file() else 'cydoc-export.json'
    metadata=json.loads((output/marker).read_text('utf-8'))
    metadata['files']={p.relative_to(output).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in output.rglob('*') if p.is_file() and p!=output/marker}
    (output/marker).write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
    verify_manual(output,'CyCodeReader');verify_manual(output/'cycode','CyCode')
    print('Pages artifact verified: Reader at /; CyCode at /cycode/.')


if __name__=='__main__': main()
