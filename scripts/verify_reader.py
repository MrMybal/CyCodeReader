"""Exercise the real Reader UI/editor in hidden native windows only."""
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get('CYCODE_READER',ROOT/'dist'/('CyCodeReader.exe' if os.name=='nt' else 'CyCodeReader')))
SOURCE='''#pragma once
#include "CoreMinimal.h"

// A small code editor using the shared CyCode colors.
UCLASS(BlueprintType)
class UReaderExample : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "Reader")
    static int32 Calculate(int32 Value)
    {
        return Value * 2;
    }
};
'''

def run(folder,name,files,actions=(),theme='graphite',language='en',frames=65):
    if not EXE.is_file(): raise RuntimeError('No deployed Reader binary; this script does not restore executables. Set CYCODE_READER only after the protection issue is resolved.')
    reports=ROOT/'data/reports/reader'
    reports.mkdir(parents=True,exist_ok=True)
    script=folder/(name+'-script.json')
    script.write_text(json.dumps(list(actions)),'utf-8')
    report=reports/(name+'.json')
    args=[str(EXE),*[str(f) for f in files],'--theme',theme,'--language',language,
          '--test-hidden','--test-frames',str(frames),'--test-script',str(script),
          '--test-report',str(report),'--capture',str(reports/(name+'.bmp'))]
    result=subprocess.run(args,capture_output=True,timeout=35)
    assert result.returncode==0,(result.returncode,(ROOT/'data/reports/reader-startup-error.txt').read_text('utf-8') if (ROOT/'data/reports/reader-startup-error.txt').exists() else result.stderr.decode(errors='replace'))
    state=json.loads(report.read_text('utf-8'))
    assert state['allWindowsHidden'],state
    return state

def main():
    base=ROOT/'data/validation'
    base.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='reader-',dir=base) as temporary:
        folder=Path(temporary)
        (folder/'Example.uproject').write_text('{"FileVersion":3}','utf-8')
        header=folder/'ReaderExample.h'; header.write_text(SOURCE,'utf-8')
        shader=folder/'Example.usf'; shader.write_text('float4 MainPS() : SV_Target { return float4(1, 0, 0, 1); }\n','utf-8')
        for theme in ['graphite','mid-grey','light-grey','amber-night','violet','navy']:
            state=run(folder,theme,[shader,header],theme=theme,language='fr' if theme=='light-grey' else 'en')
            assert not state['error'] and len(state['documents'])==2 and all(not d['dirty'] for d in state['documents']),state
            assert {d['syntax'] for d in state['documents']}=={'C++ / Unreal','HLSL'},state
            assert header.read_text('utf-8')==SOURCE
            print(f'{theme}: shared Unreal/HLSL colors, hidden native window',flush=True)
        file=folder/'code é.cpp'; initial='\tint value = 1;\n'; edited='\tint value = 2;\n'
        file.write_bytes(b'\xef\xbb\xbf'+initial.replace('\n','\r\n').encode())
        output=folder/'saved.cpp'; sentinel=folder/'existing.cpp'; sentinel.write_text('preserved\n','utf-8')
        actions=[]
        def action(frame,command,**values): actions.append({'frame':frame,'command':command,**values})
        action(8,'setText',text=edited); action(10,'checkpoint',name='dirty')
        action(12,'undo'); action(14,'checkpoint',name='undo')
        action(16,'redo'); action(18,'checkpoint',name='redo')
        action(20,'keys',keys=['Ctrl','S']); action(21,'keys',keys=['Ctrl','S'],down=False); action(24,'checkpoint',name='saved')
        action(28,'keys',keys=['Ctrl','F']); action(29,'keys',keys=['Ctrl','F'],down=False)
        action(35,'text',text='value'); action(39,'keys',keys=['Enter']); action(40,'keys',keys=['Enter'],down=False)
        action(44,'checkpoint',name='search'); action(46,'keys',keys=['Escape']); action(47,'keys',keys=['Escape'],down=False)
        action(52,'setText',text='\tint value = 3;\n'); action(55,'close'); action(58,'checkpoint',name='close-guard')
        action(60,'resolveClose',choice='cancel'); action(63,'checkpoint',name='cancel-close')
        action(66,'saveAs',path=str(sentinel)); action(69,'checkpoint',name='overwrite-guard')
        action(72,'resolveOverwrite',accept=False); action(76,'saveAs',path=str(output)); action(79,'checkpoint',name='save-as')
        action(84,'keys',keys=['Ctrl','N']); action(85,'keys',keys=['Ctrl','N'],down=False)
        action(90,'setText',text='// new file\n'); action(94,'keys',keys=['Ctrl','S']); action(95,'keys',keys=['Ctrl','S'],down=False)
        action(98,'checkpoint',name='new-save-dialog'); action(100,'dialogResult',files=[str(folder/'new.cpp')]); action(104,'checkpoint',name='new-saved')
        action(108,'close'); action(112,'checkpoint',name='clean-close'); action(115,'detach')
        state=run(folder,'editing',[file],actions,frames=135)
        traces={s['name']:s for s in state['trace']}
        assert traces['dirty']['documents'][0]['dirty']
        assert traces['undo']['documents'][0]['text']==initial and not traces['undo']['documents'][0]['dirty']
        assert traces['redo']['documents'][0]['text']==edited
        assert not traces['saved']['documents'][0]['dirty'],traces['saved']
        assert traces['search']['documents'][0]['hasFind'] and traces['search']['documents'][0]['text']==edited,traces['search']
        assert traces['close-guard']['pendingClose'] and len(traces['cancel-close']['documents'])==1
        assert traces['overwrite-guard']['pendingOverwrite']
        assert traces['new-save-dialog']['pendingDialog'] and len(traces['new-save-dialog']['documents'])==2,traces['new-save-dialog']
        assert not traces['new-saved']['pendingDialog'] and all(not d['dirty'] for d in traces['new-saved']['documents'])
        assert len(traces['clean-close']['documents'])==1 and state['nativeWindows']>=2,state
        assert not state['error'],state
        assert file.read_bytes()==b'\xef\xbb\xbf'+edited.replace('\n','\r\n').encode()
        assert output.read_bytes()==b'\xef\xbb\xbf\tint value = 3;\r\n'
        assert sentinel.read_text('utf-8')=='preserved\n'
        assert (folder/'new.cpp').read_bytes()==b'// new file\n'
        print('Actual editor undo/redo, Ctrl+S/F/N, Save As, close/overwrite guards and hidden native detachment passed.',flush=True)
        reports=ROOT/'data/reports/reader'
        processes=[]
        for index in range(2):
            processes.append(subprocess.Popen([str(EXE),str(header),'--test-hidden','--test-frames','120','--test-report',str(reports/f'instance-{index}.json')],stdout=subprocess.PIPE,stderr=subprocess.PIPE))
        assert processes[0].pid!=processes[1].pid
        for index,process in enumerate(processes):
            process.communicate(timeout=30)
            assert process.returncode==0
            state=json.loads((reports/f'instance-{index}.json').read_text('utf-8'))
            assert state['allWindowsHidden'] and not state['error'] and state['documents'][0]['text']==SOURCE
        print('Two independent Reader processes passed without visible windows.')

if __name__=='__main__': main()
