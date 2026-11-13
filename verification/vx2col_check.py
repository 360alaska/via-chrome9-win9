#!/usr/bin/env python3
"""VX2COL (colour pattern from video memory measurement program): its logic is run against a host model of the lab with known layouts and known faults."""
import os,subprocess,tempfile
HERE=os.path.dirname(os.path.abspath(__file__));SRC=os.path.join(os.path.dirname(HERE),'src')
def vx2col_checks():
 cases=[];d=tempfile.mkdtemp();exe=os.path.join(d,'vx2col_host')
 r=subprocess.run(['gcc','-DMOCK','-w','-I'+SRC,'-o',exe,os.path.join(SRC,'vx2col.c'),os.path.join(HERE,'vx2col_mock.c')],capture_output=True,text=True);assert r.returncode==0,r.stderr
 def run(**env):
  e=dict(os.environ);e.update({k:str(v) for k,v in env.items()})
  p=subprocess.run([exe],capture_output=True,text=True,env=e);return p.returncode,p.stdout
 rc,o=run(COLLAY='1,0,1,2,5');assert rc==0 and 'PASS: colour pattern' in o
 assert o.count('fit transpose 1 mirrorX 0 mirrorY 1 phase (2,5)')>=20;cases.append('a known layout (transpose, mirror Y, phase 2,5) is found at all three depths')
 assert o.count('anchored to the screen')==9 and 'phase = base + offset' in o;cases.append('screen anchoring and an additive offset are reported')
 rc,o=run(COLANCHOR=1,COLOFF=-1);assert 'anchored to the rectangle corner' in o and 'phase = base - offset' in o;cases.append('rectangle anchoring and a subtractive offset are reported')
 rc,o=run(COLBUG=1);assert rc==1 and 'stale pattern' in o;cases.append('a stale pattern cache is diagnosed')
 rc,o=run(COLBUG=2);assert rc==1 and 'no layout explains' in o;cases.append('a depth that ignores the block fails with the observed tile logged')
 rc,o=run(COLBUG=3);assert rc==1 and 'D1' in o and 'no layout' in o;cases.append('an alignment requirement fails the misaligned block tests')
 rc,o=run(COLBUG=4);assert rc==1 and 'SPILL' in o;cases.append('a clip rectangle that is ignored is reported as a spill')
 rc,o=run(COLBUG=5);assert rc==1 and 'monochrome all-ones fill after colour fills: FAIL' in o;cases.append('a monochrome fill disturbed by a colour fill is reported')
 return cases
