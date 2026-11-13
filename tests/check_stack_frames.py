#!/usr/bin/env python3
"""v10.237: every function's stack frame (its sub esp) in the DirectDraw/D3D HAL DLL stays under one page: VXDDRAW.DLL is built
without stack probes (-s), and Windows 98 grows a thread's stack only through its one-page guard. A 9272-byte packet on the
stack (v10.235-v10.236: the DP2 clear, the legacy Clear) skipped the guard. The VxD's objects are reported, with a 2 KB limit."""
import os,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
wc=Path(os.environ.get('WATCOM','/opt/watcom'))
wdis=next((p for p in (wc/'binl64'/'wdis',wc/'binl'/'wdis') if p.exists()),None)
if not wdis:print('FAIL: wdis (Open Watcom) not found: no frame was checked');sys.exit(1) # v10.319 (audit T3)
env=dict(os.environ,WATCOM=str(wc))
def frames(obj):
 # v10.319 (audit T3): a failed disassembly or one with no function labels fails the check (it passed having checked nothing)
 r=subprocess.run([str(wdis),str(obj)],capture_output=True,text=True,env=env)
 if r.returncode:raise SystemExit('%s: wdis failed (%d): %s'%(obj,r.returncode,r.stderr.strip()[-300:]))
 fn=None;res={};names=0
 for line in r.stdout.splitlines():
  m=re.match(r'^[0-9A-F]{4,8}\t+([\w@$]+):$',line)
  if m:fn=m.group(1);names+=1;continue
  m=re.search(r'sub\s+esp,0x([0-9a-fA-F]+)',line)
  if m:
   if fn is None:raise SystemExit('%s: a frame before any function label'%obj)
   res[fn]=max(res.get(fn,0),int(m.group(1),16))
 if not names:raise SystemExit('%s: no function labels recognised in the disassembly'%obj)
 return res
bad=[];report=[]
for name,limit in [('vx_ddraw',4096),('vx_d3d_core',4096),('vx_d3d_hal',4096),('vx_native3d_vxd',2048),('vx_native3d_hw',2048),('vxd_main_vesa',2048)]:
 o=ROOT/'build'/(name+'.obj')
 if not o.is_file():raise SystemExit('missing object: %s (build first)'%o) # v10.319 (audit T3): not skipped
 f=frames(o);top=sorted(f.items(),key=lambda x:-x[1])[:3];report.append('%s: largest frames %s'%(name,', '.join('%s %d'%t for t in top)))
 bad+=['%s %s %d bytes (limit %d)'%(name,k,v,limit) for k,v in f.items() if v>=limit]
print('\n'.join(report))
if bad:print('FRAMES TOO LARGE:\n'+'\n'.join(bad));sys.exit(1)
print('every frame under its limit')
