# VX900 supplement to the Chrome9 Win98 cookbook

Nov 13, 2026 · 360ALASKA

## 1. Scope

Everything known about the VX900 (Chrome9 HD, PCI 1106:7122) that bears on the Windows 98 driver, collected before the first board run. Nothing here has run on a VX900 except the boot fix in cookbook section 35. Recipes move into the cookbook as the board confirms them.

On paper the port is smaller in 2D and display than the handoff assumed, and the real work is the 3D vertex input. Section 6 explains why the 2D engine is expected to be the one the driver already drives.

Each fact carries the grade of its source:

| Grade | Meaning |
| --- | --- |
| Board | Seen on a VX900 board |
| Manual | VIA's VX900 manuals (Part I core and 2D, Part II 3D and video, the system manual) |
| Source | OpenChrome, which runs on the VX900 |
| Binary | Static reading of VIA's XP (2013) and Windows CE drivers; never run here |
| Inferred | A conclusion drawn from the above, to be tested |

The manuals are preliminary and contradict themselves and VIA's own drivers in places. Where a register is marked reserved but VIA's drivers write it, both facts are given.

## 2. Device, BARs and memory

The frame buffer is on the BAR at 18h and the registers on the BAR at 14h; the driver already reads them there.

| Configuration offset | VIA's name | Size | Holds | Manual default |
| --- | --- | --- | --- | --- |
| 10h | MB0 | 512 MB | Registers of the AES and video IP blocks. Not graphics. | 04h |
| 14h | MB1 | 16 MB | Graphics registers, the 2D host-blit space, the burst command area | 00h |
| 18h (and 1Ch) | MB2 | 512 MB | Frame buffer (S.L.) | 0Ch: 64-bit, prefetchable |

Grade: Manual (Part I, PCI table); Source (OpenChrome takes the registers from region 1 on all three chips and the frame buffer from region 2 on the VX900 only).

**Rules.**

- The decode rule of cookbook 34.1 holds unchanged: expect the BAR at 18h to read as 64-bit, and refuse it unless the dword at 1Ch is 0.
- The 512 MB is address space, not memory. The real frame buffer size is in the host bridge: D0F3 register A1h-A0h, bits 14:12 (001 = 8 MB, 010 = 16, 011 = 32, 100 = 64, 101 = 128, 110 = 256, 111 = 512); bit 15 enables the internal graphics device (Manual, system manual). It is a second figure to check the BIOS's against.
- Code that looks for the frame buffer at 10h finds the AES block's registers on this chip. Nothing in the driver may fall back to 10h.

**On the board (2026-11-13, Gigabyte, subsystem 1458:D000, VXREG v1.58).** The registers' BAR at 14h reads FC000000 and the frame buffer's at 18h reads F0000000; the VxD reaches bring-up stage 6 with failure 0. So 14h is a BAR of its own, and the manual's 64-bit default for 10h does not apply on this board. DirectDraw reports 48,790 KB of video memory at 800x600, 16 bits.

VXSTATE on the same board: the frame buffer is 128 MB. The hardware cursor is in use and works (image block at 03FFB000h). Registers 70h and 78h, inside the range the manual calls reserved, read 00010001h and 00000200h. CR3E reads 0F and sequencer 5D reads F0, as section 3 expects.

**Boards.** Gigabyte M7V90PI-VL (Nano U3300, AMI BIOS V9F5, video BIOS 09/01/11 Ver19) and an HP board (Eden X2 U4200). Details in cookbook section 35.

## 3. Starting Windows 98 on a VX900 board

A patched `VDD.VXD` in `C:\WINDOWS\SYSTEM\VMM32` is the precondition for everything else; the recipe and the four changes are in cookbook section 35 (Board). VIA's manual explains the two registers that section found by tracing.

| Register | Manual's definition | What it explains |
| --- | --- | --- |
| Sequencer 5D | Timer Control. Bits 6:0 are a countdown step count, one step about 10 microseconds (8.9). Bit 7 reads 1 when the timer has run out. | The BIOS writes 70h (112 steps, about 1 ms) and polls bit 7. The stock VDD drops the write and answers the read from its own data, so the poll never ends. |
| CRTC 3E | Scratch Pad Register 5 | A BIOS-private flag byte, not hardware. That is why one board's BIOS runs the delay routine (CR3E = 0F) and the VX855's never does. |

**Rules for the driver.**

- The driver makes its mode sets through the video BIOS inside a VM (`vesa_bios_V86`), and on a 7122 it uses the BIOS's timings for every mode. Those calls pass through the same VDD trapping, so the patched VDD must be in place before the driver is installed.
- The driver must not write sequencer 5D itself or depend on its value: the BIOS owns that timer during a mode set.
- Bring-up order on a new VX900 board: patched VDD first, confirm the desktop in the standard VGA driver, then install the driver.

## 4. What the driver does on a 7122 today

On a VX900 the .597 binaries are a frame buffer driver: modes, cursor and flips on the BIOS's timings, with no engine touched. From 2026-11-13 `CHROME9.INF` lists the device again (`%DeviceHD%=VXInstall,PCI\VEN_1106&DEV_7122`, string "VIA Chrome9 HD (VX900)"); the binaries are unchanged.

| Area | Behaviour on 7122 | Where |
| --- | --- | --- |
| Identification | Accepted beside 1122 and 5122 | `vx85598.c` line 119 |
| Frame buffer BAR | Configuration 18h | `vx85598.c` line 127 |
| Register BAR | Configuration 14h | `vx85598.c` line 128 |
| Video ROM check | Any VIA x86 ROM; the strict device-number test applies to the VX855 only | `vx85598.c` line 147 |
| 2D engine | Off`  until .600. On from .601: VX_chip_2d() returns 1 ` | `vx85598.c` line 74, `vxd_main.c` line 715, `vx855_2d.c` line 1925 |
| 3D engine | Reported not ready: OpenGL in software. From .598 no Direct3D is offered either (see below). | `vx_native3d_vxd.c` line 177 |
| Overlay | Not offered: `VX_chip_video()` returns 0, the query answers mask 0 | `vx85598.c` line 76, `vx855_2d.c` line 1680 |
| Mode timing | The BIOS's. The driver's own timing is enabled for 5122 and 1122 only. | `vxd_main.c` line 705 |
| Pixel clock | Multiplier and divider written as computed, as on the VX855 | `vxd_vesa.c` line 665 |
| Report | VXREG prints the device, both BARs and "engines not used" (TEXDBG 75) | `vxd_main.c` line 1094 |

**Two gates from .601.** `VX_chip_accel()` used to switch 2D and 3D together. Now `VX_chip_2d()` answers for the 2D engine (all three chips) and `VX_chip_accel()` for the 3D engine and Direct3D (VX800 and VX855). TEXDBG 75 word 4 carries both: bit 0 the 3D engine, bit 1 the 2D engine.

**Direct3D was still offered (.597, fixed in .598).** On the board VXREG showed "Direct3D offered" and the Direct3D HAL started, with the 3D engine off. The VxD's readiness flag only makes context creation fail; the offer itself comes from the HAL's ACCEL\_D3D query (control FFFFFE50h), which answered 1 on every chip. An application would pick the hardware device and fail instead of taking the software one. Rule: the query answers 0 wherever `VX_chip_accel()` is 0 (`vx855_2d.c`). VXREG v1.59 reports WRONG if Direct3D is offered with the engines off, and `tests/chip_profile.py` checks the answer for each chip.

## 5. Mode setting and display

The VX900's display side follows the VX855's with four additions; none matters while the BIOS sets the timings, and all four matter before the driver's own timing is enabled on this chip.

| Item | VX855 | VX900 | Grade |
| --- | --- | --- | --- |
| Extended CRTC unlock | Clear CR47 bit 4 | Same | Source |
| Pixel clock | Multiplier and divider as computed | Same | Source |
| Primary FIFO (SR17 / SR16 / SR18 / SR22) | C7 / 90 / 90 / 28 | Same | Source |
| Secondary FIFO, CR68 bits 7:4 | 8 | 7 | Source |
| Primary horizontal timing | 8-bit fields plus the usual overflow bits | One more high bit each: CR45 bit 1 for display end, CR45 bit 2 for blank start | Source |
| Secondary horizontal timing | No extra bits | CR55 bit 7 and CR6B bit 0 | Source |
| Saved and restored CRTC range | CRF6 to CRFC | CRF6 to CRFD, with CRF7 to CRFF handled late | Source |
| Extra sequencer register in VIA's XP driver | None | SR C8, saved and restored in the 7122 branch | Binary |
| Monitor power states, CR36 bits 5:4 | Every non-zero value is standby | Standby, suspend and off are distinct | Manual |
| Panel downscale off | No extra write | Also clears CR89 bit 0 | Source |
| Panel power sequencing | Hardware | Hardware | Source |
| DisplayPort | None | Its own register block | Manual |

**Notes.**

- SR C8 is the Back Light Clock Divider Control (Manual). It matters only with a panel; a CRT or DVI board loses nothing if it is left alone.
- A timing routine without the CR45 bits truncates wide modes silently. Add them before enabling the driver's own timing for 7122.
- The flip recipe is unchanged: CR0C, CR0D, CR48, then CR34 last (Source, same code for all three chips). VIA's CE driver flips through the registers at +214h and +218h instead and waits on +200h bit 1, +204h bits 1 and 2 and CR61 bit 5; those definitions are the same in all three manuals.

**On the board (2026-11-13, VXTIME v1.1, 1280x1024 from an 800x600 16-bit desktop).**

| Timing | Measured | PLL written (SR44 / SR45 / SR46) |
| --- | --- | --- |
| BIOS's | 59.97 Hz | 97 / 08 / 05 |
| Driver's | 60.00 Hz, picture steady and complete | B5 / 88 / 86 |

The driver's PLL words, written as computed, lock on the VX900: the VX855 rule holds (Board). This is one size on one board. The gate in `vxd_main.c` still gives the VX900 the BIOS's timing; 1280x1024 needs none of the CR45 bits, so the wide modes remain untested.

## 6. 2D engine

Three of four sources say the VX900 has the same 2D engine (M1) as the VX800 and VX855, at the same offsets. The fourth, one routine in VIA's XP miniport, uses the register layout of the older chips. The board probe in section 11 settles it; the expected answer is that the driver's existing 2D code runs unchanged except for the colour-key polarity.

### 6.1 The register map

VIA's VX900 manual lists the 2D registers at the offsets the driver already uses (Manual, Part I, table 9):

| Offset | Register | Offset | Register |
| --- | --- | --- | --- |
| 00 | Command | 2C | Line error term |
| 04 | Mode and status | 30 | Colour format conversion |
| 08 | Pitch and frame buffer location | 40 / 44 | Clip top-left / bottom-right |
| 0C | Dimension | 48 | Colour key control |
| 10 | Destination position | 4C | Foreground, destination key, fill colour |
| 14 | Destination base | 50 | Background, source key |
| 18 | Source position | 58 / 5C | Pattern foreground / background |
| 1C | Source base | 60-9F | Reserved |
| 20 | Pattern address | 100-1FF | Colour pattern RAM |
| 24 / 28 | Mono pattern 0 / 1 |  |  |

Supporting evidence:

- OpenChrome installs the same fill and copy functions for VX800, VX855 and VX900, with no chip branch inside them. Fill writes 04, 14, 08, 10, 0C, 58, 00; copy writes 04, 1C, 14, 08, 18, 10, 0C, 00. Bases and pitches are in 8-byte units, width and height are less one (Source).
- VIA's CE 6.0 driver gives all three chips the same compiled solid fill, blank rectangle and fast 8-bit copy. The VX900 reuses the VX855's mono transfer and pattern fill. Only the screen-to-screen copy and the 8-bit transfer are the VX900's own (Binary).

### 6.2 The XP miniport's other sequence, decoded

The handoff's open question was the 7122 branch of one XP miniport routine, which writes `+04,+34,+38,+30,+2C,+08,+0C,+10,+00` where the VX800 and VX855 write `+04,+14,+08,+1C,+48,+18,+10,+0C,+00`. Set side by side with OpenChrome's register names for the pre-M1 engines, the two sequences are the same operation, role for role, in the same order:

| Step | Role | M1 offset (1122 / 5122 branch) | Offset in the 7122 branch | Older layout's name for that offset |
| --- | --- | --- | --- | --- |
| 1 | Mode | 04 | 04 | GEMODE |
| 2 | Destination base | 14 | 34 | DSTBASE |
| 3 | Pitch | 08 | 38 | PITCH |
| 4 | Source base | 1C | 30 | SRCBASE |
| 5 | Key control (0) | 48 | 2C | KEYCONTROL |
| 6 | Source position | 18 | 08 | SRCPOS |
| 7 | Destination position | 10 | 0C | DSTPOS |
| 8 | Dimension | 0C | 10 | DIMENSION |
| 9 | Command | 00 | 00 | GECMD |

Grade: Inferred from the offsets and their order; the values the 7122 branch writes have not been read.

So the 7122 branch is the routine's path for the older engines (CN896 class), not a third layout. Two readings remain:

1. The ID test in that routine admits only 1122 and 5122, and 7122 falls through to the old path by omission. The routine is a miniport rectangle copy, not the GDI path, and may never run on a VX900.
2. The VX900 also decodes the old offsets. OpenChrome has one hint: on the VX900 only, it writes 0 to register 60h with the comment "410 redefine 0x30 34 38", and 30, 34 and 38 are exactly the base and pitch offsets of the old layout.

On an M1 engine the old offsets mean something else (30 is colour conversion, 2C the line error term, 08 the pitch), so the probe cannot give an ambiguous picture: one sequence fills the rectangle and the other does not.

### 6.3 Fields that differ from the VX855

