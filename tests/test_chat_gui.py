"""Opt-in actual Reader/ChatUI acceptance against synthetic loopback AI and MCP."""
import contextlib
import hashlib
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request
from test_cymcp import request


class Provider(BaseHTTPRequestHandler):
    requests=[]
    cancelled=False
    remote_ok=False
    remote_error=False
    chat_read_denied=False
    def log_message(self,*args):pass
    def do_GET(self):
        body=json.dumps({'data':[{'id':'synthetic-tools'}]}).encode();self.send_response(200);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(body)
    def do_POST(self):
        data=json.loads(self.rfile.read(int(self.headers['Content-Length'])));self.requests.append(data)
        messages=data['messages'];user=next(m for m in reversed(messages) if m['role']=='user');text=user.get('content','')
        if not isinstance(text,str):text=''.join(x.get('text','') for x in text)
        if text=='error':
            self.send_response(503);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(b'{"error":{"message":"synthetic provider unavailable"}}');return
        self.send_response(200);self.send_header('Content-Type','text/event-stream');self.end_headers()
        def emit(delta,finish=None):
            event={'choices':[{'index':0,'delta':delta,'finish_reason':finish}]}
            self.wfile.write(('data: '+json.dumps(event)+'\n\n').encode());self.wfile.flush()
        def tool(name,args):
            if "." in name:name="cy_"+hashlib.sha256(name.encode()).hexdigest()[:32]
            emit({'tool_calls':[{'index':0,'id':'fixture-'+str(len(self.requests)),'type':'function','function':{'name':name,'arguments':json.dumps(args)}}]},'tool_calls')
        try:
            tools=[]
            for m in reversed(messages):
                if m['role']=='user':break
                if m['role']=='tool':tools.append(m)
            tools.reverse()
            if text=='edit':
                if not tools:tool('documents_list',{})
                elif len(tools)==1:
                    document=json.loads(tools[0]['content'])['documents'][0];tool('documents_read',{'id':document['id']})
                elif len(tools)==2:
                    document=json.loads(tools[1]['content']);tool('documents_replace',{'id':document['id'],'expectedRevision':document['revision'],'text':'int changed_by_chat;\n'})
                else:
                    assert json.loads(tools[-1]['content'])['text']=='int changed_by_chat;\n'
                    emit({'content':'Modified the live Reader buffer. '});time.sleep(.1);emit({'content':'Saving remains explicit.'},'stop')
            elif text=='denied-edit':
                if not tools:tool('documents_new',{})
                else:
                    type(self).chat_read_denied=json.loads(tools[-1]['content'])['error']['code']=='AccessDenied'
                    emit({'content':'Chat read permission preserved.'},'stop')
            elif text=='remote':
                if not tools:tool('remote.other.automation_status',{})
                elif len(tools)==1:
                    type(self).remote_ok=json.loads(tools[0]['content'])['result']['structuredContent']['sdk']=='CyMCPSDK';tool('remote.other.documents_new',{})
                else:
                    type(self).remote_error=json.loads(tools[-1]['content'])['error']['code']=='RemoteToolFailed';emit({'content':'Remote result received and remote denial surfaced.'},'stop')
            elif text=='cancel':
                for i in range(150):emit({'content':'Streaming… '});time.sleep(.1)
                emit({},'stop')
            else:emit({'content':'Private full history preserved; bounded context accepted.'},'stop')
            self.wfile.write(b'data: [DONE]\n\n');self.wfile.flush()
        except (BrokenPipeError,ConnectionResetError,ConnectionAbortedError):type(self).cancelled=True


