"""Build Reader without launching it. Native tests require explicit opt-in."""
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
    parser.add_argument('--cyui',type=Path,help='Optional clean checkout at editor/cyui-source.lock.json revision')
    parser.add_argument('--build-dir',type=Path,help='Separate build directory for a clean configuration')
    parser.add_argument('--cyui-development',action='store_true',help='Explicit opt-in to an unpublished local CyUI checkout')
    parser.add_argument('--deps',type=Path,help='Use an existing checksum-pinned dependency cache')
    parser.add_argument('--public-updates',action='store_true',help='Enable public GitHub updates in release binaries (no authentication)')
    parser.add_argument('--build-tests',action='store_true',help='Compile native tests in a separate build; do not execute them')
    parser.add_argument('--run-tests',action='store_true',help='Compile and execute native tests explicitly')
    args=parser.parse_args()
    if args.run_tests: args.build_tests=True
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
    name=('tests-core-build' if args.core_only else 'tests-build') if args.build_tests else ('core-build' if args.core_only else 'build')
    build=(args.build_dir or ROOT/'runtime'/(name+('-vs' if sys.platform=='win32' else ''))).resolve()
    build.mkdir(parents=True,exist_ok=True);log=build/'build.log'
    generator=['-G','Visual Studio 17 2022','-A','x64'] if sys.platform=='win32' else ['-G','Ninja',f'-DCMAKE_MAKE_PROGRAM={ninja or "ninja"}']
    commands=[[cmake,'-S',str(ROOT),'-B',str(build),*generator,f'-DCMAKE_BUILD_TYPE={args.configuration}',f'-DCYUI_DIR={args.cyui.resolve() if args.cyui else ""}',f'-DCYCODE_EDITOR_DEPS={args.deps.resolve() if args.deps else ROOT/"runtime/deps"}',f'-DCYCODE_READER_PUBLIC_UPDATES={"ON" if args.public_updates else "OFF"}',f'-DCYCODE_CYUI_DEVELOPMENT={"ON" if args.cyui_development else "OFF"}',f'-DCYCODE_READER_BUILD_GUI={"OFF" if args.core_only else "ON"}',f'-DBUILD_TESTING={"ON" if args.build_tests else "OFF"}'],
              [cmake,'--build',str(build),'--config',args.configuration,'--parallel','4']]
    if not args.build_tests:
        commands[1] += ['--target', *(['cycode_reader_core','cycode_reader_distribution'] if args.core_only else ['CyCodeReader'])]
    with log.open('w',encoding='utf-8') as stream:
        for command in commands:
            result=subprocess.run(command,env=env,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT);stream.flush()
            if result.returncode:
                print('\n'.join(log.read_text('utf-8',errors='replace').splitlines()[-30:]));return result.returncode
    ctest=str(Path(cmake).with_name('ctest.exe' if sys.platform=='win32' else 'ctest'))
    if args.run_tests:
        subprocess.run([ctest,'--test-dir',str(build),'-C',args.configuration,'--output-on-failure','--stop-on-failure','--parallel','1'],env=env,check=True)
    print(f'Build complete: {build}. No desktop application launched. Full log: {log}')
    return 0

if __name__=='__main__': raise SystemExit(main())
