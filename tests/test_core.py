#!/usr/bin/env python3
"""Contract tests of compiled portable DP2 and native-emitter C, no GPU emulation."""
import ctypes as C
# v10.357: every feature on but v10.354's opt-in ones (STRICT 1<<20, KEYZERO 1<<21, DDBLT3D 1<<22): the driver's defaults
ALLF=0xFFFFFFFF&~(1048576|2097152|4194304|8388608) # v10.358: and GUARD (1<<23)
import json
from pathlib import Path
import struct
import unittest
U=C.c_uint32; I=C.c_int32; P=C.c_void_p
ROOT=Path(__file__).resolve().parents[1]
class Surface(C.Structure): _fields_=[(n,U) for n in ('offset','pitch','width','height','format')]
class Rect(C.Structure): _fields_=[(n,I) for n in ('left','top','right','bottom')]
class Vertex(C.Structure): _fields_=[(n,U) for n in ('x','y','z','rhw','color')]
MAXV=192 # v10.235: VX3DN_MAX_VERTICES
IXV=512 # .559 (.529: 384): VX3DN_IX_VERTICES (an indexed packet's pre.w)
class Texture(C.Structure): _fields_=[('enable',U),('surface',Surface)]+[(n,U) for n in ('filter','address_u','address_v','mode')]+[('uv',(U*2)*MAXV)]
class Shader(C.Structure): _fields_=[('count',U),('words',U*32),('konst',U*4),('kflags',U),('spec',U*MAXV),('key',U*4)]
class Raster(C.Structure): _fields_=[(n,U) for n in ('blend_src','blend_dst','alpha_func','alpha_ref')]
class Fog(C.Structure): _fields_=[('enable',U),('r',U*9)]
class Mip(C.Structure): _fields_=[('levels',U),('filter',U),('bias',U),('aniso',U),('level',U*11)]
class Ext(C.Structure): _fields_=[(n,U) for n in ('prim','dither','stencil','stencil_w1','stencil_w2')]
class Unit(C.Structure): _fields_=[('surface',Surface)]+[(n,U) for n in ('filter','address_u','address_v')]
class Stages(C.Structure): _fields_=[('units',U),('set',U*3),('unit',Unit*2),('uv1',(U*2)*MAXV),('c22',U)]+[(n,U) for n in ('ps','h90','h91','h93','h95','ntau')]+[('tau',U*4),('nkc',U),('kc_reg',U*4),('kc',(U*4)*4),('layout',U),('q',U*MAXV),('face',U*6),('umip',U*2),('ubias',U*2),('ulevel',(U*8)*2),('border',U*3),('ulayout',U*2),('uface',(U*6)*2),('s3',U),('r1',U*MAXV)]
class Bursts(C.Structure): _fields_=[('n',U),('b',U*64)] # v10.236; v10.364: 64
class TexRun(C.Structure): _fields_=[('at',U),('surface',Surface)]+[(n,U) for n in ('filter','address_u','address_v')]+[('mip',Mip)] # .528
class TexRuns(C.Structure): _fields_=[('n',U),('r',TexRun*15)]
class Packet(C.Structure): _fields_=[(n,U) for n in ('cb','abi','arm','op','generation')]+[('target',Surface),('depth',Surface),('clip',Rect)]+[(n,U) for n in ('z_enable','z_write','z_func','blend','write_mask','clear_flags','clear_color','clear_depth','vertex_count')]+[('vertices',Vertex*MAXV),('tex',Texture),('shader',Shader),('raster',Raster),('fog',Fog),('mip',Mip),('ext',Ext),('stages',Stages),('bursts',Bursts),('stipple',U*33),('pre_on',U),('pre_w',U*(IXV*10)),('runs',TexRuns)] # v10.441: prepacked vertices; .528: texture runs
class Info(C.Structure): _fields_=[(n,U) for n in ('cb','abi','ready','generation','fb_bytes','primary_bytes','width','height','pitch','bpp','submitted','triangles','clears','fault','last_status','last_result')]
class State(C.Structure): _fields_=[('rs',U*256),('tss',(U*32)*8),('viewport',Rect),('stip',U*33)]
class Block(C.Structure): _fields_=[('handle',U),('rs_mask',U*8),('tss_mask',U*8),('viewport_mask',U),('state',State)]
class Context(C.Structure): _fields_=[('generation',U),('lost',U),('recording',U),('target',Surface),('depth',Surface),('state',State),('blocks',Block*8),('target_handle',U),('depth_handle',U)]
class Input(C.Structure): _fields_=[('commands',P),('vertices',P)]+[(n,U) for n in ('command_bytes','vertex_bytes','vertex_count','vertex_stride','fvf','flags')]+[('render_states',P),('render_state_count',U)]
SUBMIT=C.CFUNCTYPE(U,P,C.POINTER(Packet)); RESOLVE=C.CFUNCTYPE(C.c_int,P,U,C.POINTER(Surface)); UNKNOWN=C.CFUNCTYPE(U,P,P,C.POINTER(P)); BIND=C.CFUNCTYPE(U,P,C.POINTER(Input)); CKEY=C.CFUNCTYPE(C.c_int,P,U,C.POINTER(U)); MIPS=C.CFUNCTYPE(U,P,U,C.POINTER(U),U); FACES=C.CFUNCTYPE(U,P,U,C.POINTER(U))
SUBMITN=C.CFUNCTYPE(U,P,C.POINTER(Packet),U,C.POINTER(U)) # v10.276
class Services(C.Structure): _fields_=[('context',P),('submit',SUBMIT),('resolve',RESOLVE),('unknown',UNKNOWN),('bind_vertices',BIND),('colorkey',CKEY),('mips',MIPS),('faces',FACES),('submit_n',SUBMITN),('volume',FACES),('target',RESOLVE),('target_done',P)] # v10.317: volume
READ=C.CFUNCTYPE(U,P,U); WRITE=C.CFUNCTYPE(None,P,U,U); TIME=C.CFUNCTYPE(U,P)
class IO(C.Structure): _fields_=[('context',P),('read',READ),('write',WRITE),('read_fb',READ),('write_fb',WRITE),('milliseconds',TIME)]
lib=C.CDLL(str(ROOT/'tests/core.so'))
lib.vx_d3d_init.argtypes=[C.POINTER(Context),C.POINTER(Surface),C.POINTER(Surface),U]
lib.vx_d3d_draw_status.argtypes=[C.POINTER(Context),C.POINTER(Services)];lib.vx_d3d_draw_status.restype=U
lib.vx_d3d_dp2.argtypes=[C.POINTER(Context),C.POINTER(Input),C.POINTER(Services),C.POINTER(U)];lib.vx_d3d_dp2.restype=U
dropped=lambda:U.in_dll(lib,'vx_d3d_dropped');drop_why=lambda:U.in_dll(lib,'vx_d3d_drop_why')
lib.vx_d3d_vertex_size.argtypes=[U];lib.vx_d3d_vertex_size.restype=U
lib.vx_d3d_texture_status.argtypes=[C.POINTER(Context)];lib.vx_d3d_texture_status.restype=U
lib.vx_d3d_draw_primitive.argtypes=[C.POINTER(Context),C.POINTER(Input),C.POINTER(Services),U,P,U]
lib.vx_d3d_draw_primitive.restype=U
lib.vx_d3d_packet_copy.argtypes=[C.POINTER(Packet),C.POINTER(Packet)];lib.vx_d3d_packet_copy.restype=None
lib.vx3dn_validate.argtypes=[C.POINTER(Packet),C.POINTER(Info)];lib.vx3dn_validate.restype=U
lib.vx3dn_execute.argtypes=[C.POINTER(Packet),C.POINTER(IO),C.POINTER(U)];lib.vx3dn_execute.restype=U
INVALID=0x80070057;UNSUPPORTED=0x80004001;UNPARSED=0x88760bb8;LOST=0x887601c2
pack=lambda *a:struct.pack('<'+'I'*len(a),*a)
def cmd(op,count,data=b'',reserved=0):return struct.pack('<BBH',op,reserved,count)+data
def rs(*pairs):return cmd(8,len(pairs),b''.join(pack(*p) for p in pairs))
def bits(f):return struct.unpack('<I',struct.pack('<f',f))[0]
def vb_color(vb,i):return struct.unpack_from('<I',vb,i*20+16)[0]
def vertices(points=None):
 points=points or [(8,8),(56,8),(8,56),(56,56),(32,16)]
 return b''.join(struct.pack('<4fI',x,y,.5,1.,0xff000000|(i+1)*0x112233) for i,(x,y) in enumerate(points))
