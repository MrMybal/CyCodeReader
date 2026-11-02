"""Strict CyToolsCore schemas for the actual native Reader operations."""
import json
from pathlib import Path

def schema(properties=None,required=()): return {'type':'object','properties':properties or {},'required':list(required),'additionalProperties':False}
STRING={'type':'string'};BOOL={'type':'boolean'};ID={'type':'integer','minimum':1}
DOCUMENT=schema({'id':ID,'path':STRING,'revision':ID,'dirty':BOOL,'readOnly':BOOL,'encoding':STRING,'lineEnding':STRING},('id','path','revision','dirty','readOnly','encoding','lineEnding'))
TEXT=schema({**DOCUMENT['properties'],'text':STRING},(*DOCUMENT['required'],'text'))
GUARD={'id':ID,'expectedRevision':ID}
ENCODING={'type':'string','enum':['utf-8','utf-8-bom','utf-16le','utf-16be','windows-1252']}
PLUGINS=schema({'plugins':{'type':'array','items':{'type':'object'}},'errors':{'type':'array','items':{'type':'object'}}},('plugins','errors'))
OPERATIONS=[
 ('documents_list','documents.list','List documents currently open in this Reader instance.',schema(),schema({'documents':{'type':'array','items':DOCUMENT}},('documents',)),[]),
 ('documents_read','documents.read','Read an open live document by issued ID. Paths are persistent user files, never SDK task artifacts.',schema({'id':ID},('id',)),TEXT,[]),
 ('documents_open','documents.open','Open a user file; auto detects UTF-8/BOM and UTF-16 BOM, or use an explicit encoding (Windows-1252 is never guessed). Requires filesystem.read. GUI shows it on its next frame.',schema({'path':STRING,'encoding':ENCODING},('path',)),TEXT,['filesystem.read']),
 ('documents_new','documents.new','Create an unsaved native document.',schema(),TEXT,['workspace.write']),
 ('documents_replace','documents.replace','Replace an open buffer at expectedRevision; does not save. Read-only documents and obsolete revisions are rejected.',schema({**GUARD,'text':{'type':'string','maxLength':4194304}},(*GUARD,'text')),TEXT,['workspace.write']),
 ('documents_save','documents.save','Save an already-named document at expectedRevision using its native disk baseline and guarded atomic replacement.',schema(GUARD,GUARD),TEXT,['workspace.write']),
 ('documents_format','documents.format','Change the pending encoding/newline convention at expectedRevision. Unrepresentable characters fail without lossy replacement. Changes remain unsaved until documents_save.',schema({**GUARD,'encoding':ENCODING,'lineEnding':{'type':'string','enum':['LF','CRLF','CR']}},GUARD),TEXT,['workspace.write']),
 ('documents_close','documents.close','Close an open document at expectedRevision. Dirty documents require explicit discard=true.',schema({**GUARD,'discard':BOOL},GUARD),schema({'closed':ID},('closed',)),['workspace.write']),
 ('search_open_documents','search.openDocuments','Bounded literal search over current live buffers. Results use zero-based UTF-8 byte offsets and one-based lines/byte columns. Case-insensitive matching folds ASCII only.',schema({'query':{'type':'string','minLength':1,'maxLength':4096},'caseSensitive':BOOL},('query',)),schema({'matches':{'type':'array','maxItems':2000,'items':schema({'id':ID,'revision':ID,'offset':{'type':'integer','minimum':0},'line':ID,'column':ID},('id','revision','offset','line','column'))},'truncated':BOOL},('matches','truncated')),[]),
 ('text_transform','text.transform','Apply a native full-document text transformation at expectedRevision. Case conversion preserves non-ASCII bytes; no automatic save.',schema({**GUARD,'operation':{'type':'string','enum':['uppercase','lowercase','trimTrailing','sortLines','sortLinesDescending','uniqueLines']}},(*GUARD,'operation')),TEXT,['workspace.write']),
 ('plugins_list','plugins.list','Inspect loaded native/data plugins, commands, options, language definitions and load errors.',schema(),PLUGINS,[]),
 ('plugins_configure','plugins.configure','Validate declared plugin option types and update this instance. No plugin code executes.',schema({'plugin':STRING,'options':{'type':'object'}},('plugin','options')),PLUGINS,['workspace.write']),
 ('plugins_invoke','plugins.invoke','Execute a trusted plugin command against an open guarded buffer. A returned text field updates its unsaved document. Native extensions run in process and cannot be forcibly cancelled or sandboxed; cancellation is checked before/after invocation.',schema({**GUARD,'plugin':STRING,'command':STRING},(*GUARD,'plugin','command')),schema({'document':TEXT,'result':{'type':'object'}},('document','result')),['plugins.execute','workspace.write']),
]
METHODS={op:method for op,method,*_ in OPERATIONS}

def write_manifest():
    path=Path(__file__).with_name('CyTool.json');manifest=json.loads(path.read_text('utf-8'))
    manifest['operations']=[dict(id=op,name=op.replace('_',' ').title(),description=description,inputSchema=inputs,outputSchema=outputs,resourceRequirements={'ramBytes':16777216},supportedBackends=[],permissions=permissions,canRunAsync=True,canCancel=True,supportsProgress=False,**({'exclusiveGroups':['reader-mutations']} if permissions else {})) for op,method,description,inputs,outputs,permissions in OPERATIONS]
    path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__': write_manifest()
