"""Compose verified Reader and CyCode exports for one GitHub Pages project."""
import argparse
import hashlib
import json
from pathlib import Path
from build_documentation import publish
from verify_documentation import verify

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reader',type=Path)
    parser.add_argument('cycode',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    reader,cycode,output=[p.resolve() for p in (args.reader,args.cycode,args.output)]
    if output.exists():
        raise RuntimeError('Compose into a fresh directory; preserve previous outputs')
    for site,name in [(reader,'CyCodeReader'),(cycode,'CyCode')]:
        verify(site)
        if json.loads((site/'manifest.json').read_text('utf-8'))['name']!=name:
            raise RuntimeError('Wrong manual for this publication location')
        if output.is_relative_to(site) or site.is_relative_to(output):
            raise RuntimeError('Inputs and output must be separate')
    publish(reader,output)
    publish(cycode,output/'cycode')
    # Extend the outer ownership manifest to cover the nested, complete reader.
    # Each inner manifest and every reader asset remain byte-for-byte unchanged.
    files={p.relative_to(output).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
           for p in output.rglob('*') if p.is_file() and p!=output/'cydoc-export.json'}
    (output/'cydoc-export.json').write_text(json.dumps({'format':1,'generator':'CyDoc application help','files':files},indent=2)+'\n',encoding='utf-8')
    verify(output)
    verify(output/'cycode')
    print('Pages artifact ready: Reader at /; CyCode at /cycode/.')

if __name__=='__main__': main()
