# VIA VX800, VX855 and VX900 XP driver comparison with Windows CE cross-checks

VX800 **`1106:1122`** selects CIL backend **4**, family **13**, and sets device-record field `+0x24` to **1**. VX855 also selects backend 4 and family 13. In the same 2009 binary its `5122` branch joins after the extra `1122` write, leaving the field at its initialized value of 0. VX900 selects backend 4 and family 5.

The closest Win98 starting point for VX800 is the current VX855 rendering path with a distinct PCI device profile. The **VX800 and VX855 IDs select the same 2D register sequence** in the corresponding miniport routine. **VX900 takes the alternate sequence** in the 2013 build. All three show the same FE02/FE03 mapped transport, the same 8×8 auxiliary stencil byte addressing and a common DirectDraw flip packet in one traced branch. The scanout latch sequence, PCI resources, effective framebuffer cache policy and hardware rendering still need board validation.

VIA's Windows CE 6.0 and CE 7 display drivers independently group VX800 and VX855 on H6 and use H5S2 on VX900. The cross-check below ties that split to the XP vertex path, distinguishes display waits from fetcher waits and records the successful startup tuple from two older Win98 VX855 hardware sessions.

## Three-way comparison

| Area | VX800 XP · 2009 | VX855 XP · 2010 | VX900 XP · 2013 |
|---|---|---|---|
| INF and device | `dc05emb.inf`, `6.14.10.0274`, Chrome9 HC3, `1106:1122` | `dc06emb.inf`, `6.14.10.0293`, Chrome9 HCM, `1106:5122` | `dc07emb.inf`, `6.14.10.0436`, Chrome9 HD, `1106:7122` |
| CIL device path | `8A2DA → 8A305`. Backend 4, family 13, `+24=1`. | Backend 4, family 13. The `5122` entry joins at the backend/family stores. | Backend 4, family 5. |
| Register aperture | Miniport `960A8` queries access ranges. `9660D` maps a resource and `96639` derives the indexed VGA window at mapped `+83C0`. | Original miniport listing shows the same VGA window derivation at `A38B9`. | Miniport `951BE` queries ranges, `9581C` maps and `9583C` adds `83C0`. |
| Framebuffer mapping | IOCTL `230474`, display call `5C315`, miniport handler `2B8F0`. Input 16 bytes, output 12. Conditional cache request at extension `+384`. | Same IOCTL, handler `2BDC0`, extension `+384`. | Same IOCTL, dispatch `30DCC → 2D170`, extension `+5B4`. |
| Display and modes | XP miniport calls `ucb_32.sys!CBiosSetMode` at `44210 → 6AAE2 → 11010`. | CBIOS module and existing Win98 analog scanout baseline. | CBIOS module and the additional sequencer `SR C8` save/restore in the `7122` lifecycle branch. |
| GDI entry points | Display CopyBits `1FF90`, BitBlt `20060`, callback calls at `2980D/29792`. | CopyBits `202E0`, BitBlt `203B0`. | CopyBits `20B80`, BitBlt `20C50`, callback calls at `2A42D/2A3B2`. |
| 2D miniport submission | `51FC0`, ID gate `52117`. Writes `+04,+14,+08,+1C,+48,+18,+10,+0C,+00`. | ID gate `52937/52948` explicitly admits `1122/5122` to the same sequence. | ID gate `524D6/524E7` admits `1122/5122`. `7122` takes the alternate sequence `+04,+34,+38,+30,+2C,+08,+0C,+10,+00`. |
| DirectDraw | Surface callback table `AB6E0`. Flip `2A390 → 31CD0`, blit `2A3D0 → 313F0`. | Flip `2A6E0 → 32040`, blit `2A720 → 31760`. Win98 CR34-last behavior provides the test baseline. | Surface callback table `B31D0`. Flip `2AC10 → 32960`, blit `2AC50 → 32050`. The cited flip loop counts linked surfaces. |
| Direct3D DP2 | `2A6D0 → 37BF0`, table `3B714`, 96 opcode slots. | `2AA20 → 37F60`, 96 slots. | `2AF50 → 38CB0`, table `3C680`, 96 slots. |
| CIL function tables | Export `7ACA0`, ten groups, 341 slots, 337 populated. | Export `7CFC0`, 341 slots, 337 populated. | Export `805E0`, 347 slots, 343 populated. |
| Mapped FE packet interpreter | `BA9C0`. FE02 control store `BAC9E`, FE03 `BAD40/BAD84`. | `BCD30`. FE02 control store `BD00E`. | `C0D40`. FE02 control store `C101E`, FE03 `C10C0/C1104`. |
| Queue progress and reserve | Mapped `+43C` read via `B97A2/B97AB → B9BCD`. Default helper `B9B00`. `7122` branch installs `B9E10`. | Mapped `+43C` read via `BBB12/BBB1B → BBF3D`. Default `BBE70`. `7122` branch installs `BC180`. | Mapped `+43C` read via `BF9F2/BF9FB → BFF0D`. Default `BFE40`. `7122/A122` branch installs `C0150`. |
| 3D idle | Selector 9 asks for `(MMIO[400] & 00101FF3) == 0`. Miniport callback `30EE0`. | Same predicate, callback `313B0`. | Same predicate, callback `32E60`. |
| Stencil clear | Backend-4 clear `AA660`. Partial CPU store `AADFC`, full helper `1AB8D0`. | Backend-4 clear `AC9C0`. CPU store `AD15C`, full helper `1AE480`. | Backend-4 clear `B05A0`. CPU store `B0D3C`, full helper `1B3700`. |
| OpenGL | ICD `43D6CD` → display `19697`, `[device+B1C]` → `133D0`. | ICD `43D6FD` → display `199E7`, `[device+B1C]` → `133D0`. | ICD `43D74D` → display `19B57`, `[device+B24]` → `13440`. |

