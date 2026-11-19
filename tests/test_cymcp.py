"""Opt-in real native Reader CLI/HTTP/MCP acceptance using synthetic private files."""
import contextlib
import hashlib
import json
import os
from pathlib import Path
import queue
import sqlite3
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request


class Lines:
    def __init__(self,argv):
        self.p=subprocess.Popen([str(a) for a in argv],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8',bufsize=1)
        self.q=queue.Queue()
        def read():
            for line in self.p.stdout:self.q.put(json.loads(line))
        self.thread=threading.Thread(target=read,daemon=True);self.thread.start();self.sequence=0
    def call(self,message,rpc=False):
        self.sequence+=1
        if rpc:message={'jsonrpc':'2.0','id':self.sequence,**message}
        self.p.stdin.write(json.dumps(message)+'\n');self.p.stdin.flush()
        deadline=time.monotonic()+20
        while time.monotonic()<deadline:
            try:r=self.q.get(timeout=.1)
            except queue.Empty:
                if self.p.poll() is not None:raise RuntimeError(self.p.stderr.read())
                continue
            if not rpc or r.get('id')==self.sequence:return r
        raise TimeoutError(message.get('method'))
    def close(self):
        self.p.stdin.close()
        try:self.p.wait(timeout=20)
        except subprocess.TimeoutExpired:self.p.kill();self.p.wait();raise
        error=self.p.stderr.read();self.thread.join(2);self.p.stdout.close();self.p.stderr.close()
        assert self.p.returncode==0,error
    def __enter__(self):return self
    def __exit__(self,*exc):self.close()


def invoke(name,arguments=None):return {'method':'invoke','name':name,'arguments':arguments or {}}


@contextlib.contextmanager
def service(cli,root,files=(),config=None):
    descriptor=root/'connection.json'
    command=[cli,'--data-dir',root,'serve','--descriptor',descriptor]
    if config:command+=['--config',config]
    for file in files:command+=['--file',file]
    p=subprocess.Popen([str(x) for x in command],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    try:
        deadline=time.monotonic()+20
        while not descriptor.exists():
            if p.poll() is not None:raise RuntimeError(p.stderr.read().decode())
            if time.monotonic()>deadline:raise TimeoutError('Reader descriptor')
            time.sleep(.01)
        yield json.loads(descriptor.read_text()),descriptor
    finally:
        p.stdin.close()
        try:p.wait(timeout=20)
        except subprocess.TimeoutExpired:p.kill();p.wait();raise
        error=p.stderr.read().decode();p.stdout.close();p.stderr.close()
        assert p.returncode==0,error
        assert not descriptor.exists()


def request(endpoint,role,message,mcp=False,session=None):
    headers={'Content-Type':'application/json','Authorization':'Bearer '+endpoint[role+'Token']}
    if session:headers['Mcp-Session-Id']=session;headers['MCP-Protocol-Version']='2025-11-25'
    req=urllib.request.Request(endpoint['mcpUrl' if mcp else 'url'],data=json.dumps(message).encode(),headers=headers)
    try:
        with urllib.request.urlopen(req,timeout=20) as response:return json.loads(response.read()),response.headers
    except urllib.error.HTTPError as error:return json.loads(error.read()),error.headers


def main():
    cli=Path(sys.argv[1]).resolve();source=Path(__file__).resolve().parents[1]
    report=[]
    with tempfile.TemporaryDirectory(prefix='reader-cymcp-') as temporary:
        root=Path(temporary)
        def run(*args,code=0):
            r=subprocess.run([str(cli),*[str(x) for x in args]],capture_output=True,text=True,encoding='utf-8',timeout=25)
            assert r.returncode==code,(r.returncode,r.stderr,r.stdout[:100]);return r
        assert 'describe' in run('--help').stdout
        ops=json.loads(run('--data-dir',root/'list','list').stdout);assert len(ops)==22
        assert json.loads(run('--data-dir',root/'list','describe','documents_read').stdout)['name']=='documents_read'
        assert json.loads(run('--data-dir',root/'list','invoke','automation_status','--json','{}').stdout)['sdk']=='CyMCPSDK'
        run('--data-dir',root/'list','invoke','documents_new','--json','{}',code=3)
        run('--data-dir',root/'list','invoke','documents_read','--json','{}',code=2)
        report.append('CLI help/list/describe/invoke, JSON and exit codes')
        with Lines([cli,'--data-dir',root/'jsonl','--access','edit','jsonl']) as lines:
            def call(n,a=None):return lines.call(invoke(n,a))
            doc=call('documents_new')['result'];guard={'id':doc['id'],'expectedRevision':doc['revision']}
            doc=call('documents_replace',{**guard,'text':'int synthetic;\n'})['result']
            assert call('documents_replace',{**guard,'text':'stale'})['error']['code']=='RevisionConflict'
            path=root/'été.cpp';doc=call('documents_save_as',{'id':doc['id'],'expectedRevision':doc['revision'],'path':str(path)})['result']
            assert path.read_text()=='int synthetic;\n'
            doc=call('documents_replace',{'id':doc['id'],'expectedRevision':doc['revision'],'text':'int unsaved;\n'})['result']
            assert path.read_text()=='int synthetic;\n'
            assert 'error' in call('documents_close',{'id':doc['id'],'expectedRevision':doc['revision']})
            path.write_text('external\n')
            assert 'error' in call('documents_save',{'id':doc['id'],'expectedRevision':doc['revision']})
            jobs=call('jobs_list')['result']['jobs'];receipt=next(j for j in jobs if j['operation']=='documents_replace' and j['status']=='completed')
            assert call('jobs_get',{'id':receipt['id']})['result']['result']['text'].startswith('int ')
            assert 'error' in call('jobs_forget',{'id':receipt['id'],'forget':True,'confirm':False})
            assert call('jobs_forget',{'id':receipt['id'],'forget':True,'confirm':True})['result']['forgotten']==receipt['id']
        assert json.loads(run('--data-dir',root/'jsonl','--access','edit','invoke','jobs_list','--json','{}').stdout)['total']>0
        report.append('Persistent JSONL buffers, save-as/dirty-close/disk/revision guards, owner receipts and explicit forgetting')
        config=root/'settings.json';config.write_text(json.dumps({'metrics':True,'resultTtlSeconds':2,'cacheTtlSeconds':60}))
        text='SYNTHETIC_SECRET '+'😀café '*130000+'\n';file=root/'large.txt';file.write_text(text,encoding='utf-8')
        with service(cli,root/'http',[file],config) as (ep,descriptor):
            def call(n,a=None,role='edit'):return request(ep,role,invoke(n,a))[0]
            doc=call('documents_list')['documents'][0];doc=call('documents_read',{'id':doc['id']})
            assert call('documents_replace',{'id':doc['id'],'expectedRevision':doc['revision'],'text':'denied'},'read')['error']['code']=='AccessDenied'
            assert request(ep,'read',{**invoke('documents_new'),'principal':'edit'})[0]['error']['code']=='AccessDenied'
            run('--connect',descriptor,'--access','read','invoke','documents_new','--json','{}',code=3)
            assert len(json.loads(run('--connect',descriptor,'list').stdout))==22
            before=descriptor.read_bytes()
            duplicate=subprocess.run([str(cli),'--data-dir',str(root/'collision'),'serve','--descriptor',str(descriptor)],input='\n',capture_output=True,text=True,timeout=15)
            assert duplicate.returncode==1 and 'DescriptorBusy' in duplicate.stderr and descriptor.read_bytes()==before
            busy=run('--data-dir',root/'http','invoke','automation_status',code=1);assert 'ProfileBusy' in busy.stderr
            init,headers=request(ep,'edit',{'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':'fixture','version':'1'}}},True)
            assert 'result' in init,init
            session=headers.get('Mcp-Session-Id');assert session
            def mcp(method,params=None,role='edit'):
                return request(ep,role,{'jsonrpc':'2.0','id':2,'method':method,'params':params or {}},True,session)[0]
            tools=mcp('tools/list')['result']['tools'];assert len(tools)==5
            result=mcp('tools/call',{'name':'cy_tools_call','arguments':{'name':'documents_read','arguments':{'id':doc['id']}}})['result'];assert not result['isError'],result
            uri=result['structuredContent']['resource']['uri']
            assert call('automation_status')['resourceStorage']['disk_bytes']>1024*1024
            def resource(u,role='edit'):return mcp('tools/call',{'name':'cy_resources_read','arguments':{'uri':u}},role)['result']
            manifest=json.loads(resource(uri)['structuredContent']['contents'][0]['text']);u=manifest['firstUri'];chunks=[]
            while u:
                part=resource(u)['structuredContent'];chunks.append(part['contents'][0]['text']);u=part.get('_meta',{}).get('com.cymcp/chunk',{}).get('nextUri')
            recovered=json.loads(''.join(chunks));assert recovered['text']==text
            assert resource(uri,'read')['isError']
            snapshots=mcp('resources/list')['result']['resources'];assert snapshots
            snapshot=snapshots[0]['uri']
            updated=call('documents_replace',{'id':doc['id'],'expectedRevision':doc['revision'],'text':'changed\n'})
            assert updated['text']=='changed\n'
            assert mcp('resources/read',{'uri':snapshot}).get('error'), 'stale snapshot must be invalidated'
            time.sleep(2.1);assert resource(uri)['isError']
            assert call('documents_read',{'id':doc['id']})['text']=='changed\n'
            receipts=call('jobs_list')['jobs'];durable=next(j for j in receipts if j['operation']=='documents_replace' and j['status']=='completed')['id']
            metrics=(root/'http/performance.jsonl').read_text();assert 'SYNTHETIC_SECRET' not in metrics and 'café' not in metrics and ep['editToken'] not in metrics
        assert json.loads(run('--data-dir',root/'http','--access','edit','invoke','jobs_get','--json',json.dumps({'id':durable})).stdout)['result']['text']=='changed\n'
        report.append('Actual HTTP instance/CLI policy, MCP discovery/private UTF-8 chunks, TTL/no replay, invalidation, metadata-only metrics and restart recovery')
        with Lines([cli,'--data-dir',root/'stdio','--access','edit','--full-tools','mcp']) as pipe:
            init=pipe.call({'method':'initialize','params':{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':'fixture','version':'1'}}},True);assert 'result' in init
            tools=pipe.call({'method':'tools/list'},True)['result']['tools'];assert len(tools)==22
            new=pipe.call({'method':'tools/call','params':{'name':'documents_new','arguments':{}}},True)['result'];assert new['structuredContent']['id']
        report.append('Real MCP stdio full discovery and native operation')
        quota=root/'quota.json';quota.write_text(json.dumps({'maxDurableJobs':1}))
        with Lines([cli,'--data-dir',root/'quota','--config',quota,'--access','edit','jsonl']) as pipe:
            doc=pipe.call(invoke('documents_new'))['result']
            denied=pipe.call(invoke('documents_replace',{'id':doc['id'],'expectedRevision':doc['revision'],'text':'must not commit'}))
            assert denied['error']['code']=='StorageFull'
            receipt=pipe.call(invoke('jobs_list'))['result']['jobs'][0]
            pipe.call(invoke('jobs_forget',{'id':receipt['id'],'forget':True,'confirm':True}))
            assert pipe.call(invoke('documents_read',{'id':doc['id']}))['result']['text']==''
        (root/'quota/jobs/interrupted-fixture.json').write_text(json.dumps({'id':'interrupted-fixture','owner':'edit','operation':'documents_replace','status':'running','created':0}))
        recovered=json.loads(run('--data-dir',root/'quota','--access','edit','invoke','jobs_get','--json',json.dumps({'id':'interrupted-fixture'})).stdout)
        assert recovered['status']=='interrupted-unknown'
        report.append('Durable quota refuses work before mutation; interrupted receipts recover without replay')
        native=root/'old-native.json';compat=root/'old-sdk.json'
        raw=subprocess.Popen([str(cli),'--serve',str(native),str(file)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        legacy=None
        try:
            deadline=time.monotonic()+15
            while not native.exists():
                if raw.poll() is not None:raise RuntimeError(raw.stderr.read().decode())
                if time.monotonic()>deadline:raise TimeoutError('Old native bridge')
                time.sleep(.01)
            legacy=subprocess.Popen([str(cli),'--desktop-service','--native-descriptor',str(native),'--service-descriptor',str(compat),'--data-dir',str(root/'compat')],stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            while not compat.exists():
                if legacy.poll() is not None:raise RuntimeError(legacy.stderr.read().decode())
                if time.monotonic()>deadline:raise TimeoutError('Legacy SDK adapter')
                time.sleep(.01)
            endpoint=json.loads(compat.read_text());documents=request(endpoint,'read',invoke('documents_list'))[0]['documents'];assert len(documents)==1
            assert request(endpoint,'read',invoke('documents_new'))[0]['error']['code']=='AccessDenied'
            original=request(endpoint,'edit',invoke('documents_read',{'id':documents[0]['id']}))[0]
            changed=request(endpoint,'edit',invoke('documents_replace',{'id':original['id'],'expectedRevision':original['revision'],'text':'compat unsaved'}))[0]
            assert changed['text']=='compat unsaved' and file.read_text(encoding='utf-8')==text
            env=dict(os.environ,CYCODE_READER_CLI=str(cli))
            shim=subprocess.run([sys.executable,'-B',str(source/'app/tool.py'),'--bridge-config',str(compat),'list'],env=env,capture_output=True,text=True,timeout=15)
            assert shim.returncode==0,shim.stderr
            assert len(json.loads(shim.stdout))==21
        finally:
            raw.stdin.close();raw.wait(timeout=15);raw.stdout.close();raw.stderr.close()
            if legacy:
                try:legacy.wait(timeout=15)
                except subprocess.TimeoutExpired:legacy.kill();legacy.wait();raise
                error=legacy.stderr.read().decode();legacy.stdout.close();legacy.stderr.close();assert legacy.returncode==0,error
        assert not compat.exists()
        report.append('Original --desktop-service/raw TextBridge and Python forwarding commands preserve one real document owner')

        # Preserve the legacy DB byte-for-byte and import only the chosen owner.
        old=root/'legacy.sqlite3'
        with sqlite3.connect(old) as db:
            db.execute('CREATE TABLE records(kind TEXT,id TEXT,value TEXT)')
            for owner in ['selected','other']:db.execute('INSERT INTO records VALUES(?,?,?)',('jobs',owner,json.dumps({'clientId':owner,'state':'Completed','result':{'value':owner}})))
        db.close()
        before=hashlib.sha256(old.read_bytes()).hexdigest()
        importer=source/'scripts/import_legacy_jobs.py'
        if importer.is_file():
            args=[sys.executable,'-B',str(importer),'--database',str(old),'--destination',str(root/'list'),'--client','selected','--owner','read']
            imported=subprocess.run(args,capture_output=True,text=True,timeout=20);assert imported.returncode==0,imported.stderr
            assert json.loads(imported.stdout)['imported']==1
            assert hashlib.sha256(old.read_bytes()).hexdigest()==before
            assert json.loads(run('--data-dir',root/'list','invoke','jobs_list','--json','{}').stdout)['total']==1
            report.append('Read-only legacy SQLite import, ownership selection and original checksum preserved')
    print(json.dumps({'passed':report,'tests':len(report)},ensure_ascii=False,indent=2))


if __name__=='__main__':main()
