#!/usr/bin/env python3
"""Validate loader structures and execute the compiled VxD with mocked OS/BIOS calls.
This is not a Windows 98 or VX855 hardware emulator.
"""
import json, pathlib, re, struct, os
ROOT=pathlib.Path(__file__).resolve().parents[1]
DRIVER_PATH=ROOT/os.environ.get('VX_TEST_DRIVER_DIR','driver')
MAP_PATH=ROOT/'verification'/('software' if DRIVER_PATH.name=='software' else '')
u16=lambda b,o: struct.unpack_from('<H',b,o)[0]
u32=lambda b,o: struct.unpack_from('<I',b,o)[0]

def ne_check():
 b=(DRIVER_PATH/'VX85598.DRV').read_bytes(); h=u32(b,60)
 assert b[:2]==b'MZ' and b[h:h+2]==b'NE'
 assert u16(b,h+0x3e)==0x400 and u16(b,h+0xc)&0x8000
 segs=[]
 for i in range(u16(b,h+0x1c)):
  off,size,flags,minimum=struct.unpack_from('<4H',b,h+u16(b,h+0x22)+8*i)
  size=size or 65536; off <<=u16(b,h+0x32)
  assert off+size<=len(b); segs.append((off,size,flags,minimum))
 # Parse named export table, including ordinal contracts.
 names={}; p=h+u16(b,h+0x26)
 while b[p]:
  n=b[p];p+=1; name=b[p:p+n].decode('ascii');p+=n
  names[name]=u16(b,p);p+=2
 p=u32(b,h+0x2c)
 while b[p]:
  n=b[p];p+=1;name=b[p:p+n].decode('ascii');p+=n
  names[name]=u16(b,p);p+=2
 required={'BitBlt':1,'ColorInfo':2,'Control':3,'Disable':4,'Enable':5,
 'ExtTextOut':14,'ReEnable':31,'Inquire':101,'SetCursor':102,'MoveCursor':103,
 'CheckCursor':104,'GetDriverResourceID':450,'UserRepaintDisable':500,'ValidateMode':700}
 for name,ordinal in required.items(): assert names.get(name.upper())==ordinal,(name,names.get(name.upper()))
 # Ensure exported entries point inside real executable segments.
 entries={};p=h+u16(b,h+4);ordinal=1
 while b[p]:
  cnt,kind=b[p:p+2];p+=2
  if kind==0: ordinal+=cnt;continue
  for _ in range(cnt):
   if kind==255:
    flag=b[p];seg=b[p+3];off=u16(b,p+4);p+=6
   else:
    flag=b[p];seg=kind;off=u16(b,p+1);p+=3
   assert 1<=seg<=len(segs) and off<segs[seg-1][1]
   entries[ordinal]=(seg,off,flag);ordinal+=1
 for ordinal in required.values(): assert ordinal in entries
 # Validate imported module names.
 modules=[];refs=h+u16(b,h+0x28);imports=h+u16(b,h+0x2a)
 for i in range(u16(b,h+0x1e)):
  at=imports+u16(b,refs+2*i);n=b[at];modules.append(b[at+1:at+1+n].decode())
 assert 'DIBENG' in modules and set(modules)<={'DIBENG','KERNEL'}
 # Every segment relocation record must fit its segment and target structure.
 relocations=0
 for off,size,flags,_ in segs:
  if flags&0x100:
   p=off+size;count=u16(b,p);p+=2
   for _ in range(count):
    typ,flg,src,a,c=struct.unpack_from('<BBHHH',b,p);p+=8
    assert src<size
    if flg&3 in (1,2): assert 1<=a<=len(modules)
    elif flg&3==0:
     if a==255: assert c in entries,('unresolved internal entry',c)
     else: assert 1<=a<=len(segs),('invalid internal segment',a)
    relocations+=1
 return {'format':'NE','expected_windows':'4.0','segments':len(segs),'exports':len(entries),'imports':modules,'relocations':relocations}

