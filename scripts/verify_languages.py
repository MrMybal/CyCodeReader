"""Reader-only syntax acceptance, manually run after the protection issue is resolved."""
from pathlib import Path
import tempfile
from verify_reader import ROOT,run

def main():
    samples={
        'main.rs':'fn main() { let message = "hello"; }\n',
        'Main.java':'class Main { String message = "hello"; }\n',
        'main.js':'const message = `hello ${name}`;\n',
        'main.ts':'interface Props { title: string; }\nconst count: number = 1;\n',
        'view.jsx':'const view = <Widget title="Hello">{count + 1}</Widget>;\n',
        'view.tsx':'interface Props { title: string; }\nconst view = <Widget title="Hello">{count + 1}</Widget>;\n',
        'index.html':'<style>.card { color: red; }</style><script>const count = 1;</script><main></main>\n',
        'style.css':'.card { color: #ffffff; }\n',
    }
    base=ROOT/'data/validation';base.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='languages-',dir=base) as folder:
        root=Path(folder);files=[]
        for name,text in samples.items():
            path=root/name;path.write_text(text,'utf-8');files.append(path)
        for theme in ('graphite','light-grey'):
            state=run(root,'languages-'+theme,files,theme=theme,frames=75)
            assert not state['error'],state
            for document in state['documents']:
                assert document['syntaxSpans']>0 and not document['syntaxTruncated'],document
            for path in files: assert path.read_text('utf-8')==samples[path.name]
            print(theme+': eight Reader language documents passed in hidden windows',flush=True)

if __name__=='__main__': main()
