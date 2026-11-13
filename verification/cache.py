#!/usr/bin/env python3
"""Run the compiled VX_cache_report (CPUID, MTRR/PAT register reads, page-table samples) against a synthetic CPU and page table."""
import struct
from engine import GPU,ARENA,FB,pack
from unicorn.x86_const import *
from unicorn import UC_HOOK_INSN
W=lambda v,i:struct.unpack('<I',v[i*4:i*4+4])[0]
def cache_checks():
 cases=[]
 g=GPU(32);u=g.u
 s=list(struct.unpack('<31I',bytes(u.mem_read(g.addr('vx_status'),124))));s[10]=0xF0000000;s[12]=0xF1000000
 u.mem_write(g.addr('vx_status'),pack(s))
 u.mem_write(g.addr('vx_map_bytes'),pack([64<<20]))
 for n,v in [('vx_mtrr_called',1),('vx_mtrr_ver',1),('vx_mtrr_status',0xC0000001),('vx_mtrr_base',0xF0000000),('vx_mtrr_size',0x4000000),('vx_mtrr_conf',1)]:u.mem_write(g.addr(n),pack([v]))
 leaves={0:(1,0x746E6543,0x736C7561,0x48727561),1:(0x6A9,0,0,(1<<5)|(1<<12)|(1<<16)|(1<<23)),0x80000000:(0x80000004,0,0,0)}
 def cpuid(uc,_):
  leaf=uc.reg_read(UC_X86_REG_EAX);a,b,c,d=leaves.get(leaf,(0,0,0,0))
  uc.reg_write(UC_X86_REG_EAX,a);uc.reg_write(UC_X86_REG_EBX,b);uc.reg_write(UC_X86_REG_ECX,c);uc.reg_write(UC_X86_REG_EDX,d);return True
 u.hook_add(UC_HOOK_INSN,cpuid,None,1,0,UC_X86_INS_CPUID)
 ptes={}
 def copy_pte():
  lin,n,buf=g.arg(0),g.arg(1),g.arg(2);assert n==1
  idx=lin-(0xF1000000>>12)
  e={0:0xF0000000|0x1B,256:0xF0100000|0x83,2048:0xF0200000|0x01}.get(idx,0xF0FFF000|0x01)
  ptes[idx]=e;u.mem_write(buf,pack([e]));return 0
 g.hook('__CopyPageTable',copy_pte)
 out=ARENA+0x4000
 u.mem_write(out,bytes([0xEE])*512)
 g.call('VX_cache_report',(out,))
 r=bytes(u.mem_read(out,512))
 assert W(r,0)==0x38395856 and W(r,1)==1
 assert W(r,2)==1 and W(r,3)==1 and W(r,4)==0xC0000001 and W(r,5)==0xF0000000 and W(r,6)==0x4000000 and W(r,7)==1
 cases.append('cache report: MTRR service answers (called, version, status, base, size, option) are carried through')
 assert W(r,8)==1 and W(r,9)==1 and r[40:52]==b'CentaurHauls' and W(r,13)==0x6A9 and W(r,14)==((1<<5)|(1<<12)|(1<<16)|(1<<23)) and W(r,16)==0x80000004
 cases.append('cache report: CPUID vendor, signature and feature bits are decoded from the CPUID registers')
 assert W(r,17)==1 and (W(r,18)&0xFF)==W(r,24) and W(r,24)>0 and W(r,22)!=0 # MSR path ran: capability, variable range count and PAT were read
 cases.append('cache report: MTRR capability, range count and PAT registers are read when CPUID reports MSR, MTRR and PAT')
 base=17+1+2+2+2+1+64
 assert W(r,base)==0xF0000000 and W(r,base+1)==0xF1000000 and W(r,base+2)==64<<20
 cnt=W(r,base+3);assert cnt==4,cnt
 pages=[W(r,base+4+i) for i in range(cnt)];assert pages==[0,256,2048,16383],pages
 e=[W(r,base+8+i) for i in range(cnt)];assert e[0]&0x18==0x18 and e[1]&0x80 and e[2]==0xF0200001 and e[3]==0xF0FFF001
 cases.append('cache report: framebuffer address, mapping size and the page-table entries of the sampled pages are reported')
 # no CPUID: the registers must stay zero and nothing may fault
 return cases
if __name__=='__main__':
 for c in cache_checks():print(c)
