#!/usr/bin/env python3
"""tnl_asm.py (.741): the driver's T&L group kernels for the board (vx_tnl_sse.asm, Open Watcom wasm).

The HAL is built for the 486 and wasm refuses SSE mnemonics, so the kernels are assembled here
(i686, flat, no relocations) and written as `db` bytes. Each is cdecl with one argument, the tnk_frame
(vx_d3d_tnl_soa.inc). _tnk_pos, _tnk_light, _tnk_light_k, _tnk_plight and _tnk_vcodes are the
rescheduled sequences below: same products and the same association as .740, two dots in flight,
the matrix and the normalized normal kept in registers. _tnk_light_k is _tnk_light without the
normal transform (it loads nv). _tnk_spec, _tnk_cv and _tnk_splight stay the clang sequences of
the C in vx_d3d_tnl_soa.inc: regenerated when clang is on PATH, otherwise copied out of the
asm already written so a run here cannot put the old position and light kernels back.

  python3 tnl_asm.py
  python3 tools/tnl_asm.py
"""
import subprocess,sys,re,tempfile,shutil
from pathlib import Path
HERE=Path(__file__).resolve().parent
SRC=HERE if (HERE/'vx_d3d_tnl_soa.inc').exists() else HERE.parent/'src'
# board tnk_frame offsets the kernels were written against (TNK_OFF in vx_d3d_tnl_soa.inc)
F={'m':0,'vp':256,'e':384,'nm':640,'ld':784,'lc':1296,'base':1808,'fog':1872,'nl':1936,'lit':1940,'fogmode':1944,'rfog':1948,'nnorm':1952,'haveN':1956,
 'stride':1960,'n':1964,'first':1968,'reuse_diffuse':1972,'eye_z_ready':1976,'gx':1984,'gy':2000,'gz':2016,'gnx':2032,'gny':2048,'gnz':2064,'gdc':2080,'gsc':2096,'od':2112,'os':2128,
 'k255':2144,'kone':2160,'khalf':2176,'kl2e':2192,'kexpc':2208,'kneg80':2336,'kinf':2352,'ksign':2368,'k127':2384,'kmask24':2400,'kb':2416,'k7f':2512,
 'sx':2528,'sy':2532,'sz':2536,'cx':2540,'cy':2544,'cz':2548,'cw':2552,'rw':2556,'cc':2560,'rec':2564,'scratch':2576,
 'npl':2704,'pp':2720,'pr':3104,'pdc':3616,'pac':4000,'vc':8980,'vacc':9008,'kvc':9024,'nv':9088,'ev':9136,'nready':9184}
def o(name,i=0):return F[name]+i

