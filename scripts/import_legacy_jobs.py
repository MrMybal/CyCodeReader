"""Read-only migration of one explicitly selected old client's job receipts.

Run the native CLI with --data-dir DESTINATION invoke automation_status first.
The old SQLite DB and all artifact/user-document paths remain untouched.
"""
import argparse
import ctypes
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import sqlite3


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--database',type=Path,required=True)
    p.add_argument('--destination',type=Path,required=True)
    p.add_argument('--client',required=True,help='Old clientId whose private jobs may be imported')
    p.add_argument('--owner',choices=['read','edit'],required=True)
    args=p.parse_args();database=args.database.resolve();root=args.destination.resolve()
    if not (root/'jobs').is_dir() or not (root/'profile.lock').is_file():p.error('Initialize the destination with the native CLI first')
    lock=None
    if os.name=='nt':
        api=ctypes.WinDLL('kernel32',use_last_error=True)
        api.CreateFileW.restype=ctypes.c_void_p
        api.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p]
        lock=api.CreateFileW(str(root/'profile.lock'),0xc0000000,0,None,3,0,None)
        if lock==ctypes.c_void_p(-1).value:raise RuntimeError('Destination profile is in use')
    else:
        import fcntl
        lock=open(root/'profile.lock','r+b');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    try:
        imported=0
        with sqlite3.connect(database.as_uri()+'?mode=ro',uri=True) as db:
            for identity,value in db.execute("SELECT id,value FROM records WHERE kind='jobs'"):
                job=json.loads(value)
                if job.get('clientId')!=args.client:continue
                identifier='legacy-'+hashlib.sha256((str(database)+'\0'+identity).encode()).hexdigest()[:32]
                target=root/'jobs'/(identifier+'.json')
                if target.exists():continue
                state=job.get('state','unknown').lower()
                status={'completed':'completed','failed':'failed','cancelled':'cancelled'}.get(state,'interrupted-unknown')
                created=job.get('createdAt','')
                try:created=int(datetime.datetime.fromisoformat(created.replace('Z','+00:00')).timestamp()*1000)
                except (ValueError,AttributeError):created=0
                receipt={'id':identifier,'owner':args.owner,'operation':job.get('operationId','legacy'),'status':status,'created':created,'legacy':job}
                if 'result' in job:receipt['result']=job['result']
                payload=json.dumps(receipt,ensure_ascii=False).encode('utf-8')
                if len(payload)>64*1024*1024:raise RuntimeError('Legacy receipt exceeds the native read limit; original preserved')
                # Exclusive creation makes reruns idempotent and never replaces a native receipt.
                fd=os.open(target,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
                with os.fdopen(fd,'wb') as out:out.write(payload);out.flush();os.fsync(out.fileno())
                imported+=1
        print(json.dumps({'imported':imported,'originalPreserved':True,'actionsReplayed':0}))
    finally:
        if os.name=='nt':
            api.CloseHandle.argtypes=[ctypes.c_void_p];api.CloseHandle(lock)
        else:lock.close()


if __name__=='__main__':main()
