"""Opt-in real hidden Reader access-profile acceptance on synthetic documents."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from test_cymcp import Lines

def http(url, message, token=None, session=None):
    headers={'Content-Type':'application/json'}
    if token is not None: headers['Authorization']='Bearer '+token
    if session: headers.update({'Mcp-Session-Id':session,'MCP-Protocol-Version':'2025-11-25'})
    req=urllib.request.Request(url,data=json.dumps(message).encode(),headers=headers)
    try:
        with urllib.request.urlopen(req,timeout=10) as r:return json.loads(r.read()),r.status,r.headers
    except urllib.error.HTTPError as r:
        body=r.read();return json.loads(body) if body else {},r.code,r.headers

def main():
    app=Path(sys.argv[1]).resolve(); cli=app.with_name('cycodereader_cli.exe' if sys.platform=='win32' else 'cycodereader_cli')
    output=Path(sys.argv[2]).resolve();output.mkdir(parents=True,exist_ok=True)
    results=[]
    with tempfile.TemporaryDirectory(prefix='reader-profile-config-') as tmp:
        root=Path(tmp)
        for i,(settings,expected) in enumerate([
            ({},'free'),({'accessProfile':'protected_access'},'protected'),
            ({'access_profile':'free'},'free'),({'accessProfile':'token_only'},'token_only'),
            ({'accessProfile':'unknown'},None),({'accessProfile':True},None),
            ({'accessProfile':'free','access_profile':'protected'},None)]):
            config=root/(str(i)+'.json');config.write_text(json.dumps(settings))
            run=subprocess.run([str(cli),'--data-dir',str(root/str(i)),'--config',str(config),'invoke','automation_status'],capture_output=True,text=True,encoding='utf-8',timeout=10)
            if expected:assert run.returncode==0 and json.loads(run.stdout)['accessProfile']==expected,run.stderr
            else:assert run.returncode!=0 and 'InvalidOptions' in run.stderr,run.stderr

    with tempfile.TemporaryDirectory(prefix='reader-access-') as tmp:
        root=Path(tmp)
        for profile,require_token,controlled in [('free',False,False),('local_controlled',False,True),('token_only',True,False),('protected',True,True)]:
            case=root/profile;case.mkdir();document=case/'synthetic.cpp';document.write_text('int original;\n')
            # A pre-existing profile without a choice migrates to Free, without rewriting unrelated settings.
            settings={'compact':True,'chat':{'model':'preserve-this-choice'}}
            if profile!='free':settings['access_profile']=profile
            config=case/'settings.json';config.write_text(json.dumps(settings));before=config.read_bytes()
            descriptor=case/'connection.json';report=output/(profile+'-gui.json')
            process=subprocess.Popen([str(app),'--test-hidden','--test-frames','2400','--test-report',str(report),
                '--automation-config',str(config),'--automation-data-dir',str(case/'data'),'--automation-descriptor',str(descriptor),'--open',str(document)],
                stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            try:
                deadline=time.monotonic()+20
                while not descriptor.exists():
                    if process.poll() is not None:raise RuntimeError(process.stderr.read().decode(errors='replace'))
                    if time.monotonic()>deadline:raise TimeoutError('Reader GUI service')
                    time.sleep(.01)
                ep=json.loads(descriptor.read_text());assert ep['accessProfile']==profile
                assert ep['requireToken']==require_token and ep['controlOperations']==controlled
                assert ('readToken' in ep)==require_token and ('editToken' in ep)==require_token
                def call(name,args=None,token=None):return http(ep['url'],{'method':'invoke','name':name,'arguments':args or {}},token)
                if require_token:
                    assert call('documents_list')[1]==401
                    denied=subprocess.run([str(cli),'--connect',ep['mcpUrl'],'invoke','documents_list','--json','{}'],capture_output=True,text=True,encoding='utf-8',timeout=10)
                    assert denied.returncode==3 and 'AuthenticationRequired' in denied.stderr,denied.stderr
                    assert call('documents_list',token='invalid-fixture-token')[1]==401
                    assert http(ep['mcpUrl'],{'jsonrpc':'2.0','id':1,'method':'tools/list'})[1]==401
                token=ep.get('readToken')
                status=call('automation_status',token=token)[0]
                assert status['gui'] and status['operations']==30
                assert status['version']=='0.7.2' and status['accessProfile']==profile
                assert status['sourceRevision']=='e562c731a146d599e3404718ea8414bb5b96b66d'
                doc=call('documents_list',token=token)[0]['documents'][0]
                doc=call('documents_read',{'id':doc['id']},token)[0];assert doc['text']=='int original;\n'
                init,code,headers=http(ep['mcpUrl'],{'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':'reader-profile-fixture','version':'1'}}},token)
                assert code==200 and 'result' in init,init
                session=headers['Mcp-Session-Id']
                def mcp(name,args):
                    response,code,_=http(ep['mcpUrl'],{'jsonrpc':'2.0','id':2,'method':'tools/call','params':{'name':'cy_tools_call','arguments':{'name':name,'arguments':args}}},token,session)
                    assert code==200,response
                    return response['result']
                read=mcp('documents_read',{'id':doc['id']});assert read['structuredContent']['text']==doc['text']
                args={'id':doc['id'],'expectedRevision':doc['revision'],'text':'int changed_without_prompt;\n'}
                change=mcp('documents_replace',args)
                if controlled:
                    assert change['isError'],change
                    assert call('documents_replace',args,token)[0]['error']['code']=='AccessDenied'
                    # JSON cannot elevate the shared local read principal.
                    forged={'method':'invoke','name':'documents_new','arguments':{},'principal':'edit'}
                    assert http(ep['url'],forged,token)[0]['error']['code']=='AccessDenied'
                    assert call('documents_read',{'id':doc['id']},token)[0]['text']==doc['text']
                    if require_token:
                        changed=call('documents_replace',args,ep['editToken'])[0]
                        assert changed['text']==args['text']
                else:
                    assert not change.get('isError') and change['structuredContent']['text']==args['text'],change
                    assert call('documents_replace',args,token)[0]['error']['code']=='RevisionConflict'
                assert call('documents_read',{'id':doc['id'],'unexpected':True},token)[0]['error']['code']=='InvalidParameters'
                if profile=='free':
                    # Actual CLI proxy connects using the copied address alone, with no descriptor or secret.
                    r=subprocess.run([str(cli),'--connect',ep['mcpUrl'],'invoke','documents_read','--json',json.dumps({'id':doc['id']})],capture_output=True,text=True,encoding='utf-8',timeout=10)
                    assert r.returncode==0,r.stderr
                    assert json.loads(r.stdout)['text']==args['text']
                    with Lines([cli,'--connect',ep['mcpUrl'],'--full-tools','mcp']) as proxy:
                        init=proxy.call({'method':'initialize','params':{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':'address-only-cli','version':'1'}}},True)
                        assert 'result' in init,init
                        assert len(proxy.call({'method':'tools/list'},True)['result']['tools'])==30
                assert document.read_text()=='int original;\n' and config.read_bytes()==before
                out,err=process.communicate(timeout=35);assert process.returncode==0,err.decode(errors='replace')
                state=json.loads(report.read_text(encoding='utf-8'));assert state['allWindowsHidden'] and not state['error'],state.get('error')
                assert not descriptor.exists()
                results.append({'profile':profile,'requireToken':require_token,'controlled':controlled,'actualGui':True,'read':True,'mutation':not controlled or require_token,'configurationPreserved':True})
            finally:
                if process.poll() is None:process.kill();process.communicate(timeout=5)
    summary={'passed':results,'sourceRevision':'e562c731a146d599e3404718ea8414bb5b96b66d','defaultMigration':'Free','workflowsLaunched':0}
    (output/'access-summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
