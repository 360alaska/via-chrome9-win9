#!/usr/bin/env python3
""".605: the VX900's vertex input in the driver's 3D layer (vx_native3d_hw.c v900_layout) in the compiled VxD, by the register path:
with vx3dn_chip900 set, a draw's Type 04 words end with the VX900's (the descriptors, 04/12, the masks, 04/60-62), constant 0 by selector
32030000 and the vertex program by 31030000, as the board drew them (VX93D v1.3 form 3, v1.4 form 4, v1.6 form Y on .604); the vertex
words are the same as on the VX855; without the flag not one word differs from before; the chip's gate (VX_chip_accel) follows the
test switch ACCEL_VX900_3D on the VX900 and nothing else. The host has no model of the VX900's 3D engine: what it draws is the
board's answer."""
import struct,sys,pathlib
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent.parent/'verification'))
from engine import pack
from native3d import NativeGPU,packet,full_packet
SH_COUNT,SH_WORDS=5528,5532
def run(chip900,p):
 g=NativeGPU(32)
 g.u.mem_write(g.addr('vx3dn_chip900'),pack([chip900]));g.u.mem_write(g.addr('vx3dn_fetch'),pack([0]))
 rc=g.submit(p);return rc,list(g.port_trace),g
def split(tr):
 """the trace as (type selector, word) with the selector in force"""
 sel=None;out=[]
 for port,v in tr:
  if port==0x43c:sel=v
  else:out.append((sel,v))
 return out
def t4(words):return {v>>24:v&0xFFFFFF for s,v in words if s==0x00040000}
def block(words,sel):
 """the data words of the last block with this selector"""
 runs=[];cur=None
 for s,v in words:
  if s==sel:
   if cur is None:cur=[];runs.append(cur)
   cur.append(v)
  else:cur=None
 return runs[-1] if runs else None
