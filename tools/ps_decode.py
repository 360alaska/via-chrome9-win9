#!/usr/bin/env python3
""".730: the Chrome9 pixel program's words decoded by the layout in docs/PIXEL_SHADER_ANSWERS.md (VIA's PDB field names: INVPS_AINST,
the 128-bit ALU instruction at block 20030000; INVPS_TINST, the 32-bit TAU word at block 21030000), for the R2 work (DirectX 7 stages on
more of the shader). decode_alu(w0, w1, w2, w3) / decode_tau(w) give the fields and a one-line text; check_alu / check_tau list what does
not fit the layout (a reserved bit set, an opcode or bank the drivers never use).
  python3 tools/ps_decode.py W0 W1 W2 W3 [...]   prints each 4-word instruction (hex words)"""
import sys
OPS={0x00:'MAD',0x02:'DP3',0x03:'DP4',0x04:'EXP',0x05:'LOG',0x06:'RCP',0x07:'RSQ',0x09:'CMP',0x0A:'CND',0x0B:'MAX',0x0C:'MIN',0x0D:'MOV',0x0E:'FRC',0x0F:'SINCOS',0x10:'MAD_FFP',0x11:'LRP'}
BANKS={0:'r',1:'v',2:'c',3:'t',5:'imm',7:'null'}
MODS={0:'',8:'-',1:'bias',9:'-bias',2:'1-',5:'bx2',0xD:'-bx2',4:'x2',0xC:'-x2'}
CH='xyzw10'
def swz(s12):
 return ''.join(CH[(s12>>(9-3*i))&7] if ((s12>>(9-3*i))&7)<6 else '?' for i in range(4))
def decode_alu(w0,w1,w2,w3):
 a=dict(op=(w3>>16)&0x1F,scale=(w3>>13)&7,sat=(w3>>12)&1,d0bank=(w3>>10)&3,d0idx=(w3>>6)&15,d1bank=(w3>>4)&3,d1idx=w3&15,coissue=(w3>>26)&1,
  wmask=(w2>>20)&15,
  a_fire=(w2>>17)&1,a_bank=(w2>>14)&7,a_idx=(w2>>8)&63,a_swz=((w2&0xFF)<<4)|((w1>>28)&15),a_mod=(w1>>24)&15,
  b_fire=(w1>>21)&1,b_bank=(w1>>18)&7,b_idx=(w1>>12)&63,b_swz=w1&0xFFF,b_mod=(w0>>28)&15,
  c_fire=(w0>>25)&1,c_bank=(w0>>22)&7,c_idx=(w0>>16)&63,c_swz=(w0>>4)&0xFFF,c_mod=w0&15)
 def src(k):
  b=a[k+'_bank'];m=MODS.get(a[k+'_mod'],'mod%X'%a[k+'_mod'])
  return '%s%s%d.%s%s'%(m+'(' if m else '',BANKS.get(b,'bank%d'%b),a[k+'_idx'],swz(a[k+'_swz']),')' if m else '')+("'" if a[k+'_fire'] else '')
 db={0:'r',1:'T',2:'oC',3:'r'}.get(a['d0bank']);di=a['d0idx']+(12 if a['d0bank']==3 else 0)
 a['text']='%s%s%s %s%d.%s, %s, %s, %s'%(OPS.get(a['op'],'op%02X'%a['op']),'_sat' if a['sat'] else '',' x%d'%(1<<a['scale']) if 0<a['scale']<4 else (' /%d'%(1<<(a['scale']-4)) if a['scale']>4 else ''),
  db,di,''.join(c for i,c in enumerate('xyzw') if a['wmask']>>i&1),src('a'),src('b'),src('c'))
 return a
def check_alu(w0,w1,w2,w3):
 a=decode_alu(w0,w1,w2,w3);bad=[]
 if w2>>24:bad.append('w2 31:24 set (DEST_RESERVED)')
 if (w2>>18)&3:bad.append('w2 19:18 set (SRC0_RESERVED)')
 if (w1>>22)&3:bad.append('w1 23:22 set (SRC1_RESERVED)')
 if (w0>>26)&3:bad.append('w0 27:26 set (SRC2_RESERVED)')
 if (w3>>30)&3 or (w3>>27)&1 or (w3>>21)&31:bad.append('w3 reserved bits set')
 if a['op'] not in OPS:bad.append('opcode %02X unknown'%a['op'])
 for k in 'abc':
  if a[k+'_bank'] not in BANKS:bad.append('%s bank %d unknown'%(k,a[k+'_bank']))
  if a[k+'_mod'] not in MODS:bad.append('%s modifier %X unknown'%(k,a[k+'_mod']))
 if not a['wmask']:bad.append('no write mask')
 return bad
def decode_tau(w):
 t=dict(dfire=w>>31&1,pvalid=w>>29&1,sfire=w>>28&1,stage=w>>24&15,op=w>>21&7,dreg=w>>20&1,dst=w>>16&15,coord=w>>12&15,swz=w>>4&0xFF,sampler=w&15)
 t['text']='%s r%d, t%d, s%d'%({0:'texld',1:'texldb',2:'texkill',3:'texldp'}.get(t['op'],'op%d'%t['op']),t['dst'],t['coord'],t['sampler'])
 return t
def check_tau(w):
 t=decode_tau(w);bad=[]
 if not t['pvalid']:bad.append('bit 29 (MISC_PVALID) clear')
 if t['op']>3:bad.append('TAU op %d unknown'%t['op'])
 if w>>30&1:bad.append('bit 30 (debug) set')
 return bad
if __name__=='__main__':
 v=[int(x,16) for x in sys.argv[1:]]
 for i in range(0,len(v)-3,4):print('%08X %08X %08X %08X  %s'%(*v[i:i+4],decode_alu(*v[i:i+4])['text']),*check_alu(*v[i:i+4]))
