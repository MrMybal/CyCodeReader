"""Compatibility entry point for the private Pages composer."""
from pathlib import Path
import runpy, sys
work = Path(__file__).resolve().parents[1] / '.CyDocBuild'
sys.path.insert(0, str(work))
if __name__ == '__main__': runpy.run_path(str(work / 'compose.py'), run_name='__main__')
