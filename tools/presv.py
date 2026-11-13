"""Refresh verification/PRESERVATION.json for deliberately edited files: python3 tools/presv.py path..."""
import json,hashlib,sys,pathlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=ROOT/'verification/PRESERVATION.json'; d=json.loads(p.read_text())
for name in sys.argv[1:]:
    h=hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
    if name in d['unchanged_files']:
        old=d['unchanged_files'].pop(name)
        if old!=h: d['modified_files'][name]={'baseline_sha256':old,'current_sha256':h}
        else: d['unchanged_files'][name]=old
    elif name in d['modified_files']: d['modified_files'][name]['current_sha256']=h
    else: print('not tracked (added file):',name)
p.write_text(json.dumps(d,indent=1)+'\n')
