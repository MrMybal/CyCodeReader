"""Fetch checksum-pinned build sources only; never install IDE/runtime tools."""
import argparse
import hashlib
import json
from pathlib import Path
import tarfile
import sys
import urllib.request

ROOT=Path(__file__).resolve().parents[1]

def fetch(destination,core_only=False):
    destination.mkdir(parents=True,exist_ok=True)
    lock=json.loads((ROOT/'dependencies.lock.json').read_text('utf-8'))
    for key,item in lock.items():
        if core_only and key!='json': continue
        target=destination/key;marker=target/'.cycode-dependency'
        if marker.exists():
            if marker.read_text('utf-8')!=item['sha256']: raise RuntimeError(f'{key}: cached revision differs from lock')
            print(f'{key}: cached',flush=True);continue
        archive=destination/(key+'.tar.gz')
        if not archive.exists():
            request=urllib.request.Request(item['url'],headers={'User-Agent':'CyCodeReader-build/0.1'})
            with urllib.request.urlopen(request,timeout=120) as response: archive.write_bytes(response.read())
        if hashlib.sha256(archive.read_bytes()).hexdigest()!=item['sha256']: raise RuntimeError(f'{key}: checksum mismatch')
        target.mkdir(exist_ok=True)
        with tarfile.open(archive) as package:
            for member in package.getmembers():
                parts=Path(member.name).parts
                if len(parts)<2: continue
                member.name=str(Path(*parts[1:]));package.extract(member,target,filter='data')
        marker.write_text(item['sha256'],encoding='utf-8')
        print(f'{key}: {item["revision"]}',flush=True)

    if not core_only:
        sys.path.insert(0, str(ROOT/'editor/scripts'))
        from cyui_dependency import resolve
        print(f'CyUI: {resolve(destination/"CyUI")}',flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core-only',action='store_true')
    parser.add_argument('--destination',type=Path,default=ROOT/'runtime/deps')
    args=parser.parse_args();fetch(args.destination.resolve(),args.core_only)
