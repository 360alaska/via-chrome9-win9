#!/usr/bin/env python3
"""VX2LINE (line measurement program): its logic is run against a host model of the lab (an engine that runs the Bresenham recurrence from its registers) and of the DIB engine, with different tie rules and faults."""
import os,subprocess,tempfile,re
HERE=os.path.dirname(os.path.abspath(__file__));SRC=os.path.join(os.path.dirname(HERE),'src')
def vx2line_checks():
 cases=[];d=tempfile.mkdtemp();exe=os.path.join(d,'vx2line_host')
 r=subprocess.run(['gcc','-DMOCK','-w','-I'+SRC,'-o',exe,os.path.join(SRC,'vx2line.c'),os.path.join(HERE,'vx2line_mock.c')],capture_output=True,text=True);assert r.returncode==0,r.stderr
 def run(**env):
  e=dict(os.environ);e.update({k:str(v) for k,v in env.items()})
  p=subprocess.run([exe],capture_output=True,text=True,env=e);return p.returncode,p.stdout
 rc,o=run();assert rc==0 and o.count('T1 4 bytes')==1 and o.count('T1 2 bytes')==1 and o.count('T1 1 bytes')==1
 assert 'ENGINE != MODEL' not in o and 'DIB != A and != B' not in o
 assert o.count('DIB rule A (step the minor axis on a tie)')>=8 and 'MIXED' not in o
 assert o.count('engine A=model A')==24;cases.append('an engine that runs the recurrence equals the software model, and a DIB engine with tie rule A is identified as A in all octants at three depths')
 assert 'T2' not in o and 'form 0 rop F0:' not in o and 'T3 4 bytes per pixel: 32 of 32' in o;cases.append('the DIB model matches all 16 R2 codes; the hang-prone ROP exploration is gone')
 assert 'engine clip 40 of 40 match, DIB clip 40 of 40 match' in o;cases.append('the clip tests pass for a correct engine and DIB')
 rc,o=run(DIBMODE=1);assert o.count('DIB rule B (no step on a tie)')>=8 and 'rule A (step' not in o;cases.append('a DIB engine with tie rule B is identified as B')
 rc,o=run(DIBMODE=2)
 xm=[l for l in o.splitlines() if 'X-major' in l and 'DIB rule' in l];ym=[l for l in o.splitlines() if 'Y-major' in l and 'DIB rule' in l]
 assert all(('rule A' in l or 'no ties' in l) for l in xm) and all(('rule B' in l or 'no ties' in l) for l in ym);cases.append('a DIB engine with different rules for X-major and Y-major lines is reported per octant')
 rc,o=run(DIBMODE=3)
 for l in o.splitlines():
  if 'DIB rule' in l and 'octant' in l:
   o_=int(l.split('octant ')[1][0]);assert ('rule A' in l and o_&2) or ('rule B' in l and not o_&2),l
 cases.append('the Y-direction tie rule (A when Y decreases, B when it increases) is reported in every octant')
 rc,o=run(DIBR2BUG=7);assert 'DIB R2 7 dest' in o and 'R2  7 (ROP 5A): 0 of 2' in o;cases.append('a DIB R2 code that does not follow the pen/destination ROP3 is named')
 rc,o=run(DIBLAST=1);assert 'pixel at the last point changed (drawn)' in o and 'Polyline equals the three LineTo segments: NO' in o;cases.append('a Polyline that draws its last point is reported')
 return cases