Addresses are hexadecimal preferred PE virtual addresses. MMIO offsets, file offsets and software structure fields are identified separately. **CIL** is `s3ginv.dll` in the VX800/VX855 packages and `s3ginvA.dll` in VX900. The INF version dates identify the shipped bundles. Shared modules can contain code for several PCI IDs.

## 1. VX800 2D write sequence and its VX855 equivalent

The VX800 miniport's `58BE8` call reaches the rectangle submission routine at `51FC0`. Before the chip-specific sequence, request field `+0x38` and two globals can select a read-modify-write of bit 0 at mapped `+0x3D0` or `+0x13D0` (`52064–52104`). The routine calls status helper `51DC0` at `52108`, then takes the `1122` branch at `52117`. The helper samples mapped `+0x400` for a nonzero result under mask `0x00101FF3`, with an initial sampling limit, then polls at `51E0D–51E2E` until the masked value is zero. That second loop has no local timeout. At `9660D/96616` the miniport maps the hardware resource into extension field `+0x3D0`, the pointer used by these writes. IAT slot `6D054` resolves to `VideoPortWriteRegisterUlong`.

| Order | Register offset | Value in the traced VX800 routine | VX800 write call |
|---:|---:|---|---|
| 1 | `0x04` | `0x00000100` | `52133` |
| 2 | `0x14` | Request DWORD at `+0x0C` shifted right by 3 | `5214D` |
| 3 | `0x08` | Packed value from request `+0x28/+0x24`. Each field is masked with `0xFFF8`, then shifted left 13 and right 3, respectively, and ORed. | `52164` |
| 4 | `0x1C` | Request DWORD at `+0x08` shifted right by 3 | `5217E` |
| 5 | `0x48` | 0 | `52195` |
| 6 | `0x18` | 0 | `521BA` |
| 7 | `0x10` | 0 | `521DE` |
| 8 | `0x0C` | `(request[+0x18] - 1) << 16`, ORed with `request[+0x14] - 1` | `52203` |
| 9 | `0x00` | `0xCC000001` | `52217` |

