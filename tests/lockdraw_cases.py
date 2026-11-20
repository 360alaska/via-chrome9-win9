#!/usr/bin/env python3
""".672 (ACCEL_OPENGL_LOCKDRAW): HarborBench's drawN on the host (gl/host/vxgl_host.c bench scene 11: a 41 x 41 lit, textured grid in one
locked array, split into N glDrawElements calls), with the lock-wide draws off and on: every draw packet the same (the packet hash covers
every vertex and burst), the hits counted, and with every fifth draw changing a colour (colour material), a material, the modelview, the
bound texture, the shade model or lighting: the same packets again (each change must end the kept results). Run after tests/gl_host.py,
which builds build/hostgl/vxglhost."""
import os,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];EXE=ROOT/'build'/'hostgl'/'vxglhost'
def run(ld,n,var):
 env=os.environ.copy();env.update({'VXGL_LOCKDRAW':str(ld),'VXGL_BENCH':'11','VXGL_BENCH_N':'3','VXGL_BENCH_DRAWS':str(n),'VXGL_BENCH_VAR':str(var)})
 r=subprocess.run([str(EXE)],cwd=ROOT,env=env,capture_output=True,text=True)
 err=[l for l in r.stderr.splitlines() if l.strip()]
 m=re.search(r'packet hash ([0-9A-F]+); lock-wide draws (\d+), hits (\d+), kept (\d+)',r.stdout)
 if r.returncode or err or not m:raise SystemExit('run %d %d %d failed:\n%s\n%s'%(ld,n,var,r.stdout[-1500:],'\n'.join(err[-40:])))
 return m.group(1),int(m.group(2)),int(m.group(3)),int(m.group(4))
fails=0
for var in (0,1,2,3,4,5,8):
 for n in (100,2000):
  a=run(0,n,var);b=run(1,n,var)
  ok=a[0]==b[0]and a[1]==0 and b[2]>0 and (var==0)==(b[3]==3)
  print('var %d, %4d draws: off %s, on %s (lock-wide %d, hits %d, kept %d): %s'%(var,n,a[0],b[0],b[1],b[2],b[3],'right' if ok else 'WRONG'))
  fails+=not ok
print('lock-wide draws: %d failed'%fails);sys.exit(1 if fails else 0)
