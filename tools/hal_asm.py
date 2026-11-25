#!/usr/bin/env python3
"""hal_asm.py (.545; .767: vxs_check rewritten, vxs_conv8, vxs_mm16, vxs_add16, vxs_eqw added): the HAL's span kernels for the indexed pass-through (src/vx_hal_sse.asm, Open Watcom wasm).

As tools/tnl_asm.py (whose assemble and wasm this uses): Intel syntax, assembled for i686 with no relocations, written out as `db` bytes
with the mnemonic as a comment. Each kernel is cdecl with one argument, the vxs_frame (vxs_max16: the indices and their count) of src/vx_d3d_span.inc (offsets below, checked there
by VXS_OFF under Watcom). tests/test_hal_asm.c runs the bytes against the C reference (vxs_check_c, vxs_conv_c) on random spans.

  python3 tools/hal_asm.py            writes src/vx_hal_sse.asm and tests/hal_kernels.s
"""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from tnl_asm import assemble,wasm
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent # E769: tnl_asm.py has no ROOT (this tool could not run as handed over)
F={'xy':52,'vb':0,'stride':4,'n':8,'w':12,'wstride':16,'coff':20,'smode':24,'soff':28,'t0':32,'v0on':36,'two':40,'t1':44,'v1on':48,
 'kbias':96,'kgu':112,'kz':128,'kw':144,'kone':160,'kneg0':176,'kxlo':192,'kxr':208,'kylo':224,'kyr':240,'kxg':256,'kyg':272,
 'k1':288,'rb1':304,'rb2':320,'ng':336}
def axis(r,lo,rng,g):
 # fail lanes of one axis (GB_IN) or'd into xmm6: out = (a - lo) ^ bias > range ^ bias; the guard's alternative: a negative, a <= 0xBF000000
 return f"""
 movdqu xmm7,[ebp+{F['kbias']}]
 pxor xmm7,{r}
 movdqu xmm1,[ebp+{F['kgu']}]
 pcmpgtd xmm7,xmm1
 movdqa xmm3,{r}
 psrad xmm3,31
 pandn xmm7,xmm3
 movdqu xmm1,[ebp+{F[g]}]
 pand xmm7,xmm1
 movdqu xmm1,[ebp+{F[lo]}]
 movdqa xmm3,{r}
 psubd xmm3,xmm1
 movdqu xmm1,[ebp+{F['kbias']}]
 pxor xmm3,xmm1
 movdqu xmm1,[ebp+{F[rng]}]
 pcmpgtd xmm3,xmm1
 pandn xmm7,xmm3
 por xmm6,xmm7"""
