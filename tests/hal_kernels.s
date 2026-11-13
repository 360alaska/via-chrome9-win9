.intel_syntax noprefix
.text
.globl _vxs_check
.globl _vxs_conv
.globl _vxs_max16

_vxs_check:
 push ebx
 push esi
 push edi
 push ebp
 mov ebp,[esp+20]
 mov esi,[ebp+0]
 mov ebx,[ebp+4]
 mov ecx,[ebp+8]
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

 movdqu xmm7,[ebp+96]
 pxor xmm7,xmm0
 movdqu xmm1,[ebp+112]
 pcmpgtd xmm7,xmm1
 movdqa xmm3,xmm0
 psrad xmm3,31
 pandn xmm7,xmm3
 movdqu xmm1,[ebp+256]
 pand xmm7,xmm1
 movdqu xmm1,[ebp+192]
 movdqa xmm3,xmm0
 psubd xmm3,xmm1
 movdqu xmm1,[ebp+96]
 pxor xmm3,xmm1
 movdqu xmm1,[ebp+208]
 pcmpgtd xmm3,xmm1
 pandn xmm7,xmm3
 por xmm6,xmm7

 movdqu xmm7,[ebp+96]
 pxor xmm7,xmm2
 movdqu xmm1,[ebp+112]
 pcmpgtd xmm7,xmm1
 movdqa xmm3,xmm2
 psrad xmm3,31
 pandn xmm7,xmm3
 movdqu xmm1,[ebp+272]
 pand xmm7,xmm1
 movdqu xmm1,[ebp+224]
 movdqa xmm3,xmm2
 psubd xmm3,xmm1
 movdqu xmm1,[ebp+96]
 pxor xmm3,xmm1
 movdqu xmm1,[ebp+240]
 pcmpgtd xmm3,xmm1
 pandn xmm7,xmm3
 por xmm6,xmm7
 movdqu xmm1,[ebp+96]
 movdqa xmm3,xmm4
 pxor xmm3,xmm1
 movdqu xmm1,[ebp+128]
 pcmpgtd xmm3,xmm1
 por xmm6,xmm3
 movdqu xmm1,[ebp+160]
 movdqa xmm3,xmm5
 psubd xmm3,xmm1
 movdqu xmm1,[ebp+96]
 pxor xmm3,xmm1
 movdqu xmm1,[ebp+144]
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

_vxs_conv:
 push ebx
 push esi
 push edi
 push ebp
 mov ebp,[esp+20]
 mov esi,[ebp+0]
 mov edi,[ebp+12]
 mov ecx,[ebp+8]
 test ecx,ecx
 jz 9f
 movdqu xmm7,[ebp+176]
1:
 movdqu xmm0,[esi]
 movdqa xmm1,xmm0
 pcmpeqd xmm1,xmm7
 pandn xmm1,xmm0
 movdqu [edi],xmm1
 mov edx,[ebp+20]
 mov eax,-1
 test edx,edx
 jz 2f
 mov eax,[esi+edx]
2:
 mov [edi+16],eax
 mov edx,[ebp+24]
 xor eax,eax
 cmp edx,1
 jb 3f
 mov eax,0xFF000000
 je 3f
 mov edx,[ebp+28]
 mov eax,[esi+edx]
3:
 mov [edi+20],eax
 xor eax,eax
 xor ebx,ebx
 mov edx,[ebp+32]
 test edx,edx
 jz 4f
 mov eax,[esi+edx]
 cmp dword ptr [ebp+36],0
 je 6f
 mov ebx,[esi+edx+4]
6:
 mov edx,eax
 and edx,0x7F800000
 cmp edx,0x7F800000
 jne 5f
 xor eax,eax
5:
 mov edx,ebx
 and edx,0x7F800000
 cmp edx,0x7F800000
 jne 5f
 xor ebx,ebx
5:
4:
 mov [edi+24],eax
 mov [edi+28],ebx
 cmp dword ptr [ebp+40],0
 je 7f
 xor eax,eax
 xor ebx,ebx
 mov edx,[ebp+44]
 test edx,edx
 jz 7f
 mov eax,[esi+edx]
 cmp dword ptr [ebp+48],0
 je 6f
 mov ebx,[esi+edx+4]
6:
 mov edx,eax
 and edx,0x7F800000
 cmp edx,0x7F800000
 jne 5f
 xor eax,eax
5:
 mov edx,ebx
 and edx,0x7F800000
 cmp edx,0x7F800000
 jne 5f
 xor ebx,ebx
5:
7:
 mov [edi+32],eax
 mov [edi+36],ebx
 add esi,[ebp+4]
 add edi,[ebp+16]
 dec ecx
 jnz 1b
9:
 pop ebp
 pop edi
 pop esi
 pop ebx
 ret

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