The VX855 XP routine at `527E0` takes this sequence for both `1122` and `5122`. The 2013 miniport retains that same branch for both IDs. In the corresponding `7122` path at `525D0–526A7`, the write order changes and the arithmetic uses a different layout. The GDI and DirectDraw wrappers can use other paths. The full command-word bitfields, request field names and scanout update sequence remain open.

| Order | VX800 offset / call | VX855 offset / call | VX900 offset / call |
|---:|---|---|---|
| 1 | `+04 / 52133` | `+04 / 52965` | `+04 / 525DD` |
| 2 | `+14 / 5214D` | `+14 / 5297F` | `+34 / 525F3` |
| 3 | `+08 / 52164` | `+08 / 52996` | `+38 / 5260B` |
| 4 | `+1C / 5217E` | `+1C / 529B0` | `+30 / 52621` |
| 5 | `+48 / 52195` | `+48 / 529C7` | `+2C / 52634` |
| 6 | `+18 / 521BA` | `+18 / 529EC` | `+08 / 52655` |
| 7 | `+10 / 521DE` | `+10 / 52A11` | `+0C / 52676` |
| 8 | `+0C / 52203` | `+0C / 52A36` | `+10 / 52697` |
| 9 | `+00 / 52217` | `+00 / 52A4A` | `+00 / 526A7` |

## 2. PCI resources, cache request and display

- The VX800 mapping path `5C315 → 29D00` sends IOCTL `230474` to the miniport. At `29F9C` the driver subtracts `230458`. The byte at `30948+1C` is 4, and entry 4 at `30930+4*4` resolves to `2B8F0`. The handler requires at least 16 input bytes and 12 output bytes. On success output fields `+0/+4/+8` receive the requested address, the resulting length and the mapped pointer. On the direct path at `2BF9C`, VideoPort receives the length by address and can update it. [Microsoft VideoPortMapMemory documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/video/nf-video-videoportmapmemory)
- VX800's initializer at `92F53–92F72` tests mask `0x200` in capability DWORD `+0x68`. It stores 0 when set and 8 otherwise in extension `+0x384`. The mapping handler reads that field at `2BC8C` into a local, then the direct VideoPort call passes the local by address as `InIoSpace`. The VX855 reference uses the same policy, and VX900 stores the flag at a different structure offset. Actual Win98 page attributes and UMA size depend on the target system.
- The VX800 miniport's VGA window comes from the mapped register resource at `96639`. The CBIOS module exports `CBiosInitialization` at `14FD0`, `CBiosSetClock` at `13C50`, `CBiosSetMode` at `11010` and `CBiosGetEdid` at `15320`. The mode call is traced through the miniport import, so its parameters and output routing can be studied independently of 2D. The supplied VX800 set contains the XP CBIOS module and INF, with board option-ROM bytes outside the inputs.

## 3. GDI, DirectDraw and Direct3D dispatch

The VX800 CopyBits and BitBlt wrappers reference their own names and invoke slots `+0x40` and `+0x3C` in the selected display callback table. The module stores its table pointer at `A83A8` in `11022`. These offsets correspond to the VX855/VX900 GDI callback slots. The VX800 DirectDraw registration code copies the `0x40`-byte table at `AB6E0` at display `2CB70`. The Flip and Blt pointers are `AB6EC` and `AB6F4`.

**DirectDraw flip.** VX800's display flip handler calls CIL group 7 slot 21 through `[B7054]`. At CIL `1A8110`, one branch invokes backend method `+34`. Backend 4 resolves to `B54E0`, where the ordinary chip-ID branch admits `1122/5122/7122` and calls packet builder `B4800`. Separate branches in this method take precedence when their configuration fields are set. VX855 and VX900 have the corresponding packet builder behind the same CIL group and slot.

