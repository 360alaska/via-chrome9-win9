; vx_tnl_sse.asm: T&L group kernels (SSE2) as bytes for wasm.
; _tnk_light, _tnk_light_k, _tnk_plight are the rescheduled kernels
; (same operations, same association, fewer spills, two dot-products in flight).
; _tnk_spec, _tnk_cv, _tnk_splight are the previous clang sequences: same bits as vx_d3d_tnl_soa.inc.
; R1: _tnq_*, the reworked kernels (positions, range lighting, attributes, cull, packet copies): clang, part 3 of the C.
.386
_TEXT segment page public use32 'CODE'
assume cs:_TEXT
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
public _tnk_light_h
_tnk_light_h proc near
 db 053h ; push	ebx
 db 056h ; push	esi
 db 057h ; push	edi
 db 055h ; push	ebp
 db 08Bh,044h,024h,014h ; mov	eax, dword ptr [esp + 0x14]
 db 00Fh,010h,0A0h,070h,008h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x870]
 db 08Bh,0B0h,098h,007h,000h,000h ; mov	esi, dword ptr [eax + 0x798]
 db 085h,0F6h ; test	esi, esi
 db 00Fh,084h,020h,001h,000h,000h ; je	0x13d <_tnk_light_h+0x13d>
 db 083h,0B8h,0B8h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7b8], 0x0
 db 074h,016h ; je	0x3c <_tnk_light_h+0x3c>
 db 00Fh,010h,080h,0E0h,007h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x7e0]
 db 00Fh,010h,088h,040h,009h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x940]
 db 00Fh,055h,0C8h ; andnps	xmm1, xmm0
 db 0E9h,0CCh,000h,000h,000h ; jmp	0x108 <_tnk_light_h+0x108>
 db 00Fh,010h,0A8h,0C0h,007h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x7c0]
 db 00Fh,010h,0B0h,0D0h,007h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x7d0]
 db 00Fh,010h,0B8h,0E0h,007h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x7e0]
 db 00Fh,010h,080h,0A0h,001h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x1a0]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,010h,088h,0E0h,001h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1e0]
 db 00Fh,059h,0CEh ; mulps	xmm1, xmm6
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,020h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x220]
 db 00Fh,059h,0CFh ; mulps	xmm1, xmm7
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,060h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x260]
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 083h,0B8h,09Ch,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x79c], 0x0
 db 075h,00Ch ; jne	0x94 <_tnk_light_h+0x94>
 db 00Fh,010h,088h,040h,009h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x940]
 db 00Fh,055h,0C8h ; andnps	xmm1, xmm0
 db 0EBh,074h ; jmp	0x108 <_tnk_light_h+0x108>
 db 00Fh,059h,0C0h ; mulps	xmm0, xmm0
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,010h,080h,080h,001h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x180]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,010h,088h,0C0h,001h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1c0]
 db 00Fh,059h,0CEh ; mulps	xmm1, xmm6
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,000h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x200]
 db 00Fh,059h,0CFh ; mulps	xmm1, xmm7
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,040h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x240]
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,059h,0C0h ; mulps	xmm0, xmm0
 db 00Fh,028h,0D8h ; movaps	xmm3, xmm0
 db 00Fh,010h,080h,090h,001h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x190]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,010h,088h,0D0h,001h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1d0]
 db 00Fh,059h,0CEh ; mulps	xmm1, xmm6
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,010h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x210]
 db 00Fh,059h,0CFh ; mulps	xmm1, xmm7
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,050h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x250]
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,059h,0C0h ; mulps	xmm0, xmm0
 db 00Fh,058h,0D8h ; addps	xmm3, xmm0
 db 00Fh,058h,0DCh ; addps	xmm3, xmm4
 db 00Fh,051h,0CBh ; sqrtps	xmm1, xmm3
 db 083h,0FEh,003h ; cmp	esi, 0x3
 db 075h,013h ; jne	0x120 <_tnk_light_h+0x120>
 db 00Fh,010h,0A0h,060h,007h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x760]
 db 00Fh,05Ch,0E1h ; subps	xmm4, xmm1
 db 00Fh,05Eh,0A0h,070h,007h,000h,000h ; divps	xmm4, xmmword ptr [eax + 0x770]
 db 0EBh,01Dh ; jmp	0x13d <_tnk_light_h+0x13d>
 db 00Fh,010h,080h,050h,007h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x750]
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 083h,0FEh,001h ; cmp	esi, 0x1
 db 074h,003h ; je	0x132 <_tnk_light_h+0x132>
 db 00Fh,059h,0C0h ; mulps	xmm0, xmm0
 db 00Fh,057h,0D2h ; xorps	xmm2, xmm2
 db 00Fh,05Ch,0D0h ; subps	xmm2, xmm0
 db 0E8h,0B7h,002h,000h,000h ; call	0x3f4 <_tnk_light_h+0x3f4>
 db 00Fh,011h,0A0h,040h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa40], xmm4
 db 083h,0B8h,094h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x794], 0x0
 db 00Fh,084h,04Bh,002h,000h,000h ; je	0x39c <_tnk_light_h+0x39c>
 db 083h,0B8h,0B4h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7b4], 0x0
 db 00Fh,085h,00Ah,002h,000h,000h ; jne	0x368 <_tnk_light_h+0x368>
 db 083h,0B8h,0A4h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7a4], 0x0
 db 00Fh,084h,07Fh,001h,000h,000h ; je	0x2ea <_tnk_light_h+0x2ea>
 db 00Fh,010h,080h,0F0h,007h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x7f0]
 db 00Fh,010h,088h,000h,008h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x800]
 db 00Fh,010h,090h,010h,008h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x810]
 db 00Fh,010h,098h,080h,002h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x280]
 db 00Fh,010h,0A0h,090h,002h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x290]
 db 00Fh,059h,0D8h ; mulps	xmm3, xmm0
 db 00Fh,059h,0E0h ; mulps	xmm4, xmm0
 db 00Fh,010h,0A8h,0B0h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x2b0]
 db 00Fh,010h,0B0h,0C0h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x2c0]
 db 00Fh,059h,0E9h ; mulps	xmm5, xmm1
 db 00Fh,059h,0F1h ; mulps	xmm6, xmm1
 db 00Fh,058h,0DDh ; addps	xmm3, xmm5
 db 00Fh,058h,0E6h ; addps	xmm4, xmm6
 db 00Fh,010h,0A8h,0E0h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x2e0]
 db 00Fh,010h,0B0h,0F0h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x2f0]
 db 00Fh,059h,0EAh ; mulps	xmm5, xmm2
 db 00Fh,059h,0F2h ; mulps	xmm6, xmm2
 db 00Fh,058h,0DDh ; addps	xmm3, xmm5
 db 00Fh,058h,0E6h ; addps	xmm4, xmm6
 db 00Fh,010h,0A8h,0A0h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x2a0]
 db 00Fh,059h,0E8h ; mulps	xmm5, xmm0
 db 00Fh,010h,0B0h,0D0h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x2d0]
 db 00Fh,059h,0F1h ; mulps	xmm6, xmm1
 db 00Fh,058h,0EEh ; addps	xmm5, xmm6
 db 00Fh,010h,0B0h,000h,003h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x300]
 db 00Fh,059h,0F2h ; mulps	xmm6, xmm2
 db 00Fh,058h,0EEh ; addps	xmm5, xmm6
 db 00Fh,028h,0CBh ; movaps	xmm1, xmm3
 db 00Fh,028h,0D4h ; movaps	xmm2, xmm4
 db 00Fh,028h,0DDh ; movaps	xmm3, xmm5
 db 083h,0B8h,0A0h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7a0], 0x0
 db 074h,06Eh ; je	0x26c <_tnk_light_h+0x26c>
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0E2h ; mulps	xmm4, xmm2
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,059h,0E3h ; mulps	xmm4, xmm3
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,051h,0C0h ; sqrtps	xmm0, xmm0
 db 00Fh,057h,0F6h ; xorps	xmm6, xmm6
 db 00Fh,0C2h,0F0h,001h ; cmpltps	xmm6, xmm0
 db 00Fh,010h,0B8h,070h,008h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x870]
 db 00Fh,05Eh,0F8h ; divps	xmm7, xmm0
 db 00Fh,054h,0FEh ; andps	xmm7, xmm6
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,0C7h ; mulps	xmm0, xmm7
 db 00Fh,054h,0C6h ; andps	xmm0, xmm6
 db 00Fh,028h,0E6h ; movaps	xmm4, xmm6
 db 00Fh,055h,0E1h ; andnps	xmm4, xmm1
 db 00Fh,056h,0C4h ; orps	xmm0, xmm4
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0E7h ; mulps	xmm4, xmm7
 db 00Fh,054h,0E6h ; andps	xmm4, xmm6
 db 00Fh,028h,0EEh ; movaps	xmm5, xmm6
 db 00Fh,055h,0EAh ; andnps	xmm5, xmm2
 db 00Fh,056h,0E5h ; orps	xmm4, xmm5
 db 00Fh,028h,0EBh ; movaps	xmm5, xmm3
 db 00Fh,059h,0EFh ; mulps	xmm5, xmm7
 db 00Fh,054h,0EEh ; andps	xmm5, xmm6
 db 00Fh,028h,0FEh ; movaps	xmm7, xmm6
 db 00Fh,055h,0FBh ; andnps	xmm7, xmm3
 db 00Fh,056h,0EFh ; orps	xmm5, xmm7
 db 00Fh,028h,0C8h ; movaps	xmm1, xmm0
 db 00Fh,028h,0D4h ; movaps	xmm2, xmm4
 db 00Fh,028h,0DDh ; movaps	xmm3, xmm5
 db 00Fh,010h,0A8h,010h,007h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x710]
 db 00Fh,010h,0B0h,020h,007h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x720]
 db 00Fh,010h,0B8h,030h,007h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x730]
 db 08Bh,088h,090h,007h,000h,000h ; mov	ecx, dword ptr [eax + 0x790]
 db 085h,0C9h ; test	ecx, ecx
 db 074h,074h ; je	0x2ff <_tnk_light_h+0x2ff>
 db 089h,0C2h ; mov	edx, eax
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,082h,010h,003h,000h,000h ; mulps	xmm0, xmmword ptr [edx + 0x310]
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0A2h,020h,003h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x320]
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,059h,0A2h,030h,003h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x330]
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,0C2h,0E0h,001h ; cmpltps	xmm4, xmm0
 db 00Fh,054h,0C4h ; andps	xmm0, xmm4
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,059h,0A2h,010h,005h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x510]
 db 00Fh,058h,0ECh ; addps	xmm5, xmm4
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,059h,0A2h,020h,005h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x520]
 db 00Fh,058h,0F4h ; addps	xmm6, xmm4
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,059h,0A2h,030h,005h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x530]
 db 00Fh,058h,0FCh ; addps	xmm7, xmm4
 db 083h,0C2h,040h ; add	edx, 0x40
 db 049h ; dec	ecx
 db 075h,0A5h ; jne	0x28d <_tnk_light_h+0x28d>
 db 0EBh,015h ; jmp	0x2ff <_tnk_light_h+0x2ff>
 db 00Fh,010h,0A8h,010h,007h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x710]
 db 00Fh,010h,0B0h,020h,007h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x720]
 db 00Fh,010h,0B8h,030h,007h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x730]
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,059h,0A8h,060h,008h,000h,000h ; mulps	xmm5, xmmword ptr [eax + 0x860]
 db 066h,00Fh,05Bh,0EDh ; cvtps2dq	xmm5, xmm5
 db 066h,00Fh,06Bh,0EDh ; packssdw	xmm5, xmm5
 db 066h,00Fh,067h,0E9h ; packuswb	xmm5, xmm1
 db 00Fh,059h,0B0h,060h,008h,000h,000h ; mulps	xmm6, xmmword ptr [eax + 0x860]
 db 066h,00Fh,05Bh,0F6h ; cvtps2dq	xmm6, xmm6
 db 066h,00Fh,06Bh,0F6h ; packssdw	xmm6, xmm6
 db 066h,00Fh,067h,0F1h ; packuswb	xmm6, xmm1
 db 00Fh,059h,0B8h,060h,008h,000h,000h ; mulps	xmm7, xmmword ptr [eax + 0x860]
 db 066h,00Fh,05Bh,0FFh ; cvtps2dq	xmm7, xmm7
 db 066h,00Fh,06Bh,0FFh ; packssdw	xmm7, xmm7
 db 066h,00Fh,067h,0F9h ; packuswb	xmm7, xmm1
 db 00Fh,010h,080h,040h,007h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x740]
 db 00Fh,059h,080h,060h,008h,000h,000h ; mulps	xmm0, xmmword ptr [eax + 0x860]
 db 066h,00Fh,05Bh,0C0h ; cvtps2dq	xmm0, xmm0
 db 066h,00Fh,06Bh,0C0h ; packssdw	xmm0, xmm0
 db 066h,00Fh,067h,0C1h ; packuswb	xmm0, xmm1
 db 066h,00Fh,060h,0EFh ; punpcklbw	xmm5, xmm7      # xmm5 = xmm5[0],xmm7[0],xmm5[1],xmm7[1],xmm5[2],xmm7[2],xmm5[3],xmm7[3],xmm5[4],xmm7[4],xmm5[5],xmm7[5],xmm5[6],xmm7[6],xmm5[7],xmm7[7]
 db 066h,00Fh,060h,0F0h ; punpcklbw	xmm6, xmm0      # xmm6 = xmm6[0],xmm0[0],xmm6[1],xmm0[1],xmm6[2],xmm0[2],xmm6[3],xmm0[3],xmm6[4],xmm0[4],xmm6[5],xmm0[5],xmm6[6],xmm0[6],xmm6[7],xmm0[7]
 db 066h,00Fh,060h,0EEh ; punpcklbw	xmm5, xmm6      # xmm5 = xmm5[0],xmm6[0],xmm5[1],xmm6[1],xmm5[2],xmm6[2],xmm5[3],xmm6[3],xmm5[4],xmm6[4],xmm5[5],xmm6[5],xmm5[6],xmm6[6],xmm5[7],xmm6[7]
 db 00Fh,011h,0A8h,040h,008h,000h,000h ; movups	xmmword ptr [eax + 0x840], xmm5
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,010h,080h,040h,00Ah,000h,000h ; movups	xmm0, xmmword ptr [eax + 0xa40]
 db 00Fh,059h,080h,060h,008h,000h,000h ; mulps	xmm0, xmmword ptr [eax + 0x860]
 db 066h,00Fh,05Bh,0C0h ; cvtps2dq	xmm0, xmm0
 db 066h,00Fh,06Bh,0C0h ; packssdw	xmm0, xmm0
 db 066h,00Fh,067h,0C1h ; packuswb	xmm0, xmm1
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 066h,00Fh,060h,0D0h ; punpcklbw	xmm2, xmm0      # xmm2 = xmm2[0],xmm0[0],xmm2[1],xmm0[1],xmm2[2],xmm0[2],xmm2[3],xmm0[3],xmm2[4],xmm0[4],xmm2[5],xmm0[5],xmm2[6],xmm0[6],xmm2[7],xmm0[7]
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 066h,00Fh,060h,0DAh ; punpcklbw	xmm3, xmm2      # xmm3 = xmm3[0],xmm2[0],xmm3[1],xmm2[1],xmm3[2],xmm2[2],xmm3[3],xmm2[3],xmm3[4],xmm2[4],xmm3[5],xmm2[5],xmm3[6],xmm2[6],xmm3[7],xmm2[7]
 db 00Fh,011h,098h,050h,008h,000h,000h ; movups	xmmword ptr [eax + 0x850], xmm3
 db 0EBh,053h ; jmp	0x3ef <_tnk_light_h+0x3ef>
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,010h,080h,040h,00Ah,000h,000h ; movups	xmm0, xmmword ptr [eax + 0xa40]
 db 00Fh,059h,080h,060h,008h,000h,000h ; mulps	xmm0, xmmword ptr [eax + 0x860]
 db 066h,00Fh,05Bh,0C0h ; cvtps2dq	xmm0, xmm0
 db 066h,00Fh,06Bh,0C0h ; packssdw	xmm0, xmm0
 db 066h,00Fh,067h,0C1h ; packuswb	xmm0, xmm1
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 066h,00Fh,060h,0D0h ; punpcklbw	xmm2, xmm0      # xmm2 = xmm2[0],xmm0[0],xmm2[1],xmm0[1],xmm2[2],xmm0[2],xmm2[3],xmm0[3],xmm2[4],xmm0[4],xmm2[5],xmm0[5],xmm2[6],xmm0[6],xmm2[7],xmm0[7]
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 066h,00Fh,060h,0DAh ; punpcklbw	xmm3, xmm2      # xmm3 = xmm3[0],xmm2[0],xmm3[1],xmm2[1],xmm3[2],xmm2[2],xmm3[3],xmm2[3],xmm3[4],xmm2[4],xmm3[5],xmm2[5],xmm3[6],xmm2[6],xmm3[7],xmm2[7]
 db 00Fh,010h,080h,020h,008h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x820]
 db 00Fh,011h,080h,040h,008h,000h,000h ; movups	xmmword ptr [eax + 0x840], xmm0
 db 00Fh,010h,080h,030h,008h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x830]
 db 066h,00Fh,0DBh,080h,060h,009h,000h,000h ; pand	xmm0, xmmword ptr [eax + 0x960]
 db 066h,00Fh,0EBh,0C3h ; por	xmm0, xmm3
 db 00Fh,011h,080h,050h,008h,000h,000h ; movups	xmmword ptr [eax + 0x850], xmm0
 db 05Dh ; pop	ebp
 db 05Fh ; pop	edi
 db 05Eh ; pop	esi
 db 05Bh ; pop	ebx
 db 0C3h ; ret
 db 00Fh,028h,0DAh ; movaps	xmm3, xmm2
 db 00Fh,010h,080h,020h,009h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x920]
 db 00Fh,0C2h,0C2h,001h ; cmpltps	xmm0, xmm2
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,028h,0CAh ; movaps	xmm1, xmm2
 db 00Fh,0C2h,0CCh,001h ; cmpltps	xmm1, xmm4
 db 00Fh,05Fh,090h,020h,009h,000h,000h ; maxps	xmm2, xmmword ptr [eax + 0x920]
 db 00Fh,05Dh,0D4h ; minps	xmm2, xmm4
 db 00Fh,059h,090h,090h,008h,000h,000h ; mulps	xmm2, xmmword ptr [eax + 0x890]
 db 0F3h,00Fh,05Bh,0E2h ; cvttps2dq	xmm4, xmm2
 db 00Fh,05Bh,0ECh ; cvtdq2ps	xmm5, xmm4
 db 00Fh,028h,0F2h ; movaps	xmm6, xmm2
 db 00Fh,0C2h,0F5h,001h ; cmpltps	xmm6, xmm5
 db 066h,00Fh,072h,0D6h,01Fh ; psrld	xmm6, 0x1f
 db 066h,00Fh,0FAh,0E6h ; psubd	xmm4, xmm6
 db 00Fh,05Bh,0ECh ; cvtdq2ps	xmm5, xmm4
 db 00Fh,028h,0F2h ; movaps	xmm6, xmm2
 db 00Fh,05Ch,0F5h ; subps	xmm6, xmm5
 db 00Fh,010h,0A8h,000h,009h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x900]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0F0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8f0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0E0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8e0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0D0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8d0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0C0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8c0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0B0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8b0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0A0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8a0]
 db 066h,00Fh,0FEh,0A0h,050h,009h,000h,000h ; paddd	xmm4, xmmword ptr [eax + 0x950]
 db 066h,00Fh,072h,0F4h,017h ; pslld	xmm4, 0x17
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,054h,0E8h ; andps	xmm5, xmm0
 db 00Fh,054h,0E9h ; andps	xmm5, xmm1
 db 00Fh,010h,0A0h,070h,008h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x870]
 db 00Fh,057h,0C0h ; xorps	xmm0, xmm0
 db 00Fh,028h,0F8h ; movaps	xmm7, xmm0
 db 00Fh,0C2h,0FBh,002h ; cmpleps	xmm7, xmm3
 db 00Fh,054h,0E7h ; andps	xmm4, xmm7
 db 00Fh,056h,0ECh ; orps	xmm5, xmm4
 db 00Fh,028h,0E5h ; movaps	xmm4, xmm5
 db 0C3h ; ret
_tnk_light_h endp
public _tnk_plight
_tnk_plight proc near
 db 053h ; push	ebx
 db 056h ; push	esi
 db 057h ; push	edi
 db 055h ; push	ebp
 db 08Bh,044h,024h,014h ; mov	eax, dword ptr [esp + 0x14]
 db 00Fh,010h,0A8h,0C0h,007h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x7c0]
 db 00Fh,010h,0B0h,0D0h,007h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x7d0]
 db 00Fh,010h,0B8h,0E0h,007h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x7e0]
 db 00Fh,010h,080h,080h,001h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x180]
 db 00Fh,010h,088h,090h,001h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x190]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,059h,0CDh ; mulps	xmm1, xmm5
 db 00Fh,010h,090h,0C0h,001h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x1c0]
 db 00Fh,010h,098h,0D0h,001h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x1d0]
 db 00Fh,059h,0D6h ; mulps	xmm2, xmm6
 db 00Fh,059h,0DEh ; mulps	xmm3, xmm6
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,010h,090h,000h,002h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x200]
 db 00Fh,010h,098h,010h,002h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x210]
 db 00Fh,059h,0D7h ; mulps	xmm2, xmm7
 db 00Fh,059h,0DFh ; mulps	xmm3, xmm7
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,010h,090h,040h,002h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x240]
 db 00Fh,010h,098h,050h,002h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x250]
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,011h,080h,010h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa10], xmm0
 db 00Fh,011h,088h,020h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa20], xmm1
 db 00Fh,010h,080h,0A0h,001h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x1a0]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,010h,090h,0E0h,001h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x1e0]
 db 00Fh,059h,0D6h ; mulps	xmm2, xmm6
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,010h,090h,020h,002h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x220]
 db 00Fh,059h,0D7h ; mulps	xmm2, xmm7
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,010h,090h,060h,002h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x260]
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,011h,080h,030h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa30], xmm0
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,057h,0D2h ; xorps	xmm2, xmm2
 db 00Fh,057h,0DBh ; xorps	xmm3, xmm3
 db 083h,0B8h,0A4h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7a4], 0x0
 db 00Fh,084h,001h,001h,000h,000h ; je	0x1d3 <_tnk_plight+0x1d3>
 db 00Fh,010h,080h,0F0h,007h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x7f0]
 db 00Fh,010h,088h,000h,008h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x800]
 db 00Fh,010h,090h,010h,008h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x810]
 db 00Fh,010h,098h,080h,002h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x280]
 db 00Fh,010h,0A0h,090h,002h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x290]
 db 00Fh,059h,0D8h ; mulps	xmm3, xmm0
 db 00Fh,059h,0E0h ; mulps	xmm4, xmm0
 db 00Fh,010h,0A8h,0B0h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x2b0]
 db 00Fh,010h,0B0h,0C0h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x2c0]
 db 00Fh,059h,0E9h ; mulps	xmm5, xmm1
 db 00Fh,059h,0F1h ; mulps	xmm6, xmm1
 db 00Fh,058h,0DDh ; addps	xmm3, xmm5
 db 00Fh,058h,0E6h ; addps	xmm4, xmm6
 db 00Fh,010h,0A8h,0E0h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x2e0]
 db 00Fh,010h,0B0h,0F0h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x2f0]
 db 00Fh,059h,0EAh ; mulps	xmm5, xmm2
 db 00Fh,059h,0F2h ; mulps	xmm6, xmm2
 db 00Fh,058h,0DDh ; addps	xmm3, xmm5
 db 00Fh,058h,0E6h ; addps	xmm4, xmm6
 db 00Fh,010h,0A8h,0A0h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x2a0]
 db 00Fh,059h,0E8h ; mulps	xmm5, xmm0
 db 00Fh,010h,0B0h,0D0h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x2d0]
 db 00Fh,059h,0F1h ; mulps	xmm6, xmm1
 db 00Fh,058h,0EEh ; addps	xmm5, xmm6
 db 00Fh,010h,0B0h,000h,003h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x300]
 db 00Fh,059h,0F2h ; mulps	xmm6, xmm2
 db 00Fh,058h,0EEh ; addps	xmm5, xmm6
 db 00Fh,028h,0CBh ; movaps	xmm1, xmm3
 db 00Fh,028h,0D4h ; movaps	xmm2, xmm4
 db 00Fh,028h,0DDh ; movaps	xmm3, xmm5
 db 083h,0B8h,0A0h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7a0], 0x0
 db 074h,06Eh ; je	0x1d3 <_tnk_plight+0x1d3>
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0E2h ; mulps	xmm4, xmm2
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,059h,0E3h ; mulps	xmm4, xmm3
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,051h,0C0h ; sqrtps	xmm0, xmm0
 db 00Fh,057h,0F6h ; xorps	xmm6, xmm6
 db 00Fh,0C2h,0F0h,001h ; cmpltps	xmm6, xmm0
 db 00Fh,010h,0B8h,070h,008h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x870]
 db 00Fh,05Eh,0F8h ; divps	xmm7, xmm0
 db 00Fh,054h,0FEh ; andps	xmm7, xmm6
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,0C7h ; mulps	xmm0, xmm7
 db 00Fh,054h,0C6h ; andps	xmm0, xmm6
 db 00Fh,028h,0E6h ; movaps	xmm4, xmm6
 db 00Fh,055h,0E1h ; andnps	xmm4, xmm1
 db 00Fh,056h,0C4h ; orps	xmm0, xmm4
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0E7h ; mulps	xmm4, xmm7
 db 00Fh,054h,0E6h ; andps	xmm4, xmm6
 db 00Fh,028h,0EEh ; movaps	xmm5, xmm6
 db 00Fh,055h,0EAh ; andnps	xmm5, xmm2
 db 00Fh,056h,0E5h ; orps	xmm4, xmm5
 db 00Fh,028h,0EBh ; movaps	xmm5, xmm3
 db 00Fh,059h,0EFh ; mulps	xmm5, xmm7
 db 00Fh,054h,0EEh ; andps	xmm5, xmm6
 db 00Fh,028h,0FEh ; movaps	xmm7, xmm6
 db 00Fh,055h,0FBh ; andnps	xmm7, xmm3
 db 00Fh,056h,0EFh ; orps	xmm5, xmm7
 db 00Fh,028h,0C8h ; movaps	xmm1, xmm0
 db 00Fh,028h,0D4h ; movaps	xmm2, xmm4
 db 00Fh,028h,0DDh ; movaps	xmm3, xmm5
 db 00Fh,011h,088h,040h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa40], xmm1
 db 00Fh,011h,090h,050h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa50], xmm2
 db 00Fh,011h,098h,060h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa60], xmm3
 db 00Fh,010h,0A8h,090h,022h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x2290]
 db 00Fh,010h,0B0h,0A0h,022h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x22a0]
 db 00Fh,010h,0B8h,0B0h,022h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x22b0]
 db 08Bh,088h,090h,00Ah,000h,000h ; mov	ecx, dword ptr [eax + 0xa90]
 db 085h,0C9h ; test	ecx, ecx
 db 00Fh,084h,0F8h,001h,000h,000h ; je	0x403 <_tnk_plight+0x403>
 db 089h,0C2h ; mov	edx, eax
 db 089h,0C3h ; mov	ebx, eax
 db 0BDh,001h,000h,000h,000h ; mov	ebp, 0x1
 db 085h,0A8h,0E8h,023h,000h,000h ; test	dword ptr [eax + 0x23e8], ebp
 db 00Fh,085h,0D4h,001h,000h,000h ; jne	0x3f4 <_tnk_plight+0x3f4>
 db 00Fh,010h,08Ah,0A0h,00Ah,000h,000h ; movups	xmm1, xmmword ptr [edx + 0xaa0]
 db 00Fh,05Ch,088h,010h,00Ah,000h,000h ; subps	xmm1, xmmword ptr [eax + 0xa10]
 db 00Fh,010h,092h,0B0h,00Ah,000h,000h ; movups	xmm2, xmmword ptr [edx + 0xab0]
 db 00Fh,05Ch,090h,020h,00Ah,000h,000h ; subps	xmm2, xmmword ptr [eax + 0xa20]
 db 00Fh,010h,09Ah,0C0h,00Ah,000h,000h ; movups	xmm3, xmmword ptr [edx + 0xac0]
 db 00Fh,05Ch,098h,030h,00Ah,000h,000h ; subps	xmm3, xmmword ptr [eax + 0xa30]
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0E2h ; mulps	xmm4, xmm2
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,059h,0E3h ; mulps	xmm4, xmm3
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,011h,080h,070h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa70], xmm0
 db 00Fh,052h,0E0h ; rsqrtps	xmm4, xmm0
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,05Ch,080h,070h,008h,000h,000h ; subps	xmm0, xmmword ptr [eax + 0x870]
 db 00Fh,059h,080h,080h,008h,000h,000h ; mulps	xmm0, xmmword ptr [eax + 0x880]
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,05Ch,0E0h ; subps	xmm4, xmm0
 db 00Fh,057h,0C0h ; xorps	xmm0, xmm0
 db 00Fh,0C2h,080h,070h,00Ah,000h,000h,001h ; cmpltps	xmm0, xmmword ptr [eax + 0xa70]
 db 00Fh,011h,080h,080h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa80], xmm0
 db 00Fh,010h,080h,070h,00Ah,000h,000h ; movups	xmm0, xmmword ptr [eax + 0xa70]
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,054h,080h,080h,00Ah,000h,000h ; andps	xmm0, xmmword ptr [eax + 0xa80]
 db 00Fh,011h,080h,070h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa70], xmm0
 db 00Fh,0C2h,083h,020h,00Ch,000h,000h,002h ; cmpleps	xmm0, xmmword ptr [ebx + 0xc20]
 db 00Fh,050h,0F0h ; movmskps	esi, xmm0
 db 085h,0F6h ; test	esi, esi
 db 00Fh,084h,031h,001h,000h,000h ; je	0x3f4 <_tnk_plight+0x3f4>
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,057h,0C1h ; xorps	xmm0, xmm1
 db 00Fh,054h,080h,080h,00Ah,000h,000h ; andps	xmm0, xmmword ptr [eax + 0xa80]
 db 00Fh,057h,0C8h ; xorps	xmm1, xmm0
 db 00Fh,028h,0C2h ; movaps	xmm0, xmm2
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,057h,0C2h ; xorps	xmm0, xmm2
 db 00Fh,054h,080h,080h,00Ah,000h,000h ; andps	xmm0, xmmword ptr [eax + 0xa80]
 db 00Fh,057h,0D0h ; xorps	xmm2, xmm0
 db 00Fh,028h,0C3h ; movaps	xmm0, xmm3
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,057h,0C3h ; xorps	xmm0, xmm3
 db 00Fh,054h,080h,080h,00Ah,000h,000h ; andps	xmm0, xmmword ptr [eax + 0xa80]
 db 00Fh,057h,0D8h ; xorps	xmm3, xmm0
 db 00Fh,010h,080h,070h,00Ah,000h,000h ; movups	xmm0, xmmword ptr [eax + 0xa70]
 db 00Fh,010h,0A3h,040h,00Ch,000h,000h ; movups	xmm4, xmmword ptr [ebx + 0xc40]
 db 00Fh,059h,0E0h ; mulps	xmm4, xmm0
 db 00Fh,058h,0A3h,030h,00Ch,000h,000h ; addps	xmm4, xmmword ptr [ebx + 0xc30]
 db 00Fh,059h,083h,050h,00Ch,000h,000h ; mulps	xmm0, xmmword ptr [ebx + 0xc50]
 db 00Fh,059h,080h,070h,00Ah,000h,000h ; mulps	xmm0, xmmword ptr [eax + 0xa70]
 db 00Fh,058h,0E0h ; addps	xmm4, xmm0
 db 00Fh,057h,0C0h ; xorps	xmm0, xmm0
 db 00Fh,0C2h,0C4h,001h ; cmpltps	xmm0, xmm4
 db 00Fh,011h,080h,080h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa80], xmm0
 db 00Fh,053h,0C4h ; rcpps	xmm0, xmm4
 db 00Fh,059h,0E0h ; mulps	xmm4, xmm0
 db 00Fh,05Ch,0A0h,070h,008h,000h,000h ; subps	xmm4, xmmword ptr [eax + 0x870]
 db 00Fh,059h,0E0h ; mulps	xmm4, xmm0
 db 00Fh,05Ch,0C4h ; subps	xmm0, xmm4
 db 00Fh,057h,080h,070h,008h,000h,000h ; xorps	xmm0, xmmword ptr [eax + 0x870]
 db 00Fh,054h,080h,080h,00Ah,000h,000h ; andps	xmm0, xmmword ptr [eax + 0xa80]
 db 00Fh,057h,080h,070h,008h,000h,000h ; xorps	xmm0, xmmword ptr [eax + 0x870]
 db 00Fh,010h,0A0h,070h,00Ah,000h,000h ; movups	xmm4, xmmword ptr [eax + 0xa70]
 db 00Fh,0C2h,0A3h,020h,00Ch,000h,000h,002h ; cmpleps	xmm4, xmmword ptr [ebx + 0xc20]
 db 00Fh,054h,0C4h ; andps	xmm0, xmm4
 db 085h,0A8h,0D8h,022h,000h,000h ; test	dword ptr [eax + 0x22d8], ebp
 db 074h,027h ; je	0x39c <_tnk_plight+0x39c>
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,059h,0A2h,0A0h,00Fh,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0xfa0]
 db 00Fh,058h,0ECh ; addps	xmm5, xmm4
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,059h,0A2h,0B0h,00Fh,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0xfb0]
 db 00Fh,058h,0F4h ; addps	xmm6, xmm4
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,059h,0A2h,0C0h,00Fh,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0xfc0]
 db 00Fh,058h,0FCh ; addps	xmm7, xmm4
 db 083h,0B8h,0A4h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7a4], 0x0
 db 074h,04Fh ; je	0x3f4 <_tnk_plight+0x3f4>
 db 00Fh,059h,088h,040h,00Ah,000h,000h ; mulps	xmm1, xmmword ptr [eax + 0xa40]
 db 00Fh,059h,090h,050h,00Ah,000h,000h ; mulps	xmm2, xmmword ptr [eax + 0xa50]
 db 00Fh,058h,0CAh ; addps	xmm1, xmm2
 db 00Fh,059h,098h,060h,00Ah,000h,000h ; mulps	xmm3, xmmword ptr [eax + 0xa60]
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,0C2h,0E1h,001h ; cmpltps	xmm4, xmm1
 db 00Fh,054h,0CCh ; andps	xmm1, xmm4
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,028h,0E1h ; movaps	xmm4, xmm1
 db 00Fh,059h,0A2h,020h,00Eh,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0xe20]
 db 00Fh,058h,0ECh ; addps	xmm5, xmm4
 db 00Fh,028h,0E1h ; movaps	xmm4, xmm1
 db 00Fh,059h,0A2h,030h,00Eh,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0xe30]
 db 00Fh,058h,0F4h ; addps	xmm6, xmm4
 db 00Fh,028h,0E1h ; movaps	xmm4, xmm1
 db 00Fh,059h,0A2h,040h,00Eh,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0xe40]
 db 00Fh,058h,0FCh ; addps	xmm7, xmm4
 db 083h,0C2h,030h ; add	edx, 0x30
 db 083h,0C3h,040h ; add	ebx, 0x40
 db 001h,0EDh ; add	ebp, ebp
 db 049h ; dec	ecx
 db 00Fh,085h,011h,0FEh,0FFh,0FFh ; jne	0x214 <_tnk_plight+0x214>
 db 083h,0B8h,0A4h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7a4], 0x0
 db 074h,034h ; je	0x440 <_tnk_plight+0x440>
 db 00Fh,010h,080h,040h,00Ah,000h,000h ; movups	xmm0, xmmword ptr [eax + 0xa40]
 db 00Fh,011h,080h,080h,023h,000h,000h ; movups	xmmword ptr [eax + 0x2380], xmm0
 db 00Fh,010h,080h,050h,00Ah,000h,000h ; movups	xmm0, xmmword ptr [eax + 0xa50]
 db 00Fh,011h,080h,090h,023h,000h,000h ; movups	xmmword ptr [eax + 0x2390], xmm0
 db 00Fh,010h,080h,060h,00Ah,000h,000h ; movups	xmm0, xmmword ptr [eax + 0xa60]
 db 00Fh,011h,080h,0A0h,023h,000h,000h ; movups	xmmword ptr [eax + 0x23a0], xmm0
 db 0C7h,080h,0E0h,023h,000h,000h,001h,000h,000h,000h ; mov	dword ptr [eax + 0x23e0], 0x1
 db 00Fh,011h,0A8h,010h,007h,000h,000h ; movups	xmmword ptr [eax + 0x710], xmm5
 db 00Fh,011h,0B0h,020h,007h,000h,000h ; movups	xmmword ptr [eax + 0x720], xmm6
 db 00Fh,011h,0B8h,030h,007h,000h,000h ; movups	xmmword ptr [eax + 0x730], xmm7
 db 00Fh,010h,080h,0C0h,022h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x22c0]
 db 00Fh,011h,080h,040h,007h,000h,000h ; movups	xmmword ptr [eax + 0x740], xmm0
 db 05Dh ; pop	ebp
 db 05Fh ; pop	edi
 db 05Eh ; pop	esi
 db 05Bh ; pop	ebx
 db 0C3h ; ret
_tnk_plight endp
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
public _tnk_light
_tnk_light proc near
 db 053h ; push	ebx
 db 056h ; push	esi
 db 057h ; push	edi
 db 055h ; push	ebp
 db 08Bh,044h,024h,014h ; mov	eax, dword ptr [esp + 0x14]
 db 00Fh,010h,0A0h,070h,008h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x870]
 db 08Bh,0B0h,098h,007h,000h,000h ; mov	esi, dword ptr [eax + 0x798]
 db 085h,0F6h ; test	esi, esi
 db 00Fh,084h,033h,002h,000h,000h ; je	0x250 <.text+0x250>
 db 083h,0B8h,0B8h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7b8], 0x0
 db 074h,016h ; je	0x3c <.text+0x3c>
 db 00Fh,010h,080h,0E0h,007h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x7e0]
 db 00Fh,010h,088h,040h,009h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x940]
 db 00Fh,055h,0C8h ; andnps	xmm1, xmm0
 db 0E9h,025h,001h,000h,000h ; jmp	0x161 <.text+0x161>
 db 083h,0B8h,0E4h,023h,000h,000h,000h ; cmp	dword ptr [eax + 0x23e4], 0x0
 db 074h,027h ; je	0x6c <.text+0x6c>
 db 00Fh,010h,080h,0D0h,023h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x23d0]
 db 083h,0B8h,09Ch,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x79c], 0x0
 db 00Fh,084h,0FEh,000h,000h,000h ; je	0x157 <.text+0x157>
 db 00Fh,010h,098h,0B0h,023h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x23b0]
 db 00Fh,010h,088h,0C0h,023h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x23c0]
 db 0E9h,0D7h,000h,000h,000h ; jmp	0x143 <.text+0x143>
 db 00Fh,010h,0A8h,0C0h,007h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x7c0]
 db 00Fh,010h,0B0h,0D0h,007h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x7d0]
 db 00Fh,010h,0B8h,0E0h,007h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x7e0]
 db 00Fh,010h,080h,0A0h,001h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x1a0]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,010h,088h,0E0h,001h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1e0]
 db 00Fh,059h,0CEh ; mulps	xmm1, xmm6
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,020h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x220]
 db 00Fh,059h,0CFh ; mulps	xmm1, xmm7
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,060h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x260]
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 083h,0B8h,09Ch,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x79c], 0x0
 db 00Fh,084h,09Bh,000h,000h,000h ; je	0x157 <.text+0x157>
 db 00Fh,011h,080h,0D0h,023h,000h,000h ; movups	xmmword ptr [eax + 0x23d0], xmm0
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,010h,080h,080h,001h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x180]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,010h,088h,0C0h,001h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1c0]
 db 00Fh,059h,0CEh ; mulps	xmm1, xmm6
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,000h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x200]
 db 00Fh,059h,0CFh ; mulps	xmm1, xmm7
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,040h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x240]
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,011h,080h,0B0h,023h,000h,000h ; movups	xmmword ptr [eax + 0x23b0], xmm0
 db 00Fh,028h,0D8h ; movaps	xmm3, xmm0
 db 00Fh,010h,080h,090h,001h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x190]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,010h,088h,0D0h,001h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1d0]
 db 00Fh,059h,0CEh ; mulps	xmm1, xmm6
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,010h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x210]
 db 00Fh,059h,0CFh ; mulps	xmm1, xmm7
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,050h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x250]
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,011h,080h,0C0h,023h,000h,000h ; movups	xmmword ptr [eax + 0x23c0], xmm0
 db 00Fh,028h,0C8h ; movaps	xmm1, xmm0
 db 00Fh,028h,0C4h ; movaps	xmm0, xmm4
 db 0C7h,080h,0E4h,023h,000h,000h,001h,000h,000h,000h ; mov	dword ptr [eax + 0x23e4], 0x1
 db 00Fh,059h,0DBh ; mulps	xmm3, xmm3
 db 00Fh,059h,0C9h ; mulps	xmm1, xmm1
 db 00Fh,059h,0C0h ; mulps	xmm0, xmm0
 db 00Fh,058h,0D9h ; addps	xmm3, xmm1
 db 00Fh,058h,0D8h ; addps	xmm3, xmm0
 db 00Fh,051h,0CBh ; sqrtps	xmm1, xmm3
 db 0EBh,00Ah ; jmp	0x161 <.text+0x161>
 db 00Fh,010h,088h,040h,009h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x940]
 db 00Fh,055h,0C8h ; andnps	xmm1, xmm0
 db 083h,0FEh,003h ; cmp	esi, 0x3
 db 075h,016h ; jne	0x17c <.text+0x17c>
 db 00Fh,010h,0A0h,060h,007h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x760]
 db 00Fh,05Ch,0E1h ; subps	xmm4, xmm1
 db 00Fh,05Eh,0A0h,070h,007h,000h,000h ; divps	xmm4, xmmword ptr [eax + 0x770]
 db 0E9h,0D4h,000h,000h,000h ; jmp	0x250 <.text+0x250>
 db 00Fh,010h,080h,050h,007h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x750]
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 083h,0FEh,001h ; cmp	esi, 0x1
 db 074h,003h ; je	0x18e <.text+0x18e>
 db 00Fh,059h,0C0h ; mulps	xmm0, xmm0
 db 00Fh,057h,0D2h ; xorps	xmm2, xmm2
 db 00Fh,05Ch,0D0h ; subps	xmm2, xmm0
 db 00Fh,028h,0DAh ; movaps	xmm3, xmm2
 db 00Fh,010h,080h,020h,009h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x920]
 db 00Fh,0C2h,0C2h,001h ; cmpltps	xmm0, xmm2
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,028h,0CAh ; movaps	xmm1, xmm2
 db 00Fh,0C2h,0CCh,001h ; cmpltps	xmm1, xmm4
 db 00Fh,05Fh,090h,020h,009h,000h,000h ; maxps	xmm2, xmmword ptr [eax + 0x920]
 db 00Fh,05Dh,0D4h ; minps	xmm2, xmm4
 db 00Fh,059h,090h,090h,008h,000h,000h ; mulps	xmm2, xmmword ptr [eax + 0x890]
 db 0F3h,00Fh,05Bh,0E2h ; cvttps2dq	xmm4, xmm2
 db 00Fh,05Bh,0ECh ; cvtdq2ps	xmm5, xmm4
 db 00Fh,028h,0F2h ; movaps	xmm6, xmm2
 db 00Fh,0C2h,0F5h,001h ; cmpltps	xmm6, xmm5
 db 066h,00Fh,072h,0D6h,01Fh ; psrld	xmm6, 0x1f
 db 066h,00Fh,0FAh,0E6h ; psubd	xmm4, xmm6
 db 00Fh,05Bh,0ECh ; cvtdq2ps	xmm5, xmm4
 db 00Fh,028h,0F2h ; movaps	xmm6, xmm2
 db 00Fh,05Ch,0F5h ; subps	xmm6, xmm5
 db 00Fh,010h,0A8h,000h,009h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x900]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0F0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8f0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0E0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8e0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0D0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8d0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0C0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8c0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0B0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8b0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0A0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8a0]
 db 066h,00Fh,0FEh,0A0h,050h,009h,000h,000h ; paddd	xmm4, xmmword ptr [eax + 0x950]
 db 066h,00Fh,072h,0F4h,017h ; pslld	xmm4, 0x17
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,054h,0E8h ; andps	xmm5, xmm0
 db 00Fh,054h,0E9h ; andps	xmm5, xmm1
 db 00Fh,010h,0A0h,070h,008h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x870]
 db 00Fh,057h,0C0h ; xorps	xmm0, xmm0
 db 00Fh,028h,0F8h ; movaps	xmm7, xmm0
 db 00Fh,0C2h,0FBh,002h ; cmpleps	xmm7, xmm3
 db 00Fh,054h,0E7h ; andps	xmm4, xmm7
 db 00Fh,056h,0ECh ; orps	xmm5, xmm4
 db 00Fh,028h,0E5h ; movaps	xmm4, xmm5
 db 00Fh,011h,0A0h,040h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa40], xmm4
 db 083h,0B8h,094h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x794], 0x0
 db 00Fh,084h,08Eh,002h,000h,000h ; je	0x4f2 <.text+0x4f2>
 db 083h,0B8h,0B4h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7b4], 0x0
 db 00Fh,085h,04Dh,002h,000h,000h ; jne	0x4be <.text+0x4be>
 db 083h,0B8h,0A4h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7a4], 0x0
 db 00Fh,084h,0C2h,001h,000h,000h ; je	0x440 <.text+0x440>
 db 083h,0B8h,0E0h,023h,000h,000h,000h ; cmp	dword ptr [eax + 0x23e0], 0x0
 db 00Fh,085h,022h,001h,000h,000h ; jne	0x3ad <.text+0x3ad>
 db 00Fh,010h,080h,0F0h,007h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x7f0]
 db 00Fh,010h,088h,000h,008h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x800]
 db 00Fh,010h,090h,010h,008h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x810]
 db 00Fh,010h,098h,080h,002h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x280]
 db 00Fh,010h,0A0h,090h,002h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x290]
 db 00Fh,059h,0D8h ; mulps	xmm3, xmm0
 db 00Fh,059h,0E0h ; mulps	xmm4, xmm0
 db 00Fh,010h,0A8h,0B0h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x2b0]
 db 00Fh,010h,0B0h,0C0h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x2c0]
 db 00Fh,059h,0E9h ; mulps	xmm5, xmm1
 db 00Fh,059h,0F1h ; mulps	xmm6, xmm1
 db 00Fh,058h,0DDh ; addps	xmm3, xmm5
 db 00Fh,058h,0E6h ; addps	xmm4, xmm6
 db 00Fh,010h,0A8h,0E0h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x2e0]
 db 00Fh,010h,0B0h,0F0h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x2f0]
 db 00Fh,059h,0EAh ; mulps	xmm5, xmm2
 db 00Fh,059h,0F2h ; mulps	xmm6, xmm2
 db 00Fh,058h,0DDh ; addps	xmm3, xmm5
 db 00Fh,058h,0E6h ; addps	xmm4, xmm6
 db 00Fh,010h,0A8h,0A0h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x2a0]
 db 00Fh,059h,0E8h ; mulps	xmm5, xmm0
 db 00Fh,010h,0B0h,0D0h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x2d0]
 db 00Fh,059h,0F1h ; mulps	xmm6, xmm1
 db 00Fh,058h,0EEh ; addps	xmm5, xmm6
 db 00Fh,010h,0B0h,000h,003h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x300]
 db 00Fh,059h,0F2h ; mulps	xmm6, xmm2
 db 00Fh,058h,0EEh ; addps	xmm5, xmm6
 db 00Fh,028h,0CBh ; movaps	xmm1, xmm3
 db 00Fh,028h,0D4h ; movaps	xmm2, xmm4
 db 00Fh,028h,0DDh ; movaps	xmm3, xmm5
 db 083h,0B8h,0A0h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7a0], 0x0
 db 074h,06Eh ; je	0x38c <.text+0x38c>
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0E2h ; mulps	xmm4, xmm2
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,059h,0E3h ; mulps	xmm4, xmm3
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,051h,0C0h ; sqrtps	xmm0, xmm0
 db 00Fh,057h,0F6h ; xorps	xmm6, xmm6
 db 00Fh,0C2h,0F0h,001h ; cmpltps	xmm6, xmm0
 db 00Fh,010h,0B8h,070h,008h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x870]
 db 00Fh,05Eh,0F8h ; divps	xmm7, xmm0
 db 00Fh,054h,0FEh ; andps	xmm7, xmm6
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,0C7h ; mulps	xmm0, xmm7
 db 00Fh,054h,0C6h ; andps	xmm0, xmm6
 db 00Fh,028h,0E6h ; movaps	xmm4, xmm6
 db 00Fh,055h,0E1h ; andnps	xmm4, xmm1
 db 00Fh,056h,0C4h ; orps	xmm0, xmm4
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0E7h ; mulps	xmm4, xmm7
 db 00Fh,054h,0E6h ; andps	xmm4, xmm6
 db 00Fh,028h,0EEh ; movaps	xmm5, xmm6
 db 00Fh,055h,0EAh ; andnps	xmm5, xmm2
 db 00Fh,056h,0E5h ; orps	xmm4, xmm5
 db 00Fh,028h,0EBh ; movaps	xmm5, xmm3
 db 00Fh,059h,0EFh ; mulps	xmm5, xmm7
 db 00Fh,054h,0EEh ; andps	xmm5, xmm6
 db 00Fh,028h,0FEh ; movaps	xmm7, xmm6
 db 00Fh,055h,0FBh ; andnps	xmm7, xmm3
 db 00Fh,056h,0EFh ; orps	xmm5, xmm7
 db 00Fh,028h,0C8h ; movaps	xmm1, xmm0
 db 00Fh,028h,0D4h ; movaps	xmm2, xmm4
 db 00Fh,028h,0DDh ; movaps	xmm3, xmm5
 db 00Fh,011h,088h,080h,023h,000h,000h ; movups	xmmword ptr [eax + 0x2380], xmm1
 db 00Fh,011h,090h,090h,023h,000h,000h ; movups	xmmword ptr [eax + 0x2390], xmm2
 db 00Fh,011h,098h,0A0h,023h,000h,000h ; movups	xmmword ptr [eax + 0x23a0], xmm3
 db 0C7h,080h,0E0h,023h,000h,000h,001h,000h,000h,000h ; mov	dword ptr [eax + 0x23e0], 0x1
 db 0EBh,015h ; jmp	0x3c2 <.text+0x3c2>
 db 00Fh,010h,088h,080h,023h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x2380]
 db 00Fh,010h,090h,090h,023h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x2390]
 db 00Fh,010h,098h,0A0h,023h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x23a0]
 db 00Fh,010h,0A8h,010h,007h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x710]
 db 00Fh,010h,0B0h,020h,007h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x720]
 db 00Fh,010h,0B8h,030h,007h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x730]
 db 08Bh,088h,090h,007h,000h,000h ; mov	ecx, dword ptr [eax + 0x790]
 db 085h,0C9h ; test	ecx, ecx
 db 074h,074h ; je	0x455 <.text+0x455>
 db 089h,0C2h ; mov	edx, eax
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,082h,010h,003h,000h,000h ; mulps	xmm0, xmmword ptr [edx + 0x310]
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0A2h,020h,003h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x320]
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,059h,0A2h,030h,003h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x330]
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,0C2h,0E0h,001h ; cmpltps	xmm4, xmm0
 db 00Fh,054h,0C4h ; andps	xmm0, xmm4
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,059h,0A2h,010h,005h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x510]
 db 00Fh,058h,0ECh ; addps	xmm5, xmm4
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,059h,0A2h,020h,005h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x520]
 db 00Fh,058h,0F4h ; addps	xmm6, xmm4
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,059h,0A2h,030h,005h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x530]
 db 00Fh,058h,0FCh ; addps	xmm7, xmm4
 db 083h,0C2h,040h ; add	edx, 0x40
 db 049h ; dec	ecx
 db 075h,0A5h ; jne	0x3e3 <.text+0x3e3>
 db 0EBh,015h ; jmp	0x455 <.text+0x455>
 db 00Fh,010h,0A8h,010h,007h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x710]
 db 00Fh,010h,0B0h,020h,007h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x720]
 db 00Fh,010h,0B8h,030h,007h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x730]
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,010h,080h,040h,007h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x740]
 db 00Fh,059h,0A8h,060h,008h,000h,000h ; mulps	xmm5, xmmword ptr [eax + 0x860]
 db 00Fh,059h,0B0h,060h,008h,000h,000h ; mulps	xmm6, xmmword ptr [eax + 0x860]
 db 00Fh,059h,0B8h,060h,008h,000h,000h ; mulps	xmm7, xmmword ptr [eax + 0x860]
 db 00Fh,059h,080h,060h,008h,000h,000h ; mulps	xmm0, xmmword ptr [eax + 0x860]
 db 066h,00Fh,05Bh,0EDh ; cvtps2dq	xmm5, xmm5
 db 066h,00Fh,05Bh,0F6h ; cvtps2dq	xmm6, xmm6
 db 066h,00Fh,05Bh,0FFh ; cvtps2dq	xmm7, xmm7
 db 066h,00Fh,05Bh,0C0h ; cvtps2dq	xmm0, xmm0
 db 066h,00Fh,06Bh,0EDh ; packssdw	xmm5, xmm5
 db 066h,00Fh,06Bh,0F6h ; packssdw	xmm6, xmm6
 db 066h,00Fh,06Bh,0FFh ; packssdw	xmm7, xmm7
 db 066h,00Fh,06Bh,0C0h ; packssdw	xmm0, xmm0
 db 066h,00Fh,067h,0E9h ; packuswb	xmm5, xmm1
 db 066h,00Fh,067h,0F1h ; packuswb	xmm6, xmm1
 db 066h,00Fh,067h,0F9h ; packuswb	xmm7, xmm1
 db 066h,00Fh,067h,0C1h ; packuswb	xmm0, xmm1
 db 066h,00Fh,060h,0EFh ; punpcklbw	xmm5, xmm7
 db 066h,00Fh,060h,0F0h ; punpcklbw	xmm6, xmm0
 db 066h,00Fh,060h,0EEh ; punpcklbw	xmm5, xmm6
 db 00Fh,011h,0A8h,040h,008h,000h,000h ; movups	xmmword ptr [eax + 0x840], xmm5
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,010h,080h,040h,00Ah,000h,000h ; movups	xmm0, xmmword ptr [eax + 0xa40]
 db 00Fh,059h,080h,060h,008h,000h,000h ; mulps	xmm0, xmmword ptr [eax + 0x860]
 db 066h,00Fh,05Bh,0C0h ; cvtps2dq	xmm0, xmm0
 db 066h,00Fh,06Bh,0C0h ; packssdw	xmm0, xmm0
 db 066h,00Fh,067h,0C1h ; packuswb	xmm0, xmm1
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 066h,00Fh,060h,0D0h ; punpcklbw	xmm2, xmm0
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 066h,00Fh,060h,0DAh ; punpcklbw	xmm3, xmm2
 db 00Fh,011h,098h,050h,008h,000h,000h ; movups	xmmword ptr [eax + 0x850], xmm3
 db 0EBh,053h ; jmp	0x545 <.text+0x545>
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,010h,080h,040h,00Ah,000h,000h ; movups	xmm0, xmmword ptr [eax + 0xa40]
 db 00Fh,059h,080h,060h,008h,000h,000h ; mulps	xmm0, xmmword ptr [eax + 0x860]
 db 066h,00Fh,05Bh,0C0h ; cvtps2dq	xmm0, xmm0
 db 066h,00Fh,06Bh,0C0h ; packssdw	xmm0, xmm0
 db 066h,00Fh,067h,0C1h ; packuswb	xmm0, xmm1
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 066h,00Fh,060h,0D0h ; punpcklbw	xmm2, xmm0
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 066h,00Fh,060h,0DAh ; punpcklbw	xmm3, xmm2
 db 00Fh,010h,080h,020h,008h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x820]
 db 00Fh,011h,080h,040h,008h,000h,000h ; movups	xmmword ptr [eax + 0x840], xmm0
 db 00Fh,010h,080h,030h,008h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x830]
 db 066h,00Fh,0DBh,080h,060h,009h,000h,000h ; pand	xmm0, xmmword ptr [eax + 0x960]
 db 066h,00Fh,0EBh,0C3h ; por	xmm0, xmm3
 db 00Fh,011h,080h,050h,008h,000h,000h ; movups	xmmword ptr [eax + 0x850], xmm0
 db 05Dh ; pop	ebp
 db 05Fh ; pop	edi
 db 05Eh ; pop	esi
 db 05Bh ; pop	ebx
 db 0C3h ; ret
_tnk_light endp
public _tnk_light_k
_tnk_light_k proc near
 db 053h ; push	ebx
 db 056h ; push	esi
 db 057h ; push	edi
 db 055h ; push	ebp
 db 08Bh,044h,024h,014h ; mov	eax, dword ptr [esp + 0x14]
 db 00Fh,010h,0A0h,070h,008h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x870]
 db 08Bh,0B0h,098h,007h,000h,000h ; mov	esi, dword ptr [eax + 0x798]
 db 085h,0F6h ; test	esi, esi
 db 00Fh,084h,033h,002h,000h,000h ; je	0x250 <.text+0x250>
 db 083h,0B8h,0B8h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7b8], 0x0
 db 074h,016h ; je	0x3c <.text+0x3c>
 db 00Fh,010h,080h,0E0h,007h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x7e0]
 db 00Fh,010h,088h,040h,009h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x940]
 db 00Fh,055h,0C8h ; andnps	xmm1, xmm0
 db 0E9h,025h,001h,000h,000h ; jmp	0x161 <.text+0x161>
 db 083h,0B8h,0E4h,023h,000h,000h,000h ; cmp	dword ptr [eax + 0x23e4], 0x0
 db 074h,027h ; je	0x6c <.text+0x6c>
 db 00Fh,010h,080h,0D0h,023h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x23d0]
 db 083h,0B8h,09Ch,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x79c], 0x0
 db 00Fh,084h,0FEh,000h,000h,000h ; je	0x157 <.text+0x157>
 db 00Fh,010h,098h,0B0h,023h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x23b0]
 db 00Fh,010h,088h,0C0h,023h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x23c0]
 db 0E9h,0D7h,000h,000h,000h ; jmp	0x143 <.text+0x143>
 db 00Fh,010h,0A8h,0C0h,007h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x7c0]
 db 00Fh,010h,0B0h,0D0h,007h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x7d0]
 db 00Fh,010h,0B8h,0E0h,007h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x7e0]
 db 00Fh,010h,080h,0A0h,001h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x1a0]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,010h,088h,0E0h,001h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1e0]
 db 00Fh,059h,0CEh ; mulps	xmm1, xmm6
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,020h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x220]
 db 00Fh,059h,0CFh ; mulps	xmm1, xmm7
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,060h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x260]
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 083h,0B8h,09Ch,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x79c], 0x0
 db 00Fh,084h,09Bh,000h,000h,000h ; je	0x157 <.text+0x157>
 db 00Fh,011h,080h,0D0h,023h,000h,000h ; movups	xmmword ptr [eax + 0x23d0], xmm0
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,010h,080h,080h,001h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x180]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,010h,088h,0C0h,001h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1c0]
 db 00Fh,059h,0CEh ; mulps	xmm1, xmm6
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,000h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x200]
 db 00Fh,059h,0CFh ; mulps	xmm1, xmm7
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,040h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x240]
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,011h,080h,0B0h,023h,000h,000h ; movups	xmmword ptr [eax + 0x23b0], xmm0
 db 00Fh,028h,0D8h ; movaps	xmm3, xmm0
 db 00Fh,010h,080h,090h,001h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x190]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,010h,088h,0D0h,001h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1d0]
 db 00Fh,059h,0CEh ; mulps	xmm1, xmm6
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,010h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x210]
 db 00Fh,059h,0CFh ; mulps	xmm1, xmm7
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,010h,088h,050h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x250]
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,011h,080h,0C0h,023h,000h,000h ; movups	xmmword ptr [eax + 0x23c0], xmm0
 db 00Fh,028h,0C8h ; movaps	xmm1, xmm0
 db 00Fh,028h,0C4h ; movaps	xmm0, xmm4
 db 0C7h,080h,0E4h,023h,000h,000h,001h,000h,000h,000h ; mov	dword ptr [eax + 0x23e4], 0x1
 db 00Fh,059h,0DBh ; mulps	xmm3, xmm3
 db 00Fh,059h,0C9h ; mulps	xmm1, xmm1
 db 00Fh,059h,0C0h ; mulps	xmm0, xmm0
 db 00Fh,058h,0D9h ; addps	xmm3, xmm1
 db 00Fh,058h,0D8h ; addps	xmm3, xmm0
 db 00Fh,051h,0CBh ; sqrtps	xmm1, xmm3
 db 0EBh,00Ah ; jmp	0x161 <.text+0x161>
 db 00Fh,010h,088h,040h,009h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x940]
 db 00Fh,055h,0C8h ; andnps	xmm1, xmm0
 db 083h,0FEh,003h ; cmp	esi, 0x3
 db 075h,016h ; jne	0x17c <.text+0x17c>
 db 00Fh,010h,0A0h,060h,007h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x760]
 db 00Fh,05Ch,0E1h ; subps	xmm4, xmm1
 db 00Fh,05Eh,0A0h,070h,007h,000h,000h ; divps	xmm4, xmmword ptr [eax + 0x770]
 db 0E9h,0D4h,000h,000h,000h ; jmp	0x250 <.text+0x250>
 db 00Fh,010h,080h,050h,007h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x750]
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 083h,0FEh,001h ; cmp	esi, 0x1
 db 074h,003h ; je	0x18e <.text+0x18e>
 db 00Fh,059h,0C0h ; mulps	xmm0, xmm0
 db 00Fh,057h,0D2h ; xorps	xmm2, xmm2
 db 00Fh,05Ch,0D0h ; subps	xmm2, xmm0
 db 00Fh,028h,0DAh ; movaps	xmm3, xmm2
 db 00Fh,010h,080h,020h,009h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x920]
 db 00Fh,0C2h,0C2h,001h ; cmpltps	xmm0, xmm2
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,028h,0CAh ; movaps	xmm1, xmm2
 db 00Fh,0C2h,0CCh,001h ; cmpltps	xmm1, xmm4
 db 00Fh,05Fh,090h,020h,009h,000h,000h ; maxps	xmm2, xmmword ptr [eax + 0x920]
 db 00Fh,05Dh,0D4h ; minps	xmm2, xmm4
 db 00Fh,059h,090h,090h,008h,000h,000h ; mulps	xmm2, xmmword ptr [eax + 0x890]
 db 0F3h,00Fh,05Bh,0E2h ; cvttps2dq	xmm4, xmm2
 db 00Fh,05Bh,0ECh ; cvtdq2ps	xmm5, xmm4
 db 00Fh,028h,0F2h ; movaps	xmm6, xmm2
 db 00Fh,0C2h,0F5h,001h ; cmpltps	xmm6, xmm5
 db 066h,00Fh,072h,0D6h,01Fh ; psrld	xmm6, 0x1f
 db 066h,00Fh,0FAh,0E6h ; psubd	xmm4, xmm6
 db 00Fh,05Bh,0ECh ; cvtdq2ps	xmm5, xmm4
 db 00Fh,028h,0F2h ; movaps	xmm6, xmm2
 db 00Fh,05Ch,0F5h ; subps	xmm6, xmm5
 db 00Fh,010h,0A8h,000h,009h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x900]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0F0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8f0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0E0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8e0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0D0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8d0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0C0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8c0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0B0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8b0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0A8h,0A0h,008h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x8a0]
 db 066h,00Fh,0FEh,0A0h,050h,009h,000h,000h ; paddd	xmm4, xmmword ptr [eax + 0x950]
 db 066h,00Fh,072h,0F4h,017h ; pslld	xmm4, 0x17
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,054h,0E8h ; andps	xmm5, xmm0
 db 00Fh,054h,0E9h ; andps	xmm5, xmm1
 db 00Fh,010h,0A0h,070h,008h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x870]
 db 00Fh,057h,0C0h ; xorps	xmm0, xmm0
 db 00Fh,028h,0F8h ; movaps	xmm7, xmm0
 db 00Fh,0C2h,0FBh,002h ; cmpleps	xmm7, xmm3
 db 00Fh,054h,0E7h ; andps	xmm4, xmm7
 db 00Fh,056h,0ECh ; orps	xmm5, xmm4
 db 00Fh,028h,0E5h ; movaps	xmm4, xmm5
 db 00Fh,011h,0A0h,040h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa40], xmm4
 db 083h,0B8h,094h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x794], 0x0
 db 00Fh,084h,08Dh,002h,000h,000h ; je	0x4f1 <.text+0x4f1>
 db 083h,0B8h,0B4h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7b4], 0x0
 db 00Fh,085h,04Ch,002h,000h,000h ; jne	0x4bd <.text+0x4bd>
 db 083h,0B8h,0A4h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7a4], 0x0
 db 00Fh,084h,0C1h,001h,000h,000h ; je	0x43f <.text+0x43f>
 db 083h,0B8h,0E0h,023h,000h,000h,000h ; cmp	dword ptr [eax + 0x23e0], 0x0
 db 074h,01Ah ; je	0x2a1 <.text+0x2a1>
 db 00Fh,010h,088h,080h,023h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x2380]
 db 00Fh,010h,090h,090h,023h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x2390]
 db 00Fh,010h,098h,0A0h,023h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x23a0]
 db 0E9h,020h,001h,000h,000h ; jmp	0x3c1 <.text+0x3c1>
 db 00Fh,010h,080h,0F0h,007h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x7f0]
 db 00Fh,010h,088h,000h,008h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x800]
 db 00Fh,010h,090h,010h,008h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x810]
 db 00Fh,010h,098h,080h,002h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x280]
 db 00Fh,010h,0A0h,090h,002h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x290]
 db 00Fh,059h,0D8h ; mulps	xmm3, xmm0
 db 00Fh,059h,0E0h ; mulps	xmm4, xmm0
 db 00Fh,010h,0A8h,0B0h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x2b0]
 db 00Fh,010h,0B0h,0C0h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x2c0]
 db 00Fh,059h,0E9h ; mulps	xmm5, xmm1
 db 00Fh,059h,0F1h ; mulps	xmm6, xmm1
 db 00Fh,058h,0DDh ; addps	xmm3, xmm5
 db 00Fh,058h,0E6h ; addps	xmm4, xmm6
 db 00Fh,010h,0A8h,0E0h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x2e0]
 db 00Fh,010h,0B0h,0F0h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x2f0]
 db 00Fh,059h,0EAh ; mulps	xmm5, xmm2
 db 00Fh,059h,0F2h ; mulps	xmm6, xmm2
 db 00Fh,058h,0DDh ; addps	xmm3, xmm5
 db 00Fh,058h,0E6h ; addps	xmm4, xmm6
 db 00Fh,010h,0A8h,0A0h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x2a0]
 db 00Fh,059h,0E8h ; mulps	xmm5, xmm0
 db 00Fh,010h,0B0h,0D0h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x2d0]
 db 00Fh,059h,0F1h ; mulps	xmm6, xmm1
 db 00Fh,058h,0EEh ; addps	xmm5, xmm6
 db 00Fh,010h,0B0h,000h,003h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x300]
 db 00Fh,059h,0F2h ; mulps	xmm6, xmm2
 db 00Fh,058h,0EEh ; addps	xmm5, xmm6
 db 00Fh,028h,0CBh ; movaps	xmm1, xmm3
 db 00Fh,028h,0D4h ; movaps	xmm2, xmm4
 db 00Fh,028h,0DDh ; movaps	xmm3, xmm5
 db 083h,0B8h,0A0h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7a0], 0x0
 db 074h,06Eh ; je	0x3a2 <.text+0x3a2>
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0E2h ; mulps	xmm4, xmm2
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,059h,0E3h ; mulps	xmm4, xmm3
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,051h,0C0h ; sqrtps	xmm0, xmm0
 db 00Fh,057h,0F6h ; xorps	xmm6, xmm6
 db 00Fh,0C2h,0F0h,001h ; cmpltps	xmm6, xmm0
 db 00Fh,010h,0B8h,070h,008h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x870]
 db 00Fh,05Eh,0F8h ; divps	xmm7, xmm0
 db 00Fh,054h,0FEh ; andps	xmm7, xmm6
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,0C7h ; mulps	xmm0, xmm7
 db 00Fh,054h,0C6h ; andps	xmm0, xmm6
 db 00Fh,028h,0E6h ; movaps	xmm4, xmm6
 db 00Fh,055h,0E1h ; andnps	xmm4, xmm1
 db 00Fh,056h,0C4h ; orps	xmm0, xmm4
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0E7h ; mulps	xmm4, xmm7
 db 00Fh,054h,0E6h ; andps	xmm4, xmm6
 db 00Fh,028h,0EEh ; movaps	xmm5, xmm6
 db 00Fh,055h,0EAh ; andnps	xmm5, xmm2
 db 00Fh,056h,0E5h ; orps	xmm4, xmm5
 db 00Fh,028h,0EBh ; movaps	xmm5, xmm3
 db 00Fh,059h,0EFh ; mulps	xmm5, xmm7
 db 00Fh,054h,0EEh ; andps	xmm5, xmm6
 db 00Fh,028h,0FEh ; movaps	xmm7, xmm6
 db 00Fh,055h,0FBh ; andnps	xmm7, xmm3
 db 00Fh,056h,0EFh ; orps	xmm5, xmm7
 db 00Fh,028h,0C8h ; movaps	xmm1, xmm0
 db 00Fh,028h,0D4h ; movaps	xmm2, xmm4
 db 00Fh,028h,0DDh ; movaps	xmm3, xmm5
 db 00Fh,011h,088h,080h,023h,000h,000h ; movups	xmmword ptr [eax + 0x2380], xmm1
 db 00Fh,011h,090h,090h,023h,000h,000h ; movups	xmmword ptr [eax + 0x2390], xmm2
 db 00Fh,011h,098h,0A0h,023h,000h,000h ; movups	xmmword ptr [eax + 0x23a0], xmm3
 db 0C7h,080h,0E0h,023h,000h,000h,001h,000h,000h,000h ; mov	dword ptr [eax + 0x23e0], 0x1
 db 00Fh,010h,0A8h,010h,007h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x710]
 db 00Fh,010h,0B0h,020h,007h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x720]
 db 00Fh,010h,0B8h,030h,007h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x730]
 db 08Bh,088h,090h,007h,000h,000h ; mov	ecx, dword ptr [eax + 0x790]
 db 085h,0C9h ; test	ecx, ecx
 db 074h,074h ; je	0x454 <.text+0x454>
 db 089h,0C2h ; mov	edx, eax
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,082h,010h,003h,000h,000h ; mulps	xmm0, xmmword ptr [edx + 0x310]
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0A2h,020h,003h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x320]
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,059h,0A2h,030h,003h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x330]
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,0C2h,0E0h,001h ; cmpltps	xmm4, xmm0
 db 00Fh,054h,0C4h ; andps	xmm0, xmm4
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,059h,0A2h,010h,005h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x510]
 db 00Fh,058h,0ECh ; addps	xmm5, xmm4
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,059h,0A2h,020h,005h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x520]
 db 00Fh,058h,0F4h ; addps	xmm6, xmm4
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,059h,0A2h,030h,005h,000h,000h ; mulps	xmm4, xmmword ptr [edx + 0x530]
 db 00Fh,058h,0FCh ; addps	xmm7, xmm4
 db 083h,0C2h,040h ; add	edx, 0x40
 db 049h ; dec	ecx
 db 075h,0A5h ; jne	0x3e2 <.text+0x3e2>
 db 0EBh,015h ; jmp	0x454 <.text+0x454>
 db 00Fh,010h,0A8h,010h,007h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x710]
 db 00Fh,010h,0B0h,020h,007h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x720]
 db 00Fh,010h,0B8h,030h,007h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x730]
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,010h,080h,040h,007h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x740]
 db 00Fh,059h,0A8h,060h,008h,000h,000h ; mulps	xmm5, xmmword ptr [eax + 0x860]
 db 00Fh,059h,0B0h,060h,008h,000h,000h ; mulps	xmm6, xmmword ptr [eax + 0x860]
 db 00Fh,059h,0B8h,060h,008h,000h,000h ; mulps	xmm7, xmmword ptr [eax + 0x860]
 db 00Fh,059h,080h,060h,008h,000h,000h ; mulps	xmm0, xmmword ptr [eax + 0x860]
 db 066h,00Fh,05Bh,0EDh ; cvtps2dq	xmm5, xmm5
 db 066h,00Fh,05Bh,0F6h ; cvtps2dq	xmm6, xmm6
 db 066h,00Fh,05Bh,0FFh ; cvtps2dq	xmm7, xmm7
 db 066h,00Fh,05Bh,0C0h ; cvtps2dq	xmm0, xmm0
 db 066h,00Fh,06Bh,0EDh ; packssdw	xmm5, xmm5
 db 066h,00Fh,06Bh,0F6h ; packssdw	xmm6, xmm6
 db 066h,00Fh,06Bh,0FFh ; packssdw	xmm7, xmm7
 db 066h,00Fh,06Bh,0C0h ; packssdw	xmm0, xmm0
 db 066h,00Fh,067h,0E9h ; packuswb	xmm5, xmm1
 db 066h,00Fh,067h,0F1h ; packuswb	xmm6, xmm1
 db 066h,00Fh,067h,0F9h ; packuswb	xmm7, xmm1
 db 066h,00Fh,067h,0C1h ; packuswb	xmm0, xmm1
 db 066h,00Fh,060h,0EFh ; punpcklbw	xmm5, xmm7
 db 066h,00Fh,060h,0F0h ; punpcklbw	xmm6, xmm0
 db 066h,00Fh,060h,0EEh ; punpcklbw	xmm5, xmm6
 db 00Fh,011h,0A8h,040h,008h,000h,000h ; movups	xmmword ptr [eax + 0x840], xmm5
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,010h,080h,040h,00Ah,000h,000h ; movups	xmm0, xmmword ptr [eax + 0xa40]
 db 00Fh,059h,080h,060h,008h,000h,000h ; mulps	xmm0, xmmword ptr [eax + 0x860]
 db 066h,00Fh,05Bh,0C0h ; cvtps2dq	xmm0, xmm0
 db 066h,00Fh,06Bh,0C0h ; packssdw	xmm0, xmm0
 db 066h,00Fh,067h,0C1h ; packuswb	xmm0, xmm1
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 066h,00Fh,060h,0D0h ; punpcklbw	xmm2, xmm0
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 066h,00Fh,060h,0DAh ; punpcklbw	xmm3, xmm2
 db 00Fh,011h,098h,050h,008h,000h,000h ; movups	xmmword ptr [eax + 0x850], xmm3
 db 0EBh,053h ; jmp	0x544 <.text+0x544>
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,010h,080h,040h,00Ah,000h,000h ; movups	xmm0, xmmword ptr [eax + 0xa40]
 db 00Fh,059h,080h,060h,008h,000h,000h ; mulps	xmm0, xmmword ptr [eax + 0x860]
 db 066h,00Fh,05Bh,0C0h ; cvtps2dq	xmm0, xmm0
 db 066h,00Fh,06Bh,0C0h ; packssdw	xmm0, xmm0
 db 066h,00Fh,067h,0C1h ; packuswb	xmm0, xmm1
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 066h,00Fh,060h,0D0h ; punpcklbw	xmm2, xmm0
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 066h,00Fh,060h,0DAh ; punpcklbw	xmm3, xmm2
 db 00Fh,010h,080h,020h,008h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x820]
 db 00Fh,011h,080h,040h,008h,000h,000h ; movups	xmmword ptr [eax + 0x840], xmm0
 db 00Fh,010h,080h,030h,008h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x830]
 db 066h,00Fh,0DBh,080h,060h,009h,000h,000h ; pand	xmm0, xmmword ptr [eax + 0x960]
 db 066h,00Fh,0EBh,0C3h ; por	xmm0, xmm3
 db 00Fh,011h,080h,050h,008h,000h,000h ; movups	xmmword ptr [eax + 0x850], xmm0
 db 05Dh ; pop	ebp
 db 05Fh ; pop	edi
 db 05Eh ; pop	esi
 db 05Bh ; pop	ebx
 db 0C3h ; ret
_tnk_light_k endp
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
public _tnk_splight
_tnk_splight proc near
 db 055h ; push	ebp
 db 089h,0E5h ; mov	ebp, esp
 db 053h ; push	ebx
 db 056h ; push	esi
 db 057h ; push	edi
 db 08Bh,07Dh,008h ; mov	edi, dword ptr [ebp + 0x8]
 db 083h,0E4h,0F0h ; and	esp, -0x10
 db 081h,0ECh,0C0h,000h,000h,000h ; sub	esp, 0xc0
 db 089h,0ACh,024h,090h,000h,000h,000h ; mov	dword ptr [esp + 0x90], ebp
 db 031h,0C0h ; xor	eax, eax
 db 083h,0BFh,090h,014h,000h,000h,000h ; cmp	dword ptr [edi + 0x1490], 0x0
 db 074h,00Ah ; je	0x2e <.text+0x2e>
 db 083h,0BFh,0A4h,007h,000h,000h,000h ; cmp	dword ptr [edi + 0x7a4], 0x0
 db 00Fh,095h,0C0h ; setne	al
 db 089h,084h,024h,094h,000h,000h,000h ; mov	dword ptr [esp + 0x94], eax
 db 083h,0BFh,0E4h,023h,000h,000h,000h ; cmp	dword ptr [edi + 0x23e4], 0x0
 db 00Fh,085h,0B5h,000h,000h,000h ; jne	0xf7 <.text+0xf7>
 db 00Fh,028h,0AFh,0C0h,007h,000h,000h ; movaps	xmm5, xmmword ptr [edi + 0x7c0]
 db 00Fh,028h,0B7h,0D0h,007h,000h,000h ; movaps	xmm6, xmmword ptr [edi + 0x7d0]
 db 00Fh,028h,0BFh,0E0h,007h,000h,000h ; movaps	xmm7, xmmword ptr [edi + 0x7e0]
 db 00Fh,028h,087h,080h,001h,000h,000h ; movaps	xmm0, xmmword ptr [edi + 0x180]
 db 00Fh,028h,08Fh,090h,001h,000h,000h ; movaps	xmm1, xmmword ptr [edi + 0x190]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,059h,0CDh ; mulps	xmm1, xmm5
 db 00Fh,028h,097h,0C0h,001h,000h,000h ; movaps	xmm2, xmmword ptr [edi + 0x1c0]
 db 00Fh,028h,09Fh,0D0h,001h,000h,000h ; movaps	xmm3, xmmword ptr [edi + 0x1d0]
 db 00Fh,059h,0D6h ; mulps	xmm2, xmm6
 db 00Fh,059h,0DEh ; mulps	xmm3, xmm6
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,028h,097h,000h,002h,000h,000h ; movaps	xmm2, xmmword ptr [edi + 0x200]
 db 00Fh,028h,09Fh,010h,002h,000h,000h ; movaps	xmm3, xmmword ptr [edi + 0x210]
 db 00Fh,059h,0D7h ; mulps	xmm2, xmm7
 db 00Fh,059h,0DFh ; mulps	xmm3, xmm7
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,058h,087h,040h,002h,000h,000h ; addps	xmm0, xmmword ptr [edi + 0x240]
 db 00Fh,058h,08Fh,050h,002h,000h,000h ; addps	xmm1, xmmword ptr [edi + 0x250]
 db 00Fh,029h,087h,0B0h,023h,000h,000h ; movaps	xmmword ptr [edi + 0x23b0], xmm0
 db 00Fh,029h,08Fh,0C0h,023h,000h,000h ; movaps	xmmword ptr [edi + 0x23c0], xmm1
 db 00Fh,028h,097h,0A0h,001h,000h,000h ; movaps	xmm2, xmmword ptr [edi + 0x1a0]
 db 00Fh,059h,0D5h ; mulps	xmm2, xmm5
 db 00Fh,028h,09Fh,0E0h,001h,000h,000h ; movaps	xmm3, xmmword ptr [edi + 0x1e0]
 db 00Fh,059h,0DEh ; mulps	xmm3, xmm6
 db 00Fh,058h,0D3h ; addps	xmm2, xmm3
 db 00Fh,028h,09Fh,020h,002h,000h,000h ; movaps	xmm3, xmmword ptr [edi + 0x220]
 db 00Fh,059h,0DFh ; mulps	xmm3, xmm7
 db 00Fh,058h,0D3h ; addps	xmm2, xmm3
 db 00Fh,058h,097h,060h,002h,000h,000h ; addps	xmm2, xmmword ptr [edi + 0x260]
 db 00Fh,029h,097h,0D0h,023h,000h,000h ; movaps	xmmword ptr [edi + 0x23d0], xmm2
 db 0C7h,087h,0E4h,023h,000h,000h,001h,000h,000h,000h ; mov	dword ptr [edi + 0x23e4], 0x1
 db 083h,0BFh,0A4h,007h,000h,000h,000h ; cmp	dword ptr [edi + 0x7a4], 0x0
 db 00Fh,084h,024h,001h,000h,000h ; je	0x228 <.text+0x228>
 db 083h,0BFh,0E0h,023h,000h,000h,000h ; cmp	dword ptr [edi + 0x23e0], 0x0
 db 00Fh,085h,017h,001h,000h,000h ; jne	0x228 <.text+0x228>
 db 00Fh,028h,0AFh,0F0h,007h,000h,000h ; movaps	xmm5, xmmword ptr [edi + 0x7f0]
 db 00Fh,028h,0B7h,000h,008h,000h,000h ; movaps	xmm6, xmmword ptr [edi + 0x800]
 db 00Fh,028h,0BFh,010h,008h,000h,000h ; movaps	xmm7, xmmword ptr [edi + 0x810]
 db 00Fh,028h,087h,080h,002h,000h,000h ; movaps	xmm0, xmmword ptr [edi + 0x280]
 db 00Fh,028h,08Fh,090h,002h,000h,000h ; movaps	xmm1, xmmword ptr [edi + 0x290]
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,059h,0CDh ; mulps	xmm1, xmm5
 db 00Fh,028h,097h,0B0h,002h,000h,000h ; movaps	xmm2, xmmword ptr [edi + 0x2b0]
 db 00Fh,028h,09Fh,0C0h,002h,000h,000h ; movaps	xmm3, xmmword ptr [edi + 0x2c0]
 db 00Fh,059h,0D6h ; mulps	xmm2, xmm6
 db 00Fh,059h,0DEh ; mulps	xmm3, xmm6
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,028h,097h,0E0h,002h,000h,000h ; movaps	xmm2, xmmword ptr [edi + 0x2e0]
 db 00Fh,028h,09Fh,0F0h,002h,000h,000h ; movaps	xmm3, xmmword ptr [edi + 0x2f0]
 db 00Fh,059h,0D7h ; mulps	xmm2, xmm7
 db 00Fh,059h,0DFh ; mulps	xmm3, xmm7
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,028h,097h,0A0h,002h,000h,000h ; movaps	xmm2, xmmword ptr [edi + 0x2a0]
 db 00Fh,059h,0D5h ; mulps	xmm2, xmm5
 db 00Fh,028h,09Fh,0D0h,002h,000h,000h ; movaps	xmm3, xmmword ptr [edi + 0x2d0]
 db 00Fh,059h,0DEh ; mulps	xmm3, xmm6
 db 00Fh,058h,0D3h ; addps	xmm2, xmm3
 db 00Fh,028h,09Fh,000h,003h,000h,000h ; movaps	xmm3, xmmword ptr [edi + 0x300]
 db 00Fh,059h,0DFh ; mulps	xmm3, xmm7
 db 00Fh,058h,0D3h ; addps	xmm2, xmm3
 db 083h,0BFh,0A0h,007h,000h,000h,000h ; cmp	dword ptr [edi + 0x7a0], 0x0
 db 074h,06Eh ; je	0x209 <.text+0x209>
 db 00Fh,028h,0D8h ; movaps	xmm3, xmm0
 db 00Fh,059h,0D8h ; mulps	xmm3, xmm0
 db 00Fh,028h,0E1h ; movaps	xmm4, xmm1
 db 00Fh,059h,0E1h ; mulps	xmm4, xmm1
 db 00Fh,058h,0DCh ; addps	xmm3, xmm4
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0E2h ; mulps	xmm4, xmm2
 db 00Fh,058h,0DCh ; addps	xmm3, xmm4
 db 00Fh,051h,0DBh ; sqrtps	xmm3, xmm3
 db 00Fh,028h,0A7h,070h,008h,000h,000h ; movaps	xmm4, xmmword ptr [edi + 0x870]
 db 00Fh,05Eh,0E3h ; divps	xmm4, xmm3
 db 00Fh,057h,0EDh ; xorps	xmm5, xmm5
 db 00Fh,0C2h,0EBh,001h ; cmpltps	xmm5, xmm3
 db 00Fh,054h,0E5h ; andps	xmm4, xmm5
 db 00Fh,028h,0D8h ; movaps	xmm3, xmm0
 db 00Fh,059h,0DCh ; mulps	xmm3, xmm4
 db 00Fh,054h,0DDh ; andps	xmm3, xmm5
 db 00Fh,028h,0F5h ; movaps	xmm6, xmm5
 db 00Fh,055h,0F0h ; andnps	xmm6, xmm0
 db 00Fh,056h,0DEh ; orps	xmm3, xmm6
 db 00Fh,028h,0C3h ; movaps	xmm0, xmm3
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 00Fh,059h,0DCh ; mulps	xmm3, xmm4
 db 00Fh,054h,0DDh ; andps	xmm3, xmm5
 db 00Fh,028h,0F5h ; movaps	xmm6, xmm5
 db 00Fh,055h,0F1h ; andnps	xmm6, xmm1
 db 00Fh,056h,0DEh ; orps	xmm3, xmm6
 db 00Fh,028h,0CBh ; movaps	xmm1, xmm3
 db 00Fh,028h,0DAh ; movaps	xmm3, xmm2
 db 00Fh,059h,0DCh ; mulps	xmm3, xmm4
 db 00Fh,054h,0DDh ; andps	xmm3, xmm5
 db 00Fh,028h,0F5h ; movaps	xmm6, xmm5
 db 00Fh,055h,0F2h ; andnps	xmm6, xmm2
 db 00Fh,056h,0DEh ; orps	xmm3, xmm6
 db 00Fh,028h,0D3h ; movaps	xmm2, xmm3
 db 00Fh,029h,087h,080h,023h,000h,000h ; movaps	xmmword ptr [edi + 0x2380], xmm0
 db 00Fh,029h,08Fh,090h,023h,000h,000h ; movaps	xmmword ptr [edi + 0x2390], xmm1
 db 00Fh,029h,097h,0A0h,023h,000h,000h ; movaps	xmmword ptr [edi + 0x23a0], xmm2
 db 0C7h,087h,0E0h,023h,000h,000h,001h,000h,000h,000h ; mov	dword ptr [edi + 0x23e0], 0x1
 db 00Fh,028h,087h,090h,022h,000h,000h ; movaps	xmm0, xmmword ptr [edi + 0x2290]
 db 00Fh,028h,08Fh,0A0h,022h,000h,000h ; movaps	xmm1, xmmword ptr [edi + 0x22a0]
 db 00Fh,028h,097h,0B0h,022h,000h,000h ; movaps	xmm2, xmmword ptr [edi + 0x22b0]
 db 00Fh,028h,09Fh,0C0h,022h,000h,000h ; movaps	xmm3, xmmword ptr [edi + 0x22c0]
 db 00Fh,029h,087h,010h,007h,000h,000h ; movaps	xmmword ptr [edi + 0x710], xmm0
 db 00Fh,029h,08Fh,020h,007h,000h,000h ; movaps	xmmword ptr [edi + 0x720], xmm1
 db 00Fh,029h,097h,030h,007h,000h,000h ; movaps	xmmword ptr [edi + 0x730], xmm2
 db 00Fh,029h,09Fh,040h,007h,000h,000h ; movaps	xmmword ptr [edi + 0x740], xmm3
 db 083h,0BCh,024h,094h,000h,000h,000h,000h ; cmp	dword ptr [esp + 0x94], 0x0
 db 00Fh,084h,038h,001h,000h,000h ; je	0x3a6 <.text+0x3a6>
 db 00Fh,057h,0C0h ; xorps	xmm0, xmm0
 db 00Fh,029h,087h,0E0h,022h,000h,000h ; movaps	xmmword ptr [edi + 0x22e0], xmm0
 db 00Fh,029h,087h,0F0h,022h,000h,000h ; movaps	xmmword ptr [edi + 0x22f0], xmm0
 db 00Fh,029h,087h,000h,023h,000h,000h ; movaps	xmmword ptr [edi + 0x2300], xmm0
 db 00Fh,029h,004h,024h ; movaps	xmmword ptr [esp], xmm0
 db 00Fh,029h,044h,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm0
 db 00Fh,028h,0C8h ; movaps	xmm1, xmm0
 db 00Fh,05Ch,08Fh,070h,008h,000h,000h ; subps	xmm1, xmmword ptr [edi + 0x870]
 db 00Fh,029h,04Ch,024h,020h ; movaps	xmmword ptr [esp + 0x20], xmm1
 db 083h,0BFh,094h,014h,000h,000h,000h ; cmp	dword ptr [edi + 0x1494], 0x0
 db 00Fh,084h,082h,000h,000h,000h ; je	0x32d <.text+0x32d>
 db 00Fh,028h,08Fh,0B0h,023h,000h,000h ; movaps	xmm1, xmmword ptr [edi + 0x23b0]
 db 00Fh,028h,097h,0C0h,023h,000h,000h ; movaps	xmm2, xmmword ptr [edi + 0x23c0]
 db 00Fh,028h,09Fh,0D0h,023h,000h,000h ; movaps	xmm3, xmmword ptr [edi + 0x23d0]
 db 00Fh,028h,0E1h ; movaps	xmm4, xmm1
 db 00Fh,059h,0E1h ; mulps	xmm4, xmm1
 db 00Fh,028h,0EAh ; movaps	xmm5, xmm2
 db 00Fh,059h,0EAh ; mulps	xmm5, xmm2
 db 00Fh,058h,0E5h ; addps	xmm4, xmm5
 db 00Fh,028h,0EBh ; movaps	xmm5, xmm3
 db 00Fh,059h,0EBh ; mulps	xmm5, xmm3
 db 00Fh,058h,0E5h ; addps	xmm4, xmm5
 db 00Fh,028h,0E8h ; movaps	xmm5, xmm0
 db 00Fh,0C2h,0ECh,001h ; cmpltps	xmm5, xmm4
 db 00Fh,052h,0F4h ; rsqrtps	xmm6, xmm4
 db 00Fh,028h,0FCh ; movaps	xmm7, xmm4
 db 00Fh,059h,0FEh ; mulps	xmm7, xmm6
 db 00Fh,059h,0FEh ; mulps	xmm7, xmm6
 db 00Fh,05Ch,0BFh,070h,008h,000h,000h ; subps	xmm7, xmmword ptr [edi + 0x870]
 db 00Fh,059h,0BFh,080h,008h,000h,000h ; mulps	xmm7, xmmword ptr [edi + 0x880]
 db 00Fh,059h,0FEh ; mulps	xmm7, xmm6
 db 00Fh,05Ch,0F7h ; subps	xmm6, xmm7
 db 00Fh,05Ch,0C6h ; subps	xmm0, xmm6
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,059h,0D0h ; mulps	xmm2, xmm0
 db 00Fh,059h,0D8h ; mulps	xmm3, xmm0
 db 00Fh,054h,0CDh ; andps	xmm1, xmm5
 db 00Fh,054h,0D5h ; andps	xmm2, xmm5
 db 00Fh,054h,0DDh ; andps	xmm3, xmm5
 db 00Fh,028h,0F5h ; movaps	xmm6, xmm5
 db 00Fh,055h,074h,024h,020h ; andnps	xmm6, xmmword ptr [esp + 0x20]
 db 00Fh,056h,0DEh ; orps	xmm3, xmm6
 db 00Fh,029h,00Ch,024h ; movaps	xmmword ptr [esp], xmm1
 db 00Fh,029h,054h,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm2
 db 00Fh,029h,05Ch,024h,020h ; movaps	xmmword ptr [esp + 0x20], xmm3
 db 08Bh,087h,090h,007h,000h,000h ; mov	eax, dword ptr [edi + 0x790]
 db 085h,0C0h ; test	eax, eax
 db 074h,06Fh ; je	0x3a6 <.text+0x3a6>
 db 031h,0DBh ; xor	ebx, ebx
 db 031h,0F6h ; xor	esi, esi
 db 031h,0EDh ; xor	ebp, ebp
 db 0C7h,084h,024h,09Ch,000h,000h,000h,000h,000h,000h,000h ; mov	dword ptr [esp + 0x9c], 0x0
 db 00Fh,028h,0A4h,02Fh,010h,003h,000h,000h ; movaps	xmm4, xmmword ptr [edi + ebp + 0x310]
 db 00Fh,028h,0ACh,02Fh,020h,003h,000h,000h ; movaps	xmm5, xmmword ptr [edi + ebp + 0x320]
 db 00Fh,028h,0B4h,02Fh,030h,003h,000h,000h ; movaps	xmm6, xmmword ptr [edi + ebp + 0x330]
 db 00Fh,028h,0C4h ; movaps	xmm0, xmm4
 db 00Fh,059h,087h,080h,023h,000h,000h ; mulps	xmm0, xmmword ptr [edi + 0x2380]
 db 00Fh,028h,0CDh ; movaps	xmm1, xmm5
 db 00Fh,059h,08Fh,090h,023h,000h,000h ; mulps	xmm1, xmmword ptr [edi + 0x2390]
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,028h,0CEh ; movaps	xmm1, xmm6
 db 00Fh,059h,08Fh,0A0h,023h,000h,000h ; mulps	xmm1, xmmword ptr [edi + 0x23a0]
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,028h,097h,070h,008h,000h,000h ; movaps	xmm2, xmmword ptr [edi + 0x870]
 db 00Fh,028h,0CAh ; movaps	xmm1, xmm2
 db 00Fh,0C2h,0CAh,000h ; cmpeqps	xmm1, xmm2
 db 0E9h,0F7h,003h,000h,000h ; jmp	0x78e <.text+0x78e>
 db 083h,0C6h,030h ; add	esi, 0x30
 db 083h,0C5h,040h ; add	ebp, 0x40
 db 043h ; inc	ebx
 db 03Bh,09Fh,090h,007h,000h,000h ; cmp	ebx, dword ptr [edi + 0x790]
 db 072h,0A2h ; jb	0x348 <.text+0x348>
 db 08Bh,087h,090h,00Ah,000h,000h ; mov	eax, dword ptr [edi + 0xa90]
 db 085h,0C0h ; test	eax, eax
 db 00Fh,084h,0BEh,003h,000h,000h ; je	0x772 <.text+0x772>
 db 031h,0DBh ; xor	ebx, ebx
 db 031h,0F6h ; xor	esi, esi
 db 031h,0EDh ; xor	ebp, ebp
 db 08Bh,087h,0E8h,023h,000h,000h ; mov	eax, dword ptr [edi + 0x23e8]
 db 00Fh,0A3h,0D8h ; bt	eax, ebx
 db 00Fh,082h,096h,003h,000h,000h ; jb	0x75f <.text+0x75f>
 db 00Fh,028h,08Ch,037h,0A0h,00Ah,000h,000h ; movaps	xmm1, xmmword ptr [edi + esi + 0xaa0]
 db 00Fh,028h,094h,037h,0B0h,00Ah,000h,000h ; movaps	xmm2, xmmword ptr [edi + esi + 0xab0]
 db 00Fh,028h,09Ch,037h,0C0h,00Ah,000h,000h ; movaps	xmm3, xmmword ptr [edi + esi + 0xac0]
 db 00Fh,05Ch,08Fh,0B0h,023h,000h,000h ; subps	xmm1, xmmword ptr [edi + 0x23b0]
 db 00Fh,05Ch,097h,0C0h,023h,000h,000h ; subps	xmm2, xmmword ptr [edi + 0x23c0]
 db 00Fh,05Ch,09Fh,0D0h,023h,000h,000h ; subps	xmm3, xmmword ptr [edi + 0x23d0]
 db 00Fh,028h,0E9h ; movaps	xmm5, xmm1
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0E9h ; mulps	xmm5, xmm1
 db 00Fh,059h,0E2h ; mulps	xmm4, xmm2
 db 00Fh,058h,0ECh ; addps	xmm5, xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,059h,0E3h ; mulps	xmm4, xmm3
 db 00Fh,058h,0ECh ; addps	xmm5, xmm4
 db 00Fh,052h,0E5h ; rsqrtps	xmm4, xmm5
 db 00Fh,057h,0F6h ; xorps	xmm6, xmm6
 db 00Fh,0C2h,0F5h,001h ; cmpltps	xmm6, xmm5
 db 00Fh,028h,0C5h ; movaps	xmm0, xmm5
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,05Ch,087h,070h,008h,000h,000h ; subps	xmm0, xmmword ptr [edi + 0x870]
 db 00Fh,059h,087h,080h,008h,000h,000h ; mulps	xmm0, xmmword ptr [edi + 0x880]
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,05Ch,0E0h ; subps	xmm4, xmm0
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,054h,0EEh ; andps	xmm5, xmm6
 db 00Fh,028h,0FDh ; movaps	xmm7, xmm5
 db 00Fh,0C2h,0BCh,02Fh,020h,00Ch,000h,000h,002h ; cmpleps	xmm7, xmmword ptr [edi + ebp + 0xc20]
 db 00Fh,050h,0CFh ; movmskps	ecx, xmm7
 db 085h,0C9h ; test	ecx, ecx
 db 00Fh,084h,00Dh,003h,000h,000h ; je	0x75f <.text+0x75f>
 db 00Fh,028h,084h,02Fh,040h,00Ch,000h,000h ; movaps	xmm0, xmmword ptr [edi + ebp + 0xc40]
 db 00Fh,054h,0E6h ; andps	xmm4, xmm6
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,028h,0B4h,02Fh,050h,00Ch,000h,000h ; movaps	xmm6, xmmword ptr [edi + ebp + 0xc50]
 db 00Fh,059h,0F5h ; mulps	xmm6, xmm5
 db 00Fh,058h,084h,02Fh,030h,00Ch,000h,000h ; addps	xmm0, xmmword ptr [edi + ebp + 0xc30]
 db 00Fh,059h,0F5h ; mulps	xmm6, xmm5
 db 00Fh,058h,0C6h ; addps	xmm0, xmm6
 db 00Fh,057h,0EDh ; xorps	xmm5, xmm5
 db 00Fh,053h,0F0h ; rcpps	xmm6, xmm0
 db 00Fh,0C2h,0E8h,001h ; cmpltps	xmm5, xmm0
 db 00Fh,059h,0CCh ; mulps	xmm1, xmm4
 db 00Fh,059h,0D4h ; mulps	xmm2, xmm4
 db 00Fh,059h,0C6h ; mulps	xmm0, xmm6
 db 00Fh,059h,0DCh ; mulps	xmm3, xmm4
 db 00Fh,05Ch,087h,070h,008h,000h,000h ; subps	xmm0, xmmword ptr [edi + 0x870]
 db 00Fh,059h,0C6h ; mulps	xmm0, xmm6
 db 00Fh,05Ch,0F0h ; subps	xmm6, xmm0
 db 00Fh,057h,0B7h,070h,008h,000h,000h ; xorps	xmm6, xmmword ptr [edi + 0x870]
 db 00Fh,054h,0F5h ; andps	xmm6, xmm5
 db 00Fh,057h,0B7h,070h,008h,000h,000h ; xorps	xmm6, xmmword ptr [edi + 0x870]
 db 00Fh,054h,0F7h ; andps	xmm6, xmm7
 db 083h,0BCh,09Fh,050h,01Fh,000h,000h,000h ; cmp	dword ptr [edi + 4*ebx + 0x1f50], 0x0
 db 00Fh,084h,046h,001h,000h,000h ; je	0x604 <.text+0x604>
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,084h,037h,090h,01Fh,000h,000h ; mulps	xmm0, xmmword ptr [edi + esi + 0x1f90]
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0A4h,037h,0A0h,01Fh,000h,000h ; mulps	xmm4, xmmword ptr [edi + esi + 0x1fa0]
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,059h,0A4h,037h,0B0h,01Fh,000h,000h ; mulps	xmm4, xmmword ptr [edi + esi + 0x1fb0]
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,05Ch,0E0h ; subps	xmm4, xmm0
 db 00Fh,028h,084h,037h,010h,021h,000h,000h ; movaps	xmm0, xmmword ptr [edi + esi + 0x2110]
 db 00Fh,0C2h,0C4h,001h ; cmpltps	xmm0, xmm4
 db 00Fh,050h,0C8h ; movmskps	ecx, xmm0
 db 085h,0C9h ; test	ecx, ecx
 db 075h,00Fh ; jne	0x50d <.text+0x50d>
 db 00Fh,054h,087h,070h,008h,000h,000h ; andps	xmm0, xmmword ptr [edi + 0x870]
 db 00Fh,059h,0F0h ; mulps	xmm6, xmm0
 db 0E9h,0F7h,000h,000h,000h ; jmp	0x604 <.text+0x604>
 db 00Fh,028h,0ECh ; movaps	xmm5, xmm4
 db 00Fh,0C2h,0ACh,037h,020h,021h,000h,000h,001h ; cmpltps	xmm5, xmmword ptr [edi + esi + 0x2120]
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,054h,0E5h ; andps	xmm4, xmm5
 db 00Fh,050h,0CCh ; movmskps	ecx, xmm4
 db 085h,0C9h ; test	ecx, ecx
 db 075h,00Fh ; jne	0x535 <.text+0x535>
 db 00Fh,054h,087h,070h,008h,000h,000h ; andps	xmm0, xmmword ptr [edi + 0x870]
 db 00Fh,059h,0F0h ; mulps	xmm6, xmm0
 db 0E9h,0CFh,000h,000h,000h ; jmp	0x604 <.text+0x604>
 db 00Fh,029h,074h,024h,030h ; movaps	xmmword ptr [esp + 0x30], xmm6
 db 00Fh,029h,07Ch,024h,040h ; movaps	xmmword ptr [esp + 0x40], xmm7
 db 00Fh,029h,04Ch,024h,050h ; movaps	xmmword ptr [esp + 0x50], xmm1
 db 00Fh,029h,054h,024h,060h ; movaps	xmmword ptr [esp + 0x60], xmm2
 db 00Fh,029h,05Ch,024h,070h ; movaps	xmmword ptr [esp + 0x70], xmm3
 db 00Fh,028h,04Ch,024h,050h ; movaps	xmm1, xmmword ptr [esp + 0x50]
 db 00Fh,059h,08Ch,037h,090h,01Fh,000h,000h ; mulps	xmm1, xmmword ptr [edi + esi + 0x1f90]
 db 00Fh,028h,054h,024h,060h ; movaps	xmm2, xmmword ptr [esp + 0x60]
 db 00Fh,059h,094h,037h,0A0h,01Fh,000h,000h ; mulps	xmm2, xmmword ptr [edi + esi + 0x1fa0]
 db 00Fh,058h,0CAh ; addps	xmm1, xmm2
 db 00Fh,028h,054h,024h,070h ; movaps	xmm2, xmmword ptr [esp + 0x70]
 db 00Fh,059h,094h,037h,0B0h,01Fh,000h,000h ; mulps	xmm2, xmmword ptr [edi + esi + 0x1fb0]
 db 00Fh,058h,0CAh ; addps	xmm1, xmm2
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,05Ch,0E1h ; subps	xmm4, xmm1
 db 00Fh,028h,094h,037h,010h,021h,000h,000h ; movaps	xmm2, xmmword ptr [edi + esi + 0x2110]
 db 00Fh,028h,09Ch,037h,020h,021h,000h,000h ; movaps	xmm3, xmmword ptr [edi + esi + 0x2120]
 db 00Fh,029h,084h,024h,0A0h,000h,000h,000h ; movaps	xmmword ptr [esp + 0xa0], xmm0
 db 00Fh,029h,0ACh,024h,0B0h,000h,000h,000h ; movaps	xmmword ptr [esp + 0xb0], xmm5
 db 00Fh,028h,0C4h ; movaps	xmm0, xmm4
 db 00Fh,05Ch,0C2h ; subps	xmm0, xmm2
 db 00Fh,05Ch,0DAh ; subps	xmm3, xmm2
 db 00Fh,05Eh,0C3h ; divps	xmm0, xmm3
 db 08Dh,094h,037h,030h,021h,000h,000h ; lea	edx, [edi + esi + 0x2130]
 db 08Bh,08Ch,09Fh,070h,01Fh,000h,000h ; mov	ecx, dword ptr [edi + 4*ebx + 0x1f70]
 db 0C7h,084h,024h,098h,000h,000h,000h,000h,000h,000h,000h ; mov	dword ptr [esp + 0x98], 0x0
 db 0E9h,0ECh,002h,000h,000h ; jmp	0x8b7 <.text+0x8b7>
 db 00Fh,028h,09Ch,024h,0B0h,000h,000h,000h ; movaps	xmm3, xmmword ptr [esp + 0xb0]
 db 00Fh,054h,0C3h ; andps	xmm0, xmm3
 db 00Fh,055h,09Fh,070h,008h,000h,000h ; andnps	xmm3, xmmword ptr [edi + 0x870]
 db 00Fh,056h,0C3h ; orps	xmm0, xmm3
 db 00Fh,054h,084h,024h,0A0h,000h,000h,000h ; andps	xmm0, xmmword ptr [esp + 0xa0]
 db 00Fh,028h,074h,024h,030h ; movaps	xmm6, xmmword ptr [esp + 0x30]
 db 00Fh,059h,0F0h ; mulps	xmm6, xmm0
 db 00Fh,028h,07Ch,024h,040h ; movaps	xmm7, xmmword ptr [esp + 0x40]
 db 00Fh,028h,04Ch,024h,050h ; movaps	xmm1, xmmword ptr [esp + 0x50]
 db 00Fh,028h,054h,024h,060h ; movaps	xmm2, xmmword ptr [esp + 0x60]
 db 00Fh,028h,05Ch,024h,070h ; movaps	xmm3, xmmword ptr [esp + 0x70]
 db 08Bh,087h,0D8h,022h,000h,000h ; mov	eax, dword ptr [edi + 0x22d8]
 db 00Fh,0A3h,0D8h ; bt	eax, ebx
 db 073h,04Bh ; jae	0x65a <.text+0x65a>
 db 00Fh,028h,084h,037h,0A0h,00Fh,000h,000h ; movaps	xmm0, xmmword ptr [edi + esi + 0xfa0]
 db 00Fh,028h,0A4h,037h,0B0h,00Fh,000h,000h ; movaps	xmm4, xmmword ptr [edi + esi + 0xfb0]
 db 00Fh,028h,0ACh,037h,0C0h,00Fh,000h,000h ; movaps	xmm5, xmmword ptr [edi + esi + 0xfc0]
 db 00Fh,059h,0C6h ; mulps	xmm0, xmm6
 db 00Fh,059h,0E6h ; mulps	xmm4, xmm6
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,087h,010h,007h,000h,000h ; addps	xmm0, xmmword ptr [edi + 0x710]
 db 00Fh,058h,0A7h,020h,007h,000h,000h ; addps	xmm4, xmmword ptr [edi + 0x720]
 db 00Fh,058h,0AFh,030h,007h,000h,000h ; addps	xmm5, xmmword ptr [edi + 0x730]
 db 00Fh,029h,087h,010h,007h,000h,000h ; movaps	xmmword ptr [edi + 0x710], xmm0
 db 00Fh,029h,0A7h,020h,007h,000h,000h ; movaps	xmmword ptr [edi + 0x720], xmm4
 db 00Fh,029h,0AFh,030h,007h,000h,000h ; movaps	xmmword ptr [edi + 0x730], xmm5
 db 083h,0BFh,0A4h,007h,000h,000h,000h ; cmp	dword ptr [edi + 0x7a4], 0x0
 db 00Fh,084h,0F8h,000h,000h,000h ; je	0x75f <.text+0x75f>
 db 083h,0BCh,024h,094h,000h,000h,000h,000h ; cmp	dword ptr [esp + 0x94], 0x0
 db 075h,020h ; jne	0x691 <.text+0x691>
 db 00Fh,059h,08Fh,080h,023h,000h,000h ; mulps	xmm1, xmmword ptr [edi + 0x2380]
 db 00Fh,059h,097h,090h,023h,000h,000h ; mulps	xmm2, xmmword ptr [edi + 0x2390]
 db 00Fh,058h,0CAh ; addps	xmm1, xmm2
 db 00Fh,059h,09Fh,0A0h,023h,000h,000h ; mulps	xmm3, xmmword ptr [edi + 0x23a0]
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 0EBh,076h ; jmp	0x707 <.text+0x707>
 db 00Fh,029h,074h,024h,030h ; movaps	xmmword ptr [esp + 0x30], xmm6
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,087h,080h,023h,000h,000h ; mulps	xmm0, xmmword ptr [edi + 0x2380]
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,059h,0A7h,090h,023h,000h,000h ; mulps	xmm4, xmmword ptr [edi + 0x2390]
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,059h,0A7h,0A0h,023h,000h,000h ; mulps	xmm4, xmmword ptr [edi + 0x23a0]
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,0C2h,0E0h,001h ; cmpltps	xmm4, xmm0
 db 00Fh,054h,0E7h ; andps	xmm4, xmm7
 db 00Fh,050h,0CCh ; movmskps	ecx, xmm4
 db 085h,0C9h ; test	ecx, ecx
 db 074h,037h ; je	0x702 <.text+0x702>
 db 00Fh,029h,044h,024h,050h ; movaps	xmmword ptr [esp + 0x50], xmm0
 db 00Fh,028h,0E1h ; movaps	xmm4, xmm1
 db 00Fh,028h,0EAh ; movaps	xmm5, xmm2
 db 00Fh,028h,0F3h ; movaps	xmm6, xmm3
 db 00Fh,028h,0CFh ; movaps	xmm1, xmm7
 db 00Fh,028h,054h,024h,030h ; movaps	xmm2, xmmword ptr [esp + 0x30]
 db 081h,0C6h,080h,001h,000h,000h ; add	esi, 0x180
 db 0C7h,084h,024h,09Ch,000h,000h,000h,001h,000h,000h,000h ; mov	dword ptr [esp + 0x9c], 0x1
 db 0E9h,097h,000h,000h,000h ; jmp	0x78e <.text+0x78e>
 db 081h,0EEh,080h,001h,000h,000h ; sub	esi, 0x180
 db 00Fh,028h,044h,024h,050h ; movaps	xmm0, xmmword ptr [esp + 0x50]
 db 00Fh,028h,074h,024h,030h ; movaps	xmm6, xmmword ptr [esp + 0x30]
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,0C2h,0E0h,001h ; cmpltps	xmm4, xmm0
 db 00Fh,054h,0C4h ; andps	xmm0, xmm4
 db 00Fh,059h,0C6h ; mulps	xmm0, xmm6
 db 00Fh,028h,08Ch,037h,020h,00Eh,000h,000h ; movaps	xmm1, xmmword ptr [edi + esi + 0xe20]
 db 00Fh,028h,094h,037h,030h,00Eh,000h,000h ; movaps	xmm2, xmmword ptr [edi + esi + 0xe30]
 db 00Fh,028h,09Ch,037h,040h,00Eh,000h,000h ; movaps	xmm3, xmmword ptr [edi + esi + 0xe40]
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,059h,0D0h ; mulps	xmm2, xmm0
 db 00Fh,059h,0D8h ; mulps	xmm3, xmm0
 db 00Fh,058h,08Fh,010h,007h,000h,000h ; addps	xmm1, xmmword ptr [edi + 0x710]
 db 00Fh,058h,097h,020h,007h,000h,000h ; addps	xmm2, xmmword ptr [edi + 0x720]
 db 00Fh,058h,09Fh,030h,007h,000h,000h ; addps	xmm3, xmmword ptr [edi + 0x730]
 db 00Fh,029h,08Fh,010h,007h,000h,000h ; movaps	xmmword ptr [edi + 0x710], xmm1
 db 00Fh,029h,097h,020h,007h,000h,000h ; movaps	xmmword ptr [edi + 0x720], xmm2
 db 00Fh,029h,09Fh,030h,007h,000h,000h ; movaps	xmmword ptr [edi + 0x730], xmm3
 db 083h,0C6h,030h ; add	esi, 0x30
 db 083h,0C5h,040h ; add	ebp, 0x40
 db 043h ; inc	ebx
 db 03Bh,09Fh,090h,00Ah,000h,000h ; cmp	ebx, dword ptr [edi + 0xa90]
 db 00Fh,082h,048h,0FCh,0FFh,0FFh ; jb	0x3ba <.text+0x3ba>
 db 08Bh,084h,024h,094h,000h,000h,000h ; mov	eax, dword ptr [esp + 0x94]
 db 089h,087h,0D0h,022h,000h,000h ; mov	dword ptr [edi + 0x22d0], eax
 db 08Bh,0ACh,024h,090h,000h,000h,000h ; mov	ebp, dword ptr [esp + 0x90]
 db 08Dh,065h,0F4h ; lea	esp, [ebp - 0xc]
 db 05Fh ; pop	edi
 db 05Eh ; pop	esi
 db 05Bh ; pop	ebx
 db 05Dh ; pop	ebp
 db 0C3h ; ret
 db 00Fh,057h,0DBh ; xorps	xmm3, xmm3
 db 00Fh,0C2h,0D8h,001h ; cmpltps	xmm3, xmm0
 db 00Fh,054h,0CBh ; andps	xmm1, xmm3
 db 00Fh,050h,0C9h ; movmskps	ecx, xmm1
 db 085h,0C9h ; test	ecx, ecx
 db 00Fh,084h,001h,001h,000h,000h ; je	0x8a4 <.text+0x8a4>
 db 00Fh,058h,024h,024h ; addps	xmm4, xmmword ptr [esp]
 db 00Fh,058h,06Ch,024h,010h ; addps	xmm5, xmmword ptr [esp + 0x10]
 db 00Fh,058h,074h,024h,020h ; addps	xmm6, xmmword ptr [esp + 0x20]
 db 00Fh,028h,0C4h ; movaps	xmm0, xmm4
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,028h,0FDh ; movaps	xmm7, xmm5
 db 00Fh,059h,0FDh ; mulps	xmm7, xmm5
 db 00Fh,058h,0C7h ; addps	xmm0, xmm7
 db 00Fh,028h,0FEh ; movaps	xmm7, xmm6
 db 00Fh,059h,0FEh ; mulps	xmm7, xmm6
 db 00Fh,058h,0C7h ; addps	xmm0, xmm7
 db 00Fh,057h,0DBh ; xorps	xmm3, xmm3
 db 00Fh,0C2h,0D8h,001h ; cmpltps	xmm3, xmm0
 db 00Fh,054h,0CBh ; andps	xmm1, xmm3
 db 00Fh,052h,0F8h ; rsqrtps	xmm7, xmm0
 db 00Fh,059h,0A7h,080h,023h,000h,000h ; mulps	xmm4, xmmword ptr [edi + 0x2380]
 db 00Fh,059h,0AFh,090h,023h,000h,000h ; mulps	xmm5, xmmword ptr [edi + 0x2390]
 db 00Fh,028h,0D8h ; movaps	xmm3, xmm0
 db 00Fh,059h,0DFh ; mulps	xmm3, xmm7
 db 00Fh,059h,0B7h,0A0h,023h,000h,000h ; mulps	xmm6, xmmword ptr [edi + 0x23a0]
 db 00Fh,058h,0E5h ; addps	xmm4, xmm5
 db 00Fh,059h,0DFh ; mulps	xmm3, xmm7
 db 00Fh,058h,0E6h ; addps	xmm4, xmm6
 db 00Fh,05Ch,09Fh,070h,008h,000h,000h ; subps	xmm3, xmmword ptr [edi + 0x870]
 db 00Fh,059h,09Fh,080h,008h,000h,000h ; mulps	xmm3, xmmword ptr [edi + 0x880]
 db 00Fh,059h,0DFh ; mulps	xmm3, xmm7
 db 00Fh,05Ch,0FBh ; subps	xmm7, xmm3
 db 00Fh,059h,0E7h ; mulps	xmm4, xmm7
 db 00Fh,028h,09Fh,020h,023h,000h,000h ; movaps	xmm3, xmmword ptr [edi + 0x2320]
 db 00Fh,0C2h,0DCh,001h ; cmpltps	xmm3, xmm4
 db 00Fh,054h,0CBh ; andps	xmm1, xmm3
 db 00Fh,050h,0C9h ; movmskps	ecx, xmm1
 db 085h,0C9h ; test	ecx, ecx
 db 074h,07Eh ; je	0x8a4 <.text+0x8a4>
 db 00Fh,029h,04Ch,024h,040h ; movaps	xmmword ptr [esp + 0x40], xmm1
 db 00Fh,029h,054h,024h,030h ; movaps	xmmword ptr [esp + 0x30], xmm2
 db 00Fh,028h,0C4h ; movaps	xmm0, xmm4
 db 00Fh,054h,0C1h ; andps	xmm0, xmm1
 db 08Dh,097h,020h,014h,000h,000h ; lea	edx, [edi + 0x1420]
 db 08Bh,08Fh,098h,014h,000h,000h ; mov	ecx, dword ptr [edi + 0x1498]
 db 0C7h,084h,024h,098h,000h,000h,000h,001h,000h,000h,000h ; mov	dword ptr [esp + 0x98], 0x1
 db 0EBh,068h ; jmp	0x8b7 <.text+0x8b7>
 db 00Fh,059h,044h,024h,030h ; mulps	xmm0, xmmword ptr [esp + 0x30]
 db 00Fh,054h,044h,024h,040h ; andps	xmm0, xmmword ptr [esp + 0x40]
 db 00Fh,028h,08Ch,037h,020h,011h,000h,000h ; movaps	xmm1, xmmword ptr [edi + esi + 0x1120]
 db 00Fh,028h,094h,037h,030h,011h,000h,000h ; movaps	xmm2, xmmword ptr [edi + esi + 0x1130]
 db 00Fh,028h,09Ch,037h,040h,011h,000h,000h ; movaps	xmm3, xmmword ptr [edi + esi + 0x1140]
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,059h,0D0h ; mulps	xmm2, xmm0
 db 00Fh,059h,0D8h ; mulps	xmm3, xmm0
 db 00Fh,058h,08Fh,0E0h,022h,000h,000h ; addps	xmm1, xmmword ptr [edi + 0x22e0]
 db 00Fh,058h,097h,0F0h,022h,000h,000h ; addps	xmm2, xmmword ptr [edi + 0x22f0]
 db 00Fh,058h,09Fh,000h,023h,000h,000h ; addps	xmm3, xmmword ptr [edi + 0x2300]
 db 00Fh,029h,08Fh,0E0h,022h,000h,000h ; movaps	xmmword ptr [edi + 0x22e0], xmm1
 db 00Fh,029h,097h,0F0h,022h,000h,000h ; movaps	xmmword ptr [edi + 0x22f0], xmm2
 db 00Fh,029h,09Fh,000h,023h,000h,000h ; movaps	xmmword ptr [edi + 0x2300], xmm3
 db 083h,0BCh,024h,09Ch,000h,000h,000h,000h ; cmp	dword ptr [esp + 0x9c], 0x0
 db 00Fh,084h,0E5h,0FAh,0FFh,0FFh ; je	0x397 <.text+0x397>
 db 0E9h,040h,0FEh,0FFh,0FFh ; jmp	0x6f7 <.text+0x6f7>
 db 083h,0F9h,003h ; cmp	ecx, 0x3
 db 075h,03Eh ; jne	0x8fa <.text+0x8fa>
 db 00Fh,05Dh,087h,070h,008h,000h,000h ; minps	xmm0, xmmword ptr [edi + 0x870]
 db 08Bh,087h,0D4h,022h,000h,000h ; mov	eax, dword ptr [edi + 0x22d4]
 db 085h,0C0h ; test	eax, eax
 db 075h,005h ; jne	0x8d2 <.text+0x8d2>
 db 0B8h,001h,000h,000h,000h ; mov	eax, 0x1
 db 0D1h,0E8h ; shr	eax
 db 072h,007h ; jb	0x8dd <.text+0x8dd>
 db 00Fh,059h,0C0h ; mulps	xmm0, xmm0
 db 0D1h,0E8h ; shr	eax
 db 073h,0F9h ; jae	0x8d6 <.text+0x8d6>
 db 00Fh,028h,0C8h ; movaps	xmm1, xmm0
 db 085h,0C0h ; test	eax, eax
 db 074h,00Eh ; je	0x8f2 <.text+0x8f2>
 db 00Fh,059h,0C0h ; mulps	xmm0, xmm0
 db 0D1h,0E8h ; shr	eax
 db 073h,0F9h ; jae	0x8e4 <.text+0x8e4>
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 085h,0C0h ; test	eax, eax
 db 075h,0F2h ; jne	0x8e4 <.text+0x8e4>
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 0E9h,0AAh,001h,000h,000h ; jmp	0xaa4 <.text+0xaa4>
 db 083h,0F9h,001h ; cmp	ecx, 0x1
 db 075h,00Ch ; jne	0x90b <.text+0x90b>
 db 00Fh,05Dh,087h,070h,008h,000h,000h ; minps	xmm0, xmmword ptr [edi + 0x870]
 db 0E9h,099h,001h,000h,000h ; jmp	0xaa4 <.text+0xaa4>
 db 083h,0F9h,002h ; cmp	ecx, 0x2
 db 075h,00Ch ; jne	0x91c <.text+0x91c>
 db 00Fh,028h,087h,070h,008h,000h,000h ; movaps	xmm0, xmmword ptr [edi + 0x870]
 db 0E9h,088h,001h,000h,000h ; jmp	0xaa4 <.text+0xaa4>
 db 00Fh,028h,0BFh,070h,008h,000h,000h ; movaps	xmm7, xmmword ptr [edi + 0x870]
 db 00Fh,05Dh,0C7h ; minps	xmm0, xmm7
 db 00Fh,029h,084h,024h,080h,000h,000h,000h ; movaps	xmmword ptr [esp + 0x80], xmm0
 db 00Fh,028h,0C8h ; movaps	xmm1, xmm0
 db 066h,00Fh,072h,0D1h,017h ; psrld	xmm1, 0x17
 db 066h,00Fh,0FAh,08Fh,050h,009h,000h,000h ; psubd	xmm1, xmmword ptr [edi + 0x950]
 db 00Fh,054h,087h,080h,014h,000h,000h ; andps	xmm0, xmmword ptr [edi + 0x1480]
 db 00Fh,056h,0C7h ; orps	xmm0, xmm7
 db 00Fh,028h,097h,070h,014h,000h,000h ; movaps	xmm2, xmmword ptr [edi + 0x1470]
 db 00Fh,0C2h,0D0h,001h ; cmpltps	xmm2, xmm0
 db 00Fh,028h,0D8h ; movaps	xmm3, xmm0
 db 00Fh,059h,09Fh,080h,008h,000h,000h ; mulps	xmm3, xmmword ptr [edi + 0x880]
 db 00Fh,054h,0DAh ; andps	xmm3, xmm2
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,055h,0E0h ; andnps	xmm4, xmm0
 db 00Fh,056h,0DCh ; orps	xmm3, xmm4
 db 00Fh,05Bh,0C9h ; cvtdq2ps	xmm1, xmm1
 db 00Fh,054h,0D7h ; andps	xmm2, xmm7
 db 00Fh,058h,0CAh ; addps	xmm1, xmm2
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,058h,0E7h ; addps	xmm4, xmm7
 db 00Fh,053h,0ECh ; rcpps	xmm5, xmm4
 db 00Fh,05Ch,0DFh ; subps	xmm3, xmm7
 db 00Fh,059h,08Fh,0B0h,008h,000h,000h ; mulps	xmm1, xmmword ptr [edi + 0x8b0]
 db 00Fh,028h,0F4h ; movaps	xmm6, xmm4
 db 00Fh,059h,0F5h ; mulps	xmm6, xmm5
 db 00Fh,028h,0D5h ; movaps	xmm2, xmm5
 db 00Fh,058h,0D5h ; addps	xmm2, xmm5
 db 00Fh,059h,0F5h ; mulps	xmm6, xmm5
 db 00Fh,05Ch,0D6h ; subps	xmm2, xmm6
 db 00Fh,059h,0DAh ; mulps	xmm3, xmm2
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,059h,0E3h ; mulps	xmm4, xmm3
 db 00Fh,028h,0AFh,060h,014h,000h,000h ; movaps	xmm5, xmmword ptr [edi + 0x1460]
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,058h,0AFh,050h,014h,000h,000h ; addps	xmm5, xmmword ptr [edi + 0x1450]
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,058h,0AFh,040h,014h,000h,000h ; addps	xmm5, xmmword ptr [edi + 0x1440]
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,058h,0AFh,030h,014h,000h,000h ; addps	xmm5, xmmword ptr [edi + 0x1430]
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,058h,0EFh ; addps	xmm5, xmm7
 db 00Fh,058h,0DBh ; addps	xmm3, xmm3
 db 00Fh,059h,0DDh ; mulps	xmm3, xmm5
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,028h,012h ; movaps	xmm2, xmmword ptr [edx]
 db 00Fh,059h,0D1h ; mulps	xmm2, xmm1
 db 00Fh,028h,0DAh ; movaps	xmm3, xmm2
 db 00Fh,028h,087h,020h,009h,000h,000h ; movaps	xmm0, xmmword ptr [edi + 0x920]
 db 00Fh,0C2h,0C2h,001h ; cmpltps	xmm0, xmm2
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,028h,0CAh ; movaps	xmm1, xmm2
 db 00Fh,0C2h,0CCh,001h ; cmpltps	xmm1, xmm4
 db 00Fh,05Fh,097h,020h,009h,000h,000h ; maxps	xmm2, xmmword ptr [edi + 0x920]
 db 00Fh,05Dh,0D4h ; minps	xmm2, xmm4
 db 00Fh,059h,097h,090h,008h,000h,000h ; mulps	xmm2, xmmword ptr [edi + 0x890]
 db 0F3h,00Fh,05Bh,0E2h ; cvttps2dq	xmm4, xmm2
 db 00Fh,05Bh,0ECh ; cvtdq2ps	xmm5, xmm4
 db 00Fh,028h,0F2h ; movaps	xmm6, xmm2
 db 00Fh,0C2h,0F5h,001h ; cmpltps	xmm6, xmm5
 db 066h,00Fh,072h,0D6h,01Fh ; psrld	xmm6, 0x1f
 db 066h,00Fh,0FAh,0E6h ; psubd	xmm4, xmm6
 db 00Fh,05Bh,0ECh ; cvtdq2ps	xmm5, xmm4
 db 00Fh,028h,0F2h ; movaps	xmm6, xmm2
 db 00Fh,05Ch,0F5h ; subps	xmm6, xmm5
 db 00Fh,028h,0AFh,000h,009h,000h,000h ; movaps	xmm5, xmmword ptr [edi + 0x900]
 db 066h,00Fh,0FEh,0A7h,050h,009h,000h,000h ; paddd	xmm4, xmmword ptr [edi + 0x950]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 066h,00Fh,072h,0F4h,017h ; pslld	xmm4, 0x17
 db 00Fh,058h,0AFh,0F0h,008h,000h,000h ; addps	xmm5, xmmword ptr [edi + 0x8f0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0AFh,0E0h,008h,000h,000h ; addps	xmm5, xmmword ptr [edi + 0x8e0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0AFh,0D0h,008h,000h,000h ; addps	xmm5, xmmword ptr [edi + 0x8d0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0AFh,0C0h,008h,000h,000h ; addps	xmm5, xmmword ptr [edi + 0x8c0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0AFh,0B0h,008h,000h,000h ; addps	xmm5, xmmword ptr [edi + 0x8b0]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,058h,0AFh,0A0h,008h,000h,000h ; addps	xmm5, xmmword ptr [edi + 0x8a0]
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,054h,0E8h ; andps	xmm5, xmm0
 db 00Fh,054h,0E9h ; andps	xmm5, xmm1
 db 00Fh,028h,0E7h ; movaps	xmm4, xmm7
 db 00Fh,057h,0C0h ; xorps	xmm0, xmm0
 db 00Fh,0C2h,0C3h,002h ; cmpleps	xmm0, xmm3
 db 00Fh,054h,0E0h ; andps	xmm4, xmm0
 db 00Fh,056h,0ECh ; orps	xmm5, xmm4
 db 00Fh,028h,0C7h ; movaps	xmm0, xmm7
 db 00Fh,0C2h,084h,024h,080h,000h,000h,000h,002h ; cmpleps	xmm0, xmmword ptr [esp + 0x80]
 db 00Fh,028h,0C8h ; movaps	xmm1, xmm0
 db 00Fh,054h,0CFh ; andps	xmm1, xmm7
 db 00Fh,055h,0C5h ; andnps	xmm0, xmm5
 db 00Fh,056h,0C1h ; orps	xmm0, xmm1
 db 083h,0BCh,024h,098h,000h,000h,000h,000h ; cmp	dword ptr [esp + 0x98], 0x0
 db 00Fh,084h,019h,0FBh,0FFh,0FFh ; je	0x5cb <.text+0x5cb>
 db 0E9h,098h,0FDh,0FFh,0FFh ; jmp	0x84f <.text+0x84f>
_tnk_splight endp
public _tnk_spec
_tnk_spec proc near
 db 055h ; push	ebp
 db 089h,0E5h ; mov	ebp, esp
 db 053h ; push	ebx
 db 056h ; push	esi
 db 083h,0E4h,0F0h ; and	esp, -0x10
 db 081h,0ECh,0F0h,000h,000h,000h ; sub	esp, 0xf0
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 083h,0B8h,0D0h,022h,000h,000h,000h ; cmp	dword ptr [eax + 0x22d0], 0x0
 db 074h,021h ; je	0x3b <_tnk_spec+0x3b>
 db 00Fh,010h,090h,0E0h,022h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x22e0]
 db 00Fh,010h,080h,0F0h,022h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x22f0]
 db 00Fh,010h,0A0h,000h,023h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x2300]
 db 00Fh,010h,0B0h,060h,001h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x160]
 db 0E9h,065h,007h,000h,000h ; jmp	0x7a0 <_tnk_spec+0x7a0>
 db 00Fh,010h,0B0h,060h,001h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x160]
 db 00Fh,010h,080h,070h,008h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x870]
 db 00Fh,029h,004h,024h ; movaps	xmmword ptr [esp], xmm0
 db 00Fh,011h,0B0h,0E0h,022h,000h,000h ; movups	xmmword ptr [eax + 0x22e0], xmm6
 db 00Fh,011h,0B0h,0F0h,022h,000h,000h ; movups	xmmword ptr [eax + 0x22f0], xmm6
 db 00Fh,011h,0B0h,000h,023h,000h,000h ; movups	xmmword ptr [eax + 0x2300], xmm6
 db 08Bh,088h,094h,014h,000h,000h ; mov	ecx, dword ptr [eax + 0x1494]
 db 00Fh,029h,074h,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm6
 db 00Fh,028h,0E6h ; movaps	xmm4, xmm6
 db 00Fh,028h,0C6h ; movaps	xmm0, xmm6
 db 085h,0C9h ; test	ecx, ecx
 db 00Fh,084h,0FAh,000h,000h,000h ; je	0x175 <_tnk_spec+0x175>
 db 083h,0B8h,0E4h,023h,000h,000h,000h ; cmp	dword ptr [eax + 0x23e4], 0x0
 db 074h,01Fh ; je	0xa3 <_tnk_spec+0xa3>
 db 00Fh,010h,080h,0B0h,023h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x23b0]
 db 00Fh,010h,088h,0C0h,023h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x23c0]
 db 00Fh,029h,04Ch,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm1
 db 00Fh,010h,0A0h,0D0h,023h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x23d0]
 db 0E9h,0D2h,000h,000h,000h ; jmp	0x175 <_tnk_spec+0x175>
 db 00Fh,010h,098h,0C0h,007h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x7c0]
 db 00Fh,010h,090h,0D0h,007h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x7d0]
 db 00Fh,010h,088h,0E0h,007h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x7e0]
 db 00Fh,010h,080h,080h,001h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x180]
 db 00Fh,010h,0A0h,090h,001h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x190]
 db 00Fh,059h,0C3h ; mulps	xmm0, xmm3
 db 00Fh,010h,0A8h,0C0h,001h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x1c0]
 db 00Fh,059h,0EAh ; mulps	xmm5, xmm2
 db 00Fh,058h,0E8h ; addps	xmm5, xmm0
 db 00Fh,028h,0FEh ; movaps	xmm7, xmm6
 db 00Fh,010h,0B0h,000h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x200]
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,0F1h ; mulps	xmm6, xmm1
 db 00Fh,058h,0F5h ; addps	xmm6, xmm5
 db 00Fh,010h,088h,040h,002h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x240]
 db 00Fh,058h,0CEh ; addps	xmm1, xmm6
 db 00Fh,028h,0F7h ; movaps	xmm6, xmm7
 db 00Fh,059h,0E3h ; mulps	xmm4, xmm3
 db 00Fh,010h,0A8h,0D0h,001h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x1d0]
 db 00Fh,059h,0EAh ; mulps	xmm5, xmm2
 db 00Fh,058h,0ECh ; addps	xmm5, xmm4
 db 00Fh,010h,0A0h,010h,002h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x210]
 db 00Fh,059h,0E0h ; mulps	xmm4, xmm0
 db 00Fh,058h,0E5h ; addps	xmm4, xmm5
 db 00Fh,010h,0A8h,050h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x250]
 db 00Fh,058h,0ECh ; addps	xmm5, xmm4
 db 00Fh,010h,0A0h,0A0h,001h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x1a0]
 db 00Fh,059h,0E3h ; mulps	xmm4, xmm3
 db 00Fh,010h,098h,0E0h,001h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x1e0]
 db 00Fh,059h,0DAh ; mulps	xmm3, xmm2
 db 00Fh,058h,0DCh ; addps	xmm3, xmm4
 db 00Fh,010h,0A0h,020h,002h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x220]
 db 00Fh,059h,0E0h ; mulps	xmm4, xmm0
 db 00Fh,058h,0E3h ; addps	xmm4, xmm3
 db 00Fh,010h,080h,060h,002h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x260]
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,011h,088h,0B0h,023h,000h,000h ; movups	xmmword ptr [eax + 0x23b0], xmm1
 db 00Fh,029h,06Ch,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm5
 db 00Fh,011h,0A8h,0C0h,023h,000h,000h ; movups	xmmword ptr [eax + 0x23c0], xmm5
 db 00Fh,011h,0A0h,0D0h,023h,000h,000h ; movups	xmmword ptr [eax + 0x23d0], xmm4
 db 0C7h,080h,0E4h,023h,000h,000h,001h,000h,000h,000h ; mov	dword ptr [eax + 0x23e4], 0x1
 db 083h,0B8h,0E0h,023h,000h,000h,000h ; cmp	dword ptr [eax + 0x23e0], 0x0
 db 00Fh,029h,074h,024h,040h ; movaps	xmmword ptr [esp + 0x40], xmm6
 db 074h,01Ah ; je	0x19d <_tnk_spec+0x19d>
 db 00Fh,010h,090h,080h,023h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x2380]
 db 00Fh,010h,098h,090h,023h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x2390]
 db 00Fh,010h,0A8h,0A0h,023h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x23a0]
 db 0E9h,021h,001h,000h,000h ; jmp	0x2be <_tnk_spec+0x2be>
 db 00Fh,029h,044h,024h,020h ; movaps	xmmword ptr [esp + 0x20], xmm0
 db 00Fh,028h,0C4h ; movaps	xmm0, xmm4
 db 00Fh,010h,0A0h,0F0h,007h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x7f0]
 db 00Fh,010h,098h,000h,008h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x800]
 db 00Fh,010h,088h,010h,008h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x810]
 db 00Fh,010h,0B0h,080h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x280]
 db 00Fh,010h,0A8h,090h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x290]
 db 00Fh,059h,0F4h ; mulps	xmm6, xmm4
 db 00Fh,010h,0B8h,0B0h,002h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x2b0]
 db 00Fh,059h,0FBh ; mulps	xmm7, xmm3
 db 00Fh,058h,0FEh ; addps	xmm7, xmm6
 db 00Fh,010h,090h,0E0h,002h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x2e0]
 db 00Fh,059h,0D1h ; mulps	xmm2, xmm1
 db 00Fh,058h,0D7h ; addps	xmm2, xmm7
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,010h,0B0h,0C0h,002h,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x2c0]
 db 00Fh,059h,0F3h ; mulps	xmm6, xmm3
 db 00Fh,058h,0F5h ; addps	xmm6, xmm5
 db 00Fh,010h,0B8h,0F0h,002h,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x2f0]
 db 00Fh,059h,0F9h ; mulps	xmm7, xmm1
 db 00Fh,058h,0FEh ; addps	xmm7, xmm6
 db 00Fh,010h,0A8h,0A0h,002h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x2a0]
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,010h,0A0h,0D0h,002h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x2d0]
 db 00Fh,059h,0E3h ; mulps	xmm4, xmm3
 db 00Fh,058h,0E5h ; addps	xmm4, xmm5
 db 00Fh,010h,0A8h,000h,003h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x300]
 db 00Fh,059h,0E9h ; mulps	xmm5, xmm1
 db 00Fh,058h,0ECh ; addps	xmm5, xmm4
 db 083h,0B8h,0A0h,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7a0], 0x0
 db 074h,060h ; je	0x28f <_tnk_spec+0x28f>
 db 00Fh,028h,0CAh ; movaps	xmm1, xmm2
 db 00Fh,059h,0CAh ; mulps	xmm1, xmm2
 db 00Fh,028h,0DFh ; movaps	xmm3, xmm7
 db 00Fh,059h,0DFh ; mulps	xmm3, xmm7
 db 00Fh,058h,0D9h ; addps	xmm3, xmm1
 db 00Fh,028h,0CDh ; movaps	xmm1, xmm5
 db 00Fh,059h,0CDh ; mulps	xmm1, xmm5
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,051h,0E1h ; sqrtps	xmm4, xmm1
 db 00Fh,028h,074h,024h,040h ; movaps	xmm6, xmmword ptr [esp + 0x40]
 db 00Fh,028h,0CEh ; movaps	xmm1, xmm6
 db 00Fh,0C2h,0CCh,001h ; cmpltps	xmm1, xmm4
 db 00Fh,028h,01Ch,024h ; movaps	xmm3, xmmword ptr [esp]
 db 00Fh,05Eh,0DCh ; divps	xmm3, xmm4
 db 00Fh,054h,0D9h ; andps	xmm3, xmm1
 db 00Fh,028h,0E1h ; movaps	xmm4, xmm1
 db 00Fh,055h,0E2h ; andnps	xmm4, xmm2
 db 00Fh,059h,0D3h ; mulps	xmm2, xmm3
 db 00Fh,054h,0D1h ; andps	xmm2, xmm1
 db 00Fh,056h,0D4h ; orps	xmm2, xmm4
 db 00Fh,028h,0E1h ; movaps	xmm4, xmm1
 db 00Fh,055h,0E7h ; andnps	xmm4, xmm7
 db 00Fh,059h,0FBh ; mulps	xmm7, xmm3
 db 00Fh,054h,0F9h ; andps	xmm7, xmm1
 db 00Fh,056h,0FCh ; orps	xmm7, xmm4
 db 00Fh,059h,0DDh ; mulps	xmm3, xmm5
 db 00Fh,054h,0D9h ; andps	xmm3, xmm1
 db 00Fh,055h,0CDh ; andnps	xmm1, xmm5
 db 00Fh,056h,0CBh ; orps	xmm1, xmm3
 db 00Fh,028h,0E9h ; movaps	xmm5, xmm1
 db 0EBh,005h ; jmp	0x294 <_tnk_spec+0x294>
 db 00Fh,028h,074h,024h,040h ; movaps	xmm6, xmmword ptr [esp + 0x40]
 db 00Fh,011h,090h,080h,023h,000h,000h ; movups	xmmword ptr [eax + 0x2380], xmm2
 db 00Fh,011h,0B8h,090h,023h,000h,000h ; movups	xmmword ptr [eax + 0x2390], xmm7
 db 00Fh,011h,0A8h,0A0h,023h,000h,000h ; movups	xmmword ptr [eax + 0x23a0], xmm5
 db 0C7h,080h,0E0h,023h,000h,000h,001h,000h,000h,000h ; mov	dword ptr [eax + 0x23e0], 0x1
 db 00Fh,028h,0DFh ; movaps	xmm3, xmm7
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,028h,044h,024h,020h ; movaps	xmm0, xmmword ptr [esp + 0x20]
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,028h,03Ch,024h ; movaps	xmm7, xmmword ptr [esp]
 db 00Fh,05Ch,0CFh ; subps	xmm1, xmm7
 db 085h,0C9h ; test	ecx, ecx
 db 00Fh,029h,054h,024h,030h ; movaps	xmmword ptr [esp + 0x30], xmm2
 db 00Fh,029h,06Ch,024h,060h ; movaps	xmmword ptr [esp + 0x60], xmm5
 db 00Fh,029h,05Ch,024h,020h ; movaps	xmmword ptr [esp + 0x20], xmm3
 db 00Fh,084h,085h,000h,000h,000h ; je	0x364 <_tnk_spec+0x364>
 db 00Fh,029h,04Ch,024h,050h ; movaps	xmmword ptr [esp + 0x50], xmm1
 db 00Fh,028h,0C8h ; movaps	xmm1, xmm0
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,028h,054h,024h,010h ; movaps	xmm2, xmmword ptr [esp + 0x10]
 db 00Fh,028h,0DAh ; movaps	xmm3, xmm2
 db 00Fh,059h,0DAh ; mulps	xmm3, xmm2
 db 00Fh,058h,0D9h ; addps	xmm3, xmm1
 db 00Fh,029h,0A4h,024h,0A0h,000h,000h,000h ; movaps	xmmword ptr [esp + 0xa0], xmm4
 db 00Fh,059h,0E4h ; mulps	xmm4, xmm4
 db 00Fh,058h,0E3h ; addps	xmm4, xmm3
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,0C2h,0CCh,001h ; cmpltps	xmm1, xmm4
 db 00Fh,052h,0ECh ; rsqrtps	xmm5, xmm4
 db 00Fh,059h,0E5h ; mulps	xmm4, xmm5
 db 00Fh,059h,0E5h ; mulps	xmm4, xmm5
 db 00Fh,05Ch,0E7h ; subps	xmm4, xmm7
 db 00Fh,010h,098h,080h,008h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x880]
 db 00Fh,059h,0DCh ; mulps	xmm3, xmm4
 db 00Fh,059h,0DDh ; mulps	xmm3, xmm5
 db 00Fh,05Ch,0DDh ; subps	xmm3, xmm5
 db 00Fh,028h,06Ch,024h,060h ; movaps	xmm5, xmmword ptr [esp + 0x60]
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,058h,0DCh ; addps	xmm3, xmm4
 db 00Fh,059h,0C3h ; mulps	xmm0, xmm3
 db 00Fh,059h,0D3h ; mulps	xmm2, xmm3
 db 00Fh,059h,09Ch,024h,0A0h,000h,000h,000h ; mulps	xmm3, xmmword ptr [esp + 0xa0]
 db 00Fh,054h,0C1h ; andps	xmm0, xmm1
 db 00Fh,054h,0D1h ; andps	xmm2, xmm1
 db 00Fh,029h,054h,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm2
 db 00Fh,028h,054h,024h,030h ; movaps	xmm2, xmmword ptr [esp + 0x30]
 db 00Fh,054h,0D9h ; andps	xmm3, xmm1
 db 00Fh,055h,04Ch,024h,050h ; andnps	xmm1, xmmword ptr [esp + 0x50]
 db 00Fh,056h,0CBh ; orps	xmm1, xmm3
 db 00Fh,028h,05Ch,024h,020h ; movaps	xmm3, xmmword ptr [esp + 0x20]
 db 0EBh,008h ; jmp	0x36c <_tnk_spec+0x36c>
 db 00Fh,057h,0C0h ; xorps	xmm0, xmm0
 db 00Fh,029h,044h,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm0
 db 08Bh,0B0h,090h,007h,000h,000h ; mov	esi, dword ptr [eax + 0x790]
 db 085h,0F6h ; test	esi, esi
 db 00Fh,084h,01Dh,004h,000h,000h ; je	0x797 <_tnk_spec+0x797>
 db 00Fh,029h,084h,024h,0D0h,000h,000h,000h ; movaps	xmmword ptr [esp + 0xd0], xmm0
 db 00Fh,029h,04Ch,024h,050h ; movaps	xmmword ptr [esp + 0x50], xmm1
 db 031h,0D2h ; xor	edx, edx
 db 00Fh,057h,0C0h ; xorps	xmm0, xmm0
 db 00Fh,028h,00Ch,024h ; movaps	xmm1, xmmword ptr [esp]
 db 00Fh,0C2h,0C8h,007h ; cmpordps	xmm1, xmm0
 db 00Fh,029h,08Ch,024h,0A0h,000h,000h,000h ; movaps	xmmword ptr [esp + 0xa0], xmm1
 db 00Fh,029h,0B4h,024h,090h,000h,000h,000h ; movaps	xmmword ptr [esp + 0x90], xmm6
 db 00Fh,029h,0B4h,024h,080h,000h,000h,000h ; movaps	xmmword ptr [esp + 0x80], xmm6
 db 00Fh,029h,074h,024h,070h ; movaps	xmmword ptr [esp + 0x70], xmm6
 db 0EBh,01Bh ; jmp	0x3ce <_tnk_spec+0x3ce>
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 00Fh,028h,054h,024h,030h ; movaps	xmm2, xmmword ptr [esp + 0x30]
 db 042h ; inc	edx
 db 039h,0F2h ; cmp	edx, esi
 db 00Fh,084h,0ADh,003h,000h,000h ; je	0x77b <_tnk_spec+0x77b>
 db 089h,0D1h ; mov	ecx, edx
 db 0C1h,0E1h,006h ; shl	ecx, 0x6
 db 00Fh,010h,0A4h,008h,010h,003h,000h,000h ; movups	xmm4, xmmword ptr [eax + ecx + 0x310]
 db 00Fh,010h,08Ch,008h,020h,003h,000h,000h ; movups	xmm1, xmmword ptr [eax + ecx + 0x320]
 db 00Fh,010h,0B4h,008h,030h,003h,000h,000h ; movups	xmm6, xmmword ptr [eax + ecx + 0x330]
 db 00Fh,028h,0C2h ; movaps	xmm0, xmm2
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,028h,0D3h ; movaps	xmm2, xmm3
 db 00Fh,059h,0D1h ; mulps	xmm2, xmm1
 db 00Fh,058h,0D0h ; addps	xmm2, xmm0
 db 00Fh,028h,0C5h ; movaps	xmm0, xmm5
 db 00Fh,059h,0C6h ; mulps	xmm0, xmm6
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,057h,0FFh ; xorps	xmm7, xmm7
 db 00Fh,0C2h,0F8h,001h ; cmpltps	xmm7, xmm0
 db 00Fh,054h,0BCh,024h,0A0h,000h,000h,000h ; andps	xmm7, xmmword ptr [esp + 0xa0]
 db 00Fh,050h,0CFh ; movmskps	ecx, xmm7
 db 085h,0C9h ; test	ecx, ecx
 db 074h,0A7h ; je	0x3c0 <_tnk_spec+0x3c0>
 db 00Fh,058h,0A4h,024h,0D0h,000h,000h,000h ; addps	xmm4, xmmword ptr [esp + 0xd0]
 db 00Fh,058h,04Ch,024h,010h ; addps	xmm1, xmmword ptr [esp + 0x10]
 db 00Fh,058h,074h,024h,050h ; addps	xmm6, xmmword ptr [esp + 0x50]
 db 00Fh,028h,0C4h ; movaps	xmm0, xmm4
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 00Fh,059h,0D9h ; mulps	xmm3, xmm1
 db 00Fh,058h,0D8h ; addps	xmm3, xmm0
 db 00Fh,028h,0D6h ; movaps	xmm2, xmm6
 db 00Fh,059h,0D6h ; mulps	xmm2, xmm6
 db 00Fh,058h,0D3h ; addps	xmm2, xmm3
 db 00Fh,057h,0EDh ; xorps	xmm5, xmm5
 db 00Fh,0C2h,0EAh,001h ; cmpltps	xmm5, xmm2
 db 00Fh,052h,0C2h ; rsqrtps	xmm0, xmm2
 db 00Fh,059h,0D0h ; mulps	xmm2, xmm0
 db 00Fh,059h,0D0h ; mulps	xmm2, xmm0
 db 00Fh,05Ch,014h,024h ; subps	xmm2, xmmword ptr [esp]
 db 00Fh,010h,098h,080h,008h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x880]
 db 00Fh,029h,09Ch,024h,0E0h,000h,000h,000h ; movaps	xmmword ptr [esp + 0xe0], xmm3
 db 00Fh,059h,0D3h ; mulps	xmm2, xmm3
 db 00Fh,059h,0D0h ; mulps	xmm2, xmm0
 db 00Fh,05Ch,0C2h ; subps	xmm0, xmm2
 db 00Fh,028h,054h,024h,030h ; movaps	xmm2, xmmword ptr [esp + 0x30]
 db 00Fh,059h,0E2h ; mulps	xmm4, xmm2
 db 00Fh,059h,04Ch,024h,020h ; mulps	xmm1, xmmword ptr [esp + 0x20]
 db 00Fh,028h,05Ch,024h,060h ; movaps	xmm3, xmmword ptr [esp + 0x60]
 db 00Fh,058h,0CCh ; addps	xmm1, xmm4
 db 00Fh,028h,0E5h ; movaps	xmm4, xmm5
 db 00Fh,028h,0EBh ; movaps	xmm5, xmm3
 db 00Fh,059h,0F3h ; mulps	xmm6, xmm3
 db 00Fh,058h,0F1h ; addps	xmm6, xmm1
 db 00Fh,059h,0F0h ; mulps	xmm6, xmm0
 db 00Fh,054h,0E7h ; andps	xmm4, xmm7
 db 00Fh,010h,080h,020h,023h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x2320]
 db 00Fh,0C2h,0C6h,001h ; cmpltps	xmm0, xmm6
 db 00Fh,054h,0E0h ; andps	xmm4, xmm0
 db 00Fh,050h,0CCh ; movmskps	ecx, xmm4
 db 085h,0C9h ; test	ecx, ecx
 db 00Fh,084h,0C2h,002h,000h,000h ; je	0x771 <_tnk_spec+0x771>
 db 00Fh,054h,0F4h ; andps	xmm6, xmm4
 db 08Bh,088h,098h,014h,000h,000h ; mov	ecx, dword ptr [eax + 0x1498]
 db 083h,0F9h,003h ; cmp	ecx, 0x3
 db 074h,020h ; je	0x4dd <_tnk_spec+0x4dd>
 db 00Fh,028h,004h,024h ; movaps	xmm0, xmmword ptr [esp]
 db 00Fh,028h,0C8h ; movaps	xmm1, xmm0
 db 083h,0F9h,002h ; cmp	ecx, 0x2
 db 00Fh,084h,02Fh,002h,000h,000h ; je	0x6fc <_tnk_spec+0x6fc>
 db 00Fh,05Dh,0F0h ; minps	xmm6, xmm0
 db 083h,0F9h,001h ; cmp	ecx, 0x1
 db 075h,079h ; jne	0x54e <_tnk_spec+0x54e>
 db 00Fh,028h,0CEh ; movaps	xmm1, xmm6
 db 0E9h,01Fh,002h,000h,000h ; jmp	0x6fc <_tnk_spec+0x6fc>
 db 08Bh,088h,0D4h,022h,000h,000h ; mov	ecx, dword ptr [eax + 0x22d4]
 db 00Fh,028h,004h,024h ; movaps	xmm0, xmmword ptr [esp]
 db 00Fh,05Dh,0F0h ; minps	xmm6, xmm0
 db 083h,0F9h,001h ; cmp	ecx, 0x1
 db 083h,0D1h,000h ; adc	ecx, 0x0
 db 0F6h,0C1h,001h ; test	cl, 0x1
 db 075h,019h ; jne	0x50e <_tnk_spec+0x50e>
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 00Fh,059h,0F6h ; mulps	xmm6, xmm6
 db 089h,0CBh ; mov	ebx, ecx
 db 0D1h,0EBh ; shr	ebx
 db 0F6h,0C1h,002h ; test	cl, 0x2
 db 089h,0D9h ; mov	ecx, ebx
 db 074h,0F2h ; je	0x500 <_tnk_spec+0x500>
 db 00Fh,028h,0CEh ; movaps	xmm1, xmm6
 db 0EBh,015h ; jmp	0x528 <_tnk_spec+0x528>
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 089h,0D9h ; mov	ecx, ebx
 db 00Fh,059h,0F6h ; mulps	xmm6, xmm6
 db 00Fh,059h,0CEh ; mulps	xmm1, xmm6
 db 083h,0F9h,002h ; cmp	ecx, 0x2
 db 00Fh,082h,0CBh,001h,000h,000h ; jb	0x6fc <_tnk_spec+0x6fc>
 db 089h,0CBh ; mov	ebx, ecx
 db 0D1h,0EBh ; shr	ebx
 db 0F6h,0C1h,002h ; test	cl, 0x2
 db 075h,0E6h ; jne	0x520 <_tnk_spec+0x520>
 db 089h,0D9h ; mov	ecx, ebx
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 00Fh,059h,0F6h ; mulps	xmm6, xmm6
 db 0D1h,0E9h ; shr	ecx
 db 0F6h,0C3h,002h ; test	bl, 0x2
 db 089h,0CBh ; mov	ebx, ecx
 db 074h,0F4h ; je	0x540 <_tnk_spec+0x540>
 db 0EBh,0D4h ; jmp	0x522 <_tnk_spec+0x522>
 db 00Fh,028h,0CEh ; movaps	xmm1, xmm6
 db 066h,00Fh,072h,0D1h,017h ; psrld	xmm1, 0x17
 db 0F3h,00Fh,06Fh,090h,050h,009h,000h,000h ; movdqu	xmm2, xmmword ptr [eax + 0x950]
 db 066h,00Fh,07Fh,094h,024h,0B0h,000h,000h,000h ; movdqa	xmmword ptr [esp + 0xb0], xmm2
 db 066h,00Fh,0FAh,0CAh ; psubd	xmm1, xmm2
 db 00Fh,010h,090h,080h,014h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x1480]
 db 00Fh,054h,0D6h ; andps	xmm2, xmm6
 db 00Fh,056h,0D0h ; orps	xmm2, xmm0
 db 00Fh,029h,0A4h,024h,0C0h,000h,000h,000h ; movaps	xmmword ptr [esp + 0xc0], xmm4
 db 00Fh,010h,0A0h,070h,014h,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x1470]
 db 00Fh,0C2h,0E2h,001h ; cmpltps	xmm4, xmm2
 db 00Fh,028h,09Ch,024h,0E0h,000h,000h,000h ; movaps	xmm3, xmmword ptr [esp + 0xe0]
 db 00Fh,059h,0DAh ; mulps	xmm3, xmm2
 db 00Fh,054h,0DCh ; andps	xmm3, xmm4
 db 00Fh,028h,0C4h ; movaps	xmm0, xmm4
 db 00Fh,055h,0C2h ; andnps	xmm0, xmm2
 db 00Fh,056h,0C3h ; orps	xmm0, xmm3
 db 00Fh,05Bh,0C9h ; cvtdq2ps	xmm1, xmm1
 db 00Fh,054h,024h,024h ; andps	xmm4, xmmword ptr [esp]
 db 00Fh,058h,0E1h ; addps	xmm4, xmm1
 db 00Fh,028h,014h,024h ; movaps	xmm2, xmmword ptr [esp]
 db 00Fh,058h,0D0h ; addps	xmm2, xmm0
 db 00Fh,053h,0CAh ; rcpps	xmm1, xmm2
 db 00Fh,05Ch,004h,024h ; subps	xmm0, xmmword ptr [esp]
 db 00Fh,059h,0D1h ; mulps	xmm2, xmm1
 db 00Fh,059h,0D1h ; mulps	xmm2, xmm1
 db 00Fh,058h,0C9h ; addps	xmm1, xmm1
 db 00Fh,05Ch,0CAh ; subps	xmm1, xmm2
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 00Fh,010h,090h,060h,014h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x1460]
 db 00Fh,059h,0D0h ; mulps	xmm2, xmm0
 db 00Fh,010h,0A8h,050h,014h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x1450]
 db 00Fh,058h,0EAh ; addps	xmm5, xmm2
 db 00Fh,059h,0E8h ; mulps	xmm5, xmm0
 db 00Fh,010h,090h,040h,014h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x1440]
 db 00Fh,058h,0D5h ; addps	xmm2, xmm5
 db 00Fh,059h,0D0h ; mulps	xmm2, xmm0
 db 00Fh,010h,0A8h,030h,014h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x1430]
 db 00Fh,058h,0EAh ; addps	xmm5, xmm2
 db 00Fh,059h,0E8h ; mulps	xmm5, xmm0
 db 00Fh,058h,02Ch,024h ; addps	xmm5, xmmword ptr [esp]
 db 00Fh,058h,0C9h ; addps	xmm1, xmm1
 db 00Fh,059h,0CDh ; mulps	xmm1, xmm5
 db 00Fh,010h,098h,0B0h,008h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x8b0]
 db 00Fh,059h,0E3h ; mulps	xmm4, xmm3
 db 00Fh,058h,0E1h ; addps	xmm4, xmm1
 db 00Fh,010h,088h,020h,014h,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1420]
 db 00Fh,059h,0CCh ; mulps	xmm1, xmm4
 db 00Fh,010h,0A8h,020h,009h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x920]
 db 00Fh,028h,0E1h ; movaps	xmm4, xmm1
 db 00Fh,05Fh,0E5h ; maxps	xmm4, xmm5
 db 00Fh,0C2h,0E9h,001h ; cmpltps	xmm5, xmm1
 db 00Fh,057h,0FFh ; xorps	xmm7, xmm7
 db 00Fh,0C2h,0F9h,002h ; cmpleps	xmm7, xmm1
 db 00Fh,057h,0C0h ; xorps	xmm0, xmm0
 db 00Fh,0C2h,0C8h,001h ; cmpltps	xmm1, xmm0
 db 00Fh,054h,0CDh ; andps	xmm1, xmm5
 db 00Fh,05Dh,0E0h ; minps	xmm4, xmm0
 db 00Fh,010h,080h,090h,008h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x890]
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 0F3h,00Fh,05Bh,0D0h ; cvttps2dq	xmm2, xmm0
 db 00Fh,05Bh,0EAh ; cvtdq2ps	xmm5, xmm2
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,0C2h,0E5h,001h ; cmpltps	xmm4, xmm5
 db 066h,00Fh,0FEh,0E2h ; paddd	xmm4, xmm2
 db 00Fh,05Bh,0D4h ; cvtdq2ps	xmm2, xmm4
 db 00Fh,05Ch,0C2h ; subps	xmm0, xmm2
 db 00Fh,010h,090h,000h,009h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x900]
 db 00Fh,059h,0D0h ; mulps	xmm2, xmm0
 db 00Fh,010h,0A8h,0F0h,008h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x8f0]
 db 00Fh,058h,0EAh ; addps	xmm5, xmm2
 db 00Fh,059h,0E8h ; mulps	xmm5, xmm0
 db 00Fh,010h,090h,0E0h,008h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x8e0]
 db 00Fh,058h,0D5h ; addps	xmm2, xmm5
 db 00Fh,059h,0D0h ; mulps	xmm2, xmm0
 db 00Fh,010h,0A8h,0D0h,008h,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x8d0]
 db 00Fh,058h,0EAh ; addps	xmm5, xmm2
 db 00Fh,059h,0E8h ; mulps	xmm5, xmm0
 db 00Fh,010h,090h,0C0h,008h,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x8c0]
 db 00Fh,058h,0D5h ; addps	xmm2, xmm5
 db 00Fh,028h,06Ch,024h,060h ; movaps	xmm5, xmmword ptr [esp + 0x60]
 db 00Fh,059h,0D0h ; mulps	xmm2, xmm0
 db 00Fh,058h,0D3h ; addps	xmm2, xmm3
 db 00Fh,059h,0D0h ; mulps	xmm2, xmm0
 db 00Fh,010h,080h,0A0h,008h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x8a0]
 db 00Fh,058h,0D0h ; addps	xmm2, xmm0
 db 066h,00Fh,0FEh,0A4h,024h,0B0h,000h,000h,000h ; paddd	xmm4, xmmword ptr [esp + 0xb0]
 db 066h,00Fh,072h,0F4h,017h ; pslld	xmm4, 0x17
 db 00Fh,059h,0E2h ; mulps	xmm4, xmm2
 db 00Fh,028h,054h,024h,030h ; movaps	xmm2, xmmword ptr [esp + 0x30]
 db 00Fh,054h,0E1h ; andps	xmm4, xmm1
 db 00Fh,054h,03Ch,024h ; andps	xmm7, xmmword ptr [esp]
 db 00Fh,056h,0FCh ; orps	xmm7, xmm4
 db 00Fh,028h,0A4h,024h,0C0h,000h,000h,000h ; movaps	xmm4, xmmword ptr [esp + 0xc0]
 db 00Fh,028h,00Ch,024h ; movaps	xmm1, xmmword ptr [esp]
 db 00Fh,0C2h,0CEh,002h ; cmpleps	xmm1, xmm6
 db 00Fh,028h,004h,024h ; movaps	xmm0, xmmword ptr [esp]
 db 00Fh,054h,0C1h ; andps	xmm0, xmm1
 db 00Fh,055h,0CFh ; andnps	xmm1, xmm7
 db 00Fh,056h,0C8h ; orps	xmm1, xmm0
 db 00Fh,028h,004h,024h ; movaps	xmm0, xmmword ptr [esp]
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,054h,0CCh ; andps	xmm1, xmm4
 db 08Dh,00Ch,052h ; lea	ecx, [edx + 2*edx]
 db 0C1h,0E1h,004h ; shl	ecx, 0x4
 db 00Fh,010h,084h,008h,020h,011h,000h,000h ; movups	xmm0, xmmword ptr [eax + ecx + 0x1120]
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 00Fh,028h,05Ch,024h,070h ; movaps	xmm3, xmmword ptr [esp + 0x70]
 db 00Fh,058h,0D8h ; addps	xmm3, xmm0
 db 00Fh,029h,05Ch,024h,070h ; movaps	xmmword ptr [esp + 0x70], xmm3
 db 00Fh,011h,098h,0E0h,022h,000h,000h ; movups	xmmword ptr [eax + 0x22e0], xmm3
 db 00Fh,010h,084h,008h,030h,011h,000h,000h ; movups	xmm0, xmmword ptr [eax + ecx + 0x1130]
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 00Fh,028h,09Ch,024h,080h,000h,000h,000h ; movaps	xmm3, xmmword ptr [esp + 0x80]
 db 00Fh,058h,0D8h ; addps	xmm3, xmm0
 db 00Fh,029h,09Ch,024h,080h,000h,000h,000h ; movaps	xmmword ptr [esp + 0x80], xmm3
 db 00Fh,011h,098h,0F0h,022h,000h,000h ; movups	xmmword ptr [eax + 0x22f0], xmm3
 db 00Fh,010h,084h,008h,040h,011h,000h,000h ; movups	xmm0, xmmword ptr [eax + ecx + 0x1140]
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 00Fh,028h,08Ch,024h,090h,000h,000h,000h ; movaps	xmm1, xmmword ptr [esp + 0x90]
 db 00Fh,058h,0C8h ; addps	xmm1, xmm0
 db 00Fh,029h,08Ch,024h,090h,000h,000h,000h ; movaps	xmmword ptr [esp + 0x90], xmm1
 db 00Fh,011h,088h,000h,023h,000h,000h ; movups	xmmword ptr [eax + 0x2300], xmm1
 db 00Fh,028h,05Ch,024h,020h ; movaps	xmm3, xmmword ptr [esp + 0x20]
 db 0E9h,04Ah,0FCh,0FFh,0FFh ; jmp	0x3c5 <_tnk_spec+0x3c5>
 db 00Fh,028h,074h,024h,040h ; movaps	xmm6, xmmword ptr [esp + 0x40]
 db 00Fh,028h,0A4h,024h,090h,000h,000h,000h ; movaps	xmm4, xmmword ptr [esp + 0x90]
 db 00Fh,028h,084h,024h,080h,000h,000h,000h ; movaps	xmm0, xmmword ptr [esp + 0x80]
 db 00Fh,028h,054h,024h,070h ; movaps	xmm2, xmmword ptr [esp + 0x70]
 db 0EBh,009h ; jmp	0x7a0 <_tnk_spec+0x7a0>
 db 00Fh,028h,0E6h ; movaps	xmm4, xmm6
 db 00Fh,028h,0C6h ; movaps	xmm0, xmm6
 db 00Fh,028h,0D6h ; movaps	xmm2, xmm6
 db 0F3h,00Fh,06Fh,088h,050h,008h,000h,000h ; movdqu	xmm1, xmmword ptr [eax + 0x850]
 db 00Fh,010h,098h,060h,008h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x860]
 db 00Fh,059h,0D3h ; mulps	xmm2, xmm3
 db 066h,00Fh,05Bh,0D2h ; cvtps2dq	xmm2, xmm2
 db 00Fh,059h,0C3h ; mulps	xmm0, xmm3
 db 066h,00Fh,05Bh,0C0h ; cvtps2dq	xmm0, xmm0
 db 00Fh,059h,0E3h ; mulps	xmm4, xmm3
 db 066h,00Fh,05Bh,0DCh ; cvtps2dq	xmm3, xmm4
 db 066h,00Fh,06Bh,0D2h ; packssdw	xmm2, xmm2
 db 066h,00Fh,067h,0D2h ; packuswb	xmm2, xmm2
 db 066h,00Fh,06Bh,0C0h ; packssdw	xmm0, xmm0
 db 066h,00Fh,067h,0C0h ; packuswb	xmm0, xmm0
 db 066h,00Fh,060h,0C6h ; punpcklbw	xmm0, xmm6
 db 066h,00Fh,06Bh,0DBh ; packssdw	xmm3, xmm3
 db 066h,00Fh,067h,0DBh ; packuswb	xmm3, xmm3
 db 066h,00Fh,060h,0D3h ; punpcklbw	xmm2, xmm3
 db 066h,00Fh,060h,0D0h ; punpcklbw	xmm2, xmm0
 db 0F3h,00Fh,06Fh,080h,060h,009h,000h,000h ; movdqu	xmm0, xmmword ptr [eax + 0x960]
 db 066h,00Fh,0DFh,0C1h ; pandn	xmm0, xmm1
 db 066h,00Fh,0EBh,0C2h ; por	xmm0, xmm2
 db 0F3h,00Fh,07Fh,080h,050h,008h,000h,000h ; movdqu	xmmword ptr [eax + 0x850], xmm0
 db 08Dh,065h,0F8h ; lea	esp, [ebp - 0x8]
 db 05Eh ; pop	esi
 db 05Bh ; pop	ebx
 db 05Dh ; pop	ebp
 db 0C3h ; ret
_tnk_spec endp
public _tnk_cv
_tnk_cv proc near
 db 055h ; push	ebp
 db 089h,0E5h ; mov	ebp, esp
 db 053h ; push	ebx
 db 057h ; push	edi
 db 056h ; push	esi
 db 083h,0E4h,0F0h ; and	esp, -0x10
 db 081h,0ECh,0C0h,000h,000h,000h ; sub	esp, 0xc0
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 08Bh,090h,0B4h,014h,000h,000h ; mov	edx, dword ptr [eax + 0x14b4]
 db 08Bh,088h,090h,014h,000h,000h ; mov	ecx, dword ptr [eax + 0x1490]
 db 085h,0C9h ; test	ecx, ecx
 db 074h,008h ; je	0x2a <_tnk_cv+0x2a>
 db 08Bh,0B8h,0B0h,014h,000h,000h ; mov	edi, dword ptr [eax + 0x14b0]
 db 0EBh,002h ; jmp	0x2c <_tnk_cv+0x2c>
 db 031h,0FFh ; xor	edi, edi
 db 00Fh,010h,090h,030h,01Fh,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x1f30]
 db 0F3h,00Fh,010h,088h,0B8h,014h,000h,000h ; movss	xmm1, dword ptr [eax + 0x14b8]
 db 00Fh,0C6h,0C9h,000h ; shufps	xmm1, xmm1, 0x0
 db 00Fh,057h,0C0h ; xorps	xmm0, xmm0
 db 00Fh,057h,0F6h ; xorps	xmm6, xmm6
 db 00Fh,057h,0EDh ; xorps	xmm5, xmm5
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,057h,0DBh ; xorps	xmm3, xmm3
 db 0F6h,0C2h,001h ; test	dl, 0x1
 db 074h,03Fh ; je	0x92 <_tnk_cv+0x92>
 db 00Fh,010h,098h,020h,008h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x820]
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,054h,0E2h ; andps	xmm4, xmm2
 db 00Fh,05Bh,0F4h ; cvtdq2ps	xmm6, xmm4
 db 00Fh,059h,0F1h ; mulps	xmm6, xmm1
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 066h,00Fh,072h,0D4h,008h ; psrld	xmm4, 0x8
 db 066h,00Fh,0DBh,0E2h ; pand	xmm4, xmm2
 db 00Fh,05Bh,0ECh ; cvtdq2ps	xmm5, xmm4
 db 00Fh,059h,0E9h ; mulps	xmm5, xmm1
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 066h,00Fh,072h,0D4h,010h ; psrld	xmm4, 0x10
 db 066h,00Fh,0DBh,0E2h ; pand	xmm4, xmm2
 db 00Fh,05Bh,0E4h ; cvtdq2ps	xmm4, xmm4
 db 00Fh,059h,0E1h ; mulps	xmm4, xmm1
 db 0F6h,0C2h,002h ; test	dl, 0x2
 db 075h,01Fh ; jne	0xae <_tnk_cv+0xae>
 db 00Fh,057h,0DBh ; xorps	xmm3, xmm3
 db 0F6h,0C2h,004h ; test	dl, 0x4
 db 075h,02Bh ; jne	0xc2 <_tnk_cv+0xc2>
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,029h,04Ch,024h,070h ; movaps	xmmword ptr [esp + 0x70], xmm1
 db 00Fh,029h,04Ch,024h,060h ; movaps	xmmword ptr [esp + 0x60], xmm1
 db 00Fh,029h,04Ch,024h,050h ; movaps	xmmword ptr [esp + 0x50], xmm1
 db 0E9h,087h,000h,000h,000h ; jmp	0x135 <_tnk_cv+0x135>
 db 066h,00Fh,072h,0D3h,018h ; psrld	xmm3, 0x18
 db 066h,00Fh,0DBh,0DAh ; pand	xmm3, xmm2
 db 00Fh,05Bh,0DBh ; cvtdq2ps	xmm3, xmm3
 db 00Fh,059h,0D9h ; mulps	xmm3, xmm1
 db 0F6h,0C2h,004h ; test	dl, 0x4
 db 074h,0D5h ; je	0x97 <_tnk_cv+0x97>
 db 00Fh,029h,09Ch,024h,080h,000h,000h,000h ; movaps	xmmword ptr [esp + 0x80], xmm3
 db 00Fh,028h,0FCh ; movaps	xmm7, xmm4
 db 00Fh,010h,098h,030h,008h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x830]
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,054h,0E2h ; andps	xmm4, xmm2
 db 00Fh,05Bh,0E4h ; cvtdq2ps	xmm4, xmm4
 db 00Fh,059h,0E1h ; mulps	xmm4, xmm1
 db 00Fh,029h,064h,024h,070h ; movaps	xmmword ptr [esp + 0x70], xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 066h,00Fh,072h,0D4h,008h ; psrld	xmm4, 0x8
 db 066h,00Fh,0DBh,0E2h ; pand	xmm4, xmm2
 db 00Fh,05Bh,0E4h ; cvtdq2ps	xmm4, xmm4
 db 00Fh,059h,0E1h ; mulps	xmm4, xmm1
 db 00Fh,029h,064h,024h,060h ; movaps	xmmword ptr [esp + 0x60], xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 066h,00Fh,072h,0D4h,010h ; psrld	xmm4, 0x10
 db 066h,00Fh,0DBh,0E2h ; pand	xmm4, xmm2
 db 00Fh,05Bh,0E4h ; cvtdq2ps	xmm4, xmm4
 db 00Fh,059h,0E1h ; mulps	xmm4, xmm1
 db 00Fh,029h,064h,024h,050h ; movaps	xmmword ptr [esp + 0x50], xmm4
 db 0F6h,0C2h,008h ; test	dl, 0x8
 db 074h,012h ; je	0x12a <_tnk_cv+0x12a>
 db 066h,00Fh,072h,0D3h,018h ; psrld	xmm3, 0x18
 db 066h,00Fh,0DBh,0DAh ; pand	xmm3, xmm2
 db 00Fh,05Bh,0C3h ; cvtdq2ps	xmm0, xmm3
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,028h,0E7h ; movaps	xmm4, xmm7
 db 00Fh,028h,09Ch,024h,080h,000h,000h,000h ; movaps	xmm3, xmmword ptr [esp + 0x80]
 db 085h,0C9h ; test	ecx, ecx
 db 00Fh,095h,044h,024h,00Bh ; setne	byte ptr [esp + 0xb]
 db 08Bh,0B0h,0DCh,022h,000h,000h ; mov	esi, dword ptr [eax + 0x22dc]
 db 08Bh,088h,010h,023h,000h,000h ; mov	ecx, dword ptr [eax + 0x2310]
 db 089h,0CAh ; mov	edx, ecx
 db 083h,0E2h,002h ; and	edx, 0x2
 db 009h,0F2h ; or	edx, esi
 db 089h,054h,024h,02Ch ; mov	dword ptr [esp + 0x2c], edx
 db 089h,0CBh ; mov	ebx, ecx
 db 083h,0E3h,006h ; and	ebx, 0x6
 db 009h,0F3h ; or	ebx, esi
 db 089h,0C2h ; mov	edx, eax
 db 089h,04Ch,024h,00Ch ; mov	dword ptr [esp + 0xc], ecx
 db 089h,0C8h ; mov	eax, ecx
 db 083h,0E0h,008h ; and	eax, 0x8
 db 009h,0F0h ; or	eax, esi
 db 00Fh,095h,0C1h ; setne	cl
 db 00Fh,029h,0A4h,024h,090h,000h,000h,000h ; movaps	xmmword ptr [esp + 0x90], xmm4
 db 00Fh,029h,0ACh,024h,0A0h,000h,000h,000h ; movaps	xmmword ptr [esp + 0xa0], xmm5
 db 00Fh,029h,074h,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm6
 db 08Bh,082h,0A4h,014h,000h,000h ; mov	eax, dword ptr [edx + 0x14a4]
 db 083h,0F8h,001h ; cmp	eax, 0x1
 db 074h,036h ; je	0x1c0 <_tnk_cv+0x1c0>
 db 00Fh,028h,0D8h ; movaps	xmm3, xmm0
 db 00Fh,028h,064h,024h,050h ; movaps	xmm4, xmmword ptr [esp + 0x50]
 db 00Fh,028h,06Ch,024h,060h ; movaps	xmm5, xmmword ptr [esp + 0x60]
 db 00Fh,028h,074h,024h,070h ; movaps	xmm6, xmmword ptr [esp + 0x70]
 db 083h,0F8h,002h ; cmp	eax, 0x2
 db 074h,01Fh ; je	0x1c0 <_tnk_cv+0x1c0>
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 00Fh,010h,0B0h,000h,01Eh,000h,000h ; movups	xmm6, xmmword ptr [eax + 0x1e00]
 db 00Fh,010h,0A8h,010h,01Eh,000h,000h ; movups	xmm5, xmmword ptr [eax + 0x1e10]
 db 00Fh,010h,0A0h,020h,01Eh,000h,000h ; movups	xmm4, xmmword ptr [eax + 0x1e20]
 db 00Fh,010h,098h,030h,01Eh,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x1e30]
 db 00Fh,029h,09Ch,024h,080h,000h,000h,000h ; movaps	xmmword ptr [esp + 0x80], xmm3
 db 020h,04Ch,024h,00Bh ; and	byte ptr [esp + 0xb], cl
 db 08Bh,044h,024h,02Ch ; mov	eax, dword ptr [esp + 0x2c]
 db 009h,0D8h ; or	eax, ebx
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 00Fh,084h,0B2h,000h,000h,000h ; je	0x28d <_tnk_cv+0x28d>
 db 08Bh,088h,0A8h,014h,000h,000h ; mov	ecx, dword ptr [eax + 0x14a8]
 db 00Fh,028h,094h,024h,090h,000h,000h,000h ; movaps	xmm2, xmmword ptr [esp + 0x90]
 db 00Fh,029h,054h,024h,040h ; movaps	xmmword ptr [esp + 0x40], xmm2
 db 00Fh,028h,0BCh,024h,0A0h,000h,000h,000h ; movaps	xmm7, xmmword ptr [esp + 0xa0]
 db 00Fh,029h,07Ch,024h,030h ; movaps	xmmword ptr [esp + 0x30], xmm7
 db 00Fh,028h,04Ch,024h,010h ; movaps	xmm1, xmmword ptr [esp + 0x10]
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 083h,0F9h,001h ; cmp	ecx, 0x1
 db 074h,03Dh ; je	0x245 <_tnk_cv+0x245>
 db 00Fh,028h,044h,024h,050h ; movaps	xmm0, xmmword ptr [esp + 0x50]
 db 00Fh,029h,044h,024h,040h ; movaps	xmmword ptr [esp + 0x40], xmm0
 db 00Fh,028h,044h,024h,060h ; movaps	xmm0, xmmword ptr [esp + 0x60]
 db 00Fh,029h,044h,024h,030h ; movaps	xmmword ptr [esp + 0x30], xmm0
 db 00Fh,028h,044h,024h,070h ; movaps	xmm0, xmmword ptr [esp + 0x70]
 db 083h,0F9h,002h ; cmp	ecx, 0x2
 db 074h,01Fh ; je	0x245 <_tnk_cv+0x245>
 db 00Fh,010h,080h,040h,01Eh,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x1e40]
 db 00Fh,010h,098h,050h,01Eh,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x1e50]
 db 00Fh,029h,05Ch,024h,030h ; movaps	xmmword ptr [esp + 0x30], xmm3
 db 00Fh,010h,098h,060h,01Eh,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x1e60]
 db 00Fh,029h,05Ch,024h,040h ; movaps	xmmword ptr [esp + 0x40], xmm3
 db 085h,0DBh ; test	ebx, ebx
 db 074h,032h ; je	0x27b <_tnk_cv+0x27b>
 db 08Bh,080h,0ACh,014h,000h,000h ; mov	eax, dword ptr [eax + 0x14ac]
 db 083h,0F8h,001h ; cmp	eax, 0x1
 db 074h,033h ; je	0x287 <_tnk_cv+0x287>
 db 083h,0F8h,002h ; cmp	eax, 0x2
 db 00Fh,085h,091h,000h,000h,000h ; jne	0x2ee <_tnk_cv+0x2ee>
 db 00Fh,028h,054h,024h,050h ; movaps	xmm2, xmmword ptr [esp + 0x50]
 db 00Fh,028h,07Ch,024h,060h ; movaps	xmm7, xmmword ptr [esp + 0x60]
 db 00Fh,028h,05Ch,024h,070h ; movaps	xmm3, xmmword ptr [esp + 0x70]
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 080h,07Ch,024h,00Bh,000h ; cmp	byte ptr [esp + 0xb], 0x0
 db 075h,01Eh ; jne	0x294 <_tnk_cv+0x294>
 db 0E9h,092h,000h,000h,000h ; jmp	0x30d <_tnk_cv+0x30d>
 db 080h,07Ch,024h,00Bh,000h ; cmp	byte ptr [esp + 0xb], 0x0
 db 075h,012h ; jne	0x294 <_tnk_cv+0x294>
 db 0E9h,086h,000h,000h,000h ; jmp	0x30d <_tnk_cv+0x30d>
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 080h,07Ch,024h,00Bh,000h ; cmp	byte ptr [esp + 0xb], 0x0
 db 074h,079h ; je	0x30d <_tnk_cv+0x30d>
 db 083h,0FFh,001h ; cmp	edi, 0x1
 db 074h,074h ; je	0x30d <_tnk_cv+0x30d>
 db 00Fh,028h,04Ch,024h,050h ; movaps	xmm1, xmmword ptr [esp + 0x50]
 db 00Fh,029h,08Ch,024h,090h,000h,000h,000h ; movaps	xmmword ptr [esp + 0x90], xmm1
 db 00Fh,028h,04Ch,024h,060h ; movaps	xmm1, xmmword ptr [esp + 0x60]
 db 00Fh,029h,08Ch,024h,0A0h,000h,000h,000h ; movaps	xmmword ptr [esp + 0xa0], xmm1
 db 00Fh,028h,04Ch,024h,070h ; movaps	xmm1, xmmword ptr [esp + 0x70]
 db 00Fh,029h,04Ch,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm1
 db 083h,0FFh,002h ; cmp	edi, 0x2
 db 074h,04Bh ; je	0x30d <_tnk_cv+0x30d>
 db 00Fh,010h,088h,0C0h,01Eh,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1ec0]
 db 00Fh,029h,04Ch,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm1
 db 00Fh,010h,088h,0D0h,01Eh,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1ed0]
 db 00Fh,029h,08Ch,024h,0A0h,000h,000h,000h ; movaps	xmmword ptr [esp + 0xa0], xmm1
 db 00Fh,010h,088h,0E0h,01Eh,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1ee0]
 db 00Fh,029h,08Ch,024h,090h,000h,000h,000h ; movaps	xmmword ptr [esp + 0x90], xmm1
 db 0EBh,01Fh ; jmp	0x30d <_tnk_cv+0x30d>
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 00Fh,010h,098h,080h,01Eh,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x1e80]
 db 00Fh,010h,0B8h,090h,01Eh,000h,000h ; movups	xmm7, xmmword ptr [eax + 0x1e90]
 db 00Fh,010h,090h,0A0h,01Eh,000h,000h ; movups	xmm2, xmmword ptr [eax + 0x1ea0]
 db 080h,07Ch,024h,00Bh,000h ; cmp	byte ptr [esp + 0xb], 0x0
 db 075h,087h ; jne	0x294 <_tnk_cv+0x294>
 db 08Bh,07Dh,00Ch ; mov	edi, dword ptr [ebp + 0xc]
 db 085h,0DBh ; test	ebx, ebx
 db 00Fh,084h,016h,001h,000h,000h ; je	0x42e <_tnk_cv+0x42e>
 db 00Fh,010h,088h,000h,01Fh,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1f00]
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,058h,0D9h ; addps	xmm3, xmm1
 db 08Bh,088h,0A0h,014h,000h,000h ; mov	ecx, dword ptr [eax + 0x14a0]
 db 085h,0C9h ; test	ecx, ecx
 db 00Fh,0B6h,05Ch,024h,00Bh ; movzx	ebx, byte ptr [esp + 0xb]
 db 089h,0F2h ; mov	edx, esi
 db 074h,03Ch ; je	0x372 <_tnk_cv+0x372>
 db 083h,0F9h,001h ; cmp	ecx, 0x1
 db 00Fh,085h,01Bh,003h,000h,000h ; jne	0x65a <_tnk_cv+0x65a>
 db 031h,0F6h ; xor	esi, esi
 db 0F6h,0C1h,001h ; test	cl, 0x1
 db 00Fh,0B6h,05Ch,024h,00Bh ; movzx	ebx, byte ptr [esp + 0xb]
 db 08Bh,07Dh,00Ch ; mov	edi, dword ptr [ebp + 0xc]
 db 074h,024h ; je	0x372 <_tnk_cv+0x372>
 db 083h,0BCh,0B0h,0C0h,014h,000h,000h,000h ; cmp	dword ptr [eax + 4*esi + 0x14c0], 0x0
 db 074h,01Ah ; je	0x372 <_tnk_cv+0x372>
 db 08Dh,004h,0F6h ; lea	eax, [esi + 8*esi]
 db 0C1h,0E0h,004h ; shl	eax, 0x4
 db 08Bh,04Dh,008h ; mov	ecx, dword ptr [ebp + 0x8]
 db 00Fh,010h,08Ch,001h,030h,015h,000h,000h ; movups	xmm1, xmmword ptr [ecx + eax + 0x1530]
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,058h,0D9h ; addps	xmm3, xmm1
 db 00Fh,011h,01Fh ; movups	xmmword ptr [edi], xmm3
 db 00Fh,010h,088h,010h,01Fh,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1f10]
 db 00Fh,059h,04Ch,024h,030h ; mulps	xmm1, xmmword ptr [esp + 0x30]
 db 00Fh,058h,0F9h ; addps	xmm7, xmm1
 db 08Bh,088h,0A0h,014h,000h,000h ; mov	ecx, dword ptr [eax + 0x14a0]
 db 085h,0C9h ; test	ecx, ecx
 db 074h,03Eh ; je	0x3cc <_tnk_cv+0x3cc>
 db 083h,0F9h,001h ; cmp	ecx, 0x1
 db 00Fh,085h,016h,003h,000h,000h ; jne	0x6ad <_tnk_cv+0x6ad>
 db 031h,0F6h ; xor	esi, esi
 db 0F6h,0C1h,001h ; test	cl, 0x1
 db 00Fh,0B6h,05Ch,024h,00Bh ; movzx	ebx, byte ptr [esp + 0xb]
 db 08Bh,07Dh,00Ch ; mov	edi, dword ptr [ebp + 0xc]
 db 074h,026h ; je	0x3cc <_tnk_cv+0x3cc>
 db 083h,0BCh,0B0h,0C0h,014h,000h,000h,000h ; cmp	dword ptr [eax + 4*esi + 0x14c0], 0x0
 db 074h,01Ch ; je	0x3cc <_tnk_cv+0x3cc>
 db 08Dh,004h,0F6h ; lea	eax, [esi + 8*esi]
 db 0C1h,0E0h,004h ; shl	eax, 0x4
 db 08Bh,04Dh,008h ; mov	ecx, dword ptr [ebp + 0x8]
 db 00Fh,010h,08Ch,001h,040h,015h,000h,000h ; movups	xmm1, xmmword ptr [ecx + eax + 0x1540]
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 00Fh,059h,04Ch,024h,030h ; mulps	xmm1, xmmword ptr [esp + 0x30]
 db 00Fh,058h,0F9h ; addps	xmm7, xmm1
 db 00Fh,011h,07Fh,010h ; movups	xmmword ptr [edi + 0x10], xmm7
 db 00Fh,010h,088h,020h,01Fh,000h,000h ; movups	xmm1, xmmword ptr [eax + 0x1f20]
 db 00Fh,059h,04Ch,024h,040h ; mulps	xmm1, xmmword ptr [esp + 0x40]
 db 00Fh,058h,0D1h ; addps	xmm2, xmm1
 db 08Bh,088h,0A0h,014h,000h,000h ; mov	ecx, dword ptr [eax + 0x14a0]
 db 085h,0C9h ; test	ecx, ecx
 db 074h,054h ; je	0x43d <_tnk_cv+0x43d>
 db 083h,0F9h,001h ; cmp	ecx, 0x1
 db 00Fh,028h,07Ch,024h,010h ; movaps	xmm7, xmmword ptr [esp + 0x10]
 db 00Fh,085h,00Ah,003h,000h,000h ; jne	0x701 <_tnk_cv+0x701>
 db 031h,0F6h ; xor	esi, esi
 db 0F6h,0C1h,001h ; test	cl, 0x1
 db 00Fh,0B6h,05Ch,024h,00Bh ; movzx	ebx, byte ptr [esp + 0xb]
 db 08Bh,07Dh,00Ch ; mov	edi, dword ptr [ebp + 0xc]
 db 074h,03Ch ; je	0x442 <_tnk_cv+0x442>
 db 083h,0BCh,0B0h,0C0h,014h,000h,000h,000h ; cmp	dword ptr [eax + 4*esi + 0x14c0], 0x0
 db 074h,032h ; je	0x442 <_tnk_cv+0x442>
 db 08Dh,004h,0F6h ; lea	eax, [esi + 8*esi]
 db 0C1h,0E0h,004h ; shl	eax, 0x4
 db 08Bh,04Dh,008h ; mov	ecx, dword ptr [ebp + 0x8]
 db 00Fh,010h,08Ch,001h,050h,015h,000h,000h ; movups	xmm1, xmmword ptr [ecx + eax + 0x1550]
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 00Fh,059h,04Ch,024h,040h ; mulps	xmm1, xmmword ptr [esp + 0x40]
 db 00Fh,058h,0D1h ; addps	xmm2, xmm1
 db 0EBh,014h ; jmp	0x442 <_tnk_cv+0x442>
 db 00Fh,010h,01Fh ; movups	xmm3, xmmword ptr [edi]
 db 00Fh,028h,07Ch,024h,010h ; movaps	xmm7, xmmword ptr [esp + 0x10]
 db 00Fh,0B6h,05Ch,024h,00Bh ; movzx	ebx, byte ptr [esp + 0xb]
 db 0EBh,00Bh ; jmp	0x448 <_tnk_cv+0x448>
 db 00Fh,028h,07Ch,024h,010h ; movaps	xmm7, xmmword ptr [esp + 0x10]
 db 00Fh,011h,057h,020h ; movups	xmmword ptr [edi + 0x20], xmm2
 db 089h,0D6h ; mov	esi, edx
 db 00Fh,011h,098h,010h,007h,000h,000h ; movups	xmmword ptr [eax + 0x710], xmm3
 db 00Fh,010h,04Fh,010h ; movups	xmm1, xmmword ptr [edi + 0x10]
 db 00Fh,011h,088h,020h,007h,000h,000h ; movups	xmmword ptr [eax + 0x720], xmm1
 db 00Fh,010h,04Fh,020h ; movups	xmm1, xmmword ptr [edi + 0x20]
 db 00Fh,011h,088h,030h,007h,000h,000h ; movups	xmmword ptr [eax + 0x730], xmm1
 db 083h,0B8h,0A0h,014h,000h,000h,000h ; cmp	dword ptr [eax + 0x14a0], 0x0
 db 00Fh,028h,094h,024h,0A0h,000h,000h,000h ; movaps	xmm2, xmmword ptr [esp + 0xa0]
 db 00Fh,028h,09Ch,024h,090h,000h,000h,000h ; movaps	xmm3, xmmword ptr [esp + 0x90]
 db 00Fh,084h,0B0h,001h,000h,000h ; je	0x632 <_tnk_cv+0x632>
 db 08Bh,04Ch,024h,00Ch ; mov	ecx, dword ptr [esp + 0xc]
 db 083h,0E1h,001h ; and	ecx, 0x1
 db 009h,0F1h ; or	ecx, esi
 db 089h,04Ch,024h,00Ch ; mov	dword ptr [esp + 0xc], ecx
 db 08Dh,0B8h,080h,015h,000h,000h ; lea	edi, [eax + 0x1580]
 db 031h,0D2h ; xor	edx, edx
 db 031h,0F6h ; xor	esi, esi
 db 031h,0C9h ; xor	ecx, ecx
 db 0EBh,017h ; jmp	0x4b4 <_tnk_cv+0x4b4>
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 046h ; inc	esi
 db 041h ; inc	ecx
 db 081h,0C7h,090h,000h,000h,000h ; add	edi, 0x90
 db 03Bh,088h,0A0h,014h,000h,000h ; cmp	ecx, dword ptr [eax + 0x14a0]
 db 00Fh,083h,07Eh,001h,000h,000h ; jae	0x632 <_tnk_cv+0x632>
 db 083h,0BCh,088h,0C0h,014h,000h,000h,000h ; cmp	dword ptr [eax + 4*ecx + 0x14c0], 0x0
 db 089h,054h,024h,010h ; mov	dword ptr [esp + 0x10], edx
 db 00Fh,084h,08Ah,000h,000h,000h ; je	0x550 <_tnk_cv+0x550>
 db 084h,0DBh ; test	bl, bl
 db 074h,036h ; je	0x500 <_tnk_cv+0x500>
 db 08Dh,014h,076h ; lea	edx, [esi + 2*esi]
 db 0C1h,0E2h,004h ; shl	edx, 0x4
 db 00Fh,010h,04Fh,0E0h ; movups	xmm1, xmmword ptr [edi - 0x20]
 db 00Fh,059h,0CFh ; mulps	xmm1, xmm7
 db 00Fh,011h,08Ch,010h,020h,011h,000h,000h ; movups	xmmword ptr [eax + edx + 0x1120], xmm1
 db 00Fh,010h,04Fh,0F0h ; movups	xmm1, xmmword ptr [edi - 0x10]
 db 00Fh,059h,0CAh ; mulps	xmm1, xmm2
 db 00Fh,011h,08Ch,010h,030h,011h,000h,000h ; movups	xmmword ptr [eax + edx + 0x1130], xmm1
 db 00Fh,010h,00Fh ; movups	xmm1, xmmword ptr [edi]
 db 00Fh,059h,0CBh ; mulps	xmm1, xmm3
 db 00Fh,011h,08Ch,010h,040h,011h,000h,000h ; movups	xmmword ptr [eax + edx + 0x1140], xmm1
 db 08Bh,054h,024h,010h ; mov	edx, dword ptr [esp + 0x10]
 db 083h,07Ch,024h,00Ch,000h ; cmp	dword ptr [esp + 0xc], 0x0
 db 074h,099h ; je	0x4a0 <_tnk_cv+0x4a0>
 db 089h,0F2h ; mov	edx, esi
 db 0C1h,0E2h,006h ; shl	edx, 0x6
 db 00Fh,010h,04Fh,080h ; movups	xmm1, xmmword ptr [edi - 0x80]
 db 00Fh,059h,0CEh ; mulps	xmm1, xmm6
 db 00Fh,011h,08Ch,010h,010h,005h,000h,000h ; movups	xmmword ptr [eax + edx + 0x510], xmm1
 db 00Fh,010h,04Fh,090h ; movups	xmm1, xmmword ptr [edi - 0x70]
 db 00Fh,059h,0CDh ; mulps	xmm1, xmm5
 db 00Fh,011h,08Ch,010h,020h,005h,000h,000h ; movups	xmmword ptr [eax + edx + 0x520], xmm1
 db 00Fh,010h,04Fh,0A0h ; movups	xmm1, xmmword ptr [edi - 0x60]
 db 00Fh,059h,0CCh ; mulps	xmm1, xmm4
 db 00Fh,011h,08Ch,010h,030h,005h,000h,000h ; movups	xmmword ptr [eax + edx + 0x530], xmm1
 db 08Bh,054h,024h,010h ; mov	edx, dword ptr [esp + 0x10]
 db 0E9h,05Eh,0FFh,0FFh,0FFh ; jmp	0x4a0 <_tnk_cv+0x4a0>
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 083h,07Ch,024h,02Ch,000h ; cmp	dword ptr [esp + 0x2c], 0x0
 db 074h,050h ; je	0x5a7 <_tnk_cv+0x5a7>
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 08Bh,080h,0D8h,022h,000h,000h ; mov	eax, dword ptr [eax + 0x22d8]
 db 00Fh,0A3h,0D0h ; bt	eax, edx
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 073h,03Fh ; jae	0x5a7 <_tnk_cv+0x5a7>
 db 08Bh,054h,024h,010h ; mov	edx, dword ptr [esp + 0x10]
 db 08Dh,014h,052h ; lea	edx, [edx + 2*edx]
 db 0C1h,0E2h,004h ; shl	edx, 0x4
 db 00Fh,010h,04Fh,0B0h ; movups	xmm1, xmmword ptr [edi - 0x50]
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,011h,08Ch,010h,0A0h,00Fh,000h,000h ; movups	xmmword ptr [eax + edx + 0xfa0], xmm1
 db 00Fh,010h,04Fh,0C0h ; movups	xmm1, xmmword ptr [edi - 0x40]
 db 00Fh,059h,04Ch,024h,030h ; mulps	xmm1, xmmword ptr [esp + 0x30]
 db 00Fh,011h,08Ch,010h,0B0h,00Fh,000h,000h ; movups	xmmword ptr [eax + edx + 0xfb0], xmm1
 db 00Fh,010h,04Fh,0D0h ; movups	xmm1, xmmword ptr [edi - 0x30]
 db 00Fh,059h,04Ch,024h,040h ; mulps	xmm1, xmmword ptr [esp + 0x40]
 db 00Fh,011h,08Ch,010h,0C0h,00Fh,000h,000h ; movups	xmmword ptr [eax + edx + 0xfc0], xmm1
 db 08Bh,054h,024h,010h ; mov	edx, dword ptr [esp + 0x10]
 db 08Dh,004h,052h ; lea	eax, [edx + 2*edx]
 db 084h,0DBh ; test	bl, bl
 db 074h,03Dh ; je	0x5eb <_tnk_cv+0x5eb>
 db 089h,0C2h ; mov	edx, eax
 db 0C1h,0E2h,004h ; shl	edx, 0x4
 db 00Fh,010h,04Fh,0E0h ; movups	xmm1, xmmword ptr [edi - 0x20]
 db 00Fh,059h,0CFh ; mulps	xmm1, xmm7
 db 08Bh,05Dh,008h ; mov	ebx, dword ptr [ebp + 0x8]
 db 00Fh,011h,08Ch,01Ah,0A0h,012h,000h,000h ; movups	xmmword ptr [edx + ebx + 0x12a0], xmm1
 db 00Fh,010h,04Fh,0F0h ; movups	xmm1, xmmword ptr [edi - 0x10]
 db 00Fh,059h,0CAh ; mulps	xmm1, xmm2
 db 00Fh,011h,08Ch,01Ah,0B0h,012h,000h,000h ; movups	xmmword ptr [edx + ebx + 0x12b0], xmm1
 db 00Fh,010h,00Fh ; movups	xmm1, xmmword ptr [edi]
 db 00Fh,059h,0CBh ; mulps	xmm1, xmm3
 db 00Fh,011h,08Ch,01Ah,0C0h,012h,000h,000h ; movups	xmmword ptr [edx + ebx + 0x12c0], xmm1
 db 08Bh,054h,024h,010h ; mov	edx, dword ptr [esp + 0x10]
 db 00Fh,0B6h,05Ch,024h,00Bh ; movzx	ebx, byte ptr [esp + 0xb]
 db 083h,07Ch,024h,00Ch,000h ; cmp	dword ptr [esp + 0xc], 0x0
 db 074h,037h ; je	0x629 <_tnk_cv+0x629>
 db 0C1h,0E0h,004h ; shl	eax, 0x4
 db 00Fh,010h,04Fh,080h ; movups	xmm1, xmmword ptr [edi - 0x80]
 db 00Fh,059h,0CEh ; mulps	xmm1, xmm6
 db 08Bh,055h,008h ; mov	edx, dword ptr [ebp + 0x8]
 db 00Fh,011h,08Ch,002h,020h,00Eh,000h,000h ; movups	xmmword ptr [edx + eax + 0xe20], xmm1
 db 00Fh,010h,04Fh,090h ; movups	xmm1, xmmword ptr [edi - 0x70]
 db 00Fh,059h,0CDh ; mulps	xmm1, xmm5
 db 00Fh,011h,08Ch,002h,030h,00Eh,000h,000h ; movups	xmmword ptr [edx + eax + 0xe30], xmm1
 db 00Fh,010h,04Fh,0A0h ; movups	xmm1, xmmword ptr [edi - 0x60]
 db 00Fh,059h,0CCh ; mulps	xmm1, xmm4
 db 00Fh,011h,08Ch,002h,040h,00Eh,000h,000h ; movups	xmmword ptr [edx + eax + 0xe40], xmm1
 db 08Bh,054h,024h,010h ; mov	edx, dword ptr [esp + 0x10]
 db 042h ; inc	edx
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 0E9h,06Fh,0FEh,0FFh,0FFh ; jmp	0x4a1 <_tnk_cv+0x4a1>
 db 00Fh,028h,084h,024h,080h,000h,000h,000h ; movaps	xmm0, xmmword ptr [esp + 0x80]
 db 00Fh,011h,080h,040h,007h,000h,000h ; movups	xmmword ptr [eax + 0x740], xmm0
 db 08Bh,04Dh,00Ch ; mov	ecx, dword ptr [ebp + 0xc]
 db 00Fh,011h,041h,030h ; movups	xmmword ptr [ecx + 0x30], xmm0
 db 0C7h,080h,0DCh,022h,000h,000h,000h,000h,000h,000h ; mov	dword ptr [eax + 0x22dc], 0x0
 db 08Dh,065h,0F4h ; lea	esp, [ebp - 0xc]
 db 05Eh ; pop	esi
 db 05Fh ; pop	edi
 db 05Bh ; pop	ebx
 db 05Dh ; pop	ebp
 db 0C3h ; ret
 db 089h,0CBh ; mov	ebx, ecx
 db 083h,0E3h,0FEh ; and	ebx, -0x2
 db 08Dh,0B8h,030h,015h,000h,000h ; lea	edi, [eax + 0x1530]
 db 031h,0F6h ; xor	esi, esi
 db 0EBh,018h ; jmp	0x681 <_tnk_cv+0x681>
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 083h,0C6h,002h ; add	esi, 0x2
 db 081h,0C7h,020h,001h,000h,000h ; add	edi, 0x120
 db 039h,0F3h ; cmp	ebx, esi
 db 00Fh,084h,0C0h,0FCh,0FFh,0FFh ; je	0x341 <_tnk_cv+0x341>
 db 083h,0BCh,0B0h,0C0h,014h,000h,000h,000h ; cmp	dword ptr [eax + 4*esi + 0x14c0], 0x0
 db 074h,009h ; je	0x694 <_tnk_cv+0x694>
 db 00Fh,010h,00Fh ; movups	xmm1, xmmword ptr [edi]
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,058h,0D9h ; addps	xmm3, xmm1
 db 083h,0BCh,0B0h,0C4h,014h,000h,000h,000h ; cmp	dword ptr [eax + 4*esi + 0x14c4], 0x0
 db 074h,0D2h ; je	0x670 <_tnk_cv+0x670>
 db 00Fh,010h,08Fh,090h,000h,000h,000h ; movups	xmm1, xmmword ptr [edi + 0x90]
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,058h,0D9h ; addps	xmm3, xmm1
 db 0EBh,0C3h ; jmp	0x670 <_tnk_cv+0x670>
 db 089h,0CBh ; mov	ebx, ecx
 db 083h,0E3h,0FEh ; and	ebx, -0x2
 db 08Dh,0B8h,040h,015h,000h,000h ; lea	edi, [eax + 0x1540]
 db 031h,0F6h ; xor	esi, esi
 db 0EBh,015h ; jmp	0x6d1 <_tnk_cv+0x6d1>
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 083h,0C6h,002h ; add	esi, 0x2
 db 081h,0C7h,020h,001h,000h,000h ; add	edi, 0x120
 db 039h,0F3h ; cmp	ebx, esi
 db 00Fh,084h,0C8h,0FCh,0FFh,0FFh ; je	0x399 <_tnk_cv+0x399>
 db 083h,0BCh,0B0h,0C0h,014h,000h,000h,000h ; cmp	dword ptr [eax + 4*esi + 0x14c0], 0x0
 db 074h,00Bh ; je	0x6e6 <_tnk_cv+0x6e6>
 db 00Fh,010h,00Fh ; movups	xmm1, xmmword ptr [edi]
 db 00Fh,059h,04Ch,024h,030h ; mulps	xmm1, xmmword ptr [esp + 0x30]
 db 00Fh,058h,0F9h ; addps	xmm7, xmm1
 db 083h,0BCh,0B0h,0C4h,014h,000h,000h,000h ; cmp	dword ptr [eax + 4*esi + 0x14c4], 0x0
 db 074h,0D0h ; je	0x6c0 <_tnk_cv+0x6c0>
 db 00Fh,010h,08Fh,090h,000h,000h,000h ; movups	xmm1, xmmword ptr [edi + 0x90]
 db 00Fh,059h,04Ch,024h,030h ; mulps	xmm1, xmmword ptr [esp + 0x30]
 db 00Fh,058h,0F9h ; addps	xmm7, xmm1
 db 0EBh,0BFh ; jmp	0x6c0 <_tnk_cv+0x6c0>
 db 089h,0CBh ; mov	ebx, ecx
 db 083h,0E3h,0FEh ; and	ebx, -0x2
 db 08Dh,0B8h,050h,015h,000h,000h ; lea	edi, [eax + 0x1550]
 db 031h,0F6h ; xor	esi, esi
 db 0EBh,011h ; jmp	0x721 <_tnk_cv+0x721>
 db 083h,0C6h,002h ; add	esi, 0x2
 db 081h,0C7h,020h,001h,000h,000h ; add	edi, 0x120
 db 039h,0F3h ; cmp	ebx, esi
 db 00Fh,084h,0D8h,0FCh,0FFh,0FFh ; je	0x3f9 <_tnk_cv+0x3f9>
 db 083h,0BCh,0B0h,0C0h,014h,000h,000h,000h ; cmp	dword ptr [eax + 4*esi + 0x14c0], 0x0
 db 074h,00Bh ; je	0x736 <_tnk_cv+0x736>
 db 00Fh,010h,00Fh ; movups	xmm1, xmmword ptr [edi]
 db 00Fh,059h,04Ch,024h,040h ; mulps	xmm1, xmmword ptr [esp + 0x40]
 db 00Fh,058h,0D1h ; addps	xmm2, xmm1
 db 083h,0BCh,0B0h,0C4h,014h,000h,000h,000h ; cmp	dword ptr [eax + 4*esi + 0x14c4], 0x0
 db 074h,0D0h ; je	0x710 <_tnk_cv+0x710>
 db 00Fh,010h,08Fh,090h,000h,000h,000h ; movups	xmm1, xmmword ptr [edi + 0x90]
 db 00Fh,059h,04Ch,024h,040h ; mulps	xmm1, xmmword ptr [esp + 0x40]
 db 00Fh,058h,0D1h ; addps	xmm2, xmm1
 db 0EBh,0BFh ; jmp	0x710 <_tnk_cv+0x710>
_tnk_cv endp
public _tnk_bounds
_tnk_bounds proc near
 db 055h ; push	ebp
 db 089h,0E5h ; mov	ebp, esp
 db 053h ; push	ebx
 db 057h ; push	edi
 db 056h ; push	esi
 db 083h,0E4h,0F0h ; and	esp, -0x10
 db 083h,0ECh,030h ; sub	esp, 0x30
 db 08Bh,075h,014h ; mov	esi, dword ptr [ebp + 0x14]
 db 08Bh,055h,010h ; mov	edx, dword ptr [ebp + 0x10]
 db 08Bh,04Dh,00Ch ; mov	ecx, dword ptr [ebp + 0xc]
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 00Fh,010h,080h,040h,009h,000h,000h ; movups	xmm0, xmmword ptr [eax + 0x940]
 db 00Fh,0B7h,03Ah ; movzx	edi, word ptr [edx]
 db 08Bh,03Ch,0B9h ; mov	edi, dword ptr [ecx + 4*edi]
 db 0F2h,00Fh,010h,00Fh ; movsd	xmm1, qword ptr [edi]
 db 0F3h,00Fh,010h,057h,008h ; movss	xmm2, dword ptr [edi + 0x8]
 db 00Fh,016h,0CAh ; movlhps	xmm1, xmm2
 db 083h,0FEh,002h ; cmp	esi, 0x2
 db 072h,015h ; jb	0x4b <_tnk_bounds+0x4b>
 db 08Dh,07Eh,0FFh ; lea	edi, [esi - 0x1]
 db 083h,0FEh,002h ; cmp	esi, 0x2
 db 089h,03Ch,024h ; mov	dword ptr [esp], edi
 db 075h,00Fh ; jne	0x50 <_tnk_bounds+0x50>
 db 0BEh,001h,000h,000h,000h ; mov	esi, 0x1
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 0EBh,05Ch ; jmp	0xa7 <_tnk_bounds+0xa7>
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 0EBh,07Fh ; jmp	0xcf <_tnk_bounds+0xcf>
 db 083h,0E7h,0FEh ; and	edi, -0x2
 db 0F7h,0DFh ; neg	edi
 db 0BEh,001h,000h,000h,000h ; mov	esi, 0x1
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 090h ; nop
 db 090h ; nop
 db 090h ; nop
 db 00Fh,0B7h,01Ch,072h ; movzx	ebx, word ptr [edx + 2*esi]
 db 08Bh,01Ch,099h ; mov	ebx, dword ptr [ecx + 4*ebx]
 db 0F2h,00Fh,010h,01Bh ; movsd	xmm3, qword ptr [ebx]
 db 0F3h,00Fh,010h,063h,008h ; movss	xmm4, dword ptr [ebx + 0x8]
 db 00Fh,016h,0DCh ; movlhps	xmm3, xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,05Dh,0E1h ; minps	xmm4, xmm1
 db 00Fh,05Fh,0DAh ; maxps	xmm3, xmm2
 db 00Fh,0B7h,05Ch,072h,002h ; movzx	ebx, word ptr [edx + 2*esi + 0x2]
 db 08Bh,01Ch,099h ; mov	ebx, dword ptr [ecx + 4*ebx]
 db 0F2h,00Fh,010h,013h ; movsd	xmm2, qword ptr [ebx]
 db 0F3h,00Fh,010h,04Bh,008h ; movss	xmm1, dword ptr [ebx + 0x8]
 db 00Fh,016h,0D1h ; movlhps	xmm2, xmm1
 db 00Fh,028h,0CAh ; movaps	xmm1, xmm2
 db 00Fh,05Dh,0CCh ; minps	xmm1, xmm4
 db 00Fh,05Fh,0D3h ; maxps	xmm2, xmm3
 db 08Dh,01Ch,037h ; lea	ebx, [edi + esi]
 db 083h,0C3h,002h ; add	ebx, 0x2
 db 083h,0C6h,002h ; add	esi, 0x2
 db 083h,0FBh,001h ; cmp	ebx, 0x1
 db 075h,0B9h ; jne	0x60 <_tnk_bounds+0x60>
 db 0F6h,004h,024h,001h ; test	byte ptr [esp], 0x1
 db 074h,022h ; je	0xcf <_tnk_bounds+0xcf>
 db 00Fh,0B7h,014h,072h ; movzx	edx, word ptr [edx + 2*esi]
 db 08Bh,00Ch,091h ; mov	ecx, dword ptr [ecx + 4*edx]
 db 0F2h,00Fh,010h,019h ; movsd	xmm3, qword ptr [ecx]
 db 0F3h,00Fh,010h,061h,008h ; movss	xmm4, dword ptr [ecx + 0x8]
 db 00Fh,016h,0DCh ; movlhps	xmm3, xmm4
 db 00Fh,028h,0E3h ; movaps	xmm4, xmm3
 db 00Fh,05Dh,0E1h ; minps	xmm4, xmm1
 db 00Fh,05Fh,0DAh ; maxps	xmm3, xmm2
 db 00Fh,028h,0D3h ; movaps	xmm2, xmm3
 db 00Fh,028h,0CCh ; movaps	xmm1, xmm4
 db 0F3h,00Fh,010h,098h,050h,002h,000h,000h ; movss	xmm3, dword ptr [eax + 0x250]
 db 0F3h,00Fh,010h,0A0h,040h,002h,000h,000h ; movss	xmm4, dword ptr [eax + 0x240]
 db 00Fh,014h,0E3h ; unpcklps	xmm4, xmm3
 db 0F3h,00Fh,010h,098h,060h,002h,000h,000h ; movss	xmm3, dword ptr [eax + 0x260]
 db 00Fh,016h,0E3h ; movlhps	xmm4, xmm3
 db 00Fh,028h,0F8h ; movaps	xmm7, xmm0
 db 00Fh,028h,0F0h ; movaps	xmm6, xmm0
 db 00Fh,055h,0FCh ; andnps	xmm7, xmm4
 db 0F3h,00Fh,010h,0A8h,080h,001h,000h,000h ; movss	xmm5, dword ptr [eax + 0x180]
 db 0F3h,00Fh,010h,098h,090h,001h,000h,000h ; movss	xmm3, dword ptr [eax + 0x190]
 db 00Fh,014h,0EBh ; unpcklps	xmm5, xmm3
 db 0F3h,00Fh,010h,098h,0A0h,001h,000h,000h ; movss	xmm3, dword ptr [eax + 0x1a0]
 db 00Fh,016h,0EBh ; movlhps	xmm5, xmm3
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 00Fh,0C6h,0D9h,000h ; shufps	xmm3, xmm1, 0x0
 db 00Fh,059h,0DDh ; mulps	xmm3, xmm5
 db 00Fh,028h,0C2h ; movaps	xmm0, xmm2
 db 00Fh,0C6h,0C2h,000h ; shufps	xmm0, xmm2, 0x0
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,028h,0EBh ; movaps	xmm5, xmm3
 db 00Fh,05Dh,0E8h ; minps	xmm5, xmm0
 db 00Fh,058h,0ECh ; addps	xmm5, xmm4
 db 00Fh,029h,02Ch,024h ; movaps	xmmword ptr [esp], xmm5
 db 00Fh,028h,0EEh ; movaps	xmm5, xmm6
 db 00Fh,055h,0F3h ; andnps	xmm6, xmm3
 db 00Fh,05Fh,0D8h ; maxps	xmm3, xmm0
 db 00Fh,058h,0DCh ; addps	xmm3, xmm4
 db 00Fh,028h,0E5h ; movaps	xmm4, xmm5
 db 00Fh,055h,0E0h ; andnps	xmm4, xmm0
 db 00Fh,05Fh,0F4h ; maxps	xmm6, xmm4
 db 00Fh,058h,0F7h ; addps	xmm6, xmm7
 db 00Fh,029h,074h,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm6
 db 0F3h,00Fh,010h,080h,0D0h,001h,000h,000h ; movss	xmm0, dword ptr [eax + 0x1d0]
 db 0F3h,00Fh,010h,0B8h,0C0h,001h,000h,000h ; movss	xmm7, dword ptr [eax + 0x1c0]
 db 00Fh,014h,0F8h ; unpcklps	xmm7, xmm0
 db 0F3h,00Fh,010h,080h,0E0h,001h,000h,000h ; movss	xmm0, dword ptr [eax + 0x1e0]
 db 00Fh,016h,0F8h ; movlhps	xmm7, xmm0
 db 00Fh,028h,0E1h ; movaps	xmm4, xmm1
 db 00Fh,0C6h,0E1h,055h ; shufps	xmm4, xmm1, 0x55
 db 00Fh,059h,0E7h ; mulps	xmm4, xmm7
 db 00Fh,028h,0C2h ; movaps	xmm0, xmm2
 db 00Fh,0C6h,0C2h,055h ; shufps	xmm0, xmm2, 0x55
 db 00Fh,059h,0C7h ; mulps	xmm0, xmm7
 db 00Fh,028h,0FCh ; movaps	xmm7, xmm4
 db 00Fh,05Dh,0F8h ; minps	xmm7, xmm0
 db 00Fh,058h,03Ch,024h ; addps	xmm7, xmmword ptr [esp]
 db 00Fh,028h,0F5h ; movaps	xmm6, xmm5
 db 00Fh,055h,0F4h ; andnps	xmm6, xmm4
 db 00Fh,05Fh,0E0h ; maxps	xmm4, xmm0
 db 00Fh,058h,0E3h ; addps	xmm4, xmm3
 db 00Fh,028h,0DDh ; movaps	xmm3, xmm5
 db 00Fh,055h,0D8h ; andnps	xmm3, xmm0
 db 00Fh,05Fh,0F3h ; maxps	xmm6, xmm3
 db 00Fh,058h,074h,024h,010h ; addps	xmm6, xmmword ptr [esp + 0x10]
 db 0F3h,00Fh,010h,080h,010h,002h,000h,000h ; movss	xmm0, dword ptr [eax + 0x210]
 db 0F3h,00Fh,010h,098h,000h,002h,000h,000h ; movss	xmm3, dword ptr [eax + 0x200]
 db 00Fh,014h,0D8h ; unpcklps	xmm3, xmm0
 db 0F3h,00Fh,010h,080h,020h,002h,000h,000h ; movss	xmm0, dword ptr [eax + 0x220]
 db 00Fh,016h,0D8h ; movlhps	xmm3, xmm0
 db 00Fh,0C6h,0C9h,0AAh ; shufps	xmm1, xmm1, 0xaa
 db 00Fh,059h,0CBh ; mulps	xmm1, xmm3
 db 00Fh,0C6h,0D2h,0AAh ; shufps	xmm2, xmm2, 0xaa
 db 00Fh,059h,0D3h ; mulps	xmm2, xmm3
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 00Fh,05Dh,0DAh ; minps	xmm3, xmm2
 db 00Fh,058h,0DFh ; addps	xmm3, xmm7
 db 00Fh,028h,0C5h ; movaps	xmm0, xmm5
 db 00Fh,055h,0C1h ; andnps	xmm0, xmm1
 db 00Fh,05Fh,0CAh ; maxps	xmm1, xmm2
 db 00Fh,058h,0CCh ; addps	xmm1, xmm4
 db 00Fh,055h,0EAh ; andnps	xmm5, xmm2
 db 00Fh,05Fh,0C5h ; maxps	xmm0, xmm5
 db 00Fh,058h,0C6h ; addps	xmm0, xmm6
 db 00Fh,011h,080h,060h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa60], xmm0
 db 00Fh,011h,098h,070h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa70], xmm3
 db 00Fh,011h,088h,080h,00Ah,000h,000h ; movups	xmmword ptr [eax + 0xa80], xmm1
 db 08Dh,065h,0F4h ; lea	esp, [ebp - 0xc]
 db 05Eh ; pop	esi
 db 05Fh ; pop	edi
 db 05Bh ; pop	ebx
 db 05Dh ; pop	ebp
 db 0C3h ; ret
_tnk_bounds endp
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh
public _tnq_pos
_tnq_pos proc near
 db 053h ; push	ebx
 db 056h ; push	esi
 db 057h ; push	edi
 db 055h ; push	ebp
 db 08Bh,044h,024h,014h ; mov	eax, dword ptr [esp + 0x14]
 db 08Bh,0B0h,040h,029h,000h,000h ; mov	esi, dword ptr [eax + 0x2940]
 db 08Bh,0B8h,044h,029h,000h,000h ; mov	edi, dword ptr [eax + 0x2944]
 db 08Bh,088h,0C0h,028h,000h,000h ; mov	ecx, dword ptr [eax + 0x28c0]
 db 08Bh,090h,0C4h,028h,000h,000h ; mov	edx, dword ptr [eax + 0x28c4]
 db 08Bh,098h,0BCh,028h,000h,000h ; mov	ebx, dword ptr [eax + 0x28bc]
 db 00Fh,0AFh,0D9h ; imul	ebx, ecx
 db 001h,0F3h ; add	ebx, esi
 db 053h ; push	ebx
 db 08Dh,02Ch,049h ; lea	ebp, [ecx + 2*ecx]
 db 08Dh,01Ch,052h ; lea	ebx, [edx + 2*edx]
 db 03Bh,034h,024h ; cmp	esi, dword ptr [esp]
 db 00Fh,084h,0AAh,001h,000h,000h ; je	0x1e5 <_tnq_pos+0x1e5>
 db 00Fh,012h,006h ; movlps	xmm0, qword ptr [esi]   # xmm0 = mem[0,1],xmm0[2,3]
 db 00Fh,012h,00Ch,04Eh ; movlps	xmm1, qword ptr [esi + 2*ecx] # xmm1 = mem[0,1],xmm1[2,3]
 db 00Fh,012h,056h,008h ; movlps	xmm2, qword ptr [esi + 0x8] # xmm2 = mem[0,1],xmm2[2,3]
 db 00Fh,012h,05Ch,04Eh,008h ; movlps	xmm3, qword ptr [esi + 2*ecx + 0x8] # xmm3 = mem[0,1],xmm3[2,3]
 db 00Fh,016h,004h,00Eh ; movhps	xmm0, qword ptr [esi + ecx] # xmm0 = xmm0[0,1],mem[0,1]
 db 00Fh,016h,00Ch,02Eh ; movhps	xmm1, qword ptr [esi + ebp] # xmm1 = xmm1[0,1],mem[0,1]
 db 00Fh,016h,054h,00Eh,008h ; movhps	xmm2, qword ptr [esi + ecx + 0x8] # xmm2 = xmm2[0,1],mem[0,1]
 db 00Fh,016h,05Ch,02Eh,008h ; movhps	xmm3, qword ptr [esi + ebp + 0x8] # xmm3 = xmm3[0,1],mem[0,1]
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,0C6h,0C1h,088h ; shufps	xmm0, xmm1, 0x88        # xmm0 = xmm0[0,2],xmm1[0,2]
 db 00Fh,0C6h,0D3h,088h ; shufps	xmm2, xmm3, 0x88        # xmm2 = xmm2[0,2],xmm3[0,2]
 db 00Fh,0C6h,0E1h,0DDh ; shufps	xmm4, xmm1, 0xdd        # xmm4 = xmm4[1,3],xmm1[1,3]
 db 00Fh,028h,048h,030h ; movaps	xmm1, xmmword ptr [eax + 0x30]
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,028h,058h,070h ; movaps	xmm3, xmmword ptr [eax + 0x70]
 db 00Fh,059h,0DCh ; mulps	xmm3, xmm4
 db 00Fh,028h,0A8h,0B0h,000h,000h,000h ; movaps	xmm5, xmmword ptr [eax + 0xb0]
 db 00Fh,059h,0EAh ; mulps	xmm5, xmm2
 db 00Fh,028h,038h ; movaps	xmm7, xmmword ptr [eax]
 db 00Fh,059h,0F8h ; mulps	xmm7, xmm0
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,028h,070h,040h ; movaps	xmm6, xmmword ptr [eax + 0x40]
 db 00Fh,059h,0F4h ; mulps	xmm6, xmm4
 db 00Fh,028h,098h,080h,000h,000h,000h ; movaps	xmm3, xmmword ptr [eax + 0x80]
 db 00Fh,059h,0DAh ; mulps	xmm3, xmm2
 db 00Fh,058h,0CDh ; addps	xmm1, xmm5
 db 00Fh,058h,0FEh ; addps	xmm7, xmm6
 db 00Fh,028h,070h,010h ; movaps	xmm6, xmmword ptr [eax + 0x10]
 db 00Fh,059h,0F0h ; mulps	xmm6, xmm0
 db 00Fh,058h,088h,0F0h,000h,000h,000h ; addps	xmm1, xmmword ptr [eax + 0xf0]
 db 00Fh,028h,068h,050h ; movaps	xmm5, xmmword ptr [eax + 0x50]
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,058h,0FBh ; addps	xmm7, xmm3
 db 00Fh,028h,098h,090h,000h,000h,000h ; movaps	xmm3, xmmword ptr [eax + 0x90]
 db 00Fh,059h,0DAh ; mulps	xmm3, xmm2
 db 00Fh,059h,040h,020h ; mulps	xmm0, xmmword ptr [eax + 0x20]
 db 00Fh,058h,0F5h ; addps	xmm6, xmm5
 db 00Fh,053h,0E9h ; rcpps	xmm5, xmm1
 db 00Fh,059h,060h,060h ; mulps	xmm4, xmmword ptr [eax + 0x60]
 db 00Fh,058h,0F3h ; addps	xmm6, xmm3
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 00Fh,059h,0DDh ; mulps	xmm3, xmm5
 db 00Fh,059h,090h,0A0h,000h,000h,000h ; mulps	xmm2, xmmword ptr [eax + 0xa0]
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,028h,0A0h,050h,024h,000h,000h ; movaps	xmm4, xmmword ptr [eax + 0x2450]
 db 00Fh,05Ch,0E3h ; subps	xmm4, xmm3
 db 00Fh,058h,0B8h,0C0h,000h,000h,000h ; addps	xmm7, xmmword ptr [eax + 0xc0]
 db 00Fh,058h,0C2h ; addps	xmm0, xmm2
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,058h,0B0h,0D0h,000h,000h,000h ; addps	xmm6, xmmword ptr [eax + 0xd0]
 db 00Fh,058h,080h,0E0h,000h,000h,000h ; addps	xmm0, xmmword ptr [eax + 0xe0]
 db 00Fh,059h,0CDh ; mulps	xmm1, xmm5
 db 00Fh,028h,098h,070h,008h,000h,000h ; movaps	xmm3, xmmword ptr [eax + 0x870]
 db 08Dh,034h,08Eh ; lea	esi, [esi + 4*ecx]
 db 00Fh,05Ch,0D9h ; subps	xmm3, xmm1
 db 00Fh,018h,00Ch,08Eh ; prefetcht0	byte ptr [esi + 4*ecx]
 db 00Fh,018h,04Ch,08Eh,040h ; prefetcht0	byte ptr [esi + 4*ecx + 0x40]
 db 00Fh,059h,0DDh ; mulps	xmm3, xmm5
 db 00Fh,058h,0EBh ; addps	xmm5, xmm3
 db 00Fh,059h,0FDh ; mulps	xmm7, xmm5
 db 00Fh,059h,0F5h ; mulps	xmm6, xmm5
 db 00Fh,059h,0C5h ; mulps	xmm0, xmm5
 db 00Fh,058h,0B8h,000h,001h,000h,000h ; addps	xmm7, xmmword ptr [eax + 0x100]
 db 00Fh,058h,0B0h,020h,001h,000h,000h ; addps	xmm6, xmmword ptr [eax + 0x120]
 db 00Fh,058h,080h,040h,001h,000h,000h ; addps	xmm0, xmmword ptr [eax + 0x140]
 db 00Fh,028h,0CFh ; movaps	xmm1, xmm7
 db 00Fh,028h,0D0h ; movaps	xmm2, xmm0
 db 00Fh,014h,0CEh ; unpcklps	xmm1, xmm6              # xmm1 = xmm1[0],xmm6[0],xmm1[1],xmm6[1]
 db 00Fh,014h,0D5h ; unpcklps	xmm2, xmm5              # xmm2 = xmm2[0],xmm5[0],xmm2[1],xmm5[1]
 db 00Fh,015h,0FEh ; unpckhps	xmm7, xmm6              # xmm7 = xmm7[2],xmm6[2],xmm7[3],xmm6[3]
 db 00Fh,015h,0C5h ; unpckhps	xmm0, xmm5              # xmm0 = xmm0[2],xmm5[2],xmm0[3],xmm5[3]
 db 00Fh,013h,00Fh ; movlps	qword ptr [edi], xmm1
 db 00Fh,013h,057h,008h ; movlps	qword ptr [edi + 0x8], xmm2
 db 00Fh,017h,00Ch,017h ; movhps	qword ptr [edi + edx], xmm1
 db 00Fh,017h,054h,017h,008h ; movhps	qword ptr [edi + edx + 0x8], xmm2
 db 00Fh,013h,03Ch,057h ; movlps	qword ptr [edi + 2*edx], xmm7
 db 00Fh,013h,044h,057h,008h ; movlps	qword ptr [edi + 2*edx + 0x8], xmm0
 db 00Fh,017h,03Ch,01Fh ; movhps	qword ptr [edi + ebx], xmm7
 db 00Fh,017h,044h,01Fh,008h ; movhps	qword ptr [edi + ebx + 0x8], xmm0
 db 066h,00Fh,0FAh,088h,080h,024h,000h,000h ; psubd	xmm1, xmmword ptr [eax + 0x2480]
 db 066h,00Fh,0FAh,0B8h,080h,024h,000h,000h ; psubd	xmm7, xmmword ptr [eax + 0x2480]
 db 066h,00Fh,0FAh,090h,090h,024h,000h,000h ; psubd	xmm2, xmmword ptr [eax + 0x2490]
 db 066h,00Fh,0FAh,080h,090h,024h,000h,000h ; psubd	xmm0, xmmword ptr [eax + 0x2490]
 db 066h,00Fh,066h,088h,0A0h,024h,000h,000h ; pcmpgtd	xmm1, xmmword ptr [eax + 0x24a0]
 db 066h,00Fh,066h,0B8h,0A0h,024h,000h,000h ; pcmpgtd	xmm7, xmmword ptr [eax + 0x24a0]
 db 066h,00Fh,066h,090h,0B0h,024h,000h,000h ; pcmpgtd	xmm2, xmmword ptr [eax + 0x24b0]
 db 066h,00Fh,066h,080h,0B0h,024h,000h,000h ; pcmpgtd	xmm0, xmmword ptr [eax + 0x24b0]
 db 066h,00Fh,0EBh,0CFh ; por	xmm1, xmm7
 db 066h,00Fh,0EBh,0D0h ; por	xmm2, xmm0
 db 066h,00Fh,06Bh,0CAh ; packssdw	xmm1, xmm2
 db 066h,00Fh,0EBh,088h,0C0h,024h,000h,000h ; por	xmm1, xmmword ptr [eax + 0x24c0]
 db 066h,00Fh,07Fh,088h,0C0h,024h,000h,000h ; movdqa	xmmword ptr [eax + 0x24c0], xmm1
 db 08Dh,03Ch,097h ; lea	edi, [edi + 4*edx]
 db 03Bh,034h,024h ; cmp	esi, dword ptr [esp]
 db 00Fh,085h,056h,0FEh,0FFh,0FFh ; jne	0x3b <_tnq_pos+0x3b>
 db 05Bh ; pop	ebx
 db 05Dh ; pop	ebp
 db 05Fh ; pop	edi
 db 05Eh ; pop	esi
 db 05Bh ; pop	ebx
 db 0C3h ; ret
_tnq_pos endp
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh
public _tnq_gpos
_tnq_gpos proc near
 db 055h ; push	ebp
 db 08Dh,02Ch,024h ; lea	ebp, [esp]
 db 057h ; push	edi
 db 056h ; push	esi
 db 083h,0E4h,0F0h ; and	esp, -0x10
 db 08Dh,0A4h,024h,050h,0FFh,0FFh,0FFh ; lea	esp, [esp - 0xb0]
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 083h,0B8h,0ACh,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7ac], 0x0
 db 00Fh,084h,0CCh,003h,000h,000h ; je	0x3ec <_tnq_gpos+0x3ec>
 db 00Fh,028h,088h,070h,008h,000h,000h ; movaps	xmm1, xmmword ptr [eax + 0x870]
 db 00Fh,028h,080h,060h,001h,000h,000h ; movaps	xmm0, xmmword ptr [eax + 0x160]
 db 00Fh,028h,090h,050h,024h,000h,000h ; movaps	xmm2, xmmword ptr [eax + 0x2450]
 db 031h,0C9h ; xor	ecx, ecx
 db 00Fh,029h,08Ch,024h,090h,000h,000h,000h ; movaps	xmmword ptr [esp + 0x90], xmm1
 db 00Fh,028h,088h,030h,009h,000h,000h ; movaps	xmm1, xmmword ptr [eax + 0x930]
 db 00Fh,029h,044h,024h,060h ; movaps	xmmword ptr [esp + 0x60], xmm0
 db 00Fh,029h,094h,024h,080h,000h,000h,000h ; movaps	xmmword ptr [esp + 0x80], xmm2
 db 00Fh,029h,04Ch,024h,070h ; movaps	xmmword ptr [esp + 0x70], xmm1
 db 0E9h,08Ch,000h,000h,000h ; jmp	0xe9 <_tnq_gpos+0xe9>
 db 00Fh,028h,004h,024h ; movaps	xmm0, xmmword ptr [esp]
 db 00Fh,028h,054h,024h,010h ; movaps	xmm2, xmmword ptr [esp + 0x10]
 db 08Bh,0B0h,0A8h,007h,000h,000h ; mov	esi, dword ptr [eax + 0x7a8]
 db 00Fh,028h,05Ch,024h,050h ; movaps	xmm3, xmmword ptr [esp + 0x50]
 db 08Bh,0B8h,004h,00Ah,000h,000h ; mov	edi, dword ptr [eax + 0xa04]
 db 083h,0C1h,004h ; add	ecx, 0x4
 db 00Fh,059h,0D0h ; mulps	xmm2, xmm0
 db 00Fh,059h,0F0h ; mulps	xmm6, xmm0
 db 00Fh,059h,0F8h ; mulps	xmm7, xmm0
 db 00Fh,0AFh,0D6h ; imul	edx, esi
 db 00Fh,058h,090h,040h,001h,000h,000h ; addps	xmm2, xmmword ptr [eax + 0x140]
 db 00Fh,058h,0B0h,000h,001h,000h,000h ; addps	xmm6, xmmword ptr [eax + 0x100]
 db 00Fh,058h,0B8h,020h,001h,000h,000h ; addps	xmm7, xmmword ptr [eax + 0x120]
 db 00Fh,05Dh,090h,020h,024h,000h,000h ; minps	xmm2, xmmword ptr [eax + 0x2420]
 db 00Fh,028h,0C6h ; movaps	xmm0, xmm6
 db 00Fh,015h,0F7h ; unpckhps	xmm6, xmm7
 db 00Fh,014h,0C7h ; unpcklps	xmm0, xmm7
 db 00Fh,013h,004h,017h ; movlps	qword ptr [edi + edx], xmm0
 db 00Fh,028h,0CAh ; movaps	xmm1, xmm2
 db 00Fh,015h,0D3h ; unpckhps	xmm2, xmm3
 db 00Fh,014h,0CBh ; unpcklps	xmm1, xmm3
 db 00Fh,013h,04Ch,017h,008h ; movlps	qword ptr [edi + edx + 0x8], xmm1
 db 001h,0FAh ; add	edx, edi
 db 08Dh,03Ch,076h ; lea	edi, [esi + 2*esi]
 db 00Fh,017h,004h,016h ; movhps	qword ptr [esi + edx], xmm0
 db 00Fh,017h,04Ch,016h,008h ; movhps	qword ptr [esi + edx + 0x8], xmm1
 db 00Fh,013h,034h,072h ; movlps	qword ptr [edx + 2*esi], xmm6
 db 00Fh,013h,054h,072h,008h ; movlps	qword ptr [edx + 2*esi + 0x8], xmm2
 db 00Fh,017h,034h,03Ah ; movhps	qword ptr [edx + edi], xmm6
 db 00Fh,017h,054h,03Ah,008h ; movhps	qword ptr [edx + edi + 0x8], xmm2
 db 03Bh,088h,0ACh,007h,000h,000h ; cmp	ecx, dword ptr [eax + 0x7ac]
 db 00Fh,083h,003h,003h,000h,000h ; jae	0x3ec <_tnq_gpos+0x3ec>
 db 08Bh,090h,0E0h,009h,000h,000h ; mov	edx, dword ptr [eax + 0x9e0]
 db 08Bh,0B0h,0E4h,009h,000h,000h ; mov	esi, dword ptr [eax + 0x9e4]
 db 00Fh,028h,040h,030h ; movaps	xmm0, xmmword ptr [eax + 0x30]
 db 00Fh,028h,050h,070h ; movaps	xmm2, xmmword ptr [eax + 0x70]
 db 00Fh,028h,098h,0B0h,000h,000h,000h ; movaps	xmm3, xmmword ptr [eax + 0xb0]
 db 00Fh,028h,078h,040h ; movaps	xmm7, xmmword ptr [eax + 0x40]
 db 08Bh,0B8h,0ECh,009h,000h,000h ; mov	edi, dword ptr [eax + 0x9ec]
 db 00Fh,028h,024h,08Eh ; movaps	xmm4, xmmword ptr [esi + 4*ecx]
 db 00Fh,028h,034h,08Ah ; movaps	xmm6, xmmword ptr [edx + 4*ecx]
 db 08Bh,090h,0E8h,009h,000h,000h ; mov	edx, dword ptr [eax + 0x9e8]
 db 08Bh,0B0h,0B0h,007h,000h,000h ; mov	esi, dword ptr [eax + 0x7b0]
 db 00Fh,059h,0C6h ; mulps	xmm0, xmm6
 db 00Fh,059h,0D4h ; mulps	xmm2, xmm4
 db 00Fh,028h,00Ch,08Ah ; movaps	xmm1, xmmword ptr [edx + 4*ecx]
 db 00Fh,059h,0FCh ; mulps	xmm7, xmm4
 db 08Dh,014h,031h ; lea	edx, [ecx + esi]
 db 00Fh,059h,0D9h ; mulps	xmm3, xmm1
 db 00Fh,029h,04Ch,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm1
 db 00Fh,028h,0CCh ; movaps	xmm1, xmm4
 db 00Fh,059h,048h,060h ; mulps	xmm1, xmmword ptr [eax + 0x60]
 db 00Fh,058h,0D0h ; addps	xmm2, xmm0
 db 00Fh,028h,000h ; movaps	xmm0, xmmword ptr [eax]
 db 00Fh,059h,0C6h ; mulps	xmm0, xmm6
 db 00Fh,058h,0DAh ; addps	xmm3, xmm2
 db 00Fh,028h,0D6h ; movaps	xmm2, xmm6
 db 00Fh,028h,070h,010h ; movaps	xmm6, xmmword ptr [eax + 0x10]
 db 00Fh,029h,044h,024h,020h ; movaps	xmmword ptr [esp + 0x20], xmm0
 db 00Fh,028h,040h,050h ; movaps	xmm0, xmmword ptr [eax + 0x50]
 db 00Fh,059h,0F2h ; mulps	xmm6, xmm2
 db 00Fh,059h,050h,020h ; mulps	xmm2, xmmword ptr [eax + 0x20]
 db 00Fh,058h,098h,0F0h,000h,000h,000h ; addps	xmm3, xmmword ptr [eax + 0xf0]
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,028h,064h,024h,010h ; movaps	xmm4, xmmword ptr [esp + 0x10]
 db 00Fh,058h,07Ch,024h,020h ; addps	xmm7, xmmword ptr [esp + 0x20]
 db 00Fh,029h,054h,024h,030h ; movaps	xmmword ptr [esp + 0x30], xmm2
 db 00Fh,028h,090h,090h,000h,000h,000h ; movaps	xmm2, xmmword ptr [eax + 0x90]
 db 00Fh,053h,0EBh ; rcpps	xmm5, xmm3
 db 00Fh,029h,05Ch,024h,040h ; movaps	xmmword ptr [esp + 0x40], xmm3
 db 00Fh,058h,0C6h ; addps	xmm0, xmm6
 db 00Fh,059h,0D4h ; mulps	xmm2, xmm4
 db 00Fh,058h,04Ch,024h,030h ; addps	xmm1, xmmword ptr [esp + 0x30]
 db 00Fh,028h,074h,024h,040h ; movaps	xmm6, xmmword ptr [esp + 0x40]
 db 00Fh,058h,0D0h ; addps	xmm2, xmm0
 db 00Fh,028h,044h,024h,040h ; movaps	xmm0, xmmword ptr [esp + 0x40]
 db 00Fh,058h,090h,0D0h,000h,000h,000h ; addps	xmm2, xmmword ptr [eax + 0xd0]
 db 00Fh,029h,02Ch,024h ; movaps	xmmword ptr [esp], xmm5
 db 00Fh,059h,0EBh ; mulps	xmm5, xmm3
 db 00Fh,028h,09Ch,024h,080h,000h,000h,000h ; movaps	xmm3, xmmword ptr [esp + 0x80]
 db 00Fh,029h,094h,024h,0A0h,000h,000h,000h ; movaps	xmmword ptr [esp + 0xa0], xmm2
 db 00Fh,05Ch,0DDh ; subps	xmm3, xmm5
 db 00Fh,028h,0A8h,080h,000h,000h,000h ; movaps	xmm5, xmmword ptr [eax + 0x80]
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,059h,0A0h,0A0h,000h,000h,000h ; mulps	xmm4, xmmword ptr [eax + 0xa0]
 db 00Fh,059h,01Ch,024h ; mulps	xmm3, xmmword ptr [esp]
 db 00Fh,058h,0EFh ; addps	xmm5, xmm7
 db 00Fh,058h,0E1h ; addps	xmm4, xmm1
 db 00Fh,028h,0BCh,024h,090h,000h,000h,000h ; movaps	xmm7, xmmword ptr [esp + 0x90]
 db 00Fh,059h,0F3h ; mulps	xmm6, xmm3
 db 00Fh,058h,0A8h,0C0h,000h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0xc0]
 db 00Fh,058h,0A0h,0E0h,000h,000h,000h ; addps	xmm4, xmmword ptr [eax + 0xe0]
 db 00Fh,05Ch,0FEh ; subps	xmm7, xmm6
 db 00Fh,029h,02Ch,097h ; movaps	xmmword ptr [edi + 4*edx], xmm5
 db 00Fh,029h,06Ch,024h,030h ; movaps	xmmword ptr [esp + 0x30], xmm5
 db 00Fh,029h,064h,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm4
 db 08Bh,0B8h,0F0h,009h,000h,000h ; mov	edi, dword ptr [eax + 0x9f0]
 db 00Fh,029h,014h,097h ; movaps	xmmword ptr [edi + 4*edx], xmm2
 db 00Fh,028h,054h,024h,060h ; movaps	xmm2, xmmword ptr [esp + 0x60]
 db 08Bh,0B8h,0F4h,009h,000h,000h ; mov	edi, dword ptr [eax + 0x9f4]
 db 00Fh,028h,0CAh ; movaps	xmm1, xmm2
 db 00Fh,028h,0F2h ; movaps	xmm6, xmm2
 db 00Fh,029h,024h,097h ; movaps	xmmword ptr [edi + 4*edx], xmm4
 db 00Fh,059h,0FBh ; mulps	xmm7, xmm3
 db 00Fh,0C2h,0C8h,001h ; cmpltps	xmm1, xmm0
 db 00Fh,028h,0E6h ; movaps	xmm4, xmm6
 db 08Bh,0B8h,0F8h,009h,000h,000h ; mov	edi, dword ptr [eax + 0x9f8]
 db 00Fh,029h,004h,097h ; movaps	xmmword ptr [edi + 4*edx], xmm0
 db 00Fh,028h,0A8h,0F0h,023h,000h,000h ; movaps	xmm5, xmmword ptr [eax + 0x23f0]
 db 00Fh,028h,090h,000h,024h,000h,000h ; movaps	xmm2, xmmword ptr [eax + 0x2400]
 db 003h,0B0h,000h,00Ah,000h,000h ; add	esi, dword ptr [eax + 0xa00]
 db 00Fh,059h,0E8h ; mulps	xmm5, xmm0
 db 00Fh,059h,0D0h ; mulps	xmm2, xmm0
 db 00Fh,058h,0FBh ; addps	xmm7, xmm3
 db 00Fh,028h,098h,010h,024h,000h,000h ; movaps	xmm3, xmmword ptr [eax + 0x2410]
 db 00Fh,059h,0D8h ; mulps	xmm3, xmm0
 db 00Fh,029h,00Ch,024h ; movaps	xmmword ptr [esp], xmm1
 db 00Fh,028h,04Ch,024h,010h ; movaps	xmm1, xmmword ptr [esp + 0x10]
 db 00Fh,0C2h,0CEh,001h ; cmpltps	xmm1, xmm6
 db 00Fh,0C2h,05Ch,024h,010h,001h ; cmpltps	xmm3, xmmword ptr [esp + 0x10]
 db 00Fh,05Ch,0E5h ; subps	xmm4, xmm5
 db 00Fh,029h,06Ch,024h,020h ; movaps	xmmword ptr [esp + 0x20], xmm5
 db 00Fh,028h,0E8h ; movaps	xmm5, xmm0
 db 00Fh,028h,044h,024h,030h ; movaps	xmm0, xmmword ptr [esp + 0x30]
 db 00Fh,0C2h,0EEh,002h ; cmpleps	xmm5, xmm6
 db 00Fh,028h,0F0h ; movaps	xmm6, xmm0
 db 00Fh,054h,098h,080h,009h,000h,000h ; andps	xmm3, xmmword ptr [eax + 0x980]
 db 00Fh,0C2h,0F4h,001h ; cmpltps	xmm6, xmm4
 db 00Fh,028h,024h,024h ; movaps	xmm4, xmmword ptr [esp]
 db 00Fh,054h,0E7h ; andps	xmm4, xmm7
 db 00Fh,029h,024h,024h ; movaps	xmmword ptr [esp], xmm4
 db 00Fh,028h,064h,024h,060h ; movaps	xmm4, xmmword ptr [esp + 0x60]
 db 00Fh,056h,0E9h ; orps	xmm5, xmm1
 db 00Fh,028h,04Ch,024h,020h ; movaps	xmm1, xmmword ptr [esp + 0x20]
 db 00Fh,05Ch,0E2h ; subps	xmm4, xmm2
 db 00Fh,0C2h,0C8h,001h ; cmpltps	xmm1, xmm0
 db 00Fh,028h,084h,024h,0A0h,000h,000h,000h ; movaps	xmm0, xmmword ptr [esp + 0xa0]
 db 00Fh,054h,0A8h,070h,009h,000h,000h ; andps	xmm5, xmmword ptr [eax + 0x970]
 db 00Fh,054h,0B0h,090h,009h,000h,000h ; andps	xmm6, xmmword ptr [eax + 0x990]
 db 00Fh,028h,0F8h ; movaps	xmm7, xmm0
 db 00Fh,056h,0DDh ; orps	xmm3, xmm5
 db 00Fh,054h,088h,0A0h,009h,000h,000h ; andps	xmm1, xmmword ptr [eax + 0x9a0]
 db 00Fh,056h,0CBh ; orps	xmm1, xmm3
 db 066h,00Fh,06Fh,05Ch,024h,030h ; movdqa	xmm3, xmmword ptr [esp + 0x30]
 db 00Fh,0C2h,0FCh,001h ; cmpltps	xmm7, xmm4
 db 00Fh,028h,064h,024h,070h ; movaps	xmm4, xmmword ptr [esp + 0x70]
 db 00Fh,056h,0CEh ; orps	xmm1, xmm6
 db 00Fh,028h,0ECh ; movaps	xmm5, xmm4
 db 00Fh,029h,04Ch,024h,020h ; movaps	xmmword ptr [esp + 0x20], xmm1
 db 00Fh,028h,088h,070h,024h,000h,000h ; movaps	xmm1, xmmword ptr [eax + 0x2470]
 db 00Fh,028h,0F4h ; movaps	xmm6, xmm4
 db 00Fh,054h,0E8h ; andps	xmm5, xmm0
 db 00Fh,059h,00Ch,024h ; mulps	xmm1, xmmword ptr [esp]
 db 00Fh,054h,074h,024h,040h ; andps	xmm6, xmmword ptr [esp + 0x40]
 db 066h,00Fh,076h,0ECh ; pcmpeqd	xmm5, xmm4
 db 066h,00Fh,076h,0F4h ; pcmpeqd	xmm6, xmm4
 db 00Fh,029h,07Ch,024h,050h ; movaps	xmmword ptr [esp + 0x50], xmm7
 db 00Fh,028h,0F8h ; movaps	xmm7, xmm0
 db 066h,00Fh,06Fh,0C4h ; movdqa	xmm0, xmm4
 db 066h,00Fh,0DBh,044h,024h,010h ; pand	xmm0, xmmword ptr [esp + 0x10]
 db 00Fh,0C2h,0D7h,001h ; cmpltps	xmm2, xmm7
 db 066h,00Fh,076h,0C4h ; pcmpeqd	xmm0, xmm4
 db 066h,00Fh,0EBh,0C5h ; por	xmm0, xmm5
 db 066h,00Fh,06Fh,0ECh ; movdqa	xmm5, xmm4
 db 066h,00Fh,0DBh,0EBh ; pand	xmm5, xmm3
 db 00Fh,054h,090h,0C0h,009h,000h,000h ; andps	xmm2, xmmword ptr [eax + 0x9c0]
 db 066h,00Fh,076h,0ECh ; pcmpeqd	xmm5, xmm4
 db 066h,00Fh,0EBh,0F5h ; por	xmm6, xmm5
 db 00Fh,028h,0ECh ; movaps	xmm5, xmm4
 db 00Fh,054h,0E9h ; andps	xmm5, xmm1
 db 066h,00Fh,0EBh,0F0h ; por	xmm6, xmm0
 db 00Fh,028h,044h,024h,050h ; movaps	xmm0, xmmword ptr [esp + 0x50]
 db 00Fh,029h,04Ch,024h,050h ; movaps	xmmword ptr [esp + 0x50], xmm1
 db 066h,00Fh,076h,0ECh ; pcmpeqd	xmm5, xmm4
 db 00Fh,054h,080h,0B0h,009h,000h,000h ; andps	xmm0, xmmword ptr [eax + 0x9b0]
 db 00Fh,056h,0D0h ; orps	xmm2, xmm0
 db 066h,00Fh,0EBh,0EEh ; por	xmm5, xmm6
 db 066h,00Fh,06Fh,0F3h ; movdqa	xmm6, xmm3
 db 066h,00Fh,06Fh,0C5h ; movdqa	xmm0, xmm5
 db 00Fh,056h,054h,024h,020h ; orps	xmm2, xmmword ptr [esp + 0x20]
 db 066h,00Fh,0DBh,0A8h,0D0h,009h,000h,000h ; pand	xmm5, xmmword ptr [eax + 0x9d0]
 db 066h,00Fh,0DFh,0C2h ; pandn	xmm0, xmm2
 db 066h,00Fh,0EBh,0E8h ; por	xmm5, xmm0
 db 066h,00Fh,06Bh,0EDh ; packssdw	xmm5, xmm5
 db 066h,00Fh,067h,0EDh ; packuswb	xmm5, xmm5
 db 066h,00Fh,07Eh,02Ch,031h ; movd	dword ptr [ecx + esi], xmm5
 db 083h,0B8h,014h,023h,000h,000h,000h ; cmp	dword ptr [eax + 0x2314], 0x0
 db 00Fh,084h,0B5h,0FCh,0FFh,0FFh ; je	0x5d <_tnq_gpos+0x5d>
 db 00Fh,028h,080h,030h,024h,000h,000h ; movaps	xmm0, xmmword ptr [eax + 0x2430]
 db 00Fh,028h,064h,024h,040h ; movaps	xmm4, xmmword ptr [esp + 0x40]
 db 00Fh,028h,088h,040h,009h,000h,000h ; movaps	xmm1, xmmword ptr [eax + 0x940]
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,059h,0A0h,040h,024h,000h,000h ; mulps	xmm4, xmmword ptr [eax + 0x2440]
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 00Fh,055h,0CFh ; andnps	xmm1, xmm7
 db 00Fh,055h,0D6h ; andnps	xmm2, xmm6
 db 00Fh,0C2h,0C2h,001h ; cmpltps	xmm0, xmm2
 db 00Fh,0C2h,0E1h,001h ; cmpltps	xmm4, xmm1
 db 00Fh,056h,0E0h ; orps	xmm4, xmm0
 db 00Fh,056h,0A0h,030h,023h,000h,000h ; orps	xmm4, xmmword ptr [eax + 0x2330]
 db 00Fh,029h,0A0h,030h,023h,000h,000h ; movaps	xmmword ptr [eax + 0x2330], xmm4
 db 0E9h,071h,0FCh,0FFh,0FFh ; jmp	0x5d <_tnq_gpos+0x5d>
 db 08Dh,065h,0F8h ; lea	esp, [ebp - 0x8]
 db 05Eh ; pop	esi
 db 05Fh ; pop	edi
 db 05Dh ; pop	ebp
 db 0C3h ; ret
_tnq_gpos endp
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
public _tnq_vcodes
_tnq_vcodes proc near
 db 055h ; push	ebp
 db 08Dh,02Ch,024h ; lea	ebp, [esp]
 db 057h ; push	edi
 db 056h ; push	esi
 db 083h,0E4h,0FCh ; and	esp, -0x4
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 083h,0B8h,0ACh,007h,000h,000h,000h ; cmp	dword ptr [eax + 0x7ac], 0x0
 db 00Fh,084h,0AAh,000h,000h,000h ; je	0xc3 <_tnq_vcodes+0xc3>
 db 00Fh,028h,080h,060h,001h,000h,000h ; movaps	xmm0, xmmword ptr [eax + 0x160]
 db 00Fh,028h,088h,030h,024h,000h,000h ; movaps	xmm1, xmmword ptr [eax + 0x2430]
 db 00Fh,028h,090h,040h,024h,000h,000h ; movaps	xmm2, xmmword ptr [eax + 0x2440]
 db 031h,0C9h ; xor	ecx, ecx
 db 08Bh,090h,0B0h,007h,000h,000h ; mov	edx, dword ptr [eax + 0x7b0]
 db 08Bh,0B8h,0F8h,009h,000h,000h ; mov	edi, dword ptr [eax + 0x9f8]
 db 00Fh,028h,0E1h ; movaps	xmm4, xmm1
 db 00Fh,028h,0F0h ; movaps	xmm6, xmm0
 db 00Fh,028h,0F8h ; movaps	xmm7, xmm0
 db 08Dh,034h,011h ; lea	esi, [ecx + edx]
 db 003h,090h,014h,023h,000h,000h ; add	edx, dword ptr [eax + 0x2314]
 db 00Fh,028h,01Ch,0B7h ; movaps	xmm3, xmmword ptr [edi + 4*esi]
 db 08Bh,0B8h,0ECh,009h,000h,000h ; mov	edi, dword ptr [eax + 0x9ec]
 db 00Fh,059h,0E3h ; mulps	xmm4, xmm3
 db 00Fh,059h,0DAh ; mulps	xmm3, xmm2
 db 00Fh,028h,02Ch,0B7h ; movaps	xmm5, xmmword ptr [edi + 4*esi]
 db 08Bh,0B8h,0F0h,009h,000h,000h ; mov	edi, dword ptr [eax + 0x9f0]
 db 00Fh,05Ch,0F4h ; subps	xmm6, xmm4
 db 00Fh,05Ch,0FBh ; subps	xmm7, xmm3
 db 00Fh,0C2h,0E5h,001h ; cmpltps	xmm4, xmm5
 db 00Fh,054h,0A0h,050h,023h,000h,000h ; andps	xmm4, xmmword ptr [eax + 0x2350]
 db 00Fh,0C2h,0EEh,001h ; cmpltps	xmm5, xmm6
 db 00Fh,028h,034h,0B7h ; movaps	xmm6, xmmword ptr [edi + 4*esi]
 db 00Fh,0C2h,0DEh,001h ; cmpltps	xmm3, xmm6
 db 00Fh,0C2h,0F7h,001h ; cmpltps	xmm6, xmm7
 db 00Fh,054h,0A8h,040h,023h,000h,000h ; andps	xmm5, xmmword ptr [eax + 0x2340]
 db 00Fh,054h,098h,070h,023h,000h,000h ; andps	xmm3, xmmword ptr [eax + 0x2370]
 db 00Fh,054h,0B0h,060h,023h,000h,000h ; andps	xmm6, xmmword ptr [eax + 0x2360]
 db 00Fh,056h,0DCh ; orps	xmm3, xmm4
 db 00Fh,056h,0DDh ; orps	xmm3, xmm5
 db 00Fh,056h,0DEh ; orps	xmm3, xmm6
 db 066h,00Fh,06Bh,0DBh ; packssdw	xmm3, xmm3
 db 066h,00Fh,067h,0DBh ; packuswb	xmm3, xmm3
 db 066h,00Fh,07Eh,01Ch,011h ; movd	dword ptr [ecx + edx], xmm3
 db 083h,0C1h,004h ; add	ecx, 0x4
 db 03Bh,088h,0ACh,007h,000h,000h ; cmp	ecx, dword ptr [eax + 0x7ac]
 db 00Fh,082h,06Dh,0FFh,0FFh,0FFh ; jb	0x30 <_tnq_vcodes+0x30>
 db 08Dh,065h,0F8h ; lea	esp, [ebp - 0x8]
 db 05Eh ; pop	esi
 db 05Fh ; pop	edi
 db 05Dh ; pop	ebp
 db 0C3h ; ret
_tnq_vcodes endp
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
public _tnq_bbxy
_tnq_bbxy proc near
 db 055h ; push	ebp
 db 08Dh,02Ch,024h ; lea	ebp, [esp]
 db 057h ; push	edi
 db 056h ; push	esi
 db 083h,0E4h,0FCh ; and	esp, -0x4
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 08Bh,088h,0BCh,028h,000h,000h ; mov	ecx, dword ptr [eax + 0x28bc]
 db 00Fh,028h,098h,0D0h,024h,000h,000h ; movaps	xmm3, xmmword ptr [eax + 0x24d0]
 db 00Fh,028h,088h,0E0h,024h,000h,000h ; movaps	xmm1, xmmword ptr [eax + 0x24e0]
 db 085h,0C9h ; test	ecx, ecx
 db 074h,04Eh ; je	0x72 <_tnq_bbxy+0x72>
 db 08Bh,090h,0C4h,028h,000h,000h ; mov	edx, dword ptr [eax + 0x28c4]
 db 00Fh,028h,090h,030h,009h,000h,000h ; movaps	xmm2, xmmword ptr [eax + 0x930]
 db 08Bh,0B0h,044h,029h,000h,000h ; mov	esi, dword ptr [eax + 0x2944]
 db 066h,00Fh,0EFh,0C0h ; pxor	xmm0, xmm0
 db 00Fh,028h,0E1h ; movaps	xmm4, xmm1
 db 00Fh,028h,0EBh ; movaps	xmm5, xmm3
 db 08Dh,03Ch,012h ; lea	edi, [edx + edx]
 db 0F2h,00Fh,010h,00Eh ; movsd	xmm1, qword ptr [esi]
 db 00Fh,016h,00Ch,016h ; movhps	xmm1, qword ptr [esi + edx]
 db 001h,0FEh ; add	esi, edi
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 083h,0C1h,0FEh ; add	ecx, -0x2
 db 00Fh,05Dh,0DDh ; minps	xmm3, xmm5
 db 00Fh,028h,0EAh ; movaps	xmm5, xmm2
 db 00Fh,054h,0E9h ; andps	xmm5, xmm1
 db 00Fh,05Fh,0CCh ; maxps	xmm1, xmm4
 db 066h,00Fh,076h,0EAh ; pcmpeqd	xmm5, xmm2
 db 066h,00Fh,0EBh,0C5h ; por	xmm0, xmm5
 db 00Fh,028h,0EBh ; movaps	xmm5, xmm3
 db 00Fh,028h,0E1h ; movaps	xmm4, xmm1
 db 075h,0D4h ; jne	0x44 <_tnq_bbxy+0x44>
 db 0EBh,004h ; jmp	0x76 <_tnq_bbxy+0x76>
 db 066h,00Fh,0EFh,0C0h ; pxor	xmm0, xmm0
 db 00Fh,029h,098h,0D0h,024h,000h,000h ; movaps	xmmword ptr [eax + 0x24d0], xmm3
 db 00Fh,029h,088h,0E0h,024h,000h,000h ; movaps	xmmword ptr [eax + 0x24e0], xmm1
 db 066h,00Fh,07Fh,080h,0C0h,024h,000h,000h ; movdqa	xmmword ptr [eax + 0x24c0], xmm0
 db 08Dh,065h,0F8h ; lea	esp, [ebp - 0x8]
 db 05Eh ; pop	esi
 db 05Fh ; pop	edi
 db 05Dh ; pop	ebp
 db 0C3h ; ret
_tnq_bbxy endp
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
public _tnq_vc
_tnq_vc proc near
 db 055h ; push	ebp
 db 08Dh,02Ch,024h ; lea	ebp, [esp]
 db 053h ; push	ebx
 db 057h ; push	edi
 db 056h ; push	esi
 db 083h,0E4h,0FCh ; and	esp, -0x4
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 08Bh,088h,0BCh,028h,000h,000h ; mov	ecx, dword ptr [eax + 0x28bc]
 db 085h,0C9h ; test	ecx, ecx
 db 074h,07Eh ; je	0x95 <_tnq_vc+0x95>
 db 08Bh,090h,0C4h,028h,000h,000h ; mov	edx, dword ptr [eax + 0x28c4]
 db 00Fh,028h,080h,0F0h,024h,000h,000h ; movaps	xmm0, xmmword ptr [eax + 0x24f0]
 db 00Fh,028h,088h,000h,025h,000h,000h ; movaps	xmm1, xmmword ptr [eax + 0x2500]
 db 00Fh,028h,090h,010h,025h,000h,000h ; movaps	xmm2, xmmword ptr [eax + 0x2510]
 db 00Fh,028h,0A0h,020h,025h,000h,000h ; movaps	xmm4, xmmword ptr [eax + 0x2520]
 db 08Bh,0B0h,044h,029h,000h,000h ; mov	esi, dword ptr [eax + 0x2944]
 db 08Bh,0B8h,04Ch,029h,000h,000h ; mov	edi, dword ptr [eax + 0x294c]
 db 066h,00Fh,0EFh,0DBh ; pxor	xmm3, xmm3
 db 08Dh,01Ch,012h ; lea	ebx, [edx + edx]
 db 0F2h,00Fh,010h,02Eh ; movsd	xmm5, qword ptr [esi]
 db 00Fh,028h,0F1h ; movaps	xmm6, xmm1
 db 00Fh,016h,02Ch,016h ; movhps	xmm5, qword ptr [esi + edx]
 db 001h,0DEh ; add	esi, ebx
 db 00Fh,0C2h,0F5h,001h ; cmpltps	xmm6, xmm5
 db 00Fh,0C2h,0E8h,001h ; cmpltps	xmm5, xmm0
 db 00Fh,054h,0EAh ; andps	xmm5, xmm2
 db 00Fh,054h,0F4h ; andps	xmm6, xmm4
 db 00Fh,056h,0F5h ; orps	xmm6, xmm5
 db 00Fh,028h,0EEh ; movaps	xmm5, xmm6
 db 066h,00Fh,073h,0D5h,020h ; psrlq	xmm5, 0x20
 db 066h,00Fh,0EBh,0EEh ; por	xmm5, xmm6
 db 066h,00Fh,070h,0F5h,008h ; pshufd	xmm6, xmm5, 0x8
 db 066h,00Fh,0EBh,0DDh ; por	xmm3, xmm5
 db 066h,00Fh,06Bh,0F6h ; packssdw	xmm6, xmm6
 db 066h,00Fh,067h,0F6h ; packuswb	xmm6, xmm6
 db 066h,00Fh,07Eh,037h ; movd	dword ptr [edi], xmm6
 db 083h,0C7h,002h ; add	edi, 0x2
 db 083h,0C1h,0FEh ; add	ecx, -0x2
 db 075h,0B9h ; jne	0x4c <_tnq_vc+0x4c>
 db 0EBh,004h ; jmp	0x99 <_tnq_vc+0x99>
 db 066h,00Fh,0EFh,0DBh ; pxor	xmm3, xmm3
 db 066h,00Fh,07Fh,098h,0C0h,024h,000h,000h ; movdqa	xmmword ptr [eax + 0x24c0], xmm3
 db 08Dh,065h,0F4h ; lea	esp, [ebp - 0xc]
 db 05Eh ; pop	esi
 db 05Fh ; pop	edi
 db 05Bh ; pop	ebx
 db 05Dh ; pop	ebp
 db 0C3h ; ret
_tnq_vc endp
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
public _tnq_lit
_tnq_lit proc near
 db 055h ; push	ebp
 db 089h,0E5h ; mov	ebp, esp
 db 053h ; push	ebx
 db 057h ; push	edi
 db 056h ; push	esi
 db 083h,0E4h,0F0h ; and	esp, -0x10
 db 08Dh,0A4h,024h,060h,0FFh,0FFh,0FFh ; lea	esp, [esp - 0xa0]
 db 08Bh,055h,008h ; mov	edx, dword ptr [ebp + 0x8]
 db 08Bh,082h,0C0h,028h,000h,000h ; mov	eax, dword ptr [edx + 0x28c0]
 db 08Bh,09Ah,048h,029h,000h,000h ; mov	ebx, dword ptr [edx + 0x2948]
 db 00Fh,028h,082h,0A0h,028h,000h,000h ; movaps	xmm0, xmmword ptr [edx + 0x28a0]
 db 08Bh,08Ah,040h,029h,000h,000h ; mov	ecx, dword ptr [edx + 0x2940]
 db 08Bh,0BAh,044h,029h,000h,000h ; mov	edi, dword ptr [edx + 0x2944]
 db 083h,0BAh,0B8h,028h,000h,000h,000h ; cmp	dword ptr [edx + 0x28b8], 0x0
 db 089h,044h,024h,00Ch ; mov	dword ptr [esp + 0xc], eax
 db 08Bh,082h,0C4h,028h,000h,000h ; mov	eax, dword ptr [edx + 0x28c4]
 db 00Fh,029h,084h,024h,080h,000h,000h,000h ; movaps	xmmword ptr [esp + 0x80], xmm0
 db 089h,05Ch,024h,034h ; mov	dword ptr [esp + 0x34], ebx
 db 089h,044h,024h,008h ; mov	dword ptr [esp + 0x8], eax
 db 08Bh,082h,0C8h,028h,000h,000h ; mov	eax, dword ptr [edx + 0x28c8]
 db 089h,044h,024h,02Ch ; mov	dword ptr [esp + 0x2c], eax
 db 08Bh,082h,0B0h,028h,000h,000h ; mov	eax, dword ptr [edx + 0x28b0]
 db 089h,044h,024h,030h ; mov	dword ptr [esp + 0x30], eax
 db 08Bh,082h,0BCh,028h,000h,000h ; mov	eax, dword ptr [edx + 0x28bc]
 db 089h,044h,024h,010h ; mov	dword ptr [esp + 0x10], eax
 db 08Bh,082h,0B4h,028h,000h,000h ; mov	eax, dword ptr [edx + 0x28b4]
 db 00Fh,084h,004h,003h,000h,000h ; je	0x381 <_tnq_lit+0x381>
 db 085h,0C0h ; test	eax, eax
 db 08Bh,082h,0CCh,028h,000h,000h ; mov	eax, dword ptr [edx + 0x28cc]
 db 089h,044h,024h,024h ; mov	dword ptr [esp + 0x24], eax
 db 089h,0D0h ; mov	eax, edx
 db 00Fh,084h,057h,005h,000h,000h ; je	0x5e8 <_tnq_lit+0x5e8>
 db 083h,07Ch,024h,010h,000h ; cmp	dword ptr [esp + 0x10], 0x0
 db 00Fh,084h,0F1h,009h,000h,000h ; je	0xa8d <_tnq_lit+0xa8d>
 db 08Bh,074h,024h,00Ch ; mov	esi, dword ptr [esp + 0xc]
 db 08Dh,014h,0F5h,000h,000h,000h,000h ; lea	edx, [8*esi]
 db 089h,054h,024h,01Ch ; mov	dword ptr [esp + 0x1c], edx
 db 08Dh,014h,036h ; lea	edx, [esi + esi]
 db 089h,054h,024h,03Ch ; mov	dword ptr [esp + 0x3c], edx
 db 08Dh,014h,092h ; lea	edx, [edx + 4*edx]
 db 089h,054h,024h,018h ; mov	dword ptr [esp + 0x18], edx
 db 08Dh,014h,076h ; lea	edx, [esi + 2*esi]
 db 089h,054h,024h,038h ; mov	dword ptr [esp + 0x38], edx
 db 08Bh,054h,024h,008h ; mov	edx, dword ptr [esp + 0x8]
 db 08Dh,014h,052h ; lea	edx, [edx + 2*edx]
 db 089h,054h,024h,07Ch ; mov	dword ptr [esp + 0x7c], edx
 db 08Dh,014h,0B5h,000h,000h,000h,000h ; lea	edx, [4*esi]
 db 031h,0F6h ; xor	esi, esi
 db 089h,054h,024h,014h ; mov	dword ptr [esp + 0x14], edx
 db 08Bh,054h,024h,008h ; mov	edx, dword ptr [esp + 0x8]
 db 08Dh,014h,095h,000h,000h,000h,000h ; lea	edx, [4*edx]
 db 089h,054h,024h,028h ; mov	dword ptr [esp + 0x28], edx
 db 08Dh,090h,0D0h,026h,000h,000h ; lea	edx, [eax + 0x26d0]
 db 089h,054h,024h,078h ; mov	dword ptr [esp + 0x78], edx
 db 0E9h,027h,001h,000h,000h ; jmp	0x21d <_tnq_lit+0x21d>
 db 08Bh,054h,024h,024h ; mov	edx, dword ptr [esp + 0x24]
 db 08Bh,074h,024h,00Ch ; mov	esi, dword ptr [esp + 0xc]
 db 08Bh,05Ch,024h,008h ; mov	ebx, dword ptr [esp + 0x8]
 db 08Dh,004h,011h ; lea	eax, [ecx + edx]
 db 066h,00Fh,06Eh,004h,011h ; movd	xmm0, dword ptr [ecx + edx]
 db 08Bh,054h,024h,038h ; mov	edx, dword ptr [esp + 0x38]
 db 066h,00Fh,06Eh,02Ch,002h ; movd	xmm5, dword ptr [edx + eax]
 db 08Bh,054h,024h,03Ch ; mov	edx, dword ptr [esp + 0x3c]
 db 066h,00Fh,06Eh,00Ch,006h ; movd	xmm1, dword ptr [esi + eax]
 db 08Bh,074h,024h,020h ; mov	esi, dword ptr [esp + 0x20]
 db 066h,00Fh,06Eh,034h,002h ; movd	xmm6, dword ptr [edx + eax]
 db 066h,00Fh,062h,0C1h ; punpckldq	xmm0, xmm1
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 08Bh,054h,024h,07Ch ; mov	edx, dword ptr [esp + 0x7c]
 db 066h,00Fh,062h,0F5h ; punpckldq	xmm6, xmm5
 db 066h,00Fh,06Fh,0A8h,030h,01Fh,000h,000h ; movdqa	xmm5, xmmword ptr [eax + 0x1f30]
 db 066h,00Fh,06Ch,0C6h ; punpcklqdq	xmm0, xmm6
 db 066h,00Fh,06Fh,0CDh ; movdqa	xmm1, xmm5
 db 066h,00Fh,06Fh,0F0h ; movdqa	xmm6, xmm0
 db 066h,00Fh,06Fh,0F8h ; movdqa	xmm7, xmm0
 db 066h,00Fh,0DBh,0C8h ; pand	xmm1, xmm0
 db 066h,00Fh,072h,0D0h,018h ; psrld	xmm0, 0x18
 db 066h,00Fh,072h,0D6h,010h ; psrld	xmm6, 0x10
 db 066h,00Fh,072h,0D7h,008h ; psrld	xmm7, 0x8
 db 00Fh,05Bh,0C9h ; cvtdq2ps	xmm1, xmm1
 db 066h,00Fh,0DBh,0F5h ; pand	xmm6, xmm5
 db 066h,00Fh,0DBh,0FDh ; pand	xmm7, xmm5
 db 00Fh,05Bh,0F6h ; cvtdq2ps	xmm6, xmm6
 db 00Fh,05Bh,0EFh ; cvtdq2ps	xmm5, xmm7
 db 00Fh,059h,0CBh ; mulps	xmm1, xmm3
 db 00Fh,059h,0F4h ; mulps	xmm6, xmm4
 db 00Fh,059h,0EAh ; mulps	xmm5, xmm2
 db 00Fh,058h,088h,030h,028h,000h,000h ; addps	xmm1, xmmword ptr [eax + 0x2830]
 db 00Fh,058h,0B0h,050h,028h,000h,000h ; addps	xmm6, xmmword ptr [eax + 0x2850]
 db 00Fh,058h,0A8h,040h,028h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x2840]
 db 066h,00Fh,05Bh,0C9h ; cvtps2dq	xmm1, xmm1
 db 066h,00Fh,05Bh,0DDh ; cvtps2dq	xmm3, xmm5
 db 066h,00Fh,05Bh,0D6h ; cvtps2dq	xmm2, xmm6
 db 066h,00Fh,06Bh,0D8h ; packssdw	xmm3, xmm0
 db 066h,00Fh,06Bh,0CAh ; packssdw	xmm1, xmm2
 db 066h,00Fh,0EFh,0C0h ; pxor	xmm0, xmm0
 db 066h,00Fh,067h,0CBh ; packuswb	xmm1, xmm3
 db 066h,00Fh,06Fh,0D1h ; movdqa	xmm2, xmm1
 db 066h,00Fh,060h,0C8h ; punpcklbw	xmm1, xmm0
 db 066h,00Fh,068h,0D0h ; punpckhbw	xmm2, xmm0
 db 066h,00Fh,06Fh,0D9h ; movdqa	xmm3, xmm1
 db 066h,00Fh,061h,0DAh ; punpcklwd	xmm3, xmm2
 db 066h,00Fh,069h,0CAh ; punpckhwd	xmm1, xmm2
 db 066h,00Fh,06Fh,094h,024h,080h,000h,000h,000h ; movdqa	xmm2, xmmword ptr [esp + 0x80]
 db 066h,00Fh,067h,0DBh ; packuswb	xmm3, xmm3
 db 066h,00Fh,067h,0CBh ; packuswb	xmm1, xmm3
 db 066h,00Fh,061h,0D9h ; punpcklwd	xmm3, xmm1
 db 066h,00Fh,06Fh,0CBh ; movdqa	xmm1, xmm3
 db 066h,00Fh,06Ah,0DAh ; punpckhdq	xmm3, xmm2
 db 066h,00Fh,062h,0CAh ; punpckldq	xmm1, xmm2
 db 066h,00Fh,070h,0D3h,0EEh ; pshufd	xmm2, xmm3, 0xee
 db 066h,00Fh,0D6h,04Fh,010h ; movq	qword ptr [edi + 0x10], xmm1
 db 066h,00Fh,070h,0C9h,0EEh ; pshufd	xmm1, xmm1, 0xee
 db 066h,00Fh,0D6h,04Ch,01Fh,010h ; movq	qword ptr [edi + ebx + 0x10], xmm1
 db 066h,00Fh,0D6h,05Ch,05Fh,010h ; movq	qword ptr [edi + 2*ebx + 0x10], xmm3
 db 08Bh,05Ch,024h,034h ; mov	ebx, dword ptr [esp + 0x34]
 db 066h,00Fh,0D6h,054h,017h,010h ; movq	qword ptr [edi + edx + 0x10], xmm2
 db 083h,0C6h,004h ; add	esi, 0x4
 db 003h,04Ch,024h,014h ; add	ecx, dword ptr [esp + 0x14]
 db 003h,07Ch,024h,028h ; add	edi, dword ptr [esp + 0x28]
 db 03Bh,074h,024h,010h ; cmp	esi, dword ptr [esp + 0x10]
 db 00Fh,083h,070h,008h,000h,000h ; jae	0xa8d <_tnq_lit+0xa8d>
 db 08Bh,054h,024h,01Ch ; mov	edx, dword ptr [esp + 0x1c]
 db 085h,0DBh ; test	ebx, ebx
 db 00Fh,018h,00Ch,011h ; prefetcht0	byte ptr [ecx + edx]
 db 08Bh,054h,024h,018h ; mov	edx, dword ptr [esp + 0x18]
 db 00Fh,018h,00Ch,011h ; prefetcht0	byte ptr [ecx + edx]
 db 074h,006h ; je	0x237 <_tnq_lit+0x237>
 db 083h,03Ch,033h,000h ; cmp	dword ptr [ebx + esi], 0x0
 db 074h,0D1h ; je	0x208 <_tnq_lit+0x208>
 db 083h,07Ch,024h,030h,000h ; cmp	dword ptr [esp + 0x30], 0x0
 db 089h,074h,024h,020h ; mov	dword ptr [esp + 0x20], esi
 db 074h,050h ; je	0x292 <_tnq_lit+0x292>
 db 089h,0FAh ; mov	edx, edi
 db 08Bh,07Ch,024h,02Ch ; mov	edi, dword ptr [esp + 0x2c]
 db 08Bh,074h,024h,00Ch ; mov	esi, dword ptr [esp + 0xc]
 db 08Dh,01Ch,039h ; lea	ebx, [ecx + edi]
 db 0F2h,00Fh,010h,02Ch,039h ; movsd	xmm5, qword ptr [ecx + edi]
 db 0F3h,00Fh,010h,07Ch,039h,008h ; movss	xmm7, dword ptr [ecx + edi + 0x8]
 db 089h,0D7h ; mov	edi, edx
 db 08Dh,004h,033h ; lea	eax, [ebx + esi]
 db 00Fh,016h,02Ch,01Eh ; movhps	xmm5, qword ptr [esi + ebx]
 db 00Fh,016h,07Ch,01Eh,008h ; movhps	xmm7, qword ptr [esi + ebx + 0x8]
 db 0F2h,00Fh,010h,004h,006h ; movsd	xmm0, qword ptr [esi + eax]
 db 0F3h,00Fh,010h,04Ch,006h,008h ; movss	xmm1, dword ptr [esi + eax + 0x8]
 db 001h,0F0h ; add	eax, esi
 db 00Fh,016h,04Ch,006h,008h ; movhps	xmm1, qword ptr [esi + eax + 0x8]
 db 00Fh,016h,004h,006h ; movhps	xmm0, qword ptr [esi + eax]
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 00Fh,0C6h,0F9h,088h ; shufps	xmm7, xmm1, 0x88
 db 00Fh,028h,0CDh ; movaps	xmm1, xmm5
 db 00Fh,0C6h,0E8h,0DDh ; shufps	xmm5, xmm0, 0xdd
 db 00Fh,0C6h,0C8h,088h ; shufps	xmm1, xmm0, 0x88
 db 0EBh,009h ; jmp	0x29b <_tnq_lit+0x29b>
 db 00Fh,057h,0FFh ; xorps	xmm7, xmm7
 db 00Fh,057h,0EDh ; xorps	xmm5, xmm5
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 08Bh,098h,0B0h,028h,000h,000h ; mov	ebx, dword ptr [eax + 0x28b0]
 db 00Fh,028h,098h,060h,028h,000h,000h ; movaps	xmm3, xmmword ptr [eax + 0x2860]
 db 00Fh,028h,090h,070h,028h,000h,000h ; movaps	xmm2, xmmword ptr [eax + 0x2870]
 db 00Fh,028h,0A0h,080h,028h,000h,000h ; movaps	xmm4, xmmword ptr [eax + 0x2880]
 db 085h,0DBh ; test	ebx, ebx
 db 00Fh,084h,038h,0FEh,0FFh,0FFh ; je	0xf6 <_tnq_lit+0xf6>
 db 00Fh,028h,0F5h ; movaps	xmm6, xmm5
 db 00Fh,029h,04Ch,024h,040h ; movaps	xmmword ptr [esp + 0x40], xmm1
 db 00Fh,059h,0C9h ; mulps	xmm1, xmm1
 db 00Fh,028h,0C7h ; movaps	xmm0, xmm7
 db 00Fh,029h,06Ch,024h,060h ; movaps	xmmword ptr [esp + 0x60], xmm5
 db 00Fh,059h,0F5h ; mulps	xmm6, xmm5
 db 00Fh,059h,0C7h ; mulps	xmm0, xmm7
 db 00Fh,058h,0F1h ; addps	xmm6, xmm1
 db 00Fh,028h,088h,080h,008h,000h,000h ; movaps	xmm1, xmmword ptr [eax + 0x880]
 db 00Fh,058h,0C6h ; addps	xmm0, xmm6
 db 00Fh,052h,0E8h ; rsqrtps	xmm5, xmm0
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 00Fh,028h,0F5h ; movaps	xmm6, xmm5
 db 00Fh,059h,0F0h ; mulps	xmm6, xmm0
 db 00Fh,059h,0F5h ; mulps	xmm6, xmm5
 db 00Fh,029h,074h,024h,050h ; movaps	xmmword ptr [esp + 0x50], xmm6
 db 00Fh,028h,0B0h,060h,024h,000h,000h ; movaps	xmm6, xmmword ptr [eax + 0x2460]
 db 00Fh,05Ch,074h,024h,050h ; subps	xmm6, xmmword ptr [esp + 0x50]
 db 00Fh,059h,0F5h ; mulps	xmm6, xmm5
 db 00Fh,059h,0C6h ; mulps	xmm0, xmm6
 db 00Fh,059h,0C6h ; mulps	xmm0, xmm6
 db 00Fh,05Ch,0C8h ; subps	xmm1, xmm0
 db 00Fh,059h,0CEh ; mulps	xmm1, xmm6
 db 00Fh,058h,0CEh ; addps	xmm1, xmm6
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,028h,088h,060h,001h,000h,000h ; movaps	xmm1, xmmword ptr [eax + 0x160]
 db 08Bh,044h,024h,078h ; mov	eax, dword ptr [esp + 0x78]
 db 00Fh,029h,04Ch,024h,050h ; movaps	xmmword ptr [esp + 0x50], xmm1
 db 00Fh,028h,0CFh ; movaps	xmm1, xmm7
 db 00Fh,028h,0A8h,060h,0FEh,0FFh,0FFh ; movaps	xmm5, xmmword ptr [eax - 0x1a0]
 db 00Fh,028h,0B8h,070h,0FEh,0FFh,0FFh ; movaps	xmm7, xmmword ptr [eax - 0x190]
 db 00Fh,028h,0B0h,080h,0FEh,0FFh,0FFh ; movaps	xmm6, xmmword ptr [eax - 0x180]
 db 00Fh,059h,06Ch,024h,040h ; mulps	xmm5, xmmword ptr [esp + 0x40]
 db 00Fh,059h,07Ch,024h,060h ; mulps	xmm7, xmmword ptr [esp + 0x60]
 db 00Fh,059h,0F1h ; mulps	xmm6, xmm1
 db 00Fh,058h,0FDh ; addps	xmm7, xmm5
 db 00Fh,028h,068h,0E0h ; movaps	xmm5, xmmword ptr [eax - 0x20]
 db 00Fh,058h,0F7h ; addps	xmm6, xmm7
 db 00Fh,028h,078h,0F0h ; movaps	xmm7, xmmword ptr [eax - 0x10]
 db 00Fh,059h,0F0h ; mulps	xmm6, xmm0
 db 00Fh,05Fh,074h,024h,050h ; maxps	xmm6, xmmword ptr [esp + 0x50]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,059h,0FEh ; mulps	xmm7, xmm6
 db 00Fh,059h,030h ; mulps	xmm6, xmmword ptr [eax]
 db 083h,0C0h,030h ; add	eax, 0x30
 db 04Bh ; dec	ebx
 db 00Fh,058h,0DDh ; addps	xmm3, xmm5
 db 00Fh,058h,0D7h ; addps	xmm2, xmm7
 db 00Fh,058h,0E6h ; addps	xmm4, xmm6
 db 075h,0B0h ; jne	0x32c <_tnq_lit+0x32c>
 db 0E9h,075h,0FDh,0FFh,0FFh ; jmp	0xf6 <_tnq_lit+0xf6>
 db 085h,0C0h ; test	eax, eax
 db 00Fh,084h,0F4h,004h,000h,000h ; je	0x87d <_tnq_lit+0x87d>
 db 083h,07Ch,024h,010h,000h ; cmp	dword ptr [esp + 0x10], 0x0
 db 00Fh,084h,0F9h,006h,000h,000h ; je	0xa8d <_tnq_lit+0xa8d>
 db 08Bh,074h,024h,00Ch ; mov	esi, dword ptr [esp + 0xc]
 db 08Dh,014h,0F5h,000h,000h,000h,000h ; lea	edx, [8*esi]
 db 08Dh,004h,036h ; lea	eax, [esi + esi]
 db 089h,054h,024h,020h ; mov	dword ptr [esp + 0x20], edx
 db 08Bh,054h,024h,008h ; mov	edx, dword ptr [esp + 0x8]
 db 08Dh,004h,080h ; lea	eax, [eax + 4*eax]
 db 08Dh,014h,052h ; lea	edx, [edx + 2*edx]
 db 089h,044h,024h,024h ; mov	dword ptr [esp + 0x24], eax
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 089h,054h,024h,014h ; mov	dword ptr [esp + 0x14], edx
 db 08Dh,014h,0B5h,000h,000h,000h,000h ; lea	edx, [4*esi]
 db 089h,054h,024h,01Ch ; mov	dword ptr [esp + 0x1c], edx
 db 08Bh,054h,024h,008h ; mov	edx, dword ptr [esp + 0x8]
 db 08Dh,014h,095h,000h,000h,000h,000h ; lea	edx, [4*edx]
 db 089h,054h,024h,018h ; mov	dword ptr [esp + 0x18], edx
 db 08Dh,090h,0D0h,026h,000h,000h ; lea	edx, [eax + 0x26d0]
 db 089h,054h,024h,028h ; mov	dword ptr [esp + 0x28], edx
 db 031h,0D2h ; xor	edx, edx
 db 0E9h,09Eh,000h,000h,000h ; jmp	0x484 <_tnq_lit+0x484>
 db 066h,00Fh,05Bh,0C6h ; cvtps2dq	xmm0, xmm6
 db 066h,00Fh,05Bh,0CFh ; cvtps2dq	xmm1, xmm7
 db 066h,00Fh,05Bh,0D5h ; cvtps2dq	xmm2, xmm5
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 08Bh,05Ch,024h,008h ; mov	ebx, dword ptr [esp + 0x8]
 db 08Bh,074h,024h,014h ; mov	esi, dword ptr [esp + 0x14]
 db 066h,00Fh,06Bh,090h,090h,028h,000h,000h ; packssdw	xmm2, xmmword ptr [eax + 0x2890]
 db 066h,00Fh,06Bh,0C1h ; packssdw	xmm0, xmm1
 db 066h,00Fh,0EFh,0C9h ; pxor	xmm1, xmm1
 db 066h,00Fh,067h,0C2h ; packuswb	xmm0, xmm2
 db 066h,00Fh,06Fh,0D0h ; movdqa	xmm2, xmm0
 db 066h,00Fh,060h,0C1h ; punpcklbw	xmm0, xmm1
 db 066h,00Fh,068h,0D1h ; punpckhbw	xmm2, xmm1
 db 066h,00Fh,06Fh,0C8h ; movdqa	xmm1, xmm0
 db 066h,00Fh,061h,0CAh ; punpcklwd	xmm1, xmm2
 db 066h,00Fh,069h,0C2h ; punpckhwd	xmm0, xmm2
 db 066h,00Fh,06Fh,094h,024h,080h,000h,000h,000h ; movdqa	xmm2, xmmword ptr [esp + 0x80]
 db 066h,00Fh,067h,0C9h ; packuswb	xmm1, xmm1
 db 066h,00Fh,067h,0C0h ; packuswb	xmm0, xmm0
 db 066h,00Fh,061h,0C8h ; punpcklwd	xmm1, xmm0
 db 066h,00Fh,06Fh,0C1h ; movdqa	xmm0, xmm1
 db 066h,00Fh,06Ah,0CAh ; punpckhdq	xmm1, xmm2
 db 066h,00Fh,062h,0C2h ; punpckldq	xmm0, xmm2
 db 066h,00Fh,070h,0D1h,0EEh ; pshufd	xmm2, xmm1, 0xee
 db 066h,00Fh,0D6h,047h,010h ; movq	qword ptr [edi + 0x10], xmm0
 db 066h,00Fh,070h,0C0h,0EEh ; pshufd	xmm0, xmm0, 0xee
 db 066h,00Fh,0D6h,044h,01Fh,010h ; movq	qword ptr [edi + ebx + 0x10], xmm0
 db 066h,00Fh,0D6h,04Ch,05Fh,010h ; movq	qword ptr [edi + 2*ebx + 0x10], xmm1
 db 08Bh,05Ch,024h,034h ; mov	ebx, dword ptr [esp + 0x34]
 db 066h,00Fh,0D6h,054h,037h,010h ; movq	qword ptr [edi + esi + 0x10], xmm2
 db 083h,0C2h,004h ; add	edx, 0x4
 db 003h,04Ch,024h,01Ch ; add	ecx, dword ptr [esp + 0x1c]
 db 003h,07Ch,024h,018h ; add	edi, dword ptr [esp + 0x18]
 db 03Bh,054h,024h,010h ; cmp	edx, dword ptr [esp + 0x10]
 db 00Fh,083h,009h,006h,000h,000h ; jae	0xa8d <_tnq_lit+0xa8d>
 db 08Bh,074h,024h,020h ; mov	esi, dword ptr [esp + 0x20]
 db 085h,0DBh ; test	ebx, ebx
 db 00Fh,018h,00Ch,031h ; prefetcht0	byte ptr [ecx + esi]
 db 08Bh,074h,024h,024h ; mov	esi, dword ptr [esp + 0x24]
 db 00Fh,018h,00Ch,031h ; prefetcht0	byte ptr [ecx + esi]
 db 074h,006h ; je	0x49e <_tnq_lit+0x49e>
 db 083h,03Ch,013h,000h ; cmp	dword ptr [ebx + edx], 0x0
 db 074h,0D1h ; je	0x46f <_tnq_lit+0x46f>
 db 083h,07Ch,024h,030h,000h ; cmp	dword ptr [esp + 0x30], 0x0
 db 074h,054h ; je	0x4f9 <_tnq_lit+0x4f9>
 db 089h,07Ch,024h,040h ; mov	dword ptr [esp + 0x40], edi
 db 08Bh,07Ch,024h,02Ch ; mov	edi, dword ptr [esp + 0x2c]
 db 08Bh,074h,024h,00Ch ; mov	esi, dword ptr [esp + 0xc]
 db 08Dh,01Ch,039h ; lea	ebx, [ecx + edi]
 db 0F2h,00Fh,010h,014h,039h ; movsd	xmm2, qword ptr [ecx + edi]
 db 0F3h,00Fh,010h,064h,039h,008h ; movss	xmm4, dword ptr [ecx + edi + 0x8]
 db 08Bh,07Ch,024h,040h ; mov	edi, dword ptr [esp + 0x40]
 db 08Dh,004h,033h ; lea	eax, [ebx + esi]
 db 00Fh,016h,014h,01Eh ; movhps	xmm2, qword ptr [esi + ebx]
 db 00Fh,016h,064h,01Eh,008h ; movhps	xmm4, qword ptr [esi + ebx + 0x8]
 db 0F2h,00Fh,010h,004h,006h ; movsd	xmm0, qword ptr [esi + eax]
 db 0F3h,00Fh,010h,04Ch,006h,008h ; movss	xmm1, dword ptr [esi + eax + 0x8]
 db 001h,0F0h ; add	eax, esi
 db 00Fh,016h,04Ch,006h,008h ; movhps	xmm1, qword ptr [esi + eax + 0x8]
 db 00Fh,016h,004h,006h ; movhps	xmm0, qword ptr [esi + eax]
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 00Fh,0C6h,0E1h,088h ; shufps	xmm4, xmm1, 0x88
 db 00Fh,028h,0CAh ; movaps	xmm1, xmm2
 db 00Fh,0C6h,0D0h,0DDh ; shufps	xmm2, xmm0, 0xdd
 db 00Fh,0C6h,0C8h,088h ; shufps	xmm1, xmm0, 0x88
 db 0EBh,009h ; jmp	0x502 <_tnq_lit+0x502>
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,057h,0D2h ; xorps	xmm2, xmm2
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 08Bh,098h,0B0h,028h,000h,000h ; mov	ebx, dword ptr [eax + 0x28b0]
 db 00Fh,028h,0B0h,030h,028h,000h,000h ; movaps	xmm6, xmmword ptr [eax + 0x2830]
 db 00Fh,028h,0A8h,040h,028h,000h,000h ; movaps	xmm5, xmmword ptr [eax + 0x2840]
 db 00Fh,028h,0B8h,050h,028h,000h,000h ; movaps	xmm7, xmmword ptr [eax + 0x2850]
 db 085h,0DBh ; test	ebx, ebx
 db 00Fh,084h,0C1h,0FEh,0FFh,0FFh ; je	0x3e6 <_tnq_lit+0x3e6>
 db 00Fh,028h,0DAh ; movaps	xmm3, xmm2
 db 00Fh,029h,04Ch,024h,040h ; movaps	xmmword ptr [esp + 0x40], xmm1
 db 00Fh,059h,0C9h ; mulps	xmm1, xmm1
 db 00Fh,028h,0C4h ; movaps	xmm0, xmm4
 db 00Fh,029h,054h,024h,060h ; movaps	xmmword ptr [esp + 0x60], xmm2
 db 00Fh,059h,0DAh ; mulps	xmm3, xmm2
 db 00Fh,059h,0C4h ; mulps	xmm0, xmm4
 db 00Fh,058h,0D9h ; addps	xmm3, xmm1
 db 00Fh,028h,088h,080h,008h,000h,000h ; movaps	xmm1, xmmword ptr [eax + 0x880]
 db 00Fh,058h,0C3h ; addps	xmm0, xmm3
 db 00Fh,052h,0D0h ; rsqrtps	xmm2, xmm0
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 00Fh,028h,0DAh ; movaps	xmm3, xmm2
 db 00Fh,059h,0D8h ; mulps	xmm3, xmm0
 db 00Fh,059h,0DAh ; mulps	xmm3, xmm2
 db 00Fh,029h,05Ch,024h,050h ; movaps	xmmword ptr [esp + 0x50], xmm3
 db 00Fh,028h,098h,060h,024h,000h,000h ; movaps	xmm3, xmmword ptr [eax + 0x2460]
 db 00Fh,05Ch,05Ch,024h,050h ; subps	xmm3, xmmword ptr [esp + 0x50]
 db 00Fh,059h,0DAh ; mulps	xmm3, xmm2
 db 00Fh,059h,0C3h ; mulps	xmm0, xmm3
 db 00Fh,059h,0C3h ; mulps	xmm0, xmm3
 db 00Fh,05Ch,0C8h ; subps	xmm1, xmm0
 db 00Fh,059h,0CBh ; mulps	xmm1, xmm3
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,028h,0C1h ; movaps	xmm0, xmm1
 db 00Fh,028h,088h,060h,001h,000h,000h ; movaps	xmm1, xmmword ptr [eax + 0x160]
 db 08Bh,044h,024h,028h ; mov	eax, dword ptr [esp + 0x28]
 db 00Fh,029h,04Ch,024h,050h ; movaps	xmmword ptr [esp + 0x50], xmm1
 db 00Fh,028h,0CCh ; movaps	xmm1, xmm4
 db 00Fh,028h,090h,060h,0FEh,0FFh,0FFh ; movaps	xmm2, xmmword ptr [eax - 0x1a0]
 db 00Fh,028h,0A0h,070h,0FEh,0FFh,0FFh ; movaps	xmm4, xmmword ptr [eax - 0x190]
 db 00Fh,028h,098h,080h,0FEh,0FFh,0FFh ; movaps	xmm3, xmmword ptr [eax - 0x180]
 db 00Fh,059h,054h,024h,040h ; mulps	xmm2, xmmword ptr [esp + 0x40]
 db 00Fh,059h,064h,024h,060h ; mulps	xmm4, xmmword ptr [esp + 0x60]
 db 00Fh,059h,0D9h ; mulps	xmm3, xmm1
 db 00Fh,058h,0E2h ; addps	xmm4, xmm2
 db 00Fh,028h,050h,0E0h ; movaps	xmm2, xmmword ptr [eax - 0x20]
 db 00Fh,058h,0DCh ; addps	xmm3, xmm4
 db 00Fh,028h,060h,0F0h ; movaps	xmm4, xmmword ptr [eax - 0x10]
 db 00Fh,059h,0D8h ; mulps	xmm3, xmm0
 db 00Fh,05Fh,05Ch,024h,050h ; maxps	xmm3, xmmword ptr [esp + 0x50]
 db 00Fh,059h,0D3h ; mulps	xmm2, xmm3
 db 00Fh,059h,0E3h ; mulps	xmm4, xmm3
 db 00Fh,059h,018h ; mulps	xmm3, xmmword ptr [eax]
 db 083h,0C0h,030h ; add	eax, 0x30
 db 04Bh ; dec	ebx
 db 00Fh,058h,0F2h ; addps	xmm6, xmm2
 db 00Fh,058h,0ECh ; addps	xmm5, xmm4
 db 00Fh,058h,0FBh ; addps	xmm7, xmm3
 db 075h,0B0h ; jne	0x593 <_tnq_lit+0x593>
 db 0E9h,0FEh,0FDh,0FFh,0FFh ; jmp	0x3e6 <_tnq_lit+0x3e6>
 db 083h,07Ch,024h,010h,000h ; cmp	dword ptr [esp + 0x10], 0x0
 db 00Fh,084h,09Ah,004h,000h,000h ; je	0xa8d <_tnq_lit+0xa8d>
 db 08Bh,074h,024h,00Ch ; mov	esi, dword ptr [esp + 0xc]
 db 08Dh,014h,0F5h,000h,000h,000h,000h ; lea	edx, [8*esi]
 db 089h,054h,024h,050h ; mov	dword ptr [esp + 0x50], edx
 db 08Dh,014h,036h ; lea	edx, [esi + esi]
 db 089h,054h,024h,014h ; mov	dword ptr [esp + 0x14], edx
 db 08Dh,014h,092h ; lea	edx, [edx + 4*edx]
 db 089h,054h,024h,020h ; mov	dword ptr [esp + 0x20], edx
 db 08Dh,014h,076h ; lea	edx, [esi + 2*esi]
 db 089h,054h,024h,028h ; mov	dword ptr [esp + 0x28], edx
 db 08Bh,054h,024h,008h ; mov	edx, dword ptr [esp + 0x8]
 db 08Dh,014h,052h ; lea	edx, [edx + 2*edx]
 db 089h,054h,024h,03Ch ; mov	dword ptr [esp + 0x3c], edx
 db 08Dh,014h,0B5h,000h,000h,000h,000h ; lea	edx, [4*esi]
 db 089h,054h,024h,01Ch ; mov	dword ptr [esp + 0x1c], edx
 db 08Bh,054h,024h,008h ; mov	edx, dword ptr [esp + 0x8]
 db 08Dh,014h,095h,000h,000h,000h,000h ; lea	edx, [4*edx]
 db 089h,054h,024h,018h ; mov	dword ptr [esp + 0x18], edx
 db 08Dh,090h,0D0h,026h,000h,000h ; lea	edx, [eax + 0x26d0]
 db 089h,054h,024h,038h ; mov	dword ptr [esp + 0x38], edx
 db 031h,0D2h ; xor	edx, edx
 db 0E9h,01Bh,001h,000h,000h ; jmp	0x768 <_tnq_lit+0x768>
 db 08Bh,074h,024h,024h ; mov	esi, dword ptr [esp + 0x24]
 db 08Bh,05Ch,024h,008h ; mov	ebx, dword ptr [esp + 0x8]
 db 08Dh,004h,031h ; lea	eax, [ecx + esi]
 db 0F3h,00Fh,010h,004h,031h ; movss	xmm0, dword ptr [ecx + esi]
 db 08Bh,074h,024h,00Ch ; mov	esi, dword ptr [esp + 0xc]
 db 0F3h,00Fh,010h,00Ch,006h ; movss	xmm1, dword ptr [esi + eax]
 db 08Bh,074h,024h,028h ; mov	esi, dword ptr [esp + 0x28]
 db 0F3h,00Fh,010h,02Ch,006h ; movss	xmm5, dword ptr [esi + eax]
 db 08Bh,074h,024h,014h ; mov	esi, dword ptr [esp + 0x14]
 db 00Fh,014h,0C1h ; unpcklps	xmm0, xmm1
 db 0F3h,00Fh,010h,03Ch,006h ; movss	xmm7, dword ptr [esi + eax]
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 08Bh,074h,024h,03Ch ; mov	esi, dword ptr [esp + 0x3c]
 db 00Fh,014h,0FDh ; unpcklps	xmm7, xmm5
 db 00Fh,028h,0B0h,030h,01Fh,000h,000h ; movaps	xmm6, xmmword ptr [eax + 0x1f30]
 db 00Fh,016h,0C7h ; movlhps	xmm0, xmm7
 db 00Fh,028h,0CEh ; movaps	xmm1, xmm6
 db 00Fh,054h,0C8h ; andps	xmm1, xmm0
 db 00Fh,028h,0F8h ; movaps	xmm7, xmm0
 db 00Fh,05Bh,0E9h ; cvtdq2ps	xmm5, xmm1
 db 00Fh,028h,0C8h ; movaps	xmm1, xmm0
 db 066h,00Fh,072h,0D7h,008h ; psrld	xmm7, 0x8
 db 066h,00Fh,072h,0D0h,018h ; psrld	xmm0, 0x18
 db 066h,00Fh,072h,0D1h,010h ; psrld	xmm1, 0x10
 db 066h,00Fh,0DBh,0FEh ; pand	xmm7, xmm6
 db 066h,00Fh,0DBh,0CEh ; pand	xmm1, xmm6
 db 00Fh,05Bh,0F7h ; cvtdq2ps	xmm6, xmm7
 db 00Fh,05Bh,0C9h ; cvtdq2ps	xmm1, xmm1
 db 00Fh,059h,0EBh ; mulps	xmm5, xmm3
 db 00Fh,059h,0F2h ; mulps	xmm6, xmm2
 db 00Fh,058h,0A8h,030h,028h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x2830]
 db 00Fh,059h,0CCh ; mulps	xmm1, xmm4
 db 00Fh,058h,0B0h,040h,028h,000h,000h ; addps	xmm6, xmmword ptr [eax + 0x2840]
 db 00Fh,058h,088h,050h,028h,000h,000h ; addps	xmm1, xmmword ptr [eax + 0x2850]
 db 066h,00Fh,05Bh,0D5h ; cvtps2dq	xmm2, xmm5
 db 066h,00Fh,05Bh,0DEh ; cvtps2dq	xmm3, xmm6
 db 066h,00Fh,05Bh,0C9h ; cvtps2dq	xmm1, xmm1
 db 066h,00Fh,06Bh,0D8h ; packssdw	xmm3, xmm0
 db 066h,00Fh,0EFh,0C0h ; pxor	xmm0, xmm0
 db 066h,00Fh,06Bh,0D1h ; packssdw	xmm2, xmm1
 db 066h,00Fh,067h,0D3h ; packuswb	xmm2, xmm3
 db 066h,00Fh,06Fh,0CAh ; movdqa	xmm1, xmm2
 db 066h,00Fh,060h,0D0h ; punpcklbw	xmm2, xmm0
 db 066h,00Fh,068h,0C8h ; punpckhbw	xmm1, xmm0
 db 066h,00Fh,06Fh,0DAh ; movdqa	xmm3, xmm2
 db 066h,00Fh,061h,0D9h ; punpcklwd	xmm3, xmm1
 db 066h,00Fh,069h,0D1h ; punpckhwd	xmm2, xmm1
 db 066h,00Fh,067h,0DBh ; packuswb	xmm3, xmm3
 db 066h,00Fh,067h,0D3h ; packuswb	xmm2, xmm3
 db 066h,00Fh,061h,0DAh ; punpcklwd	xmm3, xmm2
 db 066h,00Fh,06Fh,094h,024h,080h,000h,000h,000h ; movdqa	xmm2, xmmword ptr [esp + 0x80]
 db 066h,00Fh,06Fh,0CBh ; movdqa	xmm1, xmm3
 db 066h,00Fh,06Ah,0DAh ; punpckhdq	xmm3, xmm2
 db 066h,00Fh,062h,0CAh ; punpckldq	xmm1, xmm2
 db 066h,00Fh,070h,0D3h,0EEh ; pshufd	xmm2, xmm3, 0xee
 db 066h,00Fh,0D6h,04Fh,010h ; movq	qword ptr [edi + 0x10], xmm1
 db 066h,00Fh,070h,0C9h,0EEh ; pshufd	xmm1, xmm1, 0xee
 db 066h,00Fh,0D6h,04Ch,01Fh,010h ; movq	qword ptr [edi + ebx + 0x10], xmm1
 db 066h,00Fh,0D6h,05Ch,05Fh,010h ; movq	qword ptr [edi + 2*ebx + 0x10], xmm3
 db 08Bh,05Ch,024h,034h ; mov	ebx, dword ptr [esp + 0x34]
 db 066h,00Fh,0D6h,054h,037h,010h ; movq	qword ptr [edi + esi + 0x10], xmm2
 db 083h,0C2h,004h ; add	edx, 0x4
 db 003h,04Ch,024h,01Ch ; add	ecx, dword ptr [esp + 0x1c]
 db 003h,07Ch,024h,018h ; add	edi, dword ptr [esp + 0x18]
 db 03Bh,054h,024h,010h ; cmp	edx, dword ptr [esp + 0x10]
 db 00Fh,083h,025h,003h,000h,000h ; jae	0xa8d <_tnq_lit+0xa8d>
 db 08Bh,074h,024h,050h ; mov	esi, dword ptr [esp + 0x50]
 db 085h,0DBh ; test	ebx, ebx
 db 00Fh,018h,00Ch,031h ; prefetcht0	byte ptr [ecx + esi]
 db 08Bh,074h,024h,020h ; mov	esi, dword ptr [esp + 0x20]
 db 00Fh,018h,00Ch,031h ; prefetcht0	byte ptr [ecx + esi]
 db 074h,006h ; je	0x782 <_tnq_lit+0x782>
 db 083h,03Ch,013h,000h ; cmp	dword ptr [ebx + edx], 0x0
 db 074h,0D1h ; je	0x753 <_tnq_lit+0x753>
 db 083h,07Ch,024h,030h,000h ; cmp	dword ptr [esp + 0x30], 0x0
 db 074h,057h ; je	0x7e0 <_tnq_lit+0x7e0>
 db 089h,07Ch,024h,040h ; mov	dword ptr [esp + 0x40], edi
 db 08Bh,07Ch,024h,02Ch ; mov	edi, dword ptr [esp + 0x2c]
 db 08Bh,074h,024h,00Ch ; mov	esi, dword ptr [esp + 0xc]
 db 08Dh,01Ch,039h ; lea	ebx, [ecx + edi]
 db 0F2h,00Fh,010h,014h,039h ; movsd	xmm2, qword ptr [ecx + edi]
 db 0F3h,00Fh,010h,06Ch,039h,008h ; movss	xmm5, dword ptr [ecx + edi + 0x8]
 db 08Bh,07Ch,024h,040h ; mov	edi, dword ptr [esp + 0x40]
 db 08Dh,004h,033h ; lea	eax, [ebx + esi]
 db 00Fh,016h,014h,01Eh ; movhps	xmm2, qword ptr [esi + ebx]
 db 00Fh,016h,06Ch,01Eh,008h ; movhps	xmm5, qword ptr [esi + ebx + 0x8]
 db 0F2h,00Fh,010h,004h,006h ; movsd	xmm0, qword ptr [esi + eax]
 db 0F3h,00Fh,010h,04Ch,006h,008h ; movss	xmm1, dword ptr [esi + eax + 0x8]
 db 001h,0F0h ; add	eax, esi
 db 00Fh,028h,0FAh ; movaps	xmm7, xmm2
 db 00Fh,016h,04Ch,006h,008h ; movhps	xmm1, qword ptr [esi + eax + 0x8]
 db 00Fh,016h,004h,006h ; movhps	xmm0, qword ptr [esi + eax]
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 00Fh,0C6h,0E9h,088h ; shufps	xmm5, xmm1, 0x88
 db 00Fh,028h,0CAh ; movaps	xmm1, xmm2
 db 00Fh,0C6h,0F8h,088h ; shufps	xmm7, xmm0, 0x88
 db 00Fh,0C6h,0C8h,0DDh ; shufps	xmm1, xmm0, 0xdd
 db 0EBh,009h ; jmp	0x7e9 <_tnq_lit+0x7e9>
 db 00Fh,057h,0EDh ; xorps	xmm5, xmm5
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,057h,0FFh ; xorps	xmm7, xmm7
 db 08Bh,098h,0B0h,028h,000h,000h ; mov	ebx, dword ptr [eax + 0x28b0]
 db 00Fh,028h,098h,060h,028h,000h,000h ; movaps	xmm3, xmmword ptr [eax + 0x2860]
 db 00Fh,028h,090h,070h,028h,000h,000h ; movaps	xmm2, xmmword ptr [eax + 0x2870]
 db 00Fh,028h,0A0h,080h,028h,000h,000h ; movaps	xmm4, xmmword ptr [eax + 0x2880]
 db 085h,0DBh ; test	ebx, ebx
 db 00Fh,084h,041h,0FEh,0FFh,0FFh ; je	0x64d <_tnq_lit+0x64d>
 db 00Fh,028h,080h,060h,001h,000h,000h ; movaps	xmm0, xmmword ptr [eax + 0x160]
 db 08Bh,044h,024h,038h ; mov	eax, dword ptr [esp + 0x38]
 db 00Fh,029h,06Ch,024h,060h ; movaps	xmmword ptr [esp + 0x60], xmm5
 db 00Fh,029h,044h,024h,040h ; movaps	xmmword ptr [esp + 0x40], xmm0
 db 00Fh,028h,0B0h,060h,0FEh,0FFh,0FFh ; movaps	xmm6, xmmword ptr [eax - 0x1a0]
 db 00Fh,028h,0A8h,070h,0FEh,0FFh,0FFh ; movaps	xmm5, xmmword ptr [eax - 0x190]
 db 00Fh,028h,0C7h ; movaps	xmm0, xmm7
 db 00Fh,059h,0F7h ; mulps	xmm6, xmm7
 db 00Fh,059h,0E9h ; mulps	xmm5, xmm1
 db 00Fh,028h,0F9h ; movaps	xmm7, xmm1
 db 00Fh,028h,088h,080h,0FEh,0FFh,0FFh ; movaps	xmm1, xmmword ptr [eax - 0x180]
 db 00Fh,059h,04Ch,024h,060h ; mulps	xmm1, xmmword ptr [esp + 0x60]
 db 00Fh,058h,0EEh ; addps	xmm5, xmm6
 db 00Fh,028h,070h,0F0h ; movaps	xmm6, xmmword ptr [eax - 0x10]
 db 00Fh,058h,0CDh ; addps	xmm1, xmm5
 db 00Fh,028h,068h,0E0h ; movaps	xmm5, xmmword ptr [eax - 0x20]
 db 00Fh,05Fh,04Ch,024h,040h ; maxps	xmm1, xmmword ptr [esp + 0x40]
 db 00Fh,059h,0E9h ; mulps	xmm5, xmm1
 db 00Fh,059h,0F1h ; mulps	xmm6, xmm1
 db 00Fh,059h,008h ; mulps	xmm1, xmmword ptr [eax]
 db 083h,0C0h,030h ; add	eax, 0x30
 db 04Bh ; dec	ebx
 db 00Fh,058h,0E1h ; addps	xmm4, xmm1
 db 00Fh,058h,0DDh ; addps	xmm3, xmm5
 db 00Fh,058h,0D6h ; addps	xmm2, xmm6
 db 00Fh,028h,0CFh ; movaps	xmm1, xmm7
 db 00Fh,028h,0F8h ; movaps	xmm7, xmm0
 db 075h,0A9h ; jne	0x821 <_tnq_lit+0x821>
 db 0E9h,0D0h,0FDh,0FFh,0FFh ; jmp	0x64d <_tnq_lit+0x64d>
 db 083h,07Ch,024h,010h,000h ; cmp	dword ptr [esp + 0x10], 0x0
 db 00Fh,084h,005h,002h,000h,000h ; je	0xa8d <_tnq_lit+0xa8d>
 db 08Bh,074h,024h,00Ch ; mov	esi, dword ptr [esp + 0xc]
 db 08Dh,014h,0F5h,000h,000h,000h,000h ; lea	edx, [8*esi]
 db 08Dh,004h,036h ; lea	eax, [esi + esi]
 db 089h,054h,024h,050h ; mov	dword ptr [esp + 0x50], edx
 db 08Bh,054h,024h,008h ; mov	edx, dword ptr [esp + 0x8]
 db 08Dh,004h,080h ; lea	eax, [eax + 4*eax]
 db 08Dh,014h,052h ; lea	edx, [edx + 2*edx]
 db 089h,044h,024h,020h ; mov	dword ptr [esp + 0x20], eax
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 089h,054h,024h,018h ; mov	dword ptr [esp + 0x18], edx
 db 08Dh,014h,0B5h,000h,000h,000h,000h ; lea	edx, [4*esi]
 db 089h,054h,024h,024h ; mov	dword ptr [esp + 0x24], edx
 db 08Bh,054h,024h,008h ; mov	edx, dword ptr [esp + 0x8]
 db 08Dh,014h,095h,000h,000h,000h,000h ; lea	edx, [4*edx]
 db 089h,054h,024h,01Ch ; mov	dword ptr [esp + 0x1c], edx
 db 08Dh,090h,0D0h,026h,000h,000h ; lea	edx, [eax + 0x26d0]
 db 089h,054h,024h,014h ; mov	dword ptr [esp + 0x14], edx
 db 031h,0D2h ; xor	edx, edx
 db 0E9h,09Eh,000h,000h,000h ; jmp	0x978 <_tnq_lit+0x978>
 db 066h,00Fh,05Bh,0C6h ; cvtps2dq	xmm0, xmm6
 db 066h,00Fh,05Bh,0CFh ; cvtps2dq	xmm1, xmm7
 db 066h,00Fh,05Bh,0D5h ; cvtps2dq	xmm2, xmm5
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 08Bh,05Ch,024h,008h ; mov	ebx, dword ptr [esp + 0x8]
 db 08Bh,074h,024h,018h ; mov	esi, dword ptr [esp + 0x18]
 db 066h,00Fh,06Bh,090h,090h,028h,000h,000h ; packssdw	xmm2, xmmword ptr [eax + 0x2890]
 db 066h,00Fh,06Bh,0C1h ; packssdw	xmm0, xmm1
 db 066h,00Fh,0EFh,0C9h ; pxor	xmm1, xmm1
 db 066h,00Fh,067h,0C2h ; packuswb	xmm0, xmm2
 db 066h,00Fh,06Fh,0D0h ; movdqa	xmm2, xmm0
 db 066h,00Fh,060h,0C1h ; punpcklbw	xmm0, xmm1
 db 066h,00Fh,068h,0D1h ; punpckhbw	xmm2, xmm1
 db 066h,00Fh,06Fh,0C8h ; movdqa	xmm1, xmm0
 db 066h,00Fh,061h,0CAh ; punpcklwd	xmm1, xmm2
 db 066h,00Fh,069h,0C2h ; punpckhwd	xmm0, xmm2
 db 066h,00Fh,06Fh,094h,024h,080h,000h,000h,000h ; movdqa	xmm2, xmmword ptr [esp + 0x80]
 db 066h,00Fh,067h,0C9h ; packuswb	xmm1, xmm1
 db 066h,00Fh,067h,0C0h ; packuswb	xmm0, xmm0
 db 066h,00Fh,061h,0C8h ; punpcklwd	xmm1, xmm0
 db 066h,00Fh,06Fh,0C1h ; movdqa	xmm0, xmm1
 db 066h,00Fh,06Ah,0CAh ; punpckhdq	xmm1, xmm2
 db 066h,00Fh,062h,0C2h ; punpckldq	xmm0, xmm2
 db 066h,00Fh,070h,0D1h,0EEh ; pshufd	xmm2, xmm1, 0xee
 db 066h,00Fh,0D6h,047h,010h ; movq	qword ptr [edi + 0x10], xmm0
 db 066h,00Fh,070h,0C0h,0EEh ; pshufd	xmm0, xmm0, 0xee
 db 066h,00Fh,0D6h,044h,01Fh,010h ; movq	qword ptr [edi + ebx + 0x10], xmm0
 db 066h,00Fh,0D6h,04Ch,05Fh,010h ; movq	qword ptr [edi + 2*ebx + 0x10], xmm1
 db 08Bh,05Ch,024h,034h ; mov	ebx, dword ptr [esp + 0x34]
 db 066h,00Fh,0D6h,054h,037h,010h ; movq	qword ptr [edi + esi + 0x10], xmm2
 db 083h,0C2h,004h ; add	edx, 0x4
 db 003h,04Ch,024h,024h ; add	ecx, dword ptr [esp + 0x24]
 db 003h,07Ch,024h,01Ch ; add	edi, dword ptr [esp + 0x1c]
 db 03Bh,054h,024h,010h ; cmp	edx, dword ptr [esp + 0x10]
 db 00Fh,083h,015h,001h,000h,000h ; jae	0xa8d <_tnq_lit+0xa8d>
 db 08Bh,074h,024h,050h ; mov	esi, dword ptr [esp + 0x50]
 db 085h,0DBh ; test	ebx, ebx
 db 00Fh,018h,00Ch,031h ; prefetcht0	byte ptr [ecx + esi]
 db 08Bh,074h,024h,020h ; mov	esi, dword ptr [esp + 0x20]
 db 00Fh,018h,00Ch,031h ; prefetcht0	byte ptr [ecx + esi]
 db 074h,006h ; je	0x992 <_tnq_lit+0x992>
 db 083h,03Ch,013h,000h ; cmp	dword ptr [ebx + edx], 0x0
 db 074h,0D1h ; je	0x963 <_tnq_lit+0x963>
 db 083h,07Ch,024h,030h,000h ; cmp	dword ptr [esp + 0x30], 0x0
 db 074h,057h ; je	0x9f0 <_tnq_lit+0x9f0>
 db 089h,07Ch,024h,040h ; mov	dword ptr [esp + 0x40], edi
 db 08Bh,07Ch,024h,02Ch ; mov	edi, dword ptr [esp + 0x2c]
 db 08Bh,074h,024h,00Ch ; mov	esi, dword ptr [esp + 0xc]
 db 08Dh,01Ch,039h ; lea	ebx, [ecx + edi]
 db 0F2h,00Fh,010h,014h,039h ; movsd	xmm2, qword ptr [ecx + edi]
 db 0F3h,00Fh,010h,05Ch,039h,008h ; movss	xmm3, dword ptr [ecx + edi + 0x8]
 db 08Bh,07Ch,024h,040h ; mov	edi, dword ptr [esp + 0x40]
 db 08Dh,004h,033h ; lea	eax, [ebx + esi]
 db 00Fh,016h,014h,01Eh ; movhps	xmm2, qword ptr [esi + ebx]
 db 00Fh,016h,05Ch,01Eh,008h ; movhps	xmm3, qword ptr [esi + ebx + 0x8]
 db 0F2h,00Fh,010h,004h,006h ; movsd	xmm0, qword ptr [esi + eax]
 db 0F3h,00Fh,010h,04Ch,006h,008h ; movss	xmm1, dword ptr [esi + eax + 0x8]
 db 001h,0F0h ; add	eax, esi
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,016h,04Ch,006h,008h ; movhps	xmm1, qword ptr [esi + eax + 0x8]
 db 00Fh,016h,004h,006h ; movhps	xmm0, qword ptr [esi + eax]
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 00Fh,0C6h,0D9h,088h ; shufps	xmm3, xmm1, 0x88
 db 00Fh,028h,0CAh ; movaps	xmm1, xmm2
 db 00Fh,0C6h,0E0h,088h ; shufps	xmm4, xmm0, 0x88
 db 00Fh,0C6h,0C8h,0DDh ; shufps	xmm1, xmm0, 0xdd
 db 0EBh,009h ; jmp	0x9f9 <_tnq_lit+0x9f9>
 db 00Fh,057h,0E4h ; xorps	xmm4, xmm4
 db 00Fh,057h,0C9h ; xorps	xmm1, xmm1
 db 00Fh,057h,0DBh ; xorps	xmm3, xmm3
 db 08Bh,098h,0B0h,028h,000h,000h ; mov	ebx, dword ptr [eax + 0x28b0]
 db 00Fh,028h,0B0h,030h,028h,000h,000h ; movaps	xmm6, xmmword ptr [eax + 0x2830]
 db 00Fh,028h,0A8h,040h,028h,000h,000h ; movaps	xmm5, xmmword ptr [eax + 0x2840]
 db 00Fh,028h,0B8h,050h,028h,000h,000h ; movaps	xmm7, xmmword ptr [eax + 0x2850]
 db 085h,0DBh ; test	ebx, ebx
 db 00Fh,084h,0BEh,0FEh,0FFh,0FFh ; je	0x8da <_tnq_lit+0x8da>
 db 00Fh,028h,080h,060h,001h,000h,000h ; movaps	xmm0, xmmword ptr [eax + 0x160]
 db 08Bh,044h,024h,014h ; mov	eax, dword ptr [esp + 0x14]
 db 00Fh,029h,05Ch,024h,060h ; movaps	xmmword ptr [esp + 0x60], xmm3
 db 00Fh,029h,044h,024h,040h ; movaps	xmmword ptr [esp + 0x40], xmm0
 db 00Fh,028h,090h,060h,0FEh,0FFh,0FFh ; movaps	xmm2, xmmword ptr [eax - 0x1a0]
 db 00Fh,028h,098h,070h,0FEh,0FFh,0FFh ; movaps	xmm3, xmmword ptr [eax - 0x190]
 db 00Fh,028h,0C4h ; movaps	xmm0, xmm4
 db 00Fh,059h,0D4h ; mulps	xmm2, xmm4
 db 00Fh,059h,0D9h ; mulps	xmm3, xmm1
 db 00Fh,028h,0E1h ; movaps	xmm4, xmm1
 db 00Fh,028h,088h,080h,0FEh,0FFh,0FFh ; movaps	xmm1, xmmword ptr [eax - 0x180]
 db 00Fh,059h,04Ch,024h,060h ; mulps	xmm1, xmmword ptr [esp + 0x60]
 db 00Fh,058h,0DAh ; addps	xmm3, xmm2
 db 00Fh,028h,050h,0E0h ; movaps	xmm2, xmmword ptr [eax - 0x20]
 db 00Fh,058h,0CBh ; addps	xmm1, xmm3
 db 00Fh,028h,058h,0F0h ; movaps	xmm3, xmmword ptr [eax - 0x10]
 db 00Fh,05Fh,04Ch,024h,040h ; maxps	xmm1, xmmword ptr [esp + 0x40]
 db 00Fh,059h,0D1h ; mulps	xmm2, xmm1
 db 00Fh,059h,0D9h ; mulps	xmm3, xmm1
 db 00Fh,059h,008h ; mulps	xmm1, xmmword ptr [eax]
 db 083h,0C0h,030h ; add	eax, 0x30
 db 04Bh ; dec	ebx
 db 00Fh,058h,0F9h ; addps	xmm7, xmm1
 db 00Fh,058h,0F2h ; addps	xmm6, xmm2
 db 00Fh,058h,0EBh ; addps	xmm5, xmm3
 db 00Fh,028h,0CCh ; movaps	xmm1, xmm4
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 075h,0A9h ; jne	0xa31 <_tnq_lit+0xa31>
 db 0E9h,04Dh,0FEh,0FFh,0FFh ; jmp	0x8da <_tnq_lit+0x8da>
 db 08Dh,065h,0F4h ; lea	esp, [ebp - 0xc]
 db 05Eh ; pop	esi
 db 05Fh ; pop	edi
 db 05Bh ; pop	ebx
 db 05Dh ; pop	ebp
 db 0C3h ; ret
_tnq_lit endp
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
public _tnq_lit4
_tnq_lit4 proc near
 db 055h ; push	ebp
 db 08Dh,02Ch,024h ; lea	ebp, [esp]
 db 083h,0E4h,0F0h ; and	esp, -0x10
 db 08Dh,064h,024h,0A0h ; lea	esp, [esp - 0x60]
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 00Fh,028h,090h,0F0h,007h,000h,000h ; movaps	xmm2, xmmword ptr [eax + 0x7f0]
 db 00Fh,028h,0A8h,000h,008h,000h,000h ; movaps	xmm5, xmmword ptr [eax + 0x800]
 db 00Fh,028h,088h,010h,008h,000h,000h ; movaps	xmm1, xmmword ptr [eax + 0x810]
 db 083h,0B8h,0B8h,028h,000h,000h,000h ; cmp	dword ptr [eax + 0x28b8], 0x0
 db 08Bh,090h,0B4h,028h,000h,000h ; mov	edx, dword ptr [eax + 0x28b4]
 db 00Fh,028h,0B0h,060h,001h,000h,000h ; movaps	xmm6, xmmword ptr [eax + 0x160]
 db 08Bh,088h,0B0h,028h,000h,000h ; mov	ecx, dword ptr [eax + 0x28b0]
 db 00Fh,084h,0F0h,000h,000h,000h ; je	0x133 <_tnq_lit4+0x133>
 db 00Fh,028h,080h,020h,008h,000h,000h ; movaps	xmm0, xmmword ptr [eax + 0x820]
 db 085h,0D2h ; test	edx, edx
 db 00Fh,028h,098h,070h,028h,000h,000h ; movaps	xmm3, xmmword ptr [eax + 0x2870]
 db 00Fh,028h,0B8h,080h,028h,000h,000h ; movaps	xmm7, xmmword ptr [eax + 0x2880]
 db 00Fh,029h,044h,024h,040h ; movaps	xmmword ptr [esp + 0x40], xmm0
 db 00Fh,028h,080h,060h,028h,000h,000h ; movaps	xmm0, xmmword ptr [eax + 0x2860]
 db 00Fh,084h,0ABh,001h,000h,000h ; je	0x217 <_tnq_lit4+0x217>
 db 085h,0C9h ; test	ecx, ecx
 db 00Fh,084h,003h,002h,000h,000h ; je	0x277 <_tnq_lit4+0x277>
 db 00Fh,028h,0E2h ; movaps	xmm4, xmm2
 db 00Fh,029h,074h,024h,030h ; movaps	xmmword ptr [esp + 0x30], xmm6
 db 00Fh,028h,0F5h ; movaps	xmm6, xmm5
 db 00Fh,029h,06Ch,024h,020h ; movaps	xmmword ptr [esp + 0x20], xmm5
 db 00Fh,029h,014h,024h ; movaps	xmmword ptr [esp], xmm2
 db 00Fh,029h,04Ch,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm1
 db 08Dh,090h,0D0h,026h,000h,000h ; lea	edx, [eax + 0x26d0]
 db 00Fh,059h,0F5h ; mulps	xmm6, xmm5
 db 00Fh,059h,0E2h ; mulps	xmm4, xmm2
 db 00Fh,028h,0E9h ; movaps	xmm5, xmm1
 db 00Fh,059h,0E9h ; mulps	xmm5, xmm1
 db 00Fh,058h,0F4h ; addps	xmm6, xmm4
 db 00Fh,028h,0A0h,080h,008h,000h,000h ; movaps	xmm4, xmmword ptr [eax + 0x880]
 db 00Fh,058h,0EEh ; addps	xmm5, xmm6
 db 00Fh,052h,0D5h ; rsqrtps	xmm2, xmm5
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,028h,0F2h ; movaps	xmm6, xmm2
 db 00Fh,059h,0F5h ; mulps	xmm6, xmm5
 db 00Fh,059h,0F2h ; mulps	xmm6, xmm2
 db 00Fh,028h,0CEh ; movaps	xmm1, xmm6
 db 00Fh,028h,0B0h,060h,024h,000h,000h ; movaps	xmm6, xmmword ptr [eax + 0x2460]
 db 00Fh,05Ch,0F1h ; subps	xmm6, xmm1
 db 00Fh,059h,0F2h ; mulps	xmm6, xmm2
 db 00Fh,028h,054h,024h,010h ; movaps	xmm2, xmmword ptr [esp + 0x10]
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,059h,0EEh ; mulps	xmm5, xmm6
 db 00Fh,05Ch,0E5h ; subps	xmm4, xmm5
 db 00Fh,059h,0E6h ; mulps	xmm4, xmm6
 db 00Fh,058h,0E6h ; addps	xmm4, xmm6
 db 00Fh,028h,0B2h,060h,0FEh,0FFh,0FFh ; movaps	xmm6, xmmword ptr [edx - 0x1a0]
 db 00Fh,028h,08Ah,070h,0FEh,0FFh,0FFh ; movaps	xmm1, xmmword ptr [edx - 0x190]
 db 00Fh,028h,0AAh,080h,0FEh,0FFh,0FFh ; movaps	xmm5, xmmword ptr [edx - 0x180]
 db 00Fh,059h,034h,024h ; mulps	xmm6, xmmword ptr [esp]
 db 00Fh,059h,04Ch,024h,020h ; mulps	xmm1, xmmword ptr [esp + 0x20]
 db 00Fh,059h,0EAh ; mulps	xmm5, xmm2
 db 00Fh,058h,0CEh ; addps	xmm1, xmm6
 db 00Fh,028h,072h,0F0h ; movaps	xmm6, xmmword ptr [edx - 0x10]
 db 00Fh,058h,0E9h ; addps	xmm5, xmm1
 db 00Fh,028h,04Ah,0E0h ; movaps	xmm1, xmmword ptr [edx - 0x20]
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,05Fh,06Ch,024h,030h ; maxps	xmm5, xmmword ptr [esp + 0x30]
 db 00Fh,059h,0CDh ; mulps	xmm1, xmm5
 db 00Fh,059h,0F5h ; mulps	xmm6, xmm5
 db 00Fh,059h,02Ah ; mulps	xmm5, xmmword ptr [edx]
 db 083h,0C2h,030h ; add	edx, 0x30
 db 049h ; dec	ecx
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,058h,0DEh ; addps	xmm3, xmm6
 db 00Fh,058h,0FDh ; addps	xmm7, xmm5
 db 075h,0B1h ; jne	0xdf <_tnq_lit4+0xdf>
 db 0E9h,044h,001h,000h,000h ; jmp	0x277 <_tnq_lit4+0x277>
 db 00Fh,029h,074h,024h,030h ; movaps	xmmword ptr [esp + 0x30], xmm6
 db 00Fh,028h,0FDh ; movaps	xmm7, xmm5
 db 085h,0D2h ; test	edx, edx
 db 00Fh,028h,0A8h,030h,028h,000h,000h ; movaps	xmm5, xmmword ptr [eax + 0x2830]
 db 00Fh,028h,0B0h,040h,028h,000h,000h ; movaps	xmm6, xmmword ptr [eax + 0x2840]
 db 00Fh,028h,0A0h,050h,028h,000h,000h ; movaps	xmm4, xmmword ptr [eax + 0x2850]
 db 00Fh,084h,07Ah,001h,000h,000h ; je	0x2d2 <_tnq_lit4+0x2d2>
 db 085h,0C9h ; test	ecx, ecx
 db 00Fh,084h,0D7h,001h,000h,000h ; je	0x337 <_tnq_lit4+0x337>
 db 00Fh,028h,0C2h ; movaps	xmm0, xmm2
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 00Fh,028h,0CFh ; movaps	xmm1, xmm7
 db 00Fh,029h,014h,024h ; movaps	xmmword ptr [esp], xmm2
 db 00Fh,029h,07Ch,024h,020h ; movaps	xmmword ptr [esp + 0x20], xmm7
 db 08Dh,090h,0D0h,026h,000h,000h ; lea	edx, [eax + 0x26d0]
 db 00Fh,059h,0C2h ; mulps	xmm0, xmm2
 db 00Fh,059h,0CFh ; mulps	xmm1, xmm7
 db 00Fh,029h,05Ch,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm3
 db 00Fh,059h,0DBh ; mulps	xmm3, xmm3
 db 00Fh,058h,0C8h ; addps	xmm1, xmm0
 db 00Fh,028h,080h,080h,008h,000h,000h ; movaps	xmm0, xmmword ptr [eax + 0x880]
 db 00Fh,058h,0D9h ; addps	xmm3, xmm1
 db 00Fh,028h,088h,060h,024h,000h,000h ; movaps	xmm1, xmmword ptr [eax + 0x2460]
 db 00Fh,052h,0D3h ; rsqrtps	xmm2, xmm3
 db 00Fh,059h,0D8h ; mulps	xmm3, xmm0
 db 00Fh,028h,0FAh ; movaps	xmm7, xmm2
 db 00Fh,059h,0FBh ; mulps	xmm7, xmm3
 db 00Fh,059h,0FAh ; mulps	xmm7, xmm2
 db 00Fh,05Ch,0CFh ; subps	xmm1, xmm7
 db 00Fh,059h,0CAh ; mulps	xmm1, xmm2
 db 00Fh,028h,054h,024h,030h ; movaps	xmm2, xmmword ptr [esp + 0x30]
 db 00Fh,059h,0D9h ; mulps	xmm3, xmm1
 db 00Fh,059h,0D9h ; mulps	xmm3, xmm1
 db 00Fh,05Ch,0C3h ; subps	xmm0, xmm3
 db 00Fh,059h,0C1h ; mulps	xmm0, xmm1
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,028h,08Ah,060h,0FEh,0FFh,0FFh ; movaps	xmm1, xmmword ptr [edx - 0x1a0]
 db 00Fh,028h,0BAh,070h,0FEh,0FFh,0FFh ; movaps	xmm7, xmmword ptr [edx - 0x190]
 db 00Fh,028h,09Ah,080h,0FEh,0FFh,0FFh ; movaps	xmm3, xmmword ptr [edx - 0x180]
 db 00Fh,059h,00Ch,024h ; mulps	xmm1, xmmword ptr [esp]
 db 00Fh,059h,07Ch,024h,020h ; mulps	xmm7, xmmword ptr [esp + 0x20]
 db 00Fh,059h,05Ch,024h,010h ; mulps	xmm3, xmmword ptr [esp + 0x10]
 db 00Fh,058h,0F9h ; addps	xmm7, xmm1
 db 00Fh,028h,04Ah,0E0h ; movaps	xmm1, xmmword ptr [edx - 0x20]
 db 00Fh,058h,0DFh ; addps	xmm3, xmm7
 db 00Fh,028h,07Ah,0F0h ; movaps	xmm7, xmmword ptr [edx - 0x10]
 db 00Fh,059h,0D8h ; mulps	xmm3, xmm0
 db 00Fh,05Fh,0DAh ; maxps	xmm3, xmm2
 db 00Fh,059h,0CBh ; mulps	xmm1, xmm3
 db 00Fh,059h,0FBh ; mulps	xmm7, xmm3
 db 00Fh,059h,01Ah ; mulps	xmm3, xmmword ptr [edx]
 db 083h,0C2h,030h ; add	edx, 0x30
 db 049h ; dec	ecx
 db 00Fh,058h,0E9h ; addps	xmm5, xmm1
 db 00Fh,058h,0F7h ; addps	xmm6, xmm7
 db 00Fh,058h,0E3h ; addps	xmm4, xmm3
 db 075h,0B1h ; jne	0x1c3 <_tnq_lit4+0x1c3>
 db 0E9h,020h,001h,000h,000h ; jmp	0x337 <_tnq_lit4+0x337>
 db 085h,0C9h ; test	ecx, ecx
 db 074h,05Ch ; je	0x277 <_tnq_lit4+0x277>
 db 00Fh,029h,014h,024h ; movaps	xmmword ptr [esp], xmm2
 db 08Dh,090h,0D0h,026h,000h,000h ; lea	edx, [eax + 0x26d0]
 db 00Fh,028h,0D5h ; movaps	xmm2, xmm5
 db 00Fh,029h,04Ch,024h,010h ; movaps	xmmword ptr [esp + 0x10], xmm1
 db 00Fh,028h,08Ah,060h,0FEh,0FFh,0FFh ; movaps	xmm1, xmmword ptr [edx - 0x1a0]
 db 00Fh,028h,0AAh,070h,0FEh,0FFh,0FFh ; movaps	xmm5, xmmword ptr [edx - 0x190]
 db 00Fh,028h,0A2h,080h,0FEh,0FFh,0FFh ; movaps	xmm4, xmmword ptr [edx - 0x180]
 db 00Fh,059h,00Ch,024h ; mulps	xmm1, xmmword ptr [esp]
 db 00Fh,059h,0EAh ; mulps	xmm5, xmm2
 db 00Fh,059h,064h,024h,010h ; mulps	xmm4, xmmword ptr [esp + 0x10]
 db 00Fh,058h,0E9h ; addps	xmm5, xmm1
 db 00Fh,028h,04Ah,0E0h ; movaps	xmm1, xmmword ptr [edx - 0x20]
 db 00Fh,058h,0E5h ; addps	xmm4, xmm5
 db 00Fh,028h,06Ah,0F0h ; movaps	xmm5, xmmword ptr [edx - 0x10]
 db 00Fh,05Fh,0E6h ; maxps	xmm4, xmm6
 db 00Fh,059h,0CCh ; mulps	xmm1, xmm4
 db 00Fh,059h,0ECh ; mulps	xmm5, xmm4
 db 00Fh,059h,022h ; mulps	xmm4, xmmword ptr [edx]
 db 083h,0C2h,030h ; add	edx, 0x30
 db 049h ; dec	ecx
 db 00Fh,058h,0C1h ; addps	xmm0, xmm1
 db 00Fh,058h,0DDh ; addps	xmm3, xmm5
 db 00Fh,058h,0FCh ; addps	xmm7, xmm4
 db 075h,0B6h ; jne	0x22d <_tnq_lit4+0x22d>
 db 00Fh,028h,0A0h,030h,01Fh,000h,000h ; movaps	xmm4, xmmword ptr [eax + 0x1f30]
 db 00Fh,028h,04Ch,024h,040h ; movaps	xmm1, xmmword ptr [esp + 0x40]
 db 00Fh,028h,0D4h ; movaps	xmm2, xmm4
 db 00Fh,054h,0D1h ; andps	xmm2, xmm1
 db 00Fh,05Bh,0EAh ; cvtdq2ps	xmm5, xmm2
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 066h,00Fh,072h,0D2h,008h ; psrld	xmm2, 0x8
 db 066h,00Fh,0DBh,0D4h ; pand	xmm2, xmm4
 db 00Fh,05Bh,0F2h ; cvtdq2ps	xmm6, xmm2
 db 00Fh,028h,0D1h ; movaps	xmm2, xmm1
 db 066h,00Fh,072h,0D2h,010h ; psrld	xmm2, 0x10
 db 066h,00Fh,0DBh,0D4h ; pand	xmm2, xmm4
 db 00Fh,05Bh,0E2h ; cvtdq2ps	xmm4, xmm2
 db 00Fh,059h,0E8h ; mulps	xmm5, xmm0
 db 00Fh,058h,0A8h,030h,028h,000h,000h ; addps	xmm5, xmmword ptr [eax + 0x2830]
 db 00Fh,059h,0F3h ; mulps	xmm6, xmm3
 db 00Fh,028h,0D9h ; movaps	xmm3, xmm1
 db 066h,00Fh,072h,0D3h,018h ; psrld	xmm3, 0x18
 db 00Fh,058h,0B0h,040h,028h,000h,000h ; addps	xmm6, xmmword ptr [eax + 0x2840]
 db 00Fh,059h,0E7h ; mulps	xmm4, xmm7
 db 00Fh,058h,0A0h,050h,028h,000h,000h ; addps	xmm4, xmmword ptr [eax + 0x2850]
 db 0EBh,06Dh ; jmp	0x33f <_tnq_lit4+0x33f>
 db 085h,0C9h ; test	ecx, ecx
 db 074h,061h ; je	0x337 <_tnq_lit4+0x337>
 db 00Fh,029h,014h,024h ; movaps	xmmword ptr [esp], xmm2
 db 00Fh,029h,07Ch,024h,020h ; movaps	xmmword ptr [esp + 0x20], xmm7
 db 08Dh,090h,0D0h,026h,000h,000h ; lea	edx, [eax + 0x26d0]
 db 00Fh,028h,0F9h ; movaps	xmm7, xmm1
 db 00Fh,028h,054h,024h,030h ; movaps	xmm2, xmmword ptr [esp + 0x30]
 db 00Fh,028h,08Ah,060h,0FEh,0FFh,0FFh ; movaps	xmm1, xmmword ptr [edx - 0x1a0]
 db 00Fh,028h,09Ah,070h,0FEh,0FFh,0FFh ; movaps	xmm3, xmmword ptr [edx - 0x190]
 db 00Fh,028h,082h,080h,0FEh,0FFh,0FFh ; movaps	xmm0, xmmword ptr [edx - 0x180]
 db 00Fh,059h,00Ch,024h ; mulps	xmm1, xmmword ptr [esp]
 db 00Fh,059h,05Ch,024h,020h ; mulps	xmm3, xmmword ptr [esp + 0x20]
 db 00Fh,059h,0C7h ; mulps	xmm0, xmm7
 db 00Fh,058h,0D9h ; addps	xmm3, xmm1
 db 00Fh,028h,04Ah,0E0h ; movaps	xmm1, xmmword ptr [edx - 0x20]
 db 00Fh,058h,0C3h ; addps	xmm0, xmm3
 db 00Fh,028h,05Ah,0F0h ; movaps	xmm3, xmmword ptr [edx - 0x10]
 db 00Fh,05Fh,0C2h ; maxps	xmm0, xmm2
 db 00Fh,059h,0C8h ; mulps	xmm1, xmm0
 db 00Fh,059h,0D8h ; mulps	xmm3, xmm0
 db 00Fh,059h,002h ; mulps	xmm0, xmmword ptr [edx]
 db 083h,0C2h,030h ; add	edx, 0x30
 db 049h ; dec	ecx
 db 00Fh,058h,0E9h ; addps	xmm5, xmm1
 db 00Fh,058h,0F3h ; addps	xmm6, xmm3
 db 00Fh,058h,0E0h ; addps	xmm4, xmm0
 db 075h,0B6h ; jne	0x2ed <_tnq_lit4+0x2ed>
 db 066h,00Fh,06Fh,098h,090h,028h,000h,000h ; movdqa	xmm3, xmmword ptr [eax + 0x2890]
 db 066h,00Fh,05Bh,0D6h ; cvtps2dq	xmm2, xmm6
 db 066h,00Fh,05Bh,0C5h ; cvtps2dq	xmm0, xmm5
 db 066h,00Fh,05Bh,0CCh ; cvtps2dq	xmm1, xmm4
 db 066h,00Fh,06Bh,0C1h ; packssdw	xmm0, xmm1
 db 066h,00Fh,06Bh,0D3h ; packssdw	xmm2, xmm3
 db 066h,00Fh,0EFh,0C9h ; pxor	xmm1, xmm1
 db 066h,00Fh,067h,0C2h ; packuswb	xmm0, xmm2
 db 066h,00Fh,06Fh,0D0h ; movdqa	xmm2, xmm0
 db 066h,00Fh,060h,0C1h ; punpcklbw	xmm0, xmm1
 db 066h,00Fh,068h,0D1h ; punpckhbw	xmm2, xmm1
 db 066h,00Fh,06Fh,0C8h ; movdqa	xmm1, xmm0
 db 066h,00Fh,061h,0CAh ; punpcklwd	xmm1, xmm2
 db 066h,00Fh,069h,0C2h ; punpckhwd	xmm0, xmm2
 db 066h,00Fh,067h,0C9h ; packuswb	xmm1, xmm1
 db 066h,00Fh,067h,0C0h ; packuswb	xmm0, xmm0
 db 066h,00Fh,061h,0C8h ; punpcklwd	xmm1, xmm0
 db 066h,00Fh,07Fh,088h,040h,008h,000h,000h ; movdqa	xmmword ptr [eax + 0x840], xmm1
 db 089h,0ECh ; mov	esp, ebp
 db 05Dh ; pop	ebp
 db 0C3h ; ret
_tnq_lit4 endp
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh
public _tnq_attr
_tnq_attr proc near
 db 055h ; push	ebp
 db 089h,0E5h ; mov	ebp, esp
 db 053h ; push	ebx
 db 057h ; push	edi
 db 056h ; push	esi
 db 083h,0E4h,0FCh ; and	esp, -0x4
 db 08Dh,064h,024h,0C4h ; lea	esp, [esp - 0x3c]
 db 08Bh,04Dh,008h ; mov	ecx, dword ptr [ebp + 0x8]
 db 08Bh,091h,0E8h,028h,000h,000h ; mov	edx, dword ptr [ecx + 0x28e8]
 db 08Bh,0B1h,044h,029h,000h,000h ; mov	esi, dword ptr [ecx + 0x2944]
 db 08Bh,081h,048h,029h,000h,000h ; mov	eax, dword ptr [ecx + 0x2948]
 db 08Bh,0B9h,0D4h,028h,000h,000h ; mov	edi, dword ptr [ecx + 0x28d4]
 db 08Bh,099h,040h,029h,000h,000h ; mov	ebx, dword ptr [ecx + 0x2940]
 db 089h,054h,024h,02Ch ; mov	dword ptr [esp + 0x2c], edx
 db 083h,0FAh,001h ; cmp	edx, 0x1
 db 08Bh,091h,0BCh,028h,000h,000h ; mov	edx, dword ptr [ecx + 0x28bc]
 db 089h,034h,024h ; mov	dword ptr [esp], esi
 db 08Bh,0B1h,0C0h,028h,000h,000h ; mov	esi, dword ptr [ecx + 0x28c0]
 db 089h,044h,024h,008h ; mov	dword ptr [esp + 0x8], eax
 db 089h,07Ch,024h,028h ; mov	dword ptr [esp + 0x28], edi
 db 089h,054h,024h,004h ; mov	dword ptr [esp + 0x4], edx
 db 08Bh,091h,0C4h,028h,000h,000h ; mov	edx, dword ptr [ecx + 0x28c4]
 db 089h,074h,024h,018h ; mov	dword ptr [esp + 0x18], esi
 db 08Bh,0B1h,0D0h,028h,000h,000h ; mov	esi, dword ptr [ecx + 0x28d0]
 db 089h,054h,024h,014h ; mov	dword ptr [esp + 0x14], edx
 db 08Bh,091h,0D8h,028h,000h,000h ; mov	edx, dword ptr [ecx + 0x28d8]
 db 089h,074h,024h,030h ; mov	dword ptr [esp + 0x30], esi
 db 08Bh,0B1h,0DCh,028h,000h,000h ; mov	esi, dword ptr [ecx + 0x28dc]
 db 089h,054h,024h,024h ; mov	dword ptr [esp + 0x24], edx
 db 08Bh,091h,0E0h,028h,000h,000h ; mov	edx, dword ptr [ecx + 0x28e0]
 db 089h,054h,024h,020h ; mov	dword ptr [esp + 0x20], edx
 db 08Bh,091h,0F0h,028h,000h,000h ; mov	edx, dword ptr [ecx + 0x28f0]
 db 08Bh,089h,0F4h,028h,000h,000h ; mov	ecx, dword ptr [ecx + 0x28f4]
 db 089h,04Ch,024h,010h ; mov	dword ptr [esp + 0x10], ecx
 db 075h,074h ; jne	0x108 <_tnq_attr+0x108>
 db 083h,0FAh,0FFh ; cmp	edx, -0x1
 db 074h,06Fh ; je	0x108 <_tnq_attr+0x108>
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 083h,0B8h,000h,029h,000h,000h,000h ; cmp	dword ptr [eax + 0x2900], 0x0
 db 00Fh,084h,0A4h,000h,000h,000h ; je	0x14d <_tnq_attr+0x14d>
 db 083h,0B8h,010h,029h,000h,000h,006h ; cmp	dword ptr [eax + 0x2910], 0x6
 db 00Fh,094h,0C0h ; sete	al
 db 031h,0C9h ; xor	ecx, ecx
 db 084h,0C0h ; test	al, al
 db 00Fh,084h,090h,000h,000h,000h ; je	0x14d <_tnq_attr+0x14d>
 db 083h,0FEh,002h ; cmp	esi, 0x2
 db 00Fh,083h,087h,000h,000h,000h ; jae	0x14d <_tnq_attr+0x14d>
 db 08Bh,044h,024h,030h ; mov	eax, dword ptr [esp + 0x30]
 db 089h,054h,024h,01Ch ; mov	dword ptr [esp + 0x1c], edx
 db 083h,0F8h,002h ; cmp	eax, 0x2
 db 00Fh,084h,041h,002h,000h,000h ; je	0x318 <_tnq_attr+0x318>
 db 083h,0F8h,001h ; cmp	eax, 0x1
 db 00Fh,085h,005h,003h,000h,000h ; jne	0x3e5 <_tnq_attr+0x3e5>
 db 085h,0F6h ; test	esi, esi
 db 00Fh,084h,0C3h,003h,000h,000h ; je	0x4ab <_tnq_attr+0x4ab>
 db 084h,0C9h ; test	cl, cl
 db 00Fh,084h,0E1h,005h,000h,000h ; je	0x6d1 <_tnq_attr+0x6d1>
 db 083h,07Ch,024h,004h,000h ; cmp	dword ptr [esp + 0x4], 0x0
 db 00Fh,084h,07Dh,008h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 08Bh,074h,024h,008h ; mov	esi, dword ptr [esp + 0x8]
 db 083h,004h,024h,024h ; add	dword ptr [esp], 0x24
 db 0E9h,0FAh,001h,000h,000h ; jmp	0x302 <_tnq_attr+0x302>
 db 083h,07Ch,024h,02Ch,002h ; cmp	dword ptr [esp + 0x2c], 0x2
 db 075h,03Eh ; jne	0x14d <_tnq_attr+0x14d>
 db 083h,0FAh,0FFh ; cmp	edx, -0x1
 db 074h,039h ; je	0x14d <_tnq_attr+0x14d>
 db 083h,07Ch,024h,010h,0FFh ; cmp	dword ptr [esp + 0x10], -0x1
 db 074h,032h ; je	0x14d <_tnq_attr+0x14d>
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 083h,0B8h,000h,029h,000h,000h,000h ; cmp	dword ptr [eax + 0x2900], 0x0
 db 074h,026h ; je	0x14d <_tnq_attr+0x14d>
 db 083h,0B8h,004h,029h,000h,000h,000h ; cmp	dword ptr [eax + 0x2904], 0x0
 db 074h,01Dh ; je	0x14d <_tnq_attr+0x14d>
 db 083h,0B8h,010h,029h,000h,000h,006h ; cmp	dword ptr [eax + 0x2910], 0x6
 db 075h,014h ; jne	0x14d <_tnq_attr+0x14d>
 db 083h,0B8h,014h,029h,000h,000h,008h ; cmp	dword ptr [eax + 0x2914], 0x8
 db 00Fh,094h,0C0h ; sete	al
 db 089h,0C1h ; mov	ecx, eax
 db 084h,0C0h ; test	al, al
 db 00Fh,085h,070h,0FFh,0FFh,0FFh ; jne	0xbd <_tnq_attr+0xbd>
 db 083h,07Ch,024h,004h,000h ; cmp	dword ptr [esp + 0x4], 0x0
 db 089h,074h,024h,038h ; mov	dword ptr [esp + 0x38], esi
 db 00Fh,084h,01Ch,008h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 0C7h,044h,024h,00Ch,000h,000h,000h,000h ; mov	dword ptr [esp + 0xc], 0x0
 db 005h,010h,029h,000h,000h ; add	eax, 0x2910
 db 089h,044h,024h,034h ; mov	dword ptr [esp + 0x34], eax
 db 0EBh,020h ; jmp	0x192 <_tnq_attr+0x192>
 db 08Bh,04Ch,024h,00Ch ; mov	ecx, dword ptr [esp + 0xc]
 db 08Bh,044h,024h,014h ; mov	eax, dword ptr [esp + 0x14]
 db 003h,05Ch,024h,018h ; add	ebx, dword ptr [esp + 0x18]
 db 041h ; inc	ecx
 db 001h,004h,024h ; add	dword ptr [esp], eax
 db 03Bh,04Ch,024h,004h ; cmp	ecx, dword ptr [esp + 0x4]
 db 089h,0C8h ; mov	eax, ecx
 db 089h,04Ch,024h,00Ch ; mov	dword ptr [esp + 0xc], ecx
 db 00Fh,084h,0E6h,007h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,07Ch,024h,008h,000h ; cmp	dword ptr [esp + 0x8], 0x0
 db 074h,00Eh ; je	0x1a7 <_tnq_attr+0x1a7>
 db 08Bh,044h,024h,008h ; mov	eax, dword ptr [esp + 0x8]
 db 08Bh,04Ch,024h,00Ch ; mov	ecx, dword ptr [esp + 0xc]
 db 080h,03Ch,008h,000h ; cmp	byte ptr [eax + ecx], 0x0
 db 074h,0CBh ; je	0x172 <_tnq_attr+0x172>
 db 08Bh,04Ch,024h,030h ; mov	ecx, dword ptr [esp + 0x30]
 db 08Bh,044h,024h,028h ; mov	eax, dword ptr [esp + 0x28]
 db 08Bh,054h,024h,038h ; mov	edx, dword ptr [esp + 0x38]
 db 083h,0F9h,002h ; cmp	ecx, 0x2
 db 074h,00Ch ; je	0x1c4 <_tnq_attr+0x1c4>
 db 083h,0F9h,001h ; cmp	ecx, 0x1
 db 075h,00Dh ; jne	0x1ca <_tnq_attr+0x1ca>
 db 08Bh,044h,024h,024h ; mov	eax, dword ptr [esp + 0x24]
 db 08Bh,004h,003h ; mov	eax, dword ptr [ebx + eax]
 db 08Bh,00Ch,024h ; mov	ecx, dword ptr [esp]
 db 089h,041h,010h ; mov	dword ptr [ecx + 0x10], eax
 db 083h,0FAh,001h ; cmp	edx, 0x1
 db 08Bh,044h,024h,020h ; mov	eax, dword ptr [esp + 0x20]
 db 074h,02Bh ; je	0x1fe <_tnq_attr+0x1fe>
 db 083h,0FAh,003h ; cmp	edx, 0x3
 db 074h,01Ah ; je	0x1f2 <_tnq_attr+0x1f2>
 db 083h,0FAh,002h ; cmp	edx, 0x2
 db 075h,027h ; jne	0x204 <_tnq_attr+0x204>
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 0B9h,000h,000h,000h,0FFh ; mov	ecx, 0xff000000
 db 08Bh,080h,0E4h,028h,000h,000h ; mov	eax, dword ptr [eax + 0x28e4]
 db 08Bh,004h,003h ; mov	eax, dword ptr [ebx + eax]
 db 009h,0C8h ; or	eax, ecx
 db 0EBh,00Ch ; jmp	0x1fe <_tnq_attr+0x1fe>
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 08Bh,080h,0E4h,028h,000h,000h ; mov	eax, dword ptr [eax + 0x28e4]
 db 08Bh,004h,003h ; mov	eax, dword ptr [ebx + eax]
 db 08Bh,00Ch,024h ; mov	ecx, dword ptr [esp]
 db 089h,041h,014h ; mov	dword ptr [ecx + 0x14], eax
 db 083h,07Ch,024h,02Ch,000h ; cmp	dword ptr [esp + 0x2c], 0x0
 db 00Fh,084h,063h,0FFh,0FFh,0FFh ; je	0x172 <_tnq_attr+0x172>
 db 08Bh,054h,024h,034h ; mov	edx, dword ptr [esp + 0x34]
 db 08Bh,07Ch,024h,02Ch ; mov	edi, dword ptr [esp + 0x2c]
 db 089h,05Ch,024h,01Ch ; mov	dword ptr [esp + 0x1c], ebx
 db 0EBh,03Dh ; jmp	0x25a <_tnq_attr+0x25a>
 db 031h,0F6h ; xor	esi, esi
 db 089h,0F3h ; mov	ebx, esi
 db 089h,0C8h ; mov	eax, ecx
 db 089h,04Ch,024h,010h ; mov	dword ptr [esp + 0x10], ecx
 db 08Bh,00Ch,024h ; mov	ecx, dword ptr [esp]
 db 0F7h,0D3h ; not	ebx
 db 0F7h,0D0h ; not	eax
 db 081h,0E3h,000h,000h,080h,07Fh ; and	ebx, 0x7f800000
 db 00Fh,045h,0DEh ; cmovne	ebx, esi
 db 08Bh,032h ; mov	esi, dword ptr [edx]
 db 025h,000h,000h,080h,07Fh ; and	eax, 0x7f800000
 db 089h,01Ch,0B1h ; mov	dword ptr [ecx + 4*esi], ebx
 db 00Fh,045h,044h,024h,010h ; cmovne	eax, dword ptr [esp + 0x10]
 db 08Bh,05Ch,024h,01Ch ; mov	ebx, dword ptr [esp + 0x1c]
 db 08Bh,032h ; mov	esi, dword ptr [edx]
 db 083h,0C2h,004h ; add	edx, 0x4
 db 04Fh ; dec	edi
 db 089h,044h,0B1h,004h ; mov	dword ptr [ecx + 4*esi + 0x4], eax
 db 00Fh,084h,018h,0FFh,0FFh,0FFh ; je	0x172 <_tnq_attr+0x172>
 db 08Bh,042h,0E0h ; mov	eax, dword ptr [edx - 0x20]
 db 031h,0C9h ; xor	ecx, ecx
 db 083h,0F8h,0FFh ; cmp	eax, -0x1
 db 074h,0B9h ; je	0x21d <_tnq_attr+0x21d>
 db 083h,07Ah,0F0h,000h ; cmp	dword ptr [edx - 0x10], 0x0
 db 08Bh,034h,003h ; mov	esi, dword ptr [ebx + eax]
 db 074h,0B2h ; je	0x21f <_tnq_attr+0x21f>
 db 08Bh,04Ch,003h,004h ; mov	ecx, dword ptr [ebx + eax + 0x4]
 db 0EBh,0ACh ; jmp	0x21f <_tnq_attr+0x21f>
 db 08Bh,044h,024h,024h ; mov	eax, dword ptr [esp + 0x24]
 db 08Bh,00Ch,003h ; mov	ecx, dword ptr [ebx + eax]
 db 08Dh,003h ; lea	eax, [ebx]
 db 08Bh,01Ch,024h ; mov	ebx, dword ptr [esp]
 db 089h,04Bh,0ECh ; mov	dword ptr [ebx - 0x14], ecx
 db 08Bh,04Ch,024h,020h ; mov	ecx, dword ptr [esp + 0x20]
 db 089h,04Bh,0F0h ; mov	dword ptr [ebx - 0x10], ecx
 db 08Bh,03Ch,010h ; mov	edi, dword ptr [eax + edx]
 db 08Bh,04Ch,010h,004h ; mov	ecx, dword ptr [eax + edx + 0x4]
 db 089h,0FAh ; mov	edx, edi
 db 089h,07Ch,024h,00Ch ; mov	dword ptr [esp + 0xc], edi
 db 089h,0CFh ; mov	edi, ecx
 db 0F7h,0D2h ; not	edx
 db 0F7h,0D7h ; not	edi
 db 081h,0E2h,000h,000h,080h,07Fh ; and	edx, 0x7f800000
 db 00Fh,045h,054h,024h,00Ch ; cmovne	edx, dword ptr [esp + 0xc]
 db 081h,0E7h,000h,000h,080h,07Fh ; and	edi, 0x7f800000
 db 00Fh,045h,0F9h ; cmovne	edi, ecx
 db 089h,053h,0F4h ; mov	dword ptr [ebx - 0xc], edx
 db 08Bh,054h,024h,010h ; mov	edx, dword ptr [esp + 0x10]
 db 089h,07Bh,0F8h ; mov	dword ptr [ebx - 0x8], edi
 db 08Bh,03Ch,010h ; mov	edi, dword ptr [eax + edx]
 db 08Bh,04Ch,010h,004h ; mov	ecx, dword ptr [eax + edx + 0x4]
 db 089h,0FAh ; mov	edx, edi
 db 089h,07Ch,024h,00Ch ; mov	dword ptr [esp + 0xc], edi
 db 089h,0CFh ; mov	edi, ecx
 db 0F7h,0D2h ; not	edx
 db 0F7h,0D7h ; not	edi
 db 081h,0E2h,000h,000h,080h,07Fh ; and	edx, 0x7f800000
 db 00Fh,045h,054h,024h,00Ch ; cmovne	edx, dword ptr [esp + 0xc]
 db 081h,0E7h,000h,000h,080h,07Fh ; and	edi, 0x7f800000
 db 00Fh,045h,0F9h ; cmovne	edi, ecx
 db 089h,053h,0FCh ; mov	dword ptr [ebx - 0x4], edx
 db 08Bh,054h,024h,01Ch ; mov	edx, dword ptr [esp + 0x1c]
 db 089h,03Bh ; mov	dword ptr [ebx], edi
 db 089h,0C3h ; mov	ebx, eax
 db 08Bh,04Ch,024h,014h ; mov	ecx, dword ptr [esp + 0x14]
 db 003h,05Ch,024h,018h ; add	ebx, dword ptr [esp + 0x18]
 db 046h ; inc	esi
 db 001h,00Ch,024h ; add	dword ptr [esp], ecx
 db 0FFh,04Ch,024h,004h ; dec	dword ptr [esp + 0x4]
 db 00Fh,084h,076h,006h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,07Ch,024h,008h,000h ; cmp	dword ptr [esp + 0x8], 0x0
 db 00Fh,084h,066h,0FFh,0FFh,0FFh ; je	0x273 <_tnq_attr+0x273>
 db 080h,03Eh,000h ; cmp	byte ptr [esi], 0x0
 db 00Fh,085h,05Dh,0FFh,0FFh,0FFh ; jne	0x273 <_tnq_attr+0x273>
 db 0EBh,0D4h ; jmp	0x2ec <_tnq_attr+0x2ec>
 db 085h,0F6h ; test	esi, esi
 db 00Fh,084h,049h,002h,000h,000h ; je	0x569 <_tnq_attr+0x569>
 db 084h,0C9h ; test	cl, cl
 db 00Fh,084h,02Ah,004h,000h,000h ; je	0x752 <_tnq_attr+0x752>
 db 083h,07Ch,024h,004h,000h ; cmp	dword ptr [esp + 0x4], 0x0
 db 00Fh,084h,045h,006h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,0C3h,004h ; add	ebx, 0x4
 db 08Bh,074h,024h,008h ; mov	esi, dword ptr [esp + 0x8]
 db 083h,004h,024h,024h ; add	dword ptr [esp], 0x24
 db 0E9h,08Ch,000h,000h,000h ; jmp	0x3cf <_tnq_attr+0x3cf>
 db 089h,0D8h ; mov	eax, ebx
 db 08Bh,01Ch,024h ; mov	ebx, dword ptr [esp]
 db 08Bh,04Ch,024h,028h ; mov	ecx, dword ptr [esp + 0x28]
 db 089h,04Bh,0ECh ; mov	dword ptr [ebx - 0x14], ecx
 db 08Bh,04Ch,024h,020h ; mov	ecx, dword ptr [esp + 0x20]
 db 089h,04Bh,0F0h ; mov	dword ptr [ebx - 0x10], ecx
 db 08Bh,07Ch,010h,0FCh ; mov	edi, dword ptr [eax + edx - 0x4]
 db 08Bh,00Ch,010h ; mov	ecx, dword ptr [eax + edx]
 db 089h,0FAh ; mov	edx, edi
 db 089h,07Ch,024h,00Ch ; mov	dword ptr [esp + 0xc], edi
 db 089h,0CFh ; mov	edi, ecx
 db 0F7h,0D2h ; not	edx
 db 0F7h,0D7h ; not	edi
 db 081h,0E2h,000h,000h,080h,07Fh ; and	edx, 0x7f800000
 db 00Fh,045h,054h,024h,00Ch ; cmovne	edx, dword ptr [esp + 0xc]
 db 081h,0E7h,000h,000h,080h,07Fh ; and	edi, 0x7f800000
 db 00Fh,045h,0F9h ; cmovne	edi, ecx
 db 089h,053h,0F4h ; mov	dword ptr [ebx - 0xc], edx
 db 08Bh,054h,024h,010h ; mov	edx, dword ptr [esp + 0x10]
 db 089h,07Bh,0F8h ; mov	dword ptr [ebx - 0x8], edi
 db 08Bh,07Ch,010h,0FCh ; mov	edi, dword ptr [eax + edx - 0x4]
 db 08Bh,00Ch,010h ; mov	ecx, dword ptr [eax + edx]
 db 089h,0FAh ; mov	edx, edi
 db 089h,07Ch,024h,00Ch ; mov	dword ptr [esp + 0xc], edi
 db 089h,0CFh ; mov	edi, ecx
 db 0F7h,0D2h ; not	edx
 db 0F7h,0D7h ; not	edi
 db 081h,0E2h,000h,000h,080h,07Fh ; and	edx, 0x7f800000
 db 00Fh,045h,054h,024h,00Ch ; cmovne	edx, dword ptr [esp + 0xc]
 db 081h,0E7h,000h,000h,080h,07Fh ; and	edi, 0x7f800000
 db 00Fh,045h,0F9h ; cmovne	edi, ecx
 db 089h,053h,0FCh ; mov	dword ptr [ebx - 0x4], edx
 db 08Bh,054h,024h,01Ch ; mov	edx, dword ptr [esp + 0x1c]
 db 089h,03Bh ; mov	dword ptr [ebx], edi
 db 089h,0C3h ; mov	ebx, eax
 db 08Bh,04Ch,024h,014h ; mov	ecx, dword ptr [esp + 0x14]
 db 003h,05Ch,024h,018h ; add	ebx, dword ptr [esp + 0x18]
 db 046h ; inc	esi
 db 001h,00Ch,024h ; add	dword ptr [esp], ecx
 db 0FFh,04Ch,024h,004h ; dec	dword ptr [esp + 0x4]
 db 00Fh,084h,0A9h,005h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,07Ch,024h,008h,000h ; cmp	dword ptr [esp + 0x8], 0x0
 db 00Fh,084h,069h,0FFh,0FFh,0FFh ; je	0x343 <_tnq_attr+0x343>
 db 080h,03Eh,000h ; cmp	byte ptr [esi], 0x0
 db 00Fh,085h,060h,0FFh,0FFh,0FFh ; jne	0x343 <_tnq_attr+0x343>
 db 0EBh,0D4h ; jmp	0x3b9 <_tnq_attr+0x3b9>
 db 085h,0F6h ; test	esi, esi
 db 00Fh,084h,03Ah,002h,000h,000h ; je	0x627 <_tnq_attr+0x627>
 db 084h,0C9h ; test	cl, cl
 db 00Fh,084h,0D3h,003h,000h,000h ; je	0x7c8 <_tnq_attr+0x7c8>
 db 083h,07Ch,024h,004h,000h ; cmp	dword ptr [esp + 0x4], 0x0
 db 00Fh,084h,078h,005h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,0C3h,004h ; add	ebx, 0x4
 db 08Bh,074h,024h,008h ; mov	esi, dword ptr [esp + 0x8]
 db 083h,004h,024h,024h ; add	dword ptr [esp], 0x24
 db 0E9h,085h,000h,000h,000h ; jmp	0x495 <_tnq_attr+0x495>
 db 08Dh,003h ; lea	eax, [ebx]
 db 08Bh,01Ch,024h ; mov	ebx, dword ptr [esp]
 db 08Bh,04Ch,024h,020h ; mov	ecx, dword ptr [esp + 0x20]
 db 089h,04Bh,0F0h ; mov	dword ptr [ebx - 0x10], ecx
 db 08Bh,07Ch,010h,0FCh ; mov	edi, dword ptr [eax + edx - 0x4]
 db 08Bh,00Ch,010h ; mov	ecx, dword ptr [eax + edx]
 db 089h,0FAh ; mov	edx, edi
 db 089h,07Ch,024h,00Ch ; mov	dword ptr [esp + 0xc], edi
 db 089h,0CFh ; mov	edi, ecx
 db 0F7h,0D2h ; not	edx
 db 0F7h,0D7h ; not	edi
 db 081h,0E2h,000h,000h,080h,07Fh ; and	edx, 0x7f800000
 db 00Fh,045h,054h,024h,00Ch ; cmovne	edx, dword ptr [esp + 0xc]
 db 081h,0E7h,000h,000h,080h,07Fh ; and	edi, 0x7f800000
 db 00Fh,045h,0F9h ; cmovne	edi, ecx
 db 089h,053h,0F4h ; mov	dword ptr [ebx - 0xc], edx
 db 08Bh,054h,024h,010h ; mov	edx, dword ptr [esp + 0x10]
 db 089h,07Bh,0F8h ; mov	dword ptr [ebx - 0x8], edi
 db 08Bh,07Ch,010h,0FCh ; mov	edi, dword ptr [eax + edx - 0x4]
 db 08Bh,00Ch,010h ; mov	ecx, dword ptr [eax + edx]
 db 089h,0FAh ; mov	edx, edi
 db 089h,07Ch,024h,00Ch ; mov	dword ptr [esp + 0xc], edi
 db 089h,0CFh ; mov	edi, ecx
 db 0F7h,0D2h ; not	edx
 db 0F7h,0D7h ; not	edi
 db 081h,0E2h,000h,000h,080h,07Fh ; and	edx, 0x7f800000
 db 00Fh,045h,054h,024h,00Ch ; cmovne	edx, dword ptr [esp + 0xc]
 db 081h,0E7h,000h,000h,080h,07Fh ; and	edi, 0x7f800000
 db 00Fh,045h,0F9h ; cmovne	edi, ecx
 db 089h,053h,0FCh ; mov	dword ptr [ebx - 0x4], edx
 db 08Bh,054h,024h,01Ch ; mov	edx, dword ptr [esp + 0x1c]
 db 089h,03Bh ; mov	dword ptr [ebx], edi
 db 089h,0C3h ; mov	ebx, eax
 db 08Bh,04Ch,024h,014h ; mov	ecx, dword ptr [esp + 0x14]
 db 003h,05Ch,024h,018h ; add	ebx, dword ptr [esp + 0x18]
 db 046h ; inc	esi
 db 001h,00Ch,024h ; add	dword ptr [esp], ecx
 db 0FFh,04Ch,024h,004h ; dec	dword ptr [esp + 0x4]
 db 00Fh,084h,0E3h,004h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,07Ch,024h,008h,000h ; cmp	dword ptr [esp + 0x8], 0x0
 db 00Fh,084h,070h,0FFh,0FFh,0FFh ; je	0x410 <_tnq_attr+0x410>
 db 080h,03Eh,000h ; cmp	byte ptr [esi], 0x0
 db 00Fh,085h,067h,0FFh,0FFh,0FFh ; jne	0x410 <_tnq_attr+0x410>
 db 0EBh,0D4h ; jmp	0x47f <_tnq_attr+0x47f>
 db 084h,0C9h ; test	cl, cl
 db 00Fh,084h,084h,003h,000h,000h ; je	0x837 <_tnq_attr+0x837>
 db 083h,07Ch,024h,004h,000h ; cmp	dword ptr [esp + 0x4], 0x0
 db 00Fh,084h,0BAh,004h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 08Bh,074h,024h,008h ; mov	esi, dword ptr [esp + 0x8]
 db 083h,004h,024h,024h ; add	dword ptr [esp], 0x24
 db 0E9h,088h,000h,000h,000h ; jmp	0x553 <_tnq_attr+0x553>
 db 08Bh,044h,024h,024h ; mov	eax, dword ptr [esp + 0x24]
 db 08Bh,00Ch,003h ; mov	ecx, dword ptr [ebx + eax]
 db 08Dh,003h ; lea	eax, [ebx]
 db 08Bh,01Ch,024h ; mov	ebx, dword ptr [esp]
 db 089h,04Bh,0ECh ; mov	dword ptr [ebx - 0x14], ecx
 db 08Bh,03Ch,010h ; mov	edi, dword ptr [eax + edx]
 db 08Bh,04Ch,010h,004h ; mov	ecx, dword ptr [eax + edx + 0x4]
 db 089h,0FAh ; mov	edx, edi
 db 089h,07Ch,024h,00Ch ; mov	dword ptr [esp + 0xc], edi
 db 089h,0CFh ; mov	edi, ecx
 db 0F7h,0D2h ; not	edx
 db 0F7h,0D7h ; not	edi
 db 081h,0E2h,000h,000h,080h,07Fh ; and	edx, 0x7f800000
 db 00Fh,045h,054h,024h,00Ch ; cmovne	edx, dword ptr [esp + 0xc]
 db 081h,0E7h,000h,000h,080h,07Fh ; and	edi, 0x7f800000
 db 00Fh,045h,0F9h ; cmovne	edi, ecx
 db 089h,053h,0F4h ; mov	dword ptr [ebx - 0xc], edx
 db 08Bh,054h,024h,010h ; mov	edx, dword ptr [esp + 0x10]
 db 089h,07Bh,0F8h ; mov	dword ptr [ebx - 0x8], edi
 db 08Bh,03Ch,010h ; mov	edi, dword ptr [eax + edx]
 db 08Bh,04Ch,010h,004h ; mov	ecx, dword ptr [eax + edx + 0x4]
 db 089h,0FAh ; mov	edx, edi
 db 089h,07Ch,024h,00Ch ; mov	dword ptr [esp + 0xc], edi
 db 089h,0CFh ; mov	edi, ecx
 db 0F7h,0D2h ; not	edx
 db 0F7h,0D7h ; not	edi
 db 081h,0E2h,000h,000h,080h,07Fh ; and	edx, 0x7f800000
 db 00Fh,045h,054h,024h,00Ch ; cmovne	edx, dword ptr [esp + 0xc]
 db 081h,0E7h,000h,000h,080h,07Fh ; and	edi, 0x7f800000
 db 00Fh,045h,0F9h ; cmovne	edi, ecx
 db 089h,053h,0FCh ; mov	dword ptr [ebx - 0x4], edx
 db 08Bh,054h,024h,01Ch ; mov	edx, dword ptr [esp + 0x1c]
 db 089h,03Bh ; mov	dword ptr [ebx], edi
 db 089h,0C3h ; mov	ebx, eax
 db 08Bh,04Ch,024h,014h ; mov	ecx, dword ptr [esp + 0x14]
 db 003h,05Ch,024h,018h ; add	ebx, dword ptr [esp + 0x18]
 db 046h ; inc	esi
 db 001h,00Ch,024h ; add	dword ptr [esp], ecx
 db 0FFh,04Ch,024h,004h ; dec	dword ptr [esp + 0x4]
 db 00Fh,084h,025h,004h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,07Ch,024h,008h,000h ; cmp	dword ptr [esp + 0x8], 0x0
 db 00Fh,084h,06Dh,0FFh,0FFh,0FFh ; je	0x4cb <_tnq_attr+0x4cb>
 db 080h,03Eh,000h ; cmp	byte ptr [esi], 0x0
 db 00Fh,085h,064h,0FFh,0FFh,0FFh ; jne	0x4cb <_tnq_attr+0x4cb>
 db 0EBh,0D4h ; jmp	0x53d <_tnq_attr+0x53d>
 db 084h,0C9h ; test	cl, cl
 db 00Fh,084h,040h,003h,000h,000h ; je	0x8b1 <_tnq_attr+0x8b1>
 db 083h,07Ch,024h,004h,000h ; cmp	dword ptr [esp + 0x4], 0x0
 db 00Fh,084h,0FCh,003h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,0C3h,004h ; add	ebx, 0x4
 db 08Bh,074h,024h,008h ; mov	esi, dword ptr [esp + 0x8]
 db 083h,004h,024h,024h ; add	dword ptr [esp], 0x24
 db 0E9h,085h,000h,000h,000h ; jmp	0x611 <_tnq_attr+0x611>
 db 08Dh,003h ; lea	eax, [ebx]
 db 08Bh,01Ch,024h ; mov	ebx, dword ptr [esp]
 db 08Bh,04Ch,024h,028h ; mov	ecx, dword ptr [esp + 0x28]
 db 089h,04Bh,0ECh ; mov	dword ptr [ebx - 0x14], ecx
 db 08Bh,07Ch,010h,0FCh ; mov	edi, dword ptr [eax + edx - 0x4]
 db 08Bh,00Ch,010h ; mov	ecx, dword ptr [eax + edx]
 db 089h,0FAh ; mov	edx, edi
 db 089h,07Ch,024h,00Ch ; mov	dword ptr [esp + 0xc], edi
 db 089h,0CFh ; mov	edi, ecx
 db 0F7h,0D2h ; not	edx
 db 0F7h,0D7h ; not	edi
 db 081h,0E2h,000h,000h,080h,07Fh ; and	edx, 0x7f800000
 db 00Fh,045h,054h,024h,00Ch ; cmovne	edx, dword ptr [esp + 0xc]
 db 081h,0E7h,000h,000h,080h,07Fh ; and	edi, 0x7f800000
 db 00Fh,045h,0F9h ; cmovne	edi, ecx
 db 089h,053h,0F4h ; mov	dword ptr [ebx - 0xc], edx
 db 08Bh,054h,024h,010h ; mov	edx, dword ptr [esp + 0x10]
 db 089h,07Bh,0F8h ; mov	dword ptr [ebx - 0x8], edi
 db 08Bh,07Ch,010h,0FCh ; mov	edi, dword ptr [eax + edx - 0x4]
 db 08Bh,00Ch,010h ; mov	ecx, dword ptr [eax + edx]
 db 089h,0FAh ; mov	edx, edi
 db 089h,07Ch,024h,00Ch ; mov	dword ptr [esp + 0xc], edi
 db 089h,0CFh ; mov	edi, ecx
 db 0F7h,0D2h ; not	edx
 db 0F7h,0D7h ; not	edi
 db 081h,0E2h,000h,000h,080h,07Fh ; and	edx, 0x7f800000
 db 00Fh,045h,054h,024h,00Ch ; cmovne	edx, dword ptr [esp + 0xc]
 db 081h,0E7h,000h,000h,080h,07Fh ; and	edi, 0x7f800000
 db 00Fh,045h,0F9h ; cmovne	edi, ecx
 db 089h,053h,0FCh ; mov	dword ptr [ebx - 0x4], edx
 db 08Bh,054h,024h,01Ch ; mov	edx, dword ptr [esp + 0x1c]
 db 089h,03Bh ; mov	dword ptr [ebx], edi
 db 089h,0C3h ; mov	ebx, eax
 db 08Bh,04Ch,024h,014h ; mov	ecx, dword ptr [esp + 0x14]
 db 003h,05Ch,024h,018h ; add	ebx, dword ptr [esp + 0x18]
 db 046h ; inc	esi
 db 001h,00Ch,024h ; add	dword ptr [esp], ecx
 db 0FFh,04Ch,024h,004h ; dec	dword ptr [esp + 0x4]
 db 00Fh,084h,067h,003h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,07Ch,024h,008h,000h ; cmp	dword ptr [esp + 0x8], 0x0
 db 00Fh,084h,070h,0FFh,0FFh,0FFh ; je	0x58c <_tnq_attr+0x58c>
 db 080h,03Eh,000h ; cmp	byte ptr [esi], 0x0
 db 00Fh,085h,067h,0FFh,0FFh,0FFh ; jne	0x58c <_tnq_attr+0x58c>
 db 0EBh,0D4h ; jmp	0x5fb <_tnq_attr+0x5fb>
 db 084h,0C9h ; test	cl, cl
 db 00Fh,084h,0EDh,002h,000h,000h ; je	0x91c <_tnq_attr+0x91c>
 db 083h,07Ch,024h,004h,000h ; cmp	dword ptr [esp + 0x4], 0x0
 db 00Fh,084h,03Eh,003h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,0C3h,004h ; add	ebx, 0x4
 db 08Bh,044h,024h,008h ; mov	eax, dword ptr [esp + 0x8]
 db 083h,004h,024h,024h ; add	dword ptr [esp], 0x24
 db 0EBh,078h ; jmp	0x6bf <_tnq_attr+0x6bf>
 db 08Bh,074h,013h,0FCh ; mov	esi, dword ptr [ebx + edx - 0x4]
 db 08Bh,00Ch,013h ; mov	ecx, dword ptr [ebx + edx]
 db 089h,0F2h ; mov	edx, esi
 db 089h,0CFh ; mov	edi, ecx
 db 0F7h,0D2h ; not	edx
 db 0F7h,0D7h ; not	edi
 db 081h,0E2h,000h,000h,080h,07Fh ; and	edx, 0x7f800000
 db 00Fh,045h,0D6h ; cmovne	edx, esi
 db 081h,0E7h,000h,000h,080h,07Fh ; and	edi, 0x7f800000
 db 089h,0DEh ; mov	esi, ebx
 db 08Bh,01Ch,024h ; mov	ebx, dword ptr [esp]
 db 00Fh,045h,0F9h ; cmovne	edi, ecx
 db 089h,053h,0F4h ; mov	dword ptr [ebx - 0xc], edx
 db 08Bh,054h,024h,010h ; mov	edx, dword ptr [esp + 0x10]
 db 089h,07Bh,0F8h ; mov	dword ptr [ebx - 0x8], edi
 db 08Bh,07Ch,016h,0FCh ; mov	edi, dword ptr [esi + edx - 0x4]
 db 08Bh,00Ch,016h ; mov	ecx, dword ptr [esi + edx]
 db 089h,0FAh ; mov	edx, edi
 db 089h,07Ch,024h,00Ch ; mov	dword ptr [esp + 0xc], edi
 db 089h,0CFh ; mov	edi, ecx
 db 0F7h,0D2h ; not	edx
 db 0F7h,0D7h ; not	edi
 db 081h,0E2h,000h,000h,080h,07Fh ; and	edx, 0x7f800000
 db 00Fh,045h,054h,024h,00Ch ; cmovne	edx, dword ptr [esp + 0xc]
 db 081h,0E7h,000h,000h,080h,07Fh ; and	edi, 0x7f800000
 db 00Fh,045h,0F9h ; cmovne	edi, ecx
 db 089h,053h,0FCh ; mov	dword ptr [ebx - 0x4], edx
 db 08Bh,054h,024h,01Ch ; mov	edx, dword ptr [esp + 0x1c]
 db 089h,03Bh ; mov	dword ptr [ebx], edi
 db 089h,0F3h ; mov	ebx, esi
 db 08Bh,04Ch,024h,014h ; mov	ecx, dword ptr [esp + 0x14]
 db 003h,05Ch,024h,018h ; add	ebx, dword ptr [esp + 0x18]
 db 040h ; inc	eax
 db 001h,00Ch,024h ; add	dword ptr [esp], ecx
 db 0FFh,04Ch,024h,004h ; dec	dword ptr [esp + 0x4]
 db 00Fh,084h,0B9h,002h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,07Ch,024h,008h,000h ; cmp	dword ptr [esp + 0x8], 0x0
 db 074h,081h ; je	0x647 <_tnq_attr+0x647>
 db 080h,038h,000h ; cmp	byte ptr [eax], 0x0
 db 00Fh,085h,078h,0FFh,0FFh,0FFh ; jne	0x647 <_tnq_attr+0x647>
 db 0EBh,0D8h ; jmp	0x6a9 <_tnq_attr+0x6a9>
 db 083h,07Ch,024h,004h,000h ; cmp	dword ptr [esp + 0x4], 0x0
 db 00Fh,084h,09Ch,002h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 08Bh,044h,024h,008h ; mov	eax, dword ptr [esp + 0x8]
 db 083h,004h,024h,01Ch ; add	dword ptr [esp], 0x1c
 db 0EBh,05Eh ; jmp	0x744 <_tnq_attr+0x744>
 db 08Bh,04Ch,024h,024h ; mov	ecx, dword ptr [esp + 0x24]
 db 089h,0DEh ; mov	esi, ebx
 db 08Bh,00Ch,00Bh ; mov	ecx, dword ptr [ebx + ecx]
 db 08Bh,01Ch,024h ; mov	ebx, dword ptr [esp]
 db 089h,04Bh,0F4h ; mov	dword ptr [ebx - 0xc], ecx
 db 08Bh,04Ch,024h,020h ; mov	ecx, dword ptr [esp + 0x20]
 db 089h,04Bh,0F8h ; mov	dword ptr [ebx - 0x8], ecx
 db 08Bh,03Ch,016h ; mov	edi, dword ptr [esi + edx]
 db 08Bh,04Ch,016h,004h ; mov	ecx, dword ptr [esi + edx + 0x4]
 db 089h,0FAh ; mov	edx, edi
 db 089h,07Ch,024h,010h ; mov	dword ptr [esp + 0x10], edi
 db 089h,0CFh ; mov	edi, ecx
 db 0F7h,0D2h ; not	edx
 db 0F7h,0D7h ; not	edi
 db 081h,0E2h,000h,000h,080h,07Fh ; and	edx, 0x7f800000
 db 00Fh,045h,054h,024h,010h ; cmovne	edx, dword ptr [esp + 0x10]
 db 081h,0E7h,000h,000h,080h,07Fh ; and	edi, 0x7f800000
 db 00Fh,045h,0F9h ; cmovne	edi, ecx
 db 089h,053h,0FCh ; mov	dword ptr [ebx - 0x4], edx
 db 08Bh,054h,024h,01Ch ; mov	edx, dword ptr [esp + 0x1c]
 db 089h,03Bh ; mov	dword ptr [ebx], edi
 db 089h,0F3h ; mov	ebx, esi
 db 08Bh,04Ch,024h,014h ; mov	ecx, dword ptr [esp + 0x14]
 db 003h,05Ch,024h,018h ; add	ebx, dword ptr [esp + 0x18]
 db 040h ; inc	eax
 db 001h,00Ch,024h ; add	dword ptr [esp], ecx
 db 0FFh,04Ch,024h,004h ; dec	dword ptr [esp + 0x4]
 db 00Fh,084h,034h,002h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,07Ch,024h,008h,000h ; cmp	dword ptr [esp + 0x8], 0x0
 db 074h,09Bh ; je	0x6e6 <_tnq_attr+0x6e6>
 db 080h,038h,000h ; cmp	byte ptr [eax], 0x0
 db 075h,096h ; jne	0x6e6 <_tnq_attr+0x6e6>
 db 0EBh,0DCh ; jmp	0x72e <_tnq_attr+0x72e>
 db 083h,07Ch,024h,004h,000h ; cmp	dword ptr [esp + 0x4], 0x0
 db 00Fh,084h,01Bh,002h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 08Dh,044h,01Ah,004h ; lea	eax, [edx + ebx + 0x4]
 db 08Bh,054h,024h,008h ; mov	edx, dword ptr [esp + 0x8]
 db 083h,004h,024h,01Ch ; add	dword ptr [esp], 0x1c
 db 0EBh,04Fh ; jmp	0x7ba <_tnq_attr+0x7ba>
 db 08Bh,00Ch,024h ; mov	ecx, dword ptr [esp]
 db 08Bh,074h,024h,028h ; mov	esi, dword ptr [esp + 0x28]
 db 089h,071h,0F4h ; mov	dword ptr [ecx - 0xc], esi
 db 08Bh,074h,024h,020h ; mov	esi, dword ptr [esp + 0x20]
 db 089h,071h,0F8h ; mov	dword ptr [ecx - 0x8], esi
 db 08Bh,070h,0FCh ; mov	esi, dword ptr [eax - 0x4]
 db 08Bh,038h ; mov	edi, dword ptr [eax]
 db 089h,0FBh ; mov	ebx, edi
 db 089h,074h,024h,010h ; mov	dword ptr [esp + 0x10], esi
 db 0F7h,0D6h ; not	esi
 db 081h,0E6h,000h,000h,080h,07Fh ; and	esi, 0x7f800000
 db 0F7h,0D3h ; not	ebx
 db 00Fh,045h,074h,024h,010h ; cmovne	esi, dword ptr [esp + 0x10]
 db 081h,0E3h,000h,000h,080h,07Fh ; and	ebx, 0x7f800000
 db 00Fh,045h,0DFh ; cmovne	ebx, edi
 db 089h,071h,0FCh ; mov	dword ptr [ecx - 0x4], esi
 db 089h,019h ; mov	dword ptr [ecx], ebx
 db 08Bh,04Ch,024h,014h ; mov	ecx, dword ptr [esp + 0x14]
 db 003h,044h,024h,018h ; add	eax, dword ptr [esp + 0x18]
 db 042h ; inc	edx
 db 001h,00Ch,024h ; add	dword ptr [esp], ecx
 db 0FFh,04Ch,024h,004h ; dec	dword ptr [esp + 0x4]
 db 00Fh,084h,0BEh,001h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,07Ch,024h,008h,000h ; cmp	dword ptr [esp + 0x8], 0x0
 db 074h,0AAh ; je	0x76b <_tnq_attr+0x76b>
 db 080h,03Ah,000h ; cmp	byte ptr [edx], 0x0
 db 075h,0A5h ; jne	0x76b <_tnq_attr+0x76b>
 db 0EBh,0DCh ; jmp	0x7a4 <_tnq_attr+0x7a4>
 db 083h,07Ch,024h,004h,000h ; cmp	dword ptr [esp + 0x4], 0x0
 db 00Fh,084h,0A5h,001h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 08Dh,044h,01Ah,004h ; lea	eax, [edx + ebx + 0x4]
 db 08Bh,054h,024h,008h ; mov	edx, dword ptr [esp + 0x8]
 db 083h,004h,024h,01Ch ; add	dword ptr [esp], 0x1c
 db 0EBh,048h ; jmp	0x829 <_tnq_attr+0x829>
 db 08Bh,03Ch,024h ; mov	edi, dword ptr [esp]
 db 08Bh,074h,024h,020h ; mov	esi, dword ptr [esp + 0x20]
 db 089h,077h,0F8h ; mov	dword ptr [edi - 0x8], esi
 db 08Bh,070h,0FCh ; mov	esi, dword ptr [eax - 0x4]
 db 08Bh,008h ; mov	ecx, dword ptr [eax]
 db 089h,0CBh ; mov	ebx, ecx
 db 089h,074h,024h,010h ; mov	dword ptr [esp + 0x10], esi
 db 0F7h,0D6h ; not	esi
 db 081h,0E6h,000h,000h,080h,07Fh ; and	esi, 0x7f800000
 db 0F7h,0D3h ; not	ebx
 db 00Fh,045h,074h,024h,010h ; cmovne	esi, dword ptr [esp + 0x10]
 db 081h,0E3h,000h,000h,080h,07Fh ; and	ebx, 0x7f800000
 db 00Fh,045h,0D9h ; cmovne	ebx, ecx
 db 089h,077h,0FCh ; mov	dword ptr [edi - 0x4], esi
 db 089h,01Fh ; mov	dword ptr [edi], ebx
 db 08Bh,04Ch,024h,014h ; mov	ecx, dword ptr [esp + 0x14]
 db 003h,044h,024h,018h ; add	eax, dword ptr [esp + 0x18]
 db 042h ; inc	edx
 db 001h,00Ch,024h ; add	dword ptr [esp], ecx
 db 0FFh,04Ch,024h,004h ; dec	dword ptr [esp + 0x4]
 db 00Fh,084h,04Fh,001h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,07Ch,024h,008h,000h ; cmp	dword ptr [esp + 0x8], 0x0
 db 074h,0B1h ; je	0x7e1 <_tnq_attr+0x7e1>
 db 080h,03Ah,000h ; cmp	byte ptr [edx], 0x0
 db 075h,0ACh ; jne	0x7e1 <_tnq_attr+0x7e1>
 db 0EBh,0DCh ; jmp	0x813 <_tnq_attr+0x813>
 db 083h,07Ch,024h,004h,000h ; cmp	dword ptr [esp + 0x4], 0x0
 db 00Fh,084h,036h,001h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 08Bh,044h,024h,008h ; mov	eax, dword ptr [esp + 0x8]
 db 083h,004h,024h,01Ch ; add	dword ptr [esp], 0x1c
 db 0EBh,057h ; jmp	0x8a3 <_tnq_attr+0x8a3>
 db 08Bh,04Ch,024h,024h ; mov	ecx, dword ptr [esp + 0x24]
 db 08Dh,033h ; lea	esi, [ebx]
 db 08Bh,00Ch,00Bh ; mov	ecx, dword ptr [ebx + ecx]
 db 08Bh,01Ch,024h ; mov	ebx, dword ptr [esp]
 db 089h,04Bh,0F4h ; mov	dword ptr [ebx - 0xc], ecx
 db 08Bh,03Ch,016h ; mov	edi, dword ptr [esi + edx]
 db 08Bh,04Ch,016h,004h ; mov	ecx, dword ptr [esi + edx + 0x4]
 db 089h,0FAh ; mov	edx, edi
 db 089h,07Ch,024h,010h ; mov	dword ptr [esp + 0x10], edi
 db 089h,0CFh ; mov	edi, ecx
 db 0F7h,0D2h ; not	edx
 db 0F7h,0D7h ; not	edi
 db 081h,0E2h,000h,000h,080h,07Fh ; and	edx, 0x7f800000
 db 00Fh,045h,054h,024h,010h ; cmovne	edx, dword ptr [esp + 0x10]
 db 081h,0E7h,000h,000h,080h,07Fh ; and	edi, 0x7f800000
 db 00Fh,045h,0F9h ; cmovne	edi, ecx
 db 089h,053h,0FCh ; mov	dword ptr [ebx - 0x4], edx
 db 08Bh,054h,024h,01Ch ; mov	edx, dword ptr [esp + 0x1c]
 db 089h,03Bh ; mov	dword ptr [ebx], edi
 db 089h,0F3h ; mov	ebx, esi
 db 08Bh,04Ch,024h,014h ; mov	ecx, dword ptr [esp + 0x14]
 db 003h,05Ch,024h,018h ; add	ebx, dword ptr [esp + 0x18]
 db 040h ; inc	eax
 db 001h,00Ch,024h ; add	dword ptr [esp], ecx
 db 0FFh,04Ch,024h,004h ; dec	dword ptr [esp + 0x4]
 db 00Fh,084h,0D5h,000h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 083h,07Ch,024h,008h,000h ; cmp	dword ptr [esp + 0x8], 0x0
 db 074h,0A2h ; je	0x84c <_tnq_attr+0x84c>
 db 080h,038h,000h ; cmp	byte ptr [eax], 0x0
 db 075h,09Dh ; jne	0x84c <_tnq_attr+0x84c>
 db 0EBh,0DCh ; jmp	0x88d <_tnq_attr+0x88d>
 db 083h,07Ch,024h,004h,000h ; cmp	dword ptr [esp + 0x4], 0x0
 db 00Fh,084h,0BCh,000h,000h,000h ; je	0x978 <_tnq_attr+0x978>
 db 08Dh,044h,01Ah,004h ; lea	eax, [edx + ebx + 0x4]
 db 08Bh,054h,024h,008h ; mov	edx, dword ptr [esp + 0x8]
 db 083h,004h,024h,01Ch ; add	dword ptr [esp], 0x1c
 db 0EBh,044h ; jmp	0x90e <_tnq_attr+0x90e>
 db 08Bh,03Ch,024h ; mov	edi, dword ptr [esp]
 db 08Bh,074h,024h,028h ; mov	esi, dword ptr [esp + 0x28]
 db 089h,077h,0F4h ; mov	dword ptr [edi - 0xc], esi
 db 08Bh,070h,0FCh ; mov	esi, dword ptr [eax - 0x4]
 db 08Bh,008h ; mov	ecx, dword ptr [eax]
 db 089h,0CBh ; mov	ebx, ecx
 db 089h,074h,024h,010h ; mov	dword ptr [esp + 0x10], esi
 db 0F7h,0D6h ; not	esi
 db 081h,0E6h,000h,000h,080h,07Fh ; and	esi, 0x7f800000
 db 0F7h,0D3h ; not	ebx
 db 00Fh,045h,074h,024h,010h ; cmovne	esi, dword ptr [esp + 0x10]
 db 081h,0E3h,000h,000h,080h,07Fh ; and	ebx, 0x7f800000
 db 00Fh,045h,0D9h ; cmovne	ebx, ecx
 db 089h,077h,0FCh ; mov	dword ptr [edi - 0x4], esi
 db 089h,01Fh ; mov	dword ptr [edi], ebx
 db 08Bh,04Ch,024h,014h ; mov	ecx, dword ptr [esp + 0x14]
 db 003h,044h,024h,018h ; add	eax, dword ptr [esp + 0x18]
 db 042h ; inc	edx
 db 001h,00Ch,024h ; add	dword ptr [esp], ecx
 db 0FFh,04Ch,024h,004h ; dec	dword ptr [esp + 0x4]
 db 074h,06Ah ; je	0x978 <_tnq_attr+0x978>
 db 083h,07Ch,024h,008h,000h ; cmp	dword ptr [esp + 0x8], 0x0
 db 074h,0B5h ; je	0x8ca <_tnq_attr+0x8ca>
 db 080h,03Ah,000h ; cmp	byte ptr [edx], 0x0
 db 075h,0B0h ; jne	0x8ca <_tnq_attr+0x8ca>
 db 0EBh,0E0h ; jmp	0x8fc <_tnq_attr+0x8fc>
 db 083h,07Ch,024h,004h,000h ; cmp	dword ptr [esp + 0x4], 0x0
 db 074h,055h ; je	0x978 <_tnq_attr+0x978>
 db 08Dh,044h,01Ah,004h ; lea	eax, [edx + ebx + 0x4]
 db 08Bh,054h,024h,008h ; mov	edx, dword ptr [esp + 0x8]
 db 083h,004h,024h,01Ch ; add	dword ptr [esp], 0x1c
 db 0EBh,039h ; jmp	0x96a <_tnq_attr+0x96a>
 db 08Bh,078h,0FCh ; mov	edi, dword ptr [eax - 0x4]
 db 08Bh,008h ; mov	ecx, dword ptr [eax]
 db 089h,0FEh ; mov	esi, edi
 db 089h,0CBh ; mov	ebx, ecx
 db 0F7h,0D6h ; not	esi
 db 0F7h,0D3h ; not	ebx
 db 081h,0E6h,000h,000h,080h,07Fh ; and	esi, 0x7f800000
 db 00Fh,045h,0F7h ; cmovne	esi, edi
 db 081h,0E3h,000h,000h,080h,07Fh ; and	ebx, 0x7f800000
 db 08Bh,03Ch,024h ; mov	edi, dword ptr [esp]
 db 00Fh,045h,0D9h ; cmovne	ebx, ecx
 db 089h,077h,0FCh ; mov	dword ptr [edi - 0x4], esi
 db 089h,01Fh ; mov	dword ptr [edi], ebx
 db 08Bh,04Ch,024h,014h ; mov	ecx, dword ptr [esp + 0x14]
 db 003h,044h,024h,018h ; add	eax, dword ptr [esp + 0x18]
 db 042h ; inc	edx
 db 001h,00Ch,024h ; add	dword ptr [esp], ecx
 db 0FFh,04Ch,024h,004h ; dec	dword ptr [esp + 0x4]
 db 074h,00Eh ; je	0x978 <_tnq_attr+0x978>
 db 083h,07Ch,024h,008h,000h ; cmp	dword ptr [esp + 0x8], 0x0
 db 074h,0C0h ; je	0x931 <_tnq_attr+0x931>
 db 080h,03Ah,000h ; cmp	byte ptr [edx], 0x0
 db 075h,0BBh ; jne	0x931 <_tnq_attr+0x931>
 db 0EBh,0E0h ; jmp	0x958 <_tnq_attr+0x958>
 db 08Dh,065h,0F4h ; lea	esp, [ebp - 0xc]
 db 05Eh ; pop	esi
 db 05Fh ; pop	edi
 db 05Bh ; pop	ebx
 db 05Dh ; pop	ebp
 db 0C3h ; ret
_tnq_attr endp
public _tnq_cull
_tnq_cull proc near
 db 055h ; push	ebp
 db 089h,0E5h ; mov	ebp, esp
 db 053h ; push	ebx
 db 057h ; push	edi
 db 056h ; push	esi
 db 083h,0E4h,0FCh ; and	esp, -0x4
 db 08Dh,064h,024h,0C4h ; lea	esp, [esp - 0x3c]
 db 08Bh,04Dh,008h ; mov	ecx, dword ptr [ebp + 0x8]
 db 031h,0C0h ; xor	eax, eax
 db 08Bh,091h,02Ch,029h,000h,000h ; mov	edx, dword ptr [ecx + 0x292c]
 db 08Bh,0B9h,028h,029h,000h,000h ; mov	edi, dword ptr [ecx + 0x2928]
 db 08Bh,0B1h,050h,029h,000h,000h ; mov	esi, dword ptr [ecx + 0x2950]
 db 08Bh,099h,020h,029h,000h,000h ; mov	ebx, dword ptr [ecx + 0x2920]
 db 083h,0FAh,003h ; cmp	edx, 0x3
 db 089h,054h,024h,02Ch ; mov	dword ptr [esp + 0x2c], edx
 db 08Bh,091h,058h,029h,000h,000h ; mov	edx, dword ptr [ecx + 0x2958]
 db 00Fh,094h,0C0h ; sete	al
 db 083h,0FFh,020h ; cmp	edi, 0x20
 db 089h,054h,024h,01Ch ; mov	dword ptr [esp + 0x1c], edx
 db 066h,00Fh,06Eh,0C0h ; movd	xmm0, eax
 db 08Bh,081h,05Ch,029h,000h,000h ; mov	eax, dword ptr [ecx + 0x295c]
 db 089h,044h,024h,00Ch ; mov	dword ptr [esp + 0xc], eax
 db 08Bh,081h,060h,029h,000h,000h ; mov	eax, dword ptr [ecx + 0x2960]
 db 066h,00Fh,073h,0F0h,03Fh ; psllq	xmm0, 0x3f
 db 089h,044h,024h,028h ; mov	dword ptr [esp + 0x28], eax
 db 08Bh,081h,064h,029h,000h,000h ; mov	eax, dword ptr [ecx + 0x2964]
 db 089h,044h,024h,024h ; mov	dword ptr [esp + 0x24], eax
 db 08Bh,081h,054h,029h,000h,000h ; mov	eax, dword ptr [ecx + 0x2954]
 db 08Bh,089h,024h,029h,000h,000h ; mov	ecx, dword ptr [ecx + 0x2924]
 db 089h,04Ch,024h,020h ; mov	dword ptr [esp + 0x20], ecx
 db 089h,044h,024h,014h ; mov	dword ptr [esp + 0x14], eax
 db 00Fh,084h,026h,001h,000h,000h ; je	0x1a8 <_tnq_cull+0x1a8>
 db 083h,0FFh,028h ; cmp	edi, 0x28
 db 00Fh,085h,02Ch,002h,000h,000h ; jne	0x2b7 <_tnq_cull+0x2b7>
 db 085h,0DBh ; test	ebx, ebx
 db 00Fh,084h,04Eh,003h,000h,000h ; je	0x3e1 <_tnq_cull+0x3e1>
 db 083h,0C6h,004h ; add	esi, 0x4
 db 08Bh,044h,024h,014h ; mov	eax, dword ptr [esp + 0x14]
 db 0E9h,0A2h,000h,000h,000h ; jmp	0x141 <_tnq_cull+0x141>
 db 08Bh,04Ch,024h,008h ; mov	ecx, dword ptr [esp + 0x8]
 db 08Bh,05Ch,024h,01Ch ; mov	ebx, dword ptr [esp + 0x1c]
 db 08Dh,03Ch,089h ; lea	edi, [ecx + 4*ecx]
 db 089h,0D1h ; mov	ecx, edx
 db 08Bh,014h,024h ; mov	edx, dword ptr [esp]
 db 00Fh,05Ah,014h,0FBh ; cvtps2pd	xmm2, qword ptr [ebx + 8*edi]
 db 08Dh,03Ch,092h ; lea	edi, [edx + 4*edx]
 db 089h,0CAh ; mov	edx, ecx
 db 08Bh,04Ch,024h,004h ; mov	ecx, dword ptr [esp + 0x4]
 db 00Fh,05Ah,00Ch,0FBh ; cvtps2pd	xmm1, qword ptr [ebx + 8*edi]
 db 08Dh,03Ch,089h ; lea	edi, [ecx + 4*ecx]
 db 00Fh,05Ah,01Ch,0FBh ; cvtps2pd	xmm3, qword ptr [ebx + 8*edi]
 db 066h,00Fh,05Ch,0CAh ; subpd	xmm1, xmm2
 db 066h,00Fh,05Ch,0DAh ; subpd	xmm3, xmm2
 db 066h,00Fh,0C6h,0C9h,001h ; shufpd	xmm1, xmm1, 0x1
 db 066h,00Fh,059h,0CBh ; mulpd	xmm1, xmm3
 db 066h,00Fh,028h,0D1h ; movapd	xmm2, xmm1
 db 066h,00Fh,015h,0D1h ; unpckhpd	xmm2, xmm1
 db 066h,00Fh,05Ch,0CAh ; subpd	xmm1, xmm2
 db 066h,00Fh,057h,0D2h ; xorpd	xmm2, xmm2
 db 066h,00Fh,057h,0C8h ; xorpd	xmm1, xmm0
 db 0F2h,00Fh,0C2h,0D1h,002h ; cmplesd	xmm2, xmm1
 db 066h,00Fh,050h,0FAh ; movmskpd	edi, xmm2
 db 0F7h,0D7h ; not	edi
 db 021h,0FAh ; and	edx, edi
 db 08Bh,07Ch,024h,008h ; mov	edi, dword ptr [esp + 0x8]
 db 089h,0CBh ; mov	ebx, ecx
 db 089h,0F9h ; mov	ecx, edi
 db 0C1h,0E3h,010h ; shl	ebx, 0x10
 db 009h,0FBh ; or	ebx, edi
 db 08Bh,07Ch,024h,028h ; mov	edi, dword ptr [esp + 0x28]
 db 085h,0D2h ; test	edx, edx
 db 089h,018h ; mov	dword ptr [eax], ebx
 db 089h,0D3h ; mov	ebx, edx
 db 08Bh,014h,024h ; mov	edx, dword ptr [esp]
 db 00Fh,044h,07Ch,024h,024h ; cmove	edi, dword ptr [esp + 0x24]
 db 066h,089h,050h,004h ; mov	word ptr [eax + 0x4], dx
 db 003h,074h,024h,020h ; add	esi, dword ptr [esp + 0x20]
 db 0C6h,004h,00Fh,001h ; mov	byte ptr [edi + ecx], 0x1
 db 08Bh,04Ch,024h,004h ; mov	ecx, dword ptr [esp + 0x4]
 db 0C6h,004h,00Fh,001h ; mov	byte ptr [edi + ecx], 0x1
 db 08Dh,00Ch,05Bh ; lea	ecx, [ebx + 2*ebx]
 db 08Bh,05Ch,024h,010h ; mov	ebx, dword ptr [esp + 0x10]
 db 0C6h,004h,017h,001h ; mov	byte ptr [edi + edx], 0x1
 db 04Bh ; dec	ebx
 db 08Dh,004h,048h ; lea	eax, [eax + 2*ecx]
 db 00Fh,084h,0A0h,002h,000h,000h ; je	0x3e1 <_tnq_cull+0x3e1>
 db 00Fh,0B7h,00Eh ; movzx	ecx, word ptr [esi]
 db 089h,05Ch,024h,010h ; mov	dword ptr [esp + 0x10], ebx
 db 00Fh,0B7h,07Eh,0FCh ; movzx	edi, word ptr [esi - 0x4]
 db 00Fh,0B7h,05Eh,0FEh ; movzx	ebx, word ptr [esi - 0x2]
 db 083h,07Ch,024h,00Ch,000h ; cmp	dword ptr [esp + 0xc], 0x0
 db 00Fh,0B7h,0C9h ; movzx	ecx, cx
 db 089h,07Ch,024h,008h ; mov	dword ptr [esp + 0x8], edi
 db 089h,05Ch,024h,004h ; mov	dword ptr [esp + 0x4], ebx
 db 089h,00Ch,024h ; mov	dword ptr [esp], ecx
 db 074h,026h ; je	0x18b <_tnq_cull+0x18b>
 db 08Bh,054h,024h,00Ch ; mov	edx, dword ptr [esp + 0xc]
 db 00Fh,0B6h,01Ch,01Ah ; movzx	ebx, byte ptr [edx + ebx]
 db 022h,01Ch,03Ah ; and	bl, byte ptr [edx + edi]
 db 08Bh,03Ch,024h ; mov	edi, dword ptr [esp]
 db 084h,01Ch,03Ah ; test	byte ptr [edx + edi], bl
 db 0BAh,000h,000h,000h,000h ; mov	edx, 0x0
 db 00Fh,094h,0C2h ; sete	dl
 db 083h,07Ch,024h,02Ch,000h ; cmp	dword ptr [esp + 0x2c], 0x0
 db 00Fh,085h,016h,0FFh,0FFh,0FFh ; jne	0x9f <_tnq_cull+0x9f>
 db 0EBh,010h ; jmp	0x19b <_tnq_cull+0x19b>
 db 0BAh,001h,000h,000h,000h ; mov	edx, 0x1
 db 083h,07Ch,024h,02Ch,000h ; cmp	dword ptr [esp + 0x2c], 0x0
 db 00Fh,085h,004h,0FFh,0FFh,0FFh ; jne	0x9f <_tnq_cull+0x9f>
 db 08Bh,07Ch,024h,008h ; mov	edi, dword ptr [esp + 0x8]
 db 08Bh,04Ch,024h,004h ; mov	ecx, dword ptr [esp + 0x4]
 db 0E9h,055h,0FFh,0FFh,0FFh ; jmp	0xfd <_tnq_cull+0xfd>
 db 085h,0DBh ; test	ebx, ebx
 db 00Fh,084h,031h,002h,000h,000h ; je	0x3e1 <_tnq_cull+0x3e1>
 db 083h,0C6h,004h ; add	esi, 0x4
 db 08Bh,044h,024h,014h ; mov	eax, dword ptr [esp + 0x14]
 db 0EBh,04Ch ; jmp	0x205 <_tnq_cull+0x205>
 db 089h,0CBh ; mov	ebx, ecx
 db 08Bh,07Ch,024h,004h ; mov	edi, dword ptr [esp + 0x4]
 db 089h,04Ch,024h,008h ; mov	dword ptr [esp + 0x8], ecx
 db 0C1h,0E3h,010h ; shl	ebx, 0x10
 db 089h,0F9h ; mov	ecx, edi
 db 009h,0FBh ; or	ebx, edi
 db 08Bh,07Ch,024h,028h ; mov	edi, dword ptr [esp + 0x28]
 db 085h,0D2h ; test	edx, edx
 db 089h,018h ; mov	dword ptr [eax], ebx
 db 089h,0D3h ; mov	ebx, edx
 db 08Bh,014h,024h ; mov	edx, dword ptr [esp]
 db 00Fh,044h,07Ch,024h,024h ; cmove	edi, dword ptr [esp + 0x24]
 db 066h,089h,050h,004h ; mov	word ptr [eax + 0x4], dx
 db 003h,074h,024h,020h ; add	esi, dword ptr [esp + 0x20]
 db 0C6h,004h,00Fh,001h ; mov	byte ptr [edi + ecx], 0x1
 db 08Bh,04Ch,024h,008h ; mov	ecx, dword ptr [esp + 0x8]
 db 0C6h,004h,00Fh,001h ; mov	byte ptr [edi + ecx], 0x1
 db 08Dh,00Ch,05Bh ; lea	ecx, [ebx + 2*ebx]
 db 08Bh,05Ch,024h,010h ; mov	ebx, dword ptr [esp + 0x10]
 db 0C6h,004h,017h,001h ; mov	byte ptr [edi + edx], 0x1
 db 04Bh ; dec	ebx
 db 08Dh,004h,048h ; lea	eax, [eax + 2*ecx]
 db 00Fh,084h,0DCh,001h,000h,000h ; je	0x3e1 <_tnq_cull+0x3e1>
 db 00Fh,0B7h,00Eh ; movzx	ecx, word ptr [esi]
 db 089h,05Ch,024h,010h ; mov	dword ptr [esp + 0x10], ebx
 db 00Fh,0B7h,07Eh,0FCh ; movzx	edi, word ptr [esi - 0x4]
 db 083h,07Ch,024h,00Ch,000h ; cmp	dword ptr [esp + 0xc], 0x0
 db 00Fh,0B7h,05Eh,0FEh ; movzx	ebx, word ptr [esi - 0x2]
 db 00Fh,0B7h,0C9h ; movzx	ecx, cx
 db 089h,07Ch,024h,004h ; mov	dword ptr [esp + 0x4], edi
 db 089h,00Ch,024h ; mov	dword ptr [esp], ecx
 db 074h,027h ; je	0x24c <_tnq_cull+0x24c>
 db 08Bh,054h,024h,00Ch ; mov	edx, dword ptr [esp + 0xc]
 db 089h,0D9h ; mov	ecx, ebx
 db 00Fh,0B6h,01Ch,01Ah ; movzx	ebx, byte ptr [edx + ebx]
 db 022h,01Ch,03Ah ; and	bl, byte ptr [edx + edi]
 db 08Bh,03Ch,024h ; mov	edi, dword ptr [esp]
 db 084h,01Ch,03Ah ; test	byte ptr [edx + edi], bl
 db 0BAh,000h,000h,000h,000h ; mov	edx, 0x0
 db 00Fh,094h,0C2h ; sete	dl
 db 083h,07Ch,024h,02Ch,000h ; cmp	dword ptr [esp + 0x2c], 0x0
 db 075h,017h ; jne	0x25e <_tnq_cull+0x25e>
 db 0E9h,06Dh,0FFh,0FFh,0FFh ; jmp	0x1b9 <_tnq_cull+0x1b9>
 db 089h,0D9h ; mov	ecx, ebx
 db 0BAh,001h,000h,000h,000h ; mov	edx, 0x1
 db 083h,07Ch,024h,02Ch,000h ; cmp	dword ptr [esp + 0x2c], 0x0
 db 00Fh,084h,05Bh,0FFh,0FFh,0FFh ; je	0x1b9 <_tnq_cull+0x1b9>
 db 08Bh,07Ch,024h,004h ; mov	edi, dword ptr [esp + 0x4]
 db 08Bh,05Ch,024h,01Ch ; mov	ebx, dword ptr [esp + 0x1c]
 db 0C1h,0E7h,005h ; shl	edi, 0x5
 db 00Fh,05Ah,014h,03Bh ; cvtps2pd	xmm2, qword ptr [ebx + edi]
 db 08Bh,03Ch,024h ; mov	edi, dword ptr [esp]
 db 0C1h,0E7h,005h ; shl	edi, 0x5
 db 00Fh,05Ah,00Ch,03Bh ; cvtps2pd	xmm1, qword ptr [ebx + edi]
 db 089h,0CFh ; mov	edi, ecx
 db 0C1h,0E7h,005h ; shl	edi, 0x5
 db 00Fh,05Ah,01Ch,03Bh ; cvtps2pd	xmm3, qword ptr [ebx + edi]
 db 066h,00Fh,05Ch,0CAh ; subpd	xmm1, xmm2
 db 066h,00Fh,05Ch,0DAh ; subpd	xmm3, xmm2
 db 066h,00Fh,0C6h,0C9h,001h ; shufpd	xmm1, xmm1, 0x1
 db 066h,00Fh,059h,0CBh ; mulpd	xmm1, xmm3
 db 066h,00Fh,028h,0D1h ; movapd	xmm2, xmm1
 db 066h,00Fh,015h,0D1h ; unpckhpd	xmm2, xmm1
 db 066h,00Fh,05Ch,0CAh ; subpd	xmm1, xmm2
 db 066h,00Fh,057h,0D2h ; xorpd	xmm2, xmm2
 db 066h,00Fh,057h,0C8h ; xorpd	xmm1, xmm0
 db 0F2h,00Fh,0C2h,0D1h,002h ; cmplesd	xmm2, xmm1
 db 066h,00Fh,050h,0FAh ; movmskpd	edi, xmm2
 db 0F7h,0D7h ; not	edi
 db 021h,0FAh ; and	edx, edi
 db 0E9h,002h,0FFh,0FFh,0FFh ; jmp	0x1b9 <_tnq_cull+0x1b9>
 db 085h,0DBh ; test	ebx, ebx
 db 089h,07Ch,024h,038h ; mov	dword ptr [esp + 0x38], edi
 db 00Fh,084h,01Eh,001h,000h,000h ; je	0x3e1 <_tnq_cull+0x3e1>
 db 083h,0C6h,004h ; add	esi, 0x4
 db 08Bh,044h,024h,014h ; mov	eax, dword ptr [esp + 0x14]
 db 0E9h,0ABh,000h,000h,000h ; jmp	0x37a <_tnq_cull+0x37a>
 db 08Bh,04Ch,024h,038h ; mov	ecx, dword ptr [esp + 0x38]
 db 089h,044h,024h,030h ; mov	dword ptr [esp + 0x30], eax
 db 089h,0CFh ; mov	edi, ecx
 db 089h,0CBh ; mov	ebx, ecx
 db 08Bh,044h,024h,030h ; mov	eax, dword ptr [esp + 0x30]
 db 00Fh,0AFh,07Ch,024h,030h ; imul	edi, dword ptr [esp + 0x30]
 db 00Fh,0AFh,0DAh ; imul	ebx, edx
 db 08Bh,054h,024h,01Ch ; mov	edx, dword ptr [esp + 0x1c]
 db 00Fh,05Ah,00Ch,03Ah ; cvtps2pd	xmm1, qword ptr [edx + edi]
 db 089h,0CFh ; mov	edi, ecx
 db 08Bh,04Ch,024h,034h ; mov	ecx, dword ptr [esp + 0x34]
 db 00Fh,05Ah,014h,01Ah ; cvtps2pd	xmm2, qword ptr [edx + ebx]
 db 00Fh,0AFh,0F9h ; imul	edi, ecx
 db 00Fh,05Ah,01Ch,03Ah ; cvtps2pd	xmm3, qword ptr [edx + edi]
 db 08Bh,054h,024h,018h ; mov	edx, dword ptr [esp + 0x18]
 db 066h,00Fh,05Ch,0D1h ; subpd	xmm2, xmm1
 db 066h,00Fh,05Ch,0D9h ; subpd	xmm3, xmm1
 db 066h,00Fh,0C6h,0D2h,001h ; shufpd	xmm2, xmm2, 0x1
 db 066h,00Fh,059h,0D3h ; mulpd	xmm2, xmm3
 db 066h,00Fh,028h,0CAh ; movapd	xmm1, xmm2
 db 066h,00Fh,015h,0CAh ; unpckhpd	xmm1, xmm2
 db 066h,00Fh,05Ch,0D1h ; subpd	xmm2, xmm1
 db 066h,00Fh,057h,0C9h ; xorpd	xmm1, xmm1
 db 066h,00Fh,057h,0D0h ; xorpd	xmm2, xmm0
 db 0F2h,00Fh,0C2h,0CAh,002h ; cmplesd	xmm1, xmm2
 db 066h,00Fh,050h,0F9h ; movmskpd	edi, xmm1
 db 0F7h,0D7h ; not	edi
 db 021h,0FAh ; and	edx, edi
 db 089h,0CBh ; mov	ebx, ecx
 db 089h,0C6h ; mov	esi, eax
 db 08Bh,07Ch,024h,028h ; mov	edi, dword ptr [esp + 0x28]
 db 0C1h,0E3h,010h ; shl	ebx, 0x10
 db 009h,0C3h ; or	ebx, eax
 db 08Bh,044h,024h,008h ; mov	eax, dword ptr [esp + 0x8]
 db 085h,0D2h ; test	edx, edx
 db 089h,018h ; mov	dword ptr [eax], ebx
 db 089h,0CBh ; mov	ebx, ecx
 db 08Bh,00Ch,024h ; mov	ecx, dword ptr [esp]
 db 08Dh,014h,052h ; lea	edx, [edx + 2*edx]
 db 00Fh,044h,07Ch,024h,024h ; cmove	edi, dword ptr [esp + 0x24]
 db 066h,089h,048h,004h ; mov	word ptr [eax + 0x4], cx
 db 08Dh,004h,050h ; lea	eax, [eax + 2*edx]
 db 0C6h,004h,037h,001h ; mov	byte ptr [edi + esi], 0x1
 db 08Bh,074h,024h,004h ; mov	esi, dword ptr [esp + 0x4]
 db 0C6h,004h,01Fh,001h ; mov	byte ptr [edi + ebx], 0x1
 db 08Bh,05Ch,024h,010h ; mov	ebx, dword ptr [esp + 0x10]
 db 0C6h,004h,00Fh,001h ; mov	byte ptr [edi + ecx], 0x1
 db 003h,074h,024h,020h ; add	esi, dword ptr [esp + 0x20]
 db 04Bh ; dec	ebx
 db 074h,067h ; je	0x3e1 <_tnq_cull+0x3e1>
 db 00Fh,0B7h,00Eh ; movzx	ecx, word ptr [esi]
 db 089h,044h,024h,008h ; mov	dword ptr [esp + 0x8], eax
 db 089h,05Ch,024h,010h ; mov	dword ptr [esp + 0x10], ebx
 db 00Fh,0B7h,05Eh,0FEh ; movzx	ebx, word ptr [esi - 0x2]
 db 083h,07Ch,024h,00Ch,000h ; cmp	dword ptr [esp + 0xc], 0x0
 db 00Fh,0B7h,046h,0FCh ; movzx	eax, word ptr [esi - 0x4]
 db 00Fh,0B7h,0D1h ; movzx	edx, cx
 db 089h,05Ch,024h,034h ; mov	dword ptr [esp + 0x34], ebx
 db 089h,014h,024h ; mov	dword ptr [esp], edx
 db 074h,01Fh ; je	0x3bd <_tnq_cull+0x3bd>
 db 08Bh,054h,024h,00Ch ; mov	edx, dword ptr [esp + 0xc]
 db 031h,0C9h ; xor	ecx, ecx
 db 08Bh,03Ch,024h ; mov	edi, dword ptr [esp]
 db 00Fh,0B6h,01Ch,01Ah ; movzx	ebx, byte ptr [edx + ebx]
 db 022h,01Ch,002h ; and	bl, byte ptr [edx + eax]
 db 084h,01Ch,03Ah ; test	byte ptr [edx + edi], bl
 db 08Bh,014h,024h ; mov	edx, dword ptr [esp]
 db 00Fh,094h,0C1h ; sete	cl
 db 089h,04Ch,024h,018h ; mov	dword ptr [esp + 0x18], ecx
 db 0EBh,008h ; jmp	0x3c5 <_tnq_cull+0x3c5>
 db 0C7h,044h,024h,018h,001h,000h,000h,000h ; mov	dword ptr [esp + 0x18], 0x1
 db 083h,07Ch,024h,02Ch,000h ; cmp	dword ptr [esp + 0x2c], 0x0
 db 089h,074h,024h,004h ; mov	dword ptr [esp + 0x4], esi
 db 00Fh,085h,0FBh,0FEh,0FFh,0FFh ; jne	0x2cf <_tnq_cull+0x2cf>
 db 08Bh,04Ch,024h,034h ; mov	ecx, dword ptr [esp + 0x34]
 db 08Bh,054h,024h,018h ; mov	edx, dword ptr [esp + 0x18]
 db 0E9h,055h,0FFh,0FFh,0FFh ; jmp	0x336 <_tnq_cull+0x336>
 db 02Bh,044h,024h,014h ; sub	eax, dword ptr [esp + 0x14]
 db 0B9h,0ABh,0AAh,0AAh,0AAh ; mov	ecx, 0xaaaaaaab
 db 0D1h,0F8h ; sar	eax
 db 0F7h,0E1h ; mul	ecx
 db 0D1h,0EAh ; shr	edx
 db 08Bh,045h,008h ; mov	eax, dword ptr [ebp + 0x8]
 db 089h,090h,030h,029h,000h,000h ; mov	dword ptr [eax + 0x2930], edx
 db 08Dh,065h,0F4h ; lea	esp, [ebp - 0xc]
 db 05Eh ; pop	esi
 db 05Fh ; pop	edi
 db 05Bh ; pop	ebx
 db 05Dh ; pop	ebp
 db 0C3h ; ret
_tnq_cull endp
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
public _tnq_ixadd
_tnq_ixadd proc near
 db 055h ; push	ebp
 db 08Dh,02Ch,024h ; lea	ebp, [esp]
 db 053h ; push	ebx
 db 057h ; push	edi
 db 056h ; push	esi
 db 083h,0E4h,0FCh ; and	esp, -0x4
 db 08Bh,045h,010h ; mov	eax, dword ptr [ebp + 0x10]
 db 031h,0FFh ; xor	edi, edi
 db 08Bh,04Dh,014h ; mov	ecx, dword ptr [ebp + 0x14]
 db 08Bh,055h,00Ch ; mov	edx, dword ptr [ebp + 0xc]
 db 08Bh,075h,008h ; mov	esi, dword ptr [ebp + 0x8]
 db 083h,0F8h,004h ; cmp	eax, 0x4
 db 072h,02Ah ; jb	0x47 <_tnq_ixadd+0x47>
 db 066h,00Fh,06Eh,0C1h ; movd	xmm0, ecx
 db 031h,0DBh ; xor	ebx, ebx
 db 0F2h,00Fh,070h,0C0h,000h ; pshuflw	xmm0, xmm0, 0x0
 db 066h,00Fh,070h,0C0h,000h ; pshufd	xmm0, xmm0, 0x0
 db 0F3h,00Fh,07Eh,00Ch,05Ah ; movq	xmm1, qword ptr [edx + 2*ebx]
 db 08Dh,07Bh,004h ; lea	edi, [ebx + 0x4]
 db 066h,00Fh,0FDh,0C8h ; paddw	xmm1, xmm0
 db 066h,00Fh,0D6h,00Ch,05Eh ; movq	qword ptr [esi + 2*ebx], xmm1
 db 083h,0C3h,008h ; add	ebx, 0x8
 db 039h,0C3h ; cmp	ebx, eax
 db 08Dh,01Fh ; lea	ebx, [edi]
 db 076h,0E6h ; jbe	0x2d <_tnq_ixadd+0x2d>
 db 039h,0C7h ; cmp	edi, eax
 db 073h,012h ; jae	0x5d <_tnq_ixadd+0x5d>
 db 00Fh,0B7h,01Ch,07Ah ; movzx	ebx, word ptr [edx + 2*edi]
 db 066h,001h,0CBh ; add	bx, cx
 db 066h,089h,01Ch,07Eh ; mov	word ptr [esi + 2*edi], bx
 db 08Dh,07Fh,001h ; lea	edi, [edi + 0x1]
 db 039h,0F8h ; cmp	eax, edi
 db 075h,0EEh ; jne	0x4b <_tnq_ixadd+0x4b>
 db 08Dh,065h,0F4h ; lea	esp, [ebp - 0xc]
 db 05Eh ; pop	esi
 db 05Fh ; pop	edi
 db 05Bh ; pop	ebx
 db 05Dh ; pop	ebp
 db 0C3h ; ret
_tnq_ixadd endp
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
public _tnq_ixmap
_tnq_ixmap proc near
 db 055h ; push	ebp
 db 08Dh,02Ch,024h ; lea	ebp, [esp]
 db 053h ; push	ebx
 db 057h ; push	edi
 db 056h ; push	esi
 db 083h,0E4h,0FCh ; and	esp, -0x4
 db 08Bh,045h,010h ; mov	eax, dword ptr [ebp + 0x10]
 db 085h,0C0h ; test	eax, eax
 db 074h,025h ; je	0x36 <_tnq_ixmap+0x36>
 db 08Bh,07Dh,018h ; mov	edi, dword ptr [ebp + 0x18]
 db 08Bh,075h,014h ; mov	esi, dword ptr [ebp + 0x14]
 db 08Bh,04Dh,00Ch ; mov	ecx, dword ptr [ebp + 0xc]
 db 08Bh,055h,008h ; mov	edx, dword ptr [ebp + 0x8]
 db 001h,0FFh ; add	edi, edi
 db 029h,0FEh ; sub	esi, edi
 db 031h,0FFh ; xor	edi, edi
 db 00Fh,0B7h,01Ch,079h ; movzx	ebx, word ptr [ecx + 2*edi]
 db 00Fh,0B7h,01Ch,05Eh ; movzx	ebx, word ptr [esi + 2*ebx]
 db 066h,089h,01Ch,07Ah ; mov	word ptr [edx + 2*edi], bx
 db 08Dh,07Fh,001h ; lea	edi, [edi + 0x1]
 db 039h,0F8h ; cmp	eax, edi
 db 075h,0EDh ; jne	0x23 <_tnq_ixmap+0x23>
 db 08Dh,065h,0F4h ; lea	esp, [ebp - 0xc]
 db 05Eh ; pop	esi
 db 05Fh ; pop	edi
 db 05Bh ; pop	ebx
 db 05Dh ; pop	ebp
 db 0C3h ; ret
_tnq_ixmap endp
 db 0CCh,0CCh
public _tnq_pack
_tnq_pack proc near
 db 055h ; push	ebp
 db 089h,0E5h ; mov	ebp, esp
 db 053h ; push	ebx
 db 057h ; push	edi
 db 056h ; push	esi
 db 083h,0E4h,0FCh ; and	esp, -0x4
 db 08Dh,064h,024h,0F4h ; lea	esp, [esp - 0xc]
 db 08Bh,045h,01Ch ; mov	eax, dword ptr [ebp + 0x1c]
 db 08Bh,07Dh,018h ; mov	edi, dword ptr [ebp + 0x18]
 db 08Bh,04Dh,00Ch ; mov	ecx, dword ptr [ebp + 0xc]
 db 08Bh,055h,008h ; mov	edx, dword ptr [ebp + 0x8]
 db 083h,0E0h,002h ; and	eax, 0x2
 db 08Dh,004h,085h,018h,000h,000h,000h ; lea	eax, [4*eax + 0x18]
 db 083h,07Dh,014h,000h ; cmp	dword ptr [ebp + 0x14], 0x0
 db 089h,044h,024h,008h ; mov	dword ptr [esp + 0x8], eax
 db 074h,07Eh ; je	0xab <_tnq_pack+0xab>
 db 083h,07Dh,020h,000h ; cmp	dword ptr [ebp + 0x20], 0x0
 db 074h,078h ; je	0xab <_tnq_pack+0xab>
 db 08Bh,045h,01Ch ; mov	eax, dword ptr [ebp + 0x1c]
 db 0A8h,001h ; test	al, 0x1
 db 00Fh,085h,0FFh,000h,000h,000h ; jne	0x13d <_tnq_pack+0x13d>
 db 08Bh,045h,024h ; mov	eax, dword ptr [ebp + 0x24]
 db 085h,0FFh ; test	edi, edi
 db 089h,044h,024h,004h ; mov	dword ptr [esp + 0x4], eax
 db 00Fh,084h,060h,001h,000h,000h ; je	0x1ad <_tnq_pack+0x1ad>
 db 089h,044h,024h,004h ; mov	dword ptr [esp + 0x4], eax
 db 083h,0C1h,018h ; add	ecx, 0x18
 db 031h,0DBh ; xor	ebx, ebx
 db 08Bh,045h,018h ; mov	eax, dword ptr [ebp + 0x18]
 db 0EBh,00Ch ; jmp	0x67 <_tnq_pack+0x67>
 db 043h ; inc	ebx
 db 003h,04Dh,010h ; add	ecx, dword ptr [ebp + 0x10]
 db 039h,0D8h ; cmp	eax, ebx
 db 00Fh,084h,046h,001h,000h,000h ; je	0x1ad <_tnq_pack+0x1ad>
 db 08Bh,075h,014h ; mov	esi, dword ptr [ebp + 0x14]
 db 080h,03Ch,01Eh,000h ; cmp	byte ptr [esi + ebx], 0x0
 db 074h,0EBh ; je	0x5b <_tnq_pack+0x5b>
 db 0F2h,00Fh,010h,041h,0E8h ; movsd	xmm0, qword ptr [ecx - 0x18]
 db 0F2h,00Fh,010h,049h,0F0h ; movsd	xmm1, qword ptr [ecx - 0x10]
 db 0F2h,00Fh,010h,051h,0F8h ; movsd	xmm2, qword ptr [ecx - 0x8]
 db 0F2h,00Fh,010h,019h ; movsd	xmm3, qword ptr [ecx]
 db 08Bh,07Ch,024h,004h ; mov	edi, dword ptr [esp + 0x4]
 db 08Bh,075h,020h ; mov	esi, dword ptr [ebp + 0x20]
 db 0F2h,00Fh,011h,002h ; movsd	qword ptr [edx], xmm0
 db 0F2h,00Fh,011h,04Ah,008h ; movsd	qword ptr [edx + 0x8], xmm1
 db 0F2h,00Fh,011h,052h,010h ; movsd	qword ptr [edx + 0x10], xmm2
 db 0F2h,00Fh,011h,05Ah,018h ; movsd	qword ptr [edx + 0x18], xmm3
 db 083h,0C2h,020h ; add	edx, 0x20
 db 066h,089h,03Ch,05Eh ; mov	word ptr [esi + 2*ebx], di
 db 047h ; inc	edi
 db 089h,07Ch,024h,004h ; mov	dword ptr [esp + 0x4], edi
 db 0EBh,0B0h ; jmp	0x5b <_tnq_pack+0x5b>
 db 085h,0FFh ; test	edi, edi
 db 00Fh,084h,007h,001h,000h,000h ; je	0x1ba <_tnq_pack+0x1ba>
 db 031h,0DBh ; xor	ebx, ebx
 db 0C7h,044h,024h,004h,000h,000h,000h,000h ; mov	dword ptr [esp + 0x4], 0x0
 db 0EBh,011h ; jmp	0xd0 <_tnq_pack+0xd0>
 db 046h ; inc	esi
 db 089h,074h,024h,004h ; mov	dword ptr [esp + 0x4], esi
 db 043h ; inc	ebx
 db 003h,04Dh,010h ; add	ecx, dword ptr [ebp + 0x10]
 db 039h,0DFh ; cmp	edi, ebx
 db 00Fh,084h,0F2h,000h,000h,000h ; je	0x1c2 <_tnq_pack+0x1c2>
 db 083h,07Dh,014h,000h ; cmp	dword ptr [ebp + 0x14], 0x0
 db 074h,009h ; je	0xdf <_tnq_pack+0xdf>
 db 08Bh,075h,014h ; mov	esi, dword ptr [ebp + 0x14]
 db 080h,03Ch,01Eh,000h ; cmp	byte ptr [esi + ebx], 0x0
 db 074h,0E5h ; je	0xc4 <_tnq_pack+0xc4>
 db 08Bh,045h,01Ch ; mov	eax, dword ptr [ebp + 0x1c]
 db 0F2h,00Fh,010h,001h ; movsd	xmm0, qword ptr [ecx]
 db 0F2h,00Fh,010h,049h,008h ; movsd	xmm1, qword ptr [ecx + 0x8]
 db 0F2h,00Fh,010h,051h,010h ; movsd	xmm2, qword ptr [ecx + 0x10]
 db 0F2h,00Fh,010h,059h,018h ; movsd	xmm3, qword ptr [ecx + 0x18]
 db 0A8h,001h ; test	al, 0x1
 db 0F2h,00Fh,011h,002h ; movsd	qword ptr [edx], xmm0
 db 0F2h,00Fh,011h,04Ah,008h ; movsd	qword ptr [edx + 0x8], xmm1
 db 0F2h,00Fh,011h,052h,010h ; movsd	qword ptr [edx + 0x10], xmm2
 db 0F2h,00Fh,011h,05Ah,018h ; movsd	qword ptr [edx + 0x18], xmm3
 db 075h,005h ; jne	0x111 <_tnq_pack+0x111>
 db 083h,0C2h,020h ; add	edx, 0x20
 db 0EBh,011h ; jmp	0x122 <_tnq_pack+0x122>
 db 08Bh,074h,024h,008h ; mov	esi, dword ptr [esp + 0x8]
 db 0F2h,00Fh,010h,004h,031h ; movsd	xmm0, qword ptr [ecx + esi]
 db 0F2h,00Fh,011h,042h,020h ; movsd	qword ptr [edx + 0x20], xmm0
 db 083h,0C2h,028h ; add	edx, 0x28
 db 083h,07Dh,020h,000h ; cmp	dword ptr [ebp + 0x20], 0x0
 db 08Bh,074h,024h,004h ; mov	esi, dword ptr [esp + 0x4]
 db 074h,093h ; je	0xbf <_tnq_pack+0xbf>
 db 08Bh,07Dh,024h ; mov	edi, dword ptr [ebp + 0x24]
 db 08Bh,045h,020h ; mov	eax, dword ptr [ebp + 0x20]
 db 001h,0F7h ; add	edi, esi
 db 066h,089h,03Ch,058h ; mov	word ptr [eax + 2*ebx], di
 db 08Bh,07Dh,018h ; mov	edi, dword ptr [ebp + 0x18]
 db 0EBh,082h ; jmp	0xbf <_tnq_pack+0xbf>
 db 08Bh,045h,024h ; mov	eax, dword ptr [ebp + 0x24]
 db 085h,0FFh ; test	edi, edi
 db 089h,044h,024h,004h ; mov	dword ptr [esp + 0x4], eax
 db 074h,065h ; je	0x1ad <_tnq_pack+0x1ad>
 db 089h,044h,024h,004h ; mov	dword ptr [esp + 0x4], eax
 db 031h,0DBh ; xor	ebx, ebx
 db 08Bh,045h,018h ; mov	eax, dword ptr [ebp + 0x18]
 db 0EBh,008h ; jmp	0x15b <_tnq_pack+0x15b>
 db 043h ; inc	ebx
 db 003h,04Dh,010h ; add	ecx, dword ptr [ebp + 0x10]
 db 039h,0D8h ; cmp	eax, ebx
 db 074h,052h ; je	0x1ad <_tnq_pack+0x1ad>
 db 08Bh,075h,014h ; mov	esi, dword ptr [ebp + 0x14]
 db 080h,03Ch,01Eh,000h ; cmp	byte ptr [esi + ebx], 0x0
 db 074h,0EFh ; je	0x153 <_tnq_pack+0x153>
 db 0F2h,00Fh,010h,001h ; movsd	xmm0, qword ptr [ecx]
 db 0F2h,00Fh,010h,049h,008h ; movsd	xmm1, qword ptr [ecx + 0x8]
 db 0F2h,00Fh,010h,051h,010h ; movsd	xmm2, qword ptr [ecx + 0x10]
 db 0F2h,00Fh,010h,059h,018h ; movsd	xmm3, qword ptr [ecx + 0x18]
 db 08Bh,074h,024h,008h ; mov	esi, dword ptr [esp + 0x8]
 db 08Bh,07Ch,024h,004h ; mov	edi, dword ptr [esp + 0x4]
 db 0F2h,00Fh,011h,002h ; movsd	qword ptr [edx], xmm0
 db 0F2h,00Fh,011h,04Ah,008h ; movsd	qword ptr [edx + 0x8], xmm1
 db 0F2h,00Fh,011h,052h,010h ; movsd	qword ptr [edx + 0x10], xmm2
 db 0F2h,00Fh,011h,05Ah,018h ; movsd	qword ptr [edx + 0x18], xmm3
 db 0F2h,00Fh,010h,004h,031h ; movsd	xmm0, qword ptr [ecx + esi]
 db 08Bh,075h,020h ; mov	esi, dword ptr [ebp + 0x20]
 db 0F2h,00Fh,011h,042h,020h ; movsd	qword ptr [edx + 0x20], xmm0
 db 083h,0C2h,028h ; add	edx, 0x28
 db 066h,089h,03Ch,05Eh ; mov	word ptr [esi + 2*ebx], di
 db 047h ; inc	edi
 db 089h,07Ch,024h,004h ; mov	dword ptr [esp + 0x4], edi
 db 0EBh,0A6h ; jmp	0x153 <_tnq_pack+0x153>
 db 08Bh,044h,024h,004h ; mov	eax, dword ptr [esp + 0x4]
 db 02Bh,045h,024h ; sub	eax, dword ptr [ebp + 0x24]
 db 089h,044h,024h,004h ; mov	dword ptr [esp + 0x4], eax
 db 0EBh,008h ; jmp	0x1c2 <_tnq_pack+0x1c2>
 db 0C7h,044h,024h,004h,000h,000h,000h,000h ; mov	dword ptr [esp + 0x4], 0x0
 db 08Bh,044h,024h,004h ; mov	eax, dword ptr [esp + 0x4]
 db 08Dh,065h,0F4h ; lea	esp, [ebp - 0xc]
 db 05Eh ; pop	esi
 db 05Fh ; pop	edi
 db 05Bh ; pop	ebx
 db 05Dh ; pop	ebp
 db 0C3h ; ret
_tnq_pack endp
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh,0CCh
 db 0CCh,0CCh
public _tnq_mm4
_tnq_mm4 proc near
 db 055h ; push	ebp
 db 08Dh,02Ch,024h ; lea	ebp, [esp]
 db 083h,0E4h,0FCh ; and	esp, -0x4
 db 08Bh,04Dh,010h ; mov	ecx, dword ptr [ebp + 0x10]
 db 08Bh,045h,00Ch ; mov	eax, dword ptr [ebp + 0xc]
 db 00Fh,010h,009h ; movups	xmm1, xmmword ptr [ecx]
 db 00Fh,010h,041h,010h ; movups	xmm0, xmmword ptr [ecx + 0x10]
 db 0F3h,00Fh,010h,018h ; movss	xmm3, dword ptr [eax]
 db 0F3h,00Fh,010h,060h,004h ; movss	xmm4, dword ptr [eax + 0x4]
 db 00Fh,010h,051h,020h ; movups	xmm2, xmmword ptr [ecx + 0x20]
 db 0F3h,00Fh,010h,068h,008h ; movss	xmm5, dword ptr [eax + 0x8]
 db 0F3h,00Fh,010h,070h,00Ch ; movss	xmm6, dword ptr [eax + 0xc]
 db 00Fh,0C6h,0DBh,000h ; shufps	xmm3, xmm3, 0x0
 db 00Fh,0C6h,0E4h,000h ; shufps	xmm4, xmm4, 0x0
 db 00Fh,0C6h,0EDh,000h ; shufps	xmm5, xmm5, 0x0
 db 00Fh,0C6h,0F6h,000h ; shufps	xmm6, xmm6, 0x0
 db 00Fh,059h,0D9h ; mulps	xmm3, xmm1
 db 00Fh,059h,0E0h ; mulps	xmm4, xmm0
 db 00Fh,059h,0EAh ; mulps	xmm5, xmm2
 db 00Fh,058h,0E3h ; addps	xmm4, xmm3
 db 00Fh,010h,059h,030h ; movups	xmm3, xmmword ptr [ecx + 0x30]
 db 08Bh,04Dh,008h ; mov	ecx, dword ptr [ebp + 0x8]
 db 00Fh,059h,0F3h ; mulps	xmm6, xmm3
 db 00Fh,058h,0ECh ; addps	xmm5, xmm4
 db 00Fh,058h,0F5h ; addps	xmm6, xmm5
 db 00Fh,011h,031h ; movups	xmmword ptr [ecx], xmm6
 db 0F3h,00Fh,010h,068h,010h ; movss	xmm5, dword ptr [eax + 0x10]
 db 0F3h,00Fh,010h,060h,014h ; movss	xmm4, dword ptr [eax + 0x14]
 db 0F3h,00Fh,010h,070h,018h ; movss	xmm6, dword ptr [eax + 0x18]
 db 00Fh,0C6h,0EDh,000h ; shufps	xmm5, xmm5, 0x0
 db 00Fh,0C6h,0E4h,000h ; shufps	xmm4, xmm4, 0x0
 db 00Fh,0C6h,0F6h,000h ; shufps	xmm6, xmm6, 0x0
 db 00Fh,059h,0E9h ; mulps	xmm5, xmm1
 db 00Fh,059h,0E0h ; mulps	xmm4, xmm0
 db 00Fh,059h,0F2h ; mulps	xmm6, xmm2
 db 00Fh,058h,0E5h ; addps	xmm4, xmm5
 db 0F3h,00Fh,010h,068h,01Ch ; movss	xmm5, dword ptr [eax + 0x1c]
 db 00Fh,0C6h,0EDh,000h ; shufps	xmm5, xmm5, 0x0
 db 00Fh,059h,0EBh ; mulps	xmm5, xmm3
 db 00Fh,058h,0F4h ; addps	xmm6, xmm4
 db 00Fh,058h,0EEh ; addps	xmm5, xmm6
 db 00Fh,011h,069h,010h ; movups	xmmword ptr [ecx + 0x10], xmm5
 db 0F3h,00Fh,010h,068h,020h ; movss	xmm5, dword ptr [eax + 0x20]
 db 0F3h,00Fh,010h,060h,024h ; movss	xmm4, dword ptr [eax + 0x24]
 db 0F3h,00Fh,010h,070h,028h ; movss	xmm6, dword ptr [eax + 0x28]
 db 00Fh,0C6h,0EDh,000h ; shufps	xmm5, xmm5, 0x0
 db 00Fh,0C6h,0E4h,000h ; shufps	xmm4, xmm4, 0x0
 db 00Fh,0C6h,0F6h,000h ; shufps	xmm6, xmm6, 0x0
 db 00Fh,059h,0E9h ; mulps	xmm5, xmm1
 db 00Fh,059h,0E0h ; mulps	xmm4, xmm0
 db 00Fh,059h,0F2h ; mulps	xmm6, xmm2
 db 00Fh,058h,0E5h ; addps	xmm4, xmm5
 db 0F3h,00Fh,010h,068h,02Ch ; movss	xmm5, dword ptr [eax + 0x2c]
 db 00Fh,0C6h,0EDh,000h ; shufps	xmm5, xmm5, 0x0
 db 00Fh,059h,0EBh ; mulps	xmm5, xmm3
 db 00Fh,058h,0F4h ; addps	xmm6, xmm4
 db 00Fh,058h,0EEh ; addps	xmm5, xmm6
 db 00Fh,011h,069h,020h ; movups	xmmword ptr [ecx + 0x20], xmm5
 db 0F3h,00Fh,010h,068h,030h ; movss	xmm5, dword ptr [eax + 0x30]
 db 0F3h,00Fh,010h,060h,034h ; movss	xmm4, dword ptr [eax + 0x34]
 db 00Fh,0C6h,0EDh,000h ; shufps	xmm5, xmm5, 0x0
 db 00Fh,0C6h,0E4h,000h ; shufps	xmm4, xmm4, 0x0
 db 00Fh,059h,0E9h ; mulps	xmm5, xmm1
 db 00Fh,059h,0E0h ; mulps	xmm4, xmm0
 db 0F3h,00Fh,010h,040h,038h ; movss	xmm0, dword ptr [eax + 0x38]
 db 0F3h,00Fh,010h,048h,03Ch ; movss	xmm1, dword ptr [eax + 0x3c]
 db 00Fh,0C6h,0C0h,000h ; shufps	xmm0, xmm0, 0x0
 db 00Fh,0C6h,0C9h,000h ; shufps	xmm1, xmm1, 0x0
 db 00Fh,059h,0C2h ; mulps	xmm0, xmm2
 db 00Fh,059h,0CBh ; mulps	xmm1, xmm3
 db 00Fh,058h,0E5h ; addps	xmm4, xmm5
 db 00Fh,058h,0C4h ; addps	xmm0, xmm4
 db 00Fh,058h,0C8h ; addps	xmm1, xmm0
 db 00Fh,011h,049h,030h ; movups	xmmword ptr [ecx + 0x30], xmm1
 db 089h,0ECh ; mov	esp, ebp
 db 05Dh ; pop	ebp
 db 0C3h ; ret
_tnq_mm4 endp
_TEXT ends
end