CHECK=f"""
_vxs_check:
 push ebx
 push esi
 push edi
 push ebp
 mov ebp,[esp+20]
 mov esi,[ebp+{F['vb']}]
 mov ebx,[ebp+{F['stride']}]
 mov ecx,[ebp+{F['n']}]
 xor eax,eax
1:
 mov edx,ecx
 sub edx,eax
 cmp edx,4
 jb 9f
 mov edx,esi
 movdqu xmm0,[edx]
 add edx,ebx
 movdqu xmm1,[edx]
 add edx,ebx
 movdqu xmm2,[edx]
 add edx,ebx
 movdqu xmm3,[edx]
 movaps xmm4,xmm0
 unpcklps xmm0,xmm1
 unpckhps xmm4,xmm1
 movaps xmm5,xmm2
 unpcklps xmm2,xmm3
 unpckhps xmm5,xmm3
 movaps xmm1,xmm0
 movlhps xmm0,xmm2
 movhlps xmm2,xmm1
 movaps xmm3,xmm4
 movlhps xmm4,xmm5
 movhlps xmm5,xmm3
 pxor xmm6,xmm6
{axis('xmm0','kxlo','kxr','kxg')}
{axis('xmm2','kylo','kyr','kyg')}
 movdqu xmm1,[ebp+{F['kbias']}]
 movdqa xmm3,xmm4
 pxor xmm3,xmm1
 movdqu xmm1,[ebp+{F['kz']}]
 pcmpgtd xmm3,xmm1
 por xmm6,xmm3
 movdqu xmm1,[ebp+{F['kone']}]
 movdqa xmm3,xmm5
 psubd xmm3,xmm1
 movdqu xmm1,[ebp+{F['kbias']}]
 pxor xmm3,xmm1
 movdqu xmm1,[ebp+{F['kw']}]
 pcmpgtd xmm1,xmm3
 pcmpeqd xmm3,xmm3
 pxor xmm1,xmm3
 por xmm6,xmm1
 movmskps edi,xmm6
 test edi,edi
 jnz 8f
 lea esi,[esi+ebx*4]
 add eax,4
 jmp 1b
8:
 bsf edi,edi
 add eax,edi
9:
 pop ebp
 pop edi
 pop esi
 pop ebx
 ret
"""
def fin(r):
 # a coordinate word not finite as 0 (edx the scratch)
 return f"""
 mov edx,{r}
 and edx,0x7F800000
 cmp edx,0x7F800000
 jne 5f
 xor {r},{r}
5:"""
CONV=f"""
_vxs_conv:
 push ebx
 push esi
 push edi
 push ebp
 mov ebp,[esp+20]
 mov esi,[ebp+{F['vb']}]
 mov edi,[ebp+{F['w']}]
 mov ecx,[ebp+{F['n']}]
 test ecx,ecx
 jz 9f
 movdqu xmm7,[ebp+{F['kneg0']}]
1:
 movdqu xmm0,[esi]
 movdqa xmm1,xmm0
 pcmpeqd xmm1,xmm7
 pandn xmm1,xmm0
 movdqu [edi],xmm1
 mov edx,[ebp+{F['coff']}]
 mov eax,-1
 test edx,edx
 jz 2f
 mov eax,[esi+edx]
2:
 mov [edi+16],eax
 mov edx,[ebp+{F['smode']}]
 xor eax,eax
 cmp edx,1
 jb 3f
 mov eax,0xFF000000
 je 3f
 mov edx,[ebp+{F['soff']}]
 mov eax,[esi+edx]
3:
 mov [edi+20],eax
 xor eax,eax
 xor ebx,ebx
 mov edx,[ebp+{F['t0']}]
 test edx,edx
 jz 4f
 mov eax,[esi+edx]
 cmp dword ptr [ebp+{F['v0on']}],0
 je 6f
 mov ebx,[esi+edx+4]
6:{fin('eax')}{fin('ebx')}
4:
 mov [edi+24],eax
 mov [edi+28],ebx
 cmp dword ptr [ebp+{F['two']}],0
 je 7f
 xor eax,eax
 xor ebx,ebx
 mov edx,[ebp+{F['t1']}]
 test edx,edx
 jz 7f
 mov eax,[esi+edx]
 cmp dword ptr [ebp+{F['v1on']}],0
 je 6f
 mov ebx,[esi+edx+4]
6:{fin('eax')}{fin('ebx')}
7:
 mov [edi+32],eax
 mov [edi+36],ebx
 add esi,[ebp+{F['stride']}]
 add edi,[ebp+{F['wstride']}]
 dec ecx
 jnz 1b
9:
 pop ebp
 pop edi
 pop esi
 pop ebx
 ret
"""
# .767: vxs_convg: vxs_conv with x and y the pair at the frame's xy (the OpenGL ICD's grid words), z's -0 as 0, rhw as it is; the
# layout part is vxs_conv's text. Integer instructions only.
CONVG=f"""
_vxs_convg:
 push ebx
 push esi
 push edi
 push ebp
 mov ebp,[esp+20]
 mov esi,[ebp+{F['vb']}]
 mov edi,[ebp+{F['w']}]
 mov ecx,[ebp+{F['n']}]
 test ecx,ecx
 jz 9f
 push dword ptr [ebp+{F['xy']}]
1:
 mov edx,[esp]
 mov eax,[edx]
 mov ebx,[edx+4]
 add edx,8
 mov [esp],edx
 mov [edi],eax
 mov [edi+4],ebx
 mov eax,[esi+8]
 mov ebx,[esi+12]
 cmp eax,0x80000000
 jne 5f
 xor eax,eax
5:
 mov [edi+8],eax
 mov [edi+12],ebx
 mov edx,[ebp+{F['coff']}]
 mov eax,-1
 test edx,edx
 jz 2f
 mov eax,[esi+edx]
2:
 mov [edi+16],eax
 mov edx,[ebp+{F['smode']}]
 xor eax,eax
 cmp edx,1
 jb 3f
 mov eax,0xFF000000
 je 3f
 mov edx,[ebp+{F['soff']}]
 mov eax,[esi+edx]
3:
 mov [edi+20],eax
 xor eax,eax
 xor ebx,ebx
 mov edx,[ebp+{F['t0']}]
 test edx,edx
 jz 4f
 mov eax,[esi+edx]
 cmp dword ptr [ebp+{F['v0on']}],0
 je 6f
 mov ebx,[esi+edx+4]
6:{fin('eax')}{fin('ebx')}
4:
 mov [edi+24],eax
 mov [edi+28],ebx
 cmp dword ptr [ebp+{F['two']}],0
 je 7f
 xor eax,eax
 xor ebx,ebx
 mov edx,[ebp+{F['t1']}]
 test edx,edx
 jz 7f
 mov eax,[esi+edx]
 cmp dword ptr [ebp+{F['v1on']}],0
 je 6f
 mov ebx,[esi+edx+4]
6:{fin('eax')}{fin('ebx')}
7:
 mov [edi+32],eax
 mov [edi+36],ebx
 add esi,[ebp+{F['stride']}]
 add edi,[ebp+{F['wstride']}]
 dec ecx
 jnz 1b
 add esp,4
9:
 pop ebp
 pop edi
 pop esi
 pop ebx
 ret
"""
MAX16="""
_vxs_max16:
 push esi
 mov esi,[esp+8]
 mov ecx,[esp+12]
 pcmpeqw xmm1,xmm1
 psllw xmm1,15
 movdqa xmm0,xmm1
1:
 cmp ecx,8
 jb 2f
 movdqu xmm2,[esi]
 pxor xmm2,xmm1
 pmaxsw xmm0,xmm2
 add esi,16
 sub ecx,8
 jmp 1b
2:
 pshufd xmm2,xmm0,0x4E
 pmaxsw xmm0,xmm2
 pshufd xmm2,xmm0,0xB1
 pmaxsw xmm0,xmm2
 pshuflw xmm2,xmm0,0xB1
 pmaxsw xmm0,xmm2
 pxor xmm0,xmm1
 movd eax,xmm0
 and eax,0xFFFF
3:
 test ecx,ecx
 jz 9f
 movzx edx,word ptr [esi]
 cmp edx,eax
 jbe 4f
 mov eax,edx
4:
 add esi,2
 dec ecx
 jmp 3b
9:
 pop esi
 ret
"""
CMM16="""
_vxs_cmm16:
 push esi
 push edi
 push ebx
 mov esi,[esp+16]
 mov edi,[esp+20]
 mov ecx,[esp+24]
 pcmpeqw xmm1,xmm1
 psllw xmm1,15
 movdqa xmm0,xmm1
 pcmpeqw xmm3,xmm3
 pxor xmm3,xmm1
1:
 cmp ecx,8
 jb 2f
 movdqu xmm2,[esi]
 movdqu [edi],xmm2
 pxor xmm2,xmm1
 pmaxsw xmm0,xmm2
 pminsw xmm3,xmm2
 add esi,16
 add edi,16
 sub ecx,8
 jmp 1b
2:
 pshufd xmm2,xmm0,0x4E
 pmaxsw xmm0,xmm2
 pshufd xmm2,xmm0,0xB1
 pmaxsw xmm0,xmm2
 pshuflw xmm2,xmm0,0xB1
 pmaxsw xmm0,xmm2
 pxor xmm0,xmm1
 movd eax,xmm0
 and eax,0xFFFF
 pshufd xmm2,xmm3,0x4E
 pminsw xmm3,xmm2
 pshufd xmm2,xmm3,0xB1
 pminsw xmm3,xmm2
 pshuflw xmm2,xmm3,0xB1
 pminsw xmm3,xmm2
 pxor xmm3,xmm1
 movd ebx,xmm3
 and ebx,0xFFFF
3:
 test ecx,ecx
 jz 9f
 movzx edx,word ptr [esi]
 mov [edi],dx
 cmp edx,eax
 jbe 4f
 mov eax,edx
4:
 cmp edx,ebx
 jae 5f
 mov ebx,edx
5:
 add esi,2
 add edi,2
 dec ecx
 jmp 3b
9:
 mov edx,[esp+28]
 mov [edx],ebx
 mov [edx+4],eax
 pop ebx
 pop edi
 pop esi
 ret
"""
# .767: vxs_check a vertex a load. The four tests of a vertex are one compare of its 16 bytes x y z rhw against per-lane constants:
# each is "(word - lo) above range, unsigned" with lo and range by lane: x (xlo, xrange), y (ylo, yrange), z (0, 0x3F800000: z <= 1.0),
# rhw (1, 0x7F7FFFFE: rhw - 1 < 0x7F7FFFFF). k1 = bias - lo (one add gives (word - lo) ^ bias), rb1 = range ^ bias. The guard's
# alternative for x and y (a word from 0x80000000 to 0xBF000000) is one signed compare with rb2 = 0xBF000000 (those words are the most
# negative ones); ng has all bits set in a lane that has no alternative (the guard off for that axis; z and rhw). A lane fails when
# it fails the range and has no alternative that takes it. Four vertices' fail lanes are gathered and tested once; a group with a
# failure is looked through vertex by vertex for the first. The answer is .545's: the leading multiple of four, or the first vertex
# that fails. (.545's kernel transposed four vertices and tested by axis, each constant loaded from the frame: 26 instructions a
# vertex against 10 here.)
def v4(off):
 return f"""
 movdqu xmm0,[edx{off}]
 movdqa xmm1,xmm0
 paddd xmm0,xmm4
 pcmpgtd xmm0,xmm5
 pcmpgtd xmm1,xmm6
 por xmm1,xmm7
 pand xmm0,xmm1
 por xmm3,xmm0"""