LIGHT = r"""_tnk_light:
 push ebx
 push esi
 push edi
 push ebp
 mov eax,[esp+20]
 movups xmm4,[eax+2160]
 mov esi,[eax+1944]
 test esi,esi
 jz 3f
 cmp dword ptr [eax+1976],0
 je 13f
 movups xmm0,[eax+2016]
 movups xmm1,[eax+2368]
 andnps xmm1,xmm0
 jmp 2f
13:
 movups xmm5,[eax+1984]
 movups xmm6,[eax+2000]
 movups xmm7,[eax+2016]

 movups xmm0,[eax+416]
 mulps xmm0,xmm5
 movups xmm1,[eax+480]
 mulps xmm1,xmm6
 addps xmm0,xmm1
 movups xmm1,[eax+544]
 mulps xmm1,xmm7
 addps xmm0,xmm1
 movups xmm1,[eax+608]
 addps xmm0,xmm1
 cmp dword ptr [eax+1948],0
 jne 12f
 movups xmm1,[eax+2368]
 andnps xmm1,xmm0
 jmp 2f
12:
 mulps xmm0,xmm0
 movaps xmm4,xmm0

 movups xmm0,[eax+384]
 mulps xmm0,xmm5
 movups xmm1,[eax+448]
 mulps xmm1,xmm6
 addps xmm0,xmm1
 movups xmm1,[eax+512]
 mulps xmm1,xmm7
 addps xmm0,xmm1
 movups xmm1,[eax+576]
 addps xmm0,xmm1
 mulps xmm0,xmm0
 movaps xmm3,xmm0

 movups xmm0,[eax+400]
 mulps xmm0,xmm5
 movups xmm1,[eax+464]
 mulps xmm1,xmm6
 addps xmm0,xmm1
 movups xmm1,[eax+528]
 mulps xmm1,xmm7
 addps xmm0,xmm1
 movups xmm1,[eax+592]
 addps xmm0,xmm1
 mulps xmm0,xmm0
 addps xmm3,xmm0
 addps xmm3,xmm4
 sqrtps xmm1,xmm3
2:
 cmp esi,3
 jne 4f
 movups xmm4,[eax+1888]
 subps xmm4,xmm1
 divps xmm4,[eax+1904]
 jmp 3f
4:
 movups xmm0,[eax+1872]
 mulps xmm0,xmm1
 cmp esi,1
 je 5f
 mulps xmm0,xmm0
5:
 xorps xmm2,xmm2
 subps xmm2,xmm0
 call 8f
3:
 movups [eax+2624],xmm4
 cmp dword ptr [eax+1940],0
 je 6f
 cmp dword ptr [eax+1972],0
 jne 14f
 cmp dword ptr [eax+1956],0
 je 15f
 movups xmm0,[eax+2032]
 movups xmm1,[eax+2048]
 movups xmm2,[eax+2064]
 movups xmm3,[eax+640]
 movups xmm4,[eax+656]
 mulps xmm3,xmm0
 mulps xmm4,xmm0
 movups xmm5,[eax+688]
 movups xmm6,[eax+704]
 mulps xmm5,xmm1
 mulps xmm6,xmm1
 addps xmm3,xmm5
 addps xmm4,xmm6
 movups xmm5,[eax+736]
 movups xmm6,[eax+752]
 mulps xmm5,xmm2
 mulps xmm6,xmm2
 addps xmm3,xmm5
 addps xmm4,xmm6
 movups xmm5,[eax+672]
 mulps xmm5,xmm0
 movups xmm6,[eax+720]
 mulps xmm6,xmm1
 addps xmm5,xmm6
 movups xmm6,[eax+768]
 mulps xmm6,xmm2
 addps xmm5,xmm6
 movaps xmm1,xmm3
 movaps xmm2,xmm4
 movaps xmm3,xmm5
 cmp dword ptr [eax+1952],0
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
 xorps xmm6,xmm6
 cmpltps xmm6,xmm0
 movups xmm7,[eax+2160]
 divps xmm7,xmm0
 andps xmm7,xmm6
 movaps xmm0,xmm1
 mulps xmm0,xmm7
 andps xmm0,xmm6
 movaps xmm4,xmm6
 andnps xmm4,xmm1
 orps xmm0,xmm4
 movaps xmm4,xmm2
 mulps xmm4,xmm7
 andps xmm4,xmm6
 movaps xmm5,xmm6
 andnps xmm5,xmm2
 orps xmm4,xmm5
 movaps xmm5,xmm3
 mulps xmm5,xmm7
 andps xmm5,xmm6
 movaps xmm7,xmm6
 andnps xmm7,xmm3
 orps xmm5,xmm7
 movaps xmm1,xmm0
 movaps xmm2,xmm4
 movaps xmm3,xmm5
10:
 movups xmm5,[eax+1808]
 movups xmm6,[eax+1824]
 movups xmm7,[eax+1840]
 mov ecx,[eax+1936]
 test ecx,ecx
 jz 7f
 mov edx,eax
11:
 movaps xmm0,xmm1
 mulps xmm0,[edx+784]
 movaps xmm4,xmm2
 mulps xmm4,[edx+800]
 addps xmm0,xmm4
 movaps xmm4,xmm3
 mulps xmm4,[edx+816]
 addps xmm0,xmm4
 xorps xmm4,xmm4
 cmpltps xmm4,xmm0
 andps xmm0,xmm4
 movaps xmm4,xmm0
 mulps xmm4,[edx+1296]
 addps xmm5,xmm4
 movaps xmm4,xmm0
 mulps xmm4,[edx+1312]
 addps xmm6,xmm4
 movaps xmm4,xmm0
 mulps xmm4,[edx+1328]
 addps xmm7,xmm4
 add edx,64
 dec ecx
 jnz 11b
 jmp 7f
15:
 movups xmm5,[eax+1808]
 movups xmm6,[eax+1824]
 movups xmm7,[eax+1840]
7:
 xorps xmm1,xmm1

 mulps xmm5,[eax+2144]
 cvtps2dq xmm5,xmm5
 packssdw xmm5,xmm5
 packuswb xmm5,xmm1

 mulps xmm6,[eax+2144]
 cvtps2dq xmm6,xmm6
 packssdw xmm6,xmm6
 packuswb xmm6,xmm1

 mulps xmm7,[eax+2144]
 cvtps2dq xmm7,xmm7
 packssdw xmm7,xmm7
 packuswb xmm7,xmm1
 movups xmm0,[eax+1856]

 mulps xmm0,[eax+2144]
 cvtps2dq xmm0,xmm0
 packssdw xmm0,xmm0
 packuswb xmm0,xmm1
 punpcklbw xmm5,xmm7
 punpcklbw xmm6,xmm0
 punpcklbw xmm5,xmm6
 movups [eax+2112],xmm5
14:
 xorps xmm1,xmm1
 movups xmm0,[eax+2624]

 mulps xmm0,[eax+2144]
 cvtps2dq xmm0,xmm0
 packssdw xmm0,xmm0
 packuswb xmm0,xmm1
 movaps xmm2,xmm1
 punpcklbw xmm2,xmm0
 movaps xmm3,xmm1
 punpcklbw xmm3,xmm2
 movups [eax+2128],xmm3
 jmp 9f
6:
 xorps xmm1,xmm1
 movups xmm0,[eax+2624]

 mulps xmm0,[eax+2144]
 cvtps2dq xmm0,xmm0
 packssdw xmm0,xmm0
 packuswb xmm0,xmm1
 movaps xmm2,xmm1
 punpcklbw xmm2,xmm0
 movaps xmm3,xmm1
 punpcklbw xmm3,xmm2
 movups xmm0,[eax+2080]
 movups [eax+2112],xmm0
 movups xmm0,[eax+2096]
 pand xmm0,[eax+2400]
 por xmm0,xmm3
 movups [eax+2128],xmm0
9:
 pop ebp
 pop edi
 pop esi
 pop ebx
 ret

8:
 movaps xmm3,xmm2
 movups xmm0,[eax+2336]
 cmpltps xmm0,xmm2
 xorps xmm4,xmm4
 movaps xmm1,xmm2
 cmpltps xmm1,xmm4
 maxps xmm2,[eax+2336]
 minps xmm2,xmm4
 mulps xmm2,[eax+2192]
 cvttps2dq xmm4,xmm2
 cvtdq2ps xmm5,xmm4
 movaps xmm6,xmm2
 cmpltps xmm6,xmm5
 psrld xmm6,31
 psubd xmm4,xmm6
 cvtdq2ps xmm5,xmm4
 movaps xmm6,xmm2
 subps xmm6,xmm5
 movups xmm5,[eax+2304]
 mulps xmm5,xmm6
 addps xmm5,[eax+2288]
 mulps xmm5,xmm6
 addps xmm5,[eax+2272]
 mulps xmm5,xmm6
 addps xmm5,[eax+2256]
 mulps xmm5,xmm6
 addps xmm5,[eax+2240]
 mulps xmm5,xmm6
 addps xmm5,[eax+2224]
 mulps xmm5,xmm6
 addps xmm5,[eax+2208]
 paddd xmm4,[eax+2384]
 pslld xmm4,23
 mulps xmm5,xmm4
 andps xmm5,xmm0
 andps xmm5,xmm1
 movups xmm4,[eax+2160]
 xorps xmm0,xmm0
 movaps xmm7,xmm0
 cmpleps xmm7,xmm3
 andps xmm4,xmm7
 orps xmm5,xmm4
 movaps xmm4,xmm5
 ret
"""
# .743: POS_O, the hand-written tnk_pos of .544-.741 (195 instructions), kept for out-of-order processors: T742, VX900 (Nano): the rewritten one 19% slower; VX855 (C7): 9% faster
PLIGHT = r"""_tnk_plight:
 push ebx
 push esi
 push edi
 push ebp
 mov eax,[esp+20]
 movups xmm5,[eax+1984]
 movups xmm6,[eax+2000]
 movups xmm7,[eax+2016]
 movups xmm0,[eax+384]
 movups xmm1,[eax+400]
 mulps xmm0,xmm5
 mulps xmm1,xmm5
 movups xmm2,[eax+448]
 movups xmm3,[eax+464]
 mulps xmm2,xmm6
 mulps xmm3,xmm6
 addps xmm0,xmm2
 addps xmm1,xmm3
 movups xmm2,[eax+512]
 movups xmm3,[eax+528]
 mulps xmm2,xmm7
 mulps xmm3,xmm7
 addps xmm0,xmm2
 addps xmm1,xmm3
 movups xmm2,[eax+576]
 movups xmm3,[eax+592]
 addps xmm0,xmm2
 addps xmm1,xmm3
 movups [eax+2576],xmm0
 movups [eax+2592],xmm1
 movups xmm0,[eax+416]
 mulps xmm0,xmm5
 movups xmm2,[eax+480]
 mulps xmm2,xmm6
 addps xmm0,xmm2
 movups xmm2,[eax+544]
 mulps xmm2,xmm7
 addps xmm0,xmm2
 movups xmm2,[eax+608]
 addps xmm0,xmm2
 movups [eax+2608],xmm0
 xorps xmm1,xmm1
 xorps xmm2,xmm2
 xorps xmm3,xmm3
 cmp dword ptr [eax+1956],0
 je 10f
 movups xmm0,[eax+2032]
 movups xmm1,[eax+2048]
 movups xmm2,[eax+2064]
 movups xmm3,[eax+640]
 movups xmm4,[eax+656]
 mulps xmm3,xmm0
 mulps xmm4,xmm0
 movups xmm5,[eax+688]
 movups xmm6,[eax+704]
 mulps xmm5,xmm1
 mulps xmm6,xmm1
 addps xmm3,xmm5
 addps xmm4,xmm6
 movups xmm5,[eax+736]
 movups xmm6,[eax+752]
 mulps xmm5,xmm2
 mulps xmm6,xmm2
 addps xmm3,xmm5
 addps xmm4,xmm6
 movups xmm5,[eax+672]
 mulps xmm5,xmm0
 movups xmm6,[eax+720]
 mulps xmm6,xmm1
 addps xmm5,xmm6
 movups xmm6,[eax+768]
 mulps xmm6,xmm2
 addps xmm5,xmm6
 movaps xmm1,xmm3
 movaps xmm2,xmm4
 movaps xmm3,xmm5
 cmp dword ptr [eax+1952],0
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
 xorps xmm6,xmm6
 cmpltps xmm6,xmm0
 movups xmm7,[eax+2160]
 divps xmm7,xmm0
 andps xmm7,xmm6
 movaps xmm0,xmm1
 mulps xmm0,xmm7
 andps xmm0,xmm6
 movaps xmm4,xmm6
 andnps xmm4,xmm1
 orps xmm0,xmm4
 movaps xmm4,xmm2
 mulps xmm4,xmm7
 andps xmm4,xmm6
 movaps xmm5,xmm6
 andnps xmm5,xmm2
 orps xmm4,xmm5
 movaps xmm5,xmm3
 mulps xmm5,xmm7
 andps xmm5,xmm6
 movaps xmm7,xmm6
 andnps xmm7,xmm3
 orps xmm5,xmm7
 movaps xmm1,xmm0
 movaps xmm2,xmm4
 movaps xmm3,xmm5
10:
 movups [eax+2624],xmm1
 movups [eax+2640],xmm2
 movups [eax+2656],xmm3
 movups xmm5,[eax+8848]
 movups xmm6,[eax+8864]
 movups xmm7,[eax+8880]
 mov ecx,[eax+2704]
 test ecx,ecx
 jz 8f
 mov edx,eax
 mov ebx,eax
 mov ebp,1
1:
 test dword ptr [eax+9192],ebp
 jnz 2f
 movups xmm1,[edx+2720]
 subps xmm1,[eax+2576]
 movups xmm2,[edx+2736]
 subps xmm2,[eax+2592]
 movups xmm3,[edx+2752]
 subps xmm3,[eax+2608]
 movaps xmm0,xmm1
 mulps xmm0,xmm1
 movaps xmm4,xmm2
 mulps xmm4,xmm2
 addps xmm0,xmm4
 movaps xmm4,xmm3
 mulps xmm4,xmm3
 addps xmm0,xmm4
 movups [eax+2672],xmm0
 rsqrtps xmm4,xmm0
 mulps xmm0,xmm4
 mulps xmm0,xmm4
 subps xmm0,[eax+2160]
 mulps xmm0,[eax+2176]
 mulps xmm0,xmm4
 subps xmm4,xmm0
 xorps xmm0,xmm0
 cmpltps xmm0,[eax+2672]
 movups [eax+2688],xmm0
 movups xmm0,[eax+2672]
 mulps xmm0,xmm4
 andps xmm0,[eax+2688]
 movups [eax+2672],xmm0
 cmpleps xmm0,[ebx+3104]
 movmskps esi,xmm0
 test esi,esi
 jz 2f
 movaps xmm0,xmm1
 mulps xmm0,xmm4
 xorps xmm0,xmm1
 andps xmm0,[eax+2688]
 xorps xmm1,xmm0
 movaps xmm0,xmm2
 mulps xmm0,xmm4
 xorps xmm0,xmm2
 andps xmm0,[eax+2688]
 xorps xmm2,xmm0
 movaps xmm0,xmm3
 mulps xmm0,xmm4
 xorps xmm0,xmm3
 andps xmm0,[eax+2688]
 xorps xmm3,xmm0
 movups xmm0,[eax+2672]
 movups xmm4,[ebx+3136]
 mulps xmm4,xmm0
 addps xmm4,[ebx+3120]
 mulps xmm0,[ebx+3152]
 mulps xmm0,[eax+2672]
 addps xmm4,xmm0
 xorps xmm0,xmm0
 cmpltps xmm0,xmm4
 movups [eax+2688],xmm0
 rcpps xmm0,xmm4
 mulps xmm4,xmm0
 subps xmm4,[eax+2160]
 mulps xmm4,xmm0
 subps xmm0,xmm4
 xorps xmm0,[eax+2160]
 andps xmm0,[eax+2688]
 xorps xmm0,[eax+2160]
 movups xmm4,[eax+2672]
 cmpleps xmm4,[ebx+3104]
 andps xmm0,xmm4
 test dword ptr [eax+8920],ebp
 jz 3f
 movaps xmm4,xmm0
 mulps xmm4,[edx+4000]
 addps xmm5,xmm4
 movaps xmm4,xmm0
 mulps xmm4,[edx+4016]
 addps xmm6,xmm4
 movaps xmm4,xmm0
 mulps xmm4,[edx+4032]
 addps xmm7,xmm4
3:
 cmp dword ptr [eax+1956],0
 je 2f
 mulps xmm1,[eax+2624]
 mulps xmm2,[eax+2640]
 addps xmm1,xmm2
 mulps xmm3,[eax+2656]
 addps xmm1,xmm3
 xorps xmm4,xmm4
 cmpltps xmm4,xmm1
 andps xmm1,xmm4
 mulps xmm1,xmm0
 movaps xmm4,xmm1
 mulps xmm4,[edx+3616]
 addps xmm5,xmm4
 movaps xmm4,xmm1
 mulps xmm4,[edx+3632]
 addps xmm6,xmm4
 movaps xmm4,xmm1
 mulps xmm4,[edx+3648]
 addps xmm7,xmm4
2:
 add edx,48
 add ebx,64
 add ebp,ebp
 dec ecx
 jnz 1b
8:
 cmp dword ptr [eax+1956],0
 je 9f
 movups xmm0,[eax+2624]
 movups [eax+9088],xmm0
 movups xmm0,[eax+2640]
 movups [eax+9104],xmm0
 movups xmm0,[eax+2656]
 movups [eax+9120],xmm0
 mov dword ptr [eax+9184],1
9:
 movups [eax+1808],xmm5
 movups [eax+1824],xmm6
 movups [eax+1840],xmm7
 movups xmm0,[eax+8896]
 movups [eax+1856],xmm0
 pop ebp
 pop edi
 pop esi
 pop ebx
 ret
"""
VCODES = r"""
_tnk_vcodes:
 push esi
 push edi
 mov eax,[esp+12]
 mov ecx,[eax+1968]
 mov esi,[eax+1964]
 add esi,ecx
 mov edi,[eax+8980]
 cmp ecx,esi
 jae 9f
1:
 mov edx,[eax+2540]
 movups xmm0,[edx+ecx*4]
 mov edx,[eax+2544]
 movups xmm1,[edx+ecx*4]
 mov edx,[eax+2552]
 movups xmm2,[edx+ecx*4]
 movups xmm3,[eax+2176]
 mulps xmm2,xmm3
 xorps xmm3,xmm3
 subps xmm3,xmm2
 movaps xmm4,xmm0
 cmpltps xmm4,xmm3
 andps xmm4,[eax+9024]
 movaps xmm5,xmm2
 cmpltps xmm5,xmm0
 andps xmm5,[eax+9040]
 orps xmm4,xmm5
 movaps xmm5,xmm1
 cmpltps xmm5,xmm3
 andps xmm5,[eax+9056]
 orps xmm4,xmm5
 movaps xmm5,xmm2
 cmpltps xmm5,xmm1
 andps xmm5,[eax+9072]
 orps xmm4,xmm5
 packssdw xmm4,xmm4
 packuswb xmm4,xmm4
 movd [edi+ecx],xmm4
 add ecx,4
 cmp ecx,esi
 jb 1b
9:
 pop edi
 pop esi
 ret
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
def compile_c(name,kernel,sig='tnk_frame *f',args='f',ret='void'):
 """.651: a kernel compiled from its C in src/vx_d3d_tnl_soa.inc (the TNK KERNELS parts) by clang for i686 with SSE2, as the bytes of
 one function: no relocations (no constant in memory, no call out), the stack realigned in its prologue (the board's callers keep 4
 bytes). The host's build of the same C runs the same operations in the same order (clang may swap an addition's or a product's two
 operands, which gives the same bits), so tests/test_tnl_asm.c still compares the board's bytes with the host's C."""
 src=(SRC/'vx_d3d_tnl_soa.inc').read_text()
 part=lambda n:src[src.index('/* TNK KERNELS BEGIN %d'%n):src.index('/* TNK KERNELS END %d */'%n)]
 c='#include <stdint.h>\n#include <stddef.h>\n'+part(1)+part(2)+'\n__attribute__((used,noinline)) %s %s(%s){%s%s(%s);}\n'%(ret,name,sig,'' if ret=='void' else 'return ',kernel,args)
 with tempfile.TemporaryDirectory() as d:
  cf=Path(d)/'k.c';obj=Path(d)/'k.o';cf.write_text(c)
  subprocess.run(['clang','--target=i686-linux-gnu','-O2','-msse2','-mstackrealign','-fno-pic','-ffreestanding','-fno-builtin','-fno-stack-protector',
   '-fno-asynchronous-unwind-tables','-fno-unwind-tables','-fno-jump-tables','-w','-c',str(cf),'-o',str(obj)],check=True)
  rel=subprocess.check_output(['llvm-objdump','-r',str(obj)],text=True)
  if re.search(r'^\s*[0-9a-f]+\s+R_',rel,re.M):sys.exit(name+': relocations in the compiled kernel:\n'+rel)
  syms=subprocess.check_output(['llvm-objdump','-t',str(obj)],text=True)
  m=re.search(r'^([0-9a-f]+)\s+g\s+F\s+\.text\s+([0-9a-f]+)\s+'+re.escape(name)+r'$',syms,re.M)
  if not m or int(m.group(1),16)!=0:sys.exit(name+': not the only function at the start of .text:\n'+syms)
  listing=subprocess.check_output(['llvm-objdump','-d','-M','intel','--no-show-raw-insn',str(obj)],text=True)
  if re.search(r'\bcall\b',listing):sys.exit(name+': a call in the compiled kernel')
  listing=subprocess.check_output(['llvm-objdump','-d','-M','intel',str(obj)],text=True)
  lines=[]
  for ln in listing.splitlines():
   mm=re.match(r'^\s*([0-9a-f]+):\s*((?:[0-9a-f]{2}\s)+)\s*(.*)',ln)
   if mm:lines.append((bytes.fromhex(mm.group(2)),mm.group(3).split('#')[0].strip()))
  binary=Path(d)/'k.bin'
  subprocess.run([shutil.which('llvm-objcopy') or shutil.which('objcopy'),'-O','binary','-j','.text',str(obj),str(binary)],check=True)
  if b''.join(b for b,_ in lines)!=binary.read_bytes():sys.exit(name+': listing did not cover .text')
 return lines
def listed(name,code):
 """.746: a kernel kept as bytes (tools/tnl_win.py), listed by llvm-objdump for the .asm's comments; without llvm-objdump one row, no listing."""
 if not (shutil.which('clang') and shutil.which('llvm-objdump')):return [(code,'(no listing)')]
 with tempfile.TemporaryDirectory() as d:
  sf=Path(d)/'k.s';obj=Path(d)/'k.o';sf.write_text('.text\n'+''.join(' .byte '+','.join('0x%02x'%x for x in code[i:i+16])+'\n' for i in range(0,len(code),16)))
  subprocess.run(['clang','--target=i686-linux-gnu','-c',str(sf),'-o',str(obj)],check=True)
  listing=subprocess.check_output(['llvm-objdump','-d','-M','intel',str(obj)],text=True)
 lines=[]
 for ln in listing.splitlines():
  mm=re.match(r'^\s*([0-9a-f]+):\s*((?:[0-9a-f]{2}\s)+)\s*(.*)',ln)
  if mm:lines.append((bytes.fromhex(mm.group(2)),mm.group(3).split('#')[0].strip()))
 if b''.join(b for b,_ in lines)!=code or any(re.match(r'call|int\b|out|in\b|cli|sti|hlt',m) for _,m in lines):sys.exit(name+': the listing does not cover the bytes, or an instruction that has no place in a kernel')
 return lines
