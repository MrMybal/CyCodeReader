"""Embed the canonical JSON catalogue without oversized MSVC string tokens."""
import json,sys
from pathlib import Path
manifest=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'))
source=json.dumps(manifest,separators=(',',':'),ensure_ascii=True)
chunks=[json.dumps(source[i:i+2000]) for i in range(0,len(source),2000)]
target=Path(sys.argv[2]);target.parent.mkdir(parents=True,exist_ok=True)
content='#pragma once\ninline constexpr const char readerOperationManifest[]=\n'+'\n'.join(chunks)+';\n'
if not target.exists() or target.read_text(encoding='utf-8')!=content:target.write_text(content,encoding='utf-8')
