"""Install the real pinned CyToolsCore/MCP in Reader's private Python environment."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import venv

ROOT=Path(__file__).resolve().parents[1]
VERSION='0.10.1'
SDK_SHA256='5f2f6f0eccdcce960ff7ff9deee36280a4ee1c671cc2ba29b103fa15bb479702'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-wheel',type=Path,default=ROOT.parent/'CyToolsCore/dist'/f'cytools_core-{VERSION}-py3-none-any.whl')
    args=parser.parse_args();wheel=args.sdk_wheel.resolve()
    if not wheel.is_file(): raise SystemExit('Provide the owner\'s CyToolsCore 0.10.1 wheel with --sdk-wheel; it is not redistributed in this repository.')
    if hashlib.sha256(wheel.read_bytes()).hexdigest()!=SDK_SHA256: raise SystemExit('CyToolsCore wheel checksum mismatch')
    environment=ROOT/'runtime/python';python=environment/('Scripts/python.exe' if os.name=='nt' else 'bin/python')
    if not python.exists(): venv.EnvBuilder(with_pip=True).create(environment)
    subprocess.run([str(python),'-m','pip','install',str(wheel),'mcp==1.30.0'],check=True)
    subprocess.run([str(python),'-c','from cytools_core import load_manifest; import sys; load_manifest(sys.argv[1])',str(ROOT/'app/CyTool.json')],check=True)
    print('Reader SDK runtime ready:',python)

if __name__=='__main__': main()
