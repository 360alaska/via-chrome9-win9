; vx_kbench_old.asm: the position kernels of .746-.767 for VXKBENCH only (tools/kb_old.py); not in the driver since .768.
.386
.model flat
.code
public _tnk_old_pos
_tnk_old_pos proc near
 db 053h ; push	ebx
 db 056h ; push	esi
 db 057h ; push	edi
 db 055h ; push	ebp
 db 08Bh,044h,024h,014h ; mov	eax, dword ptr [esp + 0x14]
 db 08Bh,088h,0B0h,007h,000h,000h ; mov	ecx, dword ptr [eax + 0x7b0]
 db 08Bh,090h,0ACh,007h,000h,000h ; mov	edx, dword ptr [eax + 0x7ac]
 db 001h,0CAh ; add	edx, ecx
 db 031h,0DBh ; xor	ebx, ebx
 db 039h,0D1h ; cmp	ecx, edx
 db 00Fh,083h,0CDh,002h,000h,000h ; jae	0x2ed <.text+0x2ed>
 db 08Bh,0B0h,0E0h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9e0]
 db 00Fh,010h,02Ch,09Eh ; movups	xmm5, xmmword ptr [esi + 4*ebx]
 db 08Bh,0B0h,0E4h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9e4]
 db 00Fh,010h,034h,09Eh ; movups	xmm6, xmmword ptr [esi + 4*ebx]
 db 08Bh,0B0h,0E8h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9e8]
 db 00Fh,010h,03Ch,09Eh ; movups	xmm7, xmmword ptr [esi + 4*ebx]
 db 00Fh,010h,000h ; movups	xmm0, xmmword ptr [eax]
 db 00Fh,010h,048h,010h ; movups	xmm1, xmmword ptr [eax + 0x10]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,059h,0CDh ; mulps	xmm1, xmm5
 db 00Fh,010h,050h,040h ; movups	xmm2, xmmword ptr [eax + 0x40]
 db 00Fh,010h,058h,050h ; movups	xmm3, xmmword ptr [eax + 0x50]
 db 00Fh,059h,0D6h ; mulps	xmm2, xmm6
 db 00Fh,059h,0DEh ; mulps	xmm3, xmm6
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,010h,090h,080h,000h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x80]
 db 00Fh,010h,098h,090h,000h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x90]
 db 00Fh,059h,0D7h ; mulps	xmm2, xmm7
 db 00Fh,059h,0DFh ; mulps	xmm3, xmm7
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,010h,090h,0C0h,000h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0xc0]
 db 00Fh,010h,098h,0D0h,000h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0xd0]
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,010h,050h,020h ; movups	xmm2, xmmword ptr [eax + 0x20]
 db 00Fh,010h,058h,030h ; movups	xmm3, xmmword ptr [eax + 0x30]
 db 00Fh,059h,0D5h ; mulps	xmm2, xmm5
 db 00Fh,059h,0DDh ; mulps	xmm3, xmm5
 db 00Fh,010h,060h,060h ; movups	xmm4, xmmword ptr [eax + 0x60]
 db 00Fh,059h,0E6h ; mulps	xmm4, xmm6
 db 00Fh,058h,0D4h ; addps	xmm2, xmm4
 db 00Fh,010h,060h,070h ; movups	xmm4, xmmword ptr [eax + 0x70]
 db 00Fh,059h,0E6h ; mulps	xmm4, xmm6
 db 00Fh,058h,0DCh ; addps	xmm3, xmm4
 db 00Fh,010h,0A0h,0A0h,000h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0xa0]
 db 00Fh,059h,0E7h ; mulps	xmm4, xmm7
 db 00Fh,058h,0D4h ; addps	xmm2, xmm4
 db 00Fh,010h,0A0h,0B0h,000h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0xb0]
 db 00Fh,059h,0E7h ; mulps	xmm4, xmm7
 db 00Fh,058h,0DCh ; addps	xmm3, xmm4
 db 00Fh,010h,0A0h,0E0h,000h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0xe0]
 db 00Fh,058h,0D4h ; addps	xmm2, xmm4
 db 00Fh,010h,0A0h,0F0h,000h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0xf0]
 db 00Fh,058h,0DCh ; addps	xmm3, xmm4
 db 00Fh,010h,0A0h,070h,008h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x870]
 db 00Fh,05Eh,0E3h ; divps	xmm4, xmm3
 db 08Bh,0B0h,0ECh,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9ec]
 db 00Fh,011h,004h,08Eh ; movups	xmmword ptr [esi + 4*ecx], xmm0
 db 08Bh,0B0h,0F0h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9f0]
 db 00Fh,011h,00Ch,08Eh ; movups	xmmword ptr [esi + 4*ecx], xmm1
 db 08Bh,0B0h,0F4h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9f4]
 db 00Fh,011h,014h,08Eh ; movups	xmmword ptr [esi + 4*ecx], xmm2
 db 08Bh,0B0h,0F8h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9f8]
 db 00Fh,011h,01Ch,08Eh ; movups	xmmword ptr [esi + 4*ecx], xmm3
 db 00Fh,028h,0E8h ; movaps	xmm5, xmm0
 db 00Fh,028h,0F1h ; movaps	xmm6, xmm1
 db 00Fh,028h,0FAh ; movaps	xmm7, xmm2
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 00Fh,0C2h,0D3h,001h ; cmpltps	xmm2, xmm3
 db 00Fh,054h,0E2h ; andps	xmm4, xmm2
 db 00Fh,028h,0C7h ; movaps	xmm0, xmm7
 db 00Fh,0C2h,0C1h,001h ; cmpltps	xmm0, xmm1
 db 00Fh,028h,0D3h ; movaps	xmm2, xmm3
 db 00Fh,0C2h,0D1h,002h ; cmpleps	xmm2, xmm1
 db 00Fh,056h,0C2h ; orps	xmm0, xmm2
 db 00Fh,054h,080h,070h,009h,000h,000h ; andps	xmm0, xmmword ptr [eax + 0x970]
 db 00Fh,028h,0D3h ; movaps	xmm2, xmm3
 db 00Fh,0C2h,0D7h,001h ; cmpltps	xmm2, xmm7
 db 00Fh,054h,090h,080h,009h,000h,000h ; andps	xmm2, xmmword ptr [eax + 0x980]
 db 00Fh,056h,0C2h ; orps	xmm0, xmm2
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 00Fh,05Ch,0D3h ; subps	xmm2, xmm3
 db 00Fh,028h,0CDh ; movaps	xmm1, xmm5
 db 00Fh,0C2h,0CAh,001h ; cmpltps	xmm1, xmm2
 db 00Fh,054h,088h,090h,009h,000h,000h ; andps	xmm1, xmmword ptr [eax + 0x990]
 db 00Fh,056h,0C1h ; orps	xmm0, xmm1
 db 00Fh,028h,0CBh ; movaps	xmm1, xmm3
 db 00Fh,0C2h,0CDh,001h ; cmpltps	xmm1, xmm5
 db 00Fh,054h,088h,0A0h,009h,000h,000h ; andps	xmm1, xmmword ptr [eax + 0x9a0]
 db 00Fh,056h,0C1h ; orps	xmm0, xmm1
 db 00Fh,028h,0CEh ; movaps	xmm1, xmm6
 db 00Fh,0C2h,0CAh,001h ; cmpltps	xmm1, xmm2
 db 00Fh,054h,088h,0B0h,009h,000h,000h ; andps	xmm1, xmmword ptr [eax + 0x9b0]
 db 00Fh,056h,0C1h ; orps	xmm0, xmm1
 db 00Fh,028h,0CBh ; movaps	xmm1, xmm3
 db 00Fh,0C2h,0CEh,001h ; cmpltps	xmm1, xmm6
 db 00Fh,054h,088h,0C0h,009h,000h,000h ; andps	xmm1, xmmword ptr [eax + 0x9c0]
 db 00Fh,056h,0C1h ; orps	xmm0, xmm1
 db 00Fh,010h,090h,030h,009h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x930]
 db 00Fh,028h,0CBh ; movaps	xmm1, xmm3
 db 066h,00Fh,0DBh,0CAh ; pand	xmm1, xmm2
 db 066h,00Fh,076h,0CAh ; pcmpeqd	xmm1, xmm2
 db 00Fh,028h,0DDh ; movaps	xmm3, xmm5
 db 066h,00Fh,0DBh,0DAh ; pand	xmm3, xmm2
 db 066h,00Fh,076h,0DAh ; pcmpeqd	xmm3, xmm2
 db 066h,00Fh,0EBh,0CBh ; por	xmm1, xmm3
 db 00Fh,028h,0DEh ; movaps	xmm3, xmm6
 db 066h,00Fh,0DBh,0DAh ; pand	xmm3, xmm2
 db 066h,00Fh,076h,0DAh ; pcmpeqd	xmm3, xmm2
 db 066h,00Fh,0EBh,0CBh ; por	xmm1, xmm3
 db 00Fh,028h,0DFh ; movaps	xmm3, xmm7
 db 066h,00Fh,0DBh,0DAh ; pand	xmm3, xmm2
 db 066h,00Fh,076h,0DAh ; pcmpeqd	xmm3, xmm2
 db 066h,00Fh,0EBh,0CBh ; por	xmm1, xmm3
 db 00Fh,028h,0DCh ; movaps	xmm3, xmm4
 db 066h,00Fh,0DBh,0DAh ; pand	xmm3, xmm2
 db 066h,00Fh,076h,0DAh ; pcmpeqd	xmm3, xmm2
 db 066h,00Fh,0EBh,0CBh ; por	xmm1, xmm3
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 066h,00Fh,0DFh,0D8h ; pandn	xmm3, xmm0
 db 066h,00Fh,0DBh,088h,0D0h,009h,000h,000h ; pand	xmm1, xmmword ptr [eax + 0x9d0]
 db 066h,00Fh,0EBh,0D9h ; por	xmm3, xmm1
 db 066h,00Fh,06Bh,0DBh ; packssdw	xmm3, xmm3
 db 066h,00Fh,067h,0DBh ; packuswb	xmm3, xmm3
 db 08Bh,0B0h,000h,00Ah,000h,000h ; mov	esi, dword ptr [eax + 0xa00]
 db 066h,00Fh,07Eh,01Ch,00Eh ; movd	dword ptr [esi + ecx], xmm3
 db 08Bh,0B0h,014h,023h,000h,000h ; mov	esi, dword ptr [eax + 0x2314]
 db 085h,0F6h ; test	esi, esi
 db 074h,043h ; je	0x259 <.text+0x259>
 db 08Bh,0B0h,0F8h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9f8]
 db 00Fh,010h,01Ch,08Eh ; movups	xmm3, xmmword ptr [esi + 4*ecx]
 db 00Fh,010h,088h,080h,008h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x880]
 db 00Fh,059h,0D9h ; mulps	xmm3, xmm1
 db 00Fh,010h,090h,040h,009h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x940]
 db 00Fh,028h,0C2h ; movaps	xmm0, xmm2
 db 00Fh,055h,0C5h ; andnps	xmm0, xmm5
 db 00Fh,055h,0D6h ; andnps	xmm2, xmm6
 db 00Fh,028h,0CBh ; movaps	xmm1, xmm3
 db 00Fh,0C2h,0C8h,001h ; cmpltps	xmm1, xmm0
 db 00Fh,0C2h,0DAh,001h ; cmpltps	xmm3, xmm2
 db 00Fh,056h,0CBh ; orps	xmm1, xmm3
 db 00Fh,010h,080h,030h,023h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x2330]
 db 00Fh,056h,0C1h ; orps	xmm0, xmm1
 db 00Fh,011h,080h,030h,023h,000h,000h ; movups	xmmword ptr [eax + 0x2330], xmm0
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,059h,0F4h ; mulps	xmm6, xmm4
 db 00Fh,059h,0FCh ; mulps	xmm7, xmm4
 db 00Fh,059h,0A8h,010h,001h,000h,000h ; mulps	xmm5, xmmword ptr [eax + 0x110]
 db 00Fh,059h,0B0h,030h,001h,000h,000h ; mulps	xmm6, xmmword ptr [eax + 0x130]
 db 00Fh,059h,0B8h,050h,001h,000h,000h ; mulps	xmm7, xmmword ptr [eax + 0x150]
 db 00Fh,058h,0A8h,000h,001h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x100]
 db 00Fh,058h,0B0h,020h,001h,000h,000h ; addps	xmm6, xmmword ptr [eax + 0x120]
 db 00Fh,058h,0B8h,040h,001h,000h,000h ; addps	xmm7, xmmword ptr [eax + 0x140]
 db 00Fh,058h,0E9h ; addps	xmm5, xmm1
 db 00Fh,058h,0F1h ; addps	xmm6, xmm1
 db 00Fh,058h,0F9h ; addps	xmm7, xmm1
 db 00Fh,028h,0C5h ; movaps	xmm0, xmm5
 db 00Fh,014h,0C6h ; unpcklps	xmm0, xmm6
 db 00Fh,028h,0CFh ; movaps	xmm1, xmm7
 db 00Fh,014h,0CCh ; unpcklps	xmm1, xmm4
 db 00Fh,015h,0EEh ; unpckhps	xmm5, xmm6
 db 00Fh,015h,0FCh ; unpckhps	xmm7, xmm4
 db 08Bh,0B8h,0A8h,007h,000h,000h ; mov	edi, dword ptr [eax + 0x7a8]
 db 089h,0CEh ; mov	esi, ecx
 db 00Fh,0AFh,0F7h ; imul	esi, edi
 db 003h,0B0h,004h,00Ah,000h,000h ; add	esi, dword ptr [eax + 0xa04]
 db 00Fh,028h,0D0h ; movaps	xmm2, xmm0
 db 00Fh,016h,0D1h ; movlhps	xmm2, xmm1
 db 00Fh,011h,016h ; movups	xmmword ptr [esi], xmm2
 db 00Fh,012h,0C8h ; movhlps	xmm1, xmm0
 db 00Fh,011h,00Ch,03Eh ; movups	xmmword ptr [esi + edi], xmm1
 db 00Fh,028h,0D5h ; movaps	xmm2, xmm5
 db 00Fh,016h,0D7h ; movlhps	xmm2, xmm7
 db 00Fh,011h,014h,07Eh ; movups	xmmword ptr [esi + 2*edi], xmm2
 db 00Fh,012h,0FDh ; movhlps	xmm7, xmm5
 db 08Dh,034h,07Eh ; lea	esi, [esi + 2*edi]
 db 00Fh,011h,03Ch,03Eh ; movups	xmmword ptr [esi + edi], xmm7
 db 083h,0C1h,004h ; add	ecx, 0x4
 db 083h,0C3h,004h ; add	ebx, 0x4
 db 039h,0D1h ; cmp	ecx, edx
 db 00Fh,082h,033h,0FDh,0FFh,0FFh ; jb	0x20 <.text+0x20>
 db 05Dh ; pop	ebp
 db 05Fh ; pop	edi
 db 05Eh ; pop	esi
 db 05Bh ; pop	ebx
 db 0C3h ; ret
