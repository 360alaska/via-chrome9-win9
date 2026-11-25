; vx_tnl_sse.asm: T&L group kernels (SSE2) as bytes for wasm.
; _tnk_pos, _tnk_light, _tnk_light_k, _tnk_plight, _tnk_vcodes are the rescheduled kernels
; (same operations, same association, fewer spills, two dot-products in flight).
; _tnk_spec, _tnk_cv, _tnk_splight are the previous clang sequences: same bits as vx_d3d_tnl_soa.inc.
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
public _tnk_vcodes
_tnk_vcodes proc near
 db 056h ; push	esi
 db 057h ; push	edi
 db 08Bh,044h,024h,00Ch ; mov	eax, dword ptr [esp + 0xc]
 db 08Bh,088h,0B0h,007h,000h,000h ; mov	ecx, dword ptr [eax + 0x7b0]
 db 08Bh,0B0h,0ACh,007h,000h,000h ; mov	esi, dword ptr [eax + 0x7ac]
 db 001h,0CEh ; add	esi, ecx
 db 08Bh,0B8h,014h,023h,000h,000h ; mov	edi, dword ptr [eax + 0x2314]
 db 039h,0F1h ; cmp	ecx, esi
 db 00Fh,083h,087h,000h,000h,000h ; jae	0xa9 <_tnk_vcodes+0xa9>
 db 08Bh,090h,0ECh,009h,000h,000h ; mov	edx, dword ptr [eax + 0x9ec]
 db 00Fh,010h,004h,08Ah ; movups	xmm0, xmmword ptr [edx + 4*ecx]
 db 08Bh,090h,0F0h,009h,000h,000h ; mov	edx, dword ptr [eax + 0x9f0]
 db 00Fh,010h,00Ch,08Ah ; movups	xmm1, xmmword ptr [edx + 4*ecx]
 db 08Bh,090h,0F8h,009h,000h,000h ; mov	edx, dword ptr [eax + 0x9f8]
 db 00Fh,010h,014h,08Ah ; movups	xmm2, xmmword ptr [edx + 4*ecx]
 db 00Fh,010h,098h,080h,008h,000h,000h ; movups	xmm3, xmmword ptr [eax + 0x880]
 db 00Fh,059h,0D3h ; mulps	xmm2, xmm3
 db 00Fh,057h,0DBh ; xorps	xmm3, xmm3
 db 00Fh,05Ch,0DAh ; subps	xmm3, xmm2
 db 00Fh,028h,0E0h ; movaps	xmm4, xmm0
 db 00Fh,0C2h,0E3h,001h ; cmpltps	xmm4, xmm3
 db 00Fh,054h,0A0h,040h,023h,000h,000h ; andps	xmm4, xmmword ptr [eax + 0x2340]
 db 00Fh,028h,0EAh ; movaps	xmm5, xmm2
 db 00Fh,0C2h,0E8h,001h ; cmpltps	xmm5, xmm0
 db 00Fh,054h,0A8h,050h,023h,000h,000h ; andps	xmm5, xmmword ptr [eax + 0x2350]
 db 00Fh,056h,0E5h ; orps	xmm4, xmm5
 db 00Fh,028h,0E9h ; movaps	xmm5, xmm1
 db 00Fh,0C2h,0EBh,001h ; cmpltps	xmm5, xmm3
 db 00Fh,054h,0A8h,060h,023h,000h,000h ; andps	xmm5, xmmword ptr [eax + 0x2360]
 db 00Fh,056h,0E5h ; orps	xmm4, xmm5
 db 00Fh,028h,0EAh ; movaps	xmm5, xmm2
 db 00Fh,0C2h,0E9h,001h ; cmpltps	xmm5, xmm1
 db 00Fh,054h,0A8h,070h,023h,000h,000h ; andps	xmm5, xmmword ptr [eax + 0x2370]
 db 00Fh,056h,0E5h ; orps	xmm4, xmm5
 db 066h,00Fh,06Bh,0E4h ; packssdw	xmm4, xmm4
 db 066h,00Fh,067h,0E4h ; packuswb	xmm4, xmm4
 db 066h,00Fh,07Eh,024h,00Fh ; movd	dword ptr [edi + ecx], xmm4
 db 083h,0C1h,004h ; add	ecx, 0x4
 db 039h,0F1h ; cmp	ecx, esi
 db 00Fh,082h,079h,0FFh,0FFh,0FFh ; jb	0x22 <_tnk_vcodes+0x22>
 db 05Fh ; pop	edi
 db 05Eh ; pop	esi
 db 0C3h ; ret
_tnk_vcodes endp
public _tnk_pos
_tnk_pos proc near
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
_tnk_pos endp
public _tnk_pos_o
_tnk_pos_o proc near
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
_tnk_pos_o endp
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
_TEXT ends
end