CHECK2=f"""
_vxs_check:
 push ebx
 push esi
 push edi
 mov edi,[esp+16]
 mov esi,[edi+{F['vb']}]
 mov ebx,[edi+{F['stride']}]
 mov ecx,[edi+{F['n']}]
 movdqu xmm4,[edi+{F['k1']}]
 movdqu xmm5,[edi+{F['rb1']}]
 movdqu xmm6,[edi+{F['rb2']}]
 movdqu xmm7,[edi+{F['ng']}]
 xor eax,eax
 and ecx,-4
 jz 9f
1:
 pxor xmm3,xmm3
 mov edx,esi{v4('')}
 add edx,ebx{v4('')}
 add edx,ebx{v4('')}
 add edx,ebx{v4('')}
 movmskps edi,xmm3
 test edi,edi
 jnz 8f
 lea esi,[esi+ebx*4]
 add eax,4
 cmp eax,ecx
 jb 1b
 jmp 9f
8:
 lea edx,[eax+4]
7:
 movdqu xmm0,[esi]
 movdqa xmm1,xmm0
 paddd xmm0,xmm4
 pcmpgtd xmm0,xmm5
 pcmpgtd xmm1,xmm6
 por xmm1,xmm7
 pand xmm0,xmm1
 movmskps edi,xmm0
 test edi,edi
 jnz 9f
 add esi,ebx
 inc eax
 cmp eax,edx
 jb 7b
9:
 pop edi
 pop esi
 pop ebx
 ret
"""
# .767: vxs_conv8, vxs_conv for the layout Direct3D 8's pre-transformed vertices have (x y z rhw, diffuse, specular, u v: the colour at
# 16, the specular word at 20, one coordinate pair at 24) into 8-word records: the vertex is 32 bytes copied with two fixes, x y z of
# -0 as +0 (the first 16 bytes, as vxs_conv) and u, v not finite as 0 (the last 8: the exponent bits of lanes 2 and 3 compared with all
# ones; lanes 0 and 1, the colours, compared with 1 after the mask left them 0: never equal). 15 instructions a vertex (vxs_conv: 45,
# its offsets and modes read from the frame for every vertex). The caller takes it only for that layout and a 32-byte record.
CONV8=f"""
_vxs_conv8:
 push esi
 push edi
 mov eax,[esp+12]
 mov esi,[eax+{F['vb']}]
 mov edi,[eax+{F['w']}]
 mov ecx,[eax+{F['n']}]
 mov edx,[eax+{F['stride']}]
 test ecx,ecx
 jz 9f
 movdqu xmm7,[eax+{F['kneg0']}]
 pcmpeqd xmm6,xmm6
 psrld xmm6,24
 pslld xmm6,23
 pslldq xmm6,8
 movdqu xmm5,[eax+{F['kone']}]
 psrldq xmm5,8
 por xmm5,xmm6
1:
 movdqu xmm0,[esi]
 movdqu xmm2,[esi+16]
 movdqa xmm1,xmm0
 pcmpeqd xmm1,xmm7
 pandn xmm1,xmm0
 movdqa xmm3,xmm2
 pand xmm3,xmm6
 pcmpeqd xmm3,xmm5
 pandn xmm3,xmm2
 movdqu [edi],xmm1
 movdqu [edi+16],xmm3
 add esi,edx
 add edi,32
 dec ecx
 jnz 1b
9:
 pop edi
 pop esi
 ret
"""
# .767: vxs_mm16, the smallest and the largest of count 16-bit indices (mm[0], mm[1]; 0xFFFF and 0 for none): vxs_cmm16 without the
# copy. DrawPrimitives2's INDEXEDTRIANGLELIST2 needs both: the largest for the vertex range check (vxs_max16 before), the smallest for
# the run the pass-through takes whole (a C loop in ix_pass before, ten instructions an index in the DLL).
MM16="""
_vxs_mm16:
 push esi
 push ebx
 mov esi,[esp+12]
 mov ecx,[esp+16]
 pcmpeqw xmm1,xmm1
 psllw xmm1,15
 movdqa xmm0,xmm1
 pcmpeqw xmm3,xmm3
 pxor xmm3,xmm1
1:
 cmp ecx,8
 jb 2f
 movdqu xmm2,[esi]
 pxor xmm2,xmm1
 pmaxsw xmm0,xmm2
 pminsw xmm3,xmm2
 add esi,16
 sub ecx,8
 jmp 1b
2:
 pshufd xmm2,xmm0,0x4E
 pmaxsw xmm0,xmm2
 pshufd xmm2,xmm0,0xB1
 pmaxsw xmm0,xmm2
 pshuflw xmm2,xmm0,0xB1
 pmaxsw xmm0,xmm2
 pxor xmm0,xmm1
 movd eax,xmm0
 and eax,0xFFFF
 pshufd xmm2,xmm3,0x4E
 pminsw xmm3,xmm2
 pshufd xmm2,xmm3,0xB1
 pminsw xmm3,xmm2
 pshuflw xmm2,xmm3,0xB1
 pminsw xmm3,xmm2
 pxor xmm3,xmm1
 movd ebx,xmm3
 and ebx,0xFFFF
3:
 test ecx,ecx
 jz 9f
 movzx edx,word ptr [esi]
 cmp edx,eax
 jbe 4f
 mov eax,edx
4:
 cmp edx,ebx
 jae 5f
 mov ebx,edx
5:
 add esi,2
 dec ecx
 jmp 3b
9:
 mov edx,[esp+20]
 mov [edx],ebx
 mov [edx+4],eax
 pop ebx
 pop esi
 ret
"""
# .767: vxs_add16, count 16-bit indices copied with gain added to each (the low 16 bits of each sum): a run's source indices made the
# packet's (ix_pass: two an addition in C before, eleven instructions a pair in the DLL). Eight a step; nothing written past the count.
ADD16="""
_vxs_add16:
 push esi
 push edi
 mov edi,[esp+12]
 mov esi,[esp+16]
 mov ecx,[esp+20]
 mov edx,[esp+24]
 movd xmm1,edx
 pshuflw xmm1,xmm1,0
 pshufd xmm1,xmm1,0
1:
 cmp ecx,8
 jb 2f
 movdqu xmm0,[esi]
 paddw xmm0,xmm1
 movdqu [edi],xmm0
 add esi,16
 add edi,16
 sub ecx,8
 jmp 1b
2:
 test ecx,ecx
 jz 9f
 movzx eax,word ptr [esi]
 add eax,edx
 mov [edi],ax
 add esi,2
 add edi,2
 dec ecx
 jmp 2b
9:
 pop edi
 pop esi
 ret
"""
# .767: vxs_eqw, 1 when n 32-bit words at a and at b are the same (the packets' state ranges compared for the deltas and the texture
# runs: a word a step in C before). Eight words a step, their compares gathered and tested once.
EQW="""
_vxs_eqw:
 push esi
 push edi
 mov esi,[esp+12]
 mov edi,[esp+16]
 mov ecx,[esp+20]
1:
 cmp ecx,8
 jb 2f
 movdqu xmm0,[esi]
 movdqu xmm1,[edi]
 movdqu xmm2,[esi+16]
 movdqu xmm3,[edi+16]
 pcmpeqd xmm0,xmm1
 pcmpeqd xmm2,xmm3
 pand xmm0,xmm2
 pmovmskb eax,xmm0
 cmp eax,0xFFFF
 jne 8f
 add esi,32
 add edi,32
 sub ecx,8
 jmp 1b
2:
 test ecx,ecx
 jz 7f
 mov eax,[esi]
 cmp eax,[edi]
 jne 8f
 add esi,4
 add edi,4
 dec ecx
 jmp 2b
7:
 mov eax,1
 pop edi
 pop esi
 ret
8:
 xor eax,eax
 pop edi
 pop esi
 ret
"""
def main():
 # .767: _vxs_check is CHECK2 (CHECK, .545's, is kept above for reference and is no longer assembled)
 ks=[('_vxs_check',CHECK2),('_vxs_conv',CONV),('_vxs_max16',MAX16),('_vxs_cmm16',CMM16),('_vxs_conv8',CONV8),('_vxs_mm16',MM16),('_vxs_add16',ADD16),('_vxs_eqw',EQW),('_vxs_convg',CONVG)]
 asm=[(n,assemble(n,t)) for n,t in ks]
 text='; vx_hal_sse.asm: the HAL\'s span kernels for the indexed pass-through (SSE2) as bytes for wasm. GENERATED by tools/hal_asm.py: edit that file, not this.\n'\
  '; cdecl, one argument (the vxs_frame of src/vx_d3d_span.inc); the C reference there (vxs_check_c, vxs_conv_c) gives the same results.\n'\
  '.386\n.model flat\n.code\n'+'\n'.join(wasm(n,l) for n,l in asm)+'\nend\n'
 (ROOT/'src'/'vx_hal_sse.asm').write_text(text)
 (ROOT/'tests'/'hal_kernels.s').write_text(".intel_syntax noprefix\n.text\n"+''.join('.globl '+n+'\n' for n,_ in ks)+''.join(t for _,t in ks))
 # the C reference for tests/test_hal_asm.c: the kernels' part of src/vx_d3d_span.inc as it is
 inc=(ROOT/'src'/'vx_d3d_span.inc').read_text(encoding='latin-1');a=inc.index('/* VXS KERNELS BEGIN */');b=inc.index('/* VXS KERNELS END */')+len('/* VXS KERNELS END */')
 (ROOT/'tests'/'hal_kernels_c.inc').write_text(inc[a:b]+'\n',encoding='latin-1')
 for n,l in asm:print('%s %d bytes, %d instructions'%(n[1:],sum(len(b) for b,_ in l),len(l)))
if __name__=='__main__':main()
