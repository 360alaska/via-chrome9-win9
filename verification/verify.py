#!/usr/bin/env python3
"""Build-artifact checks and compiled-code tests; no Win98 boot or hardware test."""
import json,re,struct,hashlib
from validate import ROOT,ne_check,LEImage,pe_check,u16,u32
from engine import checks as engine_checks
from frontend import checks as frontend_checks
from directdraw import checks as directdraw_checks
from flip import checks as flip_checks
from cache import cache_checks
from eight import eight_checks
from v1073 import v1073_checks
from v1075 import v1075_checks
from v10105 import v10105_checks
from v10106 import v10106_checks
from v10116 import v10116_checks
from v1077 import v1077_checks
from vx2col_check import vx2col_checks
from vx2rop_check import vx2rop_checks
from vx2line_check import vx2line_checks
from keep import keep_checks

def abi_check():
 b=(ROOT/'driver'/'VX85598.DRV').read_bytes();h=u32(b,60)
 match=re.search(r'0002:([0-9a-fA-F]{4})[* ]+_VXDDABI',(ROOT/'verification'/'VX85598_DRV.map').read_text());assert match
 seg=h+u16(b,h+0x22)+8;off=(u16(b,seg)<<u16(b,h+0x32))+int(match[1],16);ne=struct.unpack_from('<7I',b,off)
 b=(ROOT/'driver'/'VXDDRAW.DLL').read_bytes();h=u32(b,60);opt=h+24;base=u32(b,opt+28)
 assert base>=0x80000000,('HAL must be in shared Win9x address space',hex(base))
 entry=re.search(r'([0-9a-fA-F]{8})[* +]+_DllMain@12',(ROOT/'verification'/'VXDDRAW.map').read_text());assert entry
 assert base+u32(b,opt+16)==int(entry[1],16)
 for i in range(u16(b,h+6)):
  s=opt+u16(b,h+20)+i*40;assert u32(b,s+36)&0x10000000,'HAL sections must be shared'
 assert not u16(b,h+22)&1 and u32(b,opt+136) and u32(b,opt+140),'HAL base relocations required'
 match=re.search(r'([0-9a-fA-F]{8})[* +]+VXDDABI',(ROOT/'verification'/'VXDDRAW.map').read_text());assert match
 rva=int(match[1],16)-base;pe=None
 for i in range(u16(b,h+6)):
  s=opt+u16(b,h+20)+i*40;at=u32(b,s+12);size=max(u32(b,s+8),u32(b,s+16))
  if at<=rva<at+size:pe=struct.unpack_from('<7I',b,u32(b,s+20)+rva-at)
 assert ne==pe==(27492,616,720,460,36,32,372),(ne,pe)
 # Verify the actual undecorated DriverInit export used by DirectDraw.
 def raw(r):
  for i in range(u16(b,h+6)):
   s=opt+u16(b,h+20)+i*40;at=u32(b,s+12);size=max(u32(b,s+8),u32(b,s+16))
   if at<=r<at+size:return u32(b,s+20)+r-at
  raise AssertionError(r)
 ep=raw(u32(b,opt+96));names=raw(u32(b,ep+32));exports=[]
 for i in range(u32(b,ep+24)):
  n=raw(u32(b,names+4*i));exports.append(b[n:b.index(0,n)].decode())
 assert {'DriverInit','VXDDABI','VXDBGKEY','VXDBGKEYON','VXDD3DABI','VX_D3D_QueryStats'}<=set(exports)
 assert u16(b,h+22)&0x2000 # IMAGE_FILE_DLL
 inf=(ROOT/'driver'/'VX85598.INF').read_text();assert 'VXDDRAW.DLL=1' in inf and 'VXDDRAW.DLL,,,0x00000004' in inf
 return {'layout':ne,'exports':exports,'inf_installs_callback_dll':True}

def main():
 build=json.loads((ROOT/'verification/BUILD.json').read_text())
 for name,expected in build['source_sha256'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==expected,('build/source mismatch',name)
 result={'scope':__doc__,'ne':ne_check(),'le':LEImage().result(),'abi':abi_check(),
  'pe':{n:pe_check(n,3 if n in ['VXDX7.EXE','VX3DM1.EXE'] else 2) for n in ['VXDDRAW.DLL','VX2SYNC.EXE','VX2BENCH.EXE','VX2FLIP.EXE','VXDX7.EXE','VX3DM1.EXE']}}
 for name,fn in [('engine',engine_checks),('frontend',frontend_checks),('flip',flip_checks),('directdraw',directdraw_checks),('cache',cache_checks),('eight',eight_checks),('v1073',v1073_checks),('v1075',v1075_checks),('v10105',v10105_checks),('v10106',v10106_checks),('v10116',v10116_checks),('v1077',v1077_checks),('vx2col',vx2col_checks),('vx2rop',vx2rop_checks),('vx2line',vx2line_checks),('keep',keep_checks)]:
  cases=fn();result[name]={'passed':len(cases),'cases':cases};print(f'{name}: {len(cases)} passed',flush=True)
 preservation=json.loads((ROOT/'verification'/'PRESERVATION.json').read_text())
 for name,expected in preservation['unchanged_files'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==expected,name
 for name,rule in preservation['modified_files'].items():
  assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==rule['current_sha256'],name
 result['preservation']=preservation
 result['passed_compiled_code_checks']=sum(result[n]['passed'] for n in ['engine','frontend','flip','directdraw','cache','eight','v1073','v1075','v10105','v10106','v10116','v1077','vx2col','vx2rop','vx2line','keep'])
 (ROOT/'verification'/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
 print(f"{result['passed_compiled_code_checks']} compiled-code checks passed; loader/ABI/preservation checks passed.")
if __name__=='__main__':main()
