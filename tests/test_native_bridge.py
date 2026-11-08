"""Opt-in native CLI/IPC acceptance. CI only; never imported by source checks."""
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

def main():
    cli=Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='reader-native-bridge-') as temporary:
        root=Path(temporary);source=root/'été.cpp';source.write_bytes(b'int original;\r\n')
        descriptor=root/'endpoint.json'
        process=subprocess.Popen([str(cli),'--serve',str(descriptor),str(source)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        try:
            deadline=time.monotonic()+15
            while not descriptor.exists():
                if process.poll() is not None:raise RuntimeError('Native text host exited: '+process.stderr.read().decode(errors='replace'))
                if time.monotonic()>deadline:raise RuntimeError('Native descriptor was not published')
                time.sleep(.01)
            endpoint=json.loads(descriptor.read_text(encoding='utf-8'))
            sequence=0
            def call(method,params=None,**overrides):
                nonlocal sequence
                sequence+=1
                request={'protocol':'cycode-core/1','id':sequence,'token':endpoint['token'],'method':method,'params':params or {}}
                request.update(overrides)
                with socket.create_connection((endpoint['host'],endpoint['port']),timeout=5) as connection:
                    connection.sendall((json.dumps(request)+'\n').encode())
                    with connection.makefile('rb') as reader:response=json.loads(reader.readline(20*1024*1024+1))
                return response
            status=call('service.status')['result']
            assert endpoint['protocol']=='cycodereader-native/1' and endpoint['coreProtocol']=='cycode-core/1'
            assert status['profile']=='text' and status['analysis']['supported'] is False
            document=call('documents.list')['result']['documents'][0]
            document_id=document['id'];original_revision=document['revision']
            changed=call('documents.replace',{'id':document_id,'expectedRevision':original_revision,'text':'int unsaved;\n'})['result']
            assert changed['dirty'] and source.read_bytes()==b'int original;\r\n'
            stale=call('documents.replace',{'id':document_id,'expectedRevision':original_revision,'text':'overwrite'})
            assert stale['error']['code']=='RevisionConflict' and stale['id']==sequence
            assert call('documents.list',protocol='unsupported/99')['error']['code']=='UnsupportedProtocol'
            assert call('documents.list',token='wrong')['error']['code']=='TransportError'
            source.write_bytes(b'int external;\n')
            conflict=call('documents.save',{'id':document_id,'expectedRevision':changed['revision']})
            assert conflict['error']['code']=='DiskConflict'
            assert call('documents.read',{'id':document_id})['result']['text']=='int unsaved;\n'
            process.stdin.close();process.wait(timeout=15)
            assert process.returncode==0 and not descriptor.exists()
            assert source.read_bytes()==b'int external;\n'
        finally:
            if not process.stdin.closed:process.stdin.close()
            if process.poll() is None:
                try:process.wait(timeout=15)
                except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
            process.stdout.close();process.stderr.close()
    print('Reader uses the shared native session/envelope: revisions, disk conflicts, compatibility descriptor and EOF shutdown passed.')

if __name__=='__main__':main()
