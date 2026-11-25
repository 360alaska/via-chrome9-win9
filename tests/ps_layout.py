#!/usr/bin/env python3
""".730: every pixel program the composer builds, and the colour-key program, read with the shader layout of docs/PIXEL_SHADER_ANSWERS.md
(VIA's PDB fields, tools/ps_decode.py): random DirectX 7 stage states (one to three stages, every colour and alpha operation, every argument
and modifier, one or two textures, TFACTOR, specular), each draw's ALU words and explicit TAU words checked: reserved bits clear, opcodes,
source banks and modifiers the drivers use, a write mask. A word outside the layout means the layout (or the composer) is wrong before the
R2 work builds on it. usage: ps_layout.py [core.so] [cases]"""
import sys,os,random,ctypes as C
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)));sys.path.insert(0,os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','tools'))
import test_core as T,ps_decode as D
if len(sys.argv)>1 and sys.argv[1]!='-':T.lib=C.CDLL(os.path.abspath(sys.argv[1]))
N=int(sys.argv[2]) if len(sys.argv)>2 else 3000
class B(T.Contract):
 def runTest(self):pass
t=B();t.setUp();T.U.in_dll(T.lib,'vx_d3d_feat').value=T.ALLF
rnd=random.Random(730);ops=list(range(1,25));args=[0,1,2,3,4]+[a|0x10 for a in (0,1,2,3)]+[a|0x20 for a in (1,2)]
seen=set();bad=0;progs=0;words=0;taus=0;opcodes={}
for n in range(N):
 st=[]
 for k in range(rnd.choice((1,1,2,2,3))):
  st.append((5 if rnd.random()<0.8 else 0,rnd.choice(ops),rnd.choice(args),rnd.choice(args),rnd.choice(ops),rnd.choice(args),rnd.choice(args),rnd.choice((0,0,1))))
 t._stages(st,spec=rnd.random()<0.3);t.context.state.rs[60]=rnd.getrandbits(32)
 t.calls.clear()
 try:t.run_dp2(t.tri())
 except Exception:continue
 for p in t.calls:
  c=p.shader.count
  if not c:continue
  w=list(p.shader.words[:c*4]);key=tuple(w)
  if key in seen:continue
  seen.add(key);progs+=1
  for i in range(0,len(w),4):
   words+=1;e=D.check_alu(*w[i:i+4]);op=D.decode_alu(*w[i:i+4])['op'];opcodes[op]=opcodes.get(op,0)+1
   if e:
    bad+=1
    if bad<=10:print('  case %d instruction %d: %08X %08X %08X %08X %s: %s'%(n,i//4,*w[i:i+4],D.decode_alu(*w[i:i+4])['text'],'; '.join(e)))
  if p.stages.ps:
   for k in range(p.stages.ntau):
    taus+=1;e=D.check_tau(p.stages.tau[k])
    if e:
     bad+=1
     if bad<=10:print('  case %d TAU %d: %08X: %s'%(n,k,p.stages.tau[k],'; '.join(e)))
t._reset_stages()
print('pixel programs read with the PDB layout: %d different programs, %d instructions, %d TAU words; opcodes %s; %d outside the layout'%(progs,words,taus,' '.join('%s %d'%(D.OPS.get(k,'%02X'%k),v) for k,v in sorted(opcodes.items())),bad))
print('PASS' if bad==0 and progs>20 else 'FAIL');sys.exit(0 if bad==0 and progs>20 else 1)