| Flip evidence | VX800 | VX855 | VX900 |
|---|---|---|---|
| Display calls CIL flip slot | `31F10/3214D → [B7054]` | `32280/324BD → [B8094]` | `32BA7/32EBF → [C2E54]` |
| CIL wrapper | `1A80D0` | `1AAC80` | `1AFD60` |
| Chip-group builder call | `B5743 → B4800` | `B7A93 → B6B50` | `BC2EA → BB390` |
| Address packing | `B4A00–B4A33` | `B6D50–B6D83` | `BB59A–BB5CC` |
| FE02 header for the traced packet | `B4B65` | `B6EB5` | `BB827` |

In the traced branch, the builders pack the computed address into `0x30000000 | (address & 0x00FFFFF8) | flags` and `0x31000000 | (address >> 24)`. They place those words before `0x32000008` in the packet. The corresponding FE02 header and following DWORD (`FE020420,0000041C`) select mapped `+0x41C` for the `0x00110000` control word and `+0x420` for the stream. The builders have additional branches and indexed VGA accesses. The packet establishes an XP flip route and its address packing. The hardware latch behavior and equivalence to the Win98 CRTC sequence remain open.

The Direct3D callback table at `AB7D0` points to `2A6D0`, which calls the parser `37BF0`. The parser decrements the opcode and bounds the index to `0x5F` before jumping through `3B714`. A decoded 96-entry table and the cross-module function pointers are included.

| Operation | VX800 | VX855 | VX900 |
|---|---|---|---|
| Indexed triangle list, DP2 `03` | Handler `38791`, calls display slot `B71B0` at `388A5`, CIL `8A9E0` | Handler `38B01`, slot `B81F0`, CIL `8CD30` | Handler `39851`, slot `C2FB0`, CIL `908E0` |
| Clear, DP2 `2A` | Handler `391F1` → `35250`, slot `B7050` at `355AB` → CIL `1A13B0` | Handler `39561` → `355C0`, slot `B8090` → CIL `1A3F30` | Handler `3A241` → `36270`, slot `C2E50` → CIL `1A88E0` |
| Backend-4 clear target | Table `1F98D8`, method `+20` → `AA660` | Table `1FC8D8`, method `+20` → `AC9C0` | Table `2028D8`, method `+20` → `B05A0` |

At VX800 CIL `1B5E5D` the backend table is computed with stride `0xB0`. Its initializer `B29D0` includes `1122/5122/7122` in a gated group and conditionally overwrites methods `+0C/+18/+54/+58/+A4`. The clear method stays in the on-disk slot. The device constructor can use a configured device override, so the default-path conclusions refer to the detected-ID branch. Table slot numbers are comparison keys. A matching slot alone establishes neither identical behavior nor equivalent arguments.

## 4. Command queue, FE transport and stencil

**FE transport.** In the VX800 interpreter, FE02 takes the second DWORD's low 16 bits as the control destination and the first DWORD's low 16 bits as the stream destination. The observed `FE020440,0000043C` form uses `+43C` for control and `+440` for stream data. FE03 takes its first DWORD's low 16 bits as the control destination, the next register as the stream destination, and its second DWORD as the continuation count. `FE03043C` therefore uses the same two offsets. The VX800 reader loads the mapped pointer from CIL device record `+F4`, as VX855 does. VX900 uses record `+FC`. These are driver structure fields.

**Queue reservation.** The VX800 queue initializer `BB010` checks `7122` at `BB02A`. On that ID it changes queue method `+24` to `B9E10`; `1122` uses `B9B00` on the ordinary table path. The 2010 VX855 bundle also contains a `7122` override at `BD39A–BD3AC`. The 2013 VX900 build keeps the override and extends its gate to `A122`. The older alternate helper already contains the `+41C/+420` wrap writes with `0x66xxxxxx/0x67xxxxxx` at `BA007–BA056`. All three XP bundles use the **read side** of mapped `+43C` as a progress value in queue reservation. The CPU packet interpreter also **writes** that offset as a control port. This yields an XP comparison for the previous Win98 zero-readback probe. The setup sequence needed for progress on each Win98 board remains unresolved.

