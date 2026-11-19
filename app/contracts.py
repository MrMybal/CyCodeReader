"""Read the canonical, native CyMCPSDK operation schemas; no second catalogue."""
import json
from pathlib import Path
MANIFEST=json.loads(Path(__file__).with_name('CyTool.json').read_text(encoding='utf-8'))
OPERATIONS=[(o['id'],o['nativeMethod'],o['description'],o['inputSchema'],o['outputSchema'],o['permissions']) for o in MANIFEST['operations']]
METHODS={o[0]:o[1] for o in OPERATIONS}
def write_manifest():
    # Compatibility entry point: validate the source rather than regenerate it.
    assert len(METHODS)==len(OPERATIONS)
    for o in MANIFEST['operations']:assert o['inputSchema']['type']=='object' and o['outputSchema']['type']=='object'
if __name__=='__main__':write_manifest()
