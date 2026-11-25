#!/usr/bin/env python3
"""c7est.py LISTING [label]: a rough cycle estimate of a kernel's loops on the VIA C7 (Esther), from InstLatx64's measured latency (L) and
throughput (T) of each instruction. Model: in order, one instruction issued at a time; an instruction issues when the one before it has
held the issue stage for its T and its register inputs are ready; its result is ready L later. Reads llvm-objdump -d -M intel output
(or 'mnemonic operands' lines). Prints, per loop (a backward jump), instructions and cycles an iteration in the steady state."""
import re,sys
FP=(6,2);LOG=(2,2);INT=(2,2)
TAB={'addps':FP,'subps':FP,'mulps':FP,'minps':FP,'maxps':FP,'cmpps':FP,'cmpltps':FP,'cmpleps':FP,'cmpeqps':FP,'cmpneqps':FP,'cmpnltps':FP,'cmpnleps':FP,'cmpunordps':FP,'cmpordps':FP,
 'addss':FP,'subss':FP,'mulss':FP,'minss':FP,'maxss':FP,
 'andps':LOG,'orps':LOG,'xorps':LOG,'andnps':LOG,'andpd':LOG,'orpd':LOG,'xorpd':LOG,'andnpd':LOG,
 'movhlps':(2,1),'movlhps':(2,1),'unpcklps':(3,2),'unpckhps':(3,2),'shufps':(4,3),'unpcklpd':(3,2),'unpckhpd':(3,2),'shufpd':(4,3),
 'cvtps2dq':FP,'cvtdq2ps':FP,'cvttps2dq':FP,'divps':(72,72),'rcpps':(20,16),'rsqrtps':(26,22),'sqrtps':(74,74),'divss':(34,36),'sqrtss':(36,36),
 'movmskps':(7,3),'movmskpd':(7,3),'pmovmskb':(6,3),'cvtps2pd':(9,4),'cvtss2sd':(9,4),'mulpd':(7,4),'mulsd':(7,2),'subpd':FP,'addpd':FP,'subsd':FP,'addsd':FP,
 'cmplesd':FP,'cmpltsd':FP,'cmpsd':FP,'cmppd':FP,'comiss':(18,18),'ucomiss':(18,18),'comisd':(18,18),'ucomisd':(18,18),
 'pshufd':(4,3),'pshuflw':(4,3),'pshufhw':(4,3),'psrldq':(3,2),'pslldq':(3,2),
 'imul':(5,5),'cmovne':(2,2),'cmove':(2,2),'cmovb':(2,2),'cmovae':(2,2),'cmova':(2,2),'cmovbe':(2,2),'cmovg':(2,2),'cmovl':(2,2),
 'prefetcht0':(0,1),'nop':(0,1),'push':(1,1),'pop':(2,1),'ret':(1,5),'rep':(1,10)}
XR=re.compile(r'\b(xmm[0-7]|e[abcd]x|e[sd]i|e[bs]p|[abcd][lh]|[abcd]x|[sd]i|bp)\b')
CAN={'al':'eax','ah':'eax','ax':'eax','bl':'ebx','bh':'ebx','bx':'ebx','cl':'ecx','ch':'ecx','cx':'ecx','dl':'edx','dh':'edx','dx':'edx','si':'esi','di':'edi','bp':'ebp'}
def regs(s):return [CAN.get(r,r) for r in XR.findall(s)]
def parse(path):
 ins=[]
 for ln in open(path):
  m=re.match(r'^\s*([0-9a-f]+):\s*(?:(?:[0-9a-f]{2}\s)+\s*)?([a-z][a-z0-9]*)\s*(.*)$',ln)
  if not m:continue
  a,mn,ops=int(m.group(1),16),m.group(2),m.group(3).split('#')[0].split('<')[0].strip()
  ins.append((a,mn,ops))
 return ins
def cost(mn,ops):
 o=[x.strip() for x in re.split(r',(?![^\[]*\])',ops)] if ops else []
 mem=[x for x in o if '[' in x];dm=bool(o) and '[' in o[0]
 if mn in('movaps','movdqa','movapd'):return ((0,3) if dm else (2,2))
 if mn in('movups','movdqu'):return ((0,9) if dm else ((9,9) if mem else (2,2)))
 if mn in('movlps','movhps','movlpd','movhpd'):return ((0,2) if dm else (2,1))
 if mn in('movss','movsd','movd','movq'):
  if dm:return (0,2)
  if mem:return (2,2)
  if mn=='movd' and o and not o[0].startswith('xmm'):return (7,2)
  return (2,2)
 if mn in TAB:return TAB[mn]
 if mn[0]=='p':return INT
 if mn[0]=='j':return (0,2)
 if mn in('mov','movzx','movsx'):return ((0,1) if dm else ((3,1) if mem else (1,1)))
 if mn.startswith('set'):return (1,1)
 if mem and not dm and mn in('add','sub','and','or','xor','cmp','test'):return (4,2)
 if dm and mn in('add','sub','and','or','xor','inc','dec'):return (6,6)
 return (1,1)
WRONLY={'movaps','movups','movdqa','movdqu','movapd','movss','movsd','movd','movq','mov','movzx','movsx','lea','rcpps','rsqrtps','sqrtps','cvtps2dq','cvtdq2ps','cvttps2dq','cvtps2pd','pshufd','pshuflw','pshufhw','pop'}
NOWR={'cmp','test','push','prefetcht0','ucomiss','comiss','ucomisd','comisd','nop'}
def sim(body,iters=6):
 ready={};t=0.0;marks=[]
 for it in range(iters):
  for a,mn,ops in body:
   L,T=cost(mn,ops)
   o=[x.strip() for x in re.split(r',(?![^\[]*\])',ops)] if ops else []
   dst=o[0] if o else '';dm='[' in dst
   src=[]
   for k,x in enumerate(o):
    if k==0 and not dm and (mn in WRONLY or (mn in('xorps','pxor','xorpd','pcmpeqd','xor','sub') and len(o)==2 and o[0]==o[1])):continue
    src+=regs(x)
   if dm:src+=regs(dst)
   st=max([t]+[ready.get(r,0) for r in src])
   if not dm and dst and mn not in NOWR and mn[0]!='j':
    for r in regs(dst)[:1]:ready[r]=st+L
   t=st+T
  marks.append(t)
 return marks[-1]-marks[-2]
def main():
 ins=parse(sys.argv[1]);idx={a:i for i,(a,_,_) in enumerate(ins)};loops=[]
 for i,(a,mn,ops) in enumerate(ins):
  if mn[0]=='j':
   m=re.match(r'^(?:0x)?([0-9a-f]+)',ops)
   if m and int(m.group(1),16) in idx and idx[int(m.group(1),16)]<=i:loops.append((idx[int(m.group(1),16)],i))
 tot=sum(cost(mn,ops)[1] for _,mn,ops in ins)
 print('%s: %d instructions, %d bytes'%(sys.argv[2] if len(sys.argv)>2 else sys.argv[1],len(ins),ins[-1][0]+1 if ins else 0))
 for h,e in loops:
  body=ins[h:e+1]
  print('  loop %x..%x: %3d instructions, sum of T %.0f, estimate %.0f cycles an iteration'%(ins[h][0],ins[e][0],len(body),sum(cost(mn,ops)[1] for _,mn,ops in body),sim(body)))
if __name__=='__main__':main()