def main():
 fails=0;n=0
 def check(ok,name,extra=''):
  nonlocal fails,n;n+=1;fails+=not ok;print('PASS' if ok else 'FAIL',name,'' if ok else extra)
 # 1. the plain packet: five words a vertex (X Y Z W, diffuse)
 rc0,tr0,_=run(0,packet(32));rc9,tr9,_=run(1,packet(32))
 check(rc0==0 and rc9==0,'both draws accepted',(rc0,rc9))
 w0,w9=split(tr0),split(tr9)
 r=t4(w9)
 check(r.get(0)==0xA80105 and r.get(1)==0x0000C2 and r.get(2)==0x0001D1 and all(r.get(k)==0 for k in range(3,9)),'5 words: 04/00 = A80105, inputs 0 (four floats) and 1 (a D3DCOLOR), the other descriptors 0',{k:hex(v) for k,v in r.items() if k<9})
 check(r.get(0x12)==3 and r.get(0x25)==0xF40000 and r.get(0x28)==0xF40000,'04/12 = 3, the masks F40000',(r.get(0x12),hex(r.get(0x25,0)),hex(r.get(0x28,0))))
 check(r.get(0x60)==0x800001 and r.get(0x61)==0x800000 and r.get(0x62)==0x104001,'04/60-62 = 800001, 800000, 104001',[hex(r.get(k,0)) for k in (0x60,0x61,0x62)])
 check(block(w9,0x32030000)==[0x49400000,0x49400000,0,0],'constant 0 = (786432.0, 786432.0, 0, 0) by selector 32030000',block(w9,0x32030000))
 check(block(w9,0x31030000)==[0x0008014C,0x00206490,0x0040014C,0x0004021F,0x0008414C,0,0,0x0008422F],'the program: output 1 = input 0 x 1.0 + constant 0; diffuse to output 2, clamped',[hex(x) for x in block(w9,0x31030000) or []])
 check(r.get(0x52)==0x147FFF and r.get(0x50)==0 and r.get(0x51)==0,'04/50-52 = 0, 0, 147FFF (the three words the chip needs, already the set-up\'s)')
 v0=[v for s,v in w0 if s==0];v9=[v for s,v in w9 if s==0]
 check(v0==v9 and len(v9)==15,'the vertex words are the VX855\'s, unchanged (15)',(len(v0),len(v9)))
 # the VX900 words are an insertion: everything else is word for word the VX855's stream
 ins=[x for x in tr9];base=[x for x in tr0]
 i=0;rest=[]
 for x in ins:
  if i<len(base) and x==base[i]:i+=1
  else:rest.append(x)
 check(i==len(base),'every word of the VX855\'s stream is there, in order',(i,len(base)))
 check(len(rest)==1+15+1+4+1+8,'and only the VX900\'s words are added (the selector, 15 registers, constant 0, the program)',len(rest))
 # 2. a program packet: XP's ten words (X Y Z W, diffuse, specular, two texture sets)
 p=full_packet(32,[(0,3)]);struct.pack_into('<I',p,SH_COUNT,1);struct.pack_into('<4I',p,SH_WORDS,0x01C00530,0x301C0053,0x00F04005,0x000D0820)
 rc0,tr0,_=run(0,p);rc9,tr9,_=run(1,p)
 if rc0 or rc9:check(False,'the ten-word packet accepted',(rc0,rc9))
 else:
  w0,w9=split(tr0),split(tr9);r=t4(w9)
  check(r.get(0)==0xA8040A and [r.get(k) for k in range(1,9)]==[0x0000C2,0x0001D1,0x0002D1,0x000342,0x000442,0,0,0],'10 words: 04/00 = A8040A, five inputs (position, two colours, two texture sets)',[hex(r.get(k,0)) for k in range(0,9)])
  check(r.get(0x12)==0x1F and r.get(0x25)==0xF6CC00 and r.get(0x28)==0xF6CC00 and r.get(0x60)==0x800004 and r.get(0x62)==0x404004,'04/12 = 1F, the masks F6CC00, 04/60 = 800004, 04/62 = 404004',[hex(r.get(k,0)) for k in (0x12,0x25,0x28,0x60,0x62)])
  check(block(w9,0x31030000)==[0x0008014C,0x00206490,0x0040014C,0x0004021F,0x0008414C,0,0,0x0008422F,0x0008814C,0,0,0x0008423F,0x0008C124,0,0,0x00080283,0x00090124,0,0,0x00080293],'the program: position, diffuse to 2, specular to 3, the sets to 8 and 9',[hex(x) for x in block(w9,0x31030000) or []])
  v0=[v for s,v in w0 if s==0];v9=[v for s,v in w9 if s==0]
  check(v0==v9 and len(v9)==30,'the vertex words unchanged (30)',(len(v0),len(v9)))
  check(len([1 for s,v in w9 if s==0x31030000])==20,'one program upload a packet (the set-up\'s own five-word layout is not sent first)',len([1 for s,v in w9 if s==0x31030000]))
 # 3. the gate
 g=NativeGPU(32)
 def accel(chip,sw):
  g.u.mem_write(g.addr('vx_chip_id'),pack([chip]));return g.call('VX_chip_accel',())
 check([accel(0x7122,0),accel(0x7122,1),accel(0x5122,0),accel(0x1122,0),accel(0x3371,1)]==[1,1,1,1,0],'VX_chip_accel: the VX900, the VX855 and the VX800 always (.617: no switch), another device never')
 # 4. .606: the occlusion count is refused on the VX900 when a query begins (the ICD then counts in software), and taken as before elsewhere
 from engine import ARENA
 def q(chip):
  g=NativeGPU(32);g.u.mem_write(g.addr('vx3dn_chip900'),pack([chip]));g.u.mem_write(ARENA+0x3000,pack([0x55]));return g.call('VX3DN_query',(0,0x1234,ARENA+0x3000)),len(g.port_trace)
 r9,w9=q(1);r0,w0=q(0)
 check(r9==2 and w9==0,'VX900: a query\'s begin is answered UNSUPPORTED and writes nothing',(r9,w9))
 check(r0!=2 or w0==0,'VX855: the begin is not refused for the chip',(r0,w0))
 # 5. .607: the same draw through the command fetcher (ACCEL_VX900_3D bit 1): the VX900's words are in the fetched stream, whole and in
 #    blocks the fetcher takes (the constant and the program as FE03 data blocks under their selectors), the vertex words unchanged
 from native3d import FetchGPU,decode,FB
 def fetched(chip):
  g=FetchGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_chip900'),pack([chip]))
  q=packet(32);struct.pack_into('<I',q,20,0x100000);rc=g.submit(q)
  ctl=[v for a,v in g.port_trace if a==0x420];w=ctl[:7]
  if [x>>24 for x in w]!=[0x60,0x61,0x62,0x63,0x64,0x65,0x68]:return rc,None,g
  start=(w[0]&0xFFFFFF)|((w[1]&0xFF)<<24);pause=(w[4]&0xFFFFFF)|((w[5]&0xFF)<<24);k=(pause+4-start)//4
  return rc,decode(list(struct.unpack('<%dI'%k,bytes(g.u.mem_read(FB+start,4*k))))),g
 rc9,d9,g9=fetched(1);rc0,d0,g0=fetched(0)
 check(rc9==0 and d9 is not None and rc0==0 and d0 is not None,'both draws go through the fetcher',(rc9,rc0))
 if d9 and d0:
  b9={b[0]:b[1:] for t,b in d9 if t=='43C'};b0={b[0]:b[1:] for t,b in d0 if t=='43C'}
  check(b9.get(0x32030000)==[0x49400000,0x49400000,0,0],'fetched: constant 0 under selector 32030000',b9.get(0x32030000))
  check(b9.get(0x31030000)==[0x0008014C,0x00206490,0x0040014C,0x0004021F,0x0008414C,0,0,0x0008422F],'fetched: the program under selector 31030000',[hex(x) for x in b9.get(0x31030000) or []])
  check(0x31030000 not in b0 and 0x32030000 not in b0,'the VX855\'s fetched stream has neither')
  check(b9.get(0)==b0.get(0) and b9.get(0),'fetched: the vertex words are the VX855\'s',(b9.get(0),b0.get(0)))
  # .614: CE's wait words (eight 2D blocks with register 6C = 00800000, a ninth with 00FC0000) after the burst, on the VX900 only
  def raw(g):
   ctl=[v for a,v in g.port_trace if a==0x420];w=ctl[:7];start=(w[0]&0xFFFFFF)|((w[1]&0xFF)<<24);pause=(w[4]&0xFFFFFF)|((w[5]&0xFF)<<24);k=(pause+4-start)//4
   return list(struct.unpack('<%dI'%k,bytes(g.u.mem_read(FB+start,4*k))))
  blk=lambda v:[0xFE000000,1,0,0xCC000000,0x6C,v,0xCC000000,0xCC000000]
  def count(w,b):return sum(1 for i in range(0,len(w)-7,4) if w[i:i+8]==b)
  r9,r0=raw(g9),raw(g0)
  check(count(r9,blk(0x00800000))==0 and count(r9,blk(0x00FC0000))==1,'fetched, VX900: the one wait block (6C = 00FC0000) after the burst (.632: always; ACCEL_VX900_WAITS gone)',(count(r9,blk(0x00800000)),count(r9,blk(0x00FC0000))))
  check(count(r0,blk(0x00800000))==0 and count(r0,blk(0x00FC0000))==0,'fetched, VX855: no wait blocks')
  err=g9.word(g9.addr('cap_err')) if 'cap_err' in g9.image.symbols else 0
  check(err==0,'no capture error',err)
 print('vx900_d3d: %d checks, %d failures'%(n,fails));sys.exit(1 if fails else 0)
if __name__=='__main__':main()
