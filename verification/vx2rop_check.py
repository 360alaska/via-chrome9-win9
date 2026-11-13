#!/usr/bin/env python3
"""VX2ROP (ROP3 measurement program): its logic is run against a host model of the lab with an exact ROP3 engine and with injected faults."""
import os,subprocess,tempfile
HERE=os.path.dirname(os.path.abspath(__file__));SRC=os.path.join(os.path.dirname(HERE),'src')
def vx2rop_checks():
 cases=[];d=tempfile.mkdtemp();exe=os.path.join(d,'vx2rop_host')
 r=subprocess.run(['gcc','-DMOCK','-w','-I'+SRC,'-o',exe,os.path.join(SRC,'vx2rop.c'),os.path.join(HERE,'vx2rop_mock.c')],capture_output=True,text=True);assert r.returncode==0,r.stderr
 def run(**env):
  e=dict(os.environ);e.update({k:str(v) for k,v in env.items()})
  p=subprocess.run([exe],capture_output=True,text=True,env=e);return p.returncode,p.stdout
 rc,o=run();assert rc==0 and 'PASS: 960 operations ran' in o
 assert o.count('256 ROPs run, 256 match')==3 and o.count('16 ROPs run, 16 match')==12;cases.append('an exact ROP3 engine matches in all 960 operations (T1 3x256, T2 to T5 12x16)')
 rc,o=run(ROPBUG='1:88');assert 'ROP 88: ' in o and o.count('255 match')==3 and 'failing: 88' in o;cases.append('a wrong ROP in the fixed-colour form is named at every depth')
 rc,o=run(ROPBUG='5:a0');assert 'failing: A0' in o and o.count('15 match')==3;cases.append('a transparent mono fill that paints the zero pixels is named')
 rc,o=run(ROPHANG='2:ee');assert rc==0 and 'PASS with hangs' in o and 'HANG' in o and 'hung the engine' in o;cases.append('a hang is logged, recovered and the run continues')
 return cases
