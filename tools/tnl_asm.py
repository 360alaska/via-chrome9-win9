#!/usr/bin/env python3
"""tnl_asm.py (.544): the driver's T&L group kernels for the board (src/vx_tnl_sse.asm, Open Watcom wasm).

The HAL is built for the 486 and wasm refuses SSE mnemonics, so the kernels are written here in Intel syntax, assembled by clang's
assembler or GNU as (i686, flat, no relocations allowed), and written out as `db` bytes, one line an instruction with the mnemonic as a comment.
Each kernel is cdecl with one argument, the tnk_frame (src/vx_d3d_tnl_soa.inc: the offsets below are that struct's on the board, checked
there by TNK_OFF). The sequences are the intrinsics' of vx_d3d_tnl_soa.inc instruction for instruction, in the same order, so a lane's
result is the same on the host and the board; tests/test_tnl_asm.c runs both on random frames and compares the words.

  python3 tools/tnl_asm.py            writes src/vx_tnl_sse.asm (and tests/tnl_kernels.s, the GAS source, for the test)
"""
import subprocess,sys,re,tempfile,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
F={'m':0,'vp':256,'e':384,'nm':640,'ld':784,'lc':1296,'base':1808,'fog':1872,'nl':1936,'lit':1940,'fogmode':1944,'rfog':1948,'nnorm':1952,'haveN':1956,
 'stride':1960,'n':1964,'first':1968,'reuse_diffuse':1972,'eye_z_ready':1976,'gx':1984,'gy':2000,'gz':2016,'gnx':2032,'gny':2048,'gnz':2064,'gdc':2080,'gsc':2096,'od':2112,'os':2128,
 'k255':2144,'kone':2160,'khalf':2176,'kl2e':2192,'kexpc':2208,'kneg80':2336,'kinf':2352,'ksign':2368,'k127':2384,'kmask24':2400,'kb':2416,'k7f':2512,
 'sx':2528,'sy':2532,'sz':2536,'cx':2540,'cy':2544,'cz':2548,'cw':2552,'rw':2556,'cc':2560,'rec':2564,'scratch':2576}
def o(name,i=0):return F[name]+i
# a matrix row r by the lanes in xmm5 (x), xmm6 (y), xmm7 (z): ((x m[r] + y m[4+r]) + z m[8+r]) + m[12+r] into xmm0 (m at base b)
def row(b,r):
 return f"""
 movups xmm0,[eax+{b+16*r}]
 mulps xmm0,xmm5
 movups xmm1,[eax+{b+16*(4+r)}]
 mulps xmm1,xmm6
 addps xmm0,xmm1
 movups xmm1,[eax+{b+16*(8+r)}]
 mulps xmm1,xmm7
 addps xmm0,xmm1
 movups xmm1,[eax+{b+16*(12+r)}]
 addps xmm0,xmm1"""
