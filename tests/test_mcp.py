"""Real Reader native core, shared CyToolsCore HTTP service and official MCP clients."""
import asyncio
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'app'))
from tool import NativeClient

class ReaderMCP(unittest.TestCase):
    def test_shared_runtime_live_documents_permissions_plugins_and_save(self):
        from mcp import ClientSession,StdioServerParameters
        from mcp.client.stdio import stdio_client
        with tempfile.TemporaryDirectory(prefix='reader-mcp-') as temp:
            root=Path(temp);file=root/'été.txt';original='beta\nalpha 🛠\n';file.write_text(original,'utf-8')
            native=root/'native.json';service=root/'core.json'
            executable=Path(os.environ.get('CYCODE_READER_TEST_CLI',ROOT/'runtime/core-build-vs/Release/cycodereader_cli.exe'))
            plugins=ROOT/'runtime/core-build-vs/plugins'
            process=subprocess.Popen([str(executable),'--serve',str(native),'--plugins',str(plugins),str(file)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            sdk=None
            try:
                deadline=time.monotonic()+10
                while not native.exists():
                    if process.poll() is not None: self.fail(process.communicate()[1].decode(errors='replace'))
                    self.assertLess(time.monotonic(),deadline);time.sleep(.03)
                sdk=subprocess.Popen([sys.executable,str(ROOT/'app/tool.py'),'--desktop-service','--native-descriptor',str(native),'--service-descriptor',str(service),'--data-dir',str(root/'jobs')],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                while not service.exists():
                    if sdk.poll() is not None: self.fail(sdk.communicate()[1].decode(errors='replace'))
                    self.assertLess(time.monotonic(),deadline);time.sleep(.03)
                async def scenario():
                    def connection(access): return StdioServerParameters(command=sys.executable,args=[str(ROOT/'app/tool.py'),'--bridge-config',str(service),'--access',access,'mcp'])
                    async with stdio_client(connection('read')) as (ar,aw),stdio_client(connection('edit')) as (br,bw):
                        async with ClientSession(ar,aw) as reader,ClientSession(br,bw) as writer:
                            await reader.initialize();await writer.initialize()
                            names={tool.name for tool in (await reader.list_tools()).tools}
                            self.assertTrue({'op_plugins_list','op_plugins_invoke','op_documents_read','op_text_transform'}.issubset(names))
                            async def invoke(session,name,inputs,expected='Completed'):
                                submitted=await session.call_tool('op_'+name,{'parameters':inputs});self.assertFalse(submitted.isError,submitted)
                                identifier=json.loads(submitted.content[0].text)['jobId'];deadline=time.monotonic()+15
                                while time.monotonic()<deadline:
                                    response=await session.call_tool('cy_get_job',{'jobId':identifier});job=json.loads(response.content[0].text)
                                    if job['state'] in ('Completed','Failed','Cancelled'):
                                        self.assertEqual(job['state'],expected,job);return job,identifier
                                    await asyncio.sleep(.02)
                                self.fail('Reader MCP operation timed out')
                            listed,_=await invoke(reader,'documents_list',{});id=listed['outputs']['documents'][0]['id']
                            scratch,_=await invoke(writer,'documents_new',{});scratch=scratch['outputs']
                            formatted,_=await invoke(writer,'documents_format',{'id':scratch['id'],'expectedRevision':scratch['revision'],'encoding':'utf-16be','lineEnding':'CRLF'})
                            self.assertEqual(formatted['outputs']['encoding'],'UTF-16 BE BOM');self.assertTrue(formatted['outputs']['dirty'])
                            await invoke(writer,'documents_replace',{'id':scratch['id'],'expectedRevision':scratch['revision'],'text':'obsolete'},'Failed')
                            await invoke(writer,'documents_close',{'id':scratch['id'],'expectedRevision':formatted['outputs']['revision'],'discard':True})
                            doc,_=await invoke(reader,'documents_read',{'id':id});guard={'id':id,'expectedRevision':doc['outputs']['revision']}
                            for name,inputs in [('documents_replace',{**guard,'text':'denied'}),('plugins_invoke',{**guard,'plugin':'cy.reader.text-tools','command':'sort'}),('documents_open',{'path':str(file)})]:
                                denied=await reader.call_tool('op_'+name,{'parameters':inputs});self.assertTrue(denied.isError)
                            changed,jobId=await invoke(writer,'plugins_invoke',{**guard,'plugin':'cy.reader.text-tools','command':'sort'})
                            self.assertEqual(changed['outputs']['document']['text'],'alpha 🛠\nbeta\n');self.assertEqual(file.read_text('utf-8'),original)
                            private=await reader.call_tool('cy_get_job',{'jobId':jobId});self.assertTrue(private.isError)
                            await invoke(writer,'documents_replace',{**guard,'text':'obsolete'},'Failed')
                            current,_=await invoke(reader,'documents_read',{'id':id});guard['expectedRevision']=current['outputs']['revision']
                            count,_=await invoke(writer,'plugins_invoke',{**guard,'plugin':'cy.reader.native-sample','command':'wordCount'})
                            self.assertEqual(count['outputs']['result']['wordCount'],3)
                            search,_=await invoke(reader,'search_open_documents',{'query':'🛠'});self.assertEqual(search['outputs']['matches'][0]['line'],1)
                            await invoke(writer,'documents_save',guard);self.assertEqual(file.read_text('utf-8'),'alpha 🛠\nbeta\n')
                            live,_=await invoke(writer,'documents_replace',{**guard,'text':'live\n'});guard['expectedRevision']=live['outputs']['revision']
                            file.write_text('external\n','utf-8');await invoke(writer,'documents_save',guard,'Failed');self.assertEqual(file.read_text('utf-8'),'external\n')
                            await invoke(writer,'documents_close',guard,'Failed')
                            await invoke(writer,'documents_close',{**guard,'discard':True})
                asyncio.run(scenario())
            finally:
                process.communicate(b'\n' if process.poll() is None else None,timeout=10)
                if sdk:
                    try: sdk.communicate(timeout=10)
                    except subprocess.TimeoutExpired: sdk.terminate();sdk.communicate(timeout=5)

if __name__=='__main__': unittest.main()