_tnk_old_pos endp
public _tnk_old_pos_o
_tnk_old_pos_o proc near
 db 053h ; push	ebx
 db 056h ; push	esi
 db 057h ; push	edi
 db 055h ; push	ebp
 db 08Bh,044h,024h,014h ; mov	eax, dword ptr [esp + 0x14]
 db 08Bh,088h,0B0h,007h,000h,000h ; mov	ecx, dword ptr [eax + 0x7b0]
 db 08Bh,090h,0ACh,007h,000h,000h ; mov	edx, dword ptr [eax + 0x7ac]
 db 001h,0CAh ; add	edx, ecx
 db 031h,0DBh ; xor	ebx, ebx
 db 039h,0D1h ; cmp	ecx, edx
 db 00Fh,083h,0D4h,002h,000h,000h ; jae	0x2f4 <.text+0x2f4>
 db 08Bh,0B0h,0E0h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9e0]
 db 00Fh,010h,02Ch,09Eh ; movups	xmm5, xmmword ptr [esi + 4*ebx]
 db 08Bh,0B0h,0E4h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9e4]
 db 00Fh,010h,034h,09Eh ; movups	xmm6, xmmword ptr [esi + 4*ebx]
 db 08Bh,0B0h,0E8h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9e8]
 db 00Fh,010h,03Ch,09Eh ; movups	xmm7, xmmword ptr [esi + 4*ebx]
 db 00Fh,010h,000h ; movups	xmm0, xmmword ptr [eax]
 db 00Fh,010h,048h,010h ; movups	xmm1, xmmword ptr [eax + 0x10]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,059h,0CDh ; mulps	xmm1, xmm5
 db 00Fh,010h,050h,040h ; movups	xmm2, xmmword ptr [eax + 0x40]
 db 00Fh,010h,058h,050h ; movups	xmm3, xmmword ptr [eax + 0x50]
 db 00Fh,059h,0D6h ; mulps	xmm2, xmm6
 db 00Fh,059h,0DEh ; mulps	xmm3, xmm6
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,010h,090h,080h,000h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x80]
 db 00Fh,010h,098h,090h,000h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x90]
 db 00Fh,059h,0D7h ; mulps	xmm2, xmm7
 db 00Fh,059h,0DFh ; mulps	xmm3, xmm7
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,010h,090h,0C0h,000h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0xc0]
 db 00Fh,010h,098h,0D0h,000h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0xd0]
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,010h,050h,020h ; movups	xmm2, xmmword ptr [eax + 0x20]
 db 00Fh,010h,058h,030h ; movups	xmm3, xmmword ptr [eax + 0x30]
 db 00Fh,059h,0D5h ; mulps	xmm2, xmm5
 db 00Fh,059h,0DDh ; mulps	xmm3, xmm5
 db 00Fh,010h,060h,060h ; movups	xmm4, xmmword ptr [eax + 0x60]
 db 00Fh,059h,0E6h ; mulps	xmm4, xmm6
 db 00Fh,058h,0D4h ; addps	xmm2, xmm4
 db 00Fh,010h,060h,070h ; movups	xmm4, xmmword ptr [eax + 0x70]
 db 00Fh,059h,0E6h ; mulps	xmm4, xmm6
 db 00Fh,058h,0DCh ; addps	xmm3, xmm4
 db 00Fh,010h,0A0h,0A0h,000h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0xa0]
 db 00Fh,059h,0E7h ; mulps	xmm4, xmm7
 db 00Fh,058h,0D4h ; addps	xmm2, xmm4
 db 00Fh,010h,0A0h,0B0h,000h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0xb0]
 db 00Fh,059h,0E7h ; mulps	xmm4, xmm7
 db 00Fh,058h,0DCh ; addps	xmm3, xmm4
 db 00Fh,010h,0A0h,0E0h,000h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0xe0]
 db 00Fh,058h,0D4h ; addps	xmm2, xmm4
 db 00Fh,010h,0A0h,0F0h,000h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0xf0]
 db 00Fh,058h,0DCh ; addps	xmm3, xmm4
 db 08Bh,0B0h,0ECh,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9ec]
 db 00Fh,011h,004h,08Eh ; movups	xmmword ptr [esi + 4*ecx], xmm0
 db 08Bh,0B0h,0F0h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9f0]
 db 00Fh,011h,00Ch,08Eh ; movups	xmmword ptr [esi + 4*ecx], xmm1
 db 08Bh,0B0h,0F4h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9f4]
 db 00Fh,011h,014h,08Eh ; movups	xmmword ptr [esi + 4*ecx], xmm2
 db 08Bh,0B0h,0F8h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9f8]
 db 00Fh,011h,01Ch,08Eh ; movups	xmmword ptr [esi + 4*ecx], xmm3
 db 00Fh,028h,0E8h ; movaps	xmm5, xmm0
 db 00Fh,028h,0F1h ; movaps	xmm6, xmm1
 db 00Fh,028h,0FAh ; movaps	xmm7, xmm2
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 00Fh,0C2h,0D3h,001h ; cmpltps	xmm2, xmm3
 db 00Fh,010h,0A0h,070h,008h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x870]
 db 00Fh,05Eh,0E3h ; divps	xmm4, xmm3
 db 00Fh,054h,0E2h ; andps	xmm4, xmm2
 db 00Fh,028h,0C7h ; movaps	xmm0, xmm7
 db 00Fh,0C2h,0C1h,001h ; cmpltps	xmm0, xmm1
 db 00Fh,028h,0D3h ; movaps	xmm2, xmm3
 db 00Fh,0C2h,0D1h,002h ; cmpleps	xmm2, xmm1
 db 00Fh,056h,0C2h ; orps	xmm0, xmm2
 db 00Fh,054h,080h,070h,009h,000h,000h ; andps	xmm0, xmmword ptr [eax + 0x970]
 db 00Fh,028h,0D3h ; movaps	xmm2, xmm3
 db 00Fh,0C2h,0D7h,001h ; cmpltps	xmm2, xmm7
 db 00Fh,054h,090h,080h,009h,000h,000h ; andps	xmm2, xmmword ptr [eax + 0x980]
 db 00Fh,056h,0C2h ; orps	xmm0, xmm2
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 00Fh,05Ch,0D3h ; subps	xmm2, xmm3
 db 00Fh,028h,0CDh ; movaps	xmm1, xmm5
 db 00Fh,0C2h,0CAh,001h ; cmpltps	xmm1, xmm2
 db 00Fh,054h,088h,090h,009h,000h,000h ; andps	xmm1, xmmword ptr [eax + 0x990]
 db 00Fh,056h,0C1h ; orps	xmm0, xmm1
 db 00Fh,028h,0CBh ; movaps	xmm1, xmm3
 db 00Fh,0C2h,0CDh,001h ; cmpltps	xmm1, xmm5
 db 00Fh,054h,088h,0A0h,009h,000h,000h ; andps	xmm1, xmmword ptr [eax + 0x9a0]
 db 00Fh,056h,0C1h ; orps	xmm0, xmm1
 db 00Fh,028h,0CEh ; movaps	xmm1, xmm6
 db 00Fh,0C2h,0CAh,001h ; cmpltps	xmm1, xmm2
 db 00Fh,054h,088h,0B0h,009h,000h,000h ; andps	xmm1, xmmword ptr [eax + 0x9b0]
 db 00Fh,056h,0C1h ; orps	xmm0, xmm1
 db 00Fh,028h,0CBh ; movaps	xmm1, xmm3
 db 00Fh,0C2h,0CEh,001h ; cmpltps	xmm1, xmm6
 db 00Fh,054h,088h,0C0h,009h,000h,000h ; andps	xmm1, xmmword ptr [eax + 0x9c0]
 db 00Fh,056h,0C1h ; orps	xmm0, xmm1
 db 066h,00Fh,06Fh,090h,030h,009h,000h,000h ; movdqa	xmm2, xmmword ptr [eax + 0x930]
 db 066h,00Fh,06Fh,0CBh ; movdqa	xmm1, xmm3
 db 066h,00Fh,0DBh,0CAh ; pand	xmm1, xmm2
 db 066h,00Fh,076h,0CAh ; pcmpeqd	xmm1, xmm2
 db 066h,00Fh,06Fh,0DDh ; movdqa	xmm3, xmm5
 db 066h,00Fh,0DBh,0DAh ; pand	xmm3, xmm2
 db 066h,00Fh,076h,0DAh ; pcmpeqd	xmm3, xmm2
 db 066h,00Fh,0EBh,0CBh ; por	xmm1, xmm3
 db 066h,00Fh,06Fh,0DEh ; movdqa	xmm3, xmm6
 db 066h,00Fh,0DBh,0DAh ; pand	xmm3, xmm2
 db 066h,00Fh,076h,0DAh ; pcmpeqd	xmm3, xmm2
 db 066h,00Fh,0EBh,0CBh ; por	xmm1, xmm3
 db 066h,00Fh,06Fh,0DFh ; movdqa	xmm3, xmm7
 db 066h,00Fh,0DBh,0DAh ; pand	xmm3, xmm2
 db 066h,00Fh,076h,0DAh ; pcmpeqd	xmm3, xmm2
 db 066h,00Fh,0EBh,0CBh ; por	xmm1, xmm3
 db 066h,00Fh,06Fh,0DCh ; movdqa	xmm3, xmm4
 db 066h,00Fh,0DBh,0DAh ; pand	xmm3, xmm2
 db 066h,00Fh,076h,0DAh ; pcmpeqd	xmm3, xmm2
 db 066h,00Fh,0EBh,0CBh ; por	xmm1, xmm3
 db 066h,00Fh,06Fh,0D9h ; movdqa	xmm3, xmm1
 db 066h,00Fh,0DFh,0D8h ; pandn	xmm3, xmm0
 db 066h,00Fh,0DBh,088h,0D0h,009h,000h,000h ; pand	xmm1, xmmword ptr [eax + 0x9d0]
 db 066h,00Fh,0EBh,0D9h ; por	xmm3, xmm1
 db 066h,00Fh,06Bh,0DBh ; packssdw	xmm3, xmm3
 db 066h,00Fh,067h,0DBh ; packuswb	xmm3, xmm3
 db 08Bh,0B0h,000h,00Ah,000h,000h ; mov	esi, dword ptr [eax + 0xa00]
 db 066h,00Fh,07Eh,01Ch,00Eh ; movd	dword ptr [esi + ecx], xmm3
 db 08Bh,0B0h,014h,023h,000h,000h ; mov	esi, dword ptr [eax + 0x2314]
 db 085h,0F6h ; test	esi, esi
 db 074h,043h ; je	0x260 <.text+0x260>
 db 08Bh,0B0h,0F8h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9f8]
 db 00Fh,010h,01Ch,08Eh ; movups	xmm3, xmmword ptr [esi + 4*ecx]
 db 00Fh,010h,088h,080h,008h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x880]
 db 00Fh,059h,0D9h ; mulps	xmm3, xmm1
 db 00Fh,010h,090h,040h,009h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x940]
 db 00Fh,028h,0C2h ; movaps	xmm0, xmm2
 db 00Fh,055h,0C5h ; andnps	xmm0, xmm5
 db 00Fh,055h,0D6h ; andnps	xmm2, xmm6
 db 00Fh,028h,0CBh ; movaps	xmm1, xmm3
 db 00Fh,0C2h,0C8h,001h ; cmpltps	xmm1, xmm0
 db 00Fh,0C2h,0DAh,001h ; cmpltps	xmm3, xmm2
 db 00Fh,056h,0CBh ; orps	xmm1, xmm3
 db 00Fh,010h,080h,030h,023h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x2330]
 db 00Fh,056h,0C1h ; orps	xmm0, xmm1
 db 00Fh,011h,080h,030h,023h,000h,000h ; movups	xmmword ptr [eax + 0x2330], xmm0
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,059h,0F4h ; mulps	xmm6, xmm4
 db 00Fh,059h,0FCh ; mulps	xmm7, xmm4
 db 00Fh,059h,0A8h,010h,001h,000h,000h ; mulps	xmm5, xmmword ptr [eax + 0x110]
 db 00Fh,059h,0B0h,030h,001h,000h,000h ; mulps	xmm6, xmmword ptr [eax + 0x130]
 db 00Fh,059h,0B8h,050h,001h,000h,000h ; mulps	xmm7, xmmword ptr [eax + 0x150]
 db 00Fh,058h,0A8h,000h,001h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x100]
 db 00Fh,058h,0B0h,020h,001h,000h,000h ; addps	xmm6, xmmword ptr [eax + 0x120]
 db 00Fh,058h,0B8h,040h,001h,000h,000h ; addps	xmm7, xmmword ptr [eax + 0x140]
 db 00Fh,058h,0E9h ; addps	xmm5, xmm1
 db 00Fh,058h,0F1h ; addps	xmm6, xmm1
 db 00Fh,058h,0F9h ; addps	xmm7, xmm1
 db 00Fh,028h,0C5h ; movaps	xmm0, xmm5
 db 00Fh,014h,0C6h ; unpcklps	xmm0, xmm6
 db 00Fh,028h,0CFh ; movaps	xmm1, xmm7
 db 00Fh,014h,0CCh ; unpcklps	xmm1, xmm4
 db 00Fh,015h,0EEh ; unpckhps	xmm5, xmm6
 db 00Fh,015h,0FCh ; unpckhps	xmm7, xmm4
 db 08Bh,0B8h,0A8h,007h,000h,000h ; mov	edi, dword ptr [eax + 0x7a8]
 db 089h,0CEh ; mov	esi, ecx
 db 00Fh,0AFh,0F7h ; imul	esi, edi
 db 003h,0B0h,004h,00Ah,000h,000h ; add	esi, dword ptr [eax + 0xa04]
 db 00Fh,028h,0D0h ; movaps	xmm2, xmm0
 db 00Fh,016h,0D1h ; movlhps	xmm2, xmm1
 db 00Fh,011h,016h ; movups	xmmword ptr [esi], xmm2
 db 00Fh,012h,0C8h ; movhlps	xmm1, xmm0
 db 00Fh,011h,00Ch,03Eh ; movups	xmmword ptr [esi + edi], xmm1
 db 00Fh,028h,0D5h ; movaps	xmm2, xmm5
 db 00Fh,016h,0D7h ; movlhps	xmm2, xmm7
 db 00Fh,011h,014h,07Eh ; movups	xmmword ptr [esi + 2*edi], xmm2
 db 00Fh,012h,0FDh ; movhlps	xmm7, xmm5
 db 08Dh,034h,07Eh ; lea	esi, [esi + 2*edi]
 db 00Fh,011h,03Ch,03Eh ; movups	xmmword ptr [esi + edi], xmm7
 db 083h,0C1h,004h ; add	ecx, 0x4
 db 083h,0C3h,004h ; add	ebx, 0x4
 db 039h,0D1h ; cmp	ecx, edx
 db 00Fh,082h,02Ch,0FDh,0FFh,0FFh ; jb	0x20 <.text+0x20>
 db 05Dh ; pop	ebp
 db 05Fh ; pop	edi
 db 05Eh ; pop	esi
 db 05Bh ; pop	ebx
 db 0C3h ; ret
_tnk_old_pos_o endp
end