POS=f"""
_tnk_pos:
 push ebx
 push esi
 push edi
 push ebp
 mov eax,[esp+20]
 mov ecx,[eax+{o('first')}]
 mov edx,[eax+{o('n')}]
 add edx,ecx
 xor ebx,ebx
 cmp ecx,edx
 jae 9f
1:
 mov esi,[eax+{o('sx')}]
 movups xmm5,[esi+ebx*4]
 mov esi,[eax+{o('sy')}]
 movups xmm6,[esi+ebx*4]
 mov esi,[eax+{o('sz')}]
 movups xmm7,[esi+ebx*4]
{row(0,0)}
 mov esi,[eax+{o('cx')}]
 movups [esi+ecx*4],xmm0
{row(0,1)}
 mov esi,[eax+{o('cy')}]
 movups [esi+ecx*4],xmm0
{row(0,2)}
 mov esi,[eax+{o('cz')}]
 movups [esi+ecx*4],xmm0
{row(0,3)}
 mov esi,[eax+{o('cw')}]
 movups [esi+ecx*4],xmm0
 movaps xmm3,xmm0
 xorps xmm1,xmm1
 movaps xmm2,xmm1
 cmpltps xmm2,xmm3
 movups xmm4,[eax+{o('kone')}]
 divps xmm4,xmm3
 andps xmm4,xmm2
 mov esi,[eax+{o('cx')}]
 movups xmm5,[esi+ecx*4]
 mov esi,[eax+{o('cy')}]
 movups xmm6,[esi+ecx*4]
 mov esi,[eax+{o('cz')}]
 movups xmm7,[esi+ecx*4]
 movaps xmm0,xmm7
 cmpltps xmm0,xmm1
 movaps xmm2,xmm3
 cmpleps xmm2,xmm1
 orps xmm0,xmm2
 andps xmm0,[eax+{o('kb',0)}]
 movaps xmm2,xmm3
 cmpltps xmm2,xmm7
 andps xmm2,[eax+{o('kb',16)}]
 orps xmm0,xmm2
 movaps xmm2,xmm1
 subps xmm2,xmm3
 movaps xmm1,xmm5
 cmpltps xmm1,xmm2
 andps xmm1,[eax+{o('kb',32)}]
 orps xmm0,xmm1
 movaps xmm1,xmm3
 cmpltps xmm1,xmm5
 andps xmm1,[eax+{o('kb',48)}]
 orps xmm0,xmm1
 movaps xmm1,xmm6
 cmpltps xmm1,xmm2
 andps xmm1,[eax+{o('kb',64)}]
 orps xmm0,xmm1
 movaps xmm1,xmm3
 cmpltps xmm1,xmm6
 andps xmm1,[eax+{o('kb',80)}]
 orps xmm0,xmm1
 movups xmm2,[eax+{o('kinf')}]
 movaps xmm1,xmm3
 pand xmm1,xmm2
 pcmpeqd xmm1,xmm2
 movaps xmm3,xmm5
 pand xmm3,xmm2
 pcmpeqd xmm3,xmm2
 por xmm1,xmm3
 movaps xmm3,xmm6
 pand xmm3,xmm2
 pcmpeqd xmm3,xmm2
 por xmm1,xmm3
 movaps xmm3,xmm7
 pand xmm3,xmm2
 pcmpeqd xmm3,xmm2
 por xmm1,xmm3
 movaps xmm3,xmm4
 pand xmm3,xmm2
 pcmpeqd xmm3,xmm2
 por xmm1,xmm3
 movaps xmm3,xmm1
 pandn xmm3,xmm0
 pand xmm1,[eax+{o('k7f')}]
 por xmm3,xmm1
 packssdw xmm3,xmm3
 packuswb xmm3,xmm3
 mov esi,[eax+{o('cc')}]
 movd [esi+ecx],xmm3
 xorps xmm1,xmm1
 mulps xmm5,xmm4
 mulps xmm5,[eax+{o('vp',16)}]
 addps xmm5,[eax+{o('vp',0)}]
 addps xmm5,xmm1
 mulps xmm6,xmm4
 mulps xmm6,[eax+{o('vp',48)}]
 addps xmm6,[eax+{o('vp',32)}]
 addps xmm6,xmm1
 mulps xmm7,xmm4
 mulps xmm7,[eax+{o('vp',80)}]
 addps xmm7,[eax+{o('vp',64)}]
 addps xmm7,xmm1
 movaps xmm0,xmm5
 unpcklps xmm0,xmm6
 movaps xmm1,xmm7
 unpcklps xmm1,xmm4
 unpckhps xmm5,xmm6
 unpckhps xmm7,xmm4
 mov edi,[eax+{o('stride')}]
 mov esi,ecx
 imul esi,edi
 add esi,[eax+{o('rec')}]
 movaps xmm2,xmm0
 movlhps xmm2,xmm1
 movups [esi],xmm2
 movhlps xmm1,xmm0
 movups [esi+edi],xmm1
 movaps xmm2,xmm5
 movlhps xmm2,xmm7
 movups [esi+edi*2],xmm2
 movhlps xmm7,xmm5
 lea esi,[esi+edi*2]
 movups [esi+edi],xmm7
 add ecx,4
 add ebx,4
 cmp ecx,edx
 jb 1b
9:
 pop ebp
 pop edi
 pop esi
 pop ebx
 ret
"""
# e^x of xmm2 (x <= 0) into xmm4; uses xmm0 xmm1 xmm3 xmm5 xmm6 xmm7
EXP=f"""
8:
 movaps xmm3,xmm2
 movups xmm0,[eax+{o('kneg80')}]
 cmpltps xmm0,xmm2
 xorps xmm4,xmm4
 movaps xmm1,xmm2
 cmpltps xmm1,xmm4
 maxps xmm2,[eax+{o('kneg80')}]
 minps xmm2,xmm4
 mulps xmm2,[eax+{o('kl2e')}]
 cvttps2dq xmm4,xmm2
 cvtdq2ps xmm5,xmm4
 movaps xmm6,xmm2
 cmpltps xmm6,xmm5
 psrld xmm6,31
 psubd xmm4,xmm6
 cvtdq2ps xmm5,xmm4
 movaps xmm6,xmm2
 subps xmm6,xmm5
 movups xmm5,[eax+{o('kexpc',96)}]
 mulps xmm5,xmm6
 addps xmm5,[eax+{o('kexpc',80)}]
 mulps xmm5,xmm6
 addps xmm5,[eax+{o('kexpc',64)}]
 mulps xmm5,xmm6
 addps xmm5,[eax+{o('kexpc',48)}]
 mulps xmm5,xmm6
 addps xmm5,[eax+{o('kexpc',32)}]
 mulps xmm5,xmm6
 addps xmm5,[eax+{o('kexpc',16)}]
 mulps xmm5,xmm6
 addps xmm5,[eax+{o('kexpc',0)}]
 paddd xmm4,[eax+{o('k127')}]
 pslld xmm4,23
 mulps xmm5,xmm4
 andps xmm5,xmm0
 andps xmm5,xmm1
 movups xmm4,[eax+{o('kone')}]
 xorps xmm0,xmm0
 movaps xmm7,xmm0
 cmpleps xmm7,xmm3
 andps xmm4,xmm7
 orps xmm5,xmm4
 movaps xmm4,xmm5
 ret
"""
# a byte vector from four lanes times 255: cvtps2dq, packssdw, packuswb with zero (xmm1 must be zero): v in/out
def tobytes(v):
 return f"""
 mulps {v},[eax+{o('k255')}]
 cvtps2dq {v},{v}
 packssdw {v},{v}
 packuswb {v},xmm1"""