def main():
    app=Path(sys.argv[1]).resolve();cli=app.with_name('cycodereader_cli.exe' if sys.platform=='win32' else 'cycodereader_cli')
    output=Path(sys.argv[2]).resolve();output.mkdir(parents=True,exist_ok=True)
    server=ThreadingHTTPServer(('127.0.0.1',0),Provider);server.daemon_threads=True
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:
        with tempfile.TemporaryDirectory(prefix='reader-chat-') as temporary:
            root=Path(temporary);file=root/'synthetic.cpp';file.write_text('int original;\n')
            attachment=root/'attachment.txt';attachment.write_text('SYNTHETIC_ATTACHMENT')
            protected=root/'remote-protected.json';protected.write_text(json.dumps({'accessProfile':'protected'}))
            settings={'accessProfile':'free','chat':{'provider':'openai-compatible','model':'synthetic-tools','baseUrl':f'http://127.0.0.1:{server.server_port}/v1','compact':False,'contextEnabled':True,'contextKiB':16,'recentTurns':1},'remoteServers':[{'id':'other','command':str(cli),'args':['--data-dir',str(root/'remote'),'--config',str(protected),'--full-tools','mcp'],'allowTools':['automation_status','documents_new']}]}
            config=root/'settings.json';config.write_text(json.dumps(settings));script=[]
            def action(frame,command,**values):script.append(dict(frame=frame,command=command,**values))
            action(2,'assistant.remotes');action(120,'assistant.connect',edit=True);action(180,'assistant.models');action(240,'assistant.send',text='edit')
            action(600,'checkpoint',name='edited');action(650,'assistant.send',text='remote');action(1050,'checkpoint',name='remote')
            action(1100,'assistant.send',text='cancel');action(1150,'checkpoint',name='streaming');action(1220,'assistant.cancel')
            action(1400,'checkpoint',name='cancelled');action(1450,'assistant.connect',edit=True,restore=True);action(1650,'checkpoint',name='restored')
            action(1700,'assistant.send',text='longA '+('a'*7000));action(2000,'assistant.send',text='longB '+('b'*7000));action(2300,'assistant.send',text='longC '+('c'*7000))
            action(2600,'checkpoint',name='history');action(2650,'assistant.send',text='error');action(2850,'checkpoint',name='error')
            # Reconnect on the same restored history, keeping the failure visible in the checkpoint.
            action(2900,'assistant.connect',edit=True,restore=True);action(3000,'assistant.models');action(3050,'assistant.send',text='attach',attachments=[str(attachment)])
            action(3300,'checkpoint',name='models');action(3400,'assistant.connect',edit=False);action(3500,'assistant.send',text='denied-edit')
            script_file=root/'script.json';script_file.write_text(json.dumps(script))
            descriptor=root/'connection.json';report=output/'chat-gui.json';capture=output/'chat-gui.bmp'
            command=[str(app),'--test-hidden','--test-frames','3900','--test-script',str(script_file),'--test-report',str(report),'--capture',str(capture),'--automation-config',str(config),'--automation-data-dir',str(root/'profile'),'--automation-descriptor',str(descriptor),'--open',str(file)]
            process=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            try:
                deadline=time.monotonic()+25
                while not descriptor.exists():
                    if process.poll() is not None:raise RuntimeError(process.stderr.read().decode(errors='replace'))
                    if time.monotonic()>deadline:raise TimeoutError('GUI native service')
                    time.sleep(.01)
                # CLI/MCP act on the same actual GUI/editor; UI thread runs these handlers.
                def call(name,args=None,access='edit'):
                    r=subprocess.run([str(cli),'--connect',str(descriptor),'--access',access,'invoke',name,'--json',json.dumps(args or {})],capture_output=True,text=True,encoding='utf-8',timeout=15)
                    assert r.returncode==0,r.stderr;return json.loads(r.stdout)
                docs=call('documents_list')['documents'];doc=call('documents_read',{'id':docs[0]['id']})
                call('editor_goto',{'id':doc['id'],'expectedRevision':doc['revision'],'line':0,'character':0})
                selected=call('editor_read_selection',{'id':doc['id']});assert selected['positionEncoding']=='utf-16'
                inserted=call('editor_replace_selection',{'id':doc['id'],'expectedRevision':doc['revision'],'text':'// GUI tool\n'})
                assert inserted['text'].startswith('// GUI tool')
                undone=call('editor_undo',{'id':doc['id'],'expectedRevision':inserted['revision']});assert undone['text']=='int original;\n'
                redone=call('editor_redo',{'id':doc['id'],'expectedRevision':undone['revision']});assert redone['text'].startswith('// GUI tool')
                call('editor_undo',{'id':doc['id'],'expectedRevision':redone['revision']})
                ep=json.loads(descriptor.read_text())
                init,headers=request(ep,'edit',{'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':'gui-fixture','version':'1'}}},True)
                session=headers['Mcp-Session-Id']
                def mcp(name,args,role='edit'):
                    return request(ep,role,{'jsonrpc':'2.0','id':2,'method':'tools/call','params':{'name':'cy_tools_call','arguments':{'name':name,'arguments':args}}},True,session)[0]['result']
                assert not mcp('automation_status',{},'read')['structuredContent']['controlOperations']
                current=mcp('documents_read',{'id':doc['id']})['structuredContent']
                changed=mcp('documents_replace',{'id':doc['id'],'expectedRevision':current['revision'],'text':'int from_external_mcp;\n'})['structuredContent']
                assert changed['text']=='int from_external_mcp;\n'
                call('documents_replace',{'id':doc['id'],'expectedRevision':changed['revision'],'text':'int original;\n'})
                out,err=process.communicate(timeout=90);assert process.returncode==0,err.decode(errors='replace')
            finally:
                if process.poll() is None:process.kill();process.communicate(timeout=5)
            state=json.loads(report.read_text(encoding='utf-8'));check={x['name']:x for x in state['trace']}
            assert state['allWindowsHidden'] and not state['error'],state.get('error')
            assert check['edited']['documents'][0]['text']=='int changed_by_chat;\n'
            assert file.read_text()=='int original;\n'
            assert check['streaming']['assistant']['busy'] and 'Streaming' in str(check['streaming']['assistant']['messages'])
            assert not check['cancelled']['assistant']['busy'] and Provider.cancelled
            assert check['restored']['assistant']['messages'] and check['restored']['assistant']['connected']
            assert Provider.remote_ok and Provider.remote_error and Provider.chat_read_denied
            assert check['error']['assistant']['error'],check['error']['assistant']
            final_history=check['history']['assistant']['messages'];assert all(any(tag in m['text'] for m in final_history) for tag in ['longA','longB','longC'])
            last=max((r for r in Provider.requests if any('longC' in str(m.get('content','')) for m in r['messages'])),key=lambda r:len(r['messages']))
            assert not any('longA ' in str(m.get('content','')) for m in last['messages'])
            assert last['messages'][0]['role']=='system'
            assert (root/'profile/conversations').is_dir()
            assert any('SYNTHETIC_ATTACHMENT' in json.dumps(r) for r in Provider.requests), 'attached text must reach the provider'
            assert check['models']['assistant']['models'], 'real provider model discovery before changing connection'
            summary={'passed':['actual hidden GUI/editor CLI selection/Undo/Redo','chat read-only choice denies document mutations','ChatUI native tool edits the real unsaved buffer','streaming/cancellation','private history restoration','allowlisted remote tool and denied remote mutation','bounded API context preserves full UI history/system','provider error surfaced'],'requests':len(Provider.requests),'capture':str(capture)}
            (output/'chat-summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2))
    finally:server.shutdown();server.server_close();thread.join(2)


if __name__=='__main__':main()
