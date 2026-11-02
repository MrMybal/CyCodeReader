"""Build/test Reader without installing or launching the desktop application."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core-only',action='store_true')
    parser.add_argument('--configuration',default='Release')
    parser.add_argument('--cyui',type=Path,default=ROOT.parent/'CyImgui_Interface')
    args=parser.parse_args()
    env=dict(os.environ);cmake=shutil.which('cmake');ninja=shutil.which('ninja')
    if sys.platform=='win32':
        env={key.upper():value for key,value in os.environ.items()};env['VSLANG']='1033'
        vs=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Microsoft Visual Studio/2022/Community'
        vswhere=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
        if vswhere.exists():
            detected=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath']).decode().strip()
            if detected: vs=Path(detected)
        if not cmake: cmake=str(vs/'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
        vcvars=vs/'VC/Auxiliary/Build/vcvars64.bat'
        if vcvars.exists() and not shutil.which('cl'):
            output=subprocess.check_output(chr(34)+str(vcvars)+chr(34)+' >nul && set',shell=True,env=env).decode(errors='replace')
            env.update(dict(line.split('=',1) for line in output.splitlines() if '=' in line and not line.startswith('=')))
        env={key.upper():value for key,value in env.items()}
    if not cmake: raise SystemExit('Install CMake 3.24+ and a C++20 compiler.')
    build=ROOT/'runtime'/(('core-build' if args.core_only else 'build')+('-vs' if sys.platform=='win32' else ''))
    build.mkdir(parents=True,exist_ok=True);log=build/'build.log'
    generator=['-G','Visual Studio 17 2022','-A','x64'] if sys.platform=='win32' else ['-G','Ninja',f'-DCMAKE_MAKE_PROGRAM={ninja or "ninja"}']
    commands=[[cmake,'-S',str(ROOT),'-B',str(build),*generator,f'-DCMAKE_BUILD_TYPE={args.configuration}',f'-DCYUI_DIR={args.cyui}',f'-DCYCODE_READER_BUILD_GUI={"OFF" if args.core_only else "ON"}','-DBUILD_TESTING=ON'],
              [cmake,'--build',str(build),'--config',args.configuration,'--parallel','4']]
    with log.open('w',encoding='utf-8') as stream:
        for command in commands:
            result=subprocess.run(command,env=env,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT);stream.flush()
            if result.returncode:
                print('\n'.join(log.read_text('utf-8',errors='replace').splitlines()[-30:]));return result.returncode
    ctest=str(Path(cmake).with_name('ctest.exe' if sys.platform=='win32' else 'ctest'))
    subprocess.run([ctest,'--test-dir',str(build),'-C',args.configuration,'--output-on-failure'],env=env,check=True)
    print(f'Build complete: {build}. Nothing installed or launched. Full log: {log}')
    return 0

if __name__=='__main__': raise SystemExit(main())