LIGHT=f"""
_tnk_light:
 push ebx
 push esi
 push edi
 push ebp
 mov eax,[esp+20]
 movups xmm4,[eax+{o('kone')}]
 mov esi,[eax+{o('fogmode')}]
 test esi,esi
 jz 3f
 cmp dword ptr [eax+{o('eye_z_ready')}],0
 je 13f
 movups xmm0,[eax+{o('gz')}]
 movups xmm1,[eax+{o('ksign')}]
 andnps xmm1,xmm0
 jmp 2f
13:
 movups xmm5,[eax+{o('gx')}]
 movups xmm6,[eax+{o('gy')}]
 movups xmm7,[eax+{o('gz')}]
{row(o('e'),2)}
 cmp dword ptr [eax+{o('rfog')}],0
 jne 12f
 movups xmm1,[eax+{o('ksign')}]
 andnps xmm1,xmm0
 jmp 2f
12:
 mulps xmm0,xmm0
 movaps xmm4,xmm0
{row(o('e'),0)}
 mulps xmm0,xmm0
 movaps xmm3,xmm0
{row(o('e'),1)}
 mulps xmm0,xmm0
 addps xmm3,xmm0
 addps xmm3,xmm4
 sqrtps xmm1,xmm3
2:
 cmp esi,3
 jne 4f
 movups xmm4,[eax+{o('fog',16)}]
 subps xmm4,xmm1
 divps xmm4,[eax+{o('fog',32)}]
 jmp 3f
4:
 movups xmm0,[eax+{o('fog',0)}]
 mulps xmm0,xmm1
 cmp esi,1
 je 5f
 mulps xmm0,xmm0
5:
 xorps xmm2,xmm2
 subps xmm2,xmm0
 call 8f
3:
 movups [eax+{o('scratch',48)}],xmm4
 cmp dword ptr [eax+{o('lit')}],0
 je 6f
 cmp dword ptr [eax+{o('reuse_diffuse')}],0
 jne 14f
 movups xmm5,[eax+{o('base',0)}]
 movups xmm6,[eax+{o('base',16)}]
 movups xmm7,[eax+{o('base',32)}]
 cmp dword ptr [eax+{o('haveN')}],0
 je 7f
 movups xmm0,[eax+{o('gnx')}]
 movups xmm1,[eax+{o('gny')}]
 movups xmm2,[eax+{o('gnz')}]
 movaps xmm3,xmm0
 mulps xmm3,[eax+{o('nm',0)}]
 movaps xmm4,xmm1
 mulps xmm4,[eax+{o('nm',48)}]
 addps xmm3,xmm4
 movaps xmm4,xmm2
 mulps xmm4,[eax+{o('nm',96)}]
 addps xmm3,xmm4
 movups [eax+{o('scratch',64)}],xmm3
 movaps xmm3,xmm0
 mulps xmm3,[eax+{o('nm',16)}]
 movaps xmm4,xmm1
 mulps xmm4,[eax+{o('nm',64)}]
 addps xmm3,xmm4
 movaps xmm4,xmm2
 mulps xmm4,[eax+{o('nm',112)}]
 addps xmm3,xmm4
 movups [eax+{o('scratch',80)}],xmm3
 mulps xmm0,[eax+{o('nm',32)}]
 mulps xmm1,[eax+{o('nm',80)}]
 addps xmm0,xmm1
 mulps xmm2,[eax+{o('nm',128)}]
 addps xmm0,xmm2
 movaps xmm3,xmm0
 movups xmm1,[eax+{o('scratch',64)}]
 movups xmm2,[eax+{o('scratch',80)}]
 cmp dword ptr [eax+{o('nnorm')}],0
 je 10f
 movaps xmm0,xmm1
 mulps xmm0,xmm1
 movaps xmm4,xmm2
 mulps xmm4,xmm2
 addps xmm0,xmm4
 movaps xmm4,xmm3
 mulps xmm4,xmm3
 addps xmm0,xmm4
 sqrtps xmm0,xmm0
 xorps xmm4,xmm4
 cmpltps xmm4,xmm0
 movups [eax+{o('scratch',96)}],xmm4
 movups xmm4,[eax+{o('kone')}]
 divps xmm4,xmm0
 andps xmm4,[eax+{o('scratch',96)}]
 movaps xmm0,xmm1
 mulps xmm0,xmm4
 andps xmm0,[eax+{o('scratch',96)}]
 movups xmm1,[eax+{o('scratch',96)}]
 andnps xmm1,[eax+{o('scratch',64)}]
 orps xmm1,xmm0
 movaps xmm0,xmm2
 mulps xmm0,xmm4
 andps xmm0,[eax+{o('scratch',96)}]
 movups xmm2,[eax+{o('scratch',96)}]
 andnps xmm2,[eax+{o('scratch',80)}]
 orps xmm2,xmm0
 movaps xmm0,xmm3
 mulps xmm0,xmm4
 andps xmm0,[eax+{o('scratch',96)}]
 movups xmm4,[eax+{o('scratch',96)}]
 movups [eax+{o('scratch',112)}],xmm3
 andnps xmm4,[eax+{o('scratch',112)}]
 orps xmm0,xmm4
 movaps xmm3,xmm0
10:
 mov ecx,[eax+{o('nl')}]
 test ecx,ecx
 jz 7f
 mov edx,eax
11:
 movaps xmm0,xmm1
 mulps xmm0,[edx+{o('ld',0)}]
 movaps xmm4,xmm2
 mulps xmm4,[edx+{o('ld',16)}]
 addps xmm0,xmm4
 movaps xmm4,xmm3
 mulps xmm4,[edx+{o('ld',32)}]
 addps xmm0,xmm4
 xorps xmm4,xmm4
 cmpltps xmm4,xmm0
 andps xmm0,xmm4
 movaps xmm4,xmm0
 mulps xmm4,[edx+{o('lc',0)}]
 addps xmm5,xmm4
 movaps xmm4,xmm0
 mulps xmm4,[edx+{o('lc',16)}]
 addps xmm6,xmm4
 movaps xmm4,xmm0
 mulps xmm4,[edx+{o('lc',32)}]
 addps xmm7,xmm4
 add edx,64
 dec ecx
 jnz 11b
7:
 xorps xmm1,xmm1
{tobytes('xmm5')}
{tobytes('xmm6')}
{tobytes('xmm7')}
 movups xmm0,[eax+{o('base',48)}]
{tobytes('xmm0')}
 punpcklbw xmm5,xmm7
 punpcklbw xmm6,xmm0
 punpcklbw xmm5,xmm6
 movups [eax+{o('od')}],xmm5
14:
 xorps xmm1,xmm1
 movups xmm0,[eax+{o('scratch',48)}]
{tobytes('xmm0')}
 movaps xmm2,xmm1
 punpcklbw xmm2,xmm0
 movaps xmm3,xmm1
 punpcklbw xmm3,xmm2
 movups [eax+{o('os')}],xmm3
 jmp 9f
6:
 xorps xmm1,xmm1
 movups xmm0,[eax+{o('scratch',48)}]
{tobytes('xmm0')}
 movaps xmm2,xmm1
 punpcklbw xmm2,xmm0
 movaps xmm3,xmm1
 punpcklbw xmm3,xmm2
 movups xmm0,[eax+{o('gdc')}]
 movups [eax+{o('od')}],xmm0
 movups xmm0,[eax+{o('gsc')}]
 pand xmm0,[eax+{o('kmask24')}]
 por xmm0,xmm3
 movups [eax+{o('os')}],xmm0
9:
 pop ebp
 pop edi
 pop esi
 pop ebx
 ret
{EXP}
"""
def assemble(name,text):
 """the function's bytes and a listing of (bytes, mnemonic) by instruction"""
 src=".intel_syntax noprefix\n.text\n.globl "+name+"\n"+text
 with tempfile.TemporaryDirectory() as d:
  s=Path(d)/'k.s';obj=Path(d)/'k.o';s.write_text(src)
  llvm=bool(shutil.which('clang') and shutil.which('llvm-objdump'))
  asm=['clang','--target=i686-linux-gnu','-c'] if llvm else ['as','--32']
  dump='llvm-objdump' if llvm else 'objdump'
  subprocess.run(asm+[str(s),'-o',str(obj)],check=True)
  rel=subprocess.check_output([dump,'-r',str(obj)],text=True)
  if re.search(r'^\s*[0-9a-f]+\s+R_',rel,re.M):sys.exit(name+': relocations in the kernel:\n'+rel)
  flags=['-d','-M','intel'] if llvm else ['-d','-w','-M','intel','--insn-width=16']
  listing=subprocess.check_output([dump,*flags,str(obj)],text=True)
  lines=[]
  for ln in listing.splitlines():
   m=re.match(r'^\s*([0-9a-f]+):\s*((?:[0-9a-f]{2}\s+)+)(.*)',ln)
   if m:lines.append((bytes.fromhex(m.group(2)),m.group(3).strip()))
  binary=Path(d)/'k.bin'
  objcopy=shutil.which('llvm-objcopy') or shutil.which('objcopy')
  subprocess.run([objcopy,'-O','binary','-j','.text',str(obj),str(binary)],check=True)
  if b''.join(b for b,_ in lines)!=binary.read_bytes():sys.exit(name+': listing did not cover .text')
 return lines

