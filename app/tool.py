"""Compatibility launcher for the native Reader CLI. Python standard library only."""
import os,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    executable='cycodereader_cli.exe' if os.name=='nt' else 'cycodereader_cli'
    candidates=[os.environ.get('CYCODE_READER_CLI',''),str(ROOT/"app"/executable),str(ROOT/executable),shutil.which(executable) or '']
    cli=next((p for p in candidates if p and Path(p).is_file()),None)
    if not cli:
        print('Reader native CLI unavailable. Install the companion next to Reader, or set CYCODE_READER_CLI to its path.',file=sys.stderr);return 1
    args=sys.argv[1:]
    if not any(a in args for a in ('--connect','--bridge-config')) and os.environ.get('CYCODE_READER_BRIDGE'):
        args=['--connect',os.environ['CYCODE_READER_BRIDGE'],*args]
    return subprocess.run([cli,*args]).returncode
if __name__=='__main__':raise SystemExit(main())
