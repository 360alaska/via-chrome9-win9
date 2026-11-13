#!/usr/bin/env python3
"""Structural/import checks only; this does not execute Windows or GPU code."""
import hashlib,json,pathlib,re,struct,sys
root=pathlib.Path(__file__).resolve().parent.parent
baseline=pathlib.Path(sys.argv[1]).resolve() if len(sys.argv)>1 else None
def pe(path):
 b=path.read_bytes();u16=lambda n:struct.unpack_from('<H',b,n)[0];u32=lambda n:struct.unpack_from('<I',b,n)[0]
 assert b[:2]==b'MZ',path
 nt=u32(0x3c);assert b[nt:nt+4]==b'PE\0\0',path
 assert u16(nt+4)==0x14c,path
 opt=nt+24;assert u16(opt)==0x10b,path
 sec=opt+u16(nt+20);sections=[]
 for i in range(u16(nt+6)):
  a=sec+i*40;sections.append((u32(a+12),max(u32(a+8),u32(a+16)),u32(a+20)))
 def pos(r):
  if r<u32(opt+60):return r
  for va,n,off in sections:
   if va<=r<va+n:return off+r-va
  raise ValueError((str(path),hex(r)))
 def z(r):
  o=pos(r);return b[o:b.index(0,o)].decode('ascii')
 imports={};ir=u32(opt+104)
 if ir:
  a=pos(ir)
  while any(b[a:a+20]):
   orig,_,_,name,thunk=struct.unpack_from('<5I',b,a);dll=z(name).upper();names=[];q=pos(orig or thunk)
   while u32(q):
    x=u32(q);names.append('#'+str(x&0xffff) if x&0x80000000 else z(x+2));q+=4
   imports[dll]=names;a+=20
 exports={};er=u32(opt+96)
 if er:
  a=pos(er);n=u32(a+24);functions=pos(u32(a+28));names=pos(u32(a+32));ordinals=pos(u32(a+36))
  for i in range(n):exports[z(u32(names+i*4))]=u32(functions+u16(ordinals+i*2)*4)
 result={'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest(),'format':'PE32 i386','subsystem':u16(opt+68),'subsystem_version':[u16(opt+48),u16(opt+50)],'imports':imports,'exports':sorted(exports)}
 assert result['subsystem_version'][0]<=4,(path,result['subsystem_version'])
 if 'VXDD3DABI' in exports:result['native_packet_bytes']=u32(pos(exports['VXDD3DABI'])+9*4)
 if path.resolve()==(root/'driver'/'VXGL.DLL').resolve():
  # Verify the actual linked name-table row, not just the source declaration.
  name_offset=12978;dispatch_offset=748
  table=b.index(b'glNewList\0glEndList\0')
  assert b[table+name_offset:table+name_offset+17]==b'glDepthBoundsEXT\0',path
  rows=[]
  for a in range(0,len(b)-12):
   if u32(a)==name_offset and u32(a+8)==dispatch_offset:rows.append(u32(a+4))
  m=re.search(r'^([0-9a-fA-F]{8})[ *]+_gl_dispatch_stub_748@16\s*$',(root/'verification'/'VXGL.map').read_text(),re.M)
  assert m, 'typed stdcall depth-bounds stub missing from link map'
  target=int(m.group(1),16)
  assert rows==[target], ('depth-bounds lookup missing/null/wrong target',rows,hex(target))
  assert b[pos(target-u32(opt+28))]!=0, 'depth-bounds lookup points to empty code'
  result['depth_bounds_dispatch']={'name':'glDepthBoundsEXT','offset':dispatch_offset,'linked_target':hex(target),'status':'passed'}
 return result
result={}
for name in ['VXDDRAW.DLL','VXGL.DLL','VXFEAT.EXE']:
 r=pe(root/'driver'/name)
 assert set(r['imports'])<={'KERNEL32.DLL','USER32.DLL','GDI32.DLL','ADVAPI32.DLL','OPENGL32.DLL'},name
 if baseline and name!='VXFEAT.EXE':
  old=pe(baseline/name);r['new_imports']={dll:sorted(set(names)-set(old['imports'].get(dll,[]))) for dll,names in r['imports'].items()}
  r['new_imports']={k:v for k,v in r['new_imports'].items() if v}
  allowed_new={'USER32.DLL':{'IsIconic'}} # available on Windows 95/98; avoids waiting on minimized windows
  assert all(set(v)<=allowed_new.get(k,set()) for k,v in r['new_imports'].items()),(name,r['new_imports'])
  if 'native_packet_bytes' in old:assert r['native_packet_bytes']==old['native_packet_bytes'];r['baseline_packet_bytes']=old['native_packet_bytes']
 result[name]=r
for name,magic in [('VX85598.DRV',b'NE'),('VX85598.VXD',b'LE')]:
 b=(root/'driver'/name).read_bytes();off=struct.unpack_from('<I',b,0x3c)[0];assert b[off:off+2]==magic,name
 result[name]={'bytes':len(b),'format':magic.decode(),'sha256':hashlib.sha256(b).hexdigest()}
assert 'DriverInit' in result['VXDDRAW.DLL']['exports']
assert 'DrvGetProcAddress' in result['VXGL.DLL']['exports']
(root/'verification'/'BINARY_CHECKS.json').write_text(json.dumps({'status':'passed','scope':'formats, imports, subsystem versions, exports and unchanged native packet size; no hardware execution','files':result},indent=2)+'\n')
print('Binary checks passed; native packet bytes:',result['VXDDRAW.DLL']['native_packet_bytes'])
