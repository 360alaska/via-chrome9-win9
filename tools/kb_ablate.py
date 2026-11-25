"""kb_ablate.py (.749B): cut-down copies of the two position kernels for VXKBENCH only (src/vx_kbench_abl.asm). They compute WRONG results
on purpose and are never linked into the driver. Each is the kernel's own bytes (tools/tnl_win.py) with single instructions replaced by
one of the same length, so every jump keeps its target:
  d  the divide: divps (0F 5E) -> mulps (0F 59)
  a  the four clip-array stores (movups [esi+ecx*4],xmm: cx, cy, cz, cw) -> lea esi,[esi+0]
  r  the four record stores (movups [esi] / [esi+edi] / [esi+edi*2],xmm) -> lea esi,[esi+0]
Variants: d, a, r, ar, dar. What is left in 'dar' is the transform's multiplies and adds, the clip codes and the loop."""
import sys,re
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE))
import tnl_win,tnl_asm
def rows(key):return tnl_asm.listed('_k',bytes.fromhex(tnl_win.WIN[key]))
def cut(rs,what):
 out=[];n={'d':0,'a':0,'r':0}
 for b,m in rs:
  if b[:2]==b'\x0f\x5e' and 'divps' in m:
   n['d']+=1
   if 'd' in what:b=b'\x0f\x59'+b[2:];m='mulps (was divps)'
  elif b[:2]==b'\x0f\x11' and len(b)==4 and b[3]==0x8e:
   n['a']+=1
   if 'a' in what:b=b'\x8d\x74\x26\x00';m='lea esi,[esi+0] (was a clip-array store)'
  elif b[:2]==b'\x0f\x11' and ((len(b)==3 and (b[2]&0xC7)==0x06) or (len(b)==4 and b[3] in (0x3e,0x7e))):
   n['r']+=1
   if 'r' in what:b=(b'\x8d\x76\x00' if len(b)==3 else b'\x8d\x74\x26\x00');m='lea esi,[esi+0] (was a record store)'
  out.append((b,m))
 if (n['d'],n['a'],n['r'])!=(1,4,4):sys.exit('kb_ablate: the kernel is not the shape expected: %r'%n)
 return out
def main():
 parts=[];names=[]
 for key,tag in (('pos_c7','c7'),('pos_o','o')):
  rs=rows(key)
  if len(rs)<50:sys.exit('kb_ablate needs clang and llvm-objdump for the instruction boundaries')
  for v in ('d','a','r','ar','dar'):
   nm='_tnk_abl_%s_%s'%(tag,v);names.append(nm);parts.append(tnl_asm.wasm(nm,cut(rs,v)))
 (HERE.parent/'src'/'vx_kbench_abl.asm').write_text('; vx_kbench_abl.asm: cut-down position kernels for VXKBENCH only (tools/kb_ablate.py). WRONG RESULTS ON PURPOSE.\n.386\n.model flat\n.code\n'+'\n'.join(parts)+'\nend\n')
 print(len(names),'variants')
if __name__=='__main__':main()
