#!/usr/bin/env python3
""".575 (peer review of .572, finding 4; the fixture is the review's): the linked VXDDRAW.DLL with packets held and the callback lock taken
by another thread of the process for longer than the wait. Lock, Blt and Flip answer DDERR_WASSTILLDRAWING, handled, and touch nothing
(they went on with DD_OK); with the lock free the held packets are sent and the call goes on."""
import re,sys,pathlib
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent.parent/'verification'))
from directdraw import HAL,ARENA,ROOT,pack
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
class FaultHAL(HAL):
 tick=0
 def import_call(self,u,a,z,data):
  if data[0]!='GetTickCount':return super().import_call(u,a,z,data)
  self.tick+=101;sp=u.reg_read(UC_X86_REG_ESP)
  u.reg_write(UC_X86_REG_EAX,self.tick);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,self.word(sp))
def main():
 mp=ROOT/'verification/VXDDRAW.map'
 if not mp.exists():mp=ROOT/'build/VXDDRAW.map'
 sym={n:int(a,16) for a,n in re.findall(r'^([0-9a-fA-F]{8})[s+*]* +([^\s]+)',mp.read_text(),re.M)}
 fails=0
 for name,entry,off in (('Lock','_VXLock@4',32),('Blt','_VXBlit@4',None),('Flip','_VXFlip@4',None)):
  h=FaultHAL()
  for n,v in (('cq_held',1),('hold_pid',0x1000),('hold_tid',0x3000)):h.u.mem_write(sym[n],pack([v]))
  h.u.mem_write(sym['cb_enter'],b'\x31\xc0\xc3')  # the owning thread stays inside the HAL
  blk=ARENA+0xa000;h.u.mem_write(blk,pack([0,h.dst]+[0x5A5A5A5A]*40))
  ioc=len(getattr(h,'ioctls',[]))
  rc=h.call(entry,(blk,))
  words=[h.word(blk+4*k) for k in range(42)]
  held=h.word(sym['cq_held']);tmo=h.word(sym['hold_why']+28)
  ok=rc==1 and 0x8876021C in words and held==1 and tmo==1
  print(('PASS' if ok else 'FAIL'),name,'result',rc,'answer present',0x8876021C in words,'held',held,'timeouts',tmo)
  if not ok:fails+=1
 sys.exit(1 if fails else 0)
if __name__=='__main__':main()