class Contract(unittest.TestCase):
 def setUp(self):
  self.context=Context();self.target=Surface(0,256,64,64,1);self.depth=Surface(0x10000,256,64,64,3)
  lib.vx_d3d_init(C.byref(self.context),C.byref(self.target),None,7);self.context.state.rs[22]=1
  self.calls=[];self.result=0;self.unknown_next=0
  U.in_dll(lib,'vx_d3d_feat').value=ALLF&~128 # v10.214: the contract tests count packets one primitive each; test_batching turns batching on
  def submit(_,p):self.calls.append(Packet.from_buffer_copy(p.contents));return self.result
  def resolve(_,h,p):
   if h==3:p[0]=self.target;return 1
   if h==4:p[0]=self.depth;return 1
   if h==5:p[0]=Surface(0x20000,32,8,8,4);return 1
   if h==6:p[0]=Surface(0x20000,16,8,8,6);return 1
   if h==7:p[0]=Surface(0x20000,32*4,32,32,4);return 1
   if h==8:p[0]=Surface(0x20000,2048,512,4,22);return 1
   if h==11:p[0]=Surface(0x30000,32,8,8,24);return 1 # v10.208: V8U8 bump
   if h==12:p[0]=Surface(0x30000,32,8,8,25);return 1 # L6V5U5
   return 0
  self.keys={}
  def colorkey(_,h,k):
   if h not in self.keys:return 0
   k[0]=self.keys[h];return 1
  def unknown(_,p,next):
   if self.unknown_next:next[0]=p+self.unknown_next;return 0
   return UNPARSED
  self.functions=(SUBMIT(submit),RESOLVE(resolve),UNKNOWN(unknown));self.ckey=CKEY(colorkey);self.chain=[]
  def mips(_,h,o,m):
   for k,v in enumerate(self.chain[:m]):o[k]=v
   return len(self.chain[:m])
  self.mipf=MIPS(mips);self.cubes={}
  def faces(_,h,o):
   if h not in self.cubes:return 0
   if self.cubes[h] is None:return 0xFFFFFFFF
   for k,v in enumerate(self.cubes[h]):o[k]=v
   return 6
  self.facef=FACES(faces);self.services=Services(None,*self.functions,BIND(0),self.ckey,self.mipf,self.facef)
  self.info=Info(C.sizeof(Info),0x10000,1,7,0x1000000,256*64,64,64,256,32)
 def run_dp2(self,commands,verts=None,count=None,stride=20,fvf=0x44,flags=0):
  vb=vertices() if verts is None else bytes(verts);cb=C.create_string_buffer(commands);vbuf=C.create_string_buffer(vb)
  inp=Input(C.addressof(cb),C.addressof(vbuf),len(commands),len(vb),len(vb)//stride if count is None and stride else (count or 0),stride,fvf,flags,None,0)
  self.render_states=(U*256)();inp.render_states=C.addressof(self.render_states);inp.render_state_count=256
  err=U(0xdeadbeef);rc=lib.vx_d3d_dp2(C.byref(self.context),C.byref(inp),C.byref(self.services),C.byref(err))
  return rc,err.value
 def tri(self,op=18,n=1,start=0):return cmd(op,n,struct.pack('<H',start))
 def test_fog_registers_as_xp_writes_them(self):
  # v10.195: CLXP3D T17 (LINEAR 0.2..0.8, density 2.0, colour 406080) and T16 (vertex fog: plain diffuse switches to XP's T13 program)
  r=self.context.state.rs;r[28]=1;r[34]=0x406080;r[35]=3;r[36]=bits(.2);r[37]=bits(.8);r[38]=bits(2.)
  self.calls.clear();self.assertEqual(self.run_dp2(self.tri()),(0,0));p=self.calls[0]
  self.assertEqual((p.fog.enable,list(p.fog.r)),(1,[0x32,0x181202,0x101,0x3E4C,0,0x0A,0x7F,0x3F4C,0x100000]))
  r[35]=1;self.calls.clear();self.run_dp2(self.tri());self.assertEqual(self.calls[0].fog.r[0],0x34)
  r[35]=0;self.calls.clear();self.run_dp2(self.tri());p=self.calls[0];self.assertEqual(p.fog.r[0],0x30);self.assertEqual(p.shader.count,1)
  trace=[];refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:trace.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0))
  io=IO(None,*refs);last=U();self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0)
  w=[v for r2,v in trace if r2==0x440];self.assertIn(0x70000030,w);self.assertIn(0x71181202,w);self.assertIn(0x72000101,w);self.assertIn(0x00000F00,w)
  r[28]=0;self.calls.clear();self.run_dp2(self.tri());self.assertEqual(self.calls[0].fog.enable,0);self.assertEqual(self.calls[0].shader.count,0)
 def test_untextured_program_uses_xp_untextured_header(self):
  # v10.194: diffuse + specular without a texture (T14) starts with XP's untextured header, the textured one keeps its TAU
  self.context.state.rs[29]=1;self.calls.clear()
  self.assertEqual(self.run_dp2(self.tri()),(0,0));self.assertEqual(len(self.calls),1);self.assertEqual(self.calls[0].shader.count,2)
  trace=[];refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:trace.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0))
  io=IO(None,*refs);last=U();self.assertEqual(lib.vx3dn_execute(C.byref(self.calls[0]),C.byref(io),C.byref(last)),0)
  w=[v for r,v in trace if r==0x440]
  for want in (0x90058000,0x91000000,0x92000002,0x93000000):self.assertIn(want,w)
  self.assertNotIn(0x92010002,w);self.assertNotIn(0x21030000,[v for r,v in trace if r==0x43C])
  k=max(i for i,(r,v) in enumerate(trace) if r==0x43C and v==0xFE020000);self.assertEqual(trace[k+1],(0x440,0x00000001))
 def test_negative_zero_vertex_is_drawable(self):
  # v10.221 review: -0.0 (80000000) at the left/top edge passed the clamp and the VxD refused the packet (IEEE encodings compared)
  vb=vertices([(-0.0,-0.0),(56,8),(8,56)])
  self.calls.clear();self.assertEqual(self.run_dp2(self.tri(),verts=vb),(0,0));self.assertEqual(len(self.calls),1)
  self.assertEqual(self.calls[0].vertices[0].x&0x80000000,0)
  for c in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(c),C.byref(self.info)),0)
 def test_imm_vertices_are_dword_aligned(self):
  # v10.219: after INDEXEDTRIANGLELIST2 with an even count the next command is 2 mod 4; the inline vertices start on the next DWORD
  # (WinTune's clipped walls: DDERR_INVALIDPARAMS at offsets 2 mod 4)
  inline=vertices([(8,8),(56,8),(56,56),(8,56)])
  lead=cmd(26,2,struct.pack('<H',0)+struct.pack('<6H',0,1,2,0,2,3))
  self.assertEqual(len(lead)%4,2)
  stream=lead+cmd(23,2,struct.pack('<I',0)+b'\0\0'+inline)+self.tri()
  self.calls.clear();self.assertEqual(self.run_dp2(stream),(0,0));self.assertEqual(len(self.calls),5)
  self.assertEqual(self.calls[2].vertices[0].color,0xff224466)
  for c in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(c),C.byref(self.info)),0)
  stream=lead+cmd(24,1,b'\0\0'+vertices([(1,1),(9,9)]))+self.tri()
  self.calls.clear();self.assertEqual(self.run_dp2(stream),(0,0));self.assertEqual([c.ext.prim for c in self.calls],[0,0,1,0])
 def test_triangle_fan_imm_and_skipped_lines(self):
  # v10.189: TRIANGLEFAN_IMM carries edge flags and n+2 vertices inline; the next command follows them
  inline=vertices([(8,8),(56,8),(56,56),(8,56)])
  stream=cmd(23,2,struct.pack('<I',0)+inline)+self.tri()
  self.assertEqual(self.run_dp2(stream),(0,0));self.assertEqual(len(self.calls),3)
  self.assertEqual(self.calls[0].vertices[0].color,0xff224466) # fan order (1,2,0) as for TRIANGLEFAN
  # truncated inline data is rejected at the command
  self.calls.clear();self.assertEqual(self.run_dp2(cmd(23,2,struct.pack('<I',0)+inline[:-4])),(INVALID,0));self.assertFalse(self.calls)
  # v10.201: points and lines are drawn (CLXP3D T62-T64)
  d=dropped().value;self.calls.clear()
  stream=cmd(1,1,struct.pack('<HH',2,1))+cmd(15,1,struct.pack('<H',0))+cmd(2,1,struct.pack('<HH',0,1))+cmd(16,1,struct.pack('<H',0))+cmd(17,1,struct.pack('<HHH',0,0,1))+cmd(27,1,struct.pack('<HHH',0,0,1))+cmd(24,1,vertices([(1,1),(9,9)]))+self.tri()
  self.assertEqual(self.run_dp2(stream),(0,0));self.assertEqual(dropped().value,d)
  self.assertEqual([(c.ext.prim,c.vertex_count) for c in self.calls],[(0,6),(0,6)]+[(1,2)]*6+[(0,3)])
  for c in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(c),C.byref(self.info)),0)
  self.assertEqual(self.calls[2].vertices[1].color,0xff224466)
  # a point outside the viewport is dropped; a line crossing it is clipped to it
  self.calls.clear();self.run_dp2(cmd(1,1,struct.pack('<HH',1,0)),vertices([(100,8)]));self.assertFalse(self.calls)
  self.calls.clear();self.run_dp2(cmd(15,1,struct.pack('<H',0)),vertices([(8,8),(120,8)]))
  self.assertEqual(self.calls[0].ext.prim,1);self.assertLessEqual(struct.unpack('<f',struct.pack('<I',self.calls[0].vertices[1].x))[0],64.0)
 def test_clip_vertex_on_plane(self):
  # v10.302 (review): a vertex exactly on a clip plane with its neighbour outside is kept once (it was kept and interpolated again at
  # t = 0: a duplicate vertex and a sliver triangle)
  self.calls.clear();self.assertEqual(self.run_dp2(self.tri(),vertices([(0,10),(-10,50),(40,40)])),(0,0))
  self.assertEqual(sum(p.vertex_count for p in self.calls),3)
 def test_fill_modes(self):
  # v10.296: FILLMODE wireframe (2) draws the triangle's enabled edges as lines, point (1) its vertices as points, after the cull
  # (Tunnel's Ctrl+W: wireframe was refused and every draw left out, the picture stood still)
  def drawn(cmds):
   self.calls.clear();d=dropped().value;self.assertEqual(self.run_dp2(cmds),(0,0));self.assertEqual(dropped().value,d)
   return [(p.ext.prim,p.vertex_count) for p in self.calls]
  def lines(cmds):return sum(n for pr,n in drawn(cmds) if pr==1)//2
  self.assertEqual(lines(rs((8,2))+self.tri()),3)
  self.assertEqual(lines(rs((8,2))+cmd(3,1,struct.pack('<4H',0,1,2,0x300))),2) # EDGEENABLE1 and 2
  self.assertEqual(lines(rs((8,2))+cmd(3,1,struct.pack('<4H',0,1,2,0x400))),1)
  self.assertEqual(lines(rs((8,2))+cmd(3,1,struct.pack('<4H',0,1,2,0))),0)
  self.assertEqual(lines(rs((8,2))+self.tri(op=19,n=3)),9) # a strip: every triangle's three edges
  # TRIANGLEFAN_IMM: only the polygon's outline (dwEdgeFlags bit k: vertex k to k+1); 4 vertices, 2 triangles, all 4 edges set
  vtx=vertices([(8,8),(56,8),(56,56),(8,56)]);imm=cmd(23,2,struct.pack('<I',0xF))
  pad=(4-(len(imm)%4))%4;self.assertEqual(lines(rs((8,2))+imm+b'\0'*pad+vtx),4)
  imm=cmd(23,2,struct.pack('<I',0x5));self.assertEqual(lines(rs((8,2))+imm+b'\0'*pad+vtx),2) # edges 0 and 2
  self.assertEqual(drawn(rs((8,2),(22,2))+self.tri()),[]) # culled (CW cull, this triangle is clockwise on screen)
  pts=drawn(rs((8,1),(22,1))+self.tri());self.assertEqual(sum(n for pr,n in pts if pr==0),18) # three one-pixel squares
  self.assertEqual(lines(rs((8,3))+self.tri()),0);self.assertTrue(drawn(self.tri()))
  self.assertEqual(lib.vx_d3d_unsupported_why(C.byref(self.context))&2,0);self.context.state.rs[8]=0;self.assertEqual(lib.vx_d3d_unsupported_why(C.byref(self.context))&2,2);self.context.state.rs[8]=3
 def test_lines_points_stencil_dither_as_xp(self):
  # v10.201: ring 0's mode words (T62 lines 00CE01, T64 points 001E00 with 04/41 = 0 42 = 3F), dither (T49: 01/53 bit 16 off),
  # stencil (T52: 01/23..26 = w1 w2 w1 w2, 01/01 | 10)
  def trace(p):
   tr=[];refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:tr.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0))
   io=IO(None,*refs);last=U();self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0);return [v for r,v in tr if r==0x440]
  self.calls.clear();self.run_dp2(cmd(15,1,struct.pack('<H',0)),vertices([(8,8),(40,30)]));w=trace(self.calls[0])
  for x in (0x2300CE01,0x24000002,0x85800000):self.assertIn(x,w)
  self.calls.clear();self.run_dp2(cmd(1,1,struct.pack('<HH',1,0)),vertices([(8,8)]));p=self.calls[0] # v10.203: a point is two triangles
  f=lambda b:struct.unpack('<f',struct.pack('<I',b))[0]
  self.assertEqual((p.ext.prim,p.vertex_count),(0,6));self.assertEqual(sorted({(f(p.vertices[k].x),f(p.vertices[k].y)) for k in range(6)}),[(7.5,7.5),(7.5,8.5),(8.5,7.5),(8.5,8.5)])
  self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0);w=trace(p);self.assertIn(0x2300CE02,w);self.assertNotIn(0x85800000,w)
  self.calls.clear();self.run_dp2(cmd(1,1,struct.pack('<HH',8,0)),vertices([(8+k*4,8) for k in range(8)]));self.assertEqual(len(self.calls),8) # v10.204: {wCount 8, wVStart 0}, as DrawPrimitive(POINTLIST, 8) sends it
  self.calls.clear();self.run_dp2(cmd(1,2,struct.pack('<HHHH',2,3,1,0)),vertices([(8+k*4,8) for k in range(5)]));f=lambda b:struct.unpack('<f',struct.pack('<I',b))[0]
  self.assertEqual([min(f(c.vertices[k].x) for k in range(6))+.5 for c in self.calls],[20.,24.,8.]) # two records: 2 points from vertex 3, 1 from vertex 0
  r=self.context.state.rs;r[26]=1;self.calls.clear();self.run_dp2(self.tri());p=self.calls[0];p.target.format=2;w=trace(p) # v10.205: dither = 01/00 bit 0
  self.assertIn(0x53000C0F,w);self.assertIn(0x89,[(v>>16)&0xFF for v in w if v>>24==0x52]);self.assertTrue(any(v>>24==0 and v&1 for v in w));r[26]=0
  r=self.context.state.rs;r[26]=1;self.calls.clear();self.run_dp2(self.tri());w=trace(self.calls[0]);self.assertIn(0x53010C0F,w);self.assertNotIn(0x53000C0F,w) # v10.415: a 32-bit target is not dithered
  f52=[(v>>16)&0xFF for v in w if v>>24==0x52][0];self.assertEqual(f52,0x89 if self.calls[0].target.format==2 else 0x98) # v10.205: 565 dither stays 89
  p=self.calls[0];p.target.format=2
  p.ext.dither=0;w=trace(p);self.assertIn(0x89,[(v>>16)&0xFF for v in w if v>>24==0x52]);self.assertIn(0x53010C0F,w)
  o=p.target.offset;p.target.format=30;self.assertNotEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0) if o<self.info.primary_bytes else None # the primary is the desktop's format only
  p.target.offset=0x400000;self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0);w=trace(p);self.assertIn(0x52990008,w);p.target.format=2;p.target.offset=o # v10.460: an A8R8G8B8 target is engine code 99
  r[26]=0;self.context.depth=Surface(0x10000,256,64,64,13);r[7]=1;r[14]=1;r[23]=4
  r[52]=1;r[53]=1;r[54]=1;r[55]=3;r[56]=8;r[57]=1;r[58]=255;r[59]=255
  self.calls.clear();self.run_dp2(self.tri());p=self.calls[0];self.assertEqual((p.ext.stencil,p.ext.stencil_w1,p.ext.stencil_w2),(1,0x01FFFF,0x070002))
  self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0);w=trace(p)
  # .532 (review of .530, B3): the region after the plane that 01/2A names [15200, 15600) is inside video memory and off the target too
  fb=self.info.fb_bytes
  for end,ok in ((0x155FF,0),(0x15600,1),(0x15200,0)):
   self.info.fb_bytes=end;self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info))==0,bool(ok),hex(end))
  self.info.fb_bytes=fb;t=p.target;sv=(t.offset,t.height)
  for off,ok in ((0x15200,0),(0x15500,0),(0x15600,1)):
   t.offset=off;self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info))==0,bool(ok),hex(off))
  t.offset=sv[0]
  for x in (0x2301FFFF,0x24070002,0x2501FFFF,0x26070002):self.assertIn(x,w)
  self.assertTrue(any((v>>24)==1 and v&0x10 and v&0xCE0000 for v in w))
  r[55]=7;r[56]=6;self.calls.clear();self.run_dp2(self.tri());self.assertEqual(self.calls[0].ext.stencil_w2,0x050006) # T52's second draw: NOTEQUAL, INCR
  # v10.245: a stencil draw gets XP's T52 state (ACCEL_D3D_STENCIL_ZW 1): 01/17 = 80, 01/01 | 2200 and the stencil plane in 28/29/2A after
  # the Z words; 2 sets both plane bits; 0 none of it (as v10.241). Other draws get 01/17 = 0 and no plane.
  zw=C.c_uint32.in_dll(lib,'vx3dn_stencil_zw');self.assertEqual(zw.value,2);zw.value=1 # v10.247: 2 is the default
  def zr(p):
   w=trace(p);g=lambda r:[v&0xFFFFFF for v in w if v>>24==r];k=w.index(0x26000000|p.ext.stencil_w2)+1 if 0x26000000|p.ext.stencil_w2 in w else len(w);pl={}
   while k<len(w) and w[k]>>24 in (0x28,0x29,0x2A):pl[w[k]>>24]=w[k]&0xFFFFFF;k+=1 # the plane follows the stencil words (setup zeroes 20-3F first)
   return g(1)[0],g(0x17)[0],pl.get(0x28),pl.get(0x29),pl.get(0x2A)
  self.assertEqual(zr(p)[1:],(0x80,0x141,0x418404,0x152));self.assertEqual(zr(p)[0]&0x221C,0x1C) # ALWAYS REPLACE, 256-pitch 64x64 at 10000: plane 14100, pitch 64; write only
  p.ext.stencil_w2=0x050006;self.assertEqual(zr(p)[2:],(0x141,0x419404,0x152)) # NOTEQUAL INCR: read and write
  p.ext.stencil_w1=0x01FF0F;self.assertEqual(zr(p)[3],0x419404);p.ext.stencil_w1=0x01FFFF # a partial write mask reads too
  p.ext.stencil_w2=0x070000;self.assertEqual(zr(p)[3],0x418004);p.ext.stencil_w2=0x050006 # KEEP everywhere: neither
  zw.value=2;p.ext.stencil_w2=0x070002;self.assertEqual(zr(p)[3],0x419404);p.ext.stencil_w2=0x050006
  zw.value=0;e1,e17,a,b,c=zr(p);self.assertEqual((e1&0x221C,e17,a,b,c),(0x1C,0,0,0,0)) # v10.295: 28-2A are written once, 0 without the plane
  zw.value=1;p.ext.stencil=0;e1,e17,a,b,c=zr(p);self.assertEqual((e1&0x221C,e17,a,b,c),(0x0C,0,None,None,None));p.ext.stencil=1
  # XP's T52 numbers: Z at 300500, pitch 1024, 256x192: 28 = 003306, 29 = 418410 (ALWAYS REPLACE) and 419410 (NOTEQUAL INCR), 2A = 0033C7
  d=p.depth;sv=(d.offset,d.pitch,d.width,d.height);d.offset,d.pitch,d.width,d.height=0x300500,1024,256,192
  self.assertEqual(zr(p)[2:],(0x3306,0x419410,0x33C7));p.ext.stencil_w2=0x070002;self.assertEqual(zr(p)[2:],(0x3306,0x418410,0x33C7))
  p.ext.stencil_w2=0x050006;d.offset,d.pitch,d.width,d.height=sv;zw.value=2
  # v10.245: a stencil clear fills the plane over the clip rectangle, whole words inside, the edges' other bytes kept
  mem={};io=IO(None,READ(lambda _,r:0),WRITE(lambda _,r,v:None),READ(lambda _,a:mem.get(a,0x11111111)),WRITE(lambda _,a,v:mem.__setitem__(a,v)),TIME(lambda _:0))
  cl=Packet();C.memmove(C.byref(cl),C.byref(p),C.sizeof(Packet));cl.clear_flags=4|(0x5A<<8);cl.clip.left,cl.clip.top,cl.clip.right,cl.clip.bottom=3,2,10,4
  lib.vx3dn_plane_clear(C.byref(cl),C.byref(io))
  at=lambda x,y:0x14100+(y>>3)*64*8+(x>>3)*64+(y&7)*8+(x&7) # v10.246: 8x8 tiles (VXDX7 v2.69 --splane)
  self.assertEqual(sorted(mem),sorted(at(x,y) for y in (2,3) for x in (0,4,8)))
  self.assertEqual([mem[at(x,2)] for x in (0,4,8)],[0x5A111111,0x5A5A5A5A,0x11115A5A])
  # the board's numbers (pitch 256): pixel 80,72 -> +4A80, 200,150 -> +9670, 37,53 -> +312D, 255,191 -> +BFFF
  cl.clear_flags=4|(0x5A<<8);cl.depth.offset,cl.depth.pitch,cl.depth.width,cl.depth.height=0x300500,1024,256,192;so=0x330600
  for (x,y),want in [((80,72),(0x4A80,0x1111115A)),((200,150),(0x9670,0x1111115A)),((37,53),(0x312C,0x11115A11)),((255,191),(0xBFFC,0x5A111111))]:
   mem.clear();cl.clip.left,cl.clip.top,cl.clip.right,cl.clip.bottom=x,y,x+1,y+1;lib.vx3dn_plane_clear(C.byref(cl),C.byref(io))
   self.assertEqual({k-so:v for k,v in mem.items()},{want[0]:want[1]},(x,y)) # the board's bytes: +4A80, +9670, +312D, +BFFF
  cl.clear_flags=2;mem.clear();lib.vx3dn_plane_clear(C.byref(cl),C.byref(io));self.assertEqual(mem,{}) # Z only: the plane is left
  # v10.249: with the plane the Z words' low byte is not the stencil: a Z clear writes whole words, a stencil-only clear none
  dv,dm=U(),U();cl.depth.format=13;cl.clear_depth=0x80000000
  def dw(f):cl.clear_flags=f;lib.vx3dn_depth_word(C.byref(cl),C.byref(dv),C.byref(dm));return dv.value,dm.value
  self.assertEqual(dw(2),(0x80000000,0xFFFFFFFF));self.assertEqual(dw(2|4|(0x5A<<8)),(0x80000000,0xFFFFFFFF));self.assertEqual(dw(4|(0x5A<<8))[1],0)
  cl.clear_depth=0xFFFFFFFF;self.assertEqual(dw(2|4),(0xFFFFFFFF,0xFFFFFFFF));cl.clear_depth=0x80000000 # v10.296: halves equal (a 2D fill at 16 bpp)
  zw.value=0;self.assertEqual(dw(2),(0x80000000,0xFFFFFF00));self.assertEqual(dw(4|(0x5A<<8)),(0x5A,0xFF));zw.value=2
  self.context.depth=Surface(0x10000,256,64,64,3);self.calls.clear();self.run_dp2(self.tri());self.assertFalse(self.calls) # stencil needs D24S8
  r[52]=0;r[47]=8;self.calls.clear();self.run_dp2(self.tri());z=struct.unpack('<f',struct.pack('<I',self.calls[0].vertices[0].z))[0];self.assertAlmostEqual(z,.5-8/4096,5)
 def test_triangle_and_reserved_byte(self):
  self.assertEqual(self.run_dp2(cmd(18,1,b'\0\0',reserved=255)),(0,0));self.assertEqual(len(self.calls),1)
  self.assertEqual(self.calls[0].vertices[0].color,0xff112233)
  self.assertEqual(lib.vx3dn_validate(C.byref(self.calls[0]),C.byref(self.info)),0)
 def test_stages_built_as_xp_builds_them(self):
  # v10.206: two and three stages and the v2.6 ops (CLXP3D T116-T124, T138-T145, T149) rebuilt word for word, with XP's header 91/92/93
  import sys;sys.path.insert(0,str(ROOT/'tests'));from xp_multi import XP_MULTI
  pts=[(8,8,0.,0.),(56,8,1.,0.),(8,56,0.,1.)]
  vb=b''.join(struct.pack('<4fII4f',x,y,.5,1.,0xA0C08040,0xFF204060,u,v,v,u) for x,y,u,v in pts)
  bad=[]
  def trace(p):
   tr=[];refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:tr.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0))
   io=IO(None,*refs);last=U();self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0);return tr
  for n,stages,spec,want,hdr in XP_MULTI:
   for k in range(8):
    t=self.context.state.tss[k];t[0]=0;t[1]=1;t[4]=1
   for k,(op,a1,a2,aop,b1,b2,tci) in enumerate(stages):
    t=self.context.state.tss[k];t[0]=5 if tci>=0 else 0;t[1]=op;t[2]=a1;t[3]=a2;t[4]=aop;t[5]=b1;t[6]=b2;t[11]=max(tci,0)
   self.context.state.rs[29]=spec;self.calls.clear()
   self.assertEqual(self.run_dp2(self.tri(),vb,stride=40,fvf=0x2C4),(0,0),n);self.assertEqual(len(self.calls),1,n)
   p=self.calls[0];got=list(p.shader.words[:p.shader.count*4])
   if got!=want:bad.append((n,['%08X'%w for w in got],['%08X'%w for w in want]));continue
   self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0,n)
   w=[v for r,v in trace(p) if r==0x440]
   for h,x in zip((0x91,0x92,0x93),hdr):self.assertIn((h<<24)|x,w,n)
   nu=hdr[0].bit_length()
   for u in range(nu):self.assertIn(0x301001B0+u*0x01011001,w,n)
   if nu>1:self.assertEqual((p.stages.units,list(p.stages.set[:nu])),(nu,[0]+[0 if stages[u][6]==0 else 1 for u in range(1,nu)]),n)
   r28=[x for x in w if x>>24==0x28][-1];self.assertEqual(r28,0x28FFCCC0 if nu==3 else 0x28FFCC00 if nu>1 and any(p.stages.set[:nu]) else 0x28FFC000,n) # v10.209: 04/28 set 1 S,T (T24); v10.310: three units, TextureC S,T
   if nu==3: # v10.310: the vertex's third set (TextureC) is the set unit 2 reads; 12 attributes; FE/01 byte 2 names TextureC
    self.assertIn(0x00A8000C,w,n);self.assertIn(0x0411100D,w,n);fe=0x01212121|(p.stages.set[1]<<10)|(p.stages.set[2]<<18);self.assertIn(fe,w,n) # v10.313: byte 2 names unit 2's set (21 / 25)
   if n in('T142','T143','T145'):self.assertEqual(p.stages.c22,1);self.assertIn(0xBF000000,w)
   if n=='T116':self.assertEqual([p.stages.uv1[k][0] for k in range(3)],[bits(0.),bits(0.),bits(1.)]) # set 1 = (v, u)
  for k in range(8):
   t=self.context.state.tss[k];t[0]=0;t[1]=1 if k else 4;t[4]=1 if k else 2;t[11]=k
  self.context.state.rs[29]=0
  self.assertEqual(bad,[])
 def _stages(self,stages,spec=0):
  for k in range(8):
   t=self.context.state.tss[k];t[0]=0;t[1]=1;t[4]=1;t[11]=k
  for k,(h,op,a1,a2,aop,b1,b2,tci) in enumerate(stages):
   t=self.context.state.tss[k];t[0]=h;t[1]=op;t[2]=a1;t[3]=a2;t[4]=aop;t[5]=b1;t[6]=b2;t[11]=tci
  self.context.state.rs[29]=spec
 def test_unread_texture_arg1_keeps_the_alpha(self):
  # v10.335 (review #196; VXPROBE stages on the board): no texture, SELECTARG2 DIFFUSE colour, SELECTARG1 TFACTOR alpha: ARG1 is not read,
  # so TEXTURE there draws what TFACTOR there draws (the TFACTOR alpha), not a disabled stage (the diffuse alpha)
  got=[]
  for a1 in (2,3):
   self._stages([(0,3,a1,0,2,3,0,0)]);self.calls.clear();self.assertEqual(self.run_dp2(self.tri()),(0,0))
   p=self.calls[0];got.append((p.shader.count,list(p.shader.words[:p.shader.count*4])))
  self._reset_stages()
  self.assertGreater(got[1][0],0);self.assertEqual(got[0],got[1])
 def _reset_stages(self):
  for k in range(8):
   t=self.context.state.tss[k];t[0]=0;t[1]=1 if k else 4;t[2]=2;t[3]=1;t[4]=1 if k else 2;t[5]=2;t[6]=1;t[11]=k;t[24]=0
  self.context.state.rs[29]=0;self.context.state.rs[41]=0
 def _trace(self,p):
  tr=[];refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:tr.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0))
  io=IO(None,*refs);last=U();self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0);return [v for r,v in tr if r in(0x440,0x43C)]
 def test_colour_key_with_specular_and_two_stages_as_xp(self):
  # v10.208: CLXP3D T73 (key + MODULATE + specular) and T125 (key on stage 0 + MODULATE texture 1), word for word, with XP's schedule
  T,D,CUR=2,0,1
  pts=[(8,8,0.,0.),(56,8,1.,0.),(8,56,0.,1.)]
  vb=b''.join(struct.pack('<4fII4f',x,y,.5,1.,0xA0C08040,0xFF204060,u,v,u,v) for x,y,u,v in pts)
  self.keys[5]=0xFF2182C6;self.context.state.rs[41]=1
  t73=[0x80000550,0x00140924,0x0070AB05,0x00000C20,0x0140B6D0,0x50040055,0x00700005,0x00001420,0x000C0558,0x5000C055,0x00700C05,0x00090C20,0x01C00530,0x5008024D,0x00F00C05,0x00021C20,
   0x800C0550,0x00140924,0x00708100,0x00000460,0x01C00530,0x301C0053,0x00820C05,0x040D0460,0x00410550,0x50140924,0x0072C005,0x00000820,0x01C00530,0xB01C0053,0x008200B6,0x040D0820]
  t125=[0x80000550,0x00140924,0x0070AB05,0x00000C20,0x01C00530,0x301C0053,0x00F20005,0x040D0420,0x000C0558,0x5000C055,0x00700C05,0x00090C20,0x01C00530,0x5008024D,0x00F00C05,0x00021C20,
   0x800C0550,0x00140924,0x00708100,0x00000460,0x01C00530,0x301C0053,0x00820C05,0x040D0460,0x0140B6D0,0x500C0055,0x00720105,0x00000820,0x01C00530,0xB01C0053,0x0082C0B6,0x040D0820]
  key=[0x3E048485,0x3F028283,0x3F46C6C8,0x3F800000]
  for name,stages,spec,want,hdr,tau in (('T73',[(5,4,T,D,2,T,D,0)],1,t73,(0x18001,3,3,0x20),[0x301001B0,0x314011B1]),
                                     ('T125',[(5,2,T,D,2,T,D,0),(5,4,T,CUR,3,T,CUR,1)],0,t125,(0x18001,3,6,0x20),[0x301001B0,0x211111B1,0x314011B1])):
   self._stages(stages,spec);self.context.state.rs[41]=1;self.calls.clear()
   self.assertEqual(self.run_dp2(self.tri(),vb,stride=40,fvf=0x2C4),(0,0),name);p=self.calls[0]
   self.assertEqual(['%08X'%w for w in p.shader.words[:p.shader.count*4]],['%08X'%w for w in want],name)
   self.assertEqual((p.stages.ps,p.stages.h90,p.stages.h91,p.stages.h93,p.stages.h95,list(p.stages.tau[:p.stages.ntau])),(1,)+hdr+(tau,),name)
   kc={p.stages.kc_reg[k]:list(p.stages.kc[k]) for k in range(p.stages.nkc)}
   self.assertTrue(all(abs(a-b)<=1 for a,b in zip(kc[43],key)),name) # XP rounds 0xC6/255 one ulp up
   self.assertEqual(kc[0]+kc[1],[0x3F000000,0x3F800000,0x40000000,0x40800000,0x3B000000,0,0,0],name)
   self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0,name)
   w=self._trace(p);self.assertIn(0x220300AC,w);self.assertIn(0x95000020,w);self.assertIn(0x92000008|(len(tau)<<16),w)
  self._reset_stages()
 def test_bump_mapping_as_xp(self):
  # v10.208: CLXP3D T146 (BUMPENVMAP V8U8) and T147 (BUMPENVMAPLUMINANCE): XP's words, schedule and constants c11 / c35
  T,D,CUR=2,0,1
  t146=[0x02C10530,0x0008B092,0x00300000,0x00000C20,0x020C06C0,0x9008B2ED,0x00F00024,0x00000420,0x01C00530,0x501C0053,0x00720005,0x000D0820,0x01C00530,0xB01C0053,0x008040B6,0x040D0820]
  t147=[0x00C10530,0x0008B092,0x00300000,0x00000C20,0x00A3B4D0,0x500A3B45,0x004000B5,0x00001420,0x020C06C0,0x9008B2ED,0x00F00024,0x00000460,0x0140B6D0,0x502C0495,0x00700005,0x00000020,
   0x01C00530,0x501C0053,0x00720005,0x000D0820,0x01C00530,0xB01C0053,0x008040B6,0x040D0820]
  for name,op,h,want,hdr,tau in (('T146',22,11,t146,(0x18002,1,3,2),[0x201001B0,0x311001B1]),('T147',23,12,t147,(0x18001,3,3,4),[0x301001B0,0x311011B1])):
   self._stages([(h,op,T,D,2,D,T,0),(5,2,T,CUR,3,T,CUR,1)])
   t=self.context.state.tss[0];t[7]=bits(.5);t[8]=0;t[9]=0;t[10]=bits(.5);t[22]=bits(1.);t[23]=bits(.25)
   self.calls.clear();self.assertEqual(self.run_dp2(self.tri()),(0,0),name);p=self.calls[0]
   self.assertEqual(['%08X'%w for w in p.shader.words[:p.shader.count*4]],['%08X'%w for w in want],name)
   self.assertEqual((p.stages.h90,p.stages.h91,p.stages.h93,p.stages.h95,list(p.stages.tau[:2]),p.stages.units),hdr+(tau,2),name)
   kc={p.stages.kc_reg[k]:list(p.stages.kc[k]) for k in range(p.stages.nkc)}
   self.assertEqual(kc[11],[bits(.5),0,0,bits(.5)],name)
   if op==23:self.assertEqual(kc[35],[bits(1.),bits(.25),0,0],name)
   self.assertEqual(list(p.stages.set[:2]),[0,1],name)
   self._stages([(h,op,T,D,2,D,T,0),(5,2,T,CUR,3,T,CUR,0)]);t=self.context.state.tss[0];t[7]=bits(.5);t[10]=bits(.5)
   self.calls.clear();self.assertEqual(self.run_dp2(self.tri()),(0,0),name);p=self.calls[0]
   self.assertEqual(list(p.stages.set[:2]),[0,1],name+' set 0') # v10.211: the dependent read reads TextureB
   self.assertIn(0x28FFCC00,self._trace(p),name+' set 0')
   self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0,name)
   w=self._trace(p);self.assertIn(0x2203002C,w);self.assertIn(0x30380000 if op==22 else 0x30390000,w)
  self._reset_stages()
 def test_feature_switches(self):
  # v10.212: ACCEL_D3D_STAGES / NEWOPS / BUMP / PROJECTED / DITHER (bits of vx_d3d_feat); all on by default
  T,D,CUR=2,0,1;feat=U.in_dll(lib,'vx_d3d_feat');self.assertEqual(feat.value,ALLF&~128);feat.value=ALLF
  pts=[(8,8,0.,0.),(56,8,1.,0.),(8,56,0.,1.)]
  vb=b''.join(struct.pack('<4fII4f',x,y,.5,1.,0xA0C08040,0xFF204060,u,v,v,u) for x,y,u,v in pts)
  def draw():
   self.calls.clear();self.assertEqual(self.run_dp2(self.tri(),vb,stride=40,fvf=0x2C4),(0,0));return self.calls[0] if self.calls else None
  try:
   self._stages([(5,4,T,D,2,T,D,0),(5,4,T,CUR,3,T,CUR,1)]);self.assertEqual(draw().stages.units,2)
   feat.value=ALLF&~1;p=draw();self.assertEqual((p.stages.units,p.tex.enable),(0,1)) # stage 1 off: one unit
   feat.value=ALLF&~2;self._stages([(5,24,T,D,2,T,D,0)]);self.assertIsNone(draw()) # DOT3 off: left out
   feat.value=ALLF;self.assertIsNotNone(draw())
   self._stages([(5,4,T,D,2,T,D,0)]);self.context.state.rs[26]=1;self.assertEqual(draw().ext.dither,1)
   feat.value=ALLF&~32;self.assertEqual(draw().ext.dither,0)
  finally:
   feat.value=ALLF&~128;self.context.state.rs[26]=0;self._reset_stages()
 def test_submit_n(self):self._submit_n_body()
 def _submit_n_body(self):
  # v10.276: ACCEL_D3D_SUBMITN: a DrawPrimitives2 call's offscreen draw packets go to ring 0 in one call at its end (16 at most a call), in
  # order, the same packets as one at a time; a clear or a draw on the primary sends the queue first; a refusal reports the command of the
  # first packet not taken: busy before any was taken is WASSTILLDRAWING there (the retry draws nothing twice), after some the context is lost
  feat=U.in_dll(lib,'vx_d3d_feat');feat.value=ALLF;self.batches=[];self.take=None;self.nresult=0
  def submit_n(_,p,n,done):
   t=self.take.pop(0) if isinstance(self.take,list) else self.take
   arr=C.cast(p,C.POINTER(Packet));k=n if t is None else min(n,t)
   self.batches.append([Packet.from_buffer_copy(arr[i]) for i in range(n)]);self.calls.extend(self.batches[-1][:k]);done[0]=k;return self.nresult.pop(0) if isinstance(self.nresult,list) else self.nresult
  try:
   off=Surface(0x40000,256,64,64,1);lib.vx_d3d_init(C.byref(self.context),C.byref(off),None,7);self.context.state.rs[22]=1
   vb=vertices([(2+(i%10)*5+dx,2+(i//10)*5+dy) for i in range(70) for dx,dy in ((0,0),(3,0),(0,3))])
   stream=self.tri(n=2)+rs((27,1),(19,2),(20,6))+self.tri(n=2,start=6)+rs((27,0))+self.tri(n=1,start=12)
   def used(p):d=Packet();lib.vx_d3d_packet_copy(C.byref(d),C.byref(p));return bytes(d) # what ring 0 reads (the state, the runs, vertex_count entries of each array)
   self.calls.clear();self.assertEqual(self.run_dp2(stream,vb),(0,0));single=[used(p) for p in self.calls]
   self.services.submit_n=SUBMITN(submit_n);self.calls.clear()
   self.assertEqual(self.run_dp2(stream,vb),(0,0));self.assertEqual([len(b) for b in self.batches],[3])
   # .524 (ACCEL_D3D_DELTA): each queued packet after the first carries VX3DN_SAME_TAG | the state ranges 1-6 the same as the packet
   # before's in its clear_depth (a draw's unused word), which the VxD clears in its copy
   R=[(3952,3992),(5528,5680),(6448,6680),(8216,8348),(9116,9292),(10320,10452)]
   def mark(b,prev):return 0x5D000000|sum(1<<k for k,(lo,hi) in enumerate(R) if b[lo:hi]==prev[lo:hi])
   got=[bytearray(used(p)) for p in self.calls]
   self.assertEqual(struct.unpack_from('<I',got[0],104)[0],struct.unpack_from('<I',single[0],104)[0])
   for k in range(1,len(got)):self.assertEqual(struct.unpack_from('<I',got[k],104)[0],mark(got[k],got[k-1]),k);struct.pack_into('<I',got[k],104,0)
   self.assertTrue(any(struct.unpack_from('<I',single[k],104)[0]==0 for k in range(len(single))))
   self.assertEqual([bytes(b) for b in got],single,'the same packets, in order (the marks cleared, as the VxD does)')
   L=U.in_dll(lib,'vx_d3d_delta');L.value=0;self.calls.clear();self.assertEqual(self.run_dp2(stream,vb),(0,0));L.value=1
   self.assertEqual([used(p) for p in self.calls],single,'ACCEL_D3D_DELTA off: no marks')
   # 40 packets: a full queue goes at 32 (v10.315; 16 before)
   long=b''.join(self.tri(n=1,start=3*(k%23))+rs((27,(k+1)&1)) for k in range(40));self.batches=[];self.calls.clear() # v10.309: each state command changes 27 (an unchanged one ends no batch); v10.315: 40 packets, the queue holds 32
   self.assertEqual(self.run_dp2(long,vb),(0,0));self.assertEqual([len(b) for b in self.batches],[24,16]) # .560: the queue holds 24 (VX3DN_BATCH_SEND; 32 before)
   # a clear sends the queue first
   self.batches=[];self.calls.clear();clear=cmd(42,1,pack(1,0xFF00FF,0,0)+pack(0,0,8,8))
   self.assertEqual(self.run_dp2(self.tri(n=1)+rs((27,1))+self.tri(n=1,start=3)+clear+self.tri(n=1,start=6),vb),(0,0))
   self.assertEqual([p.op for p in self.calls],[1,1,2,1]);self.assertEqual([len(b) for b in self.batches],[2,1])
   # busy with nothing taken: WASSTILLDRAWING at the first queued packet's command, nothing drawn
   self.context.state.rs[27]=0;self.batches=[];self.calls.clear();self.take=0;self.nresult=3
   rc,err=self.run_dp2(rs((16,1))+self.tri(n=1)+rs((27,1))+self.tri(n=1,start=3),vb)
   self.assertEqual((rc,err,self.context.lost),(0x8876021C,len(rs((16,1))),0))
   # v10.279 (review): the state the first packet was made with is put back, so the retry draws [27 = 0, 27 = 1] as made, not [1, 1]
   self.assertEqual(self.context.state.rs[27],0,'the refused packet\'s state put back')
   s2=rs((16,1))+self.tri(n=1)+rs((27,1))+self.tri(n=1,start=3);self.take=None;self.nresult=0
   self.batches=[];self.calls.clear();self.assertEqual(self.run_dp2(s2[err:],vb),(0,0));retry=[used(p) for p in self.calls]
   self.context.state.rs[27]=0;self.calls.clear();self.assertEqual(self.run_dp2(s2,vb),(0,0))
   self.assertEqual(retry,[used(p) for p in self.calls],'the retry sends the packets the first call made')
   self.take=0;self.nresult=3
   # v10.309: an unchanged state command inside a batch: one packet; refused as busy, WASSTILLDRAWING at the batch's first command (the retry
   # replays the draws and the state command that changes nothing)
   self.context.state.rs[27]=0;self.batches=[];self.calls.clear()
   rc,err=self.run_dp2(self.tri(n=1)+rs((27,0))+self.tri(n=1,start=3),vb)
   self.assertEqual((rc,err,self.context.lost),(0x8876021C,0,0));self.assertEqual([[p.vertex_count for p in b] for b in self.batches],[[6]])
   # v10.368 (audit N2): one taken a call, each answered busy: back-pressure, the rest is sent again until every packet is taken
   self.context.state.rs[27]=0;self.batches=[];self.calls.clear();self.take=1
   a=self.tri(n=1)+rs((27,1));rc,err=self.run_dp2(a+self.tri(n=1,start=3),vb)
   self.assertEqual((rc,err,self.context.lost),(0,0,0));self.assertEqual([len(b) for b in self.batches],[2,1]);self.assertEqual(len(self.calls),2)
   # one taken, then busy with nothing taken: the context is lost (a retry would draw the first again), the error at the second's command
   self.context.state.rs[27]=0;self.batches=[];self.calls.clear();self.take=[1,0]
   rc,err=self.run_dp2(a+self.tri(n=1,start=3),vb)
   self.assertEqual((rc,err,self.context.lost),(LOST,len(a),1))
   # v10.368 (audit N8): draws on the primary's memory are queued too (ACCEL_D3D_PRIMBATCH); off, each goes alone as before
   prim=Surface(0,256,64,64,1);self.take=None;self.nresult=0
   for on,want in ((1,[3]),(0,[])):
    feat.value=ALLF if on else ALLF&~16777216;lib.vx_d3d_init(C.byref(self.context),C.byref(prim),None,7);self.context.state.rs[22]=1
    self.batches=[];self.calls.clear();self.assertEqual(self.run_dp2(stream,vb),(0,0))
    self.assertEqual([len(b) for b in self.batches],want);self.assertEqual(len(self.calls),3)
   # a VxD that refuses them in a batch (BAD_PACKET, none taken): that packet alone, the switch off from then on, nothing lost
   feat.value=ALLF;lib.vx_d3d_init(C.byref(self.context),C.byref(prim),None,7);self.context.state.rs[22]=1
   self.batches=[];self.calls.clear();self.take=0;self.nresult=1
   self.assertEqual(self.run_dp2(self.tri(n=1)+rs((27,1))+self.tri(n=1,start=3),vb),(0,0))
   self.assertEqual(feat.value&16777216,0);self.assertEqual(len(self.batches),2);self.assertEqual(len(self.calls),2);self.assertEqual(self.context.lost,0)
   # .532 (review of .530, B2): RECOVERED with none taken (the VxD reset the engine before issuing any): the queue is sent once more, not
   # dropped; a second such answer drops it as before; RECOVERED after some were taken: the rest goes on (none sent twice)
   lib.vx_d3d_init(C.byref(self.context),C.byref(off),None,7);self.context.state.rs[22]=1
   for take,res,want_calls,want_batches in (([0,None],[6,0],3,[3,3]),([0,0],[6,6],0,[3,3]),([1,None],[6,0],3,[3,2])):
    self.batches=[];self.calls.clear();self.take=take;self.nresult=res;self.context.state.rs[27]=0
    self.assertEqual(self.run_dp2(stream,vb),(0,0));self.assertEqual(len(self.calls),want_calls,(take,res));self.assertEqual([len(b) for b in self.batches],want_batches,(take,res))
   self.take=None;self.nresult=0
  finally:
   feat.value=ALLF&~128;self.services.submit_n=SUBMITN(0)
 def test_batching(self):
  # v10.214: ACCEL_D3D_BATCH: same-state primitives share a packet (MAXV vertices at most, 48 before v10.235); a state change, a clear
  # and the end of the call end a batch; a batch refused as busy reports the offset of the command that began it
  feat=U.in_dll(lib,'vx_d3d_feat');feat.value=ALLF
  try:
   pts=[(2+(i%10)*5+dx,2+(i//10)*5+dy) for i in range(70) for dx,dy in ((0,0),(3,0),(0,3))]
   vb=vertices(pts);self.calls.clear()
   self.assertEqual(self.run_dp2(self.tri(n=70),vb),(0,0))
   self.assertEqual([p.vertex_count for p in self.calls],[MAXV,210-MAXV])
   self.assertEqual([self.calls[0].vertices[k].color for k in (0,MAXV-1)]+[self.calls[1].vertices[0].color],[vb_color(vb,0),vb_color(vb,MAXV-1),vb_color(vb,MAXV)])
   # a render state between two draws that changes the packet: two packets; one that does not (LASTPIXEL 16): one
   self.calls.clear();stream=self.tri(n=2)+rs((27,1),(19,2),(20,6))+self.tri(n=2,start=6)
   self.assertEqual(self.run_dp2(stream,vb),(0,0));self.assertEqual([p.vertex_count for p in self.calls],[6,6])
   self.context.state.rs[27]=0
   # v10.215: every state command that changes a value ends a batch, even one that does not change the packet (LASTPIXEL 16 1 -> 0)
   self.assertEqual(self.context.state.rs[16],1)
   self.calls.clear();stream=self.tri(n=2)+rs((16,0))+self.tri(n=2,start=6)
   self.assertEqual(self.run_dp2(stream,vb),(0,0));self.assertEqual([p.vertex_count for p in self.calls],[6,6])
   # v10.309 (review): a state command whose values are all the state already ends nothing (render state, stage state, viewport)
   for st in (rs((16,0)),rs((16,0),(27,0)),cmd(25,1,struct.pack('<HHI',0,1,self.context.state.tss[0][1]))):
    self.calls.clear();self.assertEqual(self.run_dp2(self.tri(n=2)+st+self.tri(n=2,start=6),vb),(0,0));self.assertEqual([p.vertex_count for p in self.calls],[12])
   self.calls.clear();self.assertEqual(self.run_dp2(self.tri(n=2)+rs((16,0),(27,1))+self.tri(n=2,start=6),vb),(0,0));self.assertEqual([p.vertex_count for p in self.calls],[6,6],'one value changed')
   self.context.state.rs[27]=0;self.context.state.rs[16]=1
   # consecutive draw commands with no state between them share a batch
   self.calls.clear();self.assertEqual(self.run_dp2(self.tri(n=2)+self.tri(n=2,start=6),vb),(0,0));self.assertEqual([p.vertex_count for p in self.calls],[12])
   # busy at the batch sent before a state command: WASSTILLDRAWING at the batch's first command, the state not yet applied
   self.calls.clear();self.result=3;rc,err=self.run_dp2(self.tri(n=1)+rs((27,1))+self.tri(n=1,start=3),vb);self.result=0
   self.assertNotEqual(rc,0);self.assertEqual((err,self.context.state.rs[27]),(0,0))
   # a later command that fails while the pending batch is refused as busy: the batch's error at its first command
   self.calls.clear();self.result=3;rc,err=self.run_dp2(self.tri(n=1)+cmd(8,1,pack(300,0)),vb);self.result=0
   self.assertEqual(err,0)
   # a clear: the batch goes first
   self.calls.clear();clear=cmd(42,1,struct.pack('<IIfI',1,0xFF00FF00,1.,0)+struct.pack('<4i',0,0,64,64))
   self.assertEqual(self.run_dp2(self.tri(n=2)+clear+self.tri(n=1,start=6),vb),(0,0))
   self.assertEqual([(p.op,p.vertex_count) for p in self.calls],[(1,6),(2,0),(1,3)])
   # busy on the first packet: WASSTILLDRAWING at the batch's first command (after a 12-byte render-state command)
   self.calls.clear();self.result=3;stream=rs((16,0))+self.tri(n=2)
   rc,err=self.run_dp2(stream,vb);self.result=0
   self.assertEqual(err,12);self.assertNotEqual(rc,0)
  finally:
   feat.value=ALLF&~128;self.result=0
 def test_fast_points_and_lines(self):
  # v10.317: ACCEL_D3D_FASTPL: points and lines straight from the vertex buffer, the same packets as the general path (clip, finish,
  # output) for vertices inside, on the edges, outside, at z 0 and 1, -0, a tiny rhw, with flat shading and ZBIAS, batched or not
  import random
  rnd=random.Random(317);feat=U.in_dll(lib,'vx_d3d_feat');FAST=131072
  def key(p):
   n=p.vertex_count;b=p.bursts
   return (p.op,n,p.ext.prim,b.n,tuple(b.b[:b.n]),tuple((v.x,v.y,v.z,v.rhw,v.color) for v in p.vertices[:n]),tuple(tuple(x) for x in p.tex.uv[:n]),
    tuple(tuple(x) for x in p.stages.uv1[:n]),tuple(p.shader.spec[:n]),tuple(p.stages.q[:n]),(p.clip.left,p.clip.top,p.clip.right,p.clip.bottom))
  special=[0.,64.,-0.,63.5,0.5,.25,64.5,-1.,65.,32.] # non-finite positions are refused whole (valid_vertex)
  def coord():return rnd.choice(special) if rnd.random()<.25 else rnd.uniform(-3,67)
  try:
   for fvf,stride,tex in [(0x44,20,0),(0xC4,24,0),(0x1C4,32,1)]:
    if tex:self._stages([(5,4,2,0,2,2,0,0)])
    else:self._reset_stages()
    for it in range(60):
     pts=[]
     for k in range(12):
      z=rnd.choice([0.,1.,.5,-0.,1.5,rnd.random()]);w=rnd.choice([1.,.5,1e-30,2.]) # rhw <= 0 is refused whole (valid_vertex)
      pts.append((struct.pack('<4fI',coord(),coord(),z,w,rnd.getrandbits(32))+(struct.pack('<I',rnd.getrandbits(32)) if fvf&0x80 else b'')+(struct.pack('<2f',rnd.uniform(-2,2),rnd.choice([.5,float('inf')])) if tex else b''))[:stride])
     vb=b''.join(pts)
     stream=cmd(1,2,struct.pack('<HHHH',3,0,2,5))+cmd(15,2,struct.pack('<H',4))+cmd(16,3,struct.pack('<H',7))+cmd(2,1,struct.pack('<HH',11,3))
     rs=self.context.state.rs;rs[9]=rnd.choice([1,2,2]);rs[47]=rnd.choice([0,0,0,2]);batch=rnd.choice([0,128])
     out=[]
     for f in (0,FAST):
      feat.value=(ALLF&~128&~FAST)|batch|f;self.calls.clear()
      self.assertEqual(self.run_dp2(stream,vb,stride=stride,fvf=fvf),(0,0))
      out.append([key(p) for p in self.calls])
     self.assertEqual(out[0],out[1],(fvf,it))
     rs[9]=2;rs[47]=0
  finally:
   feat.value=ALLF&~128;self._reset_stages()
 def test_cube_map_as_xp(self):
  # v10.214: CLXP3D T150: a cube map on the only unit: faces in 02/00..05, 02/18 = 001300, FE/08 = 010201, s t r vertices, TAU 301001B0
  T,D=2,0;self.cubes[5]=[0x30000+k*0x100 for k in range(6)]
  try:
   pts=[(8,8,1.,.2,.2),(56,8,1.,.2,.2),(8,56,1.,.2,.2)]
   vb=b''.join(struct.pack('<4fII3f',x,y,.5,1.,0xFFFFFFFF,0xFF000000,s1,t1,r1) for x,y,s1,t1,r1 in pts)
   self._stages([(5,2,T,D,2,T,D,0)])
   self.calls.clear();self.assertEqual(self.run_dp2(self.tri(),vb,stride=36,fvf=0x101C4),(0,0));p=self.calls[0]
   self.assertEqual((p.stages.layout,list(p.stages.face),p.mip.levels),(1,self.cubes[5],1))
   self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
   w=self._trace(p)
   for want in [0x08010201,0x01000022,0x18001300,0x301001B0,0xA80009|0,0x28FFE000]+[(k<<24)|(self.cubes[5][k]>>8) for k in range(6)]:self.assertIn(want,w,hex(want))
   self.assertIn(0x00A80009,w);self.assertEqual(p.stages.q[0],bits(.2))
   # a cube whose faces cannot be sampled together is left out
   self.assertEqual(lib.vx_d3d_draw_status(C.byref(self.context),C.byref(self.services)),0)
   # v10.215: a cube map with a second unit, or on unit 1, is left out and ValidateDevice says so. v10.321 (ACCEL_D3D_UNIT3D): drawn, the
   # two coordinate sets in the VxD's two-unit layout (FE/08 per set, S_A T_A R_A S_B T_B), the cube's faces on its unit
   self._stages([(5,2,T,D,2,T,D,0),(7,4,T,1,3,T,1,0)])
   self.calls.clear();self.assertEqual(self.run_dp2(self.tri(),vb,stride=36,fvf=0x101C4),(0,0));p=self.calls[0]
   self.assertEqual((p.stages.units,p.stages.layout,p.stages.s3,list(p.stages.ulayout)),(2,1,1,[0,0]))
   self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
   w=self._trace(p)
   for want in [0x08010202,0x18001300,0x00A8000B,0x01020100,0x02060503,0x030A0908,0x04000D0C,0x25F6EC00,0x28FFEC00]:self.assertIn(want,w,hex(want))
   self.assertEqual(lib.vx_d3d_draw_status(C.byref(self.context),C.byref(self.services)),0)
   self._stages([(7,2,T,D,2,T,D,0),(5,4,T,1,3,T,1,0)])
   self.calls.clear();self.assertEqual(self.run_dp2(self.tri(),vb,stride=36,fvf=0x101C4),(0,0));p=self.calls[0]
   self.assertEqual((p.stages.units,p.stages.layout,p.stages.s3,list(p.stages.set)[:2],list(p.stages.ulayout),list(p.stages.uface[0])),(2,0,1,[0,0],[1,0],self.cubes[5]))
   self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
   w=self._trace(p) # both units on set 0: FE/01 unit 0 21 (2D, set 0), unit 1 22 (3D, set 0); FE/08 set A 3D
   for want in [0x01002221,0x08010202,0x18001300]+[(k<<24)|(self.cubes[5][k]>>8) for k in range(6)]:self.assertIn(want,w,hex(want))
   self.assertEqual(lib.vx_d3d_draw_status(C.byref(self.context),C.byref(self.services)),0)
   self._stages([(5,2,T,D,2,T,D,0)])
   self.cubes[5]=None;self.calls.clear();self.assertEqual(self.run_dp2(self.tri(),vb,stride=36,fvf=0x101C4),(0,0));self.assertEqual(self.calls,[])
  finally:
   self.cubes.pop(5,None);self._reset_stages()
 def test_burst(self):
  # v10.214: vx3dn_burst: one 04/23 + 04/24 = vertex count for a triangle-list packet, one idle wait
  b=U.in_dll(lib,'vx3dn_burst');feat=U.in_dll(lib,'vx_d3d_feat');feat.value=ALLF
  try:
   pts=[(2+(i%10)*5+dx,2+(i//10)*5+dy) for i in range(4) for dx,dy in ((0,0),(3,0),(0,3))]
   self.calls.clear();self.assertEqual(self.run_dp2(self.tri(n=4),vertices(pts)),(0,0));p=self.calls[0];self.assertEqual(p.vertex_count,12)
   b.value=0;w=self._trace(p);self.assertEqual(w.count(0x2300CE02),4)
   b.value=1;w=self._trace(p);self.assertEqual(w.count(0x2300CE02),1);self.assertIn(0x2400000C,w)
   # v10.235: a packet of 50 triangles (150 vertices) goes in bursts of 48, 48, 48 and 6, the vertices in order
   pts=[(2+(i%10)*5+dx,2+(i//10)*5+dy) for i in range(50) for dx,dy in ((0,0),(3,0),(0,3))]
   self.calls.clear();vb=vertices(pts);self.assertEqual(self.run_dp2(self.tri(n=50),vb),(0,0));p=self.calls[0];self.assertEqual(p.vertex_count,150)
   w=self._trace(p);self.assertEqual([w[i+1] for i in range(len(w)-1) if w[i]==0x2300CE02],[0x24000030,0x24000030,0x24000030,0x24000006])
   cols=[x for x in w if x>>24==0xFF];self.assertEqual(cols,[p.vertices[k].color for k in range(150)])
  finally:
   b.value=1;feat.value=ALLF&~128
 # v10.236: triangle strips (ACCEL_D3D_STRIPS): a grid drawn as an indexed list becomes strip runs; the same triangles as the list
 def _grid(self,cols,rows,x0=1.,y0=1.,dx=1.5,dy=6.):
  pts=[(x0+i*dx,y0+r*dy) for r in range(rows+1) for i in range(cols+1)]
  vb=b''.join(struct.pack('<4fI',x,y,.5,1.,0xA5000000|(k*0x101+1)) for k,(x,y) in enumerate(pts))
  tris=[]
  for r in range(rows):
   for i in range(cols):
    a,b,c,d=r*(cols+1)+i,(r+1)*(cols+1)+i,r*(cols+1)+i+1,(r+1)*(cols+1)+i+1
    tris+=[(a,b,c),(c,b,d)]
  return vb,tris
 def _itl(self,tris):return cmd(26,len(tris),struct.pack('<H',0)+b''.join(struct.pack('<3H',*t) for t in tris))
 def _runs(self,p):return [(b>>16,b&0xFFFF) for b in p.bursts.b[:p.bursts.n]]
 def _ptris(self,p):
  """the packet's triangles (vertex colour triples, sorted: the engine does not cull, so winding does not matter)"""
  out=[];base=0;col=lambda k:p.vertices[k].color
  for t,c in (self._runs(p) or [(0,p.vertex_count)]):
   if t==0:out+=[tuple(sorted(col(base+k+j) for j in range(3))) for k in range(0,c,3)]
   else:out+=[tuple(sorted(col(base+k+j) for j in range(3))) for k in range(c-2)]
   base+=c
  return out
 def _vtris(self,vb,tris):return [tuple(sorted(struct.unpack_from('<I',vb,k*20+16)[0] for k in t)) for t in tris]
 def test_no_empty_indexed_packet(self):
  # .540 (board, GTA3 with ACCEL_D3D_TNL on .539: VXD3D.LOG callback 13, the VxD refused an indexed packet of 0 vertices at
  # vx_native3d_hw.c line 223, after 2 of the batch's 6 had gone: the context lost, reason 26 value 801A0001): a triangle whose span
  # does not fit the open packet's room starts a new packet (ix_pass), and a bad vertex then stops ix_list before it adds anything: the
  # empty packet was sent. Every packet sent has vertices and passes the VxD's checks. The board's settings: indexed packets, ix_pass,
  # prepacked vertices, a textured stage.
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');ixp=U.in_dll(lib,'vx_d3d_ixpass');ixd=U.in_dll(lib,'vx_d3d_indexed')
  old=(feat.value,pre.value,ixp.value,ixd.value);feat.value=ALLF;pre.value=1;ixp.value=1;ixd.value=1
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1);e0=U.in_dll(lib,'vx_d3d_empty_drops').value
   vb=bytearray(b''.join(struct.pack('<4fII2f',2+(k%60),2+(k//60)*6+(k%2)*3,.5,1.,0xff000000|k,0xff000000,0,0) for k in range(600)))
   struct.pack_into('<f',vb,500*32+12,-1.0) # vertex 500: rhw negative (a bad vertex)
   for tris in ([[(0,1,2)],[(0,1,500)]],[[(0,1,2)],[(3,4,500)],[(5,6,7)]],[[(0,1,2),(0,1,500)]]):
    self.calls.clear();self.run_dp2(b''.join(self._itl(t) for t in tris),bytes(vb),stride=32,fvf=0x1C4)
    self.assertTrue(self.calls,tris)
    for p in self.calls:
     self.assertTrue(p.vertex_count>0,(tris,p.vertex_count,hex(p.bursts.b[0])))
     self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0,tris)
   self.assertTrue(U.in_dll(lib,'vx_d3d_ix').value>0) # the indexed path was taken
   self.assertEqual(U.in_dll(lib,'vx_d3d_empty_drops').value,e0) # no empty packet made (the flush's guard not needed)
  finally:feat.value,pre.value,ixp.value,ixd.value=old
 def test_offscreen_triangles_dropped_early(self):
  # .546: in the indexed loop, a triangle whose three valid vertices all lie strictly past one clip edge is dropped as culled (counted in
  # vx_d3d_off[0]) instead of stopping the loop: the packets are the ones the token makes without it. A triangle only partly off the
  # screen still stops the loop (vx_d3d_off[1]) and is drawn one by one; a bad vertex (rhw negative) is never dropped this way.
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');ixp=U.in_dll(lib,'vx_d3d_ixpass');ixd=U.in_dll(lib,'vx_d3d_indexed')
  off=(U*2).in_dll(lib,'vx_d3d_off');old=(feat.value,pre.value,ixp.value,ixd.value);feat.value=ALLF;pre.value=1;ixp.value=1;ixd.value=1
  def sig(calls): # the triangles drawn, each as its three vertices' words (the span path also converts unused vertices of a span)
   out=[]
   for c in calls:
    ni=c.bursts.b[0]&0xFFFF;vs=8 if c.bursts.b[0]&0x04000000 else 10;ix=struct.unpack('<%dH'%ni,C.string_at(C.addressof(c.vertices),ni*2))
    out+=[tuple(tuple(c.pre_w[i*vs+j] for j in range(vs)) for i in ix[t:t+3]) for t in range(0,ni,3)]
   return out
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1)
   pts=[(2+(k%8)*7,2+(k//8)*6+(k%2)*3) for k in range(24)]+[(-40,10),(-20,20),(-30,40),(70,-30),(90,-10),(80,-20),(-10,30),(20,30),(5,50),(-50,5),(-30,5),(-40,25)]
   vb=bytearray(b''.join(struct.pack('<4fII2f',x,y,.5,1.,0xff000000|k,0xff000000,0,0) for k,(x,y) in enumerate(pts)))
   struct.pack_into('<f',vb,33*32+12,-1.0) # vertex 33: rhw negative (its triangle 33 34 35 is bad, never dropped as off the screen)
   inside=[(0,1,2),(3,4,5),(8,9,10),(12,13,14)]
   for cull in (1,2,3):
    self.context.state.rs[22]=cull
    self.calls.clear();self.run_dp2(self._itl(inside),bytes(vb),stride=32,fvf=0x1C4);ref=sig(self.calls)
    a,b=off[0],off[1]
    self.calls.clear();self.run_dp2(self._itl([inside[0],(24,25,26),inside[1],(27,28,29),inside[2],inside[3]]),bytes(vb),stride=32,fvf=0x1C4)
    self.assertEqual(sig(self.calls),ref,cull);self.assertEqual(off[0]-a,2,cull);self.assertEqual(off[1]-b,0,cull)
    for p in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
    a,b=off[0],off[1];self.calls.clear();self.run_dp2(self._itl([inside[0],(30,31,32),(33,34,35)]),bytes(vb),stride=32,fvf=0x1C4)
    self.assertEqual(off[0]-a,0,cull);self.assertTrue(off[1]-b>=1,cull) # partly off the screen / a bad vertex: the loop stops
    self.assertTrue(self.calls)
    for p in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  finally:feat.value,pre.value,ixp.value,ixd.value=old;self.context.state.rs[22]=1
 def test_sparse_run_returns_to_pass_through(self):
  # .548: a pass-through run refused as sparse goes through the indexed loop, then the pass-through takes the rest of the token again
  # (vx_d3d_px_back); the same triangles are drawn as when the token is in order, every packet passes the VxD's checks
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');ixp=U.in_dll(lib,'vx_d3d_ixpass');ixd=U.in_dll(lib,'vx_d3d_indexed')
  back=U.in_dll(lib,'vx_d3d_px_back');old=(feat.value,pre.value,ixp.value,ixd.value);feat.value=ALLF;pre.value=1;ixp.value=1;ixd.value=1
  def tris(calls):
   out=[]
   for c in calls:
    ni=c.bursts.b[0]&0xFFFF;vs=8 if c.bursts.b[0]&0x04000000 else 10;ix=struct.unpack('<%dH'%ni,C.string_at(C.addressof(c.vertices),ni*2))
    out+=[tuple(tuple(c.pre_w[i*vs+j] for j in range(vs)) for i in ix[t:t+3]) for t in range(0,ni,3)]
   return out
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1);self.context.state.rs[22]=1
   NT=400;vb=b''.join(struct.pack('<4fII2f',2+(k%40)*1.5,2+((k//3)%60),.5,1.,0xff000000|k,0xff000000,0,0) for k in range(NT*3))
   order=[]
   for k in range(NT):
    b,i=k//100,k%100
    if b&1:continue
    for t in (b*100+i,b*100+i+100):
     if t<NT:order.append(t)
   self.assertEqual(sorted(order),list(range(NT)))
   self.calls.clear();self.run_dp2(self._itl([(3*t,3*t+1,3*t+2) for t in range(NT)]),vb,stride=32,fvf=0x1C4);ref=sorted(tris(self.calls))
   b0=back.value;self.calls.clear();self.run_dp2(self._itl([(3*t,3*t+1,3*t+2) for t in order]),vb,stride=32,fvf=0x1C4)
   self.assertEqual(sorted(tris(self.calls)),ref);self.assertTrue(back.value>b0)
   for p in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  finally:feat.value,pre.value,ixp.value,ixd.value=old
 def test_vertex_check_cache_per_call(self):
  # v10.237 (third-party review B2): a vertex index checked in an earlier call with more vertices is checked again against this call's
  # count (the cache's bits above the new count were left set)
  vb=vertices([(2+(k%8)*7,2+(k//8)*6+(k%2)*3) for k in range(72)]);t=self._itl([(64,65,66)])
  self.calls.clear();self.assertEqual(self.run_dp2(t,vb,count=3)[0],INVALID);self.assertEqual(self.calls,[])
  self.calls.clear();self.assertEqual(self.run_dp2(t,vb,count=72),(0,0));self.assertEqual(len(self.calls),1)
  self.calls.clear();self.assertEqual(self.run_dp2(t,vb,count=3)[0],INVALID);self.assertEqual(self.calls,[])
  self.calls.clear();self.assertEqual(self.run_dp2(self._itl([(0,1,2)]),vb,count=3),(0,0));self.assertEqual(len(self.calls),1)
 def test_strips(self):
  feat=U.in_dll(lib,'vx_d3d_feat');hw=U.in_dll(lib,'vx3dn_strip_hw');feat.value=ALLF
  try:
   vb,tris=self._grid(10,1)
   self.calls.clear();self.assertEqual(self.run_dp2(self._itl(tris),vb),(0,0));p=self.calls[0]
   self.assertEqual((len(self.calls),p.vertex_count,self._runs(p)),(1,22,[(1,22)]))
   self.assertEqual(sorted(self._ptris(p)),sorted(self._vtris(vb,tris)))
   self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
   w=self._trace(p);i=w.index(0x20386300);self.assertEqual(w[i+1:i+3],[0x2300CE02,0x24000016])
   cols=[x for x in w if x>>24==0xA5];self.assertEqual(cols,[p.vertices[k].color for k in range(22)])
   # ACCEL_D3D_STRIP_HW 0: the VxD sends the strip's 20 triangles as list bursts of 16 and 4 triangles, 04/20 left 0
   hw.value=0;w=self._trace(p);hw.value=1
   self.assertNotIn(0x20386300,w);self.assertEqual([w[i+1] for i in range(len(w)-1) if w[i]==0x2300CE02],[0x24000030,0x2400000C])
   cols=[x for x in w if x>>24==0xA5];self.assertEqual(cols,[p.vertices[k+j].color for k in range(20) for j in range(3)])
   # ACCEL_D3D_STRIPS 0: the list as before (60 vertices, no runs)
   feat.value=ALLF&~512;self.calls.clear();self.assertEqual(self.run_dp2(self._itl(tris),vb),(0,0));p=self.calls[0];feat.value=ALLF
   self.assertEqual((p.vertex_count,self._runs(p)),(60,[]));self.assertNotIn(0x20386300,self._trace(p))
   # two rows: two strips; a lone triangle before and after: list runs; 04/20 goes back to 0 for the list after a strip
   vb,tris=self._grid(10,2);lone=[(0,32,22)]
   self.calls.clear();self.assertEqual(self.run_dp2(self._itl(lone+tris+lone),vb),(0,0));p=self.calls[0]
   self.assertEqual(self._runs(p),[(0,3),(1,22),(1,22),(0,3)])
   self.assertEqual(sorted(self._ptris(p)),sorted(self._vtris(vb,lone+tris+lone)))
   self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
   w=self._trace(p);heads=[w[i-1] for i in range(1,len(w)) if w[i]==0x2300CE02]
   self.assertEqual(heads,[0x00040000,0x20386300,0x00040000,0x20000000])
   self.assertEqual([w[i+1] for i in range(len(w)-1) if w[i]==0x2300CE02],[0x24000003,0x24000016,0x24000016,0x24000003])
   # a strip longer than a burst: 48 vertices, then 36 from vertex 46 (the last two repeated)
   vb,tris=self._grid(41,1,dx=1.4)
   self.calls.clear();self.assertEqual(self.run_dp2(self._itl(tris),vb),(0,0));p=self.calls[0]
   self.assertEqual(self._runs(p),[(1,84)]);w=self._trace(p)
   self.assertEqual([w[i+1] for i in range(len(w)-1) if w[i]==0x2300CE02],[0x24000030,0x24000026])
   cols=[x for x in w if x>>24==0xA5];self.assertEqual(cols,[p.vertices[k].color for k in list(range(48))+list(range(46,84))])
   # a batch full at 192 vertices (a strip filling it): the rest in the next packet, the same triangles
   vb,tris=self._grid(41,3,dx=1.4,dy=4.)
   self.calls.clear();self.assertEqual(self.run_dp2(self._itl(tris),vb),(0,0))
   self.assertEqual([p.vertex_count for p in self.calls],[192,62]);self.assertEqual(sorted(sum((self._ptris(p) for p in self.calls),[])),sorted(self._vtris(vb,tris)))
   # DrawPrimitive and DP2 strips (op 19): one strip run in the runtime's vertex order
   vb,tris=self._grid(10,1);seq=[k for i in range(11) for k in (i,11+i)]
   vb2=b''.join(vb[k*20:k*20+20] for k in seq)
   self.calls.clear();self.assertEqual(self.run_dp2(cmd(19,20,struct.pack('<H',0)),vb2),(0,0));p=self.calls[0]
   self.assertEqual(self._runs(p),[(1,22)]);self.assertEqual([p.vertices[k].color for k in range(22)],[struct.unpack_from('<I',vb2,k*20+16)[0] for k in range(22)])
  finally:
   feat.value=ALLF&~128;hw.value=1
 def test_prepacked_same_words(self):
  # v10.441: ACCEL_D3D_PREPACK: the HAL's packets with the vertices prepacked (XP's 10-word form) give the emitter's words exactly as the
  # six arrays do: one and two texture units with specular, lists and strips (shared edges), triangles the clip cuts (general path),
  # points (squares); untextured diffuse-only draws (no XP program) stay on the arrays
  import random
  feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');feat.value=ALLF
  rnd=random.Random(441)
  def mesh(n,stride,two):
   out=b''
   for k in range(n):
    x,y=rnd.uniform(-30,90),rnd.uniform(-30,90)
    if rnd.random()<.7:x,y=rnd.uniform(1,62),rnd.uniform(1,62)
    out+=struct.pack('<4fII2f',x,y,rnd.random(),rnd.uniform(.2,2),0xA0000000|rnd.getrandbits(24),0xFF000000|rnd.getrandbits(24),rnd.uniform(-2,2),rnd.uniform(-2,2))
    if two:out+=struct.pack('<2f',rnd.uniform(-1,1),rnd.uniform(-1,1))
   return out
  def grid_idx(cols,rows):
   t=[]
   for r in range(rows):
    for i in range(cols):
     a,b,c,d=r*(cols+1)+i,(r+1)*(cols+1)+i,r*(cols+1)+i+1,(r+1)*(cols+1)+i+1;t+=[(a,b,c),(c,b,d)]
   return t
  def run(on,stages,fvf,stride,vb,cmds):
   pre.value=on;self._stages(stages,spec=1);self.calls.clear();rc=self.run_dp2(cmds,vb,stride=stride,fvf=fvf);pre.value=0
   self.assertEqual(rc,(0,0));return list(self.calls)
  one=[(7,4,2,0,2,2,0,0)];two=[(7,4,2,0,2,2,0,0),(5,4,2,1,2,2,0,1)]
  try:
   for name,stages,fvf,stride,tw in (('one unit',one,0x1C4,32,0),('two units',two,0x2C4,40,1)):
    for trial in range(6):
     n=60;vb=mesh(n,stride,tw);idx=grid_idx(5,3) if trial%2 else [tuple(rnd.sample(range(n),3)) for _ in range(40)]
     cmds=self._itl(idx)+cmd(1,1,struct.pack('<HH',3,2))+self.tri(op=18,n=4,start=10)
     a=run(0,stages,fvf,stride,vb,cmds);b=run(1,stages,fvf,stride,vb,cmds)
     self.assertEqual(len(a),len(b));self.assertTrue(a)
     for pa,pb in zip(a,b):
      self.assertEqual(pa.pre_on,0);self.assertEqual(pb.pre_on,1,name)
      self.assertEqual((pb.vertex_count,self._runs(pb)),(pa.vertex_count,self._runs(pa)))
      self.assertEqual(self._trace(pb),self._trace(pa),(name,trial))
      self.assertEqual(lib.vx3dn_validate(C.byref(pb),C.byref(self.info)),lib.vx3dn_validate(C.byref(pa),C.byref(self.info)))
   # no XP program (plain diffuse, untextured): the arrays
   self._stages([(0,2,0,0,2,0,0,0)]);self.context.state.rs[29]=0
   pre.value=1;self.calls.clear();self.assertEqual(self.run_dp2(self.tri()),(0,0));pre.value=0
   self.assertTrue(all(p.pre_on==0 for p in self.calls))
  finally:
   feat.value=ALLF&~128;pre.value=0
 def test_pairs_as_strips(self):
  # .524: v10.442's ACCEL_D3D_PAIRLIST removed (no saving in three GTA3 A/B runs): two triangles sharing an edge start a strip run (4
  # quads: 4 strips of 4); a grid row is one strip; random lists: every triangle drawn once, each packet valid
  feat=U.in_dll(lib,'vx_d3d_feat');feat.value=ALLF
  try:
   pts=[(2+(k%4)*12+(k//8)*2,2+((k//4)%2)*10+(k//8)*12) for k in range(32)] # 4 quads: corners a b c d each
   vb=vertices(pts);quads=[]
   for q in range(4):a,b,c,d=q*8,q*8+4,q*8+1,q*8+5;quads+=[(a,b,c),(c,b,d)]
   self.calls.clear();self.assertEqual(self.run_dp2(self._itl(quads),vb),(0,0));old=self.calls[0]
   self.assertEqual(self._runs(old),[(1,4)]*4)
   self.assertEqual(sorted(self._ptris(old)),sorted(self._vtris(vb,quads)))
   vb,tris=self._grid(10,1);self.calls.clear();self.assertEqual(self.run_dp2(self._itl(tris),vb),(0,0));p=self.calls[0]
   self.assertEqual((p.vertex_count,self._runs(p)),(22,[(1,22)]))
   import random
   rnd=random.Random(442)
   for trial in range(20):
    vb2=vertices([(rnd.uniform(1,62),rnd.uniform(1,62)) for _ in range(40)])
    tri=[tuple(rnd.sample(range(40),3)) for _ in range(30)]
    for i in range(0,len(tri)-1,3):a,b,c=tri[i];tri[i+1]=(c,b,rnd.randrange(40))
    out=[]
    for f in (ALLF&~(1<<9),ALLF): # the lists alone (ACCEL_D3D_STRIPS off) and with strips: the same triangles
     feat.value=f;self.calls.clear();self.assertEqual(self.run_dp2(self._itl(tri),vb2),(0,0))
     out.append(sorted(sum((self._ptris(q) for q in self.calls),[])))
     for q in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(q),C.byref(self.info)),0)
    self.assertEqual(out[0],out[1]);self.assertGreater(len(out[0]),20)
  finally:
   feat.value=ALLF&~128
 def test_geometry_counters(self):
  # v10.440: the HAL's geometry counters (on with vx_d3d_geo_on): a 10x1 grid as one indexed list: 20 triangles from 22 source vertices,
  # a list triangle, a strip start and 18 strips' next triangles, 22 vertices written; the first triangle shares nothing, the rest two
  # indices; off: nothing counted
  feat=U.in_dll(lib,'vx_d3d_feat');on=U.in_dll(lib,'vx_d3d_geo_on');g=(U*24).in_dll(lib,'vx_d3d_geo');feat.value=ALLF
  try:
   vb,tris=self._grid(10,1)
   for k in range(24):g[k]=0
   on.value=1;self.calls.clear();self.assertEqual(self.run_dp2(self._itl(tris),vb),(0,0))
   self.assertEqual(list(g)[:17],[20,22,0,0,18,1,1,0,0,0,0,22,0,1,0,19,0])
   self.assertEqual(g[21],1);self.assertEqual(self.calls[0].vertex_count,22)
   # a triangle outside the viewport: the general path, clipped; the same indices again in the next call are first uses again
   vb2=vertices([(-20,4),(30,4),(4,30)]);self.assertEqual(self.run_dp2(self._itl([(0,1,2)]),vb2),(0,0))
   self.assertEqual((g[0],g[1],g[2],g[7],g[8],g[21]),(21,25,1,1,1,2));self.assertGreater(g[12],0)
   on.value=0;before=list(g);self.assertEqual(self.run_dp2(self._itl(tris),vb),(0,0));self.assertEqual(list(g),before)
  finally:
   feat.value=ALLF&~128;on.value=0
 def test_strips_random(self):
  # v10.236: random meshes (shared edges, culling, vertices outside the viewport that take the general path): strips on and off
  # give the same triangles, every packet valid
  import random
  feat=U.in_dll(lib,'vx_d3d_feat')
  try:
   for seed in range(40):
    R=random.Random(seed);n=R.randint(4,60)
    pts=[(R.uniform(-8,72) if R.random()<.1 else R.uniform(1,62),R.uniform(1,62)) for _ in range(n)]
    vb=b''.join(struct.pack('<4fI',x,y,.5,1.,0xA5000000|(k*0x101+1)) for k,(x,y) in enumerate(pts))
    tris=[];last=None
    for _ in range(R.randint(1,150)):
     if last and R.random()<.7:a,b=R.sample(last,2);c=R.randrange(n);t=(b,a,c) if R.random()<.5 else (a,c,b)
     else:t=tuple(R.randrange(n) for _ in range(3))
     tris.append(t);last=t
    self.context.state.rs[22]=R.choice((1,1,2,3))
    out=[]
    for f in (ALLF&~512,ALLF):
     feat.value=f;self.calls.clear();self.assertEqual(self.run_dp2(self._itl(tris),vb),(0,0))
     for p in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0,seed)
     out.append(sorted(sum((self._ptris(p) for p in self.calls),[])))
    self.assertEqual(out[0],out[1],seed)
   # textured with two units and specular (stride 40): every per-vertex array goes with its vertex when a strip head is reordered
   T,D,CUR=2,0,1;self._stages([(5,4,T,D,2,T,D,0),(5,4,T,CUR,3,T,CUR,1)])
   def ptex(p):
    out=[];base=0
    vt=lambda k:(p.vertices[k].color,p.shader.spec[k],p.tex.uv[k][0],p.tex.uv[k][1],p.stages.uv1[k][0],p.stages.uv1[k][1])
    for t,c in (self._runs(p) or [(0,p.vertex_count)]):
     out+=[tuple(sorted(vt(base+k+j) for j in range(3))) for k in (range(0,c,3) if t==0 else range(c-2))];base+=c
    return out
   for seed in range(20):
    R=random.Random(1000+seed);n=R.randint(4,40)
    vb=b''.join(struct.pack('<4fII4f',R.uniform(1,62),R.uniform(1,62),.5,1.,0xA5000000|k,0xFF000000|k*3,k/64.,k/32.,k/16.,k/8.) for k in range(n))
    tris=[];last=None
    for _ in range(R.randint(1,120)):
     if last and R.random()<.8:a,b=R.sample(last,2);t=(b,a,R.randrange(n))
     else:t=tuple(R.randrange(n) for _ in range(3))
     tris.append(t);last=t
    out=[]
    for f in (ALLF&~512,ALLF):
     feat.value=f;self.calls.clear();self.assertEqual(self.run_dp2(self._itl(tris),vb,stride=40,fvf=0x2C4),(0,0))
     self.assertTrue(self.calls and all(p.stages.units==2 for p in self.calls),seed)
     for p in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0,seed)
     out.append(sorted(sum((ptex(p) for p in self.calls),[])))
    self.assertEqual(out[0],out[1],seed)
  finally:
   feat.value=ALLF&~128;self.context.state.rs[22]=1;self._reset_stages()
 def test_projected_texture_as_xp(self):
  # v10.208: CLXP3D T151: COUNT3 | PROJECTED on the only unit: 9-dword s t q vertices, TAU 307001A0, 04/00 = A80009, FE02/01 = 22
  pts=[(8,8,0.,0.,1.),(56,8,2.,0.,2.),(8,56,0.,1.,1.)]
  vb=b''.join(struct.pack('<4fII3f',x,y,.5,1.,0xFFFFFFFF,0xFF000000,u,v,q) for x,y,u,v,q in pts)
  self._stages([(5,2,2,0,2,2,0,0)]);self.context.state.tss[0][24]=0x103
  self.calls.clear();self.assertEqual(self.run_dp2(self.tri(),vb,stride=36,fvf=0x101C4),(0,0));p=self.calls[0]
  self.assertEqual(p.stages.layout,2);f=lambda b:struct.unpack('<f',struct.pack('<I',b))[0]
  self.assertEqual(sorted(round(f(p.stages.q[k]),3) for k in range(3)),[1.,1.,2.])
  self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  w=self._trace(p)
  for x in (0x307001A0,0x00A80009,0x030A0908,0x25F6E000):self.assertIn(x,w)
  self.assertTrue(any(v>>24==1 and v&0xFF==0x22 for v in w))
  self._reset_stages()
 def test_premodulate_and_three_units(self):
  # v10.206: PREMODULATE (T144) multiplies the next stage's texture in: t1 * t0 out, alpha t0
  T,D,CUR=2,0,1
  st=self.context.state.tss
  for k,(op,a1,a2,aop,b1,b2) in enumerate([(17,T,D,2,T,D),(3,T,CUR,3,T,CUR)]):
   t=st[k];t[0]=5;t[1]=op;t[2]=a1;t[3]=a2;t[4]=aop;t[5]=b1;t[6]=b2;t[11]=k
  self.calls.clear();self.assertEqual(self.run_dp2(self.tri()),(0,0));p=self.calls[0];w=list(p.shader.words[:p.shader.count*4])
  self.assertEqual(p.stages.units,2);regs=[((w[i+2]>>8)&0xFF,(w[i+1]>>12)&0xFF) for i in range(0,len(w),4)]
  self.assertIn((1,0x0C),regs)
  # four stages are refused (three at most)
  for k in range(4):st[k][0]=5;st[k][1]=4;st[k][2]=T;st[k][3]=CUR if k else D;st[k][4]=2;st[k][5]=T
  self.calls.clear();self.run_dp2(self.tri());self.assertFalse(self.calls)
  for k in range(8):
   t=st[k];t[0]=0;t[1]=1 if k else 4;t[2]=2;t[3]=1;t[4]=1 if k else 2;t[5]=2;t[6]=1;t[11]=k
 def test_programs_built_as_xp_builds_them(self):
  # v10.198: every captured stage-0 program (CLXP3D T00-T15, T68-T115) is rebuilt from its Direct3D state word for word
  import sys;sys.path.insert(0,str(ROOT/'tests'));from xp_cases import XP_CASES
  pts=[(8,8,0.,0.),(56,8,1.,0.),(8,56,0.,1.)]
  vb=b''.join(struct.pack('<4fII2f',x,y,.5,1.,0xA0C08040,0xFF204060,u,v) for x,y,u,v in pts)
  self.keys[5]=0xFF2182C6;bad=[]
  for n,(op,a1,a2,aop,b1,b2,spec,key),want in XP_CASES:
   t=self.context.state.tss[0];t[0]=5;t[1]=op;t[2]=a1;t[3]=a2;t[4]=aop;t[5]=b1;t[6]=b2;self.context.state.rs[29]=spec;self.context.state.rs[41]=key
   self.calls.clear();self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4),(0,0),n)
   p=self.calls[0];got=list(p.shader.words[:p.shader.count*4])
   if got!=want:bad.append((n,['%08X'%w for w in got],['%08X'%w for w in want]))
   else:self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0,n)
  self.assertEqual(bad,[])
  # v10.303 (board, VXGL v3.2 scene 27): the texture read as B (MODULATE(TEXTURE | COMPLEMENT, DIFFUSE): the template with the complement on
  # B) carries B's last-read mark (w1 bit 21), not A's (w2 bit 17); ADDSMOOTH reads it as A and C: A's mark, as XP marks T85
  for (op,a1,a2,aop,b1),want in (((4,0x12,0,2,0),'B'),((11,2,0,2,0),'A'),((4,2,0,2,0),'A')):
   t[0]=5;t[1]=op;t[2]=a1;t[3]=a2;t[4]=aop;t[5]=b1;t[6]=2;self.context.state.rs[29]=0;self.context.state.rs[41]=0
   self.calls.clear();self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4),(0,0));w=list(self.calls[0].shader.words[:self.calls[0].shader.count*4])
   marks=[('A' if w[i*4+2]&0x20000 else '')+('B' if w[i*4+1]&0x200000 else '')+('C' if w[i*4]&0x2000000 else '') for i in range(len(w)//4)]
   self.assertEqual(''.join(marks),want,(op,a1,['%08X'%x for x in w]))
  # T14: diffuse + specular without a texture
  t[0]=0;t[1]=3;t[2]=2;t[3]=0;t[4]=3;t[5]=2;t[6]=0;self.context.state.rs[29]=1;self.context.state.rs[41]=0;self.calls.clear()
  self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4);p=self.calls[0]
  self.assertEqual((p.tex.enable,list(p.shader.words[:8])),(0,[0x00410550,0x50140924,0x00704005,0x00000820,0x01C00530,0xB01C0053,0x008040B6,0x040D0820]))
 def trace_words(self,p):
  tr=[];refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:tr.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0))
  io=IO(None,*refs);last=U();self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0);return [v for r,v in tr if r in (0x43C,0x440)]
 def test_mipmaps_aniso_and_formats_as_xp(self):
  # v10.199: CLXP3D T21 (32x32, 6 levels, MIPFILTER POINT, linear), T23 (LOD bias +1), T47 (anisotropic 2), T128 (non-square), T33-T35 (DXT)
  t=self.context.state.tss[0];t[0]=7;t[1]=2;t[2]=2;t[4]=2;t[5]=2;t[16]=2;t[17]=2;t[18]=2;t[13]=1;t[14]=1
  base=0x20000;self.chain=[base,base+0x1000,base+0x1400,base+0x1500,base+0x1600,base+0x1700]
  pts=[(8,8,0.,0.),(56,8,1.,0.),(8,56,0.,1.)]
  vb=b''.join(struct.pack('<4fI2f',x,y,.5,1.,0xffffffff,u,v) for x,y,u,v in pts)
  self.run_dp2(self.tri(),vb,stride=28,fvf=0x144);p=self.calls[0]
  self.assertEqual((p.mip.levels,p.mip.filter,list(p.mip.level[:5]),p.mip.aniso,p.mip.bias),(6,1,[0x10,0x14,0x15,0x16,0x17],1,0))
  self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  w=self.trace_words(p)
  for x in (0x30000140,0x31012491,0x32090092,0x20020020,0x22000055):self.assertIn(x,w)
  i=w.index(0x10030000);self.assertEqual(w[i+1:i+9],[0x10,0x14,0x15,0x16,0x17,0,0,0])
  t[19]=bits(1.0);self.calls.clear();self.run_dp2(self.tri(),vb,stride=28,fvf=0x144);self.assertIn(0x30020140,self.trace_words(self.calls[0]))
  t[19]=0;t[18]=1;t[16]=1;t[17]=3;t[21]=2;self.calls.clear();self.run_dp2(self.tri(),vb,stride=28,fvf=0x144);p=self.calls[0]
  self.assertEqual((p.mip.levels,p.mip.aniso),(1,2));w=self.trace_words(p);self.assertIn(0x31020820,w);self.assertIn(0x30000000,w)
  self.chain=[];t[17]=1;t[0]=8;self.calls.clear();self.run_dp2(self.tri(),vb,stride=28,fvf=0x144);p=self.calls[0]   # DXT3 512x4
  self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  w=self.trace_words(p)
  for x in (0x20004200,0x22008029,0x302A0000):self.assertIn(x,w)
  p.tex.surface.pitch=1024;self.assertNotEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  # v10.200: rows at least 32 bytes (board, v10.199: 8x8 L8 read black, 8x8 DXT1 lost its second block row)
  p.tex.surface=Surface(0x20000,32,8,8,20);self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  p.tex.surface=Surface(0x20000,8,8,8,20);self.assertNotEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  p.tex.surface=Surface(0x20000,32,8,8,21);self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  p.tex.surface=Surface(0x20000,16,8,8,21);self.assertNotEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
 def test_colour_key_program_as_xp_t45(self):
  # v10.197: COLORKEYENABLE on SELECTARG1 texture runs XP's six-instruction program with the key as floats; off, T00 alone
  t=self.context.state.tss[0];t[0]=5;t[1]=2;t[2]=2;t[4]=2;t[5]=2;self.context.state.rs[41]=1;self.keys[5]=0xFFFF00FF;self.keys[6]=0x07E0
  pts=[(8,8,0.,0.),(56,8,1.,0.),(8,56,0.,1.)]
  vb=b''.join(struct.pack('<4fI2f',x,y,.5,1.,0xffffffff,u,v) for x,y,u,v in pts)
  self.assertEqual(self.run_dp2(self.tri(),vb,stride=28,fvf=0x144),(0,0));p=self.calls[0]
  self.assertEqual((p.shader.count,p.shader.kflags&2,p.shader.words[0],p.shader.words[23],list(p.shader.key)),(6,2,0x80000550,0x000D0820,[0x3F800000,0,0x3F800000,0x3F800000]))
  self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  def trace_of(p):
   tr=[];refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:tr.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0))
   io=IO(None,*refs);last=U();self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0);return [v for r,v in tr]
  w=trace_of(p)
  for x in (0x92020006,0x93000003,0x95000010,0x80000550):self.assertIn(x,w)
  i=len(w)-1-w[::-1].index(0x21030000);self.assertEqual(w[i+1:i+3],[0x201001B0,0x314001B1])
  i=w.index(0x220300AC);self.assertEqual(w[i+1:i+5],[0x3F800000,0,0x3F800000,0x3F800000])
  ck=U.in_dll(lib,'vx3dn_ckey');ck.value=0;w=trace_of(p);ck.value=1
  self.assertIn(0x92010001,w);self.assertNotIn(0x80000550,w);self.assertNotIn(0x220300AC,w);i=len(w)-1-w[::-1].index(0x21030000);self.assertEqual(w[i+1],0x301001B0)
  t[0]=6;self.calls.clear();self.run_dp2(self.tri(),vb,stride=28,fvf=0x144);self.assertEqual(list(self.calls[0].shader.key),[0,0x3F800000,0,0x3F800000]) # 565 green
  del self.keys[6];self.calls.clear();self.run_dp2(self.tri(),vb,stride=28,fvf=0x144);self.assertEqual((self.calls[0].shader.count,self.calls[0].shader.kflags),(1,0))
  t[0]=5;self.context.state.rs[41]=0;self.calls.clear();self.run_dp2(self.tri(),vb,stride=28,fvf=0x144);self.assertEqual(self.calls[0].shader.count,1)
 def test_textured_triangle_packet_and_program(self):
  t=self.context.state.tss[0];t[0]=5;t[1]=2;t[2]=2;t[4]=2;t[5]=2;t[13]=3;t[14]=1;t[16]=2
  pts=[(8,8,0.,0.),(56,8,1.,0.),(8,56,0.,1.)]
  vb=b''.join(struct.pack('<4fI2f',x,y,.5,1.,0xffffffff,u,v) for x,y,u,v in pts)
  self.assertEqual(self.run_dp2(self.tri(),vb,stride=28,fvf=0x144),(0,0));self.assertEqual(len(self.calls),1)
  p=self.calls[0];self.assertEqual((p.tex.enable,p.tex.surface.offset,p.tex.surface.format,p.tex.filter,p.tex.address_u,p.tex.address_v,p.tex.mode),(1,0x20000,4,2,1,2,1))
  self.assertEqual([tuple(p.tex.uv[k]) for k in range(3)],[(bits(u),bits(v)) for _,_,u,v in pts])
  self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  trace=[];refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:trace.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0))
  io=IO(None,*refs);last=U();self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0)
  w=[v for r,v in trace if r==0x440];self.assertIn((0x43C,0x00020000),trace);self.assertIn(0x200,w);self.assertIn(0x30990000,w);self.assertIn(0x00F20005,w)
  self.assertIn(bits(1.),w[-8:])
  # an unbound texture with a TEXTURE argument draws diffuse; a missing handle is reported
  d=dropped().value;t[0]=9;self.calls.clear();self.assertEqual(self.run_dp2(self.tri(),vb,stride=28,fvf=0x144)[0],0);self.assertFalse(self.calls)
  self.assertEqual(dropped().value,d+1);self.assertTrue(drop_why().value&0x400)
  bad=Packet.from_buffer_copy(p);bad.tex.surface.pitch=64;self.assertEqual(lib.vx3dn_validate(C.byref(bad),C.byref(self.info)),1)
  bad=Packet.from_buffer_copy(p);bad.tex.surface.width=6;bad.tex.surface.pitch=24;self.assertEqual(lib.vx3dn_validate(C.byref(bad),C.byref(self.info)),1)
 def test_address_sets_both_coordinates(self):
  self.assertEqual(self.run_dp2(cmd(25,1,struct.pack('<HHI',0,12,2))),(0,0))
  self.assertEqual((self.context.state.tss[0][13],self.context.state.tss[0][14]),(2,2))
 def test_xp_programs_selected_and_emitted(self):
  # v10.172: DirectX 7's default stage (MODULATE texture x current, alpha = texture) -> XP's two-instruction program, 10-dword vertices
  t=self.context.state.tss[0];t[0]=5
  pts=[(8,8,0.,0.),(56,8,1.,0.),(8,56,0.,1.)]
  vb=b''.join(struct.pack('<4fII2f',x,y,.5,.5,0xff808080,0xff102030,u,v) for x,y,u,v in pts)
  self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4),(0,0))
  p=self.calls[-1];self.assertEqual(p.shader.count,2);self.assertEqual((p.shader.words[0],p.shader.words[4]),(0x0140B6D0,0x01C00530))
  self.assertEqual(p.shader.spec[0],0xff102030)
  trace=[];refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:trace.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0))
  io=IO(None,*refs);last=U();self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0);self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0)
  w=[v for r,v in trace if r==0x440]
  for word in (0x92010002,0x2A3F8000,0x2B3F8000,0x00A8000A,0x0140B6D0):self.assertIn(word,w)
  i=w.index(bits(8.));self.assertEqual(w[i:i+10],[bits(8.),bits(8.),bits(.5),bits(.5),0xff808080,0xff102030,bits(0.),bits(0.),bits(0.),bits(0.)])
  # ADD, MODULATE2X, TFACTOR (constant c2), and untextured specular
  for op,a2,count,first in [(7,1,2,0x00400550),(5,1,2,0x0140B6D0),(4,3,2,0x0140B6D0)]:
   self.calls.clear();t[1]=op;t[2]=2;t[3]=a2;self.context.state.rs[60]=0xFF40C080
   self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);q=self.calls[-1]
   self.assertEqual((q.shader.count,q.shader.words[0]),(count,first))
   if a2==3:self.assertEqual(q.shader.kflags,1);self.assertEqual(q.shader.konst[3],bits(1.))
   if op==5:self.assertEqual(q.shader.words[3],0x00002820)
  self.calls.clear();t[0]=0;t[1]=4;t[3]=1;self.context.state.rs[29]=1
  self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);q=self.calls[-1]
  self.assertEqual((q.tex.enable,q.shader.count,q.shader.words[0]),(0,2,0x00410550))
  self.calls.clear();self.context.state.rs[29]=0
  self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);self.assertEqual(self.calls[-1].shader.count,0) # plain diffuse keeps the proven path
 def test_header_truncation(self):
  for n in range(1,4):self.assertEqual(self.run_dp2(b'\0'*n)[0],INVALID)
 def test_payload_truncation(self):
  for stream in [self.tri(),rs((7,0)),cmd(42,0,pack(9,0,bits(1.),0))]:
   for n in range(4,len(stream)):self.assertEqual(self.run_dp2(stream[:n])[0],INVALID)
  self.assertFalse(self.calls)
 def test_error_offset_first_unhandled_token(self):self.assertEqual(self.run_dp2(rs((22,1))+cmd(255,0)),(UNPARSED,12))
 def test_unknown_callback_forward_progress(self):
  self.unknown_next=4;self.assertEqual(self.run_dp2(cmd(255,0)+self.tri())[0],0);self.assertEqual(len(self.calls),1)
 def test_unknown_callback_out_of_bounds(self):self.unknown_next=100;self.assertEqual(self.run_dp2(cmd(255,0))[0],UNPARSED)
 def test_state_only_needs_no_vertices(self):self.assertEqual(self.run_dp2(rs((22,1)),b'',0,0,0)[0],0)
 def test_lazy_binding_only_at_consuming_primitive(self):
  calls=[]
  def bind(_,p):calls.append(1);return INVALID
  callback=BIND(bind);self.services.bind_vertices=callback
  self.assertEqual(self.run_dp2(rs((22,1)))[0],0);self.assertFalse(calls)
  self.assertEqual(self.run_dp2(rs((22,1))+self.tri()),(INVALID,12));self.assertEqual(calls,[1])
 def test_texture_validation_reports_specific_failure(self):
  checks=[([(0,0,1)],0),([(0,1,2),(0,2,7)],2074),([(0,4,24)],2075),([(0,1,22)],2073),([(0,1,2),(0,2,0)],0),
   ([(0,2,7)],2074),([(0,5,7)],2076),([(1,1,2)],0),([(1,1,2),(2,1,2),(3,1,2)],2077)] # v10.206: three stages; DOT3 taken, BUMPENVMAP not
  for values,expected in checks:
   self.setUp()
   self.context.state.tss[0][0]=5 # keep the stage active while testing invalid state
   for stage,state,value in values:self.context.state.tss[stage][state]=value
   self.assertEqual(lib.vx_d3d_texture_status(C.byref(self.context)),(0x88760000|expected) if expected else 0)
  self.context.state.tss[0][5]=0;self.context.state.tss[1][1]=2;self.context.state.tss[2][1]=2;self.context.state.tss[3][1]=2
  self.assertEqual(lib.vx_d3d_texture_status(C.byref(self.context)),0x88760000|2077)
  self.context.state.tss[0][1]=1
  self.assertEqual(lib.vx_d3d_texture_status(C.byref(self.context)),0)
 def test_target_roles_and_logical_handles(self):
  self.assertEqual(self.run_dp2(cmd(41,1,pack(4,3)))[0],INVALID)
  self.assertEqual(self.run_dp2(cmd(41,1,pack(3,4)))[0],0)
  self.assertEqual((self.context.target_handle,self.context.depth_handle),(3,4))
 def test_fvf_skips_all_eight_texture_coordinate_sets(self):
  self.assertEqual(lib.vx_d3d_vertex_size(0x844),84)
  vb=b''.join(vertices()[i:i+20]+bytes(64) for i in range(0,len(vertices()),20))
  self.assertEqual(self.run_dp2(self.tri(),vb,stride=84,fvf=0x844)[0],0)
 def test_vertex_count_units_and_late_index_preflight(self):
  stream=cmd(3,2,struct.pack('<8H',0,1,2,0,0,1,5,0))
  self.assertEqual(self.run_dp2(stream)[0],INVALID);self.assertFalse(self.calls)
 def test_nonfinite_and_zero_rhw(self):
  # v10.367 (GTA3: one bad vertex failed the whole DrawPrimitives2 call): a primitive using a vertex that is not finite or has rhw <= 0 is
  # left out and counted (drop reason 2000); the call succeeds and its other primitives are drawn; nothing bad reaches a packet
  bad=U.in_dll(lib,'vx_d3d_bad_vertex_prims')
  for at,value in [(0,0x7fc00000),(4,0x7f800000),(8,0xff800000),(12,0),(12,0xbf800000)]:
   v=bytearray(vertices());v[at:at+4]=pack(value);b0=bad.value;d0=dropped().value;self.calls.clear()
   self.assertEqual(self.run_dp2(self.tri(),v),(0,0));self.assertFalse(self.calls);self.assertEqual(bad.value,b0+1);self.assertEqual(dropped().value,d0+1)
   # the same bad vertex 0 in a line list, a point, a fan and an indexed list, then a good triangle (vertices 2, 3, 4) in the same call
   good=cmd(26,1,struct.pack('<H',0)+struct.pack('<3H',2,3,4))
   stream=cmd(15,1,struct.pack('<H',0))+cmd(1,1,struct.pack('<HH',1,0))+cmd(21,1,struct.pack('<H',0))+self._itl([(0,1,2)])+good
   # .531: rhw 0 alone in the point (every vertex of it rhw 0, nothing reads rhw) is drawn with rhw 1; the rest are mixed and left out
   z=at==12 and value==0
   self.calls.clear();self.assertEqual(self.run_dp2(stream,v),(0,0));self.assertEqual(bad.value,b0+(4 if z else 5))
   self.assertEqual([(c.ext.prim,c.vertex_count) for c in self.calls][-1:],[(0,3)]);self.assertEqual(len(self.calls),2 if z else 1)
   for c in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(c),C.byref(self.info)),0)
  # an index past the vertices is still a malformed call
  self.calls.clear();self.assertEqual(self.run_dp2(self._itl([(0,1,9)]))[0],INVALID);self.assertFalse(self.calls)
 def test_rhw0_primitives_drawn_as_rhw1(self):
  # .531 (GTA3: its light streaks, every vertex rhw 0): a primitive whose vertices all have rhw +0 or -0 is drawn with rhw 1 when no texture
  # (.545: textured too) is bound on an active stage, table fog is off and the depth test is not a w-buffer; the packet is the one rhw 1 makes (the same words),
  # counted in vx_d3d_rhw0_prims, not as left out. Mixed rhw, a texture, table fog, a w-buffer: left out (drop reason 2000) as before.
  n0=U.in_dll(lib,'vx_d3d_rhw0_prims');bad=U.in_dll(lib,'vx_d3d_bad_vertex_prims')
  def vb(w,first=None):
   out=bytearray(vertices())
   for i in range(5):struct.pack_into('<I',out,i*20+12,w if first is None or i else first)
   return bytes(out)
  one=bits(1.0)
  streams=[('triangle',self.tri()),('line list',cmd(15,2,struct.pack('<H',0))),('points',cmd(1,1,struct.pack('<HH',3,0))),('fan',cmd(21,2,struct.pack('<H',0))),
           ('indexed list',self._itl([(0,1,2),(2,3,4)])),('strip',cmd(19,2,struct.pack('<H',0)))]
  def sig(calls):
   if not batch:return [(c.op,c.vertex_count,c.ext.prim,bytes(c.vertices)[:c.vertex_count*20],c.shader.count,tuple(c.shader.words)) for c in calls]
   # batched, the fast path (rhw 1) joins strips and pairs the general path (rhw 0) does not: the same vertices and programs
   return (sorted({bytes(c.vertices[i]) for c in calls for i in range(c.vertex_count)}),sorted({(c.shader.count,tuple(c.shader.words)) for c in calls}))
  for batch in (0,1):
   U.in_dll(lib,'vx_d3d_feat').value=ALLF if batch else ALLF&~128
   for name,s in streams:
    self.calls.clear();self.assertEqual(self.run_dp2(s,vb(one)),(0,0));ref=sig(self.calls);self.assertTrue(ref[0] if batch else ref,name)
    for z in (0,0x80000000):
     a=n0.value;b=bad.value;self.calls.clear();self.assertEqual(self.run_dp2(s,vb(z)),(0,0))
     self.assertEqual(sig(self.calls),ref,(name,hex(z),batch));self.assertTrue(n0.value>a,name);self.assertEqual(bad.value,b,name)
     for c in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(c),C.byref(self.info)),0)
   # .545: textured on stage 0, every vertex rhw 0: drawn as rhw 1 (the same packet), not left out
   self._stages([(5,4,2,0,4,2,0,0)])
   for name,s in streams:
    self.calls.clear();self.assertEqual(self.run_dp2(s,vb(one)),(0,0));ref=sig(self.calls);self.assertTrue(ref[0] if batch else ref,name)
    for z in (0,0x80000000):
     a=n0.value;b=bad.value;self.calls.clear();self.assertEqual(self.run_dp2(s,vb(z)),(0,0))
     self.assertEqual(sig(self.calls),ref,('textured',name,hex(z),batch));self.assertTrue(n0.value>a,name);self.assertEqual(bad.value,b,name)
     for c in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(c),C.byref(self.info)),0)
   self._stages([])
   # left out: one vertex rhw 1 (mixed), table fog, a w-buffer
   for name,setup,v in [('mixed',lambda:None,vb(0,one)),('mixed textured',lambda:self._stages([(5,4,2,0,4,2,0,0)]),vb(0,one)),
                        ('table fog',lambda:(self.context.state.rs.__setitem__(28,1),self.context.state.rs.__setitem__(35,3)),vb(0)),
                        ('w-buffer',lambda:self.context.state.rs.__setitem__(7,2),vb(0))]:
    setup();a=n0.value;b=bad.value;self.calls.clear();self.assertEqual(self.run_dp2(self.tri(),v),(0,0))
    self.assertFalse(self.calls,name);self.assertEqual(n0.value,a,name);self.assertEqual(bad.value,b+(name!='w-buffer'),name) # (a w-buffer: the whole state is refused before the vertices)
    self._stages([]);self.context.state.rs[28]=0;self.context.state.rs[35]=0;self.context.state.rs[7]=0
  U.in_dll(lib,'vx_d3d_feat').value=ALLF&~128
 def test_tnl(self):
  # .536 (ACCEL_D3D_TNL): the T&L commands kept (SETLIGHT with or without its D3DLIGHT7) and counted; a call of pretransformed vertices
  # draws as before; a call of untransformed vertices (FVF XYZ) is transformed (world x view x projection, the viewport, lighting, vertex
  # fog, generated and transformed coordinates), clipped at the near and far planes and drawn as an indexed list from the scratch buffer.
  # Off, the T&L commands are the runtime's to parse (unparsed here, as before).
  P=U.in_dll(lib,'vx_d3d_tnl');T=(U*18).in_dll(lib,'vx_d3d_tlp');N=(U*12).in_dll(lib,'vx_d3d_tn')
  I=bytes(struct.pack('<16f',*[1.0 if k%5==0 else 0.0 for k in range(16)]))
  xf=cmd(36,3,b''.join(struct.pack('<I',t)+I for t in (1,2,3)))
  def mat(d=(1,0.5,0.25,1),a=(0,0,0,0)):return cmd(33,1,struct.pack('<17f',*d,*a,0,0,0,0,0,0,0,0,0))
  def light(i=0,d=(0,0,1)):return cmd(34,2,struct.pack('<II',i,2)+struct.pack('<I',3)+struct.pack('<12f',1,1,1,1,0,0,0,0,0,0,0,0)+struct.pack('<3f',0,0,0)+struct.pack('<3f',*d)+struct.pack('<7f',1000,0,1,0,0,0,0)+struct.pack('<II',i,0))
  vp=cmd(28,1,struct.pack('<4I',0,0,64,64))
  def xyzc(pts):return b''.join(struct.pack('<3fI',x,y,z,0xff336699) for x,y,z in pts)
  def fl(u):return struct.unpack('<f',struct.pack('<I',u))[0]
  P.value=0
  self.assertEqual(self.run_dp2(xf+self.tri())[0],UNPARSED)
  P.value=1
  try:
   t0=list(T);self.calls.clear()
   self.assertEqual(self.run_dp2(xf+mat()+light()+cmd(35,1,struct.pack('<I',0))+cmd(44,1,bytes(20))+self.tri()),(0,0));self.assertTrue(self.calls)
   d=[a-b for a,b in zip(T,t0)];self.assertEqual(d[:8],[0,0,1,2,1,3,1,1],d)
   # transform and viewport: identity matrices, clip-space positions onto the 64x64 viewport
   n0=list(N);self.calls.clear()
   self.assertEqual(self.run_dp2(vp+xf+rs((137,0))+self.tri(),xyzc([(-.5,-.5,.5),(.5,-.5,.5),(-.5,.5,.5)]),stride=16,fvf=0x42),(0,0))
   v=sorted((round(fl(c.vertices[i].x),3),round(fl(c.vertices[i].y),3),round(fl(c.vertices[i].z),3),fl(c.vertices[i].rhw),c.vertices[i].color) for c in self.calls for i in range(c.vertex_count))
   self.assertEqual(v,[(16.0,16.0,.5,1.0,0xff336699),(16.0,48.0,.5,1.0,0xff336699),(48.0,48.0,.5,1.0,0xff336699)],v)
   self.assertEqual((N[0]-n0[0],N[1]-n0[1],N[2]-n0[2]),(1,1,3))
   # the near plane: one vertex behind it, the triangle clipped there (two vertices made, a quad: two triangles, every z >= 0)
   n0=list(N);self.calls.clear()
   self.assertEqual(self.run_dp2(vp+xf+rs((137,0))+self.tri(),xyzc([(-.5,-.5,-.5),(.5,-.5,.5),(-.5,.5,.5)]),stride=16,fvf=0x42),(0,0))
   self.assertEqual((N[3]-n0[3],N[5]-n0[5]),(1,2));zs=[fl(c.vertices[i].z) for c in self.calls for i in range(c.vertex_count)]
   self.assertEqual(len(zs),6);self.assertTrue(all(z>=0 for z in zs),zs)
   # wholly behind the near plane: dropped
   n0=list(N);self.calls.clear()
   self.assertEqual(self.run_dp2(vp+xf+self.tri(),xyzc([(-.5,-.5,-.5),(.5,-.5,-.2),(-.5,.5,-.1)]),stride=16,fvf=0x42),(0,0))
   self.assertEqual(N[4]-n0[4],1);self.assertFalse(self.calls)
   # .538: a NaN or infinite position: the triangle dropped as outside, no vertex sent on (none refused as bad)
   for bad in (float('nan'),float('inf')):
    n0=list(N);self.calls.clear()
    self.assertEqual(self.run_dp2(vp+xf+self.tri(),xyzc([(bad,-.5,.5),(.5,-.5,.5),(-.5,.5,.5)]),stride=16,fvf=0x42),(0,0))
    self.assertEqual((N[3]-n0[3],N[4]-n0[4]),(0,1));self.assertFalse(self.calls)
   # lighting: a directional light along +z, the normal facing it, material diffuse (1, .5, .25, 1): the colour (255, 128, 64)
   nv=b''.join(struct.pack('<6f',x,y,.5,0,0,-1) for x,y in [(-.5,-.5),(.5,-.5),(-.5,.5)])
   self.calls.clear()
   self.assertEqual(self.run_dp2(vp+xf+mat()+light()+rs((137,1),(139,0),(145,0),(147,0),(148,0))+self.tri(),nv,stride=24,fvf=0x12),(0,0))
   self.assertEqual({c.vertices[i].color for c in self.calls for i in range(c.vertex_count)},{0xffff8040})
   # facing away: ambient only (0 here): black, alpha from the material
   nb=b''.join(struct.pack('<6f',x,y,.5,0,0,1) for x,y in [(-.5,-.5),(.5,-.5),(-.5,.5)])
   self.calls.clear()
   self.assertEqual(self.run_dp2(vp+xf+mat()+light()+rs((137,1),(139,0),(145,0),(147,0),(148,0))+self.tri(),nb,stride=24,fvf=0x12),(0,0))
   self.assertEqual({c.vertices[i].color for c in self.calls for i in range(c.vertex_count)},{0xff000000})
   # an index past the vertex data: the draw left out, counted
   n0=list(N);self.calls.clear()
   self.assertEqual(self.run_dp2(vp+xf+self._itl([(0,1,9)]),xyzc([(-.5,-.5,.5),(.5,-.5,.5),(-.5,.5,.5)]),stride=16,fvf=0x42),(0,0))
   self.assertEqual(N[7]-n0[7],1);self.assertFalse(self.calls)
   # a call without a vertex format counts nothing; a D3DLIGHT7 cut short is refused
   t0=list(T);self.assertEqual(self.run_dp2(rs((22,1)),b'',0,0,0)[0],0);self.assertEqual(list(T),t0)
   self.assertEqual(self.run_dp2(cmd(34,1,struct.pack('<II',0,2)+bytes(40)))[0],INVALID)
   # .540 (review): two SETLIGHT entries, the first with its D3DLIGHT7 and the second cut off: refused without reading past the buffer
   self.assertEqual(self.run_dp2(cmd(34,2,struct.pack('<II',0,2)+bytes(104)))[0],INVALID)
   # .540 (review): a normal turned by the world matrix: +90 degrees about z takes (1,0,0) to (0,1,0) (row vectors); a light shining
   # along -y lights it fully. .536 turned normals by the transpose (to (0,-1,0): black)
   Rz=struct.pack('<16f',0,1,0,0,-1,0,0,0,0,0,1,0,0,0,0,1)
   xr=cmd(36,3,struct.pack('<I',1)+Rz+struct.pack('<I',2)+I+struct.pack('<I',3)+I)
   nr=b''.join(struct.pack('<6f',x,y,.5,1,0,0) for x,y in [(-.5,-.5),(.5,-.5),(-.5,.5)])
   self.calls.clear()
   self.assertEqual(self.run_dp2(vp+xr+mat()+light(d=(0,-1,0))+rs((137,1),(139,0),(145,0),(147,0),(148,0),(143,1))+self.tri(),nr,stride=24,fvf=0x12),(0,0))
   self.assertEqual({c.vertices[i].color for c in self.calls for i in range(c.vertex_count)},{0xffff8040})
   self.context.state.rs[137]=0;self.context.state.rs[143]=0
   # .540 (review): two texture stages: two output sets (FVF 2C4, 40 bytes); .536 named 3C4 and the core refused the draw
   self._stages([(0,3,2,0,3,2,0,0),(0,3,2,1,3,2,1,1)])
   t2=b''.join(struct.pack('<3f4f',x,y,.5,0,0,1,1) for x,y in [(-.5,-.5),(.5,-.5),(-.5,.5)])
   self.calls.clear();self.assertEqual(self.run_dp2(vp+xf+rs((137,0))+self.tri(),t2,stride=28,fvf=0x202),(0,0));self.assertTrue(self.calls)
   for p in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
   # a TEXTURETRANSFORMFLAGS count out of range (255 | PROJECTED): not used (counted), no read past the 4-vector
   self.context.state.tss[0][24]=0x1FF;n0=list(N);self.calls.clear()
   self.assertEqual(self.run_dp2(vp+xf+rs((137,0))+self.tri(),t2,stride=28,fvf=0x202),(0,0));self.assertTrue(N[11]>n0[11])
   self.context.state.tss[0][24]=0;self._stages([])
   # .540 (review): exp fog with a NaN position, used and unused (index 5 of a sparse draw 0, 1, 9): no hang (tn_exp looped 2^31 times)
   r=self.context.state.rs;r[28]=1;r[35]=0;r[140]=1;r[38]=0x3F000000
   pts=[(-.5,-.5,.5),(.5,-.5,.5),(-.5,.5,.5)]+[(0,0,.5)]*7;pts[5]=(float('nan'),0,.5)
   self.calls.clear();self.assertEqual(self.run_dp2(vp+xf+self._itl([(0,1,9)]),xyzc(pts),stride=16,fvf=0x42),(0,0));self.assertTrue(self.calls)
   self.assertEqual(self.run_dp2(vp+xf+self._itl([(0,1,5)]),xyzc(pts),stride=16,fvf=0x42),(0,0))
   r[28]=0;r[140]=0;r[38]=0
   # .540 (review): only the vertices a draw uses are transformed (3 of the span 0-9)
   n0=list(N);self.run_dp2(vp+xf+self._itl([(0,1,9)]),xyzc(pts),stride=16,fvf=0x42);self.assertEqual(N[2]-n0[2],3)
   # .540 (review): a context made again (vx_d3d_init) starts with identity matrices, not the last context's at that address
   S=struct.pack('<16f',.5,0,0,0,0,.5,0,0,0,0,1,0,0,0,0,1)
   self.run_dp2(cmd(36,1,struct.pack('<I',1)+S));lib.vx_d3d_init(C.byref(self.context),C.byref(self.target),None,7);self.context.state.rs[22]=1
   self.calls.clear();self.assertEqual(self.run_dp2(vp+rs((137,0))+self.tri(),xyzc([(-.5,-.5,.5),(.5,-.5,.5),(-.5,.5,.5)]),stride=16,fvf=0x42),(0,0))
   xs=sorted(round(fl(c.vertices[i].x),3) for c in self.calls for i in range(c.vertex_count));self.assertEqual(xs,[16.0,16.0,48.0],xs)
  finally:P.value=0;self.context.state.rs[22]=1;self.context.state.rs[137]=0
 def test_tnl_fused_same_triangles(self):
  # .541: the driver's T&L draws into the indexed packets directly (tn_ix: each vertex lit when it first enters a packet, its words
  # written there once) and gives the same triangles, word for word, as the per-triangle path (indexed packets off: every vertex lit,
  # then read back from the scratch buffer): lit, fogged, textured grids, some across the near plane, some off the screen
  import ctypes,random
  U.in_dll(lib,'vx_d3d_tnl').value=1;feat=U.in_dll(lib,'vx_d3d_feat');pre=U.in_dll(lib,'vx_d3d_prepack');ixp=U.in_dll(lib,'vx_d3d_ixpass');ixd=U.in_dll(lib,'vx_d3d_indexed')
  old=(feat.value,pre.value,ixp.value,ixd.value);feat.value=ALLF;pre.value=1;ixp.value=1
  m4=lambda *m:struct.pack('<16f',*m);I=m4(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1);zn,zf=1.0,200.0
  P=m4(1.5,0,0,0,0,2,0,0,0,0,zf/(zf-zn),1,0,0,-zn*zf/(zf-zn),0);N=12;vb=b'';tris=[]
  for j in range(N+1):
   for i in range(N+1):vb+=struct.pack('<6fI2f',i-N/2,0,j,0.3,1,0.1,0xff806040,i/N,j/N)
  for j in range(N):
   for i in range(N):a=j*(N+1)+i;tris+=[(a,a+N+1,a+1),(a+1,a+N+1,a+N+2)]
  light=cmd(34,2,struct.pack('<II',0,2)+struct.pack('<I',3)+struct.pack('<12f',1,1,1,1,0,0,0,0,.2,.2,.2,1)+struct.pack('<6f',0,0,0,.3,-1,.2)+struct.pack('<7f',1000,0,1,0,0,0,0)+struct.pack('<II',0,0))
  f2u=lambda f:struct.unpack('<I',struct.pack('<f',f))[0];rnd=random.Random(541);draws=b''
  for k in range(12):draws+=cmd(36,1,struct.pack('<I',1)+m4(1,0,0,0,0,1,0,0,0,0,1,0,rnd.uniform(-30,30),rnd.uniform(-8,-2),rnd.uniform(-10,60),1))+self._itl(tris)
  cmds=cmd(28,1,struct.pack('<4I',0,0,64,64))+cmd(36,2,struct.pack('<I',2)+I+struct.pack('<I',3)+P)+cmd(33,1,struct.pack('<17f',1,1,1,1,.3,.3,.3,1,0,0,0,0,0,0,0,0,0))+light
  cmds+=rs((137,1),(139,0xff404040),(145,1),(147,0),(148,0),(28,1),(35,0),(140,3),(36,f2u(10.)),(37,f2u(150.)),(22,1))+draws
  def tris_of(calls):
   out=[]
   for p in calls:
    b0=p.bursts.b[0];w=list(p.pre_w)
    if p.pre_on==2:
     ni=b0&0xFFFF;st=8 if b0&0x04000000 else 10;ix=(ctypes.c_uint16*ni).from_buffer_copy(bytes(p.vertices)[:ni*2])
     out+=[tuple(sorted(tuple(w[i*st:i*st+8]) for i in ix[k:k+3])) for k in range(0,ni,3)];continue
    V=(lambda i:tuple(w[i*10:i*10+8])) if p.pre_on else (lambda i:(p.vertices[i].x,p.vertices[i].y,p.vertices[i].z,p.vertices[i].rhw,p.vertices[i].color))
    base=0
    for tp,cn in ([(b>>16,b&0xFFFF) for b in p.bursts.b[:p.bursts.n]] or [(0,p.vertex_count)]):
     out+=[tuple(sorted(V(base+k+j) for j in range(3))) for k in (range(0,cn,3) if tp==0 else range(cn-2))];base+=cn
   return sorted(out)
  try:
   self._stages([(7,4,2,0,2,2,0,0)],spec=1);got={}
   for on in (1,0):
    ixd.value=on;self.calls.clear();self.assertEqual(self.run_dp2(cmds,vb,stride=36,fvf=0x152),(0,0));got[on]=tris_of(self.calls)
    for p in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
   self.assertTrue(len(got[1])>2000,len(got[1]))
   self.assertEqual(got[1],got[0]);self._tnl_scene_tris=got[1]
  finally:feat.value,pre.value,ixp.value,ixd.value=old;U.in_dll(lib,'vx_d3d_tnl').value=0;self._stages([]);r=self.context.state.rs;r[137]=r[28]=r[140]=0
 def test_bad_vertex_reasons(self):
  # v10.431: the bad-vertex checks counted by reason: [0] x, y or z not finite, [1] rhw not finite, [2] rhw 0 (either sign), [3] rhw < 0
  why=(U*4).in_dll(lib,'vx_d3d_bad_why')
  for at,value,slot in [(0,0x7fc00000,0),(4,0x7f800000,0),(8,0xff800000,0),(12,0x7f800000,1),(12,0x7fc00000,1),(12,0,2),(12,0x80000000,2),(12,0xbf800000,3)]:
   v=bytearray(vertices());v[at:at+4]=pack(value);w0=list(why);self.calls.clear()
   self.assertEqual(self.run_dp2(self.tri(),v),(0,0));self.assertFalse(self.calls)
   d=[a-b for a,b in zip(why,w0)];self.assertTrue(d[slot]>0 and sum(d)==d[slot],(at,hex(value),d))
 def test_phase_clock(self):
  # v10.431: with the phase clock set, a call's time is split into exclusive phases whose sum is the clock's advance; off, nothing is read
  t=[0]
  def tick():t[0]+=7;return t[0]
  CLK=C.CFUNCTYPE(U);ck=CLK(tick);ptr=C.c_void_p.in_dll(lib,'vx_d3d_phclock');ph=(U*10).in_dll(lib,'vx_d3d_ph_call')
  ptr.value=C.cast(ck,C.c_void_p).value;lib.vx_d3d_ph_begin(8);start=t[0]
  try:
   self.calls.clear();self.assertEqual(self.run_dp2(self.tri()),(0,0));self.assertEqual(len(self.calls),1);lib.vx_d3d_ph_end()
  finally:ptr.value=None
  self.assertEqual(sum(ph),t[0]-start)
  for k in (1,2,3,4,5):self.assertTrue(ph[k]>0,(k,list(ph))) # commands, template, vertices, primitives, buffers (the submit)
  self.assertEqual(ph[0]+ph[6]+ph[7]+ph[9],0) # the HAL's phases are not the core's
  n=t[0];self.calls.clear();self.assertEqual(self.run_dp2(self.tri()),(0,0));self.assertEqual(t[0],n) # off: no clock read
 def test_fast_path_parts(self):
  # v10.444: the fast path's parts, sampled one triangle in 16 (vx_d3d_ftprof with the phase clock): the sampled triangles each counted
  # once by outcome, the parts' time read from the clock, the token loop's time at least the parts'; the packets as with it off
  t=[0]
  def tick():t[0]+=16;return t[0]
  CLK=C.CFUNCTYPE(U);ck=CLK(tick);ptr=C.c_void_p.in_dll(lib,'vx_d3d_phclock');ft=(U*32).in_dll(lib,'vx_d3d_ft');on=U.in_dll(lib,'vx_d3d_ftprof')
  feat=U.in_dll(lib,'vx_d3d_feat');feat.value=ALLF
  try:
   vb,tris=self._grid(16,2);tris=tris+[(0,1,2)]*0
   self.calls.clear();self.assertEqual(self.run_dp2(self._itl(tris),vb),(0,0));off=[bytes(p) for p in self.calls]
   for k in range(32):ft[k]=0
   ptr.value=C.cast(ck,C.c_void_p).value;on.value=1
   self.calls.clear();self.assertEqual(self.run_dp2(self._itl(tris),vb),(0,0))
   self.assertEqual([bytes(p) for p in self.calls],off)
   n=len(tris)//16;self.assertEqual((ft[0],ft[12]),(n,n));self.assertEqual(ft[7]+ft[8]+ft[9]+ft[18],ft[0])
   self.assertTrue(ft[1]>0 and ft[2]>0 and ft[4]>0 and ft[14]==n and ft[13]==16*n,list(ft))
   self.assertGreaterEqual(ft[10],sum(ft[1:7])+ft[11]);self.assertGreater(ft[16],0)
   # v10.453: the sampled triangle is one chain from the loop's head: head, counter, call, the parts, return add up to the whole
   self.assertEqual(ft[20]+ft[11]+ft[21]+sum(ft[1:7])+ft[22],ft[10],list(ft));self.assertTrue(ft[20]==n and ft[21]==n and ft[22]==n and 0<ft[26]<=ft[10],list(ft))
   self.assertEqual(ft[25],len(tris));self.assertTrue(ft[23]>0 and ft[24]>0,list(ft))
  finally:
   ptr.value=None;on.value=0;feat.value=ALLF&~128
 def test_signed_zero_coordinates_pass_native_validation(self):
  v=bytearray(vertices([(-0.0,0),(56,8),(8,56)]));v[8:12]=pack(bits(-0.0))
  self.assertEqual(self.run_dp2(self.tri(),v),(0,0));self.assertEqual(len(self.calls),1)
  self.assertEqual(lib.vx3dn_validate(C.byref(self.calls[0]),C.byref(self.info)),0)
 def test_fvf_stride_and_flags(self):
  self.assertEqual(lib.vx_d3d_vertex_size(0x44),20)
  self.assertEqual(lib.vx_d3d_vertex_size(0xc4),24)
  self.assertEqual(lib.vx_d3d_vertex_size(0x144),28)
  for f in [0,0x42,0x54,0x45,0x944]:self.assertEqual(lib.vx_d3d_vertex_size(f),0)
  self.assertEqual(self.run_dp2(self.tri(),stride=16)[0],INVALID)
 def test_strip_winding_and_provoking_vertex(self):
  self.context.state.rs[9]=1;self.assertEqual(self.run_dp2(self.tri(19,2))[0],0)
  self.assertEqual([p.vertices[0].color for p in self.calls],[0xff112233,0xff224466])
  self.assertEqual(self.calls[1].vertices[1].x,bits(56.));self.assertEqual(self.calls[1].vertices[1].y,bits(56.))
 def test_fan_flat_uses_vertex_after_hub(self):
  self.context.state.rs[9]=1;self.assertEqual(self.run_dp2(self.tri(21,2))[0],0)
  self.assertEqual([p.vertices[0].color for p in self.calls],[0xff224466,0xff336699])
 def test_indexed_list2_base_vertex(self):
  self.assertEqual(self.run_dp2(cmd(26,1,struct.pack('<4H',1,0,2,1)))[0],0)
  self.assertEqual(self.calls[0].vertices[0].color,0xff224466)
 def test_indexed_strip_base_and_order(self):
  self.context.state.rs[9]=1
  self.assertEqual(self.run_dp2(cmd(20,2,struct.pack('<5H',1,0,2,1,3)))[0],0)
  self.assertEqual([p.vertices[0].color for p in self.calls],[0xff224466,0xff4488cc])
 def test_indexed_fan_base_and_flat_color(self):
  self.context.state.rs[9]=1
  self.assertEqual(self.run_dp2(cmd(22,2,struct.pack('<5H',1,0,1,2,3)))[0],0)
  self.assertEqual([p.vertices[0].color for p in self.calls],[0xff336699,0xff4488cc])
 def test_cull_modes(self):
  self.context.state.rs[22]=2;self.assertEqual(self.run_dp2(self.tri())[0],0);self.assertFalse(self.calls)
  self.context.state.rs[22]=3;self.assertEqual(self.run_dp2(self.tri())[0],0);self.assertEqual(len(self.calls),1)
 def test_cpu_clipping_all_packet_vertices_are_inside(self):
  self.assertEqual(self.run_dp2(self.tri(),vertices([(-40,-20),(90,10),(10,90)]))[0],0)
  self.assertTrue(self.calls)
  for p in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
 def test_fully_clipped_triangle_does_not_submit(self):
  self.assertEqual(self.run_dp2(self.tri(),vertices([(-40,-20),(-10,-10),(-10,-30)]))[0],0);self.assertFalse(self.calls)
 def test_outside_z_is_clipped_not_wrapped(self):
  v=bytearray(vertices());v[8:12]=pack(bits(-1.));self.assertEqual(self.run_dp2(self.tri(),v)[0],0)
  for p in self.calls:self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
 def test_clear_as_xp(self):
  # v10.372 (XP's S3gIGP.dll, static RE v5): depth/stencil bits dropped without a depth surface (the colour clear still runs), a
  # rectangle past the surface clamped to it, a finite depth outside 0..1 clamped; each failed the whole call
  self.assertEqual(self.run_dp2(cmd(42,1,pack(3,0xff00ff00,bits(1.),0)+pack(0,0,64,64))+self.tri())[0],0)
  self.assertEqual([p.op for p in self.calls],[2,1]);self.assertEqual(self.calls[0].clear_flags&7,1)
  self.calls.clear();self.assertEqual(self.run_dp2(cmd(42,1,pack(1,0,bits(1.),0)+struct.pack('<4i',-8,10,100,90)))[0],0)
  self.assertEqual(tuple(getattr(self.calls[0].clip,k) for k in ('left','top','right','bottom')),(0,10,64,64))
  lib.vx_d3d_init(C.byref(self.context),C.byref(self.target),C.byref(self.depth),7);self.context.state.rs[22]=1;self.calls.clear()
  feat=U.in_dll(lib,'vx_d3d_feat');f0=feat.value
  try:
   feat.value=f0&~524288;self.assertEqual(self.run_dp2(cmd(42,1,pack(2,0,bits(1.5),0)+pack(0,0,64,64)))[0],0);self.assertEqual(self.calls[0].clear_depth,0xffffffff)
  finally:feat.value=f0
 def test_zero_rectangle_clear_is_16_bytes_and_uses_viewport(self):
  stream=cmd(42,0,pack(9,0xffabcdef,bits(1.),0))+self.tri()
  self.assertEqual(self.run_dp2(stream)[0],0);self.assertEqual([p.op for p in self.calls],[2,1]);self.assertEqual(self.calls[0].clip.right,64)
 def test_compute_rects_clips(self):
  stream=cmd(28,1,pack(8,8,40,40))+cmd(42,1,pack(9,0,bits(1.),0)+struct.pack('<4i',-4,-4,64,64))
  self.assertEqual(self.run_dp2(stream)[0],0);self.assertEqual(tuple(getattr(self.calls[0].clip,k) for k in ('left','top','right','bottom')),(8,8,48,48))
 def test_clear_cpu_depth_conversion_and_binding(self):
  stream=cmd(41,1,pack(3,4))+cmd(42,1,pack(2,0,bits(.75),0)+pack(0,0,64,64))
  feat=U.in_dll(lib,'vx_d3d_feat');f0=feat.value
  try:
   feat.value=f0&~524288 # the clear packet (.530: the 3D clear, ACCEL_3D_CLEAR3D, removed)
   self.assertEqual(self.run_dp2(stream)[0],0);self.assertEqual(self.calls[0].clear_depth,0xbfffffff)
  finally:feat.value=f0
 def test_stencil_clear(self):
  # v10.179: no stencil storage -> nothing to clear; D24S8 -> a stencil clear packet carrying the value in clear_flags 15:8
  self.assertEqual(self.run_dp2(cmd(42,1,pack(4,0,bits(1.),0)+pack(0,0,64,64)))[0],0);self.assertFalse(self.calls)
  self.context.depth=Surface(0x10000,256,64,64,13)
  feat=U.in_dll(lib,'vx_d3d_feat');f0=feat.value
  try:
   feat.value=f0&~524288 # the clear packet (.530: the 3D clear, ACCEL_3D_CLEAR3D, removed)
   self.assertEqual(self.run_dp2(cmd(42,1,pack(6,0,bits(.5),0x6b)+pack(0,0,64,64)))[0],0)
   self.assertEqual(self.calls[0].clear_flags,0x6b06)
  finally:feat.value=f0
 def test_stateblock_record_execute_capture_delete(self):
  sb=lambda op:cmd(39,1,pack(op,17,0))
  self.assertEqual(self.run_dp2(sb(0)+rs((22,2))+sb(1))[0],0);self.assertEqual(self.context.state.rs[22],1)
  self.assertEqual(self.run_dp2(sb(3))[0],0);self.assertEqual(self.context.state.rs[22],2)
  self.assertEqual(self.run_dp2(rs((22,3))+sb(4)+rs((22,1))+sb(3))[0],0);self.assertEqual(self.context.state.rs[22],3)
  self.assertEqual(self.run_dp2(sb(2))[0],0);self.assertEqual(self.run_dp2(sb(3))[0],INVALID)
 def test_stateblocks_keep_unrecorded_state(self):
  self.run_dp2(cmd(39,1,pack(0,9,0))+rs((22,2))+cmd(39,1,pack(1,9,0)))
  self.run_dp2(rs((9,1))+cmd(39,1,pack(3,9,0)));self.assertEqual(self.context.state.rs[9],1)
 def test_context_state_is_separate(self):
  self.run_dp2(rs((22,2)));other=Context();lib.vx_d3d_init(C.byref(other),C.byref(self.target),None,7);self.assertEqual(other.state.rs[22],3)
 def test_execute_buffer_renderstate_feedback(self):
  self.run_dp2(rs((22,2)),flags=0);self.assertEqual(self.render_states[22],0)
  self.run_dp2(rs((22,3)),flags=2);self.assertEqual(self.render_states[22],3)
 def test_undrawable_state_drops_primitive_and_batch_succeeds(self):
  # v10.177: a failed DP2 fails the application's EndScene (DxDiag, D3DERR_SCENE_END_FAILED); the primitive is left out and counted
  for state,value,why in [(8,4,2),(52,1,0x40),(168,16,0x80),(25,9,0)]: # v10.295: COLORWRITEENABLE 0-15 is drawn (the channel mask), above that not; v10.296: FILLMODE 1-3
   self.setUp();d=dropped().value;rc=self.run_dp2(rs((state,value),(15,1) if state==25 else (16,0))+self.tri())[0]
   self.assertEqual(rc,0);self.assertFalse(self.calls);self.assertEqual(dropped().value,d+1)
   if why:self.assertEqual(drop_why().value&why,why)
 def test_colour_write_mask_and_logic_op(self):
  # v10.295: COLORWRITEENABLE's channels go in the packet's write mask (01/53 bits 3:0); render state 255 (the ICD's logic op) is never
  # taken from DP2; set by the ICD it is write_mask 0x100 | the truth table << 4 and 01/53 bits 11:8 (C = COPY otherwise)
  self.assertEqual(self.run_dp2(rs((168,5))+self.tri())[0],0);self.assertEqual(self.calls[-1].write_mask,5)
  self.assertEqual(self.run_dp2(rs((168,15),(255,0x16))+self.tri())[0],0);self.assertEqual(self.calls[-1].write_mask,15)
  self.context.state.rs[255]=0x16;self.assertEqual(self.run_dp2(self.tri())[0],0);p=self.calls[-1];self.assertEqual(p.write_mask,0x16F)
  self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  tr=[];io=IO(None,READ(lambda _,r:0),WRITE(lambda _,r,v:tr.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0));last=U()
  self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0);w=[v for r,v in tr if r==0x440]
  self.assertIn(0x60F,[v&0xFFF for v in w if v>>24==0x53]) # XOR (0110), all channels
  p.write_mask=0x161;tr.clear();self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0);w=[v for r,v in tr if r==0x440]
  self.assertIn(0x604,[v&0xFFF for v in w if v>>24==0x53]) # v10.296: red only is 01/53 bit 2 (B 1, G 2, R 4, A 8)
  p.write_mask=0x16C;tr.clear();self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0);w=[v for r,v in tr if r==0x440]
  self.assertIn(0x609,[v&0xFFF for v in w if v>>24==0x53]) # blue and alpha: 1 | 8
  p.write_mask=0x200;self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),1);self.context.state.rs[255]=0
 def test_v10304_engine_features(self):
  # v10.304: blend equations and constant factors (HCM 01/34-3F), LERP, result scale, the maximum level, border colour, unit-1 mip chains
  def ex(p):
   tr=[];io=IO(None,READ(lambda _,r:0),WRITE(lambda _,r,v:tr.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0));last=U()
   self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0);self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0);return tr
  def regs1(tr,r):return [v&0xFFFFFF for a,v in tr if a==0x440 and v>>24==r]
  vb=b''.join(struct.pack('<4fII2f',x,y,.5,1.,0xA0C08040,0xFF204060,u,v) for x,y,u,v in [(8,8,0.,0.),(56,8,1.,0.),(8,56,0.,1.)])
  c11=lambda v:(v*1024+127)//255
  st=self.context.state;r=st.rs
  r[27]=1;r[19]=14;r[20]=17;r[171]=3;r[193]=0x80402010
  self.assertEqual(self.run_dp2(self.tri())[0],0);p=self.calls[-1]
  self.assertEqual((p.raster.blend_src,p.raster.blend_dst),(14|(2<<5)|(0x402010<<8),17|(0x808080<<8)))
  tr=ex(p) # REVERSE_SUBTRACT: Ca = destination with its factor (1 - constant alpha), Cb = source with the constant colour
  self.assertEqual(regs1(tr,0x34)[-1],0x10000|(1<<10)|(0x15<<4));self.assertEqual(regs1(tr,0x35)[-1],(1<<14)|(5<<2))
  self.assertEqual((regs1(tr,0x39)[-1],regs1(tr,0x3A)[-1]),(c11(0x80)<<12,(c11(0x80)<<12)|c11(0x80)))
  self.assertEqual((regs1(tr,0x3C)[-1],regs1(tr,0x3D)[-1]),(c11(0x40)<<12,(c11(0x20)<<12)|c11(0x10)))
  r[171]=4;self.assertEqual(self.run_dp2(self.tri())[0],0);tr=ex(self.calls[-1]);self.assertEqual(regs1(tr,0x35)[-1],(3<<14)|(1<<8)|(0x15<<2)) # MIN: no factors (ONE)
  r[171]=6;self.assertEqual(lib.vx_d3d_unsupported_why(C.byref(self.context))&0x20,0x20);r[171]=0;r[27]=0;r[193]=0;r[19]=2;r[20]=1
  # LERP (26) on stage 0, v10.306: one instruction, the engine's interpolation (w3 op 11, XP's T10): A = arg1, B = arg2, C = arg0, no temporary
  t=st.tss[0];t[0]=7;t[1]=26;t[2]=3;t[3]=1;t[26]=2;t[4]=2;t[5]=0;self.chain=[]
  self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);p=self.calls[-1];w=list(p.shader.words[:p.shader.count*4])
  self.assertEqual(p.shader.count,2);self.assertEqual(w[:4],[0x02000500,0x00040050,0x00708205,0x00110820]) # A TFACTOR, B diffuse, C texture (its last read)
  self.assertEqual(p.shader.kflags&1,1);ex(p)
  t[26]=0x12;self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);w=list(self.calls[-1].shader.words[:4]) # complemented arg0: A and B swap
  self.assertEqual(((w[2]>>8)&0xFF,(w[1]>>12)&0xFF),(0x40,0x82));t[26]=2
  # colour and alpha both LERP (GL_BLEND on INTENSITY): two instructions, the alpha one under mask 8 with swizzles 6DB; no temporary, no complement
  t[4]=26;t[5]=3;t[6]=1;t[27]=2
  self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);p=self.calls[-1];w=list(p.shader.words[:p.shader.count*4])
  self.assertEqual(p.shader.count,2);self.assertEqual(w[4:8],[0x02006DB0,0xB00406DB,0x0080826D,0x04110820])
  self.assertTrue(all((w[k*4+3]>>4)&0xFF==0x82 and not w[k*4]&0x20000000 for k in range(2)));ex(p)
  t[4]=2;t[5]=0;t[6]=0;t[27]=0
  # result scale: MODULATE x2 (word 3 bits 14:13 = 1), colour and alpha folded only when their scales match
  t[1]=4;t[2]=2;t[3]=0;t[26]=0;t[4]=4;t[5]=2;t[6]=0;t[30]=1;t[31]=1
  self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);p=self.calls[-1];self.assertEqual((p.shader.count,(p.shader.words[3]>>13)&3),(1,1))
  t[31]=0;self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);p=self.calls[-1];self.assertEqual((p.shader.count,(p.shader.words[3]>>13)&3,(p.shader.words[7]>>13)&3),(2,1,0))
  t[1]=5;t[30]=1;self.assertEqual(lib.vx_d3d_unsupported_why(C.byref(self.context))&0x200,0x200);t[1]=4;t[30]=0
  # the maximum level (sampler 30 bits 11:6) from the chain, 8 at most; border colour (address 0, sampler 35 / 36)
  t[18]=2;t[13]=4;t[14]=4;t[15]=0xFF804020
  for n,ml in ((6,5),(9,8),(12,8)):
   self.chain=[0x20000+k*0x1000 for k in range(n)]
   self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);p=self.calls[-1];self.assertEqual((p.mip.levels,p.tex.address_u,p.tex.address_v,p.stages.border[0]),(n,0,0,0xFF804020))
   tr=ex(p);s30=[v for a,v in tr if a==0x440 and v>>24==0x30]
   self.assertIn((ml<<6)&0xFC0,[v&0xFC0 for v in s30]);self.assertIn((c11(0x80)<<12)|c11(0x40),regs1(tr,0x35));self.assertIn((c11(0x20)<<12)|c11(0xFF),regs1(tr,0x36))
  feat=U.in_dll(lib,'vx_d3d_feat');feat.value&=~2048
  self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);self.assertEqual((self.calls[-1].tex.address_u,self.calls[-1].tex.address_v),(1,1));feat.value|=2048
  t[13]=1;t[14]=1;t[15]=0;t[18]=1
  # unit 1's mip chain: umip = levels | filter << 8, its offsets in block 10030008
  self.chain=[0x20000+k*0x1000 for k in range(4)];t1=st.tss[1]
  t1[0]=7;t1[1]=4;t1[2]=2;t1[3]=1;t1[4]=2;t1[5]=1;t1[11]=0;t1[16]=1;t1[17]=1;t1[18]=2;t[1]=2;t[2]=2;t[4]=2;t[5]=2
  self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);p=self.calls[-1]
  self.assertEqual((p.stages.units,p.stages.umip[0],list(p.stages.ulevel[0][:3])),(2,4|(1<<8),[0x10,0x20,0x30]))
  tr=ex(p);i=[k for k,(a,v) in enumerate(tr) if a==0x43C and v==0x10030008][0];self.assertEqual([v for a,v in tr[i+1:i+4]],[0x10,0x20,0x30])
  t1[1]=1;t1[4]=1;t1[0]=0;self.chain=[]
 def test_v10319_audit_composer(self):
  # v10.319 (audit C1): SELECTARG1 TEXTURE x2 with specular: the scale stays on the colour (to the temporary), then the specular add; it was
  # XP's one-instruction SELECT + specular, whose word 3 replaced the scale
  vb=b''.join(struct.pack('<4fII2f',x,y,.5,1.,0xA0C08040,0xFF204060,u,v) for x,y,u,v in [(8,8,0.,0.),(56,8,1.,0.),(8,56,0.,1.)])
  st=self.context.state;t=st.tss[0]
  try:
   t[0]=7;t[1]=2;t[2]=2;t[3]=0;t[4]=2;t[5]=0;t[6]=0;t[30]=1;st.rs[29]=1
   self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);p=self.calls[-1]
   self.assertEqual((p.shader.count,(p.shader.words[3]>>13)&3),(3,1))
   t[30]=0;self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);self.assertEqual(self.calls[-1].shader.count,2) # unscaled: XP's form
   st.rs[29]=0
   # (audit C2) three stages that name TEXTURE only where the operation does not read it (SELECTARG2's ARG1, SELECTARG1's ARG2,
   # DOTPRODUCT3's alpha arguments): no texture unit, the draw is made
   for k,(op,a1,a2,aop,b1,b2) in enumerate([(3,2,0,2,0,2),(2,1,2,3,2,1),(3,2,1,2,1,2)]):
    q=st.tss[k];q[0]=7;q[1]=op;q[2]=a1;q[3]=a2;q[4]=aop;q[5]=b1;q[6]=b2;q[11]=k;q[30]=0
   self.calls.clear();self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);self.assertEqual(len(self.calls),1);p=self.calls[-1]
   self.assertEqual((p.tex.enable,p.stages.units),(0,0))
   # (review) BLENDTEXTUREALPHA as the alpha op reads the texture whatever the arguments: its unit stays with a SELECTARG1 colour
   self._reset_stages();q=st.tss[0];q[0]=7;q[1]=2;q[2]=0;q[3]=2;q[4]=13;q[5]=0;q[6]=1
   q1=st.tss[1];q1[0]=0;q1[1]=2;q1[2]=1;q1[4]=2;q1[5]=1
   self.calls.clear();self.assertEqual(self.run_dp2(self.tri(),vb,stride=32,fvf=0x1C4)[0],0);p=self.calls[-1];self.assertEqual(p.tex.enable,1)
  finally:
   st.rs[29]=0;self._reset_stages();t[30]=0
 def test_v10308_polygon_stipple(self):
  # v10.308: state.stip (the ICD's pattern as the engine takes it) -> packet.stipple -> block 14030000 and 01/01 bit 1; never on a clear
  def ex(p):
   tr=[];io=IO(None,READ(lambda _,r:0),WRITE(lambda _,r,v:tr.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0));last=U()
   self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0);self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0);return tr
  st=self.context.state;st.stip[0]=1
  for k in range(32):st.stip[1+k]=(0x80000001|k<<8)&0xFFFFFFFF
  try:
   self.assertEqual(self.run_dp2(self.tri())[0],0);p=self.calls[-1]
   self.assertEqual((p.stipple[0],p.stipple[1],p.stipple[32]),(1,0x80000001,0x80001F01))
   tr=ex(p);i=[k for k,(a,v) in enumerate(tr) if a==0x43C and v==0x14030000][0]
   self.assertEqual([v for a,v in tr[i+1:i+33]],[(0x80000001|k<<8)&0xFFFFFFFF for k in range(32)])
   self.assertEqual([v&0xFFFFFF for a,v in tr if a==0x440 and v>>24==1][-1]&2,2)
   p.stipple[0]=2;self.assertNotEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0);p.stipple[0]=1
   c=Packet();c.cb=C.sizeof(Packet);c.abi=p.abi;c.arm=p.arm;c.op=2;c.generation=p.generation;c.target=p.target;c.clip=p.clip;c.clear_flags=1;c.stipple[0]=1
   tr=[];io=IO(None,READ(lambda _,r:0),WRITE(lambda _,r,v:tr.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0));last=U()
   if lib.vx3dn_validate(C.byref(c),C.byref(self.info))==0:
    lib.vx3dn_execute(C.byref(c),C.byref(io),C.byref(last));self.assertFalse([1 for a,v in tr if a==0x43C and v==0x14030000])
  finally:
   st.stip[0]=0
 def test_harmless_states_are_ignored(self):
  for state,value in [(26,1),(28,1),(33,1),(39,1),(40,1),(47,1),(2,1),(41,1),(144,1),(152,1)]:
   self.setUp();self.assertEqual(self.run_dp2(rs((state,value))+self.tri())[0],0);self.assertEqual(len(self.calls),1)
 def test_zbias_fast_path(self):
  # v10.368 (audit N17): ZBIAS in the fast vertex path: z less 2^-12 a step (at most 16), not below 0, as primitive() writes it
  zs=(.5,.0001,1.)
  vb=b''.join(struct.pack('<4fI',x,y,z,1.,0xFF000000) for (x,y),z in zip(((8,8),(56,8),(8,56)),zs))
  for bias in (0,2,40):
   self.calls.clear();self.assertEqual(self.run_dp2(rs((47,bias))+self.tri(),vb),(0,0))
   zb=min(bias,16)/4096.;want=[struct.unpack('<I',struct.pack('<f',max(0.,struct.unpack('<f',struct.pack('<f',z))[0]-zb)))[0] if max(0.,z-zb)>0 else 0 for z in zs]
   self.assertEqual([v.z for v in self.calls[0].vertices[:3]],want,bias)
 def test_inert_states_keep_the_batch(self):
  # v10.368 (audits N16, 2.1): render states nothing reads (vertex processing, WRAPn) do not send the batch; the value is still kept
  U.in_dll(lib,'vx_d3d_feat').value=ALLF
  self.assertEqual(self.run_dp2(self.tri()+rs((137,0),(139,0x404040),(128,3))+self.tri(start=2))[0],0)
  self.assertEqual([p.vertex_count for p in self.calls],[6]);self.assertEqual((self.context.state.rs[137],self.context.state.rs[139]),(0,0x404040))
  self.calls.clear();self.assertEqual(self.run_dp2(self.tri()+rs((27,1))+self.tri(start=2))[0],0);self.assertEqual(len(self.calls),2)
 def test_phong_and_wbuffer_drawn(self):
  # v10.368 (audit N5): Phong as Gouraud, W-buffer as Z; both left every draw out
  self.assertEqual(self.run_dp2(rs((9,3))+self.tri())[0],0);self.assertEqual(len(self.calls),1)
  lib.vx_d3d_init(C.byref(self.context),C.byref(self.target),C.byref(self.depth),7);self.context.state.rs[22]=1;self.calls.clear()
  self.assertEqual(self.run_dp2(rs((7,2))+self.tri())[0],0);self.assertEqual([p.z_enable for p in self.calls],[1])
  self.calls.clear();self.assertEqual(self.run_dp2(rs((7,3))+self.tri())[0],0);self.assertEqual(self.calls,[])
 def test_blend_pairs(self):
  self.assertEqual(self.run_dp2(rs((19,5),(20,6),(27,1))+self.tri())[0],0);self.assertEqual(self.calls[0].blend,1)
  self.assertEqual((self.calls[0].raster.blend_src,self.calls[0].raster.blend_dst),(5,6))
  self.assertEqual(self.run_dp2(rs((19,2))+self.tri())[0],0);self.assertEqual((self.calls[1].raster.blend_src,self.calls[1].raster.blend_dst),(2,6))
  self.assertEqual(self.run_dp2(rs((19,12),(20,1))+self.tri())[0],0);self.assertEqual((self.calls[2].raster.blend_src,self.calls[2].raster.blend_dst),(5,6))
  self.assertEqual(self.run_dp2(rs((19,13))+self.tri())[0],0);self.assertEqual((self.calls[3].raster.blend_src,self.calls[3].raster.blend_dst),(6,5))
  d=dropped().value;self.assertEqual(self.run_dp2(rs((19,2),(20,11))+self.tri())[0],0);self.assertEqual(len(self.calls),4);self.assertEqual(dropped().value,d+1)
 def test_alpha_test_packet(self):
  self.assertEqual(self.run_dp2(rs((15,1),(25,7),(24,0x90))+self.tri())[0],0)
  self.assertEqual((self.calls[0].raster.alpha_func,self.calls[0].raster.alpha_ref),(7,0x90))
  self.assertEqual(self.run_dp2(rs((15,0))+self.tri())[0],0);self.assertEqual(self.calls[1].raster.alpha_func,0)
 def test_submit_failure_stops_and_latches_lost(self):
  self.result=4;self.assertEqual(self.run_dp2(self.tri()+self.tri())[0],LOST);self.assertEqual(len(self.calls),1);self.assertEqual(self.context.lost,1)
 def test_native_validation_epoch_alignment_bounds_overlap(self):
  self.run_dp2(self.tri());p=self.calls[0]
  p.generation=6;self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),5);p.generation=7
  p.target.offset=4;self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),1);p.target.offset=0
  p.z_enable=1;p.depth=self.target;p.depth.format=3;self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),1)
 def test_native_pitch_must_fit_ten_bit_hardware_fields(self):
  self.run_dp2(self.tri());p=self.calls[0]
  p.target=Surface(0x10000,0x7fe0,64,64,1)
  self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  p.target.pitch=0x8000;self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),1)
  p.target=self.target;p.z_enable=1;p.depth=Surface(0x10000,0x7fe0,64,64,3)
  self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),0)
  p.depth.pitch=0x8000;self.assertEqual(lib.vx3dn_validate(C.byref(p),C.byref(self.info)),1)
 def test_d32_far_endpoint_uses_xp_fixed_depth_clamps(self):
  self.context.depth=self.depth;self.context.state.rs[7]=1
  v=bytearray(vertices())
  for i in range(3):v[i*20+8:i*20+12]=pack(bits(1.0))
  self.assertEqual(self.run_dp2(self.tri(),v)[0],0);p=self.calls[0];trace=[]
  refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:trace.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0))
  self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(IO(None,*refs)),C.byref(U())),0)
  self.assertIn((0x440,0x123c0000),trace)
 def test_native_emitter_exact_pixel_program_and_triangle_packet(self):
  self.run_dp2(self.tri());p=self.calls[0];trace=[]
  refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:trace.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0))
  io=IO(None,*refs);last=U();self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0)
  self.assertIn((0x440,0x90058000),trace)
  self.assertIn((0x440,0x1e800000),trace)
  self.assertIn((0x440,0x20003f00),trace)
  self.assertIn((0x440,0x21000000),trace)
  self.assertIn((0x440,0x14ffffff),trace)
  self.assertIn((0x440,0x150000ff),trace)
  self.assertIn((0x440,0x2300ce02),trace)
  program=[(0x43c,0x20030000),(0x440,0x01c00530),(0x440,0x301c0053),(0x440,0x00f04005),(0x440,0x000d0820)]
  self.assertTrue(any(trace[i:i+5]==program for i in range(len(trace)-4)))
  self.assertTrue(all(r in (0x43c,0x440) for r,v in trace))
 def test_native_nonsquare_color_scissor_windows(self):
  self.run_dp2(self.tri());p=self.calls[0];p.target.width=640;p.target.height=480;p.target.pitch=2560
  p.clip=Rect(4,8,600,400);trace=[]
  refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:trace.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0));io=IO(None,*refs);last=U()
  self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),0)
  for word in [0x80000000|(8<<12)|400,0x81000000|(4<<12)|600,0x82000000|(8<<12)|400,0x83000000|(4<<12)|600,0x89000000|640]:self.assertIn((0x440,word),trace)
 def test_native_unreadable_status_never_writes(self):
  self.run_dp2(self.tri());p=self.calls[0];writes=[]
  refs=(READ(lambda _,r:0xffffffff),WRITE(lambda _,r,v:writes.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0));io=IO(None,*refs);last=U()
  self.assertEqual(lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last)),4);self.assertFalse(writes)
 def immediate(self,kind,count,ix=None):
  v=C.create_string_buffer(vertices());raw=None if ix is None else C.create_string_buffer(struct.pack('<'+'H'*len(ix),*ix))
  inp=Input(None,C.addressof(v),0,len(v)-1,5,20,0x44,0,None,0)
  return lib.vx_d3d_draw_primitive(C.byref(self.context),C.byref(inp),C.byref(self.services),kind,None if raw is None else C.addressof(raw),count)
 def test_immediate_list_and_indexed_validation_before_submission(self):
  self.assertEqual(self.immediate(4,3),0);self.assertEqual(len(self.calls),1)
  self.calls.clear();self.assertEqual(self.immediate(4,6,[0,1,2,0,1,99]),INVALID);self.assertFalse(self.calls)
 def test_immediate_fan_flat_provoking_vertices(self):
  self.context.state.rs[9]=1
  self.assertEqual(self.immediate(6,4),0);self.assertEqual(len(self.calls),2)
  self.assertEqual(self.calls[0].vertices[0].color,0xFF224466)
  self.assertEqual(self.calls[1].vertices[0].color,0xFF336699)
 def test_immediate_unsupported_primitives_and_partial_list(self):
  self.assertEqual(self.immediate(1,3),UNSUPPORTED)
  self.assertEqual(self.immediate(4,4),INVALID);self.assertFalse(self.calls)
 def test_texture_alpha_and_legacy_handles_are_not_silently_ignored(self):
  # v10.185: TEXTUREHANDLE (render state 1) and stage 0's TEXTUREMAP are one state
  self.assertEqual(self.run_dp2(cmd(8,1,struct.pack('<II',1,7))),(0,0));self.assertEqual(self.context.state.tss[0][0],7)
  self.assertEqual(self.run_dp2(cmd(25,1,struct.pack('<HHI',0,0,9))),(0,0));self.assertEqual(self.context.state.rs[1],9)
  d=dropped().value;self.calls.clear();self.assertEqual(self.run_dp2(self.tri())[0],0);self.assertFalse(self.calls) # handle 9 resolves to nothing
  self.assertEqual(dropped().value,d+1);self.assertTrue(drop_why().value&0x400)
  self.assertEqual(self.run_dp2(cmd(8,1,struct.pack('<II',1,0))),(0,0));self.assertEqual(self.context.state.tss[0][0],0)
  self.calls.clear();self.assertEqual(self.run_dp2(self.tri())[0],0);self.assertEqual(len(self.calls),1)
  self.calls.clear();self.context.state.tss[0][0]=5;self.context.state.tss[0][4]=24 # v10.198: DOTPRODUCT3 (BLENDFACTORALPHA now draws)
  self.assertEqual(self.run_dp2(self.tri())[0],0);self.assertFalse(self.calls);self.assertEqual(dropped().value,d+2);self.assertEqual(drop_why().value,0x200)
 def test_pixel_shader_disabled_during_program_load(self):
  self.run_dp2(self.tri());trace=[]
  refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:trace.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0))
  io=IO(None,*refs);last=U();self.assertEqual(lib.vx3dn_execute(C.byref(self.calls[0]),C.byref(io),C.byref(last)),0)
  bank=None;enabled=False;loaded=False
  for r,v in trace:
   if r==0x43C:bank=v
   elif bank==0x10000 and v>>24==1:enabled=bool(v&0x400)
   elif bank==0x20030000:self.assertFalse(enabled);loaded=True
  self.assertTrue(loaded);self.assertTrue(enabled)

if __name__=='__main__':
 suite=unittest.defaultTestLoader.loadTestsFromTestCase(Contract);result=unittest.TextTestRunner(verbosity=1).run(suite)
 (ROOT/'verification'/'D3D_CORE.json').write_text(json.dumps({'scope':__doc__,'tests':result.testsRun,'failures':len(result.failures),'errors':len(result.errors)},indent=2)+'\n')
 raise SystemExit(not result.wasSuccessful())
