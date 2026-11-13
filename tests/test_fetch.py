# v10.470: indexed packets (ACCEL_D3D_INDEXED; v10.469's FETCH experiment made the production path): the HAL's indexed batch against its
# plain list, the emitter's draw by index and its inline expansion, and the validation of the indexed form.
import math,ctypes as C,struct,random,unittest,sys,os
sys.path.insert(0,os.path.dirname(__file__))
from test_core import Contract,lib,IO,READ,WRITE,TIME,U,cmd,ALLF,Packet,SUBMITN,INVALID
class Indexed(Contract):
 pass
for _m in [m for m in dir(Contract) if m.startswith('test_')]:setattr(Indexed,_m,None)
STRIPS=512
class Indexed(Indexed):
 def _trace_fb(self,p):
  tr=[];fb={}
  refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:tr.append((r,v))),READ(lambda _,r:fb.get(r,0)),WRITE(lambda _,r,v:fb.__setitem__(r,v)),TIME(lambda _:0))
  io=IO(None,*refs);last=U();rc=lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last));return rc,tr,fb
 def _trace_sink(self,p):
  """.554: the same trace with the emitter's direct sink open, as in the VxD (vx3dn_wp, vx3dn_wend, vx3dn_wform: 3 after a type-0
  header, 2 after any other): the words the emitter stores itself are taken, in order, before every word that comes through the io"""
  tr=[];fb={};N=16384;buf=(U*N)();b0=C.addressof(buf)
  wp=C.c_void_p.in_dll(lib,'vx3dn_wp');wend=C.c_void_p.in_dll(lib,'vx3dn_wend');wform=U.in_dll(lib,'vx3dn_wform');direct=[0]
  def sync():
   if wform.value:
    n=((wp.value or b0)-b0)//4;direct[0]+=n
    for k in range(n):tr.append((0x440,buf[k]))
   wp.value=b0
  def write(_,r,v):
   sync();tr.append((r,v))
   if r==0x43C:wform.value=3 if ((v>>16)&255)==0 else 2;wp.value=b0;wend.value=b0+4*(N-8)
  refs=(READ(lambda _,r:0),WRITE(write),READ(lambda _,r:fb.get(r,0)),WRITE(lambda _,r,v:fb.__setitem__(r,v)),TIME(lambda _:0))
  io=IO(None,*refs);last=U()
  try:rc=lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last));sync()
  finally:wform.value=0;wend.value=None;wp.value=None
  return rc,tr,fb,direct[0]
 @staticmethod
 def _indices(tr):
  """.555: the indices of the index bursts as the engine takes them (04/20 = 1: two 16-bit indices a data word, the low half first,
  04/24 of them a burst; an odd burst's last word must have 0 above its index)"""
  out=[];bank=None;r4={};left=0
  for r,v in tr:
   if r==0x43C:bank=(v>>16)&255;continue
   if r!=0x440:continue
   if bank==4:
    r4[v>>24]=v&0xFFFFFF
    if v>>24==0x24 and r4.get(0x20)==1:left=v&0xFFFFFF
   elif bank==0 and left:
    out.append(v&0xFFFF);left-=1
    if left:out.append(v>>16);left-=1
    else:assert v>>16==0
  assert left==0;return out
 @staticmethod
 def _data(tr):
  out=[];bank=None
  for r,v in tr:
   if r==0x43C:bank=v
   elif r==0x440 and bank==0:out.append(v)
  return out
 @staticmethod
 def _vs(p):
  """.523: the words a vertex in pre_w: 8 for an indexed packet whose run says VX3DN_IX_V8, else 10"""
  return 8 if p.pre_on==2 and p.bursts.n and p.bursts.b[0]&0x04000000 else 10
 @staticmethod
 def _w10(p):
  """.523: the packet's vertices as the 10-word form (an 8-word vertex's texture B u v a copy of its u v, as the HAL writes the 10 words)"""
  s=Indexed._vs(p);out=[]
  for v in range(p.vertex_count):
   w=[p.pre_w[v*s+k] for k in range(8)];out+=w+([w[6],w[7]] if s==8 else [p.pre_w[v*s+8],p.pre_w[v*s+9]])
  return out
 @staticmethod
 def _words(p):
  """the packet's triangles as their vertices' words, in drawing order (a list packet: its vertices; an indexed one: by its indices)"""
  if p.pre_on==2:
   ix=struct.unpack_from('<%dH'%(p.bursts.b[0]&0xFFFF),bytes(p.vertices));w=Indexed._w10(p)
   return [w[i*10+k] for i in ix for k in range(10)]
  assert all((b>>16)==0 for b in p.bursts.b[:p.bursts.n]),'a list packet'
  return [p.pre_w[k] for k in range(p.vertex_count*10)]
 def _mesh(self,rnd,n,outside=0):
  out=b''
  for k in range(n):
   x,y=rnd.uniform(1,62),rnd.uniform(1,62)
   if k<outside:x=rnd.uniform(70,90) # outside the 64x64 viewport: those triangles take the general path (clipped)
   out+=struct.pack('<4fII2f',x,y,rnd.random(),rnd.uniform(.2,2),0xA0000000|rnd.getrandbits(24),0xFF000000|rnd.getrandbits(24),rnd.uniform(-2,2),rnd.uniform(-2,2))
  return out
 def _run(self,cmds,vb,indexed):
  ix=U.in_dll(lib,'vx_d3d_indexed');ix.value=indexed;self.calls.clear()
  try:self.assertEqual(self.run_dp2(cmds,vb,stride=32,fvf=0x1C4),(0,0))
  finally:ix.value=0
  return [Packet.from_buffer_copy(bytes(p)) for p in self.calls]
 def test_hal_indexed_same_triangles(self):
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');feat.value=ALLF&~STRIPS;pre.value=1
  stat=(U*4).in_dll(lib,'vx_d3d_ix');rnd=random.Random(470)
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1);total=0
   for trial in range(40):
    n=rnd.choice((6,30,60,200,400));outside=rnd.choice((0,0,0,3))
    vb=self._mesh(rnd,n,outside);tris=[tuple(rnd.sample(range(n),3)) for _ in range(rnd.choice((1,5,40,100,300,700)))]
    cmds=self._itl(tris)
    off=self._run(cmds,vb,0);s0=stat[0];on=self._run(cmds,vb,1)
    self.assertEqual(sum((self._words(p) for p in off),[]),sum((self._words(p) for p in on),[]),('the same triangles in the same order',trial))
    for p in on:
     self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
     if p.pre_on!=2:continue
     ni=p.bursts.b[0]&0xFFFF;ix=struct.unpack_from('<%dH'%ni,bytes(p.vertices))
     self.assertEqual((p.bursts.n,(p.bursts.b[0]>>16)&0xFF),(1,2));self.assertEqual(p.bursts.b[0]&0x04000000,0x04000000,".523: one unit, the 8-word form");self.assertTrue(ni%3==0 and 3<=ni<=1920 and 1<=p.vertex_count<=512) # .559: 512 (.529: 384) by index
     # .529: the pass-through (cull none) takes a run's whole span of source vertices, at most 32 more than its triangles use (a span of a
     # random mesh now fits 384); the per-triangle loop takes only the vertices it uses
     used=set(ix);self.assertTrue(used<=set(range(p.vertex_count)),'every index inside the packet');self.assertLessEqual(p.vertex_count-len(used),32*max(1,ni//3),'unused vertices only from the pass-through spans')
     if self.context.state.rs[22]!=1:self.assertEqual(sorted(used),list(range(p.vertex_count)),'every vertex used, none twice in the packet')
    total+=stat[0]-s0
   self.assertGreater(total,5000)
  finally:pre.value=0;feat.value=ALLF&~128
 @staticmethod
 def _area(w,a,b,c):
  f=lambda i,k:struct.unpack('<f',struct.pack('<I',w[i*10+k]))[0]
  return (f(b,0)-f(a,0))*(f(c,1)-f(a,1))-(f(b,1)-f(a,1))*(f(c,0)-f(a,0))
 def _drawn(self,packets):
  """.515: the triangles the engine draws, as vertex words in order; triangles with no area left out (.529: the engine's cull removed)"""
  out=[]
  for p in packets:
   w=self._w10(p) if p.pre_on==2 else [p.pre_w[k] for k in range(p.vertex_count*10)] # .523: an 8-word packet as the 10-word form
   if p.pre_on==2:
    b=p.bursts.b[0];ix=struct.unpack_from('<%dH'%(b&0xFFFF),bytes(p.vertices))
    for t in range(0,len(ix),3):
     a=self._area(w,*ix[t:t+3])
     if a==0:continue
     out.append(tuple(w[i*10+k] for i in ix[t:t+3] for k in range(10)))
   else:
    for t in range(0,p.vertex_count,3):
     if self._area(w,t,t+1,t+2)==0:continue
     out.append(tuple(w[(t+j)*10+k] for j in range(3) for k in range(10)))
  return out
 def _grid(self,rnd,w,h):
  return b''.join(struct.pack('<4fII2f',1+60.0*(k%w)/(w-1)+rnd.uniform(-.3,.3),1+60.0*(k//w)/(h-1)+rnd.uniform(-.3,.3),.5,1.,0xA0000000|rnd.getrandbits(24),0xFF000000,0,0) for k in range(w*h))
 def test_pass_through_and_384_draw_the_same(self):
  """.515 (ACCEL_D3D_IXPASS) and .529 (384 vertices an indexed packet; .533: always, the 192 form gone): with the pass-through on or off the
  engine draws exactly the triangles the HAL's own cull kept, in the same order, for every cull mode, random and grid meshes (both
  windings), every packet valid; some packets past 192 vertices; the pass-through takes the grid meshes"""
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');xp=U.in_dll(lib,'vx_d3d_ixpass')
  px=(U*6).in_dll(lib,'vx_d3d_px');feat.value=ALLF&~STRIPS;pre.value=1;rnd=random.Random(529)
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1);passed=0;big=0
   for trial in range(60):
    if trial%2:
     n=rnd.choice((30,200,400));vb=self._mesh(rnd,n,rnd.choice((0,0,3)));tris=[tuple(rnd.sample(range(n),3)) for _ in range(rnd.choice((5,40,300,600)))]
    else:
     gw=rnd.choice((6,10,16,25));gh=rnd.choice((5,8,12,20));vb=self._grid(rnd,gw,gh);tris=[]
     for y in range(gh-1):
      for x in range(gw-1):
       a=y*gw+x;tris+=[(a,a+1,a+gw),(a+1,a+gw+1,a+gw)] if rnd.random()<.8 else [(a,a+gw,a+1),(a+1,a+gw,a+gw+1)]
     if trial%4==2:tris+=[(0,0,1)]
    mode=rnd.choice((1,2,3));self.context.state.rs[22]=mode
    cmds=self._itl(tris)
    xp.value=0;r0=self._run(cmds,vb,1);ref=self._drawn(r0)
    for v,x in ((512,1),): # .559: 512 vertices an indexed packet (.529: 384)
     xp.value=x;p0=px[0];got=self._run(cmds,vb,1)
     for p in got:
      self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
      self.assertEqual(p.bursts.b[0]&0x03000000,0,'no cull bits (.529)')
      self.assertLessEqual(p.vertex_count,v)
      if p.pre_on==2 and p.vertex_count>192:big+=1
     self.assertEqual(self._drawn(got),ref,('the same triangles drawn',trial,mode,v,x))
     passed+=px[0]-p0
   self.assertGreater(passed,2000,'the pass-through took the grids');self.assertGreater(big,10,'packets past 192 vertices')
  finally:xp.value=1;pre.value=0;feat.value=ALLF&~128;self.context.state.rs[22]=3
 def test_384_vertex_limits(self):
  """.529: the validation takes an indexed packet of up to 384 vertices and refuses 385, the old cull bits (24, 25), and more than 192
  vertices with the arrays of 192 (a layout or third coordinates); a packet that is not indexed keeps 192"""
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');feat.value=ALLF&~STRIPS;pre.value=1;rnd=random.Random(5290)
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1);gw=20;gh=19;vb=self._grid(rnd,gw,gh);tris=[]
   for y in range(gh-1):
    for x in range(gw-1):
     a=y*gw+x;tris+=[(a,a+1,a+gw),(a+1,a+gw+1,a+gw)]
   got=[q for q in self._run(self._itl(tris[:600]),vb,1) if q.pre_on==2];p=max(got,key=lambda q:q.vertex_count)
   self.assertGreater(p.vertex_count,192);self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
   def bad(f):
    q=Packet();C.memmove(C.byref(q),C.byref(p),C.sizeof(Packet));f(q);return lib.vx3dn_validate(C.byref(q),C.byref(self.info))
   self.assertNotEqual(bad(lambda q:setattr(q,'vertex_count',513)),0)
   for bit in (0x01000000,0x02000000):
    def setb(q,bit=bit):q.bursts.b[0]|=bit
    self.assertNotEqual(bad(setb),0,hex(bit))
   self.assertNotEqual(bad(lambda q:setattr(q.stages,'layout',2)),0)
   self.assertNotEqual(bad(lambda q:setattr(q,'pre_on',1)),0,'past 192 only by index')
  finally:pre.value=0;feat.value=ALLF&~128;self.context.state.rs[22]=3
 def test_emitter_by_index_and_expanded(self):
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');feat.value=ALLF&~STRIPS;pre.value=1
  base=U.in_dll(lib,'vx3dn_va_base');byts=U.in_dll(lib,'vx3dn_va_bytes');st=(U*6).in_dll(lib,'vx3dn_ix_stat');rnd=random.Random(4701)
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1);seen=0;v8seen=0
   for trial in range(10):
    n=60;vb=self._mesh(rnd,n);tris=[tuple(rnd.sample(range(n),3)) for _ in range(rnd.choice((2,16,64,300)))]
    for p in self._run(self._itl(tris),vb,1):
     if p.pre_on!=2:continue
     seen+=1;nv=p.vertex_count;ni=p.bursts.b[0]&0xFFFF;ix=list(struct.unpack_from('<%dH'%ni,bytes(p.vertices)));vs=self._vs(p);words=[p.pre_w[k] for k in range(nv*vs)];v8seen+=vs==8 # .523
     # by index: the vertices uploaded once, from a 256-byte-aligned slot, the data words the indices, in bursts of at most 48
     base.value=0x400000;byts.value=0x80000;s0=st[0];rc,tr,fb=self._trace_fb(p);byts.value=0
     self.assertEqual((rc,st[0]),(0,s0+1))
     offs=sorted(fb);self.assertEqual([fb[o] for o in offs],words);self.assertEqual(offs[0]%256,0);self.assertTrue(0x400000<=offs[0] and offs[-1]<0x480000)
     self.assertEqual(self._indices(tr),ix) # .555: two a data word
     regs=[(v>>24,v&0xFFFFFF) for r,v in tr if r==0x440 and v>>24 in (0x20,0x21,0x22,0x2C,0x24)]
     self.assertIn((0x21,offs[0]&0xFFFFFF),regs);self.assertIn((0x22,((vs*4)<<8)|(offs[0]>>24)),regs);self.assertIn((0x2C,nv-1),regs);self.assertIn((0x20,1),regs);self.assertNotIn((0x20,3),regs) # .555: 04/20 = 1
     self.assertEqual(regs[-4:],[(0x20,0),(0x21,0),(0x22,0),(0x2C,0)])
     b=[v for a,v in regs if a==0x24];self.assertEqual(sum(b),ni);self.assertTrue(all(x<=960 and x%3==0 for x in b)) # .555: up to 480 data words
     # no slot (no vertex area): the indices expanded into inline list bursts, the same words as the triangles' vertices
     s1=st[1];rc,tr,fb=self._trace_fb(p);self.assertEqual((rc,fb,st[1]),(0,{},s1+1))
     w10=self._w10(p);self.assertEqual(self._data(tr),[w10[i*10+k] for i in ix for k in range(10)]) # .523: an 8-word packet expanded to 10 words
   self.assertGreaterEqual(seen,10);self.assertGreaterEqual(v8seen,10,'.523: the 8-word form (ACCEL_D3D_V8 on, one texture unit)')
  finally:pre.value=0;feat.value=ALLF&~128;byts.value=0
 def test_culled_pass_through(self):
  """.567 (.572: always on; the comparison is against the per-triangle loop, the pass-through off): an indexed list and a clipper's fan with CULLMODE clockwise or counter-clockwise through the pass-through,
  the triangles filtered by ix_list's own area rule: with it on or off the same triangles drawn, in the same order, with the same
  vertex words; every packet valid. Grids with a share of the triangles turned over (the span path, and runs by the vertices used
  when many are culled), grids all one winding (runs with nothing kept open no packet), random meshes, a vertex outside the viewport
  (the run goes to the loop), the list command with an index base and the one of 8 bytes a triangle (3). Fans: one winding (drawn or
  left out whole), a folded one (the loop)."""
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');cp=(U*6).in_dll(lib,'vx_d3d_cp')
  fan=(U*3).in_dll(lib,'vx_d3d_fan');feat.value=ALLF&~STRIPS;pre.value=1;rnd=random.Random(567);c0=list(cp);tri_n=0;nothing=0
  xp=U.in_dll(lib,'vx_d3d_ixpass');xp0=xp.value;xp.value=1 # as the driver runs
  off=(U*4).in_dll(lib,'vx_d3d_off');off0=off[0]
  def both(cmds,vb):
   xp.value=0;a=self._run(cmds,vb,1);xp.value=1;b=self._run(cmds,vb,1)
   for q in b:
    self.assertEqual(lib.vx3dn_validate(C.byref(q),C.byref(self.info)),0)
    if q.pre_on==2: # .569: no vertex of an indexed packet is one that failed (a failing vertex converted with its span is written over)
     w=self._w10(q)
     for v in range(q.vertex_count):self.assertLessEqual(struct.unpack('<f',struct.pack('<I',w[v*10]))[0],64.5,'a vertex outside the viewport in a packet')
   return self._drawn(a),self._drawn(b),b
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1)
   for trial in range(120):
    kind=trial%4;base=0
    if kind==3:
     n=rnd.choice((30,200,400));vb=self._mesh(rnd,n,rnd.choice((0,0,3)));tris=[tuple(rnd.sample(range(n),3)) for _ in range(rnd.choice((5,40,300,600)))]
    else:
     gw=rnd.choice((6,10,16,25,40));gh=rnd.choice((5,8,12,20));vb=self._grid(rnd,gw,gh);tris=[];flip=(0.,.2,.5,.9,1.)[rnd.randrange(5)]
     for y in range(gh-1):
      band=rnd.random()<flip # whole rows turned over: runs with nothing kept, and vertices only culled triangles use
      for x in range(gw-1):
       a=y*gw+x;tris+=[(a,a+gw,a+1),(a+1,a+gw,a+gw+1)] if band or rnd.random()<.05 else [(a,a+1,a+gw),(a+1,a+gw+1,a+gw)]
     if trial%8==2:tris+=[(0,0,1)]
     if kind==2: # .568: vertices outside the viewport (they fail the check) in the spans
      var=(trial//4)%3;out=2 if var==2 else 1;vl=[vb[32*i:32*i+32] for i in range(gw*gh)]
      for i in range(gw*gh):
       if i%gw>=gw-out:vl[i]=self._mesh(rnd,1,1)
      vb=b''.join(vl);side=lambda t:[i%gw>=gw-out for i in t]
      if var==0:tris=[t for t in tris if not any(side(t))] # none used: the run goes on with its vertices checked one by one
      elif var==2:tris=[t for t in tris if all(side(t)) or not any(side(t))] # used only by triangles wholly past the right edge: left out
      # var 1: triangles across the edge end the run (the per-triangle path clips them)
    for mode in (2,3):
     if trial%5==4: # command 3: 8 bytes a triangle (the edge flags), no base
      cmds=rs((22,mode))+cmd(3,len(tris),b''.join(struct.pack('<4H',t[0],t[1],t[2],7) for t in tris))
     elif trial%5==3 and kind!=3: # a base: the vertex buffer begins 5 vertices in
      base=5;cmds=rs((22,mode))+cmd(26,len(tris),struct.pack('<H',base)+b''.join(struct.pack('<3H',*t) for t in tris))
     else:cmds=rs((22,mode))+self._itl(tris)
     v2=self._mesh(rnd,base)+vb if base else vb
     a,b,on=both(cmds,v2);self.assertEqual(a,b,('the same triangles drawn',trial,mode));tri_n+=len(b)
     if not b:nothing+=1
   print('culled pass-through:',[cp[k]-c0[k] for k in range(6)],tri_n,nothing);self.assertGreater(cp[4]-c0[4],20,'.568: runs with a failing vertex in the span filtered, vertices checked one by one');self.assertGreater(cp[4]-c0[4]-(cp[5]-c0[5]),20,'.568: such runs that went on to their end');self.assertGreater(off[0]-off0,50,'.568: triangles wholly past an edge left out');self.assertGreater(cp[5]-c0[5],3,'.568: runs ended by a triangle that uses a failing vertex');self.assertGreater(cp[0]-c0[0],150,'runs filtered');self.assertGreater(cp[1]-c0[1],5000,'triangles culled in them')
   self.assertGreater(cp[2]-c0[2],20,'runs with nothing kept');self.assertGreater(cp[3]-c0[3],20,'runs by the vertices used')
   self.assertGreater(tri_n,20000);self.assertGreater(nothing,3,'lists with every triangle culled draw nothing')
   # fans: a convex polygon about its first vertex, either way round; one folded (two windings)
   lead=cmd(26,2,struct.pack('<H',0)+struct.pack('<6H',0,1,2,0,2,1));vb=self._mesh(rnd,6);f0=fan[0];f2=fan[2];drawn=0;left=0
   def poly(k,turn,fold=0):
    out=b''
    for j in range(k+2):
     t=(j if turn else k+1-j)*5.5/(k+2);r=20.0 if not fold or j!=k else -20.0
     out+=struct.pack('<4fII2f',32+r*math.cos(t),32+r*math.sin(t),.5,1.,0xA0000000|rnd.getrandbits(24),0xFF000000,0,0)
    return out
   for trial in range(60):
    k=rnd.choice((1,2,3,6));turn=trial&1;fold=trial%10==9 and k>=3
    for mode in (2,3):
     a,b,on=both(rs((22,mode))+lead+cmd(23,k,struct.pack('<I',0)+b'\0\0'+poly(k,turn,fold)),vb);self.assertEqual(a,b,('fan',trial,mode,k,turn,fold))
     if len(b)>1:drawn+=1
     else:left+=1
   self.assertGreater(fan[0]-f0,80,'fans of one winding passed through');self.assertGreater(fan[2]-f2,5,'folded fans by the loop')
   self.assertGreater(drawn,30);self.assertGreater(left,30)
  finally:xp.value=xp0;pre.value=0;feat.value=ALLF&~128;self.context.state.rs[22]=3
 def test_strips_and_fans_pass_through(self):
  """.555: an indexed strip (20) drawn as the indexed list it is, and the clipper's fan with its vertices in the command (23) passed
  through as one span: the same triangles, in the same order, with the same vertex words as the per-triangle path draws (the HAL with
  indexed packets off: plain lists). A strip triangle with an index twice is left out by both. A fan with a vertex outside the viewport
  still takes the loop (the clipper); a fan with no area is sent and its packet passes the validation"""
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');feat.value=ALLF&~STRIPS;pre.value=1
  ixs=(U*4).in_dll(lib,'vx_d3d_ixs');fan=(U*3).in_dll(lib,'vx_d3d_fan');px=(U*6).in_dll(lib,'vx_d3d_px');rnd=random.Random(555)
  ixp=U.in_dll(lib,'vx_d3d_ixpass');ixp0=ixp.value # the driver runs with it 1 (the list's spans); 0 here keeps ix_list's own loop covered
  words=lambda ps:sum((self._words(p) for p in ps),[])
  def both(cmds,vb):
   off=self._run(cmds,vb,0);on=self._run(cmds,vb,1)
   for p in on:self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
   return words(off),words(on),on
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1);s0=ixs[0];left=ixs[3];tri_n=0;sp0=px[0]
   for trial in range(60):
    ixp.value=trial%3!=0
    n=rnd.choice((6,30,60,200,400));vb=self._mesh(rnd,n,rnd.choice((0,0,0,3)));L=rnd.choice((1,2,5,40,300,1024,1500))
    idx=[rnd.randrange(n) for _ in range(L+2)]
    for k in range(2,L+2):
     if rnd.random()<.3:idx[k]=idx[k-rnd.choice((1,2))] # a stitch: an index twice in two or three triangles
    cull=rnd.choice((1,1,2,3))
    cmds=rs((22,cull))+cmd(20,L,struct.pack('<H',0)+struct.pack('<%dH'%(L+2),*idx))
    a,b,on=both(cmds,vb);self.assertEqual(a,b,('strip',trial,L,cull));tri_n+=len(b)//30
    # and as the list of its triangles in the strip's order (an odd triangle's last two swapped), the stitches left out
    tris=[(idx[k],idx[k+2],idx[k+1]) if k&1 else (idx[k],idx[k+1],idx[k+2]) for k in range(L)];tris=[t for t in tris if len(set(t))==3]
    if tris:self.assertEqual(words(self._run(rs((22,cull))+self._itl(tris),vb,1)),b,('strip as its list',trial))
   self.assertGreater(ixs[0]-s0,40);self.assertGreater(ixs[3]-left,500);self.assertGreater(tri_n,1000);self.assertGreater(px[0]-sp0,500,'strip triangles through the spans')
   ixp.value=1
   # fans: after one triangle from the vertex buffer (it builds the template), fans of 1 to 6 triangles with their vertices inline
   f0=fan[0];tri_n=0;lead=rs((22,1))+cmd(26,1,struct.pack('<H',0)+struct.pack('<3H',0,1,2))
   for trial in range(40):
    vb=self._mesh(rnd,6);cmds=lead;nf=rnd.choice((1,3,12))
    for _ in range(nf):
     k=rnd.choice((1,1,2,3,6));cmds+=cmd(23,k,struct.pack('<I',0)+self._mesh(rnd,k+2,rnd.choice((0,0,0,0,1))))
    self.assertEqual(len(cmds)%4,0)
    a,b,on=both(cmds,vb);self.assertEqual(a,b,('fans',trial));tri_n+=len(b)//30
   self.assertGreater(fan[0]-f0,150);self.assertGreater(fan[2],5);self.assertGreater(tri_n,300)
   # culled fans (.567: test_culled_pass_through); a fan with no area (its vertices one point) goes through and validates
   f0=fan[0];vb=self._mesh(rnd,6);inline=self._mesh(rnd,5)
   lead2=cmd(26,2,struct.pack('<H',0)+struct.pack('<6H',0,1,2,0,2,1)) # both windings: the template is built whatever the cull
   for cull in (2,3):
    a,b,on=both(rs((22,cull))+lead2+cmd(23,3,struct.pack('<I',0)+b'\0\0'+inline),vb);self.assertEqual(a,b) # .567: a culled fan of one winding passes through too (test_culled_pass_through)
    self.assertLess(len(a),5*30,'the cull took a triangle')
   # a fan whose last vertex alone is outside the viewport takes the loop (the check's tail)
   for k in (1,2,3,5,6):
    v=bytearray(self._mesh(rnd,k+2));struct.pack_into('<f',v,(k+1)*32,80.0)
    a,b,on=both(lead+cmd(23,k,struct.pack('<I',0)+bytes(v)),vb);self.assertEqual(a,b,('last vertex outside',k));self.assertEqual(fan[0],f0)
   # .556: command 3 (four words a triangle): a token taken whole finds its smallest index in any of the three places
   tk=(U*2).in_dll(lib,'vx_d3d_tokrun');t0=tk[0];vb30=self._mesh(rnd,30)
   for place in (0,1,2):
    tris=[[9+k,10+k,11+k] for k in range(12)];tris[5][place]=3
    a,b,on=both(rs((22,1))+cmd(3,len(tris),b''.join(struct.pack('<4H',t[0],t[1],t[2],0) for t in tris)),vb30);self.assertEqual(a,b,('command 3',place))
   self.assertEqual(tk[0]-t0,3,'each taken whole')
   # .557: a list run whose span holds a vertex that fails is drawn by the vertices it uses (ix_ref): failing vertices no triangle
   # uses, some a triangle does use (that triangle clipped or left to the old path, the rest still drawn), off against on
   rf=(U*4).in_dll(lib,'vx_d3d_ref');r0=rf[0];e0=rf[3];tri_n=0;ixp.value=1
   for trial in range(40):
    n=rnd.choice((40,120,300));v=bytearray(self._mesh(rnd,n));bad=set(rnd.sample(range(n),rnd.choice((1,2,n//10))))
    for i in bad:struct.pack_into('<f',v,i*32,rnd.choice((80.0,float('nan'),-30.0)))
    tris=[]
    for _ in range(rnd.choice((3,30,200))):
     a0=rnd.randrange(n-8);t=tuple(a0+x for x in rnd.sample(range(8),3))
     if trial%4 or not (set(t)&bad):tris.append(t) # three trials in four: some triangles use a failing vertex
    if not tris:continue
    a,b,on=both(rs((22,1))+self._itl(tris),bytes(v));self.assertEqual(a,b,('by the vertices used',trial));tri_n+=len(b)//30
   self.assertGreater(rf[0]-r0,30);self.assertGreater(rf[3]-e0,5);self.assertGreater(tri_n,800)
   # a strip with an index at the vertex data's end is refused whole, as before (nothing drawn)
   for bad in (None,0,2,8,9):
    idx=[0,1,2,3,4,5,0,1,2,3]
    if bad is not None:idx[bad]=6
    for on in (0,1):
     ix=U.in_dll(lib,'vx_d3d_indexed');ix.value=on;self.calls.clear()
     try:rc=self.run_dp2(rs((22,1))+cmd(20,8,struct.pack('<H',0)+struct.pack('<10H',*idx)),vb,stride=32,fvf=0x1C4)
     finally:ix.value=0
     if bad is None:self.assertEqual(rc,(0,0))
     else:self.assertEqual(rc[0],INVALID,('strip index past the data',bad,on));self.assertFalse(self.calls)
   a,b,on=both(lead+cmd(23,2,struct.pack('<I',0)+inline[:32]*4),vb);self.assertEqual(fan[0],f0+1);self.assertEqual(len(b)-len(a),2*30)
  finally:pre.value=0;feat.value=ALLF&~128;ixp.value=ixp0
 def test_index_bursts_xp_form(self):
  """.555 (.554's ACCEL_D3D_IX16 mode 3, always): one head for up to 480 data words, 04/20 = 1 and two 16-bit indices a data word (the
  low half first). The engine model reads the packet's indices in order from the packet's vertex slot, each burst whole triangles,
  base, stride and bound sent before each head, 04/20-22 and 2C zero after; with texture runs and through the VxD's direct sink each
  index is drawn with the texture registers and vertex words the inline expansion (no vertex area) draws it with"""
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');feat.value=ALLF&~STRIPS;pre.value=1
  base=U.in_dll(lib,'vx3dn_va_base');byts=U.in_dll(lib,'vx3dn_va_bytes');rnd=random.Random(554)
  def model(tr):
   """the index bursts as the engine takes them: [(04/20, count, [indices], data words, the 04 registers at the head)]"""
   out=[];bank=None;r4={};left=0;cur=None
   for r,v in tr:
    if r==0x43C:
     bank=(v>>16)&255;self.assertTrue(left==0 or bank==0,'a burst cut by a register block');continue
    if r!=0x440:continue
    if bank==4:
     r4[v>>24]=v&0xFFFFFF
     if v>>24==0x24 and r4.get(0x20) in (1,3):left=v&0xFFFFFF;cur=[r4[0x20],left,[],0,dict(r4)];out.append(cur)
    elif bank==0 and left:
     cur[3]+=1
     if cur[0]==3:cur[2].append(v);left-=1
     else:
      cur[2].append(v&0xFFFF);left-=1
      if left:cur[2].append(v>>16);left-=1
      else:self.assertEqual(v>>16,0,'an odd burst: 0 above the last index')
   self.assertEqual(left,0);return out
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1);seen=0;long_seen=0;odd_seen=0
   for trial in range(12):
    n=rnd.choice((60,200,380));vb=self._mesh(rnd,n);tris=[tuple(rnd.sample(range(n),3)) for _ in range(rnd.choice((1,2,16,17,64,161,300,640)))]
    for p in self._run(self._itl(tris),vb,1):
     if p.pre_on!=2:continue
     seen+=1;nv=p.vertex_count;ni=p.bursts.b[0]&0xFFFF;ix=list(struct.unpack_from('<%dH'%ni,bytes(p.vertices)));vs=self._vs(p);ref=None
     for mode in (3,):
      base.value=0x400000;byts.value=0x80000;rc,tr,fb=self._trace_fb(p);byts.value=0
      self.assertEqual(rc,0);b=model(tr);offs=sorted(fb)
      words=[fb[o] for o in offs];self.assertEqual([o-offs[0] for o in offs],list(range(0,4*nv*vs,4)));self.assertEqual(offs[0]%256,0)
      if ref is None:ref=words
      self.assertTrue(words==ref,'the slot holds the same vertex words in every mode')
      self.assertEqual([i for x in b for i in x[2]],ix)
      cap=(48,480,48,960)[mode]
      self.assertTrue(all(x[1]<=cap and x[1]%3==0 and x[0]==(1 if mode&2 else 3) for x in b))
      self.assertEqual([x[1] for x in b],[min(cap,ni-k) for k in range(0,ni,cap)])
      self.assertEqual([x[3] for x in b],[(x[1]+1)//2 if mode&2 else x[1] for x in b])
      for x in b:self.assertEqual((x[4][0x21],x[4][0x22],x[4][0x2C],x[4][0x2D],x[4][0x2E]),(offs[0]&0xFFFFFF,((vs*4)<<8)|(offs[0]>>24),nv-1,0,0))
      self.assertTrue(max(ix)<=nv-1)
      regs=[(v>>24,v&0xFFFFFF) for r,v in tr if r==0x440 and v>>24 in (0x20,0x21,0x22,0x2C)]
      self.assertEqual(regs[-4:],[(0x20,0),(0x21,0),(0x22,0),(0x2C,0)])
      long_seen+=len(b)<(ni+47)//48;odd_seen+=bool(mode&2) and any(x[1]&1 for x in b)
   self.assertGreater(seen,8);self.assertGreater(long_seen,2);self.assertGreater(odd_seen,2)
   # texture runs (a run can begin at an odd index: the 16-bit words are then built from two reads), and the VxD's direct sink: in
   # every mode each index is drawn with the same texture registers and vertex words as in mode 0, and the sink's stream is the io's
   trn=U.in_dll(lib,'vx_d3d_texruns');trn.value=1;self.take=None;self.nresult=0
   def submit_n(_,q,k,done):
    arr=C.cast(q,C.POINTER(Packet));self.calls.extend(Packet.from_buffer_copy(arr[i]) for i in range(k));done[0]=k;return 0
   self.services.submit_n=SUBMITN(submit_n);tss=lambda h:cmd(25,1,struct.pack('<HHI',0,0,h))
   def norm(tr):
    out=[];bank=None
    for r,v in tr:
     if r==0x43C:bank=(v>>16)&255
     elif bank==4 and v>>24==0x21:v=0x21000000
     elif bank==4 and v>>24==0x22:v&=0xFFFFFF00
     out.append((r,v))
    return out
   n=60;vb=self._mesh(rnd,n);groups=[[tuple(rnd.sample(range(n),3)) for _ in range(k)] for k in (7,5,30,1,161)];hs=(5,7,5,7)
   stream=self._itl(groups[0])
   for g,h in zip(groups[1:],hs):stream+=tss(h)+self._itl(g)
   self.context.state.tss[0][0]=7;ps=self._run(stream,vb,1);self.assertEqual(len(ps),1);p=ps[0]
   self.assertEqual([p.runs.r[k].at for k in range(p.runs.n)],[21,36,126,129]);ref=None;sunk=0
   byts.value=0;rc,tr,fb=self._trace_fb(p);self.assertEqual((rc,fb),(0,{}));ref=self._draw_model(tr,fb);self.assertEqual(len(ref),3*204) # no vertex area: the triangles inline
   base.value=0x400000;byts.value=0x80000;rc,tr,fb=self._trace_fb(p);rc2,tr2,fb2,dn=self._trace_sink(p);byts.value=0
   self.assertEqual((rc,rc2),(0,0));d=self._draw_model(tr,fb)
   self.assertTrue(d==ref,'every index with the inline expansion\'s texture registers and vertex words')
   self.assertTrue(self._draw_model(tr2,fb2)==ref and norm(tr2)==norm(tr),'the direct sink sends the io\'s stream');sunk+=dn
   self.assertGreater(sunk,3*204//2,'the sink took the index words')
  finally:feat.value=ALLF;pre.value=0;byts.value=0;U.in_dll(lib,'vx_d3d_texruns').value=0;self.services.submit_n=SUBMITN(0);self.take=None;self.nresult=0
 def test_v8_layout_follows(self):
  """.523: an 8-word packet drawn from the vertex area writes the 8-word layout (04/00 A80008, 04/03 000908, 04/25 F6C000) after its
  set-up and the 32-byte stride; a second one whose set-up is skipped (vertices only) writes no layout (the engine has it); a list packet
  whose set-up would be skipped while the engine has the 8-word layout gets its whole set-up (the 10-word layout, 04/00 A8000A); with
  ACCEL_D3D_V8 off the HAL makes 10-word packets"""
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');v8=U.in_dll(lib,'vx_d3d_v8');feat.value=ALLF&~STRIPS;pre.value=1
  base=U.in_dll(lib,'vx3dn_va_base');byts=U.in_dll(lib,'vx3dn_va_bytes');vo=U.in_dll(lib,'vx3dn_vertices_only');lay=U.in_dll(lib,'vx3dn_lay8');full=U.in_dll(lib,'vx3dn_v8_full')
  rnd=random.Random(523)
  def regs4(tr):
   out=[];bank=None
   for r,v in tr:
    if r==0x43C:bank=v>>16
    elif r==0x440 and bank==4:out.append((v>>24,v&0xFFFFFF))
   return out
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1);vb=self._mesh(rnd,40);tris=[tuple(rnd.sample(range(40),3)) for _ in range(30)]
   ip=[p for p in self._run(self._itl(tris),vb,1) if p.pre_on==2][0];self.assertEqual(self._vs(ip),8)
   lp=[p for p in self._run(self._itl(tris),vb,0)][0];self.assertEqual(lp.pre_on,1)
   base.value=0x400000;byts.value=0x80000;lay.value=0xFF
   rc,tr,fb=self._trace_fb(ip);g=regs4(tr);self.assertEqual(rc,0)
   self.assertIn((0x00,0xA80008),g);self.assertIn((0x03,0x000908),g);self.assertIn((0x25,0xF6C000),g);self.assertEqual(lay.value,1)
   k8=[i for i,x in enumerate(g) if x==(0x00,0xA80008)][-1];k10=[i for i,x in enumerate(g) if x[0]==0 and x[1]!=0xA80008]
   self.assertTrue(all(i<k8 for i in k10),'the 8-word layout after the set-up\'s')
   self.assertEqual(len(fb),ip.vertex_count*8,'8 words a vertex in the area')
   vo.value=1;rc,tr,fb=self._trace_fb(ip);vo.value=0;g=regs4(tr);self.assertEqual(rc,0);self.assertFalse([x for x in g if x[0]==0],'vertices only, the engine has the layout: none written')
   f0=full.value;vo.value=1;rc,tr,fb=self._trace_fb(lp);vo.value=0;g=regs4(tr);self.assertEqual(rc,0)
   self.assertIn((0x00,0xA8000A),g);self.assertEqual(full.value,f0+1);self.assertEqual(lay.value,0)
   v8.value=0;q=[p for p in self._run(self._itl(tris),vb,1) if p.pre_on==2][0];self.assertEqual(self._vs(q),10)
  finally:v8.value=1;pre.value=0;feat.value=ALLF&~128;byts.value=0;vo.value=0
 def test_validation_refuses_bad_indexed(self):
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');feat.value=ALLF&~STRIPS;pre.value=1;rnd=random.Random(4702)
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1)
   ps=[p for p in self._run(self._itl([tuple(rnd.sample(range(30),3)) for _ in range(20)]),self._mesh(rnd,30),1) if p.pre_on==2]
   p=ps[0];self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0);ni=p.bursts.b[0]&0xFFFF
   def bad(mut):
    q=Packet.from_buffer_copy(bytes(p));mut(q);return lib.vx3dn_validate(C.byref(q),C.byref(self.info))
   def setix(q,k,v):b=bytearray(bytes(q.vertices));struct.pack_into('<H',b,2*k,v);C.memmove(C.addressof(q.vertices),bytes(b),len(b))
   BAD=lib.vx3dn_validate.restype(5).value if False else None
   self.assertNotEqual(bad(lambda q:setix(q,ni-1,q.vertex_count)),0,'an index at the vertex count')
   self.assertNotEqual(bad(lambda q:setattr(q.bursts,'b',type(q.bursts.b)(*([(2<<16)|(ni-1)]+[0]*(len(q.bursts.b)-1))))),0,'not whole triangles')
   self.assertNotEqual(bad(lambda q:setattr(q.bursts,'n',2)),0,'two runs')
   self.assertNotEqual(bad(lambda q:setattr(q,'pre_on',3)),0,'an unknown form')
   self.assertNotEqual(bad(lambda q:setattr(q.bursts,'b',type(q.bursts.b)(*([(2<<16)|1923]+[0]*(len(q.bursts.b)-1))))),0,'more indices than the packet holds')
   def badvert(q):q.pre_w[(q.vertex_count-1)*self._vs(q)+3]=0 # rhw 0 (.523: at the packet's stride)
   self.assertNotEqual(bad(badvert),0,'a bad vertex')
   def listrun(q):q.pre_on=1 # a list packet whose run says indexed
   self.assertNotEqual(bad(listrun),0,'an indexed run in a list packet')
   # .523: the 8-word mark only on its form: on a two-unit packet refused; an 8-word packet's bad vertex found at its stride
   self.assertEqual(self._vs(p),8,'the HAL marked it (one unit, ACCEL_D3D_V8 on)')
   def twounits(q):q.stages.units=2
   self.assertNotEqual(bad(twounits),0,'.523: V8 on a two-unit packet')
   def nov8(q):q.bursts.b[0]&=~0x04000000
   self.assertEqual(bad(lambda q:None),0)
   def badu(q):q.pre_w[(q.vertex_count-1)*8+6]=0x7F800000 # u infinite in the last vertex (8 words a vertex)
   self.assertNotEqual(bad(badu),0,'.523: an 8-word vertex checked at stride 8')
  finally:pre.value=0;feat.value=ALLF&~128
 def _draw_model(self,tr,fb):
  """.528: what each index (or inline vertex) is drawn with: unit 0's texture registers 02/00 (base), 02/20 (size), 02/22, 02/30 (format)
  and the sampler's 30-32 as the words leave them, and the vertex's 8 words (from the vertex area by index, or inline)"""
  out=[];tex={};smp={};t4={};bank=None;mode=None;inl=[];left=0
  for r,v in tr:
   if r==0x43C:bank=(v>>16)&255;mode=v>>24;continue
   if r!=0x440:continue
   if bank==2 and mode==0:tex[v>>24]=v&0xFFFFFF
   elif bank==2 and mode==0x20:smp[v>>24]=v&0xFFFFFF
   elif bank==4:
    t4[v>>24]=v&0xFFFFFF
    if v>>24==0x24:left=v&0xFFFFFF
   elif bank==0:
    st=(tex.get(0),tex.get(0x20),tex.get(0x22),tex.get(0x30),smp.get(0x30),smp.get(0x31),smp.get(0x32))
    if t4.get(0x20) in (1,3): # .554: 1 = two 16-bit indices a word, the low half first
     for i in ([v] if t4[0x20]==3 else [v&0xFFFF,v>>16][:min(2,left)]):
      a=(t4[0x21]|((t4[0x22]&255)<<24))+i*((t4[0x22]>>8)&0xFFFF);out.append((st,tuple(fb.get(a+4*k) for k in range(8))));left-=1
    else:
     inl.append(v)
     if len(inl)==10:out.append((st,tuple(inl[:8])));inl=[]
  return out
 def test_texture_runs(self):
  """.528 ACCEL_D3D_TEXRUNS: draws by index whose state differs only in the unit-0 texture go into one packet as texture runs (the batch kept
  open over the texture stage state, through the queue); every index drawn with the same texture registers and vertex words as with one
  packet a texture, from the vertex area and inline; the validation's run checks; a busy answer puts back the state of the batch's first
  draws, so the retry from its first command draws as made"""
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');trn=U.in_dll(lib,'vx_d3d_texruns');feat.value=ALLF&~STRIPS;pre.value=1
  base=U.in_dll(lib,'vx3dn_va_base');byts=U.in_dll(lib,'vx3dn_va_bytes');rnd=random.Random(528);self.take=None;self.nresult=0
  def submit_n(_,p,n,done):
   arr=C.cast(p,C.POINTER(Packet));k=n if self.take is None else min(n,self.take)
   self.calls.extend(Packet.from_buffer_copy(arr[i]) for i in range(k));done[0]=k;return self.nresult
  tss=lambda h:cmd(25,1,struct.pack('<HHI',0,0,h))
  try:
   self.services.submit_n=SUBMITN(submit_n)
   self._stages([(7,4,2,0,2,2,0,0)],spec=1);n=60;vb=self._mesh(rnd,n)
   A,B,Cc=[[tuple(rnd.sample(range(n),3)) for _ in range(k)] for k in (20,7,30)]
   stream=self._itl(A)+tss(5)+self._itl(B)+tss(7)+self._itl(Cc)
   got={}
   for on in (0,1):
    trn.value=on;self.context.state.tss[0][0]=7
    ps=self._run(stream,vb,1);self.assertTrue(all(p.pre_on==2 for p in ps))
    for p in ps:self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
    got[on]=ps
   self.assertEqual(len(got[0]),3);self.assertEqual(len(got[1]),1)
   p=got[1][0];self.assertEqual(p.runs.n,2);self.assertEqual([p.runs.r[k].at for k in range(2)],[60,81])
   self.assertEqual((p.runs.r[0].surface.width,p.runs.r[1].surface.width,p.tex.surface.width),(8,32,32))
   for area in (1,0):
    m={}
    for on in (0,1):
     out=[]
     for q in got[on]:
      base.value=0x400000;byts.value=0x80000 if area else 0;rc,tr,fb=self._trace_fb(q);byts.value=0
      self.assertEqual(rc,0);out+=self._draw_model(tr,fb)
     m[on]=out
    self.assertEqual(len(m[0]),3*57);self.assertEqual(m[0],m[1],'every index with the same texture registers and vertex words (area %d)'%area)
    self.assertEqual(len({s for s,_ in m[1]}),2,'two textures drawn')
   # the validation: runs only well formed, on an indexed packet, each texture inside video memory
   def bad(f):
    q=Packet.from_buffer_copy(bytes(p));f(q);return lib.vx3dn_validate(C.byref(q),C.byref(self.info))
   self.assertEqual(bad(lambda q:None),0)
   for f in (lambda q:setattr(q.runs.r[0],'at',0),lambda q:setattr(q.runs.r[0],'at',61),lambda q:setattr(q.runs.r[1],'at',60),
             lambda q:setattr(q.runs.r[1],'at',171),lambda q:setattr(q.runs,'n',16),lambda q:setattr(q.runs.r[1].surface,'offset',0xFFFFFF00),
             lambda q:setattr(q.runs.r[0].mip,'levels',0),lambda q:setattr(q.runs.r[0].surface,'pitch',4),lambda q:setattr(q.tex,'enable',0)):
    self.assertNotEqual(bad(f),0)
   lp=Packet.from_buffer_copy(bytes(got[0][0]));lp.pre_on=1;lp.runs.n=1;self.assertNotEqual(lib.vx3dn_validate(C.byref(lp),C.byref(self.info)),0)
   # busy with nothing taken: WASSTILLDRAWING at the batch's first command, stage 0's texture put back to the first draws' (7); the retry
   # from there makes the same packet
   trn.value=1;self.context.state.tss[0][0]=7;self.take=0;self.nresult=3;self.calls.clear()
   ix=U.in_dll(lib,'vx_d3d_indexed');ix.value=1
   try:rc,err=self.run_dp2(stream,vb,stride=32,fvf=0x1C4)
   finally:ix.value=0
   self.assertEqual((rc,err,self.context.lost,self.context.state.tss[0][0]),(0x8876021C,0,0,7))
   self.take=None;self.nresult=0;self.assertEqual(len(self._run(stream[err:],vb,1)),1)
   self.assertEqual(bytes(self.calls[0])[:C.sizeof(Packet)],bytes(p)[:C.sizeof(Packet)])
  finally:
   pre.value=0;feat.value=ALLF&~128;byts.value=0;trn.value=0;self.services.submit_n=SUBMITN(0);self._reset_stages();self.take=None;self.nresult=0
 def test_refusal_line(self):
  # .527: every BAD_PACKET from the validation notes its source line (vx3dn_bad_line, VX3DN_BAD_HERE) for the VxD's refusal record: good
  # packets pass and leave it; each spoiled one is refused at a line of vx_native3d_hw.c that returns VX3DN_BAD_HERE, the vertex tests'
  # line for a spoiled vertex word
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');feat.value=ALLF&~STRIPS;pre.value=1;rnd=random.Random(527)
  ln=U.in_dll(lib,'vx3dn_bad_line');src=open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','src','vx_native3d_hw.c')).read().split('\n')
  n_bad=0;lines=set()
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1)
   mesh=b''.join(struct.pack('<4fII2f',rnd.uniform(1,62),rnd.uniform(1,62),rnd.random(),rnd.uniform(.2,2),0xA0000000|k,0xFF000000|k,rnd.uniform(-2,2),rnd.uniform(-2,2)) for k in range(40))
   ix=U.in_dll(lib,'vx_d3d_indexed');ix.value=1;self.calls.clear()
   try:self.assertEqual(self.run_dp2(self._itl([tuple(rnd.sample(range(40),3)) for _ in range(30)]),mesh,stride=32,fvf=0x1C4),(0,0))
   finally:ix.value=0
   ps=[Packet.from_buffer_copy(bytes(q)) for q in self.calls if q.pre_on==2];self.assertTrue(ps)
   for p in ps:
    vs=self._vs(p);ln.value=0
    self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0);self.assertEqual(ln.value,0)
    for trial in range(40):
     q=Packet.from_buffer_copy(bytes(p));w=rnd.randrange(q.vertex_count)*vs+rnd.choice([0,1,2,3,6,7])
     q.pre_w[w]=rnd.choice([0x7F800000,0x7FC00000,0xFF800000,0xBF800000,0x3F800001,0x80000000]);ln.value=0
     if lib.vx3dn_validate(C.byref(q),C.byref(self.info))==0:continue
     n_bad+=1;self.assertTrue(0<ln.value<=len(src),ln.value);self.assertIn('VX3DN_BAD_HERE',src[ln.value-1]);lines.add(ln.value)
   self.assertGreater(n_bad,20)
  finally:pre.value=0;feat.value=ALLF&~128;self._reset_stages()
from test_core import rs
class IndexedList(Indexed):
 """v10.472: ix_list, the indexed list token's loop: the same triangles in the same order as the plain list path, for cull modes, the
 8-byte op 3, base vertices, tokens with different bases in one call, bad vertices in the middle, indices past the direct table (the hash),
 and a token with an index past the vertex data (the call fails as before)"""
 def _ph(self,on):
  ptr=C.c_void_p.in_dll(lib,'vx_d3d_phclock');pr=U.in_dll(lib,'vx_d3d_ftprof');ft=(U*32).in_dll(lib,'vx_d3d_ft')
  if on:
   t=[0]
   def tick():t[0]+=16;return t[0]
   self._ck=C.CFUNCTYPE(U)(tick);ptr.value=C.cast(self._ck,C.c_void_p).value;pr.value=1
   for k in range(32):ft[k]=0
  else:ptr.value=None;pr.value=0
  return ft
 def _both(self,cmds,vb,expect_rc=(0,0)):
  ix=U.in_dll(lib,'vx_d3d_indexed');out=[]
  for on in (0,1):
   ix.value=on;self.calls.clear();ft=self._ph(on)
   try:rc=self.run_dp2(cmds,vb,stride=32,fvf=0x1C4)
   finally:ix.value=0;used=(ft[28],ft[29],ft[30]);self._ph(0)
   self.assertEqual(rc,expect_rc);out.append(sum((self._words(Packet.from_buffer_copy(bytes(p))) for p in self.calls),[]))
   for p in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  self.assertEqual(out[0],out[1],'the same triangles in the same order')
  return used
 def _t26(self,tris,base=0):return cmd(26,len(tris),struct.pack('<H',base)+b''.join(struct.pack('<3H',*t) for t in tris))
 def test_ixlist_matches_the_list_path(self):
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');feat.value=ALLF&~STRIPS;pre.value=1;rnd=random.Random(472)
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1)
   n=300;vb=self._mesh(rnd,n);tris=[tuple(rnd.sample(range(n),3)) for _ in range(900)]
   for cull in (1,2,3):
    used=self._both(rs((22,cull))+self._t26(tris),vb);self.assertTrue(used[0]>=899 and used[1]==0,('every triangle through ix_list (the first builds the template)',cull,used))
   used=self._both(rs((22,1))+cmd(3,len(tris),b''.join(struct.pack('<4H',*t,0) for t in tris)),vb);self.assertTrue(used[0]>=899 and used[1]==0,('op 3',used))
   # base vertices, and two tokens with different bases in one call (one packet: the table keyed by the whole index)
   vb2=self._mesh(rnd,700);t1=[tuple(rnd.sample(range(300),3)) for _ in range(200)]
   used=self._both(rs((22,1))+self._t26(t1,400)+self._t26(t1,100),vb2);self.assertTrue(used[0]>=399 and used[1]==0,used)
   # bad vertices (rhw 0, x NaN) used by triangles in the middle: those dropped by the per-triangle path, ix_list stops at each
   bad=bytearray(vb);struct.pack_into('<f',bad,7*32+12,0.0);struct.pack_into('<I',bad,11*32,0x7FC00000)
   tb=tris[:100]+[(7,1,2),(3,11,4)]+tris[100:200]
   used=self._both(rs((22,1))+self._t26(tb),bytes(bad));self.assertTrue(used[1]>=2,used)
   # vertices off the viewport (the general path, clipped): stops
   vo=self._mesh(rnd,300,outside=40);used=self._both(rs((22,1))+self._t26(tris),vo);self.assertTrue(used[1]>0 and used[0]>0,used)
   # 20,000 vertices: indices past the direct table's window (the hash) and inside it
   big=self._mesh(rnd,20000);tt=[tuple(rnd.sample(range(20000),3)) for _ in range(600)]+[tuple(rnd.sample(range(64),3)) for _ in range(600)]
   used=self._both(rs((22,1))+self._t26(tt),big);self.assertTrue(used[0]>=1199 and used[1]==0,used)
   # an index past the vertex data: the token fails before drawing, as with the switch off
   self._both(rs((22,1))+self._t26(tris[:50])+self._t26([(0,1,n+5)]),vb,expect_rc=(0x80070057,lib_off(rs((22,1))+self._t26(tris[:50]))))
  finally:pre.value=0;feat.value=ALLF&~128
class PathCounts(IndexedList):
 """v10.473: vx_d3d_tk, the triangles by draw command and path: an indexed list with INDEXED on goes to the loop (the first triangle
 builds the template on the per-triangle path), culled ones counted; with INDEXED off counted as kept out; an index past the data as kept
 out for it; a plain list and a strip counted by their commands"""
 test_ixlist_matches_the_list_path=None;test_hal_indexed_same_triangles=None;test_emitter_by_index_and_expanded=None;test_validation_refuses_bad_indexed=None
 def test_path_counts(self):
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');ix=U.in_dll(lib,'vx_d3d_indexed');tk=(U*16).in_dll(lib,'vx_d3d_tk')
  feat.value=ALLF&~STRIPS;pre.value=1;rnd=random.Random(4731)
  def run(cmds,vb,on,rc=(0,0)):
   for k in range(16):tk[k]=0
   ix.value=on
   try:self.assertEqual(self.run_dp2(cmds,vb,stride=32,fvf=0x1C4),rc)
   finally:ix.value=0
   return list(tk)
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1)
   n=200;vb=self._mesh(rnd,n);tris=[tuple(rnd.sample(range(n),3)) for _ in range(300)]
   t=run(rs((22,1))+self._t26(tris),vb,1)
   self.assertEqual((t[6],t[8]+t[11],t[10],t[12],t[13]),(300,300,0,0,0),t);self.assertEqual(t[11],0,t) # v10.475: the template built before the loop: the first triangle through it too
   self.assertEqual(t[14]+t[15]+t[10],t[11],t) # v10.474: each per-triangle one has its reason (the first: the template not built yet)
   t=run(rs((22,3))+self._t26(tris),vb,1);self.assertEqual(t[6],300);self.assertTrue(0<t[9]<t[8],t) # CCW culls part of random windings
   t=run(rs((22,1))+self._t26(tris),vb,0);self.assertEqual((t[6],t[8],t[12],t[13]),(300,0,300,0),t)
   t=run(rs((22,1))+self._t26(tris[:10])+self._t26([(0,1,n+5)]),vb,1,rc=(0x80070057,len(rs((22,1))+self._t26(tris[:10]))))
   self.assertEqual((t[6],t[13]),(11,1),t)
   t=run(rs((22,1))+cmd(3,len(tris),b''.join(struct.pack('<4H',*x,0) for x in tris)),vb,1);self.assertEqual((t[0],t[8]+t[11]),(300,300),t)
   t=run(rs((22,1))+cmd(18,5,struct.pack('<H',0))+cmd(19,4,struct.pack('<H',20)),vb,1);self.assertEqual((t[1],t[2],t[8]),(5,4,0),t)
  finally:pre.value=0;feat.value=ALLF&~128
def lib_off(prefix):return len(prefix)
if __name__=='__main__':unittest.main()
