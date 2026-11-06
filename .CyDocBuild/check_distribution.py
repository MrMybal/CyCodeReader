"""Data-only export/packaging checks; no Reader executable is launched."""
import argparse, hashlib, json, runpy, sys, tempfile, zipfile
from pathlib import Path
from build import inventory,publish
from verify import verify

def main():
    parser=argparse.ArgumentParser();parser.add_argument('site',type=Path);args=parser.parse_args()
    site=args.site.resolve();work=Path(__file__).resolve().parent;verify(site)
    sys.path.insert(0,str(work.parent/'scripts'))
    package=runpy.run_path(str(work.parent/'scripts/package_windows.py'))
    assert package['allowed']('Documentation/DocData/viewer.js')
    assert not package['allowed']('app/.CyDocBuild/cydoc.config.json')
    with tempfile.TemporaryDirectory(dir=work/'.cache',prefix='distribution-') as temporary:
        root=Path(temporary);assert root.resolve().is_relative_to((work/'.cache').resolve())
        target=root/'flat';(target/'assets').mkdir(parents=True)
        old=target/'assets/old.txt';old.write_text('old resource',encoding='utf8')
        (target/'cydoc-export.json').write_text(json.dumps({'files':{'assets/old.txt':hashlib.sha256(old.read_bytes()).hexdigest()}}),encoding='utf8')
        publish(site,target);assert inventory(site)==inventory(target);assert not (target/'assets').exists()
        local=target/'user.txt';local.write_text('preserve',encoding='utf8')
        try:publish(site,target)
        except RuntimeError:pass
        else:raise AssertionError('Unknown output must be preserved')
        assert local.read_text('utf8')=='preserve';local.unlink()
        changed=target/'DocAI.md';changed.write_text('local edit',encoding='utf8')
        try:publish(site,target)
        except RuntimeError:pass
        else:raise AssertionError('Modified output must be preserved')
        archive=root/'manual.zip';files=inventory(site)
        with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as stream:
            for name in files:stream.write(site/name,'Documentation/'+name)
        with zipfile.ZipFile(archive) as stream:
            assert set(stream.namelist())=={'Documentation/'+name for name in files}
            assert all(hashlib.sha256(stream.read('Documentation/'+name)).hexdigest()==digest for name,digest in files.items())
    print('Reader: flat-layout upgrade, local-file preservation and complete documentation archive passed.')

if __name__=='__main__':main()