class LEImage:
 def __init__(self):
  self.b=(DRIVER_PATH/'VX85598.VXD').read_bytes();b=self.b;h=u32(b,60);self.h=h
  assert b[h:h+2]==b'LE' and u16(b,h+10)==4
  assert u32(b,h+0x10)&0x8000
  self.pagesize=u32(b,h+0x28);assert self.pagesize==4096
  self.objects=[]; self.bases=[]
  obj=h+u32(b,h+0x40);pages=h+u32(b,h+0x48)
  for i in range(u32(b,h+0x44)):
   size,base,flags,first,count,_=struct.unpack_from('<6I',b,obj+24*i)
   assert base==0 and flags&4 # fixlink made all objects executable for the VxD loader
   data=bytearray((size+4095)&~4095)
   for j in range(count):
    pr=b[pages+(first+j-1)*4:pages+(first+j)*4]
    number=int.from_bytes(pr[:3],'big');assert pr[3]==0
    pos=u32(b,h+0x80)+(number-1)*4096
    n=u32(b,h+0x2c) if number==u32(b,h+0x14) else 4096
    assert pos+n<=len(b)
    data[j*4096:j*4096+n]=b[pos:pos+n]
   self.objects.append({'size':size,'flags':flags,'first':first,'pages':count,'data':data})
   self.bases.append(0xB0000000+i*0x100000)
  fp=h+u32(b,h+0x68);fr=h+u32(b,h+0x6c);self.fixups=0
  for oi,o in enumerate(self.objects):
   for j in range(o['pages']):
    pn=o['first']+j-1;p=fr+u32(b,fp+pn*4);end=fr+u32(b,fp+(pn+1)*4)
    while p<end:
     typ,flags=b[p:p+2];p+=2
     assert typ==7 and flags&3==0 and not flags&0xac,(typ,flags)
     src=struct.unpack_from('<h',b,p)[0];p+=2
     if flags&0x40:target=u16(b,p);p+=2
     else:target=b[p];p+=1
     if flags&0x10:off=u32(b,p);p+=4
     else:off=u16(b,p);p+=2
     assert 1<=target<=len(self.objects) and off<self.objects[target-1]['size']
     at=j*4096+src;assert 0<=at<=len(o['data'])-4
     struct.pack_into('<I',o['data'],at,self.bases[target-1]+off);self.fixups+=1
    assert p==end
  # Bundle 3 is a 32-bit exported entry; ordinal 1 is the DDB.
  p=h+u32(b,h+0x5c);assert b[p:p+2]==b'\x01\x03'
  ddbobj=u16(b,p+2);ddboff=u32(b,p+5)
  self.ddb_address=self.bases[ddbobj-1]+ddboff
  ddb=self.objects[ddbobj-1]['data'][ddboff:ddboff+80]
  assert u16(ddb,6)==0x4333 and ddb[12:20].rstrip(b' \0')==b'VX85598'
  self.symbols={}
  for line in (MAP_PATH/'VX85598_VXD.map').read_text().splitlines():
   m=re.match(r'(\d{4}):([0-9a-fA-F]{8})[ *+]*\s+(\S+)',line)
   if m:
    objnum,off,name=int(m[1]),int(m[2],16),m[3]
    if 0<objnum<=len(self.bases):self.symbols[name]=self.bases[objnum-1]+off
  assert self.symbols['VXD_DDB']==self.ddb_address
  self.ddb=ddb

 def result(self):
  return {'format':'LE','objects':len(self.objects),'internal_fixups':self.fixups,'device_id':'4333','device_name':'VX85598','ddb_ordinal':1,'external_import_modules':u32(self.b,self.h+0x74)}

def pe_check(filename='VX98TEST.EXE',subsystem=2):
 b=(DRIVER_PATH/filename).read_bytes();h=u32(b,60)
 assert b[h:h+4]==b'PE\0\0' and u16(b,h+4)==0x14c
 opt=h+24;assert u16(b,opt)==0x10b and u16(b,opt+68)==subsystem
 sections=[]
 for i in range(u16(b,h+6)):
  p=opt+u16(b,h+20)+i*40
  sections.append((u32(b,p+12),max(u32(b,p+8),u32(b,p+16)),u32(b,p+20)))
 def raw(rva):
  for at,n,off in sections:
   if at<=rva<at+n:return off+rva-at
  raise AssertionError(('bad RVA',rva))
 def string(p):return b[p:b.index(0,p)].decode('ascii')
 imported={};p=raw(u32(b,opt+104))
 while u32(b,p+12):
  module=string(raw(u32(b,p+12))).upper();thunk=u32(b,p) or u32(b,p+16)
  names=[];q=raw(thunk)
  while u32(b,q):
   t=u32(b,q);assert not t&0x80000000
   names.append(string(raw(t)+2));q+=4
  imported[module]=names;p+=20
 assert set(imported)<={'KERNEL32.DLL','USER32.DLL','GDI32.DLL'}
 assert u16(b,opt+48)<=4 # subsystem major
 return {'format':'PE32','subsystem':'Windows GUI','subsystem_version':[u16(b,opt+48),u16(b,opt+50)],'imports':imported}

def main():
 result={'ne':ne_check(),'le':LEImage().result(),'pe':pe_check(),
  'scope':'Loader structures only. Kernel execution tests are in exercise.py. No Windows boot or physical GPU test has occurred.'}
 (MAP_PATH/'STRUCTURES.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(result,indent=2))
if __name__=='__main__':main()