In the VX800 binary, `BAEDE–BAF41` assigns `0x7F` to both queue fields `+4060/+4064` for `1122` and `0xFF` for `5122`. The 2013 counterpart also gives `5122` `0xFF` and assigns `0x7F` to `7122`. The default VX800 reservation code at `B9B80–B9B9D` adds both fields, each plus one, to its space calculation. Their physical FIFO interpretation remains unresolved.

**Context initialization.** All three CIL builds write the same seven DWORDs in the chip group that includes `1122/5122/7122`. This is an initialization path distinct from the per-reservation wrap sequence.

| MMIO offset | Value | VX800 write | VX855 write | VX900 write |
|---:|---:|---:|---:|---:|
| `+41C` | `00110000` | `111A9B` | `113E4B` | `115C95` |
| `+420` | `06000000` | `111AB4` | `113E64` | `115CAE` |
| `+420` | `07100000` | `111ACD` | `113E7D` | `115CC7` |
| `+41C` | `00100000` | `111AEB` | `113E9B` | `115CE5` |
| `+420` | `02000000` | `111B00` | `113EB0` | `115CFA` |
| `+41C` | `00100000` | `111B15` | `113EC5` | `115D0F` |
| `+420` | `69000000` | `111B2A` | `113EDA` | `115D28` |

The 2013 build then has extra writes at `115D4A–115D91` gated by capability mask `0x00400000` at device field `+0x68`. The shared initialization sequence gives concrete values to compare against the Win98 fetcher trace. See §6 for the later ring start writes and the observed Win98 readback.

**Stencil.** The separate-stencil path requires clear mask `0x0004`, resource mask `0x4000`, and a companion pointer. For VX800 these are checked at `AAB5A–AAB71` using resource fields `+110/+2CC`. VX855 uses `+118/+2D4`, and VX900 uses `+114/+2D8`. VX800 auxiliary fields are CPU mapping `+EC`, padded width `+138`, and padded height `+13C`. The partial branch calls wait selector 9 at `AAD9E`, then stores at `AADFC`. The exact padded-full branch requires `(left, top, right, bottom) = (0, 0, padded_width, padded_height)` and calls `1AB8D0` at `AAE82` with a byte mask of `FF`.

All three CPU paths implement the same byte offset from the separate stencil mapping.

```text
(y >> 3) * (padded_width << 3)
+ (y & 7) * 8
+ (x >> 3) * 64
+ (x & 7)
```

VX800 selector 9 dispatches to `110603`, passes status mask `0x00101FF3`, and invokes the miniport callback installed at `2D7B4`. The selector sets the flag for the timer path before the call. That path requests a 1 ms recurring interval from `KeSetTimerEx` and checks for elapsed time greater than 120 seconds at `31031` using `KeQuerySystemTime`. Actual wake-up granularity depends on the XP timer. The alternate polling path has no local overall timeout. The XP partial clear continues after the call without a success test and has no explicit post-store publication in that branch. The Win98 port should retain its bounded waits and publish CPU writes before GPU consumption. [Microsoft KeSetTimerEx documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kesettimerex) · [Microsoft KeQuerySystemTime documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kequerysystemtime-r1)

**Allocation geometry.** VX800 layout initialization at `111BD0` installs geometry `216360` into 24-byte layout entries starting at device-record `+5D0`. PE table reads for 8/16/24/32 bpp give 32 bytes horizontally, 8 rows vertically and 256 bytes per block, matching the VX855 and VX900 reference records. The secondary record's `+0x0C` field starts at 256. The family-5 path changes it to 1024. The normal VX800 and VX855 family-13 paths retain 256 at this site. These values describe the allocator geometry and the auxiliary stencil's byte addressing still uses its own padded width.

