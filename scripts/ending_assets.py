"""Pack the original generated ending illustrations and their captions."""
import json
from pathlib import Path
import struct
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]

def append_endings(records):
    spec=json.loads((ROOT/'config/endings.json').read_text())
    assert [c['id'] for c in spec['characters']]==list(range(12))
    rows=[]
    for c in spec['characters']:
        if c['art_mode']=='illustration':
            im=Image.open(ROOT/'assets/endings'/c['file']).convert('RGB')
            im=im.resize((256,144),Image.Resampling.NEAREST)
            pixels=b''.join(struct.pack('<I',0xff000000|r<<16|g<<8|b) for r,g,b in im.getdata())
            records.append(struct.pack('<7H',c['id'],51,1,256,144,0,0)+pixels)
        rows.append('  {'+','.join(json.dumps(s) for s in [c['title'],*c['lines']])+'},')
    header='/* Generated from config/endings.json by scripts/ending_assets.py. */\n'
    header+='static const char *const ending_text[FC_ROSTER_COUNT][4]={\n'+'\n'.join(rows)+'\n};\n'
    (ROOT/'src/ending_text.h').write_text(header)