def gas_bytes(name,lines):
 return '%s:\n'%name+''.join(' .byte '+','.join('0x%02x'%x for x in b)+' # '+mn+'\n' for b,mn in lines)

def kept(prev,name):
 a=prev.index('public %s\n'%name)
 b=prev.index('%s endp'%name,a)+len('%s endp'%name)
 return prev[a:b]
PIN={'_tnk_light_h':46,'_tnk_plight':31,'_tnk_vcodes':7,'_tnk_pos':51,'_tnk_pos_o':37,'_tnk_light':30,'_tnk_light_k':40,'_tnk_splight':47,'_tnk_spec':38,'_tnk_cv':45,'_tnk_bounds':62} # .750: T748's places (VXDDRAW.map, address mod 64)
def main():
 if F['nv']!=9088 or F['nready']!=9184 or F['scratch']!=2576 or F['khalf']!=2176:
  sys.exit('tnk_frame offsets moved; the kernels below are baked to .741')
 # .746: the kernels VXKBENCH chose on the boards (tools/tnl_win.py, bytes): _tnk_pos (C7 family), _tnk_pos_o (every other processor), _tnk_light, _tnk_light_k.
 # The .742 rewritten tnk_pos and the .741 tnk_light_k are deleted (slower on both boards). _tnk_pos_s is the .544 kernel, kept for the VX900's in-driver
 # A/B (ACCEL_D3D_TNL=2) until T746 settles it; _tnk_light_h is the .742 kernel, which the Nano runs for two lights or fog (pack4 2.5% / 5.2% slower there).
 hand=[('_tnk_light_h',LIGHT.replace('_tnk_light:','_tnk_light_h:')),('_tnk_plight',PLIGHT),('_tnk_vcodes',VCODES)]
 import tnl_win
 won=[('_tnk_pos','pos_c7'),('_tnk_pos_o','pos_o'),('_tnk_light','light'),('_tnk_light_k','light_k'),('_tnk_splight','splight')] # .747: the two triangle-cull kernels under test
 parts=[]; counts=[]
 for name,text in hand:
  rows=assemble(name,text)
  counts.append((name,len(rows),sum(len(b) for b,_ in rows)))
  parts.append(wasm(name,rows))
 for name,key in won:
  rows=listed(name,bytes.fromhex(tnl_win.WIN[key]))
  counts.append((name,len(rows),sum(len(b) for b,_ in rows)))
  parts.append(wasm(name,rows))
 asm=SRC/'vx_tnl_sse.asm'
 prev=asm.read_text() if asm.exists() else ''
 soa=SRC/'vx_d3d_tnl_soa.inc'
 clang=bool(shutil.which('clang') and shutil.which('llvm-objdump') and soa.exists())
 for name,kernel,sig,args in (
   ('_tnk_spec','tnk_spec','tnk_frame *f','f'),
   ('_tnk_cv','tnk_cv','tnk_frame *f,float (*b)[4]','f,b'),
   ('_tnk_bounds','tnk_bounds','tnk_frame *f,const uint8_t *const *sp,const uint16_t *list,uint32_t n','f,sp,list,n')): # .741F: the tile's eye-space box
  if clang:
   rows=compile_c(name,kernel,sig,args)
   counts.append((name,len(rows),sum(len(b) for b,_ in rows)))
   parts.append(wasm(name,rows))
  else:
   if 'public %s\n'%name not in prev:sys.exit(name+': no clang, and no previous bytes to keep')
   counts.append((name,'kept',0)); parts.append(kept(prev,name))
 hdr=('; vx_tnl_sse.asm: T&L group kernels (SSE2) as bytes for wasm.\n'
      '; _tnk_pos, _tnk_light, _tnk_light_k, _tnk_plight, _tnk_vcodes are the rescheduled kernels\n'
      '; (same operations, same association, fewer spills, two dot-products in flight).\n'
      '; _tnk_spec, _tnk_cv, _tnk_splight are the previous clang sequences: same bits as vx_d3d_tnl_soa.inc.\n'
      '.386\n_TEXT segment page public use32 \'CODE\'\nassume cs:_TEXT\n')
 # .750: every kernel at a fixed place within a 64-byte line (PIN), in a segment aligned to 256 bytes, so a change elsewhere in the DLL cannot
 # move them. T749, VX900 (Nano): deleting other kernels moved tnk_pos_o from 37 to 11 within its line and it took 49% longer in the frame
 # (1249.6 -> 1856.6 ms over the run; pursuit 2.7% slower) with not one byte of it changed. PIN holds T748's places until VXKBENCH's sweep picks better.
 body=[];off=0;sizes=[]
 for part,(name,_,nb) in zip(parts,counts):
  if not nb:nb=sum(len(x.split(',')) for x in re.findall(r'^ db ([^;\n]*)',part,re.M))
  pad=(PIN.get(name,0)-off)%64
  if pad:body.append('\n'.join(' db '+','.join(['0CCh']*min(16,pad-k)) for k in range(0,pad,16)));off+=pad
  body.append(part);off+=nb;sizes.append((name,nb))
 text=hdr+'\n'.join(body)+'\n_TEXT ends\nend\n'
 # .751: the T&L's working arrays in a segment of their own, each on a 64-byte line (Watcom gives a static array 4-byte alignment, so a
 # change anywhere in the DLL's data moved them: the kernels' 16-byte stores then straddled lines). Sizes from the C's own #defines.
 csrc=(SRC/'vx_d3d_tnl.inc').read_text()+(SRC/'vx_d3d_tnl_soa.inc').read_text()
 D=lambda n:int(re.search(r'#define %s (\d+)'%n,csrc).group(1))
 TN_MAX,TN_CH,TN_REC=D('TN_MAX'),D('TN_CH'),D('TN_REC')
 arr=[('tn_sx',(TN_CH+8)*4),('tn_sy',(TN_CH+8)*4),('tn_sz',(TN_CH+8)*4),('tn_cx',(TN_MAX+8)*4),('tn_cy',(TN_MAX+8)*4),('tn_cz',(TN_MAX+8)*4),('tn_cw',(TN_MAX+8)*4),('tn_cc',TN_MAX+8),('tn_vc',16384+8),('tn_buf',(TN_MAX+8)*TN_REC)]
 dl=['; vx_tnl_data.asm (.751): the T&L working arrays, each on a 64-byte line (tools/tnl_asm.py)','.386',"DGROUP group _BSS","_BSS segment page public use32 'BSS'"];o=0
 for n,b in arr:
  pad=(-o)%64
  if pad:dl.append(' db %d dup(?)'%pad);o+=pad
  dl+=['public %s'%n,'%s db %d dup(?)'%(n,b)];o+=b # no underscore: Watcom's stack-call builds (-4s, -6s) name data plainly
 dl+=['_BSS ends','end']
 (SRC/'vx_tnl_data.asm').write_text('\n'.join(dl)+'\n')
 (SRC/'vx_tnl_sizes.inc').write_text('/* vx_tnl_sizes.inc: the kernels\' sizes in bytes and places within a 64-byte line (tools/tnl_asm.py), for VXKBENCH\'s sweep */\n'+''.join('#define KSZ%s %d\n#define KPIN%s %d\n'%(n,b,n,PIN.get(n,0)) for n,b in sizes))
 out=asm if asm.parent.exists() else HERE/'vx_tnl_sse.asm'
 out.write_text(text)
 # .742: tests/tnl_kernels.s, the same bytes for the host's kernel test (made from the .asm just written, so it cannot go stale)
 names=re.findall(r'^public (\w+)',text,re.M);gs='.intel_syntax noprefix\n.text\n'+''.join('.globl %s\n'%n for n in names)
 for n in names:
  body=text[text.index('%s proc near'%n):text.index('%s endp'%n)];gs+=n+':\n'
  for m in re.finditer(r'^ db ([^;\n]*)',body,re.M):gs+=' .byte '+','.join('0x%02x'%int(x.strip().rstrip('h'),16) for x in m.group(1).split(','))+'\n'
 if (HERE.parent/'tests').exists():(HERE.parent/'tests'/'tnl_kernels.s').write_text(gs)
 for n,ins,nb in counts:
  print('%s %s ins %s bytes'%(n,ins,nb))
if __name__=='__main__':
 main()