## 5. OpenGL and XP feature defaults

The VX800 ICD escape passes 36 bytes of input and a 64-byte output buffer with escape `0x1100`. The display checks a 16-byte minimum, then calls the installed callback. At `133E2` that callback requires 36 bytes, and at `13404–13417` dispatches selectors `0x64–0x76`. The corresponding VX855 and VX900 paths use the same sizes and selector bounds. All 18 named VX800 ICD exports are at preferred addresses 0x10 below their corresponding VX855 and VX900 exports. Rendering command generation and presentation behavior still need per-path verification.

| INF policy | VX800 | VX855 | VX900 |
|---|---:|---:|---:|
| Hardware clipping | 0 | 0 | 1 |
| Hardware vertex buffer | 0 | 0 | 1 |
| Hardware index buffer | Absent | Absent | 1 |
| T&L and vertex shaders | 1 | 1 | 1 |
| Coarse-Z and separate stencil | 1 | 1 | 1 |
| Fast-Z clear | 0 | 0 | 0 |
| Tiled display, RT, Z/W, texture | 0 | 0 | 0 |
| AGP DMA and 2D AGP commands | 1 | 1 | 1 |

These are XP configuration defaults. Actual runtime activation depends on the selected device path and resources.

## 6. Windows CE corroboration and Win98 implications

The CE observations below come from the attached consolidated research and its build-identified retail x86 DLL and linker-map analyses. CE 6.0 references use display driver `7.06.32.411`. CE 7 references use `7.07.32.42` and concern the display driver's internal emitters. Its release notes exclude Direct3D Mobile. XP addresses were cross-checked against the supplied VX800/VX900 CIL binaries and the archived VX855 disassembly. The Win98 results cited here are historical board logs. vx_8_VS_9_wince_re.md (§§86, 261–270, 365–370, 375, 381 and Appendix C).

### Backend, 2D and scanout

| Evidence | VX800 | VX855 | VX900 |
|---|---|---|---|
| CE 6.0 3D dispatch | H6 branch `10063735` | Same H6 branch | H5S2 branch `100636F7` |
| CE 7 3D dispatch | H6 branch `10072ACD` | Same H6 branch | H5S2 branch `10072A8F` |
| CE 6.0 direct screen-copy body | `1001A8B1` | Same body | `10027ED4` |
| CE 7 examined source-key branch, bits ORed at GE `+48` | `0x4000` | `0x4000` | `0x6000` |
| Graphics framebuffer BAR config bytes in the cited manuals | `10h–17h` | `10h–13h` | `18h–1Fh` |
| Graphics-control MMIO BAR config bytes in the cited manuals | `18h–1Fh` | `14h–17h` | `14h–17h` |