| Register | VX900 | Rule |
| --- | --- | --- |
| 48, bit 13 (key type) | Programmable: 0 writes where the pixel matches the key, 1 where it differs. VIA's CE copy sets 6000h for a keyed copy on the VX900 and 4000h on the VX800 and VX855. | On the board 4000h and 6000h both skip the key colour (6.6): keep 4000h, no gate. |
| 04, bits 1:0 (rotation) | VIA's CE copy clears them whenever it sets the depth on the VX900; on the older chips it preserves them. | Write the whole mode word with rotation 0. |
| 04, bits 20 and 19:12 | Alpha source select and constant alpha, for the new command below. The same table also calls bits 31:10 reserved. | Leave 0. |
| 00, command 0011 | AlphaBLT, ROP CC only. Reserved on the older chips. | Not used. A later experiment. |
| 30 | Colour format conversion as on the VX855, 16 and 32 bits per pixel, with an 8-bit source code listed and the VX855's "no 8-bit" note absent. | Not used. Does not solve 8-bit DIB to 16-bit screen until tested. |
| 08, location 11b | L.L., as on the VX855 | No change. |
| 60, 6C | Reserved | See section 7. |

### 6.4 What the port needs

- If the probe confirms M1: no new register table. One chip-gated value (the key control) and the engine model in `verification/engine.py` gains the key polarity only.
- If the probe shows the old layout works and M1 does not: a second offset table for the same operations, as cookbook 36.2 describes for the CN896, and a model branch.
- OpenChrome's start-up for all three: write 0 to 04 through 40 and to 44 through 5C. On the VX900 it also writes 0 to 60h (Source). Do not copy that write until the probe shows a need.

### 6.5 The probe (.599, VX92D v1.0)

The VxD carries a probe that works with the 2D engine off, so the layout question is answered before the driver depends on it. It runs on all three chips; a VX800 or VX855 is the control (M1 must draw, the older layout must not).

**Recipe.**

- Work in the first 16 rows of the screen, 256 pixels wide. Save them, fill them with a background value, run one command, count pixels, put the rows back. Hide the cursor over the area for the duration.
- With the engine off, open it the way the 2D code does (sequencer unlock, the enables in SR1A and SR1E), save the 19 engine registers and write them back afterwards. With the engine on, go through the 2D code's own open and close.
- Place the fill at (8,4), 240 by 8. If the engine decodes the other layout, position and size swap roles and the stray write is a 9 by 5 block at (239,7), still inside the saved rows. The result reports the bounding box of everything that changed, so the swap is recognisable.
- The older layout's fill: 04 mode, 34 base, 38 pitch with bit 31 set, 0C position, 10 size, 18 colour, command F0002001h. Registers 34 and 38 are read first and written back.
- Keyed copy: 64 pixels alternating the key colour and another, copied with 4000h and then 6000h in 48h; count what arrived.
- Status timing: two whole-screen inverts (ROP 55h), which need no saved area and leave the screen as it was.
- Bound every wait (400,000 status reads). On a command that does not finish, pulse the 2D reset in SR4E and report whether it worked.

| Control | Step |
| --- | --- |
| FFFFFE70 | M1 fill |
| FFFFFE71 | Older layout fill |
| FFFFFE73 / FFFFFE74 | Keyed copy with 4000h / 6000h |
| FFFFFE75 | Two whole-screen inverts |
| FFFFFE7C to 7E | Result words 4 to 15 of the last step |

**Test.** `tests/vx900_probe.py`: 189 checks against the M1 model at 8, 16 and 32 bits, engine on and off. Every step leaves the frame buffer byte-identical and the registers as found.

### 6.6 On the board: the VX900 has the M1 engine

Gigabyte board, 2026-11-13, VX92D v1.1 on .600, 800x600 at 16 bits, the driver's 2D engine off (Board).

| Step | Result |
| --- | --- |
| M1 fill | 1920 of 1920 pixels, nothing outside the rectangle |
| Keyed copy, 4000h | The 32 non-key pixels copied, the 32 key pixels left |
| Keyed copy, 6000h | The same |
| Two whole-screen inverts | Both finished; about 2,500 status reads each; the screen as before |
| Older layout fill | 0 of 1920 pixels. Nine pixels changed at (239,0) to (247,0). |
| M1 fill again | 1920 of 1920 |

**Rules.**

- The VX900 runs the VX855's 2D code unchanged: same offsets, same command words, same key control. No second register table and no chip-gated key value.
- XP's 7122 sequence is not this chip's layout. The nine stray pixels are exactly what an M1 engine makes of it: the position word lands in the size register (9 wide), the size word in the position register (x = 239), and with the pitch register left at 0 every row falls on row 0.
- Bit 13 of the key control does nothing visible here. VIA's CE value of 6000h and the manual's "write if match" for 0 are both not how the chip behaves; 4000h skips the key colour.
- The sequencer is already as the 2D code's open expects: SR1A 28, SR1E 31, SR3C 1D, SR4E 00 (the engine enables set by the BIOS, no reset bits).
- The engine registers 00 to 5C all read 0 before the driver has used the engine (VXSTATE). Whether they read back what was written on this chip is not yet known; the 2D code does not depend on it, since every command writes each register it uses.

**From .601** the 2D engine is used on the VX900: `VX_chip_2d()` answers for 2D, `VX_chip_accel()` stays the 3D engine's and Direct3D's answer. Registry `ACCEL_VX900_2D` = 0 turned it off in .601 only; .602 has the engine on without the switch (section 6.7).

### 6.7 The 2D engine and DirectDraw in use (.601, on the board)

Same board, 2026-11-13, started from 800x600 at 16 bits. Every tool passed (Board).

| Tool | Result |
| --- | --- |
| VXREG v1.63 | 2D engine used, 3D engine not used, Direct3D not offered; 0 wrong |
| VX92D v1.2, through the driver's own open and close | M1 fill exact, both key values skip the key colour, whole-screen fills finish |
| VX2GDI v2.6 | 16 cases at 640x480 in 16, 32 and 8 bits: 0 differing pixels against Windows, the engine drew every case it should |
| VXGTUNE v1.5 | 0 of 17 line, fill and text cases differ; 0 DIB routes differ |
| VX2FLIP v1.0 | 15 blit cases (stretch, mirror, both colour keys, system and video memory) with 0 mismatches; 9 flips with the CRTC start as expected; 60.5 Hz |
| VXFLIPF v1.2 | Flip flags behave as on the other chips; a second DONOTWAIT flip is refused while the first is pending |

**Notes.**

- With the pitch register set by the driver, the older layout's stray write is the full 9 by 5 block at (239,7) to (247,11), as section 6.5 predicted. With the pitch at 0 it had collapsed onto row 0.
- Figures to compare with the other chips on the same tool, not yet compared: an engine line costs about 14.5 microseconds a call; a 256x256 solid fill 2.5 ms for 60; a video-to-video 320x200 to 640x400 stretch 10.2 ms.
- VXGTUNE puts the line crossovers at about 240 pixels shallow and 192 steep on this machine, against the shipped 368 and 112.

## 7. Waits and synchronisation

Reading idle from the status register at 400h carries over unchanged; the two wait registers the 3D code writes (60h and 6Ch) are the part in doubt.

