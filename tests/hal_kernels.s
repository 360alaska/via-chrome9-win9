.intel_syntax noprefix
.text
.globl _vxs_check
.globl _vxs_conv
.globl _vxs_max16
.globl _vxs_cmm16
.globl _vxs_conv8
.globl _vxs_mm16
.globl _vxs_add16
.globl _vxs_eqw
.globl _vxs_convg

_vxs_check:
 push ebx
 push esi
 push edi
 mov edi,[esp+16]
 mov esi,[edi+0]
 mov ebx,[edi+4]
 mov ecx,[edi+8]
 movdqu xmm4,[edi+288]
 movdqu xmm5,[edi+304]
 movdqu xmm6,[edi+320]
 movdqu xmm7,[edi+336]
 xor eax,eax
 and ecx,-4
 jz 9f
1:
 pxor xmm3,xmm3
 mov edx,esi
 movdqu xmm0,[edx]
 movdqa xmm1,xmm0
 paddd xmm0,xmm4
 pcmpgtd xmm0,xmm5
 pcmpgtd xmm1,xmm6
 por xmm1,xmm7
 pand xmm0,xmm1
 por xmm3,xmm0
 add edx,ebx
 movdqu xmm0,[edx]
 movdqa xmm1,xmm0
 paddd xmm0,xmm4
 pcmpgtd xmm0,xmm5
 pcmpgtd xmm1,xmm6
 por xmm1,xmm7
 pand xmm0,xmm1
 por xmm3,xmm0
 add edx,ebx
 movdqu xmm0,[edx]
 movdqa xmm1,xmm0
 paddd xmm0,xmm4
 pcmpgtd xmm0,xmm5
 pcmpgtd xmm1,xmm6
 por xmm1,xmm7
 pand xmm0,xmm1
 por xmm3,xmm0
 add edx,ebx
 movdqu xmm0,[edx]
 movdqa xmm1,xmm0
 paddd xmm0,xmm4
 pcmpgtd xmm0,xmm5
 pcmpgtd xmm1,xmm6
 por xmm1,xmm7
 pand xmm0,xmm1
 por xmm3,xmm0
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

_vxs_conv8:
 push esi
 push edi
 mov eax,[esp+12]
 mov esi,[eax+0]
 mov edi,[eax+12]
 mov ecx,[eax+8]
 mov edx,[eax+4]
 test ecx,ecx
 jz 9f
 movdqu xmm7,[eax+176]
 pcmpeqd xmm6,xmm6
 psrld xmm6,24
 pslld xmm6,23
 pslldq xmm6,8
 movdqu xmm5,[eax+160]
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

_vxs_convg:
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
 push dword ptr [ebp+52]
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
 add esp,4
9:
 pop ebp
 pop edi
 pop esi
 pop ebx
 ret