- **3D reuse.** Both CE versions assign the same H6 initializer, texture helper and blitter to VX800/VX855 within each build. VX900 changes the Type-04 vertex declaration and uses Type-03 selector `0x31030000` for vertex-program upload. In the 2009 XP CIL, the traced path enables the counted upload for family 5 (`C040F`, gated call at `82166)). VX800/VX855 family 13 bypasses that gate. The 2013 XP CIL emits the same selector at `CB9CA`. This supports reusing the tested VX855 fixed-function path for VX800 and giving VX900 its own vertex encoder. CE blit parity covers the examined operations. (§§86, 292–295, 366, 372, 378.)
- **2D reuse.** CE 6.0's shared VX800/VX855 screen-copy routine reinforces the XP miniport pairing. The CE 6.0 VX900 routine also clears the two rotation bits in GE `+04` when updating depth. The older routine preserves those bits. CE 7 independently preserves the `0x4000/0x6000` source-key programming distinction in the examined path, even though callback sharing changes between builds. Compare keyed pixels separately from ordinary fills and copies. The literal OR operations leave the incoming register state relevant, and the VX800 manual/code interpretation remains open. (§§112–114, 367.)
- **Resource discovery.** Use the board's assigned PCI ranges and decoded BAR widths. The documented configuration offsets explain why a fixed VX855 framebuffer/MMIO lookup cannot be reused across the three profiles. The BAR aperture size and the firmware's actual UMA allocation are distinct quantities. (§§4, 365.)
- **Scanout.** The CE 6.0 non-shrink flip at `10029F1A` uses `addr >> 1` for IGA1 `+214` and `addr & ~7` for IGA2 `+218`, with per-path gating. The incoming address unit still needs confirmation. Log primary VBlank `+200[1]`, secondary VBlank `+204[2]`, primary address-update pending `+204[1]` and secondary flip flag `CR61[5]` as separate events. CE provides a scanout reference alongside the XP FE02 flip packet and the Win98 CRTC path. Their latch timing still needs a board trace. (§§189–193.)
- **Overlay/HQV.** CE 6.0 shares several flip and fire routines between VX800 and VX855, while all three use distinct initialization routines. CE 7 installs distinct show, flip, fire and wait entry points for each chip. Both builds use per-family software mode values `4/5/6`. Keep overlay/HQV initialization and validation separate from basic GE and primary scanout bring-up. (§§349, 368.)

### Command transport, fetch startup and readback

CE 6.0's software interpreter independently gives FE02 and FE03 the same selector/stream behavior described in §4. The CE INV ring-space routine separately reads `+43C` at `1004EE48` and compares against translated device addresses. The archived VIA Linux definitions identify the read-side roles of `+43C` (current command address) and `+444[31]` (paused). CE uses the paused-status recovery branch for VX800/VX855 and skips it for VX900, whose ring-space code also reads `+47C`. The latter register's hardware meaning remains open. (§§47, 194–207.)

| Startup stage | CE evidence | XP comparison |
|---|---|---|
| Command-regulator preamble | CE 6.0 `hwmInitialize_inv` writes the same Type-11 and Type-10 payload values as §4. It keeps the Type-10 selector active between `02000000` and `69000000`. CE 7 also shows the `69000000` payload at `1005F561`. | The three XP context initializers repeat the Type-10 selector before `69000000`. |
| Program ring and trigger | CE 6.0 programs Type-10 `60h–67h` and writes `0x680A0001` at `1004EAD8`. The source documents the read-threshold field and trigger bit. | The same trigger is written to mapped `+420` at VX800 `B95E9`, VX855 `BB959` and VX900 `BF83B`. These are the ring-start paths after address programming. |
| Publish and wrap | CE 6.0 calls `CacheSync(1)` twice before updating the pause address on its nonempty kickoff path. Its rewind aligns VX800/VX855 to 256 bytes and VX900 to 64 bytes. | XP has its own reserve and wrap paths. Check command visibility, device-space address, pause ID and wrap alignment together when comparing the Win98 trace. |

CE's enabled path includes command backing and GART setup before triggering. The useful comparison is the full sequence from mapping and command visibility through start, pause, trigger and completed work. Keep the observed CE alignment scoped to that rewind implementation. (§§195–196, 214–220, 237, 373, 380.)

**Historical Win98 hardware baseline.** The attached analysis reports two VXD3DMON v5.4 sessions identifying the HAL as `v10.441`. Both record `+43C=0x00100100`, `+45C=0x001000F0`, `+444=0xC2000000` and `+464=0x00100001` at startup, then report 107,771 and 80,585 command groups kicked with zero unfinished fetcher packets. The current-address readback was **16 bytes past the programmed pause** in both sessions. This qualifies the zero-readback observations from other configurations. For a new run, correlate stable readback, paused status and completed draws. Treat the observed 16-byte offset as specific to those sessions. (§§261–270.)

### Depth and stencil corroboration

- **8×8 locality and addressing.** The VX855 manual's separate-stencil formula uses 64-byte tile increments and a base in 256-byte units. That reinforces the tile organization of the XP CPU loop in §4. The XP software trace supplies the byte-within-tile formula. Packed D24S8 and the separate auxiliary have distinct layout tests. (CE research §19.3, citing VX855 Part II p. 93.)
- **Binding and state upload.** The broader XP state uploader copies shadow `+9C/+A0/+48` in the applicable family branch on VX800 (`BE500`), VX855 (`C0870`) and VX900 (`C4A30`). On VX855, the incremental path marks stencil mode `+48` with group 0 bit 18 and stencil base `+A0` with group 1 bit 8. Coarse-Z uses a distinct companion and shadow fields `+A4/+A8`. Confirm actual state emission when testing a forced rebind. VX855 combined findings (§§1, 3 and 7).
- **Coarse-Z tag reset.** The attached research identifies a gated XP depth-clear path in VX800 and VX900 that updates the auxiliary and submits Type-01 `29h[15]` as `0x29008000`, followed by shadow `+A4`. The archived VX855 disassembly has the same sequence. The literal writes are at `C7249`, `C95B9` and `CD4F9` respectively. This is a separate operation from the stencil byte loop. Check whether Coarse-Z is active before using it to explain a VXSTEN result. (CE research §§19.2, 374, 379.)
- **VXSTEN control.** Recheck the v10.386 mixed 2D-depth/CPU-stencil early return in the current Win98 build. If the local `publish()` omission persists, compare baseline, publish-only, bounded finish-only and verified rebind-only at the same clear value and geometry. Under the modeled VX855 normal flags, 641×480 and 648×480 produce the same padded auxiliary and pitch while changing full-clear eligibility. Confirm which branch actually ran. Keep pre-draw CPU readback as a separate test because it can affect write visibility. VX855 combined findings (§§4–7).

The ZIP includes the original CE research attachment at `References/vx_8_VS_9_wince_re.md`, SHA-256 `0a2101b21357aeed1311a091d2feef3973f91128dcf98958f54ed90d1598948e`. The section numbers above refer to that snapshot.

## Win98 bring-up sequence

1. Add `1106:1122` as a distinct profile. Capture PCI revision, subsystem, decoded BARs, UMA size, active output and firmware mode before enabling acceleration.
2. Start with linear framebuffer and 2D. Compare VX855 and VX800 fill/copy output on 16/32-bit surfaces, including overlap, page boundaries and a separate source-key check.
3. Test DirectDraw locks and offscreen surfaces, then flip between two identifiable images at low and high VRAM offsets. Capture CRTC `0D/0C/48/34` and the relevant scanout status around the transition.
4. Use the existing MMIO path for one pretransformed triangle, then add texture, Z16 and D24S8. Compare consecutive depth clears and partial stencil updates across tile boundaries using the controls in §6.
5. Exercise VXGL windowed and fullscreen swaps and Quake 2 with `gl_ztrick 0`. Test fetch separately using translated producer, pause and jump addresses, `+43C/+444/+45C` readback and bounded completion of known work.

The XP/CE driver comparison is static. The cited VX855 hardware logs describe their original sessions. A running VX800 and a current Win98 driver build are needed to validate the proposed profile. The three-way archive preserves the previous reports, includes the complete VX800 disassembly listings and source hashes, and adds CIL, DP2, ICD and INF comparison tables with byte-checked VX800 instruction evidence. The second pass rebuilt both XP evidence sets from a fresh archive extraction and traced the flip and context initialization paths across all three builds. The VX855 comparisons use the historical reference disassembly. The supplied original XP PE files cover VX800 and VX900.

To regenerate the VX800 evidence, run `python3 VX855_WXP_RE/VX800/rebuild_evidence.py /path/to/original/uploads` from the extracted package with GNU objdump on PATH. The original VX855 binary comparisons use the disassembly preserved in its reference package.
