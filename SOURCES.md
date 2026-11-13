> Note: this file is the working record of sources kept while the driver was built. The `references/` folder it mentions (VIA's manuals and notes on other drivers) is not part of this repository; VIA's Open Graphics Programming Manuals are available from x.org.

# v10.58 sources and evidence

The baseline is the user's VX855_source_v10_57.zip and the supplied updated
VX855 Win98 reverse-engineering notebook. The notebook's fill/copy/pattern,
clip and key recipes, plus its later overlap tests, define the native path.
Their prior physical evidence does not establish this new DDI integration.

The unchanged VIA Chrome9 HCM Open Graphics Programming Manual, Part I,
revision 1.0 (2009-07-29), is included as references/VIA_Chrome9_HCM_Core_2D.pdf.
SHA-256: 46c711f84e4aab4247d5caa90b3fb8b9c54fe46e808ddeb220ba95755d2a2c71.
Its published page 79 documents key-control bits 14/15 and the exclusion of
alpha from 32-bit color-key comparisons. The command model follows that rule;
the physical DirectDraw test varies X bytes to check actual integration.
The manual is redistributed unmodified with VIA's original authorship and
redistribution notice.

Win9x bootstrap/callback contracts were cross-checked against the original
VMDisp9x companion HAL, not copied as a CPU renderer:
https://github.com/JHRobotics/vmhal9x/tree/8bc480e4d7a93bbf208526aa3b9c64079092487b

- vmhal9x.c: DriverInit context and callback registration.
- ddraw.c: runtime allocation fallback, shared dwReserved3 context, Lock/Unlock.
- vmdahal32.h: shared context mirror (MIT; upstream attribution retained here).

The public DirectDraw ABI headers in src/ddhal32 are from ReactOS/Wine:
https://github.com/reactos/reactos/tree/master/sdk/include/psdk
and https://github.com/wine-mirror/wine/blob/master/include/dciddi.h.
ddrawi.h and dciddi.h retain their LGPL-2.1-or-later notices; COPYING.LIB is
included. dmemmgr.h, ddkernel.h and dvp.h carry public-domain notices. The new
HAL/backend code is MIT under the existing project license. vmdahal32.h's only
layout adjustment makes the final invalid field a fixed DWORD in both ABIs.

## Historical peer source notes

# Native engine source map

This package implements the first physical Chrome9 HCM submission/readback
stage. It is not a complete Windows 98 Direct3D or OpenGL driver.

## Original VX855 hardware reference

VIA Technologies, *Chrome9 HCM Graphics Processor VX855 and VX875 Open Graphics
Programming Manual*, Part II, preliminary revision 1.0, July 29, 2009.
The exact unmodified PDF is `references/VIA_Chrome9_HCM_3D.pdf`.
Original: https://www.x.org/docs/via/OGPM_Chrome9%20HCM_R100_Part2_3D_Video.pdf
The manual contains its redistribution terms and authorship notice.

| Implementation | Manual printed pages |
|---|---|
| BAR1 + 0x43C type selection; +0x440 parameter/vertex port | 56-61 |
| Automatic primitive fire and PLEND from vertex count | 61, 77 |
| Disable PCI command queues | 67-68 |
| CR FIFO default depths | 69 |
| Color write, disabled PS/blending/culling/depth defaults | 78-80 |
| Z address/pitch/format/LESS, zero bias and float near/far limits | 80-84 |
| RT0 base in 256-byte units, linear local storage, ARGB8888, pitch/32 | 94-98 |
| Color/scissor/guard-band bounds and solid color | 107-109 |
| Five-dword x/y/z/w/ARGB FVF and masks | 148-152, 156-163 |
| Full triangle cycle, Gouraud/flat/solid, vertex count 3 | 153-155 |
| Screen coordinate clamps | 165-166 |

## Engine-control and 2D correction in 1.3b

VIA's companion Part I manual, *Chrome9 HCM R100 Core and 2D*, is included
unmodified as `references/VIA_Chrome9_HCM_Core_2D.pdf`.
Original: https://www.x.org/docs/via/OGPM_Chrome9%20HCM_R100_Part1_Core_2D.pdf

| Correction or diagnostic | Primary reference |
|---|---|
| SR1A[3] extended memory access; preserve reset and LUT bits | Part I p28; `via_ums.c:viaMMIOEnable` explicitly selects VX855 |
| SR1E[0] ROC ECK and SR2D[1:0] engine PLL power | Part I pp29,31; `via_display.c` common initialization |
| SR3F CR/3D/2D gates set to always-on for explicit test | Part I p33; `via_display.c` common initialization; only engine fields are modified |
| Bounded SR3C[4] ECK PLL lock detection | Part I p32 |
| Refuse asserted reset instead of clearing it | Part I SR1A p28 and SR4E p36 |
| VX855 32bpp fixed-color BitBLT, target base /8, pitch /8 | Part I pp74-80; `via_exa_h6.c:viaExaSolid_H6` and `via_regs.h` M1 offsets |
| Clear CR pause/stop and use known non-triggering AGP threshold | Part II pp63,65; `via_ums.c:viaInitPCIe` supplies threshold 0x0A0000 |

The test uses always-on engine gates while it owns the explicit operation,
then restores their saved values on an idle completion. It keeps existing
PLL frequencies and display-clock fields. The subsequent 1.3b physical capture shows those controls were
already enabled, so their omission did not explain this board's failure.
The independent 2D fill passed while the solid triangle still failed. That release copied no FE configuration; 1.3c adds the independently
verified VX855-specific block described below.

## Linux source used as reference

OpenChrome, mirrored/maintained by X11Libre:
https://github.com/X11Libre/xf86-video-openchrome
Pinned commit: `8c35bafd6d65c6c80b9b93b50724dd8655feaaf8`.
Six exact files were fetched and byte-compared against this commit; their
per-file URLs and SHA-256 values are in `references/SOURCE_HASHES.json`.
Each source file retains its original permissive copyright/license header.

- `via_regs.h`: engine status at 0x400 and Chrome9 busy masks.
- `via_exa.c`: explicitly applies that status convention to VX855.
- `via_ums.c`: explicit VX855 MMIO enable, PCI/MMIO path and disabled queue.
- `via_display.c`: engine-clock enable and power-management fields.
- `via_exa_h6.c` and `via_eng_regs.h`: architecture/2D cross-checks.

VIA's original Linux Chrome9 DRM submission was also examined:
https://lkml.iu.edu/0805.3/2636.html
It confirms type/parameter ports 0x43C/0x440 and the 0x1FE1 3D busy mask.
Its H5/H6 initialization predates VX855 and is insufficient alone for
exact VX855 defaults. No AGP/GART or old UniChrome layout is used.
The HCM manual defines the actual target and vertex commands used here.

These references supply register facts and submission conventions. They do
not supply a working Windows 98 3D HAL. Linux code has not been renamed and
represented as a completed Windows driver.

## Windows baseline

VMDisp9x commit `767761d0a49e4c9d4c66e906f38e6118b8b44f89` and fixlink
`c1fd1d2264995b9fa76985c76ef2bd1396af377a` supply the existing Win98 display
infrastructure. This stage retains the previously tested physical VBE path.
The VMware 3D protocol is not a VX855 command set and is not used here.

## Remaining work after native hardware evidence

The real GPU must first accept and render these commands. Initialization,
packing or board-specific requirements may need correction from that result.
Then expose reusable surface allocation, synchronization, context state and
presentation through an actual Win98 DirectDraw/Direct3D HAL or OpenGL ICD.
Texture sampling, blending, state translation, loss/recreation, mode/DOS
handoff, capability reporting and real applications all need implementation
and hardware testing. This release advertises no accelerated 3D API caps.

The earlier EDID/1920x1200/extra pixel-format/dual-output requests are separate
pending work. The current test intentionally retains the verified modes and
mapping while bringing up the native rendering engine.

## FE/global correction in 1.3c

The 1.3b physical report proves 2D and already enabled clocks, while the
first 3D triangle produces no pixels. The HCM required draw sequence includes
FE, omitted by the previous implementation. VIA's Linux initialization and
the actual PCI 5122 branch in the user's DLL establish the added constants.
See references/VX855_FE.md for addresses, binary hash, values, differences
from old Linux defaults and the external executable oracle. Extracted VIA
source with original permission notice is via_chrome9_drm_2008.c. The vendor
DLL is not distributed or needed to compile. The omission is established;
its causal contribution must still be tested on the physical GPU.

## Multi-probe diagnostic matrix in 1.3d

The same primary HCM manual defines FVF lengths/attribute numbers/masks
(pp148-152,156-163), command/index mode and vertex base/stride (pp153-154),
flat/Gouraud/point/line/winding controls and counts (p155), point size in
s[8].15 (p162), default Z/W and index bounds (p163), and the parameter
transmission window and auto-fire (pp56-61). The manual leaves the vertex
stride unit unclear; probes 10/11 test byte/word candidates with fixed
bounded local source storage. Neither interpretation is claimed proven.
Burst submission uses at most fifteen vertex words, with a final same-device
status read; it changes pacing only. Existing type/FE state remains checked
against the documented HCM family and the executed PCI 5122 vendor branch.

The framebuffer hold gate and all-buffer span inspection are original
integration code in vxd_vesa.c. They test possible desktop-copy interference
without attributing it as the cause. None of the new paths comes from the
incompatible VX900 shader/FVF scheme. All hardware outcomes remain pending.

## Acceptance and submission diagnostics in 1.3e

The original HCM manual defines T10 A02/A03 register readback (p62), T11
A06/A07 mode-0 fence recording (p70), CR T11 AA/AB inspection tags (p74),
006C wait-2D/wait-3D (p75), T01AA inspection (p115), T04AA inspection (p166),
and both CR programming ports / the 3D parameter window (pp57-61).
T11 bit17 is RESERVED in HCM; the different VX900 arbitration setting is
not transplanted. Synchronization is an ordering probe, not an unlock bit.

The original user's s3ginv_vx855.dll SHA-256 is
fec5211548bc2e1100efa8de55b89407717c14997d78e0d3eed333d62a1ca532.
VA 00112BB4..00112C27 selects T10 through 041C, writes A02=0 and A03=011C
through 0420, then reads 0420/0424 and reconstructs the value. VA
00112182..001121DB explicitly branches PCI 5122 to 0494 current-fence read;
3230 uses 0474. verification/vendor_readback.py executes both real CPU
paths against modeled MMIO, with distinct return data. It establishes port
conventions and chip dispatch, not live readback semantics or rendering.
The HCM manual supplies the new inspection-register addresses and their
field masks. Full raw state windows are preserved to investigate packing.

The modern via-chrome9 VX900 project was consulted as a research lead for
006C boundaries only. Its hardware is different. No GPL implementation,
VX900 shader program, vertex format or reserved-bit setting is copied here.
Primary HCM/manual and original vendor observations above define this code.

## Pipeline and readback corrections in 1.3f

The user's unmodified 1.3e capture physically returns aligned four-register
blocks with each register index in the high byte. For example a request for
T01 A52 returns 50002010,51000000,52990008,53000C0F. A52 is lane two;
A53 is lane three. The original checker used only lane zero. All ten selected
values match in all four queried probes. Raw files, corrected analysis and
check_readback_layout.py retain reproducible evidence. Inspection AA is lane
two of A8..AB; the old capture stored lane zero, so its old tag result is
inconclusive. The new code validates the selected register-index tag too.

HCM Part II pp128-136 defines T02/FE texture count/cache control, texture
coordinate mappings and user clip controls at A05, including enable bits
15 and 7. The old pipeline omitted this entire shared state block. The new
code sets only documented A00..05 and A08..0E; A06/A07/A0F are reserved and
not written. No texture addresses or shader instructions are supplied.
Correcting the omission does not prove it caused this board's blank output.

HCM pp164-166 defines viewport scale/offset float packing and the area
threshold. The identity path is fully explicit. Clip-space paths use x=32x+32,
y=-32y+32, z=z, with W=1 and bounded vertices. T01A01 bits20/11 control C2S
and clipping (p79). The direct depth test sets bit6 color-write off and bit2
Z-write on. T01A11 bit9 selects the documented PERZ-to-BE short data path
(p80), A12 sets linear 32-bit fixed Z (p81), A13 uses ALWAYS (p82).
Internal occlusion counter A1C/A1D (p83) is read using the exact same query
011C used by the vendor CPU oracle. No writeback memory destination is enabled;
A21/A22 remain zero. The original counter is restored on supported readback.

The state clearing, transforms, checks and integration are original code.
Hardware readback identifies actual stored values, not completed rasterization.
No VX900 VS/FVF/shader code, reset pulse, DMA or GART setup is added.


## 1.3g queue comparison

The exact pinned `via_ums.c` bodies `viaEnablePCIeVQ` and the VX855 branch of
`viaDisableVQ` are compiled/executed in `verification/openchrome_queue.py`.
They establish the original CPU sequence and bytes/8 length formula. VIA
HCM Part II pp66-68 describes the queue bounds/control fields but says
128-bit length units. This discrepancy is retained explicitly, with doubled
backing and strict declared-end guards. The original source itself does not
prove physical behavior. `via_exa.c:viaFlushPCI` establishes the separate
PCI/MMIO submission path; an allocated virtual queue is optional buffering.
The two current encoded streams are compared in full and differ only at A74
bit0. No VX900-specific queue/vertex/shader definitions are transplanted.

The peer DOS log's SR16/17/18/22 definitions are in HCM Part I pp27 and 31:
display FIFO normal threshold, depth, high threshold/arbitration and request
expiry. These are not copied as a 3D fix. Its reported RAM amount lacks the
raw config value/decoder for an independent check in this package.


## 1.3i: write-only queue controls and independent completion

HCM Part II pp66-68 marks T10 A70..74 and A78..7C as WO. The T10 A03
readback mechanism on p62 does not promise readable values for those controls.
Both physical 1.3g captures returned identical queue banks despite our writes.
The earlier mandatory echo/restoration check was unsupported. OpenChrome's
VX855 `viaDisableVQ` posts 74301000 without that readback gate.

1.3i initializes an owned disabled controller before touching scratch and
leaves that known disabled baseline at completion. It never replays unknown
queue snapshots. Completion uses documented idle bits, same-device posted
write drains, 006C engine waits, and memory-free T11 A06/A07 mode-0 fences
read at the vendor-confirmed PCI-5122 0494 register. The fence changes and
its original ID is restored. These checks establish CR progression, not
rasterization or an otherwise unobservable queue-control effect.

Raw banks stay in the report. Only a bank that changes from its original
snapshot and matches all five programmed controls is allowed to contradict
shutdown. No conclusion about live rendering-unit state follows from it.
The direct diagnostic bypasses Direct3D. No Direct3D HAL or OpenGL ICD is
implemented or advertised here. The peer 1.3h scanner source was absent;
its report extension is identified separately and labeled as inferred.

## 1.3n: traced source formats and fetched-buffer test

The user-uploaded OpenChrome archive is hashed in
`references/OPENCHROME_UPLOAD.json`. It confirms our pinned source and adds
unmodified quad/parser, DRI and option references. Its default VX855 path
consumes F210F110 on the CPU; it is not a vendor FE02/FE03 DMA builder.
`openchrome_submission.py` executes those original functions.

Original VIA Linux DMA and register files from the 2008 publication retain
their notices as `via_chrome9_dma_2008.c` and `via_chrome9_3d_reg_2008.h`.
`linux_dma_oracle.py` compiles their unchanged decoder/STOP-launch functions
as 32-bit code and checks both production lists. `vendor_dma.py` independently
executes the supplied XP launcher and a Header3 framing fragment.
The Linux PCI table predates 5122; neither an older chip's fields nor VX900
GART/shader paths are claimed as validated VX855 behavior.

`references/LINUX_SUBMISSION.md` distinguishes software packet framing,
port stores, optional PCI VQ, local fetch and AGP/GART address spaces.
Production uses fixed guarded local source addresses, complete recorded
packets, and a fetched fence-only control before submitting a triangle.
All original source/vendor CPU and GPU-model checks remain separate from
physical evidence; no physical 1.3n draw is claimed.

## v6 SF mapping

See references/SF_MAPPING.md. VX855 manual Part I SR6A/B/C and SR6F define
the RTSF base/protection/flush. The included original VIA Linux 2008
InitPCIEGART and AllocAndBindPCIEMemory establish PFN packing. The supplied
XP PCI-5122 initializer is executed by the optional hash-pinned oracle.
SR7B is reserved in the VX855 manual and is preserved. No VX900-only
undocumented enable bits are copied into this build.