**On the board (VX92D, .600).** The status reads 08000010h (0C000010h once the driver's engine is in use) on the first read after a burst of register writes, before and after a command alike, and passes the driver's idle test from the next read on when nothing is running. A small fill shows the 2D busy bit (02h) at the first read and is idle by the third; a whole-screen fill is busy for about 2,500 reads. So bit 4, the command regulator's busy bit, is briefly set by the register writes themselves, and bit 27 is set at rest and is outside every mask. Consequence: the quick two-read idle check before a command will often fall through to the full wait of 32 idle reads on this chip. It is correct, only slower per operation; measure it with VXGTUNE before changing anything.

**Measuring it (.602).** VX92D step 7 reads the status sixteen times each after nothing, after one engine register write, after an engine register read, after a sequencer read and after the driver's flush, and reports how many first reads were not idle and how many reads it took. The VxD also counts how often the quick two-read check fell through to the full wait since Windows started. Decide from those numbers, not from the single readings above.

**Result (.602, VX900 and VX855, Board).** On both chips the command regulator's busy bit is set for exactly one status read after an engine register write: 16 of 16 tries, one read. It is never set after a register read, a sequencer read or the flush alone. The quick idle check runs before an operation writes its registers, so it never meets that read: 0 of 88,550 calls fell through on the VX900, 0 of 87,764 on the VX855. There is no cost and nothing to change; the counters were removed again in .603. The status at rest is 0C000000h on the VX900 and 0F000000h on the VX855, bits no mask tests. A whole-screen fill takes about 2,500 status reads at 800x600 on the VX900 and 970 at 640x480 on the VX855.

**What carries over.**

- VIA's XP driver uses one idle test on all three chips: `(MMIO[400h] & 00101FF3h) == 0` (Binary). Its 2D routine waits on the same mask before writing.
- OpenChrome's bits for the M1 chips, VX900 included: 2D busy 02h, virtual queue busy 04h, command regulator busy 10h, 3D busy 1FE1h (Source).
- The 2D engine code never writes 60h or 6Ch, so a 2D-only build has no wait question beyond 400h.

**What changed.**

| Mechanism | VX800 / VX855 | VX900 | Grade |
| --- | --- | --- | --- |
| Register 60h (3D/2D ID control) | Documented | Reserved (60-9F) | Manual |
| Register 6Ch (wait control) | Documented; bits 23 and 22 wait for 3D and 2D idle | Reserved | Manual |
| 2D against 3D exclusion | Software, with the wait command | Hardware. Type 11h sub-address 00h bit 17, default 1: "only one engine can work at same time". | Manual |
| Type 11h sub-address 00h, bits 15:0 | FIFO depth and critical-command control | Reserved | Manual |
| Type 11h sub-address 03h | Not this function | Fence wait mask: bit 23 3D, 22 2D, 21 video decoder, 20 HQV0, 19 HQV1, 18 to 15 DMA3 to DMA0, 8:4 wait count, bit 0 all engines | Manual |

**Reserved is not the same as dead.** VIA's CE driver uses one wait routine for the VX800, VX855 and VX900, and it writes 00C00000h to 6Ch on all three, directly or as an FE00 packet. The VX900 path also sends extra wait packets after a draw (Binary). OpenChrome writes 0 to 60h on the VX900 (Source).

**Rules.**

- Never send the VX855's Type 11h sub-address 00h word to a VX900: its low 16 bits are reserved there.
- The 3D core writes 6Ch in four places (`vx855_3d_core.c` lines 63, 755, 1183 and 2541 to 2548) and Type 10h sub-address 60h at lines 838 and 3473. Each needs a VX900 decision before 3D is enabled.
- First 3D attempt: keep the 6Ch writes as VIA's CE driver does, and confirm by reading 400h that the engine goes busy and returns to idle. If a write to 6Ch stalls or does nothing, switch to the fence mask at Type 11h sub-address 03h.
- Keep every wait bounded, as now. VIA's XP polling loop has no timeout of its own.

## 8. Command transport and the ring

The ports and packet framing are the same as the VX855's; the command regulator's control words and the ring's wrap are not. Bring 3D up through the register path first and leave the ring for last, as the VX855 port did.

### 8.1 The same on all three

- Ports: 41Ch selects and 420h carries the regulator's Type 10h and 11h words; 43Ch selects and 440h carries parameters. FE02 and FE03 packets use them the same way (Binary, XP and CE).
- Context start-up, identical in VIA's three XP builds (Binary):

| Port | Value |
| --- | --- |
| 41C | 00110000 |
| 420 | 06000000 |
| 420 | 07100000 |
| 41C | 00100000 |
| 420 | 02000000 |
| 41C | 00100000 |
| 420 | 69000000 |

- Ring start: Type 10h sub-addresses 60h to 67h, then the trigger 680A0001h to 420h (Binary).
- The read side of 43Ch is the fetcher's current address in all three XP ring managers (Binary).

### 8.2 Different on the VX900

| Item | VX855 | VX900 | Grade |
| --- | --- | --- | --- |
| Type 10h sub-address 04h | Interrupt state buffer address | Command dispatch setting: bits 2:1 priority between video and graphics commands, bit 0 the graphics / HQV path | Manual |
| Type 10h 05h to 07h | Interrupt settings | Reserved | Manual |
| Type 10h 08h to 0Bh, 0Ch to 0Eh | Not these functions | Preemption settings; CRMI settings | Manual |
| Type 10h 65h, pause ID 01b | Wraps at once | A jump point that waits for a later pause-address command with ID 00b or 10b | Manual |
| Type 10h 68h bit 5 | Not present | Header-1 width select | Manual |
| Type 10h 78h to 7Ch (second command queue) | Documented | Absent | Manual |
| Type 10h 60h bits 1:0 at ring start | 01b in the driver's ring today | 01b in XP's 7122 branch too: no change | Binary |
| Ring rewind alignment in CE | 256 bytes | 64 bytes | Binary |
| XP queue fields (+4060 / +4064) | FF | 7F, as on the VX800 | Binary |
| XP reserve routine | Default | An alternate one for 7122: an 80h-byte margin before the buffer end, 43Ch read twice in a row | Binary |
| Paused-fetcher recovery (444h bit 31) in CE | Used | Skipped | Binary |
| Extra status read in CE's ring-space code | None | 47Ch, meaning unknown | Binary |
| Extra start-up writes in XP | None | A block gated by capability bit 00400000h, after the seven words above | Binary |
| FE8x branch packets | Allowed | Forbidden while virtual queue 1 is on | Manual |

**Rules.**

- The seven start-up words are safe to share. The VX855's value for Type 10h sub-address 04h is not: on the VX900 the same address steers command dispatch.
- The wrap is the part to rewrite. The driver's ring wraps the VX855's way; on the VX900 the same pause ID parks the fetcher until another pause address arrives.
- VIA's drivers still send 69000000h on the VX900 although its manual has no sub-address 69h. A missing manual entry does not make a write wrong.
- The ring's sequencer registers (SR 6A, 6B, 6C, 6F, 7B in the driver) have not been compared with the VX900 manual's translation-table registers. Do that before the ring build.

## 9. 3D

The pixel side of the 3D engine looks shared with the VX855; the vertex side is redesigned around a vertex program and is where nearly all the new code goes. VIA's drivers name the split: H6 for the VX800 and VX855, H5S2 for the VX900 (CE); class 4 family 13 against class 4 family 5 (XP).

### 9.1 Expected to carry over

| Item | Evidence | Grade |
| --- | --- | --- |
| Type 01h render state, Type 02h texture state | The same templates in CE's H6 and H5S2 blits; one OpenChrome emitter for all three chips | Binary, Source |
| Pixel program words (Type 03h sub-types 20h to 22h, Type 01h 90h to 9Ah) | Byte-identical tables in CE's H6 and H5S2 blits. The VX900 manual marks them reserved. | Binary against Manual |
| Primitive control (Type 04h 20h, 23h, 24h) | The same literals in CE: 201C5C00, 23002E02, 24000003. Primitive type in bits 3:0, shading in 14:12. | Binary, Manual |
| Clip-to-screen scale and offset (Type 04h 40h to 52h) | Same registers, plus the vertex-program constants below | Manual |
| Separate stencil byte addressing, 8 by 8 tiles | The same formula in all three XP builds | Binary |
| Coarse-Z tag reset, 29008000h | The same in all three XP builds | Binary |
| Idle test at 400h | Section 7 | Binary |

### 9.2 Vertex input: the rewrite

On the VX855, Type 04h sub-addresses 00h to 10h list fixed attributes in order and 21h to 22h give the vertex buffer's base and pitch. On the VX900 the same sub-addresses describe inputs to a vertex program (Manual, Part II):

| Word | Bits | Meaning |
| --- | --- | --- |
| 04h:00h | 6:0 | Total vertex length in dwords |
|  | 11:8 | Number of inputs, less one |
|  | 16:15 | Memory region of the vertex buffer |
|  | 23:18 | NaN tests on X, Y, Z, W and X/Y swap |
| 04h:01h to 10h, one per input | 23:20 | Stream number |
|  | 19:12 | Offset in the vertex, 4-byte units |
|  | 11:8 | Vertex program input register, IR0 to IRf |
|  | 7:0 | Format; bits 7:6 give 1 to 4 components |
| 04h:12h |  | Mask of input registers fed from vertex data |
| 04h:21h, 22h |  | Reserved (the VX855's base and pitch) |
| 03h sub-type 30h |  | 16 stream entries: base in 1 KB units, pitch in 4-byte units |

Two complete settings from VIA's CE driver, both for pretransformed, textured vertices (Binary):

|  | One texture | Two textures |
| --- | --- | --- |
| 04h:00h | 00A80106 (6 dwords, 2 inputs) | 00A80208 (8 dwords, 3 inputs) |
| Input words | 010000C2 (IR0, four floats), 02000142 (IR1, two floats) | The same, plus 03000242 (IR2, two floats) |
| 04h:12h | 12000003 | 12000007 |
| 04h:60h / 61h / 62h | 60800001 / 61800000 / 62104001 | 60800002 / 61800000 / 62204002 |
| Vertex program | 8 dwords under 03h sub-type 31h | 12 dwords |

### 9.3 The vertex program

VIA's drivers load a vertex program for every VX900 draw, pretransformed vertices included. VIA's XP driver uploads it like this (Binary):

```
FE020440  0000043C  00040000  CC000000
04h:61h   04h:62h   04h:60h   [CC000000 padding]
FE03043C  4*N       31030000  CC000000
[N records of four dwords]
```

- 60h bits 8:0 hold N less one, with bit 23 set.
- 62h bits 8:0 hold the index of the last record in which one of the first three dwords satisfies `(word & 00780000h) == 00080000h`.
- 61h bit 23, bits 22:20 and bit 19 come from the program's header fields.
- Constants go under sub-type 32h (floats; XP's selector is 32030400, 12 dwords at a time) and 33h (integers). The stream table goes under selector 30030000, 16 dwords.
- The viewport scale and offset must be written twice: to the clip registers and to program constants c256 and c257 (Manual).

**The gap.** The instruction encoding is not documented: the manual names sub-type 31h in its summary and reserves it in the detail, and reserves Type 04h 53h to 62h. All that exists is two fixed programs in CE (8 and 12 dwords) and whatever XP's generator produces. The driver cannot write new vertex programs; it can only replay known ones.

**CE's two programs** (Binary; CE 6.0 retail `DDI_VIA.dll`, data at 1000870Ch and 1000872Ch). One record of four dwords per input:

```
0008014C 00000000 00000000 0008021F    input IR0 (position, four floats)
00084124 00000000 00000000 00080283    input IR1 (texture 0, two floats)
00088124 00000000 00000000 00080293    input IR2 (texture 1; the 12-dword program only)
```

Each record looks like a move from one input register to one output: bits 15:14 of the first dword count 0, 1, 2 with the input register, and the low byte of the fourth dword (1F, 83, 93) changes with the destination. That reading is Inferred. The code for a colour output is not known, so a vertex with diffuse colour cannot be built from these two programs alone.

### 9.4 Two routes to a first triangle

1. **Direct vertices, the old way.** OpenChrome draws its textured quads on all three chips with one routine and no chip branch: a Type 00h packet, control words at sub-addresses ECh and EEh, then X, Y, W and up to two texture coordinate pairs per vertex. If the VX900 accepts that, pretransformed drawing needs no vertex program. OpenChrome's 3D clients on these chips are software, so this path is known to compile for the VX900, not known to draw on one (Source).
2. **CE's short setting, replayed.** The one-texture column of 9.2 with CE's 8-dword program. It needs the program's bytes from `DDI_VIA.dll` (data at 1000870Ch in the CE 6.0 retail build), which are listed in 9.3.

Try route 1 first: it costs one test tool and no new data.

### 9.5 Smaller differences

- Occlusion count: the VX855's Type 01h 21h to 22h writeback is gone. The VX900 has counters under Type 08h 01h to 03h (Manual).
- VIA's XP defaults turn on hardware clipping, vertex buffers and index buffers for the VX900 and leave them off for the other two (Binary, INF).
- XP's allocator uses 1024 where the other two use 256 in one geometry field for family 5 (Binary).
- The OpenGL driver's fast vertex paths (`gl/vxgl_*.c`) write the VX855 vertex format directly, so they need the VX900 encoder as well as the Direct3D path.
- The VxD's packet checks validate the VX855's vertex words. A VX900 encoder needs its own checks beside it.

### 9.6 The first-triangle test (.604, VX93D)

The VxD's old 3D test endpoint still holds the VX855's original first-triangle test: a 64 by 64 32-bit target in a scratch area at 2 MB of video memory (saved and restored), a 2D fill check, the FE block, the render state, a triangle, and the target read back by the CPU. It does not depend on the driver's 3D path being ready. From .604 its last step can be words supplied by the tool, so each vertex input form is tried without a new VxD.

**Recipe.**

- Request: probe 1, `lab_cmd` = 'RAW3', then up to 512 pairs of port and value. Only ports 41Ch, 420h, 43Ch, 440h and 6Ch are accepted; a pair with port FFFFFFFFh means wait for idle there.
- The fixed steps run first and can be skipped one by one with flag bits. The words follow, then a bounded wait, then the readback.
- A form that leaves the engine busy latches the test's fault. The next request asks for the 3D reset (SR4E bits 5:4, then SR1A bit 6) before it runs.
- The desktop must end below the scratch area: 800x600 at 16 bits or smaller.

**The forms VX93D sends**, each for the triangle (8,8) (56,8) (8,56):

| Form | Vertex input |
| --- | --- |
| A | The VX855's words: five a vertex. The control; it must draw on a VX800 or VX855. |
| B | VIA's CE setting for the VX900, complete (below) |
| C | B without the 786432.0 |
| D | B with plain primitive words (04/20 = 0, 04/23 solid) |
| E | B with 04/61, 62, 60 in XP's order |
| F | OpenChrome's direct form: EC006800, EE020000, X Y W a vertex, EE120300 twice |
| G | B, then 6Ch = 00800000h, the wait word CE sends after a VX900 draw |

**CE's VX900 setting in full** (Binary, CE 6.0 `Texture3D_H5S2`), all Type 04h unless noted:

| Word | Meaning |
| --- | --- |
| 00A80106 | Six dwords a vertex, two inputs |
| 010000C2, 02000142, 03000000 | IR0 four floats, IR1 two floats, third unused |
| 12000003 | IR0 and IR1 come from vertex data |
| 201C5C00 | Primitive cycle (the VX855's CE path sends 20000000) |
| 25F0C000, 28F0C000 | Attribute masks: X Y Z W and texture S T, no colours |
| 2A3F8000, 2B3F8000 | Point size defaults |
| 2F582000 | Defaults: W = 1, colour and alpha = 1, texture Q = 1 |
| 60800001, 61800000, 62104001 | Vertex program controls |
| Selector 31030000h at 43Ch, then 8 dwords | The vertex program of section 9.3 |
| 23002E02, 24000003 | Flat shading from vertex B; three vertices |
| Vertex | X + 786432.0, Y + 786432.0, Z = 0, W = 1.0, S, T |

**New facts from the CE reading.**

- CE adds 786432.0 to X and Y in software on the VX900. On the VX800 and VX855 the same offset sits in the clip-to-screen registers (04h 41h/42h and 44h/45h).
- CE's FE block for the VX900 (000E1004, 08000249, ... 47000404) is word for word the block the driver already sends on the VX855. CE's H6 block differs in the first word (000E1804) and zeroes 14h to 19h.
- After a VX900 draw CE sends 6Ch = 00800000h eight times, then 00FC0000h.

**Host test.** `tests/vx900_tri.py`: the words reach the ports in order after the fixed steps, a bad port is refused before anything is written, the scratch area is put back. The host has no model of what the 3D engine draws; that is the board's answer.

**On the board (.604, VX93D v1.0, 800x600 at 16 bits).** The 2D fill check passed before every form. No form drew.

| Forms | Result | Status at 400h |
| --- | --- | --- |
| A (VX855 words), F (OpenChrome direct) | Engine left busy; recovered by the 3D reset each time | 00080021 |
| B, C, D, E, G (CE's descriptors and 8-word program) | Words taken, engine idle, 0 pixels changed | 00080031 seen, then idle |

**What this settles.**

- The VX900 does not take the VX855's five-word vertices, and it does not take OpenChrome's direct form after this set-up. Both leave the engine waiting for vertex words. A vertex program is needed for pretransformed drawing.
- CE's descriptors count the vertex words right: six a vertex, three vertices, and the engine finishes.
- 6Ch = 00800000h after the draw (form G) changes nothing seen here.

**Failed approach: CE's draw words without CE's Type 04h table.** v1.0 sent only the descriptor, program and primitive words. CE's VX900 path first copies a 32-word Type 04h table (the same table its VX855 path copies), and that table holds the clip-to-screen scale and offset. Left at the BIOS's zeros, the scale is 0.

| Words of CE's table (Type 04h) | Meaning |
| --- | --- |
| 04 to 0D, 21, 22, 26, 29, 2C, 2D, 2E = 0 | Cleared state |
| 2A3F8000, 2B3F8000, 2F582000 | Point size and attribute defaults |
| 40800000, 41400000, 4200493F | X scale 1.0, X offset 786432.0 |
| 43800000, 44400000, 4500493F | Y scale 1.0, Y offset 786432.0 |
| 46800000, 47000000, 4800003F | Z scale 1.0, Z offset 0 |
| 50000000, 51000000, 52147FFF | Not yet named |

**VX93D v1.1 (driver still .604).** Forms H to N send the table before form B's words:

| Form | Change from B |
| --- | --- |
| H | CE's whole Type 04h table |
| I | H without the 786432.0 added in software |
| J | I with 01h/01h bit 20 (clip-to-screen) on |
| K | H with 01h/01h bit 20 on |
| L | H at Z = 0.5 |
| M | H with 01h/01h = CE1140 (the driver's draw enable word) |
| N | H with 01h/01h = CE1540 (CE's) |

**On the board (.604, VX93D v1.1, from a restart, 800x600 at 16 bits): the first triangle.**

| Form | Result |
| --- | --- |
| B, C, D, E, G (no table), run before any hang | Nothing drawn. The reset after a hang was not the cause of the v1.0 result. |
| H (table, 786432.0 added in software) | Triangle drawn: 1176 pixels, exactly (8,8) to (55,55), white |
| I (table, no software offset) | Nothing |
| J (I with 01h/01h bit 20 on) | Nothing |
| K (H with 01h/01h bit 20 on) | Drawn as H |
| L (Z = 0.5) | Drawn as H |
| M (01h/01h = CE1140) | Drawn as H |
| N (01h/01h = CE1540) | Engine left busy, status 00000701 |
| F (OpenChrome direct), run last | Engine left busy, status 00080031 |

**Recipe: a pretransformed triangle on the VX900 (Board).**

1. The VX855's set-up as it stands: command path, FE block, render target, Type 01h state.
2. CE's 32-word Type 04h table (above).
3. The descriptors: 00A80106, 010000C2, 02000142, 03000000, 12000003; 201C5C00; masks 25F0C000 and 28F0C000; 60800001, 61800000, 62104001.
4. Selector 31030000h at 43Ch, then the 8-word vertex program. Wait for idle.
5. 01h/01h enable word (40h and CE1140 both draw), 23002E02, 24000003, Type 00h.
6. Six words a vertex: X + 786432.0, Y + 786432.0, Z, W = 1.0, S, T.

**Rules.**

- Add 786432.0 to X and Y in software. The offset in 04h/41h-42h and 44h-45h is not applied to these vertices, with 01h/01h bit 20 on or off.
- With no colour in the vertex the triangle is white: the default of 04h/2Fh.
- Do not send 01h/01h = CE1540 with this set-up. It differs from CE1140 in bit 10 and leaves the engine busy.
- OpenChrome's direct form (EC006800, EE020000) is not usable after this set-up, from a clean engine either.

**VX93D v1.2 (driver still .604).**

| Forms | Question |
| --- | --- |
| H, N, H | Does the engine draw again after the reset that follows a hang? |
| O, P, Q | Which words of the table are needed: only 40h-48h, all but those, only 50h-52h |
| R to X | Vertex colour: a D3DCOLOR as the second input (00A80105, 020001D1, masks F40000), the program's second instruction 0008414C 0 0 000802nF with n = 2, 3, 4, 5, 6, 7, 0 |

The colour forms rest on a reading of CE's two programs, not on the manual: the last word of an instruction is 0008021F for the position and 00080283, 00080293 for the two texture sets, which looks like an output number in bits 7:4 and a write mask in bits 3:0; the first word carries the input register in bits 15:14.

**CE 7 check (BSP 7.0.0.71, `DDI_VIA.dll`, Binary).** The CE 7 driver holds the same data as CE 6: the 8-word and 12-word vertex programs (VA 1000BACC, 1000BAEC), the 32-word Type 04h table (1000BB60), and after it the Type 01h words that start 11000000, 10000000, 01CE1540, 150000FF, 14FFFFFF, 13070000. CE sends CE1540 together with its depth buffer words (01h/10h, 11h, 13h), which the test does not send; that fits form N's hang. The package has no Direct3D or OpenGL driver and no third vertex program, so it says nothing about a colour output.

**On the board (.604, VX93D v1.2).**

| Forms | Result |
| --- | --- |
| H after N's hang and the 3D reset | Drawn. The reset (SR4E bits 5:4, then SR1A bit 6) restores the engine on the VX900. |
| O (only 04h/40h-48h) | Nothing |
| P (table less 40h-48h), Q (only 50h-52h) | Drawn |
| R (colour to program output 2) | Drawn in the vertex colour, E04020, all 1176 pixels |
| S to X (colour to outputs 3, 4, 5, 6, 7, 0) | Drawn white; no hang |

**Rules added.**

- Of CE's table only 04h/50h = 0, 51h = 0, 52h = 147FFFh are needed for the triangle to appear. The clip-to-screen words 40h to 48h do nothing for these vertices. Send the whole table anyway: it is what VIA sends.
- The diffuse colour is vertex program output 2. A coloured vertex is X Y Z W and a D3DCOLOR: 04h/00h = 00A80105, 04h/02h = 020001D1, masks 25F40000 and 28F40000, second instruction 0008414C 0 0 0008022F.
- Instruction words as read so far: the first word carries the input register in bits 15:14 (and 14Ch for a four-part input, 124h for a two-part one); the last word carries the output in bits 7:4 and a write mask in bits 3:0. Outputs: 1 position, 2 diffuse colour, 8 and 9 texture sets 0 and 1.
- A hang leaves the test's fault latched across runs of the tool. The tool's first request must ask for the reset when it meets the latch.

**VX93D v1.3 (driver still .604).**

| Form | Vertex |
| --- | --- |
| 1 | R with red, green and blue vertices and Gouraud shading (04h/23h = 004E02) |
| 2 | Seven words: X Y Z W, colour, S T. 00A80207, 03000242, 12000007, masks F4C000, 60800002, 62204002; three instructions |
| 3 | Direct3D's own vertex, eight words: X Y Z W, colour, specular, S T. 00A80308, 030002D1, 04000342, 1200000F, masks F6C000, 60800003, 62304003; four instructions, specular to output 3 (a guess) |

**A better source for the vertex program.** `VIA Graphics Driver Group2.msi` also holds `d3dm_via.dll`, VIA's Direct3D Mobile driver, in a debug and a retail build. It was not read for the earlier research. It builds vertex programs in code (`VS.cpp`, `InvVS.cpp`; four words an instruction, named `Src0`, `Src1`, `Src2`, `Misc_Dest` in its debug text) and uploads them with selector 31030000h. Read it for the output numbers of specular, fog and further texture sets before guessing them on the board.

**First reading of `d3dm_via.dll` (debug build, Binary).** The driver serves 3371 and 3225 as one class, 1122 and 5122 as a second, 7122 as a third. For the VX900 it turns every vertex format, fixed-function ones included, into a vs\_1\_1 token stream (it starts FFFE0101), compiles that to hardware instructions, and uploads them after 04h/60h, 61h, 62h.

| Fact | Where | Against this driver |
| --- | --- | --- |
| An instruction is four words in memory order Src0, Src1, Src2, Misc\_Dest | The dump code at 10038152 | Agrees with the reading used for forms R to X and 1 to 3 |
| Output numbers: position 0 in the compiler's table (1 in CE's hardware words), diffuse 2, specular 3, two more colours 4 and 5, fog 6, point size 7, texture set n at 8 + n | The destination switch at 10083EA0 | Diffuse 2 is board-proven (form R); texture 8 and 9 match CE's programs; specular 3 is what form 3 sends; fog and point size are not yet tried |
| VX900 start-up block (Type FEh): 000E1004, 08000249, 0A0002C9, 0B0002FB, 0C000000, 0D0002CB, 0E000009, 10000049, 110002FF, 12000008, 130002DB, 20000000, 40008408, 44000000, 46000000, 47000404 | 1002429D | Word for word the block this driver sends on every chip |
| Before that block, on the VX900 only: selector 00010000h, then every Type 01h register from 00h to 9Ah written as zero | 1002425F | Not done here; the triangle draws without it |
| Generic start-up block for every other chip (on the VX855 a later routine at 1002693F overwrites its fields with 249, 2C9, 2FB, 2CB, 2FF, 2DB, 8 and E0000 in the first word, which are this driver's values; on the VX800 it stands): 00001004, 01040000, 0800004B, 0A000049, 0B0000FB, 0C000001, 0D0000CB, 0E000009, 12000000, 130000DB, 200F0000, 40008408, 44000000, 46000000, 47000400 | 100243B2 | VX855: agrees after the override. VX800: differs; this driver sends the VX855's block there too and it is board-proven, so it stays; the difference is recorded, not acted on |

Not yet read: the instruction encoder itself (how a compiler output number becomes the bits of Misc\_Dest, and the source word's fields), the rule for 04h/60h to 62h with more than three inputs, and the per-chip Type 01h, 02h and 03h state words.

**Later tests (low priority, with the VX855 and VX800 runs at the end).**

- A/B on the VX800 (and the VX855 for completeness): CE's generic start-up block (00001004, 01040000, 0800004B ... 47000400) against this driver's. CE is a different, older codebase, so the difference is not treated as a fault; the test only shows whether it matters.
- On the VX900: CE's clearing of Type 01h registers 00h to 9Ah before the start-up block, with and without.

### 9.7 Differences between VIA's CE drivers, VIA's XP driver and this driver

Three codebases for three systems. A chip-level encoding (a register's fields, an instruction word) carries from one to another. Driver policy (start-up order, defaults, what is cleared) does not: a difference there is a note until the XP driver or a board says otherwise. Order of trust for the VX800 and VX855: the board, then XP, then CE. For the VX900 there are no XP files yet, so CE is the only vendor source and each thing taken from it is board-tested first.

CE DDI = `DDI_VIA.dll` (display driver, CE 6 and CE 7). CE D3DM = `d3dm_via.dll` (Direct3D Mobile, built from a CE 5 tree).

| Item | CE DDI | CE D3DM | XP | This driver | Standing |
| --- | --- | --- | --- | --- | --- |
| Start-up block (Type FEh), VX900 | 000E1004 ... 47000404 | The same 16 words | No files | The same 16 words | Agree; board draws |
| Start-up block, VX855 | A table with 14h-19h zeroed, 4000840F, 46000005; first word 001C1004 in CE 7 (000E1804 read in CE 6) | Generic block, then overwritten to this driver's values | 5122 branch: this driver's words | 000E1004 ... 40008408, 46000000, 47000404 | Agrees with XP and D3DM; DDI differs in 00h, 40h, 46h and 14h-19h. Not tested |
| Start-up block, VX800 | Older values: 0800004B, 0A000049, 0B0000FB, 0C000001, 0D0000CB, 110000FF, 130000DB, 200F0000, 4000840F, 47000400 | The same family, 40008408 | Not compared | The VX855's block | Differs from CE; board-proven as it is. Later A/B |
| Clearing state before the start-up block | Clears Type 01h, 02h, 04h groups and sets Type 03h tables on every chip | VX900: Type 01h 00h-9Ah written as zero | Not compared | Not done | Later test |
| Draw enable word 01h/01h | CE1540, sent with the depth buffer words 10h, 11h, 13h, 14h, 15h | Built from state | CE1140 with depth bits by state | CE1140 with depth bits by state | VX900: CE1140 draws; CE1540 without the depth words hangs (form N) |
| Vertex input, VX800 and VX855 | Attribute-order words (04h/00h = word count, 01h, 02h) | The same kind | The same kind | The same kind | Agree |
| Vertex input, VX900 | Descriptors, 04h/12h, 04h/60h-62h, one of two fixed programs (position and texture only) | Any format: a vs\_1\_1 stream compiled to instructions | No files | None yet; forms R and 1 to 3 of VX93D | Board: position, diffuse colour |
| The 786432.0 XY offset | VX800/VX855: in 04h/41h-42h and 44h-45h. VX900: added in software | A 786432.0 constant in its viewport routine (not read further) | Not compared | VX855: by the registers. VX900 test: in software | VX900 board: the registers do nothing, software is required |
| Type 04h table before a draw | 32 words, the same for VX855 and VX900 | Built from state | Not compared | Not sent on the VX855; the VX900 test sends it | VX900 board: 04h/50h-52h are required |
| Clip limits | Not read | ±32677 / 32676 for 3371, 1122, 5122; ±2047 / 2046 for the rest, 7122 included (100A4989) | Not compared | Not compared | Open: what this driver uses, and whether the VX900 needs the smaller limits |
| 2D source-key control at 48h | 6000h on the VX900, 4000h on the others | n/a | 4000h (VX855) | 4000h on all three | VX900 board: 4000h is right |
| 6Ch = 00800000h after a VX900 draw | Sent eight times, then 00FC0000h | Not read | No files | Not sent | VX900 board: no effect seen (form G) |
| Overlay | VX900: its own initialisation | n/a | Not compared | VX855's path plus 0 to 3A4h (OpenChrome) | VX900 board: all twelve steps right |
| OpenChrome's direct vertex form (EC006800, EE020000) | Not used | Not used | Not used | Not used | VX900 board: hangs the engine |

**Still to compare** (chip-level only): the Type 01h depth, stencil, alpha, blend and fog words; the Type 02h and 03h texture words and the pixel program words (CE sends 01C00530, 301C0053, 00F20005, 000D0820, 301001B0 and 90h-9Bh for one texture); the rule for 04h/60h-62h with four or more inputs; the instruction encoder.

### 9.8 VIA's XP driver for the VX900 (20.16.01f, files built 11 April 2013)

Now in hand; this fills the XP column for the VX900. First look only (Binary).

| File | What it is | First findings |
| --- | --- | --- |
| `S3gIGPmA.sys` | Miniport | Holds the VX900 start-up block (000E1004 ... 47000404) and the older block (0800004B ...); compares the device against 7122, 5122 and 1122 throughout, so one binary serves all three chips |
| `S3gIGPA.dll` | Display driver (2D, DirectDraw) | Not read yet |
| `s3ginvA.dll` | Direct3D driver | Uploads vertex programs with selector 31030000h. Holds about 90 ready-made vs\_1\_1 shaders for fixed-function vertex formats, from `m4x4 oPos, v0, c8; mov oT0.xy, v1` up to lit, fogged, two-texture ones |
| `s3gIGPglA.dll` | OpenGL driver | The same start-up blocks and the same selector |
| `VTROM.bin` | Video ROM, VT3410, dated 03/15/10 | Not read yet |

**What follows from the shader library.** On the VX900 VIA's XP driver does the fixed-function transform and lighting in the chip, by vertex programs: position by a 4x4 matrix at constants c8 to c11, lighting into oD0, fog into oFog, texture coordinates moved to oT0 and oT1. Pretransformed vertices need only the moves. Hardware transform and lighting is therefore possible on this chip, which it is not on the VX800 and VX855.

**To read next, in this order:** the routine in `s3ginvA.dll` that turns a vs\_1\_1 instruction into the four hardware words (this gives the encoder and settles position 0 against 1); how it sets 04h/00h to 04h/12h and 04h/60h to 62h for a given format; where the 786432.0 is added; the 7122 branches of the miniport.

### 9.9 Vertex formats proven on the VX900 (.604, VX93D v1.3)

All four forms drew the triangle, 1176 pixels each, no hang.

| Vertex | 04h/00h | Descriptors 01h to 04h | 04h/12h | Masks 25h, 28h | 04h/60h, 61h, 62h | Program (first and last word of each instruction; the middle two are 0) | Board |
| --- | --- | --- | --- | --- | --- | --- | --- |
| X Y Z W, colour (5 words) | 00A80105 | 010000C2, 020001D1, 03000000 | 3 | F40000 | 60800001, 61800000, 62104001 | 0008014C / 0008021F; 0008414C / 0008022F | Vertex colour; with 04h/23h = 004E02, Gouraud: corners EA0A0A, 0AEA0A, 0A0AEA, the middle 555555 |
| X Y Z W, colour, S T (7 words) | 00A80207 | 010000C2, 020001D1, 03000242 | 7 | F4C000 | 60800002, 61800000, 62204002 | as above, then 00088124 / 00080283 | Vertex colour |
| X Y Z W, colour, specular, S T (8 words, Direct3D's D3DTLVERTEX) | 00A80308 | 010000C2, 020001D1, 030002D1, 04000342 | F | F6C000 | 60800003, 61800000, 62304003 | position; 0008414C / 0008022F; 0008814C / 0008023F; 0008C124 / 00080283 | Vertex colour |

**Rules.**

- With n inputs: 04h/00h = 00A80000 + (n - 1) x 100h + words a vertex; 04h/12h has one bit an input; 04h/60h = 60800000 + (n - 1); 04h/62h = 62004000 + (n - 1) x 100000h + (n - 1). Proven for n = 2, 3, 4.
- An instruction's first word is 00080000 + input x 4000h + 14Ch (a four-part input: position, D3DCOLOR) or 124h (two floats).
- Gouraud shading is 04h/23h bits 14:12 = 100, as the manual says; CE's flat value is 010.
- Direct3D's pretransformed vertex goes to the chip as it is, but for the 786432.0 added to X and Y. Whether the fourth word wants W or 1/W, and whether the texture and specular outputs reach the pixel stage, is not shown by an untextured triangle.

**Not yet proven:** a textured draw, depth, specular add, fog, blending; a vertex program that adds the XY offset itself (so vertices can be copied unchanged, as this driver's fast paths do on the VX855).

### 9.10 The vertex program's instruction words

Read from VIA's own code generator: the debug build of CE's `d3dm_via.dll` run in an emulator on small programs (`tools/via_vs_gen.py`; the routine at 10095220 and its per-operation handlers). Grade Binary until a board run confirms each part; the parts used by forms R and 1 to 3 are already board-proven.

**An instruction is four words:** three sources, then the operation and result.

| Field of a source word (words 0 to 2) | Bits | Values |
| --- | --- | --- |
| Class | 22:19 | 1 vertex input, 2 temporary, 4 special, 7 viewport constants, 8 + n/32 constant register n (number = n mod 32) |
| Number | 18:14 |  |
| Which part goes to x, y, z, w | 13:11, 10:8, 7:5, 4:2 | 0 x, 1 y, 2 z, 3 w; 4 in all four with class 4 number 1 is VIA's constant 1.0 (00206490) |
| Negate | 0 |  |

| Field of the last word | Bits | Values |
| --- | --- | --- |
| Operation | 21:17 | 4 move, 2 multiply-add (add, sub, mul, mad), 3 dot product, 8 max, 9 min, 10 sge, 11 slt, 16 exp, 18 log, 20 reciprocal, 21 reciprocal square root, 24 lit, 26 closes a matrix product |
| Clamp to 0..1 | 14 | VIA sets it when the result is one of the two colours |
| Also keep as the unprojected position | 11 | Set on VIA's write of oPos |
| Class of the result | 9:8 | 2 output, 1 temporary, 0 address |
| Number | 7:4 |  |
| Parts written | 3:0 | x = 1, y = 2, z = 4, w = 8 |

**Outputs.** 0 the position before the screen transform; 1 the position on the screen; 2 diffuse; 3 specular; 6 fog; 7 point size; 8 + n texture set n.

**What VIA appends to every compiled program** (after `mov oPos, v0` became 0008014C 0 0 00080A0F):

| Words | Meaning |
| --- | --- |
| 00201B6C 0 0 00280108 | temporary 0.w = 1 / position.w |
| 00200150 00101B6C 00201B6C 001C0107 | temporary 0.xyz = position.xyz scaled by that |
| 0010014C 00380150 00384154 0004021F | output 1 = temporary 0 x viewport constant 0 + viewport constant 1 |
| 0020014C 0 0 0008020F (twice) | output 0 = position |

So the screen transform, and with it the 786432.0, is done by the program on this chip; that is why the clip-to-screen registers do nothing. It also says the fourth word of the screen position is 1/W, which is what Direct3D's pretransformed vertex carries. CE's display driver skips all of this and writes its ready-made screen position straight to output 1.

**Constants.** Selector 32030000h + 4 x register at 43Ch, then four floats a register (the pixel side's 2203xxxxh form). VIA also writes 32030400h, 32030424h and 32030428h: registers above 255, not yet read; the viewport constants are probably among them.

**Recipe: add the XY offset in the program.** One instruction in place of the position move: 0008014C 00206490 0040014C 0004021F (output 1 = input 0 x 1.0 + constant 0), with constant 0 = (786432.0, 786432.0, 0, 0) loaded by selector 32030000h. Vertices then go to the chip unchanged. VX93D v1.4 forms 4 and 5 test it (constant loaded before and after the program).

**On the board (.604, VX93D v1.4).** Forms 4 and 5 drew the orange triangle from unchanged vertices. Proven: the multiply-add instruction, the constant 1.0 source, constant registers by selector 32030000h (loaded before or after the program, either works), and the clamp bit on the colour results. The recipe above is the VX900's way to place pretransformed vertices; this driver's copy-through vertex paths can be kept.

### 9.11 What the driver's Direct3D path still needs from the VX900

The driver's 3D layer (`vx_native3d_hw.c`) sends its state and vertices through the command ring, uses a list word of 0 and a strip word of 386300h at 04h/20h, fetches indexed vertices from video memory, and has several vertex layouts (5, 7, 8, 10 and 12 words). Each is a chip-level question on the VX900. In test order:

| Question | Test | Result |
| --- | --- | --- |
| List word 04h/20h = 0 | VX93D v1.5 form 6 | Works (Board) |
| Depth test (01h/01h = 4Ch, LESS, 32-bit depth buffer) | forms 7 and 8 | Right both ways (Board) |
| Strip word 386300h | form 9 | Works: the square, 2304 pixels (Board) |
| The driver's own state set-up with the pixel program on (01h/01h = CE1540; bit 10 is the pixel program's enable, which is why form N hung with no program loaded), 04h/23h = CE02, and the driver's depth words | VX93D v1.6 forms Y, Z, 0: setup() of vx\_native3d\_hw.c word for word, with the VX900's vertex words |  |
| A textured draw with the VX855's texture and pixel program words; 1/W in the fourth word | Needs texels written by the VxD: .605 |  |
| Vertex layouts of 10 and 12 words (two and three texture sets) | By the rule of 9.9; .605 |  |
| Indexed vertices fetched from video memory | .605 or later |  |
| The command ring (section 8.2) | The ring tools, after the above |  |

**On the board (.604, VX93D v1.6).** Forms Y, Z and 0 were right: the driver's whole state set-up, its untextured pixel program with 01h/01h = CE1540, its primitive word CE02 (Gouraud) and its depth words work on the VX900 with the VX900's vertex words in place of the VX855's.

### 9.12 Recipe: the VX900 in the driver's 3D layer (.605, under test)

1. **One function writes the vertex input** (`v900_layout` in `vx_native3d_hw.c`). It takes the VX855's attribute list of the layout (0 to 3 position, 5 diffuse, 6 specular, 8 to 0Bh, 0Ch to 0Fh, 10h to 13h the three texture sets) and writes the VX900's words by the rules of 9.9 and 9.10: 04h/00h, a descriptor an input, 04h/12h, the masks (a bit an attribute at 23 minus its code, as on the VX855), 04h/60h to 62h, constant 0 = (786432.0, 786432.0, 0, 0), and a program of one instruction an input. The position instruction adds constant 0; the two colours are clamped; a texture set of three parts is a three-float input written with mask 7.
2. **It is called where each VX855 layout is written**, once a packet, at the layout the draw ends with: 5 words (plain), 7 (one texture, no program), 10 (the pixel-program path), 9, 12 and the mixed two-set forms.
3. **The vertex words are not touched.** Every vertex path of the VX855, the copied ones included, serves the VX900.
4. **Register writes only.** On the VX900 `vx3dn_fetch` is 0: no command fetch, no ring, no ring flips, clears or copies. Indexed packets are expanded inline (no vertex area).
5. **The gate** is `VX_chip_accel()`. From .605 to .616 it followed the registry value ACCEL\_VX900\_3D on the VX900 (the values named below: 1 register writes only, 3 with the fetcher and ring, + 8 without the wait words). From .617 it answers 1 for the VX900 as for the other two chips, and the VX900 always runs what value 3 selected. The HAL offers Direct3D from the same answer.

**Known gaps in .605.** Lines rewrite the clip-to-screen registers for a half-pixel offset on the VX855; on the VX900 they still get the triangle's constant. Fetched vertices, the ring, and whether the texture and specular outputs reach the pixel stage are untested. `tests/vx900_d3d.py` checks the words (16 checks); what the chip draws is VXDX7's answer.

**On the board (.605, ACCEL\_VX900\_3D = 1, 800x600 at 16 bits, VXDX7 v2.88): Direct3D works on the VX900.** All three runs finished with 0 failures, no fault, no engine reset.

| Run | What passed |
| --- | --- |
| `--triangle` | Flat and Gouraud triangles, read back |
| `--draw` | The same with a 32-bit depth buffer: a near triangle rejects a later far one |
| `--texture` | Point and bilinear sampling; wrap, mirror and clamp; textures from 2x2 to 2048x2048; A8R8G8B8, X8R8G8B8, R5G6B5, A1R5G5B5, X1R5G5B5, A4R4G4B4, L8, DXT1, DXT3, DXT5, V8U8, L6V5U5, X8L8V8U8; mip chains with point and linear mip filters and LOD bias; anisotropic filtering; every tested colour and alpha operation; specular; alpha blending; texture colour keys; two and three stages on coordinate sets 0 and 1; DOTPRODUCT3; bump mapping; a projected texture |

**What this proves (Board).**

- The VX855's Type 01h, 02h and 03h words carry over to the VX900 unchanged: render state, depth, blending, texture set-up, mip chains, the pixel programs and their constants.
- Texture set n is program output 8 + n for two sets; a three-part set (s, t, q) as a three-float input written with mask 7 gives the projected texture.
- Specular is output 3.
- The fourth vertex word is 1/W, as Direct3D gives it.
- 04h/60h to 62h by the rule of 9.9 hold for five inputs (the ten-word vertex).
- The vertex program may be loaded with the pixel program enabled.

**Still open on the VX900:** points and lines (the half-pixel offset), stencil, flips, OpenGL, a game; then fetched vertices and the command ring, which is where the speed is. Second round: VXDX7 `--raster`, `--stencil`, `--splane`, `--flip`, `--wintune`, and VXGL.

**Second round on the board (.605, VXDX7 v2.88, VXGL v5.7).**

| Run | Result |
| --- | --- |
| VXDX7 `--raster` | 0 failures, 68 checks: blending, alpha test, depth formats, clears, points, lines, fills |
| VXDX7 `--stencil` | 0 failures |
| VXDX7 `--flip` | 0 failures |
| VXDX7 `--splane` | 1 failure, the tool's: it asks for a registry switch the driver no longer has. Not the chip |
| VXDX7 `--wintune` | 5 failures, all in set-ups with no back buffer (AddAttachedSurface and Clear2 refused), the same on the software device. Not the engine; to be compared on the VX855 |
| VXGL, the engine against the software path | Every judged scene under 1%; unfilled polygons 0.96%, the largest. Lines and points 0.02% |
| VXGL, occlusion query | Wrong: no count came back (4294967295 samples) |

**Rules added.**

- Lines need no separate offset on the VX900. The VX855 switches its clip-to-screen offset to 0 for lines; the VX900 draws them right with the triangle's constant.
- **Failed approach: the VX855's occlusion read on the VX900.** The VX855 has the engine write its count to an address given in 01h/21h-22h. The VX900's manual lists the count (01h/1Ch-1Dh) but not those two registers, and the board wrote nothing. From .606 the VxD refuses a query's begin on the VX900, so the OpenGL driver counts in software, as it already does when another context holds the count. How VIA's XP driver reads the count on this chip is not yet read (`s3ginvA.dll`).
- OpenGL needs nothing of its own for the VX900: it draws through the same 3D layer.

**Confirmed (.606, VXGL v5.7, VXREG v1.68).** 0 failures: the occlusion query gives 10000 and 5000 by the software count; every judged scene is under 1% as before. Direct3D and OpenGL on the VX900 by register writes are complete at the level of the test tools. Next: the command fetcher and the ring (section 8.2), first with VXRING as it stands: read only, `fill` (a 2D fill fetched from video memory, one-shot and ring forms), `3d` (a triangle fetched).

**The command fetcher on the board (.606, VXRING v2.8 as it stands for the VX855).** No hang in any step.

| Test | Result |
| --- | --- |
| 1: a 2D fill fetched one-shot from video memory (10h/60h-65h, 68h = 0A0001h) | Filled |
| 7: the ring's start block alone | Idle, nothing drawn, as it should |
| 8: a 2D fill in the ring form, with the kick | Filled |
| 5, 9, 10, 11, 15, 17: a triangle fetched one-shot, in the FE02 / FE03 block forms, the VX900's vertex words inside | Same checksum as the register path (524EB4E0) |
| 12, 13: state by the registers, only the vertex blocks fetched | Same checksum |
| 16: as 12 without the 41Ch write before the start words | Nothing drawn. This is the VX855's known rule, and the test is there to show it |
| 5 as the first 3D fetch after start-up, before any register-path draw (in the `fill` run) | Nothing drawn; drew in the `3d` run. On the VX855 the same test behaved this way before the 41Ch rule was found |

**What this says (Board).** The VX900's fetcher takes the VX855's start words, block headers (FE02 register blocks, FE03 data blocks, CC padding), pause address and kick unchanged, and the selectors 31030000h and 32030000h pass through a fetched block. The manual's different account of the ring has not shown up yet. Still to run: VXRING's append, kick, long, xp, stale, backlog, restart and wrap groups (tests 18 to 52), which are the ring proper and XP's form with its page table.

**Failed approach: VXRING's groups 18 to 52 as a ring test.** All eight groups came back "result 1" with every field zero: the VxD refused them. Its ring test endpoint keeps only operations 0 to 4 (the read and the simple fill and 3D forms); the append, kick, long, xp, stale, backlog, restart and wrap experiments were taken out of the VxD once the production ring was built from them. They test nothing on any chip now. The ring is tested by running the driver's own path.

**.607: the fetcher and ring selectable on the VX900.** ACCEL\_VX900\_3D bit 1 (value 3) leaves `vx3dn_fetch` on, so draws go through the command fetcher and XP's ring as on the VX855; value 1 stays register writes only. The vertex area stays off either way. Host: `tests/vx900_d3d.py` checks that the constant and the program are in the fetched stream as data blocks under selectors 32030000h and 31030000h and that the vertex words are the VX855's (24 checks). Board test: the Direct3D and OpenGL runs again with value 3. If the ring hangs, the driver marks it dead and falls back to one-shot groups from video memory, the form VXRING already proved on this chip.

**On the board (.607, ACCEL\_VX900\_3D = 3, the command fetch on).** VXDX7 `--triangle`, `--draw`, `--texture` (114 checks) and `--raster` (68 checks): 0 failures, no fault. VXGL: 0 failures, the same scene differences as by register writes, the occlusion query right. VXGL's speed scene (a lit sphere, 12288 triangles): 8.6 ms a frame against 14.6 ms by register writes on the same board and desktop; the software path takes 21.6 ms.

So fetched draws work on the VX900 with the VX855's path. Not yet shown: which form ran. The driver uses XP's ring when it starts and one-shot groups from video memory when a ring is given up as hung, and these tools print neither. VXREG v1.70 reads the counters (packets fetched, packets sent by the registers, the ring's starts, kicks and wraps, rings given up).

**The ring's counters on the board (.607, VXREG v1.70).** XP's ring started once on the VX900, took 369 kicks and wrapped twice, then one ring was given up as hung; after that the driver ran one-shot groups. No packet went by the registers and no fetch was left unfinished, and every test passed. So XP's ring runs on this chip with the VX855's start, kick and wrap, but not without fault: something after the second wrap, or a particular group, hangs it. The VxD keeps the first hang's record (status, fence, the groups kicked before it and their words); VXD3DMON prints it. Open until that record is read.

**The ring's wrap on the VX900: what the CE driver does (.608).** Board, .607 with `ACCEL_VX900_3D` = 3: XP's ring started once, took 369 kicks and wrapped twice, then one ring was given up as hung. The CE 7 display driver (`DDI_VIA.dll`, wrap routine at 100B4D50, its caller at 100B5500) treats the VX900 differently from the VX800 and VX855 in three places, all selected by the device ID:

|  | VX800 / VX855 | VX900 |
| --- | --- | --- |
| Padding before the wrap (`CCCCCCC7`) | to 256 bytes | to 64 bytes |
| Jump address (Type 10, 66h/67h) | padded end − 10h | padded end − 40h |
| After the fetcher is in the new lap | nothing | 66h/67h written again as FFFFFFF0 (the jump address taken away) |

CE also reads register 47Ch on the VX900 only, as a second read pointer, before taking the jump address away. The wrap packet itself (`FE020420`, 41C, `00300000`, `CC000000`) and the order of the writes (41C = 00100000, then 66h, 67h, 64h, 65h) are the same on all three. This fits the VX900 manual's note that the jump point there pauses the fetcher until the next pause address, where the older chips jump at once.

.608 does both on the VX900 only: the jump address is the wrap point − 40h, and after the first group of a new lap has finished the driver writes 66h/67h = FFFFFFF0. The VX800 and VX855 words are unchanged. First attempt of three at the ring hang; the XP driver's own ring rules (`s3ginvA.dll`) have not been read for the VX900 yet and rank above CE's if they differ. Not yet done as CE does: the 47Ch read.

**Board, .608, and the cause of the ring hang (.609).** With .608 the ring took 225 kicks and 8 wraps: 7 good, then a hang. The record shows what went wrong at the eighth wrap:

- Before the new lap's first kick was written the fetcher already read ring + AC0h with the engine busy (400h = 00001711). The pause written with the wrap was ring start − 10h, so it should have been standing at the ring's start.
- The fence went back from group DEh to D1h: the fetcher ran the last lap's first two groups a second time.
- It hung at ring + 13C0h, in words that were half the old lap's and half the new group's.

So on the VX900 the fetcher does not stop at the ring's start for a pause of start − 10h after its jump, as the VX800 and VX855 do. A wrap is good only when the new lap's first kick is written before the fetcher reaches the jump, which is the usual case and why most wraps pass. The pause after a jump has to be an address the fetcher reaches by reading.

Both vendor drivers have such a form. XP's VX900 driver (`s3ginvA.dll`, wrap at BFA90) has a branch that writes a batch end at the ring's start, pads with `CCC00004` and pauses at its end − 10h. CE always writes 80h bytes of `CCCCCCC7` at the start and pauses after them.

.609, VX900 only: the wrap writes one 256-byte block at the ring's start (batch end, then `CCC00004`, the same form as a kick's end), the wrap's pause is start + F0h, and the lap's groups begin at start + 100h. Before the first wrap everything kicked is read first, because the block goes over the first lap's first words. The jump address stays at wrap point − 40h (8 jumps taken with it on the board). The .608 write of 66h/67h = FFFFFFF0 after a wrap is taken out: it could land after the next wrap and remove that wrap's jump address, and with a wrap point that is the same every lap it is not needed. Second attempt of three.

Failed approach, for the record: a pause of start − 10h with the wrap on the VX900 (the VX855's and XP's plain form).

**Board, .609: the wrap is good; a second cause of ring hangs (.610).** The test tools ran 249 wraps with none hung, the fetcher standing at start + 100h before each new lap's first kick. Quake 2 (timedemo, 65 frames a second on the ring, 54 on the one-shot fallback) then ran 2556 wraps and hung once, 78 s after boot, not at the wrap itself:

- The driver has an old rule from the VX855 work: when the first group after a wrap is not done after 3 ms with every engine idle, its kick (64h/65h, the same pause address) is written a second time. It did that 33 times in the run.
- At the last of them the fetcher stood exactly at its pause (43Ch = pause + 10h, 444h bit 31 set, 400h = 0). After the write it read on: 43Ch = pause + AC0h, the engine busy on the last lap's words, and the ring hung. The fence had reached the group, so the group itself was finished.

Two faults. The rule never looked at the fence, so it also fired for a group that was already done. And on the VX900 a pause address written again releases a fetcher standing at that pause. Together with the .608 record this gives the VX900's pause rule: **the pause holds until the next pause-address write, whatever its value, and the fetcher only stops at a pause it reads up to.** The VX800 and VX855 compare the position, so the same address written twice does nothing there. This matches the VX900 manual's wording for pause ID 00b ("until the pause address changes").

.610: on the VX900 a kick is never written twice; on the VX800 and VX855 the second kick is written only when the fence has not come. Third change for the ring hangs.

**Large indexed packets on the VX900 (.611).** The VX900 has no fetched-vertex form in the driver yet, so every indexed packet is sent with its indices expanded into 10-word vertices. One of more than about 790 indices is then more words than a 32 KB fetch group holds. The VX855's answer (wait until the vertex area's slots are free, build again by index) frees nothing on the VX900: in 120 s of Quake 2 on .609, 1076 packets waited and all 1076 then went through the registers, about 12 ms each. .611 gives such a packet one group of the largest size (80 KB) instead, with no wait in the ring. The measure on the board is VXD3DMON's line "indexed packets with no free slot": the second number should be 0.

**Not built yet: vertices fetched by index on the VX900.** What the manual gives (Part II, pp. 71, 160 and the Type 04 chapter): 04/20 bit 0 = index mode, bit 1 = 32-bit indices; each descriptor 04/(01+i) carries a vertex buffer ID in bits 23:20 and the attribute's offset in the buffer in bits 19:12 (4-byte units); 04/00 bits 16:15 = where the buffers are (00 video memory); the buffers' base and pitch are a 16-entry table under Type 03 subtype 30h (selector 30030000), base in bits 31:10 in units of 1024 bytes, pitch in bits 8:0 in 4-byte units; the largest and smallest index have their own Type 04 words (settings 13 and 14). A buffer therefore has to start on a 1024-byte bound (the VX855's vertex area uses 256). XP's VX900 driver writes selector 30030000 with a 16-bit offset in its low half through an FE03 block; the table's entry form has not been read out of it yet. This is the largest speed item left on the VX900: Quake 2 sent 17.4 million indices as 174 million vertex words.

**Board, .610 and .611: fetches that hang in the middle of a group (open).** GTA3's first runs on the VX900 and a Quake 2 run on .610:

- The wrap and the second kick are no longer involved: 1327 wraps in Quake 2 on .610, 0 kicks written again.
- A third kind of hang remains. The fetcher stops in the middle of a group's vertex words with the engine busy (status 000815B1 and 000817B1). It happened once in Quake 2 on .610 (6 groups after a ring start), once in the ring in GTA3 on .611, and then in one-shot groups: 237 in 80 s of GTA3 on .610, 510 on .611. Each costs about 240 ms and two engine resets; GTA3's gameplay ran at a median of 7 to 11 frames a second.
- Not the cause: .611's 80 KB groups (the same hangs on .610, where no group is over 32 KB) and flips in the ring (none ran in the .610 GTA3 session).
- After the engine resets the hardware cursor is wrong on the VX900: in Quake 2 the Windows pointer showed over the game's; after GTA3 a large block of the frame buffer's first rows follows the pointer in Y only, and a mode change does not clear it. The VX855's reset (SR4E bit 4, then SR1A bit 6) and its cursor set-up afterwards are not enough on this chip.

.612 changes nothing in the drawing. It records where the fetcher stood at the first four hangs of any kind with 896 words before the position and 128 after (VXD3DMON prints them), and VXREG lists the display registers 200h to 2FCh and 120Ch, to be compared between a clean start and the state after GTA3.

**Board, .612: the hang follows the vertex data.** GTA3, 583 hangs in 144 s. The first was in the ring; the driver reset the engine and sent the same packet again one-shot, and it hung again at the same vertices (the two records hold the same triangles, a fraction of a pixel apart). So the engine stops on particular triangles, whatever the transport. Around both positions the triangles are small and thin: slivers some ten pixels long and a few hundredths of a pixel wide, from far-away models (Z about 0.996 to 0.998). None has two equal vertices. The fetcher's position is only known to 64 bytes and may be ahead of the word the engine is on, so the record does not name the triangle.

.613 adds `VX93D HANG`: the 53 triangles around two of the hang positions, sent through the 3D lab one at a time and eight at a time, with the driver's state and 10-word vertices, without fog and with the hung group's fog words. VXREG's register list is corrected: .612's list on a clean start was read before the 3D layer had mapped the registers and is not the registers.

**Board, .613: the triangles alone all draw; CE's wait words (.614).** `VX93D HANG` sent the 53 triangles 120 times (alone and eight at a time, with and without fog): none stopped the engine. So no triangle does it by itself; the hang needs the stream around it. In the stream each burst of vertices is followed at once by the next burst's head (04/23, 04/24), a texture switch or the next state set-up, at the fetcher's speed.

VIA's CE driver has something for exactly this, on the VX900 only. Its VX900 draw builder ends every draw with a block it names `VX900_WAITPATCH` (CE 6 `DDI_VIA.dll`, 10066438): eight 2D blocks `FE000000 00000001 00000000 CC000000 0000006C 00800000` (register 6C, wait for the 3D engine) and a ninth with 6C = `00FC0000`. The VX800 and VX855 path ends with the plain batch end. A CE draw is one burst of vertices.

.614 sends that block after every burst of a fetched packet on the VX900 (72 words a burst of up to 48 vertices). `ACCEL_VX900_3D` bit 3 (value 11) leaves it out for the comparison. Not yet board-proven: that register 6C means the same on the VX900 (its manual marks 60h to 9Fh reserved) and that this is what stops the hangs.

The display registers 200h to 2FCh on a clean start are now on record (VXREG v1.76). Against the list taken after GTA3 on .612, 2C0h reads 01070002 clean and 01070003 after; the two runs were at different colour depths, so the comparison is to be repeated at one depth.

**Board, .614: the wait words stop the hangs.** GTA3, 122 s: 409,846 packets fetched, 1 fetch not finished (583 on .612). Gameplay ran at a median of 19.6 frames a second (8.3 on .612), the 99th percentile frame 86 ms (548 ms). The pointer was right after the game and the display registers 200h to 2FCh read the same before and after. So on the VX900 a burst of vertices has to be followed by CE's wait block before anything else reaches the engine: this is a rule of the chip, now in the cookbook. Cost seen: 499 words of heads and padding a packet (136 before), and 5,578 of 45,507 large indexed packets no longer fit one fetch group and went through the registers.

**The one hang left on .614 and .615: a second wrap before the fetcher reached the lap (.616).** After 3,975 wraps (.614) and after 18,543 (.615, GTA3's intro) one ring was given up with every engine idle: the fetcher paused (444h bit 31) at ring start + 100h, and the 6 or 13 groups kicked beyond it unread. In both records the writer's last group ended at ring start + 100h: after those groups it had wrapped a second time. The second wrap's pause, start + F0h, was the pause in force when the fetcher came round from the lap before, so it stopped at the ring's start in front of a whole lap it had not read. The VX800 and VX855 compare positions and take a pause behind them as passed; the VX900 stops at whatever pause is in force when it reads up to it. .615's guess (the fetcher waiting at its jump for one more pause write, answered by writing the pause again) was wrong: that write never ran in two sessions, because the driver's own pending address was already start + 100h, and it is taken out. .616: on the VX900 a wrap is written only once the fetcher is in the writer's lap; if it is not, everything kicked is waited for first. VXREG prints how many wraps waited.

The VX900's pause rules together: a pause holds until the next write of 64h/65h, whatever its value; the fetcher stops only at a pause it reads up to, and it stops at any pause in force that it reads up to, also one meant for a later lap. So no kick twice, a wrap pause the fetcher reaches by reading (start + F0h), and no second wrap while the fetcher is a lap behind.

**.615 on the board (GTA3), and flips in the ring.** With the ring alive for a whole session (149,030 groups, 19,515 wraps, no hang; gameplay median 17.4 fps) every flip ran in the ring (2,370) and the picture flashed often; in the intro (3,486 ring flips) four or five times. On .614 the ring was given up after 16 s, the CPU flipped from then on and nothing flashed. The ring flip is the VX855's: a 2D block with 6Ch = 1 (wait for IGA1's vertical blank), then the command regulator's 10h/11h/12h address words, with the clear and the draws of the next frame behind it in the ring. Neither part is proven on the VX900, and CE flips there from the CPU (`FlipPrimarySurface` polls 400h, then writes the start address). .616 does not flip in the ring on the VX900; the flip is the CPU's after the work in flight is done. Open: which of the two parts fails, if a ring flip is wanted back.

**.616 on the board (GTA3, two sessions): no flashing, no hang.** Gameplay: 127,405 groups, 16,438 wraps, 68 of them waited for the fetcher's lap, 0 hangs, 0 fetches unfinished, ring on to the end; median 17.2 fps (.615: 17.4). Intro: 103,002 groups, 14,124 wraps, 0 hangs; median 26.4 fps. Both rules stand as proven: one wrap at a time, and flips from the CPU. Cost seen: the first draw of each frame waits for the CPU flip to take effect, 4.9 ms a frame in gameplay (10.0 s of 106 s). The primitives VXD3DMON counts as left out in the intro (7,380 here, 13,782 on .615) are the game's own vertices with x, y or z not finite or rhw 0 (drop reason 2000), not the ring.

**.617: the switch is gone.** ACCEL\_VX900\_3D is no longer read; the fetcher, the ring and the wait words are always on for 1106:7122. The register-only start and the build without wait words (583 hangs in 144 s of GTA3) are not selectable any more. A value left in the registry is listed by VXREG as no longer read; `VXREG CLEAN` deletes it. Still open on the VX900: fetched vertices by index (large packets fall to the registers), the first draw's wait after a CPU flip, the pointer after an engine reset, clip limits, the occlusion count, its own mode timing.

**.617 on the board:** GTA3 (140,407 groups, 18,513 wraps, 0 hangs, gameplay median 18.2 fps) and the Quake 2 demo through OpenGL (222,939 groups, 21,419 wraps, 0 hangs, 0 packets through the registers). In Quake 2 the wait words are about half of each packet: 1,505 words of burst heads, headers and padding against 1,355 vertex words, at 18.7 bursts a packet.

**.618: the general fixes of .598 and .599 merged into the port tree** (cookbook section 37): the second display unit (IGA2) for start address, blank and pointer; the three standard sizes inside a sparse EDID's native size; IGA2's blank bit read clear; the tool VXPANEL. The source parts applied without conflict; none touches the 7122 paths. On the VX900 these are assumed, not measured: CR62-64 and CRA3, CR66/67/71, CR6A, CR97, CR99, CR36, the icon unit at 260h/224h/208h/20Ch, and the sense of MMIO 200h bit 1 and 204h bit 2. VXPANEL on the VX900 measures the status bits.

**.619: two probes, no change to drawing.** `VXREG RESET` (VXREG v1.82): one engine reset as after a hang (SR4E bit 4, then SR1A bit 6) with SR 00-FF, CR 00-FF, MMIO 200h-2FCh, 120Ch and 400h read before and after, every differing register listed, and the person asked whether the pointer is right before, after, and after a shape change (log VXRESET.LOG; TEXDBG BC C00 runs it, A00.. and B00.. read the two snapshots). `VXRING vblank` (VXRING v2.9): an 8x8 fill fetched one-shot with a 2D block writing 6Ch before it, 60 times each for 0, 1, 2, 00800000h and 00FC0000h; the time from the start to idle and port 3DA bit 3, MMIO 200h bit 1 and the scan line when done (request flag 10000h on op 1, the value in `mode`). A wait that holds spreads the times over a frame. Not built yet: the VX900's fetched vertices by index; XP's VX900 driver writes selector 30030000h at s3ginvA.dll C2FDE, C5769, C7518 and C7748, which have to be read first.

**.619 on the board: register 6Ch in a fetched stream** (VXRING vblank, 800x600 at 16 bits, 60 one-shot fills a value):

| 6Ch value | Start to idle | When done |
| --- | --- | --- |
| 0 | 0.02 ms, all 60 | anywhere in the frame (scan lines 8 to 616) |
| 1 | 0.02 to 16.48 ms, 8.15 on average, spread over a frame | scan line 601 every time; MMIO 200h bit 1 clear 60 of 60; port 3DA bit 3 set 0 of 60 |
| 00800000h (CE's wait block, 1-8) | 0.02 ms, all 60 | anywhere |
| 00FC0000h (CE's wait block, 9) | 0.02 ms, all 60 | anywhere |
| 2 | never idle, 0 of 60 drawn | the second display unit is off on this board |

So on the VX900 6Ch = 1 holds the fetcher until the first display unit's vertical blank begins, as on the VX855. CE's two values do not hold for the display; what they do that stops the engine hangs is not known, only that they take under 0.02 ms. MMIO 200h bit 1 reads clear in the blank, the sense measured on the NC20's VX800. Port 3DA bit 3 is not yet set when the blank begins. Never send 6Ch = 2 unless the second unit runs: the fetcher does not come back.

This also narrows .615's flashing: the wait half of the ring flip works on this chip, so the fault is in the command regulator's 10h/11h/12h address words or in what follows them. Not looked at further; flips stay with the CPU.

**.619 on the board: one engine reset does not disturb the pointer** (VXREG RESET). SR 00-FF, CR 00-FF, MMIO 200h-2FCh and 120Ch read the same before and after (only the counter at 208h moved); the pointer was right before, after, and after a shape change. The fault of .610-.612 therefore needs more than SR4E bit 4 and SR1A bit 6 on an idle engine: those sessions reset a hung engine in the middle of a game, hundreds of times. Not reproduced since the hangs stopped (.616). Open.

**.620: the blank wait in the ring after a CPU flip.** Before, the first colour draw on the old front waited in the CPU for the flip to be taken (4.2 to 4.9 ms a frame in GTA3). Now, on the VX900 with the ring running and the picture on the first unit, the driver kicks one group holding a 2D block with 6Ch = 1 the moment that draw would have waited, once a flip; everything sent after runs behind it. Kicked at once on purpose: a block the fetcher reads after the blank it was meant for holds for a whole frame. VXREG prints the blocks kicked, the packets that did not wait and the times the CPU waited after all. VXDX7 `--flip` (v2.89) now asks whether the spinning triangle was steady.

**Open: dots and thin lines where two textures meet (board, .619, GTA3).** Single light pixels and short lines that come and go, all over the game on the VX900; the owner has seen the like on the VX855, less often. The same at 16 and at 32 bits of colour, so not the depth buffer's precision. Two causes not yet told apart: a texture changed inside a packet (.528's texture runs) while the engine still draws the triangles before it, or gaps between neighbouring pieces of the map that share no vertices. .621 has a test switch for the first: registry `ACCEL_D3D_TEXRUNS` = 0 makes every texture change begin a new packet, as before .528 (the HAL's feature word bit 22; VXD3DMON then reports 0 runs added). Default 1.

**.622: the seam test, one part for each suspected cause** (`VXDX7 --seam`, v2.90; 256x192 off-screen target, every pixel read back). 1: eight stripes drawn with a green and a blue texture in turn, the texture changed between indexed draws of one scene, 40 frames at changing sub-pixel positions; counts pixels of the other texture inside a stripe, pixels not drawn and other colours, and prints how many times the driver changed texture inside an open packet. 2: a 12 x 9 mesh of shared, moved vertices drawn additively; every inner pixel must be drawn exactly once. 3: one long vertical edge against fifteen short ones that share no vertices with it; once everywhere. 4: the same edge slanted; counted, not judged, the figure to compare between the VX900 and the VX855. To run as installed, again with `ACCEL_D3D_TEXRUNS` = 0, and on the VX855.

**.622 on the board (VX900, 800x600 at 16 bits), the seam test twice.** With texture runs on (280 texture changes inside open packets) and off (0): parts 1, 2 and 3 clean both times, 1,289,280 pixels read in part 1 with none of the other texture, none undrawn. So the texture change inside a packet is not the cause, and triangles that share vertices, or a vertical edge, meet exactly. Part 4, the slanted edge whose neighbours share no vertices: 55 pixels not drawn and 25 drawn twice in 30 frames, the same both times, the gaps spaced along the edge. GTA3 still showed a short light line on the grass in that session. Reading: the game's map pieces meet along edges without shared vertices, and positions held to 1/16 pixel leave single-pixel gaps there (the VX900's XY constant 786432.0 = 1.5 x 2^19 leaves a float four fraction bits). About 2.7 stray pixels a frame on a 150-pixel edge is what 1/16-pixel positions predict. Not yet known: the VX855's count in part 4, which says whether the VX900 holds positions more coarsely than the VX855.

**Closed: the dots are GTA3's own.** The owner sees the same dots on an Intel GMA 900, in gameplay videos, and on an RTX 3060 (far fewer, about one every 10 to 30 seconds), most when the frame rate is low and the draw distance is down. The seam test read the same on the VX855 with .602 and with .622 (part 4: 55 not drawn, 25 drawn twice) and on the VX900. They are gaps where the game's map pieces meet without shared vertices; a chip that holds positions to 1/16 pixel shows more of them than one that holds them finer. Nothing to change in the driver. The test switch `ACCEL_D3D_TEXRUNS` (.621) goes again in the next build; the seam test stays as VXDX7 `--seam` (v2.91 adds part 5: the same corners sent as an indexed list against fans, a strip, a plain list and a thinly used vertex array).

**.623: three tests in one build.** `ACCEL_D3D_TEXRUNS` is gone again (texture runs always on).

1. The blank wait in the ring after a CPU flip (.620), unchanged: VXDX7 `--flip` and GTA3.
2. `VX93D INDEX` (v1.8): the driver's flat orange triangle drawn by index from a vertex buffer in the test arena (video offset 20E000h; the raw 3D hook takes port FFFFFFFEh = the buffer's next word). What the VX900's manual and XP's driver give: 04/20h bit 0 index mode, bit 1 32-bit indices; 04/21h-22h reserved; base and pitch in the stream table (parameter type 03, sub-type 30h, base bits 31:10 in 1024-byte units, pitch bits 8:0 in words), which XP writes as a data block under selector 30030000h + 8 an entry (s3ginvA.dll C7518, the same block form as the constants under 10030000h); each descriptor's bits 23:20 the buffer, 19:12 the offset in the vertex in words; 04/00h bits 16:15 the buffer's location; 04/2Ch-2Eh the largest and smallest index; 04/24h the count. Eight forms: two 16-bit indices a word; 32-bit indices; one 16-bit index a word; the colour's offset left 0; the table entry followed by three zero words; location 01; the pitch in bytes; the table's second entry. Form 0 is the control.
3. `ACCEL_VX900_WAITS` (under test): 0 CE's whole wait block after every burst (as .614), 1 its last block only (6Ch = 00FC0000h, 8 words for 72), 2 its last two, 3 the whole block after every second burst. To judge by VXD3DMON's hang counts and frame rate in GTA3 and Quake 2.

**.623 on the board (VX900, GTA3 four times, VXDX7 `--flip`, VX93D INDEX).**

*The blank wait in the ring after a CPU flip works.* VXDX7 `--flip`: every flipped frame the one just rendered at 16 and 32 bits, 57.9 and 60.1 frames a second, the picture steady by eye. GTA3: 2,481 blocks kicked, 0 times the CPU waited instead, 0 draws waited for the old front (2,356 and 9.8 s on .619); the flip's own wait for the engine rose from 1.5 to 4.3 ms, so the net is about 1.4 ms a frame; gameplay median 19.6 to 20.0 fps over the four runs (17.2 to 18.2 on .616-.619).

*The wait block can be cut to its last part.* `ACCEL_VX900_WAITS` 1 (one block, 6Ch = 00FC0000h) and 2 (the last two): 122 s and two runs of 106 s and 120 s of GTA3, 0 hangs, 0 fetches unfinished. With 1 a packet's heads and padding fall from 476 to 182 words and the packets sent through the registers from 514 to 7. The frame rate does not move (the CPU is the limit). The run meant for 3 was made with 2 still set. .624 makes 1 the default; the switch stays until Quake 2 has run.

*Fetched vertices by index, first round.* With the buffer's base and pitch in table entry 0 the triangle's shape is right (1,176 pixels, the same bounds as the control) with two 16-bit indices a word (04/20h = 1) and with 32-bit indices (04/20h = 3). One 16-bit index a word, location 01, and the pitch in bytes draw nothing. The colour was wrong in every form: 03EF03EFh, the desktop's pixels at video offset 0, whatever offset the colour's descriptor named. So each vertex input reads through its own table entry (the manual's "base address for data n"), and entry 1 was 0. .624's VX93D INDEX (v1.9) writes entry 1 as well: as a second word under 30030000h, or under 30030008h, 30030001h or 30030004h, with and without buffer 1 named in the descriptor; each form's buffer sits at its own place so that an entry left by an earlier form cannot pass.

**Seen once, not reproduced (.623, setting 2):** in GTA3's intro video, before the menu, a band about an inch high stood shifted to the right for a moment and went back. The video is the game's own 2D copies, before its Direct3D device exists, so none of the 3D changes act there. Open; to look at only if it comes again (which part of the start, and a RUN900 log of that session).

**.625: a review of the VX900 work, and the ring's size as a tuning value.** Review: the stricter compiler pass over the shared drawing code gives only layout warnings; the paths added since .608 (the lap rule, the short wait block, the blank wait in the ring, the reset and wait probes) were read again against the board's logs, nothing wrong found. Corrected: VXREG's line of fetch and ring counters said "since the VxD loaded", but VXD3DMON reads and clears those counters, which is why the two disagreed; VXDX7's banner named a driver build.

Found in the logs: with a 256 KB ring and 36 MB of command words a second (GTA3) the ring wraps 152 times a second and the writer meets the fetcher's position about 95 times a second (11,383 in 120 s); each time it waits for everything kicked, about 0.5 ms, some 5% of the run. `ACCEL_D3D_RINGKB` (256 as before, 512, 1024; read once, before the ring's memory is allocated) makes the ring longer. To judge on the board by VXD3DMON's "space checks after them ... waits they took", the wraps and the frame rate. All three chips have the value; first run on the VX900.

**.625 on the board (VX900, GTA3, 256 KB against 1024 KB, the same boot sequence each):**

|  | 256 KB | 1024 KB |
| --- | --- | --- |
| Ring hangs, fetches unfinished | 0, 0 | 0, 0 |
| Wraps | 22,122 | 6,406 |
| Space checks that waited | 13,015 | 2,427 |
| Waits for the engine in all | 16,133 | 6,062 |
| Wraps that waited for the fetcher's lap (.616) | 37 | 0 |
| VxD time a packet | 80.1 microseconds | 50.1 |
| Gameplay frame: average, median, 90th, 99th percentile | 49.9, 50, 66, 75 ms | 44.8, 43, 57, 67 ms |
| Gameplay seconds, median | 20.7 fps | 22.6 fps |

The longer ring is worth about 11% of the frame time in GTA3 on the VX900. What still waits: the flip, 4.2 ms a frame, for everything queued and the blank; and 2,427 space checks, so a ring longer than 1 MB may give a little more (a frame is about 1.8 MB of words). To come: the same pair on the VX855, whose wrap is the older form.

**.626: ring sizes 2048 and 4096 KB.** `ACCEL_D3D_RINGKB` now also takes 2048 and 4096; the size is halved when Windows cannot fix the pages, and VXREG prints the size in use for every chip. With 1024 KB, 2,427 waits for ring space remained in two minutes of GTA3 (13,015 with 256 KB), so the gain still to be had is expected to be small; the runs at 1024, 2048 and 4096 in one session will show where it flattens.

**.627: drawing by index.** Board, VX93D INDEX v1.9 on .625: the triangle came out in its colour when the table's words for both vertex inputs were written in a row under selector 30030000 (forms 1, 5, 7 and 8; 16-bit and 32-bit indices). With the second word under 30030001, 30030004 or 30030008 the shape was right and the colour came from the start of video memory (forms 2, 3, 4, 6). The driver now draws indexed packets this way on the VX900: vertices at a 1 KB boundary in the vertex area, the five descriptors with their offsets, five table words, 04/20 = 1, no 04/21 or 04/22, and the descriptors without offsets again afterwards. VXREG counts the packets drawn by index. Not yet known: whether descriptors with offsets also work for vertices in the command (then the rewrite after each packet can go).

**Ring size on the VX900: flat past 1024 KB (board, .625 and .626, GTA3).** Average gameplay frame: 256 KB 49.9 ms, 1024 KB 44.8 ms, 2048 KB 44.3 ms, 4096 KB 45.6 ms. Waits for ring space fell each time (13,015, 2,427, 878 in two minutes; 366 in 82 s) but past 1024 KB the frame time did not follow: the flip waits about 5 ms for the engine to finish whatever is queued, so a longer ring only moves the wait there. No hang at any size. 1024 KB is the size to keep; 2048 and 4096 cost memory for nothing.

**Drawing by index on the board (.627, GTA3, 1024 KB ring).** 469,071 packets drawn by index, 8,994 expanded for want of a free slot; no hang, no fetch left unfinished. Against .625 with the same ring: a packet's command words 2,399 to 540; the VxD's time a packet 43.7 to 26.1 microseconds; average gameplay frame 44.8 to 38.8 ms; median 22.6 to 25.9 fps. The seam test's part 5 (the same corners by index and by fans, a strip, a plain list and a sparse indexed list) drew every pixel exactly once; the whole Direct3D test passed.

**.628 (all three chips).** The 1024 KB ring is the default: the VX855 ran GTA3 on it with no hang, 486 wraps and 145 waits for space in two minutes (its frame rate could not be compared, the game's frame sync was on). Two tuning values under test, off until set: `ACCEL_D3D_SEAM` (each index burst of an opaque, depth-writing packet sent a second time with the screen offset moved: x + 256 x y sixteenths of a pixel; the VX855's 04/41 and 04/44, the VX900's constant 0) and `ACCEL_STRETCH3D` (stretching Blts between video surfaces drawn as a textured rectangle by the 3D engine: 1 nearest pixel, 2 bilinear). Supersampling measured and parked: the test scene took 8.6 ms plain and 25.1 ms supersampled on the VX900, 10.0 and 39.2 ms on the VX855; its VxD and core parts are in the tree and inactive (`ACCEL_D3D_SSAA` only keeps the memory).

**.629: the two lines of builds joined.** The other line's .600 (DirectX 5 and older texture render states carried to texture stage 0; the VXLEG tool) and .601 (the driver's transform and lighting on by default, for compatibility; `ACCEL_D3D_TNL` = 0 turns it off) are merged into this tree. The driver's T&L has not yet run on the VX900. Supersampling is removed again (code and `ACCEL_D3D_SSAA`): measured too costly for what it gives, 2.9 times the frame on the VX900 and 3.9 times on the VX855 in the test scene.

## 10. Video overlay and HQV

Start the VX900's overlay from the VX855's register map, not from a third one: OpenChrome treats the two alike except for one write.

| Item | VX800 | VX855 | VX900 | Grade |
| --- | --- | --- | --- | --- |
| HQV source-offset registers | Older set, 380h to 38Ch | Newer set: source crop at 370h to 37Ch, destination crop at 380h to 38Ch | Same as VX855 | Source |
| Extended HQV set-up (destination offsets, background, extended control, sub-picture scale at 3E0h and 3E8h) | No | Yes | Yes | Source |
| Extra write at set-up | None | None | 0 to 3A4h (sharpness and decoder handshake) | Source |
| Hardware flags (three HQV buffers, byte-unit fetch, two colour keys, new scale control) | Differ | Set | Same as VX855 | Source |
| Video stream registers 200h to 2FFh | 64 offsets shared | 64 offsets shared | 64 offsets shared; labels differ at 28Ch, 290h, 2CCh, 2E4h | Manual |
| New bits | None | None | 230h bit 1 (V1 scaling on the primary), 238h bit 31 and 2A8h bit 31 (length guards) | Manual |

**Cautions.**

- The VX900 manual describes HQV twice. Its video chapter repeats the VX800's labels; its later HQV chapter gives the newer ones, and the two disagree at 3A4h to 3ACh. Follow OpenChrome's writes, then the HQV chapter.
- VIA's CE drivers give each of the three chips its own overlay initialisation, and CE 7 its own show, flip, fire and wait routines (Binary). Expect differences OpenChrome does not show.
- The second HQV engine is at +1000h. The +2000h range is a different thing: a write window routed through the command regulator.
- The 16-bit destination colour key took a different form on the VX800 than on the VX855 (cookbook 34.5). Test the VX900's form with VX2OVL before assuming either.
- CE's VX900-only video decoder start-up (a 512-word boot loader and firmware copy) has nothing to do with the overlay. Leave it out.

**Recipe (.602, under test).** The VX900 runs the VX855's overlay code path with one addition: after the HQV crop and control registers, write 0 to 3A4h. The key mask starts at the VX855's value (961). `VX_chip_video()` answers 1 for 7122; registry `ACCEL_VX900_OVERLAY` = 0 took the overlay away again in .602 only. From .603 the overlay is on for all three chips with no switch, and the per-chip overlay gate is removed. Host checks: on a modelled VX900 every overlay register equals the VX855's, plus 3A4h = 0. Board test: VX2OVL, which also tries the three 16-bit key forms.

**On the board (.602, VX2OVL v1.5, 800x600 at 16 bits).** Through DirectDraw the VX900's overlay is right in every step but one: picture, 2x enlargement, flip, move, hide, YV12, shrink, RGB565, RGB32, source colour key and bob. So the VX855 map plus the 3A4h write is the VX900's overlay recipe (Board).

The one failure was the destination colour key with the VX855's widened value, and the same run on the VX855 failed the same way.

| 16-bit key written to 220h / 264h for pixel F81F | VX800 (.585) | VX855 (.602) | VX900 (.602) |
| --- | --- | --- | --- |
| 40F800F8, widened, low bits zero | No picture | No picture | No picture |
| 40FF00FF, widened, low bits replicated | No picture | No picture | No picture |
| 4000F81F, the pixel value itself | Right | Right | Right |

**Rule.** The destination key register takes the pixel value itself at every depth, on every chip, as OpenChrome writes it. The VX855's widened form was only ever proven at 32 bits, where all three forms are the same number, so its 16-bit key had been wrong since the overlay was written. From .603 the raw value is the only form; the mask's key bits and VX2OVL's three tries are removed.

**Confirmed (.603, VX900, VX2OVL v1.6, 800x600 at 16 bits).** All twelve steps right, the destination key at the first and only try. The VX900's overlay is complete. Still to run on .603: the same tool at 16 bits on the VX855 and the VX800, whose key value changed too.

## 11. Port order

Each step is one build with its own test tool. Steps 1 to 4 make a useful 2D and DirectDraw driver without touching 3D.

| Step | Change | Test tool | What it settles |
| --- | --- | --- | --- |
| 1. Bring-up | The INF line (done). No engine use. | VXREG, then VXTIME | Device found, both BARs, frame buffer size against D0F3 A1h-A0h; every listed mode on the BIOS's timings. First run of the frame buffer path on this chip. |
| 2. 2D probe | Probe controls in the VxD (.599); the driver still has 2D off | VX92D: one solid fill by the M1 sequence, one by the older sequence, frame buffer read back, 400h sampled before and after each | Which layout the chip decodes (section 6.2). Add a keyed copy with 4000h and with 6000h at 48h to settle the key polarity. |
| 3. 2D engine | Split `VX_chip_accel()` into a 2D gate and a 3D gate; turn the 2D gate on for 7122; chip-gated key control | VX2GDI, VXGTUNE | The engine against Windows, pixel for pixel, at 8, 16 and 32 bits |
| 4. DirectDraw | Flips, blits, colour keys | VX2FLIP, VXFLIPF | Flip recipe and keyed blits on this chip |
| 5. First triangle | The 3D gate for one test path only | New: one flat triangle by OpenChrome's direct form, read back; if nothing draws, CE's one-texture setting with its 8-dword program | Whether pretransformed drawing needs a vertex program (section 9.4) |
| 6. 3D state | Depth, texture, blend, stencil, in the cookbook's VX855 order | The VX855's tools for each | Which Type 01h and 02h words carry over; whether the reserved pixel program words work |
| 7. Ring | VX900 wrap and reserve rules | The ring tools, with 43Ch read back | Section 8.2 |
| 8. Direct3D and OpenGL | VX900 vertex encoder in the HAL, the VxD's checks and `gl/vxgl_*.c` | VXDX7, VXGL, then GTA3 and Quake |  |
| 9. Overlay | VX855 map plus the 3A4h write | VX2OVL | Section 10 |
| 10. Own timing | CR45, CR55, CR6B bits; 7122 added to the timing gate | VXTIME | Section 5 |

## 12. Open questions for the board

In the order the port meets them.

| Question | Why it is open | Settled by |
| --- | --- | --- |
| Is the BAR at 10h 32-bit or 64-bit on real boards? | Manual default says 64-bit; OpenChrome's use of region 1 says not | Settled on the Gigabyte board: 32-bit, the registers are at 14h (section 2) |
| Does the driver's mode set run cleanly under the patched VDD? | The driver's BIOS calls go through the VDD's trapping | Settled: the desktop and a 1280x1024 mode set ran, on the BIOS's timing and on the driver's (section 5) |
| Which 2D register layout does the chip decode? | Section 6.2 | Settled: M1 (section 6.6) |
| What does register 60h do on this chip? | Reserved in the manual; OpenChrome writes 0 with a comment about 30, 34 and 38 | Left alone: the M1 sequence works without touching it |
| Source-key polarity at 48h bit 13 | CE uses 6000h on the VX900, 4000h on the others | Settled: 4000h, as on the VX855 (section 6.6) |
| Does colour conversion at 30h take an 8-bit source? | Listed in the VX900 table without the VX855's exclusion | A later experiment |
| Does a write to 6Ch still wait for idle? | Reserved in the manual; CE writes it | Step 5 |
| Does the chip draw direct Type 00h vertices without a vertex program? | Only OpenChrome's source suggests it | Step 5 |
| What is the vertex program's output code for colour? | Two CE programs, neither with colour | XP's generated programs would have to be read out of `s3ginvA.dll` |
| Do the VX855's pixel program words work? | Reserved in the manual; CE sends the same words | Step 6 |
| How does the ring behave at pause ID 01b? | Manual describes a jump point, not a wrap | Step 7 |
| What is the status at 47Ch? | Read by CE's ring code on the VX900 only | Step 7 |
| HQV registers 3A4h to 3ACh, 3BCh, 3E0h | The manual's two chapters disagree | Step 9 |

**Still to ask for.** Received 13 November (section 9.8): VIA's XP driver files for the VX900 (`S3gIGPmA.sys` for the values the 7122 branch writes at 525D0h to 526A7h, `s3ginvA.dll` for generated vertex programs) and a VX900 video ROM dump.

## 13. Sources

| Source | Used for |
| --- | --- |
| VX900 Series Chrome9 HD Open Graphics Programming Manual, Part I (core, 2D), preliminary revision 1.0, 19 January 2011 | PCI table, sequencer 5D and C8, CRTC 3E, the 2D register table and fields |
| The same, Part II (3D, video) | Type 10h and 11h words, the fence mask, vertex input words |
| VX900 Series System Programming Manual, preliminary revision 1.41, 4 October 2011 | Frame buffer size at D0F3 A1h-A0h |
| OpenChrome source snapshot (`via_ums.c`, `via_exa.c`, `via_exa_h6.c`, `via_regs.h`, `via_3d.c`, `via_display.c`, `via_xv_overlay.c`) | BAR choice, 2D emitters and register names, direct vertices, display values, overlay map |
| `docs/VX800_VX855_VX900_XP_comparison.md` | VIA's XP drivers: the miniport 2D sequences, context start-up, ring, idle test |
| `docs/Chrome9_HC3_HCM_HD_programming_research.md` and `vx_8_VS_9_wince_re.md` | VIA's CE drivers, the manual-against-manual comparison, the vertex program bytes |
| Cookbook sections 34.1, 35 and 36 | The device profile, the boot fix, the chip lineage |
| Driver source, build .597 | Section 4 |

The cookbook's other sections were not re-read for this supplement. Line numbers are from the .597 tree.
