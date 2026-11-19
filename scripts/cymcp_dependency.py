"""Resolve and freeze the exact local CyMCPSDK sources selected by the host."""
from pathlib import Path
import hashlib,json,os,re,subprocess
ROOT=Path(__file__).resolve().parents[1]
def inputs(source):
    names=subprocess.check_output(['git','-c','safe.directory='+source.as_posix(),'-C',str(source),'ls-files','-z']).decode('utf-8').split('\0')
    return [name for name in names if name and (name.split('/')[0] in {'include','src','cmake','third_party'} or '/' not in name and name in {'CMakeLists.txt','LICENSE','README.md','README.fr.md','VERSION.md','VERSION.fr.md','THIRD_PARTY_NOTICES.md'})]
def source_bytes(path):
    data=path.read_bytes()
    # Source text hashes must survive Git's checkout line-ending policy.
    if b'\0' not in data:
        try:data.decode('utf-8')
        except UnicodeDecodeError:return data
        return data.replace(b'\r\n',b'\n')
    return data
def manifest(source):return {n:hashlib.sha256(source_bytes(source/n)).hexdigest() for n in sorted(inputs(source))}
def resolve():
    lock=json.loads((ROOT/'cymcp-source.lock.json').read_text(encoding='utf-8'))
    frozen=ROOT/'runtime/deps'/('cymcp-'+lock['sourceDigest'][:16])
    if frozen.is_dir() and all((frozen/n).is_file() and hashlib.sha256((frozen/n).read_bytes()).hexdigest()==h for n,h in lock['files'].items()):return frozen
    source=Path(os.environ.get('CYMCP_SOURCE_DIR',str(ROOT.parent/'CyMCPSDK'))).resolve()
    if not source.is_dir():
        source=ROOT/'runtime/deps/cymcp-checkout'
        fresh=not source.exists()
        if fresh:subprocess.run(['git','clone','--no-checkout',lock['repository'],str(source)],check=True)
        git=['git','-c','safe.directory='+source.as_posix(),'-C',str(source)]
        if not fresh and subprocess.check_output([*git,'status','--porcelain']).strip():raise RuntimeError('Preserve modified SDK dependency checkout')
        if subprocess.run([*git,'cat-file','-e',lock['revision']+'^{commit}'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode:
            subprocess.run([*git,'fetch','origin',lock['revision']],check=True)
        subprocess.run([*git,'checkout','--detach',lock['revision']],check=True)
    current=manifest(source)
    if current!=lock['files']:raise RuntimeError('CyMCPSDK sources changed: review and update the source manifest; local work was preserved')
    digest=hashlib.sha256(json.dumps(current,sort_keys=True).encode()).hexdigest()
    if digest!=lock['sourceDigest']:raise RuntimeError('CyMCPSDK source manifest digest mismatch')
    frozen=ROOT/'runtime/deps'/('cymcp-'+digest[:16]);frozen.mkdir(parents=True,exist_ok=True)
    for name,expected in current.items():
        data=source_bytes(source/name)
        if hashlib.sha256(data).hexdigest()!=expected:raise RuntimeError('CyMCPSDK changed while freezing sources; retry after review')
        target=frozen/name;target.parent.mkdir(parents=True,exist_ok=True)
        if target.exists() and target.read_bytes()!=data:raise RuntimeError('Preserve modified SDK build snapshot')
        if not target.exists():target.write_bytes(data)
    return frozen
if __name__=='__main__':print(resolve())