def wasm(name,lines):
 out=[f'public {name}',f'{name} proc near']
 for b,mn in lines:out.append(' db '+','.join('0%02Xh'%x for x in b)+' ; '+mn)
 out.append(f'{name} endp');return '\n'.join(out)
def main():
 pos=assemble('_tnk_pos',POS);light=assemble('_tnk_light',LIGHT)
 text='; vx_tnl_sse.asm: the driver\'s T&L group kernels (SSE2) as bytes for wasm. GENERATED by tools/tnl_asm.py: edit that file, not this.\n'\
  '; cdecl, one argument (the tnk_frame of src/vx_d3d_tnl_soa.inc); the sequences are the intrinsics\' there, instruction for instruction.\n'\
  '.386\n.model flat\n.code\n'+wasm('_tnk_pos',pos)+'\n'+wasm('_tnk_light',light)+'\nend\n'
 (ROOT/'src'/'vx_tnl_sse.asm').write_text(text)
 (ROOT/'tests'/'tnl_kernels.s').write_text(".intel_syntax noprefix\n.text\n.globl _tnk_pos\n.globl _tnk_light\n"+POS+LIGHT)
 print('tnk_pos %d bytes, %d instructions; tnk_light %d bytes, %d instructions'%(sum(len(b) for b,_ in pos),len(pos),sum(len(b) for b,_ in light),len(light)))
if __name__=='__main__':main() # .545: importable (tools/hal_asm.py uses assemble and wasm)
