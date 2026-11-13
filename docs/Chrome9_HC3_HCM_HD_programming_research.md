# Chrome9 HC3 / HCM / HD — Consolidated Programming Research

**Platforms** — VX800/VX820 (Chrome9 HC3), VX855/VX875 (Chrome9 HCM), VX900 (Chrome9 HD)  
**Evidence** — Six VIA programming manuals, OpenChrome source, VIA Windows CE 6.0, CE 7 and VX800/VX900 XP driver analysis, plus Win98 source and archived VX855 hardware logs. [VIA, CE and OpenChrome sources](#c9-section-370) · [VX800 XP sources](#c9-section-381) · [VX900 XP sources](#c9-section-375).

VX800 and VX855 share the H6 3D backend in both inspected CE builds. VX900 uses H5S2 with different vertex descriptors and changes command dispatch and ring-wrap semantics. Common GE commands cover ordinary 2D operations, while memory, scanout, video and HQV configuration retain family-specific rules. The CE 6.0 and CE 7 callback addresses describe their respective builds.

| Topic | Reference |
|---|---|
| [Scope and reuse boundaries](#core) | Supported scope and family boundaries |
| [PCI, graphics memory and display](#pci-display) | PCI IDs, BARs, framebuffer, display and flips |
| [2D engine and host transfers](#engine-2d) | GE commands, callbacks and indexed or native host transfers |
| [3D backends, vertex input and primitives](#geometry-3d) | H6/H5S2, FVF and VX900 vertex programs |
| [Textures, pixel programs, depth and stencil](#textures-rendering) | Texture and pixel formats, state, blending, depth and stencil |
| [Command transport and regulator](#command-interface) | FE packets, Type-10/11, fences and VQ |
| [CE command manager, locking and capacity](#command-manager) | CE locks, requests, flushes and physical capacity |
| [AGP ring, GART and Win98 measurements](#agp-ring) | Readback, GART, Win98 source and archived hardware runs |
| [Video overlay, HQV and decoder](#video-hqv) | Video overlay, HQV banks and decoder programming |
| [OpenChrome implementation and software pitfalls](#openchrome-review) | Shared emitters and implementation issues |
| [Windows 98 porting and validation](#win98-porting) | Proposed port structure, diagnostics and regression tests |
| [Compatibility matrix and evidence ledger](#compatibility-matrix) | 172 baseline rows, verification sources and open questions |

**Reference appendices** — [A. VIA and OpenChrome register differences](#differences) · [B. CE 6.0 reverse-engineering extracts](#ce-findings) · [C. CE 6.0 package and driver inventory](#ce-overview) · [D. VIA 3D register and bitfield tables](#via-matrix) · [E. OpenChrome emitted-command tables](#openchrome).

The numbered evidence sections retain their IDs so citations from the earlier consolidated report and its source matrices can still be followed. Historical measurements and binary addresses remain tied to the build that produced them. The [evidence ledger](#c9-section-360) tracks open hardware and ISA questions.

<a id="core"></a>

## Scope and reuse boundaries

The comparison covers GPU programming for VX800/VX820, VX855/VX875 and VX900. Use the hardware IDs, per-family exceptions and source scopes before adapting a Win98 command path.

### Platform and evidence scope

<a id="c9-section-1"></a>

#### 1. Purpose and scope

The goal is to determine how far the existing **VX855 / Chrome9 HCM Windows 98 driver** can be reused for the **VX800 / Chrome9 HC3** and **VX900 / Chrome9 HD** graphics cores. The six VIA manuals supplied for this comparison are the primary evidence. The analysis concerns **GPU register and command programming**, not chipset-wide USB, IDE, ACPI or CPU behavior.

**Key result:** A single source tree with hardware-family backends is viable as a **design hypothesis**. VX800 is the more natural first port. VX900 retains many common 2D/3D concepts but changes the vertex-input and command-regulator interfaces enough that blindly reusing VX855 command words is unsafe.

**Evidence terminology**

- **Documented:** Explicitly present in one or more supplied manuals.
- **Inferred:** A reasonable engineering implication, but no hardware validation is available.
- **Unresolved:** A documentation conflict, missing instruction description, or question requiring a hardware/driver trace.
- **Proposed:** Suggested implementation or experimental plan, not a confirmed hardware capability.

The documents often describe hardware programming, not working Windows 98 driver support. A feature being documented does **not** establish that it will work with DirectX 7/8, a particular chip stepping, or a proposed new driver.

<a id="c9-section-2"></a>

#### 2. Source index and page references

PDF page numbers below are **1-based PDF viewer page numbers** (not the manuals' internally printed page numbers). Use the exact filename to locate each source.

| Citation | File | Covered subsystem | PDF length |
|---|---|---|---:|
| V8-I | `vx800_1.pdf` | VX800/VX820: PCI, VGA, 2D, DMA, LVDS | 100 |
| V8-II | `vx800_2.pdf` | VX800/VX820: video, HQV, 3D | 157 |
| V855-I | `vx855_1.pdf` | VX855/VX875: PCI, VGA, 2D, DMA, display | 110 |
| V855-II | `vx855_2.pdf` | VX855/VX875: video, HQV, CR, 3D | 172 |
| V9-I | `vx900_1.pdf` | VX900: PCI, VGA, 2D, DMA, display outputs | 151 |
| V9-II | `vx900_2.pdf` | VX900: video, HQV, CR, 3D, profiling | 182 |

**Important scope limit:** These are preliminary VIA manuals, with occasional typographical, reserved-field, and chapter inconsistencies. The tables below should guide **what to test**, not be treated as a verified specification for every physical device.

<a id="c9-section-3"></a>

#### 3. Executive compatibility matrix

| Component | VX800 / HC3 compared with VX855 | VX900 / HD compared with VX855 | Initial porting treatment |
|---|---|---|---|
| PCI ID and BARs | Different device ID, 64-bit BAR layout | Different device ID, framebuffer on BAR2 | **Per-chip discovery required** |
| Legacy VGA and CRT | Similar conceptual core | Similar, with additional output capabilities | Shared API, validated register masks |
| Basic GE 2D blit, fill, line | Strongly overlapping registers | Strongly overlapping registers | Shared algorithm, per-chip feature checks |
| GE color-format conversion | Not documented at `0x30` | CFC remains documented | Feature gate; do not infer 8-bit support |
| GE AlphaBLT | Not documented | `0x00` command selector adds AlphaBLT | VX900-specific, experimental |
| Old 2D/3D wait registers | `0x60`, `0x6C` documented | Region `0x60–0x9F` reserved | **Separate synchronization backend** |
| Fundamental 3D parameter mechanism | Same general `HParaType` model | Same general `HParaType` model | Share logical builder, not raw sequences |
| Legacy FVF input words | Similar attribute-number encoding | Substantially redesigned | **Separate vertex encoder** |
| Multiple vertex input streams | Not like VX900's documented map | Explicit 16-entry VB input scheme | VX900-specific path |
| Pixel shader programming | Similar instruction interface | Older instruction detail marked reserved | **Do not assume binary compatibility** |
| Shader-based hardware T&L | No complete VS program interface shown | VS described in overview but detail incomplete | **Unresolved** |
| Depth/texture/render target state | Commonality documented | Many matching structures | Candidate for shared state definitions; compare fields |
| Video overlay/HQV | Common concepts with differences | Expanded/reorganized documentation | Separate backend and capability tests |
| Performance counters | Not equivalent VX900 PPC detail | Specific PPC writeback interface | VX900-specific instrumentation |

No percentage of binary or source compatibility can be inferred from the manuals alone.

<a id="pci-display"></a>

## PCI, graphics memory and display

PCI aperture layout and scanout programming have their own family rules. Keep OpenChrome CRTC writes separate from the Windows CE MMIO flip paths and the status bits they poll.

### PCI discovery and display setup

<a id="c9-section-4"></a>

#### 4. PCI discovery, BAR layout, and graphics memory

##### 4.1 Documented PCI IDs and BAR interpretation

| Chipset | VIA PCI ID | Framebuffer mapping | Graphics-control MMIO |
|---|---|---|---|
| VX800 / VX820 | `1106:1122` | BAR0; the manual shows address bytes `0x10–0x17` | BAR1; `0x18–0x1F` |
| VX855 / VX875 | `1106:5122` | BAR0; `0x10–0x13` | BAR1; `0x14–0x17` |
| VX900 series | `1106:7122` | BAR2; `0x18–0x1F` | BAR1; `0x14–0x17` |

**Evidence:** [V8-I, PDF pp. 9–14]; [V855-I, pp. 13–17]; [V9-I, pp. 11–17]. The VX900 also documents BAR0 for a different MMIO aperture rather than the system-local framebuffer.

The practical hazard is not just differing device IDs. Code that obtains its framebuffer address by looking at BAR0 will be wrong on VX900. Code that treats VX800's `0x18–0x1F` as a pair of unrelated 32-bit BARs would also be wrong. Always identify BAR widths and meanings for the selected device family, and let the OS assign/map the resulting resources.

**Suggested binding object** (conceptual C-like pseudocode; not a complete Win9x PCI implementation):

```c
typedef enum {
    CHROME_HC3_VX800,
    CHROME_HCM_VX855,
    CHROME_HD_VX900
} ChromeFamily;

typedef struct {
    ChromeFamily family;
    unsigned short pci_vendor, pci_device;
    unsigned char fb_bar, mmio_bar;
    unsigned char has_cfc, has_ge_alphablt;
    unsigned char has_legacy_2d3d_wait;
    unsigned char has_vx900_fence;
    unsigned char has_vx900_vb_mapping;
} ChromeChipInfo;
```

**Safety requirements:** verify BAR type and length; never manually assume a physical address from an example; validate mapped bounds; avoid treating reserved ranges as usable; preserve the existing working VX855 mappings.

##### 4.2 Aperture size is not necessarily installed video RAM

The VX800 manual describes a 256 MB system-local mapping and a 16 MB MMIO mapping, while the VX855 and VX900 manuals describe 512 MB graphics-related apertures in relevant BAR mappings. Those numbers describe **address-space apertures**, not guaranteed usable framebuffer capacity. They must not replace the board's firmware/driver discovery of real video-memory allocation. [V8-I, p. 9]; [V855-I, p. 13]; [V9-I, p. 11].

##### 4.3 VX900 manual inconsistency

V9-I's memory-space discussion uses an **MB0** partition description while its PCI table identifies **BAR1** for graphics-control MMIO and BAR2 for system-local framebuffer. Treat the PCI table and actual enumerated BARs as the starting point; validate with read-only status and known-safe 2D operations before enabling drawing. Do not hardcode a chipset-specific BDF solely from contradictory section headings. [V9-I, pp. 11–17].

<a id="c9-section-12"></a>

#### 12. Display and mode-setting differences

The manuals maintain familiar VGA sequencer/CRTC concepts, but do not warrant copying entire mode-setting tables. Pixel-clock, pitch/start-address and power-management field differences are enough to require a per-chip audit.

One explicit example is **CR36[5:4] DPMS state encoding**: the VX800/VX900 manuals distinguish non-on power states, while VX855 describes all nonzero encodings as standby behavior. [V8-I, p. 57]; [V855-I, p. 55]; [V9-I, p. 69]. The VX900 also documents DisplayPort programming not present in the equivalent VX855 display chapter. [V9-I, Display Port chapter, pp. 126–151].

**Suggested test matrix:** VGA text mode → 640×480 8/16/32 bpp (supported modes only) → 800×600 → 1024×768; switch twice and restore original mode; verify start address, pitch, visible alignment, cursor, DPMS, drawing after resume, and clean program exit. Do not infer availability of a mode or bpp from the existence of a register field alone.

<a id="c9-section-29"></a>

#### 29. Display and framebuffer programming: more differences than EXA suggests

##### 29.1 Same MMIO mapping, different framebuffer BAR

The source maps **graphics MMIO from PCI region 1 on all three**, but maps the framebuffer from **region 2 for VX900** and **region 0 for VX800/VX855** [OpenChrome `src/via_ums.c:L94–L115`, `L213–L240`]. This independently agrees with the core PCI manual comparison in §4. The source enables write-combining when mapping the framebuffer; its exact X.Org mapping API must not be transplanted into a Win9x VxD verbatim.

##### 29.2 2D/3D engine init touches register areas inconsistently documented

For all three newer chipsets, `viaInitialize2DEngine()` zeroes `0x04–0x40` and then `0x44–0x5C`. On **VX900 only**, it also writes zero to `0x60` with the comment `410 redefine 0x30 34 38` [OpenChrome `src/via_ums.c:L411–L434`]. VIA's VX900 2D register table describes `0x60–0x9F` as reserved [V9-I, PDF p. 103].

**This is a discrepancy, not an instruction to write reserved registers.** It may be a harmless ignored write, a real undocumented register, or an OpenChrome compatibility quirk. The safe Win98 VX900 port should not copy the write until a controlled trace shows why it is needed.

##### 29.3 Register save/restore changes across the three families

OpenChrome's mode-state capture/restore includes additional VGA/CRTC register sets by family:

| Register range | VX800 | VX855 | VX900 | Evidence |
|---|---|---|---|---|
| `SR14`, `SR68–SR6F` | Yes | Yes | Yes | `via_ums.c:L1042–L1055` |
| `CRF0–CRF5` | Yes | Yes | Yes | `via_ums.c:L1100–L1106` |
| `CRF6–CRFC` | No in this common capture group | Yes | Yes | `via_ums.c:L1108–L1113` |
| VX900 extra late `CRF7–CRFF` handling | No | No | Yes | `via_ums.c:L1115–L1119` |

**Modesetting implication:** preserve the board's firmware state and capture only the registers the chip family meaningfully implements. These ranges are not automatically safe to overwrite with another chipset's byte values. The larger restore logic in `via_display.c` further groups VX800 separately from VX855/VX900. [OpenChrome `src/via_display.c:L2960–L3041`].

##### 29.4 VX900 extends primary horizontal timing encoding

OpenChrome specifically programs `CR45[1]` for VX900's extra horizontal-display-end bit and `CR45[2]` for its extra horizontal-blank-start bit, above the legacy 8-bit CRTC registers [OpenChrome `src/via_display.c:L1338–L1371`]. It also handles a different `CR47` protection bit: VX800 clears bit 0, while VX855/VX900 clear bit 4 [OpenChrome `src/via_display.c:L605–L645`].

This can matter for nonstandard VGA modes, high resolutions, or an image shifted beyond the visible scanout window. The correct timing split should be verified against each chip's manual rather than writing values copied from the VX855.

##### 29.5 Display FIFO programming is explicitly per-chip

For secondary-display FIFO, OpenChrome selects **different values for VX800, VX855 and VX900** in `CR68`, `CR92`, `CR94` and `CR95`, while preserving a similar formula structure. [OpenChrome `src/via_display.c:L2829–L2878`]. It describes different FIFO-depth-related quantities for these chipsets.

**Practical warning:** even if a 2D blit executes correctly and the CRT registers look familiar, insufficient scanout FIFO settings can cause visible corruption, tearing, or intermittently wrong rendering under bandwidth pressure. Do not assume the VX855 tuning is appropriate for VX800/VX900.

##### 29.6 LCD power sequencing differs across families

`viaFPPower()` takes **software LVDS power sequencing** for VX800/CX700 because of a documented implementation issue in OpenChrome's comments, but uses **hardware primary-panel sequencing** for VX855 and VX900 [OpenChrome `src/via_fp.c:L328–L365`]. This mainly affects actual LVDS-equipped boards, not an analog-only CRT system, but is essential for a generalized driver.

### CE scanout and flip status

<a id="c9-section-189"></a>

#### 189. CE scanout-flip and ring analysis scope

**Evidence:** original retail x86 Windows CE 6.0 `DDI_VIA.dll`, verified SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`; linker map `DDI_VIA.map`; user-supplied VX800/VX855/VX900 Part I and II programming manuals. **Method:** read-only Intel disassembly of named functions, independently cross-checked against the original PDFs. No CE binary was executed, and no GPU MMIO was accessed.

This pass distinguishes **(i) VBlank waits**, **(ii) a pending primary starting-address update**, **(iii) an explicit secondary-display flip flag**, and **(iv) INV ring-space polling**. All can appear as apparently stuck display work, but they monitor different events. The precise CE display update order is not necessarily the same as the existing Windows 98/OpenChrome CRTC write-last approach.

<a id="c9-section-190"></a>

#### 190. Three different display-status bits must not be treated as one flip-completion flag

| Access in CE code | Manual meaning, all three families | Visible loop(s) | Interpretation |
|---|---|---|---|
| MMIO `+0x200`, bit 1 (`0x02`) | **Vertical Blanking Status**, primary display | `FlipShrinkPrimarySurf` at `0x10029DB0–0x10029DBD`; `FlipPrimarySurface` at `0x10029F3D–0x10029F4A` | Waits while the primary VBlank status bit is set on selected paths |
| MMIO `+0x204`, bit 2 (`0x04`) | **IGA2 Vertical Blanking Status**, secondary display | `FlipShrinkPrimarySurf` at `0x10029DD7–0x10029DE4`, `0x10029E88–0x10029E95`, etc.; `FlipPrimarySurface` at `0x10029F4E–0x10029F5B` | Waits while the secondary VBlank status bit is set |
| MMIO `+0x204`, bit 1 (`0x02`) | **Graphics Starting Address Update Status**, pending update begun by `+0x214` write | `BltPrepare` at `0x100136B2–0x100136BF` (§182) | Waits until the primary address update completes |
| Indexed **CR61**, bit 5 (`0x20`) | **Second CRTC Vertical Status 2**, secondary-display **Flip Flag** | `FlipPrimarySurface` at `0x1002A00B–0x1002A015` on selected paths | Waits for the secondary flip flag to clear |

**Potential source of confusion:** the MMIO `0x204` register simultaneously holds **IGA2 VBlank in bit 2** and **primary address-update-pending in bit 1**. Polling the same address with a different mask does **not** mean polling the same event. The `0x200` bit 1 and `0x204` bit 2 loops are VBlank-related; the `0x204` bit 1 loop from §182 is an address-update wait. These meanings were checked in all three **Part II** manual descriptions. The CR61 meaning was checked separately against all three **Part I** VGA register descriptions.

Each loop shown above has no local sleep/yield or timeout; a permanently asserted tested bit can hold the calling thread. Nothing in this static evidence proves that such a permanent condition occurs during a valid OS workload.

<a id="c9-section-191"></a>

#### 191. `FlipPrimarySurface` uses two MMIO display-start registers, not a universal `CR34` commit

The linker map names `VIA_VIDEO:VideoUtils.obj` function **`FlipPrimarySurface` at `0x10029F1A`**. Its selected non-shrink path first polls MMIO `0x400` with a mask containing `0x3` (`0x10029F82–0x10029F98`); only then does it program the selected starting address. The code has conditional branches based on the display/duoview state; it does **not** always program both start-address registers.

Observed write forms (where `addr` is the function's incoming address argument):

```text
IGA1 shadow:   MMIO + 0x214 <- (addr >> 1)       ; 0x10029FB4–0x10029FBD or 0x10029FFE–0x1002A000
IGA2 address:  MMIO + 0x218 <- (addr & ~7)       ; 0x10029FDD–0x10029FE9 or 0x10029FD1–0x10029FD4
```

The VGA manuals describe `+0x214` as **CRT Starting Address Shadow** (IGA1/primary) and `+0x218` as **The Second Display Starting Address** (IGA2/secondary), including the latter's 8-byte alignment. The code's `>>1` is a **verified software conversion**; its exact input unit/semantic contract is not independently established merely from the call signature.

The code calls `IsShrinkEnable` (`0x1000DCB3`), whose true branch delegates to `FlipShrinkPrimarySurf` (`0x10029D81`), rather than silently applying this non-shrink sequence. A separate OpenChrome primary-scanout implementation writes **CR0C, CR0D, CR48, then CR34 last**; do not mistake the CE **MMIO shadow-address** path above for identical register writes. They are two code-level recipes, each requiring its own display-mode and chip validation.

<a id="c9-section-192"></a>

#### 192. `FlipShrinkPrimarySurf` has additional VBlank, engine and duoview branches

`FlipShrinkPrimarySurf` (`0x10029D81–0x10029F19`) does not simply assign a new framebuffer address. Its visible paths check display-related flags, sometimes wait for **primary VBlank** via `+0x200 & 0x02`, sometimes wait for **secondary VBlank** via `+0x204 & 0x04`, and in other paths poll **engine status** via `+0x400` against a software-selected mask. Its address writes are to `+0x214` and `+0x218`, conditioned by these flags; some paths invoke `IsDuoviewEnable` (`0x1000DA6A`).

This matters when correlating a screenshot, a completed flip and a stalled blit: a flip call can be in a VBlank wait **before any newly requested address is written**, or in a different wait after a write. Instrument the **specific program counter and tested mask**, not merely the most recent logical `Flip` request. It does not prove that the user's Windows 98 mode-switch or frozen-ring behavior follows this CE path.

<a id="c9-section-193"></a>

#### 193. CR61[5] resolves the remaining secondary-flip wait's meaning

In `FlipPrimarySurface` the branch at `0x1002A00B` reads register index `0x61` with the shared indexed **CRTC helper** `0x1000DA83`; that helper uses I/O ports **`0x3D4`/`0x3D5`**, *not* sequencer `0x3C4`/`0x3C5`. The caller loops while bit `0x20` is set (`0x1002A012–0x1002A015`). All three Part I manuals document **3X5.61 bit 5** as the second-display **Flip Flag** (VX900 calls it *Second Display start address change/Flip Flag*).

Thus this is a **secondary-display flip-completion status wait**, distinct from the primary `MMIO+0x204[1]` pending-address-update wait. Some branches skip it. The loop has no local timeout; the exact external event that clears this bit was not reproduced on hardware. **A previous intermediate note that called this SR61 was incorrect**: disassembly of `0x1000DA83` and the VIA VGA tables confirm **CR61**.

<a id="engine-2d"></a>

## 2D engine and host transfers

Ordinary H6 fill and copy share a documented baseline. AlphaBLT, color conversion, source-key polarity, framebuffer location and FE01 packing have narrower rules. Callback equality is specific to each CE build.

### GE opcodes, registers and common copy/fill

<a id="c9-section-5"></a>

#### 5. 2D graphics engine: excellent reuse candidate

##### 5.1 Shared core offsets

All three 2D register summaries use substantially the same GE layout:

| MMIO offset | Meaning | Reuse assessment |
|---:|---|---|
| `0x00` | GE command / ROP / operation select | Common base; VX900 operation extension |
| `0x04` | GE mode/status, destination color depth | Common base; VX900 alpha fields |
| `0x08` | Source/destination pitch and framebuffer location | Common concepts |
| `0x0C` | Destination rectangle dimensions | Common |
| `0x10` | Destination X/Y or bitblt address | Common |
| `0x14` | Destination map base | Common |
| `0x18` | Source address | Common |
| `0x1C` | Source map base | Common |
| `0x20` | Pattern address | Common |
| `0x24`,`0x28` | Mono pattern/style data | Common |
| `0x2C` | Bresenham error term | Common |
| `0x40`,`0x44` | Clipping limits | Common |
| `0x48` | Color/chroma key control | Common concepts |
| `0x4C`,`0x50` | Foreground/background color or keys | Common concepts |
| `0x58`,`0x5C` | Pattern colors | Common concepts |
| `0x100–0x1FF` | Pattern RAM | Common concept; verify extent per family |

**Evidence:** 2D register summary in [V8-I, p. 83], [V855-I, p. 80], [V9-I, p. 103].

Reuse the **operation description** and most blit/fill calculations. Do **not** reuse unmasked writes to fields that are documented differently; in particular, the VX900 adds new data to `0x04` and changes the `0x60–0x9F` range.

##### 5.2 GE color-format conversion (`0x30`)

- **VX800:** no CFC register at `0x30` in its 2D register summary.
- **VX855:** CFC explicitly documented at the `0x30` dword.
- **VX900:** CFC explicitly documented at the `0x30` dword.

VX855 CFC details include `bit 31` enable, `bit 30` component extension mode, `bit 29` dither/round, destination format `bits 9:8`, source depth `bits 7:6`, and source format `bits 1:0`. The VX855 manual states **source-copy ROP only** and specifically says **16↔32-bit conversion is supported but 8-bit conversion is NOT supported**. Its examples include RGB555↔RGB565 and ARGB8888→RGB565/RGB555. [V855-I, p. 84]; [V9-I, p. 107].

**Correction to a tempting optimization idea:** This register is **not** a documented solution for the driver's **8-bit DIB → 16-bit desktop** path on VX855. Use a CPU conversion/palette lookup, a verified alternative engine path, or retain DIBENG. The VX900 excerpt lists a similar CFC register but omits the VX855's explicit exclusion note; the omission does **not** prove VX900 8-bit CFC works.

Possible useful VX855 experiments: `555→565`, `565→555`, `8888→565`, `565→8888`, with exact pixels compared to a CPU reference and destination alpha checked.

##### 5.3 VX900 adds GE AlphaBLT

The VX900 `0x00` command-selector table adds `0011b` = **AlphaBLT**, in addition to the preexisting blit, text, line and rotation operations. Its `0x04` register also describes bit 20 (constant vs per-pixel alpha) and bits 19:12 (constant alpha value). [V9-I, pp. 104–105].

**Manual inconsistency:** the VX900 `0x04` table *also* labels bits `31:10` as read-only/reserved, overlapping the alpha fields. The narrower fields likely represent the intended alpha controls, but that is an **inference**. Treat AlphaBLT as experimental until tested on physical hardware. Do not enable an alpha bit blindly in the shared GE code.

**Test:** source-over-destination in 16- and 32-bpp for constant alpha 0, 1, 127, 128, 254, 255 and a per-pixel alpha gradient. Compare every output channel to the chosen reference semantics (not just visual appearance). Check whether hardware expects premultiplied or straight alpha and whether ROP `CC` is mandatory.

##### 5.4 Legacy 2D/3D controls are not portable to VX900

| Offset | VX800 / VX855 | VX900 |
|---|---|---|
| `0x60` | 3D/2D ID control | Reserved as part of `0x60–0x9F` |
| `0x6C` | 3D/2D wait control | Reserved as part of `0x60–0x9F` |

**Evidence:** [V8-I, p. 83]; [V855-I, p. 80]; [V9-I, p. 103].

**Porting rule:** Replace these writes with the VX900 CR locking/fence scheme; avoid falsely interpreting a read of zero from reserved registers as proof of successful synchronization.

<a id="c9-section-25"></a>

#### 25. Exact H6 2D recipe and lessons for Win98 GDI

The OpenChrome H6 solid-fill and copy procedures give direct register-level examples that complement the manuals [OpenChrome `src/via_exa_h6.c:L139–L265`]. They are **source examples**, not binaries to copy into the Win98 driver.

| Operation stage | OpenChrome behavior | Potential VX855/VX800/VX900 reuse |
|---|---|---|
| Solid setup | Sets a supported screen mode, validates plane mask and selects pattern ROP | Shared GDI `PATCOPY`/solid-fill fallback decision |
| Solid emission | `GEMODE`, `DSTBASE`, `PITCH`, `DSTPOS`, `DIMENSION`, foreground, `GECMD` | Common H6 2D emission ordering |
| Copy setup | Requires equal source/dest bits-per-pixel, checks pitch alignment | Hardware-supported same-format copy; **not** a color conversion path |
| Copy direction | Adjusts starting X/Y to the last pixel for negative X/Y directions | Overlapping source/destination acceleration |
| Copy emission | Sets source/dest base, pitch, positions, dimensions, then fires command | Useful ordering cross-check for the GDI blitter |
| Completion | Uses `ADVANCE_RING`; actual flush may be direct MMIO or DRM submission | Do not mistake this API name for proof of GPU ring fetching |

OpenChrome writes **framebuffer base addresses in 8-byte units** (`dstOffset >> 3`, `srcOffset >> 3`) and **pitches in 8-byte units** (`dstPitch >> 3`, `srcPitch >> 3`) for these H6 2D operations. The width and height fields are written as `width - 1` / `height - 1`. [OpenChrome `src/via_exa_h6.c:L164–L185`, `L229–L265`; manuals V8-I/V855-I/V9-I, 2D GE chapters].

##### 25.1 8-bpp implications — an important distinction

`viaAccelSetMode()` explicitly handles **8, 16 and 32 bpp** [OpenChrome `src/via_exa.c:L216–L235`]. Thus there is source evidence for an **8-bpp 2D GE mode**, but this is **not** evidence that an 8-bpp DirectDraw HAL, 8→16 conversion, or all Windows 98 8-bpp mode transitions are implemented.

OpenChrome also:

- Refuses hardware copies when **source and destination bits-per-pixel differ** (`via_exa_h6.c:L200–L202`).
- Explicitly refuses **partial plane masking in 8 bpp** (`via_exa_h6.c:L44–L57`).
- Requires **8-byte destination pitch alignment** for solid/copy operations and checks source pitch alignment separately (`via_exa_h6.c:L145–L152`, `L200–L208`).

**Win98 implication:** treat 8-bpp GE acceleration, 8-bpp DirectDraw HAL exposure, palette handling, and 8→16 DIB conversion as **four separate issues**. These source paths support testing 8-bpp fills and copies on the VX855; they do **not** overturn the VIA manual's statement that its CFC cannot do 8-bpp conversion (§5.2).

##### 25.2 2D feasibility checks worth bringing into the VX855 backend

The following checks are useful even when the direct-copy engine is known to work:

1. Reject a zero width/height **before** constructing `(dimension - 1)`.
2. Validate pitched and surface-base alignment in bytes before shifting into hardware units.
3. Distinguish a full planemask from unsupported partial-byte masks.
4. For overlapping blits, select X/Y traversal direction and adjust the starting pixel correspondingly.
5. Separate “prepared operation” from “queued/fired command” in traces so the engine state is not confused with actual work.

These are engineering recommendations inferred from the common OpenChrome code, not a claim that the current VX855 source lacks them.

<a id="c9-section-300"></a>

#### 300. Three-way 2D command-equivalence audit: evidence scope

This pass resumes the requested **VX800 / VX855 / VX900** programming comparison; it does not diagnose the user's Win98 driver. Sources are all three original VIA Part I graphics manuals, original CE 6.0 retail `DDI_VIA.dll` with its linker map, and OpenChrome's shared H6 2D emitter. The read-only `Chrome9_2D_Three_Family_Dispatch_Audit_300_311.py` verified the original CE DLL SHA-256 (`529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`) and produced **28 operation/mode entries representing 84 independently confirmed callback-pointer assignments**. The distinction between *same opcode*, *same binary callback*, *same driver behavior*, and *same hardware behavior* is preserved below. No board tests were performed.

<a id="c9-section-301"></a>

#### 301. Common 2D GE opcode baseline

All three Part I manuals list the same 2D command selections for `GE+0x00`: `0001b` BitBLT, `0010b` monochrome text/transfer, `0101b` line, and `1001b`/`1010b` blit/text-then-rotate. The rotation angle field at `GE+0x04[1:0]`, mono pattern registers `GE+0x24/0x28`, and clipping coordinates `GE+0x40/0x44` retain their documented offsets. These are **documented coding equivalences**, not a full equality proof for every surface format, pixel size or source/destination location. The OpenChrome H6 fill/copy emitter is additional source-level evidence for ordinary supported configurations.

<a id="c9-section-302"></a>

#### 302. VX900 has a 2D AlphaBLT opcode absent from the older manuals

`GE+0x00` opcode `0011b` is marked **reserved** in the VX800 and VX855 2D command tables, but **AlphaBLT (ROP `CC`)** in VX900's GE command table. VX900's `GE+0x04` describes an alpha source selector at bit 20 and constant alpha at bits 19:12, while that same register's broader tabulation says `31:10` reserved; retain this **within-manual contradiction** rather than interpreting all bits as proven working silicon. The CE 2D dispatch-table callbacks do not include a distinct hardware AlphaBLT entry, so **VX900 capability is documented, but not verified as exercised by these CE dispatch callbacks**. This opcode must not be ported blindly to VX800/VX855.

<a id="c9-section-303"></a>

#### 303. Framebuffer location selector `11b` differs by generation

All manuals document the legacy S.L./S.F. choices at `GE+0x08` for source, destination and pattern framebuffer location. For the corresponding `11b` selector, **VX800 says reserved** while **VX855/VX900 document L.L.** (local-local). This is a genuine *documented encoding* difference across the family; it does not prove that VX800 hardware rejects the selector. The result agrees with other new-generation memory-location extension patterns, but does not establish a common memory aperture or identical bus behavior.

<a id="c9-section-304"></a>

#### 304. Hardware color-format conversion: a VX855/VX900 documentation extension

VX800 documents `GE+0x30` as reserved, while VX855 and VX900 document hardware color-format conversion fields. The VX855 manual explicitly cautions that only `ROP=CC` is supported and 8-bpp is not supported for this operation. The VX900 CFC table defines 16-/32-bpp color formats and lists an 8-bpp source-depth code. It omits the VX855-only ROP `CC` and 8-bpp exclusion notes. Actual VX900 8-bpp conversion remains unresolved. See VX855 Part I PDF p. 84 and VX900 Part I PDF p. 107. Some CE callback slots select **identical CPU pixel-conversion routines** for 15→16, 15→32, 16→15, 16→32 across all three chips; this is independent of native GE+0x30 hardware CFC. Thus shared CPU converters do not prove that VX800 supports the hardware CFC command.

<a id="c9-section-305"></a>

#### 305. GE color-key selector uses a different family mask in CE code

The examined CE screen-copy code for the older implementation emits a `0x4000` source-key control where VX900's implementation uses `0x6000` (original retail instructions at the addresses verified in the audit). The manuals differ in the interpretation/availability of `GE+0x48` key control: the VX855 table fixes/reserves a key-type choice that is programmable in the other family documents. **This is a verified driver-command difference** and a *documented field difference*, not evidence that VX900 requires the additional bit for all color keys. Source key, destination key, transparency mode and exact format need separate tests before declaring byte-identical rendering.

<a id="c9-section-310"></a>

#### 310. Rotation and mono/pattern formats: common selectors but nonuniform builders

The manuals agree on the basic rotate-after-blit command selectors and angle encoding, and on the mono-pattern and clipping register locations. Yet the CE driver often selects **different host-transfer packet builders**, particularly in the zero mode, for the three chips. Hardware compatibility is therefore **confirmed only for documented opcode/register structure**, not for FE00/FE01 packet counts, edge padding, 8-bpp palette expansion, ROP interactions or pixel/line alignment. Prior report sections (§§164–180) analyze the source-copy and 8-bpp capacity questions, which remain valid but do not complete a three-board behavior test.

<a id="c9-section-311"></a>

#### 311. Three-way 2D classification and remaining evidence

The accompanying `Chrome9_2D_Three_Family_Register_Matrix_300_311.csv` labels **same documented opcode**, **newer-generation extension**, **shared software callback** and **unresolved runtime equivalence** separately. Highest-priority hardware gaps are: VX900 GE AlphaBLT (`0011b`) behavior/ROP boundaries; `GE+0x08` location `11b` on actual VX800; VX855/VX900 CFC format and pixel restrictions; VX900-versus-older color-key semantics; and FE01 transfer tails/rotation for minimum and unusual dimensions. All code/manual checks passed, but **no measured cross-board outcomes are represented**.

### CE callback dispatch, line drawing and pattern operations

The retail CE 7 build changes 11 of 28 sharing patterns relative to CE 6.0. See [the build-by-build comparison](#c9-section-367).

<a id="c9-section-306"></a>

#### 306. Exact 2D callback topology is 11 + 11 + 6 per chipset

The original CE initializes 11 operation callback slots for one command-interface branch, 11 alternative slots for the other branch, and six shared callbacks. The identical 28 assignments per family were checked at `VX800 0x1001B520`, `VX855 0x1002622A`, and `VX900 0x10029295`, giving **84 function-pointer stores in CE 6.0**. The `zero` and `nonzero` labels in the CSV refer to the comparison against the command-interface mode field; they are **not** an assertion that zero means a particular bus transaction type on every call. The `common` callbacks appear outside this branch choice. See `Chrome9_2D_Three_Family_Dispatch_Matrix_300_311.csv`.

<a id="c9-section-307"></a>

#### 307. Selected zero-mode callbacks: two shared by all, several distinct

Of the 11 zero-branch operation slots, **two** point to exactly the same routine for all three families: **solid fill** and **color-pattern fill**. Monochrome host transfer, patterned monochrome transfer, 8-bpp host transfer, and native pixel transfer each use **three distinct callback addresses**. VX800 and VX855 share image-pattern fill, patterned 8-bpp transfer, 8-bpp host-image writer, raw host-image writer and screen-to-screen copy, whereas VX900 substitutes its own routines. All this is a **binary dispatch identity classification**; differing functions may still emit compatible commands, and shared functions can still vary based on device-global state.

<a id="c9-section-308"></a>

#### 308. Nonzero-mode callbacks show selective VX855/VX900 reuse

In the CE 6.0 nonzero branch, **three slots are identical across all three** (solid fill, 8-bpp host-image writer and raw host-image writer). VX855/VX900 alone share **five** slots (color-pattern fill, patterned mono transfer, mono host transfer, image-pattern fill and patterned 8-bpp transfer, C9-059, 060, 062, 063 and 064). VX800/VX855 alone share three others (8-bpp host transfer, native pixel transfer and screen-to-screen copy). In CE 7, solid fill moves to the VX855/VX900 pair and the three VX800/VX855 pairs split into distinct callbacks (see §367).

<a id="c9-section-309"></a>

#### 309. Six unconditional CE 6.0 callbacks share entry points across all three families

The six callbacks assigned unconditionally by each initializer include a blank rectangle helper, fast 8-bpp copy helper, and four CPU color conversions (15→16, 15→32, 16→15, 16→32). These are **exact same entry points** in the single retail DLL. These entry points establish software reuse in CE 6.0, including the CPU converters. In CE 7 only the four CPU conversions share addresses across all three. The blank rectangle and fast 8-bpp copy helpers share addresses on VX855/VX900 (see §367).

<a id="c9-section-367"></a>

#### 367. CE 6.0 and CE 7 2D callback comparison

The three CE 7 retail `init2Dbltfp*` functions start at `0x1002606D`, `0x100323F9` and `0x10035889`. Their same command-interface field test still selects **11 zero-mode and 11 nonzero-mode function assignments**, followed by **six unconditional function assignments**. Counting the assigned addresses gives these patterns. The CE 6.0 counts come from the archived matrix in §362.

| Group | CE 6.0 documented sharing | CE 7 rederived sharing |
|---|---|---|
| Zero mode, 11 slots | All three 2 · VX800/VX855 5 · distinct 4 | All three 0 · VX800/VX855 2 · VX855/VX900 2 · distinct 7 |
| Nonzero mode, 11 slots | All three 3 · VX800/VX855 3 · VX855/VX900 5 | All three 2 · VX855/VX900 6 · distinct 3 |
| Unconditional, 6 slots | All three 6 | All three 4 · VX855/VX900 2 |

Changed equality patterns are **C9-048, 050, 052, 053, 057, 061, 065, 067, 068, 070 and 071**. The four common CPU conversion callbacks remain shared in CE 7. The two builds can share the same hardware encoding while choosing different compiled entry points.

| Additional CE 7 check | Result and binary anchors |
|---|---|
| Screen-copy source key | In the examined nonzero-mode screen-copy path, the VX800 and VX855 functions OR `0x4000` into the `+0x48` field at `0x10024F3F` and `0x100315FA`. VX900 ORs `0x6000` at `0x10034642`. The extra bit is the documented key-type bit where defined. The instructions corroborate the specific path in C9-043. |

<a id="c9-section-112"></a>

#### 112. Actual 2D screen-copy command divergence: VX900 selects different source-key polarity

**Sources:** original VIA CE 6.0 Group2 retail `DDI_VIA.dll` (SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`); its `DDI_VIA.map`; VIA **VX800 Part I**, PDF p.87 / printed p.81; **VX855 Part I**, PDF p.85 / printed p.79; **VX900 Part I**, PDF p.108 / printed p.102. This section analyzes *actual CE blit code*, not just register-format similarities.

The retail DLL's `vScreenToScreenBltVX800` and `vScreenToScreenBltVX855` map to one **identical function body** at `0x1001A8B1` (end `0x1001AB90`). The VX900-specific function `vScreenToScreenBltVX900` begins at `0x10027ED4` (end `0x100281B3`). Both are `0x2DF` (=735) bytes; that equal size alone does not establish byte identity or operational equivalence.

The important branch checks bit **`0x04`** in the CE blit parameter at `[esi+0x24]`, then enables the color-key control before issuing the GE command:

| Case | VX800 / VX855 at `0x1001AA27` | VX900 at `0x1002804A` | Meaning under the public MMIO `0x48` definition |
|---|---|---|---|
| Key-conditional operation | `OR [GE + 0x48], 0x4000` | `OR [GE + 0x48], 0x6000` | Both enable **source-key select** (bit 14); VX900 additionally sets **key type** (bit 13) |
| After the draw | `MOV [GE + 0x48], 0` at `0x1001AB82` | `MOV [GE + 0x48], 0` at `0x100281A5` | The function explicitly resets the key control on its normal completion path |

VIA's **VX800** and **VX900** manual tables describe `0x48[13]` as: `0` = write on key **match**, `1` = write on **mismatch**. The **VX855** manual, in contrast, marks `[13:0]` reserved/RO while stating in its explanatory note that source-key writes occur on a *nonmatch*. Consequently, these are **not interchangeable source-key recipes** across the three generations, even though the same OpenChrome/VIA 2D-copy architecture is shared.

**Unresolved VX800 nuance:** The CE VX800 routine uses `0x4000` although the VX800 manual says bit 13 is programmable and its value zero selects *match*. This apparent mismatch requires a controlled source-key test or a more complete register-state trace; it is **not** enough to declare the driver buggy. `OR` preserves a bit-13 value left by earlier code, and the normal completion path clears the register. Don't infer the actual incoming bit-13 value in every possible caller path from the literal `OR` alone.

**Porting rule:** Keep the **source-key enable and polarity** as distinct fields in a family-aware 2D helper. Never replace `0x4000` with `0x6000` on VX855 without proving that its reserved bit is accepted, and don't assume setting only `0x4000` yields the same key-match semantics on VX800 and VX900.

<a id="c9-section-113"></a>

#### 113. VX900 screen copy explicitly resets rotation when programming GE color depth

Both CE screen-copy functions write the 2D **GE Mode and Status** register at MMIO `0x04`, whose VIA manuals document color-depth bits `[9:8]` and rotation bits `[1:0]`.

- VX800/VX855, at `0x1001A96B–0x1001A985`, computes the new depth bits with `XOR`, masks `0x300`, and XORs the original mode word back. Algebraically this is **`(old_mode & ~0x300) | new_depth_bits`**, which **preserves rotation `[1:0]`**.
- VX900, at `0x10027F91–0x10027FA8`, masks the old mode with **`0xFFFFFCFC`** and ORs the new depth field. Since `~0xFFFFFCFC = 0x00000303`, it clears **both** color depth `[9:8]` **and rotation `[1:0]`** before inserting the new depth. Equivalently **`(old_mode & ~0x303) | new_depth_bits`**.

This is an actual difference in the **state programmed by the CE driver**, not just a compiler spelling change. The formulas give identical depth bits but differ when the incoming rotation mode is nonzero.

| Incoming GE rotation | VX800/VX855 subsequent screen-copy setup | VX900 subsequent screen-copy setup |
|---|---|---|
| `00b` | `00b` | `00b` |
| `01b`, `10b`, or `11b` | Preserved | Reset to `00b` |

**Interpretation:** VX900's separate screen-copy routine appears to avoid carrying rotation state into an ordinary copy; older generation behavior may be intentional because upstream code guarantees an appropriate mode. Whether the preserved old rotation is ever nonzero in normal CE calls has not been established. A useful controlled test sets each rotation mode before a copy and compares expected pixels, with the normal GE state restored afterward. Do not treat this as a proven VX855 bug.

<a id="c9-section-114"></a>

#### 114. Reconstructed real 2D backend dispatch: VX900 reuses more VX855 code than its symbols suggest

Three actual `init2Dbltfp...` routines were statically decoded from the retail DLL:

| Chipset | Routine VA | Function-pointer entries selected |
|---|---|---|
| VX800 | `0x1001B520` | 11 AGP-labelled, 11 PCI-labelled, 6 common-tail assignments |
| VX855 | `0x1002622A` | 11 AGP-labelled, 11 PCI-labelled, 6 common-tail assignments |
| VX900 | `0x10029295` | 11 AGP-labelled, 11 PCI-labelled, 6 common-tail assignments |

The routines branch on a runtime field at `[driver_context + 0x70]`. Calling the two branches **AGP-labelled** and **PCI-labelled** follows the actual function names installed there; the branch condition alone is not an independent proof of bus activity.

**Confirmed shared compiled implementations in all three dispatch tables:**

| Operation | 2D function-pointer slot | Three-chipset target | Meaning |
|---|---|---|---|
| PCI solid fill | `0x100CA11C` | `0x10024CDE` | `vSolidFillVX800` / `vSolidFillVX855`, same body also installed by VX900 |
| AGP solid fill | `0x100CA11C` | `0x10024EB3` | Shared `vSolidFillVX800_AGP` / `vSolidFillVX855_AGP` |
| Fast 8-bpp copy | `0x100CA134` | `0x10018795` | One compiled implementation also aliased to `CX700` |
| Blank rectangle | `0x100CA110` | `0x10026045` | Same `vBlankRectangleVX800` / `vBlankRectangleVX855` function installed by all three |

**Confirmed selected differences:**

| Operation and branch | VX800 target | VX855 target | VX900 target |
|---|---|---|---|
| PCI screen-to-screen blit | `1001A8B1` | `1001A8B1` | **`10027ED4`** |
| AGP screen-to-screen blit | `1002594F` | `1002594F` | **`100281B3`** |
| PCI mono transfer | `10017E32` | `10022F11` | **`10022F11`** (VX900 reuses VX855) |
| PCI image-pattern fill | `1001AB90` | `10025D98` | **`10025D98`** (VX900 reuses VX855) |
| PCI 8-bpp transfer | `1002356F` | `1002356F` | **`10026724`** |

This is useful evidence for a **common 2D core with per-operation overrides**, rather than choosing an entirely separate 2D engine implementation solely on GPU generation. The source-level identity of individual function targets is stronger than merely observing that some register definitions look alike; it does **not** establish that the same function is always safe under every surface type or display mode.

The companion `Chrome9_CE_2D_Dispatch_and_Commands_112_116.csv` exports **all 84 actual pointer assignments** with chipset, branch, assigned pointer slot, target address, source instruction VA, and linker-map aliases. This is reproducible directly from the MSI's retail map and DLL.

<a id="c9-section-115"></a>

#### 115. Shared 2D wait/fallback behavior and consequences for VX855 stability analysis

Both the VX800/VX855 and VX900 PCI screen-copy routines begin by repeatedly reading **`[graphics_MMIO + 0x400]`**. They combine a global engine-status mask with bit 1, and branch backward to repeat the read while the masked status is nonzero. This is the same **unbounded busy-poll form** in both implementations. The examined loop has **no local iteration-count or timeout check** before proceeding.

Both routines also call the **same** `VIA::HandleEmulateBlt` function (`0x10013251`) for a particular overlapping source/destination rectangle condition. They otherwise share key geometry calculations and 8-byte-unit base-address writes to `GE + 0x14` and `GE + 0x1C`.

**Consequences:**

1. The wait and overlap fallback are **software-level compatibility evidence** across the family, not a VX900-specific wait-patch.
2. An engine stuck busy could make the CPU spin in this CE code path. That is relevant as an implementation pattern to compare against the user's VX855 Windows 98 responsiveness and hang recovery, **not evidence of the same hang or any CE failure**.
3. Because this is the source's *screen-copy routine* rather than the specialized 3D `WaitEngIdle_CR` callback, do not conflate the MMIO `+0x400` status polling with the separate `0x6C` idle-wait command.

<a id="c9-section-116"></a>

#### 116. Safe family-porting rules, unresolved claims, and read-only verification

**Ready for implementation comparison (observed):** shared PCI solid-fill backend, shared fast 8-bpp copy, shared blank rectangle, shared rectangle math and overlapping-blit fallback; distinct VX900 PCI/AGP screen-copy routines; distinct VX900 key-polarity and GE rotation-control setup.

**Still unresolved:** whether VX800 source-key bit13 is already set by higher-level paths, whether the 2D hardware's match/mismatch behavior on every revision matches the manual, whether the VX900 rotation-clear is a hardware workaround or a state-hygiene precaution, and whether the unbounded CE status polling is reached under real hangs.

**Suggested non-destructive tests:** for each chipset, run 8/16/32-bpp copies with and without keying; use sources containing equal and unequal key colors; alternate keyed and unkeyed copies; repeat after setting each rotation selection; test both same-surface overlap and separate surfaces. Compare pixels with a CPU reference and log `GE[0x04]`, `GE[0x48]`, and `GE[0x400]` **without assuming these reads are safe while commands are in flight**. On hardware, use a bounded diagnostic timeout and restore state. These are proposed experiments, not performed tests.

**New verifier:** `Chrome9_CE_2D_Blt_Compatibility_Audit_112_116.py` checks the original retail DLL hash, exact linker aliases, both color-key masks, key-control cleanup, the color-depth/rotation logic, shared MMIO busy poll and fallback, and algebraically verifies the GE-mode formulas for representative inputs. It exports the 84-row dispatch matrix. All checks pass. It performs **no driver execution or hardware I/O**.

<a id="c9-section-117"></a>

#### 117. Line-drawing dispatch: VX900 deliberately selects the VX855 line backend

**Evidence:** VIA Windows CE 6.0 Group2 retail `DDI_VIA.dll` (SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`), `DDI_VIA.map`, constructor/configuration disassembly at `0x10015B2D–0x10015C36`. **Reference:** VIA VX800/VX855/VX900 Part I, `2D Engine Register` / `GE Command` / `3D / 2D Status` sections.

VIA separates the family at the outer 2D initialization dispatcher, but **the VX900 branch explicitly calls the VX855 line-function initializer**:

| PCI GPU ID | 2D blit-pointer initializer | Line-pointer initializer | Hardware line function when using the PCI-labelled mode |
|---|---|---|---|
| `1106:1122` VX800 | `init2DbltfpVX800` at `0x1001B520` | `init2DlinefpVX800` at `0x10017DEA` | `AcceleratedSolidLineVX800` (`0x10017242`) |
| `1106:5122` VX855 | `init2DbltfpVX855` at `0x1002622A` | `init2DlinefpVX855` at `0x10022EB1` | `AcceleratedSolidLineVX855` (`0x10022309`) |
| `1106:7122` VX900 | `init2DbltfpVX900` at `0x10029295` | **`init2DlinefpVX855` at `0x10022EB1`** | **`AcceleratedSolidLineVX855` (`0x10022309`)** |

The call at **`0x10015BBA`** is shared by the immediately preceding VX855/VX900 branches. The VX800 path calls its own initializer at **`0x10015C31`**. No `AcceleratedSolidLineVX900` body appears in the provided retail linker map. This is **affirmative evidence of shared compiled line handling**, not an inference solely from the absence of a VX900 symbol.

The line dispatch initializer chooses an AGP-labelled emitter when `[context+0x70] == 0` and otherwise the PCI-labelled emitter; this is the same branch convention used in the existing 2D function-pointer matrix. The transport labels are linker names and do not independently prove activity on an external AGP bus.

<a id="c9-section-118"></a>

#### 118. VX800 versus VX855 PCI line algorithms: same hardware instructions; three software-state references differ

The examined PCI `AcceleratedSolidLineVX800` (`0x10017242–0x100176D4`) and `AcceleratedSolidLineVX855` (`0x10022309–0x1002279B`) disassemble into **345 instructions each**. After normalizing only control-transfer *destination addresses* (not opcodes or data values), **342 instructions match exactly in order**. The other three differ only in the address of an internal software-state flag:

| Operation | VX800 instruction VA / memory | VX855 instruction VA / memory |
|---|---|---|
| Test/reset special line state | `0x10017317` / `0x100CA3A4` | `0x100223DE` / `0x100CA3B4` |
| Clear state | `0x1001732D` / `0x100CA3A4` | `0x100223F4` / `0x100CA3B4` |
| Set state | `0x100174BD` / `0x100CA3A4` | `0x10022584` / `0x100CA3B4` |

These are **addresses in the CE DLL's data region**, not differences in graphics MMIO register numbers. Their precise variable names are not established by the public linker map. In all other respects the compared machine-instruction sequence—including special-case fallback checks and hardware command construction—matches.

Both paths compose the line command using **`OR ..., 0x00103005`** (`0x1002255C` in VX855). The low GE opcode is `5`, which the VIA manuals identify as **Bresenham line drawing**. Both paths also program clipping registers (`GE + 0x40`, `GE + 0x44`) and poll `graphics_MMIO + 0x400` for idle status before accessing the engine. Both share a software line-emulation fallback (`EmulatedLine@GPE`, address `0x10076330`).

**Porting significance:** VX800/VX855 line emission—and VX900's selected VX855 line emission—can be represented by the same line algorithm. Preserve the separate software-state variables when auditing the original CE implementation; their separate addresses are *not* evidence of changed 2D line-register semantics. Do not assume that every Windows 98 GDI line style can be accelerated; the CE routine is specifically the solid-line acceleration path and has fallback conditions.

<a id="c9-section-119"></a>

#### 119. Monochrome host transfer: same base GE command, different surrounding software

The 2D dispatch matrix shows **VX900 uses the exact VX855 PCI monochrome transfer function** (`vMonoXferVX855` at `0x10022F11`), while VX800 uses `vMonoXferVX800` at `0x10017E32`. The three *AGP-labelled* mono functions remain distinct: `0x100180A0` (VX800), `0x100231AC` (VX855), `0x1002635C` (VX900).

The two PCI bodies both construct their base 2D command with **`OR edx, 0x000A0141`**. The low nibble `1` is the GE **BitBLT** command, rather than the distinct mono-text command code `2`; the other bits select the monochrome/host-source behavior for this command path. Both also:

- Program GE mode/color depth at `GE + 0x04`, foreground and background/key colors at `+0x4C` and `+0x50`, destination base at `+0x14`, and the destination/rectangle registers.
- Call the same `VIA::HandleEmulateBlt` (`0x10013251`) in conditions that require a fallback.
- Call the common host-data transfer helper at `0x10012BA7` (`VIA::vImageTransfer`).

Their CPU-side source-offset, byte-alignment, and stride calculations **are not instruction-identical**; the VX855 body uses different intermediate calculations and shifts before calling the common host-transfer helper. The exact equivalence of all source cropping and pitch edge cases has **not** been proven. A common GE command encoder is supported; wholesale replacement of one generation's entire rectangle/stride routine is not yet justified.

**Porting/test priority:** Compare 1-bpp bitmap uploads using nonzero left-bit offsets, widths `1, 7, 8, 9, 31, 32, 33`, odd source pitches, transparent/opaque backgrounds, 8/16/32-bpp destinations, and rotated destinations. Validate both the emitted BitBLT setup and the exact host byte stream. These tests are **proposed**, not run on silicon.

<a id="c9-section-120"></a>

#### 120. Color pattern fill: VX800 has an additional rotation prepass absent from the selected VX855/VX900 PCI path

The retail CE driver distinguishes the PCI patterned-fill functions:

- **VX800** uses `vPatFillVX800` (`0x1001A50A–0x1001A8B1`). A branch that tests the display/rotation state (`[context+0x6214] & 0x200`) constructs an 8×8 pattern and conditionally calls **`ColorPatRamRotated` at `0x10012312`**. This is a CPU-side pattern-rotation helper according to the original symbol name.
- **VX855 and VX900** select the same PCI `vPatFillVX855` (`0x10025176–0x100254E5`). This body builds the 8×8 pattern, **but contains no call to `ColorPatRamRotated`**. It still calls other shared helpers (`ColorPatRotated_CSR` at `0x10012468`, `GetRotatedRect` at `0x100126AF`, and the related rectangle-transform helper at `0x10012754`). Therefore **the absence of the first prepass does not mean VX855/VX900 lack all rotation handling**.
- In contrast, **all three AGP-labelled dispatches install the identical compiled `vPatFillVX800_AGP` / `vPatFillVX855_AGP` function** at `0x100254E5`. The three-chip reuse of this path is explicit in the real dispatch tables, despite different PCI function bodies.

This is a confirmed *software algorithm/dispatch difference*, not evidence of a new GE pattern opcode or different Pattern RAM address on VX855. The difference could be a rotation-handling change, a different upstream rotation contract, or an optimization; the supplied static code cannot tell which explanation is correct.

**Practical follow-up:** compare 8×8 colored pattern fills and ROP results at rotations 0/90/180/270, with positive/negative pattern origins, plus CPU reference output. For the Windows 98 port, the decision to reuse the newer PCI path should follow pixel-accuracy tests rather than the function name alone.

<a id="c9-section-121"></a>

#### 121. 2D line and pattern compatibility

| Subsystem | Evidence-based family conclusion | Outstanding limitation |
|---|---|---|
| Solid Bresenham line | VX800/VX855 algorithm matches apart from 3 software globals; VX900 selects the VX855 routine | Styled/dashed/thick lines and actual hardware pixel accuracy not established |
| PCI host monochrome BitBLT | Shared base command `0x000A0141`; VX900 installs VX855 body | CPU cropping/pitch calculations differ VX800 vs VX855 |
| AGP-labelled mono transfer | Separate code on all three chipsets | Actual MMIO/packet equivalence not yet exhaustively decoded |
| PCI colored pattern fill | VX900 installs VX855 body; VX800 has an optional rotation prepass | Pixel behavior at rotation and origin boundaries untested |
| AGP-labelled colored pattern fill | Identical compiled function selected by all three | Transport paths still need comparison in a normalized command dump |
| PCI source-key screen copy (prior §112) | VX900 uses `0x6000`, VX800/VX855 use `0x4000` | Hardware key-polarity behavior remains untested |

**Correctness rule:** distinguish **same function body**, **same emitted register/command constants**, **same full software algorithm**, and **same observed pixels**. The first three are partially or substantially established by this CE package; the fourth requires real hardware tests. In particular, a selected identical function proves reuse by VIA, not guaranteed behavior under every surface and chip revision.

**New read-only assets:** `Chrome9_CE_2D_Line_Mono_Pattern_Audit_117_121.py` rehashes the exact original retail DLL, validates the chipset line dispatch, verifies the 345-instruction line correspondence and three software-data differences, checks the GE line and host-mono opcode words, and checks the unique VX800 color-pattern rotation helper. It outputs `Chrome9_CE_2D_Line_Mono_Pattern_Matrix_117_121.csv` with 11 classified function paths. No graphics driver was executed or hardware registers written. **The audit passes against the supplied retail DLL.**

<a id="c9-section-122"></a>

#### 122. AGP-labelled monochrome transfers: VX800/VX855 are virtually the same compiled emitter

**Sources:** supplied VIA Windows Embedded CE 6.0 Group2 package; retail `DDI_VIA.dll` (SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`); `DDI_VIA.map`; VIA VX800/VX855/VX900 Part I GE command/register and 2D-host aperture descriptions. **Method:** `objdump -D -M intel` over exact linker-map function ranges, with *only branch destinations* normalized before comparing instruction text. No driver or GPU execution.

| AGP-labelled mono function | VA / end | Instruction count | Relevant evidence |
|---|---|---:|---|
| `vMonoXferVX800_AGP` | `100180A0–10018463` | 300 | Reserves with `lea eax,[esi*4+0xA0]` at `10018150` |
| `vMonoXferVX855_AGP` | `100231AC–1002356F` | 300 | Reserves with `lea eax,[esi*4+0xC0]` at `1002325C` |
| `vMonoXferVX900_AGP` | `1002635C–10026724` | 301 | Changes width/row alignment and reservation calculation |

**VX800 versus VX855:** 299/300 aligned instructions match. The *only* normalized instruction difference is the additional `0x20` (32-byte) fixed reservation allowance on VX855. Their fallback checks, common helper calls (`VIA::HandleEmulateBlt`, `GetRotatedRect`, `VIA::vImageTransfer`), `cmSmartRequest` (`0x1004E42E`) and `cmSmartFlush` (`0x1004E51D`) invocation patterns are the same. Both construct `GE_CMD` with the common low-state constant **`0x000A0141`**, combined with an operation-dependent ROP in the high byte. This is strong evidence for one hardware mono-transfer encoder on VX800/VX855; it is **not** evidence of equal command-queue overhead or identical behavior under every raster operation.

**VX855 versus VX900:** 290 aligned instructions match out of 300 and 301. Changes largely involve the request-size calculation and moved intermediate stores. The subsequent GE command construction still uses `0x000A0141`. The routines are not byte-identical, and the code comparison is not a pixel equivalence test.

<a id="c9-section-123"></a>

#### 123. Exact AGP monochrome command-buffer reservation formulas

The command-space request is made through VIA's common `cmSmartRequest` helper. The allocated size is a **software request for command-buffer capacity**, *not* a measured amount of bus traffic or proof of bytes the GPU consumes. The operand `0xFE000000` appears in all three requests.

Define the following for ordinary nonnegative 1-bpp rectangles in the analyzed entry path:

- `W = right - left` (rectangle width), `H = bottom - top` (height), `O = source_left & 31` (source bit offset), `N = W + O`.
- The following expressions model the observed integer arithmetic for positive inputs; invalid/negative rectangles and 32-bit overflow are outside this equivalence claim.

| Platform | Original expression, simplified (bytes requested) |
|---|---|
| VX800 | `4 * ((N + 31) * H + 36) + 0xA0` = `4*(N+31)*H + 304` |
| VX855 | `4 * ((N + 31) * H + 36) + 0xC0` = `4*(N+31)*H + 336` |
| VX900 | `((((N+7)>>3)*8 + 24) & ~31)*H + 0xC0` = **`align_up(N,32)*H + 192`** |

VX900 implements the row-size calculation using `+7`, arithmetic right shift by three, multiply by eight, `+0x18`, and mask `0xFFFFFFE0`. For positive `N`, this simplifies exactly to `align_up(N,32)`; checked for all `N` from 1 through 2048 by the companion audit. Thus a substantial apparent change between the functions is **software reservation/accounting**, not a different base BitBLT opcode.

Example for `N=33`, `H=2`: VX800 requests 816 bytes, VX855 848 bytes, VX900 320 bytes. These are *requested capacities*, and the large disparity should not be interpreted as equivalent actual packet lengths. A runtime command-buffer capture or a full write-pointer/length proof is required before claiming wasted bytes or a buffer-overrun defect.

<a id="c9-section-124"></a>

#### 124. Patterned monochrome transfer: same core GE command, different reservations and checks

| Function | VA | Instructions | Main distinction |
|---|---|---:|---|
| `vPatMonoXferVX800_AGP` | `100194B3` | 465 | Older setup and fallback checks |
| `vPatMonoXferVX855_AGP` | `1002428A` | 467 | Adjusted geometry checks and larger fixed reservation |
| `vPatMonoXferVX900_AGP` | `1002730C` | 468 | VX900 aligned-row reservation |

The VX855/VX900 pair has **461 matching normalized instructions** (of 467/468). Its differing instructions center on buffering and where the compiler saves intermediate variables. VX800/VX855 have 435 matching normalized instructions (of 465/467); there are additional geometry/fallback and register-allocation changes near function entry. Therefore, it would be inaccurate to classify *every* difference across all three as a size-only change.

All three emit the same base monochrome host BitBLT command **`0x000A0141`** and call `cmSmartRequest`, `cmSmartFlush`, `ColorPatRotated_CSR` (`0x10012468`), the rectangle rotation helpers, and the common host-data transfer routine (`0x10012BA7`). They are compatible at the documented GE command level in this compared operation; buffer reservation and input validation differ.

For positive `N = (source_left & 31) + W` and `H`:

- VX800 observed reservation: `4*((N+31)*H+36)+0x2A0` = `4*(N+31)*H+816`.
- VX855: `4*((N+31)*H+36)+0x480` = `4*(N+31)*H+1296`.
- VX900: `align_up(N,32)*H+0x480` = `align_up(N,32)*H+1152`.

These expressions are derived from the local compiled code and apply to the buffer request, not necessarily the number of command bytes finally flushed.

<a id="c9-section-125"></a>

#### 125. Image-pattern-fill AGP command writer is identical on VX800/VX855; VX900 mainly changes its reservation

`vImagePatFillVX800_AGP` and `vImagePatFillVX855_AGP` are **one exact compiled function** at `0x1001B160`, per the original retail linker map. The body contains **300 instructions**. VX900 uses a 302-instruction `vImagePatFillVX900_AGP` at `0x100285FA`; the normalized comparison finds **296 matching instructions**.

VX800/VX855 reserve command space with `lea eax,[eax*4+0x150]` at `0x1001B1C7`, after adding `0x1F` to the width-plus-offset quantity and multiplying by height. VX900 instead computes a 32-bit-rounded row size using `+7`, `>>3`, `*8`, `+0x18`, and `&~31`, then multiplies by height and adds `0xC0` (`0x10028659–0x1002866E`). Both use the **same `0xCC0A1541` command-state constant** in the compared function, and the other substantial parts of the emitter are nearly instruction-identical.

This is unusually strong evidence that the special **image-pattern command encoding** is shared, while its software command-space accounting changed. An audit should still distinguish command request size from written command size, and patterned fill from *ordinary* screen-to-screen BitBLT.

<a id="c9-section-126"></a>

#### 126. Important exception: VX900 8-bpp and native-color AGP transfers are less alike

The broader audit also disassembled other transfer functions using the *same conservative normalization* (only branch destination addresses are replaced):

| AGP-labelled code pair | Matching instructions | Function lengths | Evidence level |
|---|---:|---:|---|
| VX800→VX855 8-bpp transfer | **253** | 254/254 | Only different `cmSmartRequest` size constant (`+0x160` versus `+0x1C0` after a 4× term) |
| VX800→VX855 native-color transfer | **231** | 232/232 | Only different `cmSmartRequest` size constant (`+0x130` versus `+0x110`) |
| VX855→VX900 8-bpp transfer | **93** | 254/295 | Significant implementation differences; cannot label equivalent from this comparison |
| VX855→VX900 native-color transfer | **190** | 232/256 | Multiple instruction-level changes; further data-flow comparison needed |
| VX800→VX900 patterned 8-bpp transfer | **423** | 466/467 | Substantial common structure, but differing unpack/bit-shift and reservation code |

The `vPat8BppXferVX800_AGP` and `vPat8BppXferVX855_AGP` symbols alias one compiled function; VX900 has its own body. **Do not extrapolate the monochrome finding to all 8-bpp transfers.** The VX900 code changes more than simply its reservation constant, and a matching low GE opcode alone would not prove equivalent source-byte handling, palette expansion, destination depth, or alignment.

For the existing Windows 98 project, this makes **8-bpp input, pitch, alignment, and color conversion** a higher-priority cross-generation test than ordinary monochrome BitBLT. The CE comparison does not demonstrate hardware-assisted 8-bpp-to-16-bpp conversion.

<a id="c9-section-127"></a>

#### 127. Evidence classification, reproducibility and next hardware tests

**Proven in this CE snapshot:** identical VX800/VX855 AGP image-pattern emitter; 299/300 instruction agreement in AGP mono; common `0x000A0141` mono GE command across the three; VX900's 32-bit row alignment in the examined mono and patterned software reservation calculations; reuse of `cmSmartRequest` and `cmSmartFlush` by all three. The underlying source invokes different *software* reserve sizes. The source does not supply a runtime command queue trace or pixel-equivalence results.

**Not proven:** byte-identical final *dynamic* command streams, hardware activity on an external AGP bus (the label follows VIA function names), a performance advantage from VX900's smaller reservations, command-buffer overflow in any path, or correct image output for every bpp/rotation/source-origin case.

**Recommended next controlled tests:** (1) instrument the buffer request and actual write/flush length for an 8×8, 32×32 and 33×9 host monochrome transfer; (2) vary source-left-bit offsets 0,1,7,8,15,31; (3) compare keyed and unkeyed 8-bpp host copies; (4) compare native-color transfers at odd byte pitches, 8/16/32-bpp destinations and all four rotations; (5) on real hardware, use bounded timeouts and CPU pixel-reference comparisons. Keep the tests separate for PCI-labelled and AGP-labelled paths.

**Companion reproducible evidence:** `Chrome9_CE_AGP_2D_Comparison_Audit_122_127.py` reads the original CE driver ZIP, verifies the retail DLL SHA-256, disassembles the linker-map function ranges without running any driver code, compares 11 pairs, verifies the shared mono command, and exports `Chrome9_CE_AGP_2D_Command_Compatibility_122_127.csv` with original function addresses, instruction counts, match counts and first differing instructions. Its checks pass against the supplied retail DLL. The comparison is a **static source-to-binary observation**, not hardware validation.

### Indexed and native host transfer packets

<a id="c9-section-128"></a>

#### 128. VX900 AGP-labelled 8-bpp transfer differences: detailed source-level comparison

**Sources and verification:** supplied VIA CE 6.0 Group2 MSI, *retail* `DDI_VIA.dll` (SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`), `DDI_VIA.map`, compiled function-pointer installations recorded in §112–116, and VIA VX800/VX855/VX900 Part I graphics-engine registers (notably `GE_CMD` offset `0x00`, `GE Mode` offset `0x04`, `Color Key` offset `0x48`, `Source Color Key` offset `0x50`, 2D Host BitBLT Space). This is **static evidence**, not hardware validation. Companion audit: `Chrome9_CE_8bpp_Native_Transfer_Audit_128_133.py` and `Chrome9_CE_8bpp_Native_Transfer_128_133.csv`.

The major code mismatch noted in §126 is **not** a different basic GPU BitBLT opcode. All three AGP-labelled 8-bpp transfer builders use `GE_CMD` low state `0x00002041`, combined with a ROP in bits `[31:24]`. What changes is how VIA reserves command memory, optionally establishes source transparency, and emits the converted host-pixel stream.

| Component | VX800 | VX855 | VX900 |
|---|---|---|---|
| AGP 8-bpp setup | `v8BppXferVX800_AGP` `0x10018463–0x10018795` | `v8BppXferVX855_AGP` `0x100237F7–0x10023B29` | `v8BppXferVX900_AGP` `0x100269B2–0x10026D6D` |
| AGP 8-bpp data writer selected | `v8BppImageTransferCN896_AGP` `0x10021E8A` | **Same function/address** | `v8BppImageTransferVX900_AGP` `0x10028D7B` |
| Common dispatch trampoline | `VIA::v8BppImageTransfer` `0x10012BC6` | Same | Same, but points at VX900 helper |
| GE base command | `0x00002041` | Same | Same |
| Optional source-key writes in the compared AGP setup | Not emitted in this function | Not emitted in this function | **Yes**, guarded by `BltParms+0x24` bit `0x04` |
| Pixel expansion | 8-bit indexed source to output pixel values via CPU palette LUT | Same | Same concept, different transfer packet layout |
| Host-data selector | `FE01` (`0xFE010000`) | Same | Same selector, reissued at row/chunk boundaries |

**Important interpretation:** the older helper's *CN896* name is not evidence that VX800/VX855 are electrically a CN896 or use identical display features. It is evidence of compiled software **function reuse** for this one transfer operation.

<a id="c9-section-129"></a>

#### 129. The 8→16-bit conversion is CPU palette expansion followed by host BitBLT

All three CE AGP transfer paths ultimately read 8-bit indices, look up 32-bit table entries, pack the results, and write them to the `FE01` host-data command buffer. The target color depth selects the packing:

- **32-bpp output:** each source index becomes one 32-bit palette-table value, one DWORD per pixel. In the older CN896 helper this appears at `0x10021EFF–0x10021F08`; the VX900 helper equivalent is at `0x10028DF6–0x10028E05`.
- **16-bpp output:** two palette outputs are combined into a DWORD, with the second in bits `[31:16]` and the first in `[15:0]`. Old helper: `0x10021F49–0x10021F59` (`shl esi,0x10`); VX900: `0x10028E6C–0x10028E83` (`shl ebx,0x10`). This is consistent with little-endian `[pixel0:uint16, pixel1:uint16]` packing. The routine has separate branches for incomplete DWORDs/rows.
- **8-bpp output:** the VX900 helper also has a path that assembles four looked-up 8-bit components into one DWORD. The exact validity of higher bits of lookup-table entries must still be checked for the 8-bpp destination contract.

**This is not a demonstrated hardware palette-to-RGB conversion.** It is a CPU-indexed lookup and CPU packing, followed by **hardware host-data BitBLT**. In particular, the `8→16` support here does **not** contradict the separate VX855 CFC register's documented lack of 8-bit conversion. These are different acceleration approaches.

**Windows 98 implication:** your 8-bit DIB → 16-bit screen path could reuse the format of VIA's LUT-to-`uint16` pairs, then feed the existing 2D host transfer path. That may reduce per-pixel conversion overhead compared with a generic DIBENG path, but its actual benefit depends on destination pitch, host aperture throughput, clipping, and available palette representation. Do not label this a GPU color-conversion engine.

<a id="c9-section-130"></a>

#### 130. VX900 row-framed `FE01` transfer packets versus the older single-stream strategy

In the VX800/VX855 AGP path, the selected `v8BppImageTransferCN896_AGP` helper establishes **one principal `0xFE010000` header** near its beginning (`0x10021EB3–0x10021EEC`), and writes successive pixel data words for the requested rectangle under that stream, with `0xCC000000` padding.

The VX900-specific `v8BppImageTransferVX900_AGP` helper writes `0xFE010000` at multiple locations **inside row-processing branches**, aligns each new packet on a 16-byte boundary (`test cl,0xF` / `0xCC000000` fill), and carries the row/chunk count inside the new header. Examples include `0x10028DD8–0x10028DF2` and `0x10028E4D–0x10028E6A` for 32-/16-bit output branches. Its `FE01` header is still the same host-data transport command; it is **not** a newly invented VX900 2D drawing opcode.

The native-color counterpart shows a related difference: VX800/VX855 install `vImageTransferCN896_AGP` (`0x10021D13–0x10021E8A`), while VX900 installs `vImageTransferVX900_AGP` (`0x100289C2–0x10028D7B`) with substantially more row/fragment framing logic and repeated `FE01` headers. A single instruction count or one request-size constant does not describe this difference adequately.

**Inference, unproven:** additional packet boundaries may help manage host-transfer alignment, queue wraparound, or row data length, but VIA's CE DLL contains no hardware timing measurements. This is only a plausible engineering rationale.

<a id="c9-section-131"></a>

#### 131. VX900 adds conditional color-key control to *both* AGP host-transfer builders

The VX900 8-bpp and native-color AGP builders test `BltParms+0x24`, bit `0x04`, before emitting two extra register writes:

| CE function | Key-flag test | MMIO `0x48` | MMIO `0x50` |
|---|---|---|---|
| `v8BppXferVX900_AGP` | `0x10026B7E`: `test byte [edi+0x24],0x4` | `0x10026B94–0x10026BAF`: read shadow `0x48`, OR `0x00006000` | `0x10026BF4–0x10026C06`: write converted key value |
| `vNativeXferVX900_AGP` | `0x10027150`: `test byte [esi+0x24],0x4` | `0x10027162–0x10027180`: same shadow-OR `0x6000` | `0x10027191–0x100271A7`: write key at `BltParms+0x20` |

Per VX800 and VX900 **Part I** offset `0x48` descriptions: bit 14 = source color-key enable; bit 13 = write if *not equal*. VX855 describes bit 13 as reserved and defines different fixed source/destination-key behavior (VX855 Part I, PDF p.85). This gives a **documented register meaning** for the CE VX900 `0x6000` constant. However, the software is OR'ing a previously shadowed register value, not freshly clearing all its other bits, so an output comparison requires known initial key state.

The **8-bpp** VX900 path does more than copy the raw key: it starts from `BltParms+0x20`, optionally dereferences the palette/lookup table from `BltParms+0x3C`, and can call a supplied conversion callback at `BltParms+0x40` with context from `+0x44` before writing the final key to `0x50` (`0x10026BB3–0x10026BE8`). The **native** path writes `+0x20` directly. This conditional palette/key conversion is a real software difference, not an additional GPU conversion feature.

The *compared* VX800/VX855 AGP transfer functions contain no equivalent `0x48/0x50` color-key setup branch. **This does not prove those chipsets cannot do color-keyed transfers**: other CE paths and the manuals document keying. It only limits what these two particular helper functions do.

<a id="c9-section-132"></a>

#### 132. Command-space reservation formulas: host transfers are not pixel-byte counts

Both old and new AGP setup functions ask `cmSmartRequest` for software command space using `0xFE000000`. Define positive `W=right-left`, `H=bottom-top`, and `B=W*(destination_bits_per_pixel/8)` for supported 8/16/32-bpp destinations; `align(N,A)=(N+A-1)&~(A-1)`.

| Transfer | VX800 capacity requested | VX855 capacity requested | VX900 capacity requested |
|---|---|---|---|
| 8-bpp **source** transfer | `4*W*H+0x160` | `4*W*H+0x1C0` | `(align(B,4)+0x20)*H+0x124` |
| Native-color source transfer | `4*W*H+0x130` | `4*W*H+0x110` | `align(B,16)*H+0xA4` |

The VX900 8-bpp capacity is reconstructed from `0x10026A26–0x10026A46` (destination bytes/pixel from VIA's color-depth table, width shifted appropriately, round to DWORD, add 32 bytes per row, then add `0x124`). The native capacity is from `0x10027015–0x10027036` (destination row bytes, 16-byte alignment, then `0xA4`). The VX800/VX855 fixed-per-pixel capacities come from `lea` instructions in their corresponding functions. The companion CSV gives numeric examples for 8, 16 and 32 bpp.

These values are *capacity requests*, **not actual PCI/AGP transfer sizes**. They incorporate packet headers and safety padding; the exact number of words finally submitted depends on source data, alignment, color-key setup and other branches. The two request formulas should not be used as proof of a throughput win without profiling.

<a id="c9-section-133"></a>

#### 133. Reuse map and isolated regression tests for the Win98 VX855 port

| Component | Reuse classification | Test before enabling |
|---|---|---|
| GE opcode / ROP / rotation bit packing | Shared across these AGP host-transfer paths | Pixel reference for 8,16,32-bpp, with reversed X/Y and clipped rectangles |
| CPU-side palette lookup / 16-bit output packing | Algorithm shared, functions differ | Exhaustive 256-index lookup, odd-width edge, negative pitch and last pixel |
| `FE01` host-data transport | Same command family, different VX900 packet splitting | Compare one-row vs many-row with widths `1,2,3,4,15,16,17,31,32,33` |
| Packet buffer reservation | Different formula | Confirm every write stays inside allocation; verify wraparound and 16-byte boundaries |
| Optional source-key mode | Documented on VX900; not emitted in compared older AGP wrappers | Key equals/does-not-equal, RGB565 vs XRGB8888, palette and callback mapping |
| Source-key `0x6000` on VX855 | **Do not automatically copy:** VX855 documents bit 13 reserved | Use only known VX855 key recipe until tested |
| CPU conversion vs GE CFC | Distinct subsystems | Measure conversion cost and host upload; do not claim hardware CFC supports 8-bpp |

For the existing Win98 project, the immediate high-value experiment is a **source-index-to-16-bit packed-host-blit test** with a CPU reference buffer, logging row pitch, emitted DWORD count, command-space usage, and resulting pixels. Do not change the production driver based solely on this CE code; its queue/payload paths and CE operation contracts are not identical to Win98 APIs.

**Reproducibility:** `Chrome9_CE_8bpp_Native_Transfer_Audit_128_133.py` validates the original retail DLL hash, map-based function ranges, common BitBLT command and FE01 selector, old and new palette emitters, conditional VX900 source-key logic, and command-reservation formulas. `Chrome9_CE_8bpp_Native_Transfer_128_133.csv` contains six numeric examples. Both artifacts are read-only; no DLL is loaded or executed and no GPU registers are written.

<a id="c9-section-134"></a>

#### 134. FE01 host-transfer helper boundary audit

**Scope and primary evidence:** Static disassembly of the SHA-256-verified Group2 CE retail `DDI_VIA.dll` (`529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`), linked against the supplied `DDI_VIA.map`. The selected procedures are `v8BppImageTransferCN896_AGP` (`0x10021E8A`), `v8BppImageTransferVX900_AGP` (`0x10028D7B`), `vImageTransferCN896_AGP` (`0x10021D13`), `vImageTransferVX900_AGP` (`0x100289C2`), `cmSmartRequest` (`0x1004E42E`) and `cmSmartFlush` (`0x1004E51D`). These are **software-side command-buffer writers**; all statements below concern compiled code and mathematically modeled bytes, not measured GPU behavior.

A key argument distinction is necessary: the helper's `+0x10` stack argument is the **number of destination/output bytes per row**, not necessarily the number of 8-bit source pixels. Its `+0x0C` argument is the source **row stride**, `+0x14` the number of rows, and `+0x18` the destination **bytes per pixel** (1, 2 or 4). The 8-bit-index expansion helper additionally accepts the lookup table at `+0x1C`. This interpretation is supported by the `>>2` DWORD count, the two-indices-per-DWORD case for 16-bit output and the one-index-per-DWORD case for 32-bit output.

With output row length `B > 0`, height `H > 0`, define `P = 4*ceil(B/4)` (bytes of padded DWORD payload per row). Only internally consistent `(B,bytes_per_pixel)` combinations are valid. The helper assumes upstream validation of source buffers, row pitches and positive dimensions; the body by itself does not establish a safe API contract for invalid or zero-size requests.

<a id="c9-section-135"></a>

#### 135. Older CN896/VX800/VX855 palette helper uses one FE01 header and requests payload space only

At `0x10021E94–0x10021EBF`, the older 8-bit-index expansion helper calculates `ceil(B/4)*H*4` and passes that value to `cmSmartRequest` with packet type `0xFE010000`. It then aligns the returned command pointer to 16 bytes with `0xCC000000` padding (`0x10021ED1–0x10021EDE`), writes **one 16-byte FE01 header** (`0x10021EDE–0x10021EF4`), streams the converted pixels for all rows, aligns the end to 16 bytes, and passes the actual pointer difference to `cmSmartFlush` (`0x100221A3–0x100221C2`).

For a returned command pointer with one of the DWORD-aligned offsets `0,4,8,12` modulo 16, the modeled **requested** and **eventually emitted** byte counts are:

```text
old_request = P * H
old_emitted = (align_up(initial_offset,16) - initial_offset)
              + align_up(16 + P*H,16)
```

Thus `old_emitted > old_request` for every valid nonempty rectangle in the model: at least the header is additional, and often alignment padding is additional too. For example, `B=33`, `H=2`, initial offset `0`: **72 requested**, **96 emitted**. If the initial offset is `12`, **108 emitted**.

**Classification: confirmed helper-level request/emission discrepancy, not a proven memory overwrite.** The command manager may maintain headroom, return aligned blocks, or rely on higher-level capacity accounting. To establish an actual boundary defect, one must trace the manager's allocation guarantee and compare the buffer's physical end with the final emitted pointer, not merely compare the helper's request argument against the final flush argument. The code nevertheless illustrates a reason not to copy the older reservation formula as a self-contained buffer allocator.

<a id="c9-section-136"></a>

#### 136. VX900 8-bit-index expansion emits separate aligned FE01 packets for each row

The VX900 palette helper computes its payload DWORD count with `sar edi,2` and a conditional increment for `B%4`, then calls `cmSmartRequest` with the equivalent of **`(P+32)*H + 16` bytes** (`0x10028D87–0x10028DB8`). It re-aligns the command pointer before each row's FE01 header; the per-row header is `FE010000`, DWORD-count, zero, `CC000000` (representative addresses `0x10028DD8–0x10028DF4`, `0x10028E4D–0x10028E6A`). Each row's payload comprises `ceil(B/4)` DWORDs and the total emitted buffer is aligned to 16 bytes before the `cmSmartFlush` call (`0x1002926D–0x10029288`).

The corresponding model is:

```text
new_request = (P + 32)*H + 16
new_emitted = initial_align_pad + H*align_up(16 + P,16)
initial_align_pad = align_up(initial_offset,16) - initial_offset
```

In a numerical sweep over **20,480** combinations (`B=1..1024`, `H={1,2,3,9,64}`, pointer offsets `{0,4,8,12}`), this model always fit within the calculated VX900 palette-helper request. That is a **numerical proof for the stated software model**, not validation of runtime allocation or an exhaustive proof over all 32-bit inputs. Example `B=33,H=2`: 152 requested, 128–140 emitted depending on initial alignment.

Both helpers compute 8→16 conversion by palette lookup and packing two 16-bit outputs per DWORD, and 8→32 conversion by emitting one lookup result per DWORD. The primary difference here is **framing and reservation**, not a new GPU color-conversion instruction.

<a id="c9-section-137"></a>

#### 137. VX900 native-color helper has an especially different 16-bpp subpath

The CE native-color helper's behavior is **not adequately described by “one FE01 packet per row” for all pixel formats**. Its initial branch at `0x10028A0F` checks `bytes_per_pixel == 2`:

- **Non-16-bpp branch:** writes an initial FE01 header and streams data in the older continuous style, subject to separate remainder handling (`0x10028A20–0x10028A3E`). Do not infer a separate header for every DWORD here.
- **16-bpp branch:** skips that initial header and, in each full-DWORD path, aligns the command pointer, writes `FE010000`, a DWORD-count of **1**, a zero word and `CC000000`, emits exactly one data DWORD and pads the packet to a 16-byte boundary. Evidence appears repeatedly at `0x10028AC2–0x10028AF1`, `0x10028B9E–0x10028BCF`, `0x10028C67–0x10028C99` and `0x10028D0C–0x10028D3A`. A two-byte tail similarly gets a final one-DWORD packet.

Consequently, for valid 16-bit destination output (`B` even), the helper builds **`ceil(B/4)*H` distinct 32-byte FE01 packets**. Example **33 pixels × 2 rows** at RGB565 gives `B=66`, `H=2`, `17*2=34` FE01 packets and **1,088 command-stream bytes**, plus any initial pointer alignment. The older native helper would use one header and 136 payload bytes (160 aligned bytes from an initially aligned pointer). This is a **software stream-size comparison**, not GPU execution time or actual PCI/AGP bus bandwidth.

For that 16-bpp path the local `cmSmartRequest` formula is `32*ceil(B/4)*H` (`0x100289E7–0x10028A07`), matching the 32 bytes per 4-byte payload DWORD. Its allowance for an initially misaligned pointer is **not independently established**; the command-manager's pointer-alignment guarantees remain to be verified. The `bytes_per_pixel==1` native branch has its own distinct reservation calculation and is **not covered by the 16-bpp model**. This is an important classification boundary.

**Why this matters:** the newer native RGB565 route may pay substantial CPU-side packet construction overhead, or it may reflect a generation-specific transport restriction. The binaries alone do not tell us why VIA chose to fragment this mode. Recombining the packets into a single FE01 command on VX900 is **not yet validated** and must not be made an unconditional optimization.

<a id="c9-section-138"></a>

#### 138. Separate the wrapper's command-space request from the nested helper's request

The earlier §132 examples concern the outer `v8BppXferVX900_AGP` / `vNativeXferVX900_AGP` wrappers. The values here concern **independent `cmSmartRequest` calls inside the data-writing helpers**. They should not be equated or subtracted as though both calls allocate one contiguous region. For example, the wrapper's native-color request for a 33×2, 16-bpp rectangle was 324 bytes in §132, while the native helper's corresponding modeled FE01 stream is 1,088 bytes. The call-stack and allocation lifetimes must be traced before claiming either is a buffer overflow.

A correct command-stream logger should capture at least: wrapper request size, helper request size, returned command pointer modulo 16, each FE01 header's DWORD count, number of headers, final flush byte count, command manager's actual allocation/free-space accounting, row pitch, destination bytes per pixel, and whether row data is padded or fragmented. Make the values available separately per draw call and per command allocation.

The `cmSmartRequest` implementation (`0x1004E42E–0x1004E51C`) checks free bytes against a computed requested amount and conditionally flushes/requests another block; `cmSmartFlush` (`0x1004E51D–0x1004E594`) uses the actual passed length. Those facts strengthen the case for auditing the request/flush relationship, but do not establish the external allocator's physical bounds or rule out reserved slack elsewhere.

<a id="c9-section-139"></a>

#### 139. Reproduction and cross-generation regression recommendations

**Read-only tools:** `Chrome9_CE_FE01_Row_Packet_Audit_134_139.py` opens the selected original driver ZIP, verifies the retail-DLL SHA-256, uses the retail map and `objdump` to confirm exact instruction sites, and validates the mathematical packet-size model over 20,480 positive-size cases without executing CE code. `Chrome9_CE_FE01_Row_Packet_Bounds_134_139.csv` provides explicit request/emission examples. These output sizes are modeled from documented instruction paths—not captured hardware transactions.

**Recommended bounded hardware tests, in priority order:**

1. On VX855, retain its existing host-transfer behavior while measuring *actual* requested and flushed command lengths. Verify that `FE01` count and physical buffer extent match at row lengths `1,2,3,4,15,16,17,31,32,33,34,66` and heights `1,2,9`; test output bytes/pixel `1,2,4` only with valid byte lengths.
2. Test a 33-pixel RGB565 row (`66` output bytes) and compare the older one-header path against a controlled, isolated packet-per-DWORD mode on suitable hardware. Check output, command count and time; do **not** assume they are interchangeable.
3. Separate source row pitch from output row byte count. Exercise padded and negative source strides only when the caller contract supports them; validate source buffer bounds before the helper executes.
4. Capture `cmSmartRequest` / `cmSmartFlush` end-to-end, including initial pointer alignment and ring-wrap behavior. If requests exceed the manager's guaranteed contiguous block size, split the workload before constructing packets.
5. For the outstanding Win98 indexed DIB→RGB565 path, CPU-expand indexes with a 256-entry lookup table, pack output by destination byte count and use a bounded host-BitBLT transport. Reuse the proven common GE command encoding, but not any unvalidated VX900-specific fragmentation rule.

**Conclusion:** The 8-bit palette conversion logic is a strong common-core candidate, but **FE01 packet construction is generation- and pixel-format-dependent in VIA's own CE code**. The older CN896 path has a verified request-vs-emission accounting difference, while the VX900 palette path includes explicit padding allowance. The VX900 native 16-bpp route is much more heavily fragmented. The next necessary evidence is the command manager's guaranteed extent and a hardware pixel/output benchmark—not another inference from raw opcode similarity.

<a id="geometry-3d"></a>

## 3D backends, vertex input and primitives

VX800 and VX855 use the same H6 initialization and blit path in the inspected CE builds. VX900 H5S2 changes Type-04 vertex declarations and uses Type-03 subtype `31h` program uploads. The CE and XP binaries establish the four-DWORD record format. The XP generator also separates the program-count and record-scan fields in Type-04 setup. [Vertex-program encoding](#c9-section-372).

### Direct vertices, indexed buffers and primitives

<a id="c9-section-371"></a>

#### 371. VX900 XP Direct3D callbacks and indexed draws

The VIA XP x86 package version `6.14.10.0436` binds `1106:7122` to the display driver `S3gIGPA.dll` and miniport `S3gIGPmA.sys`. In the display DLL, the PCI ID branch at `0x237CC` chooses filename table entry 5 (`0xB266C`, `s3ginvA.dll`). At `0x23980` it loads that image, then resolves and calls `cilSetCoreFunctionTables` at `0x23ACC–0x23AF1`. The CIL export at `0x805E0` copies the backend callback tables into the display DLL. [XP binary identities](#c9-section-375).

The `GUID_D3DCallbacks3` handler at display VA `0x2C64B` returns the 24-byte table at `0xB32AC`. It contains flags `0x0C`, a zero `Clear2` slot, `ValidateTextureStageState=0x2AF10`, and `DrawPrimitives2=0x2AF50`. The latter calls the parser at `0x38CB0`. The parser uses the DP2 opcode minus one to index the jump table at `0x3C680`. DP2 opcodes describe the runtime-to-driver interface. See [Microsoft's callback definition](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3dhal_callbacks3) and [SDK opcode definitions](https://github.com/tpn/winsdk-10/blob/master/Include/10.0.14393.0/um/d3dhal.h).

| DP2 opcode (decimal) | Display handler VA | Verified path |
|---|---|---|
| 42 `CLEAR` | `0x3A241` | Parses clear data and rectangles, then calls the display clear helper. [Clear route](#c9-section-374). |
| 45 / 47 `CREATEVERTEXSHADER` / `SETVERTEXSHADER` | `0x3A389` / `0x3A4C6` | Distinct parser entries for the shader object and active shader. [Hardware upload](#c9-section-372). |
| 49 / 50 `SETSTREAMSOURCE` / `SETSTREAMSOURCEUM` | `0x3A897` / `0x3A90B` | Separate vertex-buffer and user-memory entries. |
| 51 `SETINDICES` | `0x3A97E` | Helper at `0x49340` records the index buffer and stride in context fields `+0x481C` and `+0x4820`, with the handle at `+0x4830`. |
| 52 / 53 `DRAWPRIMITIVE` / `DRAWINDEXEDPRIMITIVE` | `0x3A9E4` / `0x3AB04` | Separate draw handlers. |
| 59 / 60 `DRAWPRIMITIVE2` / `DRAWINDEXEDPRIMITIVE2` | `0x3B67C` / `0x3B7A4` | Opcode 60 calls display slot `0xC2FB8` at `0x3B87B`. This resolves to CIL `0x90A00`, which invokes a per-context backend at `0x90A30` through table offset `+0x44`. |
| 85 / 86 `SETRENDERTARGET2` / `SETDEPTHSTENCIL` | `0x3C2CF` / `0x3C30B` | Separate target and depth/stencil binding handlers. |

The indexed path is traced through the CIL dispatch. Hardware index packing and the conditions selecting direct GPU index fetch versus CPU expansion remain open.

<a id="c9-section-376"></a>

#### 376. VX800 XP package and Direct3D dispatch

The `dc05emb.inf` package version `6.14.10.0274` lists `1106:1122` (Chrome9 HC3) as its hardware ID in both model sections. On XP x86 the `S3G700` entry installs `S3gIGP.dll`, `S3gIGPm.sys`, `s3ginv.dll`, and the separate OpenGL ICD `s3gIGPgl.dll`. [Binary identities](#c9-section-381).

The display DLL's normal filename selection branches for `1122h` and `5122h` converge at `0x22A90` on table index `0xD`. Entry `0xAB39C` is `s3ginv.dll`. The display DLL loads it at `0x22BCA`, resolves `cilSetCoreFunctionTables` at `0x22D17` and calls it at `0x22D3C`. The CIL export at `0x7ACA0` copies ten function tables. In the CIL adapter classification at `0x8A2DA–0x8A346`, VX800 and VX855 both get family code `0xD` and class index 4. VX800 additionally sets the adapter field at `+0x24` to 1. The VX900 branch at `0x8A338–0x8A354` gets family code 5 and also uses class index 4. The INF establishes installation scope. These compiled branches show where code is shared and where a later family-specific check can still alter it.

The `GUID_D3DCallbacks3` query at display VA `0x2BDF1` returns the 24-byte table at `0xAB7BC`. Flags are `0x0C`, the `Clear2` slot is zero, `ValidateTextureStageState` is `0x2A690`, and `DrawPrimitives2` is `0x2A6D0`. The parser at `0x37BF0` indexes `0x3B714` by DP2 opcode minus one. [Callback structure](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3dhal_callbacks3).

| DP2 opcode (decimal) | VX800 XP display VA | Trace |
|---|---|---|
| 42 `CLEAR` | `0x391F1` | Calls `0x35250`, which submits clear work through the CIL. [Depth and stencil](#c9-section-379). |
| 45 / 47 `CREATEVERTEXSHADER` / `SETVERTEXSHADER` | `0x3933C` / `0x39476` | Parser entries. [Vertex-program gate](#c9-section-378). |
| 49 / 50 `SETSTREAMSOURCE` / `SETSTREAMSOURCEUM` | `0x39847` / `0x398BB` | Separate buffer and user-memory stream handlers. |
| 51 `SETINDICES` | `0x3992E` | Helper `0x480B0` records the stride, buffer object, and handle at `+0x4820`, `+0x481C`, and `+0x4830` in the normal draw context. |
| 52 / 53 `DRAWPRIMITIVE` / `DRAWINDEXEDPRIMITIVE` | `0x39994` / `0x39AB4` | Separate draw handlers. |
| 59 / 60 `DRAWPRIMITIVE2` / `DRAWINDEXEDPRIMITIVE2` | `0x3A62C` / `0x3A754` | Opcode 60 calls slot `0xB71B8` at `0x3A82F`. The exported CIL table maps this slot to `0x8AB00`. [Indexed packet](#c9-section-377). |
| 85 / 86 `SETRENDERTARGET2` / `SETDEPTHSTENCIL` | `0x3B28F` / `0x3B2CB` | Separate entries, both using the binding helper at `0x34ED0` with different arguments. |

<a id="c9-section-377"></a>

#### 377. VX800 XP indexed triangle-list command packet

CIL `0x8AB00` dispatches through the context's primitive table at `+0x44`. The class-4, family-`0xD` selector at `0x8A700` resolves the table to `0x1FA430`, so this slot is `0x115B50`. Primitive type 4 (triangle list) then uses table offset `+0x20` and reaches `0x11DE80`. When the stream check and the 2- or 4-byte context field checks at `0x11DF26–0x11DF3F` pass, it calls the indexed packet emitter at `0x118C80`. [Microsoft's indexed-draw DP2 structure](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3dhal_dp2drawindexedprimitive2).

For a batch of `P` triangles with index stride `S`, that emitter sets index mode in its internal state, prepares the draw state, and uses `I = 3*P` indices and `D = ceil(I*S/4)` payload DWORDs. The buffer-lookup helper at `0x118050` handles a user-pointer variant and a bound-buffer variant. The emitter copies `I*S` bytes of source indices into the command buffer at `0x118DE8–0x118DF3`, advances by the rounded DWORD count, and submits at `0x118E2B`. The helper at `0x115FA0` emits the following framing.

```text
FE020440  0000043C  00040000  CC000000
24000000 | I        [CC000000 alignment]
FE03043C  D         00000000  CC000000
[D DWORDs of index payload]
```

The index data is inline under Type-00. The 3D engine uses those indices to address vertices in this path. A separate branch at `0x11DF48` enters `0x118560`, which reads 16- or 32-bit indices on the CPU and uses index-derived vertex addresses. This proves two distinct paths in the VX800 XP build. Their usage frequency and the selected paths on VX855 or VX900 need independent traces. V8-II, pp. 69, 139.

<a id="c9-section-7"></a>

#### 7. Vertex input: the decisive VX900 rewrite

##### 7.1 VX800/VX855 FVF organization

VX800 and VX855 describe a legacy flexible-vertex-format mapping where `HParaType=04h`, sub-addresses `01h–10h`, select the order of named vertex attributes such as `X`, `Y`, `Z`, `W`, texture coordinates and back-face colors. `04h:00h` contains vertex format length and mode controls. [V8-II, pp. 135–138]; [V855-II, pp. 154–159].

The VX800/VX855 resemblance supports the inference that much of the existing VX855 vertex packing and submission code can be adapted to VX800, but **exact mode bits, vertex memory locations, and state setup must still be compared and tested**.

##### 7.2 VX900 replaces attribute-number words with stream-to-register mapping

The VX900 gives `04h:00h` a different low-bit interpretation:

- `bits 11:8`: count/index of used vertex-buffer data (`n` denotes `n + 1` entries).
- `bits 6:0`: summed vertex data length in dwords.
- `bits 16:15`: vertex-buffer memory region selection.
- `bits 23:18`: X/Y/Z/W NaN-related test modes and X/Y swap field, expressed relative to vertex-shader `oPos`.

For each datum at `04h:01h–10h`, the VX900 words specify:

| Bits | Meaning |
|---:|---|
| `23:20` | Vertex buffer / stream identifier |
| `19:12` | Attribute offset in 4-byte units |
| `11:8` | Vertex-shader input register (`IR0`–`IRf`) |
| `7:0` | Datum format / dimension encoding |

The format dimension field `bits 7:6` specifies 1D, 2D, 3D or 4D input fill, as described by VIA. `04h:12h` adds a mask identifying which of the 16 input registers are supplied from VB data rather than using defaults. [V9-II, pp. 160–165].

At `HParaType=03h`, subtype `30h`, VX900 describes up to **16 vertex-stream base/pitch entries**. The address field is in **1024-byte units** and pitch is in **4-byte units**. [V9-II, p. 159].

**Consequences:**

1. VX855 `04h:10h` = attribute-number / back-face-color control; VX900 `04h:10h` = 16th vertex input mapping word. They are **not compatible**.
2. The VX855 FVF generator cannot simply be used with a VX900 PCI-ID patch.
3. VX900 vertex memory alignment, base addresses, register mappings and input defaults need a distinct setup path.
4. A basic triangle may be possible using the shared primitive interface, but it still requires correct VX900 vertex setup and any requisite shader/output state.

##### 7.3 Additional primitive and clipping changes

The VX900 `04h:21h–22h` entries are marked **reserved**, whereas the VX855 documents vertex-buffer setup at these addresses (including its base-address low bits at `04h:21h`). At VX900 `04h:23h`, bits include controls for clipping against **W = 0** and a clipping-engine ECO condition. VX855 `04h:23h` has overlapping shading/edge/line controls without the identical high-bit descriptions. [V855-II, pp. 159–161]; [V9-II, pp. 166–167].

**Bug-prevention rule:** Do not transplant entire `04h:20h–2Fh` initializer arrays between these platforms; even where individual bits match, others are new, removed or reserved. Use named field encoders with family-specific valid-bit masks.

##### 7.4 Common clipping transform is promising, not a complete port

Both VX855 and VX900 describe `04h:40h–52h` for clipping-window to screen-window scaling/offsets. The VX900 manual says the corresponding factors must be programmed **both** to clipping-engine settings and vertex-shader constant registers `c256/c257` for VS use. [V855-II, p. 170]; [V9-II, pp. 177–179].

**Inference:** The data model for viewport/scissor/clip transform can be shared, but the VX900 state uploader may need additional VS-constant programming. The manual does not supply enough complete shader loading information to make this a turnkey hardware-T&L path.

<a id="c9-section-20"></a>

#### 20. Vertex and primitive compatibility: narrow places that can remain shared

The common architectural names alone can be misleading. VX855 uses the legacy `04h:01h–10h` attribute-number list and `04h:21h–22h` vertex-buffer base/pitch definitions. VX900 uses those first words for explicit VB-index/offset/input-register/format descriptors and marks the old `04h:21h–22h` region reserved. [V855-II, pp. 155–160]; [V9-II, pp. 160–166].

There is still significant **logical** reuse around the primitive and shading controls. VX855 and VX900 both document `04h:23h` as the primitive-setting word with:

- A primitive-type field in **bits `3:0`** (including point, line, triangle, rectangle, wireframe and triangle-points configurations).
- Front/back primitive-mode selection and culling controls.
- Shading mode around **bits `14:12`** (solid, per-vertex flat, Gouraud).
- An edge-flag field around **bits `11:9`** for wireframe and antialiasing behavior.

**Sources:** [V855-II, p. 161]; [V9-II, p. 167]. This overlap suggests a shared abstract `PrimitiveDesc`, not an identical full-width `04h:23h` value. Reserved or new high bits must be masked per family.

Recommended abstraction:

```c
/* Conceptual API, deliberately without undocumented magic values. */
typedef struct {
    unsigned primitive_kind;   /* point / line / tri / rect */
    unsigned shade_mode;       /* solid / flat / gouraud */
    unsigned edge_mask;        /* three triangle edge bits */
    unsigned front_face_mode;
    unsigned vertex_count;
} ChromePrimitiveDesc;

/* Each family encoder validates only its documented set of bits. */
unsigned encode_vx855_primitive(const ChromePrimitiveDesc *p);
unsigned encode_vx900_primitive(const ChromePrimitiveDesc *p);
```

For **flat-vs-Gouraud performance** specifically, run the identical vertex list with identical `04h:23h` primitive mode, varying only the shade selector and independently confirming the color source and hardware pixel output. Avoid concluding that flat shading is inherently faster: a change in driver fallback, shader mode, state emission or depth rejection may dominate the measured frame time.

<a id="c9-section-28"></a>

#### 28. Narrow shared 3D functionality, but no full OpenGL acceleration

##### 28.1 Direct evidence for a compatible composition subset

OpenChrome's EXA composite path calls `viaInit3DState()` and emits:

- Render-target base, format and pitch;
- Blend/ROP and plane-mask controls;
- Texture state for one or two textures;
- Clip rectangle;
- A quadrilateral rendered as **two 3-vertex triangles**, using Header-2 / `HParaType=00h` vertex data.

[OpenChrome `src/via_exa.c:L943–L975`; `src/via_exa_h6.c:L351–L490`; `src/via_3d.c:L275–L385`, `L389–L547`]. The immediate quad emitter includes floating-point X, Y and W values and texture coordinates. It uses `0xEC`/`0xEE` sub-address control words in this particular path.

This is useful for the VX855 driver: the **fixed-layout, pretransformed-textured-quad** subset has a concrete second implementation. It might help with DirectDraw blit/composite experiments, browser/desktop alpha operations and your 2D/3D window-copy tests.

It **does not demonstrate** that the full VX855 legacy FVF programming transfers to VX900. OpenChrome's narrow quad path is compatible with leaving the more complicated game vertex-input path unresolved.

##### 28.2 Software DRI client on these chips

The crucial distinction is explicit in `src/via_dri.c`:

```c
case VIA_VX800:
case VIA_VX855:
case VIA_VX900:
    pDRIInfo->clientDriverName = SWRastClientDriverName;
    break;
```

`SWRastClientDriverName` is initialized as `"swrast"` in the same file [OpenChrome `src/via_dri.c:L70–L72`, `L529–L541`]. OpenChrome's XRender-accelerated compositing and its software GLX/DRI client can coexist. **Do not interpret a working OpenChrome EXA compositor as evidence of hardware-accelerated Quake, DirectX 7, programmable vertex shaders, or hardware T&L.**

For your project, this makes the **VX855 Windows 98 driver** the main reference for game-style 3D and OpenChrome the main independent comparison source for PCI, VGA, 2D, display, command submission, overlays, and limited 3D compositing.

##### 28.3 Performance choices visible in the compositing path

The shared H6 composite backend refuses small source/mask images below `VIA_MIN_COMPOSITE` (set to **400 pixels**) for some non-repeating images, returning to software rather than paying GPU setup cost. It also folds certain **1×1 repeating masks** into a constant blend factor and certain **1×1 repeating source images** into a solid color instead of sampling textures. [OpenChrome `src/via_driver.h:L92`; `src/via_exa_h6.c:L268–L297`, `L379–L410`].

**Potential VXGL/DirectDraw performance lesson:** measure total *setup + state writes + draw + synchronization* cost, not just rasterizer throughput. GPU acceleration for tiny rectangles or draw groups can be slower than a CPU path, while batching larger work and eliminating unnecessary one-pixel textures may save considerable overhead. The 400-pixel threshold is **OpenChrome's policy**, not a validated optimum for the VX855 Windows 98 driver, Quake 2 or Lattice.

##### 28.4 An unanswered hardware question to test, not assume

OpenChrome initializes a wide legacy 3D state range for all chipsets in a common `viaInitialize3DEngine()` path [OpenChrome `src/via_ums.c:L469–L529`], even though the VX900 manual marks some earlier shader-related register descriptions reserved. This may indicate backward-compatible write handling, a common conservative reset, or merely source code that has not exercised every hardware variant. It is **not evidence that reserved VX900 registers have legacy shader semantics**.

<a id="c9-section-35"></a>

#### 35. Refined VX900 3D compatibility: distinguish direct vertex commands from indexed vertex buffers

##### 35.1 What the manual actually says

The VX855 and VX900 manuals **both** describe `HParaType=00h` as carrying **primitive vertex data in Vertex Command Mode** or **vertex indices in Vertex Buffer Index Mode**. VX900 Part II, printed p. 89, states explicitly that the selection is made by `HVertexMode`, with the regulator assembling/firing primitives from the configured vertex attributes. VX855 Part II contains the corresponding `HParaType=00h` description.

The incompatible part is **not necessarily every use of `HParaType=00h`**. The particularly strong incompatibility is the *meaning of the vertex-format/state words* under `HParaType=04h`:

| Item | VX855 HCM | VX900 HD | Engineering interpretation |
|---|---|---|---|
| `04h:00h` low fields | FVF vertex length in 32-bit units, bits `[5:0]` | Sum of vertex buffer input-data sizes `[6:0]`; number of required input registers `[11:8]` | **Different meanings; never transplant the old complete word** |
| `04h:01h` | Packed FVF attribute-number selectors | Vertex-buffer ID, data offset, input register, component format | **Different encoding** |
| `04h:10h` | Additional FVF attribute numbers and two back-face-color mask bits | Input-data-15 description (VB ID/offset/input register/format) | **Direct register collision** |
| `03h:30h` | Not equivalent to VX900's multistream descriptor | 16 vertex-stream base/pitch entries | VX900-specific indexed-buffer machinery |
| `00h` command/vertex stream | Present | Present | **Transport concept shared, precise initialization and payload interpretation still require tests** |

Sources: **VX855 Part II** printed pp. 148–153; **VX900 Part II** printed pp. 89 and 153–159. OpenChrome `via_3d.c:L279–L385`, `via_exa.c:L903–L952`.

##### 35.2 What OpenChrome adds

`via3DEmitQuad()` has **no VX800/VX855/VX900-specific branch**. It uses a `BEGIN_H2(HC_ParaType_CmdVdata, ...)` packet, writes a vertex-attribute control word to subaddress `0xEC`, issues a vertex-list control word at `0xEE`, sends six vertices, and issues end/fire words. The same function is installed for the EXA composite path on all three chips.

This suggests a practical investigation: the **old direct, screen-space textured-quad path might be portable without adopting VX900's indexed multistream vertex format**. That is more encouraging than treating the whole VX900 3D submission path as completely new.

However, the source does *not* demonstrate correct real-hardware output on VX900, nor does it prove that the same `HParaType=04h` setup is valid for general OpenGL/Direct3D vertices. The narrowest test is: (1) untextured screen-space triangle, (2) textured rectangle, (3) depth-enabled triangle, (4) indexed vertex-buffer triangle with a VX900-specific descriptor. Keep the command-mode and indexed-buffer paths separately selectable.

**Revised rule:** preserve and test the VX855 **logical primitive and direct-vertex algorithms**, but family-gate **vertex-state encoders**. Do not declare a full game-3D reimplementation mandatory before the simplest direct-mode tests.

<a id="c9-section-41"></a>

#### 41. The compositor provides a useful *restricted* acceleration recipe

The three chip families share the same OpenChrome `via_3d.c` compositing implementation, which makes it particularly useful for **2D-through-3D** operations in the VX855 Win98 driver.

##### 41.1 Actual path implemented

- `via3DEmitQuad()` emits **two independent triangles**, six complete vertices, for each rectangular composite (`via_3d.c:L279–L385`).
- Each vertex contains `X`, `Y`, and a constant **`W=0.05f`**, followed by up to two texture `(S,T)` pairs. The source comment specifically says a W coordinate is required for correct texture mapping on this path; it does not establish why `0.05` is the only valid value.
- Source and mask textures can be supplied separately; the state code selects source/mask blend equations and up to two texture stages (`via_3d.c:L170–L231`, `L388–L531`).
- Supported compositing formats include RGB555, RGB565, ARGB4444, ARGB1555, several RGB/BGR 32-bit formats, and **A8/A4/A1 as textures only** (`via_3d.c:L75–L90`). A8 is *not* a listed render-target format.
- The compositing operators are limited to the explicit table in `via_3d.c:L53–L73`; unsupported formats and operators are rejected in `via_exa_h6.c:L268–L347`.

##### 41.2 Relevance to current VX855 problems

**Potential future 16/32-bpp alpha-blended text and cursor work:** the documented A8 texture path plus RGB render target may offer a reference recipe for compositing coverage masks onto color targets. This is **not** equivalent to a full arbitrary DIB/GDI acceleration path, and it should be benchmarked against DIBENG and a CPU blend.

**Quake 2 / Lattice / other OpenGL speed:** emitting a repeated rectangle as two full triangles rather than one strip is an obvious overhead cost in this limited OpenChrome path, but game rendering has different state and vertex requirements. The result reinforces the value of batching *compatible* quads/strips instead of blindly sending one triangle at a time. It is not proof that the current VXGL flat-strip optimization fixes every workload.

**Texture coordinate/W diagnostic:** test a simple 2D textured quad with constant nonzero W values under controlled perspective and affine modes. If behavior changes unexpectedly, log whether vertex W, RHW, and texture projection are being interpreted consistently. Do **not** generalize `W=0.05f` to arbitrary 3D primitives.

##### 41.3 One-pixel texture optimizations

OpenChrome detects repeated **1×1 source or mask pictures** and turns some into a constant-color or constant-alpha path (`via_exa_h6.c:L379–L417`). For the Win98 driver this suggests a bounded optimization: treat solid-color alpha rectangles and repeated single-color glyph masks as a separate fast path instead of allocating/reuploading a texture for every operation.

Test by batching a repeated rectangle with and without texture setup, including the upload cost, cache effects and synchronization cost. Preserve exact blend semantics; avoid changing the hardware path for two-sided lighting, alpha-test-specific content, depth, or unfilled polygons without dedicated validation.

### H6 and H5S2 dispatch and matched blit sequences

<a id="c9-section-46"></a>

#### 46. Windows Embedded CE 6.0 VIA driver: confirmed family backend dispatch

**Primary evidence:** uploaded `VIA Graphics Driver Group2.msi`, retail `DDI_VIA.dll`, PDB/map. Static analysis only; no DLL execution or GPU test. The original CE package is marked for VX800, VX855, and VX900.

##### 46.1 Corrected from the earlier symbol-only interpretation

`C3DUtils::Init` (`0x100636BA–0x10063751`) dispatches on `g_SystemInfo`'s PCI ID and chooses:

| Chipset | ID | Initialization | Textured/scaling 3D blit | Texture helper |
|---|---|---|---|---|
| VX800 | `1106:1122` | `Init3DHWRegs_H6` | `fn3DBlt_H6` | `Texture3D_H5` |
| VX855 | `1106:5122` | **same H6 code** | **same H6 code** | **same H6 code** |
| VX900 | `1106:7122` | `Init3DHWRegs_H5S2` | `fn3DBlt_H5S2` | `Texture3D_H5S2` |

This demonstrates source-generation sharing at runtime for CE 3D scaling/blits on VX800/VX855, and a distinct VX900 3D blit backend. It does **not** establish interchangeable complete Direct3D pipelines.

##### 46.2 Shared 3D setup and generation-specific words

Both H6 and H5S2 call `Init3DHWRegs_INV` (`0x100630F7`) first. It writes reset data through MMIO `0x43C/0x440` for Type-01 global state, Type-02 texture stages and subtype FE, Type-03 palette tables, and Type-04 vertex state. Then the H6 initializer sends encoded `0x00:0x0E1804` versus H5S2's `0x00:0x0E1004` (difference `0x800`, bit 11). H6 also writes zero to addresses `0x14–0x19`, which H5S2 omits. The rest of the short initialized parameter list mostly coincides; exact values are preserved in the companion CE findings.

The common zero-initialization of some VX900 registers marked 'reserved' in the public manual should not be interpreted as support for the VX855 legacy shader interface.

##### 46.3 The VX900_WAITPATCH likely consists of explicit hardware-idle waits

`VX900_WAITPATCH` (`0x10066588`) appends eight copies of:

```text
FE000000 00000001 00000000 CC000000
0000006C 00800000 CC000000 CC000000
```

The VX900-selected H5S2 3D blit calls it at `0x10066438`, then appends a final packet including `0x0000006C` and `0x00FC0000`. Under the VX800/VX855 published `MMIO+0x6C` register definition, the values mean:

- `0x00800000`: wait 3D idle (bit 23).
- `0x00C00000`: wait 3D and 2D idle (bits 23:22). This exact pair also appears in the CE driver's shared command-wait helper, either as direct MMIO or an encoded packet.
- `0x00FC0000`: wait 3D, 2D and all four DMA channels idle (bits 23–18).

**Caution:** the VX900 manual marks the corresponding `0x60–0x9F` range reserved, so this is a plausible **packet-meaning reconstruction** from VIA code plus older manuals, *not silicon-verified VX900 behavior*. The patch should not be copied verbatim into VX855 code.

##### 46.4 Where the two CE 3D blit command writers actually differ

A restricted scan of literal words emitted to command buffers finds a 38-word common opening run, then different state words and H5S2-only branches. H6 emits `0x01020100` and `0x02090803` where the corresponding H5S2 locations emit `0x010000C2` and `0x02000142`. H5S2 adds alternate small packet groups (`0x60800001/0x61800000/0x62104001` or `0x60800002/0x61800000/0x62204002`) and references the `bGdiMultiABlend` flag. This is a comparison of **static immediate writes**, not a complete dynamic stream trace.

**Next analysis targets:** read full constant tables copied into H5S2 packets; resolve actual command-header meanings; trace buffer allocation/capacity when adding eight waits; compare the CE initialization against your Win98 VX855 3D init; and, only with hardware logs, evaluate whether the wait strategy changes behavior.

The full source addresses, command words, previous CE findings, and precise manual-source caveats are retained in `VIA_CE_Chrome9_Additional_Reverse_Engineering_Findings.md`.


##### AGP kickoff direct-register-write comparison caveat

The same CE package also contains separate `KickOffVX800_AGP`, `KickOffVX855_AGP`, and `KickOffVX900_AGP` functions (retail VAs `0x100321A1`, `0x10037C7C`, and `0x10039E11`). They construct buffered AGP-style command packets rather than using the simple direct MMIO access pattern extracted for the `*_PCI` routines. **Therefore, the PCI offset-difference CSV must not be presented as an AGP command-difference matrix.** A full AGP comparison requires packet-by-packet data-flow reconstruction including computed words and copied constant tables.

<a id="c9-section-86"></a>

#### 86. Complete CE chipset-to-3D-backend dispatch recovered

**Evidence:** original VIA Windows CE 6.0 **retail** `DDI_VIA.dll` (SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`), `C3DUtils::Init` at `0x100636BA–0x10063751`; retail linker map `VIA_3DUTILITY:3DUtility.obj`. This is an examination of the CE binary's **software dispatch**, not a hardware feature probe.

The initializer reads the PCI device ID from the attached VIA object (`[object+0x4C]`) and uses a sequential subtract/branch chain. Reconstructing the exact cumulative values gives **six recognized IDs** rather than only the three headline models:

| Device ID | Dispatch branch | 3D register initializer | `C3DUtils+04h` texture callback | `C3DUtils+08h` 3D-blit callback | Wait callback at `C3DUtils+00h` |
|---|---|---|---|---|---|
| `1122h` (VX800) | `0x10063735` | `Init3DHWRegs_H6` | `Texture3D_H5` | `fn3DBlt_H6` | `WaitEngIdle_CR` |
| `5122h` (VX855) | `0x10063735` | `Init3DHWRegs_H6` | `Texture3D_H5` | `fn3DBlt_H6` | `WaitEngIdle_CR` |
| `7122h` (VX900) | `0x100636F7` | `Init3DHWRegs_H5S2` | `Texture3D_H5S2` | `fn3DBlt_H5S2` | `WaitEngIdle_CR` |
| `3371h` | `0x1006370D` | `Init3DHWRegs_H5` | `Texture3D_H5` | **cleared to NULL** | `WaitEngIdle` |
| `3157h`, `3344h` | `0x10063726` | `Init3DHWRegs_H2` | `Texture3D_H2` | **cleared to NULL** | `WaitEngIdle` |

The other three IDs are included to describe the **binary's dispatch**, not to assert that they are VX800/VX855/VX900 variants. In particular, the absence of the CE `fn3DBlt` callback in H2/H5 does *not* prove those chips lack hardware 3D. There is an unmatched-ID early exit; its higher-level error handling is outside the current scope.

**Exact confirming instructions:**

- `0x100636CF/0x100636D6/0x100636DD/0x100636E4/0x100636E9/0x100636F0`: subtract-chain immediates `1122h`, `2035h`, `01EDh`, `2Dh`, `1DB1h`, `2000h`.
- `0x100636F8`, `0x1006370E`, `0x10063727`, `0x10063736`: direct calls to H5S2, H5, H2, H6 initializers respectively.
- `0x100636FD–0x1006370B` versus `0x1006373B–0x10063749`: stores both chipset-specific texture and 3D-blit function pointers.
- `0x10063749`: installs the **same** `WaitEngIdle_CR` callback for H6 and H5S2; older H2/H5 paths install `WaitEngIdle` at `0x1006371E`.

**Driver reuse implication:** select by verified hardware ID, preserve a common high-level blit interface, and permit separate H6/H5S2 command builders. The existing shared wait callback supports a common *CE wait entry point*, but not necessarily identical timeouts or all internal queue behavior across silicon.

<a id="c9-section-89"></a>

#### 89. Consolidated shared-versus-specific 3D adapter contract

The following is a **software design recommendation**, supported by the recovered CE implementation, not a claim that an existing Win98 binary already exposes these interfaces:

| Layer | Common across VX800/VX855/VX900 in examined CE blits | Generation-specific or still unresolved |
|---|---|---|
| Device-selection API | Common `C3DUtils`-style interface | PCI-ID to H6/H5S2 dispatch |
| 3D wait entry point | Same `WaitEngIdle_CR` callback for H6/H5S2 | Silicon-specific timeout and queue behavior untested |
| Render-target/texture register formulas | Common for examined blits | Additional initialization/patch packets on VX900 |
| Primitive command words | Common `04h:20h/23h/24h` and selected FVF masks | Different `04h:01h–03h` declarations |
| Pixel-shader ALU/TAU programs | Same programs in compared H6/H5S2 blit variants | Some mode selection depends on CE retained blend state |
| Blend factors and constants | Same computations | Must make per-draw mode explicit; zero-alpha corner case |
| Vertex input | Common 6/8-DWORD *payload concepts* in these operations | VX900 `03h:31h` VS uploads and undocumented `04h:60h–62h` controls |
| Command-buffer transport | Common high-level flush / packet-interpreter choice | No hardware-validated guarantee of identical command queue semantics |

**Important distinction:** equal command words in these CE 3D blits do not establish equal *all-mode* shader capability or equal performance. Keep the VX900 wait patch and undocumented vertex controls behind chipset-specific interfaces and use original driver binaries only as reference data.

<a id="c9-section-271"></a>

#### 271. Matched H6 and H5S2 3D blit sequences

**Objective:** classify commands shared by **VX800 (Chrome9 HC3)**, **VX855 (Chrome9 HCM)** and **VX900 (Chrome9 HD)** using a comparable 3D textured/blended blit, rather than investigating Win98/VXD3DMON performance. This section explicitly continues the original three-family command-compatibility research, not a VX855-only bug hunt.

**Recovered primary source:** `VIA Graphics Driver Group2.msi` (SHA-256 `a285b255c3df8c38fdd79e044162e66b6c864af3e95eb67c726e4d2a2e0d76f2`) contains the original embedded CAB. The retail `DDI_VIA.dll` was extracted byte-for-byte from it (SHA-256 **`529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`**) along with its matching retail `DDI_VIA.map`. The map identifies `fn3DBlt_H6` at **`0x10064503–0x1006548B`** and `fn3DBlt_H5S2` at **`0x1006548C–0x100664D7`**. Disassembly and ten original literal data tables were re-extracted and checked. The VX855 and VX900 Part II manuals were independently checked for their incompatible **Type-04 vertex-declaration bitfields**; the original VX900 manual calls Type-03 subtype `31h` reserved despite the CE DLL uploading it.

**Evidence limit:** The MSI does **not** contain two captured command buffers from an identical runtime input. What has been completed here is an **end-to-end static *logical stage reconstruction*** with *two consistently chosen branch profiles* and the actual byte-exact constant tables, *not* a claim of a fully specified, runnable or hardware-observed stream. Runtime addresses, computed pitches, converted geometry and transport alignment are labeled dynamic rather than invented. The portable audit and outputs are `Chrome9_H6_H5S2_Matched_Static_3D_Audit_271_280.py`, `Chrome9_H6_H5S2_Matched_Static_3D_Stages_271_280.csv` (21 stages) and `Chrome9_H6_H5S2_3D_Program_Tables_271_280.json` (all ten extracted tables and two branch profiles).

<a id="c9-section-272"></a>

#### 272. Three-platform dispatch: VX800 and VX855 share *one H6 compiled blit*, VX900 switches engine front-end

The CE binary's `C3DUtils::Init` (`0x100636BA`) maps PCI IDs **`1122h`** and **`5122h`** to the **same** `Init3DHWRegs_H6` / `fn3DBlt_H6` entry points. PCI ID **`7122h`** selects `Init3DHWRegs_H5S2` / `fn3DBlt_H5S2`. The companion texture helper is also shared across the H6 pair and different for H5S2 (see §§46, 86). This is **stronger evidence than two similar source routines**: VX800 and VX855 use *the same machine-code emitter* for this CE blit.

| Chip | CE blit backend | What is definitely reusable | What this does **not** establish |
|---|---|---|---|
| VX800 / `1106:1122` | **H6**, `0x10064503` | Identical CE 3D blit control-flow code with VX855 | Identical every-mode hardware behavior or all non-blit 3D commands |
| VX855 / `1106:5122` | **H6**, `0x10064503` | Same compiled emitter, constant tables, branch decisions | Complete VX800 register/errata equivalence |
| VX900 / `1106:7122` | **H5S2**, `0x1006548C` | A substantial common logical rendering/pixel-state pipeline | Interchangeable vertex input instructions, synchronization or silicon shaders |

The categories **identical literal**, **same logical algorithm**, **compatible with modifications**, **generation-specific**, and **unresolved hardware effect** are kept distinct in the companion CSV. Most important: **a shared H6 function is not proof that every command used by an arbitrary DX7/9 game is shared**.

<a id="c9-section-273"></a>

#### 273. Two controlled branch profiles expose the common versus specific instructions

The compared CE functions inspect equivalent fields of `KMD_3DBLT_DATA_tag` (source offsets are retained because their complete public API semantics are not documented). Two *model profiles* select consistent static branches:

| Branch constraint | **Compact single-coordinate blit** | **Extended multi-coordinate / multiblend blit** |
|---|---|---|
| `DWORD[arg+30h]` | `3` | `3` |
| `DWORD[arg+38h]` | `0` | nonzero |
| `BYTE[arg+34h]` | `255` | `128` (illustrative nonopaque alpha) |
| `BYTE[arg+1Ch] & 0x20` | `0` | may be 0 or set; global multi-blend state is enabled by the mode-3 path |
| Global `bGdiMultiABlend` | Reset to 0 by the selected compact branch | Set to 1 by the selected extended branch |
| Shared Type-01 state template | **37 DWORDs** at `0x10008820` | **50 DWORDs** at `0x100084F8` |
| Shared Type-01 PS-setting template | **12 DWORDs** at `0x10008770` | **12 DWORDs** at `0x100088EC` |
| H5S2 candidate Type-03 `31h` VS | **8 DWORDs** | **12 DWORDs** |
| Shared candidate Type-03 `20h` PS ALU | **4 DWORDs** at `0x1000875C` | **12 DWORDs** at `0x100088B4` |

The mode-3/extra/alpha conditions are anchored at H6 `0x10064639–0x10064693` and H5S2 `0x100655C2–0x1006561D`; the second-coordinate/extra-input decision is separate (H6 `0x10064BAE–0x10064BEA`; H5S2 `0x10065B38–0x10065B82`). **These are branch-consistent input *models***, not records of actual CE GDI calls; the dynamic vertex words still depend on other input fields and internal calculations. The retained global's lifetime outside this specific mode-3 branch is separately unresolved (§§78, 82–88).

<a id="c9-section-274"></a>

#### 274. Exact shared program and state payload inventory: 86 compact or 108 extended DWORDs

The two functions reference the **same image VA**, not merely equal duplicate constants, for these literal tables:

| Shared payload | Compact | Extended | Byte-exact basis |
|---|---:|---:|---|
| Type-01 render-state template | 37 DWORDs | 50 DWORDs | `0x10008820` / `0x100084F8` |
| Type-01 PS-control template | 12 DWORDs | 12 DWORDs | `0x10008770` / `0x100088EC` |
| Type-04 following-state table | 32 DWORDs | 32 DWORDs | **same** `0x100087A0` |
| Type-03 `20h` pixel ALU program | 4 DWORDs | 12 DWORDs | **same** `0x1000875C` / `0x100088B4` |
| Type-03 `21h` TAU program | 1 DWORD | 2 DWORDs | shared literal source at `0x100088E4` and adjacent word |
| **Subtotal of shared selected literal payload** | **86 DWORDs / 344 bytes** | **108 DWORDs / 432 bytes** | **same bytes in both backends for each corresponding selected profile** |

This *subtotal excludes* identical inline primitive constants, dynamic values, packet headers, alignment, per-vertex data, H5S2-only VS words and the VX900 wait patch. It is a deliberately **bounded shared subset**, not the complete transaction's total length. The JSON companion includes all ten source tables, their complete hexadecimal DWORD arrays, addresses and SHA-256 hashes. Both backends use the original same tables and no model “optimizations” were substituted.

For example, the compact **PS ALU** program at retail VA `0x1000875C` consists of exactly:

```text
01C00530 301C0053 00F20005 000D0820
```

The historical reports already identified part of this sharing (§§74–80); this pass makes the **branch-selected whole-stage subtotal and the complete literal contents machine-readable**. It does **not** claim the same shader microcode can be used with arbitrary vertex/shader declarations on the other generation.

<a id="c9-section-275"></a>

#### 275. Ordered logical rendering stage map: common stages surrounding a generation-specific vertex island

The compiled instruction order permits the following **static stage graph**. Branches and dynamic inputs prevent a complete byte-for-byte runtime stream; the CSV preserves 21 more granular stages and their instruction anchors.

| Ordered stage | VX800/VX855 **H6** | VX900 **H5S2** | Compatibility status |
|---|---|---|---|
| Hardware dispatch and buffer request | H6 callback and conditional `FE02`/manager allocation | H5S2 callback, same abstract transport choice | **Shared transport concept**, different callback |
| Type-01 target/alpha state | 37/50-word common literal tables | **Same source table addresses** | **Identical selected literals** |
| Dynamic target/texture registers | Pitch/size/address computed from blit arguments | Same conceptual calculations in corresponding source area | **Shared approach**, exact values require inputs |
| Type-01 pixel-shader settings | 12-word short/extended table | **Same source table** | **Identical selected literals** |
| Type-04 vertex length and declarations | Legacy H6/FVF scalar-attribute numbering | H5S2 input-register descriptors | **Different encoding** |
| Primitive controls and common state | `201C5C00`, `23002E02`, `24000003`; 32-word table | **Same three values and 32-word source table** | **Identical selected literals** |
| Additional vertex configuration | No corresponding Type-03 `31h` upload in this path | `04h:60h–62h`, then Type-03 `31h` candidate VS | **H5S2-only observed stage** |
| Pixel shader and conditional constants | Same compiled short/long ALU/TAU and alpha-constant algorithm | **Same source tables and equivalent conditional generator** | **Shared selected program**, dynamic constants |
| Vertex/primitive data | Calculated coordinates; 6/8 DWORD per vertex model | 6/8 DWORD per vertex model using different input declarations | **Same payload concept, not proven byte-equivalent** |
| Tail synchronization | `FE02` transaction to `0x41C`, value `0x00300000` | Eight `FE00` idle packets plus final `0x6C/0x00FC0000` packet | **Generation-specific stream tail** |
| Commit | Buffered `cmSmartFlush` or direct `KickOffCmd_Inv` | Same API choices, with different built stream | **Common submission interface** |

The previously established 2D engine and command-regulator compatibility should **not** be inferred from any one H6/H5S2 3D stage. Likewise the shared FE framing in the CE binary is a transport property, not proof of identical shader semantics.

<a id="c9-section-276"></a>

#### 276. Type-04 words are the main confirmed binary incompatibility, even for identical six-float vertex layouts

**Compact six-DWORD vertex case:**

| Type-04 address | VX800/VX855 H6 word | VX900 H5S2 word | Interpretation from respective manuals |
|---|---|---|---|
| `00h` | `00A80006` | `00A80106` | H6 six 32-bit FVF attributes; H5S2 six DWORDs in **two used input registers** |
| `01h` | `01020100` | `010000C2` | H6 first three scalar attributes X,Y,Z; H5S2 IR0 **Float4** |
| `02h` | `02090803` | `02000142` | H6 W,S,T; H5S2 IR1 **Float2** |
| `20h` | `201C5C00` | **same** | Shared literal primitive control |
| `23h` | `23002E02` | **same** | Shared literal primitive control |
| `24h` | `24000003` | **same** | Shared literal primitive control |

**Extended eight-DWORD case:** H6 selects `04h:00h = 00A80008` and includes scalar ST1 attributes in `04h:03h = 03000D0C`; H5S2 selects `04h:00h = 00A80208` and declares a third **Float2** input at `04h:03h = 03000242`, along with the `04h:12h` enabled-input mask of `12000007`. The compact H5S2 input mask is `12000003` (§80). The same numerical payload length is **not** the same register encoding or vertex-fetch contract.

**Manual basis:** VX855 Part II Type-04 `00h/01h/02h`, PDF pp.154–155; VX900 Part II Type-04 `00h/01h/02h`, PDF pp.159–160. The VX900 manual uses input-register IDs, dimensions and formats where H6 uses enumerated scalar FVF attributes. These manuals establish an **encoding difference**; pixel equivalence remains untested.

<a id="c9-section-277"></a>

#### 277. VX900 candidate vertex program is a precisely located extra stage—not a replacement PS program

The compact H5S2 branch writes **Type-04 `60h–62h`**:

```text
60800001 61800000 62104001
```

and then uploads Type-03 subtype **`31h`**, **eight DWORDs** from `0x1000870C`:

```text
0008014C 00000000 00000000 0008021F
00084124 00000000 00000000 00080283
```

The extended H5S2 branch instead writes:

```text
60800002 61800000 62204002
```

and uploads **12 DWORDs** from `0x1000872C`. The **first eight words are byte-for-byte the same** as the compact candidate; the additional four are:

```text
00088124 00000000 00000000 00080293
```

Byte-exact anchors: compact `0x10065C30–0x10065C73`, extended `0x10065C9D–0x10065CE3`. The extra instruction group co-varies with the third Float2 input declaration and input mask, supporting its relationship to vertex/texture-coordinate routing. It does **not** establish the opcode/operand meaning of the four-DWORD instruction group. VX900's manual calls the `03h:31h` program area **reserved**, and the `04h:60h–62h` controls remain undocumented in the supplied public manual. No corresponding upload is seen in the compared H6 blit path; this is a **compiled-function difference**, not proof that VX800/VX855 hardware could never accept another stage.

<a id="c9-section-278"></a>

#### 278. Common pixel-shader stages are byte-identical *only when the conditional branch is matched*

For the **compact** profile, both emit a Type-03 subtype `20h` four-DWORD ALU program from `0x1000875C` and a subtype `21h` one-DWORD TAU candidate. For the **extended** profile, both use the 12-DWORD ALU table `0x100088B4` and two-word TAU program, with the selected Type-01 `90h–9Bh` controls changing together. The H6 and H5S2 literal tables are exactly the same source VAs; this is stronger than matching symbolic “pixel shader enabled” flags.

Both may additionally construct a dynamic Type-03 `22h` constant when the shared multi-blend global is nonzero. This calculation includes `255.0/alpha` and is therefore **not a constant DWORD array**. The earlier §§78/82/88 flag-lifetime and zero-alpha caveats remain active; choosing a compact profile intentionally clears that global in the mode-3 branch, whereas the extended profile sets it. **Do not merge the two profiles into a hybrid stream** or claim the four-DWORD PS program and a retained dynamic constant can be combined without verifying state.

**Hardware qualification:** The VX900 manual labels portions of the 3D pixel-program register space reserved, even though VIA's CE H5S2 code emits it. Thus “identical PS bytes” establishes **software-program reuse** in this binary, **not** full VX900 3D shader ISA compatibility in hardware.

<a id="c9-section-279"></a>

#### 279. The post-draw tails differ by at least eight repeated wait packets, not just padding

In H6, `fn3DBlt_H6` ends the draw builder with an aligned **32-byte `FE02` setup/tail** to MMIO `0x41C` with data `0x00300000` (around `0x10065403–0x10065435`), then takes the shared buffered or direct submission branch. H5S2 instead calls **`VX900_WAITPATCH` at `0x10066438`**, which builds **eight 32-byte `FE00` packets** writing `(0x6C,0x00800000)`, and adds a final **32-byte `FE00` packet** writing `(0x6C,0x00FC0000)` at `0x10066453–0x10066483`. This is **256 bytes more nominal packet material** than H6's single 32-byte tail, *before* possible alignment padding or command-manager reservations. The packet content and the helper's eight repeated `FE00` headers have been verified against the original DLL.

The older VX800/VX855 manual labels `MMIO+0x6C` a hardware wait control; the VX900 public manual labels its relevant MMIO range reserved. We can therefore establish the **emitted software packets**, but cannot assert a tested VX900 silicon synchronization effect. This operation is **not safe to delete as “just padding”** when comparing logical streams: it intentionally emits a different register operation from H6. On the other hand, a larger CE packet tail alone does **not** establish lower VX900 rendering throughput.

<a id="c9-section-280"></a>

#### 280. Compatibility result, machine-verifiable deliverables, and remaining evidence required for a complete trace

**For this representative CE 3D textured blit:**

- **Identical CE code**: VX800 and VX855 use one H6 3D blit emitter, not merely the same packet values. The current objective's *VX800-versus-VX855* differences remain primarily in other operations, initialization and errata; do not manufacture a difference in this shared callback.
- **Identical selected constants across H6/H5S2**: 86 compact or 108 extended known, selected shared-table DWORDs, plus three inline primitive-control values. These are *partial literal subsets*, not total stream sizes.
- **Compatible algorithm, modified encoding**: input count, Type-04 vertex descriptor and optional second ST coordinate. The same conceptual 6/8 raw float inputs use incompatible register-declaration fields.
- **Generation-specific in this CE operation**: candidate H5S2 Type-03/`31h` VS upload, Type-04 `60h–62h` controls and H5S2 wait-patch tail. The source cannot prove these are wholly absent in all H6 hardware modes.
- **Unresolved**: exact VX900 VS instruction ISA and reserved bitfields; dynamic target/texture addresses; fully matched runtime vertex data and packet framing; actual pixel equivalence; silicon interpretation of the H5S2 waits.

**Reproducibility:** `Chrome9_H6_H5S2_Matched_Static_3D_Audit_271_280.py` verifies the original **MSI and DLL SHA-256**, all ten extracted literal-table ranges, both compiled function-entry symbols and the anchored static branches, two input-consistent profiles, all 21 stage-classification entries and the eight H5S2 wait blocks. Output: `Chrome9_H6_H5S2_Matched_Static_3D_Stages_271_280.csv` (21 stage rows) and `Chrome9_H6_H5S2_3D_Program_Tables_271_280.json` (full literal DWORD arrays, hashes, example branch selection and dynamic-field caveats). The script uses `objdump` and accepts `--dll`, `--map`, `--msi` and `--out-dir`, avoiding any need to execute a proprietary driver. All verification passed on the extracted retail image; **no CE/Win98 executable or GPU was run**.

**Priority after this pass:** (1) Obtain an actual **paired, controlled H6-versus-H5S2 command capture** to finish dynamic vertex/texture/target words and compare output pixels; (2) independently decode the VX900 Type-03/`31h` program with additional shader examples; and (3) do a separate **VX800-versus-VX855** non-blit 3D register/init audit rather than assuming the shared CE H6 entry point proves universal hardware identity. This keeps the research centered on the original three-platform objective.

<a id="c9-section-291"></a>

#### 291. VX800 and VX855 H6 parity

This pass asks whether **VX800/Chrome9 HC3 (`1106:1122`)** and **VX855/Chrome9 HCM (`1106:5122`)** require *different H6 command words* in the examined CE 3D setup, blit, or textured-draw utilities. The evidence is the same original CE DLL and two distinct original manuals (`vx800_2.pdf`, 2008; `vx855_2.pdf`, 2009). The correct standard is not “the chip names differ,” but whether a **documented bitfield or compiled command sequence** differs. The `Chrome9_VX800_VX855_H6_Parity_Audit_291_299.py` script and CSV preserve the audited subset and caveats.

<a id="c9-section-292"></a>

#### 292. The CE family switch proves both IDs use the same H6 function pointers

`C3DUtils::Init` (retail `0x100636BA–0x10063751`) first compares against `1122h` at `0x100636CF–0x100636D4`; a separate chain of subtractions reaches the **same** branch at `0x10063735` for `5122h` (`0x100636E9–0x100636EE`). That shared branch calls **`Init3DHWRegs_H6` at `0x10063470`**, installs **`Texture3D_H5` at `0x10067255`**, and installs **`fn3DBlt_H6` at `0x10064503`**. There are not separate VX800 and VX855 copies of those selected 3D emitters in the retail image. The `H5` substring in the shared texture utility's name is **not** a reason to classify VX855's 3D blit as H5S2: it is a shared earlier-family utility alongside the H6 blitter.

<a id="c9-section-293"></a>

#### 293. A byte-identical selected initializer is stronger than superficially similar manuals

`Init3DHWRegs_H6` at `0x10063470–0x10063566` is a **single machine-code routine** for both PCI IDs, first invoking `Init3DHWRegs_INV` at `0x100630F7`, then emitting a stream of **Type-01** setup words over MMIO `0x43C/0x440`. Examples include the same command selector (`0x00FE0000`) and fixed words `0x000E1804`, `0x08000249`, `0x0A0002C9`, `0x40008408` (as stored by the compiler; each is quoted in the original 32-bit immediate form in the audit). The initializer does not have an in-routine `cmp/sub` on `1122h` or `5122h` in the examined code. **Result:** the compiled *selected initialization instructions* are identical. This does not establish that all register reset values, clocks or undocumented side effects match.

<a id="c9-section-294"></a>

#### 294. H6 3D blit and older Texture3D share exactly the same compiled implementation

`fn3DBlt_H6` at `0x10064503–0x1006548B` and `Texture3D_H5` at `0x10067255–0x10067B15` are **the same machine-code functions for both devices**, not separately versioned variants. The blit includes the common primitive words `201C5C00`, `23002E02`, `24000003`, selected Type-01 state tables, vertex declarations and Type-03 pixel programs already analyzed in §§271–280. It contains **no direct immediate comparison** against PCI IDs `1122h` or `5122h`; the texture function similarly contains no such immediate comparison. **Limitation:** machine-code identity holds at the entry-point level; runtime arguments, hidden globals and device initialization outside these functions could still differ. This is not blanket proof of all hardware 3D operations being bitwise identical.

<a id="c9-section-295"></a>

#### 295. VX800 and VX855 manuals agree on the H6 scalar FVF field structure

The original VX800 Part II **HParaType-04 `00h–1Fh`** table (printed pages 130–131) and VX855 Part II **Type-04 `00h–03h`** fields (printed pp. 148–150) both document **`00h` bits `[5:0]` as vertex length in 32-bit units**, bits `[16:15]` as vertex memory location, and subsequent `01h/02h/03h` words as enumerated **scalar FVF attribute numbers** (X, Y, Z, W, ST and other attributes). The compiled H6 `00A80006` and `01020100`/`02090803` words are consequently consistent with both manuals. By contrast VX900 uses a different **input-register** declaration (§287), so older-family bitfield identity **does not extend to VX900**.

<a id="c9-section-296"></a>

#### 296. Important manual wording discrepancy: vertex-buffer placement note changes

The VX800 Part II Type-04 `00h` location field says that vertex buffers are located in **S.F.** Its corresponding VX855 manual carries the same `00b S.L.`, `01b S.F.`, `10b reserved System Memory` location enumeration but broadens the following note to **“LL, SF, or all in LL”** (original typo/terminology preserved here). This is **a real text difference in the public VIA manuals**, not yet a proven hardware capability difference. Both chips still select the **same compiled H6 command generator** and we do not have separate controlled silicon traces demonstrating whether “LL” is a supported new vertex-buffer mode, a driver policy, or an editorial error. Therefore classification: **common documented encoding; location-constraint wording unresolved**.

<a id="c9-section-298"></a>

#### 298. Compatibility classification and remaining narrow family-specific work

| Evidence item | VX800 vs VX855 result | Meaning for a shared H6 driver |
|---|---|---|
| 3D dispatch | **Same CE callback targets** | Shared setup/blit/texture code is an established implementation choice |
| H6 initialization | **Same compiled stream generator** | Common register programming for the selected path |
| H6 blit | **Same compiled stream generator** | Same selected literal commands when inputs/state match |
| Texture3D | **Same compiled helper** | A shared older-generation utility; not H5S2/VX900 |
| Type-04 scalar FVF fields | **Common documented positions** | Same FVF declaration decoding for audited fields |
| Type-04 memory-placement note | **Text differs** | Hardware interpretation unresolved, do not auto-change memory location |
| Type-10h CMDQ length | **Common 128-bit unit definition** | Shared manual interpretation, hardware behavior not independently checked |
| All other 3D modes and errata | **Not exhaustively audited** | Device-specific edge cases remain possible |

Most of the remaining VX800-versus-VX855 3D uncertainty is *outside* these selected shared CE functions: broader texture/state register coverage, undocumented feature/errata bits, device configuration, AGP/memory allocation policy and actual rendering output. Absence of immediate PCI comparisons within a function is **negative evidence scoped to that function only**.

<a id="c9-section-299"></a>

#### 299. H6 parity verification and remaining questions

`Chrome9_VX800_VX855_H6_Parity_Audit_291_299.py` pins the same original retail DLL and linker map, verifies the **two-ID switch reaches the exact same pointers**, checks the absence of direct 1122/5122 immediate comparisons *within* the inspected common routines, confirms manual FVF field and queue-unit correspondence, and catches the vertex-buffer note discrepancy. The companion `Chrome9_VX800_VX855_H6_Parity_Matrix_291_299.csv` contains ten classified rows. It passed without executing hardware. **Next objective:** the remaining *2D command-equivalence* inventory, particularly mono, pattern, alpha, color key, rotation and format conversion, focusing on instructions which actually differ between the three family backends. The fact that selected H6 3D functions are shared does not settle those 2D differences.

<a id="c9-section-366"></a>

#### 366. CE 7 corroboration of 3D routing, pixel programs and the Type-10 preamble

Addresses are preferred image virtual addresses in the attached CE 7 retail DLL, with image base `0x10000000`.

| Check | Result and binary anchors |
|---|---|
| H6/H5S2 dispatch | `C3DUtils::Init` at `0x10072A52` routes `1122h` and `5122h` to the same H6 branch at `0x10072ACD`. That branch calls `Init3DHWRegs_H6` at `0x10072808` and installs `Texture3D_H5` at `0x100765F3` and `fn3DBlt_H6` at `0x1007389D`. The `7122h` branch at `0x10072A8F` uses `0x100728FF`, `0x10076EBE` and `0x1007482E` for the corresponding H5S2 functions. Both install `WaitEngIdle_CR` at `0x100729D3`. |
| VX900 vertex records | The two H5S2 blit writes of selector `0x31030000` at `0x10075008` and `0x10075075` upload 8 and 12 DWORDs from `0x1000873C` and `0x1000875C`. The third site, in `Texture3D_H5S2` at `0x1007757E`, constructs the compact payload inline. The extended payload has the compact prefix and adds `00088124 00000000 00000000 00080293`, matching §284. The compact and extended Type-04 `60h–62h` literals also match the master. Their detailed meanings and the vertex ISA remain open. |
| Shared pixel data and extra waits | H6 and H5S2 reference the same Type-01 state tables and branch-matched Type-03 `20h/21h` pixel-program tables in this build. Examples are the 50-DWORD state source `0x10008528`, which includes `0x01CE1540` with bit 10 set, and ALU sources `0x1000878C` / `0x100088E4`. Both construct conditional subtype `22h` packets at `0x10074106` and `0x1007519B`. The VX900 blitter calls the eight-packet `VX900_WAITPATCH` at `0x100757D6`, then appends the `0x6C=0x00FC0000` packet at `0x100757F1–0x1007581D`. This verifies a family-specific tail and the reuse of selected compiled payloads. |
| Type-10 `69h` | In `hwmInitialize_inv` at `0x1005F4EB`, all three IDs take the common Type-11 preamble and then the Type-10 write sequence. The latter selects `0x00100000` at MMIO `0x41C` and writes `0x69000000` at MMIO `0x420` via instruction `0x1005F561`. This independently confirms the VX900 software write in C9-091. The hardware effect of the zero-valued field remains unknown. |

### Vertex-program payloads, descriptors and routing

<a id="c9-section-372"></a>

#### 372. VX900 XP vertex-program upload and Type-04 control fields

The XP CIL backend provides a counted instruction upload in addition to the CE fixed-payload examples. Its Type-04 shadow starts at `+0x20E0` and initializes the command high bytes through subaddress `62h` at `0xC5A33–0xC5AC4`. The `0xCB3A0` emitter selects a program object through `0xE7E80` or `0xE8F30`. In the generated fixed-function path, the pretransformed FVF test is `(fvf & 0x400E) == 4`. The builder uses 16-byte records, including layout-dependent extra records, and derives the record count from the write cursor at `0xE7E54–0xE7E73`. All addresses in this section are preferred VAs in `s3ginvA.dll`. [XP binary identities](#c9-section-375).

On a full program change, the emitter writes the following two blocks with 16-byte alignment. `N` is the instruction record count from program object offset `+0x424`. The copy uses `4*N` DWORDs starting at `+0x42C`. The first block appears at `0xCB905–0xCB975`, the second at `0xCB9A5–0xCB9FE`, and the assembled stream is passed to `0x80810` at `0xCBA19`.

```text
FE020440  0000043C  00040000  CC000000
04h:61h  04h:62h   04h:60h   [CC000000 alignment]

FE03043C  4*N       31030000  CC000000
[N four-DWORD records]
```

| Type-04 field | What the XP emitter writes | CIL VA |
|---|---|---|
| `60h[8:0]` | `(program[+0x14] - 1) & 0x1FF`. In the generated fixed-function path, `+0x14` is assigned the instruction count `N`. The emitter also sets bit 23 and clears bits `[20:12]`. | `0xE8022–0xE8029`, `0xCB493–0xCB4C9` |
| `61h[23]` | Set if the unsigned DWORD at program offset `+0x00` is at most 8. | `0xCB4EE–0xCB513` |
| `61h[22:20]` and `61h[19]` | Low three bits of `program[+0x08]` and the nonzero test of `program[+0x0C]`, respectively. A partial state-update branch can emit only the `61h` word. | `0xCB519–0xCB553`, `0xCB835–0xCB8A1` |
| `62h[8:0]` | `program[+0x10] & 0x1FF`. In the generated fixed-function path, this is the last zero-based record index for which any of the first three DWORDs satisfies `(word & 0x00780000) == 0x00080000`. The scan defaults to zero. The emitter preserves the higher bits. | `0xE802C–0xE803B`, `0x105D00–0x105D5E`, `0xCB4CF–0xCB4E8` |

The two CE programs both give `60h[8:0] == 62h[8:0]`. Each CE record has a first DWORD matching the XP scan, so both formulas produce 1 for the compact program and 2 for the extended program. This explains the correlation without requiring the two fields to track each other. For example, two synthetic records with a match only in record zero yield `60h[8:0]=1` and `62h[8:0]=0` by the XP formulas. This is an offline arithmetic check. The marker's silicon meaning, the changing upper bits of the CE `62h` word, and the rest of the instruction encoding remain unresolved. [CE instruction payloads](#c9-section-284).

Other XP command sites issue the VIA-documented 16-entry stream table (`03h:30h`), float constants (`03h:32h`) and integer constants (`03h:33h`). `0xC5747–0xC579B` sends 16 DWORDs with selector `0x30030000` in the branch for internal engine code 5. `0xCB5E5` and `0xCB705` start 12-DWORD constant writes with selector `0x32030400`. The driver also constructs `0x33030000`-based selectors at `0xC9BBB`. The public manual names the program and constant areas in its summary and reserves them in the detailed table. V9-II, pp. 78, 159.

<a id="c9-section-378"></a>

#### 378. XP vertex-program upload is gated by chip family

The VX800 package's multi-family CIL contains a counterpart to the VX900 counted upload. The class-4 state table at `0x1F9468` places `0xC4E50` at offset `+0x60`. This routine writes Type-04 `61h`, `62h`, then `60h` before uploading `4*N` DWORDs under selector `0x31030000` at `0xC5455–0xC54AE`. The program record count and source offsets are `+0x424` and `+0x42C`, matching the later XP build. [VX900 packet and field encoding](#c9-section-372).

The distinction is in the initial context flags. At `0xC03D0–0xC041A` the class-4 initializer clears bit 0 of context flags `+0x8C60` and sets bit 1. It sets bit 0 again only for adapter family code 5. VX800 `1122h` and VX855 `5122h` get code `0xD`; VX900 `7122h` gets code 5. The state update at `0x82129` tests bit 0 before it can call the counted uploader at `0x82166`. Its later branch tests bit 1 at `0x82190`. Therefore the class-4 table and the DP2 shader handlers alone cannot establish a VX800 hardware vertex-program path. The default H6 initialization bypasses this upload gate. This is a static finding about the traced path. [Adapter classification](#c9-section-376).

<a id="c9-section-281"></a>

#### 281. VX900 vertex-program upload interface and decoding limits

**Objective:** distinguish the VX900 Chrome9 HD **H5S2 vertex-program upload** and Type-04 vertex-input controls from the shared VX800/VX855 H6 interface, then move to the separate VX800-versus-VX855 3D programming comparison. The original CE retail `DDI_VIA.dll` SHA-256 is `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`; the original source MSI is `a285b255c3df8c38fdd79e044162e66b6c864af3e95eb67c726e4d2a2e0d76f2`. All findings in §§281–299 are **read-only compiled-code/manual analysis**. No actual shader execution or GPU ISA emulator is claimed.

Earlier §§271–280 found two compiled H5S2 blit shader program lengths (8 and 12 DWORDs). This pass verifies another **independent upload site** and correlates the program sizes with actual vertex-input declarations and reserved controls.

<a id="c9-section-282"></a>

#### 282. VIA VX900 manual itself identifies subtype `03h:31h` as vertex-shader instructions

The VX900 Part II manual's **command-regulator parameter subtype summary** identifies:

- Type-03 subtype `31h`: **Vertex Shader Instruction**;
- Type-03 subtype `32h`: **Vertex Shader Constant Register**;
- Type-03 subtype `33h`: **Vertex Shader Integer Register**.

The very same manual's detailed Type-03 register chapter subsequently calls `31h–33h` **reserved**, while Type-04 `53h–62h` is also described as reserved. This is a **documented public-manual inconsistency**, not evidence that these writes are meaningless. The retail CE driver *actually emits* Type-03 `31h` programs on VX900. We can therefore classify the stream as **vertex-shader instruction upload**, but **cannot assign ISA opcodes, source/destination fields, instruction ordering semantics, or output registers** from the public manual. The source references are in `vx900_2.pdf` (command-regulator subtype summary and detailed Type-03/Type-04 chapters). The script checks both contradictory text strings.

<a id="c9-section-283"></a>

#### 283. Three compiled upload sites; two unique vertex programs

The original CE retail image has exactly **three** literal DWORDs `0x31030000` (Type-03 subtype `31h` upload selectors) in the examined image. The code sites are:

| Site | Compiled routine / address | Program source | Number of DWORDs | Interpretation |
|---|---|---|---:|---|
| 1 | `fn3DBlt_H5S2`, `0x10065C60` | `.rdata` VA `0x1000870C` | 8 | Compact blit with two active input registers |
| 2 | `fn3DBlt_H5S2`, `0x10065CCD` | `.rdata` VA `0x1000872C` | 12 | Extended blit with three active input registers |
| 3 | `Texture3D_H5S2`, `0x100681CC` | Inline immediate instruction data | 8 | **Same eight-DWORD program as compact blit**, independently constructed |

The third site proves **program reuse across two rendering operations**; it does **not** introduce a third distinct program. The older shared `Texture3D_H5` helper (selected for the VX800/VX855 CE path) contains no matching Type-03 `31h` selector within that compiled routine. That is a difference in the selected CE helper, **not a proof of physical impossibility** for an unknown H6 programming mode.

<a id="c9-section-284"></a>

#### 284. Complete candidate programs: common prefix and one four-DWORD extension

**Eight-DWORD compact candidate**, VA `0x1000870C`:

```text
0008014C 00000000 00000000 0008021F
00084124 00000000 00000000 00080283
```

**Twelve-DWORD extended candidate**, VA `0x1000872C`:

```text
0008014C 00000000 00000000 0008021F
00084124 00000000 00000000 00080283
00088124 00000000 00000000 00080293
```

The first **eight DWORDs are literally identical**, and the extension is exactly `00088124 00000000 00000000 00080293`. On the corresponding previously present four-word group, the first word XOR difference is `0x0000C000` and the fourth word XOR difference is `0x00000010`. Those **numerical correlations** alone do **not** establish the effect of either bit. The evidence proves the *program data* and *selection*, not the meaning of the four-DWORD instruction record. Companion JSON: `Chrome9_VX900_Vertex_Program_Records_281_290.json`.

<a id="c9-section-285"></a>

#### 285. Type-04 `00h`, `12h` and `60h–62h` co-vary with the input-register count

| Selected vertex profile | Active input registers | Vertex payload DWORDs | Type-04 `00h` | Type-04 `12h` mask | Type-04 `60h` | Type-04 `61h` | Type-04 `62h` | VS DWORDs |
|---|---:|---:|---|---|---|---|---|---:|
| Compact blit; also Texture3D | 2 | 6 | `00A80106` | `12000003` | `60800001` | `61800000` | `62104001` | 8 |
| Extended blit | 3 | 8 | `00A80208` | `12000007` | `60800002` | `61800000` | `62204002` | 12 |

The CE driver also declares **IR0 Float4** (`010000C2`) and **IR1 Float2** (`02000142`); the extended case adds **IR2 Float2** (`03000242`). This is the actual transition represented by the additional two vertex DWORDs. The observed `12h` mask changes from binary `011` to `111`, and the undocumented controls change together. Across these **two** distinct profiles the words can be represented by the *empirical* relationships `04:60 = 0x60800000|n`, `04:61=0x61800000`, and `04:62 = 0x62004000 | (n<<20) | n`, where `n = active_IR_count−1`. **These are interpolation descriptions of two observations, not verified bitfield specifications or validated commands for n≥3.**

<a id="c9-section-286"></a>

#### 286. Independent Texture3D path corroborates the compact vertex-program/control bundle

`Texture3D_H5S2` at `0x10067B16` emits a Type-03 `31h` selector at `0x100681CC` and **constructs the compact program using inline DWORD constants** at `0x100681D8–0x10068200` rather than copying the 8-DWORD `.rdata` array. Its Type-04 control words at `0x10068354–0x10068364` are `60800001 / 61800000 / 62104001`; its vertex declarations and mask match the two-input-register/6-DWORD pattern. Thus the same complete vertex-program/control **profile** occurs in two independent CE H5S2 command generators. This corroboration is stronger than repeated references within a single branch, while still offering no independently varied third **distinct** program with which to decode the ISA.

<a id="c9-section-287"></a>

#### 287. VX900 H5S2 instruction-interface difference from the older shared H6 backend

For the matched six/eight-DWORD draws, the older H6 blit declares **scalar FVF attributes** with the classic Type-04 words (`00A80006`, `01020100`, `02090803`; extended adds `03000D0C`). H5S2 uses **packed input-register declarations** (`00A80106`, `010000C2`, `02000142`, optionally `03000242`) and the Type-04 `12h` mask, then additionally configures reserved `60h–62h` and uploads Type-03 subtype `31h`. The shared pixel-shader control tables remain the same selected CE literal data (§§271–280). This identifies a **generation-specific front-end programming difference** inside otherwise substantially common rendering state.

A compact H6 stream **must not be assumed directly runnable** on VX900 merely because its vertex data has the same number of floats. Conversely, the H5S2 code's use of shader instructions does not by itself establish a general-purpose programmable vertex pipeline or map the VX900 ISA to a public API shader version.

<a id="c9-section-288"></a>

#### 288. Unresolved fields: what the current evidence cannot decode

The currently recovered DLL offers **two** distinct Type-03 `31h` instruction arrays and two distinct values for the Type-04 `60h/62h` controls. It does not supply orthogonal variations isolating destination register, source register, swizzle, arithmetic operation, output encoding, vertex count, or independent field widths. The two CE values `60800001` and `60800002` alone cannot distinguish input count from program length. The independent XP builder derives the corresponding low field from program length and the `62h` low field from a record scan ([§372](#c9-section-372)). Type-04 `61h=61800000` is constant in both observed configurations and therefore has **no informative variation**. These CE sites do not establish writes to `32h` or `33h`. The XP emitter confirms separate constant writes to those subtypes and the four-DWORD instruction size. The instruction opcode and operand encoding remains open ([§372](#c9-section-372)).

<a id="c9-section-289"></a>

#### 289. Practical comparative-classification result for the compatibility matrix

| Command family / stage | VX800/VX855 (selected CE H6 paths) | VX900 (selected CE H5S2 paths) | Defensible classification |
|---|---|---|---|
| Type-01 render-state / selected pixel-program literals | Shared values and tables | Same selected bytes | **Identical literal subset**, dynamic address differences remain |
| Type-04 vertex declaration `00h–03h` | Scalar FVF encodings | Input-register Float4/Float2 encodings | **Different required encoding** |
| Type-04 input mask `12h` | Legacy FVF path without corresponding mask in inspected branch | `12000003` or `12000007` | **H5S2-selected behavior; not transferable unchanged** |
| Type-04 `60h–62h` | No analogous selected H6 emission | Co-varies with input count | **VX900 H5S2-specific observed programming; semantics unresolved** |
| Type-03 `31h` VS upload | Not in inspected H6 blit/older Texture3D helper | 8/12 DWORDs | **VX900 CE-specific observed upload** |
| Type-03 `20h/21h` PS code | Selected common table data | Same selected common table data | **Common literals, conditional use** |
| Runtime vertex execution | No paired hardware trace | No paired hardware trace | **Unverified** |

<a id="c9-section-290"></a>

#### 290. Reproducibility and completion criterion for the VX900 vertex objective

The read-only, hash-pinned `Chrome9_VX900_Vertex_Program_and_Routing_Audit_281_290.py` passed and exports both the full program arrays and the two-profile routing matrix. It checks the original DLL, source MSI, linker-map dispatch, three compiled upload sites, exact program words, declarations, control bytes, older Texture3D absence, and contradicting manual labels. **What is complete:** the observed *program records*, associated selected *register words*, source sites, and explicit generation-specific command difference. **What remains open:** a general H5S2 shader ISA and silicon behavior. A full opcode decode needs independent, differently structured programs or a controlled hardware test, not invented instruction semantics.

<a id="c9-section-52"></a>

#### 52. CE H5 vs H5S2 vertex packet: bit-exact declarations, masks and emitted vertex layout

**Scope and provenance.** This continues §50–51 by comparing only the VIA-authored CE `Texture3D_H5` (VX800/VX855) and `Texture3D_H5S2` (VX900) **specific textured-triangle paths**. Evidence is the same retail x86 `DDI_VIA.dll`, SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`, plus the corresponding VIA Part II manuals. Virtual addresses refer to the DLL loaded at base `0x10000000`. All static assertions passed in `_ce_add_vertex52_53.py`; no GPU was run.

##### 52.1 Same `04h:25h` register semantics; different active attributes

| Type-04 word | CE H5 (VX800/VX855) | CE H5S2 (VX900) | Verified meaning |
|---|---|---|---|
| `04h:24h` | `0x24000003` | `0x24000003` | Three vertices in primitive list |
| `04h:25h` | `0x25F6C000` | `0x25F0C000` | H5: X/Y/Z/W, diffuse C0, specular C1, texture S/T. H5S2: X/Y/Z/W and texture S/T, without C0/C1. |
| Words per vertex | 8 | 6 | Each enabled attribute consumes one DWORD in this CE command-vertex path |
| Three vertices | 24 DWORDs = 96 bytes | 18 DWORDs = 72 bytes | **24-byte smaller** primitive payload on H5S2 for the same three vertices |

The FVF masks differ by `0x00060000`, **only bits 18 and 17**: the documented diffuse and specular color fields. This is a *shared mask encoding used with different data*, not a new incompatible Type-04 mask definition. H5 inserts `0xFFFF0000` into both color slots of each vertex; the H5S2 path omits them. Those literal color values are **not evidence of visible red output**: other render/texture stages can ignore or replace them.

Concrete emitted **first-vertex payload**, after the stream selector/vertex header, reconstructed from the disassembly (not a full decoded command-buffer dump):

| Word | H5 | H5S2 |
|---:|---|---|
| 0 | X, unmodified float32 | X after adding `786432.0`, float32 |
| 1 | Y, unmodified float32 | Y after adding `786432.0`, float32 |
| 2 | `0x00000000` (Z = 0) | `0x00000000` (Z = 0) |
| 3 | `0x3F800000` (W = 1) | `0x3F800000` (W = 1) |
| 4 | `0xFFFF0000` (C0) | S float32 |
| 5 | `0xFFFF0000` (C1) | T float32 |
| 6 | S float32 | — |
| 7 | T float32 | — |

H5 emission: approximately `0x10067A0E–0x10067A43` (first vertex), through `0x10067A9B` (third). H5S2 emission: approximately `0x10068382–0x100683A9` (first vertex), through `0x100683EE` (third). The biased coordinates for the three H5S2 vertices are the six values modified at `0x10067E14–0x10067E50` (§50.3). Their **GPU interpretation** remains an inference; their actual transmission as 32-bit words is established.

##### 52.2 The VX900's separate `04h:01/02/12h` input-register declarations match the six-word layout

Unlike H5, the H5S2 path writes a new Type-04 declaration sequence at `0x10068284–0x100682A4`:

| Emitted word | VX900 manual meaning | Decoding |
|---|---|---|
| `0x010000C2` (`04h:01h`) | First vertex datum D0 | Input register IR0, format `0xC2` = Float4 (4×32-bit float), unnormalized |
| `0x02000142` (`04h:02h`) | Second vertex datum D1 | Input register IR1, format `0x42` = Float2 (2×32-bit float), unnormalized |
| `0x03000000` (`04h:03h`) | Third vertex datum D2 | Unused/default declaration in this sequence; do not infer that D2 is fetched |
| `0x12000003` (`04h:12h`) | Input-register source mask | IR0 and IR1 are marked as supplied from vertex data (`[1:0]=11b`) |

The format-code interpretation is explicit in **VX900 Part II PDF pages 160–161**: `Float4 => 11 00001 0b`, `Float2 => 01 00001 0b`, which are `0xC2` and `0x42`. The IR0/IR1 mask is documented on **PDF page 165**. These declarations are consistent with the six-float payload just traced. Importantly, the public VX900 description uses vertex-buffer terminology; **this CE case also transmits direct command-vertex data**, so the exact role of its declarations in command mode should not be generalized to arbitrary hardware vertex-buffer fetches.

##### 52.3 VX900 explicitly supplies white/identity defaults for attributes it omits

H5S2 writes **`0x2F582000`** (`04h:2Fh`) at `0x100682E4`. VIA's documented bit layout decodes its payload (`0x582000`) exactly as these set bits:

- Bit 22: default W = 1.0.
- Bit 20: default color-0 RGB = 1.0.
- Bit 19: default color-0 alpha = 1.0.
- Bit 13: default texture Q = 1.0.

Together with the absence of C0/C1 from `04h:25h`, this explains **how the CE H5S2 path can omit transmitted colors while still providing defined color-0 and alpha defaults**. It is plausible that this supports texture-only compositing with a white multiplicative color, but that outcome depends on the configured texture/blend stages. Default specular C1 is *not* set to 1.0 by this word. VIA's bitfield definitions are in **VX900 Part II PDF page 176** (and the VX855 counterpart, page 169).

H5S2 additionally writes `0x2A3F8000` and `0x2B3F8000` at `04h:2Ah/2Bh`, the documented point-width/point-height defaults. This is a **triangle** operation, so those writes do not demonstrate point rendering or hardware point-sprite acceleration. Do not reinterpret their payloads as ordinary IEEE float32 without checking the manual's unusual `s[8].15` representation.

##### 52.4 `04h:28h` default-mask change follows the omitted color/fog attributes

H5 emits `0x28FFC000`; H5S2 emits `0x28F0C000`, a difference of `0x000F0000`. Both retain X/Y/Z/W and texture-0 S/T selection; H5S2 clears bits `19:16` relative to H5, including the C0/C1 and fog-related mask positions. **Bit 19 in `04h:28h` is marked reserved in the VX900 manual**, so do not infer that the entire four-bit difference is an intentionally documented functional change. The confirmed observation is that VIA's paths program different values, consistent with H5S2's shorter active attribute set.

##### 52.5 Practical cross-generation encoder boundary

A common *logical* vertex data description (`position4`, optional `diffuse/specular`, `texcoord2`) can drive both layouts, but a **single fixed eight-DWORD packet cannot serve the VX900 CE path**. Generate the payload from the mask, set the VX900 declaration/input-register words when using H5S2, and keep the coordinate-bias operation chipset-specific until its pixel behavior is measured. Nothing here proves that every OpenGL or Direct3D vertex format has the same binary layout across chips.

<a id="c9-section-53"></a>

#### 53. CE primitive-cycle/shading changes and undocumented VX900 Type-04 words

##### 53.1 VIA selects distinct primitive-cycle modes

At `04h:20h`, the CE H5 path emits `0x20000000`; H5S2 emits **`0x201C5C00`** (`0x100682AC`). The VX855/VX900 manuals describe bits `[15:8]` as the primitive render mode. The H5 value selects the full-vertex cycle. The H5S2 value includes bit 14 (`0x4000`), selecting **automatic fast primitive vertex cycling**; the paired reuse selectors in bits `[21:16]` and `[13:8]` both encode (`A=previous A`, `B=previous C`, `C=new`) under the manual's individual field descriptions. This is a concrete *command-programming difference*, **not** proof that faster vertex cycling is safe for other primitive lists or that its performance benefit is measurable in a game.

The H5S2 selector changes are compatible with fast reuse/strip-style primitive assembly; the CE routine's exact primitive start/reset behavior still needs tracing before emulating this mode on VX855. Manual refs: **VX855 Part II PDF pages 159–160**, **VX900 Part II PDF pages 166–167**.

##### 53.2 Shading and winding bits change, but triangle mode remains

At `04h:23h`, H5 emits `0x23000F02`; H5S2 emits `0x23002E02` (`0x10067977`, `0x100682B4`). Their XOR is `0x00002100`:

| Field | H5 | H5S2 | Meaning from both manuals |
|---|---:|---:|---|
| `[14:12]`, shading | 0 | 2 | Solid shading vs. flat shading from vertex B |
| `[11:9]`, edge enable | 7 | 7 | All three edge flags set (relevant when rendering edges) |
| Bit 8, backface winding convention | 1 | 0 | Different facing orientation if culling is enabled elsewhere |
| `[3:0]`, primitive type | 2 | 2 | **Both select filled triangles** |

**Do not conclude that actual face culling is enabled** solely because bit 8 differs: it chooses the winding convention used *if* culling is otherwise active. It is also not yet established whether the shading difference affects the final CE pixels or is masked by texture-stage state. Manual refs: **VX855 Part II PDF page 161**, **VX900 Part II PDF page 167**.

##### 53.3 VX900 writes another 3-word Type-04 group that its public manual marks reserved

Immediately before vertex transmission, H5S2 emits:

```
04h:60h = 0x60800001    (VA 0x10068354)
04h:61h = 0x61800000    (VA 0x1006835C)
04h:62h = 0x62104001    (VA 0x10068364)
```

**The VX900 manual expressly marks `04h:53h–62h` reserved (PDF page 179).** Therefore these are **real VIA-authored writes whose published bitfield meaning is unavailable**, not register definitions we can confidently borrow for the VX855 driver. They may be a revision-specific workaround or an omitted engine feature; the binary alone does not discriminate those possibilities. Note that the Type-04 selector here is not ordinary MMIO offset `0x60`—it is a *3D parameter sub-address*.

Additional safeguards: no hardware was tested, the direct-MMIO transport will parse the same CE packets but might have different timing, and the sequence should never be transplanted independently of its generation-specific setup.

##### 53.4 Next concrete comparisons

1. Trace the companion `fn3DBlt_H6` and `fn3DBlt_H5S2` code for **the same Type-04 attributes**, to see if the texture-helper choice is a general H5S2 convention or a one-off compositing optimization.
2. Determine which Type-01 texture/blend state consumes C0 when omitted; this is needed to explain why H5 transmits a literal C0/C1 while H5S2 uses default white.
3. Trace the first primitive after `04h:20h` reset to establish the exact meaning of the fast-cycle reuse flags.
4. Cross-reference the `04h:60h–62h` words against symbols or contemporaneous VIA driver binaries; do not guess their hardware meaning from their values.
5. Test separately on hardware: a three-vertex textured triangle, a continuing strip, flat vs solid color, reversed winding, and subpixel coordinate changes, collecting the emitted Type-04 words and the resulting pixel images.

**Status:** Exact emitted words, H5/H5S2 dispatch, mask differences, declarations, x87 bias, and per-vertex layout are verified from the supplied retail DLL. The manuals establish the specified bitfields except for reserved `04h:60h–62h`. Behavioral equivalence, actual speedups, and the function of undocumented registers are **not** established.

<a id="c9-section-54"></a>

#### 54. Fast-cycle and six-word triangles on VX800 and VX855

**This refines §52–53.** The six-word format and automatic fast-cycle setting found in the VX900 *texture helper* are **not VX900-only hardware capabilities**. The VIA CE package's **VX800/VX855-selected `fn3DBlt_H6`** emits those same Type-04 values in its own blit path. The earlier eight-word `Texture3D_H5` example is only **one particular operation**. It must not be generalized to the entire VX855 driver.

##### 54.1 Actual H6 vs H5S2 3D-blit values

The two compiled routines are `fn3DBlt_H6` at `0x10064503` (VX800/VX855) and `fn3DBlt_H5S2` at `0x1006548C` (VX900). Both build `FE02` parameter packets with selector `0x00040000` (Type-04) and choose one of two attribute formats based on a source-operation flag or VIA global blending setting (`[input+0x1C] & 0x20`, or global VA `0x100CE7EC`):

| CE 3D blit command | H6: VX800/VX855 | H5S2: VX900 | Classification |
|---|---|---|---|
| `04h:20h` | `0x201C5C00` | `0x201C5C00` | **Bit-identical** automatic fast/reuse vertex cycle |
| `04h:23h` | `0x23002E02` | `0x23002E02` | **Bit-identical** flat-from-B shading, winding convention and filled-triangle mode |
| `04h:24h` | `0x24000003` | `0x24000003` | **Bit-identical** 3-vertex primitive |
| `04h:25h`, single texture | `0x25F0C000` | `0x25F0C000` | **Bit-identical** mask: XYZW + texture0 S/T, 6 values per vertex |
| `04h:28h`, single texture | `0x28F0C000` | `0x28F0C000` | **Bit-identical** parameter/default mask |
| `04h:25h`, second texture set | `0x25F0CC00` | `0x25F0CC00` | **Bit-identical** mask: XYZW + texture0 S/T + texture1 S/T, 8 values per vertex |
| `04h:28h`, second texture set | `0x28F0CC00` | `0x28F0CC00` | **Bit-identical** parameter/default mask |
| Type-04 packet's first parameter `04h:00h`, single | `0x00A80006` | `0x00A80106` | **Different**: H5S2 programs input-declaration count in `[11:8]` |
| Type-04 packet's first parameter `04h:00h`, second texture | `0x00A80008` | `0x00A80208` | **Different**: H5S2 programs three input registers rather than two |
| Input declarations | H6 uses legacy `04h:01h...` FVF attribute ordering | H5S2 `0x010000C2` (Float4/IR0), `0x02000142` (Float2/IR1), plus `0x03000242` (Float2/IR2) for second texture | **Different input setup** |
| Input mask `04h:12h` | Not present in this H6 setup | `0x12000003` (IR0/IR1), or `0x12000007` (IR0/IR1/IR2) | **VX900 H5S2 additional setup** |
| Shared state data | Copies 32 DWORDs from DLL VA `0x100087A0` | Copies the **same** 32 DWORDs from `0x100087A0` | **Exactly shared binary table**, though later state differs |

The difference `0x25F0CC00 ^ 0x25F0C000 = 0x00000C00` sets precisely bits 11 and 10, which the manuals identify as **texture stage 1's S/T coordinates**. This gives two actual VIA-authored **cross-generation textured-triangle modes**, not just a speculative porting model. The H5S2 `04h:00h` values use bitfields `[11:8]` described in VX900's manual as used input-data count minus one: `1` for IR0/IR1, `2` for IR0/IR1/IR2. The last two digits remain 6 or 8, matching the attribute count.

**Source anchors:** H6 branch and emit `0x10064BA8–0x10064C50`; H5S2 `0x10065B32–0x10065BEF`. The Type-04 declaration, cycle, masks and shared table load are visible in both. VX900 Part II PDF pages 160–161, 165, 166–168; VX855 Part II PDF pages 155–162. Cross-checked by `_ce_add_blit54.py` (no hardware access).

##### 54.2 What really differs between the *blit* paths

The critical H6 versus H5S2 differences in this code are **the input setup and surrounding packets**, not the shared primitive/shading/attribute-mask commands. H5S2 additionally emits:

- Float4/Float2 input declarations (and an extra Float2 for the second texture) plus `04h:12h` input-register enable mask.
- In one branch, parameter `04h:60h–62h` and a Type-03 subtype `31h` table which the public VX900 manual does not fully describe.
- The H5S2-specific `VX900_WAITPATCH` through later setup/submission paths, documented in §46–47.

**An important trap:** `Texture3D_H5` (VX800/VX855) selected a different eight-word layout with transmitted C0/C1 and full vertex cycle. That represents *a different CE function*, not proof that VX855 lacks six-word vertices or fast cycles. Correspondingly, VX900's `Texture3D_H5S2` chooses a six-word path. The two helpers should be compared **within each operation**, not used as universal architectural labels.

##### 54.3 Driver recommendation updated

Implement a shared **logical** Type-04 primitive-state emitter for these documented CE triangle modes (`20h`, `23h`, `24h`, `25h`, `28h`), but dispatch separate FVF/input-declaration and command transport setup for H6 and H5S2. For VX855, **fast vertex cycle is already used in VIA's own `fn3DBlt_H6`**; a targeted regression/performance test can compare that mode to full-cycle triangles under identical vertex data. Keep VX900-only undocumented packets excluded from the VX855 backend.

**Unresolved:** whether the two CE blit paths produce pixel-identical results; full behavior of auto-cycle state reset across draws; whether this carries over to Windows 98 Direct3D/OpenGL, and the real purpose of reserved VX900 Type-04 words.

<a id="c9-section-56"></a>

#### 56. VX900 H5S2 Type-03 `31h` upload: two- and three-instruction candidate programs

The VX900-selected `fn3DBlt_H5S2` adds a Type-03 subtype `31h` upload before the shared pixel-shader program (§55). The H6 `fn3DBlt_H6` has no equivalent Type-03 subtype-`31h` upload in the compared block.

| H5S2 branch | Source VA | DWORDs | Packet fields | Associated vertex/input setup |
|---|---|---:|---|---|
| Single-texture, no global multi-blend | `0x1000870C` | 8 | `FE03` header, `0x31030000` selector, length `8` | `04h:00h=0x00A80106`, IR0 Float4 + IR1 Float2, mask `0x12000003`, `04h:60/61/62 = 0x60800001/0x61800000/0x62104001` |
| Dual-texture OR global multi-blend | `0x1000872C` | 12 | `FE03` header, `0x31030000` selector, length `12` | `04h:00h=0x00A80208`, IR0 Float4 + IR1/IR2 Float2, mask `0x12000007`, `04h:60/61/62 = 0x60800002/0x61800000/0x62204002` |

The mode branch is `([input+0x1C] & 0x20) != 0 || bGdiMultiABlend != 0`. Anchors are `0x10065BFA–0x10065CE3`; the program selectors occur at `0x10065C60` and `0x10065CCD`. Both tables are stored consecutively in the retail DLL's read-only `.text` section.

**Exact data**, in four-DWORD rows, from `0x1000870C` (8 words):

```text
0008014C 00000000 00000000 0008021F
00084124 00000000 00000000 00080283
```

From `0x1000872C` (12 words):

```text
0008014C 00000000 00000000 0008021F
00084124 00000000 00000000 00080283
00088124 00000000 00000000 00080293
```

The 12-word variant **starts with precisely the complete eight-word variant**, adding one four-DWORD group. The VX900 Part II command-regulator overview names Type-03 subtype `31h` *Vertex Shader Instruction*, so a program consisting of **two versus three 128-bit instructions** is the leading interpretation. However, the detailed VX900 register section, printed page 153 (PDF page 159), calls `31h–33h` reserved; it does not publish the VS instruction encoding. Neither program semantics nor actual executed-instruction count can be proven from these static bytes alone. Hardware performance counters may eventually help validate this hypothesis.

**Correlation, not decoded bitfields:** The first and third `04h:60–62h` packet words vary with one versus two texture-coordinate sets, while the middle word remains `0x61800000`. This is a promising clue for identifying undocumented vertex-input or shader-output controls, but **it is not a sound basis for assigning bitfield names** or writing these values on VX855. There is no full public definition of `04h:60h–62h` (reserved on VX900's detailed page 179).

<a id="c9-section-57"></a>

#### 57. Extent of VX900 coordinate-bias use, and refined porting boundary

A whole-file x86 disassembly scan of the **retail CE DLL** finds two references to the stored double-precision constant `786432.0` (`VA 0x10008928`) inside the targeted CE 3D family:

| Caller | Reference VA | Platform selected in CE driver | Notes |
|---|---|---|---|
| `fn3DBlt_H5S2` | `0x10065E39` | VX900 | Applies bias while constructing the **blit** vertex coordinates |
| `Texture3D_H5S2` | `0x10067E19` | VX900 | Applies bias to six positional values used in the **texture** path (see §§50,52) |

The compared `fn3DBlt_H6` and `Texture3D_H5` code blocks do not reference this constant. This supports a **consistent H5S2-side subpixel-coordinate preparation pattern across two separate CE operations**, not merely an accident in one texture helper. The observed float32 encoding can represent a 1/16-pixel increment for coordinates near zero, but the GPU's actual interpretation of the biased values remains unresolved. The bias should **not** be added to the VX800/VX855 vertex path without a controlled pixel-output comparison.

##### 57.1 Actual compatibility boundary after §§52–57

| Component of these CE 3D blits | VX800/VX855 H6 vs. VX900 H5S2 | Evidence status |
|---|---|---|
| `04h:20/23/24/25/28` primitive state; 6/8 data words | Shared exact programming words | Verified compiled code (§54) |
| Render-target and texture base, format, pitch, dimensions | Same calculation for the documented subset | Verified compiled code (§48, §50) |
| `03h:20/21/22` pixel-shader data | Same program and parameter rules | Verified compiled code (§55) |
| `04h:00/01/02/03/12` input declarations | Different encoding / extra declarations | Verified compiled code + VX900 manual (§54) |
| `03h:31` vertex-shader candidate | H5S2 uploads extra 8/12 DWORDs | Verified upload; exact VS semantics **unverified** (§56) |
| `04h:60–62` extra settings | H5S2-only in examined paths | Verified writes; bitfield meanings **unknown** (§56) |
| `786432.0` coordinate bias | Two H5S2 paths, absent from compared H5/H6 | Verified references; precise GPU behavior **unverified** |
| Additional wait patch and submission | H5S2-specific in CE | Verified emitted packets; hardware wait effect **unverified** (§46–47) |

**Practical recommendation:** Implement a shared programmatic renderer-state builder for the verified commands, a separate VX900 vertex declaration/shader-load layer, and a transport/synchronization layer chosen by device. To confirm the VX900 shader hypothesis, capture and compare one- and two-texture CE output or trace the exact instruction stream and performance counters on real hardware. No source-only inference here validates arbitrary Windows 98 game primitives or 3D hardware limits.

##### 57.2 Reproducibility and reference material

- Retail DLL: `/mnt/data/_via_ce_named/_A8D5429F14D64818A49E42E01B34E9D8___DDI_VIA.dll`, SHA-256 noted in §55, PE image base `0x10000000`.
- Local static verification: `_ce_verify_shader_55_57.py`, which checks byte-for-byte program data, both binary emitter branches, both H5S2 references to the bias constant, and the manual's conflicting subtype descriptions. **The verification script reads bytes only; it does not invoke the CE driver or write GPU registers.**
- VIA VX855 Part II 3D manual: printed page 147 (the pixel-shader register families).
- VIA VX900 Part II 3D manual: printed page 153 (reserved detail), and command-regulator overview (subtype names).

<a id="c9-section-58"></a>

#### 58. Cross-operation reuse of the VX900 H5S2 vertex-shader candidate (retail CE DLL; 2026-11-06)

The previous comparison established that `fn3DBlt_H5S2` (VX900-selected CE path) uploads an 8-DWORD Type-03 subtype-`31h` table for one texture and a 12-DWORD table when a second texture/blend path is selected. **New cross-check:** `Texture3D_H5S2` independently constructs the **same eight words inline**, in the same order, rather than referring to the shared read-only table. Thus the purported two-instruction VS program is reused by *two different VX900 CE operations*, not just by one blit function.

| Program location and operation | Data source | Exact sequence | Status |
|---|---|---|---|
| `fn3DBlt_H5S2`, single-texture branch | Copy 8 DWORDs from VA `0x1000870C` at `0x10065C6E–0x10065CE3` | `0008014C 00000000 00000000 0008021F / 00084124 00000000 00000000 00080283` | Verified |
| `Texture3D_H5S2`, inline command builder | Writes at `0x100681D8–0x10068206` | **Identical** four nonzero words and zero words at identical positions | Verified |
| `fn3DBlt_H5S2`, second-texture/blend branch | Copy 12 DWORDs from VA `0x1000872C` | Same 8-word prefix plus `00088124 00000000 00000000 00080293` | Verified |

The last four words differ from instruction group 2 (the second group of the eight-word program) in two relevant nonzero positions: `0x00084124 → 0x00088124` (XOR `0x0000C000`), and `0x00080283 → 0x00080293` (XOR `0x00000010`). This **correlates** with a third VS instruction / extra texture-coordinate set; the meaning of those bits is **not** defined in the public manual. In particular, the same register group `04h:60–62h = 60800001 / 61800000 / 62104001` is emitted by the single-texture `fn3DBlt_H5S2` and the `Texture3D_H5S2` helper.

The VX900 Part II manual's overview calls Type-03 subtype `31h` *Vertex Shader Instruction*, but its detailed descriptions reserve the subtype. **Conclusion limited to static code:** identical upload data and destination in two VX900 paths. No validated instruction disassembly, hardware instruction execution count, or general shader-ISA compatibility is claimed.

<a id="c9-section-59"></a>

#### 59. The 786432.0 clipping-to-screen offset is shared, not VX900-exclusive

The VIA manuals describe Type-04 `40h–48h` as the **clipping-window-to-screen-window transform**. A 32-bit float is split between the lower 24 bits at the base subaddress and the upper 8 bits in the combined third word (`42h` for X, `45h` for Y, `48h` for Z). This lets us decode actual compiled CE *register parameters*, not just recognize the numerical `786432.0` constant in x87 software.

**Shared data:** Both `Texture3D_H5` (`0x1006798F–0x100679F5`) and `Texture3D_H5S2` (`0x100682EC–0x10068352`) emit the following Type-04 transform values (their `47h` value is separately compared in §60). Both `fn3DBlt_H6` and `fn3DBlt_H5S2` copy the **same** 32-DWORD table at VA `0x100087A0`, containing the equivalent X/Y transform values.

| Type-04 parameters | Raw values used in both texture paths | Reassembled IEEE-754 float |
|---|---|---|
| `40h` + `42h[7:0]` | `0x40800000` + low byte `0x3F` of `0x4200493F` | X scale = `0x3F800000` = **1.0** |
| `41h` + `42h[15:8]` | `0x41400000` + byte `0x49` of `0x4200493F` | X offset = `0x49400000` = **786432.0** |
| `43h` + `45h[7:0]` | `0x43800000` + low byte `0x3F` of `0x4500493F` | Y scale = **1.0** |
| `44h` + `45h[15:8]` | `0x44400000` + byte `0x49` of `0x4500493F` | Y offset = **786432.0** |
| `46h` + `48h[7:0]` | `0x46800000` + low byte `0x3F` of `0x4800003F` | Z scale = **1.0** |

These values are **bit-identical** in the compared H5 and H5S2 texture helpers and in the shared blit table. The 32-DWORD blit table further sets `04h:47h=0x47000000`, yielding a Z offset of **0.0** on both chipsets. The table also includes `04h:50h=0x50000000`, `51h=0x51000000`, and `52h=0x52147FFF` for the clipping-coordinate bounds and exponent threshold. In the published register layout, `52h` encodes upper/lower clamp high bytes `0x7F/0xFF` and an exponent threshold byte `0x14`. This is a **documented representation**; it is not evidence that clipping was enabled or exercised by the CE operation.

**Important reinterpretation of §§50, 52 and 57:** Although only the two examined VX900 H5S2 functions contain an explicit *software* addition of `786432.0` to vertex X/Y fields, the older H5 path already programs an **X/Y C-to-S offset of 786432.0 in hardware state**. The hardware transform and software arithmetic occur at different stages. Whether both stages run for any particular vertex is determined by additional shader/clip/coordinate-mode state and cannot be concluded from the numerical equality alone. Do **not** assume either that VX900 adds the bias twice or that the hardware will automatically apply it to H6/H5 data.

**Primary manual sources:** VX800 Part II, Type-04 `40h–52h` table; VX855 Part II, printed pp. 164–166; VX900 Part II, printed pp. 171–173. **Binary:** VA `0x100087A0` (32-DWORD shared blit table), the two texture-helper immediate-write ranges above. See verification script supplied with the updated report.

<a id="c9-section-60"></a>

#### 60. `04h:47h` Z-offset discrepancy is specific to the CE texture operation

The two texture helpers share all 13 immediate Type-04 words from `40h` through `52h` except **one**:

| Operation | Type-04 `47h` word | Reassembled Z-offset float | Meaning |
|---|---|---|---|
| `Texture3D_H5` on VX800/VX855 | `0x47400000` | `0x00400000` ≈ `5.8774717541 × 10^-39` | Nonzero **subnormal** float if interpreted per the published transform format |
| `Texture3D_H5S2` on VX900 | `0x47000000` | `0x00000000` = 0.0 | Zero offset |
| `fn3DBlt_H6` **and** `fn3DBlt_H5S2` | Shared table contains `0x47000000` | 0.0 | **Both generations use zero** for this separate blit operation |

`04h:48h = 0x4800003F` supplies the high byte of the Z offset, zero in all these cases, and the scale high byte `0x3F`. The difference is exactly `0x00400000` in the reconstructed Z-offset float. It is **not** appropriate to attribute this to a general VX800/VX855 versus VX900 hardware change, because the VX800/VX855 H6 blit also uses zero.

The nonzero H5 texture word could be deliberate, an unimportant numerical bias, or a VIA programming oversight. The binary and manuals cannot establish which. The value should **not** be propagated into an ordinary VX855 Z-clear or 3D viewport path without an isolated test.

<a id="c9-section-63"></a>

#### 63. The extra H5S2 VS candidate precisely tracks input register 2

The H5S2 subtype-`31h` 12-DWORD table is the 8-DWORD table plus one four-DWORD group. That extra group is **not arbitrary data**: it is selected together with a third Float2 input declaration and the corresponding input-register-enable bit.

| H5S2 blit mode | Relevant Type-04 declaration words | VS candidate upload |
|---|---|---|
| First texture only | `04h:00=0x00A80106`, `04h:01=0x010000C2`, `04h:02=0x02000142`, `04h:12=0x12000003` | 2 groups (8 DWORDs) |
| Additional texture coordinates | `04h:00=0x00A80208`, same first two declarations, `04h:03=0x03000242`, `04h:12=0x12000007` | 3 groups (12 DWORDs) |

The VX900 Part II **Type-04 `01h` and `02h`** documented format fields describe `0xC2` as a four-component floating-point declaration and `0x42` as a two-component floating-point declaration. The two rows therefore describe Float4 **input register 0**, then Float2 **input register 1**, with Float2 **input register 2** added in the extended mode. The input-enable mask changes from `0x3` to `0x7`. This is consistent with the emitted six- versus eight-DWORD vertex data seen in §§52–54.

**Exact added 128-bit program candidate:**

```text
Second instruction (already present): 00084124 00000000 00000000 00080283
Third instruction (extended mode):   00088124 00000000 00000000 00080293
XOR for word 0:                      0000C000
XOR for word 3:                      00000010
```

The two instruction groups have almost identical bit patterns, and the differences correlate with the additional input register and texture-coordinate output. This is **strong circumstantial evidence of a repeated coordinate-handling operation** with different register indexes. However, the supplied VX900 detailed register manual does not define the vertex-shader instruction ISA. Consequently we **cannot** assign an official opcode mnemonic, verify exact source/destination selectors, or conclude the VS performs lighting, matrix transforms, or other arithmetic from these words alone.

**Implication for a port:** Keep a common *logical* declaration generator for position and texture-coordinate streams, then use a VX900-specific Type-04 format mapper and Type-03 subtype-`31h` instruction upload. Never assume the H6 direct-input declaration layout can encode the same fields on VX900.

<a id="c9-section-64"></a>

#### 64. Vertex default-state decoding: a shared 3D-blit table explains omitted colors

The retail binary contains one **shared 32-DWORD** Type-04 state table at VA `0x100087A0` (also analyzed in §59). It is copied into **both** `fn3DBlt_H6` at `0x10064C4B` and `fn3DBlt_H5S2` at `0x10065BEF`. Three particularly informative entries are:

| Type-04 address | Exact full command | VIA manual meaning | Decoded value |
|---|---|---|---|
| `2Ah` | `0x2A3F8000` | Default point width as signed `s[8].15` | `0x3F8000 / 32768 = 127.0` |
| `2Bh` | `0x2B3F8000` | Default point height as signed `s[8].15` | `127.0` |
| `2Fh` | `0x2F582000` | Default unspecified vertex attributes | Bits **22, 20, 19, 13** set: **W=1, Color0=1, Alpha0=1, texture Q=1** |

The same full commands appear **inline** in `Texture3D_H5S2` at `0x100682D4–0x100682EA`. They are *not* inline in the compared `Texture3D_H5` routine. But because the H6 3D-blit backend already uses the identical shared 32-DWORD table, **these commands are not VX900-exclusive hardware commands**. Their appearance is **operation-path-specific**.

Two nuances matter:

- `0x3F8000` is **not** IEEE-754 `1.0` in this register. The manual specifies signed fixed-point `s[8].15`, so its documented numerical interpretation is **127.0**. These point-size defaults appear even though the CE blit issues triangles; they may simply be part of a robust generic initial-state template. Do not infer active point-sprite rendering from their presence.
- The default state in `04h:2Fh` provides unity W, color and alpha where vertex attributes are omitted. This is **consistent** with H5S2's `0x25F0C000` six-DWORD vertex mask excluding per-vertex colors. It means the driver need not obtain every color through a custom VS instruction. Other fixed color/state values and pixel-shader behavior still need to be considered; an exact final color cannot be reconstructed from this default register alone.

**Manual corroboration:** `vx855_2.pdf` Type-04 `2Ah`, `2Bh`, `2Fh` (printed pages 162–163); `vx900_2.pdf` Type-04 same addresses (printed pages 169–170). The field meanings match in the examined excerpts. The shared compiled data is firmer cross-generation evidence than guessing from the table name.

##### Updated conclusion and follow-on test plan

**Established:** CE 3D blits independently choose vertex attribute count and pixel-shader program length; the H5S2 extra candidate VS instruction follows the added second texture-coordinate input; both families also initialize default W, color, alpha and texture-Q state. **Not established:** complete VX900 VS ISA, exact stage-to-stage value propagation, pixel equivalence of CE output, a higher hardware feature level, or that VX900 transfers specific ALU operations out of the pixel stage.

The next useful static step is to reconstruct **the exact Type-03 `31h` instruction operand fields and Type-04 `60h–62h` destination/output mapping** using at least one more distinct VX900 shader program (e.g. another CE or XP driver). The current two variants alone provide only a small set of correlated bit differences. If hardware becomes available, test same-texture and two-texture rectangles with independent position, UV and color variation, and compare pixels, counters and hangs before changing the Windows 98 driver.

**Reproducibility:** `Chrome9_CE_Command_Verification_62_64.py` reads the same SHA-256-checked retail CE DLL offline, validates the branch anchors, shader data addresses, VS table prefix/delta, vertex-default commands and manual fixed-point/bitmask decoding. No binary was executed or register written.

<a id="c9-section-68"></a>

#### 68. Boundaries of the recovered VX900 VS programs and Type-04 `60–62h` commands

A complete byte-level scan of the **retail DLL `.text` section** (not just known function intervals) found **exactly three literal `0x31030000` stores**, the FE03 selector for Type-03 subtype `31h`:

| Full-word selector location | Code path | Upload length |
|---|---|---:|
| `0x10065C62` | H5S2 blit, single-coordinate mode | 8 DWORDs |
| `0x10065CCF` | H5S2 blit, second-coordinate mode | 12 DWORDs |
| `0x100681CE` | H5S2 texture helper | 8 DWORDs |

This establishes the limit of **statically obvious inline subtype-31 uploads in this particular retail image**. It does *not* prove there are no indirect, computed, or data-driven shader uploads elsewhere; the existing binaries still provide only **two distinct candidate VS programs** from these call sites. The H5S2 texture helper's eight words match the eight-word blit variant. Consequently, the current package still does **not** support a full VX900 VS instruction disassembler or an assertion about hardware transform-and-lighting instruction coverage.

The related **Type-04 `60h/61h/62h` values** were also enumerated in the entire retail `.text` section:

| Type-04 words | Single texture/2 input regs | Dual texture/3 input regs |
|---|---|---|
| `60h` | `60800001` | `60800002` |
| `61h` | `61800000` | `61800000` |
| `62h` | `62104001` | `62204002` |
| Occurrences | Blit and H5S2 texture helper | Extended blit only |

`60h` differs by `0x00000003` (the two low values 1 and 2, so XOR is 3); `62h` differs by `0x00300003` (XOR between `0x104001` and `0x204002`); `61h` is unchanged. These differences are **correlations with the vertex input count and VS upload length**, not decoded hardware operand fields. The VX900 Part II manual (PDF p.179, printed p.173) explicitly labels **Type-04 `53h–62h` reserved**, as confirmed against its page rendering. Therefore there is no published authoritative bitfield recipe for `60h–62h` in the supplied VIA documents. The presence of the fields in VIA's CE driver is evidence that the *actual silicon/programming stack* has an additional interface, not evidence of what each bit does.

**Porting implication and next evidence requirement:** Keep a small VX900-specific vertex-declaration, candidate-VS upload, and `60h–62h` backend, while retaining shared Type-04 triangle masks, common Type-03 pixel programs, common dynamic alpha/PS-constant handling, and shared texture/target packing. To go further with the **VX900 VS ISA**, obtain a *different* compiled VX900 shader program from a CE or XP graphics driver, or a VX900-specific hardware trace; two correlated instruction variants cannot uniquely identify an instruction format. No undocumented `60h–62h` words should be emitted on VX855 merely because related Type-04 commands are shared.

**Independent static checks:** `Chrome9_CE_Command_Verification_65_68.py` verifies the retail DLL SHA-256, x87 `255.0` operand, branch anchors, same float-to-fixed helper, Type-03/22 constant upload, all three subtype-31 selector stores, and all known `60h–62h` immediate variants. It does not run the DLL or write GPU registers.

<a id="c9-section-79"></a>

#### 79. Exact legacy FVF versus VX900 input-descriptor decode from compiled words

The common six-DWORD vertex *data* payload is declared through **different register layouts**. The CE driver writes these literals within the comparable 3D blit functions:

| Hardware register | VX800/VX855 H6 | VX900 H5S2 | Source addresses |
|---|---|---|---|
| Type-04 `01h` | `01020100` | `010000C2` | H6 `0x10064C12`, H5S2 `0x10065BAA` |
| Type-04 `02h` | `02090803` | `02000142` | H6 `0x10064C1A`, H5S2 `0x10065BB2` |
| Type-04 `20h` | `201C5C00` | **identical** | `0x10064C26` / `0x10065BC5` |
| Type-04 `23h` | `23002E02` | **identical** | `0x10064C2E` / `0x10065BCD` |
| Type-04 `24h` | `24000003` | **identical** | `0x10064C36` / `0x10065BD5` |
| Shared subsequent Type-04 state | 32 DWORDs from `0x100087A0` | **same 32 DWORDs** | MOV source at `0x10064C4B` / `0x10065BEA` |

**Manual-based decode of the first two words:**

- **VX855 (and legacy VX800 FVF model):** In `04h:01h` bits `[5:0]`, `[13:8]`, and `[21:16]` enumerate the first three scalar attributes. `01020100` therefore means **X, Y, Z** (attribute numbers 0, 1, 2). `04h:02h = 02090803` specifies **W, Texture-A S, Texture-A T** (attribute numbers 3, 8, 9). Source: `vx855_2.pdf`, printed p.149, Type-04 `01h–02h`.
- **VX900:** In `04h:01h–02h`, fields `[23:20]`, `[19:12]`, `[11:8]`, and `[7:0]` encode vertex-buffer ID, 4-byte-unit offset, input-register number, and data format. `010000C2` means **buffer 0, offset 0, IR0, Float4** (`0xC2`: 4 dimensions and 32-bit floating format). `02000142` means **buffer 0, offset 0, IR1, Float2** (`0x42`: 2 dimensions, 32-bit floating format). Source: `vx900_2.pdf`, printed pp.154–156, Type-04 `01h–02h`.

Thus the **same six raw vertex DWORDs** can represent `(X,Y,Z,W,S,T)` in both CE blit backends, while the **declaration words are not binary compatible**. This supports sharing the *payload packer* for this case, but not sharing the *register declaration packer* unconditionally. It does **not** prove every other primitive or vertex format is shared.

<a id="c9-section-80"></a>

#### 80. VX900 input-count and VS-upload island in the normalized command stream

The H5S2 path changes its Type-04 vertex-descriptor set according to whether a second texture input is required. **Both the operation flag `BYTE[argument+0x1C] & 0x20` and the shared blend-state global are checked** at `0x10065BFA–0x10065C07`; either condition chooses the longer declaration / candidate vertex-shader path.

| VX900 mode | Type-04 `00h` value | Sum of input DWORDs: bits `[6:0]` | `HVBDNum`: bits `[11:8]` (number of used inputs minus one) | Type-04 `12h` input mask | Candidate VS upload |
|---|---|---:|---:|---|---|
| Short | `00A80106` | 6 | 1 → **2 inputs** | `12000003` (IR0/IR1) | Type-03 subtype `31h`, **8 DWORDs** from `0x1000870C` |
| Extended | `00A80208` | 8 | 2 → **3 inputs** | `12000007` (IR0/IR1/IR2) | Type-03 subtype `31h`, **12 DWORDs** from `0x1000872C` |

The extended path additionally supplies `04h:03h=03000242`: VB0, offset 0, **IR2, Float2**. This predicts eight scalar float words per vertex (Float4 + Float2 + Float2), matching the corresponding expanded vertex-attribute mask `0x25F0CC00` previously recovered. The short path uses Float4 + Float2 and `0x25F0C000` (six words). The Type-04 `00h` sum and IR count **independently agree with the recovered vertex-word counts**, making these a stronger static validation of the interface than merely reading the command names.

A single operation's **logical setup order** is now recoverable (conditional branches mean this is a *stage map*, not a byte-for-byte runtime trace):

1. **Both:** choose the common 50-/37-word Type-01 state template and 12-word pixel-shader-control table.
2. **Both:** emit common texture/target parameters, then Type-04 primitive control words `201C5C00`, `23002E02`, `24000003`, and the same 32-word Type-04 state table.
3. **H6:** use legacy scalar FVF declarations (`01020100`, `02090803`). **H5S2:** use descriptor declarations (`010000C2`, `02000142`, optionally `03000242`).
4. **H5S2 only in this comparison:** program the undocumented Type-04 `60h–62h` literal variants (short: `60800001/61800000/62104001`; extended: `60800002/61800000/62204002`), followed by a Type-03 `31h` 8-/12-DWORD candidate VS upload. These writes appear **before** the common Type-03 pixel-shader program upload. There is no equivalent literal `60h–62h`/subtype-`31h` step in this examined H6 section.
5. **Both:** emit Type-03 subtype `20h` and `21h` pixel-shader instructions, and conditional subtype `22h` dynamic constants. Other generation-specific synchronization/patch packets still exist elsewhere in H5S2.

This order indicates the minimum observed H5S2-specific configuration **is not a new pixel-shader language in these blits**: the extra work is visibly concentrated in input-register declarations, candidate VS upload, undocumented controls and waits. Yet the VX900 public manual labels Type-04 `60h–62h` and Type-03 subtype `31h` detailed registers *reserved*, so their exact silicon semantics remain unresolved; this is an evidence-based software boundary, **not** a safe cut-and-paste hardware recipe.

<a id="c9-section-84"></a>

#### 84. Narrower interpretation of VX900 Type-04 `60h–62h` values

Cross-checked the VX900-specific H5S2 3D blit function against `Texture3D_H5S2` in the **same retail binary**. The latter emits the **short** `60h/61h/62h` control group at `0x10068354`, `0x1006835C`, and `0x10068364`. The 3D blit emits that same group at `0x10065C30/38/40`, or its **extended** version at `0x10065C9D/A5/AD`.

| Type-04 address | One input-coordinate pair / short candidate VS | Two coordinate pairs / extended candidate VS | Bit-level observation |
|---|---|---|---|
| `60h` | `0x60800001` | `0x60800002` | Payload bit 23 constant; low one-hot bit moves from 0 to 1 |
| `61h` | `0x61800000` | `0x61800000` | Payload bit 23 constant; **no difference** |
| `62h` | `0x62104001` | `0x62204002` | Payload bit 14 constant; one-hot bits 20/0 become 21/1 |
| Type-03 `31h` upload | 8 DWORDs | 12 DWORDs | Candidate VS program grows from two to three 4-DWORD groups |
| Type-04 `12h` IR mask | `0x12000003` | `0x12000007` | Documented IR0/IR1 versus IR0/IR1/IR2 |

The short values' **reuse in both independent VX900-selected CE operations** establishes a stable programming pattern, not merely one isolated packet. The one-hot correlation makes an input/output routing or attribute-enable role **plausible**, but cannot uniquely identify these bits. The supplied VX900 Part II manual **explicitly marks Type-04 addresses `53h–62h` reserved** (printed page 173, PDF page 179). No name, safe substitute value, or comprehensive field mask is established for `60h–62h` from these documents.

Do not copy these undocumented values to VX855. Do not assume toggling one bit alone corresponds to adding a texture coordinate: the driver changes the **vertex declaration, IR mask, attribute mask, VS program, and these controls together**.

<a id="c9-section-102"></a>

#### 102. Verified H6/H5S2 Type-04 `00h` common high bits: the NaN test modes are shared

**Sources:** the user's retail `DDI_VIA.dll` (SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`); disassembly `fn3DBlt_H6` at `0x10064BBD/0x10064BCE` and `fn3DBlt_H5S2` at `0x10065B47/0x10065B5F`; VIA VX855 Part II *3D Registers*, Type-04 `00h` (printed p.148); VIA VX900 Part II, Type-04 `00h` (printed p.154). **Observed:** both backend pairs use `0x00A8xxxx` for Type-04 register `00h`.

| CE 3D-blit variant | VX800/VX855 H6 (`04h:00h`) | VX900 H5S2 (`04h:00h`) | Difference |
|---|---|---|---|
| One texture-coordinate set (6 DWORDs/vertex) | `00A80006` | `00A80106` | VX900 sets `[11:8]=1` (two vertex-data/IR entries) |
| Two texture-coordinate sets (8 DWORDs/vertex) | `00A80008` | `00A80208` | VX900 sets `[11:8]=2` (three vertex-data/IR entries) |

The **high bits** `0x00A80000` are identical in all four words. Both manuals define `04h:00h[23:22]`, `[21:20]`, and `[19:18]` as 2-bit X/Y, Z, and W NaN-test modes. All three fields decode to `10b` in the observed words, selecting the documented **discard associated primitive *list* if a NaN is detected** option. This is a real shared register encoding, and should be retained when constructing family-specific declarations.

**Semantic qualification:** the VX855 manual describes the flexible-vertex-format X/Y/Z/W values, whereas the VX900 manual explicitly says to test the vertex shader's **`oPos`** values. The same bit setting need not produce exactly the same observable effect if a future shader program changes coordinates; do not conclude identical silicon behavior beyond the documented register selection.

The low length field is also numerically common in these modes (`6` / `8` DWORDs), **but the register layout is not fully identical**: VX855 uses FVF vertex length `[5:0]`; VX900 uses the summed VB-data length `[6:0]` and a separate used-input count `[11:8]`. Consequently, reusing the *entire* H6 `04h:00h` word on VX900 loses the input-count setup even for the same raw 6-/8-word primitive data.

<a id="c9-section-103"></a>

#### 103. VX900 Type-04 input declarations and `12h` independently constrain payload size

For the same two modes, the retail H5S2 renderer selects:

| Parameter | One texture | Two texture-coordinate sets | VIA-documented interpretation |
|---|---|---|---|
| `04h:00h` | `00A80106` | `00A80208` | 2 or 3 vertex data/VS input records; 6 or 8 DWORDs total |
| `04h:01h` | `010000C2` | `010000C2` | VB0, 0-byte offset, input IR0, Float4 |
| `04h:02h` | `02000142` | `02000142` | VB0, 0-byte offset, input IR1, Float2 |
| `04h:03h` | not required in shorter declaration | `03000242` | VB0, 0-byte offset, IR2, Float2 |
| `04h:12h` | `12000003` | `12000007` | IR0–IR1 or IR0–IR2 sourced from VB data |
| Subtype-`31h` program | 8 DWORDs | 12 DWORDs | Two versus three four-DWORD groups (likely VS instructions; coding not published) |

**Source:** H5S2 declaration branch at `0x10065B38–0x10065B82`, and the single-texture `Texture3D_H5S2` literals at `0x1006828C–0x100682AC`. VX900 manual printed pp.154–159 specifies formats, input registers, and `04h:12h`. All decoded input descriptions match the raw vertex sizes. This provides **two independent checks** of the six/eight DWORD layout: declared component dimensions (`4+2`/`4+2+2`) and the Type-04 `00h` total-length field.

**Important cross-generation reservation:** VX855's manual marks its Type-04 `11h–1Fh` *reserved*, while VX900 defines `04h:12h` as the input-register source mask. A word with high byte `12h` is **not automatically** a VX900 input-register command unless the current HParaType really is `04h`. The CE H6/ H5S2 routines also construct words like `12000000` for other state groups; context, not the sub-address alone, determines register semantics.

<a id="c9-section-104"></a>

#### 104. Exact bit changes in undocumented VX900 `04h:60h–62h` are *one-hot-like*, not decoded counts

**Source:** `fn3DBlt_H5S2`, VA `0x10065C14–0x10065C46` (short) and `0x10065C81–0x10065CB3` (extended); `Texture3D_H5S2`, VA `0x10068354–0x10068364` (short). Each packet explicitly selects HParaType `04h` via `FE02` before writing this register group; the commands are not Type-10 command-regulator initialization (see §101).

| Type-04 address | One texture / two input IRs | Two textures / three input IRs | Exact bit pattern |
|---|---|---|---|
| `60h` | `60800001` | `60800002` | Fixed payload bit 23; variable bit **0 → 1** |
| `61h` | `61800000` | `61800000` | Fixed payload bit 23 |
| `62h` | `62104001` | `62204002` | Fixed payload bit 14; variable bits **20 → 21** and **0 → 1** |

The changing portions are consistent with **one-hot-selected lanes or routing slots**, rather than a simple binary count: the lower selection shifts from `0b01` to `0b10`, with another selection moving from bit 20 to bit 21. This is **pattern analysis only**, not a field-name or an interpretation of bit 23/14. In particular, don't assert that `60h` is an enable mask, or `62h` is a VS output selector, without another source or hardware test. The supplied VX900 manual explicitly marks Type-04 `53h–62h` **reserved** (printed p.173), preventing an authoritative public-register decode.

These three registers always change in step with a known other change: (a) the input mask `03h` → `07h`, (b) the third Float2 declaration, and (c) the 8-DWORD → 12-DWORD subtype-`31h` program. The present CE package exhibits **two configurations**, so there are not enough independent variations to determine which undocumented register encodes which function. The short sequence is emitted by both H5S2 texture and blit helpers; the longer one is observed in the blit helper.

<a id="c9-section-105"></a>

#### 105. Verified 4-DWORD delta in candidate VX900 vertex-shader program

**Source:** 8-DWORD program at `DDI_VIA.dll` VA `0x1000870C` and 12-DWORD program at `0x1000872C`. The short sequence is an **exact prefix** of the longer sequence; both have been checked against raw DLL bytes. Their four-DWORD groups are:

```text
Group 0: 0008014C 00000000 00000000 0008021F
Group 1: 00084124 00000000 00000000 00080283
Group 2: 00088124 00000000 00000000 00080293   (extended mode only)
```

Comparing **group 1 with group 2**:

- DWORD 0 changes `00084124 → 00088124` (XOR `0000C000`, bits 14 and 15).
- DWORD 3 changes `00080283 → 00080293` (XOR `00000010`, bit 4).
- DWORDs 1 and 2 remain zero.

The small deltas are consistent with the third group being a similar instruction acting on a different vertex input/output, because that extra group correlates with adding IR2. They **do not prove** which bits mean input selection, output selection, opcode, swizzle, or instruction control. In particular, these three instruction groups do not establish arbitrary hardware T&L, shader compilation, or a full VX900 shader instruction set. This CE driver supplies only these two distinct program variants.

<a id="c9-section-106"></a>

#### 106. Implementation guidance, negative controls, and new offline audit

**Safe static conclusion:** one family-wide primitive-data packing routine can accept six/eight scalar float values, but the **register-declaration backend** must branch: H6 uses legacy scalar FVF attributes; H5S2 uses Float4/Float2 input-register descriptors and a Type-04 `12h` IR mask. The `04h:00h` high NaN-test settings happen to be shared, while the used-input count differs.

**Do not infer:** (i) VX900's Type-04 `60h–62h` meanings solely from two mode examples, (ii) that a Type-10 register `60h` applies to Type-04, (iii) that the same numerical register `12h` means the same thing across HParaTypes, or (iv) that 6-/8-word CE blit data proves every Direct3D/OpenGL vertex declaration is interchangeable.

**Lowest-risk next research:** locate a third independent program that changes *only* an input-register index or a texture-coordinate mode, then compare Type-04 `60h–62h` independently of the shader-length change. A different VIA driver binary or a captured command trace with software-dispatched mode labels would be stronger evidence than guessing reserved-register bits. Until then, treat the two exact known CE recipes as two tightly scoped examples—not generalized chip bring-up code.

**Companion artifacts:**

- `Chrome9_CE_VX900_Vertex_Routing_Compatibility_102_106.csv` — the two recovered CE modes, side-by-side H6/H5S2 words, NaN-test values, input counts, source masks, and unknown routing patterns.
- `Chrome9_CE_VX900_Vertex_Routing_Audit_102_106.py` — read-only verifier against the original retail DLL hash. It checks 17 exact disassembly literals, the shared NaN bits, input counts and payload sizes, the one-hot-like routing patterns, and the 8-/12-DWORD shader table relationship. It does not load or execute the driver and performs no MMIO writes. **All checks passed.**

<a id="c9-section-107"></a>

#### 107. Retail and debug CE builds independently reproduce the same VX900 vertex programs

**Evidence:** The original `VIA Graphics Driver Group2.msi` supplies separately compiled **retail** and **debug** `DDI_VIA.dll` binaries; these are not two copies of one file. The retail SHA-256 is `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`; the debug SHA-256 is `75315317eb76ffed7524e4704bcf356b73bf20076eeaff7bd4f54f10810b0124`. Both are 32-bit x86 PE DLLs. The PDB files also have different SHA-256 values, so the debug build is a useful **second compiler configuration**, not a completely independent hardware implementation.

The same candidate VX900 vertex-shader instruction tables appear at different preferred virtual addresses in the two builds:

| Program | Retail VA | Debug VA | DWORDs | Exact SHA-256 of little-endian words |
|---|---|---|---:|---|
| Short | `0x1000870C` | `0x1000B954` | 8 | `f1452df7d61672a38d55c4d97e33b6ef819f5a95d99dce877b1f9b493dec8e1d` |
| Extended | `0x1000872C` | `0x1000B974` | 12 | `fb04f5d55d48209acb5e768ce3c554381fda3ddaa7c496aa2fdf264fc66f9caf` |

The **bytes match exactly across retail and debug**; the eight-word program remains a prefix of the twelve-word one:

```text
Group 0: 0008014C 00000000 00000000 0008021F
Group 1: 00084124 00000000 00000000 00080283
Group 2: 00088124 00000000 00000000 00080293  (extended only)
```

The Type-03 subtype `31h` designation and four-DWORD grouping are **candidate VS instructions**, supported by the command-regulator overview and compiled driver usage; the public VX900 detailed register listing marks subtype `31h` reserved. Exact opcode, operands and outputs remain **undecoded**. The build-to-build match verifies the **software-emitted program data**, not hardware execution.

<a id="c9-section-108"></a>

#### 108. Debug build proves the Type-04 `60h–62h` words are constructed, not missing

A literal-immediate scan of the retail blit found `0x60800001`, `0x60800002`, `0x61800000`, `0x62104001`, `0x62204002`. The corresponding debug blit does **not** embed those full 32-bit values as immediate instructions. Instead, it initializes three local words to high-byte address selectors and merges the payload with `OR` instructions:

- Debug `fn3DBlt_H5S2` at `0x100D7CB1`, `0x100D7CB8`, `0x100D7CBF` starts with `60000000`, `61000000`, `62000000`.
- Short path `0x100D8FE1–0x100D9001` ORs `00800001`, `00800000`, `00104001` into those words.
- Extended path `0x100D8EA2–0x100D8EC2` ORs `00800002`, `00800000`, `00204002`.
- Both paths select Type `04h` through `FE02` packet setup before streaming the three parameter words.

| Type-04 word | Retail immediate | Independently reconstructed debug result |
|---|---|---|
| Short `60h` | `60800001` | `60000000 \| 00800001 = 60800001` |
| Short `61h` | `61800000` | `61000000 \| 00800000 = 61800000` |
| Short `62h` | `62104001` | `62000000 \| 00104001 = 62104001` |
| Extended `60h` | `60800002` | `60000000 \| 00800002 = 60800002` |
| Extended `61h` | `61800000` | `61000000 \| 00800000 = 61800000` |
| Extended `62h` | `62204002` | `62000000 \| 00204002 = 62204002` |

**Practical lesson:** comparing only embedded 32-bit literals can falsely suggest that a debug or alternate driver lacks a hardware command. Normalize register words after compiler expressions when auditing different builds. This cross-check **does not** decode what Type-04 `60h–62h` physically do; VX900 Part II still labels them reserved.

<a id="c9-section-109"></a>

#### 109. Refined selection predicate: CE flag bit `0x20` **OR** retained global blending state

A consequential qualification to the earlier shorthand “one texture versus two textures” follows from the actual branch condition in both builds:

```c
/* Equivalent branch decision in the examined CE 3D blit implementation. */
use_extended_vertex_path = ((blit_flags & 0x20u) != 0) ||
                           (bGdiMultiABlend != 0);
```

- **Retail:** `test BYTE PTR [ecx+0x1C],0x20` at `0x10065B38`, followed by a test of the global at `0x10065B3E`; a matching check appears before routing/program upload at `0x10065BFA`.
- **Debug:** the same source-field test is expressed as `shr edx,5; and edx,1` at `0x100D8C3F–0x100D8C48` and before the routing packet at `0x100D8E87–0x100D8E9C`; the global is at debug VA `0x10160C8C`.
- The **pixel-shader program selection** is a different condition: H5S2 selects its extended pixel-shader setup when `bGdiMultiABlend` is nonzero (retail at `0x10065CE5`, debug at `0x100D911B`). The flag `0x20` alone does not request that same extra pixel-shader arithmetic in the code path examined.

| `flags & 0x20` | Global `bGdiMultiABlend` | Candidate VX900 VS program | Type-04 `60h–62h` | CE blit pixel-shader setup |
|---|---|---|---|---|
| Clear | 0 | 8 DWORDs (2 groups) | Short (`...01`) | Shorter ALU configuration |
| Set | 0 | 12 DWORDs (3 groups) | Extended (`...02`) | Shorter ALU configuration |
| Clear | Nonzero | 12 DWORDs (3 groups) | Extended (`...02`) | Longer ALU configuration |
| Set | Nonzero | 12 DWORDs (3 groups) | Extended (`...02`) | Longer ALU configuration |

**Interpretation boundary:** Within these CE blits, bit `0x20` correlates with another texture-coordinate input, but the **same extended vertex setup can also be forced by the global blending state**. Earlier tables that labeled the extended path as “two textures” describe a common use case, **not the exclusive selector**. Because the global flag can be retained between invocations (see §§82–90), the emitter's effective mode cannot be predicted from the current operation flags alone. This is a *software state dependency*, not proof that a GPU needs three vertex shader instructions whenever blending occurs.

<a id="c9-section-110"></a>

#### 110. Exhaustion limit: no third *matching-form* VX900 program in either supplied CE build

The short program is 32 bytes and appears exactly **twice** as an eight-DWORD byte sequence in **each** DLL: once as the standalone short program and once as the prefix of the twelve-DWORD extended program. The extended table occurs once per build. The audited selection sites use those same two program variants. This provides evidence **against a hidden third mode of the same table family in these two builds**.

A negative result has important limits: it does not rule out another vertex-shader program using a different first instruction or a dynamically synthesized instruction stream; nor does it establish that the GPU supports only these instructions. Retail and debug are compiled from the *same driver family*, so the second build increases confidence in the observed data but does not supply an independent implementation of the shader ISA.

**Research consequence:** no generalized mapping of `04h:60h–62h` bitfields or VS instruction fields can be established from these two selected variants. A third driver package, shader compiler, wider command capture, or controlled VX900 hardware experiment is still required for that decoding.

<a id="c9-section-111"></a>

#### 111. 3D command compatibility and reproducibility

1. **Shared primitive path remains valid for the scoped blits.** Retain the six/eight-DWORD vertex payload builder and common render-target, texture, pixel-shader, and triangle-state helpers already documented in §§48–106.
2. **Keep VX900 declaration and routing as one mode-dependent operation.** The H5S2 vertex `04h:00h–03h`, input mask `04h:12h`, Type-04 `60h–62h`, and subtype-`31h` VS table change together. Do not mix short declarations with extended program/routing words.
3. **Model effective state, not just a texture count.** The CE extended decision is `(flags & 0x20) || bGdiMultiABlend`; this is particularly important if translating the CE helper into a stateless Windows 98 renderer. Avoid carrying a hidden process-global blend flag across unrelated draws.
4. **Use the branch truth table for further audits.** Four input combinations provide a better static test matrix than merely “one vs two textures.” These tests check software command selection, not the validity of undocumented GPU controls on real hardware.
5. **Do not infer absent commands from literal search alone.** In debug the routing words are assembled via ORs. Compare normalized output words and surrounding Type-04 packet selectors.

**Companion files:** `Chrome9_CE_CrossBuild_VS_Routing_Audit_107_111.py` rehashes both user-supplied DLLs, verifies both 8-/12-DWORD shader tables at their preferred VAs, checks instruction data parity, checks routing-word construction and selection predicates, and exports `Chrome9_CE_Retail_Debug_Vertex_Parity_107_111.csv`. It is read-only, calls `objdump` for disassembly only, and neither loads a CE driver nor writes MMIO. All current checks pass on the two original binaries.

**Source limits:** VX900 Part II, 3D Register chapter, Type-04 description (printed pp.154–159 and reserved `53h–62h` at p.173). The CE group2 `DDI_VIA.dll` binaries supply the instruction and packet evidence; the manuals do not provide a public decode of Type-04 `60h–62h` or subtype-`31h` instructions.

<a id="textures-rendering"></a>

## Textures, pixel programs, depth and stencil

The manuals and binaries identify a shared subset of texture and pixel-program commands. Documented field differences and reserved labels constrain reuse beyond that subset.

### Texture stages, samplers and render targets

<a id="c9-section-374"></a>

#### 374. VX900 XP depth/stencil binding and clear dispatch

The XP front end handles depth/stencil binding separately from render target binding. DP2 opcode 86 reaches display VA `0x3C30B` and calls the depth/stencil binding helper at `0x35EF0`. DP2 `CLEAR` (opcode 42) reaches `0x3A241`, which reads 16 bytes of clear values plus 16 bytes per rectangle and calls `0x36270`. The helper carries the depth and stencil bits (`0x2` and `0x4`) into the request only when both context fields at `+0x3D44` and `+0x3D48` are present. It also clips clear rectangles. [XP callback map](#c9-section-371).

The display call at `0x365CB` uses CIL slot `0xC2E50`, mapped to `s3ginvA.dll` VA `0x1A88E0`. That thunk strips the internal flag `0x08000000` and dispatches through backend offset `+0x20` at `0x1A88FF`. The selected class-4 backend is `0xB05A0`. Its Coarse Z tag-reset packet is confirmed and matches the VX800 XP path. The complete packet sequences for depth-only, stencil-only and combined D24S8 clears and the VX855 failure mechanism remain open. [Cross-build Coarse Z trace](#c9-section-379).

<a id="c9-section-379"></a>

#### 379. VX800 XP depth/stencil clear and Coarse Z tag reset

DP2 `CLEAR` reaches display `0x35250` through `0x391F1`. It carries depth and stencil flags `0x2` and `0x4` through only when both context fields `+0x3D44` and `+0x3D48` are present (`0x3543E–0x35473`). Display slot `0xB7050` at `0x355AB` maps to CIL `0x1A13B0`. The thunk removes private flag `0x08000000`, then dispatches via offset `+0x20`. The class-4 clear table has `0xAA660` at `0x1F98F8`. [XP callback map](#c9-section-376).

The backend's 24-bit depth packing branch at `0xAA9AE–0xAA9DC` shifts the converted Z value left by 8 and sets a depth write mask of `0xFFFFFF00`. The 32-bit Z/stencil pixel-format branch maps to internal format `0x2033` at display `0x2CCA7`. In the CIL, `0xD3BE0` also masks off the low byte when caching the packed depth value for formats `0x2032` and `0x2033`. The general fill call at `0xAABFE` receives the selected packed value and mask for the eligible clear path.

There is a separate operation for Coarse Z state. Under the full-surface and auxiliary-state checks at `0xAAA89–0xAAAA7`, the `1122h`, `5122h), and `7122h` checks converge at `0xAAAEA`. The driver uses auxiliary-buffer helper `0xAA130` or `0xA98E0` depending on an additional context flag. It then calls `0xC71A0`, which emits Type-01 `29h` with literal `0x29008000` followed by the shadow word at `+0xA4`. VIA defines `01h:29h[15]` as reset of the Coarse Z cache tags. The VX900 XP build independently has the same ordered reset packet in `s3ginvA.dll` at `0xCD4B5–0xCD510`, reached from its class-4 clear backend `0xB05A0` via `0xB0A71`. This is a concrete operation to compare with the Win98 clear path when tracing stale depth. The runtime cause of the VX855 D24S8 tile failure remains open. V8-II, p. 76 · V855-II, p. 93.

<a id="c9-section-9"></a>

#### 9. Textures, render targets, depth and stencil

##### 9.1 What appears reusable

The families share the organizational model:

- `HParaType=01h`: non-texture state including render target, depth/stencil-related settings.
- `HParaType=02h`, subtypes `00h–0Fh`: texture-stage attributes.
- `HParaType=02h`, subtypes `20h–2Fh`: texture-sample-stage attributes.
- `HParaType=02h`, subtype `FEh`: generalized texture controls.

The VX855 and VX900 manuals present numerous similarly named fields for texture level-0 address, mip-level selection, Z-buffer address/pitch, render-target address, and linear/tiled address modes. That makes a **shared logical state description** attractive. It does **not** establish bit-for-bit compatibility for every state word; the bitfields should be validated before forming shared raw packets. [V855-II, pp. 100–145]; [V9-II, pp. 111–160].

##### 9.2 Particularly relevant to existing VX855 issues

**Depth-clear coherence:** The Quake 2 observation that `gl_ztrick 1` avoids a failure seen with full depth clears is consistent with investigating clear/cache/tiling state, but the manuals do not prove the cause. A dedicated test should vary whether the clear is performed by 3D, 2D or CPU, and verify the initial Z/stencil values in independently selected tiles. The XP clear code provides a specific comparison point because it updates an auxiliary buffer and issues a Coarse Z tag reset under defined conditions. [XP Coarse Z trace](#c9-section-379).

**D24S8 tile behavior:** For a two-triangle test, compare clear → first draw → second draw and draw → clear → draw, with depth/stencil state saved and all affected tiles examined. Run the sequence after cold boot, mode switch and DirectDraw surface recreation. This is a **proposed diagnostic**, not a documented bug fix.

**Texture wrapping/filtering:** Preserve the current software or tested-hardware fallback for edge cases until the sampler address and filter bits have been compared against the specific chip's manual and output images. A similarly named control need not implement identical edge behavior.

<a id="c9-section-19"></a>

#### 19. Additional depth/stencil compatibility details and likely diagnostic leverage

##### 19.1 Depth-format control is highly similar, but not proof of identical cache behavior

In VX855 and VX900, `HParaType=01h:12h` describes:

- `bit 0`: **linear** (`0`) versus **tiled** (`1`) Z/W buffer.
- `bits 18:16`: Z/W format, including the documented `110b` setting for **24-bit Z plus 8-bit stencil**, with **Z in `[31:8]` and stencil in `[7:0]`**.
- `bit 2`: existence of a **separate stencil buffer**.
- `bit 3`: whether separate stencil storage is synchronized with the stencil part of the Z buffer.
- `bit 4`: special operation to write separated-stencil values into the Z buffer.
- `bits 6:5`: separate-stencil buffer memory location.

**Sources:** [V855-II, PDF p. 87]; [V9-II, p. 99]. These fields provide a specific checklist for the current D24S8 failures; the existence of the same definitions on two families does not guarantee equivalent hardware results.

**Easy-to-overlook register difference:** `HParaType=01h:13h`, **bit 23**, is documented on VX855 as **source depth provided by the pixel shader** (`HZSrcPS`). The VX900 manual lists that bit as **reserved**, while retaining the main `bits 18:16` Z-test comparison selector. Do not copy a VX855 shader-depth-enable bit into VX900 even when all other Z-test settings appear to match. [V855-II, p. 88]; [V9-II, p. 100].

##### 19.2 Fast-Z-Clear controls relevant to Quake 2

For both VX855 and VX900, `HParaType=01h:27h` defines:

| Field | Meaning |
|---:|---|
| `bit 0` | Fast-Z-Clear enable |
| `bits 11:8` | Number of low index bits in the ZW-buffer index (`0` disables index use; `1–4` documented) |
| `bits 15:12` | Current scene's ZW-buffer index |

**Sources:** [V855-II, p. 93]; [V9-II, p. 104].

VX855 `01h:29h`, **bit 15**, separately describes a **self-clearing coarse-Z cache TAG reset**. The same register describes a coarse-Z writeback mode at bit 16, direction/near-far interpretation at bit 17, and conservative coarse-Z merge rules at bits 13:12. These are *not* interchangeable with a full memory clear. [V855-II, pp. 93–94].

**Proposed diagnostic for the existing `gl_ztrick` behavior:** Hold all other state constant, draw the same triangle, and independently vary (A) 3D depth clear versus CPU clear, (B) Fast-Z-Clear disabled/enabled in a deliberately controlled experiment, and (C) coarse-Z TAG reset when hardware is idle and the test harness knows its full required state. Inspect exact depth/stencil output after each step, not only rendered RGB. The manuals do **not** establish that changing any one of these controls fixes the bug; changing a cache control in a production driver without a verified synchronization recipe could cause a new hang or corruption.

##### 19.3 The separate-stencil address formula points toward 8×8 locality

VX855 `HParaType=01h:28h` describes the separate stencil-buffer base in units of **256 bytes** and gives the following address expression (reformatted from the manual):

```text
HZWBPit = (W + 7) >> 3
E3R(W)STADR = HSTBas*256 + Y[10:3]*HZWBPit*64
           + X[10:3]*64 + Y[2:1]*16
```

**Source:** [V855-II, p. 93]. The displayed expression uses high coordinate bits and 64-byte tile increments, strongly indicating an **8×8-derived tile organization** for this *separate-stencil* path. But it is **not** a complete documented byte-address formula for every packed D24S8 depth-buffer mode. Do not apply it uncritically to the ordinary interleaved Z24S8 surface.

**Suggested spatial tests:** draw at `x=7/8`, `15/16`, `y=7/8`, `15/16`, plus a pair of triangles that meet on an 8×8 tile boundary. Compare four controls: fresh boot; after CPU/2D clear; after a 3D clear; and after an intervening depth-format change. For each, record stencil readback, actual Z readback, and whether the symptom moves with the tile boundary. Repeat with **separate-stencil off** before assuming the separate-stencil address equation explains an ordinary packed D24S8 failure.

##### 19.4 VX855 occlusion writeback exists even without VX900 PPC

A useful refinement to §11: VX855 already documents an **occlusion-count memory-writeback facility** at `01h:21h–22h`:

- `01h:21h`: low 24 bits of the writeback address in 4-byte units.
- `01h:22h`: memory region and high address bits.
- The manual specifies that the **low-address word must be written first**, followed by the high-address word, which triggers checking the combined address for nonzero.

[V855-II, p. 90]. The corresponding detailed `21h–22h` Z-setting descriptions are **absent** from the VX900 pages between `01h:20h` and `01h:23h`; do not presume this particular VX855 address-trigger mechanism transfers. [V9-II, p. 102].

This may help VX855 **visibility/occlusion diagnostics** without implying access to VX900's richer PPC counters. It does **not** offer vertex counts, instruction counts or general memory-traffic instrumentation. The actual counter reset semantics and safe buffer setup still require testing.

<a id="c9-section-48"></a>

#### 48. CE H5/H5S2 dynamic command fields: verified shared render-target and texture setup

**Scope:** the retail x86 Windows Embedded CE 6.0 `DDI_VIA.dll` extracted from `VIA Graphics Driver Group2.msi`. The instruction VAs cited below are from the original image with base `0x10000000`. The OpenChrome sources are *not* used to infer these formulas. The three uploaded VIA Part II manuals are used to interpret their parameter addresses. This is static binary analysis only; no GPU was operated.

##### 48.1 An identical dynamically computed render-target setup across H5 and H5S2

Direct disassembly shows that the shared H5 texture helper (`Texture3D_H5`, VX800/VX855, `0x10067255`) and the H5S2 texture helper (`Texture3D_H5S2`, VX900, `0x10067B16`) **emit the same formulas** for four consecutive Type-01 rendering state words. Both start the packet with the `FE02` selector/streaming mechanism that the CE interpreter at `0x10063766` handles using MMIO `0x43C` (selector) and `0x440` (parameter data); here the selector word is `0x00010000` (Type-01 state).

| State | H5 instruction VA | H5S2 instruction VA | Identical dynamically generated command | VIA Part II meaning |
|---|---|---|---|---|
| `01h:50h` | `0x1006765C–0x1006766F` | `0x10067F52–0x10067F65` | `0x50000000 \| (destination_base >> 8)` | RT0 base address, 256-byte units |
| `01h:52h` | `0x10067671–0x10067682` | `0x10067F67–0x10067F78` | `0x52000000 \| (destination_pitch >> 5) \| format_word` | RT0 pitch in 32-byte units, plus format/other state |
| `01h:53h` | `0x10067686` | `0x10067F7C` | `0x53010C0F` | RT0 ROP, channel mask, dithering setup |
| `02h:00h` | `0x100676B3–0x100676B6` | `0x10067FA8–0x10067FAB` | `texture_level0_base >> 8` | Texture stage 0, face 0, level 0 base, 256-byte units |

The render-target color format is computed by `GetPixelFormat_INV` (`0x10064460`) before the packet is built. Both helpers call that function **twice**, returning early if either format code is `0xFFFFFFFF`. The output for the destination format is ORed into `01h:52h`. This is a direct shared formula, not merely a similar constant table.

**Cross-check with all three VIA manuals:** VX800 Part II, printed pages 76 onward, Type-01 RT0 `50h–53h`; VX855 Part II, printed pages 94–98; VX900 Part II, printed pages 106–110. Each describes RT0 base in units of 256 bytes and pitch in units of 32 bytes (linear mode). They also agree on Type-02 stage `00h` texture-level-0 base address in 256-byte units. **This is good evidence the common address/pitch encoder can be retained for these specific CE operations across the family.**

The precise emitted expressions are shown schematically below; variable names reflect their role in the manuals, not recovered original C++ names:

```c
// Reconstructed from CE binary; demonstration, not drop-in driver code.
uint32_t rt0_base    = 0x50000000u | (dest_base_bytes >> 8);
uint32_t rt0_fmtpitch= 0x52000000u | (dest_pitch_bytes >> 5)
                                      | GetPixelFormat_INV(dest_format);
uint32_t rt0_ropmask = 0x53010C0Fu;
uint32_t tex0_base   = texture_base_bytes >> 8;  // under Type-02 stage-0 selector
```

Interpretation of `0x53010C0F` under the published RT0 `53h` bitfield:

- `[11:8] = 0xC` means **COPY_PEN** (replace target color).
- `[3:0] = 0xF` enables writes to **A/R/G/B** channels.
- `[16:15] = 0b10` chooses one of the documented RT0 dithering/rounding modes.

The `01h:50h` and `02h:00h` units are 256 bytes, and `01h:52h` pitch units are 32 bytes. **These alignments are requirements of this encoding**, not evidence that arbitrary unaligned pointers can be safely truncated. The function contains no general correctness proof for misaligned client buffers.

##### 48.2 Recovered color-format mapper; exact branch values

`GetPixelFormat_INV` (`0x10064460–0x100644A3`) produces the following values for CE `EDDGPEPixelFormat` enum inputs:

| Input value | Return word | Likely target format from VIA Type-01 `52h` top-byte coding |
|---|---:|---|
| `4` | `0x00890000` | 16-bit ARGB-family RGB565 encoding |
| `5` or `7` | `0x00880000` | 16-bit ARGB-family RGB555 encoding |
| `8` or `9` | `0x00990000` | 32-bit ARGB-family encoding |
| FourCC `YUY2` (`0x32595559`) | `0x00300001` | YUV packed format code, plus a low-bit flag |
| Other values | `0xFFFFFFFF` | Caller rejects the format |

**Caution:** the binary does not expose the original enum names for values 4/5/7/8/9 here. The right column decodes the **resulting hardware register field** using VIA's register manual; it does *not* establish the exact source CE enum name or prove all these cases work for both source and destination. In particular, YUY2's returned low bit (`...0001`) must be examined together with the pitch field before assuming a complete working YUV render-target setup.

##### 48.3 Same textured-rectangle setup, including computed geometry

The H5 and H5S2 routines both recognize four branches for a texture-orientation/mapping argument at stack offset `+0x50` (`0–3`), then compute a rectangular vertex/texture-coordinate configuration with x87 floating-point conversions. For example, each emits a common quartet of geometry dwords using OR/shift combinations of the computed rectangle corners:

- H5: `0x100675CE–0x1006764F`;
- H5S2: `0x10067EDB–0x10067F45`.

The structure and constants (such as `0xFFF80000`, `0xFFF81000`, `0xFFF82000`, `0xFFF83000`, shifts of 12) are the same across both functions, even though the general vertex-programming interface differs in VX900's public documentation. **This narrows the actual incompatibility:** the restricted CE textured-rectangle data encoding survives, while the surrounding render-state and setup tables differ. The precise effect of each orientation case is not established from the selected code alone; do not label them as rotation/mirroring without tracing the caller and image output.

##### 48.4 Command-buffer request size and H5S2's additional overhead

When the command-interface object has the observed request-buffer mode selected (`[command-interface + 0x70] == 0`), both texture helpers call `cmSmartRequest` (`0x1004E42E`, named in the matching retail `.map`) with the same `0xFE020000` command class but **different nominal byte counts**:

| Helper | Request at | Requested byte count | Related code |
|---|---|---:|---|
| H5: VX800 / VX855 | `0x10067593–0x100675A4` | `0x280` = 640 bytes | The shorter command/state setup |
| H5S2: VX900 | `0x10067E8C–0x10067E9D` | `0x4B0` = 1,200 bytes | Additional state packets and wait helper |

The **difference is 560 bytes**. The separately reconstructed eight repeated 32-byte wait packets occupy **256 bytes**, so the nominal request-size increase is consistent with (but does not prove) extra state plus wait traffic. These are allocator/request parameters, **not verified executed command lengths or GPU transfer sizes**. The same code has another branch that uses a shared preexisting command-buffer address (`0x100CDD60`) rather than making this request.

##### 48.5 Portable subset versus VX900-specific setup: updated boundary

| Operation | CE H5/H5S2 comparison | Driver action |
|---|---|---|
| 256-byte render-target base encoding | Identical | Candidate shared helper |
| 32-byte render-target pitch encoding | Identical | Candidate shared helper, with field validation |
| Render-target copy ROP/channel mask (`53h`) | Identical | Candidate shared baseline state |
| 256-byte texture-stage-0 base encoding | Identical | Candidate shared helper |
| Restricted textured-rectangle geometry construction | Same four-way structure | Candidate shared helper after checking end-to-end geometry |
| CE pixel-format selector `GetPixelFormat_INV` | Same function | Share table only for supported formats |
| Command-buffer request size | **Different: 640 vs 1,200 bytes** | Gen-specific reservation |
| H5S2 Type-03 subtype `31h` extra tables | Only VX900 path | Keep chipset-specific; GPU meaning undocumented |
| H5S2 `VX900_WAITPATCH` | Only VX900 path | Do not copy into VX855 blindly |
| Full Direct3D/OpenGL texture/shader backend | **Not established** | Do not infer compatibility beyond CE textured blits |

**Suggested further static trace:** track the destination/source pitch and base arguments from the CE public entry point into `Texture3D_*`, check alignment and maximum limits before OR-composing 3D parameters, then resolve the final `cmSmartRequest`/submit path for all four orientation modes. Dynamic on-board validation is still needed to prove pixel-identical behavior or compatibility with the current Windows 98 implementation.

<a id="c9-section-50"></a>

#### 50. CE texture dynamic parameters beyond base/pitch: dimensions, exponents and coordinate encoding

**Evidence and scope.** Independently rechecked against the extracted **retail** 867,840-byte `DDI_VIA.dll` (SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`), using its original VA base `0x10000000`. The path names reflect the CE dispatch established in §46: `Texture3D_H5` = VX800/VX855 (`0x10067255`), and `Texture3D_H5S2` = VX900 (`0x10067B16`). All bitfield interpretations are checked against the three VIA **Part II** manuals; no hardware tests were run. The code below represents **one particular CE texture operation**, not the entire graphics driver.

##### 50.1 Both implementations pad texture width to a multiple of 16

Both function prologues calculate the same value from the argument at `[ebp+0x24]`:

```c
uint32_t padded_width = (input_width + 15u) & ~15u;
```

- H5: `0x1006725B–0x10067276` (result saved at `[ebp-0x34]`).
- H5S2: `0x10067B1C–0x10067B37` (same stack-local offset).

That padded width is used as **the low 12-bit width field of texture `Type-02 / subtype 00h / address 20h`** and also in subsequent floating-point coordinate calculations. The original input width is used separately to derive texture-size exponents (§50.2). The padding is thus part of VIA's actual CE texture setup, not a newly inferred optimization. This code does **not** establish whether the surface allocation or CPU upload path already padded the row storage. The manuals specify an up-to-2048 width, so the unchecked 32-bit `+15` expression should not be copied to new callers without validating that range.

##### 50.2 Both implementations generate the same `Type-02` dimension words

At H5 `0x100676B8–0x100676F5` and H5S2 `0x10067FAD–0x10067FEA`, the functions build these same **Type-02, texture stage 0** parameter words. The apparent `0x20000`/`0x2200000` constants are register tags before being shifted into the high byte; they are *not* texture dimensions:

| Parameter address | CE-generated word | Meaning in the VIA Part II manual |
|---|---|---|
| `02h:18h` | `0x18000100` (literal, identical) | Texture Control 1: mode `0001` = ordinary 2D texture; memory-location bits zero in this word |
| `02h:20h` | `0x20000000 \| (input_height << 12) \| padded_width` | Texture Control 2: 12-bit height and 12-bit width of level 0 |
| `02h:21h` | `0x21000001` (literal, identical) | Texture Control 3: length/depth `1` for the 2D case |
| `02h:22h` | `0x22000000 \| (height_exp << 4) \| width_exp` | Texture Control 4: encoded exponents of height and width, length exponent zero; tile and power-of-two flags zero in this word |
| `02h:30h` | `0x30000000 \| GetPixelFormat_INV(source_format)` | Texture Control 5: format-word mapper shared by both CE implementations |

**Exponent calculation** is an iterative integer loop rather than an x87 logarithm: start at exponent zero, increase until `1 << exponent` is at least the *original* dimension. Thus, for the documented positive texture range, `width_exp = ceil(log2(input_width))` and likewise for height; a dimension of `1` yields exponent zero. H5 loop: `0x10067551–0x10067582`; H5S2 loop: `0x10067E17–0x10067E7B`. These exponent words are constructed **even though the CE control word leaves the explicit power-of-two flag clear**. The interface's behavior with zero dimensions or sizes larger than the manual's limits is not established.

Manual cross-check: VX800 Part II printed pages 100–103 (`Type-02`, `18h/20h/21h/22h/30h`); VX855 Part II pages 117–119; VX900 Part II pages 123–125. Each manual describes these five register roles and the 2D mode encoding. This is a newly verified **shared texture declaration encoder** across the family, beyond the previously established base-address and render-target fields.

**Example of the computed words** for a notional 97 × 65 input texture (illustrative arithmetic, **not** captured GPU output):

- padded width = `(97+15)&~15 = 112`;
- width exponent = `7` (128 ≥ 97), height exponent = `7` (128 ≥ 65);
- address `20h` word = `0x20041070` (height 65 and width 112);
- address `22h` word = `0x22000077` (height exponent 7, width exponent 7).

This example deliberately exposes that the CE command encodes **padded logical width and exponent of the unpadded input width** as separate values. A port should not collapse those into one number without tests.

##### 50.3 VX900 H5S2 adds a six-value `786432.0` bias absent from H5

A **real, previously unlisted difference** appears immediately after H5S2 computes its rectangular coordinates and before it assembles the remaining command packets:

- H5S2 `0x10067E14–0x10067E50` loads the double-precision constant at **VA `0x10008928`**, verified as `786432.0` (`0x4128000000000000` little-endian bytes: `00 00 00 00 00 00 28 41`), and adds it to **six working floating-point coordinate values** at stack-local offsets `-0x14`, `-0x18`, `-0x1C`, `-0x20`, `-0x24`, `-0x28`. Some of those stack slots are subsequently reused; do not assume all six biased values survive unchanged to the final vertex stream.
- The H5 code, which reaches exponent setup directly at `0x10067551`, **has no equivalent reference to that constant** within `Texture3D_H5`.

`786432.0 == 1.5 * 2^19` is a recognizable floating-point **fixed-subpixel conversion bias**. For coordinates near zero, conversion to IEEE-754 binary32 after the addition produces a bit-level scale of 16 units per pixel:

| Input | Float32 bits of `(786432.0 + input)` | Low-bit difference from `0x49400000` |
|---:|---:|---:|
| `0` | `0x49400000` | `0` |
| `0.0625` | `0x49400001` | `1` |
| `0.5` | `0x49400008` | `8` |
| `1` | `0x49400010` | `16` |
| `2.25` | `0x49400024` | `36` |
| `-0.5` | `0x493FFFF8` | `-8` (wrapped) |

This arithmetic independently confirms **1/16-pixel representability** in the low bits for the shown values. **What remains unproven:** whether H5S2 uses it for coordinate snapping, a packed fixed-point vertex format, or an intermediate setup convention. Do not copy the bias into the VX855 vertex emitter based on this CE-only code. The sequence's use in other H5S2 functions still requires inspection.

##### 50.4 Two additional constants in the common coordinate calculations

Both texture paths load the literal double `0.5` at VA `0x10005728` and `1.5` at VA `0x100089E8` during later x87 coordinate arithmetic (H5 ~`0x10067788` and `0x100677DB`, H5S2 ~`0x1006807D` and `0x100680E0`). These are typical sampling/edge-offset constants but their *precise* semantics need a complete caller-to-vertex trace. Another constant at VA `0x10003928` is the float32 `4294967296.0` (`2^32`) used as a correction after loading some unsigned integers through signed x87 `fild`: this is ordinary integer-to-float normalization, not a GPU register command.

**Practical consequence:** share the `02h:18/20/21/22/30` texture-state builder only within the documented CE format restrictions; implement the H5S2 coordinate-bias path separately pending validation. The base/pitch formulas in §48 remain unaffected by this finding.

<a id="c9-section-327"></a>

#### 327. Texture and pixel-shader three-family audit: scope, evidence and classification

**Research objective:** compare the full *documented programming interface*, rather than assume that identical texture/shader terms imply identical hardware. Evidence comprises original VIA VX800/VX820 HC3 Part II (Nov. 2008), VX855/VX875 HCM Part II (July 2009), VX900 HD Part II (Jan. 2011), the original CE 6.0 retail `DDI_VIA.dll` (SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`), matching map, and the two compiled `fn3DBlt_H6`/`fn3DBlt_H5S2` implementations mapped in §§271–280. The companion **25-row** `Chrome9_Texture_Sampler_Three_Family_Matrix_327_336.csv` and `Chrome9_Texture_Sampler_Three_Family_Audit_327_336.py` compare documented fields and check **five program/control table hashes** plus shared source-address references in both backends.

**Evidence taxonomy:** same *documented* encoding; same broad format family but enum details not exhaustively validated; documented bit/algorithm difference; **VX900 manual says reserved but VIA's own CE H5S2 binary writes/uses the field**; and hardware behavior unresolved. These categories must not be collapsed. A compiled table in a driver shows what software submits, **not independently what the silicon accepts**. No GPU draw, output-pixel capture or timing test was performed in this pass.

The VX900 manual's contradictory reserved labels were also inspected visually on **VX900 Part II PDF page 126 (printed p120)** for Type-01 `90h–9Ah` and **PDF page 159 (printed p153)** for Type-03 `20h–22h`. These are actual reserved labels in the published manual, not parsing artifacts.

<a id="c9-section-328"></a>

#### 328. The three platforms share the fundamental Type-02 texture stage address and size format

All three manuals identify **Type-02 subtype `00h–0Fh`** as per-texture attributes and **subtype `20h–2Fh`** as per-texture-sample controls. In the per-texture namespace:

| Per-texture subaddress | Shared published field | Portability qualification |
|---|---|---|
| `00h–05h` | Face `0–5` level-0 addresses with `A[31:8]` and **256-byte address units** | All three manuals instruct programming subaddress `02h` (face 2) back to zero. Cube-map runtime behavior remains untested |
| `18h` | Texture mapping mode and face-location settings | Shared `S.L.`/`S.F.` choices; **location `11b` remains reserved** in all three texture-stage tables |
| `20h` | Level-0 width and height, 12-bit fields, documented max **2048** | Address bounds and pitch still depend on format/memory mode |
| `21h` | Level-0 3D volume length; the manuals say no MIP for volume textures | Shared documented volume limitation |
| `22h` | Linear/tiled mode; 8- or 16-texel tile height; power-of-two status and exponent fields | Common field framework, not a measured tiled-addressing equivalence |
| `30h` | Texture format family at bits `[23:19]`, subformat at `[18:16]` | Shared high-level enum; do not assume every reservation/undefined subformat is identical |

**Important distinction:** although subtype labels enumerate sixteen texture stages (`0h–Fh`), the global Type-02 `FEh:00h[7:4]` field explicitly documents **a maximum of eight textures**. A sixteen-value address namespace is **not proof of 16 simultaneously active texture units**.

The texture-stage `11b` location remains reserved on VX900 **even though** other VX900 *2D and command-regulator* location fields document L.L. as a valid fourth choice (§§300–326). Treat location encodings as **register-family specific**; it would be incorrect to extend a Type-10/GE address selector into Type-02 by analogy.

<a id="c9-section-329"></a>

#### 329. Common texel-format encodings: what is actually supported by all three manuals

The broad Type-02 `30h[23:19]` format groups are visibly carried across all three generations: intensity, luminance, alpha, compressed textures, YUV video texture, ARGB/ABGR/RGBA/BGRA, floating color and single-component/depth-scale categories. The text explicitly includes common examples such as **L16, RGB565, ARGB8888, ABGR8888, DXT1, DXT2/3 and DXT4/5** in all three manuals. The 25-row matrix records these concrete shared examples separately from the broad group-level statement.

This is **documented encoding parity**, not an exhaustive bit-perfect test of the 8-bit format selector: e.g., the manuals have occasional spelling, reserved-range and channel-layout anomalies, and a particular texture's final fetched values depend on tiling, color extension, YUV conversion and sampler settings. **Do not infer a new hardware format from wording changes alone**, and do not assume that a texture format valid for sampling is necessarily a valid render-target format. The highest-value missing evidence would be a mode-matched pixel/reference test for the common format enum, not another speculative hardware write.

<a id="c9-section-330"></a>

#### 330. The sampler control framework is shared: LOD, filter, addressing and border payloads

Across Type-02 subtype `20h–2Fh`, the manuals retain matching register groups: `30h` for MIP minimum/maximum level and signed LOD offset, `31h` for anisotropy and filter selection, `32h` for S/T/R address modes, and `35h–36h` for the border color. The common published sampler features include **nearest, linear, anisotropic and 4×4 filter modes**, up to **16** in the advertised anisotropy field, and border/clamp/repeat/mirror/mirror-once addressing. Border channels are represented by packed signed **s1.10** components; `35h` contains red/green and `36h` blue/alpha.

These are good candidates for **the same command emitter with per-chip exceptions**, because the source fields and bit widths are documented in common. **No** sample operation was run to prove identical filtering precision, derivative selection, anisotropic sampling order, subnormal/NaN handling or border-color rounding. A static compatibility classification does not imply identical GPU output.

<a id="c9-section-331"></a>

#### 331. The border-color *algorithmic note* differs significantly, especially around depth/PCF

This is a genuine **manual semantics divergence** despite identical four-channel border payload register locations:

| Family | Documented border-color handling for filtering | Evidence |
|---|---|---|
| **VX800** | Direct filtered-output border substitution is conditioned on special formats, including L16/VU16/G16R16/G16FR16F/G32F; otherwise its pseudocode describes border substitution per sampled texel **before** filtering | VX800 Part II printed p108 |
| **VX855** | Its published pseudocode begins with **“If any filtered texel is border color”** then assigns all output channels the border channels; a following alternative mentions combining border red/green as a `.24` value | VX855 Part II printed p127 |
| **VX900** | Explicitly distinguishes special-format or Z-not-PCF output substitution from **24-bit fixed Z + percentage-closer filter**, using `{R[11:0],G[11:0]}` as a `.24` border value for the latter | VX900 Part II printed p132 |

The VX855 pseudocode has its own ambiguity: an `else if texel is border color` clause appears after an `if any filtered texel is border color` condition. Treat the three texts as **different documented algorithms/wording**, not as executable specifications from which precise output differences can be proved without testing. **Depth texture border behavior and PCF are therefore not safely claimed byte-for-byte equivalent.** All three also document a special YUV-space border-color mapping when YUV-to-RGB conversion is active.

<a id="c9-section-332"></a>

#### 332. VX800-only documented Type-02 `FEh:00h[8]` `texldp` divisor/mantissa check

The **VX800** manual explicitly documents Type-02 **`FEh:00h` bit 8** as `HTXtexldpchpM`: when enabled, compare the divisor and dividend mantissas during the projected-texture instruction `texldp` and assign a quotient mantissa of 1.0 on the specified equality condition. The **VX855** and **VX900** manuals both label **`FEh:00h[23:8]` reserved**. Type-02 `FEh:00h[7:0]` is otherwise a common framework: texture count, DFIFO arbitration ordering, 2-versus-4 QW fetch option, and texture-cache clear.

**Classification:** a **VX800 documented control not portable by published definition** to VX855/VX900. Reserved does not establish whether bit 8 is physically ignored, repurposed or unsupported; only its **published contract** changed. Software targeting all three should not set that bit as an unqualified common optimization. A controlled projected-texture test would be needed to establish behavior beyond the manuals.

### Pixel programs, state tables and blend paths

<a id="c9-section-8"></a>

#### 8. Pixel shaders, vertex shaders and the hardware-T&L question

##### 8.1 Older shader programming is documented in VX800/VX855

The VX800 and VX855 manuals document `HParaType=03h` subtypes `20h` (PS ALU instructions), `21h` (PS TAU instructions), and `22h` (PS constants), along with non-texture state for pixel-shader execution. VX855 explicitly describes shader setting registers at `HParaType=01h`, `90h` onward. [V8-II, 3D register chapter, pp. 119–134]; [V855-II, pp. 115–125 and 144–153].

##### 8.2 VX900 manual conflict — do not guess ISA compatibility

The VX900 command-regulator overview **names** PS ALU, PS TAU, PS constants, VS instructions, VS float constants and VS integer registers. However:

- Detailed `HParaType=01h`, `90h–9Ah`, is labeled **reserved**.
- Detailed `HParaType=03h`, `20h–22h`, is labeled **reserved**.
- Detailed `HParaType=03h`, `31h–33h`, is labeled **reserved** even though the overview names VS instruction/constant categories.
- VX900's PPC list counts executed VS and PS instructions, and its vertex-input format names VS input/output registers.

**Evidence:** [V9-II, pp. 70–73, 126, 159–165, 180].

These statements cannot safely be collapsed into either "VX900 has no shaders" or "the VX855 shader microcode will run on VX900." The manual appears **incomplete or internally inconsistent** for instruction uploading. A functioning VX900 XP/Linux driver's output, full disassembly, or separate VIA shader ISA documentation is needed to resolve it.

**Hardware T&L assessment:** VX900 appears substantially better equipped for vertex processing, but **its usable programmable vertex-transformation path is unproven from the six manuals alone**. Maintain the current CPU-transform path as a reliable fallback. Do not advertise hardware T&L or additional Direct3D shader capabilities until a verified program can be loaded and its transformed vertices tested.

##### 8.3 Minimum shader research experiment

1. Record **read-only** PCI/BAR and engine identity information on a real VX900 board.
2. Capture known working shader initialization or command traces from a legitimate VX900 driver.
3. Decode `HParaType`, subtype, sub-address, payloads, and write order.
4. Identify a known-safe no-op/pass-through vertex program, if one exists.
5. Submit a single deterministic triangle; compare clip-space and screen-space output against CPU reference.
6. Add a matrix transform only after the baseline is stable.
7. Do not enable shader caps in Windows 98 until repeatable correctness and recovery tests pass.

<a id="c9-section-49"></a>

#### 49. CE 3D blit cross-generation: shared 50-DWORD state table, shared geometry encoder, different reservations

This section continues the retail `DDI_VIA.dll` disassembly beyond the `Texture3D_*` helpers. The public CE API/structure members are not available as source; memory offsets below are reported as observed. All VAs refer to the same retail image as sections 46–48.

##### 49.1 VIA copies the **same exact 200-byte state block** in both blit backends

The VX800/VX855-selected `fn3DBlt_H6` (`0x10064503`) and VX900-selected `fn3DBlt_H5S2` (`0x1006548C`) both:

1. Construct an `FE020440` command packet targeting parameter selector port `0x43C` with selector `0x00010000` (Type-01 registers).
2. Copy **50 dwords** (`0x32 * 4 = 200 bytes`) from the exact same DLL read-only table at **VA `0x100084F8`** into the command buffer with `rep movsd`.
3. Continue with calculated render-target, geometry, blending, and submission words, where the code paths eventually diverge.

Evidence: H6 `0x10064655–0x1006466B` and H5S2 `0x100655DE–0x100655F3` both load `0x100084F8`, count `0x32`, then execute the same block copy. This is stronger than the earlier conclusion that their initial literal writes "look similar": it is an exact **shared binary data source** used by both runtime-selected backends.

**The shared 50-DWORD block, as stored in the CE retail DLL:**

```text
11000000 10000000 01CE1540 0000000F 150000FF
14FFFFFF 13070000 19800000 18000000 17000000
16000000 28000000 27000000 1B803FBF 1A800000
1F000000 1E000000 2A000000 29000000 25000000
24000000 23000000 20007FFF 35000000 34000000
33007000 26000000 39000000 38000000 37000000
36000000 3D000000 3C000000 3B000000 3A000000
3F000000 3E000000 71000000 70000020 7600007F
75000000 73000000 72000000 780FE000 77003F80
85000000 84000000 8A820000 89000800 88001ED4
```

These are Type-01 parameter words *within the CE software packet*, not raw MMIO offsets. The first word `0x11000000` is a parameter-address-`0x11` word in the Type-01 selected stream. Each subsequent word should be decoded using the VIA Part II Type-01 register table; do not treat the entire copied region as an independent GPU command packet.

##### 49.2 Shared dynamic pitch and coordinate construction

At H6 `0x1006459F–0x100645B8` and H5S2 `0x10065528–0x10065541`, both writers calculate the RT0 `0x52` state word from a nested structure field using:

```c
// Literal logic (offsets observed, C field names not recovered):
rt0_52 = 0x52000000 | ((field_at_surface_plus_0x24 >> 5) & 0x1FF)
                         | field_at_surface_plus_0x28;
```

The VIA Part II manuals define RT0 pitch in 32-byte units and encode format in the same 24-bit payload. **Note the 9-bit mask (`0x1FF`) in this particular CE function:** the manuals allow a nominal 10-bit pitch field. The extra bits may be supplied by the ORed field at `+0x28`, or this specific CE API may impose a smaller supported range. The source alone does not establish a pitch-limit defect.

For geometry, both mask rectangle coordinates with `0xFFF`, pack X/Y into 24 bits, and OR high-byte parameter identifiers `0x82` and `0x83`. Shared constants in the calculation include `0xFFF80000`, `0xFFF81000`, and 12-bit shifts. Evidence: H6 `0x100645BB–0x10064603`, H5S2 `0x10065544–0x1006558F`. This is another **identical operation-specific encoder** across the family despite the broader VX900 FVF interface differences.

##### 49.3 Different requested buffer sizes in both blit modes

`cmSmartRequest` (`0x1004E42E`) is called with the same `0xFE020000` class, but a per-generation allocation/request length derived from the CE blit mode field `[KMD_3DBLT_DATA + 0x30] == 3`:

| CE 3D blit path | Mode equals 3 | Other mode | Code evidence |
|---|---:|---:|---|
| H6 (VX800 / VX855) | `0x500` = **1,280 bytes** | `0x438` = **1,080 bytes** | `0x10064539–0x1006455A` |
| H5S2 (VX900) | `0x640` = **1,600 bytes** | `0x4B0` = **1,200 bytes** | `0x100654C2–0x100654E3` |

The H5S2 request is **320 bytes larger in mode 3**, and **120 bytes larger in the other mode**. These values are **requested buffer capacities**, not necessarily the number of bytes finally submitted or processed by hardware. The separate `Texture3D_H5/H5S2` request sizes remain 640 vs 1,200 bytes as documented in section 48.

##### 49.4 Updated sharing boundary for implementation

**Safe candidates for a shared encoder (after validation):** initial Type-01 state upload from `0x100084F8`; RT0 pitch/format field construction; 12-bit coordinate packing for `0x82/0x83`; source/target base-address units (section 48); and selected fixed geometry/texture-state packet constructors.

**Must remain generation-specific:** command-buffer reservation sizing, H5S2-specific post-initialization tables, VX900 `VX900_WAITPATCH`, later shader/vertex-input state, and the final submission/synchronization path.

**Unresolved:** the meaning of `KMD_3DBLT_DATA + 0x30 == 3` (observed branch, not named in this binary), the significance of `+0x28` in the 0x52 pitch calculation, whether the packed geometry ever needs extension beyond 12-bit coordinates, and whether a general hardware 3D API beyond these CE blits shares the same encoders. These require the corresponding type definitions or additional disassembly/hardware logs.

<a id="c9-section-55"></a>

#### 55. Verified common pixel-shader program uploads in H6 and H5S2 CE 3D blits

**Scope:** Static analysis of the *retail* CE `DDI_VIA.dll` from `VIA Graphics Driver Group2.msi`, SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19` (same file as §§46–54). These findings concern the **CE 3D blit implementation** and do not prove that all Direct3D/OpenGL shaders on these chips share an instruction set. No code was executed on hardware.

##### 55.1 The compiled H6 and H5S2 backends reference identical pixel-shader data

Both `fn3DBlt_H6` (VX800/VX855) and `fn3DBlt_H5S2` (VX900) build `FE03` uploads to the **same** Type-03 shader subtypes:

| Parameter selector | Documented VX855 purpose | H6 compiled source anchors | H5S2 compiled source anchors | Proven result |
|---|---|---|---|---|
| `0x20030000` (Type 03, subtype `20h`) | Pixel Shader ALU instructions | `0x10064C7B`, `0x10064CE6` | `0x10065D08`, `0x10065D73` | Same 12-DWORD or 4-DWORD **program data** from the exact same read-only addresses |
| `0x21030000` (subtype `21h`) | Pixel Shader TAU instructions | `0x10064CAE`, `0x10064D17` | `0x10065D3B`, `0x10065DA4` | Same 2-word or 1-word TAU data |
| `0x22030000` (subtype `22h`) | Pixel Shader constant registers | `0x10064D63` | `0x10065DF3` | Same four-component *constant construction logic*, including optional parameter-dependent first component |

Specifically, both paths reference address `0x100088B4` for a **12-DWORD ALU program** and `0x1000875C` for the **4-DWORD ALU program**. Both load two TAU words from `0x100088E4/0x100088E8` in the longer variant and write literal `0x301001B0` in the shorter variant. Both form the subtype-`22h` constants from the operation's alpha/blend control and `1.0`, `0.0`, `0.0` in the longer branch.

The **VX855 Part II manual**, printed page 147 (PDF page 153), states that `03h:20h` ALU instructions contain **four DWORDs each**; subtype `21h` TAU instructions contain one DWORD each; and subtype `22h` constant registers contain four components each. Thus the CE's 12-word and 4-word ALU uploads correspond to **three and one** documented ALU instructions, respectively. The eight-word/twelve-word *vertex* uploads in §56 below are a different interface and should not be confused with these pixel-shader programs.

##### 55.2 Recovered shared CE pixel-shader program data (hexadecimal, DWORD order)

The longer 12-DWORD ALU program at RVA `0x000088B4` (VA `0x100088B4`), grouped according to the VX855 manual's four-DWORD instruction size:

```text
ALU instruction 0: 00802490 B8080000 00F0006D 00000C20
ALU instruction 1: 0140B6D0 3000C053 00F20105 00000C20
ALU instruction 2: 020C0530 30080000 00F20005 00000820
```

The shorter 4-DWORD ALU program at `0x1000875C`:

```text
ALU instruction 0: 01C00530 301C0053 00F20005 000D0820
```

The TAU uploads use:

```text
Long variant (2 words): 301001B0 311111B1  (source VA 0x100088E4)
Short variant (1 word): 301001B0
```

**Important distinction:** the short ALU program is **not** the first instruction of the long ALU program (`01C00530...` versus `00802490...`); it is a different program. By contrast, the short TAU sequence **is** the one-word prefix of the longer TAU sequence. A portable command emitter must select the full ALU program for the requested mode, not simply truncate the longer program.

The CE binary **does not provide named assembly instructions** for these raw words. No higher-level shader disassembly or exact rendering effect is claimed; only the command destinations, instruction counts, source addresses and equality of the copies are verified.

##### 55.3 Important VX900 manual omission, resolved only at the command-emission level

The **VX900 Part II manual**, printed page 153 (PDF page 159), calls the detailed Type-03 subtypes `20h–22h` **reserved**. Nevertheless, VIA's VX900-selected H5S2 function explicitly uploads pixel-shader words to all three subtypes. Its command-regulator *overview* still names these shader types. This means the **CE implementation provides direct evidence of actual writes on VIA's intended VX900 path**, although it does not prove every instruction supported by VX855 works on VX900. The manual's detailed “reserved” classification must be retained as an outstanding documentation conflict.

**Driver implication:** The **same ALU/TAU/constant packet builder can be shared for these CE programs**. Keep capability and validation checks per chipset and do not assume full hardware shader-ISA or resource-limit equivalence.

<a id="c9-section-61"></a>

#### 61. CE texture helper: three pixel-shader ALU instructions on H5 versus one on H5S2

This is a **different comparison** from §55, which showed identical pixel-shader programs in the separate H6/H5S2 **3D blit** functions. The `Texture3D_H5` and `Texture3D_H5S2` **texture helpers** themselves program different pixel-shader ALU code and different instruction counts.

The VX855 Part II manual defines `01h:92h` (*Pixel Shader Setting 3*) bits `[21:16]` as **Length of TAU Instruction** and bits `[8:0]` as **Length of ALU Instruction** (printed p.111). In the CE compiled code:

| Texture helper | `01h:92h` full word | TAU length | ALU length | Compiled instruction data |
|---|---|---:|---:|---|
| `Texture3D_H5` (VX800/VX855) | `0x92010003`, VA `0x10067843` | 1 | **3** | Twelve consecutive DWORDs at `0x100678CB–0x10067923` |
| `Texture3D_H5S2` (VX900) | `0x92010001`, VA `0x10068134` | 1 | **1** | Four consecutive DWORDs at `0x10068248–0x10068260` |

Both send the same one-word TAU sequence, `0x301001B0`, to Type-03 subtype `21h`. The H5 texture helper emits the following Type-03 subtype `20h` ALU program, shown as three groups of four DWORDs according to the VX855 manual's published ALU instruction width:

```text
H5 PS ALU #0: 0140B6D0 50040055 00720005 00001060
H5 PS ALU #1: 01C00530 B01C0053 008040B6 000D0060
H5 PS ALU #2: 01C00530 301C0053 00F20105 000D0820
```

The H5S2 helper emits one ALU instruction:

```text
H5S2 PS ALU #0: 01C00530 301C0053 00F20005 000D0820
```

**Precise relationship:** H5S2's sole ALU instruction matches the *last* H5 ALU instruction in DWORDs 0, 1, and 3; DWORD 2 differs by **`0x00000100`** (`00F20105` versus `00F20005`). Thus the new program is **not simply the third H5 instruction copied unchanged**. The H5S2 four words also exactly match the shorter pixel-shader program in the **shared 3D blit** implementation at DLL VA `0x1000875C`.

`01h:93h = 0x93000001` is unchanged between texture helpers, consistent with their common one-TAU stage. **There is no established 3:1 performance gain:** the code does not establish equal work, equal pixels, which shader units were active, or instruction execution times. The VX900 public manual marks detailed `01h:92h` and `03h:20h` descriptions reserved; these interpretations use the VX855 definitions applied to actual VIA CE writes, not an independently published VX900 ISA specification.

**Practical implications for a family driver:**

- Share **packet construction and documented register-field encoders** where binary commands match; select complete operation-specific ALU programs per path, not just a chipset-wide default shader.
- Treat `Texture3D_H5` and `fn3DBlt_H6` as different operations; their shader code and vertex packing are not interchangeable merely because they target VX800/VX855.
- Resolve whether the shorter H5S2 shader produces the same pixels by testing matched CE operations, rather than inferring an optimization from instruction count alone.
- Keep the VX900 Type-03 subtype `31h` and Type-04 `60h–62h` commands in a generation-specific module until their undocumented semantics are established.

**Static verification:** `Chrome9_CE_Command_Verification_58_61.py` opens the supplied selected-reference ZIP, checks the retail DLL SHA-256, reads the actual 32-DWORD state table and 8/12-DWORD VS candidates, validates immediate x86 MOV command data in the described functions, reconstructs the IEEE-754 transform values, and checks the instruction-count words. No driver installation, hardware probe, or execution of the binary is involved.

<a id="c9-section-62"></a>

#### 62. CE 3D blit mode selection: vertex and pixel programs are controlled separately

**Newly verified from the retail CE DLL, static analysis only.** This section advances the question posed at the end of §61: *does VX900 move pixel-shader arithmetic into vertex-shader instructions?* The CE 3D-blit control flow does **not** support such a simple conclusion. VIA makes two related but distinct decisions:

1. Select a **six- or eight-DWORD vertex layout** when the operation's byte at `input+0x1C` has bit `0x20` set **or** the global `bGdiMultiABlend` flag (`VA 0x100CE7EC`) is nonzero.
2. Select the **long pixel-shader ALU/TAU program** according to **`bGdiMultiABlend` alone**. The input bit `0x20` by itself does not select the long PS program in the inspected blit code.

The same separation exists in `fn3DBlt_H6` (VX800/VX855) and `fn3DBlt_H5S2` (VX900). The H5S2 path also makes a third decision: its Type-03 subtype-`31h` **candidate VS program** is eight DWORDs in the six-DWORD-vertex case, and 12 DWORDs in the eight-DWORD-vertex case.

| Input bit `0x20` | Global multi-blend | Common H6/H5S2 input attributes | H6 PS ALU/TAU upload | H5S2 VS candidate upload | H5S2 PS ALU/TAU upload |
|---|---|---|---|---|---|
| Clear | Off | XYZ/W + first ST; **6 DWORDs/vertex** | **4 / 1 DWORDs** | **8 DWORDs** (2×4) | **4 / 1 DWORDs** |
| Set | Off | XYZ/W + first ST + second ST; **8 DWORDs/vertex** | **4 / 1 DWORDs** | **12 DWORDs** (3×4) | **4 / 1 DWORDs** |
| Either | On | XYZ/W + first ST + second ST; **8 DWORDs/vertex** | **12 / 2 DWORDs** | **12 DWORDs** (3×4) | **12 / 2 DWORDs** |

*Table scope:* the named CE 3D **blit** functions, not all 3D applications. “PS” values are uploaded ALU DWORDs / TAU DWORDs; the ALU uses four DWORDs per instruction under the VX855 manual. “VS” is a **candidate vertex instruction** destination on VX900 because the manual's overview calls subtype `31h` Vertex Shader Instruction; the detailed page reserves it. No run-time shader execution or output was measured.

**Static branches and source anchors:**

- H6 checks the operation bit at `0x10064BAE` and the global flag at `0x10064BB4`. It then constructs `04h:00h` as `0x00A80006` or `0x00A80008` and the attribute mask as `0x25F0C000` or `0x25F0CC00` (`0x10064BBD–0x10064C44`). Its separate long/short PS branch reads the **global flag** at `0x10064C58`, uploading from `0x100088B4` (12 ALU DWORDs) or `0x1000875C` (4 ALU DWORDs).
- H5S2 checks the same operation bit at `0x10065BFA` and the global flag at `0x10065C00`, and selects the matching input declaration and `04h:12h` input-register mask. It selects a Type-03 subtype-`31h` upload of 8 DWORDs (`0x1000870C`) or 12 DWORDs (`0x1000872C`) at `0x10065C14–0x10065CE3`.
- H5S2 **separately** checks the global flag at `0x10065CE5` and chooses the same 12- versus 4-DWORD PS program addresses as H6 (`0x10065CF9–0x10065DB8`). The TAU upload is likewise 2 versus 1 DWORD.

**Conclusion:** Adding the second ST vertex attribute requires an **additional H5S2 candidate VS instruction**, even when the pixel shader remains the *short* program. Thus the third VS instruction most plausibly handles the additional vertex/texture-coordinate interface; it is **not evidence that an extra pixel ALU instruction was moved to VS**. The earlier H5-versus-H5S2 texture-helper difference of three versus one PS ALU instruction (§61) is genuine, but is *not* sufficient by itself to establish a transfer of arithmetic between programmable stages.

<a id="c9-section-65"></a>

#### 65. New shared H6/H5S2 dynamic blend-factor pipeline

**Evidence scope:** the original VIA CE 6.0 retail `DDI_VIA.dll`, with SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`, matched by the verification script `Chrome9_CE_Command_Verification_65_68.py`. All conclusions here are based on *static x86 disassembly*; no CE driver or GPU has been executed. This expands §62, which compared *program selection* but had not decoded the per-operation blending factors.

Both `fn3DBlt_H6` (VX800/VX855) and `fn3DBlt_H5S2` (VX900) load the exact same x87 floating-point constant **255.0**, a 64-bit double stored at DLL VA `0x10008920` (`0000000000E06F40`, little-endian). The loads are at `0x10064617` (H6) and `0x100655A0` (H5S2).

They then use the same **eight-bit alpha input at the caller's `+0x34` byte offset**, but process it differently depending on the CE operation's multi-blend state:

| CE path | H6 instructions | H5S2 instructions | Calculated value | Destination |
|---|---|---|---|---|
| Normal/non-multi-blend control setup | `0x10064699–0x100646B0` | `0x10065623–0x1006563A` | `float(alpha / 255.0)` | Shared converter `0x100644A4`, then packed state/control words |
| Multi-blend shader setup | `0x10064D3A–0x10064D80` | `0x10065DC7–0x10065E0D` | `float(255.0 / alpha)` | Type-03 subtype-`22h` pixel-shader constant upload |

The distinction is proven by the x87 operations:

- **Normal path:** `FILD alpha` followed by `FDIV ST0,ST1`, with the live `255.0` in ST1; this computes `alpha / 255` before conversion to a 32-bit float.
- **Multi-blend path:** `FILD alpha` followed by `FDIVRP ST1,ST0`; with the live `255.0` in ST1 this computes `255 / alpha`, then stores a 32-bit float. This is **not** a normalization to the range [0,1].
- The condition leading into a multi-blend enable path is the same in both backends: `DWORD[input+0x30] == 3`, `DWORD[input+0x38] != 0`, and `BYTE[input+0x34] != 0xFF`. H6 checks this at `0x10064639–0x10064652` and H5S2 at `0x100655C2–0x100655DB`. Other branches can reset the global state, so this is a documented **enable condition at these call sites**, not a claim about every GDI draw.

**Interpretation:** These are **shared command-generation algorithms** despite VIA using different H6 and H5S2 overall pipelines. For a unified Windows 98 driver, the relevant draw-state builder and alpha conversion can be common code, while the destination of the result depends on the chosen render path. Do **not** transfer a `255/alpha` value into a conventional `alpha/255` blend register.

<a id="c9-section-66"></a>

#### 66. The normal alpha path uses a reusable 10-fractional-bit conversion helper

Both paths call the **same function**, `0x100644A4`, after storing the normalized value as IEEE-754 single precision. Its static x86 logic extracts the float exponent and significand, shifts the significand, and returns an unsigned fixed-point factor with **10 fractional bits**: `0x000` for zero, around `0x200` for half, and `0x400` for unity. The unity representation `1024` **requires 11 storage bits**; calling this merely an '8-bit' or '10-bit integer' conversion would be misleading.

The compiled helper has a special value `0x400` at `0x100644DB`, and an exact-unity case at `0x100644D7`. For inputs from an eight-bit nonnegative alpha channel after `float(alpha / 255.0)`, its behavior agrees with truncating approximately `alpha/255 × 1024` to the integer fixed-point representation (subject to single-precision rounding):

| Input byte alpha | Fixed-point result | Hex |
|---:|---:|---:|
| 0 | 0 | `000` |
| 1 | 4 | `004` |
| 64 | 257 | `101` |
| 127 | 509 | `1FD` |
| 128 | 514 | `202` |
| 254 | 1019 | `3FB` |
| 255 | 1024 | `400` |

The values above were checked with IEEE-754 single-precision intermediate rounding; they are *computed checks* of the static algorithm, **not register readbacks**. They establish a portable implementation pattern for converting eight-bit alpha to VIA's fixed-factor control words. The CE function also packs the converted factor into multiple following control words; those end-to-end destination bitfields still need operation-specific manual validation.

**One edge case to investigate rather than assume away:** the multi-blend enable comparison rules above exclude alpha `0xFF` but do **not themselves exclude alpha `0x00`**. If that value reaches the `255.0/alpha` branch, the floating-point calculation is nonfinite when floating-point exceptions are masked. Static inspection is insufficient to say whether the caller rejects zero, whether the shader consumes the constant, or whether this causes a visible error. A future CE-level controlled test can check zero alpha against opaque alpha without touching the Windows 98 driver's normal drawing path.

<a id="c9-section-67"></a>

#### 67. Exact shared Type-03/22h pixel-shader constant upload

The VX855 Part II manual describes **Type-03 subtype `22h`** as the pixel-shader constant-register interface: four successive 32-bit floating components per constant register, converted internally to the hardware's 24-bit float format. In **both** compiled H6/H5S2 multi-blend branches, the driver emits an `FE03` packet to stream the same conceptual four-value constant:

```
Type-03 subtype 22h, starting constant register 0:
    [0] float32(255.0 / alpha_byte)   # dynamically calculated
    [1] 0x3F800000                   # 1.0
    [2] 0x00000000                   # 0.0
    [3] 0x00000000                   # 0.0
```

- H6: selector `0x22030000` at `0x10064D63`, dynamic first value at `0x10064D6D–0x10064D72`, unity word at `0x10064D76`.
- H5S2: selector `0x22030000` at `0x10065DF3`, dynamic first value at `0x10065DFF`, unity word at `0x10065E03`.
- Shared constant `0x3F800000` is loaded at `0x10064D33` / `0x10065DC0`.

**Why it matters:** earlier work established that H6/H5S2 upload the same *compiled short/long pixel programs*. This result additionally confirms that a **dynamic, operation-dependent shader constant** uses the **same format and calculation** on both families. Pixel program identity alone does not reproduce the rendering operation; the shader's constant upload and selection state must also match. The older VX855 manual defines the constant layout; the VX900 detailed subtype is reserved despite the CE driver's demonstrable use of it, so equality of every shader constant feature is not implied.

<a id="c9-section-71"></a>

#### 71. A byte-exact integer form of VIA's shared 8-bit-alpha conversion

Both CE 3D blit backends call the same float-to-fixed helper at `0x100644A4`, after calculating `float32(alpha_byte / 255.0)`. The helper extracts IEEE-754 exponent/mantissa and produces a fixed-point value with ten fractional bits, including `0x400` (1024) for unity.

**New exhaustive static-model check:** for **every** integer alpha `a` in `[0,255]`, emulating the helper's bit shifts after the single-precision intermediate gives the same output as this simpler expression:

```c
/* Exact equivalent for the CE helper's 0..255 byte-alpha input domain. */
static unsigned chrome9_alpha_u8_to_q10(unsigned a)
{
    /* Precondition: a <= 255; result is 0..1024 (11 bits). */
    return (a * 1024u) / 255u;
}
```

Reference points: `a=0→0`, `1→4`, `64→257`, `127→509`, `128→514`, `254→1019`, `255→1024`. The helper and integer calculation agree for the entire 256-value domain, not merely the examples. This offers a **floating-point-free way to reproduce the CE blit factor**, but it is only one conversion per operation; do not assume a material FPS improvement without profiling.

**Do not generalize the original helper into a saturating arbitrary-float converter.** Its branch structure behaves unexpectedly for some *out-of-domain* values: the same static model yields `2.0→0`, `2.5→1024`, `4.0→0`. Those inputs are never produced by the inspected `alpha_byte/255` path. These are API/precondition cautions, **not demonstrated rendering bugs**.

**Zero-alpha uncertainty retained from §66:** the examined enable-condition sequence compares alpha against `0xFF`, not zero. When its global multi-blend state is enabled, the code may compute `255.0/alpha`; no general entry-point proof that zero reaches that branch has been established. This remains a focused test case, not a confirmed crash or correctness defect.

<a id="c9-section-74"></a>

#### 74. Normalized static command-template comparison: H6 versus H5S2

**Why this pass:** §73 introduced an offline decoder for CE `FE00–FE07` packet envelopes. The present pass advances the proposed normalized comparison **without inventing a runtime trace**: it extracts four real compiled constant tables from the original retail CE DLL and verifies that **both `fn3DBlt_H6` and `fn3DBlt_H5S2` reference the same addresses**. A runtime branch still determines which table is emitted. These are *static packet payload fragments*, not a complete captured draw-call stream.

**Source identification:** VIA CE retail `DDI_VIA.dll`, SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`. Preferred DLL image base `0x10000000`. Read-only verification: `Chrome9_CE_Static_Template_Compare.py`; machine-readable diff: `Chrome9_CE_Static_State_Template_Diff.csv`.

| Static table (retail VA) | Words | H6 reference | H5S2 reference | Evidence |
|---|---:|---|---|---|
| `0x100084F8` | 50 | `0x1006465C` | `0x100655E5` | Same source address in `MOV ESI, imm32`, then `REP MOVSD` |
| `0x10008820` | 37 | `0x100647A8` | `0x10065733` | Same source and 37-DWORD copy |
| `0x10008770` | 12 | `0x1006480E` | `0x10065799` | Same short pixel-shader setting table |
| `0x100088EC` | 12 | `0x10064807` | `0x10065792` | Same alternative pixel-shader setting table |

**Result:** For these four templates, there is **zero generation-specific payload difference** in the CE DLL. This does **not** mean every emitted GPU register is identical: dynamic render target values, vertex declarations, candidate VS instructions, extra H5S2 packets and synchronization still differ, as documented in earlier sections.

<a id="c9-section-75"></a>

#### 75. Exact difference between 50-DWORD and 37-DWORD Type-01 state templates

The compact 37-DWORD template is a **multiset subset** of the 50-DWORD template: **all 37 compact words occur unchanged** in the extended template, but the emission order is not identical. The 50-DWORD template adds exactly **13 words**; there are no different values for any shared word.

| Extra Type-01 address | Encoded word(s) | Register significance in VX855 Part II |
|---|---|---|
| `00h` | `0000000F` | `Enable Setting 1`; bits 0–3 enable dithering on RT0–RT3 |
| `34h`–`3Fh` | `34000000`, `35000000`, ... through `3F000000` | Twelve **Alpha Setting** registers reset to zero |

The alpha words appear in a nonascending order in the actual 50-DWORD template. Their presence is a **state-coverage difference between CE code paths**, *not* a H6-vs-H5S2 distinction. It may matter when investigating residual alpha/blending state after switching operations: a draw that does not program a register can inherit previous state, depending on other initialization. But **no stale-state bug is established** by this static comparison.

**Manual basis:** `vx855_2.pdf`, `HParaType 01h` `Enable Setting 1` (printed p.78) and `Alpha Setting 2–13` (around printed pp.90–94). The VX900 manual contains corresponding alpha-state register definitions; the CE binary provides direct cross-generation evidence of the values. The register `01h:00h` has more fields than dithering; only the four low bits are set by `0000000F`.

<a id="c9-section-76"></a>

#### 76. Two shared 12-DWORD pixel-shader setting templates: four state-word differences

Both H6 and H5S2 use **the same short-versus-long selection** for Type-01 addresses `90h`–`9Bh`. The tables have 12 words apiece. **Eight entries are byte-for-byte identical and four differ**:

| Type-01 subaddress | Short (`0x10008770`) | Alternate (`0x100088EC`) | VX855 manual observation |
|---|---|---|---|
| `91h` | `91000001` | `91000003` | Adds bit 1 to input-to-TAU mask (texture register 1 in addition to 0) |
| `92h` | `92010001` | `92020003` | Encoded TAU-length field `[21:16]` 1→2; ALU-length field `[8:0]` 1→3 |
| `93h` | `93000001` | `93000002` | Changes TAU-to-ALU switching bit 0→bit 1 |
| `9Bh` | `9B000000` | `9B000001` | **Not defined** in the VX855 manual's `90h–9Ah` Pixel Shader Setting range |

All the other words at `90h`, `94h`–`9Ah` match exactly between the two 12-DWORD tables. In VX900 Part II the detailed `01h:90h–9Ah` block is marked *reserved*, even though the CE driver emits it. For `9Bh`, its **meaning is not established by any of the three supplied manuals**. Do not assert that bit 0 means a particular mode or copy it into a new driver solely on the strength of a CE blit.

**Control-flow evidence:** H6 compares `DWORD[0x100CE7EC]` at `0x100647FF` and chooses `0x100088EC` when nonzero, otherwise `0x10008770`. H5S2 performs the equivalent test at `0x1006578A`. Earlier H6/H5S2 paths can set this global after a multi-blend eligibility check, and other paths clear it. This establishes that **pixel-shader setting-table selection is shared software logic**, not that every possible operation depends only on the three currently inspected input fields. Its state lifetime should be traced before extracting the encoder into new code.

**Manual basis:** `vx855_2.pdf` `HParaType=01h`: `91h` Pixel Shader Setting 2, `92h` Setting 3, `93h` Setting 4 (printed pp.110–111); `vx900_2.pdf`: `90h–9Ah` reserved (printed p.120). The 12-DWORD table additionally writes `9Bh`, not documented in the named VX855 range.

<a id="c9-section-77"></a>

#### 77. Concrete cross-generation reuse boundary and next static steps

The recovered CE data supports sharing a **state-template inventory and formatter**, provided it preserves the original command ordering and draws in the same logical modes. The formatter must still parameterize/chip-select:

1. **H5S2 vertex input and VS upload** (`Type-03/31h`, `Type-04/01h–02h`, and undocumented `Type-04/60h–62h`), as already described in §§52–68.
2. **Dynamically calculated memory addresses, pitches, dimensions, and alpha constants**, instead of substituting values from a static table.
3. **Packet transport and waits**, especially the VX900-selected wait helper and separately documented command-regulator differences.
4. **State inheritance across draw calls**: a 37-DWORD state subset omits 13 explicit writes present in the larger template. Determine whether those omitted values are guaranteed to have been set previously before omitting them in a new driver.

The available MSI does **not** contain matched runtime command-buffer captures. The current comparison therefore concludes only that four literal templates are reused by both compiled backends and identifies their exact contents. A full draw-by-draw normalized diff still requires additional symbolic value propagation or a controlled CE capture; no live GPU equivalence is claimed.

**Reproducible companion files:** `Chrome9_CE_Static_Template_Compare.py` validates the retail DLL hash, both backends' source-table references, the 37/50 subset relation, all 13 additional state words, and the four pixel-shader control differences. It optionally writes `Chrome9_CE_Static_State_Template_Diff.csv` (62 classified rows). It does **not** load or execute the DLL, construct a runnable GPU command, or access MMIO.

<a id="c9-section-78"></a>

#### 78. Branch-exact static H6/H5S2 state-template selection

**New verification:** The 50-DWORD versus 37-DWORD Type-01 template selection is not merely similar: the *same three field checks and control-flow pattern* appear in both compiled blit functions. The source here is the original retail CE 6.0 `DDI_VIA.dll` (SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`), not a simulated GPU draw.

| Check / action | H6 code VA | H5S2 code VA | Recoverable meaning |
|---|---|---|---|
| `cmp dword ptr [esi+0x30],3` | `0x10064639` | `0x100655C2` | Inspect operation mode value **3**; symbolic mode name not yet recovered |
| `cmp dword ptr [esi+0x38],0` | `0x1006463F` | `0x100655C8` | Inspect an additional operation field; exact API meaning unresolved |
| `cmp byte ptr [esi+0x34],0xFF` | `0x10064645` | `0x100655CE` | Check fully opaque 8-bit alpha |
| Set global at `0x100CE7EC` to `1` | `0x1006464B` | `0x100655D4` | Selected for mode 3, extra field nonzero, and alpha != 255 |
| Clear global and choose 37 DWORDs | `0x10064691–0x100647A8` | `0x1006561B–0x10065733` | Selected for mode 3 when extra field == 0 **or** alpha == 255 |
| Choose 50 DWORDs | `0x10064655–0x1006466B` | `0x100655DE–0x100655F1` | Selected for all other branches, including mode != 3 |

This can be expressed as **reconstructed control flow**, not as official VIA API semantics:

```c
if (operation_mode == 3) {
    if (extra_field_38 == 0 || alpha_u8 == 255) {
        shared_blend_flag = 0;
        use_compact_37_word_template();
    } else {
        shared_blend_flag = 1;
        use_extended_50_word_template();
    }
} else {
    use_extended_50_word_template();
    /* This comparison block does not explicitly change the global flag. */
}
```

**Important subtlety:** The mode-`!=3` path does not visibly reset the shared global in these particular prologues. Other entry points or prior operations may manage it. This is **not** sufficient to assert a stale-state bug, but a new Win98 emitter should explicitly derive per-operation shader/alpha state rather than assume a process-wide mutable flag always starts cleared. Relevant follow-on state changes are the shared Type-01 `91h/92h/93h/9Bh` table choice previously decoded in §76. The extended template explicitly clears twelve alpha-setting registers that the compact template omits (§75); *which template is chosen* therefore matters to preservation of state.

<a id="c9-section-81"></a>

#### 81. Machine-verifiable static stage matrix and limits

Two companion artifacts were generated from the **original retail DLL** and passed verification:

- `Chrome9_CE_Normalized_Blit_Control_Flow.py`: checks the retail DLL SHA-256, byte-exact predicate instructions, 50/37 state templates and 12-word PS-control table references, legacy versus VX900 declaration words, common primitive words, the shared 32-word table source, VX900 `60h–62h` variants, candidate VS selectors, and manual-defined vertex-descriptor field decoding. It reads the DLL from `VIA_CE_Chrome9_Selected_Driver_Analysis_Files.zip` by default or accepts `--dll` with an extracted retail DLL.
- `Chrome9_CE_H6_H5S2_Static_Blit_Stages.csv`: ten normalized static stages, with H6 and H5S2 disassembly addresses, classified as common or generation-specific. It is deliberately **not** labeled a dynamic GPU command capture.

Example offline reproduction:

```sh
python Chrome9_CE_Normalized_Blit_Control_Flow.py \
  --zip VIA_CE_Chrome9_Selected_Driver_Analysis_Files.zip \
  --csv Chrome9_CE_H6_H5S2_Static_Blit_Stages.csv
```

**Result of verification:** all assertions passed against SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19` in the uploaded CE package. Neither the Python script nor this analysis executed CE code or accessed hardware. It establishes a static common/exception ordering, not exact runtime values for every dynamic texture address, pitch, vertex-coordinate word, or queued packet. The next decisive evidence would be a CE trace with controlled, identical blit inputs on two chipsets or a well-scoped symbolic emulation of both packet builders; neither is supplied in the MSI.

<a id="c9-section-82"></a>

#### 82. Full direct-reference audit of the shared `bGdiMultiABlend` global

**Scope and provenance.** Read-only static scan of the *original* Windows CE 6.0 retail `DDI_VIA.dll`, SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`. The included retail map names `bGdiMultiABlend` at preferred VA `0x100CE7EC` in `Present3DBlt.obj`. This does not imply the same memory address or software policy is used in the Windows 98 driver.

Scanning the complete retail PE file for the four-byte absolute address, then checking the surrounding x86 instructions, yields **27 direct references**, all within `fn3DBlt_H6` or `fn3DBlt_H5S2`:

| Direct instruction class | Occurrences | Meaning in inspected code |
|---|---:|---|
| `MOV dword ptr [0x100CE7EC], 1` | 2 | One set-to-one in each backend |
| `XOR EDI, EDI; MOV [0x100CE7EC], EDI` | 2 | One clear-to-zero in each backend |
| `CMP [0x100CE7EC], ...` | 23 | State reads only |
| **Total** | **27** | **Four writes and 23 reads** |

The exact write locations are **H6** `0x1006464B` (set) and `0x10064693` (clear), and **H5S2** `0x100655D4` (set) and `0x1006561D` (clear). This is stronger than merely observing that one branch does not clear the flag: **there is no other *direct-address* writer in this retail build**. An indirect or dynamically computed write is not ruled out by a literal-reference scan, and no CE runtime trace was supplied.

Both backends have byte-identical decision predicates, with their respective function arguments at `ESI`:

```c
/* Simplified state update reconstructed from H6/H5S2 assembly.
   This describes only the examined CE routines, not the CE public API. */
if (operation->mode == 3) {
    bGdiMultiABlend =
        (operation->extra_at_0x38 != 0 && operation->alpha_at_0x34 != 255)
        ? 1 : 0;
}
/* If mode != 3, the examined prologue does not assign the global. */
```

Assembly evidence: H6 `0x10064639–0x10064693`, H5S2 `0x100655C2–0x1006561D`. The identifiers `mode` and `extra` are **descriptive placeholders**: the actual CE `KMD_3DBLT_DATA_tag` field semantics are not fully recovered. `alpha_at_0x34` is accessed as a byte; `extra_at_0x38` is tested as a DWORD.

##### Why the lifetime matters to shared command programming

The flag is read again **later in the same draw** and influences command selection in *both* backends:

- H6 `0x100647FF` / H5S2 `0x1006578A`: choose one of the same 12-DWORD Type-01 pixel-shader control templates (the `91h/92h/93h/9Bh` differences from §76).
- H6 `0x10064B30–0x10064B3D` / H5S2 `0x10065B38–0x10065B45`: the per-operation flag `BYTE[argument+0x1C] & 0x20` **or** nonzero global blend flag chooses an extended vertex/texture-attribute path.
- H5S2 `0x10065CE5`: the flag also selects longer Type-03 subtype `20h/21h` pixel-shader programs; at the earlier input-selection stage the flag can also cause the 12-DWORD Type-03/`31h` candidate VS program to be used instead of the 8-DWORD version.
- H6 `0x10064D2D–0x10064D84` / H5S2 `0x10065DBA–0x10065E13`: a nonzero global triggers the dynamic Type-03/`22h` reciprocal-alpha constant upload (§65–68).

**Conclusion:** Shared command generation is not solely a function of the current per-operation arguments in this CE implementation; some emitted commands also depend on a retained, mutable process-global setting. The shared *hardware formats* remain the same; the *selection of the formats* has software state.

<a id="c9-section-83"></a>

#### 83. Controlled two-call state model and targeted regression

The above control flow permits the following **static-model sequence**, assuming the two function calls run sequentially and no other code changes the global between them:

| Stage | Input | Global after prologue | Consequence for subsequent command decisions |
|---|---|---:|---|
| Initial | Assumed cleared (`0`) | 0 | Short path possible if per-operation `0x20` is also clear |
| Call A | `mode=3`, `extra_at_0x38=1`, `alpha=128` | **1** | Multi-blend shader-control mode; extended vertex path |
| Call B | `mode=1`, `BYTE[+0x1C]&0x20=0` | **still 1** | Extended vertex path and long shader setup can remain selected despite the second operation's bit being clear |
| Call C | `mode=3`, `extra_at_0x38=0`, `alpha=255` | **0** | Short path can be selected again |

This is a **predictive path through the compiled branches**, not a claim that the CE API actually sends these combinations or that the pixels are wrong. A CE caller could intentionally rely on the inherited mode. We have **not** proved a stale-state rendering bug or a race condition.

For a self-contained future regression, record at least: operation mode (`+0x30`), extra field (`+0x38`), byte alpha (`+0x34`), operation flags (`+0x1C`), global before/after, chosen 37-/50-DWORD state template, pixel-shader-control table, vertex-word count, and candidate-VS instruction count. Run the order **A→B** and compare it with **C→B**. This isolates the global state from the input flags. Only a CE test harness or an instrumented working driver can prove which sequences are legal and what the GPU displays.

A separate test case is **`alpha=0` while the global is nonzero**. The code contains an x87 `255.0 / alpha` path in both backends. That warrants a *nonhardware* boundary check and then a carefully isolated CE runtime test; it is **not** a demonstrated crash or exception. The observed result would depend on FPU exception masking, parameter validation, and rendering behavior.

For a new Windows 98 implementation, prefer **per-draw explicit shader/vertex state derivation**, not an unexplained persistent global copied verbatim from the CE driver. Retain the CE branch behavior as a reference for compatibility testing, not an unproven feature requirement.

<a id="c9-section-85"></a>

#### 85. Reproducible audit and remaining research priorities

**Companion script:** `Chrome9_CE_Blend_Flag_State_Audit.py` reads the original retail `DDI_VIA.dll`, confirms its SHA-256, audits all 27 direct address references, verifies exact H6/H5S2 predicates and the Type-04 `60h/61h/62h` short/long literal writes, and models the three-call flag sequence above. It neither executes the CE DLL nor interacts with graphics hardware.

Offline reproduction:

```sh
python Chrome9_CE_Blend_Flag_State_Audit.py --dll retail/DDI_VIA.dll
```

**Checks passed** on the user-supplied retail driver: 27 direct references, precisely two set-one writes and two clear-zero writes, 23 compares, matching predicates, and the Type-04 short values reused in the VX900 texture helper. Results are from one CE build, not guaranteed to hold in other driver revisions.

The most valuable next evidence is (1) a CE GDI call-site trace for `KMD_3DBLT_DATA_tag` fields and the `mode=3` dispatch to establish whether the predicted flag inheritance can actually occur, and (2) a different VX900 shader program to distinguish the candidate Type-03/`31h` instruction operand bits from the undocumented Type-04 `60h–62h` controls. The present two-program sample does not support a complete ISA decoder.

<a id="c9-section-88"></a>

#### 88. Conditional alpha-zero path now connected to exact instructions

The earlier state-lifetime result (§82–83) found that a mode-3 draw may set global `bGdiMultiABlend=1`, and a subsequent non-mode-3 draw does not reset that global in the analyzed function prologue. Both backends' dynamic Type-03 subtype `22h` constant generators have the same local structure:

| Component | H6 address | H5S2 address |
|---|---|---|
| Read the retained blend flag | `0x10064D2D` | `0x10065DBA` |
| Read current operation byte-alpha at `[arg+34h]` | `0x10064D3D` | `0x10065DCA` |
| x87 `FDIVRP ST(1),ST(0)` computing `255.0 / alpha` | `0x10064D47` | `0x10065DD4` |
| Start the conditional Type-03/`22h` upload | `0x10064D57` | `0x10065DE7` |

No explicit **zero-alpha guard** occurs in the immediate guarded sequence from the global check to the `FDIVRP`. Therefore, **if** a caller legally supplies zero alpha while the retained global is nonzero, the code reaches a divide-by-zero floating-point operation. Depending on x87 exception masks, this can yield infinity rather than a CPU exception; shader interpretation and pixel output are not established.

The static code does **not** establish whether that parameter combination can reach this routine from a valid CE GDI call, or whether an upstream validator prevents it. This is a **conditional test case, not a confirmed bug**. For a new driver, explicit alpha-zero handling is preferable to inheriting a CE state dependency without understanding its API constraints.

Recommended nonhardware test: model A (`mode=3`, `extra!=0`, `alpha=128`) followed by B (`mode!=3`, `alpha=0`) and compare with a clearing mode-3 call immediately before B. For actual CE testing, additionally log the caller API, serializing lock ownership, current blend global, selector table, and emitted Type-03/`22h` constant; do not assume a deterministic failure mode.

<a id="c9-section-90"></a>

#### 90. New build-pinned static verification and remaining blockers

The companion script **`Chrome9_CE_Dispatch_and_Submission_Audit.py`** re-reads the original retail DLL, checks SHA-256, verifies the chipset ID subtraction chain and installed callback pointers, checks the common H6/H5S2 wait callback, confirms CE critical-section helper calls, checks that both command-pointer/blend writes occur before their submission locks, and verifies the conditional x87 reciprocal-alpha instruction sequences. It neither executes the driver nor touches graphics hardware.

```sh
python Chrome9_CE_Dispatch_and_Submission_Audit.py --dll retail/DDI_VIA.dll
```

**Result:** All static checks passed against the original retail CE DLL. These facts remain unknown without additional evidence: which CE public/GDI callers permit each `KMD_3DBLT_DATA_tag` mode; whether any outer lock serializes the entire draw; legal alpha-zero behavior; what VX900's undocumented `04h:60h–62h` bits do at the silicon level; and the instruction semantics of the Type-03 subtype `31h` programs. No Windows 98 driver source or hardware was modified in this pass.

<a id="c9-section-333"></a>

#### 333. VX900 *reserves* the older pixel-shader enable and control fields in its detailed register chapter

A more consequential contrast occurs **outside Type-02**. The VX800 and VX855 manuals define Type-01 **`01h[10]` as Enable Pixel Shader** and Type-01 **`90h–9Ah` as pixel-shader settings** (firing mode, source/register dependencies, ALU-to-TAU switching etc.). The VX900 detailed chapter instead labels **Type-01 `01h[10]` reserved** (printed p91) and **Type-01 `90h–9Ah` reserved** (printed p120, visually confirmed in PDF p126).

But this **cannot be interpreted as proof that VX900 lacks the old shader hardware**, because the original VIA CE `fn3DBlt_H5S2` program actually selects the same Type-01 state and PS control tables as `fn3DBlt_H6`. The common state table at **VA `0x100084F8`** contains the command word `01CE1540`, **whose Type-01 low payload sets bit 10**, and both compiled blit backends point to the same Type-01 PS-control tables at VAs **`0x10008770` (12 DWORDs)** and **`0x100088EC` (12 DWORDs)**. The short table begins `90018000 91000001 92010001 93000001`; the extended table begins `90018000 91000003 92020003 93000002`. Both tables include a `9Bh` word; the specific VX900 manual contradiction verified here is explicitly `90h–9Ah`, not a complete decode of `9Bh`.

**Conclusion:** the VX900 public *detailed* register specification is **incomplete, deliberately reserved, or otherwise inconsistent with actual VIA H5S2 writes** in the compared path. The reason and actual silicon semantics remain unresolved. We can confidently say software **writes the old command words**, but not assert that all old Type-01 pixel-shader controls have identical effects on VX900.

<a id="c9-section-334"></a>

#### 334. VX900 also labels Type-03 `20h–22h` reserved while its own command summary names them

The **VX800/VX855** manuals explicitly describe Type-03 subtype **`20h` as pixel ALU instructions** (four DWORDs per ALU instruction; up to 96), **`21h` as 32 32-bit TAU instructions**, and **`22h` as four-component floating constant registers**, with conversion to internal 24-bit floating format. The VX900 detailed chapter instead states **“HParaType = 03h (HParaSubType = 20h–22h): Reserved”** (printed p153, visually verified on PDF p159). However, the **same VX900 manual's command-transmission type list on printed p71 explicitly names `20h` = pixel ALU, `21h` = TAU and `22h` = constants**.

The CE binary removes any doubt about *software usage*: **both H6 and H5S2 blitters reference the same machine-code data tables** for compact PS ALU (**four DWORDs** at VA `0x1000875C`), extended PS ALU (**twelve DWORDs** at `0x100088B4`), and TAU (**two DWORDs** at `0x100088E4`), with a conditional Type-03 `22h` constant emission. The byte identities are backed by SHA-256 checks against the original CE retail DLL and anchored disassembly in §§271–280.

The compact four-DWORD PS ALU payload is:

```text
01C00530 301C0053 00F20005 000D0820
```

**Evidence distinction:** this is a **three-way programming-document contradiction with direct compiled H5S2 usage**, not a decoded shader ISA or a hardware conformance test. The VX900 detailed manual's reserved status must not be used to claim `20h/21h/22h` are absent from the silicon, nor should a matching compact blit be used to prove arbitrary legacy shader programs work unchanged.

<a id="c9-section-335"></a>

#### 335. What can and cannot be generalized from the CE H6/H5S2 pixel program sharing

**Verified same source data:** the two blit paths reference the same short and extended PS-control arrays (12/12 DWORDs), PS ALU arrays (4/12 DWORDs) and TAU source; conditional constants are generated by corresponding code. Those are **actual byte-for-byte program/control template matches**, not only matching symbolic register names. Independently, the two older family PCI IDs use the **same compiled H6** helper for the examined draw path (§§291–299). VX900 adds a separate vertex-input and vertex-shader path (§§271–290), so end-to-end command buffers **are not identical**.

**Not verified:** shader binary opcode semantics; arbitrary H6 PS uploads executing on VX900; PS register reset values/ignored bits; runtime texture/fence/cache state; shader timing, precision, filtered texel output and framebuffer pixels. In particular, matching the same program words under different H6 and H5S2 vertex-processing paths can produce a correct blit without establishing that the full programmable PS command space is interchangeable.

For a **portable common subset** based only on strong existing evidence, prefer the explicitly common Type-02 texture/sampler settings; keep VX800 `FEh:00h[8]` off on the newer chips; and qualify the VX900 PS writes as **VIA CE observed but not detailed-manual guaranteed**. Keep depth-PCF/border rounding and reserved register bits on an unresolved verification list.

<a id="c9-section-336"></a>

#### 336. Texture and pixel-shader classification

**25-row matrix verdict:**

- **Shared documented Type-02 foundation:** stage base addresses, dimensions/tiling, common texture-format groups and examples, sampler/filter/LOD/mapping fields, color border register packing, and global texture FIFO/count/cache controls.
- **Published differences:** VX800 `FEh:00h[8]` projected-texture mantissa bit (reserved on both newer chips); VX800/VX855/VX900 border-filtering pseudocode differs and VX900 explicitly adds a Z/PCF branch. Texture-stage location `11b` remains reserved on all three, even where VX900 enables L.L. in separate register families.
- **Highest-value contradiction:** VX900 detailed manual reserves Type-01 `01h[10]`, Type-01 `90h–9Ah`, and Type-03 subtype `20h–22h`, while its own summary names PS instructions and the independently verified **H5S2 CE backend actively reuses H6's PS words**. This is **documented-versus-compiled behavior**, not a proven full pixel-shader compatibility contract.
- **Evidence remaining:** representative paired H6/H5S2 pixel results under controlled texture border, shadow-map/PCF, and uncommon compressed/float formats; independent proof of the meaning of reserved VX900 shader fields; exhaustive register enum verification. No device tests were executed here.

**Artifact/verification:** `Chrome9_Texture_Sampler_Three_Family_Audit_327_336.py` (read-only original VIA DLL SHA-256 check, three-manual assertions, *five* table-payload SHA-256 checks, H6/H5S2 identical source-table references, and 25 matrix classifications) and `Chrome9_Texture_Sampler_Three_Family_Matrix_327_336.csv`. Both were revalidated after this append.

**Next prioritized research area:** the **three-way display/video/HQV generation comparison**, particularly register blocks and the VX900 HQV source-material ambiguities. This should be treated separately from the now documented common 3D texture/PS template and from the older Windows 98 performance investigations. Do not equate separate codec/filter/video capabilities with 3D register compatibility without a source-to-register map.

<a id="command-interface"></a>

## Command transport and regulator

The FE software interpreter, command-regulator selectors and AGP fetcher are separate interfaces. FE03 writes its selector to the header-selected address and its data repeatedly to that address plus four. VX900 changes Type-10 dispatch and pause/jump semantics and several Type-11 fields.

### Packet framing, MMIO and synchronization commands

<a id="c9-section-6"></a>

#### 6. Command and parameter transport

##### 6.1 Common model, different configurations

The three manuals describe a **3D register space beginning at MMIO offset `0x400`** with command/parameter setup and `HParaType` dispatch. Important types include:

| HParaType | General intent |
|---:|---|
| `00h` | Primitive vertex data or vertex indices |
| `01h` | Non-texture 3D state |
| `02h` | Texture / sampler stage state |
| `03h` | Palette, shader-related instructions/constants, or VB descriptions depending on platform/subtype |
| `04h` | Vertex format and primitive setup |
| `10h` | Command regulator controls |
| `11h` | Command regulator / swapping / miscellaneous controls |
| `08h` | VX900-specific CDC/performance counter group in supplied documentation |

**Evidence:** [V8-II, pp. 57–60 and 148–152]; [V855-II, pp. 62–76]; [V9-II, pp. 70–88].

The VX800 manual explicitly describes different configuration ports: writing CR via `Rx1C` versus enabling the 3D clock using the `Rx3C` window. The command/address organization needs to be interpreted relative to the documented 3D base, not as arbitrary status registers. [V8-II, pp. 57–59].

**Warning for ring experiments:** An address such as MMIO `0x43C` corresponds to `0x400 + 0x3C` in the 3D programming window in this documentation. It is not, **by address alone**, proof of a documented DMA-ring consumer/read pointer. Interpret any `0x43C` readback or write through the device's actual register definition and the working code path. An unchanging observed value does not conclusively prove that a proposed ring engine consumed nothing.

##### 6.2 VX855 ↔ VX900 CR collision table

These are **same HParaType/sub-address, different meaning** cases:

| Parameter word | VX855 | VX900 | Porting hazard |
|---|---|---|---|
| `10h:04h` | Interrupt-state-buffer base/control | CR command-dispatch configuration | **High:** corrupts control state if copied |
| `10h:05h–07h` | Interrupt-related controls | Marked reserved | **High:** remove legacy writes |
| `10h:08h–0Eh` | Not the corresponding VX900 group | VX900 preemption/related controls | **High:** new scheduler state |
| `11h:00h` | Misc control: FIFO depth, request lock, critical command bit | E3/E2 lock controls, hardware locking bit 17 | **High:** different bit meaning |
| `11h:03h` | No equivalent defined fence configuration at this word | Fence wait-source configuration | **High:** VX900-specific fence setup |

**Evidence:** [V855-II, pp. 68–76]; [V9-II, pp. 80–89].

**Concrete example:** VX855 `11h:00h` bits 10:8, 7:5, 4:2 select three CR FIFO depths; bit 0 marks subsequent commands critical/nonbreakable. VX900 instead defines bit 17 as hardware 2D/3D lock enable (documented default 1) and bit 16 as the request-lock control. If copied verbatim, a VX855 FIFO-configuration word would not configure the same FIFOs on VX900.

##### 6.3 VX900 fence capability

At `HParaType=11h`, sub-address `03h`, VX900 documents flags for waiting on 3D, 2D, video, HQV, individual DMA channels and EOF conditions; bit 0 requests waiting for all engines idle. [V9-II, p. 88].

**Inferred opportunity:** A per-engine fence might replace inefficient or unsafe polling in the VX900 backend. It is *not* proof of a synchronous CPU-visible completion event, nor does it establish that fences cure the VX855 hangs. The command-stream and readback/notification mechanism still need testing.

**Recommended implementation:** map high-level operations (`wait_3d`, `wait_2d`, `wait_dma`, `flush`, `submit`) to family-specific packet templates, each with a fixed timeout and traceable sequence number. Preserve command ordering barriers around producer/consumer operations.

##### 6.4 Proposed trace format

```
family, pci_id, mmio_bar, fb_bar, sequence, batch_id,
hpara_type, sub_type, sub_address, payload,
submit_cpu_ticks, wait_cpu_ticks, result,
2d_status, 3d_status, previous_batch, recovery_count
```

Separate **hardware submission time**, **CPU wait time**, and **overall frame time**. It is possible to have a slow application with no measurable hardware wait, or a stalled GPU with little per-call CPU cost until a later synchronization point.

<a id="c9-section-27"></a>

#### 27. Engine status and synchronization: source-derived register masks

OpenChrome groups VX800/VX855/VX900 under these MMIO `0x400` masks [OpenChrome `src/via_regs.h:L193–L214`; `src/via_exa.c:L465–L478`]:

| Name | Value | Usage in source |
|---|---:|---|
| `VIA_CMD_RGTR_BUSY_H5` | `0x00000010` | Regulator-busy mask |
| `VIA_2D_ENG_BUSY_H5` | `0x00000002` | 2D busy |
| `VIA_3D_ENG_BUSY_H5` | `0x00001FE1` | Aggregate 3D busy bits; **not just a single bit** |
| `VIA_VR_QUEUE_BUSY_H5` | `0x00000004` | Virtual queue busy (named in header) |

The primary shared idle check uses `(CR_BUSY | 2D_BUSY | 3D_BUSY)`, with a bounded polling loop. A different code path checks only regulator and 2D while processing Header-1 writes [OpenChrome `src/via_exa.c:L75–L81`, `L472–L478`].

**Useful for VX855 loggers:** record full status `0x400`, a decoded `CR_BUSY`, `2D_BUSY`, `(status & 0x1FE1)` and `VQ_BUSY`, both before and after each submission. A busy flag may not correspond to “one accepted triangle”, and a quiet flag does not prove that a particular command or surface is correct.

**Potential limitation:** some OpenChrome wait loops stop at `MAXLOOP` without an explicit device-reset/recovery protocol in the shown function. Do not transfer that omission into your Win98 driver; retain your independent timeout/recovery procedures and capture the last active batch. This is an engineering review of the shown code, not a demonstration of failure on any specific board.

<a id="c9-section-47"></a>

#### 47. CE 3D packet interpreter, copied table data, and H5/H5S2 texture-code differences (additional binary pass)

**Evidence basis:** static disassembly of retail x86 CE 6.0 `DDI_VIA.dll` from the uploaded `VIA Graphics Driver Group2.msi`, checked against its linker `.map`, and the VIA VX800/VX855/VX900 Part II manuals. No code was executed. All addresses below are image virtual addresses in the original retail DLL (image base `0x10000000`), not physical GPU addresses.

##### 47.1 Correction: the H6 path uses a function symbol named `Texture3D_H5`

The map file identifies `0x10067255` as **`Texture3D_H5`**, not `Texture3D_H6`. `C3DUtils::Init` assigns that function pointer for **both VX800 (`0x1122`) and VX855 (`0x5122`)**, while also selecting `Init3DHWRegs_H6` and `fn3DBlt_H6`. For VX900 (`0x7122`), it selects `Texture3D_H5S2` at `0x10067B16`, `Init3DHWRegs_H5S2`, and `fn3DBlt_H5S2`. These are exact *symbol and dispatch* findings; the H5 designation is a function family label and does not by itself change the chipset assignment.

##### 47.2 Packet formats are no longer just guessed from the words

The CE command-buffer executor `C3DUtils::KickOffCmd_Inv` at **`0x10063766–0x100638E0`** selects behavior using the high 16 bits of a packet header (`0xFE00` through `0xFE06`); it also skips `0xCC000000` padding words. The disassembly establishes the following modes:

| Header prefix | Observed executor behavior | Evidence in disassembly |
|---|---|---|
| `FE00` | Reads offset/value pairs from the packet and writes `*(MMIO_base + offset) = value`. | `0x100638BE–0x100638D5` |
| `FE01` | Sequential writes to `MMIO_base + 0x200000` (separate aperture). | `0x1006389E–0x100638BA` |
| `FE02` | Uses packet operands to write a selector at one MMIO offset (commonly `0x43C`), then streams subsequent values to the register port named in the header (commonly `0x440`) until the next recognized command header. | `0x10063866–0x1006389C`, header-predicate `IS_AGPHEADER_INV`, `0x10062ED9–0x10062F27` |
| `FE03` | Writes one selector word at the header's MMIO offset (commonly `0x43C`) and then a specified number of words to the following MMIO register (`0x440`); the destination port is reused, not incremented. | `0x1006383F–0x10063864` |
| `FE04` | Writes a specified number of words repeatedly to the same MMIO offset. | `0x1006381C–0x1006383A` |
| `FE06` | Performs an MMIO register write, then conditionally a second write (special-case sentinel). | `0x100637F3–0x10063817` |

`FE05` follows the same direct-pair parser branch as `FE00` in this implementation; distinct hardware semantics are **not** established. Other forms should not be inferred from their header names. This CE software parser is not necessarily the hardware command regulator's own decoder.

**Crucial consequence for `VX900_WAITPATCH`:** the repeating packet

```text
FE000000 00000001 00000000 CC000000
0000006C 00800000 CC000000 CC000000
```

is consumed by the **FE00 direct MMIO-pair** branch and results in a software write of `0x00800000` to **MMIO + `0x6C`**. The packet constructor at `0x10066588` writes eight copies (256 bytes plus any leading alignment padding). `fn3DBlt_H5S2` also appends `0x6C/0x00FC0000`. Thus the *software write target and value* are proven from the DLL. VX800/VX855 manuals call `0x6C` a wait-control register; VX900's public manual marks this region reserved. The **VX900 hardware effect remains unmeasured**, despite the proven CE software write.

##### 47.3 Recovered binary tables (they are not all immediate MOV operands)

The two CE blit emitters use shared precompiled state tables; H5S2 additionally selects one of two tables for a `HParaType=03h`, subtype `0x31` upload. Source selection is in `fn3DBlt_H5S2` at `0x10065BFA–0x10065CE3`:

- If the operation byte at offset `+0x1C` does **not** have bit `0x20` and global `bGdiMultiABlend` is zero, the function emits `0x60800001 / 0x61800000 / 0x62104001`, then an `FE03` packet selecting `0x31030000` and copying **eight dwords** from `0x1000870C`.
- Otherwise it emits `0x60800002 / 0x61800000 / 0x62204002`, then an `FE03` packet selecting `0x31030000` and copying **twelve dwords** from `0x1000872C`.
- The 12-word table has the same first eight dwords as the 8-word table, and appends four additional words. This suggests two configurations whose exact GPU semantics are not documented.
- Both H6 and H5S2 include a separate `bGdiMultiABlend`-controlled choice of short versus long Type-03 table data (subtypes `0x20/0x21`) at `0x1000875C` or `0x100088B4` and `0x100088E4`.

**Documentation gap:** VX900 Part II, printed p. 153, explicitly calls **Type-03 subtypes `0x31–0x33` reserved** and `0x20–0x22` reserved. Yet this real VX900-selected CE backend emits these selectors and data. This makes the public document incomplete or inconsistent for the code path; it does *not* establish what the undocumented data means electrically.

##### 47.4 Texture helpers also differ beyond the end-of-blit patch

The map identifies `Texture3D_H5` at `0x10067255–0x10067B16` and `Texture3D_H5S2` at `0x10067B16–0x10068473`. These have highly similar entry logic (including common source/destination format conversion helper calls), but a restricted scan of immediate DWORD stores shows these distinct values:

| H5 (VX800/VX855) | H5S2 (VX900) | Observation |
|---|---|---|
| `0x92010003` | `0x92010001` | Same command-position region, changed argument |
| `0x01020100 / 0x02060503` | `0x010000C2 / 0x02000142` | Changed state/pixel input encoding |
| `0x25F6C000 / 0x28FFC000` | `0x25F0C000 / 0x28F0C000` | Changed texture or blend state words; exact hardware semantic unverified |
| `0x47400000` | `0x47000000` | Changed final parameter value |
| No same inline `0x31030000` upload | `0x31030000` and Type-03 table data | VX900-specific software-command sequence |
| No `VX900_WAITPATCH` call | Calls `VX900_WAITPATCH` at `0x100683F4` | Extra eight serialized MMIO waits before submission |

This comparison is **incomplete for computed words**. It is an accurate description of instructions visible in the retail DLL, not a complete executed stream or claim of GPU feature support.

##### 47.5 Porting implications and remaining tests

1. **Keep separate 3D blit/texture setup backends for VX900**. VX800 and VX855 actually share these CE function pointers; H5S2 has real different state and wait commands.
2. **Do not transplant the H5S2 0x31 tables into VX855.** Their purpose is undocumented and the H5/H6 branch does not need them for the equivalent CE operation.
3. **Treat `0x6C` differently from queue polling.** The CE software writes it explicitly; this alone does not reveal when a queued command was consumed or how the ring fetcher works.
4. To finish the dynamic stream comparison, trace register-loaded values and buffer length passed to the CE executor at `0x10063766`, separately for each operation/flags branch; compare against real hardware logs before changing the Win98 path.
5. The direct software executor does not prove that an alternate AGP-submitted packet has identical execution semantics; the AGP submission path must be decoded independently.

###### Exact copied DWORD values

The following 32-bit values come directly from the retail DLL image at the indicated virtual addresses. The addresses are file-local VAs, not GPU addresses.

**H5S2 Type-03 subtype 31, mode 1 (8 words)** — VA `0x1000870C`; 8 DWORDs:

```text
0008014C 00000000 00000000 0008021F
00084124 00000000 00000000 00080283
```

**H5S2 Type-03 subtype 31, mode 2 (12 words)** — VA `0x1000872C`; 12 DWORDs:

```text
0008014C 00000000 00000000 0008021F
00084124 00000000 00000000 00080283
00088124 00000000 00000000 00080293
```

**Shared Type-03 subtype 20, short branch** — VA `0x1000875C`; 4 DWORDs:

```text
01C00530 301C0053 00F20005 000D0820
```

**Shared Type-04/primitive setup table** — VA `0x100087A0`; 32 DWORDs:

```text
07000000 06000000 05000000 04000000
0B000000 0A000000 09000000 08000000
22000000 21000000 0D000000 0C000000
26000000 2D000000 2C000000 29000000
2F582000 2B3F8000 2A3F8000 2E000000
43800000 4200493F 41400000 40800000
47000000 46800000 4500493F 44400000
52147FFF 51000000 50000000 4800003F
```

**Shared Type-03 subtype 20, long branch** — VA `0x100088B4`; 12 DWORDs:

```text
00802490 B8080000 00F0006D 00000C20
0140B6D0 3000C053 00F20105 00000C20
020C0530 30080000 00F20005 00000820
```

**Shared Type-03 subtype 21, two constants** — VA `0x100088E4`; 2 DWORDs:

```text
301001B0 311111B1
```

<a id="c9-section-51"></a>

#### 51. CE's two execution transports for the same FE command packets

The family package also exposes an important *transport* distinction. In both texture helpers, an observed command-interface field at offset `+0x70` determines which software path processes the generated packet:

| `[command_iface + 0x70]` | H5 texture, source | H5S2 texture, source | Actual path |
|---|---|---|---|
| `== 0` | `0x10067589–0x100675A7`; submit `0x10067ABA–0x10067ACC` | `0x10067E82–0x10067EA3`; submit `0x10068417–0x1006842C` | Request a writable command area through `cmSmartRequest` (`0x1004E42E`); after filling it, call `0x1004E51D` with produced byte length and flag `1` |
| `!= 0` | `0x100675AC`, dispatch `0x10067AD1–0x10067ADD` | `0x10067EA5`, dispatch `0x1006842E` onward | Construct packets in a static command area at `0x100CDD60` and execute them through the in-process FE packet interpreter `0x10063766` |

The interpreter at `0x10063766–0x100638E4` recognizes packet high-word classes `FE00`, `FE01`, `FE02`, `FE03`, `FE04`, `FE05`, and `FE06`, as well as `CC...` padding. For example, its `FE02` handler writes the selector to one MMIO offset and streams subsequent words to another. The `FE00/FE03` paths also perform MMIO writes. **Crucial distinction: this is *not* proof of software rasterization**—both transports can send commands to the hardware.

A driver port should therefore separate **packet generation** from **packet transport**. The available evidence supports sharing some packet generators across VX800/VX855/VX900 while allowing chipset- and environment-specific queue submission, idle waits, and FIFO management. The exact hardware transport selected by each interface mode, its relative performance, and any relationship to the VX855 intermittent command queue hangs require runtime traces.

**Source-level reproducibility:** the local static check script `_ce_further_trace.py` verifies the relevant disassembly, constants, two packet-submission paths, and example floating-point output against the extracted retail DLL. This script is analysis-only and does not issue hardware commands.

<a id="c9-section-69"></a>

#### 69. CE command-buffer decoder: full recognized FE packet family

**Source:** original VIA Windows CE 6.0 retail `DDI_VIA.dll` from `VIA Graphics Driver Group2.msi`. The disassembly of the header detector (`IS_AGPHEADER_INV`, VA `0x10062ED9–0x10062F27`) recognizes **eight header classes** with upper 16 bits `FE00` through `FE07`. The software MMIO executor (VA `0x10063766–0x100638E4`) directly dispatches `FE00–FE06`, while other entries—including recognized `FE07`—take the generic four-DWORD advance path in this interpreter. A recognized header is **not** automatically evidence of a fully decoded execution mode or a demonstrated packet emitter.

The interpreter uses `MMIO_base = *(this->context+8)` (through the nested pointer reached at entry). The table below describes this **specific CE direct-MMIO interpreter**; do not assume the GPU command-fetcher or AGP transport necessarily parses every packet the same way.

| Packet class | Verified software-interpreter action | x86 code anchor | Interpretation/constraint |
|---|---|---|---|
| `FE00` | Read a count, then write a sequence of DWORD `(register offset, value)` pairs to graphics MMIO | `0x100638BE–0x100638D5` | Shares code with `FE05` |
| `FE01` | Read count, stream that many DWORDs to successive addresses starting at `MMIO+0x200000` | `0x1006389E–0x100638BC` | Direct host BitBLT aperture on VX855; see §70 |
| `FE02` | Write an initial selector/value to the MMIO offset specified in packet header word 1; then stream following DWORDs to the low-16-bit destination offset encoded in the packet type word until another recognized FE header appears | `0x10063866–0x1006389C` | Its stopping condition calls `IS_AGPHEADER_INV`; count is implicit, not carried like `FE03` |
| `FE03` | Read the count and write the selector to `MMIO + low16(header)`. Then write all counted payload DWORDs repeatedly to that port plus 4 | `0x1006383F–0x10063864` | Common `0x43C`/`0x440` parameter selector/data format |
| `FE04` | After the four-DWORD prefix, write `count` subsequent values to the same MMIO offset selected by `low16(header)` | `0x1006381C–0x1006383A` | Repeated port writes, distinct from offset/value pairs |
| `FE05` | Same software-MMIO pair-write loop as `FE00` | `0x100638BE–0x100638D5` | This does **not** establish identical GPU-side queue semantics |
| `FE06` | Write one `(offset,value)` pair; conditionally perform a second pair unless the test word equals `0x00F712A0` | `0x100637F3–0x10063817` | Code branch exists; no literal `FE06` packet emitter established by the scanned immediate instructions |
| `FE07` | Recognized as a header by the detector but not given a distinct interpreter branch; generic path skips four DWORDs | `0x10062F19–0x10062F27`, `0x100637EB` | **Opaque/unresolved**; do not infer command semantics |

**Subtle but important qualification:** `FE02` determines when to stop streaming data by calling `IS_AGPHEADER_INV` on the *next* DWORD (`0x10063892`). That means proper packet framing matters: treating an arbitrary DWORD stream as safe `FE02` payload may cause a value resembling an FE header to terminate this particular software decoder. This is a consequence of the compiled parser, not a measured silicon limitation.

A static scan of the disassembly found literal immediate packet-header writes for `FE00–FE05`; the exact use of `FE06/FE07` by indirect or calculated producers remains unestablished. **These class definitions explain CE transport framing; they do not prove that identical high-level 3D operation packets work across all three GPUs.**

<a id="c9-section-70"></a>

#### 70. FE01's destination is the documented 2D Host BitBLT aperture

At `0x100638A8`, `FE01` executes `lea ecx,[ebx+0x200000]`, then sequentially writes payload DWORDs to the computed MMIO address. This agrees with **VX855 Part II, PDF page 62 (printed page 56)**: the command regulator's address-space map labels `2M–4M-1` as **"2D Host BitBLT Space"**, relative to graphics MMIO MB1. The result makes `FE01` a concrete common **packet transport primitive for host-fed 2D commands**, rather than an unexplained 3D shader packet.

For a cross-family driver this suggests separate functions: (a) build logical register/host-data operations, (b) encode a CE-style packet where appropriate, (c) submit via PCI/MMIO or queued transport using a chipset-specific backend. The CE code proves what **its software interpreter does**, but register mapping and ordering must still be checked on actual VX800 and VX900 hardware before copying the complete submission path.

<a id="c9-section-72"></a>

#### 72. How the new decoder changes the command-compatibility investigation

The evidence now supports separating three independently testable layers:

1. **CE packet-envelope format:** the direct-MMIO interpreter above, including `FE00–FE07` recognition and `FE00–FE06` dispatch. The *software parser* is shared because the whole family uses this one executable.
2. **GPU programming payload:** the embedded `HParaType`, texture, primitive, pixel-shader and vertex-shader values. Several are identical across H6/H5S2, but the VX900 vertex declarations, candidate VS program and `04h:60h–62h` remain distinct.
3. **Transport, synchronization, and aperture mapping:** VX800/VX855/VX900 may use different PCI/AGP queue or wait sequences even when the logical payload is the same; `VX900_WAITPATCH` remains a documented CE example.

**Practical next analysis:** use the recovered packet decoder to build *offline trace dumps* of each compiled blit variant, with identical semantic inputs (one or two texture coordinates, alpha and pitch), then diff the **decoded GPU payloads** after stripping packet framing, padding and patch waits. This would isolate the smallest set of generation-specific commands without misclassifying software packet construction differences as hardware register incompatibilities. The current sources do not provide a complete runtime argument trace, so such exact matched-input dumps would require additional static data-flow reconstruction or a controlled CE test.

**Reproducibility:** `Chrome9_CE_Command_Verification_69_72.py` checks the retail DLL SHA-256, header detector and interpreter opcode anchors, the `+0x200000` aperture, and all 256 alpha-byte conversions. It performs **only read-only binary inspection and arithmetic modeling**; no driver functions or graphics hardware are executed.

<a id="c9-section-73"></a>

#### 73. Read-only CE packet inspector for cross-generation command diffs

Added `Chrome9_CE_Offline_Packet_Decoder.py`, a small **offline** decoder for the software packet envelope reconstructed in §69. It accepts either a whitespace-separated sequence of hexadecimal DWORDs or a raw **little-endian DWORD** binary file, validates packet lengths, and prints JSON describing the MMIO operations. Its implementation does not map graphics MMIO, load the CE DLL, or submit GPU commands.

Example, using one **hand-assembled packet from the exact constants** in the disassembly of VIA's `VX900_WAITPATCH` (not a captured live hardware trace):

```sh
python Chrome9_CE_Offline_Packet_Decoder.py --hex \
  'FE000000 00000001 00000000 CC000000 0000006C 00800000 CC000000 CC000000'
```

Result (shortened):

```json
[
  {
    "packet": "FE00",
    "action": "MMIO offset/value pairs",
    "pairs": [
      { "mmio_offset": "0x6C", "value": "0x00800000" }
    ]
  }
]
```

This confirms the **software-decoder interpretation** of the `0x6C` wait packet already recovered in §46. The VX800/VX855 register manual defines `0x00800000` at the documented wait-control interface as wait-for-3D-idle; VX900 documents the comparable range as reserved. **Do not assume the same behavior on VX900 silicon merely because its CE driver emits these bytes.**

The tool's test suite checks one legal example for each of `FE00` through `FE07` and rejects a deliberately truncated packet. `FE07` is marked opaque rather than assigned made-up hardware semantics. The decoder is appropriate for **future offline CE packet captures or reconstructed streams**, not as a replacement for a real 3D trace or hardware test.

**Next research step:** reconstruct one complete representative `fn3DBlt_H6` and `fn3DBlt_H5S2` command stream using *identical* destination, pitch, texture dimensions, alpha, and vertex inputs. Decode both with the new inspector, compare the resulting logical 3D-register operations, and exclude wrapping/padding and the VX900 wait patch from the payload diff. This is still outstanding; the uploaded CE MSI contains compiled code and small constant tables, not a ready-made pair of matched runtime command buffers.

<a id="c9-section-94"></a>

#### 94. Common H6/H5S2 3D idle command, with two transports

The retail DLL's `WaitEngIdle_CR` (VA `0x1006363B`) is assigned as the `C3DUtils` wait callback for **both** H6 and H5S2 at `0x10063749`, so the same *code body* is used by the listed VX800/VX855/VX900 dispatch paths.

It tests the context flag at `[context+0x70]` and takes one of two forms:

- Direct-MMIO mode: `mov [MMIO+0x6C], 0x00C00000` (`0x1006364B`).
- Buffered mode: constructs an `FE00` register-write packet specifying offset `0x6C`, value `0x00C00000`, then passes the packet to the command transport (`0x10063653–0x100636A1`). A **separate old-chip `3157h` special case** uses a different register-word encoding (`0xF000001B`); do not apply that special case to H6/H5S2.

VIA's VX800/VX855 command-register descriptions map the value's bits 23 and 22 to **wait 3D idle** and **wait 2D idle**. This matches the common driver operation and strengthens the shared-command catalog. The VX900 public manual's different organization/labels mean that identical CE writes still need hardware testing before claiming identical silicon wait semantics. This common wait should not be confused with the **additional** VX900 H5S2 `VX900_WAITPATCH` packet sequence analyzed earlier.

<a id="c9-section-101"></a>

#### 101. Negative control: Type-10 `60h–64h` initialization is not Type-04 `60h–62h` VX900 vertex routing

An easy misidentification arises because both the common 3D initialization and the VX900 vertex setup contain words beginning `0x60`, `0x61`, and `0x62`:

- At `0x10062FD6` in `Init3DHWRegs_INV`, the DLL selects `0x00100000` at graphics MMIO `0x43C`, then writes `0x60000000`, `0x61000000`, `0x62000000`, `0x63000000`, `0x64000000` to `0x440`. That selects **`HParaType=10h`** (command regulator).
- In the VX900 blit at `0x10065C24` / `0x10065C97`, it instead selects `0x00040000` through its `FE02` packet targeting `0x43C`, then writes `0x60800001`/`0x60800002`, `0x61800000`, and `0x62104001`/`0x62204002`. These select **`HParaType=04h`** (vertex/primitive state).

**Thus the shared Type-10 initialization cannot be used as independent evidence that the VX900 Type-04 `60h–62h` registers have been decoded.** Their meanings remain undocumented, as in §82–85. The mode-dependent control values are genuine and reproducible, but the current two vertex-program variants are insufficient to infer every bit or use those controls generically.

**Verification and scope:** `Chrome9_CE_VX900_Boot_Command_Audit.py` is a read-only test that checks the original retail DLL SHA-256, uses `objdump` to independently reconstruct the 512-word table, verifies exact buffered/direct agreement and the normalized word-table hash, and confirms the `FE0A` and 396-zero register-loop signatures. The saved 512-row CSV gives hex values plus VAs. No CE code was executed, no hardware accessed, and none of these decoder control writes should be added to the user's existing VX855 Windows 98 3D driver.

<a id="c9-section-369"></a>

#### 369. FE03 selector and data ports

The CE 7 retail `C3DUtils::KickOffCmd_Inv` interpreter at `0x10072BD9–0x10072BFC` writes the selector once to `MMIO + low16(header)` and all counted data DWORDs repeatedly to that address **plus 4**. Thus `0xFE03043C` selects through `0x43C` and transfers data through `0x440`. Section 47 describes the same port separation for CE 6.0. The CE 6.0 decoder table in §69 uses the same interpretation. These are software-interpreter semantics.

### Type-10 and Type-11 registers and virtual queues

<a id="c9-section-312"></a>

#### 312. Three-family Type-10/Type-11 command-regulator audit: scope and classification

**Evidence:** original VIA Chrome9 HC3 **VX800/VX820 Part II** (November 19, 2008), Chrome9 HCM **VX855/VX875 Part II** (July 29, 2009), Chrome9 HD **VX900 Part II** (January 19, 2011), and the original CE retail `DDI_VIA.dll`/map (SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`). The script `Chrome9_Command_Regulator_Three_Family_Audit_312_326.py` verifies source-text anchors, **31 register-group classifications**, and the CE machine-code write sequence at `hwmInitialize_inv:0x100504D4–0x10050554`. The companion CSV records per-family fields and printed manual page references. Source manual pages are cited as *printed* pages, not PDF file indices. Crucial tables were additionally checked in rendered PDF pages: VX855 PDF p68 (printed p62), VX900 PDF p80 (printed p74), and VX900 PDF p87 (printed p81).

**Evidence categories:** **same documented encoding**; **same address with different meaning**; **generation-specific documented extension**; **documented field omitted on another generation**; and **identical CE binary write**. An absent public field description is **not** a proven absent hardware feature. No board behavior or dynamic command-buffer values were measured in this pass.

<a id="c9-section-313"></a>

#### 313. Type-10 `00h` packaging is common; readback debug target 7 is newer-generation documentation

All three manuals agree on Type-10 `00h` PCI-command packaging enable at bit 8 and ECLK timeout in `[7:0]`, and broadly on the Type-10 `02h–03h` readback/debug selector/address structure. VX800 lists readback target IDs **0–6** (CR, FE, PE, RC, PS, XE, BE). VX855 and VX900 additionally **document ID 7** for performance/profile counters (VX855 printed p62; VX900 printed p74). Do not ascribe the ID-7 option to VX800 without silicon evidence, even though the neighboring register layout matches. This is a VX855/VX900 *published* addition, not a fully new debug interface on VX900.

<a id="c9-section-314"></a>

#### 314. Type-10 `04h` is a direct command-word meaning conflict

This is the highest-consequence static **incompatibility**, not merely a changed default:

| Address | VX800 / VX855 meaning | VX900 meaning |
|---|---|---|
| **Type-10 `04h`** | Lower **interrupt-state-buffer address** `[23:4]`, buffer-location selector `[1:0]` | **CR command dispatch:** bits `[2:1]` select priority (video-related, normal GFX, or round-robin), bit `[0]` dispatches video-related 2D/3D commands into the HQV data path |
| **Type-10 `05h–07h`** | Interrupt-buffer high address/3D-save option, leftover-vertex stop threshold, CR interrupt pause/stop request | **Explicitly reserved** in VX900's Type-10 command-regulator description |

Evidence: VX800 Part II printed pp143–144; VX855 Part II pp62–63; VX900 Part II **p74** (verified visually). Thus **a copied older `0x04xxxxxx` control word does not mean the same thing on VX900**. The VX900 manual explicitly says that **command switch mode cannot coexist with traditional wait/fence and traditional branch**. Which of these mechanisms the proprietary CE driver uses in a given rendering path remains an implementation question; the manual meaning conflict is established.

<a id="c9-section-315"></a>

#### 315. VX900 Type-10 `08h–0Bh` preemption and `0Ch–0Eh` arbitration are documented additions

On VX900 only, Type-10 `08h–0Bh` programs a **preemption-fence destination and ID** and preemption mode/trigger. Type-10 `0Ch–0Eh` establishes CRMI **per-source command-request arbitration priority masks** covering main/sub-main command buffers, VQ, video, graphics, decoder and 3D writeback. The original VX800/VX855 Type-10 published lists do not describe those controls (VX900 manual pp75–76). These register ranges must be **generation-qualified**, not emitted by an unqualified H6 path. Static documentation does not prove that the newer features are required for ordinary 3D drawing, nor that older hardware behaves deterministically for these words.

<a id="c9-section-316"></a>

#### 316. Type-10 `60h–64h` retains AGP ring address structure, but VX900 adds location `11b`

Across all manuals, AGP start/end and pause addresses are represented using high/low portions in **`60h–64h`**, aligned to 128 bits. Type-10 `60h[1:0]` identifies the buffer location. `00b`=S.L., `01b`=S.F. and `10b`=S.M. remain common; **`11b` is reserved on VX800/VX855 but VX900 documents L.L.**. VX900 likewise documents L.L. in command-queue and frame-buffer-swap location selectors where the older manuals reserve it. The common base format is genuinely compatible for its shared values. The additional L.L. encoding is a **VX900 published extension**, not evidence that the older devices support it undocumented.

<a id="c9-section-317"></a>

#### 317. Type-10 `65h` pause ID `01b` changes semantics on VX900

The top-two pause ID bits **`65h[9:8]`** have these documented meanings:

| Pause ID | VX800 / VX855 manual | VX900 manual | Compatibility judgment |
|---|---|---|---|
| `00b` | Pause at the boundary; resume at the next address after update | Pause until pause address changes, then resume at next address | Broadly compatible; wording differs |
| **`01b`** | **End of portion; immediately begin fetching at ring start, without waiting** | **Jump-point: pause until a *next pause-address command* with ID `00b`/`10b` is received; then restart at ring start** | **Documented semantic difference; cannot treat as a universal immediate wrap** |
| `10b` | Stop; a fresh AGP trigger is necessary to resume | Stop; a fresh AGP trigger is necessary to resume | Common documented stop semantics |
| `11b` | Reserved | Reserved | Do not use |

Evidence: VX800 p145, VX855 p64, VX900 p78. The VX900 manual also makes explicit **Pause > Jump > End** priority when addresses coincide and specifies a **minimum 2QW pause-address advancement for kickoff**. It is not established that these latter requirements are absent in older silicon merely because their manuals omit those detailed notes. Do not infer real-world behavior of ID `01b` without target-specific tests or a matching driver trace.

<a id="c9-section-318"></a>

#### 318. Type-10 `68h[5]` changes from reserved to a 2D AGP header format control

The 6-bit AGP read threshold `68h[21:16]` (default 8) and low clear/restore/trigger bits `68h[4:0]` are shared across all three documents. **VX900 newly specifies `68h[5]`** as the *format of AGP Header1 data to the 2D engine*: `0` = 128-bit, `1` = 32-bit. On **VX800/VX855, bit 5 is reserved**. This is a concrete generation-specific 2D/command-regulator interaction, distinct from the separate 2D GE opcode changes in §§300–311. Common trigger command `0x680A0001` keeps bit 5 clear and therefore does not exercise the newer 32-bit mode; its byte identity alone must not be used as evidence that bit 5 behaves identically.

<a id="c9-section-319"></a>

#### 319. Type-10 `69h` and `6Ch–6Fh`: older published fields, but not necessarily missing on VX900

VX800/VX855 document Type-10 `69h[0]` as the *AGP branch enable* (`FE8x` permitted when set), and `6Ch–6Fh` as **interrupt-state and AGP restore-address** fields. VX900's Type-10 chapter documents **`60h–68h`, then `70h–74h`**, and **does not list** `69h` or `6Ch–6Fh` with those old meanings.

**Critical nuance from the original CE binary:** `hwmInitialize_inv` has a common path selected for PCI `1122h` (VX800), `5122h` (VX855), **and `7122h` (VX900)**, and actually writes **`0x69000000`** through the Type-10 parameter port after the Type-11 preamble. This proves an intentional *software write of the old field address* on VX900; it does **not** prove how VX900 interprets the write or that all other older restore fields still function. The correct classification is **older documented, VX900 manual omits; CE appears to write `69h` for compatibility or precaution**—rather than `69h` categorically removed or inoperative.

<a id="c9-section-320"></a>

#### 320. Type-10 `70h–74h`: the virtual-queue core remains strongly compatible

All three manuals retain the Type-10 `70h–72h` command-queue start/end and high-byte encoding, Type-10 `73h` **queue length measured in 128-bit (16-byte) units**, with a published minimum value **`0x800`** (32 KiB), and Type-10 `74h` FIFO thresholds, request arbitration, AGP/PCI source choice, and enable bit. VX900 adds the L.L. location choice at `70h[1:0]` and an explicit note: **when VQ1 is enabled, `FE8x` branch headers are forbidden**, because the hardware cannot stop VQ data reads on detection of such headers. The earlier manuals do not publish that exact note in the corresponding table. Shared unit sizing is therefore **documented**, while branch/VQ coexistence is **not safely portable** across modes.

This three-way agreement on 16-byte units is independent evidence for the previous OpenChrome/Win98 discrepancy described in the historical parts of this report; do not deduce whether the OpenChrome source's shifted length is wrong without confirming mode and effective hardware interpretation.

<a id="c9-section-321"></a>

#### 321. Type-10 `78h–7Ch`: older dedicated 2D/3D queue registers are missing from VX900's published list

VX800/VX855 both document a **second command queue for 2D/3D commands** at Type-10 `78h–7Ch`, including location, end/start, queue length in 128-bit units, thresholds and enable. The VX900 Type-10 chapter does **not** list `78h–7Ch` as an equivalent second queue; it instead adds different command-switch, arbitration and preemption controls. This is a *documented register-architecture difference*, but a negative statement about **silicon implementation is not warranted** from missing manual entries alone. A reliable cross-generational implementation should not emit `78h–7Ch` without selecting a verified VX900 recipe.

<a id="c9-section-322"></a>

#### 322. Type-11 00h request lock, hardware lock and legacy FIFO fields

Previous preliminary wording risked saying VX900 first *introduced* 2D/3D request locking. **That is false for bit 16.** VX800, VX855, and VX900 all document **Type-11 `00h[16]`** as *Enable 2D/3D Request Lock Control*, permitting 2D/3D commands to be issued under serialized memory access. **VX900 additionally documents `[17]`** (*Enable HW Lock 2D and 3D*, default 1), making the hardware-versus-software synchronization choice explicit.

An important incompatibility is in the rest of this word: VX800/VX855 describe **three FIFO depth selectors `[10:2]` and critical-command bit `[0]`**, whereas VX900 Type-11 `00h` describes **`[15:0]` reserved**. Thus an older Type-11 `00h` word that configures FIFO depth and critical-command behavior **cannot be presumed meaningful on VX900**, even if bit 16 still has a related name. This field-layout difference is verified in the original printed VX900 p81 / VX855 p69 / VX800 p147.

<a id="c9-section-323"></a>

#### 323. VX900 Type-11 `03h` adds engine-granular fence wait selection

On VX900, Type-11 `03h` selects conditions to **wait for 3D, 2D, video, HQV0/1, DMA0–3 idle and HQV EOF** before a fence, plus a wait count and all-engine-idle option. VX800/VX855 Type-11 chapter sections do **not document an equivalent `03h` wait-mask field**. All three retain broad Type-11 `04h–07h` fence address, ID, mode and trigger commands. This is a **separate VX900 fence-control extension**; the register `03h` must not be mistaken for the older Type-10 `03h` debug-readback address. Matching fence opcode words do not guarantee identical wait/synchronization rules without configuring the appropriate new field.

<a id="c9-section-324"></a>

#### 324. Type-11 `08h–0Bh` and `10h–34h`: common structures with changed priority/location documentation

All three manuals document the Type-11 `08h/09h` save-state buffer address, `0Bh[0]` save trigger and Type-11 `10h–34h` automatic frame-buffer swapping for IGA1, IGA2 and the combined display engine. VX900's save-trigger note explicitly states that `0Bh[0]` takes priority over `09h[8]` store-enable, while VX800/VX855 discuss priority of the old **Type-10 `05h[8]` interrupt-state-buffer write** over the Type-11 save-enable option. This makes the **cross-generation priority model non-identical**, even when the same save-buffer address fields exist.

The auto-swap Type-11 `10h/18h/30h` frame-buffer-location `11b` selector is reserved in VX800/VX855 and documented **L.L.** on VX900. Treat the shared swapping registers as **compatible for common location choices only**. Do not assume old interrupt-buffer priority or the L.L. extension applies to all family members.

<a id="c9-section-325"></a>

#### 325. Traditional branches are shared; a VX855 second-branch group and VX900 command-switch constraints are not

Type-11 `68h–6Bh` traditional FE8x branch commands remain broadly common: branch type, 16-bit-aligned branch-buffer address (including vertex-index use), memory location and size in 16-byte units, with nested branching forbidden. VX900's location table additionally documents L.L. VX855 Part II separately documents a **second branch group** at Type-11 **`6Dh–6Fh`** (its `6Ch` is printed *reserved* despite a surrounding `6Ch–6Fh` group label). VX800 and VX900 manuals do not describe the same secondary group. This is a **VX855 documented extension**, not proof that VX900 lacks all dual-branch support internally.

Two nontrivial VX900 mode constraints are explicitly printed: **command-switch mode cannot be combined with traditional wait/fence or traditional branches** (Type-10 `04h`), and **VQ1 cannot be combined with FE8x branching** (Type-10 `74h`). These restrictions create real *documented compatibility hazards* for unqualified cross-family code reuse. The fact that the CE initializer writes `0x69000000` on VX900 is consistent with conservative branch disabling but does not reveal the complete hardware behavior.

<a id="c9-section-326"></a>

#### 326. Consolidated command-regulator compatibility verdict and remaining work

The resulting 31-row CSV `Chrome9_Command_Regulator_Three_Family_Matrix_312_326.csv` provides a field-by-field assessment. **Confirmed shared documented core:** basic parameter transport, PCI packaging, AGP start/end/jump fields, trigger/clear bits, 128-bit command queue units, Type-11 fence base/ID and common auto-swap layouts. **Confirmed incompatible documented meaning:** Type-10 `04h` old interrupt-state buffer versus VX900 dispatch; Type-10 `65h[9:8]=01b` immediate wrap versus VX900 wait-for-pause-update; Type-11 `00h[10:0]` old FIFO/critical controls versus VX900 reserved layout. **Verified VX900 additions:** Type-10 preemption/CRMI and header-width bit, Type-11 `00h[17]` hardware lock and `03h` fence wait mask, L.L. location selectors. **Verified newer-generation additions:** Type-10 `02h` readback ID 7 on VX855/VX900. **Unresolved, despite documentation differences:** whether undocumented old registers survive on VX900; exact silicon mode interaction and pixel/command effects; the old `FE8x` mode under VX900's new branch/VQ constraints.

The **main next prioritized objective is three-family texture/pixel-shader programming**: classify Type-02 texture register encodings and Type-03/Type-04 shader/sampler differences between the H6 older backends and the H5S2 VX900 backend, retaining exact command/data equality separately from unsupported opcode hypotheses. No new board execution is claimed. Both PDF render spot checks and the SHA-pinned Python audit passed.

<a id="c9-section-26"></a>

#### 26. Virtual command queue: a particularly valuable cross-check

##### 26.1 Documented initialization in OpenChrome

For VX800/VX855/VX900, `viaInitialize2DEngine()` selects `viaEnablePCIeVQ()` when the VQ start offset is nonzero. The queue is allocated from VRAM with `drm_bo_alloc(..., VIA_VQ_SIZE, 16, TTM_PL_VRAM)`. `VIA_VQ_SIZE` is **256 KiB**. [OpenChrome `src/via_driver.h:L101`; `src/via_ums.c:L351–L370`, `L417–L464`, `L545–L551`].

The core sequence is:

```c
/* Reference sequence only: source excerpt, not a validated Win98 recipe. */
VIASETREG(0x41c, 0x00100000);
VIASETREG(0x420, vqStartEndH); /* 0x72xxxxxx */
VIASETREG(0x420, vqStartL);    /* 0x70xxxxxx */
VIASETREG(0x420, vqEndL);      /* 0x71xxxxxx */
VIASETREG(0x420, vqLen);       /* 0x73xxxxxx */
VIASETREG(0x420, 0x74301001);
VIASETREG(0x420, 0x00000000);
```

[OpenChrome `src/via_ums.c:L351–L370`]. OpenChrome disables the same family queue through writes to `0x41C` and `0x420`, ending with `0x74301000`. [OpenChrome `src/via_ums.c:L375–L390`].

**Why the earlier VX855 tests are important:** your prior test sequence involving `0x41C = 0x00100000` and `0x420` words was not arbitrary; it closely resembles this reference implementation. But the presence of that sequence cannot, by itself, demonstrate command consumption, DMA ordering, or a functioning GPU read-ahead ring.

##### 26.2 Do not confuse `0x41C/0x420` with `0x43C/0x440`

The source distinguishes:

| MMIO address | OpenChrome usage | Caution |
|---|---|---|
| `0x400` | Engine-status register | Use platform-specific busy masks |
| `0x41C` | Select CR/queue setup transport | Written in PCIe-VQ setup |
| `0x420` | Send queued setup **parameter words** | These are **writes**, not automatically a consumer register |
| `0x43C` | `VIA_REG_TRANSET`, parameter-type selector | Write path; **not established as a VQ read pointer** |
| `0x440` | `VIA_REG_TRANSPACE`, parameter words | Used when flushing Header-2 work by direct MMIO |

[OpenChrome `src/via_regs.h:L193–L214`; `src/via_exa.c:L42–L63`; `src/via_ums.c:L351–L370`]. The manuals' alternative programming windows distinguish CR writes from parameter writes. The observation that `0x43C` read as zero during a previous experiment is **not proof that no queue was consumed**, especially if the address functions as a write selector rather than a readable consumer pointer.

##### 26.3 New unresolved discrepancy: queue-length units are off by a factor of two

This is a **document-versus-source inconsistency**, not a verified bug:

| Quantity | Source / manual value |
|---|---:|
| OpenChrome `VIA_VQ_SIZE` | `256 * 1024` = 262,144 bytes |
| OpenChrome queue length expression | `VIA_VQ_SIZE >> 3` = 32,768 = `0x8000` |
| VX855/VX900 manual stated `10h:73h` unit | **128 bits = 16 bytes** |
| Size implied by manual if `0x8000` represents 16-byte units | **524,288 bytes (512 KiB)** |
| Length word implied by the manual for a 256 KiB queue | `0x4000`, not `0x8000` |

**Evidence:** [OpenChrome `src/via_driver.h:L101`; `src/via_ums.c:L351–L368`]; [V855-II, PDF p. 72]; [V9-II, PDF p. 85]; [V8-II, PDF pp. 150–151].

Possible explanations that **must not be silently conflated**:

- The manuals state the nominal unit but this particular queue setup register is actually counted in 64-bit/8-byte units on some silicon.
- The source represents an older/alternate interpretation or a compatibility workaround.
- OpenChrome has a latent length-field bug or the setup sequence is seldom exercised as assumed.
- The length is not used as a simple byte-span bound in the way the documents seem to imply.

**Do not test an apparent 512 KiB read window against a 256 KiB allocation**: that risks out-of-bounds VRAM access or corruption. A controlled test should first allocate and reserve a full guarded 512 KiB test region, if the platform/allocator permits, and only then compare the two encodings on a small, simple verified command stream. Preserve and dump the before/after guard region. Use one variable at a time and never run both sequences in a production driver without validating actual queue length semantics.

##### 26.4 Decode of OpenChrome's `0x74301001`

Its `0x74` sub-address payload is `0x301001`:

| Field (in 24-bit payload) | Value | Manual meaning |
|---|---:|---|
| Bits `21:16` | `0x30` | Write-FIFO threshold |
| Bits `13:8` | `0x10` | Read-FIFO threshold |
| Bits `7:4` | `0` | Read/write interruption mode |
| Bit `2` | `0` | PCI/AGP command-source selection (PCI) |
| Bit `1` | `0` | Fixed one-128-bit-unit request length |
| Bit `0` | `1` | Queue enable |

[V9-II, PDF p. 86]; [V855-II, PDF pp. 72–73]. This makes the sequence meaningful to the engine, but still does not guarantee that a correctly formatted payload was written to the queue or fetched by the GPU.

##### 26.5 CPU-side “ring” versus VRAM VQ versus DRM submission

OpenChrome has **three distinct concepts** that should not be merged in the Win98 implementation:

1. A CPU-allocated **command staging buffer** (`viaSetupCBuffer`, `via_dmabuffer.h`) that accumulates H1 and H2 words.
2. The on-chip/VRAM **virtual command queue**, whose start/end/length are programmed by `viaEnablePCIeVQ()`.
3. A possible **DRM command submission** path that sends staging-buffer data via `DRM_VIA_CMDBUFFER` or `DRM_VIA_PCICMD` when configured.

If neither DMA nor the relevant direct-rendering path is active, `viaFlushPCI()` walks the staging buffer and **writes Header-2 data directly** to `VIA_REG_TRANSET` / `VIA_REG_TRANSPACE`, while Header-1 writes target mapped engine registers directly. This is *not equivalent to filling the VRAM VQ with draw commands*. [OpenChrome `src/via_exa.c:L42–L110`, `L138–L171`, `L179–L199`; `src/via_dmabuffer.h:L46–L97`].

**Porting conclusion:** validate the full **producer → VRAM buffer → hardware fetch → idle/complete** chain independently. A GPU drawing the expected triangles after a VQ-enable sequence is insufficient evidence if the triangles were actually submitted through direct MMIO.

<a id="c9-section-297"></a>

#### 297. The command-queue length unit remains common to both older manuals

Both VX800 and VX855 Type-10h `70h–7Ch` command-queue sections describe the main `73h` **and extended `7Bh` length values in 128-bit (16-byte) units**, with minimum `0x800` units. They also document queue location fields at `70h` and `78h` and a queue-enable/control word at `74h`. Although the prose/table format differs, the *unit and broad register namespace* are shared. This independently reinforces the earlier need to investigate any OpenChrome `>>3` versus other drivers' `>>4` queue-length calculation. This pass alone does not decode differences in hardware queue granularity, and should not be conflated with a direct ring-occupancy measurement.

<a id="command-manager"></a>

## CE command manager, locking and capacity

The manager’s requested-space budget, the bytes emitted, and physical ring headroom are separate quantities. The normal indexed-transfer bounds and unresolved overflow or liveness cases belong to different evidence categories.

### Request, flush and lock ownership

<a id="c9-section-87"></a>

#### 87. CE critical section protects final submission, not the entire blit builder

The linked `g_3DEngineLock` is a Windows CE `CRITICAL_SECTION` at `0x100CE7D8`. `C3DUtils` stores its address at object offset `+10h` (`0x100636B2`); `C3DUtils::Init` invokes imported `InitializeCriticalSection` at `0x100636C0`. `Enter3DEngine` and `Leave3DEngine` call `EnterCriticalSection` / `LeaveCriticalSection`, respectively:

| Routine | Initial shared command-pointer store (`dwCurPointer`) | Shared blend flag write | Lock entered | Lock exited |
|---|---|---|---|---|
| `fn3DBlt_H6` | `0x10064568` | set `0x1006464B` / clear `0x10064693` | `0x10065441` | `0x1006547F` |
| `fn3DBlt_H5S2` | `0x100654F1` | set `0x100655D4` / clear `0x1006561D` | `0x1006648F` | `0x100664CC` |

The lock acquisition is **after** the command-buffer construction and its many blend-flag reads. Both backends also store the common `dwCurPointer` global (`0x100CDD5C`) before that lock. At the end, the guarded section chooses `cmSmartFlush` (buffered command transport) or `KickOffCmd_Inv` (interpreted MMIO transport), then releases the lock.

This corrects an easy but unsafe inference: **the existence of `g_3DEngineLock` does not by itself establish that `bGdiMultiABlend` or the command-builder's shared cursor are serialized during preparation.** Nor does it prove a race: Windows CE display-call serialization or an outer caller lock may already prevent concurrent blits, and neither has yet been traced exhaustively.

This is relevant to a potential Win98 port: do not copy a global, mutable command builder and assume the *late* CE lock makes simultaneous command construction safe. Prefer per-draw command storage/state, or explicitly protect the complete build+submission region if the implementation can be entered concurrently. The CE driver's actual concurrent behavior is **unknown**.

<a id="c9-section-96"></a>

#### 96. Shared lock-object observation and verification status

The same retail binary stores address **`0x100CE7D8`** (`g_3DEngineLock`, a CE `CRITICAL_SECTION`) into:

- `C3DUtils` field `+0x10` at `0x100636B2`;
- `CD3dmProxy` field `+0x04` at `0x10062EA7`.

Thus the two helper objects reference the **same lock object**, not two independent engine locks. However, this does **not** establish whether an outer CE graphics API always acquires the lock before invoking the H6/H5S2 command builder. The earlier observed late `Enter3DEngine` / `Leave3DEngine` calls still protect the builder's final submission region locally, not its whole packet construction. No rendering race or double-lock bug is established without the upstream call-chain and runtime serialization rules.

**Companion audit:** `Chrome9_CE_VX900_CnM_and_Wait_Audit.py` checks the exact SHA-256 retail DLL, relevant linker symbols, VX900 reservation branch, HQV values, decoder memory pointers, firmware image SHA-256 and four decode call edges, both shared idle-wait transports, and the shared CE lock pointer. It is **read-only**, executes no CE code, and touches no GPU. All checks passed against the supplied retail DLL.

**Porting outcome:** split *shared 3D commands* from a *VX900-only video firmware/HQV initialization block*. No justification has been found to add the firmware load or `FE0A` decoder boot commands to the existing VX855 Windows 98 3D driver. For a future VX900 video stack, re-evaluate these commands only with compatible decoder support and correct framebuffer carve-out.

<a id="c9-section-140"></a>

#### 140. Command-manager investigation: scope and verified binaries

**Objective.** Continue the earlier `FE01` 8-bit palette/native transfer analysis (§§128–139) by tracing VIA's command-buffer manager rather than inferring safety from the helpers alone. The source is the original, user-supplied *VIA Graphics Driver Group2.msi* and its extracted **x86 retail** `DDI_VIA.dll`; the SHA-256 of the audited binary is `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`. This is a **static code audit**, not a memory-corruption reproduction or GPU run.

| Manager procedure | Retail virtual address | Key role established from compiled instructions |
|---|---|---|
| `cmSmartRequest` | `0x1004E42E` | Record packet family, calculate requested capacity, conditionally release and request a buffer, return insertion pointer |
| `cmSmartFlush` | `0x1004E51D` | Account for bytes *actually emitted*, build/submit header, release or flush buffer |
| `cmSmartHeader` (internal helper) | `0x1004E33A` | Finalize FE02/FE03/FE04/FE05 headers and padding where applicable |
| `cmCreate_inv` | `0x1004EF36` | Set up the INV command-manager backend and queue pointers |
| `cmRequestBuffer_inv` | `0x1004F01C` | Align the next command write position to 16 bytes before requesting capacity |
| `cmReleaseBuffer_inv` | `0x1004F041` | Pass released INV data to the low-level command manager |
| `cmCreate_csr` | `0x1004F5E9` | Set up an alternative CSR manager, not the family path identified here |
| `cmRequestBuffer_csr` | `0x1004F67A` | Align a CSR command write position to eight bytes |
| INV wrap and queue-control routines | `0x1004EAEC`, `0x1004EC27`, `0x1004EDFE` | Pad queue boundaries, update queue pointers, account for free/busy space |

The provided retail linker map identifies the functions and their addresses. The companion audit script `Chrome9_CE_Command_Manager_Audit_140_147.py` verifies the named instruction sites and rechecks representative transfer sizes without executing any CE driver code.

<a id="c9-section-141"></a>

#### 141. All three Chrome9 targets use the same INV manager, not the CSR manager

The shared command-manager dispatcher selects the **INV** callback set for PCI device IDs `0x1122` (VX800), `0x5122` (VX855), and `0x7122` (VX900). It installs `cmCreate_inv`, `cmRequestBuffer_inv`, and `cmReleaseBuffer_inv` at `0x1004E2DC–0x1004E301`. The `0x3157` branch uses `cmCreate_csr`, `cmRequestBuffer_csr`, and `cmReleaseBuffer_csr` instead (`0x1004E2B5–0x1004E2DA`).

In `cmRequestBuffer_inv` (`0x1004F022–0x1004F03D`), the code writes zero DWORDs until the cursor is 16-byte aligned, stores the resulting cursor, saves the requested number of bytes and calls the installed buffer callback. The alternate CSR routine (`0x1004F680–0x1004F69C`) writes `0xCCCCCCCC` while aligning to **eight** bytes. Do not apply the CSR alignment rule to VX800, VX855, or VX900 merely because their code also passes through `cmSmartRequest`.

**Important distinction:** a 16-byte alignment step is visible at request time. It is **not** evidence that the manager automatically adds all of the header and padding bytes for every FE packet the caller later constructs.

<a id="c9-section-142"></a>

#### 142. `cmSmartRequest` adjusts FE00/FE04/FE05 sizes, but not FE01's 16-byte header

At `0x1004E446–0x1004E498`, the manager splits the command word into a family header and a small low field. It begins with the caller-supplied size, then applies format-specific adjustments:

- `FE00` receives an extra **16 bytes** except on the separate `0x3157` path (`0x1004E46B–0x1004E475`).
- `FE04` and `FE05` each receive **16 extra bytes** (`0x1004E475–0x1004E486`).
- A separate condition adds `0x3000` bytes if a manager mode flag is set and the packet-family byte is greater than 3 (`0x1004E486–0x1004E498`). Thus the condition does **not** add that allowance for `FE01`.
- `FE01` has **no automatic additional 16-byte header allowance** in this local function.

The manager then compares its free-space counter with the **adjusted requested size** (`0x1004E4BA–0x1004E4D6`). It may invoke the installed release/request callbacks when there is insufficient room. The return value is a pointer to the caller's insertion position (`0x1004E4D6–0x1004E51C`).

This strengthens, rather than resolves, the §135 observation: the older palette helper requests only the output payload size while writing more bytes afterward. **Neither this manager function nor the FE01-specific logic silently supplies the omitted 16-byte header as a formal addition to that request.**

<a id="c9-section-143"></a>

#### 143. `cmSmartFlush` knows the real output size, but only after the caller has emitted it

At `0x1004E52C–0x1004E534`, the code calculates a length that includes the bytes written since the saved command insertion pointer **plus the caller's flush size**. It then decides whether the data can be finalized in the existing buffer or whether a release/flush callback is required. The final available-space accounting uses this actual amount (`0x1004E550–0x1004E58E`).

That creates the following time-order distinction:

1. `cmSmartRequest` checks/request space based on **R**, the requested bytes plus its selected packet adjustments.
2. The transfer helper writes **E** actual bytes, including headers and any alignment padding.
3. `cmSmartFlush` processes the actual emitted length **after** those writes.

For the older `v8BppImageTransferCN896_AGP` helper, `R=4*ceil(B/4)*H` while the aligned-start modeled output is `E=align_up(16+R,16)`, with up to another 12 bytes needed to reach the initial 16-byte boundary. For `B=33,H=2`, this gives `R=72`, `E=96` when the start is already aligned.

**Safety classification:** the generated code demonstrates an interval where `R <= free_contiguous_bytes < E` could pass the request check but not accommodate every subsequent write. This is a **logical reservation mismatch**, not a verified out-of-bounds access in a running CE system. Other manager layers can maintain unpublished guard/headroom, and the physical allocation's actual extent is not established from this function alone.

<a id="c9-section-144"></a>

#### 144. INV queue-wrap code includes additional margins and chipset-specific padding

The INV manager contains explicit boundary-handling logic, which prevents treating the preceding discrepancy as a proven overflow without more context:

- In `0x1004EDFE–0x1004EF35`, the code compares an end-relative pointer with **`requested_bytes + 0x600`**; a subsequent check includes a **`+0x400`** margin. These represent extra space in the queue-management decisions. Their interpretation as a general-purpose per-allocation guard is **not proven**.
- The queue-wrap path (`0x1004EAEC–0x1004EC26`) aligns to 16 bytes, writes marker/padding words such as `0xCCCCCCC7`, and programs `MMIO+0x41C` / `MMIO+0x420` using queue-control words beginning with `0x64`, `0x65`, `0x66`, or `0x67` in their high byte.
- The padding used near wrap is **chip-specific**: the `0x7122` (VX900) path pads its cursor to a 64-byte boundary (`test al,0x3F`); the `0x1122` and `0x5122` paths pad to a 256-byte boundary (`test al`); another legacy device has a 32-byte case (`test al,0x1F`). These differences are visible at `0x1004EB35–0x1004EB74` and `0x1004EC3B–0x1004ECBB`.
- The INV manager also polls a command-read pointer and, on the VX900-specific path, checks `MMIO+0x47C` in some queue-boundary conditions (`0x1004EEDF–0x1004EEFC`). These are real **software wait conditions**, not measurements of latency or hardware correctness.

The margins, wrap padding and read-pointer checks may explain why CE often survives request/emission differences. They **do not establish a blanket FE01 rule** permitting callers to request less space than they will write. In a new driver, calculate and reserve the entire packet before writing; do not depend on incidental trailing slack.

<a id="c9-section-145"></a>

#### 145. Exact shared-versus-different commands in the command-manager family

| Operation | VX800 | VX855 | VX900 | Evidence classification |
|---|---|---|---|---|
| Manager backend | INV | INV | INV | Same installed callback functions |
| `cmSmartRequest` / `cmSmartFlush` | Shared code | Shared code | Shared code | Identical compiled body |
| Command insertion alignment | 16 bytes | 16 bytes | 16 bytes | Shared `cmRequestBuffer_inv` |
| `FE01` automatic extra header reserve | Not seen | Not seen | Not seen | Shared request logic |
| Queue boundary padding in selected INV paths | 256-byte | 256-byte | 64-byte | Different branches inside shared queue manager |
| Queue command writes at MMIO `0x41C/0x420` | Present | Present | Present | Same transport family, branch-specific words |
| Host-transfer helper FE01 framing | Older single-block style | Older single-block style | Often row/DWORD-specific | Chipset-specific callers, not manager opcode change |

**Cross-reference:** These observations qualify earlier §139; they do not negate the documented identical 2D command selection, vertex masks, and 3D state tables elsewhere in this report.

<a id="c9-section-146"></a>

#### 146. Implications for the Windows 98 VX855 driver

For a bounded host-BitBLT command-buffer builder, the recommended implementation is to compute required bytes **before requesting command space**:

```c
/* Pseudocode; assumes sizes/inputs were checked for overflow. */
size_t lead = (16u - (cursor & 15u)) & 15u;
size_t payload = ((row_bytes + 3u) & ~3u) * height;
size_t total = lead + ((16u + payload + 15u) & ~15u);
/* For one FE01 packet: reserve total from the current insertion position. */
reserve_contiguous(total);
emit_lead_padding(lead);
emit_fe01_header(payload / 4u);
emit_payload_and_tail_padding(payload);
assert(actual_written <= total);
flush(actual_written);
```

The pseudocode is a **defensive implementation guideline**, not a drop-in adaptation of VIA's unpublished manager contract; it assumes a contiguous destination, does not implement manager wraparound, and requires checked arithmetic for large rectangles. The **zero-padding and 0xCC marker policies** can also vary by packet type. In a real driver, use a single layer to account for both alignment and header bytes; avoid double counting if the allocator itself advances the cursor.

A useful read-only CE instrumentation log should include `chip_id, packet_type, request_size, manager_adjusted_size, available_before, insertion_ptr, aligned_ptr, actual_flush_bytes, allocation_end, ring_read_ptr, ring_write_ptr, forced_release_count, wrap_count`. Without allocation-end and read-pointer evidence, an apparent excess of emitted bytes over the request cannot be classified as a memory overwrite.

<a id="c9-section-147"></a>

#### 147. Verification and unresolved limitations

The audit script `Chrome9_CE_Command_Manager_Audit_140_147.py` checks the **original retail DLL SHA-256** and the expected instruction locations for eleven procedures, including `cmSmartRequest`, `cmSmartFlush`, the INV/CSR request callbacks, and the older palette-transfer helper. It also checks **280** small positive-size mathematical cases of the request/emission model. The original §134–139 audit covers a wider **20,480-case** parameter sweep. Both audits are read-only.

**Verified:** dispatcher selects INV for `1122/5122/7122`; 16-byte INV alignment; FE00/FE04/FE05 size adjustments; no FE01 header addition in `cmSmartRequest`; actual-length-based `cmSmartFlush`; differing queue padding and explicit margins; older palette helper's modeled reservation gap.

**Not verified:** true physical allocation bounds; whether the CE manager guarantees at least `E-R` unused bytes for every FE01 call; actual ring wrap behavior on hardware; whether the older code ever writes out of bounds; any relation between the CE commands and observed Windows 98 ring hangs. These require complete live execution traces or a controlled target-board test.

**Research conclusion:** VIA retained one command-manager family for VX800/VX855/VX900, but the **caller-side packet framing** and the **chip-specific wrap alignment** differ. For reuse in a unified Windows 98 driver, the safe common API is a bounded packet builder with an explicit requested/actual accounting contract and chipset-specific transport/wrap configuration—not the unmodified CE reservation arithmetic.

<a id="c9-section-148"></a>

#### 148. CE command-manager critical section

**Why this revises the earlier interpretation.** The §86–90 analysis established that `C3DUtils::Enter3DEngine`/`Leave3DEngine` surround *late* 3D submission, and left command-construction serialization unresolved. Inspection of the independent **command manager** resolves that question for one important branch. The original retail CE DLL's import symbols identify `0x1006CD82` as **`_EnterCriticalSection`** and `0x1006CD88` as **`_LeaveCriticalSection`** (retail linker map, `coredll.dll`). These are synchronization calls, **not logging routines**.

At `cmSmartRequest` `0x1004E42E`, the final `int` argument at `[ebp+0x14]` controls an early lock acquisition:

```text
1004E431  cmp dword ptr [ebp+14h], 0
1004E43B  je  1004E446
1004E43D  push dword ptr [edi+78h]    ; manager critical-section pointer
1004E440  call 1006CD82             ; EnterCriticalSection
...                                 ; request and reservation logic follows
```

The manager's own lock is stored in the object at `CMD_INTERFACE+0x78`. The distinct `C3DUtils` engine lock is addressed through its own object; these must not be conflated. The lock interpretation is confirmed by the import table and by exact call sites, not merely inferred from the helper names.

**Confirmed scope:** A caller setting the final request argument to `1` acquires the command-manager lock **before packet construction**. A caller setting it to `0` does not acquire it in this function; it may be continuing a sequence locked elsewhere. This does not establish that arbitrary external threads can call the routines without additional serialization.

<a id="c9-section-149"></a>

#### 149. `cmSmartFlush` is the complementary unlock operation, with a retained-lock mode

`cmSmartFlush` (`0x1004E51D`) receives `(interface, emitted_bytes, final_flag)` and computes the length to account for from the saved insertion position plus the emitted bytes (`0x1004E52C–0x1004E534`). The third argument selects different *successful* behavior:

| State / path | Retail code | Observed outcome |
|---|---|---|
| `final_flag != 0`, nonzero bytes, enough reported room | `0x1004E53B–0x1004E565` then `0x1004E577` | Complete pending header, call release callback, reset pending count, **leave manager critical section** |
| `final_flag == 0`, nonzero bytes, enough reported room | `0x1004E56E–0x1004E58E` | Complete header, increase pending count at `+0x48`, deduct accounted bytes, **retain manager critical section** |
| Either path, reported free bytes below computed length | `0x1004E550–0x1004E577` or `0x1004E572–0x1004E577` | Skip normal finalization for that flush and **leave manager critical section** |
| Zero bytes with `final_flag == 0` | `0x1004E56E–0x1004E577` | Leave manager critical section (special/edge path; caller semantics not fully traced) |

The actual unlock instruction is `0x1004E57A: call 0x1006CD88`. The routine returns `void` in the mapped C++ signature (`?cmSmartFlush@@YAXPAU_CMD_INTERFACE@@KH@Z`). This matters because its size-failure path does not return a conventional error code to the caller.

**Correction to §143:** `cmSmartFlush` is *not* always a successful finalizer of the actual emitted bytes. In its insufficient-space branch it may instead take the unlock path without invoking `CompleteCommand`. Since the caller has already written its packet data before this check, the branch cannot retroactively prevent a write beyond a too-small reservation.

<a id="c9-section-150"></a>

#### 150. Both VIA 3D blit backends take the manager lock early in the buffered branch

The **H6** blit for VX800/VX855 and **H5S2** blit for VX900 use matching two-stage synchronization. Their `CMD_INTERFACE+0x70` selector controls whether they build into the command manager's buffer or into the alternative direct-command buffer.

| Event | `fn3DBlt_H6` | `fn3DBlt_H5S2` |
|---|---|---|
| Test manager/direct mode | `0x10064525–0x10064537` | `0x100654AE–0x100654C0` |
| **Buffered:** `cmSmartRequest(..., 1)` | `0x1006453D–0x10064548` | `0x100654C6–0x100654D1` |
| **Direct:** choose `0x100CDD60` shared output buffer | `0x1006455C` | `0x100654E5` |
| Save global insertion pointer while building | `0x10064568` | `0x100654F1` |
| Enter *separate* 3D engine lock late | `0x10065441` | `0x1006648F` |
| **Buffered:** final `cmSmartFlush(..., 1)` | `0x1006545A–0x1006545E` | `0x1006645B–0x100664AB` (ESI set to 1) |
| **Direct:** call `KickOffCmd_Inv` | `0x10065468–0x10065474` | `0x100664B5–0x100664C1` |
| Leave 3D engine lock | `0x1006547F` | `0x100664CC` |

**Result:** The buffered mode holds the **command-manager critical section during most packet construction**, then acquires the **3D-engine critical section** for final submission. It releases the manager lock in the final flush while still inside the separate engine lock. The **direct mode does not enter the command manager's lock through this function** before constructing its command buffer, and relies on the later engine lock and/or higher-level caller serialization.

This substantially narrows the open item in §89. It is **not** proof of a race in direct mode: the outer GDI or display-driver entry point may serialize calls. Nor is the presence of nested locks proof of deadlock; the opposite acquisition order has not been demonstrated.

<a id="c9-section-151"></a>

#### 151. The older FE01 helper relies on a different lock-sequencing contract

`v8BppImageTransferCN896_AGP` (`0x10021E8A`) supplies **`0`** as the fourth argument to `cmSmartRequest` (`0x10021EAB`, `0x10021EBF`). Its terminal `cmSmartFlush` supplies **`1`** (`0x100221B3`, `0x100221C2`). In other words, the helper **does not acquire** the manager critical section locally but invokes the terminal release path.

That pattern suggests it may be called as a **continuation of an already locked outer sequence**, rather than as an independent transaction. The compiled helper does not prove who acquires the lock, whether every call site observes the contract, or whether a separate serialization convention applies. Therefore:

- Do **not** copy the helper's `(request_flag=0, final_flag=1)` combination into an independently callable Windows 98 function.
- Do **not** assume that this source is a demonstrated unbalanced unlock bug without tracing its indirect function-pointer callers.
- Treat a command batch's lock ownership as an explicit invariant, alongside its available-byte accounting.

<a id="c9-section-152"></a>

#### 152. Additional reservation boundary: the INV request callback writes alignment padding first

In `cmRequestBuffer_inv` (`0x1004F01C–0x1004F040`), the routine performs:

```text
1004F022  mov eax, [ecx+14h]       ; current write cursor
1004F027  and dword ptr [eax], 0    ; writes a DWORD of zero padding
1004F02A  add eax, 4
1004F02D  test al, 0Fh
1004F02F  jne 1004F027
1004F031  mov [ecx+14h], eax       ; aligned cursor
1004F037  push ecx
1004F03B  call dword ptr [ecx+4Ch] ; queue-space callback
```

With the usual DWORD-aligned cursor, that loop writes **0, 4, 8 or 12 bytes** before invoking the low-level capacity callback. The same family uses 16-byte alignment for VX800/VX855/VX900 (§141). This is *separate* from the subsequent FE01 header/payload-size discrepancy discussed in §§135–143.

**Safety limit:** The manager may maintain space for this alignment in its ring bookkeeping. A pre-callback write is not proof of an out-of-bounds store. It is, however, an additional byte count that a new, bounded packet builder must guarantee **before** any padding store.

<a id="c9-section-153"></a>

#### 153. Insufficient-space final flush is a no-error-return path, not a logging call

Consider the previously modeled VX855/CN896 FE01 example (two rows, 33 output bytes each, insertion pointer already 16-byte aligned):

| Quantity | Bytes |
|---|---:|
| Caller reservation `R` | 72 |
| Actual packet output `E` | 96 |
| Hypothetical reported contiguous room | 72 |

`cmSmartRequest` can accept `R <= free`; the caller then emits the packet. In `cmSmartFlush`, a comparison of the **computed** used length against `+0x28` can fail, leading to `0x1004E577`, which **leaves the critical section without normal header completion/release**. The mapped return type is `void`. The previously misidentified call at `0x1004E57A` is **`LeaveCriticalSection`**, not a diagnostic or error logger.

This demonstrates a **conditional silent failure mode in the compiled control flow** when the *reported* buffer capacity is insufficient. It does **not** prove that CE's actual queue/allocator state ever permits this example, nor whether a prior buffer write crosses a physical mapping. The `+0x600/+0x400` queue-management margins (§144) may prevent the scenario on normal paths. The reconstruction still cannot establish their invariant for every caller.

<a id="c9-section-154"></a>

#### 154. INV ring-space waiting can spin without visible timeout or yield

`GetRingAGPSpace_inv` (`0x1004EDFE–0x1004EF35`) has several near-local backward branches that poll the MMIO-mapped queue progress register `0x43C` (see §§197–204). Examples include `0x1004EE45–0x1004EE4C`, `0x1004EE69–0x1004EE75`, and `0x1004EEC0–0x1004EED1`. The inspected tight loops do **not** contain a timeout test, `Sleep`, or explicit CPU yield. A read pointer that does not meet the comparison can therefore keep the calling thread in the corresponding loop.

The same function checks `MMIO+0x47C` in a VX900-specific decision path (`0x1004EEDF–0x1004EEFC`); this is **not** a generic VX855 instruction to substitute for an unrelated `0x43C` write port. The observed user-reported VX855 ring hangs, lockup recovery and mouse starvation make this worth testing, but **the CE routine is not a proven root cause of any Windows 98 issue**.

Practical safeguards when adapting these ideas to the Windows 98 driver: bound wait time by a timestamp or iteration ceiling, keep critical-section duration short, capture queue read/write status on timeout, and reset/reject a timed-out batch rather than spinning indefinitely. These are design recommendations, **not** decoded VIA hardware requirements.

<a id="c9-section-155"></a>

#### 155. Reproduction, conclusions and next audit boundary

The read-only `Chrome9_CE_Lock_and_Flush_Control_Flow_Audit_148_155.py` verifies the original retail DLL SHA-256 (`529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`), linker-map symbols, instruction anchors for the two 3D backends and FE01 helper, manager lock/unlock paths, INV pre-request alignment writes, and ring-space polling sites. It also checks representative *logical* request-versus-emission size cases. It **does not execute** driver code or GPU commands.

**Confirmed:** the manager and 3D-engine locks are distinct mechanisms; buffered 3D construction can be serialized earlier than previously understood; successful non-final flushes can retain the manager lock; insufficient-space flushes release rather than report an error; the INV request callback writes alignment DWORDs before consulting its space handler; and some CE queue wait loops have no visible timeout/yield.

**Still unresolved:** direct-buffer upstream serialization, lock pairing across all 2D helpers, the physical buffer/headroom invariant for FE01, actual hardware queue-progress behavior, and whether a generation-specific queue setting can help the Windows 98 ring-fetcher experiment. No source was changed in the Windows 98 driver, and no hardware access occurred.

**Unified driver design consequence:** make the *transaction boundary* explicit: acquire once, reserve all header/data/alignment bytes, write under a verified capacity bound, perform a success/failure-returning flush, then release. Avoid mutating a global command cursor before locking, and avoid relying on a queue stall to resolve itself during an unbounded critical-section spin. For the original VIA CE binary, these are **porting guidelines**, not assertions that the shipped implementation is necessarily broken.

### FE01 caller handoff and normal transfer bounds

<a id="c9-section-156"></a>

#### 156. FE01 caller and helper lock handoff

The earlier §151 correctly observed that `v8BppImageTransferCN896_AGP` (`0x10021E8A`) calls `cmSmartRequest(..., lock_flag=0)` at `0x10021EBF` but ends with `cmSmartFlush(..., final_flag=1)` at `0x100221C2`. §151 left the **outer lock owner** unresolved. Tracing the actual `VIA::v8BppImageTransfer` indirect wrapper **resolves this for the compiled AGP-labelled VX855 and VX900 8-bpp caller paths**.

The member wrapper at `0x10012BC6–0x10012BE5` forwards six arguments through global function pointer `fp2D_v8BppImageTransfer` at **`0x100CA324`** (`0x10012BDB: call dword ptr [0x100CA324]`). It is not a distinct 8-bpp converter. Its target is installed by the per-chip 2D initialization routine (see §158).

**Actual VX855 AGP transfer:**

```text
v8BppXferVX855_AGP (0x100237F7)
  0x10023896: cmSmartRequest(FE00, 4*width*height + 0x1C0, lock=1)
  ... construct register/BitBLT packet ...
  0x10023AE3: cmSmartFlush(..., final=0)
  0x10023B1D: VIA::v8BppImageTransfer(...)
       0x10012BDB: call [fp2D_v8BppImageTransfer]
                  -> v8BppImageTransferCN896_AGP (0x10021E8A)
                      0x10021EBF: cmSmartRequest(FE01, payload_bytes, lock=0)
                      ... palette expansion and FE01 packet ...
                      0x100221C2: cmSmartFlush(..., final=1)
  return success from outer v8BppXferVX855_AGP
```

`cmSmartFlush(..., final=0)` **retains the manager lock on its successful nonzero-byte path** (§149). Consequently, the helper's `lock=0` is consistent with a continuing transaction and the helper's `final=1` is its normal terminal unlock. **This is not evidence of an unbalanced lock on the traced successful AGP path.** The existence of caller-side lock ownership does not prove the insufficient-space/error path is safe (§160).

<a id="c9-section-157"></a>

#### 157. Nine same-function AGP call chains use the same lock-handoff idiom

The original linker map defines the following functions. Direct disassembly of their complete address ranges found the order **manager request with lock flag `1` -> intermediate flush with `0` -> `VIA::v8BppImageTransfer` call** in every case. The compiled functions have one return instruction each, after the helper call. These are *static instruction counts and relative order*, not runtime call frequencies.

| Compiled outer function | Entry | `cmSmartRequest` | Non-final `cmSmartFlush` | Member helper call |
|---|---:|---:|---:|---:|
| `v8BppXferVX800_AGP` | `10018463` | `10018502` | `1001874F` | `10018789` |
| `vPat8BppXferVX855_AGP` (also VX800 alias) | `10019F3B` | `10019FEE` | `1001A4CD` | `1001A4FE` |
| `v8BppXferCX700_AGP` | `1001CDE9` | `1001CEC9` | `1001D08C` | `1001D0AC` |
| `vPat8BppXferCX700_AGP` | `1001DE40` | `1001DF17` | `1001E1E4` | `1001E204` |
| `v8BppXferCN896_AGP` | `10020D80` | `10020E2B` | `10020FEE` | `1002100E` |
| `vPat8BppXferCN896_AGP` | `1002166B` | `1002170E` | `100219DB` | `100219FB` |
| `v8BppXferVX855_AGP` | `100237F7` | `10023896` | `10023AE3` | `10023B1D` |
| `v8BppXferVX900_AGP` | `100269B2` | `10026A5E` | `10026D2D` | `10026D61` |
| `vPat8BppXferVX900_AGP` | `10027900` | `100279BE` | `10027E9D` | `10027EC8` |

**Compiler detail:** two patterned functions pass zero to the non-final flush through register `EBX`, not a literal `push 0`: `vPat8BppXferCX700_AGP` clears `EBX` at `0x1001E16B` before pushing it at `0x1001E1D6`; `vPat8BppXferCN896_AGP` clears `EBX` at `0x10021962` and pushes it at `0x100219CD`. No intervening write to `EBX` is observed in their respective ranges. A simplistic immediate-only search would miss these two matches.

**Qualification:** only the specified nine compiled AGP wrappers were checked as entire functions. The result establishes the *common intended transaction structure*; it does not prove a complete program-wide locking invariant for every possible 2D or 3D entry point.

<a id="c9-section-158"></a>

#### 158. Function-pointer initialization prevents mixing AGP and direct-MMIO helpers in the normal selected mode

`init2DbltfpVX855` (`0x1002622A`) and `init2DbltfpVX900` (`0x10029295`) test **`CMD_INTERFACE+0x70`**, then install both an outer-transfer callback and the matching palette helper. The installer uses the same global helper slot `0x100CA324` that the member wrapper calls.

| Chipset | `CMD_INTERFACE+0x70` | 8-bpp outer transfer callback | 8-bpp helper at `0x100CA324` |
|---|---:|---|---|
| VX855 | `0` (AGP-labelled path) | `v8BppXferVX855_AGP` (`100237F7`) | `v8BppImageTransferCN896_AGP` (`10021E8A`) |
| VX855 | Nonzero | `v8BppXferVX855` (`1002356F`) | `v8BppImageTransferCX700` (`1001FE44`) |
| VX900 | `0` (AGP-labelled path) | `v8BppXferVX900_AGP` (`100269B2`) | `v8BppImageTransferVX900_AGP` (`10028D7B`) |
| VX900 | Nonzero | `v8BppXferVX900` (`10026724`) | `v8BppImageTransferCX700` (`1001FE44`) |

**Source addresses:** VX855 initializer `0x10026232–0x10026312`; VX900 initializer `0x1002929D–0x1002937D`. In the non-AGP branch the shared `v8BppImageTransferCX700` (`0x1001FE44–0x1002000C`) **does not call** `cmSmartRequest` or `cmSmartFlush`. Thus the existence of a call to `VIA::v8BppImageTransfer` from a non-AGP outer function does **not** imply a missed command-manager lock: the installed helper is different.

This is *compiled configuration behavior*. A separate concurrent reinitialization or callback replacement was not demonstrated; the initialization tables alone cannot prove thread safety if those globals are changed while draws execute.

<a id="c9-section-159"></a>

#### 159. Re-evaluating the FE01 under-request in the context of the complete transaction

§§134–155 found that the CN896-derived FE01 helper may **request fewer bytes locally than it writes**. The new call-chain evidence is an important qualification: it normally follows a comparatively generous **outer `FE00` reservation under the same retained manager lock**. The outer FE00 family also receives an internal 16-byte request adjustment from `cmSmartRequest` on VX800/VX855/VX900 (§142), whereas the inner FE01 family does not.

For an illustrative 33×2-pixel, 8-bpp source to **16-bpp RGB565** destination (66 converted output bytes/row), compiled request formulas give:

| Request or emission stage | VX855 / CN896-derived | VX900 |
|---|---:|---:|
| Outer register/setup request (before FE00's automatic +16) | `4*(33*2)+0x1C0 = 712` bytes | `2*(align4(66)+32)+0x124 = 492` bytes |
| Inner FE01 helper request | `align4(66)*2 = 136` bytes | `(align4(66)+32)*2+16 = 216` bytes |
| Older helper's **modeled** combined FE01 emission, aligned insertion | `align16(136+16) = 160` bytes | Different per-row packet layout; not interchangeable with this expression |
| Older FE01 helper local request/emission gap | **24 bytes** | Inner helper budgets row packet overhead explicitly |

The VX855 outer reservation includes substantially more than the setup instructions alone are likely to use, **but the exact live remaining-byte invariant has not been established across every branch and queue boundary**. The manager may also refresh its buffer between the non-final flush and inner request. Therefore the earlier 24-byte local mismatch is **not a proven physical overflow** in the normal full transaction.

**Porting implication:** keep separate counters for *outer setup reservation*, *outer emitted bytes*, *inner helper request*, *inner emitted bytes* and *remaining contiguous command capacity*. Do not infer memory safety solely from the helper's local request argument; equally, do not infer that the manager always supplies the missing bytes without testing its preconditions.

<a id="c9-section-160"></a>

#### 160. Conditional error path remains: a failed non-final flush breaks the intended lock handoff (normal unpatterned sizing qualified in §169)

The successful lock pairing shown above has one notable exception in `cmSmartFlush` (`0x1004E51D`). With `final_flag=0`, the routine compares its computed emitted length against reported free bytes at `CMD_INTERFACE`'s substructure `+0x28`:

```text
1004E56E  compare emitted byte argument with zero
1004E572  compare computed used bytes with reported remaining space
1004E575  if enough space -> 1004E581 (complete/retain-lock path)
1004E577  otherwise -> LeaveCriticalSection(manager_lock)
1004E581  complete header; increment pending; deduct bytes; keep lock
```

If an outer function reaches the **insufficient-space** path at `0x1004E577`, it releases the command-manager lock despite its `final=0` request. The outer AGP transfer routines have no error return from this `void` flush and then invoke the helper; the helper submits `cmSmartRequest(...,lock=0)` and finally `cmSmartFlush(...,final=1)`. This yields a **potential unmatched terminal unlock after a failed intermediate flush**, conditional on that failure path being reachable. No evidence establishes that valid transfers actually cause it, or how Windows CE's critical-section implementation reacts in this case.

This is a **control-flow hazard to test**, not a verified runtime crash or an assertion that the shipping CE driver corrupts state. The generous outer reservations may make this path effectively unreachable on normal dimensions; proving that requires the caller/manager free-space bound, not just the presence of the branch.

<a id="c9-section-161"></a>

#### 161. What is—and is not—now known about broader 2D synchronization

The original CE DLL contains **15 direct calls** to the `VIA::v8BppImageTransfer` member trampoline in the traced transfer-function ranges. Nine occur in AGP-labelled wrappers with the early-lock/non-final/final-handoff pattern (§157). The remaining six occur in non-AGP-labelled variants; on the VX855/VX900 non-AGP configuration, the function pointer selects the different CX700 direct transfer helper (§158). Thus the earlier concern about this helper's terminal unlock is **resolved for these successfully paired compiled AGP paths**, without making a family-wide assertion about every graphics callback.

The command-manager lock at `CMD_INTERFACE+0x78` must remain distinct from the 3D engine lock investigated in §§148–150. The static presence of both cannot prove a deadlock or a race without an opposite-order path or actual concurrent call sequence.

<a id="c9-section-162"></a>

#### 162. Practical consequences for the VX855 Windows 98 8-bit DIB acceleration path

The strongest reusable feature here is the **transaction structure**, not VX900's more fragmented per-row packet formatting. A Win98 implementation that expands 8-bpp indexed pixels to RGB565 or XRGB8888 on the CPU, then submits host BitBLT data, can implement these safer invariants:

1. Acquire the command queue lock before creating a register/setup packet. Track lock ownership explicitly instead of representing it only through a request flag.
2. Include FE00, FE01, per-row headers, end padding, and pre-request alignment in an overflow-checked byte reservation; recheck capacity *before* writing any packet bytes.
3. Make intermediate flush **return a status**. On failure, abort the transaction and release exactly once; never blindly continue into a helper that expects the lock to be held.
4. Keep the 8-bpp color-key and palette lookup rules separately configurable for VX855/VX900; their CE outer routines do not emit identical state (§§112–133).
5. Before changing the VX855 command queue/ring backend, validate the new host BLT path in its existing known-good direct submission mode. Do not assume that successful FE01 construction implies a working ring fetcher.

These are **design/test recommendations** based on the source audit. No Windows 98 binary was changed or test was run.

<a id="c9-section-163"></a>

#### 163. Audit coverage and remaining question

`Chrome9_CE_2D_Outer_Transaction_Audit_156_163.py` verifies the original retail DLL SHA-256 (`529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`), the VX855/VX900 callback tables, the `0x100CA324` member trampoline, the nine AGP call-site sequences and their single-return envelopes, the two zero-valued `EBX` flush-flag cases, the CN896/VX900 inner helpers, the direct helper's lack of command-manager calls, the outer reservation arithmetic, and the conditional unlock site. It is a **static read-only audit**; it does not load any DLL, touch hardware, or verify runtime buffer bounds.

**Updated status:** The apparent unmatched FE01 helper unlock was a **missing-caller-context issue on the normal AGP path**. The local request/emission size discrepancy is still present, but its actual safety now depends on the **entire two-stage reservation and non-final flush contract**. The next high-value investigation is a formal upper bound for the outer setup packet plus the manager's re-reservation/wrap handling, especially around the `0x1004E572` conditional unlock. Until then, do not classify the CE driver as having a demonstrated overflow, crash, deadlock, or unbalanced lock during successful transfers.

<a id="c9-section-164"></a>

#### 164. Transaction-level capacity audit

**Question carried forward from §163:** Can the older CN896-derived FE01 helper's local byte under-request (or the `cmSmartFlush` conditional failure branch) occur in an ordinary, successful VX855/VX900 **unpatterned AGP-labelled 8-bpp transfer**?

This pass **distinguishes the logical manager budget from physical DMA-ring storage**. It uses the original retail `DDI_VIA.dll` (SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`), the map's named routines, their machine instructions, and a read-only bounded-size model. It does not execute the CE DLL, exercise GPU silicon, or validate all image format/clip variants. The reproducible checker is `Chrome9_CE_Capacity_Proof_Audit_164_171.py`.

**New conclusion:** the earlier `FE01` *local* reservation mismatch does **not** cause the manager's insufficient-space flush on the ordinary paired AGP paths for valid, non-overflowing dimensions in the modeled 8/16/32-bpp destination formats. The outer setup requests are deliberately much larger than the setup actually emits. This is a **conditional static proof about the manager's reported capacity**; it must not be described as a proof of physical memory safety, successful ring completion, or of all other transfer paths.

<a id="c9-section-165"></a>

#### 165. The INV manager's available-space counter is a requested-size budget, not direct physical capacity

`cmRequestBuffer_inv` (retail `0x1004F01C`) aligns the software write pointer to a 16-byte boundary, stores the requested capacity in the DMA-manager field `+0x24` at `0x1004F034`, and calls its installed queue-space callback (`[ecx+0x4C]`, `0x1004F03B`). The installed `GetRingAGPSpace_inv` routine (`0x1004EDFE`) performs physical queue/read-pointer checks, but **on its common return path** copies the request value back to the logical available-byte field:

```asm
1004EF2B  mov eax, [esi+24h]   ; previously requested byte capacity
1004EF2E  pop edi
1004EF2F  mov [esi+28h], eax  ; advertised logical byte budget
```

`cmSmartRequest` (`0x1004E4BA–0x1004E4D6`) then compares later requests against this logical `+0x28` budget; `cmSmartFlush` deducts its calculated emitted length from the same field on a successful non-final path (`0x1004E572–0x1004E58B`). Hence **`+0x28` is not a continually measured physical-end-minus-current-pointer value**. Treating it as such would lead to misleading overflow claims.

The INV queue callback also checks an end-relative address using `requested + 0x600` at `0x1004EE23–0x1004EE2F`, with a later `+0x400` condition at `0x1004EE53–0x1004EE5F`. The first check can provide significant physical distance from the queue end on its simple non-wrap path. That does **not** prove the same physical invariant after every rewind, or if the request's 32-bit arithmetic wraps.

<a id="c9-section-166"></a>

#### 166. FE00 finalization has a separate optional eight-byte pad

The family manager treats `FE00` specially: `cmSmartRequest` adds **16 bytes** to the caller's request (`0x1004E46B–0x1004E475`) and returns a pointer **after** the reserved 16-byte packet header (`0x1004E4FD–0x1004E515`). The outer call therefore makes two different quantities relevant:

- **Requested budget:** `outer_request + 16`.
- **Successful non-final flush charge:** `16 + setup_payload_bytes`.

There is one additional subtlety. `CompleteCommand` (`0x1004E3F7–0x1004E427`) writes the FE00 header, computes `setup_payload_bytes >> 3`, and if the low bit is set, appends **two zero DWORDs**. Its final write pointer advances by **8** in that case. `cmSmartFlush`'s earlier logical budget deduction was calculated **before** those extra completion bytes. Thus the logical budget can overstate the physical bytes remaining by up to eight bytes after a particular FE00 completion.

This is a **separate accounting detail**, not evidence of an overwrite; the outer allocation formulas below leave hundreds of additional bytes in the normal two-stage transfer. A new Windows 98 packet builder should charge packet-completion padding explicitly rather than relying on the queue's guard distance.

<a id="c9-section-167"></a>

#### 167. VX855 ordinary AGP 8-bpp transfer: full two-stage budget bound

The compiled VX855 outer routine `v8BppXferVX855_AGP` (`0x100237F7`) requests

`R_outer = 4 * width * height + 0x1C0` at `0x1002386F–0x10023896`, with `lock=1`. Its setup cursor is restored to the saved insertion pointer at `0x1002397A`. From that point to the non-final flush (`0x10023AE3`), there are **16 syntactic four-byte cursor advances**, with no backward branch in the examined setup region. Counting even the mutually exclusive branch alternatives gives a deliberately **conservative maximum 64 bytes of setup words**. The `FE00` 16-byte header is handled separately.

For a positive width and height with an ordinary 8-, 16- or 32-bit destination, the older CN896-derived helper's request is

`R_inner = align4(width * destination_bytes_per_pixel) * height` (`0x10021E94–0x10021EBF`). Since `align4(width * bpp_bytes) <= 4*width` for `bpp_bytes` in `{1,2,4}`, `R_inner <= 4*width*height`. From a 16-byte-aligned insertion point, the FE01 single-block output occupies at most `align16(R_inner + 16) <= R_inner + 28` bytes (16-byte header plus zero to 12 trailing alignment bytes). The successful outer FE00 completion yields a 16-byte-aligned cursor; its possible extra eight bytes are separately noted above.

**Conservative *logical* capacity left after the inner FE01 packet:**

```text
outer_request + 16                          # FE00 manager reservation
  - (16 + outer_setup_words)               # cmSmartFlush logical charge
  - FE01_emitted_bytes
>= (4*w*h + 448) - 64 - (4*w*h + 28)
>= 356 bytes
```

Even allowing an additional eight uncharged FE00 completion bytes, the modeled physical write-pointer displacement remains under the initial request by **at least 348 bytes**, before considering any INV queue end guard. This is a bound on the **normal transaction's emission arithmetic**, not on ring-space completion or pointer validity after a hardware stall.

**Worked example:** `33×2`, 16-bpp output (`66` bytes/row): outer request `712`, FE00-adjusted budget `728`, helper request `136`, FE01 modeled output `160`. With the conservative 64-byte setup allowance, logical capacity after FE01 is **at least `712 - 64 - 160 = 488` bytes**. This resolves the older helper's **24-byte local discrepancy** in the context of this successful outer transaction. The helper was not designed to be evaluated as an independent caller with only its local `136`-byte budget.

<a id="c9-section-168"></a>

#### 168. VX900 ordinary AGP transfer also carries ample combined budget, but uses row packets

The compiled `v8BppXferVX900_AGP` at `0x10026A29–0x10026A5E` requests

`R_outer = (align4(output_row_bytes) + 32) * height + 0x124`.

The setup cursor is restored at `0x10026B32`, then advances in **20 syntactic four-byte steps** before the non-final flush (`0x10026D2D`). The relevant branch region has no backward jump; even counting both sides of conditional alternatives gives an 80-byte **upper bound** on normal setup words.

The corresponding `v8BppImageTransferVX900_AGP` helper (`0x10028D7B`) requests

`R_inner = (align4(output_row_bytes) + 32) * height + 16`.

The row term follows from `0x10028D89–0x10028DB3`: the compiler chooses `floor(row_bytes/4)+8` DWORDs for aligned row-byte sizes and `floor(row_bytes/4)+9` for nonmultiples of four, then multiplies by 4. Those cases are algebraically the same `align4(row_bytes)+32`. Individual FE01 row headers and per-row 16-byte alignment are visible in the helper; the final output cursor is also aligned at `0x1002926D–0x10029288`.

**VX900 logical room remaining *after the entire inner request*:**

`R_outer - outer_setup - R_inner >= 0x124 - 80 - 16 = 196 bytes`.

A conservative row-packet model allows `align16(16+align4(row_bytes))` bytes per row and 12 initial leading-alignment bytes. This is at most the inner helper's explicit request and yields **at least 204 bytes** remaining under that model. This check covers the inspected standard-format packet structure; it does not replace a control-flow proof of every format-specific pixel read and emission branch.

For the same `33×2` 16-bpp output example: VX900 outer request **492**, inner request **216** and modeled aligned row packet output **192 bytes** (two 96-byte packets). Allowing 12 leading bytes and 80 setup bytes still leaves **208 bytes**. The earlier claim that VX900's smaller outer reservation necessarily increased overwrite risk is **not supported for this successful normal transaction**.

<a id="c9-section-169"></a>

#### 169. Reclassifying the conditional failed-flush/unlock concern from §§160 and 163

At `0x1004E572–0x1004E577`, a non-final `cmSmartFlush` can release its lock without completing the packet **if the computed emitted length exceeds the logical free budget**. That branch is real and should remain documented.

For the two **ordinary, unpatterned** AGP 8-bpp outer functions just traced, the branch is **not reachable under the normal positive-dimension, non-overflowing, successful-initial-request model**: the initial outer request always includes at least `0x1C0` (VX855) or `0x124` (VX900) fixed allowance, while the corresponding loop-free setup word counts are at most 64 or 80 bytes. A successful manager request refreshes the logical budget with the caller's adjusted reservation (§165), which necessarily exceeds the setup charge.

This **narrows**, rather than erases, the earlier warning: an insufficient-space flush could still be reached through a *different* caller, invalid/overflowing dimensions, an unexpected manager state transition, or a failed/aborted lower-level request. It is wrong to label the ordinary successful VX855/VX900 8-bpp path as having a demonstrated unmatched terminal unlock. The original conditional branch and its missing error return remain useful defensive-driver design evidence.

<a id="c9-section-170"></a>

#### 170. Boundaries not yet proven: integer overflow, patterned transfers and physical ring wrap

The ordinary proofs explicitly assume nonzero supported dimensions and no 32-bit arithmetic overflow. The compiled outer calculations use x86 32-bit `imul`, `shl`, `lea`, and `add` operations; this pass has **not** established public API clipping/validation constraints that make every mathematical overflow unreachable. For example, the VX855 outer `4*width*height + 0x1C0` expression cannot be treated as unbounded mathematics once computed in 32-bit machine registers.

Other unclosed cases are:

1. **Patterned 8-bpp wrappers:** `vPat8BppXferVX855_AGP` and `vPat8BppXferVX900_AGP` use larger fixed outer allowances (`0x360` and `0x2C4` respectively), but their setup logic includes more branches and pattern data; this pass did not bound all emitted words or all fallbacks.
2. **All row-format paths:** the FE01 bound assumes standard packed output (1, 2 or 4 bytes/pixel), valid palette/table pointers and positive pitches; it does not establish the behavior of unsupported conversion selectors or malformed source strides.
3. **Physical ring storage/wrap:** `GetRingAGPSpace_inv` invokes read-pointer polling, kick-off and rewind branches; mathematical headroom based on software requests is not proof that the physical queue allocation, DMA read pointer or GPU completion is valid at every wrap boundary.
4. **Failure propagation:** the low-level allocation/queue request callbacks and outer fallback behavior still need status and timeout analysis. The ordinary arithmetic proof presupposes the initial request returned a usable insertion pointer.

For a new VX855 driver, explicit `size_t` overflow checks, positive rectangle validation, a bounded queue reservation API, and a flush result code remain preferable to copying this legacy manager's assumptions.

<a id="c9-section-171"></a>

#### 171. Reproducible verification, status change and next research target

`Chrome9_CE_Capacity_Proof_Audit_164_171.py` independently opens the original CE retail DLL archive, checks its SHA-256, verifies the exact named machine-instruction sites for requested-size bookkeeping, the `0x600/0x400` wrap tests, and the FE00/FE01 call sites, and enumerates the setup cursor advances in **both** loop-free normal paths. It validates **5,376** combinations of dimensions and standard destination bytes-per-pixel against the derived arithmetic bounds. All checks pass on the supplied retail image.

| Open item carried forward | Updated status |
|---|---|
| CN896 AGP FE01 helper requests fewer bytes than it emits | **Verified locally**, but **covered by outer budget** for normal unpatterned VX855 AGP path under specified valid-input conditions |
| VX900 AGP 8-bpp helper uses too little space due to per-row packet framing | **Not supported** by modeled combined outer/helper budgets for the normal path |
| Non-final `cmSmartFlush` can drop completion and unlock on insufficient budget | **Verified conditional code**, but **unreachable by the ordinary unpatterned VX855/VX900 outer sizing model** if the initial manager request succeeds |
| FE00 finalizer may add 8 bytes not reflected in free counter | **Verified static accounting detail**; combined normal-path surplus remains much larger |
| Patterned transfer / invalid input / queue wrap-around capacity invariant | **Not resolved**, highest-value next static checks |
| Hardware ring read-pointer progress, stall recovery and actual memory safety | **Not tested**, requires controlled target-device trace |

**Suggested next continuation:** bound all patterned-transfer emissions, then trace the edge behavior of `GetRingAGPSpace_inv` around reset/rewind and 32-bit request overflow. This will provide a much better basis for connecting CE buffer handling to the existing Windows 98 VX855 command-fetcher investigation.

<a id="c9-section-172"></a>

#### 172. Patterned 8-bpp AGP capacity

This continuation closes the **normal patterned** counterpart to the unpatterned proof in §§164–171. Source: original retail VIA CE `DDI_VIA.dll`, SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`, verified against the extracted user-supplied `VIA Graphics Driver Group2.msi`. The relevant linked outer implementations are `vPat8BppXferVX855_AGP` **`0x10019F3B–0x1001A509`** (also an exact VX800 AGP alias) and `vPat8BppXferVX900_AGP` **`0x10027900–0x10027ED3`**. Both request an `FE00` setup packet through `cmSmartRequest` and subsequently call `VIA::v8BppImageTransfer` after a non-final `cmSmartFlush` (§157).

**Proven here:** a conservative static bound on bytes advanced by the outer command cursor along the normal pattern-accelerated paths, a two-stage *software accounting* bound for 8-, 16- and 32-bpp packed output, and exact 32-bit arithmetic/polling limitations in the command manager. **Not proven:** upstream input validity, actual memory ownership at a DMA-ring boundary, live read-pointer progress, or real-world crashes. The new offline audit script is `Chrome9_CE_Patterned_8bpp_Queue_Bounds_Audit_172_180.py`.

<a id="c9-section-173"></a>

#### 173. Both outer patterned routines construct the same fixed-size pattern upload

At `0x1001A177–0x1001A1B1` (VX800/VX855), and `0x10027B47–0x10027B81` (VX900), nested **8 × 8** loops rearrange 64 pattern values into a local stack scratch array: 64 DWORDs = **256 bytes of temporary data**, not yet GPU command-stream output. The following loops, `0x1001A1C9–0x1001A1FB` and `0x10027B99–0x10027BCB`, each emit exactly **64 register/value pairs** from that scratch array. Each iteration advances the command cursor `ESI` twice by 4 bytes, producing **512 bytes of FE00 pattern register/value commands**. The explicit terminal comparison is `cmp edx,0x40`.

Counting the entire compiled command-building function, each contains **25 syntactic four-byte `ESI` advances**. Two advances execute 64 times in the pattern upload loop; the remaining **23 advances** are outside that loop and, conservatively, can all be counted even where branches are mutually exclusive. Under normal execution with ordinary x86 callee-saved `ESI` handling:

```text
Pattern upload:    64 entries × 2 DWORDs × 4 bytes = 512 bytes
Other FE00 writes: 23 syntactic cursor advances × 4     =  92 bytes
Conservative outer setup cursor bound                <= 604 bytes
```

The two other backward conditional branches in each routine generate the 8×8 *scratch* pattern, not repeated FE00 output; two short backward jumps are control-flow joins, not unbounded loops. This is a deliberately loose upper bound: it counts mutually exclusive register-writing branches together. The direct callback work invoked for rotations manipulates CPU state/scratch rather than receiving the FE00 output cursor as an explicit argument; the analysis presumes standard callee-saved-register discipline, not arbitrary corruption by external callbacks.

<a id="c9-section-174"></a>

#### 174. Patterned VX800/VX855: the older single-FE01 helper's local under-request is covered by the normal outer budget

`vPat8BppXferVX855_AGP` (also VX800 alias) calculates the FE00 request at `0x10019FD3–0x10019FEE` as:

```text
R_outer_855 = 4 × width × height + 0x360     (0x360 = 864 bytes)
```

It calls `cmSmartRequest(...FE00, R_outer_855, lock=1)`; the INV-family FE00 adjustment adds 16 bytes (§142), which accounts for the FE00 header in the subsequent `cmSmartFlush` charge. Its selected CN896-derived 8-bpp helper requests `P = align4(width × destination_bytes_per_pixel) × height` for its older single FE01 payload (§§135,167). With the helper starting 16-byte aligned, its header + payload + tail pad occupy at most `align16(16+P)`. For packed 8/16/32-bpp output, `P <= 4×width×height`, so:

```text
R_outer_855 - FE00_setup_bound - FE01_emitted
  >= (4×w×h + 864) - 604 - (4×w×h + 28)
  >= 232 bytes  [conservative logical budget]
```

The FE00 finalizer's separate possible 8-byte uncharged pad (§166) does not exhaust that bound; an INV pre-request alignment can use up to 12 additional physical bytes, also substantially below the modeled slack. However, those observations do **not** establish exact physical contiguous DMA allocation end pointers or completion safety across a wrap.

<a id="c9-section-175"></a>

#### 175. Patterned VX900: 64 pattern entries plus row-packet FE01 still fit the compiled request formula

At `0x10027992–0x100279BE`, VX900 calculates a destination output-row byte count using the pixel-format-derived left shift, rounds it to 4 bytes and requests:

```text
P_row = align4(width × destination_bytes_per_pixel)
R_outer_900 = (P_row + 32) × height + 0x2C4
R_inner_900 = (P_row + 32) × height + 16  [VX900 AGP FE01 helper]
```

The formulas assume a supported packed 8/16/32-bpp destination, a valid format-selection table, positive dimensions and **no x86 32-bit overflow**. They are consistent with the VX900 helper's per-row FE01 headers (§168). Using the same conservative **604-byte** FE00 setup bound:

```text
R_outer_900 - FE00_setup_bound - R_inner_900
  >= 0x2C4 - 604 - 16
  = 88 bytes  [after the *entire* inner helper request]
```

The per-row stream emits no more than `12 + height × align16(16 + P_row)` bytes, bounded by the helper's `(P_row+32)×height+16` request for all tested positive packed-row sizes. The 88-byte residual is smaller than the unpatterned VX900 bound (§168), but remains positive in the modeled complete, successful outer+inner transaction. This does **not** prove every conversion callback, manager state, or GPU revision obeys the same physical-size contract.

<a id="c9-section-176"></a>

#### 176. Worked example and exact effect on the earlier failed-flush concern

For a `33×2` indexed-to-RGB565 patterned transfer (`destination_bytes_per_pixel=2`, 66 output bytes/row, `P_row=68`):

| Modeled quantity | VX800/VX855 patterned | VX900 patterned |
|---|---:|---:|
| Outer FE00 request *before* manager's +16 | `4×33×2+864 = 1,128` | `(68+32)×2+708 = 908` |
| Outer setup cursor advance, conservative upper bound | 604 | 604 |
| Inner FE01 requested bytes | 136 | 216 |
| Inner FE01 modeled actual byte output | 160 | 192 (two aligned 96-byte rows) |
| Lower bound remaining after modeled actual FE01 output | 364 | 112 |
| VX900 remaining after full **inner request** (not just actual bytes) | — | **88** |

`cmSmartFlush` has a real conditional branch at `0x1004E572–0x1004E577` which releases its manager lock if the non-final emitted amount exceeds its logical free-space counter (§160). **This normal patterned FE00 setup cannot reach that branch under the same successful-initial-request, normal-format, positive-dimension, non-overflowing assumptions as this proof**, because the fixed allowances (`0x360` and `0x2C4`) each exceed the conservative 604-byte setup stream. That narrows the remaining unmatched terminal-unlock concern to invalid dimensions, manager/request failures, other callers and unexpected manager state; it does *not* remove the conditional branch or add an error return to it.

<a id="c9-section-177"></a>

#### 177. Patterned functions explicitly fall back for width ≤ 1; height validity is not locally established

Both patterned wrappers compute `width = right - left` from the rectangle and test `cmp ecx,0x1` before the accelerated request (`0x10019FB1–0x10019FD3`; `0x10027976–0x10027992`). The width-≤1 branch calls the software/emulation helper at `0x10013251` and skips the FE00 request. Thus 1-pixel-wide patterned rectangles do **not** use this specific accelerated AGP path, even though other GPU line/fill paths may handle such shapes.

The corresponding height is also calculated from rectangle coordinates, but there is **no analogous positive-height validation in these two outer functions before the reservation arithmetic**. This is a **missing local check**, *not* evidence that the public blit interface supplies zero/negative heights: upstream clipping, validation and fallback routines could exclude them. All capacity claims in §§173–176 explicitly require valid, positive dimensions.

<a id="c9-section-178"></a>

#### 178. Compiled 32-bit request arithmetic can wrap without a local carry/overflow rejection

Both outer patterns use unchecked 32-bit integer math when computing request bytes: VX800/VX855 use a 32-bit `imul` followed by `lea eax,[eax*4+0x360]`; VX900 uses a variable x86 left shift, 4-byte alignment, `imul` by height and `add eax,0x2C4`. No local `jo`/`jc` guard or signed-range error branch appears between those operations and `cmSmartRequest`. For *illustration only*, arithmetic with positive but implausibly large rectangle dimensions demonstrates that these are modular 32-bit operations:

| Artificial input | True mathematical payload | 32-bit patterned outer request |
|---|---:|---:|
| VX855 `width=4,096`, `height=262,144` | `4×4,096×262,144 = 2^32` bytes | **`0x360`** after wrap |
| VX900 `width=4`, `height=2^29`, RGB565 | `(8+32)×2^29 = 5×2^32` bytes | **`0x2C4`** after wrap |

These are **size-arithmetic counterexamples**, not proven attainable rendering inputs. They do not show that such dimensions survive GPE blit validation, fit VRAM, or reach an actual DMA write. Before reusing these formulas in a Windows 98 driver, check `width>0`, `height>0`, formats/pitches and checked multiplication/addition *before* requesting or touching the command stream.

<a id="c9-section-179"></a>

#### 179. The INV ring-space guard can also wrap arithmetically; polling remains unbounded

`GetRingAGPSpace_inv` (`0x1004EDFE–0x1004EF35`) computes the requested-space end guard using a 32-bit `lea edx,[eax+ecx+0x600]` at **`0x1004EE23`**, then uses an **unsigned comparison** at `0x1004EE2A`. It does not test arithmetic carry/overflow before that comparison. An artificial request of `0xFFFFFA00` with zero current offset would produce `(0+0xFFFFFA00+0x600) mod 2^32 = 0`, potentially reversing the intended magnitude relation. **This illustrates why the guard is not a general checked-size allocator**; it is not proof such a request is accepted from a valid caller.

The same function contains duplicated tight read-pointer polling branches (`0x1004EE45–0x1004EE53`, `0x1004EE69–0x1004EE76`, `0x1004EEA0–0x1004EEAE` and `0x1004EEB7–0x1004EED3`). No local timeout, scheduling yield or failure-status return is visible within those poll loops; normal progress relies on a changing hardware/queue read pointer. The function ultimately copies its requested byte count to the bookkeeping field `+0x28` at `0x1004EF2B–0x1004EF2F`. Neither this code nor the software proof certifies queue survival after a stuck GPU, ring rewind, read-pointer discontinuity or another chipset's wrap rule. The previously verified VX900-only `MMIO+0x47C` branch remains significant (§144).

<a id="c9-section-180"></a>

#### 180. Patterned-transfer risk register and verification

The audit script `Chrome9_CE_Patterned_8bpp_Queue_Bounds_Audit_172_180.py` verifies the original DLL hash, two exact patterned call chains, 25 static cursor advances per function, the 64-entry double-DWORD upload, five backward branches per function, the FE00/FE01 reservation and flush anchors, and key ring-space instruction sites. It evaluates **10,752 positive-size model cases** across widths 2–513, heights 1/2/3/7/16/64/512 and destination byte depths 1/2/4, plus explicit 32-bit wrap examples. All checks pass offline; it does **not** run VIA code or simulate the GPU.

| Outstanding question | Status after §§172–180 |
|---|---|
| Normal unpatterned VX855/VX900 AGP FE00+FE01 budget | **Previously bounded (§§164–171)** |
| Normal patterned VX800/VX855/VX900 AGP FE00+FE01 budget | **New: bounded**, with at least 232 bytes (VX800/VX855) and 88 bytes (VX900) modeled logical residual under stated assumptions |
| Width-1 patterned AGP acceleration | **Confirmed local fallback** to emulation routine |
| Height validation before pattern reservation | **No local positive-height check**; upstream reachability unresolved |
| Arithmetic safety for unreasonable dimensions | **Unchecked 32-bit wrap demonstrated algebraically**; no valid-call exploit/reproduction |
| Non-final flush unconditional correctness | **Conditional failure branch still exists**, but normal patterned capacity model excludes it under successful request |
| Physical ring-space safety, read-pointer liveness, timeout or recovery | **Unresolved; no hardware verification** |

**Next highest-value static step:** trace upstream rectangle validation and the actual lower-level return/lock behavior when queue space cannot be obtained; compare the read-pointer fields and `MMIO+0x47C` branch against the existing VX855 Windows 98 ring-fetcher observations. Do not label these mathematically possible overflow cases as demonstrated Win98 driver crashes.

### Upstream geometry checks, display waits and queue liveness

<a id="c9-section-181"></a>

#### 181. Upstream rectangle handling and queue-read-pointer liveness

This pass re-examines the **original VIA Windows CE retail** `DDI_VIA.dll` (`529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19` SHA-256) and all three supplied **Part II** VIA graphics manuals. It does not execute the CE DLL or access hardware. The companion reproducible checker is `Chrome9_CE_Upstream_FlipWait_and_Ring_Liveness_Audit_181_188.py`. Earlier modeled capacity results in §§164–180 still apply **only** under their successful-initial-request, standard-format, positive, non-overflowing-dimension assumptions.

**Scope of this pass:** `VIA::BltPrepare` (`0x100136A0`), `VIA::BltFunctionSelecter` (`0x1001331B`), `VIA::WrappedBlt` (`0x10013126`), `VIA::BltComplete` (`0x1001284C`), the patterned VX800/VX855 and VX900 AGP wrapper entries, `GetRingAGPSpace_inv` (`0x1004EDFE–0x1004EF35`) and `cmRequestBuffer_inv` (`0x1004F01C`). Direct MMIO and the hardware manuals are cross-checked separately; observed code is not automatically proven silicon behavior.

<a id="c9-section-182"></a>

#### 182. The first BltPrepare wait polls the display-address flip

In the compiled `VIA::BltPrepare` method:

```asm
100136B0  mov  [esi], ebx                ; install default emulation callback
100136B2  mov  eax, [0x100C9F48]        ; graphics MMIO base
100136B7  mov  eax, [eax+0x204]         ; Address Flip Status
100136BD  test al, 0x02                 ; bit 1
100136BF  jne  0x100136B2              ; no visible counter/yield/timeout
```

**Manual identification, independently checked in the supplied PDFs:** the **`207–204h: Address Flip Status`** register defines bit **1** as *Graphics Starting Address Update Status* on all three chips. Bit 1 is **1** when a starting-address update has **not yet completed**, and **0** after the update. The manuals say the bit is set after writes to `Rx214`. Relevant file locations: `vx800_2.pdf` **PDF page 15**, `vx855_2.pdf` **PDF page 15**, and `vx900_2.pdf` **PDF page 17** (the printed page numbers differ). The VX855 PDF page was also visually inspected. It is **not** a generic GPU-engine-idle bit.

**Consequences:** a stuck or uncompleted primary graphics starting-address update can hold `BltPrepare` before either acceleration or software fallback is selected. The loop itself never times out or yields. A test system without a progressing display scanout or with abnormal flip sequencing could trigger this wait; **whether any valid Windows CE or Windows 98 workload actually does so remains untested**. This observation must not be conflated with the separate 3D ring-fetcher hangs in the user's Windows 98 driver.

<a id="c9-section-183"></a>

#### 183. This display-address wait runs **before** the accelerated-versus-emulated decision

`BltPrepare` first sets `GPEBltParms`' selected function to `HandleEmulateBlt` at `0x100136B0`, **then** spins on `MMIO+0x204` bit 1. Only after that loop does it examine destination format and operation flags and optionally call `BltFunctionSelecter` at `0x1001370F`. An emulation-selected operation can therefore still be delayed by a pending display-address update; switching only the final drawing callback to software would not bypass this preflight wait. This is a confirmed *ordering fact*, not proof that the wait is normally long.

<a id="c9-section-184"></a>

#### 184. Upstream geometry validation remains only **partly resolved**; the selector has a separate unchecked area heuristic

The compiled selector at `0x1001331B–0x1001369F` checks source/destination pixel formats, ROP and some operation-specific conditions. It does **not** establish a universal positive-height and positive-width contract before all acceleration callbacks are chosen. The patterned AGP wrappers independently reject `width <= 1` and fall back, but have no local analogous height guard before their request calculations (§177). **Important limit:** external Windows CE GPE/framework code or a preceding application/API layer may enforce rectangle validity; those external implementations have not been proven here. Therefore malformed rectangles are **not shown to reach the hardware**.

A newly identified signed-area heuristic exists in the *small indexed-image copy selection*, not in the DMA allocator:

```asm
100133BD  sub  edx, [ecx+4]      ; bottom - top
100133C0  sub  esi, [ecx]        ; right - left
100133C2  imul edx, esi         ; low 32 bits of product
100133C5  cmp  edx, 0x1000      ; threshold 4,096 pixels
100133CB  jge  0x10013699      ; signed compare: skip fast-copy override
100133D1  mov  [eax],0x10012A10 ; select v8BppXferFastCopy
```

The entire branch is conditional on preceding format/ROP and flag tests (`0x10013389–0x100133B4`). For **illustration only**, `65536 × 65536 = 2^32` has a truncated `imul` result of **zero**, which meets the "smaller than 4096" selection check despite the true area being huge. A negative truncated result also passes the signed `<4096` test. This **proves a truncating selection heuristic**, *not* an attacker-reachable API issue, command-buffer overrun, or a demonstrated rendering defect. For any reused implementation, compute area in a checked width or replace the multiplication with a division-based bound after validating positive dimensions.

<a id="c9-section-185"></a>

#### 185. `GetRingAGPSpace_inv` can poll a nonadvancing queue read pointer indefinitely

`GetRingAGPSpace_inv` (`0x1004EDFE–0x1004EF35`) computes a request/end margin at `0x1004EE23`:

```asm
1004EE23  lea  edx, [eax+ecx+0x600]  ; 32-bit end guard
1004EE2A  cmp  edx, edi              ; unsigned comparison
```

The code takes different paths depending on cursor position, ring end, and the read pointer obtained by dereferencing the memory pointer at manager offset `+0x1C`. Five directly verified polling regions include `0x1004EE45–0x1004EE51`, `0x1004EE69–0x1004EE75`, `0x1004EE87–0x1004EE8C`, `0x1004EEA0–0x1004EEAC` and `0x1004EECA–0x1004EED1`. Each re-reads the pointer and branches on a comparison; **there is no sleep, yield, timestamp or retry limit inside these loops**. The containing routine can call `KickoffRingAGP_inv` or `RewindRingAGP_inv` on other paths, but those calls are not escape mechanisms for a loop that never sees its desired pointer change.

If the pointer remains fixed while a particular loop's exit comparison remains false, that loop cannot terminate. The function returns `void` and, on reaching its tail, copies the requested amount from manager offset `+0x24` into its accounting field `+0x28` (`0x1004EF2B–0x1004EF2F`); it has **no local timeout/error-return path**. This is a source-proven conditional liveness limitation, not an observed or proven Windows 98 hang. The earlier arithmetic wrap concern about the 32-bit `+0x600` guard remains (§179).

<a id="c9-section-186"></a>

#### 186. Additional unconditional-in-loop waits are **different from** the scanout flip wait

There are at least two further *conditional* busy-wait call sites in the same CE blit stack:

- `VIA::WrappedBlt`, `0x10013217–0x1001322D`, polls **MMIO `+0x400`** until an object/global-computed bitmask is no longer set. It is guarded by earlier cursor/software-state conditions, not taken by every blit.
- `VIA::BltComplete`, `0x1001286E–0x10012884`, polls **MMIO `+0x400`** against an OR-adjusted mask (including bits `0` and `1`), under a separate completion-state condition.

Both inner loops show **no visible time bound or yield**. Their actual hardware-bit names and applicability should be established independently before assigning a precise engine-status meaning. Unlike those two waits, `BltPrepare` specifically tests **MMIO `+0x204` bit 1**, documented as **graphics starting-address update pending**. A useful diagnostic should log these two offsets **separately** and identify the caller/site that is waiting.

<a id="c9-section-187"></a>

#### 187. VX900-specific ring synchronization does not supply a generic progress guarantee

At `0x1004EED3–0x1004EF28`, the INV ring-space logic has an extra conditional branch for PCI ID `7122h` (VX900). It reads **MMIO `+0x47C`**, performs two range comparisons, and in one case writes `0x00100000` to MMIO `+0x41C` and `0x66FFFFF0`, then `0x670000FF`, to MMIO `+0x420`. This is a **specific hardware synchronization/programming branch**, not a ring-stall timeout. The VX800/VX855 paths do not take it. Avoid importing the VX900-only branch into the VX855 Windows 98 driver without its hardware meaning and preconditions.

The shared `cmRequestBuffer_inv` (`0x1004F01C–0x1004F040`) first pads its CPU command cursor to a 16-byte boundary by writing up to **three zero DWORDs**, **then** calls its space-check callback at `0x1004F03B`. As in §§145–155, this ordering alone does not show an out-of-bounds write: actual queue allocation, reserve margins and cursor invariants require a full physical-state proof.

<a id="c9-section-188"></a>

#### 188. Windows 98 VX855 blit and ring diagnostics

| Issue | What the source now establishes | What remains unknown |
|---|---|---|
| `BltPrepare` stuck | **Newly identified** `MMIO+0x204` bit1 is pending graphics starting-address update, identical in all three manuals | Whether the Windows 98 driver uses an equivalent wait or encounters an uncompleted scanout flip |
| Software fallback hangs | Even an emulation-selected CE blit goes through the display-address preflight wait | Whether this contributes to any observed Windows 98 software-path stalls |
| Patterned AGP capacity | §§172–180 bound the **normal logical** transaction | Invalid-input reachability and physical ring state under wrap or failed request |
| Fast 8-bpp copy dispatch | **New:** signed truncated 32-bit area heuristic can misclassify artificial oversized input | Whether normal CE/GPE surfaces can ever reach those inputs |
| INV queue stuck | **Newly enumerated five pointer-poll loops** with no local timeout/yield | Which specific loop runs on an actual hang, and whether CE/Win98 share the mechanism |
| VX900 extra wait/control | Conditional MMIO `0x47C`, `0x41C`, `0x420` sequence | Actual silicon semantics and VX855 relevance |
| Completion waits | `WrappedBlt` and `BltComplete` poll MMIO `+0x400` separately from `BltPrepare` | Exact status-bit semantics and hardware timing |

**Recommended instrumentation before any command-manager changes:** in a *separate test build* of the Windows 98 driver, timestamp and distinguish (a) display-address update pending (`MMIO+0x204`, bit1), (b) graphics-engine status polling (`MMIO+0x400`, relevant mask), (c) command-buffer producer and consumer/read-pointer values, (d) requested versus emitted command sizes and last successful group ID, and (e) how many consecutive polls observe no progress. Use bounded diagnostic reporting and safe recovery only after the exact wait source is known; never blindly recycle a GPU queue with unconsumed commands or held locks. The CE DLL establishes **comparison targets**, not a mandate to transplant its polling or MMIO writes into VX855 Win98.

**Verification:** `Chrome9_CE_Upstream_FlipWait_and_Ring_Liveness_Audit_181_188.py` performs a read-only SHA-256 check, verifies instruction addresses/branches in the original DLL, independently extracts the three VIA PDF descriptions, checks the truncated area heuristic and the polling loops, and verifies the VX900-only path. All static assertions passed; no driver code, queue commands or graphics hardware were executed.

<a id="agp-ring"></a>

## AGP ring, GART and Win98 measurements

The CE implementation and VIA Linux source establish the readback and startup model. The Win98 v10.479 source analysis and the archived v10.441 hardware sessions cover different builds. Both archived sessions exercised the VX855 ring, and their startup readback was 16 bytes beyond the programmed pause.

### Fetch addresses, status and readback

<a id="c9-section-373"></a>

#### 373. VX900 XP command-buffer setup and `0x43C` polling

| XP operation | Static evidence in `s3ginvA.dll` |
|---|---|
| Start the command buffer | `0xBF657–0xBF83B` assembles Type-10 words `60h–67h`. A PCI ID check at `0xBF6F6` includes `7122h` in the branch setting `60h[1:0]=01b` (SF in VIA's table). The code writes `0x00100000` at MMIO `0x41C`, sends the eight words to `0x420`, then writes `0x680A0001` to `0x420`. |
| Bind and poll the hardware position | `0xBF9EC–0xBF9FB` stores `MMIO base + 0x43C` as the readback address in the ring state at `+0x4028`. `0xC13AD–0xC13C4` selects `0xC0150` for PCI IDs `7122h` and `A122h`. Its near-end branch converts the submission size to bytes, applies a `0x80`-byte threshold relative to the buffer end, and reads `0x43C` against the translated write position. `0xC0221–0xC026B` polls twice in succession. The routine has additional boundary checks. |
| Stop and trigger | A separate path at `0xC09FC–0xC0A02` assembles `65h[9:8]=10b`. It polls MMIO `0x400[4]` at `0xC0A0B–0xC0A28` before sending the Type-10 words and triggering with `0x680A0001` at `0xC0AFA`. VIA defines `65h[9:8]=10b` as a stop point that requires a trigger to resume. |

This confirms software use of `0x43C` in the XP VX900 ring manager. The static path supplies a reference for comparing address translation, alignment, pause/trigger order and pointer polling with Win98. Which transport a particular workload uses, what the readback returns on hardware and whether the VX855 hang has the same cause require runtime evidence. The INF's `UseAGPDMA=1` is an installation default. V9-II, pp. 82–85. [XP binary identities](#c9-section-375).

<a id="c9-section-380"></a>

#### 380. VX800 XP ring startup, wrap and pause publication

The normal class-4 ring table at `0x1F90A0` points slot `+0x24` to `0xB9B00`. The initializer at `0xBB010` substitutes `0xB9E10` for PCI `7122h`, leaving `1122h` and `5122h` on the table's default. The separate family checks in ring setup and submission are relevant when comparing VX800 with VX855.

| Step | VX800 XP evidence and family difference |
|---|---|
| Start the ring | `0xB9474` routes `1122h` to the same location-selector write as `5122h` and `7122h`. The code sets Type-10 `60h[1:0]=01b`, selects Type-10 through MMIO `0x41C=0x00100000`, sends `60h–67h` through `0x420`, then writes `0x680A0001` at `0xB95E9`. |
| Boundary alignment | In this same binary, setup uses ring-state masks `+0x4060` and `+0x4064` of `0x7F` for VX800 at `0xB90D5–0xB90E5`, versus `0xFF` for VX855 at `0xB90B9–0xB90C9`. These are software masks in the inspected path. |
| Read position and wrap | `0xB97A2–0xB97AB` binds the readback pointer at ring state `+0x4028` to MMIO `0x43C`. The near-end branch of `0xB9B00` tests the buffer end minus `0x80` and runs two pointer polling loops at `0xB9BCA–0xB9C14`. Wrap helper `0xB98A0` emits `FE020420 0000041C 00300000 CC000000` before publishing the boundary. |
| Publish pending work | Ordinary publication at `0xBA35E–0xBA3DF` translates the write pointer, subtracts `0x10`, and writes Type-10 `64h/65h` with pause ID 0. Another path sets pause ID 2, waits on `0x400[4]`, and triggers again at `0xBA71A`. |
| Conditional wait packet | If the submission tag is `0x41`, the `1122h` and `5122h` checks converge at `0xBB30D`. The code aligns the cursor and emits `FE000000 00000001 00000000 CC000000 0000006C 003C0000` at `0xBB331–0xBB39B`. |

The XP code establishes `0x43C` as a readback address used by the VX800 ring manager. Board readback, the transport chosen for a workload, and the reason for the VX855 Win98 hang still require runtime evidence. The INF's `UseAGPDMA=1` is an installation default. V8-II, p. 149 · [VX900 XP ring](#c9-section-373).

<a id="c9-section-194"></a>

#### 194. CE ring readback through MMIO 0x43C

**Erratum (2026-11-06):** The earlier version of this section incorrectly treated `DMA_MANAGER+0x1C` as `CMD_INTERFACE+0x1C`. `DMA_MANAGER` is embedded at `CMD_INTERFACE+0x18`. Thus `DMA_MANAGER+0x1C = CMD_INTERFACE+0x34`, and `cmCreate_inv:0x1004EFA2–0x1004EFA7` explicitly stores **MMIO base+`0x43C`** into that location. The corresponding `DMA_MANAGER+0x20` pointer resolves to **MMIO base+`0x444`**.

```asm
1004EE45  mov ecx, [esi+0x1C]      ; DMA_MANAGER+1C -> interface+34 -> MMIO+43C
1004EE48  cmp dword ptr [ecx], eax ; reads MMIO register, compares with ring offset
1004EE4A  ja  0x1004EE45           ; repeats while the comparison fails
```

The disassembly therefore **does** show a read of MMIO `0x43C` in the CE queue-space wait. OpenChrome independently defines the same register address as a *write-side* parameter selector. What this register returns on reads in a particular enabled ring mode, and whether it equals the real fetch head, still require hardware validation. The old claim about a separate ordinary-RAM progress pointer is withdrawn. Full callback/offset and device-address evidence is in §§197–204.

<a id="c9-section-195"></a>

#### 195. The CE ring setup exposes a concrete six-command AGP fetcher recipe

The retail map names `SetAGPRingCmdRegs_inv` at `0x1004E921`. It constructs and writes **six** Command Regulator type-`10h` subaddress words in order through MMIO `+0x41C` (selector) and `+0x420` (payload):

| Type 10h subaddress | Built command prefix | Manual description (VX855/VX900 Part II) |
|---|---|---|
| `60h` | `0x60000000` | AGP command buffer start, low address and location |
| `61h` | `0x61000000` | AGP command buffer start, high byte |
| `62h` | `0x62000000` | AGP command buffer end, low address |
| `63h` | `0x63000000` | AGP command buffer end, high byte |
| `64h` | `0x64000000` | AGP command buffer pause address, low address |
| `65h` | `0x65000000` | AGP command buffer pause address, high byte **and pause-address ID** |

`0x1004EA31–0x1004EA49` explicitly tests PCI IDs `1122h`, `5122h`, `7122h` and ORs bit 0 into the first word for those three families (the manual identifies location `01` as **System Dynamic Frame Buffer**). The code applies address masking and upper-byte splitting rather than writing raw CPU virtual pointers. The supplied VX800 Part II PDF has a summarized `60h–65h` command-table entry; detailed standalone bitfield definitions for these six words are present in the supplied VX855/VX900 Part II PDFs. Treat these distinctions as evidence limits rather than assuming identical documentation depth.

These `10h:60h–65h` **command-regulator** words are wholly distinct from the VX900 undocumented `04h:60h–62h` **vertex-related** words studied in §§102–111. Addressing, packet type and interpretation must travel together.

<a id="c9-section-196"></a>

#### 196. Ring rewind, kickoff padding and practical diagnostic implications

`RewindRingAGP_inv` (`0x1004EAEC`) writes an in-ring **`0xFE020420`** packet containing `0x41C` and `0x00300000`, plus padding; after a chip-specific end alignment it updates dynamic **AGP jump address** `10h:66h/67h` and **pause address** `10h:64h/65h` via the command regulator. The **VX900** branch aligns to **64 bytes** (`test al,0x3F`), whereas **VX800/VX855** align to **256 bytes** (`test al,al`). These are verified software writes and agree with the command-regulator meaning of `66h/67h` in the VX855 manual; the effect of an incorrect pause/jump target was not tested.

`KickoffRingAGP_inv` (`0x1004EC27`) similarly uses a VX900-specific **64-byte** alignment and can append an `FE02` marker/padding before publishing a new pause address (`10h:64h/65h`). On one **VX800/VX855-specific** condition—`manager+0x40 == 1` and byte `manager+0x32 > 3`—it executes `rep stosd` with `ECX=0xBFC`, writing **`0xBFC * 4 = 0x2FF0` bytes** of `0xCCCCCCC0`. This is an actual conditional bulk-padding path; it is **not** proof that this condition occurs often, that it is a performance bottleneck, or that it caused earlier VX855 stalls.

**Updated diagnostic plan for a recoverable Win98 VX855 test build:**

1. Record which *specific* wait site is active: `0x200[1]` primary VBlank; `0x204[2]` secondary VBlank; `0x204[1]` primary address-update pending; `CR61[5]` secondary flip flag; `0x400` engine status; or CE command-ring MMIO progress polling at `0x43C` (read semantics require validation).
2. For any ring test, log **software producer cursor, producer address translated to the fetcher address space, **MMIO+0x43C readback as a candidate progress signal**, configured 10h:60h–65h and 10h:66h/67h addresses, and last completed draw group**. Do not assume the readback is a verified consumer pointer without testing the enabled hardware mode.
3. Capture the selected ring padding/rewind branch and whether pause/jump targets are 16-byte aligned and inside the actual allocation. Verify device-specific 64/256-byte alignment expectations before changing them.
4. Use an instrumented **read-only or logging-only first test**, with bounded waits and a recoverable system. A proposed Win98 behavior change should be tested separately; neither CE static analysis nor manual descriptions prove the Windows 98 ring implementation is wired the same way.

**Verification:** `Chrome9_CE_Scanout_and_INV_Ring_Audit_189_196.py` pins the original retail DLL by SHA-256, checks six linker-map symbols and exact instructions in both flip routines, ring creation, queue progress, setup, rewind and kickoff; also checks all six original VIA Part I/II manuals' relevant fields. All checks passed. No CE/Win98 binary or GPU command was executed. **Outstanding:** dynamic producer/consumer state, any external timeout at the calling API boundary, per-mode flip timing, queue-coherency guarantees and actual hardware effects.

<a id="c9-section-197"></a>

#### 197. Embedded DMA_MANAGER and CMD_INTERFACE pointer layout

**Evidence level: verified retail x86 CE code and linker map; no silicon execution.** The previous §194 conclusion that `GetRingAGPSpace_inv` polls a *separate system-memory progress pointer*, rather than the `0x43C` register, was **wrong**. The earlier comparison confused offsets relative to two different object bases. This continuation explicitly corrects that result instead of treating it as another competing interpretation.

`cmCreate` (`0x1004E1F0`) installs `cmCreate_inv` at interface offset `+0x7C` for PCI IDs `1122h`, `5122h` and `7122h` (`0x1004E2DC–0x1004E2F4`), and invokes it with the `CMD_INTERFACE*` at `0x1004E308–0x1004E309`. In `cmCreate_inv` (`0x1004EF36`), the pointer is advanced **`+0x18`** at `0x1004EFF0` before calling `InitAGPPausetoStop_inv` and `SetAGPRingCmdRegs_inv`, both of which accept `DMA_MANAGER*`.

Therefore the embedded-object relationship is:

```text
CMD_INTERFACE* C
DMA_MANAGER*   D = (byte*)C + 0x18
D + offset N     == C + (N + 0x18)
```

The earlier audit correctly identified the instruction `mov ecx,[esi+0x1C]` in `GetRingAGPSpace_inv`, but incorrectly assumed this was `CMD_INTERFACE+0x1C`. It is **`DMA_MANAGER+0x1C` = `CMD_INTERFACE+0x34`**. Likewise, `CMD_INTERFACE+0x1C` set from `CMD_INTERFACE+0x0C` during initialization is a *different* field and should not have been used to identify the queue-progress pointer.

<a id="c9-section-198"></a>

#### 198. Reconstructed queue-manager pointer layout and MMIO aliases

The correct field translation can be checked directly in the retail DLL:

| Field from `DMA_MANAGER* D` | Same location from `CMD_INTERFACE* C` | Evidence / role in examined code |
|---|---|---|
| `D+0x08` | `C+0x20` | Ring address base used in CPU-to-device offset calculations |
| `D+0x0C` | `C+0x24` | CPU-visible ring-buffer base (`cmCreate_inv: 0x1004EF94`) |
| `D+0x10` | `C+0x28` | CPU-visible ring-buffer end (`0x1004EF9A`) |
| `D+0x14` | `C+0x2C` | Producer/cursor address read by `GetRingAGPSpace_inv` |
| `D+0x18` | `C+0x30` | Ring insertion pointer initialized by `0x1004EF97` |
| **`D+0x1C`** | **`C+0x34`** | **Pointer assigned MMIO base + `0x43C` (`0x1004EFA2–0x1004EFA7`)** |
| **`D+0x20`** | **`C+0x38`** | **Pointer assigned MMIO base + `0x444` (`0x1004EFB3–0x1004EFB8`)** |
| `D+0x24` | `C+0x3C` | Queue-space request size used by the space calculator |
| `D+0x4C` | `C+0x64` | Callback set to `GetRingAGPSpace_inv` (`0x1004EFDB`), called from `cmRequestBuffer_inv:0x1004F03B` |

At `0x1004EFF0` the initialization actually passes `C+0x18` to the ring setup functions; at `0x1004F03B` the request allocator calls `[D+0x4C]`. These independent uses prove the offset translation rather than relying on one instruction alone. **The `0x43C` pointer is MMIO-mapped, not an allocation in ordinary system RAM.** The memory dereference in the disassembly is the CPU's way of issuing an MMIO read.

<a id="c9-section-199"></a>

#### 199. The queue-space loops really read MMIO `0x43C` in this CE backend

Representative `GetRingAGPSpace_inv` (`0x1004EDFE–0x1004EF35`) instructions:

```asm
1004EE45  mov ecx,[esi+0x1C]      ; D+1C -> *(C+34) -> MMIO+43C address
1004EE48  cmp dword ptr [ecx],eax ; CPU reads MMIO+43C
1004EE4A  ja  1004EE45            ; potentially unbounded retry
1004EE69  mov ecx,[esi+0x1C]      ; the same MMIO address on another wait branch
1004EE6C  cmp dword ptr [ecx],eax
```

Other reads from the same translated pointer occur at `0x1004EE4C`, `0x1004EE70`, `0x1004EE87`, `0x1004EEA0`, `0x1004EEA7`, `0x1004EEBB`, and `0x1004EECA`. The code compares the returned 32-bit value with ring addresses, so **the CE ring-space algorithm treats the `0x43C` readback as a ring-progress-related quantity**. Whether the value is literally a hardware fetch head, a threshold/status alias, or requires a particular fetcher mode remains a **hardware-semantics question**, not something established by the compiler output.

This must be distinguished from the *write-side* programming interface: OpenChrome's `via_regs.h` defines `VIA_REG_TRANSET = 0x43C` and `VIA_REG_TRANSPACE = 0x440`; its common 3D emitter writes parameter selectors/values there. The CE DLL also writes parameter selectors to those locations. **Read and write uses of one MMIO offset cannot automatically be assigned the same semantics.** An idle or uninitialized ring returning zero does not refute the CE code's read-side usage. Conversely, the existence of this CE polling code does not prove a valid read-pointer signal in the user's different Windows 98 driver configuration.

<a id="c9-section-200"></a>

#### 200. Queue-space comparisons use translated device-address offsets, not raw CPU virtual pointers

The start of `GetRingAGPSpace_inv` makes the address-space conversion explicit:

```text
P0 = *(D+0x08)                  ; ring start in command-fetcher/device address space
V0 = *(D+0x0C)                  ; ring CPU mapping start
V  = *(D+0x14)                  ; current CPU producer pointer
VE = *(D+0x10)                  ; CPU mapped ring end

producer_device = P0 + (V - V0)
end_device      = P0 + (VE - V0)
required_limit  = producer_device + request_bytes + 0x600
```

`0x1004EE06–0x1004EE2F` implements the first comparison with `lea edx,[eax+ecx+0x600]` followed by a `cmp` against the translated ring end. Later paths use an additional `0x400` margin. The register readback is compared against the **translated device-address quantities**, not arbitrary CPU virtual addresses. These formulas are verified x86 arithmetic, with **32-bit wrap/ordering caveats** already covered in §179; they do not prove how the hardware represents the corresponding read value.

This is directly actionable for a diagnostic tool: record *both* CPU producer VA and the converted device-address offset, then compare the latter with MMIO readback. Comparing the register directly with a CPU VA would be invalid.

<a id="c9-section-201"></a>

#### 201. MMIO `0x444` and VX900 `0x47C` are separate ring-control observations

**Updated using independent 2008 VIA Linux source (§§205–213):** `InitAGPPausetoStop_inv` first excludes **VX900 (PCI `7122h`)** at `0x1004E820–0x1004E830`; only the non-VX900 branch loads and dereferences `D+0x20` at `0x1004E82D–0x1004E836`, then tests its sign bit at `0x1004E838–0x1004E83A`. `D+0x20` points to **MMIO `0x444`**, assigned at `cmCreate_inv:0x1004EFB3–0x1004EFB8`. The historical VIA Linux header independently names this register `INV_RB_AGPCMD_STATUS` and its bit 31 `INV_AGPCMD_InPause` (**fetcher paused**). Therefore this is a **paused-ring recovery check for VX800/VX855**, **not** a universal VX900 startup check nor an unidentified status bit. This corrects the pre-§205 interpretation without claiming runtime hardware validation.

`GetRingAGPSpace_inv` also has a **VX900-specific read of MMIO `0x47C`** (`0x1004EEDF–0x1004EEFC`), with comparisons performed before it optionally submits additional `10h:66h/67h` commands. It should not be mixed up with `0x43C`, `0x444`, or `0x41C/0x420` (the command-regulator write interface). These code paths and directions are independently verified; the precise device meaning of `0x47C` remains unresolved.

<a id="c9-section-202"></a>

#### 202. Creation success and mode selection are prerequisites to interpreting `0x43C` reads

`cmCreate` chooses the INV backend by PCI device ID and invokes its initialization. The INV initializer calls `InitPCIe` for `1122h`, `5122h` and `7122h`; it has a failure/early-return path (`0x1004EF72–0x1004EF81`) before completing the field bindings. Only the successfully initialized backend installs the space/rewind/kickoff callbacks and sets up the AGP ring start/end/pause commands. Other modes may disable or bypass buffered command fetching entirely.

Thus the prior hardware experiment where `0x43C` appeared unchanged should be interpreted with **mode and initialization evidence**: confirm that the tested driver actually configured and selected the same fetcher pathway, then capture the value before and after known-completed queue batches. `0x43C` readback alone cannot establish either a functional ring or a failed ring without those prerequisites. No live-device testing was performed in this investigation.

<a id="c9-section-203"></a>

#### 203. Readback interpretation and verification tooling

The corrected status is:

| Prior claim | Revised evidence-based status |
|---|---|
| §194: `GetRingAGPSpace_inv` polls a separate CPU-memory progress pointer | **Incorrect.** It polls a **MMIO-mapped pointer to `0x43C`** once the embedded `DMA_MANAGER+0x18` offset is accounted for. |
| §§185/194/196: ring progress observation excludes `0x43C` | **Incorrect as a CE code description.** The examined CE queue-space loops read that register directly. |
| OpenChrome calls `0x43C` a parameter selector | **Still correct for OpenChrome's write-side interface.** This doesn't define its readback semantics. |
| Any `0x43C` value is a confirmed hardware consumer pointer on Win98 | **Not established.** The CE driver *uses* that readback as a progress quantity in the chosen backend, but independent mode/hardware validation is missing. |
| CE ring waits may block indefinitely | **Still supported.** The loops contain no local yield/timeout; the newly corrected data source is MMIO, not RAM. |

`Chrome9_CE_Scanout_and_INV_Ring_Audit_189_196.py` had checked the correct instructions but printed an incorrect interpretation because it did not translate the embedded structure base. It is superseded on this point by `Chrome9_CE_INV_Ring_Pointer_Alias_Audit_197_204.py`, which verifies the pointer offset, callback offset, translated producer math and both MMIO mappings against the original pinned retail DLL and the supplied OpenChrome snapshot.

<a id="c9-section-204"></a>

#### 204. VX855 ring diagnostics and unresolved cases

For a recoverable, **logging-first** Windows 98 VX855 experiment, record: (1) whether command fetching is actually enabled; (2) the programmed `10h:60h–65h` start/end/pause values; (3) software producer CPU pointer and **device-offset-translated** producer value; (4) MMIO `0x43C` readback *as a candidate signal*; (5) the associated MMIO `0x444` status value; (6) engine status `0x400`; and (7) the last known completed render group. If the queue stalls, preserve the order and context of these observations rather than equating `0x43C==0` with failure to consume commands.

**Updated by §§205–213:** the independent VIA Linux source now identifies `0x43C` as the **AGP command current-address readback** and explicitly warns that its values can be unreliable; `0x444[31]` identifies **InPause**. The highest-value *remaining* unknowns are the actual read behavior of these registers **under the user's Windows 98 VX855 enable recipe**, readback reliability/coherency in that mode, the CE/VxD initialization differences, and how jump/pause updates interact with a stuck fetch head. A safe offline next step is to compare other VIA chipset driver source/binaries for explicit read-side names and masks on `0x43C`, `0x444` and `0x47C`. No register values or driver binary have been changed during this work.

**Read-only verification:** `Chrome9_CE_INV_Ring_Pointer_Alias_Audit_197_204.py` — original retail SHA-256, six linker symbols, `CMD_INTERFACE`→`DMA_MANAGER` base shift, MMIO pointer stores, callback binding, ring address conversion, repeated register polls, and OpenChrome's write-side `TRANSET` alias. **All checks passed.**

<a id="c9-section-205"></a>

#### 205. VIA Linux definitions for CE ring readback

**Evidence:** archived Linux kernel mailing-list patch, *Re: via agp patches*, dated **30 May 2008**, adding `drivers/char/drm/via_chrome9_3d_reg.h` and `via_chrome9_dma.c` to the VIA Chrome9 DRM driver. Public source: <https://lkml.iu.edu/0805.3/2636.html>, especially lines 168–200, 667–735 and 1338–1488. This historical patch's PCI ID list includes `1106:1122` (VX800) but **does not list VX855 `5122` or VX900 `7122`**. Thus it is independent family-code corroboration, **not** a hardware test of the later chips. The supplied VIA Windows CE x86 retail DLL separately selects `1122`, `5122`, and `7122` in its INV command-manager initialization (§§197–204).

This external source confirms, in VIA's own names, the interpretation of the two embedded `CMD_INTERFACE` MMIO pointers which §§197–204 recovered from the CE DLL. The earlier pre-§197 report statements that `0x43C` is *only* a write-side parameter selector and has no documented read-pointer role are **superseded as general descriptions** by this original VIA source. Their caution regarding **the user's different Windows 98 configuration** remains valid.

<a id="c9-section-206"></a>

#### 206. Explicit read-side MMIO register map and write-side alias

| MMIO offset | 2008 VIA macro or CE pointer | Supported interpretation | Provenance / caution |
|---|---|---|---|
| `0x400` | `INV_RB_ENG_STATUS` | General engine status | VIA Linux macro; CE also polls this offset |
| **`0x43C`** | **`INV_RB_AGPCMD_CURRADDR`** | **Readback: current AGP command-buffer address** | VIA Linux macro; CE `C+0x34` / `D+0x1C` polls it (§§197–200) |
| **`0x43C`** | **`INV_REG_3D_TRANS`** | **Write-side parameter-type selector** | SAME offset, separate named direction/role in Linux header and OpenChrome `VIA_REG_TRANSET`; do not infer all hardware modes are readable |
| `0x440` | `INV_REG_3D_BEGIN` | Write-side 3D parameter data | VIA Linux macro; OpenChrome `VIA_REG_TRANSPACE` |
| **`0x444`** | **`INV_RB_AGPCMD_STATUS`** | **Readback: AGP command status** | CE `C+0x38` / `D+0x20` dereferenced in startup |
| `0x444` bit 31 | `INV_AGPCMD_InPause` (`0x80000000`) | **Command fetcher in paused state** | Explicit VIA Linux mask; CE uses sign-bit test on corresponding read |
| `0x448` bit 30 | `INV_RB_VQ_STATUS`, `INV_VQ_FULL` (`0x40000000`) | Separate VQ full indicator | **Not** the AGP fetch-head address |
| `0x45C` | `INV_RB_AGPCMD_PAUSEADDR` | AGP pause-address readback | VIA macro; do not assume the CE binary necessarily uses it |
| `0x460` | `INV_RB_AGPCMD_JUMPADDR` | AGP jump-address readback | VIA macro; not the same as the `10h:66h–67h` *write* command |
| `0x464` | `INV_RB_AGPCMD_STARTADDR` | AGP start-address readback | VIA macro; compare against actually configured ring start |

**Important distinction:** These identifiers establish the *software author's intended meaning* of the registers and independently corroborate CE code-level behavior. They do not prove the exact return values, update cadence or error handling of VX855 Win98 silicon under every fetcher mode. Do not confuse the MMIO `0x43C` **readback** with the `0x43C` **write** transport simply because they use the same offset.

<a id="c9-section-207"></a>

#### 207. VIA's source explicitly flags unreliable fetch-pointer reads and uses paired checks

The original `get_space_ring_inv` Linux routine reads `INV_RB_AGPCMD_CURRADDR` before making its safe-to-reuse comparisons (archive lines 1363–1488). In several branches it performs **two consecutive passes over the same threshold condition**. The original source comment notes that hardware readback may be unreliable and says to confirm twice. This is now an independently documented reason for the apparently redundant polling in the retail CE function `GetRingAGPSpace_inv` (`0x1004EDFE–0x1004EF35`):

| CE instruction region | Function of repeated condition | Similarity to 2008 VIA Linux logic |
|---|---|---|
| `0x1004EE45–0x1004EE51` | Twice wait until current address is no longer above the comparison value | Two-pass post-wrap check |
| `0x1004EE69–0x1004EE75` | Twice wait until readback is not below a boundary | Two-pass space-reuse guard |
| `0x1004EE87–0x1004EE8C` | Wait for a specific pointer position after rewind | Linux code also waits for `CurHW == CurSW` |
| `0x1004EEA0–0x1004EEAC` | Twice wait while readback has not crossed a boundary | Double-confirm alternative branch |
| `0x1004EEBB–0x1004EED1` | Additional read-pointer and boundary comparisons | Non-wrap condition checking |

**These are not two identical samples required to match**. The code waits for a threshold condition to be satisfied, then independently tests it again. No bounded retry counter, timer or yield is present in these specific tight loops. The two-pass check might filter transient readback anomalies; **it does not fix a permanently stale value or guarantee monotonic progress**. The old Windows 98 observation that `0x43C` stayed zero is therefore a reason to verify engine mode and register availability rather than proof that CE's ring algorithm is unrelated.

<a id="c9-section-208"></a>

#### 208. VX900 skips the paused-fetcher recovery path

The CE routine `InitAGPPausetoStop_inv` (`0x1004E81D–0x1004E920`) has a **top-level VX900 exclusion**:

```asm
1004E820  cmp dword ptr [0x100C9FC4],0x7122  ; VX900 device ID
1004E830  je  0x1004E909                   ; VX900 returns immediately
1004E82D  mov eax,[ecx+0x20]              ; D+20 points to MMIO+0x444
1004E836  mov eax,[eax]                   ; read AGP status on other chips
1004E838  test eax,eax                    ; test bit31 via sign
1004E83A  jns 0x1004E909                   ; no paused-ring recovery if clear
1004E840  mov eax,[ecx+0x1c]              ; D+1C points to MMIO+0x43C
1004E843  mov edx,[eax]                   ; fetch current AGP address
```

The addresses are checked against the original retail DLL's SHA-256. On **VX800/VX855** the function checks the pause flag; if bit 31 is set, it writes a termination/padding sequence at the current ring position, sends `10h:64h/65h` with **pause ID = stop** (`0x200` in the high command), and waits on engine status. **VX900 skips this entire startup recovery block**, not merely the pause check. The code does not prove that VX900 lacks pause hardware; it proves VIA intentionally uses a different initialization/recovery recipe here.

**Correction to §201:** the earlier statement that `InitAGPPausetoStop_inv` tests `0x444` should be read as applying to the non-VX900 branch only. The mapping of `0x444` and the bit-31 interpretation remain valid; VX900's startup does not take that read branch.

<a id="c9-section-209"></a>

#### 209. The pause-ID encoding and the meaning of `0x444` are now independently corroborated

The 2008 VIA Linux header defines `HAGPBpID` command values of **`0x00000000` (pause)**, **`0x00000100` (jump)** and **`0x00000200` (stop)**. These values occupy the control field in the `10h:65h` write word, and the public VX855 manual's `10h:65h` bit `[9:8]` descriptions match the three modes. The CE startup writes the stop form as `0x65000200 | high_address_byte` at `0x1004E8EB–0x1004E8F0` after preparing a terminating command segment.

The Linux source also labels `0x444[31]` **InPause**. In CE, `test eax,eax` followed by `jns` on the dereferenced `D+0x20` field is exactly a signed-bit-31 test. This closes a previously unresolved static identification: **the status bit checks for a paused AGP command fetcher**, not a generic render-engine idle flag. Whether the flag clears under a Windows 98 configuration is a separate hardware question.

<a id="c9-section-210"></a>

#### 210. Source and CE agree on ring address space translation, with a caveat for PCIe

Both drivers calculate a translated GPU/device queue address from a CPU-visible ring cursor:

```text
current_device_address = ring_device_base + (current_cpu_cursor - ring_cpu_base)
```

The 2008 Linux implementation sets the physical base differently according to `CHIP_PCIE` versus AGP and then compares **device-address-space quantities** with `INV_RB_AGPCMD_CURRADDR` (Linux lines 1338–1488). The CE binary has the equivalent translated comparisons in `GetRingAGPSpace_inv` (§200), but its allocation and remapping helpers must be traced separately before claiming all physical-base rules match. A Win98 diagnostic should not compare a CPU linear address directly with `0x43C`.

In particular, **a zero `0x43C` value is ambiguous**: absent or disabled fetcher, zero-based physical address, a stalled cursor, or unsupported readback under a different mode must be distinguished through initialization evidence and independent completion observations. These are *possible explanations*, not a proved explanation of the user's earlier test.

<a id="c9-section-211"></a>

#### 211. Why the source is stronger evidence than only more disassembly

The public VX-series programming manuals document command-regulator `10h:60h–68h` submission and pause IDs, but the examined passages do not fully expose these extra readback names/masks for the MMIO `0x43C/0x444/0x45C/0x460/0x464` addresses. OpenChrome's write-side `TRANSET` name alone was consequently insufficient. The **2008 original VIA DRM source** provides an independent, readable declaration of those *read-side* roles and comments on readback stability. The CE x86 binary then establishes that VIA's **later CE backend** really dereferences the corresponding pointers for **VX800/VX855/VX900**, although VX900 excludes one startup recovery path.

**Do not overgeneralize:** the archived Linux patch predates VX855/VX900 and does not demonstrate either later chipset working with these reads. The binary comparison establishes code similarity, not silicon validation. A read-only diagnostic can improve confidence without altering working ring parameters.

<a id="c9-section-212"></a>

#### 212. Refined logging plan for the VX855 Windows 98 ring experiments

For an **already enabled, working ring** and a recoverable test system, instrument a single baseline sequence without writing any new MMIO values merely for diagnosis. Capture:

1. PCI ID, driver ring-mode flag, the actual enabled state, and the exact `10h:60h–65h` start/end/pause words; distinguish intentionally paused/stopped state.
2. Ring CPU base, ring device base, producer CPU cursor and its translated **device-address** value; log queue allocation size and wrap count.
3. Read-only MMIO snapshots of `0x43C` (current command address candidate), `0x444` (including bit31 pause), `0x448` (VQ status), and, if mapped/safe, `0x45C`, `0x460`, `0x464` and `0x400` before/after *known completed* batches.
4. Two separate `0x43C` reads along with their ordering rather than a single value; inspect changes across frame boundaries and after one wrap. Do not assume adjacent reads are coherent or atomic as a pair.
5. Last submitted versus last **observably completed** render group, GPU busy flags, and whether the calling thread is inside an unbounded queue-space comparison.

Classify results as (a) fetcher never successfully enabled; (b) intentionally paused/stopped; (c) cursor advances; (d) cursor appears stale despite output completion; or (e) no output completion with engine busy/stuck. **Do not automatically retry pause/jump writes or force an engine reset** from a monitoring callback; such actions could conceal the failure or corrupt state. The read-only classification is preparatory, not a claim about the user's hardware.

<a id="c9-section-213"></a>

#### 213. Verification, historical corrections and next unresolved work

**New confirmed observations:** VIA's own header defines `0x43C` as current AGP command address on the read side and as a 3D parameter selector on the write side; `0x444[31]` is an in-pause flag; the source explicitly warns that current-address readback can be unreliable, which is consistent with the CE double-threshold checks; and CE excludes VX900 from its `InitAGPPausetoStop_inv` startup recovery. The CE offset-address correction in §§197–204 is independently corroborated. Nothing here changes the manual descriptions of Type-04 shaders or proves ring execution on VX855 Windows 98.

**Superseded general statements:** prior standalone statements in the preserved, historical OpenChrome-only portions of this master report calling `0x43C` *only* a write selector or saying no VIA source names a consumer pointer should be read as **limitations of the OpenChrome source alone**, not current conclusions. The correct current combined interpretation is the register table in §206. Older §§194/201 have explicit corrections in §§197 and 208.

**Evidence references:**

- Original 2008 VIA Chrome9 DRM patch: <https://lkml.iu.edu/0805.3/2636.html> (header definitions around lines 155–200; pause IDs around 220–228; ring initialization around 667–735; `get_space_ring_inv` around 1338–1488).
- User-provided CE retail `DDI_VIA.dll` SHA-256: `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`. Function addresses: `cmCreate_inv 0x1004EF36`, `InitAGPPausetoStop_inv 0x1004E81D`, `GetRingAGPSpace_inv 0x1004EDFE`.
- User-provided VX855 Part II manual: HParaType `10h`, subaddresses `60h–68h` for AGP buffer addressing and pause-ID commands. Original OpenChrome snapshot: `VIA_REG_TRANSET` (`0x43C`) write-side alias.
- `Chrome9_CE_AGP_Readback_Source_Parity_Audit_205_213.py` is a *read-only offline* DLL instruction audit. External archived source naming was independently reviewed and is cited above; this script does not download or execute that Linux code. No GPU hardware was used.

**Still open (updated in §§214–223):** the exact behavior of `0x43C/0x444` under the user's Windows 98 ring-enable recipe, VX900-specific `0x47C` semantics, whether dual-threshold checks are sufficient on every chipset, and whether stale reads follow from a disabled fetcher, a stuck fetcher, or a mode-dependent register mapping. **The subsequent §§214–223 trace CE `InitPCIe`, the mode-gated initialization and `10h:68h` trigger; the Windows 98 initialization still needs a direct source/log comparison.**

### CE setup, GART, cache visibility and pause publication

<a id="c9-section-214"></a>

#### 214. CE INV command-fetcher activation

**Evidence:** original user-provided VIA CE 6.0 retail `DDI_VIA.dll` (SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`), its original `DDI_VIA.map`, and original VX800/VX855/VX900 Part I and Part II manuals. All binary observations below were checked against disassembled instructions; this is **static analysis, not a Win98 execution trace**. The previous sections identified the readback roles of MMIO `0x43C` and `0x444`. This pass traces the **preconditions for those readbacks to reflect an enabled CE ring**.

**Main finding:** the CE ring does **not** become active merely because `0x41C` and `0x420` accept writes. A chipset-specific initialization path first selects a command-manager mode, allocates and maps the GART-backed command memory, flushes the translation cache, sets up the command-regulator state, installs ring callbacks, initializes start/end/pause/jump addresses and finally emits a command-fetch trigger. Observing unchanged `0x43C` in a *different* Win98 recipe is therefore insufficient to classify the readback as faulty or the fetcher as hung.

<a id="c9-section-215"></a>

#### 215. The `PCI_AGPMode` registry value gates ring initialization

`cmCreate` at retail `0x1004E1F0–0x1004E339` opens the actual embedded registry path **`HKLM\Drivers\Display\VIA`** (`RegOpenKeyExW` call site at `0x1004E22D`) and queries **`PCI_AGPMode`** (`RegQueryValueExW` call site at `0x1004E24C`). The path and name are UTF-16LE strings embedded at VAs `0x10001328` and `0x10006DCC`. Its resulting DWORD is stored as `CMD_INTERFACE+0x74` at `0x1004E282`. The interface selects INV function pointers for VX800 `1122h`, VX855 `5122h`, VX900 `7122h` (and older `3371h`) before invoking the selected initializer.

In **`cmCreate_inv`** at `0x1004EF36`, instruction `0x1004EF41` compares that field with **`1`**. If equal, the `0x1004EF44` branch returns at `0x1004F002`, **skipping `InitPCIe`, `hwmInitialize_inv`, paused-ring recovery and `SetAGPRingCmdRegs_inv`**. Thus **`PCI_AGPMode=1` is an explicit early-exit value in this CE code**. The binary alone does not show how any Windows 98 driver reads an analogous registry setting; do **not** copy this CE registry path or modify a Windows 98 registry as a presumed fix.

A subtle fallback distinction matters:

| Outcome in `cmCreate` | Resulting field for VX800/VX855/VX900 | Next INV initialization behavior |
|---|---|---|
| Registry key open **fails** | Initialized default **1** remains | Ring-specific INV setup skipped |
| Registry key opens; DWORD query succeeds | Queried value is used, **without local 0/1 normalization** | Value `1` skips; values other than `1` take the initialization branch |
| Registry key opens; DWORD query **fails** | Non-`3344h`/`3371h` device ID fallback **0** | Ring setup attempted |
| Same query failure for `3344h` or `3371h` | Fallback **1** | INV ring setup skipped for the relevant branch |

Evidence: default `1` at `0x1004E200–0x1004E209`; query result check `0x1004E255–0x1004E257`; older-device exception tests `0x1004E259–0x1004E26D`; mode store `0x1004E27F–0x1004E282`; early exit `0x1004EF41–0x1004EF44`. The **key-open-failure** and **value-query-failure** cases are different; no claim is made that either case actually occurred on the user's system.

<a id="c9-section-216"></a>

#### 216. VX800/VX855/VX900 use the same GART-backed initialization branch

For PCI IDs **`1122h`, `5122h` and `7122h`**, `cmCreate_inv` calls **`InitPCIe` at `0x1005065C`**, from `0x1004EF72–0x1004EF7B`. If it returns zero, the initializer exits before binding readback pointers or start/end/pause commands. Otherwise it populates ring fields and uses the common INV command path. The `3371h` branch instead invokes `InitAGP` at `0x1005033E`; this distinction prevents mistaking the *function name* `InitPCIe` for a general hardware command-fetch enable opcode.

`InitPCIe` computes **`align_up(0x00800000, page_size)`**, storing the result at `GART_INFO+0x0C` (`0x1005065F–0x10050679`), then computes **`align_up(command_bytes / 1024, page_size)`** for the GART translation table (`0x1005067C–0x1005068F`). At a conventional 4 KiB system page size this means **8 MiB of command backing** and **8 KiB of GART table** (2,048 four-byte translation entries). It invokes `AllocGART` and `AllocSF` at `0x10050691`/`0x10050697`, then initializes memory translation registers and fills the table (`0x10050710–0x1005075D`). These are **capacity/allocation expressions**, not measured memory usage and not proof that an unrelated Win98 VxD uses the same allocation size.

**Comparison relevance:** previous Win98 experiments that wrote fetcher commands and examined `0x43C` need an independently established device-visible backing address and valid translation before being compared with CE's running INV implementation. A CPU virtual address is not interchangeable with the device address.

<a id="c9-section-217"></a>

#### 217. GTI translation register programming and the cache-flush wait

`InitPCIe` uses **sequencer**, not CRTC, indexed registers: the original CE `ReadSR`/`WriteSR` helpers use ports `0x3C4/0x3C5` (`0x1000DAA2` and `0x1000DAC1`). The verified source sequence is:

| Sequencer register | CE instruction region | Observed action / manual interpretation |
|---|---|---|
| **SR6C** | `0x100506A8–0x100506BA`, `0x1005077A–0x1005078B` | Clear bit 7 while preparing table, then set bit 7. VX855 manual: **GART Table Write Protect Enable**. |
| **SR6A** | `0x100506BF–0x100506C8` | Program GART/RTSF base address bits `[19:12]` from table address. |
| **SR6B** | `0x100506CD–0x100506D6` | Program base address bits `[27:20]`. |
| **SR7B** | `0x100506F3–0x1005070F` | Preserve high nibble and replace low nibble with `ProtectSizeValue(table_bytes)`. **Manual descriptions conflict with software usage:** VX855 marks the register reserved/read-only; VX900 describes writable reserved low bits. Do not assign an undocumented functional meaning from this write alone. |
| **SR6F** | `0x10050761–0x10050775` | Read SR6F until **bit 7 clears**, then set bit 7 again. VX855 manual names it **GTI Cache Flush**, set by software and cleared by GTI. |

The loop at `0x10050761–0x1005076B` is **unbounded in this function**: `ReadSR(0x6F)`, test sign/bit 7, jump back while set. There is no local timeout or yield. This wait is a **translation-cache-flush completion wait**, separate from `0x444[31]` (command fetcher paused), `0x43C` progress, `0x400` engine status or display-address-flip waits. It is a specific additional place where startup could stall, without evidence that it does on valid hardware.

The manuals confirm **SR6F[7]** and **SR6C[7]** for VX800/VX855/VX900. Register `SR7B` is an example where binary behavior exceeds what the public manuals explicitly describe; retain the ambiguity.

<a id="c9-section-218"></a>

#### 218. Command regulator receives an initialization preamble before ring-address setup

`hwmInitialize_inv` (`0x100504D4–0x10050586`) has the same branch for VX800/VX855/VX900. The selected code writes the following **ordered** MMIO operations:

```text
MMIO[0x41C] <- 0x00110000    ; select HParaType 11h
MMIO[0x420] <- 0x06000000
MMIO[0x420] <- 0x07100000
MMIO[0x41C] <- 0x00100000    ; select HParaType 10h
MMIO[0x420] <- 0x02000000
MMIO[0x420] <- 0x69000000    ; subaddress 69h, AGP branch control = disabled
```

Exact instruction addresses: `0x100504FA–0x10050554`. Under the VX855 Part II HParaType `10h:69h` manual, bit 0 governs AGP branch permission: **0 = disable branched AGP headers (`FE8x`)**. Thus this CE startup explicitly disables that optional branch mode before setting up the conventional start/end/pause command-fetcher addresses. The separate Type-11h `06h/07h` words are recorded verbatim; this audit does not assert a complete functional decode of them.

This preamble is **not equivalent** to `SetAGPRingCmdRegs_inv`, and it precedes the latter in `cmCreate_inv` (`0x1004EFD1`, followed by `0x1004EFFA`). A minimal reproduction that omits this setup should not be assumed equivalent to VIA's CE code.

<a id="c9-section-219"></a>

#### 219. The CE ring initializer ends with an explicit fetch trigger

`SetAGPRingCmdRegs_inv` (`0x1004E921–0x1004EAEB`) first writes an in-ring `FE02` transaction, aligns the CPU write pointer and calculates translated device start/end/pause addresses (`0x1004E927–0x1004EA30`). For `1122h/5122h/7122h` it ORs location bit 0 into the Type-10h `60h` start-address command (`0x1004EA31–0x1004EA49`). It then sends through MMIO `0x41C/0x420`:

```text
Type10:60h  calculated ring start low + location bit
Type10:61h  calculated ring start high
Type10:62h  calculated ring end low
Type10:63h  calculated ring end high
Type10:64h  calculated initial pause address low
Type10:65h  calculated initial pause address high / pause ID
Type10:66h  0x66FFFFF0        ; initial jump address low
Type10:67h  0x670000FF        ; initial jump address high
Type10:68h  0x680A0001        ; read threshold 0x0A; AGP fetch trigger bit 0
```

The final immediate is at retail `0x1004EAD8`, and the VX855 Part II manual **explicitly** defines `10h:68h` bits `[21:16]` as the AGP read threshold and **bit 0** as *Trigger AGP Cycle*. `0x680A0001` selects a threshold of **10** and asserts the trigger. This is stronger than merely observing configuration words in a trace: **the CE driver actually issues the documented fetch trigger in the enabled path**. Its runtime success still is not established by disassembly alone.

Do not confuse this with the separate direct-command CR setup seen in OpenChrome or with the Win98 probe's previous `0x41C/0x420` writes. The exact enable procedure, mapping and hardware state must be compared, not just whether a register was written.

<a id="c9-section-220"></a>

#### 220. Startup order reconstructed from direct call sites

```text
cmCreate (0x1004E1F0)
  -> open HKLM\Drivers\Display\VIA, query PCI_AGPMode
  -> install INV callbacks for 1122h / 5122h / 7122h
  -> cmCreate_inv (0x1004EF36)
       -> if mode == 1: RETURN (no INV ring setup in this path)
       -> InitPCIe (0x1005065C)
            -> allocate command backing and GART table
            -> program SR6A/B/C and SR7B
            -> populate GART translation entries
            -> wait for SR6F[7] GTI cache-flush completion
       -> initialize CMD_INTERFACE translated ring fields
       -> associate MMIO read pointer 0x43C and status 0x444
       -> hwmInitialize_inv (0x100504D4)
       -> bind GetRingAGPSpace_inv, KickoffRingAGP_inv, RewindRingAGP_inv
       -> InitAGPPausetoStop_inv (VX900 skips recovery branch)
       -> SetAGPRingCmdRegs_inv (0x1004E921)
            -> program ring start/end/pause/jump
            -> emit 0x680A0001 AGP fetch trigger
```

Important caveat: the **registry mode is initialized before callback selection**, and the callback function pointer remains selected even if the initializer returns early. The observations above establish skipped *ring setup*; they do **not** establish that all non-ring/PCI graphics acceleration is disabled. Likewise, this does not prove that the user's Windows 98 driver contains the same registry mode logic.

<a id="c9-section-221"></a>

#### 221. Refined diagnosis for a VX855 `0x43C` value that never changes

The static evidence now identifies **distinct prerequisite classes** to check before blaming an unreliable progress register:

| Class | Read-only evidence to seek on the actual Win98 test build | What would remain unknown |
|---|---|---|
| Mode not enabled | Driver's own command-mode variable and initialization branch; any mode override | CE registry name need not exist in Win98 |
| Translation not initialized | Actual queue CPU/device bases, size, page-table address, chip-specific translation state | CE's SR setup may not be the correct Win98 recipe |
| Cache flush not complete | Whether a driver startup wait is blocked on GTI flush, if implemented | Whether HW/BIOS completes it in that mode |
| Fetch trigger absent | Logged Type-10h `60h–68h` **actual words**, including bit0 of `68h` | Whether the hardware accepted them |
| Fetcher deliberately paused/stopped | `0x444[31]`, recorded pause/jump words, stop ID | Whether readback behaves identically in the test mode |
| Readback stale or fetcher stuck | Two read samples of `0x43C`, independent completed draw and engine status | Whether the current fetch address can advance in this memory setup |

**Safety and inference limits:** these are **observability/logging checks**, not a suggestion to blindly write CE's GART or trigger values to a running Windows 98 system. The ring pages, memory aperture, mapping, address-space units, old commands and mode selection can differ. No static evidence proves that an unchanged readback alone is a failing engine.

<a id="c9-section-222"></a>

#### 222. Manual-versus-driver exceptions worth retaining

1. **Read/write MMIO alias:** `0x43C` is the VIA-defined AGP current-address readback on reads and the parameter-type selector on writes (§§197–213). This is not a contradiction once access direction is retained.
2. **Different numeric register groups:** `10h:60h–68h` are command-fetcher settings; VX900 `04h:60h–62h` are a separate vertex-routing family. Never carry their meanings across HParaType.
3. **SR7B manual gap:** CE modifies SR7B's low nibble during GART setup, even though VX855 documentation calls it reserved/read-only and VX900 calls the low bits writable but reserved. This is an **undocumented CE software sequence**, not independently decoded silicon functionality.
4. **`0x68` threshold versus trigger:** `0x680A0001` encodes two distinct settings: read threshold 10 and **fetch trigger** bit0. A `0x680A0000` value with the same threshold is not identical.
5. **CE registry path is OS-specific:** `HKLM\Drivers\Display\VIA\PCI_AGPMode` is verified for the examined Windows CE binary; no claim is made about the availability or meaning of that path on Windows 98.

<a id="c9-section-223"></a>

#### 223. Verification, limitations and next targeted check

The new **`Chrome9_CE_INV_Mode_and_GART_Audit_214_223.py`** performs read-only checks of the original DLL SHA-256, embedded Unicode registry strings, linker-map addresses and exact disassembly sites covering command mode selection, GART sizing, GTI setup/cache flush, hardware preamble and `0x680A0001` trigger. The VX855/VX900 original manuals were cross-checked for SR6C, SR6F, and HParaType `10h:68h/69h`. All checks passed. The audit **does not run CE code, read hardware, modify a registry or validate the Win98 ring**.

**Newly resolved:** an enabled CE INV ring requires a specific initialization path; its 8 MiB backing and GTI flush precede the explicit fetch trigger; `PCI_AGPMode=1` skips the CE INV ring setup; and `10h:68h[0]` is the documented trigger, not an inferred flag. **Still open:** the user's exact Windows 98 ring initialization sequence, page table format and physical mapping, chip state after trigger, why VX855 reads at `0x43C` previously remained constant, and whether any independently observed completed draw was actually fetched from the ring. The next high-value work is a **line-by-line comparison with the user's current Win98 VxD source or ring-init log**, if available, before proposing any mutating hardware experiment.

<a id="c9-section-224"></a>

#### 224. GTI translation-table contents and CE ring base

**Scope and evidence.** This pass extends §§214–223 using the user-supplied original VIA Windows CE 6.0 retail `DDI_VIA.dll`, SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`, `DDI_VIA.map`, and the original VIA VX855 Part I manual's sequencer GTI definitions. Unlike merely recognizing the `InitPCIe` entry point, it traces the **allocation mechanisms, exact four-byte GART entries, device-address zero base, ring prefix, and first kickoff conditions**. All observations are static; no CE, Windows 98, driver or GPU code was run.

**Key result:** The CE ring's *CPU virtual backing*, *system physical backing*, *framebuffer-local translation table*, and *GPU-visible ring address* are **different things**. In this initialization branch the GPU-visible ring base is specifically **zero**, not the CPU pointer or physical address. See §§225–229 for the instruction-level derivation. Consequently a read of `0x43C` equal to zero is **not by itself evidence of a failed ring**.

<a id="c9-section-225"></a>

#### 225. The CE command memory is allocated using `AllocPhysMem`; its GART table is in graphics memory

`InitPCIe` calls `AllocGART` at `0x10050691`, then `AllocSF` at `0x10050697`. The two helpers provide **different types of storage**:

| Structure field | Source and destination of value | Evidence |
|---|---|---|
| `GART_INFO+0x00` | Translation-table allocation size (4-byte PTEs) | `InitPCIe`, `0x1005067C–0x1005068F` |
| `GART_INFO+0x04` | Offset of table inside graphics-memory space | `AllocGART`, `0x100505E9` |
| `GART_INFO+0x08` | CPU pointer to that framebuffer-resident table | `AllocGART`, `0x100505EC–0x100505F4` |
| `GART_INFO+0x0C` | Command backing size, nominally `0x00800000` | `InitPCIe`, `0x10050679` |
| `GART_INFO+0x10` | Command backing's physical address, output of `AllocPhysMem` | `AllocSF`, `0x10050617–0x1005062A` |
| `GART_INFO+0x14` | CPU pointer to physically allocated command backing, return from `AllocPhysMem` | `AllocSF`, `0x10050632–0x1005064E` |

`AllocSF` passes allocation flags `0x204` and an aligned allocation size to imported **`AllocPhysMem`**, `0x1006CD58` (the `DDI_VIA.map` import identifies this function). When allocation succeeds it invokes VIA's `CeSetMemoryAttributes` wrapper (`0x10009A90`) before recording the CPU pointer. This is not a simple `malloc` of an arbitrary CPU buffer. The code uses a physically allocated system-memory region with a distinct physical address; the actual platform memory and cache behavior remain untested.

`AllocGART` (`0x100505BA–0x10050601`) obtains a local graphics-memory allocation offset from global `0x100C9F9C`. It updates this allocation cursor downward, with a **one-page subtraction in addition to the page-aligned table size**, then computes the CPU table pointer using global mapped-framebuffer base `0x100C9F94` (`0x100505E9–0x100505F7`). It **does not** allocate this PTE table through `AllocPhysMem`.

This means the CE command memory arrangement is:

```text
Local graphics memory (VRAM/framebuffer aperture)
    └── GART/RTSF translation table (4-byte entries)
             ↓ PTEs name physical system-memory pages
Physically allocated system-memory command backing (8 MiB nominal)
             ↕ mapped CPU command-writing pointer
INV ring commands, fetched using GPU-visible translated addresses
```

The table allocator has no visible local lower-bound check. If command backing allocation fails, `AllocGART` has already adjusted the graphics-memory allocation cursor and `InitPCIe` returns failure; this function does not visibly restore the cursor. This is a **local failure-path accounting observation**, not a proved persistent resource leak: higher-level cleanup or device reinitialization may reset the state.

<a id="c9-section-226"></a>

#### 226. GART PTE encoding is reconstructed from the actual write loop

The PTE loop at `0x10050710–0x1005075D` takes `GART_INFO+0x10` as a physical base address and `GART_INFO+0x08` as the PTE destination. For each entry index `i`:

```c
physical_page_start = physical_command_base + i * system_page_size;
pte_shift = (system_page_size == 4096) ? 12 : 10;
gart_table[i] = physical_page_start >> pte_shift;
```

Evidence: multiply at `0x1005072F`, physical-base addition at `0x10050732`, entry store at `0x10050734`/`0x1005074E`, system-page-size comparison at `0x10050739`, selected shift at `0x10050748–0x1005074C`, and loop test at `0x1005075B–0x1005075D`.

**Normal 4 KiB model:** an 8 MiB command allocation has **2,048 pages**, and therefore **2,048 × 4 = 8,192 bytes** of PTEs. If its physical base were `0x08000000` (illustrative only), entries 0, 1 and 2047 would be `0x00008000`, `0x00008001` and `0x000087FF`. The binary does not OR in an additional local "present" bit in this loop. The manual confirms the **SR6A/SR6B/SR6C table-base programming**, not the complete GTI interpretation of each entry's flags.

The PTEs are written to CPU-mapped framebuffer space, whereas the ring's actual pixel/graphics command bytes are written to the separate system-memory region. A driver implementing only address-register commands without a valid translation table would not reproduce this CE initialization path.

<a id="c9-section-227"></a>

#### 227. Conditional page-size mismatch in the CE translation-table size formula

The table size is calculated as `align_up(command_bytes / 1024, system_page_size)` at `0x1005067C–0x1005068F`, independent of the number of pages actually looped over. The entry count is separately calculated as `command_bytes / system_page_size` at `0x10050710–0x1005075D`, with **four bytes per entry**. These are equal **only when the system page size is 4 KiB** (for the conventional power-of-two cases modeled here).

| Assumed system page size | Command backing | Allocated table | Entries looped | Actual PTE bytes written | Mismatch |
|---|---:|---:|---:|---:|---|
| 1 KiB | 8 MiB | 8,192 B | 8,192 | 32,768 B | **24,576 B more written than allocated** |
| 2 KiB | 8 MiB | 8,192 B | 4,096 | 16,384 B | **8,192 B more written than allocated** |
| **4 KiB** | **8 MiB** | **8,192 B** | **2,048** | **8,192 B** | **0 B — balanced** |
| 8 KiB | 8 MiB | 8,192 B | 1,024 | 4,096 B | Over-reserved; additionally uses shift 10 rather than 13 |

The 2 KiB and 8 KiB rows are **counterfactual arithmetic probes**, not claims those configurations are supported by this driver. The 1 KiB alternative is noteworthy because the compiled PTE writer explicitly selects **shift 10 when page size differs from 4 KiB**. None of this proves a real overwrite on this x86 CE build: normal x86 CE page size is 4 KiB, and no runtime example of a non-4-KiB page size was supplied.

**Practical implication:** do not transplant the table sizing formula to a different OS/page-size environment without first deriving the number of four-byte PTEs from the actual page count, and without validating the hardware's PTE format for that page size. This is a conditional static arithmetic limitation, not a proven VX855 silicon error.

<a id="c9-section-228"></a>

#### 228. GTI register sequence: a previous-flush wait precedes a *new* flush request

The code establishes the RTSF table base through **sequencer SR6A/SR6B/SR6C** (`0x100506BF–0x100506EE`), modifies **SR7B** (`0x100506F3–0x1005070F`, with the manual uncertainty already discussed in §217), fills the PTE table, and executes:

1. `ReadSR(0x6F)` until bit 7 is **clear** (`0x10050761–0x1005076B`).
2. Set bit 7 in the value and call `WriteSR(0x6F, value)` (`0x1005076D–0x10050775`).
3. Set **SR6C bit 7** to protect the GART table from hardware writes (`0x1005077A–0x1005078B`).
4. Return success (`0x10050790–0x10050798`).

The VX855 Part I manual identifies **SR6F[7] as a GTI cache-flush request, set by software and cleared by the GTI**. Therefore the visible polling loop waits for a **previously pending** flush to be clear; the CE routine **does not locally poll completion after setting bit 7 for this new flush**. Later command-regulator initialization and the `10h:68h` fetch trigger occur afterward in another function.

This is a precise ordering observation, **not proof of a missing required fence**. The hardware or other initialization may guarantee that the newly requested flush becomes effective before fetching. A safe probe should log this state transition and command completion rather than changing SR registers speculatively.

<a id="c9-section-229"></a>

#### 229. A zero-based device ring is deliberate in the examined CE path

After successful `InitPCIe`, `cmCreate_inv` clears **`CMD_INTERFACE+0x20`** at retail `0x1004EF81`, while it separately installs the CPU command pointer (`+0x24` at `0x1004EF94`), the CPU end pointer (`+0x28` at `0x1004EF9A`) and the MMIO pointers to `0x43C/0x444` (`+0x34/+0x38`). Since the embedded `DMA_MANAGER` begins at **`CMD_INTERFACE+0x18`** (§§197–204), these correspond to:

| DMA manager field | Effective source in CE | Interpretation |
|---|---|---|
| `DMA_MANAGER+0x08` | `CMD_INTERFACE+0x20 = 0` | **GPU-visible ring address base = 0** |
| `DMA_MANAGER+0x0C` | `CMD_INTERFACE+0x24` | CPU ring backing start |
| `DMA_MANAGER+0x10` | `CMD_INTERFACE+0x28` | CPU ring backing end |
| `DMA_MANAGER+0x1C` | `CMD_INTERFACE+0x34` | Pointer to **MMIO `0x43C`**, read-side progress |
| `DMA_MANAGER+0x20` | `CMD_INTERFACE+0x38` | Pointer to **MMIO `0x444`**, status |

`SetAGPRingCmdRegs_inv` (`0x1004E921–0x1004EAEB`) computes the ring end and initial pause points by **subtracting the CPU ring base and adding this device address base**, then emits the Type-10h start/end/pause words. For the three family IDs it ORs the **location bit 0** into the Type-10h start command (`0x1004EA31–0x1004EA49`), so a zero ring base can still legitimately produce a start word with low data bit 1.

**Diagnostic correction:** a read-only result of `MMIO[0x43C] = 0` could be consistent with an initialized zero-based ring at its starting address, a deliberately paused or idle fetcher, an inactive/incorrectly configured fetcher, or a different stale-readback condition. It does **not** identify which explanation holds without reference to actual producer position, pause address, status and independent completed work. This strengthens, rather than reverses, the readback ambiguity discussed in §§197–223.

<a id="c9-section-230"></a>

#### 230. The ring starts with a FE02 prefix and generation-specific alignment

The newly initialized CPU ring memory starts with a **four-DWORD FE02 sequence** at `0x1004E952–0x1004E96C`:

```text
FE020420    FE02 command packet
0000041C    MMIO register address
00300000    Parameter selector / control value
CC000000    Padding/sentinel DWORD
```

After this prefix, the code repeats `0xCCCCCCC0, 0xDDD00000` pairs until the CPU pointer reaches the family-specific alignment:

| PCI ID | Initial prefix termination alignment | Instruction branch |
|---|---:|---|
| `1122h` VX800 | 256 bytes | `0x1004E9C0–0x1004E9D2` |
| `5122h` VX855 | 256 bytes | same |
| `7122h` VX900 | 64 bytes | `0x1004E994–0x1004E9A6` |
| `3371h` earlier family | 32 bytes | `0x1004E9AA–0x1004E9BC` |

For a page-aligned CPU ring base, this amounts to an initial **256-byte VX855 prefix** or **64-byte VX900 prefix**, before subtracting 16 bytes for the translated initial pause address (`0x1004E9D4–0x1004E9DC`). These are software-generated stream footprints, not measured hardware transfer traffic. The exact hardware meaning of the two repeated sentinel words is **not** decoded here.

At the end of this initializer (`0x1004EAE3–0x1004EAE6`), VIA sets **both** `DMA_MANAGER+0x14` (current CPU producer cursor) and `DMA_MANAGER+0x18` (last submitted/committed cursor) to the same pointer. The hardware fetch trigger is issued earlier at `0x1004EAD8`, but no later user workload has been submitted merely by initializing the ring.

<a id="c9-section-231"></a>

#### 231. A kickoff with no new commands is a no-op in this CE driver

`KickOffRingAGP_inv` (`0x1004EC27–0x1004EDFD`) first compares the current CPU producer cursor with the committed cursor:

```asm
1004EC2F  mov ebx,[DMA_MANAGER+0x14]
1004EC32  cmp ebx,[DMA_MANAGER+0x18]
1004EC35  je  0x1004EDFA     ; return without pause-address update
```

Because §230 establishes that these two fields are **equal immediately after initial setup**, an application or test routine that merely invokes kickoff without first appending command data will **not** issue the normal new pause-address update. The code can still have placed its initialization prefix and fetch trigger in hardware-visible memory; **this equality alone cannot show whether or how much of that prefix the hardware fetched**.

When there is newly submitted work, kickoff selects chipset-specific tail padding and updates the Type-10h pause address (`64h/65h`) through MMIO `0x41C/0x420` (`0x1004ED9E–0x1004EDF6`). For VX800/VX855 there is also a conditional large filler path (`0x1004ED0D–0x1004ED59`), discussed previously; it is **not** executed on every call. VX900 uses a separate alignment path. These are software conditions relevant to comparing command captures, not proof of superior or inferior ring throughput.

**Implication for previous probes:** to show actual CE-style ring consumption one must distinguish *initialized*, *new commands appended*, *kickoff sent*, and *work completed*. Simply observing the FE02 prefix or an unchanged `0x43C` is not enough.

<a id="c9-section-232"></a>

#### 232. CE ring setup checks and open questions

The associated `Chrome9_CE_GART_PTE_and_Ring_Startup_Audit_224_232.py` pins the original DLL SHA-256, verifies key instruction addresses and map imports, independently checks the GART field layout, GTI flush order, zero-based device ring, FE02 prefix, kickoff no-work test and mathematical table-size cases; `Chrome9_CE_GART_Page_Size_Model_224_232.csv` records the page-size scenarios. The script passed without executing CE code or touching hardware.

For a future **read-only** VX855 Win98 driver comparison, record together (from the driver's own state and logs where available):

1. Actual ring command mode and whether init returned success, plus command-queue CPU base/size and **device-visible base**.
2. Physical backing allocation and translation-table location/entry format *if that Win98 implementation uses GART*; do **not** assume the CE RTSF recipe applies to every driver mode.
3. The precise Type-10h `60h–68h` words; include the start-location bit, pause and jump addresses, and the fetch-trigger bit.
4. `0x43C` and `0x444` samples alongside a new-work counter, producer/committed pointers, pause address and an **independently confirmed completed draw**.
5. Whether the first kickoff followed any new appended commands, rather than assuming kickoff alone necessarily generates hardware work.
6. Any GTI cache-flush **request and completion** observations, especially the distinction between a pre-request wait and a post-request wait.

**Established:** GART table storage in framebuffer space, physical CPU-accessible command backing, normal 4-KiB PTE layout, zero-based GPU ring, initialized FE02 prefix, and no-work kickoff short circuit. **Conditional limitation:** non-4-KiB page table accounting is not generally correct; whether such a page size is supported or reachable is unknown. **Still unproven:** what exactly occurred in the user's older Windows 98 ring experiments, whether any draw was actually consumed from that ring, live readback monotonicity on VX855, flush latency requirements, and actual stability of a real board. No code modification to the Win98 driver is proposed from this static evidence alone.

<a id="c9-section-233"></a>

#### 233. Fetcher state, command visibility and pause publication

**Scope.** The original 867,840-byte VIA CE 6.0 retail `DDI_VIA.dll` (SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`) and its matching linker map were rechecked at `SetAGPRingCmdRegs_inv` (`0x1004E921`), `InitAGPPausetoStop_inv` (`0x1004E81D`), `KickoffRingAGP_inv` (`0x1004EC27`), `RewindRingAGP_inv` (`0x1004EAEC`) and `GetRingAGPSpace_inv` (`0x1004EDFE`). The source-based meanings below are grounded in the original VIA VX855 Part II Type-10h register tables (also present in VX800/VX900 manuals). No CE or Windows 98 code was executed; this is not evidence of the user's Win98 ring actually running.

**Key result:** it is incorrect to interpret every stationary `0x43C` result as a hung consumer. In this CE implementation, initialization, no-new-work kickoff, a **normal pause**, and a **stop-address recovery** are different states, and CE has cache-synchronization calls before publishing new ring work. The exact state sequence and register-mode fields, not merely a raw readback, must be compared.

<a id="c9-section-234"></a>

#### 234. CE selects the translated *S.F.* ring-location encoding, despite physically allocated command RAM

`SetAGPRingCmdRegs_inv` constructs a Type-10h `60h` word from the device-visible start address and **ORs its low two bits with `01b`** on the `1106:1122`, `1106:5122` and `1106:7122` branches (`0x1004EA31–0x1004EA49`). The VX855 manual Type-10h `60h` explicitly defines location bits `[1:0]` as `00` = S.L. (system local framebuffer), `01` = **S.F. (system dynamic framebuffer)**, `10` = S.M. (system memory). This is independently consistent with the equivalent tables in the other two manuals.

This **does not contradict** §225's `AllocPhysMem` command backing: CE first constructs the GTI table in framebuffer memory and maps physical pages, then provides a translated **S.F. device-address-space** ring. The hardware location selector must not be chosen by merely inspecting whether the CPU backing was allocated in ordinary RAM. For these three IDs, the setup selects `01b` rather than the literal `10b` S.M. mode.

Other fixed startup values are unchanged: the command-regulator writes Type-10h `66h/67h = 0xFFFF_FFF0` in this initialization path (`0x1004EAB7`, `0x1004EAC7`) before `68h = 0x680A0001` (`0x1004EAD8`). Although `66h/67h` are *jump-address* fields by the manual, the exact purpose of the all-ones aligned value is **not decoded**. Do not label it a validated executable jump destination or copy it independently of the complete setup sequence.

<a id="c9-section-235"></a>

#### 235. Normal kickoff uses *pause* ID `00b`, not *stop* ID `10b`

`KickoffRingAGP_inv` builds Type-10h `64h` (pause address low 24 bits) and Type-10h `65h` with the high address byte ORed with **`0x65000000`** (`0x1004EDD4–0x1004EDED`). Thus `65h[9:8]` = **`00b`** in ordinary kickoff. The VIA manual calls this a **pause point**: the fetcher pauses at the specified address and may resume when the pause address changes.

The code publishes `64h/65h` through MMIO `0x41C/0x420` after writing new command bytes. **This ordinary kickoff does not contain a local `68h` fetch trigger.** That is consistent with updating the pause boundary of an already initialized, triggered ring, but does not prove what a previously stopped fetcher will do without an additional trigger.

`0x1004EC32–0x1004EC35` compares producer and committed pointers, jumping to return if they match; no new pause-address write occurs in that no-work case. This is especially relevant immediately after ring initialization (§231), when both pointers were initialized equal.

<a id="c9-section-236"></a>

#### 236. The startup paused-fetcher recovery deliberately switches to *stop* ID `10b`

`InitAGPPausetoStop_inv` first skips the following recovery on `7122h` (VX900) (`0x1004E820–0x1004E830`). On other affected chips, it reads the MMIO `0x444` status pointer and only enters the recovery branch if bit 31 is set (`0x1004E836–0x1004E83A`). The earlier independent VIA Linux source names that bit `INV_AGPCMD_InPause` (§§205–213).

Inside that recovery branch, CE constructs a new boundary and **ORs `0x65000200`** (`0x1004E8EB`): Type-10h `65h[9:8] = 10b`, which the manual calls a **stop address**. The hardware specification distinguishes that mode from an ordinary pause: to resume after reaching a stop address, software must trigger AGP fetching (`10h:68h[0]`). The inspected recovery function itself does **not** issue the `68h` trigger. This is a state transition with a different restart requirement, not just another name for an idle ring.

It then polls MMIO `0x400` until `status & 0x00101FF3` clears (`0x1004E8F7–0x1004E907`), with no local timeout or yield. That wait is **separate** from the `0x43C` queue-space polling discussed earlier. A stuck startup recovery may occur before any user draw is submitted; none has been reproduced on hardware.

<a id="c9-section-237"></a>

#### 237. The normal kickoff invokes CE `CacheSync(1)` *twice before* changing the pause address

A previously unclassified external call at `0x1006CD8E` is resolved in the *retail linker map* as the Windows CE core DLL import **`_CacheSync`**. On the active, nonempty kickoff path:

```text
0x1004ED9E  push 1
0x1004EDA0  call CacheSync
0x1004EDA5  compute translated device pause address
0x1004EDAB  push 1
0x1004EDB1  call CacheSync
0x1004EDB6  write MMIO 0x41C selector
0x1004EDD9  write Type-10h 64h (pause low)
0x1004EDED  write Type-10h 65h (pause high + pause ID 00b)
0x1004EDF3  commit producer cursor
```

Published Windows CE cache flag definitions give **`CACHE_SYNC_DISCARD = 0x001`**, meaning *write back and discard data-cache contents*. This identifies VIA's **requested cache operation**; the *effective platform-specific x86 implementation* and memory-coherency cost have not been measured. Windows CE's documented `OEMFlushCache` implementation notes that on coherent x86 systems the implementation can instead serialize writes without a full cache invalidation. Sources: Microsoft, [OEMCacheRangeFlush](https://learn.microsoft.com/en-us/previous-versions/windows/embedded/ms904878%28v%3Dmsdn.10%29), [OEMFlushCache](https://learn.microsoft.com/en-nz/previous-versions/windows/embedded/ms904900%28v%3Dmsdn.10%29), and the [Windows CE `pkfuncs.h` flag definitions](https://github.com/hjgode/cn51_reset_enet/blob/master/CN51_enet_reset/pkfuncs.h).

**Implication:** a comparable Windows 98 ring test must verify that the GPU can see newly written command bytes **before** the pause/address handoff. `CacheSync` itself is not portable to Win98, and neither a blanket `wbinvd` nor an unconditional caching change is justified by this CE trace. Record actual mapping attributes and use appropriate, tested memory-ordering/coherency mechanisms.

<a id="c9-section-238"></a>

#### 238. The GART construction path also calls `CacheSync(1)` around setup phases

The CE GART/command-memory preparation function at `0x10050300–0x10050457` calls the same imported `_CacheSync` at **four** additional addresses: `0x100503B8`, `0x100503C7`, `0x1005040B`, `0x10050420`. The immediate call argument is `1` in these sites (register `EDI=1` is pushed).

These calls surround distinct memory preparation and initialization steps; they are **not evidence** that the GTI `SR6F[7]` cache-flush request is redundant. A CPU data-cache operation and a GTI address-translation cache operation target different state. As §228 established, the local `SR6F[7]` sequence waits for a previous flush to complete before issuing a new request; it does not visibly poll that newly issued flush to completion in that routine.

<a id="c9-section-239"></a>

#### 239. Rewind programs *jump* and *pause* addresses; it does not locally retrigger fetch

`RewindRingAGP_inv` (`0x1004EAEC–0x1004EC26`) writes `10h:66h/67h` (jump address low/high) followed by `10h:64h/65h` (pause address low/high) through `0x41C/0x420`. It resets producer and committed cursors to the ring beginning. In the inspected function there is **no Type-10h `68h` fetch trigger**. The jump target here is derived from current ring pointers; it is distinct from the all-ones `66h/67h` initialization value in §234.

For VX900, `GetRingAGPSpace_inv` additionally reads MMIO `0x47C` and conditionally writes **`0x66FFFFF0` / `0x670000FF`** near `0x1004EEFE–0x1004EF28`. The public manual identifies the Type-10h `66h/67h` fields, but the reason for this VX900-specific conditional reset remains unresolved. Avoid describing the all-ones address as a confirmed disabled-jump sentinel without hardware/source evidence proving that meaning.

<a id="c9-section-240"></a>

#### 240. Readback and publication form distinct diagnostic states—not a single progress counter

| State / operation | Evidence to capture | Do **not** conclude from `0x43C=0` alone |
|---|---|---|
| Ring disabled or initialization bypassed | `PCI_AGPMode`, actual initializer return, callback installation | A disabled fetcher is not a running but stalled fetcher |
| Ring initialized, no new work | Device base, producer == committed, kickoff no-work branch | Zero may be its intentional device-visible start (§229) |
| New packet written, not yet published | New-work counter, producer vs committed; memory visibility | Lack of consumption is not proof of failed execution |
| Normal kickoff with ID `00b` | Last `64h/65h` values, `CacheSync`/barrier completion | A pause boundary is not a stop point |
| Recovery with ID `10b` | `0x444[31]`, `65h[9:8]`, `68h[0]` trigger, engine wait | A stopped fetcher may require re-triggering |
| Rewind | `66h/67h`, `64h/65h`, ring-head/tail addresses | Rewinding does not itself demonstrate a completed draw |
| Ring queue-space wait | Double-sampled `0x43C`, translated producer, margins | A stationary pointer under zero work may be normal |

The CE manager's MMIO pointer setup at `0x1004EFA2–0x1004EFB8` and dual reads at `0x1004EE45–0x1004EE4F` were independently reverified. Do not infer a separate CPU-memory ring progress word; §§197–204 already corrected that earlier error. A log should capture `0x444[31]`, pause ID, current translated producer, queue readback, and a separately observed completed draw *together*.

<a id="c9-section-241"></a>

#### 241. Verification, limits and next evidence needed for the actual Win98 VX855 source

The read-only audit `Chrome9_CE_Ring_Publish_and_Pause_Audit_233_241.py` pins the CE retail DLL SHA-256, proves the static source/register sites for each state, cross-checks the original VX855 register tables, verifies `_CacheSync` resolves through the **retail map**, and exports `Chrome9_CE_INV_Ring_State_233_241.csv`. The audit **passes** and touches no hardware.

**Historical scope limit (superseded by §§242–251 for source availability):** the user's current Windows 98 VX855 VxD/driver source and actual ring-enabled register log were **not present in the local CE research bundle** for this specific comparison. The report therefore does **not** claim the Win98 driver has (or lacks) matching GTI/GART setup, a memory barrier, a matching S.F. location selector, or a correct re-trigger sequence. To close that gap, compare a *specific* Win98 build and an **already safe, known-good** logged ring workload against: `10h:60h[1:0]` S.F. selection, `10h:65h[9:8]` pause/stop selection, `10h:68h[0]` initial/restart trigger, GTI initialization, command-memory visibility before pause updates, and `0x43C/0x444` progress/status. Diagnose with read-only telemetry first; no experimental register changes are recommended merely from this CE static evidence.

### Win98 v10.479 implementation and diagnostics

<a id="c9-section-242"></a>

#### 242. Windows 98 VX855 v10.479 driver source

**This changes the evidence baseline after §241.** The user's earlier project Library contains `VX855_v10_479_src.zip` (SHA-256 `9d7790fc85f488321a996ee9e070c2e75c69dde37ecaa75d9de220b574b24029`). Its exact implementation is in `src356/src/vx_native3d_vxd.c`, especially lines 119–142, 510–760, 1157–1190 and 1304–1335. The independent original VIA CE retail `DDI_VIA.dll` remains SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`, with CE addresses in §§189–241. **Win98 v10.479 is a specific historical snapshot, not proof of how later unretrieved builds behave.** These findings supersede §241's *evidence-availability* limitation only; the missing **live** ring register and test workload logs remain missing. No driver source was modified.

**Main correction:** earlier sections treated Win98's actual GART mode, visibility barrier and pause/stop handling as unknown because the code had not been provided in the CE working bundle. They are **explicitly implemented** in v10.479. Their presence does not establish that initialization succeeded on the actual Wyse board or that any 3D packet was consumed.

<a id="c9-section-243"></a>

#### 243. Ring address, size and backing differ materially between CE and Win98

| Field | VIA CE retail on VX855 | User's Windows 98 VX855 v10.479 | Source |
|---|---|---|---|
| Device-visible ring base | `0x00000000` | **`0x00100000`** = `XR_IDX0<<12` | CE §229; Win98 `#define XR_IDX0 0x100UL` and `#define XR_GPU (XR_IDX0<<12)`, lines 119–123 |
| Size | `0x00800000` = **8 MiB** | **`XR_PAGES=64`, `XR_BYTES=0x40000` = 256 KiB** | CE §225; Win98 lines 119–124 |
| GART table size | **8 KiB** (2,048 4-byte entries) on a 4 KiB-page x86 CE build | **64 KiB** = 16,384 4-byte entries (`XR_ENTRIES`) | CE §226; Win98 lines 119–125 and `xr_table()`, lines 592–597 |
| CPU-side command allocation | CE `AllocPhysMem`, nominal 8 MiB | Win98 `_PageAllocate(XR_PAGES+1, ...)`: **65 pages (260 KiB) including a guard page** | CE §225; Win98 `xr_setup()`, lines 623–634 |
| Entry population | Sequential pages from physical base (`physical >> 12`) | Separate physical page frames from `_CopyPageTable`; 64 ring pages mapped at indices `0x100–0x13F`, guard at `0xFF` and `0x140–0x143` | CE `0x10050710–0x1005075D`; Win98 `xr_table()`, lines 591–597 |
| Ring-location selector | Type-10h `60h[1:0]=01b` | **Same `01b`:** `0x60000000 | ((XR_GPU|1)&0xFFFFFF)` | CE §234; Win98 `xr_start()`, line 707 |
| First startup pause | CE FE02 prefix, initial producer==committed | Win98 FE02 start block with pause **`0x001000F0`** (`XR_GPU+256-16`) | CE §§230–231; Win98 lines 698–712 |

**Consequence:** the historical observation `MMIO 0x43C=0` can be a legitimate *configured ring base* in the inspected CE scheme, but **is outside the valid Win98 v10.479 ring address interval `0x100000–0x140000`**. Thus it does not satisfy v10.479's own first-start check if its ring path is active. It still cannot be retroactively diagnosed from older Win98 builds that may have configured a different mechanism.

<a id="c9-section-244"></a>

#### 244. GART/PTE differences: the Win98 implementation explicitly tries alternative formats

CE writes physical page-number PTEs using the local `physical >> 12` shift without an additional bit-24 OR in the inspected loop (§226). Win98 gets 65 page-frame entries using `_CopyPageTable`, then for each translation PTE executes:

```c
/* Win98 v10.479: simplified from xr_entry() */
entry = ((xr_pte[page] >> 12) & 0x000FFFFF)
      | ((xr_variant & 1) ? 0 : 0x01000000);
```

Therefore the initial Win98 `xr_variant=0` **sets bit 24** in the PTE, unlike CE's local writer. Variants with bit 0 set omit that bit. The code comments identify it as an alternate page-entry form; they are not a hardware specification of bit 24. Win98 additionally fills unused GART entries with `0x80000000`, maps a guard page around the ring and checks for duplicate/invalid physical pages (`xr_setup()`, lines 628–633). The two GART programs are **not byte-for-byte interchangeable**; neither one alone establishes the correct PTE format on every Windows 98 platform.

`xr_table()` clears SR6C bit 7 before updating the table; `xr_srs()` then programs SR6A/SR6B/SR6C and sets/clears SR6C bit 7 based on `xr_variant & 2` before requesting an SR6F[7] translation-cache flush. The CE path described in §228 protects the table after writing entries. This means Win98 has an explicit *optional* protection-bit branch, not a missing GTI setup.

<a id="c9-section-245"></a>

#### 245. Normal start, pause and stopped-fetcher recovery are already implemented in Win98

Win98 `xr_start()` lines 685–712 issues Type-10h `60h–68h` commands through MMIO `0x41C/0x420`, with the same core **`0x680A0001`** initial fetch trigger observed in CE `SetAGPRingCmdRegs_inv` (`0x1004EAD8`). Both use S.F. selector `01b` in `60h`. Win98 sets `66h/67h` initially to `0x66FFFFF0 / 0x670000FF`, which also matches the CE initializer (§234). Normal Win98 publication (`xr_kick()`, lines 1304–1335) writes `64h/65h` with **pause ID `00b`**, like CE's `KickoffRingAGP_inv` (§235).

When a previously active ring is found paused at startup (`0x444[31]`), Win98's `xr_start()` can put a recovery block at the current `0x43C`, publish Type-10h `65h` with bit `0x200` (**stop ID `10b`**), and wait for idleness before beginning a new start (lines 687–698). This agrees with the meaning of CE's VX800/VX855 paused-ring startup recovery (§236). Win98 additionally **starts by issuing `0x680A0000`**, stopping an old fetcher before reprogramming its new base (lines 704–710). None of these features is generally absent from v10.479.

<a id="c9-section-246"></a>

#### 246. Both drivers publish command bytes after an explicit visibility operation; implementations differ

CE `KickoffRingAGP_inv` uses imported `_CacheSync(1)` twice before the pause-address update (§237). Win98 uses two separate mechanisms in `xr_kick()` (lines 1304–1313):

1. **Default `ACCEL_D3D_XPRING_NT=1`, if CPUID reports SSE2:** build in cached staging memory, copy DWORDs to the ring with **MOVNTI**, then issue **SFENCE** before publishing `64h/65h`.
2. **Fallback:** build in ring memory, call `xr_flush()` using `CLFLUSH` plus `MFENCE` where supported, or `WBINVD` for older CPUs; the source also keeps cache-line-size and flush-mode counters.

Win98 initializes the GART translation and requests an SR6F[7] flush separately in `xr_srs()`. **This is not a missing cache synchronization implementation.** Whether the active v10.479 Win98 mode and CE x86 cache operation actually produce equivalent bus visibility is not established by source alone. In particular, code-level SFENCE presence does not substitute for a confirmed cache/coherency configuration on real hardware.

<a id="c9-section-247"></a>

#### 247. Win98 explicitly experiments with six fetcher configurations, unlike CE's fixed path

`xr_forms[6]={0,4,0x0B,7,1,2}` (`vx_native3d_vxd.c`, around line 580). On first-start validation failure, Win98 disables fetching, updates the variant and retries (`xr_start()`, lines 714–727). Its mask-controlled options are:

| Bit in `xr_variant` | Tested behavior |
|---|---|
| `0x1` | Do **not** OR `0x01000000` into GART PTEs |
| `0x2` | Set SR6C bit 7 after table write |
| `0x4` | Enable optional 64 KiB command-regulator virtual queue |
| `0x8` | Issue the alternative command-regulator claim sequence |

The order is **`0`, `4`, `0x0B`, `7`, `1`, `2`**. When all fail, `xr_off(4)` marks the ring unavailable and the existing VxD selects fallback rather than claiming successful ring operation. These are six *software candidate initializations*, not six verified hardware modes. A diagnostic must report which form was retained (`vx3dn_xr_stat[60]`, where `0xFF` means none) and how many were tried (`[61]`).

<a id="c9-section-248"></a>

#### 248. The Win98 first-start readback check is weaker than its descriptive comment

**2026-11-06 hardware-evidence correction:** §§261–270 recover two successful logs in which `0x43C=0x100100` while programmed `0x45C=0x1000F0`. The source predicate remains broad, but **strict equality to pause is NOT a valid unqualified replacement**. Require mode-specific evidence of a completed workload and consistent status/readback, allowing the observed +16 offset.


Win98 runs the startup check only on a first start (`!xr_tested`). It reads a quiet/paused indication and `0x43C`, and accepts **any** readback in the **inclusive** ring interval:

```c
/* v10.479 vx_native3d_vxd.c, line 717 */
if (z > 0 && c >= XR_GPU && c <= XR_GPU + XR_BYTES) {
    ok = 1;
    break;
}
```

The actual expected startup pause for the 256-byte initial block is **`0x1000F0`**. The condition does **not** check `c==0x1000F0`, compare current with a previous reading, or require a recorded completed command. Thus a hypothetical *stationary* `c==0x100000` with `z>0` would satisfy this software condition. Even `c==0x140000`, the first address beyond the 256-KiB ring allocation, satisfies the written `<=` bound. Separately, `xr_rp_ok()` requires consecutive equal reads but `xr_rp()` discards the success flag; the initial test calls `xr_rp()`, not `xr_rp_ok(&ok)`.

**Classification:** demonstrable *startup-validation weakness*, **not proof of a reachable false positive, hardware hang or memory overrun**. A conservative future validation should compare the expected pause readback and a benign completed workload, with appropriate tolerance for VIA's documented occasional bad register reads, before declaring command fetching operational. Do not change fetcher registers solely from this theoretical state model.

<a id="c9-section-249"></a>

#### 249. Optional Win98 VQ programming independently supports the manual's 16-byte length units

When `xr_variant&4`, `xr_vq(1)` enables a separate **64 KiB virtual queue** from video memory. Its `10h:73h` length expression is:

```c
#define XR_VQ_BYTES 0x10000UL
len73 = (XR_VQ_BYTES - 0x400UL) >> 4;
// 0xFC0 = 4032 units; 4032 * 16 bytes = 64 KiB - 1 KiB
```

This directly demonstrates that the Win98 implementation computes this length with **16-byte units**, consistent with the VIA manuals' `10h:73h` description. It differs from the earlier OpenChrome source's `VIA_VQ_SIZE>>3` used with a 256 KiB queue (Part VI, §H (OpenChrome VQ-size discrepancy)). **It does not prove** the OpenChrome driver overruns memory: the two paths might deliberately use different units in different modes or expose an undocumented hardware interpretation. The Win98 code is nevertheless a valuable second implementation and an independent reason to leave the OpenChrome discrepancy marked unresolved rather than dismissing it.

<a id="c9-section-250"></a>

#### 250. Win98 adds bounded recovery and per-wrap safeguards absent from the traced CE waits

Compared with CE's no-local-timeout `GetRingAGPSpace_inv` read-pointer loops (§§148–155, 181–188), the Win98 v10.479 ring has:

- `xr_srs()` **10 ms GTI flush deadline**, failing the startup if SR6F[7] does not clear (`vx_native3d_vxd.c`, lines 604–618).
- `xr_start()` **100 ms first-start deadline** across its selected candidate, then tries alternate forms or disables the ring (lines 714–727).
- `xr_wait()` **100 ms without observed read-pointer/fence progress**, extending an active wait if either advances, with a **2-second absolute cap** (lines 648–669); fallback/recovery is conditional and not guaranteed to succeed.
- `xr_wrap()` places the jump at **the final 256-byte boundary**, not at the physical end of the ring (lines 1157–1169); `xr_open_x()` uses a **1 KiB read-ahead safety margin** and checks paired `0x43C` reads while a prior lap may still be in flight (lines 1175–1182).
- The first work submitted after wrap can receive one extra pause-address publication if a fence is delayed (`xr_wait()` lines 658–662), without assuming that reposting will complete a genuinely hung GPU.

**Implication:** v10.479 has substantially more error detection, alternate-start handling and telemetry than the CE reference, but those safeguards are not proof of correct execution. The remaining valuable evidence is **the actual per-attempt status vector and the first-start readback**; revisiting CE startup formulas alone will not tell us why the original Win98 tests failed.

<a id="c9-section-251"></a>

#### 251. Source-anchored diagnostic map, audit and residual uncertainty

The v10.479 VxD already exposes detailed logging through its `vx3dn_xr_stat[]` and companion arrays (see its lines 513–537 and access through the existing `TEXDBG` interface):

| Index / group | Meaning in v10.479 | Diagnostic use |
|---|---|---|
| `xr_stat[1]` | Reason ring disabled | Distinguish invalid allocations (`1–3`), no startup pause (`4`), GTI timeout (`5`) |
| `xr_stat[19]` | SR6F flush poll count, or `0xFFFFFFFF` on timeout | Distinguish GTI translation setup failure |
| `xr_stat[20–26]` | First-start `0x400`, `0x43C`, `0x444`, PTE offset, `0x440`, `0x45C`, `0x464` | Compare pause readback with **expected `0x1000F0`** and ring start `0x100000` |
| `xr_stat[30–59]` | Six groups of five values: `0x400|form`, `0x43C`, `0x444`, `0x45C`, `0x464` | Compare all startup variants without modifying hardware configuration |
| `xr_stat[60–61]` | Retained variant, count attempted | Establish which branch actually initialized |
| `xr_stat[62–71]` | Post-wrap status and whether republishing helped | Separate startup failure from later wrap/fence issues |
| `xr_stat[14–15]`, `[27–29]` | Last-hang ring readback/status/pause | Pin where the fetcher stopped after a successful startup |

**Suggested non-destructive next test:** capture the existing startup and hang logs from one known-good, already-running v10.479 configuration. Distinguish (a) no ring initialized; (b) startup variant failed; (c) initial predicate passed but `0x43C` did not reach expected `0x1000F0`; (d) initial ring works but post-wrap/retire waits fail. Do **not** change GART PTEs or toggle command-fetch bits solely on a static hypothesis. Later Win98 sources and actual board logs would need a separate comparison.

**Verification:** `Chrome9_CE_vs_Win98_v10479_Ring_Comparison_Audit_242_251.py` pins both original SHA-256 hashes, checks the cited Win98 source statements and CE compiled constants, and exercises the first-start predicate with representative addresses (including zero, base, expected pause, one-past-ring). The audit passed. It models source logic only; **no GPU, CE driver or Windows 98 binary was run**. The CSV `Chrome9_CE_vs_Win98_v10479_Ring_Matrix_242_251.csv` provides the portable side-by-side register/memory table.

<a id="c9-section-252"></a>

#### 252. Audit scope and source provenance: Win98 v10.479 telemetry, not a new ring experiment

This pass examines the **existing observational interface**, not another speculative fetcher register setup. The two original inputs are the user-owned `VX855_v10_479_src.zip` (`SHA-256 9d7790fc85f488321a996ee9e070c2e75c69dde37ecaa75d9de220b574b24029`) and VIA's CE retail `DDI_VIA.dll` (`SHA-256 529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`). The relevant Win98 files inside the ZIP are `src356/src/vx_native3d_vxd.c`, `src356/src/vxd_main.c` and `src356/src/vx_d3d_mon.c`. All statements below describe **this v10.479 snapshot**; no later driver or live board values were inspected.

Sections 242–251 already establish the expected first pause address `0x001000F0`, 256 KiB ring at `0x00100000`, and the six attempted initialization forms. The purpose here is to assess **what the code's readbacks, counters and shipped VXD3DMON display can actually prove** and which diagnostics can mask a failure.

<a id="c9-section-253"></a>

#### 253. The full ring diagnostic array is already exported; no new VxD instrumentation is necessary

In `vxd_main.c:1284–1325`, the existing `DeviceIoControl` TEXDBG selector `0xBC` accepts a **secondary index**. Indices `0x86` through `0x91` return all 72 DWORDs of `vx3dn_xr_stat[]` in 12 groups of six at output DWORDs `[2..7]` (`vxd_main.c:1325`). `0x95` returns all six `vx3dn_xr_stat2[]` entries (`:1295`); `0x92`/`0x93` return 12 profile entries (`:1294`); `0xB9` returns `vx3dn_ringprog` and two progress-timeout counters (`:1307`); `0xBA` returns six retire-wait duration buckets (`:1306`). Index `0x94` **clears the ring profile** and is not part of a read-only snapshot.

`vx_d3d_mon.c:21–47,464–479` confirms that **the shipped `VXD3DMON.EXE` already retrieves and prints these values**. For a diagnosis, use its logs or the preexisting `0xBC` read-only selectors; do not add risky raw MMIO writes merely to obtain telemetry. Multiple individual selector reads are not an atomic snapshot, so in a busy game the array can change between reads.

<a id="c9-section-254"></a>

#### 254. Startup's read-pointer reliability check is weaker than the wraparound check

`vx_native3d_vxd.c:542–544` defines `xr_rp_ok(int *ok)`: up to eight pairs of `0x43C` readings are sampled until two consecutive readings agree; `*ok=1` only on agreement. But the convenience wrapper `xr_rp()` **discards `ok`** and returns the most recent reading even if none agreed. The first-start predicate (`:714–720`) uses `c=xr_rp()`, not `xr_rp_ok(&ok)`, and accepts `xr_quiet()>0` plus `0x100000 <= c <= 0x140000`. It does not require the expected pause `0x1000F0` or stable-read evidence. The inclusive upper bound also accepts the one-past-end address `0x140000` as a software matter.

By contrast, the **post-wrap lap check** (`:1179–1181`) explicitly calls `xr_rp_ok(&rok)` and forces a retire/wait if `rok` is false, the address is out of range, or the producer could overtake the consumer with its safety margin. Thus *startup accepts an unqualified in-range readback, while wraparound is deliberately more conservative*. This is a **diagnostic/validation weakness** with a plausible false-positive mechanism; no static analysis establishes that an unstable in-range reading actually occurred on the board.

<a id="c9-section-255"></a>

#### 255. GART revalidation checks selected fields, not the complete table or register state

Before a re-start, `xr_start()` calls `xr_srs_ok()` and `xr_table_ok()` (`:685`). The former (`:599–603`) checks SR6A, SR6B and **SR6C bit 0 only** against the table base. It does **not** confirm SR6C bit 7, which is toggled by `xr_variant & 2` in `xr_srs()` (`:606–617`). The latter (`:598`) checks **only the first and last command-page entries** (`XR_IDX0`, `XR_IDX0+XR_PAGES-1`): it does not inspect the other 62 command-page PTEs, the guard-page PTEs, or all 16,384 table entries. A mismatch in *any of the fields actually checked* causes a full `xr_table()` and `xr_srs()` attempt. An unexamined middle-PTE change or SR6C protection-bit change **could pass the quick check**, but no such change has been demonstrated.

For testing, treat these routines as cheap *sentinel checks*, not proofs of complete GART integrity. A safe offline audit can examine the original PTE-writing code; active live-GART dumps or reprogramming are not required to conclude the validation is partial.

<a id="c9-section-256"></a>

#### 256. Startup statistics preserve attempted modes, but several common fields are overwritten on retry

`xr_start()` records the latest attempt's `0x400`, `0x43C`, `0x444`, `0x440`, `0x45C`, and `0x464` values into `vx3dn_xr_stat[20–22,24–26]` (`:719`) and also archives **five values per attempt** at `[30+5*i .. 34+5*i]` (`:720`). On each retry, the general `[20–26]` fields are overwritten; the six five-DWORD blocks are the useful per-form history, but they **do not include `0x440` or SR6F poll history**. Similarly, `xr_srs()` writes `vx3dn_xr_stat[19]` afresh on every GTI flush poll (`:612–616`), leaving the **most recent** poll result, not a six-form history.

The per-form status word is `0x400_status | (xr_variant << 28)` (`:720`); decoding it requires separating the high variant nibble before treating the lower bits as engine status. `[60]` is the retained **form value** (`0`, `4`, `0x0B`, `7`, `1`, `2`; `0xFF` after all failed) and `[61]` is the number tried. Those fields identify the selected *software branch*, not successful command consumption under all future workloads.

<a id="c9-section-257"></a>

#### 257. Two confirmed VXD3DMON reporting ambiguities

**Disabled reason 5 prints `?`.** `vx_native3d_vxd.c:604–617,637,685–686` explicitly records `xr_off(5)` when the GTI/SR6F translation-cache flush fails. But `vx_d3d_mon.c:32–36` has only a five-entry `why[]` array indexed 0–4 and prints `?` whenever `x[1] >= 5`. Thus a real **reason 5 is displayed as unknown** rather than “GTI cache flush timed out.” This is a confirmed **diagnostic program text bug**, not proof that the flush actually timed out.

**`xr_stat[10]` and `[11]` have mode-dependent meanings.** In `xr_flush()` (`vx_native3d_vxd.c:568–574`), `[11]` stores CLFLUSH/WBINVD mode plus cache-line size and `[10]` counts flushed lines or WBINVD calls. The MOVNTI staging path in `xr_kick()` (`:1309–1311`) subsequently stores **the sentinel `5`** in `[11]` and increments `[10]` by **DWORDs copied**, including any earlier CLFLUSH contributions. `vx_d3d_mon.c:37` recognizes this sentinel, but reporting `[10]` as one comparable unit across modes or across a startup-to-game transition would be incorrect. This is a measurement-interpretation issue; it is not itself evidence of corruption or a GPU hang.

<a id="c9-section-258"></a>

#### 258. Retirement uses two independent completion signals, with a bounded progress extension

`xr_wait()` (`vx_native3d_vxd.c:653–669`) classifies the engine quiet state through `0x400` and (in the paused-busy case) `0x444[31]`. If a **framebuffer fence is enabled**, it also requires `fx_fence_read()==(fx_seq & 0xFFFF)`, and wants this condition on **two successive polls**. The ring-progress timing option (`ACCEL_D3D_RINGPROG`, default on) checks progress in `0x43C` *or* the fence every 100 ms and can extend the wait, subject to a **2,000 ms hard ceiling**. The progress probe here calls `xr_rp()` and therefore also lacks a propagated stable-read flag; a changing but unreliable readback can appear to be progress until the hard ceiling. The sampled duration is bucketed into `vx3dn_rp_hist[0..5]`, exposed by TEXDBG `0xBC:BA`.

A late post-wrap fence has a specific retry: `xr_wrapgrp && !rk && q>0 && fx_rfenced && elapsed>=3ms` republishes the pause at `xr_pend-0x10` **once**, and counts this under `[69]`; later `[68]`, `[70]`, `[71]` classify post-wrap completion/no completion. Do not interpret a republish counter alone as an actual permanent hang. The code's deadline bounds CPU waiting, **not a guarantee that a stuck GPU can be recovered without reset**.

<a id="c9-section-259"></a>

#### 259. Recommended non-destructive interpretation of a VXD3DMON capture

**Archive update:** Two qualifying VXD3DMON v5.4 hardware logs have now been recovered and analyzed in §§261–270. The examples in this section were prospective before that recovery; their logged values and software version must now be checked against the actual archived captures rather than assuming a test is still missing.


| Observation in the existing log | Source-supported interpretation | What it does *not* establish |
|---|---|---|
| Ring state OFF; reason 1/2/3 | Missing/invalid table offset, allocation or PTE eligibility | An AGP packet was ever fetched |
| Ring state OFF; reason **5** (may print `?`) | GTI flush timeout in `xr_srs()` | The startup pause mode was tested |
| Reason 4; six forms attempted; `[60]=FF` | No candidate passed the startup quiet-plus-in-range predicate | Which register or PTE field caused it without per-form telemetry |
| `[21]` in `0x100000..0x140000`, but not `0x1000F0` | The startup software predicate can accept it | Proof that the FE02 startup block was consumed |
| `[21]=0x1000F0`, `0x444[31]` paused | Consistent with reaching the expected initial pause | Proof that later draw packets/fences work |
| `[13]` rises with `[14/15,27–29]` captured | Retire failed after an attempted fetch | Pure ring issue versus 2D/fence issue without the complete status and trace |
| `[6]` wraps, `[68/69/70/71]` and `0xBC:95` | Distinguishes post-wrap retries and failed retirements | Source of the hang without timing and packet context |
| `0xBC:B9/BA` waits over 100 ms | Retire latency and whether movement extended waits | Guaranteed genuine progress if `0x43C` reads were unstable |

**Suggested capture:** preserve an existing `VXD3DMON` report containing the full `0xBC:86–91` array, `0xBC:95`, `0xBC:B9/BA`, and the first-hang trace before clearing counters or changing ring modes. Also record driver version, enabled `ACCEL_D3D_XPRING`, `ACCEL_D3D_XPRING_NT`, `ACCEL_D3D_RINGPROG`, resolution/bpp and which program was running. Do not forcibly activate a ring, change GART entries or rely on prior logs from a different driver build.

<a id="c9-section-260"></a>

#### 260. Verification results, decision boundary and next work

**Resolution of the earlier missing-log statement:** The file library subsequently yielded two distinct user-owned ring-monitor logs. Their evidence is reviewed in §§261–270; it establishes functioning VX855 ring fetches in those older sessions but not on all earlier or later builds.


`Chrome9_Win98_v10479_XR_Telemetry_Audit_252_260.py` is a **read-only hash-pinned audit** that extracts the three actual source files from the original v10.479 ZIP, checks the CE retail SHA, verifies the selector layout and the key branches, and runs model checks for startup bounds and the monitor's reason-5 display. The checks assert the *current source contents*, not any hardware results. The original `.ZIP`, CE binary, and Windows 98 driver remain unchanged.

**What can be concluded now:** enough ring telemetry already exists to distinguish **early setup/flush failure**, **startup predicate weakness**, **post-wrap overtaking**, and **retire/fence stalls**. The main remaining unknown is the real board's captured values during a reproducible failing workload. A future source version or an actual `VXD3DMON` log can be compared with this evidence baseline; without that, asserting a specific cause for the earlier `0x43C=0` observation would still be speculation.

### VX855 v10.441 hardware captures and performance

<a id="c9-section-261"></a>

#### 261. Archived Windows 98 VXD3DMON captures

**This resolves the central uncertainty left in §§251–260 for these particular historical runs.** The user's persistent file library contains `VXD3DMON.TXT` (SHA-256 `e33bf60158217e00d373527f0be9fd13756e8e85a49c09417c3cae5d61e9e071`) and `Vxd3dmon1.txt` (SHA-256 `1dbefdc38cd6ffc2d98217da0716befababc54e257fe86b192007e1d4d79c666`). A further `Vxd3dmon1(1).txt` is an exact byte-for-byte duplicate of `Vxd3dmon1.txt`, so it is **not** counted as an independent test. The user-supplied **Win98 v10.479 source ZIP** remains the source-level comparator, SHA-256 `9d7790fc85f488321a996ee9e070c2e75c69dde37ecaa75d9de220b574b24029`.

**Important build qualification:** Both recovered logs identify themselves as **`VXD3DMON v5.4`**, with **`HAL build v10.441`**, whereas the source used to audit later ring features is **v10.479**. The logs are valid hardware-observation evidence for their captured builds, **not proof that the v10.479 binary or any later release was tested**. Counter names that persist in the later source may be compared cautiously; never infer that every later optimization was enabled or that the VxD binary version is established merely by the reported HAL build. The first line, `Direct3D off`, is a **start-of-monitor snapshot**; both logs subsequently report *Direct3D on at the end*. Their sampled gameplay metrics should therefore not be dismissed on the basis of the startup header.

The file-independent parser is `Chrome9_VXD3DMON_Log_Analysis_261_270.py`; the per-capture results are in `Chrome9_Real_Ring_Log_Comparison_261_270.csv`. It does not run the VxD or access hardware.

<a id="c9-section-262"></a>

#### 262. The CE-style AGP command fetcher was operational in both archived VX855 runs

| Observed quantity | `VXD3DMON.TXT` | `Vxd3dmon1.txt` |
|---|---:|---:|
| Ring starts | **1** | **1** |
| Command groups kicked | **107,771** | **80,585** |
| Ring wraps | **4,403** | **3,328** |
| Fences submitted | **107,771** | **80,585** |
| Ring hangs reported | **0** | **0** |
| Fetcher packets | **245,607** | **180,312** |
| Fetcher packets not finished | **0** | **0** |
| Packet submissions through the register fallback | **0** | **0** |
| First-start candidate forms tried | **1** | **1** |
| Retained candidate | **form 0**, XP-style | **form 0**, XP-style |

These are **recorded counters**, not a laboratory oscilloscope trace. Nevertheless, the convergence of tens of thousands of successful kicks, thousands of wraps, submitted fences, no reported incomplete fetcher packets, actual game-frame output, and ring still running is **strong evidence that this particular Win98 VX855 ring configuration did consume and execute command work**. The earlier `0x43C=0` observations came from other/earlier experiments; they cannot be generalized to these working captures. Nor do these two VX855 runs prove operation on **VX800 or VX900**.

<a id="c9-section-263"></a>

#### 263. Successful startup readback is 16 bytes beyond the pause address

Both captures record precisely the same initial state:

```text
MMIO 0x400 (engine status)  = 00000000
MMIO 0x43C (AGP current)   = 00100100
MMIO 0x444 (AGP status)    = C2000000
MMIO 0x440 (parameter)     = 001000C0
MMIO 0x45C (pause readback) = 001000F0
MMIO 0x464 (start readback) = 00100001  (location bit 0 included)
```

For the v10.479-style 256-KiB ring, the software startup packet's expected pause boundary is `XR_GPU + 256 - 16 = 0x001000F0`. The **measured** successful current-address readback is `0x00100100`, exactly **`+0x10`**. The separate `0x45C` readback equals the expected pause address; `0x444[31]` is set in `C2000000`, consistent with a paused fetcher. This coherent, independently repeated tuple was **not available** when §248 recommended requiring `0x43C` to equal `0x1000F0`.

**Erratum to §§248 and 259:** exact equality `0x43C == pause` is **not** a suitable unqualified startup-validation rule; it would reject these observed successful runs. A better *future validation design* is to require stable/reasonably consistent readback **relative to the documented programmed pause, start and status**, and to confirm actual completed work. The `+16` is an **observed offset in these captures**, not a proven universal register convention for every chip or engine mode. The original v10.479 predicate's overbroad inclusive range and discarded stable-read flag remain *source-level weaknesses*, but there is now an actual successful hardware baseline against which to judge them.

<a id="c9-section-264"></a>

#### 264. Translation and startup modes are corroborated by actual register samples

Both captures report **GART table at VRAM offset `0x03190000`**, a first-start sequencer state of `SR6A=0x90`, `SR6B=0x31`, `SR6C=0x00`, `SR6F=0x00` after setup, and **one startup candidate** accepted. `SR6A/SR6B` encode the stated table offset according to the VIA sequencer manual, `SR6F[7]` is clear at the logged sample, and the retained form is **`0`** (PTE bit 24 set; SR6C bit 7 clear) according to the log's accompanying description. This directly supports the **actual observed** device address-space and GART startup interpretation of §§242–247, for this Win98 build family.

Neither log reports an SR6F cache-flush timeout, six unsuccessful forms, or an initialization failure reason. The old build's precise internal failure-handling paths cannot be reconstructed solely from its successful summary, but the startup counters strongly contradict the hypothesis that *all* VX855 Win98 rings failed to initialize.

<a id="c9-section-265"></a>

#### 265. Ring wrap handling worked; re-published kicks were recoverable, not recorded hangs

| Post-wrap quantity | Capture A | Capture B |
|---|---:|---:|
| Wraps written without a preliminary wait | **4,403** | **3,328** |
| Ring-space checks after wraps | 10,406 | 8,226 |
| Space checks that needed an engine wait | **20** | **4** |
| Share of these space checks requiring a wait | **0.192%** | **0.049%** |
| Observed post-wrap group completions *without* republish | 509 | 428 |
| Observed post-wrap group completions *after* republish | **170** | **152** |
| Observed post-wrap groups never completed | **0** | **0** |

The monitor's historic line **`wraps that waited for the engine`** is misleading in isolation: its displayed `vx3dn_ring_stat[2]` increments on **each `xr_wrap()`**, while the more specific `wraps written without a wait` counter equals the total wraps. The **20** and **4** space-check waits are the measurements that actually indicate deferred space reuse in these runs. Do not report all 4,403 or 3,328 wraps as engine stalls. The 170/152 pause re-publications are **one-time re-issues**, not ring hangs; all post-wrap cases in those counters ultimately completed.

Only a subset of wraps produce one of these final-group outcome records, because `xr_wrapgrp` is a flag consumed during retirement and many kicks occur asynchronously before a retirement. Thus **do not divide 170 by 4,403 and label the result the percentage of faulty wraps**; the denominators describe different sets of events. Even the 170/(509+170) ratio only describes the *recorded post-wrap retire outcomes*, not independent GPU wrap incidents.

<a id="c9-section-266"></a>

#### 266. Severe gameplay slowdown appears predominantly CPU-side *despite* the working ring

These captures have poor gameplay FPS even while the queue executes substantial work:

| Measured performance | Capture A | Capture B |
|---|---:|---:|
| Gameplay median FPS | **7.3** | **8.3** |
| Gameplay 10th-percentile FPS | **5.0** | **5.3** |
| Slow gameplay frame, average (`>=75ms`) | **157.8 ms** | **149.7 ms** |
| GPU engine busy during those slow frames (sampler) | **24%** | **23%** |
| Render thread in VxD, *not explicitly waiting* on engine | **96.5 ms/frame** | **82.6 ms/frame** |
| Render thread in `VXDDRAW.DLL` | **20.1 ms/frame** | **17.1 ms/frame** |
| Render thread in VxD waiting for engine | **0.1 ms/frame** | **0.1 ms/frame** |

These results make it **unlikely that the command ring is the primary reason for the 7–8 FPS gameplay in these specific runs**. The bulk of sampled frame time is in the CPU's VxD or HAL/rendering pipeline rather than blocked waiting for GPU fetcher availability. Source-level packet transformation, state compilation, vertex processing, memory copy/validation and interrupt-off intervals are stronger candidates for further optimization than changing the already-working ring setup. **Do not extrapolate the exact percentages to Quake 2, Lattice, GTA3 or a later driver without matching workload evidence**. Also note the sampler reports substantial late intervals with interrupts off (~17.5 s and ~16.5 s over the monitoring window), so its percentages are estimates and cannot be treated as cycle-exact attribution.

<a id="c9-section-267"></a>

#### 267. Per-packet profiling points to CPU command generation more than register kickoff cost

| Timing or workload metric | Capture A | Capture B |
|---|---:|---:|
| 3D fetcher packets | 245,607 | 180,312 |
| Average command words/packet | 1,067 | 1,100 |
| Fetcher packet word-building time | **28.5 μs** | **25.1 μs** |
| Total fetcher processing time/packet | **56.2 μs** | **52.4 μs** |
| Ring opening groups/packet | 2.1 μs | 1.6 μs |
| MOVNTI/copy/cache flush contribution/packet | 4.0 μs | 3.9 μs |
| Kick MMIO writes/packet | **0.2 μs** | **0.2 μs** |
| Recorder bookkeeping/packet | 3.1 μs | 3.2 μs |
| Ring retire waits/packet (averaged across packets) | 6.0 μs | 7.9 μs |

The VxD packet-building stage alone consumes orders of magnitude more time per packet than the **0.2 μs** sampled kick-register write time. This suggests focusing on the *hottest CPU-side command-generation and geometry stages* rather than assuming `0x43C` pointer polling or ring writes account for most slowdown. A 4.0 μs copy and 3.1 μs recorder are not zero; but optimizing only the MMIO kick would have a very small total payoff in these captures. The logs contain no controlled A/B experiment disabling the recorder or ring and therefore do **not** quantify a speedup from any proposed change.

<a id="c9-section-268"></a>

#### 268. Version compatibility and measurement cautions

These are **VXD3DMON v5.4, HAL build v10.441** logs. Sections 242–260 analyze a **later v10.479 source ZIP**. Important ring constants, field names and decode patterns match the recorded values, but the v10.479 source contains additional changes such as improved read-pointer reliability checking and other subsequent telemetry. We therefore distinguish:

- **Observed hardware/session facts:** register tuple, successful startup candidate, number of groups/wraps, zero recorded hangs, recorded FPS and sampling categories — directly taken from the two historical logs.
- **Source corroboration:** ring start `0x100000`, expected pause `0x1000F0`, Type-10h pause/trigger semantics, GART mapping and diagnostic field layouts — supported by v10.479 and earlier reviewed VIA CE/XP references but not a rebuild of the logged binary.
- **Hypotheses requiring new evidence:** exactly why the GPU current-address readback is pause+16, whether every CPU-side function is still expensive in later versions, and whether alternative fetcher variants would improve either workload.

Both logs explicitly say `HAL build v10.441`, **not** v10.479; the VxD binary's exact version is not independently established. Because the two log files are not the same monitoring session, performance differences of 7.3 versus 8.3 FPS are **not** a controlled driver optimization experiment. The second filename with `(1)` is a duplicate; do not mistake it for a third run.

<a id="c9-section-269"></a>

#### 269. Interpreting a zero 0x43C readback

The prior driver project's isolated readback `0x43C=0` observations remain real **for their original experimental contexts**. They no longer justify a broad claim that VX855 cannot consume ring commands or return progress: these later archived runs produced a reproducible nonzero, in-range current-address report and very large successful ring workloads. Three cases should remain distinct:

1. **Older probe:** `0x43C=0` with its original registry/mapping/ring state not demonstrated. **Do not infer** the ring was operating in the v10.441/v10.479 mode.
2. **Working archived Win98 mode:** GPU ring starts at `0x100000`; current-address readback starts at `0x100100`, the pause is at `0x1000F0`, status `0x444=C2000000`, and later groups execute without a recorded ring hang.
3. **CE reference:** zero is the CE's configured device ring start; compare addresses only after translating CPU backing and ring location mode (§§224–249).

This is a **resolved source/version/context discrepancy**, not proof that any chipset has an unreliable register in all modes. The readback reliability warning in VIA's Linux code still justifies sampling more than once while the command fetcher is active.

<a id="c9-section-270"></a>

#### 270. Reproducibility, conclusions and next targeted audit

`Chrome9_VXD3DMON_Log_Analysis_261_270.py` parses the **original archived text logs** without invoking Win98 or hardware; it exports `Chrome9_Real_Ring_Log_Comparison_261_270.csv`. Its assertions verify startup `43C−45C=16`, form-0 acceptance, matching fence/kick totals, matching two wrap counters, zero reported hangs/unfinished packets and the slower-frame busy fractions. All checks passed. All three supplied log copies were hashed; **only two unique byte sequences** were found. The updated master Markdown keeps this new evidence separate from the v10.479 source-only conclusions.

**Updated conclusion for these captured workloads:** VIA's VX855 ring fetcher and queue-wrap path were *substantially functional*. Poor gameplay was accompanied by **CPU-heavy rendering submission and relatively low sampled engine utilization**, not a recorded queue-fetch hang. The next most valuable code audit is the **CPU-side transform/lighting/clip/emit and state/texture setup** in the measured older build, followed by comparison against the latest driver source when available. Any proposed startup-validation change must preserve the **observed pause+16 current-address behavior**, should not rely solely on one in-range read, and should be tested against a completed benign workload rather than only a stationary register value. No driver, hardware or registry configuration was modified in this pass.

<a id="video-hqv"></a>

## Video overlay, HQV and decoder

The overlay summaries share 64 offsets. VX900 adds documented V1 scaling and V1/V3 guard controls. Use the dedicated HQV chapter for the first-bank decoder handshake where CE initialization corroborates it, and keep the second bank and other modes qualified.

### Video registers and HQV maps

<a id="c9-section-10"></a>

#### 10. DMA, video overlay and HQV: avoid premature sharing

All three Part I manuals document multi-channel DMA controller programming, and all three Part II manuals document video playback/capture and high-quality video subsystems. However, the chapter boundaries and register summaries differ (for example, VX855 divides video capture/HQV ranges differently from the VX800 and VX900 volumes). [V8-I, DMA chapter; V8-II, pp. 7–56]; [V855-I, DMA chapter; V855-II, pp. 11–61]; [V9-I, DMA chapter; V9-II, pp. 9–74].

**Recommendation:** Start with **CPU-submitted 2D and 3D commands** on new hardware. Defer new command rings, DMA upload tricks, HQV and advanced overlay behavior until basic register mapping, fences and mode switching have passed. Keep DirectDraw caps conservative; selectively enable overlay formats, keys, scaling and flips as the actual tests pass.

**For the slow GTA intro/video path:** Profile its actual API calls and presentation mechanism. Smooth MPEG playback in another player does not alone prove the GTA path should use the same engine, format conversion or overlay interface. Trace surface creation, blits, color conversion, lock/unlock and waits; avoid assuming the issue is codec decoding or GPU processing without measurements.

<a id="c9-section-30"></a>

#### 30. Video and HQV: VX800 differs from the VX855/VX900 pair

OpenChrome's video capability structure groups all three as supporting its three-HQV-buffer and byte-based HQV-fetch logic, with two-color-key support and new-scale control [OpenChrome `src/via_xv_overlay.c:L320–L340`]. But VX855 and VX900 select a different `hqv_cme_regs_409` table than VX800. This is direct evidence that **HQV programming is not identical despite common feature names**.

In the overlay setup, VX855/VX900 have additional destination-data-offset, background-data-offset, extended-control and subpicture scale initialization. VX900 alone initializes a sharpness/decoder handshake register [OpenChrome `src/via_xv_overlay.c:L2118–L2148`].

OpenChrome's Xv initialization can enable PCI DMA transfers on these chipsets when the Linux DRM version/feature conditions are met [OpenChrome `src/via_xv.c:L638–L661`]. This is a **video data transfer** capability; it is not evidence that the 3D ring is operational or that Windows 98 DirectDraw overlay alpha/auto-flip is already implemented.

For the VX855 project's video path, OpenChrome may offer reference values for HQV features, but mappings and initialization sequences should be checked specifically against the HCM manual. The Windows 98 native overlay API and Xv are not identical interfaces.

<a id="c9-section-337"></a>

#### 337. Three-family display/video/HQV audit: scope, sources, and classification

**Objective:** establish which VX800 HC3, VX855 HCM, and VX900 HD video-overlay, display, and HQV register *meanings* can be shared, which require modification, and which remain undocumented. This continues the prioritized family-wide study after the texture/sampler audit (§§327–336); it is **not** a Windows 98 performance investigation.

**Original evidence:** VIA Part II manuals `vx800_2.pdf` (November 2008), `vx855_2.pdf` (July 2009), `vx900_2.pdf` (January 2011), plus the original VIA CE retail `DDI_VIA.dll`/`DDI_VIA.map` (DLL SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`). The executable, read-only `Chrome9_Display_Video_HQV_Three_Family_Audit_337_350.py` pins the three source-PDF SHA-256 values and DLL SHA, regenerates text from the PDFs, checks compiled function identities/initializers, and writes `Chrome9_Display_Video_HQV_Three_Family_Matrix_337_350.csv` with **41 selected register features**. Original PDF pages 60, 61, and 63 of VX900 Part II were also visually checked, because table headings and overlapping descriptions matter here.

**Classification:** shared manual label/offset is weaker than identical bitfield behavior; shared linker-map address proves a common **compiled function**, not equal video output or support for every possible resolution/pixel format. Manual entries labeled `Reserved` are **not proof of absent silicon functionality**. The driver binary is an independent implementation source; no GPU/video capture/video playback was executed in this pass.

<a id="c9-section-338"></a>

#### 338. Video-overlay *address topology* is remarkably stable: 64 of 64 offsets match

The original video-display register **summary tables** list **exactly the same 64 first video-aperture starting offsets** for VX800, VX855, and VX900 (VIA `0x200–0x2FF`, including V1, V3, color-key, alpha-window, hardware-cursor, flip and HI controls). Of these 64, **60 have identical register summary labels** across all three. This was established by extraction and direct address/name comparison, not inferred from shared chipset families. Relevant original PDF table pages: VX800 Part II p.7–8, VX855 Part II p.11–12, VX900 Part II p.9–10.

High-value shared addresses include `0x200` interrupt flags/masks, `0x204` flip status, `0x220` primary color key, `0x228/0x22C` chroma-key bounds, `0x230` V1 control, `0x24C/0x250` V1 zoom/minification, `0x2A0` V3 control, and `0x2C0` V3 minification/interpolation. Thus a driver can retain much of its **address map**; it must still select masks and derived fields by chipset. No claim is made that the entire 64-register set has identical bits or timing behavior.

<a id="c9-section-339"></a>

#### 339. The four overlay-summary label deviations are concentrated in VX855

The four *summary-label* deviations from §338 are:

| Video register | VX800 | VX855 | VX900 |
|---|---|---|---|
| `0x28C` | P Signature Adder Result 1 | **Reserved** | P Signature Adder Result 1 |
| `0x290` | Alpha Window/Color Cursor Ending Position | Alpha Window/**HI** Ending Position | Alpha Window/Color Cursor Ending Position |
| `0x2CC` | T Signature Adder Result 1 | **Reserved** | T Signature Adder Result 1 |
| `0x2E4` | T Signature Data Result 1 | **Reserved** | T Signature Data Result 1 |

This is a **nonmonotonic documentary difference**: the older VX800 and newer VX900 summaries agree on three signature-register names while VX855 reserves them. It would be unjustified to infer that VX855 hardware necessarily lacks those facilities, or that VX900 restoration is silicon-proven, without examining the detailed bitfields/actual usage. The CSV carries all three source labels verbatim.

<a id="c9-section-340"></a>

#### 340. VX900 extends Video Stream 1 `0x230` with a new bit 1

All three detailed register descriptions give V1 Stream Data Format at `0x230[4:2]`, V1 enable at bit 0, and familiar V1 flip, deinterlace, gamma/CSC controls. **VX800 and VX855 reserve bit 1**, whereas **VX900 defines `0x230[1]` as “V1 Scaling Enable on IGA1”** (`0=disable`, `1=enable`). The same overlay-command word cannot necessarily be copied from a VX900 configuration into an older V1 controller with the expectation that bit 1 has the same purpose. Source: VX900 Part II **PDF p.21** detailed Video Stream 1 Control, compared with VX800 and VX855 detailed `0x233–0x230` sections.

The larger V1 format enum remains documented as the same `000 YUV422`, `001 RGB32`, `010 RGB15`, `011 RGB16`, `100 YUV411`, `101 RGB10` group across the three chapters. This establishes **documented packet/bit compatibility in that subset** only; it does not compare the video engine's actual filtering result.

<a id="c9-section-341"></a>

#### 341. VX900 adds guard controls to the V1 and V3 second-buffer address registers

In the detailed `0x23B–0x238` V1 second buffer-address description, VX800 and VX855 designate **bit 31 reserved**; VX900 assigns **bit 31 “V1 Length Internal Guard Bit”**, `0=old design`/`1=new design` (VX900 Part II **PDF p.22**). All manuals retain the framebuffer location selection in bits `[30:29]` and an address field in `[28:3]`; VX855/VX900 additionally document L.L. `11b`, whereas VX800 calls `1x` reserved.

VX900 also defines **`0x2A8[31]` as the V3 Length Internal Guard Bit**, with `0` selecting the old design and `1` the new design. Both older manuals reserve that bit. The V3 field is in the second V3 framebuffer starting-address register, which retains location bits `[30:29]` and address bits `[28:3]`. VX800 reserves location `11b`, while VX855 and VX900 document L.L. See VX800 and VX855 Part II PDF p. 28 and VX900 Part II PDF p. 31.

**Source caution:** the VX900 detailed table also labels `[30:26]` RO Reserved while separately defining `[30:29]` RW for memory location; the older VX800 table has a similarly overlapping broad reserved range. **Those overlapping claims cannot both be literal.** Prefer the specific `[30:29]` field definition for documentation comparison, and treat contradictory broad reserved range labels as manual errors until corroborated. Do not infer bit 31's electrical effect from the names “old/new design.”

<a id="c9-section-342"></a>

#### 342. HQV changes more substantially than the V1/V3 overlay register topology

The **VX800 first-bank HQV summary** starts at `0x380` with “HQV Source Data Offset Control 1–4”, then tuning, color adjustment, scaling and deblocking controls. The **VX855 HQV summary** starts at `0x360` with four color-adjustment controls, introduces **separate source crop** controls `0x370–0x37C`, and describes `0x380–0x38C` as **destination video/subpicture crop**. It further assigns `0x3E0` to subpicture horizontal scale and `0x3E8` to subpicture vertical scale, rather than using the old source-line/fetch-count label at `0x3E0`. See VX800 Part II **PDF p.11**, VX855 Part II **PDF p.48**, and their detailed HQV chapters.

This is the most consequential three-way HQV *register-meaning* boundary: a compatible **offset number** is not enough. A VX800 `0x3E0` word intended as source fetch-count settings should **not** be used verbatim as VX855 subpicture-scaling control.

<a id="c9-section-343"></a>

#### 343. VX900's video chapter repeats VX800's entire old HQV first-bank summary

The VX900 manual contains **two separate HQV chapter descriptions**, not just one register map:

* Its earlier **Video Registers** summary (VX900 Part II **PDF p.13**, detailed old HQV pages around **p.50–60**) reproduces **all 30** VX800 first-bank HQV labels **exactly** for the corresponding 30 offsets. It lists old `0x380` “Source Data Offset Control 1”, `0x3A4` “Color Adjustment Control 2”, `0x3BC` “De-blocking Factor”, and `0x3E0` “Source Data Line Count and Fetch Count Per Line.”
* Its separate later **HQV Registers** chapter (VX900 Part II **PDF p.61–63 onward**) uses the more modern VX855-style crop/scale layout and introduces new or reorganized sharpness, decoder handshake, and macro-valid-table functions.

This is an **internal contradiction in a single original PDF** and a plausible copy-forward of the VX800 register chapter. Exact 30/30 table-name identity supports the copy-forward hypothesis, but **does not prove** which VX900 bitfields are correct on the silicon. The newer HQV chapter is a **candidate intended map**, not automatically an independently verified hardware truth. Preserve both in a compatibility matrix rather than silently replacing one.

<a id="c9-section-344"></a>

#### 344. VX900 `0x380–0x38C` destination/source naming conflict is partly clarified by detailed descriptions

At VX900 `0x380`, the **older video chapter names** the register “HQV Source Data Offset Control 1,” but its **bitfield description itself already says “Horizontal Offset of Start Point for Video Location In Destination Picture.”** The newer HQV chapter explicitly names `0x380` “HQV Video Start Point Offset Control in Destination” with the same destination-picture interpretation (VX900 Part II PDF **p.50** vs **p.64**).

Thus the old **label** is misleading even within its own description. The detailed destination coordinate interpretation is a better **manual-text-supported** model for the shared offset than the early high-level “source” label. Separate `0x370–0x37C` source-coordinate registers are explicitly present in the VX855 and newer VX900 HQV chapters. The actual transform/clip behavior remains unmeasured.

<a id="c9-section-345"></a>

#### 345. VX900's `0x3A4–0x3AC` decoder/valid-table fields conflict internally

The same VX900 original PDF gives three interpretations of `0x3A4–0x3AC`, and the **first and second HQV banks are not even consistently labeled**:

| Offset | VX800 video/HQV | VX855 HQV | VX900 early video HQV | VX900 later **first** HQV | VX900 later **second** HQV |
|---|---|---|---|---|---|
| `0x3A4` / `0x13A4` | Color Adjustment 2 | Color Adjustment 6 | Color Adjustment 2 | **Sharpness and Decoder Handshake** | **Sharpness** |
| `0x3A8` / `0x13A8` | Color Adjustment 3 | Color Adjustment 7 | Color Adjustment 3 | **SW Macro Set Valid Table 0** | **Decoder Handshake** |
| `0x3AC` / `0x13AC` | Color Adjustment 4 | Color Adjustment 8 | Color Adjustment 4 | **SW Macro Set Valid Table 1** | **Reserved** |

The VX900 **detailed first-bank** register section for `0x3A4` describes decoder waiting and frame-valid-table handshaking (`[26]` enable, `[25:20]` Source Frame ID, `[19]` timeout enable, `[18:13]` timeout cycle); `0x3A8`/`0x3AC` are explicitly 32-bit SW valid-table halves. This is a **concrete documented VX900 video-processing feature** not mirrored by the older HQV summary. The second-bank summary's different labels mean adding `0x1000` is **not enough to prove bitfield equivalence** for all fields: the bank may have intentional function differences, or that table may be another documentation defect. Both alternatives remain unresolved.

<a id="c9-section-346"></a>

#### 346. Do not transplant VX800 `0x3BC` or `0x3E0` words into newer HQV programs

Three additional repurposing hazards appear in the HQV summaries:

* **`0x3BC`**: VX800 and VX900's *early video chapter* label “HQV De-blocking Factor”; VX855 and VX900's *later HQV chapter* label **Segment Residue Pixel Frame Buffer Starting Address**.
* **`0x3E0`**: VX800 and VX900 early chapter label **Source Data Line Count and Fetch Count Per Line**; VX855 and VX900 later chapter label **Sub-picture Horizontal Scale Control**.
* **`0x3E8`**: absent from the old VX800 first-bank HQV summary and the early VX900 HQV summary, but VX855 and later VX900 name **Sub-picture Vertical Scale Control**.

These are **documented semantic differences**, not merely preferred symbolic names for identical bits. A common “HQV scale” function must be adapted to the target family register map. No actual content was written to those MMIO registers in this research pass.

<a id="c9-section-347"></a>

#### 347. Common video and deinterlace controls remain plausible compatibility anchors

Among the HQV fields, all three manuals document **`0x3D0` stream control/status**, **`0x3D4/0x3D8` luma/chroma source-address functions**, **`0x3DC` tile/address/CSC/deblocking**, **`0x3E4` motion-adaptive deinterlace**, **`0x3EC/0x3F0` destination addresses**, **`0x3F4` destination stride**, and **`0x3F8` source stride**. The older VX800 and VX900 video chapters use matching video stream-format enumerations around `0x3D0[31:28]`, including RGB, YUV422 and NV12-style YUV420. These are **candidates for shared data representation**, not proof of identical scaling, CSC coefficients, deinterlace thresholds, or pixel output. Reconfirm detailed field ranges before sharing an emitter across the old/modern HQV layouts.

<a id="c9-section-348"></a>

#### 348. Important aperture correction: `+0x1000` and `+0x2000` describe different mechanisms

The VX900 PDF has two **different** additional-address mechanisms:

1. **Second HQV engine bank**: `original HQV offset + 0x1000`, e.g. `0x3D0 -> 0x13D0` (documented at original VX900 Part II PDF **p.60** and **p.63**). VX800 and VX855 also document secondary engine HQV registers in the `0x13xx` region.
2. **Command-regulator-routed write aperture**: `original HQV offset + 0x2000`, with a write passing through the command regulator before the video display engine; the description says reads retain the original action (VX900 Part II PDF **p.62** note 2; similar aperture notes in the older manuals).

**These are not mutually exclusive addresses and are not, by themselves, a manual contradiction.** The actual VX900 internal conflict is over **register meanings within the duplicated HQV chapter** (§§343–346). This corrects the preliminary shorthand that called the two offsets contradictory.

<a id="c9-section-349"></a>

#### 349. Original CE binary validates family-level reuse *and* actual VX900 overlay specialization

The CE 6.0 retail linker map and x86 initializer disassembly give the following build-specific results. CE 7 installs distinct show, flip, fire and wait entry points for all three families (see §368).

| Function or operation | VX800 vs VX855 | VX900 implementation |
|---|---|---|
| `HQVFlipOverlaySurface_*_PCI` | **Exact same compiled address `0x10081AB1`** | Different routine `0x1008260B` |
| `HQVFlipOverlaySurface_*_AGP` | **Exact same compiled address `0x10081C6C`** | Different routine `0x1008281E` |
| `HQVFlipOverlaySurface` | **Exact same `0x10081E28`** | Different `0x10082A30` |
| `HQVOverlayFire` | **Exact same `0x10081FE2`** | Different `0x10080C26` |
| `HQVWaitFlipStatus` | **Exact same `0x10082028` across all three** | Shared |
| `HQVShowOverlay` | **Distinct** VX800 `0x100802DB` vs VX855 `0x10081A8F` | Distinct `0x100825C7` |
| `InitialHQV` | VX800 `0x100802FD`; VX855 `0x1008205B` | Distinct `0x10082ABE` |

For a fresh initialization, the CE x86 code stores different internal **mode selector** values at HQV state field `+0x0C`: **4 for VX800, 5 for VX855, 6 for VX900**. The older initializers OR `+0x08` with `1`; VX900 ORs **`0x11`**, prepares **six DWORDs** at state `+0x118` with `0x0000FFFF`, and assigns separate show/flip/fire callback addresses while preserving the shared hide and wait callbacks. These are **confirmed software branching/state-setup differences**; `+0x0C` is an internal HQV context field, not a hardware register, and its numerical values must not be interpreted as chipset silicon capability codes. Common aliases prove shared *compiled code*, not that all video surfaces or interpolation cases behave identically on every board.

<a id="c9-section-350"></a>

#### 350. Display, video and HQV evidence boundaries

**Verified source-level conclusions:** all 64 basic video-overlay offsets match; **60/64 summary labels** match; VX900 documents V1 scaling on IGA1 at `0x230[1]` and V1/V3 length guards at `0x238[31]` and `0x2A8[31]`; VX800's older HQV crop/color/scale map differs substantially from VX855's later layout; the VX900 manual includes **all 30** VX800 old HQV labels in its initial video chapter **and** a separate VX855-like HQV chapter with still more decoder and valid-frame controls; and CE overlay setup/flip paths are **not** all shared across generations. The three-pinned-PDF and original-CE-DLL audit passes, and the 41-feature CSV records each family and both VX900 chapter interpretations.

The CE 7 decoder bootstrap corroborates the dedicated first-bank HQV chapter for that path (see §368). **Open questions** include which fields apply in other decoder/HQV modes, whether the two banks deliberately assign different functions to `0x3A4–0x3AC` and `0x13A4–0x13AC`, and whether video/HQV scaling, color conversion, deinterlacing and overlay pixels match across the three devices. Hardware/capture tests are **not** implied by source-label agreement. No driver binary or device was modified.

**Next remaining highest-value objective after this family-wide display/HQV audit:** consolidate the full three-generation compatibility classification into a *single actionable command/register matrix*, with explicit `identical`, `same function/different mask`, `generation-specific`, `manual-conflict`, and `runtime-unverified` statuses, then target only the unresolved contradictions requiring independent silicon evidence. More broad static comparisons without classification are now lower value than this synthesis.

### CE HQV callbacks and CnM decoder bootstrap

<a id="c9-section-368"></a>

#### 368. CE 7 HQV callback separation and first-bank decoder programming

| CE 7 HQV retail callback | VX800 | VX855 | VX900 |
|---|---|---|---|
| Show | `0x100921F7` | `0x100941A1` | `0x10094D75` |
| Flip | `0x10092590` | `0x1009453A` | `0x100951DE` |
| Fire | `0x1009274A` | `0x100946F4` | `0x10095368` |
| Wait | `0x10092790` | `0x1009473A` | `0x100953AE` |

These pointers come from the actual initializer stores at context offsets `+0x138–+0x144`, with map symbols identifying the functions. The initializers retain mode values **4/5/6**. VX900 still ORs context `+0x08` with `0x11` and initializes six DWORDs at `+0x118` to `0xFFFF`. §349 records the CE 6.0 retail addresses.

| CE 7 decoder-path check | Result and binary anchors |
|---|---|
| First-bank VX900 decoder handshake | The `7122h` branch at `0x10015736–0x1001575B` reserves `0x80000` bytes and calls `InitialBootCodeCnM`. At `0x10077861`, `0x10077872` and `0x10077880`, it writes `0x040BE800`, zero and zero to `0x3A4`, `0x3A8` and `0x3AC`. The nonzero word sets frame-done wait and timeout enable when decoded with VX900 Part II p. 66. The decoder-only call context and two zero writes strongly corroborate the dedicated first-bank HQV chapter for this path. The `0x13xx` bank and other modes still require separate evidence. |

<a id="c9-section-91"></a>

#### 91. VX900-only CnM decoder bootstrap and 512 KiB reservation

**Scope and evidence.** This section is an additional, **separate video-decoder subsystem** discovered in the same VIA CE 6.0 retail `DDI_VIA.dll`, not an alternative to the H6/H5S2 triangle / pixel-shader path. All addresses below are preferred image VAs for the SHA-256-verified retail build `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`. The source linker map names `VIA_DXVAMISC:CnMInit.obj`, `InitialBootCodeCnM` (VA `0x10068473`), and `FirmWareData` (VA `0x100A50C0`).

In the `VIA` constructor, the chip-specific branch at `0x1000C4EC–0x1000C516` performs the following only if the device ID equals **`7122h` (VX900)**:

1. Decrease the graphics-memory budget at global `0x100C9F9C` by **`0x80000` bytes = 512 KiB** (`0x1000C4FD`).
2. Store the new budget-relative base in global `0x100CA04C` (`0x1000C50C`).
3. Call **`InitialBootCodeCnM`** (`0x1000C511`), which uses the reserved base and programs decoder-related MMIO.

This is an implementation-level *allocation/reservation* rule for the VX900 CE driver. It does not prove a VX900 hardware requirement for all operating systems, and it must not be mixed with the existing H5S2 3D state tables. Nothing here shows that the Win98 game's 3D vertex or pixel shaders need these firmware commands.

<a id="c9-section-92"></a>

#### 92. Exact decoder memory registers and embedded firmware copy

The end of `InitialBootCodeCnM` contains a concrete, statically verifiable address layout (`0x1006C9AB–0x1006CA23`):

| MMIO offset | Value passed by CE driver | Instruction evidence |
|---|---|---|
| `0x4100` | reserved GPU-memory base | `0x1006C9B0`, direct variant `0x1006C9BE` |
| `0x4104` | base + `0x1F000` | `0x1006C9CC–0x1006C9EE` |
| `0x4108` | base + `0x5F000` | `0x1006C9EE–0x1006CA11` |

The code supports **both** a register/value-pair buffer and direct MMIO writes (`MMIO + 0x4100`, `+0x4104`, `+0x4108`) according to its transport selection. The meaning of the three regions beyond being decoder-address inputs has **not** been recovered from the supplied public 3D manual. The offsets fit within the driver-reserved `0x80000`-byte span, but that alone does not prove their sizes or lifetime.

Next, `0x1006CA11–0x1006CA23` calls imported `_memcpy` (`0x1007F28C`) with:

```
source      = 0x100A50C0 (FirmWareData, VIA_DXVAMISC:CnMInit.obj)
destination = mapped GPU aperture base + reserved memory offset
length      = 0x19000 bytes (102,400 bytes / 100 KiB)
```

SHA-256 of that **exact embedded 0x19000-byte payload** in the retail DLL:

```
15b2394075a92da77e143dd869f3d85246457185459af4c06b930c3ba13086cb
```

The copy operation and symbol `FirmWareData` are unambiguous **static facts**. The payload's internal instruction set has not been decoded, and copying it does not by itself establish that the CE decoder starts successfully on any particular board.

##### Four actual users of the CnM firmware initializer

The routine **`InitialFirmWareCnM`** at `0x1006CA96` is called by the following symbol-identified decoders in the same DLL:

| Caller function / compilation unit | Call VA |
|---|---|
| `ProcessDecoder_VLD_wmv9vc1_CnM` (`VIA_WMV9:wmv9vc1_process_CnM.obj`) | `0x10053601` |
| `ProcessDecoderMPEG2VLD_CnM` (`VIA_MPEG2:Mpeg2Func_VX900.obj`) | `0x1005750E` |
| `ProcessDecoder_VLD_mpeg4_CnM` (`VIA_MPEG4:Mpeg4_Process_CnM.obj`) | `0x1005B0EF` |
| `H264ProcessDecoder_CnM` (`VIA_H264:h264vld_CnM.obj`) | `0x1005D11E` |

Those are real static call edges, **not** a successful hardware decode test or evidence that every compression profile is accelerated. The distinction is significant: a firmware initializer for MPEG/H.264/VC-1 is a video subsystem, not automatically an unrecognized VX900 vertex-shader program.

<a id="c9-section-93"></a>

#### 93. VX900 CnM HQV control and an unresolved manual overlap

Early in `InitialBootCodeCnM`, the retail DLL uses the graphics MMIO base and writes:

| MMIO offset | Initialization value | VA |
|---|---|---|
| `0x3A4` | `0x040BE800` | `0x100684AF` |
| `0x3A8` | zero | `0x100684C0` |
| `0x3AC` | zero | `0x100684CE` |

**Manual conflict:** VX900 Part II, *Video Registers* (printed p. 46 / PDF p. 52–53) describes `0x3A4` as **HQV Color Adjustment Control 2**. Its separate *HQV Registers* chapter (printed p. 60 / PDF p. 66–67) instead calls `0x3A4` **HQV Sharpness Control and Decoder Handshake Control**, `0x3A8` **HQV SW Macro Set Valid Table 0**, and `0x3AC` **HQV SW Macro Set Valid Table 1**. Both descriptions appear in the supplied manual. The overlap is an unresolved documentation/address-context issue; do not silently choose either view for all chipsets and operating modes.

When decoded using the **HQV decoder-handshake** chapter, `0x040BE800` means:

| Bitfield | Decoded value |
|---|---|
| Bit 31, sharpness | disabled |
| Bit 26, wait frame done | **1 / enabled** |
| Bits 25:20, source frame ID | 0 |
| Bit 19, wait timeout | **1 / enabled** |
| Bits 18:13, timeout cycles | 31 units (manual: 500,000 cycles per unit) |
| Bits 12:11, software set/reset valid | `01b` |
| Bits 10:5, valid-table index | 0 |

The two following zero writes would clear both 32-bit macro-valid tables under this interpretation. The correspondence with the binary's decoder boot sequence makes the HQV interpretation plausible, **not independently verified on hardware**. This evidence belongs in the video/HQV platform adapter, not the reusable `HParaType=04h` vertex-attribute encoder.

<a id="c9-section-95"></a>

#### 95. Transport-family limit: `FE0A` is outside the simple 3D packet interpreter

The CnM boot routine has a conditional **buffered** transport path that requests a `0x5140`-byte command allocation (`0x10068508`) and uses a literal `0xFE0A0000` marker (`0x1006850D`). The alternative path writes directly to the appropriate MMIO offsets (`MMIO+0x4000/0x4004` for the decoder boot-data aperture and later register pairs).

This is a **new packet family relative to the previously analyzed `C3DUtils::KickOffCmd_Inv` interpreter**, which was mapped chiefly for `FE00–FE07`. It should **not** be decoded using the ordinary FE00/FE03 word-count rules merely because its top byte starts with `FE`. The CE package has multiple command producers and dispatch contexts. A normalized family-wide decoder should dispatch on **subsystem and exact packet family**, and preserve unknown `FE0A` payloads as opaque until its own processing rules are proven.

<a id="c9-section-97"></a>

#### 97. VX900 CnM boot loader uses 512 indexed words at decoder MMIO `0x4004`

**Source:** `VIA Graphics Driver Group2.msi` → retail `DDI_VIA.dll`, SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`, `InitialBootCodeCnM` at preferred VA `0x10068473`. This section builds on §91–96 and is **decoder-only**; it does *not* describe GPU `HParaType=04h` vertex programming.

Between `0x10068555` and `0x1006C942`, the driver emits the same **512 numbered 32-bit words** through two alternative paths:

- **Buffered CE command path:** append values destined for MMIO `0x4004` as `(offset,value)` pairs following a packet header.
- **Direct MMIO path:** write each same value to `[decoder_MMIO + 0x4004]`.

The first value is prepared separately at `0x100684FC` (buffered) and `0x1006855B` (direct). The next 511 words appear in consecutive numbered branches. Their upper 16 bits progress from `0x0000` through `0x01FF`. This is an **observed numbering pattern**, not a proven hardware microinstruction address encoding.

| Index | Encoded 32-bit word | Observed program role |
|---:|---|---|
| 0 | `0000E40E` | First 32-bit `0x4004` write |
| 1 | `00010020` | Next 32-bit write |
| 2 | `0002E470` | Next 32-bit write |
| 3 | `0003E190` | Next 32-bit write |
| 510 | `01FEE190` | Penultimate numbered write |
| 511 | `01FFE190` | Final numbered write |

**Verified:** All 512 indices have exactly one buffered emission and one direct-MMIO emission with the same literal value. The normalized little-endian 2,048-byte word sequence has SHA-256:

`0e16679a63d3e76265a3cc4268e81680015631d095cbd4b7c0cf1848cbe341dd`

These are *code-emitted decoder bootstrap words*, **distinct** from the separate 100 KiB `FirmWareData` image copied to graphics memory in §92. Their lower 16-bit fields have not been decoded; do not equate them with VIA 3D ALU, TAU or vertex-shader instructions. The accompanying **`Chrome9_CE_VX900_CnM_Bootstrap_512_Words.csv`** records each word and both verified instruction VAs, enabling later firmware-revision comparisons.

<a id="c9-section-98"></a>

#### 98. `FE0A` is a decoder register-pair transport, with its own header and count rule

The **buffered**, VX900 CnM-only branch at `0x10068506–0x10068553` requests `0x5140` bytes, aligns the first packet word to a 16-byte boundary, and initially writes:

```text
FE0A0000    packet family marker
00000000    field subsequently replaced with computed pair count
00F60000    decoder transport field; bit meaning unknown
00000000    fourth header word
00004000    first MMIO offset
00000000    first MMIO value
00004004    decoder command-data port (repeated)
0000E40E    first numbered decoder boot word
...         further (MMIO offset, value) pairs
```

The two transport paths write matching logical register/value operations: in direct mode, the same values go immediately to MMIO offsets `0x4000`, `0x4004`, and subsequent decoder controls. This is why `FE0A` **must not** be decoded as generic Chrome9 3D `FE00–FE07` traffic.

At `0x1006CA58–0x1006CA89` the buffered writer computes a packet count from `(last_written_pointer - aligned_header_pointer)`: it divides the word distance by two **after subtracting the four-word header**. The result is stored at header word 1 (`[header+4]`), and the output is padded to an eight-byte boundary before a buffer-flush call. These are software packet formatting facts; the downstream DMA/packet engine's private interpretation is not proved by static code.

The allocation is a **capacity request**, not proof that all `0x5140` bytes are emitted. Likewise, the literal `00F60000` is **not decoded** as a size, opcode or hardware unit; doing so without consumer-side evidence would be speculative.

<a id="c9-section-99"></a>

#### 99. Decoder bootstrap also zeros 396 DWORD control offsets and sets up execution

Immediately after the 512 decoder bootstrap words, `InitialBootCodeCnM` writes the following controls through the same selected transport:

| MMIO operation | Source VAs | Verified action |
|---|---|---|
| `0x4024 ← 0x00000003` | `0x1006C946` / `0x1006C95D` | Decoder control word |
| `0x4018 ← 0x00000000` | `0x1006C96B` / `0x1006C97E` | Decoder control word |
| `0x4000 ← 0x00000001` | `0x1006C988` / `0x1006C99F` | Decoder control word |
| `0x4100 ← VRAM_reserved_base` | `0x1006C9B0` / `0x1006C9C4` | Base pointer |
| `0x4104 ← base+0x1F000` | `0x1006C9D4` / `0x1006C9E8` | Base-related pointer |
| `0x4108 ← base+0x5F000` | `0x1006C9F7` / `0x1006CA0B` | Base-related pointer |
| `0x5000..0x562C ← 0` | `0x1006CA26–0x1006CA4B` | 396 DWORD offset writes, stride four bytes |

The 396-offset count follows directly from `(0x562C - 0x5000)/4 + 1`. This zeros a **decoder MMIO register range**, not 396 DWORDs of the graphics framebuffer. The meaning of each indexed control is unknown.

The same routine then copies exactly `0x19000` bytes of embedded `FirmWareData` to the reserved GPU aperture (`0x1006CA11–0x1006CA23`). The static evidence therefore shows *two different initialization payload mechanisms*: 512 code-emitted decoder words through `0x4004`, and a 100 KiB data image copied to graphics memory. The image's contents have not been disassembled as a supported instruction set.

Memory **pointers**, not proven allocations: the firmware copy occupies `[base, base+0x19000)`; the next pointer is at `base+0x1F000`, leaving a `0x6000`-byte gap; the later pointer is `base+0x5F000`. The 512 KiB carve-out ends at `base+0x80000`. The actual length and usage of each region requires further decoder research.

<a id="c9-section-100"></a>

#### 100. Separate `InitialFirmWareCnM` control sequence is used by actual video decoders

The `InitialFirmWareCnM` function at `0x1006CA96`, previously identified as called by the H.264, MPEG-2, MPEG-4 and WMV9/VC-1 paths (§92), writes these additional values in both direct-MMIO and buffered-pair forms:

| Decoder offset | Literal value |
|---|---:|
| `0x410C` | `0x00000004` |
| `0x4114` | `0x00000000` |
| `0x4110` | `0x00000004` |
| `0x4170` | `0x00000000` |
| `0x4140` | `0x00000000` |
| `0x4028` | `0x00895440` |
| `0x402C` | `0x00000001` |
| `0x4000` | `0x00000001` |
| `0x4124` | `0x04000000` |
| `0x41BC` | `0x00000100` |
| `0x41B4` | `0x00000020` |

Instruction evidence: `0x1006CAA6–0x1006CC13` in the retail DLL. The function name and decoder call edges identify this as **video-decoder runtime initialization**, not Chrome9 triangle or GPU vertex-shader setup. The public VX900 Part II 3D manual does not define these decoder offsets; their bit meanings remain **unknown**. Avoid reproducing them as an undocumented 3D performance recipe.

<a id="openchrome-review"></a>

## OpenChrome implementation and software pitfalls

OpenChrome establishes selected emitted commands and source-level risks. Its compositor and H6 2D paths cover narrower workloads than a general Win98 game driver.

### Source scope and shared emitters

<a id="c9-section-24"></a>

#### 24. OpenChrome source cross-check — supplied snapshot (November 5, 2026)

**Evidence scope.** This continuation examines the user's uploaded `xf86-video-openchrome-master(4).zip` (the extracted source tree `xf86-video-openchrome-master/src/`) against the six VIA manuals in §2. Source paths and line numbers refer to **this exact ZIP**, not a newer or older upstream revision. It was inspected statically; it was **not built or run on the target hardware**. Line references are written as `src/file.c:Lstart–Lend`. The analysis distinguishes a source-code choice from demonstrated hardware behavior.

**Main revision to the portability assessment:** OpenChrome already implements a *shared H6-class 2D acceleration and XRender compositing path* for **VX800, VX855, and VX900**. This gives unusually direct support for common 2D code, but **not for a universal Direct3D/OpenGL 3D backend**. Its DRI1 client for the same chips is `swrast` (software rasterization). [OpenChrome `src/via_exa.c:L903–L958`; `src/via_dri.c:L529–L541`].

##### 24.1 OpenChrome source map

| File / function | What it shows | Practical significance |
|---|---|---|
| `src/via_regs.h:L46–L95` | Family IDs, GPU PCI IDs, host-controller IDs | Verify chip binding without mixing GPU ID and host-bridge ID |
| `src/via_ums.c:L94–L115` | MMIO mapped from `regions[1]` | Graphics MMIO is BAR1 on all three in this source |
| `src/via_ums.c:L213–L240` | VX900 framebuffer `regions[2]`; other chipsets `regions[0]` | Independent confirmation of VX900 BAR2 framebuffer |
| `src/via_exa.c:L903–L958` | Shared H6 fill/copy/composite callbacks | Direct evidence for shared 2D programming |
| `src/via_exa_h6.c:L140–L265` | H6 fill/copy register words and alignment restrictions | Concrete 2D porting recipe |
| `src/via_3d.c:L275–L385` | Two-triangle textured compositing rectangle | Narrow reusable 3D primitive path, **not** a complete game renderer |
| `src/via_3d.c:L389–L547` | Texture, blend, render-target and clip state emission | Additional evidence of selected common 3D register words |
| `src/via_ums.c:L351–L370` | PCIe virtual queue enable sequence | A real-world comparator for the previous VX855 ring probes |
| `src/via_exa.c:L42–L110` | CPU-buffer flush to direct MMIO | Explains why a command buffer is not necessarily a hardware ring |
| `src/via_exa.c:L465–L512` | Shared late-Chrome9 busy masks and synchronization | More reliable busy/status interpretation |
| `src/via_dri.c:L529–L541` | `swrast` client on all three | OpenChrome cannot validate full 3D-game portability |
| `src/via_display.c:L2829–L2878` | Chip-specific IGA2 FIFO programming | Modesetting is **not** just shared VGA writes |
| `src/via_xv_overlay.c:L320–L340` | HQV capability group differences | VX800 differs from the VX855/VX900 pair |

##### 24.2 Same H6 backend on all three, while P4M900 uses the older H2 path

OpenChrome's EXA setup uses precisely these three cases for `viaExaPrepareSolid_H6`, `viaExaSolid_H6`, `viaExaPrepareCopy_H6`, `viaExaCopy_H6`, and `viaAccelMarkSync_H6`:

- `VIA_VX800`
- `VIA_VX855`
- `VIA_VX900`

It repeats the same three-chip grouping for `viaExaCheckComposite_H6`, `viaExaPrepareComposite_H6`, and `viaExaComposite_H6`. Other chips (including `VIA_P4M900`) take the `H2` callback case. [OpenChrome `src/via_exa.c:L903–L958`]. Thus the *original CN896/P4M900 driver lineage* and the *VX800→VX855→VX900* lineage need not have identical accelerated 2D command handling merely because all are branded Chrome9.

**Conclusion (source-supported):** use the existing VX855 2D code as the shared family default for VX800/VX900, while preserving specific device identification, display power sequencing, HQV state, PCI BARs, and 3D vertex/CR differences. `H6` and `H2` here are OpenChrome's internal backend names, **not** a claim that every H6 chip has an identical complete 3D engine.

<a id="c9-section-31"></a>

#### 31. OpenChrome limitations and source-quality caveats

1. **Static source does not prove execution.** The presence of a case label says the author intended to support that chip; it does not certify that all branches work on all boards.
2. **Its complete game-3D client is not in this driver.** `swrast` for VX800/VX855/VX900 must not be mistaken for a hardware OpenGL implementation [OpenChrome `src/via_dri.c:L529–L541`].
3. **Linux DRM/TTM/X.Org APIs differ from Win98.** Reuse algorithms and documented register layouts, not OS-specific setup calls (`drm_bo_alloc`, `pci_device_map_range`, EXA callbacks).
4. **VQ length inconsistency remains unresolved.** Never copy `VIA_VQ_SIZE >> 3` as a confirmed correct register-unit conversion until tested against the manual and physical hardware.
5. **Reserved register writes in shared init are not endorsed.** The VX900 `0x60` write and legacy 3D reset loop are source facts, not proof of documented behavior.
6. **Small-work acceleration threshold is a policy choice.** The 400-pixel EXA threshold suggests a benchmark, not a universally correct driver constant.
7. **No automatic completion proof.** Status busy polling and observed pixels do not demonstrate a specific queue read pointer or verified command ordering.
8. **No full compiler-level audit was performed.** The source was read for cross-platform hardware programming behavior, not statically analyzed or built as a complete Linux driver.

<a id="c9-section-34"></a>

#### 34. OpenChrome-specific cross-check verdict

**Supported by both the manuals and OpenChrome:** common 2D register model; identical OpenChrome H6 2D callback family; shared minimal textured-triangle/compositor state patterns; BAR1 MMIO across the three; VX900 framebuffer on BAR2; shared command-regulator concepts and a source-backed VX855/VX900 queue initialization sequence.

**Now demonstrably different in source:** VX800 versus VX855/VX900 LVDS power sequence, HQV register groups, secondary-display FIFO configuration, extended CRTC state handling; VX900 additional timing and BAR use.

**Still unresolved:** whether VX900 game-style vertices require entirely distinct direct-vertex emission under all modes; whether the VX900 reserved shader fields preserve undocumented legacy behavior; the true queue length unit; whether a specific Win98 ring fetch path can be made reliable; how to prove command completion without a trusted hardware read pointer.

**Most actionable for the current VX855 effort:** treat OpenChrome's `0x41C/0x420` VQ writes as a **reference to compare against**, not proof of a working ring; instrument `0x400` with H5 masks; separately time direct-MMIO versus queued execution; investigate shared textured-quad compositing and 8-bpp GE fills; maintain the existing proven 3D backend for OpenGL/DirectDraw/Direct3D rather than swapping in OpenChrome's `swrast` path.

<a id="c9-section-45"></a>

#### 45. OpenChrome **shared command** comparison (VX800 / VX855 / VX900)

[Appendix E](#openchrome) contains the exact shared 2D/H1, 3D/H2, VQ, synchronization and display commands for the uploaded OpenChrome source. This evidence applies to the exercised paths and modes. The common source functions establish the emitted encodings for those paths, while advanced hardware commands require their own evidence.

### Packet reservation, state and alignment issues

<a id="c9-section-36"></a>

#### 36. OpenChrome issue OC-01: two-texture quad buffer reservation is short

**Classification:** reproducible static-source word-count discrepancy; possible CPU-side command-buffer overwrite near its end. This is **not** a verified bug in the user's Win98 driver.

**Source:** `via_3d.c:L279–L385`; `via_dmabuffer.h:L58–L96`; `via_exa.c:L179–L195`; `via_exa_h6.c:L420–L453`.

OpenChrome's `via3DEmitQuad()` does this:

```c
BEGIN_H2(HC_ParaType_CmdVdata, 22 + numTex * 6);
```

But its loop emits **six** complete vertices, each with `X`, `Y`, `W`, and **two coordinates per texture**. There are also two vertex-command words (`0xEC`, `0xEE`) and two end/fire words. When a new H2 header is necessary, the total emitted DWORD count is:

```text
actual_dwords = 2 header + 2 vertex-control +
                6 * (3 XYW + 2*numTex UV) + 2 fire
              = 22 + 12*numTex

BEGIN_H2 requests capacity for (22 + 6*numTex) + 6 spare DWORDs.
```

| Active textures | Actual words with H2 header | Reserved by macro call including spare | Reserve minus actual |
|---:|---:|---:|---:|
| 0 | 22 | 28 | +6 |
| 1 | 34 | 34 | 0 |
| **2** | **46** | **40** | **−6** |

With an existing compatible H2 header, some header words can be reused, but the capacity calculation still is not guaranteed sufficient for the two-texture payload. A 16-KiB/4,096-DWORD software command buffer is allocated in `viaSetupCBuffer()`; `OUT_RING` advances the position and writes without a per-word bounds check. As a result, a call made sufficiently near the buffer end could write past the allocation, or overwrite the intended tail/guard area.

**Conditional reachability:** `viaExaPrepareComposite_H6()` increments `curTex` for a source texture and again for a non-trivial mask texture, so `numTex=2` is a reachable configuration (`via_exa_h6.c:L420–L453`). It does not mean every composite overflows; the risk depends on the prior command-buffer position.

**Conservative source fix concept:** base the reserve on the true worst-case emitted words (`22 + 12*numTex`), retain the macro's alignment/header reserve, and reject impossible texture counts before any write. A safer broader change is a checked reserve/write primitive that confirms `pos + emitted <= bufSize` before emission. Do not merely increase `VIA_DMASIZE` and assume the issue disappears.

**Hardware-independent regression:** initialize a tiny guarded command buffer with `pos` just below the end, emit a zero-/one-/two-texture quad into a test sink, assert no guard DWORD changes, and assert exact emitted-word counts. Run with and without a preceding H2 header.

<a id="c9-section-37"></a>

#### 37. OpenChrome issue OC-02: stale partial plane mask survives a full-mask operation

**Classification:** confirmed logic/state-transition issue in the static code; visible-pixel consequence should be checked in a test.

**Sources:** `via_exa_h6.c:L44–L97`, with the same pattern in `via_exa_h2.c:L44–L98`.

`viaAccelPlaneMaskHelper_H6()` updates the high nibble of `tdc->keyControl` **only inside the partial-mask branch**. If a subsequent operation uses a full plane mask, the branch is skipped, so the previous byte-mask bits remain. `viaAccelTransparentHelper_H6(..., usePlaneMask=TRUE)` deliberately preserves that same high nibble when constructing the next command.

A concrete **16-bpp** state sequence:

1. Start with `keyControl=0`.
2. Prepare a partial mask `0x00FF`: the high byte is disabled, setting the high nibble to `0x2` (`keyControl=0x20000000`).
3. Prepare a full mask `0xFFFF`: the helper does **not** change the nibble.
4. The transparency helper preserves `0x20000000` for the full-mask operation.

Thus software state retains the *partial* hardware mask even though the next caller requested a *full* mask. This can plausibly cause incomplete updates to the high color byte, depending on actual GE key/mask semantics.

**Conservative fix concept:** calculate the desired mask independently for *every* prepare call, including zero disabled bytes for a full mask, then replace the high nibble unconditionally. Do not rely on a previous operation to have cleared it.

**Falsifiable test:** 16-bpp surface, initialize a known color, perform one masked fill, then a full-mask fill over a distinct rectangle, read back and compare both color bytes. Repeat for copy, alternating partial/full masks. Because H2 carries the same logic, this should be regarded as a shared-source inheritance issue, not evidence of a particular GPU silicon defect.

<a id="c9-section-38"></a>

#### 38. OpenChrome issue OC-03: undefined 32-bit shift in plane-mask calculation

**Classification:** C-language undefined behavior, confirmed from the expression and supported bit-depth selection; actual compiled result depends on compiler/target.

**Sources:** `via_exa_h6.c:L45–L50`; `via_exa_h2.c:L45–L50`; `via_exa.c:L216–L235`.

The source calculates:

```c
CARD32 modeMask = (1 << ((1 << tdc->bytesPPShift) << 3)) - 1;
```

For a 32-bpp destination, `bytesPPShift=2`, giving `1 << 32`. With a 32-bit `int`, shifting by 32 is **undefined behavior**, even if a particular x86 machine masks the shift count. A related expression `0xFF << (i << 3)` can also left-shift a signed `int` beyond its representable range for the high byte.

**Conservative fix concept:** explicit 8/16/32-bit masks (`0xFFu`, `0xFFFFu`, `0xFFFFFFFFu`) and explicitly unsigned shifts, or use `uint64_t` for intermediate masks. A compiler sanitizer or a simple separately compiled unit test can validate the corrected arithmetic without GPU access.

**Recommendation for VX855 code audit:** search for all `1 << bpp`, `1 << (bytes*8)`, `0xFF << 24`, and untyped signed bit-mask macros. Do not assume OpenChrome's defect exists in the Win98 tree; compare source first.

<a id="c9-section-39"></a>

#### 39. Two further OpenChrome 2D issues and their portability implications

##### 39.1 Source pitch validation accepts 4-byte pitches that are truncated when encoded

**Source:** `via_exa_h6.c:L194–L265`; the VIA 2D Part I manuals, GE `Pitch and FB Location` register at offset `0x08`.

`viaExaPrepareCopy_H6()` rejects a source pitch if `(srcPitch & 3) != 0`, allowing any **4-byte-aligned** pitch. Yet emission packs `(srcPitch >> 3)` into the GE source-pitch field, i.e. **8-byte units** (the manual additionally mentions stronger alignment constraints). A pitch of `12` bytes would pass the source check but be encoded as `1` (8 bytes), silently losing the remaining four bytes. The destination pitch is checked with `& 7`.

This is a high-confidence **precondition/encoding mismatch**. Whether EXA's pixmap allocator ever supplies such a pitch is environment-dependent; it should be tested rather than asserted as a commonly observed bug.

**Conservative remedy:** require source pitch alignment consistent with the actual hardware encoding, and verify any stricter chip-specific requirements; otherwise fall back to a CPU copy or a staging surface. A copy test must include pitches congruent to 4 modulo 8, not just 8-byte-aligned cases.

##### 39.2 The solid-fill path can reject ordinary depth-24/bpp-32 pixmaps

**Sources:** `via_exa_h6.c:L140–L161`; `via_exa.c:L216–L235`; compare `via_exa_h6.c:L194–L225`.

The H6 solid setup calls `viaAccelSetMode(pPixmap->drawable.depth, ...)`, which supports only **8, 16, 32**. On X.Org an ordinary TrueColor pixmap may have **depth 24 but occupy 32 bits per pixel**. In that case the solid-fill call returns `FALSE` and falls back, even though the copy path correctly uses `drawable.bitsPerPixel`.

This is a **potential performance fallback** rather than proof of broken pixels: it depends on the actual pixmap depth and the EXA surface created. The corresponding Win98 lesson is to keep color *depth*, *memory storage bits per pixel*, and *hardware format* as separate fields. Avoid letting a 24-bit RGB surface stored in 32-bit pixels disable an otherwise valid 32-bpp GE command.

<a id="c9-section-40"></a>

#### 40. OpenChrome command-stream robustness: hangs caused by software are possible

##### 40.1 The direct-MMIO command parser has a non-advancing error branch

**Source:** `via_exa.c:L42–L110`.

`viaFlushPCI()` walks `bp` through an in-memory command buffer. Header-1 and Header-2 cases increment `bp`, but an unexpected word leads to:

```c
else {
    ErrorF("Command stream parser error.\n");
}
```

The parser neither advances `bp`, clears the buffer, nor exits. Consequently, **a malformed command word at the current position can cause an infinite CPU loop**, generating repeated errors rather than reaching the GPU. This is a source-level software hang path, distinct from a stalled hardware ring or a busy GPU.

**Conservative fix concept:** log the offending index/word, mark the current command buffer invalid, leave the parser without retrying the same word, and propagate an explicit error. Do not merely skip unknown words silently in production because that could desynchronize the stream.

**Diagnostic significance for VX855:** instrument submission with at least three independent outcomes: `PARSER_ERROR`, `ENGINE_BUSY_TIMEOUT`, and `GPU_COMPLETION_TIMEOUT`. A motionless window with the process still running is not sufficient to distinguish them.

##### 40.2 Busy waits do not always propagate a timeout

**Sources:** `via_exa.c:L42–L110`, `L465–L495`.

In the direct-MMIO flush path, the code loops on engine status with `loop++ < MAXLOOP` and then continues to register writes without an explicit error result if the limit was hit. `loop` is defined near the beginning of `viaFlushPCI()` and is not independently reset for every potential status wait. `viaAccelSync()` also terminates a bounded loop without reporting a separately labeled timeout to callers.

This is useful guidance for the Win98 work: report whether a wait ended because the bit cleared or because the retry limit expired. Explicitly log the status before, during, and after the wait. A finite polling loop is not itself proof of synchronization, especially under 2D/3D contention.

##### 40.3 Buffer-flush tail reservation should be verified as a unit

**Source:** `via_exa.c:L137–L172`; `via_dmabuffer.h:L75–L96`.

`viaFlushDRIEnabled()` appends a Header-1 pair (`OUT_RING_H1(0x2f8, ...)`) and sometimes a dummy word before submitting to DRM. No explicit `BEGIN_RING` guard surrounds those appended tail words. Existing producers often reserve spare capacity, but OC-01 proves not all calculations are exact for two textures. The safe design is an invariant enforced centrally: **reserve payload + maximum header/pad/trailer before writing any of it**.

This is an additional check to perform, not a separate demonstrated out-of-bounds execution trace.

<a id="win98-porting"></a>

## Windows 98 porting and validation

Use separate family paths where packet semantics diverge. Keep the port design, regression plans and diagnostic suggestions separate from results already observed in manuals, binaries or hardware logs.

### Port design, implementation priorities and errata

<a id="c9-section-13"></a>

#### 13. Proposed common Windows 98 driver design

**Goal:** retain a common Windows 98 interface while isolating families' dangerous register differences. The precise filenames below are illustrative and should be adapted to the actual VX855 source tree.

```
win98_driver/
  common/
    pci_detect.c          -> maps PCI identity to chip family
    os_resources.c        -> Win9x MMIO/framebuffer resource mapping
    surface_state.c       -> formats, pitches, allocation and ownership
    ge2d.c                -> common blit/ROP/line algorithms
    ddraw.c               -> shared DirectDraw contract
    d3d7.c                -> shared D3D HAL contract
    vxgl.c                -> shared GL translation
    trace.c               -> bounded diagnostic logging
  families/
    hc3_vx800/
      pci_bars.c          -> VX800 framebuffer and MMIO mapping
      cr.c                -> HC3 CR initialization and waits
      vertex.c            -> HC3-compatible FVF words
    hcm_vx855/
      pci_bars.c          -> existing VX855 mapping
      cr.c                -> preserve known-working commands
      vertex.c            -> existing HCM FVF words
    hd_vx900/
      pci_bars.c          -> BAR2 framebuffer, BAR1 MMIO
      cr.c                -> HD locks, fences and dispatch
      vertex.c            -> multi-stream VB/input-register mappings
      optional_2d.c       -> experimental AlphaBLT
      ppc.c               -> profiling counter writeback
  tests/
    bar_probe/
    ge_compare/
    cmd_fence/
    triangle_flat/
    textured_quad/
    depth_tile/
    mode_switch/
```

##### 13.1 Prefer capability descriptors and typed operations

Use specific operations instead of calling `MMIO_WRITE(0x60, x)` throughout the codebase. For example:

```c
/* Illustrative interface, not a drop-in implementation. */
struct ChromeOps {
    int  (*init_bars)(void *ctx);
    int  (*init_2d)(void *ctx);
    int  (*wait_2d)(void *ctx, unsigned timeout_ms);
    int  (*wait_3d)(void *ctx, unsigned timeout_ms);
    int  (*set_vertex_format)(void *ctx, const void *layout);
    int  (*submit_primitive)(void *ctx, const void *verts, unsigned n);
    void (*recover_engine)(void *ctx);
};
```

Then share state translation **above** the backend while keeping raw register sequence generation **below** it. Prefer well-named encoders for individual bitfields with static masks. Reserved-bit writes should be rejected during diagnostic builds.

##### 13.2 Zero-impact first port

Preserve the production VX855 backend and make all new dispatch conditional on PCI ID. No change to VX855 write ordering should be necessary merely to add VX800 or VX900 detection. If a shared refactor changes VX855 output, treat it as a regression until tests show otherwise.

<a id="c9-section-15"></a>

#### 15. High-risk register list / implementation checklist

##### Must be family-gated

- [ ] PCI BAR framebuffer and MMIO resource mapping (`1122`, `5122`, `7122`).
- [ ] VX855/VX800 `GE 0x60` and `0x6C` writes: **not on VX900**.
- [ ] VX855 `HParaType 10h:04h–07h`: **not copied to VX900**.
- [ ] VX855 `HParaType 11h:00h` FIFO/critical-command bits: **not copied to VX900**.
- [ ] VX900 `11h:03h` fence setup: only on supported family.
- [ ] `HParaType 04h:00h–12h` vertex encoders: HCM/HC3 vs HD.
- [ ] VX855 `04h:21h–22h`: **not copied to VX900** (reserved there).
- [ ] VX900 `03h:30h` stream-base/pitch entries: HD only.
- [ ] VX900 `03h:31h–33h` shader uploads: **do not use until documentation resolved**.
- [ ] VX855 CFC `0x30`: no 8-bit source conversion claim.
- [ ] VX900 AlphaBLT: guarded as experimental until exact behavior known.
- [ ] VX900 PPC: correct destination buffer, address alignment and engine-idle handling.

##### Runtime safeguards

- [ ] Every wait has timeout, context, last-command sequence and safe recovery path.
- [ ] Every command producer records length and validates bounds/32-bit packing.
- [ ] State shadowing invalidated after reset, mode switch and device restore.
- [ ] Buffers may not be recycled until their associated work completes.
- [ ] No reads from write-only packet registers assumed to reflect queued state.
- [ ] Driver has a feature-disable switch to isolate accelerated subsystems.
- [ ] Existing VX855 behavior used as baseline after every common-code refactor.

<a id="c9-section-16"></a>

#### 16. Manual errors, ambiguous statements and claims to avoid

1. **VX900 `0x04` GE register:** reserved `31:10` field overlaps documented alpha controls `20` and `19:12`. [V9-I, p. 105].
2. **VX900 BAR/MMIO discussion:** different MB0/BAR references in the narrative and PCI table require hardware verification. [V9-I, pp. 11–17].
3. **VX900 shader programming:** overview names VS/PS parameter categories, detailed sections call important ranges reserved. Do not infer complete ISA compatibility. [V9-II, pp. 70–73, 126, 159].
4. **VX900 vertex chapter typo:** `04h:21h–22F` is printed as reserved in the text, an implausible range spelling; verify against the register map and do not extrapolate past `22h`. [V9-II, p. 166].
5. **VX855 CFC:** despite an 8-bpp encoding in the source-depth field, the source explicitly states it does **not support 8-bpp conversion**. [V855-I, p. 84].
6. **Feature versus functionality:** documentation of tiling, shaders, rings or counters does not prove reliability or Windows 98 runtime exposure.
7. **Common address is not common meaning:** especially CR `10h/11h` and vertex `04h` words.

<a id="c9-section-17"></a>

#### 17. Final engineering judgment

**VX800/HC3:** Most promising near-term target for sharing the current VX855 2D and basic 3D code. Main concerns are PCI/BAR setup, differences in supported features and validating commands that only *appear* identical in textual descriptions.

**VX900/HD:** Requires a real backend for vertex-input setup and CR synchronization. Its basic GE, rendering concepts, texture units and depth operations provide a valuable foundation, but its multi-stream vertex mapping and incomplete shader instructions are the primary technical risks. Added AlphaBLT, hardware locking/fences and profiling counters warrant experiments only after baseline bring-up.

**Recommended next development milestone:** Build a **read-only PCI/BAR capability probe and a deterministic 2D test**, then a **family-specific 3D flat-triangle test**. These will distinguish architecture differences from errors caused by blindly reusing the VX855 command stream. Keep the existing VX855 code unchanged until those tests produce reproducible logs.

---

*This is a manual-grounded design/audit document, not a line-by-line audit of a current source-code archive. Register and bitfield assertions should be verified against the cited PDF pages and on physical hardware before shipping a driver.*

<a id="c9-section-18"></a>

#### 18. Additional register fields by subsystem

This section extends the initial comparison with more precise **queue, depth/stencil, vertex, and observability** findings. The evidence is still **manual-only**. No new physical-hardware tests or source-tree audit have been performed. Earlier sections remain applicable; where the summary was broad, this section supplies narrower register definitions.

##### 18.1 Command queues: shared `10h:70h–74h` is real; second VX855 queue differs

All three manuals document a command queue (CMDQ) under **`HParaType=10h`**. In VX855 and VX900, the principal setup words are:

| HParaType / sub-address | Documented function | Important details |
|---|---|---|
| `10h:70h` | Queue start address and memory location | Start address in bits `23:4`; low four address bits not used; location in bits `1:0` |
| `10h:71h` | Queue end address | Low address portion; **128-bit alignment** |
| `10h:72h` | High bytes of start and end addresses | High end in bits `15:8`, high start in `7:0` |
| `10h:73h` | Queue length | Units of **128 bits**; documented minimum `0x800` units (= **32 KiB**) |
| `10h:74h` | FIFO thresholds, arbitration and enable | Read/write thresholds, arbitration mode, source selection, request length, enable bit |

**Sources:** [V8-II, PDF p. 150]; [V855-II, pp. 72–73]; [V9-II, pp. 85–86].

Crucial `10h:74h` fields (VX855 and VX900 descriptions agree in substance):

| Bits | Meaning |
|---:|---|
| `21:16` | Write CMDQ FIFO threshold |
| `13:8` | Read CMDQ FIFO threshold |
| `7:4` | Arbitration: when reading queued work may interrupt CMDQ writes |
| `2` | Select commands arriving from PCI versus AGP |
| `1` | Request grouping: single 128-bit command, or groups of 1, 2 or 4 128-bit commands |
| `0` | Enable command queue |

This is a useful **documented queue configuration interface**, not proof that any particular ring implementation in the current driver is correctly formed or clocked.

**Second-queue difference:** VX855 explicitly lists `10h:78h–7Ch` as CMDQ settings **6–10** for a separate **2D/3D-command queue**. VX800 likewise lists the `70h–7Ch` CMDQ group. The supplied VX900 detailed CMDQ subsection documents `70h–74h` but not that older `78h–7Ch` extension. **Do not copy the VX855 second-CMDQ initializer into VX900.** [V8-II, pp. 150–152]; [V855-II, pp. 73–74]; [V9-II, pp. 85–86].

**Memory-location code difference:** The `10h:70h` two-bit queue-location field uses `11b` as **reserved** in VX855, whereas VX900 describes `11b` as **LL (local-memory/local-framebuffer)**. Other documented values are `00b` SL, `01b` SF, `10b` SM. A common encoder should reject unsupported locations by chip, not by a family-wide hardcoded interpretation. [V855-II, p. 72]; [V9-II, p. 85].

**VX900 branch/queue warning:** VIA explicitly warns in the VX900 `10h:74h` section that with **VQ1 enabled**, `FE8x` branch/AGP-branch headers are **forbidden**, because hardware cannot stop the vertex-queue read when such a branch is detected. This is a documented combination to avoid, not a suggested performance tuning parameter. [V9-II, p. 86].

**Implication for the VX855 ring tests:** Check first whether the tested path used a documented queue start/end/length/enable sequence, which queue was selected, and whether the source was a permitted region with correct alignment. Separately record the producer's written bytes, the last successfully completed batch, and any readback *whose meaning has been established*. An unchanged value at MMIO `0x43C`, by itself, is **not a validated hardware consumer pointer** in the manuals.

##### 18.2 Queue arithmetic and bounds checks (proposed)

The manuals give enough structure to implement host-side configuration validation before the actual hardware programming:

```c
/* Illustrative validation only; not a command packet or Win9x API. */
#define CMDQ_GRANULARITY_BYTES 16u
#define CMDQ_MIN_UNITS         0x800u
#define CMDQ_MIN_BYTES         (CMDQ_MIN_UNITS * CMDQ_GRANULARITY_BYTES)

int check_cmdq_layout(unsigned long start, unsigned long end,
                      unsigned long byte_count)
{
    if ((start & 15u) || (end & 15u)) return 0;
    if ((byte_count & 15u) || byte_count < CMDQ_MIN_BYTES) return 0;
    if (end <= start) return 0;
    /* Do not assume whether the hardware end address is inclusive.
       That convention must be verified before committing real MMIO writes. */
    return 1;
}
```

Do not treat this illustrative check as a sufficient hardware bring-up procedure: BAR mapping, queue-location selection, addressing units, power/clock state, fence sequencing and the **end-address convention** still need confirmation. In particular, **32 KiB is the documented minimum configured length**, not a guarantee of physically available contiguous storage.

A queue-diagnostic run should include a **disabled-queue negative control**, a minimum-sized queue, a larger queue, one immediate command, 16 repeated no-state-change commands, and a cross-engine 2D→3D→2D transition. Tests must have bounded waits and an external watchdog/recovery plan; never enter an indefinite MMIO polling loop on Win98.

##### 18.3 Why CR locking needs an independent VX900 backend

At `HParaType=11h:00h`, the VX900 documents **bit 17** for hardware mutual exclusion of 2D and 3D and **bit 16** for 2D/3D request-lock behavior. On VX855 the same word describes FIFO depths in bits `10:2` and the **critical/nonbreakable command stream** flag in bit `0`, with bit 16 also used for request locking. [V855-II, p. 75]; [V9-II, p. 87].

In the VX900, `11h:03h` selects what a fence must wait for: 3D idle (`bit 23`), 2D idle (`22`), video (`21`), HQV (`20`,`19`), DMA3–DMA0 (`18`–`15`), HQV EOF (`14`,`13`), or **all engines** (`bit 0`). The manual also defines a wait-idle count in bits `8:4`. [V9-II, p. 88].

**Practical separation:** `submit_3d()` and `wait_3d()` can remain common *interfaces*, but the backend must decide whether to issue VX855 legacy controls or VX900 hardware locks/fences. A shared `write_cr(type, address, payload)` helper is acceptable **only** if its caller uses chip-specific schemas and valid-field masks.

<a id="c9-section-22"></a>

#### 22. Port implementation priorities

| Priority | Target | Change or experiment | Rationale | Evidence status |
|---:|---|---|---|---|
| 1 | VX855 | Add queue descriptor logging for `10h:70h–74h`; include `78h–7Ch` if used | Determine which documented queue path is active during hangs | Documented registers; runtime behavior unknown |
| 2 | VX855 | Confirm `0x43C` status/readback meaning before calling it a ring consumer | Avoid false diagnosis from unrelated/aliased readback | Unresolved |
| 3 | VX855 | Isolate packed D24S8 vs separate-stencil, 8×8 boundaries | Differentiate tile-address assumptions from cache coherency | Documented separate-stencil map; failure cause unknown |
| 4 | VX855 | Trace `01h:27h` and `01h:29h` surrounding depth clears | Find stale state/cache reset ordering patterns | Documented controls; experiment proposed |
| 5 | VX855 | Probe occlusion-count writeback (`01h:21h–22h`) | Possible hardware visibility signal without VX900 PPC | Documented interface; untested |
| 6 | VX800 | Reuse validated 2D/primitive abstraction after BAR fixes | Most direct new-chip port | Strong architectural overlap |
| 7 | VX900 | Implement one queue only; block forbidden queue/branch combinations | Avoid a documented VQ1 branch hazard | Explicit warning in manual |
| 8 | VX900 | Build distinct vertex and CR encoders | Same addresses have different meanings | Documented incompatibility |
| 9 | VX900 | Investigate shader ISA via reference driver traces | Required before any hardware-T&L capability claim | Source documentation incomplete |

<a id="c9-section-23"></a>

#### 23. Additional manual errata and interpretation rules

1. **Queue length units matter.** `10h:73h` uses *128-bit units*, so treating `0x800` as 2 KiB rather than **32 KiB** produces an undersized buffer. [V855-II, p. 72]; [V9-II, p. 85].
2. **`0x70` is not enough to identify a register.** The `HParaType` and sub-address jointly name state; the same raw byte occurs throughout many separate MMIO/payload spaces.
3. **VX900 queue-location value `11b` is not VX855 `11b`.** VX900 lists LL; VX855 reserves it. [V855-II, p. 72]; [V9-II, p. 85].
4. **`01h:13h` bit 23 changes meaning.** VX855 says shader-sourced depth; VX900 marks reserved. [V855-II, p. 88]; [V9-II, p. 100].
5. **Separate-stencil tile formula is not automatically the packed Z24S8 address formula.** Keep test hypotheses separate. [V855-II, p. 93].
6. **VX855 has a limited occlusion-count writeback path.** Do not equate it with the much broader VX900 `HParaType=08h` profiling subsystem. [V855-II, p. 90]; [V9-II, pp. 180–181].
7. **VX900's VQ1/FE8x combination is explicitly prohibited by the manual.** This warrants an encoder assertion, not a trial-and-error performance experiment. [V9-II, p. 86].
8. **Experimental controls have side effects.** Coarse-Z TAG resets and queue enabling must run only in a test driver under a fully understood engine-idle sequence, with a recoverable system image and known-good fallback.

**Continuation verdict:** The manuals support substantially more nuanced sharing than a simple three-way “same GPU” designation. Keep one logical state translator and diagnostic framework, but use separate family-specific register validation, queue layout, synchronization and vertex encoding. For the ongoing VX855 work, the most actionable newly documented areas are the **CMDQ `10h:70h–7Ch` descriptors**, **Fast-Z/CZ state**, **separate-stencil addressing**, and **occlusion-count writeback**.

<a id="c9-section-32"></a>

#### 32. Revised implementation design after OpenChrome review

##### 32.1 Share these driver components first

- `Chrome2D`: common 8/16/32-bpp GE mode selection, fills, same-format copies, ROPs, overlap direction, pitch/offset/dimension packing, and plane-mask restrictions (validated for each device).
- `ChromeMMIO`: common register ordering/wait/logging abstraction; chip-specific masks and validated transport selection.
- `ChromePrimitive`: common high-level point/line/triangle/quad descriptions; *not* shared raw `04h` vertex input words across VX900.
- `ChromeTextureState`: shared logical format/filter/address representation with per-family legality checks.
- `ChromeVideo`: common overlay abstraction; per-family HQV setup and register-map tables.
- `ChromeDisplay`: shared mode parameters; per-chip BAR, FIFO, extended CRTC, output and power sequencing.

##### 32.2 Keep explicit family backends for these parts

| Component | HC3/VX800 | HCM/VX855 | HD/VX900 |
|---|---|---|---|
| Framebuffer BAR | 0 | 0 | **2** |
| MMIO BAR | 1 | 1 | 1 |
| H6 2D basics | Shared with feature checks | Shared; known implementation | Shared with feature checks |
| Basic XRender-like compositing | OpenChrome common subset | OpenChrome common subset | OpenChrome common subset |
| Game 3D vertex input | Similar legacy FVF model | Working project backend | **Separate FVF/multistream encoder** |
| CR/fences | Legacy-family-specific | Working project backend with tested timeouts | **Separate locking/fence state** |
| HQV table | VX800 branch | `409` branch | `409` branch + special sharpness handshake |
| LVDS power | Software sequence in OpenChrome | Hardware sequence in OpenChrome | Hardware sequence in OpenChrome |
| CRTC/FIFO | VX800 values | VX855 values | VX900 values, extra timing bits |
| Hardware OpenGL validation from OpenChrome | **No** (`swrast`) | **No** (`swrast`) | **No** (`swrast`) |

##### 32.3 Suggested diagnostic interfaces (conceptual, not a code patch)

```c
/* Illustrative types; no raw register writes are authorized by this example. */
typedef struct {
    unsigned long raw_status_400;
    unsigned long cr_busy_mask;
    unsigned long ge2d_busy_mask;
    unsigned long ge3d_busy_mask;
    unsigned long vq_busy_mask;
} ChromeEngineStatus;

typedef struct {
    unsigned long requested_bytes;
    unsigned long reserved_bytes;
    unsigned long ring_offset;
    unsigned long programmed_length_field;
    unsigned bytes_per_unit_assumed;   /* record 8 or 16; verify */
    unsigned submission_kind;         /* direct MMIO, DRM-like, actual VQ */
    unsigned completion_proof_kind;   /* status, fence, read-pointer, NONE */
} ChromeQueueDiagnostic;
```

Do not label an experiment `VQ_SUCCESS` until it has demonstrated a **VRAM producer stream with no parallel direct-MMIO draw path** and an independent completion condition.

<a id="c9-section-44"></a>

#### 44. Common driver architecture and family-specific encoders

After reading the OpenChrome implementation further, a practical first port still looks like:

**Shared**: 2D fill/copy state encoders (with validated pitch/mask rules), ROP mapping, basic texture/render-target state, a restricted Direct3D/OpenGL primitive abstraction, and common test tools.

**Family-specific**: BAR mapping, firmware framebuffer discovery, display FIFO/timing/power sequencing, HQV register tables, queue/fence controls, and especially VX900 `04h` vertex-input encoding.

**Do not unnecessarily rewrite:** the logical direct-vertex primitive path until a small experiment proves the VX900 interpretation incompatible. Conversely, **do not borrow unvalidated OpenChrome commands** merely because the same source callback is assigned to all three chips.

**Most important immediate quality lesson for the existing VX855 driver:** use source-level tests for **worst-case packet length, full/partial state transitions, unsigned shifts, pitch alignment, and explicit CPU/hardware timeout separation**. These are plausible sources of stalls or pixel corruption across many games independent of the exact Chrome9 silicon generation.

### Tests and diagnostics

<a id="c9-section-11"></a>

#### 11. VX900 performance counters: high-value diagnostic feature

The VX900 manual gives `HParaType=08h`, sub-addresses `01h–03h`, as a **performance profile counter (PPC)** interface with counter selection and memory writeback. The documented list includes:

| Counter | Documented measurement | Interpretation |
|---:|---|---|
| `00h` | Vertices received | Primitive/input traffic |
| `01h` | Vertex-cache hits | Vertex reuse behavior |
| `02h` | Vertex-shader instructions executed | VS execution work |
| `03h` | Primitives, including dropped | Submission pressure |
| `04h` | Valid primitives | Clipping/culling outcome |
| `05h` | Pixels including dropped | Rasterization pressure |
| `06h` | Valid pixels | Passed-pixel behavior |
| `07h` | PS TAU instructions | Texture address work |
| `08h` | PS ALU instructions | Pixel arithmetic work |
| `09h` | Vertex-buffer reads | Vertex traffic |
| `0Ch`, `0Dh` | Z-buffer reads/writes | Depth bandwidth |
| `0Eh`, `0Fh` | Color buffer reads/writes | Render-target bandwidth |
| `10h`, `11h` | Texture reads from SL/SF | Texture bandwidth |

**Evidence:** [V9-II, pp. 180–181]. The list has additional counters for coarse Z, other memory traffic, activity and occlusion; consult the exact manual values before implementing the full list.

The source text says the selected counter can be **written back to memory**. The programming must include the proper address, destination memory region and fire bit; it is not necessarily a simple MMIO read. The manual does not prove the availability of the same counters on VX855 or VX800.

**Practical design:** Put counter collection behind `read_performance_counter()` that returns *unsupported* on HC3/HCM unless separate evidence emerges. Correlate counts with wall-clock/frame-level timestamps. Ratios such as valid-pixels/total-pixels can flag heavy rejection, but cannot alone identify a driver bug.

<a id="c9-section-14"></a>

#### 14. Prioritized implementation and validation plan

| Stage | Target | Test / action | Clear pass condition |
|---:|---|---|---|
| 0 | All | Enumerate PCI identity and BAR type/size | Correct device and mapping; no writes to wrong aperture |
| 1 | VX855 | Baseline existing regression suite | No deviation from known-working source |
| 2 | VX800 | 2D fill, copy, overlap, clipping and ROP | Exact framebuffer match, no hang |
| 3 | VX800 | Flat triangle then textured triangle | Correct geometry and color; reproducible after mode switch |
| 4 | VX800 | Depth/stencil and long stress | No stale tiles, bad waits or unexpected resets |
| 5 | VX900 | 2D with new PCI mapping | Screen display + memory tests reliable |
| 6 | VX900 | Re-implement CR wait/fence | Bounded completion under repeated 2D/3D transitions |
| 7 | VX900 | Flat triangle using new vertex mapping | Exact coordinates/color, no old FVF writes |
| 8 | VX900 | Textures, alpha, Z/stencil, clipping | Reference-matched test cards, stable after warm reboot |
| 9 | VX900 | Optional AlphaBLT and PPC | Verified pixels or plausible counter trends, separately gated |
| 10 | VX900 | Shader / hardware-T&L feasibility | Only if working instruction path independently verified |
| 11 | All | Real application compatibility | Quake 2/3, D3D test, DirectDraw and screensaver cases logged |

##### 14.1 Regression cases particularly relevant to the VX855 project

- **Flat shaded test:** distinguish setup overhead, primitive throughput, and per-fragment cost; compare with Gouraud/textured runs.
- **Depth clear:** Quake 2 `gl_ztrick 0` versus `1`, plus standalone clear/readback.
- **Stencils:** D24S8 tile boundaries, shared-edge triangles, and 2D/CPU clear interactions.
- **Command hangs:** transition 2D→3D→2D, game menu→gameplay, state-identical small primitive lists, fence stress.
- **Win98 desktop:** cursor integrity, 8/16/32-bpp mode transitions, GDI brush/fill/text fallback.
- **DirectDraw:** surface create/destroy, video-memory exhaustion, flip, lost/restored surfaces, overlay and color-key fallbacks.
- **VXGL:** Quake 2 point-smoothing + world triangles, Quake 3 initialization and depth changes, and representative screensavers.
- **Timing:** CPU vertex preparation, state translation, MMIO writes, idle polling, GPU completion and per-frame time in distinct buckets.

**Do not set a performance claim or a new Direct3D cap based solely on passing a synthetic probe.** Test output consistency across a long run and across mode changes.

<a id="c9-section-21"></a>

#### 21. Proposed test-log additions for present VX855 investigations

These are **proposed diagnostic records**, not claims that the existing driver exports the fields yet.

##### 21.1 Ring and synchronization tests

```text
TEST, chip, PCI-ID, test-id, batch-id, cmdq-selected,
queue-start, queue-end, queue-bytes, queue-location,
queue-control-10:74, second-queue-control-10:7C,
submission-method, submitted-128bit-units,
last-completed-sequence, CPU-submit-us, CPU-wait-us,
status-before, status-after, timeout-or-recovered
```

When there is no validated way to read a hardware consumer sequence, log `UNKNOWN` rather than interpreting an arbitrary MMIO dword as a read pointer. Mark control words with the actual `HParaType`/sub-address pair so `0x74` cannot be confused with byte offset `0x74` in another aperture.

##### 21.2 Depth and stencil tests

```text
TEST, chip, test-id, clear-path, surface-format,
Z-mode-01:12, Z-test-01:13, fast-clear-01:27,
coarse-Z-01:29, stencil-mode, separate-stencil,
surface-width, pitch-bytes, tile-x, tile-y,
expected-depth, observed-depth,
expected-stencil, observed-stencil, first-bad-pixel
```

For the prior `8×8` anomalies, record the **count of failing pixels and which tiles** contain them, not only a global pass/fail. Store the post-clear register snapshot separately from the post-draw snapshot.

<a id="c9-section-33"></a>

#### 33. Prioritized test program after the cross-check

| Rank | Target | Experiment | Required pass criteria |
|---:|---|---|---|
| 1 | VX855 | Log `0x400` status with the OpenChrome H5 masks on every batch | Can distinguish CR, 2D, 3D and VQ states without using `0x43C` as a read pointer |
| 2 | VX855 | Reproduce the previous `0x41C/0x420` VQ sequence; record all decoded `10h:70h–74h` values | Sequence, addresses, effective allocation and buffer alignment all valid |
| 3 | VX855 | Resolve `VQ_SIZE>>3` vs 128-bit-manual-unit mismatch **using a guarded reserved buffer** | No out-of-range access; independent evidence of actual unit semantics |
| 4 | VX855 | Compare direct-MMIO Header-2 submission against true-VQ submission | Deterministic pixel equivalence and independent fetch/completion evidence |
| 5 | VX855 | Benchmark H6-inspired tiny solid/copy/composite threshold decisions | Whole-operation latency measured; no regression in pixel correctness |
| 6 | VX855 | Run 8-bpp GE-only fills and same-format copies independently of DDRAW HAL | Correct pixels/palette and successful return to 16-bpp mode |
| 7 | VX800 | Map BAR0/1, verify 8/16/32-bpp 2D H6 path | Fill/copy/ROP pixel matches under full masking |
| 8 | VX900 | Map BAR2 framebuffer/BAR1 MMIO, validate 2D first | No reserved `0x60–0x9F` dependency until independently proven |
| 9 | VX900 | Rework legacy vertex setup before game-style 3D | Flat untextured triangle correct, then textured/depth tests |
| 10 | All | Compare display FIFO and extended CRTC state under changing resolutions | Stable scanout without corruption during accelerated loads |
| 11 | All | Rebuild HQV capability table per chip; compare overlays and color-key | Video/overlay correctness across format, scaling and flip transitions |

##### 33.1 A concise log schema for the queue experiment

```text
CHIP,PCI_ID,PCI_REV,FB_BAR,MMIO_BAR,SUBMISSION_MODE,
VQ_RESERVED_BYTES,VQ_EFFECTIVE_BYTES,VQ_BASE,VQ_END,
CR10_70,CR10_71,CR10_72,CR10_73,CR10_74,
STATUS400_PRE,STATUS400_POST,STATUS400_TIMEOUT,
H1_WORDS,H2_WORDS,DIRECT_MMIO_WORDS,VQ_WORDS,
GUARD_FIRST_BAD_BYTE,COMPLETION_METHOD,PIXEL_MISMATCHES
```

**Minimum acceptance rule:** if no true-VQ producer or completion proof is available, report the result as **`QUEUE_CONFIGURED_NOT_VALIDATED`**, even when the same image renders successfully through direct MMIO.

<a id="c9-section-42"></a>

#### 42. New cross-platform regression tests (hardware-independent first)

| ID | Scope | Input/sequence | Expected invariant | Evidence source |
|---|---|---|---|---|
| `OCBUF-01` | OpenChrome static harness | 0, 1, 2 textures; buffer end boundary; H2 header reused/not reused | No allocated-buffer guard overwritten | `via_3d.c`, `via_dmabuffer.h` |
| `OCMASK-01` | OpenChrome static harness | 16-bpp mask `0x00FF` then full `0xFFFF` | Full mask clears byte-disable state | `via_exa_h6.c` |
| `OCMASK-02` | Compiler-level | 32-bpp plane mask, sanitizer build | No shift >= 32 or signed overflow | `via_exa_h6.c` |
| `OCBLT-01` | 2D GE | Source pitch 12, 20, 28 bytes versus aligned pitches | Reject invalid pitch or preserve exact rows | H6 copy + GE pitch manual |
| `OCSOLID-01` | X.Org environment | Depth-24/bpp-32 pixmap | Confirm hardware path selected or explicit fallback | H6 solid mode setup |
| `OCPARSE-01` | OpenChrome static harness | Invalid first command-buffer word | Parser exits with error, no spin | `via_exa.c` |
| `V9VTX-01` | VX900 hardware | Direct 3-vertex and textured-quad path | Correct pixels before indexed-VB support | manuals + `via_3d.c` |
| `V9VTX-02` | VX900 hardware | Separate `04h` vertex-input mappings | No legacy FVF word reuse | VX900 Part II pp. 153–159 |
| `VXGL-W-01` | VX855 hardware | Constant-W textured rectangles | Distinguish coordinate conventions and precision | `via_3d.c` |
| `VXBLEND-01` | VX855 hardware | A8 texture to 16-/32-bpp color target | Correct blending; measure full operation latency | `via_3d.c` format table |
| `VQ-UNITS-01` | VX855 hardware | Allocated length, programmed length and buffer-end guards | No out-of-bounds read; no claims from busy flag alone | Part III §26.3 |

For all physical-GPU tests, protect known-good display operation with a timeout and recovery/reset path. Log precise chip ID/revision, driver build, mode, memory layout and command submission route. Begin with no shared data corruption risk and no unvalidated writes to explicitly reserved registers.

<a id="c9-section-43"></a>

#### 43. Suggested Win98 source review order after the OpenChrome audit

**First, compare implementation patterns rather than copying Linux fixes into a different driver:**

1. **Command writer bounds:** locate all counted-word reserves, macros, packet headers, end/fire words, alignment dummies, and flush trailers. Compare worst-case emitted words to reserved capacity, especially two texture stages, long strips and dynamically varying vertex formats. This has the highest direct reliability impact.
2. **State hygiene:** verify full mask/state reset after partial masks, state changes between 2D and 3D, and every fallback to software. Avoid assuming the previous draw left benign state.
3. **Integer representation:** scan bit shifts by 24–32, pitch/offset conversion, `(size−1)` underflow, and intermediate 16-bit overflow. Use unsigned fixed-width arithmetic and explicit precondition checks.
4. **Software- versus GPU-side hangs:** separate CPU parser loops, MMIO wait expiration, queue producer advancement, and independent completion. Avoid treating 2D-cursor movement as proof that the 3D command stream continues.
5. **Common code versus backend:** prefer one shared H6 2D emitter for VX800/VX855/VX900 with capability gates; separate PCI BAR/display-init/HQV tables; separate VX900 vertex-format state; validate direct-vertex submission as a potentially shared restricted path.
6. **Throughput improvements:** time repeated solid/alpha quads and texture uploads including preparation overhead. A GPU path is a net improvement only when total latency and CPU starvation improve without pixels differing.

##### 43.1 Classification of the newly identified items

| ID | Assessment | Confident conclusion | Not established |
|---|---|---|---|
| `OC-01` two-texture reserve | **High-confidence source discrepancy** | Requested capacity can be 6 DWORDs below the next worst-case emitted payload | A physical buffer overflow was observed |
| `OC-02` stale mask | **High-confidence state logic issue** | Full mask does not unconditionally reset previous partial-mask control | Pixel corruption on a particular board was measured |
| `OC-03` 32-bit shift | **Definite C undefined behavior** | `1 << 32` is not valid 32-bit C integer arithmetic | A specific compiler output or color fault occurs |
| `OC-04` source pitch | **High-confidence encoding/precondition mismatch** | 4-byte pitch passes check, then low 3 bits are lost by `>>3` | EXA supplies misaligned pixmaps in normal workloads |
| `OC-05` depth/bpp selection | **Conditional performance fallback** | depth 24 is rejected by a helper supporting 8/16/32 | A particular user desktop is taking the fallback |
| `OC-06` parser error loop | **Definite non-advancing error branch** | Unexpected header word can spin the software parser | Previous VX855 hardware hangs were caused by it |
| `OC-07` wait timeout | **Observability/safety gap** | Timeout completion is not separately propagated | A particular hang is a timeout rather than slow progress |
| VX900 direct vertex | **Promising but untested compatibility hypothesis** | Common OpenChrome emitter and documented `00h` transport exist | All VX855 direct-vertex state works on VX900 |

<a id="compatibility-matrix"></a>

## Compatibility matrix and evidence ledger

The 172 baseline rows retain their original IDs and class totals. Their scope includes manual fields, compiled commands, and build-specific function-pointer identity. The supplemental V3 guard and the verification sources sit alongside the baseline.

### Cross-family synthesis and porting boundaries

<a id="c9-section-351"></a>

#### 351. Cross-family matrix methodology and scope

This is the **single evidence-graded compatibility inventory** of the currently audited VX800 HC3, VX855 HCM and VX900 HD source material. It consolidates eight independently archived source matrices covering **172 audited rows** in §§271–350, plus historical context from §§1–270. One row can describe a register, a command-stage literal, a compiled CE callback, or a documented field, so **the row count is not the number of unique GPU registers and not a hardware compatibility percentage**. This is a bounded reference for the *examined* instructions and operations, not a statement that the entire silicon has been exhaustively verified.

Source precedence: the **VIA Part I/II manuals** establish published bitfield semantics; original **VIA CE binary/linker map** establish actual compiled use; **OpenChrome** establishes another implementation where applicable. None establishes pixel equivalence, timing, reliable undocumented behavior, or safe programming of reserved bits without device tests. A shared register name is weaker than identical bitfields; the same CE function body is weaker than hardware-level equivalence; and a reserved label is not proof that functionality is absent.

<a id="c9-section-352"></a>

#### 352. Compatibility classification and totals for the examined rows

| Class | Scoped meaning | Rows |
|---|---|---:|
| `SHARED_ENCODE_OR_SOURCE` | Same *documented subset*, exact CE literal, shared compiled function, or same register name (scope field identifies which) | 84 |
| `MODIFIED_FIELD_OR_PROGRAM` | Same type of operation with different masks, fields, or packet sequencing | 10 |
| `GENERATION_SPECIFIC_DOCUMENTED` | A control/feature documented or emitted on only the specified generation(s) | 27 |
| `REDEFINED_SAME_ADDRESS` | Identical numerical register/selector but different documented meaning or encoding | 5 |
| `DOCUMENTATION_CONFLICT` | Manual-internal conflict or contradiction between a reserved label and compiled CE usage | 20 |
| `IMPLEMENTATION_DIVERGENCE` | Different compiled callbacks/backends; not evidence of silicon incompatibility | 19 |
| `UNRESOLVED_RUNTIME_OR_FULL_ENUM` | Insufficient evidence for full enum, runtime packet contents, or exact semantics | 7 |

**Interpretation:** `SHARED_ENCODE_OR_SOURCE` is not shorthand for "any old command word works on every chip". For instance the overlay block shares **64 address offsets and 60 summary labels** (§338), but that statement is not 64 validated bitfields. More than one matrix row can cover the same feature in different evidence contexts. The table is an **inventory**, not an inferred success rate.

<a id="c9-section-353"></a>

#### 353. High-impact register incompatibilities: do not blindly reuse old DWORDs

| Feature | VX800/VX855 | VX900 | Consequence |
|---|---|---|---|
| Type-10 `04h` | Interrupt-state-buffer address/location | Command-dispatch scheduling and video path | **Repurposed:** an unchanged word can mean something else (§§312–326) |
| Type-10 `65h` pause ID `01b` | Legacy immediate ring rewind/jump | Wait for pause-address update before jumping | **Semantic change** (§§312–326) |
| Type-11 `00h[10:0]` | Legacy queue depth/critical settings | Reassigned/reserved in published table | **Nonportable register field** (§§312–326) |
| Type-04 vertex declaration | H6 scalar FVF | H5S2 Float4/Float2 IR descriptors and vertex program | **Different command language** (§§271–299) |
| Type-04 `60h–62h` | No equivalent command in compared H6 blitter | VX900 writes them but manual calls them reserved | **Undocumented with direct binary use** (§§281–290) |
| GE opcode `0011b` | Reserved | AlphaBLT / ROP CC | **VX900 documented 2D extension** (§§300–311) |
| GE source/dst location `11b` | Reserved on VX800; L.L. on VX855 | L.L. | **Family-specific GE location**; does not extend to texture-stage location fields (§§300–336) |
| Texture Type-02 FE `00h[8]` | VX800 `texldp` check | VX855 and VX900 reserve bit | **Do not carry bit across generations** (§§327–336) |
| HQV `0x3BC` | VX800 deblocking factor | VX855 segment-residue buffer | VX900 later HQV chapter maps segment-residue; earlier chapter conflicts (§§337–350) |
| HQV `0x3E0` | VX800 source line/fetch | VX855 subpicture horizontal scaling | VX900 late HQV chapter maps subpicture scaling; early chapter conflicts (§§337–350) |

<a id="c9-section-354"></a>

#### 354. VX800-to-VX855 migration: most reusable commands and unresolved notes

The examined original CE driver routes **both `1106:1122` and `1106:5122`** through the *same* H6 3D initializer, blitter, and textured-draw helper (§§291–299). Most shared H6 Type-04 FVF attributes and Type-10 regulator fields therefore have a credible source-level portability baseline. Some nonzero-mode 2D callbacks also share addresses, but others do not (§§300–311). Differences needing explicit chip-aware handling include **GE location `11b`** (reserved for VX800), **VX855 CFC fields**, **VX855 Type-10/11 extensions**, and **HQV crop/color/scale address reinterpretation**. The two H6 manuals differ in their prose about vertex-memory placement; absent runtime evidence, that is a documentation discrepancy, not a demonstrated silicon limitation.

<a id="c9-section-355"></a>

#### 355. VX855-to-VX900 migration: retain shared pixel commands, replace vertex routing

The selected VIA CE 3D blit paths reuse **byte-identical render-state templates, pixel-shader ALU/TAU program tables and selected primitive control literals** (§§271–280). The VX900 H5S2 path instead changes **vertex layout**, supplies IR-routing descriptors, emits Type-03 subtype `31h` vertex-program records, writes reserved-described Type-04 `60h–62h`, and emits additional post-draw wait/serialization packets. **Identical CE pixel-program bytes are not a proof of general VX900 pixel-ISA behavior**: its detailed manual reserves several pixel-shader registers that the CE blitter nevertheless uses (§§327–336). An H6 draw stream is *not* directly portable to H5S2 just because its texture and pixel words happen to match.

<a id="c9-section-356"></a>

#### 356. Two-dimensional graphics: shared opcodes versus non-shared CE backends

The hardware GE control fields in the examined manuals preserve BitBLT, mono text, line and rotation selector **encodings** across three families, while VX900 documents AlphaBLT and VX855/VX900 document further memory/CFC choices (§§300–311). Both CE builds have **28 named callback slots** per family (11 zero-mode, 11 nonzero-mode and 6 unconditional). The embedded matrix records CE 6.0. CE 7 changes the sharing pattern in 11 slots (see §367). A different callback address does **not** imply a different hardware opcode; it may adapt setup, source packing or timing. Conversely identical callback code does **not** establish pixel-perfect equivalence at all modes. The detailed callback IDs are in the 172-row table below.

<a id="c9-section-357"></a>

#### 357. Command-regulator portability boundaries

The common Type-10 start/pause/jump/trigger and Type-11 foundations are real, but **register identity and field identity must be checked separately**. Highest-risk Type-10 differences are `04h` (repurposed), `65h` pause ID `01b` (semantic difference), VX900 preemption/arbitration subaddresses, `68h[5]` and queue restrictions. Type-11 `00h[16]` is shared, while VX900 adds `00h[17]`; the low eleven bits of `00h` differ in published meaning. Type-10 `73h` defines VQ length in 128-bit units across the manuals. The binary CE VX900 init **still writes `0x69000000`**, even though the VX900 manual lacks a Type-10 `69h` entry (§§312–326): never equate a reserved/absent documentation label with an impossible register write.

<a id="c9-section-358"></a>

#### 358. Texture and shader portability boundaries

Shared Type-02 formats and sampler field layouts make many common texture setups reusable in principle: basic address/size, filtering, LOD, clamp/wrap, and named DXT/RGB formats. Yet this is a *documented subset*, not exhaustive verification of every format selector or filtered output. Border and Z/PCF pseudocode differs among manuals; VX800 `texldp` bit 8 is reserved later. The VX900 detailed manual labels several pixel-shader Type-01/03 fields reserved while VIA CE compiles them into working-path emitter commands; **the conflict is documented, hardware output is not** (§§327–336). VX900 Type-03 `31h` is identified as VS instruction in its command summary but marked reserved in detail (§§281–290).

<a id="c9-section-359"></a>

#### 359. Video-overlay and HQV: address parity is not semantic parity

The video display summaries list **64 matching offsets** and **60 matching labels** on all three families. VX900 documents V1 scaling on IGA1 at `0x230[1]`, the V1 length guard at `0x238[31]`, and the V3 length guard at `0x2A8[31]`. HQV is a sharper break: VX800 older first-bank mappings diverge from VX855 crop and subpicture scales; the VX900 manual reproduces the old labels in its video chapter but assigns newer and sometimes different names in its later HQV chapter. `+0x1000` second-engine bank and `+0x2000` command-routed write aperture are **different mechanisms**, not a genuine offset contradiction (§§337–350). A safe implementation must choose the HQV generation and resolve its active bank/decoder mode, not just program an offset that happens to exist on all three.

### Register, command and callback matrix

<a id="c9-section-361"></a>

#### 361. Compatibility matrix — 3D, H6 and VX900 vertex routing

##### 3D sequence — 21 audited rows

The table preserves the CE comparison baseline. For C9-013 and C9-014, the XP binary independently establishes parts of the Type-04 encoding and the counted four-DWORD instruction upload ([§372](#c9-section-372)).

| ID | Register / operation | VX800 | VX855 | VX900 | Classification | Evidence scope |
|---|---|---|---|---|---|---|
| C9-001 | 3D stage 00 · device dispatch | 1122h/5122h | 1122h/5122h | 7122h | IMPLEMENTATION_DIVERGENCE | CE binary: matched static stages and branch-conditioned literals |
| C9-002 | 3D stage 01 · buffer reservation | FE02+manager 0x438/0x500 (branch-dependent) | FE02+manager 0x438/0x500 (branch-dependent) | FE02+manager 0x438/0x500 (branch-dependent) | SHARED_ENCODE_OR_SOURCE | CE binary: matched static stages and branch-conditioned literals |
| C9-003 | 3D stage 02 · target state and alpha mode | 37 or 50 DWORDs from same VA | 37 or 50 DWORDs from same VA | same 37 or 50 DWORDs from same VA | SHARED_ENCODE_OR_SOURCE | CE binary: matched static stages and branch-conditioned literals |
| C9-004 | 3D stage 03 · texture target parameters | multiple dynamic dimensions/pitch/address words | multiple dynamic dimensions/pitch/address words | matching logical fields generated by code | UNRESOLVED_RUNTIME_OR_FULL_ENUM | CE binary: matched static stages and branch-conditioned literals |
| C9-005 | 3D stage 04 · PS setup registers | 12-word table 0x10008770 or 0x100088EC | 12-word table 0x10008770 or 0x100088EC | same 12-word table | SHARED_ENCODE_OR_SOURCE | CE binary: matched static stages and branch-conditioned literals |
| C9-006 | 3D stage 05 · vertex word count; short branch | 00A80006 | 00A80006 | 00A80106 | MODIFIED_FIELD_OR_PROGRAM | CE binary: matched static stages and branch-conditioned literals |
| C9-007 | 3D stage 06 · vertex declaration 01 | 01020100 | 01020100 | 010000C2 | REDEFINED_SAME_ADDRESS | CE binary: matched static stages and branch-conditioned literals |
| C9-008 | 3D stage 07 · vertex declaration 02 | 02090803 | 02090803 | 02000142 | REDEFINED_SAME_ADDRESS | CE binary: matched static stages and branch-conditioned literals |
| C9-009 | 3D stage 08 · vertex declaration 03 short | 03000000 | 03000000 | 03000000 | SHARED_ENCODE_OR_SOURCE | CE binary: matched static stages and branch-conditioned literals |
| C9-010 | 3D stage 09 · primitive controls | 201C5C00;23002E02;24000003 | 201C5C00;23002E02;24000003 | same three literals | SHARED_ENCODE_OR_SOURCE | CE binary: matched static stages and branch-conditioned literals |
| C9-011 | 3D stage 10 · vertex mask+FVF short | 25F0C000;28F0C000 | 25F0C000;28F0C000 | 25F0C000;28F0C000 | SHARED_ENCODE_OR_SOURCE | CE binary: matched static stages and branch-conditioned literals |
| C9-012 | 3D stage 11 · shared Type-04 state | 32 DWORD table 0x100087A0 | 32 DWORD table 0x100087A0 | same 32 DWORD table | SHARED_ENCODE_OR_SOURCE | CE binary: matched static stages and branch-conditioned literals |
| C9-013 | 3D stage 12 · H5S2 input-routing trio | none observed at corresponding stage | none observed at corresponding stage | 60800001 / 61800000 / 62104001 | GENERATION_SPECIFIC_DOCUMENTED | Emitted in CE 6.0 and CE 7. VX900 Part II PDF p. 179 reserves Type-04 `53h–62h` in detail. Field meanings are unresolved. |
| C9-014 | 3D stage 13 · H5S2 candidate VS | none observed at corresponding stage | none observed at corresponding stage | 03:31 8 DWORDs, 0x1000870C | GENERATION_SPECIFIC_DOCUMENTED | CE binary: matched static stages and branch-conditioned literals |
| C9-015 | 3D stage 14 · PS ALU short | 03:20 with 4-word program 0x1000875C | 03:20 with 4-word program 0x1000875C | same short 4-word program | SHARED_ENCODE_OR_SOURCE | CE binary: matched static stages and branch-conditioned literals |
| C9-016 | 3D stage 15 · PS TAU short | 03:21, 1 program word | 03:21, 1 program word | same program word | SHARED_ENCODE_OR_SOURCE | CE binary: matched static stages and branch-conditioned literals |
| C9-017 | 3D stage 16 · optional dynamic PS constant | 03:22 255/alpha conditional | 03:22 255/alpha conditional | same generator | SHARED_ENCODE_OR_SOURCE | CE binary: matched static stages and branch-conditioned literals |
| C9-018 | 3D stage 17 · shared pixel-input/texture words | dynamic draw-state fields | dynamic draw-state fields | logical matching formulae | UNRESOLVED_RUNTIME_OR_FULL_ENUM | CE binary: matched static stages and branch-conditioned literals |
| C9-019 | 3D stage 18 · vertex payload | 6 DWORD per vertex, dynamic positions + ST | 6 DWORD per vertex, dynamic positions + ST | 6 DWORD per vertex, dynamic positions + ST | UNRESOLVED_RUNTIME_OR_FULL_ENUM | CE binary: matched static stages and branch-conditioned literals |
| C9-020 | 3D stage 19 · post-draw serialization | FE02:420 with offset41C,value00300000 | FE02:420 with offset41C,value00300000 | VX900_WAITPATCH 8x FE00:6C/00800000; final FE00:6C/00FC0000 | MODIFIED_FIELD_OR_PROGRAM | CE binary: matched static stages and branch-conditioned literals |
| C9-021 | 3D stage 20 · submit/flush | cmSmartFlush or KickOffCmd_Inv | cmSmartFlush or KickOffCmd_Inv | same dispatch entrypoints | SHARED_ENCODE_OR_SOURCE | CE binary: matched static stages and branch-conditioned literals |


##### H6 pair — 10 audited rows

| ID | Register / operation | VX800 | VX855 | VX900 | Classification | Evidence scope |
|---|---|---|---|---|---|---|
| C9-022 | 3D utility dispatch | 0x1122 | 0x5122 | H5S2 separate backend (see 3D stages) | SHARED_ENCODE_OR_SOURCE | CE binary: identical compiled routine |
| C9-023 | 3D init | Init3DHWRegs_H6 0x10063470 | same VA | H5S2 separate backend (see 3D stages) | SHARED_ENCODE_OR_SOURCE | CE binary: identical compiled routine |
| C9-024 | 3D blit | fn3DBlt_H6 0x10064503 | same VA | H5S2 separate backend (see 3D stages) | SHARED_ENCODE_OR_SOURCE | CE binary: identical compiled routine |
| C9-025 | textured draw | Texture3D_H5 0x10067255 | same VA | H5S2 separate backend (see 3D stages) | SHARED_ENCODE_OR_SOURCE | CE binary: identical compiled routine |
| C9-026 | Type04:00 vertex length | bits5:0 DWORDs | bits5:0 DWORDs | not compared by this VX800/VX855 pair audit | SHARED_ENCODE_OR_SOURCE | VIA manual: selected H6 bitfields/notes |
| C9-027 | Type04:00 vertex location | bits16:15, SL/SF | bits16:15, SL/SF | not compared by this VX800/VX855 pair audit | SHARED_ENCODE_OR_SOURCE | VIA manual: selected H6 bitfields/notes |
| C9-028 | Type04:00 vertex placement note | all vertex buffers SF | all buffers LL, SF, or all LL | not compared by this VX800/VX855 pair audit | DOCUMENTATION_CONFLICT | VIA manual: selected H6 bitfields/notes |
| C9-029 | Type04:01-03 FVF attrs | scalar X/Y/Z/W/ST encoding | same scalar FVF scheme | not compared by this VX800/VX855 pair audit | SHARED_ENCODE_OR_SOURCE | VIA manual: selected H6 bitfields/notes |
| C9-030 | Type10:73 queue length | 128-bit units, minimum 0x800 | 128-bit units, minimum 0x800 | not compared by this VX800/VX855 pair audit | SHARED_ENCODE_OR_SOURCE | VIA manual: selected H6 bitfields/notes |
| C9-031 | Type10:70-7C CMDQ fields | documented common group | documented common group | not compared by this VX800/VX855 pair audit | SHARED_ENCODE_OR_SOURCE | VIA manual: selected H6 bitfields/notes |


##### Vertex routing — 2 audited rows

| ID | Register / operation | VX800 | VX855 | VX900 | Classification | Evidence scope |
|---|---|---|---|---|---|---|
| C9-032 | VX900 vertex-program compact profile | H6: no corresponding program in matched path | H6: no corresponding program in matched path | IR 2; vertex DWORDs 6; Type-04:00 00A80106, :60 60800001, :61 61800000, :62 62104001; 8 VS DWORDs | GENERATION_SPECIFIC_DOCUMENTED | CE 6.0 and CE 7 each show three upload sites total. Two are in the H5S2 blitter, and the third is in `Texture3D_H5S2` and repeats the compact profile. |
| C9-033 | VX900 vertex-program extended profile | H6: no corresponding program in matched path | H6: no corresponding program in matched path | IR 3; vertex DWORDs 8; Type-04:00 00A80208, :60 60800002, :61 61800000, :62 62204002; 12 VS DWORDs | GENERATION_SPECIFIC_DOCUMENTED | Extended profile in one H5S2 blitter branch in each checked CE build. See §§284–286 and §366. |

<a id="c9-section-362"></a>

#### 362. Compatibility matrix — 2D GE and CE callbacks

##### 2D GE — 14 audited rows

| ID | Register / operation | VX800 | VX855 | VX900 | Classification | Evidence scope |
|---|---|---|---|---|---|---|
| C9-034 | GE+0x00 opcode 0001 | BitBLT | BitBLT | BitBLT | SHARED_ENCODE_OR_SOURCE | VIA manuals: GE bitfield/opcode |
| C9-035 | GE+0x00 opcode 0010 | mono text | mono text | mono text | SHARED_ENCODE_OR_SOURCE | VIA manuals: GE bitfield/opcode |
| C9-036 | GE+0x00 opcode 0011 | reserved | reserved | AlphaBLT, ROP CC | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: GE bitfield/opcode |
| C9-037 | GE+0x00 opcode 0101 | line | line | line | SHARED_ENCODE_OR_SOURCE | VIA manuals: GE bitfield/opcode |
| C9-038 | GE+0x00 opcodes 1001/1010 | BLT/text then rotate | BLT/text then rotate | BLT/text then rotate | SHARED_ENCODE_OR_SOURCE | VIA manuals: GE bitfield/opcode |
| C9-039 | GE+0x04 bits1:0 | rotation | rotation | rotation | SHARED_ENCODE_OR_SOURCE | VIA manuals: GE bitfield/opcode |
| C9-040 | GE+0x04 alpha fields | not documented | not documented | bit20 selector; bits19:12 constant alpha | DOCUMENTATION_CONFLICT | VIA manuals: GE bitfield/opcode |
| C9-041 | GE+0x08 source/pattern/dst location 11b | reserved | L.L. | L.L. | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: GE bitfield/opcode |
| C9-042 | GE+0x30 color conversion | Reserved | 16-/32-bpp CFC. ROP CC only. 8 bpp excluded. | 16-/32-bpp format fields. 8-bpp source-depth code listed. Conversion behavior and ROP limits unresolved. | GENERATION_SPECIFIC_DOCUMENTED | Part I VX800 PDF p. 87, VX855 p. 84, VX900 p. 107. |
| C9-043 | GE+0x48 key-type bit13 | programmable | reserved; fixed source nonmatch | programmable | MODIFIED_FIELD_OR_PROGRAM | Manual field definitions. CE 7 nonzero-mode screen copy ORs `0x4000` on VX800/VX855 and `0x6000` on VX900 (§367). |
| C9-044 | GE+0x24/0x28 mono pattern | pattern words | pattern words | pattern words | SHARED_ENCODE_OR_SOURCE | VIA manuals: register name/offset only |
| C9-045 | GE+0x40/0x44 clipping | same coordinates | same coordinates | same coordinates | SHARED_ENCODE_OR_SOURCE | VIA manuals: register name/offset only |
| C9-046 | CE selected CPU 15/16/32 conversions | same callbacks | same callbacks | same callbacks | SHARED_ENCODE_OR_SOURCE | CE/OpenChrome source: exact routine or emitter |
| C9-047 | OpenChrome H6 fill/copy | shared emitter | shared emitter | shared emitter | SHARED_ENCODE_OR_SOURCE | CE/OpenChrome source: exact routine or emitter |


##### CE 6.0 retail 2D callbacks — 28 audited rows

| ID | Register / operation | VX800 | VX855 | VX900 | Classification | Evidence scope |
|---|---|---|---|---|---|---|
| C9-048 | zero mode · Color pattern fill | 0x100254E5 (vPatFillVX800_AGP/vPatFillVX855_AGP) | 0x100254E5 (vPatFillVX800_AGP/vPatFillVX855_AGP) | 0x100254E5 (vPatFillVX800_AGP/vPatFillVX855_AGP) | SHARED_ENCODE_OR_SOURCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-049 | zero mode · Patterned mono transfer | 0x100194B3 (vPatMonoXferVX800_AGP) | 0x1002428A (vPatMonoXferVX855_AGP) | 0x1002730C (vPatMonoXferVX900_AGP) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-050 | zero mode · Solid fill | 0x10024EB3 (vSolidFillVX800_AGP/vSolidFillVX855_AGP) | 0x10024EB3 (vSolidFillVX800_AGP/vSolidFillVX855_AGP) | 0x10024EB3 (vSolidFillVX800_AGP/vSolidFillVX855_AGP) | SHARED_ENCODE_OR_SOURCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-051 | zero mode · Mono host transfer | 0x100180A0 (vMonoXferVX800_AGP) | 0x100231AC (vMonoXferVX855_AGP) | 0x1002635C (vMonoXferVX900_AGP) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-052 | zero mode · Image pattern fill | 0x1001B160 (vImagePatFillVX800_AGP/vImagePatFillVX855_AGP) | 0x1001B160 (vImagePatFillVX800_AGP/vImagePatFillVX855_AGP) | 0x100285FA (vImagePatFillVX900_AGP) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-053 | zero mode · Patterned 8-bpp transfer | 0x10019F3B (vPat8BppXferVX800_AGP/vPat8BppXferVX855_AGP) | 0x10019F3B (vPat8BppXferVX800_AGP/vPat8BppXferVX855_AGP) | 0x10027900 (vPat8BppXferVX900_AGP) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-054 | zero mode · 8-bpp host transfer | 0x10018463 (v8BppXferVX800_AGP) | 0x100237F7 (v8BppXferVX855_AGP) | 0x100269B2 (v8BppXferVX900_AGP) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-055 | zero mode · 8-bpp host-image writer | 0x10021E8A (v8BppImageTransferCN896_AGP) | 0x10021E8A (v8BppImageTransferCN896_AGP) | 0x10028D7B (v8BppImageTransferVX900_AGP) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-056 | zero mode · Native pixel transfer | 0x10018D45 (vNativeXferVX800_AGP) | 0x10023B29 (vNativeXferVX855_AGP) | 0x10026FB5 (vNativeXferVX900_AGP) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-057 | zero mode · Screen-to-screen copy | 0x1002594F (vScreenToScreenBltVX800_AGP/vScreenToScreenBltVX855_AGP) | 0x1002594F (vScreenToScreenBltVX800_AGP/vScreenToScreenBltVX855_AGP) | 0x100281B3 (vScreenToScreenBltVX900_AGP) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-058 | zero mode · Raw host-image writer | 0x10021D13 (vImageTransferCN896_AGP) | 0x10021D13 (vImageTransferCN896_AGP) | 0x100289C2 (vImageTransferVX900_AGP) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-059 | nonzero mode · Color pattern fill | 0x1001A50A (vPatFillVX800) | 0x10025176 (vPatFillVX855) | 0x10025176 (vPatFillVX855) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-060 | nonzero mode · Patterned mono transfer | 0x1001903D (vPatMonoXferVX800) | 0x10023E21 (vPatMonoXferVX855) | 0x10023E21 (vPatMonoXferVX855) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-061 | nonzero mode · Solid fill | 0x10024CDE (vSolidFillVX800/vSolidFillVX855) | 0x10024CDE (vSolidFillVX800/vSolidFillVX855) | 0x10024CDE (vSolidFillVX800/vSolidFillVX855) | SHARED_ENCODE_OR_SOURCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-062 | nonzero mode · Mono host transfer | 0x10017E32 (vMonoXferVX800) | 0x10022F11 (vMonoXferVX855) | 0x10022F11 (vMonoXferVX855) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-063 | nonzero mode · Image pattern fill | 0x1001AB90 (vImagePatFillVX800) | 0x10025D98 (vImagePatFillVX855) | 0x10025D98 (vImagePatFillVX855) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-064 | nonzero mode · Patterned 8-bpp transfer | 0x10019A9C (vPat8BppXferVX800) | 0x10024879 (vPat8BppXferVX855) | 0x10024879 (vPat8BppXferVX855) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-065 | nonzero mode · 8-bpp host transfer | 0x1002356F (v8BppXferVX800/v8BppXferVX855) | 0x1002356F (v8BppXferVX800/v8BppXferVX855) | 0x10026724 (v8BppXferVX900) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-066 | nonzero mode · 8-bpp host-image writer | 0x1001FE44 (v8BppImageTransferCX700) | 0x1001FE44 (v8BppImageTransferCX700) | 0x1001FE44 (v8BppImageTransferCX700) | SHARED_ENCODE_OR_SOURCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-067 | nonzero mode · Native pixel transfer | 0x10018AFD (vNativeXferVX800/vNativeXferVX855) | 0x10018AFD (vNativeXferVX800/vNativeXferVX855) | 0x10026D6D (vNativeXferVX900) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-068 | nonzero mode · Screen-to-screen copy | 0x1001A8B1 (vScreenToScreenBltVX800/vScreenToScreenBltVX855) | 0x1001A8B1 (vScreenToScreenBltVX800/vScreenToScreenBltVX855) | 0x10027ED4 (vScreenToScreenBltVX900) | IMPLEMENTATION_DIVERGENCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-069 | nonzero mode · Raw host-image writer | 0x1001FD64 (vImageTransferCX700) | 0x1001FD64 (vImageTransferCX700) | 0x1001FD64 (vImageTransferCX700) | SHARED_ENCODE_OR_SOURCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-070 | common mode · Blank rectangle (shared) | 0x10026045 (vBlankRectangleVX800/vBlankRectangleVX855) | 0x10026045 (vBlankRectangleVX800/vBlankRectangleVX855) | 0x10026045 (vBlankRectangleVX800/vBlankRectangleVX855) | SHARED_ENCODE_OR_SOURCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-071 | common mode · Fast 8-bpp copy (shared) | 0x10018795 (v8BppXferFastCopyCX700/v8BppXferFastCopyVX800) | 0x10018795 (v8BppXferFastCopyCX700/v8BppXferFastCopyVX800) | 0x10018795 (v8BppXferFastCopyCX700/v8BppXferFastCopyVX800) | SHARED_ENCODE_OR_SOURCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-072 | common mode · CPU conversion 16->32 | 0x100169FE (EmulatedBltSrcCpy16to32) | 0x100169FE (EmulatedBltSrcCpy16to32) | 0x100169FE (EmulatedBltSrcCpy16to32) | SHARED_ENCODE_OR_SOURCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-073 | common mode · CPU conversion 15->16 | 0x10017033 (EmulatedBltSrcCpy15to16) | 0x10017033 (EmulatedBltSrcCpy15to16) | 0x10017033 (EmulatedBltSrcCpy15to16) | SHARED_ENCODE_OR_SOURCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-074 | common mode · CPU conversion 15->32 | 0x10016E06 (EmulatedBltSrcCpy15to32) | 0x10016E06 (EmulatedBltSrcCpy15to32) | 0x10016E06 (EmulatedBltSrcCpy15to32) | SHARED_ENCODE_OR_SOURCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |
| C9-075 | common mode · CPU conversion 16->15 | 0x10016C2B (EmulatedBltSrcCpy16to15) | 0x10016C2B (EmulatedBltSrcCpy16to15) | 0x10016C2B (EmulatedBltSrcCpy16to15) | SHARED_ENCODE_OR_SOURCE | CE 6.0 retail: callback pointer equality for this build. See §367 for CE 7. |

<a id="c9-section-363"></a>

#### 363. Compatibility matrix — command regulator, textures, display and HQV

##### Type-10/11 — 31 audited rows

| ID | Register / operation | VX800 | VX855 | VX900 | Classification | Evidence scope |
|---|---|---|---|---|---|---|
| C9-076 | T10:00 · PCI command packaging | Enable and ECLK timeout | Same fields | Same fields | SHARED_ENCODE_OR_SOURCE | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-077 | T10:02 · Register readback IDs | ID 0-6 documented | Adds ID7 perf profile counters | ID7 performance counters | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-078 | T10:04 · Core command interpretation | Interrupt state-buffer lower address/location | Same interrupt state buffer | Command dispatch mode + HQV data-path select | REDEFINED_SAME_ADDRESS | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-079 | T10:05-07 · Interrupt state buffer/stop | Buffer high + 3D save; stop threshold; interrupt stop/pause | Same legacy set | Explicitly reserved | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-080 | T10:08-0B · Preemption controls | Not in Type10 list | Not in Type10 list | Fence addr, ID, mode, preemption trigger | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-081 | T10:0C-0E · Command arbitration / CRMI | Not in Type10 list | Not in Type10 list | Priority masks for main/VQ/submain/video/graphics/decoder/writeback | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-082 | T10:60 · AGP start address location | 00 SL;01 SF;10 SM;11 reserved | Same | 00 SL;01 SF;10 SM;11 LL | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-083 | T10:61-64 · AGP start/end/pause address | 128-bit-aligned 32-bit addresses | Same | Same broad format | SHARED_ENCODE_OR_SOURCE | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-084 | T10:65 PID00 · Normal pause | pause; resume next address | Same | pause until pause address changed | SHARED_ENCODE_OR_SOURCE | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-085 | T10:65 PID01 · Auto wrap/jump | immediately continue from start with no wait | Same | Waits for next pause-address command with ID `00b` or `10b`, then jumps to start | REDEFINED_SAME_ADDRESS | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-086 | T10:65 PID10 · Stop | requires new 68h trigger | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-087 | T10:66-67 · Jump address | 32-bit 128-bit-aligned jump | Same | Same documented field | SHARED_ENCODE_OR_SOURCE | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-088 | T10:68[21:16] · AGP read threshold | 6-bit default 8 | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-089 | T10:68[5] · 2D AGP Header1 data width | Reserved | Reserved | 0 = 128 bits, 1 = 32 bits | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-090 | T10:68[4:0] · Trigger/restore/clear | Fence clear, AGP clear, restore, trigger | Same | Same broad fields | SHARED_ENCODE_OR_SOURCE | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-091 | T10:69 · FE8x branch enable | bit0 explicit | bit0 explicit | Omitted from Type-10 chapter. Both CE builds write `0x69000000`. | DOCUMENTATION_CONFLICT | VIA manuals and CE 6.0 / CE 7 initialization. Hardware effect of the write is unresolved (§366). |
| C9-092 | T10:6C-6F · Interrupt/AGP buffer restore addresses | Present | Present | Not listed in VX900 Type10 chapter | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-093 | T10:70-72 · Virtual command queue address | 128-bit alignment, 11 loc reserved | Same | Same base fields, 11 loc LL | MODIFIED_FIELD_OR_PROGRAM | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-094 | T10:73 · Virtual queue length | 128-bit units; minimum 0x800 | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-095 | T10:74 · VQ thresholds and enable | FIFO thresholds/read request, queue enable | Same | Same broad fields; FE8x forbidden with VQ1 | MODIFIED_FIELD_OR_PROGRAM | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-096 | T10:78-7C · Separate 2D/3D queue | Documented | Documented | Not listed in VX900 Type10 | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-097 | T11:00[16] · 2D/3D request lock | Present | Present | Present | SHARED_ENCODE_OR_SOURCE | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-098 | T11:00[17] · Hardware 2D/3D lock | Reserved | Reserved | New explicit HW lock, default 1 | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-099 | T11:00[10:0] · FIFO depth and critical commands | Three FIFO depths and critical bit0 | Same | Reserved 15:0 (no old controls documented) | REDEFINED_SAME_ADDRESS | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-100 | T11:03 · Engine-specific fence wait mask | Not documented | Not documented | Individual 3D/2D/VD/HQV/DMA idle and EOF waits | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-101 | T11:04-07 · Fence base/ID/mode | Fence mode common | Same | Same broad fields; wait mask separately configured | MODIFIED_FIELD_OR_PROGRAM | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-102 | T11:08-0B · Save state command | Save addr + trigger bit0; old interrupt-save priority | Same legacy priority | Same addr/trigger, priority of new safe trig over save-enable | MODIFIED_FIELD_OR_PROGRAM | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-103 | T11:10-34 · Auto framebuffer swapping | IGA1,IGA2,Duo View; 11 loc reserved | Same | Similar layout, 11 loc LL | MODIFIED_FIELD_OR_PROGRAM | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-104 | T11:68-6B · Traditional branch | FE8x, 16-bit index alignment, no nesting | Same | Same basic traditional branch, but LL location new | MODIFIED_FIELD_OR_PROGRAM | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-105 | T11:6D-6F · Second branch group | Not documented | Second branch addresses/size | Not listed; only traditional branch 68-6B | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: command-regulator registers; CE write evidence where noted |
| C9-106 | T10:04 switch · Switch vs classic wait/branch | N/A | N/A | Manual says command switch disables traditional wait/fence/branch | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: command-regulator registers; CE write evidence where noted |


##### Texture/PS — 25 audited rows

| ID | Register / operation | VX800 | VX855 | VX900 | Classification | Evidence scope |
|---|---|---|---|---|---|---|
| C9-107 | Type02 stages 00-0F · Stage subtype namespace | 0x00-0x0F | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: Type-02 texture/sampler fields |
| C9-108 | Type-02 stage subaddresses `00h–05h` · level-0 base addresses for faces 0–5 | A31:A8, 256-byte units. Face 2 has a zero-programming note. | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA Part II VX800 PDF p. 105, VX855 pp. 122–123, VX900 pp. 127–128. |
| C9-109 | Type02 stage 18 · Texture mode / face locations | SL SF; SM and 11 reserved | Same | 11 labeled reserved (L.L.) | SHARED_ENCODE_OR_SOURCE | VIA manuals: Type-02 texture/sampler fields |
| C9-110 | Type02 stage 20 · Width & height | 12-bit width/height, max 2048 | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: Type-02 texture/sampler fields |
| C9-111 | Type02 stage 21 · 3D volume texture length | 12-bit length, no mip for volume | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: Type-02 texture/sampler fields |
| C9-112 | Type02 stage 22 · Linear/tile, pow2, size exponents | tile and 8/16-texel height, exponents 0-11 | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: Type-02 texture/sampler fields |
| C9-113 | Type02 stage 30 · Texture format | Shared format class and channel encoding | Same broad table | Same broad table | UNRESOLVED_RUNTIME_OR_FULL_ENUM | VIA manuals: Type-02 texture/sampler fields |
| C9-114 | Type02 sampler 30 · MIP min/max and LOD offset | 5.5 signed LOD offset | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: Type-02 texture/sampler fields |
| C9-115 | Type02 sampler 31 · Anisotropy and filter | Max anisotropy 0-16; nearest/linear/anisotropic/4x4 | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: Type-02 texture/sampler fields |
| C9-116 | Type02 sampler 32 · S/T/R mapping | Border/clamp/repeat/mirror/mirror once | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: Type-02 texture/sampler fields |
| C9-117 | Type02 sampler 35-36 · Border color payload | 2 x s1.10 per register | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: Type-02 texture/sampler fields |
| C9-118 | Type02 sampler 36 note · Border-color filtering exception | Special L16/VU16/etc cases | Unconditional filtered-border text + .24 branch | Format-specific + Z/PCF conditional branches | MODIFIED_FIELD_OR_PROGRAM | VIA manuals: Type-02 texture/sampler fields |
| C9-119 | Type02 FE:00[8] · texldp divisor/mantissa check | Bit8 explicitly documented | Reserved (23:8) | Reserved (23:8) | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: Type-02 texture/sampler fields |
| C9-120 | Type02 FE:00[7:0] · Stage count, texture FIFO route, fetch & clear | Number max8; 3 FIFO modes; 2/4 QW fetch; cache clear | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: Type-02 texture/sampler fields |
| C9-121 | Type02 FE:01-03 · Texture source / dimensionality | Selection/perpective/dim for up to 8 | Same architecture | Same architecture | SHARED_ENCODE_OR_SOURCE | VIA manuals: Type-02 texture/sampler fields |
| C9-122 | CE Texture3D implementations · Active helper implementation | VX800 -> H5 | VX855 -> same H5 | VX900 -> H5S2 | IMPLEMENTATION_DIVERGENCE | VIA manuals: Type-01/02/03; CE binary for shader conflict |
| C9-123 | Type01:01[10] · Pixel shader enable | Bit 10: enable pixel shader | Same | Bit 10 reserved | DOCUMENTATION_CONFLICT | VIA manuals: Type-01/02/03; CE binary for shader conflict |
| C9-124 | Type01:90-9A · Pixel shader control registers | 11 PS setting fields documented | Same | Entire range reserved in detailed manual | DOCUMENTATION_CONFLICT | VIA manuals: Type-01/02/03; CE binary for shader conflict |
| C9-125 | Type03:20 · Pixel shader ALU program | 4 DWORD/instruction; up to 96 instructions | Same | Reserved in detailed manual | DOCUMENTATION_CONFLICT | VIA manuals: Type-01/02/03; CE binary for shader conflict |
| C9-126 | Type03:21 · Pixel shader TAU program | 32 x 32-bit instructions | Same | Reserved in detailed manual | DOCUMENTATION_CONFLICT | VIA manuals: Type-01/02/03; CE binary for shader conflict |
| C9-127 | Type03:22 · Pixel shader float constants | 4 x 32-bit components; float to internal 24-bit | Same | Reserved in detailed manual | DOCUMENTATION_CONFLICT | VIA manuals: Type-01/02/03; CE binary for shader conflict |
| C9-128 | Type03 command-regulator summary · Pixel ALU/TAU/constant subtype names | 20/21/22 named | Same | 20/21/22 named in summary despite detailed reserved labels | DOCUMENTATION_CONFLICT | VIA manuals: Type-01/02/03; CE binary for shader conflict |
| C9-129 | Type02:30 examples · Compressed formats | DXT1; DXT2/3; DXT4/5 | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: Type-02 texture/sampler fields |
| C9-130 | Type02:30 examples · Common color / single-channel formats | RGB565; ARGB8888; ABGR8888; L16 | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: Type-02 texture/sampler fields |
| C9-131 | Type02 stage subtypes vs active count · Texture stage namespaces 00-0F and FE00 texture count | 16 namespaces; up to 8 simultaneously configured | Same | Same | SHARED_ENCODE_OR_SOURCE | VIA manuals: Type-02 texture/sampler fields |


##### Display/HQV — 41 audited rows

| ID | Register / operation | VX800 | VX855 | VX900 | Classification | Evidence scope |
|---|---|---|---|---|---|---|
| C9-132 | Display 0x200 · Interrupt/status and masks | Interrupt Flags & Masks Control | Interrupt Flags & Masks Control | Interrupt Flags & Masks Control | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-133 | Display 0x204 · Display flip status | Address Flip Status | Address Flip Status | Address Flip Status | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-134 | Display 0x210 · Alpha window control | Alpha Window Control | Alpha Window Control | Alpha Window Control | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-135 | Display 0x220 · Primary color key | Primary Display Color Key | Primary Display Color Key | Primary Display Color Key | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-136 | Display 0x228 · Chroma key lower bound | Chroma Key Lower Bound | Chroma Key Lower Bound | Chroma Key Lower Bound | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-137 | Display 0x22C · Chroma key upper bound | Chroma Key Upper Bound | Chroma Key Upper Bound | Chroma Key Upper Bound | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-138 | Display 0x230 · V1 stream control | Video Stream 1 Control. Bit 1 reserved. | Video Stream 1 Control. Bit 1 reserved. | Video Stream 1 Control. Bit 1 enables V1 scaling on IGA1. | SHARED_ENCODE_OR_SOURCE | Class covers the shared offset and name. Bit 1 differs (Part II VX800 and VX855 PDF p. 18, VX900 p. 21). |
| C9-139 | Display 0x238 · V1 second buffer starting address | Second V1 buffer Y start. Bit 31 reserved. | Second V1 buffer Y start. Bit 31 reserved. | Second V1 buffer Y start. Bit 31 selects the V1 length guard. | SHARED_ENCODE_OR_SOURCE | Class covers the shared offset and name. Bit 31 differs (Part II VX800 and VX855 PDF p. 19, VX900 p. 22). See §341 for location bits. |
| C9-140 | Display 0x24C · V1 zoom control | Video Window 1 Display Zoom Control | Video Window 1 Display Zoom Control | Video Window 1 Display Zoom Control | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-141 | Display 0x250 · V1 minification/interpolation | Video Window 1 Minify and Interpolation Control | Video Window 1 Minify and Interpolation Control | Video Window 1 Minify and Interpolation Control | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-142 | Display 0x2A0 · V3 control | V3 Control | V3 Control | V3 Control | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-143 | Display 0x2C0 · V3 minification/interpolation | V3 Minify & Interpolation Control | V3 Minify & Interpolation Control | V3 Minify & Interpolation Control | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-144 | Display 0x28C · P signature adder | P Signature Adder Result 1 | Reserved | P Signature Adder Result 1 | UNRESOLVED_RUNTIME_OR_FULL_ENUM | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-145 | Display 0x2CC · T signature adder | T Signature Adder Result 1 | Reserved | T Signature Adder Result 1 | UNRESOLVED_RUNTIME_OR_FULL_ENUM | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-146 | Display 0x2E4 · T signature data | T Signature Data Result 1 | Reserved | T Signature Data Result 1 | UNRESOLVED_RUNTIME_OR_FULL_ENUM | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-147 | HQV 0x360 · Early color adjustment block | NOT LISTED | HQV Color Adjustment Control 1 | NOT LISTED | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-148 | HQV 0x370 · Source video crop start | NOT LISTED | HQV Video Start Point Offset Control in Source | HQV Video Start Point Offset Control in Source [EARLY CHAPTER: NOT LISTED] | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-149 | HQV 0x374 · Source subpicture crop start | NOT LISTED | HQV Sub-picture Start Point Offset Control in Source | HQV Sub-picture Start Point Offset Control in Source [EARLY CHAPTER: NOT LISTED] | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-150 | HQV 0x378 · Source video crop end | NOT LISTED | HQV Video End Point Offset Control in Source | HQV Video End Point Offset Control in Source [EARLY CHAPTER: NOT LISTED] | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-151 | HQV 0x37C · Source subpicture crop end | NOT LISTED | HQV Sub-picture End Point Offset Control in Source | HQV Sub-picture End Point Offset Control in Source [EARLY CHAPTER: NOT LISTED] | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-152 | HQV 0x380 · Destination crop start versus source offset naming | HQV Source Data Offset Control 1 | HQV Video Start point Offset Control in Destination | HQV Video Start Point Offset Control in Destination [EARLY CHAPTER: HQV Source Data Offset Control 1] | DOCUMENTATION_CONFLICT | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-153 | HQV 0x384 · Destination crop/subpicture start | HQV Source Data Offset Control 2 | HQV Sub-picture Start Point Offset Control in Destination | HQV Sub-picture Start Point Offset Control in Destination [EARLY CHAPTER: HQV Source Data Offset Control 2] | DOCUMENTATION_CONFLICT | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-154 | HQV 0x390 · HQV tuning | HQV Parameters of Hardware Tuning Performance/Quality | HQV Parameters of Hardware Tuning Performance/Quality | HQV Parameters of Hardware Tuning Performance/Quality | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-155 | HQV 0x394 · Extended control | HQV Extended Control | HQV Extended Control | HQV Extended Control | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-156 | HQV 0x3A0 · Color adjustment | HQV Color Adjustment Control 1 | HQV Color Adjustment Control 5 | HQV Color Adjustment Control [EARLY CHAPTER: HQV Color Adjustment Control 1] | DOCUMENTATION_CONFLICT | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-157 | HQV 0x3A4 · Sharpness/decoder versus color adjustment | HQV Color Adjustment Control 2 | HQV Color Adjustment Control 6 | HQV Sharpness Control and Decoder Handshake Control [EARLY CHAPTER: HQV Color Adjustment Control 2] | DOCUMENTATION_CONFLICT | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-158 | HQV 0x3A8 · SW macro valid control versus color adjustment | HQV Color Adjustment Control 3 | HQV Color Adjustment Control 7 | HQV SW Macro Set Valid Table 0 [EARLY CHAPTER: HQV Color Adjustment Control 3] | DOCUMENTATION_CONFLICT | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-159 | HQV 0x3AC · SW macro valid control versus color adjustment | HQV Color Adjustment Control 4 | HQV Color Adjustment Control 8 | HQV SW Macro Set Valid Table 1 [EARLY CHAPTER: HQV Color Adjustment Control 4] | DOCUMENTATION_CONFLICT | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-160 | HQV 0x3B0 · Horizontal video scaling | HQV Horizontal Scale Control | HQV Video Horizontal Scale Control | HQV Video Horizontal Scale Control [EARLY CHAPTER: HQV Horizontal Scale Control] | DOCUMENTATION_CONFLICT | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-161 | HQV 0x3B4 · Vertical video scaling | HQV Vertical Scale Control | HQV Video Vertical Scale Control | HQV Video Vertical Scale Control [EARLY CHAPTER: HQV Vertical Scale Control] | DOCUMENTATION_CONFLICT | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-162 | HQV 0x3B8 · Background/default video color | HQV Default Video Color | HQV Background Color | HQV Background Color [EARLY CHAPTER: HQV Default Video Color] | DOCUMENTATION_CONFLICT | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-163 | HQV 0x3BC · Deblocking factor versus residue buffer | HQV De-blocking Factor | HQV Segment Residue Pixel Frame Buffer Starting Address | HQV Segment Residue Pixel Frame Buffer Starting Address [EARLY CHAPTER: HQV De-blocking Factor] | DOCUMENTATION_CONFLICT | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-164 | HQV 0x3C0 · Subpicture stride/control | HQV Sub-picture Frame Buffer Stride and Control | HQV Sub-picture Frame Buffer Stride and Control | HQV Sub-picture Frame Buffer Stride and Control | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-165 | HQV 0x3CC · Background offset | NOT LISTED | HQV Background Offset | HQV Background Offset [EARLY CHAPTER: NOT LISTED] | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-166 | HQV 0x3D0 · Stream control/status | HQV Stream Control and Status | HQV Stream Control and Status | HQV Stream Control and Status | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-167 | HQV 0x3DC · Tile/CSC/gamma/deblocking | HQV Linear/Tile Address Mode, Color Space Conversion, Gamma | HQV Linear/Tile Address Mode, Color Space Conversion, Gamma | HQV Linear/Tile Address Mode, Color Space Conversion, Gamma | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-168 | HQV 0x3E0 · Source line/fetch versus subpicture horizontal scale | HQV Source Data Line Count and Fetch Count Per Line | HQV Sub-picture Horizontal Scale Control | HQV Sub-picture Horizontal Scale Control [EARLY CHAPTER: HQV Source Data Line Count and Fetch Count Per Line] | DOCUMENTATION_CONFLICT | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-169 | HQV 0x3E4 · Motion-adaptive deinterlace | HQV Motion Adaptive De-interlace Control & Threshold | HQV Motion Adaptive De-interlace Control & Threshold | HQV Motion Adaptive De-interlace Control & Threshold | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-170 | HQV 0x3E8 · Subpicture vertical scale | NOT LISTED | HQV Sub-picture Vertical Scale Control | HQV Sub-picture Vertical Scale Control [EARLY CHAPTER: NOT LISTED] | GENERATION_SPECIFIC_DOCUMENTED | VIA manuals: cross-chapter contrast; use later HQV where noted |
| C9-171 | HQV 0x3EC · Destination start address 0 | HQV Destination Frame Buffer Starting Address 0 | HQV Destination Frame Buffer Starting Address 0 | HQV Destination Frame Buffer Starting Address 0 | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |
| C9-172 | HQV 0x3F4 · Destination stride | HQV Destination Frame Buffer Stride | HQV Destination Frame Buffer Stride | HQV Destination Frame Buffer Stride | SHARED_ENCODE_OR_SOURCE | VIA manuals: same register offset and summary name ONLY |


##### Supplemental display field from the independent verification

| Register field | VX800 | VX855 | VX900 | Evidence scope |
|---|---|---|---|---|
| `0x2A8[31]` · V3 second-buffer address | Reserved | Reserved | V3 length internal guard. `0` selects the old design and `1` the new design. | Part II VX800 and VX855 PDF p. 28, VX900 p. 31. This field supplements the original 172-row classification. |

<a id="c9-section-364"></a>

#### 364. Reproducibility, traceability and remaining limitations

The source-anchored CSV `Chrome9_VX800_VX855_VX900_Final_Compatibility_Matrix_351_364.csv` preserves **full source notes, class, porting rule, risk, source section and exact source-row index** for all 172 baseline records. The embedded Markdown preserves those IDs and classes and incorporates the later verification corrections. The supplemental V3 field sits outside the original class totals. `Chrome9_Final_Compatibility_Matrix_Metadata_351_364.json` enumerates the eight pinned input CSV hashes, per-subsystem counts and all classification counts. The read-only Python builder verifies every input digest, asserts row counts and refuses unknown display classifications. **Nothing here was measured on a GPU during this update.**

To regenerate the result from the exact archived audited CSV files, run `python Chrome9_Cross_Generation_Compatibility_Builder_351_364.py`. It does **not** update the master on its own; that final append is separately protected against duplication. Its categories are manually curated evidence classifications rather than inferred hardware truth, and therefore are reproducible but still subject to correction if future silicon data disagrees. Apparent documentation absence does not prove unsupported hardware. This **finishes consolidation of the currently researched subset**, not the full hardware-verification research objective.

The independent static check follows in §§365–370. The P0 runtime and ISA checks remain in §360.

### Open questions and source verification

<a id="c9-section-360"></a>

#### 360. Evidence ledger: next independent silicon checks in priority order

| Priority | Hardware/software cross-check | Result that would resolve uncertainty | Current status |
|---|---|---|---|
| P0 | **VX900 Type-10 `04h` / `65h=01b`** using authorized known-good driver captures | Resolve dispatch mode and pause-to-jump behavior on hardware | XP confirms start, stop/retrigger, and `0x43C` polling code in the VX800 and VX900 builds. The divergent jump behavior remains open without a matched runtime trace ([§373](#c9-section-373), [§380](#c9-section-380)). |
| P0 | **VX900 Type-04 `60h–62h`, Type-03 `31h`** across more real shaders | Decode IR operand mappings and opcode fields by independent program changes | Two fixed CE payloads. XP establishes four-DWORD records, distinct count and scan formulas, and part of the `61h` encoding. The instruction semantics and upper `62h` fields remain open ([§372](#c9-section-372)). |
| P0 | **VX900 pixel ALU/TAU** (manual reserved vs CE writes) | Confirm selected H6-equivalent program behavior and exact per-pixel outputs | Compiled matching words only |
| P0 | **VX900 HQV `0x3A4–0x3AC`, `0x3BC`, `0x3E0`** under defined display/decoder modes | Establish true active register layout per bank and mode | CE 6.0 and CE 7 decoder initialization corroborate the dedicated first-bank map for `0x3A4–0x3AC`. Other modes, the second bank and the remaining conflicts are open (§368). |
| P1 | **VX800/VX855 H6 vertex-buffer placement** | Verify which SL/SF/LL choices really function under same geometry | Manual wording differs; CE code shared |
| P1 | **Cross-generation 2D AlphaBLT/CFC and mono/8-bpp** | Match source/destination pixels and ROPs for same input/mode | Opcode/callback analysis only |
| P1 | **Texture depth-border/PCF and complete format enum** | Capture identical sampling/filter results and out-of-range behavior | Manuals share many fields, diverge in algorithm descriptions |
| P1 | **Command queue/VQ length interpretation** | Match register length to actual buffer boundaries with safe trace | Manual 16-byte unit vs historical implementation discrepancies |
| P2 | **Video overlay zoom, HQV scaling, color controls** | Compare output pixels, frame order, and protected bank-specific registers | Shared address topology; output equivalence not tested |
| P2 | **VX800/VX855 CE callback alias tests across chip/mode** | Verify shared compiled routines work on real selected hardware | Build-specific pointer equality in CE 6.0 and CE 7. Cross-board behavior is open (§367). |

These are **verification requirements, not directions to probe undocumented registers blindly**. Any hardware tests should be confined to authorized development boards using known-good driver recipes and protected failure recovery. The present deliverables perform no GPU access.

<a id="attached-source-verification"></a>

<a id="c9-section-365"></a>

#### 365. VIA manuals, OpenChrome and CE 7 cross-check

The pre-verification master matched the archived CSV on all **172 IDs** and all **seven displayed fields**. The original class totals recomputed as shown in §352. The archived CE 6.0 package and its exact binaries, the eight intermediate matrix inputs, the Win98 v10.479 source and the historical logs were outside the independent check’s source set. Its inputs were the six VIA PDFs, OpenChrome commit `8c35bafd6d65c6c80b9b93b50724dd8655feaaf8`, and the separate Windows Embedded CE 7.0 BSP. The BSP has **178 CAB members**, including separate retail and debug display DLLs and matching maps. Its notes identify driver **7.07.32.42** for VX800, VX855 and VX900 and exclude Direct3D Mobile support. The binary checks concern the retail display driver’s internal command emitters. They were performed offline.

All manual page numbers in §§365–370 are one-based PDF pages, including front matter.

| Area | Verified result | Attached evidence |
|---|---|---|
| PCI and display | Graphics IDs are `1106:1122`, `1106:5122`, `1106:7122`. OpenChrome maps graphics MMIO through BAR1 for all three and framebuffer through BAR0 on VX800/VX855 or BAR2 on VX900. The primary restore path writes `CR0C`, `CR0D`, `CR48`, then commits with `CR34`. It clears `CR47[0]` on VX800 and `CR47[4]` on VX855/VX900. IGA1 FIFO tuning groups VX855 with VX900, while the IGA2 depth recipe differs on all three. | OpenChrome `via_regs.h:79–81`, `via_ums.c:95–117,229–233`, `via_display.c:605–645,1799–1820,2126–2154,2829–2879`. |
| 2D GE | All three document BitBLT `0001b`, mono `0010b`, line `0101b` and rotate-after-blit/text `1001b/1010b`. VX900 documents AlphaBLT `0011b` with ROP `CC`. Its mode table gives alpha selection at bit 20 and constant alpha at `[19:12]`, overlapping a broad reserved range. The `GE+0x08` location selector `11b` is reserved on VX800 and defined as L.L. on VX855/VX900. OpenChrome selects the same H6 fill/copy callbacks and rejects mixed pixel depths in its copy path. | Part I VX800 pp. 83–87, VX855 pp. 80–85, VX900 pp. 103–108. OpenChrome `via_exa.c:903–914`, `via_exa_h6.c:140–265`. Matrix C9-034–047. |
| Command regulator | Type-10 `04h` changes from the interrupt-state-buffer address/location to VX900 command dispatch and HQV routing. Type-10 `65h[9:8]=01b` changes from immediate wrap to a jump point that waits for a subsequent pause-address command with ID `00b` or `10b`. VX900 adds the `68h[5]` Header1 width selection. Type-11 `00h[16]` is shared, VX900 adds `[17]`, and VX900 reserves `[15:0]` where the older manuals specify FIFO depth and critical-command controls. VX900 also documents a fine-grained fence wait mask at Type-11 `03h`. | Part II VX800 pp. 148–152, VX855 pp. 68–75, VX900 pp. 80–88. Matrix C9-078–100. |
| VQ length | All three manuals give 16-byte units and minimum count `0x800` for Type-10 `73h`. OpenChrome requests 256 KiB and, when `VQStart != 0`, writes `0x73008000`. The manual formula would describe 512 KiB, while the programmed start/end come from the allocation. The effective silicon length and boundary behavior remain unresolved. VX900 explicitly prohibits FE8x branching with VQ1 enabled. | Part II VX800 p. 150, VX855 p. 72, VX900 pp. 85–86. OpenChrome `via_driver.h:101`, `via_ums.c:351–370,449–464,545–551`. Matrix C9-094–095. |
| 3D vertex input | The older Type-04 declarations specify scalar FVF slots. The VX900 declarations specify input descriptors, formats and stream IDs. The VX900 Type-03 summary names `31h` as vertex shader instructions, while the detailed chapter reserves `31h–33h`. The VX900 detail also reserves Type-04 `53h–62h`, including the three words used by CE. OpenChrome’s conditional shared EXA compositor covers its limited direct-vertex draw path. | Part II VX800 pp. 135–136, VX855 pp. 154–155, VX900 pp. 78, 159–162, 179. OpenChrome `via_exa.c:943–952`, `via_3d.c:280–548`. Matrix C9-006–014, C9-026–033. |
| Texture and pixel programs | Common texture base-address and sampler structures support the limited shared encoding claim. VX800 alone documents the `texldp` divisor/mantissa check at Type-02 subtype `FEh`, subaddress `00h[8]`. VX900’s detailed Type-01 `01h[10]`, Type-01 `90h–9Ah` and Type-03 `20h–22h` reserve labels coexist with actual CE 7 compiled usage. VX900’s own command summary also names the latter subtypes. | Part II VX800 pp. 105, 115, VX855 pp. 122–123, 134, VX900 pp. 78, 97, 126–139, 159. Matrix C9-107–131. |
| Video overlay | Re-extraction yields the same **64 offsets** and **60 normalized labels** across all three summaries. The four label differences are `0x28C`, `0x290`, `0x2CC` and `0x2E4`. VX900 defines `0x230[1]` for V1 scaling on IGA1 and `0x238[31]` as the V1 length guard. It also defines the **V3 length guard at `0x2A8[31]`**, reserved on both older chips. | Part II VX800 pp. 7–8, 18–19, 28, VX855 pp. 11–12, 18–19, 28, VX900 pp. 9–10, 21–22, 31. Matrix C9-132–146 and §341. |
| HQV map | VX900’s early video chapter repeats all **30** VX800 first-bank labels. Its dedicated HQV chapter assigns separate source and destination crop groups and conflicts with the old labels at several addresses. OpenChrome uses `0x380–0x38C` for the VX800 source-offset recipe, and on VX855/VX900 it uses `0x370–0x37C` for source crop plus `0x380–0x38C` for destination crop. It also uses `0x3E0/0x3E8` for subpicture scaling setup and writes zero to `0x3A4` only on VX900 in that overlay path. The manuals distinguish the `+0x1000` second-engine bank from the `+0x2000` command-routed write aperture. | Part II VX800 p. 11, VX855 p. 48, VX900 pp. 13, 60–67. OpenChrome `via_eng_regs.h:615–633`, `via_xv_overlay.c:68–82,320–340,2118–2147`. Matrix C9-147–172. |

<a id="c9-section-370"></a>

#### 370. Verification source identities

| Attachment or extracted member | Identity |
|---|---|
| Pre-verification master snapshot | SHA-256 `930bec942a522ddec4a36e7041bc569ebba49c090432c297cc3ad47c56a45dd6` |
| Archived 172-row compatibility CSV | SHA-256 `b28b31cb39f73dded00714fb924bfe04b4dab8ce9703274b6d49a96e9202b9b8` |
| VIA manuals | VX800 Part I / Part II, VX855 Part I / Part II, VX900 Part I / Part II |
| OpenChrome ZIP | SHA-256 `c1933cc0fcbec3effce035b98acbcd8ca8420bdb6ed516d88fcdfa39dd7d7c61` |
| CE 7 BSP MSI | SHA-256 `10f6c855d5302454cdc3260c0fa477ac775bab4324e0eef5b587d0e5d0e39316` |
| CE 7 retail DLL and map | CAB members `_AC0AE51EDA874E5FA231A1FC61583A82` and `_AC814DBEBCD348FC8E2DF7DF1D5FD522`. SHA-256 `81c872ba0b3e615496511e84b995e79a4f5c10706ba9e5e07812880805bca95e` and `f69c8207e8c2077adc8a43c08b5f5bade3eb3e0b977b9aea797efbc240ff5de2`. |
| CE 7 debug DLL and map | CAB members `_9E3EFB2E83354809B262EF9151595B20` and `_F675768967D4432999E72B6D7CC62B45`. SHA-256 `717f86280ff9bc76b9833c37e3be1ad7779914311caaece441626712f4edbf69` and `90d766b6989fc2e695dfc9f002467d31cf9b077bb6c70758356df8a9634d4ee9`. |
| CE 7 display-driver release notes | CAB member `_DBE8EFB1D2444747B36836DB58056E24`. |

The CAB was recovered by following the MSI compound-file stream’s sector chain and extracted without running the installer. The linker maps were paired to the DLLs by PE timestamps. The checks used `objdump -d -M intel` for branches and writes, `objdump -s` for literal payloads, and layout-preserving PDF extraction for the offset and label comparison. See the independent verification report.

<a id="c9-section-375"></a>

#### 375. VIA VX900 XP package and binary identities

The attached INF identifies a 32-bit Windows XP display package for `PCI\VEN_1106&DEV_7122`, with `DriverVer=04/09/2013, 6.14.10.0436`. The findings are from static disassembly and PE table reads. The files were not installed or executed. Addresses in the XP sections are preferred image VAs.

| Attached file | Role and SHA-256 |
|---|---|
| dc07emb.inf | XP x86 installation and hardware matching. `8b2702054ed0028b868c7564c2e07045651af9fdeb15c0575127bc2adf60a621` |
| S3gIGPA.dll | Display driver and Direct3D HAL, preferred base `0x10000`. `2e8a2c60164c171f0c34fc90743de1bf7288e403cecf3c4c8a15bf864df7bd9f` |
| s3ginvA.dll | CIL rendering and transport backend, preferred base `0x20000`. `75138cd328cb36fa14231ae115997e4a747ea0b30d29515a6a9d6190e6845776` |
| S3gIGPmA.sys | Display miniport named by the INF. `bb4892c5e4ff689d61ea5ba45a38d859fe117c2fe3b26c9d5cab73165464c1cf` |

`s3gIGPglA.dll` is the separate OpenGL ICD. The package also supplies the display utility DLL `S3DAPI32.dll`, support driver `ucb_32a.sys`, and fallback video ROM `VTROM.bin`. They are outside the Direct3D emitters traced here.

<a id="c9-section-381"></a>

#### 381. VIA VX800 XP package and binary identities

The attached package is x86 and identifies itself as `DriverVer=12/30/2009, 6.14.10.0274`. Findings are from static disassembly and PE table reads. Addresses are preferred image VAs. No binaries were installed or run.

| Attached file | Role and SHA-256 |
|---|---|
| dc05emb.inf | Installer and PCI match. `3536b630cc249e23e38d3238752b9ed2722eab326cc1a771851d9944ad44d5de` |
| S3gIGP.dll | Display DLL and Direct3D HAL, preferred base `0x10000`. `9d59ca26f7ed7bc7921fe7b1ddc886791c63b3f2a498320894ff75e7c698c860` |
| s3ginv.dll | CIL rendering and transport, preferred base `0x20000`. `055c506245c7723986e886a703ad2d93f350674c9a71cdd257ae6992b79e3fb8` |
| S3gIGPm.sys | Display miniport. `703d4f66a6b481a056a85a9ff00f0ebfd4a453d612a65afc6c50c441ef6316fc` |
| s3gIGPgl.dll | Separate OpenGL ICD. `0d8b8086bda6e362a61fd5bd08f9c569e683bdd64f2403819f27a95b75fef229` |
| ucb_32.sys | Support driver in the miniport copy group. `667f55b3c0c6a42f9ee289b0aa8cc3ca8c738a4078263a31bd33d58bdd324cc8` |

<a id="differences"></a>

## Appendix A. VIA and OpenChrome register differences

*Source section: `Chrome9_VX800_VX855_VX900_Programming_Differences.md`. See §§365–370 for the independent verification and build-specific CE 7 results.*

**Current cross-reference:** Some preserved OpenChrome-only passages below say that `0x43C` is solely a write-side selector or that no read-pointer name is known. They reflect the narrower source set available when that part was written. **The more recent, independently verified VIA read-side register names are in Part I §§205–213.** This note preserves historical text without presenting its earlier limitation as the current overall conclusion.



### Chrome9 HC3 / HCM / HD: Programming Differences
#### VX800 vs. VX855 vs. VX900 — VIA manuals and OpenChrome source audit

**Prepared:** November 5, 2026  
**Purpose:** Record *differences* between these graphics generations, especially places where copying the same register write, packet, or initialization sequence is unsafe. This is a companion to `Chrome9_OpenChrome_Shared_Command_Matrix.md`, which catalogs shared commands.  
**Scope:** VIA VX800/VX820 (Chrome9 HC3), VX855/VX875 (Chrome9 HCM), and VX900 (Chrome9 HD); the uploaded `xf86-video-openchrome-master(4).zip` snapshot. This is a source/manual comparison, **not a record of hardware tests on all three devices**.

> **Core conclusion:** The same OpenChrome *H6 2D BLT* and limited *XRender 3D compositor* emitters service VX800, VX855 and VX900. The high-risk differences are in **PCI aperture assignment, CRTC/display initialization, video/HQV register tables, command-regulator control words, and VX900 vertex/shader register interfaces**. Shared emitters do not prove all 3D commands are interchangeable.

##### Evidence convention

- **[OC] Source-confirmed:** an actual branch, register write, map selection, or emitted command in the supplied OpenChrome snapshot; source paths are relative to `xf86-video-openchrome-master/src/`.
- **[VIA] Manual-confirmed:** a stated register field/offset from the corresponding VIA programming manual. This proves a *documented interface*, not hardware behavior on an untested board.
- **[X] Conflict/ambiguity:** source and manual differ, or the documentation is incomplete. Do not use such an item as a hardware-fact shortcut.
- **[P] Porting implication:** engineering recommendation inferred from [OC] and/or [VIA], not a demonstrated silicon fault.

Manual references: **8-I/8-II** = `vx800_1.pdf`/`vx800_2.pdf`; **855-I/855-II** = `vx855_1.pdf`/`vx855_2.pdf`; **900-I/900-II** = `vx900_1.pdf`/`vx900_2.pdf`. Page references below use the **printed manual page** where specified, not necessarily the PDF viewer index. OpenChrome source line numbers refer only to the uploaded snapshot.

---

#### At-a-glance difference matrix

| Area | VX800 / HC3 | VX855 / HCM | VX900 / HD | Reuse decision |
|---|---|---|---|---|
| PCI device ID | `1106:1122` | `1106:5122` | `1106:7122` | Identify individually |
| Framebuffer PCI region in OpenChrome | Region/BAR **0** | Region/BAR **0** | Region/BAR **2** | Chip-specific mapping |
| Graphics MMIO PCI region in OpenChrome | Region/BAR **1** | Region/BAR **1** | Region/BAR **1** | Shared access API, independently map |
| Basic H6 fill/copy opcode and encoding | Common | Common | Common | Share emitter for validated formats |
| 2D color-format conversion `0x30` | Not described in 8-I engine table | Documented | Documented | Feature-gated; not the basic H6 copy path |
| 2D AlphaBLT command `0x3` | Not in GE command list | Not in GE command list | Explicitly documented | VX900-specific opcode |
| 2D/3D legacy MMIO `0x60`, `0x6C` | Documented control region | Documented control region | `0x60–0x9F` marked reserved | Do not share legacy controls blindly |
| CRTC `CR47` unlock/protect | Clear bit **0** | Clear bit **4** | Clear bit **4** | Chip-specific masked write |
| IGA1 FIFO values (OpenChrome) | `SR17/16/18/22=5F/26/26/10` | `C7/90/90/28` | `C7/90/90/28` | Separate VX800 configuration |
| IGA2 FIFO values | Older | Different from VX800 | Different from both | All three need individual values |
| Wider horizontal display fields | Legacy | Legacy | Additional `CR45`, `CR55`, `CR6B` fields | VX900 display backend |
| Panel power sequencing in OpenChrome | Software workaround | Hardware sequencing | Hardware sequencing | Chip-specific panel route |
| HQV source-data-offset map | Older CME offsets | `_409` CME offsets | `_409` CME offsets | Chip-specific address map |
| HQV sharpness/decoder handshake init | Not in this branch | Not in this branch | Extra write | VX900-only |
| CMD regulator Type `10h:04h` | See 8-II | Interrupt-state buffer control | Dispatch-priority control | Do not copy 855 value to 900 |
| CMD regulator Type `11h:00h` | Legacy control style | Lock, FIFO depth, critical commands | Different hardware 2D/3D lock format | Different encoder |
| CMD regulator Type `11h:03h` | Not established here | Not documented as VX900 fence selector | Engine-specific fence wait bits | VX900-only fence configuration |
| CMD regulator Type `10h:78h–7Ch` | Separate 2D/3D CMDQ documented | Separate 2D/3D CMDQ documented | Omitted from Type-10 chapter | Use the older-family descriptor only in a verified queue path (§321) |
| Type `04h:00h–10h` vertex input | Older FVF family | Legacy FVF attribute list | Multiple-stream/input-register mapping | **Separate VX900 vertex encoder** |
| Type `04h:21h–22h` vertex base/pitch | See 8-II | Documented vertex buffer base and pitch | Marked reserved | Do not reuse |
| Type `03h`, subtype `30h` streams | Not demonstrated | Not documented as VX900 16-stream table | 16 stream base/pitch entries | VX900-specific |
| Type `01h:90h–9Ah` pixel shader | See 8-II | Detailed pixel shader controls | Marked reserved in 900-II | Documentation gap; don't transpose |
| Type `03h`, subtypes `20h–22h` | Instruction-related definitions | Pixel shader instructions/constants | Marked reserved in 900-II | Documentation gap; don't transpose |
| Type `08h:00h–03h` | Not documented in 8-II | Not documented in 855-II | CDC + PPC hardware counters | VX900-only documented tool |
| Basic Type `01h/02h` compositor packets | Same OpenChrome emitter | Same | Same | Share only tested compositor subset |

---

#### A. PCI, mapping, and chip identification

##### D01 — Different PCI device IDs and framebuffer BAR assignments **[OC][VIA]**

- **VX800:** PCI `1106:1122`; graphics framebuffer in PCI region 0. Its Part I PCI table describes a 64-bit framebuffer aperture and a separate 64-bit MMIO region.
- **VX855:** PCI `1106:5122`; framebuffer in region 0, 2D/3D MMIO in region 1. Part I documents the graphics MMIO BAR at PCI config `0x14`.
- **VX900:** PCI `1106:7122`; framebuffer in **region 2**, graphics-control MMIO in region 1. Its PCI table identifies a separate first BAR for AES-related MMIO.
- **OpenChrome:** `via_ums.c:105` sets `MmioBase = regions[1].base_addr`; `via_ums.c:229–233` selects `regions[2]` only for VX900, otherwise region 0. `via_ums.c:678–682` repeats the framebuffer physical-base distinction.
- **[P]** A Windows 98 miniport/VxD must discover/validate BAR **type, width, size and ownership** per detected chipset before touching registers or the framebuffer. Do not assume the VX855 framebuffer BAR offset on VX900.

##### D02 — Different documented system-local aperture layout **[VIA]**

- **VX800-I:** describes a 256 MiB graphics/video system-local BAR0 window plus a separate MMIO BAR1, and another mapped memory region.
- **VX855-I:** describes a 512 MiB system-local graphics/video window via BAR0 and a 16 MiB MMIO window via BAR1.
- **VX900-I:** describes BAR2 as a 512 MiB system-local graphics/video aperture; BAR0 has a different declared purpose.
- **[P]** Do not equate a *PCI aperture size* with the physically usable frame-buffer allocation, or assume reserved/tiled allocations map identically. Query the BIOS/PCI mapping and perform bounded VRAM tests.

---

#### B. 2D graphics commands and controls

##### D03 — VX900 documents a new **2D AlphaBLT GE opcode** **[VIA]**

- **VX800/VX855:** their `GE Command` low four bits list BLT `0001`, mono text `0010`, line `0101`, and rotated operations, but do **not** list `0011` as AlphaBLT.
- **VX900:** `GE Command` at MMIO `0x00`, bits `[3:0] = 0011`, explicitly selects **AlphaBLT** (with ROP `CC`). `GE Mode` at `0x04` additionally describes a **constant-alpha versus per-pixel-alpha selector** (`bit 20`) and constant alpha (`bits 19:12`).
- **[P]** The shared H6 BLT writer is still valid for normal copying. Implement VX900 AlphaBLT, if wanted, as a separate capability and command path; a common BLT opcode does not imply this extra opcode exists on VX855.
- **Evidence:** 855-I, 2D Engine, printed pp. 75–76; 900-I, pp. 98–99.

##### D04. VX855 and VX900 document 2D color-format conversion **[VIA][OC]**

- **VX800:** the Part I GE register listing does not document `0x30` as the later CFC interface.
- **VX855:** Part I PDF p. 84 documents CFC at `0x30` for 16-/32-bpp formats, specifies ROP `CC`, and explicitly excludes 8-bpp conversion.
- **VX900:** Part I PDF p. 107 documents CFC at `0x30` with 16-/32-bpp format fields and an 8-bpp source-depth code. The VX855 restrictions are absent from this table. Actual 8-bpp conversion and the ROP limits remain unresolved.
- **OpenChrome:** `via_exa_h6.c:200–202` **rejects** H6 screen copies with differing source/destination pixel depths. Its normal copy path does not invoke the CFC control.
- **[P]** The shared copy emitter must not be promoted to “8→16 accelerated conversion”. Feature-gate CFC separately; the existing Win98 8-bit-DIB-to-16-bit-screen problem remains unsolved by this evidence.

##### D05 — Legacy 2D/3D control MMIO offsets conflict with VX900 documentation **[VIA][OC][X]**

- **VX800/VX855:** 2D/control register descriptions include `0x60` (3D/2D ID) and `0x6C` (wait control).
- **VX900:** Part I's 2D register summary marks `0x60–0x9F` **reserved**.
- **OpenChrome:** `via_ums.c:430–434` nevertheless writes **zero** to `0x60` *only* on VX900. The comment is terse and does not establish a public meaning for that write.
- **[P]** A zero-write observed in OpenChrome is **not authorization to use the VX855 `0x60` bitfield** on VX900. Keep synchronization initialization chip-specific until resolved by other documentation or a successful device test.

##### D06 — Shared H6 fill/copy sequences are **not** shared format-conversion guarantees **[OC]**

- **Identical:** OpenChrome selects `viaExaPrepareSolid_H6`, `viaExaSolid_H6`, `viaExaPrepareCopy_H6`, `viaExaCopy_H6` for all three (`via_exa.c:903–914`); the H1 fill packets use `0x04,0x14,0x08,0x10,0x0C,0x58,0x00` and copies use `0x04,0x1C,0x14,0x08,0x18,0x10,0x0C,0x00` (`via_exa_h6.c:176–183,254–263`).
- **Limitation:** the common implementation has format, pitch, and alignment eligibility checks. Shared code only establishes **the emitted command syntax in its supported cases**, not equivalence for all hardware modes.
- **[P]** Use common H6 encoders with per-chip capability/eligibility tests rather than copying an entire acceleration-capability mask from one generation to another.

---

#### C. Display, CRTC, cursor and flat-panel control

##### D07 — `CR47` unlock/protection bit changes **[OC]**

- **VX800:** OpenChrome clears **CR47 bit 0** (`ViaCrtcMask(hwp, 0x47, 0, 0x01)`).
- **VX855/VX900:** it clears **CR47 bit 4** (`ViaCrtcMask(hwp, 0x47, 0, 0x10)`).
- **Source:** `via_display.c:605–645`.
- **[P]** A common “unlock extended CRTC” routine must dispatch on the chipset. Preserving all other `CR47` bits is important.

##### D08 — VX900 extends IGA1 horizontal display timing **[OC]**

- **VX800/VX855:** the normal IGA1 horizontal display-end and blank-start encodings do not use the VX900-only extension writes in OpenChrome.
- **VX900:** horizontal display end uses **`CR45[1]`** and horizontal blank-start uses **`CR45[2]`** as additional high bits (`via_display.c:1338–1372`).
- **Source validation:** mode-size checking at `via_display.c:1574–1584` allows larger horizontal settings for VX900 than for the earlier devices.
- **[P]** At conventional narrow modes the difference may not matter; a shared encoder silently truncates some VX900-wide timings if it omits these bits.

##### D09 — VX900 extends IGA2 horizontal timing **[OC]**

- **VX900 only:** additional **`CR55[7]`** and **`CR6B[0]`** high-bit handling for IGA2 active width and blank-start (`via_display.c:2400–2436`); its IGA2 mode validation also differs (`:2607–2617`).
- **[P]** Primary and secondary timing encoders must be treated independently; “VX855/VX900 display is the same” is overly broad.

##### D10 — IGA1 FIFO numerical settings differ **[OC]**

OpenChrome programs the *same sequencer register addresses*, but different performance-oriented values:

| IGA1 register | VX800 | VX855 | VX900 |
|---|---:|---:|---:|
| `SR17` | `0x5F` | `0xC7` | `0xC7` |
| `SR16` | `0x26` | `0x90` | `0x90` |
| `SR18` | `0x26` | `0x90` | `0x90` |
| `SR22` | `0x10` | `0x28` | `0x28` |

**Source:** `via_display.c:1799–1820`. These are OpenChrome's chosen values, not an exhaustive search for each chip's optimum FIFO thresholds. **[P]** Retain per-chip FIFO tables to avoid flicker, underflow or display corruption during 3D load.

##### D11 — IGA2 FIFO values differ across **all three** **[OC]**

The OpenChrome secondary-display FIFO recipe writes `CR68`, `CR92`, `CR94`, and `CR95` with chip-specific masks:

| Representative IGA2 field | VX800 | VX855 | VX900 |
|---|---:|---:|---:|
| `CR68[7:4]` data/mask | `0xB0 / 0xF0` | `0x80 / 0xF0` | `0x70 / 0xF0` |
| `CR68[3:0]` data/mask | `0x04 / 0x0F` | `0x08 / 0x0F` | `0x08 / 0x0F` |
| `CR95[6:4]` data/mask | `0x10 / 0x70` | `0x20 / 0x70` | `0x20 / 0x70` |
| `CR94[6:0]` data/mask | `0x10` or `0x20` / `0x7F` by mode | `0x08 / 0x7F` | `0x08 / 0x7F` |

**Source:** `via_display.c:2829–2879`. Other `CR92/CR94/CR95` writes belong to the same sequence; do not reproduce only the abbreviated table as the full initialization routine. **[P]** Maintaining common IGA1 FIFO values for VX855 and VX900 does *not* justify sharing the whole IGA2 FIFO program.

##### D12 — CRTC register save/restore ranges differ **[OC]**

- **VX855/VX900:** additional secondary CRTC block `CRF6–CRFC` is restored; **VX900** further includes `CRFD`.
- **VX800:** takes an older secondary CRTC restore path instead.
- **Source:** `via_display.c:2960–3041`.
- **[P]** A suspend/resume or mode-switch routine that saves the VX855 set verbatim may miss VX900 state; do not write unimplemented ranges on VX800.

##### D13 — LCD panel power sequencing changes **[OC]**

- **VX800:** OpenChrome prefers **software LVDS panel power sequencing**, with a source comment identifying an issue in the hardware path.
- **VX855/VX900:** OpenChrome selects **hardware primary sequencing** and the LVDS1 power control.
- **Source:** `via_fp.c:331–362`.
- **[P]** Avoid a single, unconditional panel-power sequence for all three; do not confuse panel blanking with a 3D engine problem.

##### D14 — VX900-specific panel downscale control **[OC]**

- **VX900:** OpenChrome additionally clears **`CR89[0]`** while disabling downscaling. The earlier families do not execute that specific branch.
- **Source:** `via_fp.c:718–729`.
- **[P]** Treat this as a VX900 mode-setting detail, not a shared VGA setting.

##### D15 — VX900 has additional documented display-output programming **[VIA]**

- **VX900-I:** includes an explicit integrated DisplayPort register section, alongside LVDS/DVI and the core display controls.
- **VX855-I/VX800-I:** do not document the same integrated DisplayPort section.
- **[P]** Detect and initialize only physically present output circuitry. Do not extrapolate these extra output registers to older devices.

---

#### D. Overlay, video, HQV and playback

##### D16 — HQV/CME **same function, different MMIO register map** **[OC]**

- **VX800:** OpenChrome selects `hqv_cme_regs[]` (legacy `HQV_SRC_DATA_OFFSET_CONTROL1–4`).
- **VX855/VX900:** OpenChrome selects `hqv_cme_regs_409[]` (the later `*_409` offsets).
- **Source:** `via_xv_overlay.c:61–82,320–340`.
- **[P]** Abstract HQV *semantic* operations through a family-specific address table; sharing source data offsets without changing their physical MMIO registers can program the wrong subsystem.

##### D17 — HQV extended initialization is not identical **[OC]**

- **All three:** share selected source-data offset initialization, when using the CME video engine.
- **VX855/VX900:** OpenChrome adds destination data-offset, background, extended-control, and subpicture scaling writes.
- **VX900:** additionally writes `HQV_SHARPNESS_DECODER_HANDSHAKE_CTRL_410 = 0`.
- **Source:** `via_xv_overlay.c:2118–2147`.
- **[P]** Implement extended HQV initialization per family. These OpenChrome paths do not establish that arbitrary alpha/auto-flip overlay features are interchangeable across all hardware.

---

#### E. Command regulator, engine locking and command queues

##### D18 — Type `10h:04h` changes **meaning** between VX855 and VX900 **[VIA]**

- **VX855-II:** `HParaType=10h`, sub-address `04h` = **Interrupt State Buffer Control 1** (base address and location).
- **VX900-II:** the *same address* = **CR Command Dispatch Setting**. Bits `[2:1]` select video-related versus ordinary graphics command priority; bit `0` selects graphics/HQV dispatch path.
- **[P]** Never send a VX855 interrupt-state-buffer value to the VX900 sub-address. This is a direct **register alias with different semantics**, not a performance-tuning difference.

##### D19 — VX855 interrupt commands vs VX900 preemption/CRMI commands **[VIA]**

- **VX855-II:** Type `10h:05h–07h` describe interrupt-state buffer and interrupt command settings; `10h:10h` documents VMR control.
- **VX900-II:** `10h:05h–07h` are marked **reserved**; `10h:08h–0Bh` describe **preemption command settings**; `10h:0Ch–0Eh` describe **CRMI settings**. Type `10h:04h` also notes limitations on legacy wait/fence/branch in dispatch-switch modes.
- **[P]** Partition queue recovery, interrupt, dispatch and preemption into chipset-specific routines. Shared fill/copy opcodes say nothing about these command scheduling rules.

##### D20 — Type `11h:00h` changes locking and FIFO bitfields **[VIA]**

- **VX855-II:** Type `11h:00h` includes request-lock control, three selectable FIFO depths, and a critical-command control bit.
- **VX900-II:** Type `11h:00h` provides an explicit hardware 2D/3D lock enable at **bit 17** (documented default enabled), plus request-lock behavior at bit 16; the legacy FIFO-depth/critical-command interpretation should **not** be reused.
- **[P]** Do not copy a single literal `11h:00h` configuration word between VX855 and VX900.

##### D21 — VX900 adds engine-specific fence selectors **[VIA]**

- **VX900-II:** Type `11h:03h` selects waiting on 3D/2D/video/HQV/DMA engine idle conditions and a global wait mode.
- **VX855-II:** its Type `11h` fence descriptions are organized differently; `11h:03h` is not documented as the VX900 engine-idle selector.
- **[P]** Treat the fence encoding as VX900-specific. This is potentially valuable for synchronization design, but does not prove that VX900 fence semantics solve the VX855 command hang.

##### D22. VX800 and VX855 document a second CMDQ range **[VIA]**

- **VX800-II and VX855-II:** Type `10h:70h–74h` supplies one CMDQ descriptor set, and **`10h:78h–7Ch`** describes the separate 2D/3D command queue. See §321.
- **VX900-II:** documents `10h:70h–74h` but not that same additional `78h–7Ch` setup.
- **[P]** A shared OpenChrome VQ sequence is narrower than “the command regulator is identical”. Feature-gate additional queue descriptors.

##### D23 — Shared OpenChrome VQ setup has a **length-unit discrepancy** with the manuals **[OC][VIA][X]**

- **Source:** `via_driver.h:101` sets `VIA_VQ_SIZE = 256*1024`; `via_ums.c:351–370` writes `0x73000000 | (VIA_VQ_SIZE >> 3)` to the Type-10 queue-length field using `0x41C/0x420`. `via_ums.c:449–463` enables it **only if `VQStart != 0`**; otherwise VQ is disabled.
- **Manuals:** all three Part II CMDQ descriptions define the corresponding queue length in **128-bit (16-byte) units**, minimum `0x800`.
- **Arithmetic:** `256 KiB >> 3 = 32768` units; `32768 × 16 bytes = 512 KiB` if the manuals are interpreted literally, versus the 256 KiB OpenChrome allocation.
- **[X]** This proves an **inconsistency between the source and stated unit definition**, not a verified out-of-bounds DMA access. There may be mode-dependent semantics or a documentation mistake.
- **[P]** Keep the known-good queue sequence unchanged while collecting bounded read-only status/completion observations. A production queue change requires guard allocation, repeated workloads, timeout and recovery strategy, and a test board.

##### D24 — Common parameter transport is **not** a shared queue read pointer **[OC]**

- **OpenChrome:** `0x41C/0x420` transport the queue setup sequence; `0x43C/0x440` are the **direct parameter-selection/data transport** registers.
- **[P]** Register `0x43C` staying unchanged cannot, by itself, prove the VQ did or did not read ahead. Use output completion and documented status flags. This caveat matters to the earlier VX855 ring experiments.
- **Source:** `via_regs.h:193–208`, `via_ums.c:351–370`, `via_exa.c:55–82,465–479`.

---

#### F. 3D register programming — major differences beyond OpenChrome's compositor

##### D25 — VX900 redefines Type `04h:00h–10h` flexible-vertex words **[VIA]**

- **VX855-II:** addresses `04h:00h–10h` encode the older **FVF attribute ordering**. For instance, `04h:10h` contains late FVF attribute selectors and back-face color masks.
- **VX900-II:** addresses `04h:00h–10h` configure **multiple vertex streams**. At `04h:01h–10h`, bits `[23:20]` are a stream/vertex-buffer ID, `[19:12]` are an offset, `[11:8]` identify a vertex shader input register, and `[7:0]` encode element format. `04h:00h` includes the number of vertex-buffer data entries.
- **[P]** The same physical Type-04 sub-address can mean an entirely different vertex layout. The VX855 FVF emitter must **not** be treated as binary-compatible with VX900. Design two vertex input backends.
- **Evidence:** 855-II, 3D Vertex and Primitive Setting, printed pp. 148–154; 900-II, printed pp. 154–160.

##### D26 — VX855 Type `04h:21h–22h` vertex base/pitch vs VX900 reserved **[VIA]**

- **VX855-II:** `04h:21h` = low vertex-buffer base address; `04h:22h` = high address and stride/pitch.
- **VX900-II:** `04h:21h–22h` is marked **reserved**.
- **[P]** Using VX855's vertex-buffer setup on VX900 can break geometry even if a common Type-01 render-target and Type-02 texture state packet was accepted.

##### D27 — VX900 defines a **16-stream base/pitch table** **[VIA]**

- **VX900-II:** Type `03h`, subtype **`30h`** supplies 16 entries of stream base/pitch; base address field is in 1 KiB units, pitch in 4-byte units. Its Type-04 vertex descriptors choose streams and shader input registers.
- **VX855-II:** does not document that same Type `03h:30h` multi-stream table; instead has the older Type-04 base/pitch scheme.
- **[P]** A VX900 port needs a distinct stream address/format loader. Do **not** infer that the VX900 automatically provides the complete transform/shader program required by a D3D7 fixed-function pipeline.

##### D28 — VX900 vertex-to-screen transform mentions **dual register/VS constant programming** **[VIA]**

- **VX900-II:** the Type `04h:40h–52h` clipping-to-screen section says the driver must set scales/offsets both in `HC2SxScale/HC2SxOffset` and in VS constant registers **`c256` and `c257`**.
- **[P]** This is a specific VX900 vertex-path integration dependency; copying only legacy clip/view registers may leave the VS-side transform inconsistent.
- **[X]** The same manual does not fully document all vertex shader instruction-loading interfaces. The note identifies a dependency, **not a complete implementation recipe**.

##### D29 — VX855 pixel shader register definitions are marked reserved in VX900 **[VIA][X]**

- **VX855-II:** Type `01h:90h–9Ah` gives detailed pixel shader settings.
- **VX900-II:** labels Type `01h:90h–9Ah` **reserved**.
- **VX855-II:** Type `03h` subtypes `20h–22h` define pixel shader ALU instructions, TAU instructions and constants.
- **VX900-II:** marks Type `03h` subtypes `20h–22h` **reserved**.
- **[X]** The VX900 manual still mentions VS/PS activities elsewhere. Reserved in the available manual is **not proof the GPU cannot shade**; it means the earlier command encodings are **not established as VX900-compatible**.
- **[P]** Do not assume older shader microcode writes are safe or meaningful on VX900. Look for a working driver trace or further register documentation before advertising a shader hardware path.

##### D30 — VX855 occlusion-counter writeback vs VX900 PPC interface **[VIA]**

- **VX855-II:** Type `01h:21h–22h` documents an occlusion-counter writeback address interface (`HZOcclusionCNT`).
- **VX900-II:** does not document the same `01h:21h–22h` interface in its Z-setting register sequence.
- **VX900-II:** adds **Type `08h:01h–03h`** performance-profile counter selection and memory writeback, including received vertices, vertex-cache hits, executed VS and PS instructions, depth-buffer traffic, and an occlusion-count selector.
- **[P]** Implement performance instrumentation through separate feature-tested adapters, not a shared “occlusion register” function.

##### D31 — Shared Type `00h/01h/02h` compositing **does not prove identical advanced 3D** **[OC][VIA]**

- **OpenChrome:** one `via_3d.c` emitter drives simple direct textured triangles and XRender-style compositing for all three: render-target and alpha/blend states (Type `01h`), texture state (Type `02h`), and direct vertices (Type `00h`). The H6 compositing callbacks share these functions (`via_exa.c:943–952`; `via_3d.c:280–549`).
- **VX900 exception:** Type `04h` multi-stream/FVF layout is different (D25–D28); the compositor does not exercise that path.
- **[P]** Share only a *bounded, known command subset* until hardware tests establish Type-01/02 compatibility for specific texture/depth/stencil subfeatures. No claim is made here about all shader instructions, fixed-function T&L, D24S8 coherence, or hang recovery.

---

#### G. What is actually safe to share in a common Win98 driver?

| Component or function | Recommendation | Reason / required guard |
|---|---|---|
| PCI ID dispatch | **Separate tables** | Three IDs, different framebuffer BAR rules |
| MMIO read/write primitive | **Shared helper** | Mapping differs; offsets need per-chip validation |
| H6 2D fills, ROPs, 16/32-bit screen copies | **Common core** | Identical OpenChrome emitter; keep mode/pitch/format restrictions |
| 8-bit-to-16-bit DIB conversion | **Not established by H6 path** | OpenChrome rejects cross-depth copy; CFC does not support 8-bit |
| VX900 AlphaBLT | **VX900-specific option** | Explicit VX900 opcode/alpha bitfields |
| Scanout start address update | **Common ordered operation, chip-specific fields** | `CR0C`, `CR0D`, `CR48`, **`CR34` last** shown in OpenChrome |
| IGA1 FIFO | **VX800 vs VX855/VX900 values** | Source table differs |
| IGA2 FIFO | **Three independent value sets** | Source table differs for all three |
| Timing / CRT register unlock | **Per-family writes** | CR47 bit, VX900 horizontal extension bits |
| LVDS power and TMDS/DisplayPort | **Output/chip-specific** | Software vs hardware sequencing, extra output circuitry |
| HQV/overlay | **Per-family MMIO map and init** | Old CME vs `_409` offsets, additional VX900 write |
| Direct H1/H2 packet encode | **Common encoder** | Same OpenChrome transport for restricted operations |
| Type `01h/02h` limited texture/blend | **Common subset, tested by mode** | Common `via_3d.c` compositing emitter |
| Type `00h` direct textured-quad emission | **Common subset** | OpenChrome uses same path; not a vertex-buffer proof |
| Type `04h` vertex input | **Separate VX900 encoder** | Different field meanings at identical sub-addresses |
| 2D/3D wait/fence and Type 10/11 control | **Per-chip command definitions** | Several aliases have changed semantics |
| VQ setup | **Common recipe with validation** | Source common, size-unit discrepancy unresolved, conditional VQ availability |
| Shader/vertex microcode | **Do not share without evidence** | VX900 marks older registers reserved, instruction interface incomplete |
| Depth/fast-Z/stencil | **Per-feature hardware tests** | Similar documented state does not establish same cache/clear behavior |
| PPC/occlusion diagnostics | **Separate adapters** | VX855 and VX900 interfaces differ |

##### Minimal compatibility dispatch (design sketch, not ready-to-run code)

```c
switch (device_id) {
case 0x1122: family = CHROME9_HC3_VX800; fb_bar = 0; break;
case 0x5122: family = CHROME9_HCM_VX855; fb_bar = 0; break;
case 0x7122: family = CHROME9_HD_VX900; fb_bar = 2; break;
default: return DEVICE_UNSUPPORTED;
}

/* Common after successful PCI-resource validation: */
engine.ops.blit      = h6_blit;
engine.ops.fill      = h6_fill;
engine.ops.direct3d2 = common_compositor_subset;

/* Explicitly different: */
engine.ops.mode_set  = mode_set_ops[family];
engine.ops.hqv       = hqv_ops[family];
engine.ops.sync      = sync_ops[family];
engine.ops.vertices  = vertex_ops[family];
```

**Notional architecture only:** this sketch deliberately omits the Windows 98 DDI/VxD mechanics, memory-mapped BAR type/length validation, initialization dependencies, volatile registers, paging, failure recovery, and safe resource teardown.

---

#### H. Prioritized validation experiments

1. **PCI + 2D smoke test.** Log PCI device ID, BAR layout and resolved MMIO; fill a small off-screen surface using the identical H6 register packet; then BLT it back. Use a conservative supported pixel depth and stride.
2. **CRTC-only cross-check.** Confirm `CR47` protection handling, `CR0C/CR0D/CR48/CR34` scanout commit order, FIFO tables, and VX900-only extra timing writes at a known-good display mode.
3. **OpenChrome compositor subset.** Compare one 0-texture/1-texture/2-texture direct quad, the exact Type-00/01/02 words, and output pixels. Avoid drawing conclusions about Type-04 indexed buffers.
4. **VQ observation, not speculative modification.** Log queue-enable condition, allocation, exact `0x73` length word, `0x400` busy flags and completion. Keep the known-good mode until the length-unit inconsistency is resolved.
5. **VX900 vertex differential.** Program a minimal *documented* vertex stream and compare with a direct vertex path. Log Type-04 words and VRAM bounds. Do not test undocumented shader opcodes as an initial smoke test.
6. **HQV differential.** Exercise the same playback scale/crop behavior while verifying the per-chip source offset registers and the VX900 extra decoder-handshake control.
7. **Relevant VX855 driver tests.** Continue separate depth-clear, tiled Z/stencil and ring-recovery probes; the shared OpenChrome compositor does not validate those features.

##### Suggested log record

```text
family=VX855 pci=1106:5122 fb_bar=0 mmio_bar=1
subsystem=CR type=10 sub=73 data=00008000 source=OpenChrome
subsystem=3D type=04 sub=10 data=XXXXXX family_encoding=FVF_855
subsystem=display io=3D5 index=47 mask=10 value=00
observation=read_only_status_400 value=XXXXXXXX
outcome=success|timeout|pixel_mismatch|not_tested
```

Record **register namespace** (MMIO/CRTC/SEQ/3D-Type+Subtype), **bitmask**, and **family**. The same hex offset in two different namespaces must not be treated as the same register.

---

#### I. Source index and confidence limitations

##### OpenChrome files reviewed

| File | Primary evidence |
|---|---|
| `via_ums.c` | BAR mapping; 2D engine reset; VX900 `0x60` write; VQ setup/disable |
| `via_exa.c` | Same H6 callbacks on VX800/VX855/VX900; sync polling; compositing hookup |
| `via_exa_h6.c` | Exact H6 2D packet words and format/pitch checks |
| `via_dmabuffer.h` | Common Header-1/Header-2 packet construction |
| `via_3d.c`, `via_3d_reg.h` | Limited shared Type-00/01/02 compositing packets |
| `via_display.c` | CR47, horizontal extensions, IGA1/IGA2 FIFO, scanout, CRTC save/restore |
| `via_fp.c` | LVDS power sequencing and VX900 CR89 downscaling write |
| `via_xv_overlay.c` | HQV source-data-offset maps and extended playback initialization |
| `via_regs.h`, `via_driver.h` | Device IDs, engine masks, queue-size constant |

##### VIA manuals

- `vx800_1.pdf` — *VX800/VX820 Chrome9 HC3*, Part I: PCI, VGA, 2D, DMA, LVDS.
- `vx800_2.pdf` — Part II: 3D, video and command regulator.
- `vx855_1.pdf` — *VX855/VX875 Chrome9 HCM*, Part I: PCI, VGA, 2D, DMA, LVDS/DVI.
- `vx855_2.pdf` — Part II: command regulator, 3D, video and HQV.
- `vx900_1.pdf` — *VX900 Chrome9 HD*, Part I: PCI, VGA, 2D, DMA, LVDS/DVI, DisplayPort.
- `vx900_2.pdf` — Part II: command regulator, vertex formats, 3D, video and PPC.

##### What this comparison **does not establish**

- It does not show that all commands succeed on all three physical chips. OpenChrome's shared path is **source evidence**, not a cross-chip logic-analyzer capture.
- It does not show that VX900 shader hardware is absent, or provide a complete public shader microcode format.
- It does not prove that the VQ sizing discrepancy causes corruption on any working setup.
- It does not prove that identical documented Z/cache registers behave identically during tiled depth clears or D24S8 operations.
- It does not show that X.Org's EXA compositor can replace a Windows 98 Direct3D/OpenGL HAL/ICD; it demonstrates a narrower command subset.
- It does not imply that OpenChrome's particular FIFO values or memory-selection values are globally optimal; they are one implementation's chosen configuration.

**Final porting boundary:** reuse the **H6 fill/copy and restricted compositor command writers**; conditionally reuse **packet formats and VQ recipe** after chip validation; split **PCI resources, advanced vertex input, command-regulator control, display/FIFO, and HQV** into chip-aware backends. Treat the unresolved source/manual disagreements as explicit test cases rather than facts.

#### CE-driver confirmation: one shared backend and one generation-specific backend (2026-11-06)

The retail Windows CE 6.0 driver from `VIA Graphics Driver Group2.msi` confirms via `C3DUtils::Init` that VX800 (`1122`) and VX855 (`5122`) both use the same `Init3DHWRegs_H6`, `Texture3D_H5`, and `fn3DBlt_H6` function addresses. VX900 (`7122`) selects `Init3DHWRegs_H5S2`, `Texture3D_H5S2`, and `fn3DBlt_H5S2` instead. Both initializers invoke common `Init3DHWRegs_INV` before chipset-specific state.

The H5S2 blit calls `VX900_WAITPATCH`, which adds eight repeated command blocks containing `0x6C/0x00800000`, followed by a packet with `0x6C/0x00FC0000`. These values correspond to 3D/2D/DMA wait-control bits in the VX800 and VX855 manuals. However, the VX900 public manual marks this MMIO range reserved, so the precise packet semantics and effectiveness on VX900 need a hardware check. **Do not equate this with general DX7/8/9 3D shader compatibility.** Full literal packet sequences and evidence are in the CE additional findings and in the main compatibility report (section 46).


#### Addendum: decoded CE command packets and corrected texture symbol

The main compatibility report section 47 now decodes CE software packet headers using the DLL's executable interpreter at `0x10063766`. FE00 performs MMIO offset/value writes, FE02 selects a register plus streamed data, and FE03 selects a register plus counted data. The `VX900_WAITPATCH` packet therefore **provably writes MMIO+0x6C = 0x00800000 in the CE software path**. The VX900 hardware interpretation of this legacy wait register remains uncertain because its manual marks the address reserved.

Correction: `0x10067255` is `Texture3D_H5` (not a symbol named `Texture3D_H6`); VX800 and VX855 select it alongside `Init3DHWRegs_H6` and `fn3DBlt_H6`. VX900 selects `Texture3D_H5S2`. The H5S2 path writes **documented-as-reserved Type-03 subtype 0x31** table data selected by operation flags, and differs in other texture-state literals. Full words and binary addresses are provided in the main report.

#### CE dynamic-field comparison: render-target and texture formats (2026-11-06)

Disassembly of both CE texture helpers confirms shared render-target and texture-base formulas: `01h:50h = base >> 8`, `01h:52h = pitch >> 5 | format`, `01h:53h = 0x010C0F`, and Type-02 texture-stage-0 base `>> 8`. VX800/VX855 use the H5 helper and request 640 bytes of command-buffer space; VX900 uses H5S2 and requests 1,200 bytes with additional state/wait packets. The complete disassembly-address evidence, format mapper, and caveats appear in the main report §48 and CE findings §48.


##### Additional CE blit result: shared state table, different command reservations

The CE retail DLL has an exact, shared 50-DWORD (`200`-byte) Type-01 initialization table at VA `0x100084F8`. Both `fn3DBlt_H6` (VX800/VX855) and `fn3DBlt_H5S2` (VX900) copy it into their Type-01 command streams. The same two blit implementations pack rectangle coordinates identically and calculate RT0 pitch similarly. However, they request different command-buffer capacities: H6 `0x500`/`0x438` versus H5S2 `0x640`/`0x4B0`, selected by an observed mode-3 branch. See the main report §49 for instruction addresses and the full table. Requested capacity is not the actual executed length.



---

#### Additional CE static comparison, November 6, 2026: texture declaration and coordinate preparation

The CE code proves that both VX800/VX855 `Texture3D_H5` and VX900 `Texture3D_H5S2` use the same 2D-texture setup for Type-02 stage-0 registers `18h`, `20h`, `21h`, `22h`, and `30h`: the width is padded to 16 texels for the `20h` declaration, the texture's original width and height are used to compute ceiling-log2 size exponents for `22h`, and pixel-format flags are constructed by the same `GetPixelFormat_INV`. The formulas and VA evidence are documented in **the main compatibility report §50**.

**New VX900-specific operation:** `Texture3D_H5S2` adds the floating constant `786432.0` to six intermediate coordinate values (`0x10067E14–0x10067E50`); the H5 function has no corresponding block. The constant produces a 1/16-pixel-scaled float32 low-bit pattern around zero, strongly suggesting a different coordinate packing convention, but its exact GPU meaning is not yet established. Keep the operation behind an H5S2-specific path; it is not a VX855 optimization recommendation.

**Transport, not rendering:** both helpers can either reserve/submit an FE command buffer via `cmSmartRequest`/`0x1004E51D` or interpret the same FE packets to MMIO through `0x10063766`. This mode split should **not** be called hardware vs. software 3D. Full evidence is in main report §51.

**Manual labeling discrepancy (not a demonstrated hardware difference):** in `02h:18h` the VX800 manual identifies bits `[17:16]`/`[15:14]` as `HTXnF5LOC`/`HTXnF4LOC` even though their prose repeats "Face0"; VX855 similarly repeats "Face0"; VX900 explicitly labels the fields "Face5." The repeated "Face0" text is likely a documentation typo. The CE path's literal `0x18000100` leaves those location bits zero, so it cannot validate which face index those fields select.


---

##### New CE-based vertex and primitive command differences (2026-11-06)

| HParaType-04 field | VX800/VX855 H5 CE path | VX900 H5S2 CE path | Driver-port consequence |
|---|---|---|---|
| `01h/02h` vertex input declarations | Not programmed in this path | `0x010000C2` Float4 IR0; `0x02000142` Float2 IR1 | Distinct VX900 input declaration setup |
| `12h` input source mask | Not programmed here | `0x12000003` (IR0 + IR1) | Additional VX900 input state |
| `20h` primitive cycle | `0x20000000` (full cycle) | `0x201C5C00` (automatic fast/reuse) | Primitive-cycle mode differs; do not copy without reset/strip tests |
| `23h` shading/facing | `0x23000F02` (solid, face bit 8 = 1) | `0x23002E02` (flat via B, face bit 8 = 0) | Shading and winding convention differ, filled triangles in both |
| `24h` primitive count | `0x24000003` | `0x24000003` | Shared three-vertex command |
| `25h` attributes | `0x25F6C000` (8 float/color/tex words) | `0x25F0C000` (6 float/tex words) | **Same mask semantics**; omit C0/C1 in VX900 CE case |
| `28h` default attr mask | `0x28FFC000` | `0x28F0C000` | Related default-mask change; bit 19 in VX900 docs reserved |
| `2Fh` defaults | Not set in this H5 setup | `0x2F582000` (W, color0 RGB/A, Q = 1) | Default white C0 when colors not sent |
| `60h–62h` | Not emitted in this H5 setup | `0x60800001, 0x61800000, 0x62104001` | **Undocumented** in VX900 public manual; do not generalize |

The H5 vertex stream is `(X,Y,0,1,C0,C1,S,T)`; H5S2 uses `(biased-X,biased-Y,0,1,S,T)`, a six-word stream; for three vertices, 96 vs 72 bytes. VX900's declaration codes `0xC2/0x42` match Float4/Float2 in **VX900 Part II PDF pages 160–161**. Bitfields for mask, cycle and shading are described at **VX855 Part II pages 159–162** and **VX900 Part II pages 166–168**. These are **CE operation-specific**, not proof of universal gaming command compatibility. Full details and binary VAs are in the master report §§52–53.


##### Important cross-operation correction: many VX900-seeming vertex commands are actually shared

VIA's VX800/VX855 CE `fn3DBlt_H6` and VX900 CE `fn3DBlt_H5S2` **both** emit `04:20=0x201C5C00`, `04:23=0x23002E02`, `04:24=3`, and `04:25/28=0xF0C000` (one S/T texture set) or `0xF0CC00` (two S/T sets). Both use exactly the same 32-DWORD fixed state table at `0x100087A0`. Differences remain in `04:00` and the VX900 input declarations/masks (Float4 IR0, Float2 IR1, optional Float2 IR2). See master compatibility report **§54** for the exact compiled addresses. **Do not generalize** earlier H5 vs H5S2 *texture-helper* differences as family-wide primitive limitations.


##### Further binary-confirmed 3D compatibility corrections: shared PS programs, VX900-only VS packets

- Both VIA CE H6 (`fn3DBlt_H6`, VX800/VX855) and H5S2 (`fn3DBlt_H5S2`, VX900) submit **identical pixel-shader data** at `03h` subtypes `20h`/`21h`/`22h`: same 12- or 4-DWORD ALU program, same 2- or 1-DWORD TAU data, same four-component constant construction. The VX900 detailed manual calls these subtypes reserved; VIA's VX900 CE path nevertheless uses them. This is compatibility evidence for **these programs**, not blanket pixel-shader ISA equivalence.
- The VX900 H5S2 path additionally uploads **8 or 12 DWORDs at Type-03 subtype `31h`**. The manual's overview calls it Vertex Shader Instruction, while detailed definitions mark `31h–33h` reserved. The 12-DWORD table is exactly the 8-DWORD table plus four words. Semantics are unverified.
- The H5S2 3D blit and texture functions **both** reference `786432.0` to bias positional coordinates. Corresponding H6/H5 functions do not. This is a genuine CE implementation difference with unverified hardware interpretation.
- `04h:60–62h` values vary systematically with single versus double texture-coordinate modes but lack published bitfield definitions. Do not copy these words into VX855 initialization.

Exact constants, source VAs, documented/missing bitfields, and controlled-test implications are in master compatibility report **§§55–57**.


---

##### 58–61. Further CE-driver comparisons: shared VS program, C-to-S bias, and per-operation pixel shaders (2026-11-06)

- `Texture3D_H5S2` emits the exact same Type-03 subtype `31h` **8-DWORD program** (`0008014C 00000000 00000000 0008021F / 00084124 00000000 00000000 00080283`) used by single-texture `fn3DBlt_H5S2`. The texture function writes it inline at `0x100681D8–0x10068206`; the blit function copies it from `0x1000870C`. The dual-texture/blend blit adds a third 4-DWORD instruction candidate; instruction semantics remain unpublished.
- Both `Texture3D_H5` and `Texture3D_H5S2` **already program the C-to-S X/Y offset as `786432.0`** (`04:41/42` and `04:44/45`). They set scales for X, Y, and Z to `1.0`. The same X/Y words appear in the 32-DWORD table shared by `fn3DBlt_H6` and `fn3DBlt_H5S2` at `0x100087A0`. This refines the prior claim of a VX900-only bias: the VX900 routines additionally apply `786432.0` in software to emitted vertex coordinates, while the hardware transform state has the constant in both generations; whether both stages execute is unknown.
- `04:47` differs between **texture helpers only**: H5 `0x47400000` (Z offset `0x00400000`, subnormal float ≈ `5.87747e-39`) versus H5S2 `0x47000000` (zero). The H6 and H5S2 **blits both use zero**, so do not treat this as a general chipset-specific 3D rule.
- Pixel shaders differ by *operation*: the CE **texture helper** H5 programs `01:92=0x92010003` (1 TAU, **3 ALU** instructions); H5S2 programs `01:92=0x92010001` (1 TAU, **1 ALU** instruction). This exactly matches their emitted Type-03 subtype-`20h` 12/4-DWORD streams. Their one-word TAU instruction `301001B0` is identical. The H5 last ALU instruction differs from H5S2's only instruction in DWORD 2 (`00F20105` vs `00F20005`). By contrast the CE **3D blit** H6 and H5S2 programs are shared as documented in §55.
- Primary manual basis: VX800/VX855/VX900 Part II Type-04 `40h–52h` clipping-to-screen transform; VX855 Part II Type-01 `92h` Pixel Shader Setting 3. VX900 published detailed shader subtype documentation is incomplete/reserved. No hardware behavior has been tested. Full binary anchors, words, and reproduction procedure: master report §§58–61 and `Chrome9_CE_Command_Verification_58_61.py`.



---

#### New cross-generation clarifications (CE binary verification, 2026-11-06; main report §§62–64)

- **Distinct decisions:** Both H6 (VX800/VX855) and H5S2 (VX900) choose 6 versus 8 vertex DWORDs when the blit input flag `0x20` **or** global `bGdiMultiABlend` is set. The **long PS program**, however, is selected by `bGdiMultiABlend` separately. VX900 also chooses 8 versus 12 Type-03 subtype-`31h` instruction DWORDs according to the vertex-input condition. Thus a second UV set can add a VX900 VS candidate instruction **without** increasing the PS instruction count.
- **Third VX900 VS candidate:** `00088124 00000000 00000000 00080293` follows the 8-word candidate and correlates with added Float2 input register 2 (`04h:03=0x03000242`, input mask `04h:12=0x12000007`). It resembles the second four-DWORD group but has two changed fields; no published VS ISA mnemonic is available.
- **Shared default state:** Both H6 and H5S2 3D blits copy a common 32-DWORD state table containing `04h:2A=0x2A3F8000`, `2Bh=0x2B3F8000`, `2Fh=0x2F582000`. The first two specify point defaults of **127.0 in signed s[8].15**, not floating 1.0. `2Fh` sets W, Color0, Alpha0, texture Q defaults to 1. H5S2's texture *helper* also writes these inline, but they are **not VX900-only commands**.
- **Interpretation boundary:** These findings support UV/input handling, not a proven shift of pixel shader arithmetic to the vertex shader. Compare matched output pixels on hardware before porting undocumented registers.

See complete report §§62–64, CE reverse-engineering report, and `Chrome9_CE_Command_Verification_62_64.py` for the byte-level evidence.


#### Additional CE driver comparison: shared alpha handling and VX900 instruction coverage (2026-11-06)

- **Shared H6/H5S2 blend calculation:** both load `255.0` from DLL VA `0x10008920`. In a normal control path both form `alpha/255.0` from the byte at input offset `+0x34`, and pass the result through the *same* fixed-point conversion function `0x100644A4` (10 fractional bits; unity `0x400`). This is cross-generation **shared code**, not a difference.
- **Shared dynamic shader constants:** in multi-blend mode both calculate `255.0/alpha` and upload `(factor, 1.0, 0.0, 0.0)` to Type-03 subtype `22h`. Pixel-shader program equality **alone** is not sufficient; matching constants and mode state are also required.
- **VX900-only VS interface in examined CE routines:** Type-03 subtype `31h` is emitted three times in this retail binary (two distinct programs: 8 and 12 DWORDs); corresponding Type-04 `60h–62h` words switch between `60800001/61800000/62104001` and `60800002/61800000/62204002`. The VX900 public manual marks `04h:53h–62h` reserved, so these fields remain **undocumented**, even though VIA's production code writes them.
- **Safety/compatibility boundary:** avoid copying the `60h–62h` and candidate VS words into VX855; keep the input and VS encoder generation-specific. The static CE package does not contain enough distinct VS programs to decode a general VX900 shader ISA.
- **Static corner-case lead:** the multi-blend branch tested here does not exclude alpha zero before computing `255/alpha`; whether any valid operation reaches that branch, or what the GPU does with the resulting value, is **untested**.

Full machine-code addresses, tables, and reproducing static script: master report §§65–68, `Chrome9_CE_Command_Verification_65_68.py`.

---

#### CE interpreter follow-up — packet format versus chipset-specific payload (2026-11-06)

New binary comparison adds **one shared software packet interpreter** but does not collapse the generation-specific GPU register differences. `IS_AGPHEADER_INV` (retail `DDI_VIA.dll` VA `0x10062ED9`) recognizes `FE00–FE07`. The direct-MMIO interpreter at `0x10063766` handles `FE00–FE06`; `FE07` has no independently decoded action in this path. `FE00/FE05` submit register/value pairs, `FE01` streams to `MMIO+0x200000` (the VX855 manual's 2D Host BitBLT aperture), `FE02/FE03` stream parameter data, `FE04` repeats writes to one offset, and `FE06` contains a conditional second register write. These are **VIA CE software-packet semantics**, not proof that all hardware generations decode each advanced 3D payload identically.

The H6 and H5S2 3D-blit paths also share an exactly equivalent 8-bit-alpha-to-Q10 factor: `floor(alpha*1024/255)` for every alpha `0..255`, as verified from the original float32 helper. Alpha=0 remains an unverified corner case for the *separate* `255/alpha` multi-blend constant upload. The complete packet and arithmetic analysis, binary addresses and limitations are in main report §§69–72 and `Chrome9_CE_Command_Verification_69_72.py`.


**Offline analysis tool:** `Chrome9_CE_Offline_Packet_Decoder.py` decodes VIA CE `FE00–FE07` packet envelopes as software-MMIO operations, with opaque FE07 handling and truncation checks. See the complete report §73 for a verified wait-patch packet example.


---

#### VIA CE static-state templates: an additional verified commonality (2026-11-06)

Both `fn3DBlt_H6` (VX800/VX855) and `fn3DBlt_H5S2` (VX900) copy **the same two Type-01 state templates** (50 and 37 DWORDs) and **the same pair of 12-DWORD pixel-shader setting tables**. There are no per-chip literal differences in these four tables. The 37-word template is entirely contained in the 50-word one; the latter also sets `01h:00h=0000000F` (RT dithering bits) and clears `01h:34h–3Fh` (12 alpha-state registers).

The two shader-control templates have only four differing registers: `01h:91h`, `92h`, `93h`, and `9Bh`. The old VX855 manuals explain the first three fields; `9Bh` is not covered by its `90h–9Ah` section. These are **operation-mode differences**, not a VX855-versus-VX900 difference. See main report §§74–77 and `Chrome9_CE_Static_State_Template_Diff.csv` for word-for-word values and source addresses.


##### CE source update — normalized H6/H5S2 blit control flow (2026-11-06)

**[CE binary-confirmed; manual-decoded; no hardware test]** The same three checks (`mode[+0x30] == 3`, `field[+0x38]`, `alpha_u8[+0x34] == 255`) select the same compact 37-DWORD versus extended 50-DWORD Type-01 state template in both VIA backends. For mode 3, compact is used when the extra field is zero **or** alpha is 255; otherwise the extended template and global blend flag are selected. The exact symbolic meaning of the extra field is not yet established.

VIA's **six-DWORD CE triangle payload** is shared, but its *vertex declarations* are not: H6 uses `04:01=01020100` (X,Y,Z) and `04:02=02090803` (W,TextureA-S/T); H5S2 uses `04:01=010000C2` (IR0 Float4) and `04:02=02000142` (IR1 Float2). VX900 also programs `04:00=00A80106` (2 input registers, 6 data DWORDs), or `00A80208` (3 inputs, 8 DWORDs) in the extended path; the latter adds `04:03=03000242` (IR2 Float2). In both backends, primitive words `201C5C00`, `23002E02`, `24000003` and the same 32-DWORD transform/state table are reused. The VX900-selected path additionally inserts `04:60–62` state and 8-/12-DWORD candidate VS uploads **before** the otherwise shared pixel-shader uploads. These undocumented commands remain unsafe to extrapolate to VX855.

See main report §§78–81 for the exact register encodings, executable addresses, stage matrix, and read-only verification script.

---

#### Family-wide CE command-selection caveat (2026-11-06)

A byte-exact audit of the retail CE DLL (details in the main report §§82–85) found **27 direct references to `bGdiMultiABlend`** (`0x100CE7EC`), including **two set-to-1 and two clear-to-0 instructions** (one pair in each H6/H5S2 3D blit backend). The flag is recomputed only for CE operation mode `3` in the inspected entry code; other modes retain its previous value. The same flag affects common H6/H5S2 pixel-shader tables and the selection of short/extended vertex attributes; VX900 additionally selects its VS and Type-04 `60h–62h` controls accordingly. This is a **shared software-state dependency**, not proof of a GPU register incompatibility or rendering defect. For Windows 98, build explicit per-draw command state and test whether any CE-style inherited mode is actually required.

**New Type-04 cross-check:** `Texture3D_H5S2` and the short `fn3DBlt_H5S2` mode both emit `60800001/61800000/62104001` at Type-04 addresses `60h/61h/62h`. The longer 3D blit mode emits `60800002/61800000/62204002`. The VX900 manual calls Type-04 `53h–62h` reserved, so these remain undocumented and must not be ported blindly.

**Static verifier:** `Chrome9_CE_Blend_Flag_State_Audit.py` passed on the original retail CE DLL and executes no driver or GPU code.

---

#### CE driver dispatch and submission boundary (2026-11-06, see main report §§86–90)

**[CE-binary confirmed]** `C3DUtils::Init` uses the following chipset-specific 3D function-table arrangement in the uploaded CE 6.0 retail DLL (SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`):

| Device | Initializer | Texture function | 3D blit function | Wait callback |
|---|---|---|---|---|
| VX800 (`1122h`) | H6 | `Texture3D_H5` | `fn3DBlt_H6` | `WaitEngIdle_CR` |
| VX855 (`5122h`) | H6 | `Texture3D_H5` | `fn3DBlt_H6` | `WaitEngIdle_CR` |
| VX900 (`7122h`) | H5S2 | `Texture3D_H5S2` | `fn3DBlt_H5S2` | `WaitEngIdle_CR` |

Additional device IDs `3157h`/`3344h` choose H2 and `3371h` chooses H5 with the CE 3D-blit pointer cleared. This does **not** establish that the devices lack any hardware 3D.

**[CE-binary confirmed]** The linked `g_3DEngineLock` critical section is entered near the end of both `fn3DBlt` functions, only once the command words and global state have been prepared. The shared `dwCurPointer` and `bGdiMultiABlend` globals are accessed before entering this lock. This does **not** prove concurrent access occurs: outer CE driver serialization is not established. Porting implication: avoid assuming the CE late lock protects a multi-threaded Win98 command builder.

**[Conditional hazard only]** Both backends have `255.0 / alpha` in a blend-global-selected Type-03/`22h` path, with no local zero check immediately before the x87 division. If a different mode inherits a nonzero blend global and supplies alpha zero, the division may receive zero; valid call paths and exception behavior are not demonstrated. Do **not** label it a proven CE or Win98 bug.

Verification: `Chrome9_CE_Dispatch_and_Submission_Audit.py` (read-only; hash and instruction-byte checks passed). Full addresses, control-flow details, test sequences, and caveats appear in the main compatibility report §§86–90.

#### VX900 CnM video-decoder subsystem and shared wait (2026-11-06)

**Confirmed difference:** the VIA CE6 retail DLL explicitly reserves `0x80000` bytes of GPU memory for VX900 (`1106:7122`) and calls `InitialBootCodeCnM` (`0x10068473`). The decoder bootstrap writes `MMIO+0x3A4=0x040BE800`, clears `+0x3A8/+0x3AC`, programs `+0x4100=reserved_base`, `+0x4104=base+0x1F000`, and `+0x4108=base+0x5F000`, then copies `0x19000` bytes from embedded `FirmWareData` into the reserved GPU aperture. `InitialFirmWareCnM` is called by WMV9/VC-1, MPEG-2, MPEG-4, and H.264 decoder routines. **This is separate video-decoder firmware, not the VX900 3D vertex-shader program.**

**Manual caveat:** VX900 Part II's Video and HQV sections give conflicting meanings for `0x3A4–0x3AC`; the HQV section describes decoder handshake and 64-bit valid tables. The CE values fit that interpretation, but silicon behavior remains untested.

**Confirmed shared command:** `WaitEngIdle_CR` is assigned to both H6 and H5S2; it writes `0x00C00000` to MMIO `0x6C` either directly or via `FE00` transport. **Additional transport family:** VX900 CnM boot uses `FE0A`, which the earlier 3D `FE00–FE07` packet decoder does not cover. See §§91–96 in the complete report and `Chrome9_CE_VX900_CnM_and_Wait_Audit.py`.

##### VX900 CnM decoder: two payloads, distinct transport, new controls (main report §§97–101)

VIA's CE driver programs an additional **512-word numbered bootstrap sequence** through decoder MMIO `0x4004`, either directly or as `FE0A` register/value pairs. The two paths encode identical words (validated across all 512 indices; normalized SHA-256 `0e16679a63d3e76265a3cc4268e81680015631d095cbd4b7c0cf1848cbe341dd`). The routine separately copies **100 KiB** of embedded firmware to the VX900's reserved GPU-memory aperture and zeros 396 decoder offsets `0x5000..0x562C`. A later runtime decoder initializer writes `0x410C`, `0x4110`, `0x4114`, `0x4170`, `0x4140`, `0x4028`, `0x402C`, `0x4000`, `0x4124`, `0x41BC`, `0x41B4`. These are distinct **video-decoder** programming mechanisms, not differences in the common 3D triangle commands. The documented HParaType-10 reset of `60h–64h` is also distinct from VX900's undocumented HParaType-04 `60h–62h` vertex controls; do not conflate them. Full register pairs, VAs and proof script are in main report §§97–101.

#### Verified Type-04 vertex declarations and undocumented VX900 routing bits (2026-11-06)

The retail CE DLL uses shared H6/H5S2 `04h:00h` NaN-test fields `0x00A80000`, but requires a VX900-specific used-input count (`04h:00h` is `00A80106` or `00A80208` rather than H6 `00A80006` or `00A80008`). The 6-/8-DWORD vertex payloads are common; the VX900 descriptors and Type-04 `12h` IR masks are not. VX900 `04h:60h–62h` has two literal configurations (`60800001/61800000/62104001` vs `60800002/61800000/62204002`), with one-hot-like bit changes correlated with adding IR2 and extending the candidate vertex shader from 8 to 12 DWORDs. The actual reserved-register semantics remain unknown; don't generalize these values to arbitrary programs. The shared NaN-mode bits select the documented primitive-list discard mode for X/Y, Z, W, but VX900 describes testing VS `oPos`.

See the complete report §§102–106 and the new read-only audit and CSV.

##### Cross-build CE audit: VX900 vertex routing and retained blending state (main report §§107–111)

Independent retail (`529fe5c8…`) and debug (`75315317…`) builds contain **byte-identical** 8-/12-DWORD candidate vertex-shader programs. They also emit the same undocumented VX900 Type-04 `60h–62h` command values; retail uses literal DWORDs and debug composes the same DWORDs by OR-ing payload masks into `60000000`/`61000000`/`62000000` selectors. The control is **not strictly a texture-count switch**: the extended vertex declaration, routing words and 12-DWORD candidate VS program are selected when `(CE_flags & 0x20) != 0 || bGdiMultiABlend != 0`. The separate pixel-shader ALU-mode choice follows `bGdiMultiABlend`, not flag `0x20` alone. Both short and extended programs appear in both DLL builds, but no third matching-form program was recovered. The actual reserved-register bit meanings and VS opcodes remain unproven. See cross-build audit script and CSV.


#### Additional verified differences — CE 2D screen copy (main report §§112–116)

- **MMIO `0x48` key polarity:** VX800/VX855 `vScreenToScreenBlt` enables keyed copy with `0x4000`; VX900 uses `0x6000`, additionally setting bit 13. The VX800/VX900 manuals define it as match-vs-mismatch selection, whereas VX855 reserves that bit. The apparent VX800 source/manual mismatch remains unresolved.
- **MMIO `0x04` rotation:** VX800/VX855 update color-depth bits `[9:8]` and preserve rotation `[1:0]`; VX900 updates depth and clears rotation `[1:0]`. This is a verified software register-write difference, not an established silicon limitation.
- **Exact shared code across all three:** the CE `init2Dbltfp` tables install the same compiled solid-fill, blank-rectangle, and fast 8-bpp-copy routines. VX900 still selects a separate normal screen-copy routine while reusing VX855 PCI mono transfer and image-pattern fill.
- **Same unbounded 2D wait pattern:** both PCI screen-copy implementations poll `MMIO+0x400` until the relevant engine-status mask clears, with no local timeout, and share the same overlap-emulation fallback.

Evidence and test instructions: see main report §§112–116, `Chrome9_CE_2D_Dispatch_and_Commands_112_116.csv` (84 pointer assignments), and `Chrome9_CE_2D_Blt_Compatibility_Audit_112_116.py` (read-only verifier).


#### 2026-11-06: CE 2D line, monochrome and pattern-fill differences (main report §§117–121)

- **Line drawing:** VX900 installs the **VX855** solid-line initializer and emitter. VX800 and VX855 line functions each have 345 instructions; all but three are identical after normalizing branch addresses, and the three differences are only DLL software-global references (`0x100CA3A4` versus `0x100CA3B4`). All use the same Bresenham GE command (`0x00103005`, low opcode 5). This is compelling shared-encoder evidence, **not** a proof of pixel equivalence for all line modes.
- **PCI monochrome transfers:** VX800 uses `0x10017E32`; VX855 and VX900 use one shared implementation `0x10022F11`. Both base their host-monochrome BitBLT on `0x000A0141`, but source-offset/pitch calculations differ; keep source packing and stride as a separate compatibility concern. The AGP-labelled mono routines differ among all three.
- **PCI color-pattern fill:** VX800 `0x1001A50A` includes an optional `ColorPatRamRotated` preprocessing call (`0x10012312`) that VX855/VX900 PCI `0x10025176` lacks. Both retain other rotation-related helpers. **AGP-labelled pattern fill** is the exact shared `0x100254E5` body across all three. This is a software-path difference, not an identified new hardware register opcode.
- **Verification:** Refer to `Chrome9_CE_2D_Line_Mono_Pattern_Audit_117_121.py` and its 11-entry `Chrome9_CE_2D_Line_Mono_Pattern_Matrix_117_121.csv`. The audit passed against the original retail DLL. Tests of rotated patterns, source-bit alignment, and drawing results on silicon are still outstanding.

#### CE Group2 AGP-labelled 2D transfer differences (sections 122–127; 2026-11-06)

- AGP mono `vMonoXferVX800_AGP` vs. `vMonoXferVX855_AGP`: 299/300 normalized x86 instructions match. Only the `cmSmartRequest` fixed-size allowance changes from `+0xA0` to `+0xC0`. Both use base GE command `0x000A0141`.
- VX900 AGP mono: same base GE BitBLT command and mostly identical emitter (290 matching instructions out of 300/301), but the software buffer request uses `align_up((source_left & 31)+width,32)*height+192` instead of a legacy 4× intermediate calculation. This is a reservation-size difference, not measured GPU traffic.
- AGP patterned mono VX855→VX900: 461 matched normalized instructions out of 467/468, same base GE command and helpers; different request geometry. VX800 additionally changes fallback/geometry setup.
- AGP image-pattern fill VX800/VX855: exact linker-map alias; VX900 retains 296 matching instructions out of 300/302 and changes reservation arithmetic.
- Exceptions: VX900 8-bpp and native AGP transfers diverge substantially from VX855 (93/254 and 190/232 matched normalized instructions). They require separate source-byte/pitch validation; the CE binary does not prove accelerated 8→16 conversion.
- Full evidence and addresses: `Chrome9_VX800_VX855_VX900_Programming_Compatibility.md` §§122–127; `Chrome9_CE_AGP_2D_Command_Compatibility_122_127.csv`; `Chrome9_CE_AGP_2D_Comparison_Audit_122_127.py`. These are static observations only; no hardware tests were performed.


#### Additional 2D / host-transfer findings (2026-11-06; detailed report §§128–133)

- All three AGP-labelled 8-bpp and native-color wrappers share the `GE_CMD` base `0x00002041` and use the `FE01` host-data transport family. ROPs are supplied separately in the high byte.
- VX800/VX855 install the same `v8BppImageTransferCN896_AGP` and `vImageTransferCN896_AGP` functions. VX900 installs `v8BppImageTransferVX900_AGP` and `vImageTransferVX900_AGP`. These are real software dispatch choices, not hardware incompatibility proofs.
- In all three paths, indexed 8-bpp pixels are expanded by CPU palette-table lookups; 16-bpp output packs two converted pixel values per DWORD. VX900 splits the data into extra 16-byte-aligned `FE01` row/chunk packets whereas the older helper uses one main host-data stream. **No GPU-side 8→16 converter is demonstrated.**
- VX900's 8-bpp and native AGP wrappers conditionally write MMIO `0x48` and `0x50` when `BltParms+0x24 & 0x04` is true, OR'ing `0x6000` into key control (source key + write-on-mismatch according to its manual). The older compared AGP functions lack those conditional writes, which does *not* mean older chips lack keying generally.
- VX900 8-bpp source-key values are optionally mapped via an indexed table and conversion callback before emission; the native path writes the raw requested key.
- Command-space requests change: VX900 8-bpp source `(align(W*bytes_per_pixel,4)+32)*H+0x124`; VX900 native `align(W*bytes_per_pixel,16)*H+0xA4`; older routines reserve `4*W*H` plus constants. **These are capacities, not bus traffic measurements.**
- Traceable evidence: retail `DDI_VIA.dll` SHA-256 `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`, linker-map function addresses and `Chrome9_CE_8bpp_Native_Transfer_Audit_128_133.py`. Complete source-address table and page references are in the main report.

#### CE transfer packet framing correction (134–139) — 2026-11-06

The verified 2D hardware BitBLT commands remain shared, but the compiled host-data packet emitters differ substantially. The VX800/VX855 CN896-derived palette helper uses one `FE01` packet for all rows and requests only the padded payload bytes through `cmSmartRequest`; its subsequent header and alignment result in a larger `cmSmartFlush` length in the modeled positive-size cases. This is a **helper request/emission mismatch**, not a proven overrun of the command manager's actual allocation. VX900's palette helper builds a separate 16-byte-header FE01 packet per row and reserves explicit overhead.

**Important detail for VX900 native color:** the RGB565 (`bytes_per_pixel==2`) helper emits one 32-byte padded FE01 packet per output DWORD, whereas its non-16-bpp branches use other framing. This is a format-specific transport difference, not a different hardware BitBLT opcode. For 33 pixels × 2 rows, the 16-bpp native helper emits 34 such packets (1,088 bytes excluding possible initial alignment). This is a modeled software stream, not measured GPU bandwidth.

The outer wrapper and inner data helper make separate buffer requests and must not be compared as one allocation. Full disassembly addresses, request formulas, scenario matrix and reproducible audit are in the main report §§134–139; see `Chrome9_CE_FE01_Row_Packet_Audit_134_139.py` and `Chrome9_CE_FE01_Row_Packet_Bounds_134_139.csv`.

---

<a id="ce-findings"></a>

## Appendix B. CE 6.0 reverse-engineering extracts

*Historical CE 6.0 source section: `VIA_CE_Chrome9_Additional_Reverse_Engineering_Findings.md`. See §§366–369 for CE 7 corroboration and build-specific differences.*

### CE 6.0 source extracts and supplementary analysis

The main chapters hold the corresponding packet, shader and 2D evidence. In this appendix, the section numbers and CE-prefixed labels identify entries in the historical source extract.

| Evidence in the main chapters | Sections |
|---|---|
| 3D commands, state and programs | [§§47–50](#c9-section-47), [§§62–89](#c9-section-62), [§§102–110](#c9-section-102) |
| 2D dispatch and transfer details | [§§112–126](#c9-section-112), [§§128–139](#c9-section-128) |

**Evidence:** user-provided `VIA Graphics Driver Group2.msi`, retail `DDI_VIA.dll` and retail `.map` file. All findings below come from static inspection; no DLLs have been executed.

#### 1. Complete short 3D initialization sequences

VIA includes two explicitly named 3D initializers: `Init3DHWRegs_H6` (retail VA `0x10063470`) and `Init3DHWRegs_H5S2` (`0x10063567`). Both select a parameter transport mode with a write of `0x00FE0000` to `MMIO+0x43C`, then send the following dwords to `MMIO+0x440`. The high byte of each dword acts as an encoded parameter/address field, and the lower 24 bits carry the value; exact semantic field names should be cross-checked against the VIA manuals.

| Parameter high byte | H6 low 24 bits | H5S2 low 24 bits | Difference |
|---|---|---|---|
| `0x00` | `0x0E1804` | `0x0E1004` | Different |
| `0x08` | `0x000249` | `0x000249` | Same |
| `0x0A` | `0x0002C9` | `0x0002C9` | Same |
| `0x0B` | `0x0002FB` | `0x0002FB` | Same |
| `0x0C` | `0x000000` | `0x000000` | Same |
| `0x0D` | `0x0002CB` | `0x0002CB` | Same |
| `0x0E` | `0x000009` | `0x000009` | Same |
| `0x10` | `0x000049` | `0x000049` | Same |
| `0x11` | `0x0002FF` | `0x0002FF` | Same |
| `0x12` | `0x000008` | `0x000008` | Same |
| `0x13` | `0x0002DB` | `0x0002DB` | Same |
| `0x14` | `0x000000` | — | H6 only |
| `0x15` | `0x000000` | — | H6 only |
| `0x16` | `0x000000` | — | H6 only |
| `0x17` | `0x000000` | — | H6 only |
| `0x18` | `0x000000` | — | H6 only |
| `0x19` | `0x000000` | — | H6 only |
| `0x20` | `0x000000` | `0x000000` | Same |
| `0x40` | `0x008408` | `0x008408` | Same |
| `0x44` | `0x000000` | `0x000000` | Same |
| `0x46` | `0x000000` | `0x000000` | Same |
| `0x47` | `0x000404` | `0x000404` | Same |

**Confirmed:** the `0x00` parameter differs (`0x0E1804` vs `0x0E1004`); H6 additionally zeros encoded addresses `0x14` through `0x19`; all remaining listed values agree. This is a direct binary-level comparison, not evidence yet that one initializer corresponds exactly to one of the three named chipsets.

#### 2. Video/HQV PCI kickoff — actual register-write differences

The retail chip-specific routines `KickOffVX800_PCI` (`0x1003179E`), `KickOffVX855_PCI` (`0x100371C9`) and `KickOffVX900_PCI` (`0x10039319`) use a similar large block of video/HQV register writes. I identified writes through their shared base-pointer addressing pattern; counts represent static instructions, not runtime invocation frequencies.

| Comparison | Register offsets present only in newer routine(s) | Offsets absent in newer routine |
|---|---|---|
| VX800 → VX855 | `0x370`, `0x374`, `0x378`, `0x37C`, `0x3CC`, `0x1370`, `0x1374`, `0x1378`, `0x137C`, `0x13CC` | `0x3E0`, `0x3E4` |
| VX855 → VX900 | `0x3A4`, `0x13A4` | — |
| VX800 → VX900 | `0x370`, `0x374`, `0x378`, `0x37C`, `0x3A4`, `0x3CC`, `0x1370`, `0x1374`, `0x1378`, `0x137C`, `0x13A4`, `0x13CC` | `0x3E0`, `0x3E4` |

**Reading:** the basic video display register writes are heavily shared. VX855 introduces accesses to the `0x370–0x37C` series and `0x3CC`, while no longer writing `0x3E0/0x3E4` in this routine. VX900 additionally writes `0x3A4` and `0x13A4` (mirrored/extended region in the code). These are code-path observations, not a complete enumeration of hardware features. See the accompanying CSV for all offsets and write counts.

#### 3. What `VX900_WAITPATCH` actually emits

`VX900_WAITPATCH` (retail VA `0x10066588`) accepts a pointer to a caller-owned command buffer. After advancing to a 16-byte boundary with `0xCC000000` padding, the function constructs **eight consecutive 32-byte blocks** with this pattern:

```text
FE000000 00000001 00000000 CC000000
0000006C 00800000 CC000000 CC000000
```

Each block is 8 dwords (32 bytes), so the fixed payload is **256 bytes** plus any initial alignment padding (0, 4, 8 or 12 bytes). The routine returns the advanced buffer pointer. Disassembly confirms calls from `fn3DBlt_H5S2` (`0x10066438`) and `Texture3D_H5S2` (`0x100683F4`).

**Interpretation caution:** the buffer words are verified, but the chip-level meaning of the FE packet and `0x6C` / `0x00800000` payload remains to be decoded against the manuals. The *name* alone does not prove the function is called only on VX900; it is embedded in generic H5S2-labelled routines. Do **not** copy the sequence into a Windows 98 driver without confirming the dispatch, submission mode and capacity.

#### 4. Additional metadata recoverable from the PDB

Besides function names, the PDB has references to actual original compilation units, including `FireVX855.cpp`, `FireVX900.cpp`, `3DUtility.cpp`, `Texture.cpp`, `Haldd.cpp`, `HalCaps.cpp`, and `HalSurf.cpp`, together with build command strings. Several build-path families (`Driver55808`, `Driver0721`, `Driver411`) appear in the PDB. These are original build-path references **not bundled source files**. They may point to reusable libraries or historical development branches, but their presence alone does not prove distinct target silicon implementations.

#### 5. Registry evidence: modes and hardware 3D use

`GFX_VIA.reg` explicitly enumerates 8-, 16- and 32-bpp desktop modes, including low-resolution 320×200, 320×240, 400×300 and 512×384 options. It also describes `UseShrink=0` and the ability to perform output scaling with the 3D engine. This proves the package contains configuration hooks and code intended for some modes; whether all modes are usable on a particular board needs verification.

#### 6. What is worth decoding next

1. Determine the chipset dispatch table that selects `H6` versus `H5S2` and conditions controlling `UseShrink` and the wait-patch call sites.
2. Derive named register fields for the two `Init3DHWRegs_*` command lists, and compare with your Windows 98 driver startup state.
3. Diff **actual state changes** in the three `KickOff*` routines, including conditionals; the raw MMIO offset list is only a first pass.
4. Trace framebuffer allocation, flip and 8bpp mode setup using the CE HAL functions, checking for reusable edge cases.
5. Reconstruct a limited 3D scaling packet stream from `fn3DBlt_*` / `Texture3D_*` for manual comparison, without assuming full Direct3D or OpenGL support.

#### Scope and limitations

This package does not include the original driver C/C++ source. PDB and map information makes static inspection easier, but complete control-flow reconstruction or actual hardware verification requires more work. These findings do not demonstrate that VX800, VX855 and VX900 are register-identical in general. They establish exactly which code sequences were recovered in this MSI build.

---

#### 7. Verified chipset dispatch: VX800/VX855 are H6; VX900 is H5S2 (new)

**Source:** retail `DDI_VIA.dll`, `C3DUtils::Init` at `0x100636BA–0x10063751`; original linker map: `VIA_3DUTILITY:3DUtility.obj`. Inspection is static only.

The routine looks up the device ID from `g_SystemInfo` (`0x100C9F78`) at structure offset `0x4C`, then uses successive compare/subtract branches. Converting those cumulative subtractions back into actual PCI device IDs gives:

| PCI device ID | Selected initializer | 3D texture function | 3D blit function | Evidence |
|---|---|---|---|---|
| `1106:1122` (VX800) | `Init3DHWRegs_H6` `0x10063470` | `Texture3D_H5` `0x10067255` | `fn3DBlt_H6` `0x10064503` | Branch to `0x10063735` after `sub eax,0x1122` |
| `1106:5122` (VX855) | **Same H6 initializer** | **Same H6 texture function** | **Same H6 blit function** | Branch to `0x10063735` at `0x100636EE` |
| `1106:7122` (VX900) | `Init3DHWRegs_H5S2` `0x10063567` | `Texture3D_H5S2` `0x10067B16` | `fn3DBlt_H5S2` `0x1006548C` | Branch from `0x100636F0–0x1006370B` |

The same routine recognizes several *additional* device IDs: `0x3157` and `0x3344` select another path; `0x3371` selects `Init3DHWRegs_H5`. Their platform names and implications are not needed for the three-chip comparison and have not been independently assigned here.

**Interpretation:** VIA deliberately shares the CE 3D *scaling/blit* code between VX800 and VX855. VX900 uses its own path. This is actual chipset dispatch, stronger evidence than coexisting function names, but it **does not prove** that all games, shaders, depth modes, or command packets are interchangeable.

#### 8. Shared full 3D-state initialization before H6 or H5S2 (new)

Both `Init3DHWRegs_H6` (`0x10063478`) and `Init3DHWRegs_H5S2` (`0x1006356F`) first call **the identical** `Init3DHWRegs_INV` (`0x100630F7`, source `3DUtility.obj`). This shared initializer performs substantial MMIO state programming:

| Target MMIO | Exact code pattern | What can be stated |
|---|---|---|
| `0x43C = 0x00010000` | Writes dwords for addresses `0x00` through `0x9A` to `0x440` | Bulk clearing of the Type-01 register bank, including VX900 locations the public manual marks reserved |
| `0x43C = (stage << 24) \| 0x00020000` | Iterates stage indices `0x0–0xE` and resets two address ranges | Bulk clearing of texture-stage register state; Type-02 indexed banks |
| `0x43C = 0xFE020000` | Clears address entries `0x00–0x0D` | Type-02 special subtype `0xFE` state |
| `0x43C = 0x15030000`, then `0x17030000` | Fixed nonzero lookup-table dwords written to `0x440` | Programs Type-03 table entries; values should be retained exactly for tests |
| `0x43C = 0x00030000`, `0x11030000`, `0x14030000` | Short loops of zero payloads | Further Type-03 palette/table setup |
| `0x43C = 0x00040000` | Clears addresses `0x00–0x34` at `0x440` | Resets a baseline subset of vertex/primitive registers |

This initialization is a **real software requirement within this CE implementation**, not proof that every write is mandatory after power-on. Since the VX900 public manual marks certain old register addresses reserved, the CE program's *zero writes* should not be mistaken for evidence those legacy shader features work on VX900.

The immediately following chipset-specific initializer then writes `0x000E1804` (H6) versus `0x000E1004` (H5S2) as the first word at MMIO `0x440`. The difference is **bit 11 only** (`0x800`), in addition to H6 writing zeros to encoded addresses `0x14–0x19` that H5S2 omits. The underlying Type-FE register semantics are not completely documented; preserve the literal values without assigning an unsupported function.

#### 9. `VX900_WAITPATCH` decoded further: likely repeated 3D idle waits (new)

The recovered eight repeated 32-byte blocks are:

```text
FE000000 00000001 00000000 CC000000
0000006C 00800000 CC000000 CC000000
```

The H5S2 3D-blit routine calls the helper at `0x10066438`, **after its ordinary drawing packet**, then appends another command with the same `0x6C` field but **`0x00FC0000`** as the payload at `0x10066453–0x10066481`.

The **VX800/VX855 manuals** explicitly document MMIO register `0x6C` as *3D / 2D Wait Control*, with:

| Payload | Older published `0x6C` interpretation |
|---|---|
| `0x00800000` | Bit 23: Wait 3D Idle |
| `0x00C00000` | Bits 23 and 22: wait 3D and 2D idle |
| `0x00FC0000` | Bits 23–18: wait 3D, 2D and DMA channels 3, 2, 1 and 0 idle |

The independent `C3DUtils` command-wait function at `0x1006363B` also uses `0x6C / 0x00C00000`: with one command-interface mode it writes `0x00C00000` directly to `[MMIO+0x6C]`; with the other it constructs a packet containing `0x6C` and `0x00C00000`. That strongly corroborates the wait-register interpretation of this specific CE packet format.

**Crucial documentation conflict:** VX900 Part I labels MMIO `0x60–0x9F` reserved, while its actual CE-selected H5S2 functions still emit these packets. The `FE...` packet framing itself must be validated before asserting that a particular VX900 silicon revision honors identical semantics. It is therefore a **strongly supported inference**, not a hardware-proven behavioral claim. `VX900_WAITPATCH` looks like a repeated wait/ordering workaround, not an arbitrary 3D computation.

**Implication for driver testing:** compare single wait versus 8 repeated waits with a short timeout and full logging *only on a recoverable device after confirming command-buffer lengths*. Avoid blindly transplanting the patch to the Windows 98 VX855 driver; it is selected for VX900 H5S2 in this CE package.

#### 10. H6 vs H5S2 blit packet comparison (new)

The two binaries have a substantial common packet-writing prefix but are **not identical**. A filter restricted to statically immediate `mov DWORD PTR [buffer],constant` stores shows a common initial run of **38 literal words** before the first differences. This count excludes copied tables, computed words, and branch outcomes, and must not be expressed as a percentage of hardware-command compatibility.

| Observed item | H6 `fn3DBlt_H6` | H5S2 `fn3DBlt_H5S2` |
|---|---|---|
| Front-end packet framing | Common `0xFE020440`, `0x43C`, 3D state setup | Shares the corresponding early packet sequence |
| Mid-sequence literal pair | `0x01020100`, `0x02090803` | `0x010000C2`, `0x02000142` |
| Conditional extended state block | No matching block | Adds `0x60800001`, `0x61800000`, `0x62104001` or corresponding two-component case, followed by Type-03 table data |
| Blend-mode conditional | Some code paths reference same global | H5S2 explicitly branches on `bGdiMultiABlend` (`0x100CE7EC`) and a per-operation flag (`+0x1C`, bit `0x20`) |
| End packet | Header sequence including `0xFE020420`, `0x41C`, `0x00300000` | Calls `VX900_WAITPATCH`, then appends `0xFE000000 ... 0x6C,0x00FC0000` |

The H5S2 variant also includes an alternate two-component case (`0x60800002`, `0x61800000`, `0x62204002`). Actual register semantics of these values are not assigned without full packet decoding.

The presence of these differences demonstrates why the VX900 3D blit backend should not be swapped wholesale with the VX855 one, despite shared outer submission mechanisms.

#### 11. Remaining packet-decoding work, specifically grounded in this MSI

1. Decode the header words `FE020440`, `FE03043C`, and `FE000000` against the manuals and known command stream implementations. Distinguish direct MMIO writes from buffered command packets.
2. Extract static tables referenced at `0x1000870C`, `0x1000872C`, `0x1000875C`, and `0x100088B4` that the H5S2 routine copies into command buffers; they are *not* captured by simple immediate-word scanning.
3. Analyze the `bGdiMultiABlend` branch and `+0x1C` flags to determine how H5S2 selects alternate blending/texture state, particularly for Windows CE alpha-composited GDI.
4. Compare `fn3DBlt_H6/H5S2` with the user’s Windows 98 driver *source* before claiming any direct performance or stability fix; that source is not part of this MSI.
5. Investigate whether a shorter idle-wait sequence preserves ordering on VX900 with logged progress and bounded timeouts, but only after documenting packet allocation and control-flow paths.

#### 12. Provenance and verification

- Binary analyzed: **retail x86 Windows Embedded CE 6.0 `DDI_VIA.dll`** from `VIA Graphics Driver Group2.msi`.
- Linker map: `DDI_VIA.map` with `VIA_3DUTILITY:3DUtility.obj`, `VIA_3DUTILITY:Present3DBlt.obj`, and `VIA_3DUTILITY:Texture.obj` function and symbol names.
- VIA manuals cross-checked: VX800/820 Part II, *CR Registers in 2D Register Space (60–6Ch)*, printed p.151; VX855/875 Part II, same heading, printed p.75; VX900 Part I, `0x60–0x9F` reserved in 2D register summary.
- No driver code has been executed, no hardware writes performed, and no hardware observations inferred from compile-time code selection alone.


##### AGP kickoff direct-register-write comparison caveat

The same CE package also contains separate `KickOffVX800_AGP`, `KickOffVX855_AGP`, and `KickOffVX900_AGP` functions (retail VAs `0x100321A1`, `0x10037C7C`, and `0x10039E11`). They construct buffered AGP-style command packets rather than using the simple direct MMIO access pattern extracted for the `*_PCI` routines. **Therefore, the PCI offset-difference CSV must not be presented as an AGP command-difference matrix.** A full AGP comparison requires packet-by-packet data-flow reconstruction including computed words and copied constant tables.



---

#### 51. CE's two execution transports for the same FE command packets

The family package also exposes an important *transport* distinction. In both texture helpers, an observed command-interface field at offset `+0x70` determines which software path processes the generated packet:

| `[command_iface + 0x70]` | H5 texture, source | H5S2 texture, source | Actual path |
|---|---|---|---|
| `== 0` | `0x10067589–0x100675A7`; submit `0x10067ABA–0x10067ACC` | `0x10067E82–0x10067EA3`; submit `0x10068417–0x1006842C` | Request a writable command area through `cmSmartRequest` (`0x1004E42E`); after filling it, call `0x1004E51D` with produced byte length and flag `1` |
| `!= 0` | `0x100675AC`, dispatch `0x10067AD1–0x10067ADD` | `0x10067EA5`, dispatch `0x1006842E` onward | Construct packets in a static command area at `0x100CDD60` and execute them through the in-process FE packet interpreter `0x10063766` |

The interpreter at `0x10063766–0x100638E4` recognizes packet high-word classes `FE00`, `FE01`, `FE02`, `FE03`, `FE04`, `FE05`, and `FE06`, as well as `CC...` padding. For example, its `FE02` handler writes the selector to one MMIO offset and streams subsequent words to another. The `FE00/FE03` paths also perform MMIO writes. **Crucial distinction: this is *not* proof of software rasterization**—both transports can send commands to the hardware.

A driver port should therefore separate **packet generation** from **packet transport**. The available evidence supports sharing some packet generators across VX800/VX855/VX900 while allowing chipset- and environment-specific queue submission, idle waits, and FIFO management. The exact hardware transport selected by each interface mode, its relative performance, and any relationship to the VX855 intermittent command queue hangs require runtime traces.

**Source-level reproducibility:** the local static check script `_ce_further_trace.py` verifies the relevant disassembly, constants, two packet-submission paths, and example floating-point output against the extracted retail DLL. This script is analysis-only and does not issue hardware commands.


---

#### 52–53. Further verified vertex/primitive findings (retail CE DLL; 2026-11-06)

The master Chrome9 compatibility report now contains **§52–53** with the full binary addresses, emission layout, and VIA manual page references. Key recovered facts:

- H5 sends `04:25=0xF6C000`: X/Y/Z/W, diffuse C0, specular C1, S/T = **8 DWORDs per vertex**. H5S2 sends `04:25=0xF0C000`: X/Y/Z/W, S/T = **6 DWORDs**; both send three vertices with `04:24=3`. The only active FVF-mask difference is bits 18/17.
- H5's emitted vertex payload is `(X,Y,0,1,0xFFFF0000,0xFFFF0000,S,T)`; H5S2's is `(X+bias,Y+bias,0,1,S,T)`, with a `786432.0` bias on positional coordinates. These are payload word descriptions, not proof of equivalent final colors.
- H5S2 also declares Float4 IR0 with `04:01=0x010000C2`, Float2 IR1 with `04:02=0x02000142`, and activates IR0/IR1 with `04:12=0x12000003` (VX900 Part II pp.160–161,165). These explain the six-float layout.
- H5S2 sets `04:2F=0x2F582000`, providing default W=1, color0 RGB=1, color0 A=1, and texture Q=1 (VX900 Part II p.176). H5S2 sets `04:28=0x28F0C000`, versus H5 `0x28FFC000`, another color/fog-related default mask difference.
- H5 uses `04:20=0x20000000` (full vertex cycle), H5S2 uses `04:20=0x201C5C00` (automatic fast/reuse cycle). H5 `04:23=0x23000F02` vs H5S2 `0x23002E02` changes solid to flat-from-B shading and reverses the facing convention; both specify filled triangles.
- H5S2 sends `04:60/61/62` as `0x60800001`, `0x61800000`, `0x62104001` while VX900's public manual marks that Type-04 range reserved. **Do not port these to VX855 or assign them undocumented meanings**.

Evidence VAs: H5 `0x10067947–0x10067A9B`, H5S2 `0x10068284–0x100683EE`, with bias block `0x10067E14–0x10067E50`. Machine-checked in `_ce_add_vertex52_53.py`. No hardware execution.


---

#### 54. Crucial H6 vs H5S2 3D blit cross-check: commands shared more broadly than texture helpers suggested

Further static disassembly demonstrates that the **VX800/VX855 `fn3DBlt_H6`** uses precisely the same `04:20=0x201C5C00`, `04:23=0x23002E02`, `04:24=3`, and single/dual texture mask pairs (`04:25/28 = 0xF0C000` or `0xF0CC00`) as the VX900 `fn3DBlt_H5S2`. Both copy the same 32-DWORD Type-04 state table at VA `0x100087A0`. Binary evidence: H6 `0x10064BA8–0x10064C50`, H5S2 `0x10065B32–0x10065BEF`.

VIA retains different input declarations: H6 starts `04:00=0x00A80006` (or `0x00A80008`), H5S2 `04:00=0x00A80106` (or `0x00A80208`), and H5S2 sets Float4/Float2 (and optional additional Float2) input registers plus `04:12=0x12000003`/`0x12000007`. The dual-texture mask adds exactly bits 11/10 = texture stage 1 S/T. The detailed comparison and manual references are in the main report **§54**.

**Correction:** the 8-vs-6 vertex format and full-vs-fast cycle distinction in §52–53 describes `Texture3D_H5` vs `Texture3D_H5S2` **only**; VX855 already demonstrably uses the six-word/fast-cycle commands in a separate CE blit path. There is no basis for calling them VX900-only hardware features.


---

#### 55–57. Further static findings: shared H6/H5S2 shader programs and VX900 vertex-shader candidate

The CE `fn3DBlt_H6` and `fn3DBlt_H5S2` independently send exactly the **same pixel-shader programming data** to Type-03 subtypes `20h`, `21h`, and `22h`. In the longer mode, both copy 12 DWORDs from DLL VA `0x100088B4` (three 4-DWORD ALU instructions under the VX855 manual), load two TAU words from `0x100088E4/0x100088E8`, and construct the same four-component constant. In the shorter mode both use four ALU words from `0x1000875C` and literal TAU word `0x301001B0`. Evidence: H6 `0x10064C61–0x10064D80`, H5S2 `0x10065CEE–0x10065E0D`.

The VX900 manual marks detailed subtypes `20h–22h` reserved, but the retail CE implementation **does** submit identical programs in the VX900 path. Treat ISA equivalence outside these particular programs as unknown.

H5S2 additionally sends Type-03 subtype `31h` (called “Vertex Shader Instruction” in the VX900 command overview but reserved in the detailed register description): **8 DWORDs** from `0x1000870C` for single texture, or **12 DWORDs** from `0x1000872C` when `[input+0x1C]&0x20` or `bGdiMultiABlend` is true. The larger table is the shorter one plus four more words. Likely two versus three 128-bit VS instructions, but their bit-level opcodes and executed behavior remain undocumented. Both branches also program different `04h:60–62h` words matching one/two texture sets, but these Type-04 addresses are also reserved in the public VX900 manual.

Two independent H5S2 functions reference the `786432.0` coordinate bias: `fn3DBlt_H5S2` at `0x10065E39` and `Texture3D_H5S2` at `0x10067E19`. Neither corresponding H6/H5 function uses it. See master report §§55–57 for complete tables, source VAs, interpretations, and limits. Verified byte-for-byte by `_ce_verify_shader_55_57.py`; no graphics hardware was exercised.


---

##### 58–61. Further CE-driver comparisons: shared VS program, C-to-S bias, and per-operation pixel shaders (2026-11-06)

- `Texture3D_H5S2` emits the exact same Type-03 subtype `31h` **8-DWORD program** (`0008014C 00000000 00000000 0008021F / 00084124 00000000 00000000 00080283`) used by single-texture `fn3DBlt_H5S2`. The texture function writes it inline at `0x100681D8–0x10068206`; the blit function copies it from `0x1000870C`. The dual-texture/blend blit adds a third 4-DWORD instruction candidate; instruction semantics remain unpublished.
- Both `Texture3D_H5` and `Texture3D_H5S2` **already program the C-to-S X/Y offset as `786432.0`** (`04:41/42` and `04:44/45`). They set scales for X, Y, and Z to `1.0`. The same X/Y words appear in the 32-DWORD table shared by `fn3DBlt_H6` and `fn3DBlt_H5S2` at `0x100087A0`. This refines the prior claim of a VX900-only bias: the VX900 routines additionally apply `786432.0` in software to emitted vertex coordinates, while the hardware transform state has the constant in both generations; whether both stages execute is unknown.
- `04:47` differs between **texture helpers only**: H5 `0x47400000` (Z offset `0x00400000`, subnormal float ≈ `5.87747e-39`) versus H5S2 `0x47000000` (zero). The H6 and H5S2 **blits both use zero**, so do not treat this as a general chipset-specific 3D rule.
- Pixel shaders differ by *operation*: the CE **texture helper** H5 programs `01:92=0x92010003` (1 TAU, **3 ALU** instructions); H5S2 programs `01:92=0x92010001` (1 TAU, **1 ALU** instruction). This exactly matches their emitted Type-03 subtype-`20h` 12/4-DWORD streams. Their one-word TAU instruction `301001B0` is identical. The H5 last ALU instruction differs from H5S2's only instruction in DWORD 2 (`00F20105` vs `00F20005`). By contrast the CE **3D blit** H6 and H5S2 programs are shared as documented in §55.
- Primary manual basis: VX800/VX855/VX900 Part II Type-04 `40h–52h` clipping-to-screen transform; VX855 Part II Type-01 `92h` Pixel Shader Setting 3. VX900 published detailed shader subtype documentation is incomplete/reserved. No hardware behavior has been tested. Full binary anchors, words, and reproduction procedure: master report §§58–61 and `Chrome9_CE_Command_Verification_58_61.py`.


---

#### 90. New build-pinned static verification and remaining blockers

The companion script **`Chrome9_CE_Dispatch_and_Submission_Audit.py`** re-reads the original retail DLL, checks SHA-256, verifies the chipset ID subtraction chain and installed callback pointers, checks the common H6/H5S2 wait callback, confirms CE critical-section helper calls, checks that both command-pointer/blend writes occur before their submission locks, and verifies the conditional x87 reciprocal-alpha instruction sequences. It neither executes the driver nor touches graphics hardware.

```sh
python Chrome9_CE_Dispatch_and_Submission_Audit.py --dll retail/DDI_VIA.dll
```

**Result:** All static checks passed against the original retail CE DLL. These facts remain unknown without additional evidence: which CE public/GDI callers permit each `KMD_3DBLT_DATA_tag` mode; whether any outer lock serializes the entire draw; legal alpha-zero behavior; what VX900's undocumented `04h:60h–62h` bits do at the silicon level; and the instruction semantics of the Type-03 subtype `31h` programs. No Windows 98 driver source or hardware was modified in this pass.

#### CE-91. VX900-only CnM decoder bootstrap and 512 KiB reservation (2026-11-06)

**Scope and evidence.** This section is an additional, **separate video-decoder subsystem** discovered in the same VIA CE 6.0 retail `DDI_VIA.dll`, not an alternative to the H6/H5S2 triangle / pixel-shader path. All addresses below are preferred image VAs for the SHA-256-verified retail build `529fe5c819bc303b989316db077e3a172374fcdf99dd6f06d9641945ec251a19`. The source linker map names `VIA_DXVAMISC:CnMInit.obj`, `InitialBootCodeCnM` (VA `0x10068473`), and `FirmWareData` (VA `0x100A50C0`).

In the `VIA` constructor, the chip-specific branch at `0x1000C4EC–0x1000C516` performs the following only if the device ID equals **`7122h` (VX900)**:

1. Decrease the graphics-memory budget at global `0x100C9F9C` by **`0x80000` bytes = 512 KiB** (`0x1000C4FD`).
2. Store the new budget-relative base in global `0x100CA04C` (`0x1000C50C`).
3. Call **`InitialBootCodeCnM`** (`0x1000C511`), which uses the reserved base and programs decoder-related MMIO.

This is an implementation-level *allocation/reservation* rule for the VX900 CE driver. It does not prove a VX900 hardware requirement for all operating systems, and it must not be mixed with the existing H5S2 3D state tables. Nothing here shows that the Win98 game's 3D vertex or pixel shaders need these firmware commands.

#### CE-92. Exact decoder memory registers and embedded firmware copy

The end of `InitialBootCodeCnM` contains a concrete, statically verifiable address layout (`0x1006C9AB–0x1006CA23`):

| MMIO offset | Value passed by CE driver | Instruction evidence |
|---|---|---|
| `0x4100` | reserved GPU-memory base | `0x1006C9B0`, direct variant `0x1006C9BE` |
| `0x4104` | base + `0x1F000` | `0x1006C9CC–0x1006C9EE` |
| `0x4108` | base + `0x5F000` | `0x1006C9EE–0x1006CA11` |

The code supports **both** a register/value-pair buffer and direct MMIO writes (`MMIO + 0x4100`, `+0x4104`, `+0x4108`) according to its transport selection. The meaning of the three regions beyond being decoder-address inputs has **not** been recovered from the supplied public 3D manual. The offsets fit within the driver-reserved `0x80000`-byte span, but that alone does not prove their sizes or lifetime.

Next, `0x1006CA11–0x1006CA23` calls imported `_memcpy` (`0x1007F28C`) with:

```
source      = 0x100A50C0 (FirmWareData, VIA_DXVAMISC:CnMInit.obj)
destination = mapped GPU aperture base + reserved memory offset
length      = 0x19000 bytes (102,400 bytes / 100 KiB)
```

SHA-256 of that **exact embedded 0x19000-byte payload** in the retail DLL:

```
15b2394075a92da77e143dd869f3d85246457185459af4c06b930c3ba13086cb
```

The copy operation and symbol `FirmWareData` are unambiguous **static facts**. The payload's internal instruction set has not been decoded, and copying it does not by itself establish that the CE decoder starts successfully on any particular board.

##### Four actual users of the CnM firmware initializer

The routine **`InitialFirmWareCnM`** at `0x1006CA96` is called by the following symbol-identified decoders in the same DLL:

| Caller function / compilation unit | Call VA |
|---|---|
| `ProcessDecoder_VLD_wmv9vc1_CnM` (`VIA_WMV9:wmv9vc1_process_CnM.obj`) | `0x10053601` |
| `ProcessDecoderMPEG2VLD_CnM` (`VIA_MPEG2:Mpeg2Func_VX900.obj`) | `0x1005750E` |
| `ProcessDecoder_VLD_mpeg4_CnM` (`VIA_MPEG4:Mpeg4_Process_CnM.obj`) | `0x1005B0EF` |
| `H264ProcessDecoder_CnM` (`VIA_H264:h264vld_CnM.obj`) | `0x1005D11E` |

Those are real static call edges, **not** a successful hardware decode test or evidence that every compression profile is accelerated. The distinction is significant: a firmware initializer for MPEG/H.264/VC-1 is a video subsystem, not automatically an unrecognized VX900 vertex-shader program.

#### CE-93. VX900 CnM HQV control and an unresolved manual overlap

Early in `InitialBootCodeCnM`, the retail DLL uses the graphics MMIO base and writes:

| MMIO offset | Initialization value | VA |
|---|---|---|
| `0x3A4` | `0x040BE800` | `0x100684AF` |
| `0x3A8` | zero | `0x100684C0` |
| `0x3AC` | zero | `0x100684CE` |

**Manual conflict:** VX900 Part II, *Video Registers* (printed p. 46 / PDF p. 52–53) describes `0x3A4` as **HQV Color Adjustment Control 2**. Its separate *HQV Registers* chapter (printed p. 60 / PDF p. 66–67) instead calls `0x3A4` **HQV Sharpness Control and Decoder Handshake Control**, `0x3A8` **HQV SW Macro Set Valid Table 0**, and `0x3AC` **HQV SW Macro Set Valid Table 1**. Both descriptions appear in the supplied manual. The overlap is an unresolved documentation/address-context issue; do not silently choose either view for all chipsets and operating modes.

When decoded using the **HQV decoder-handshake** chapter, `0x040BE800` means:

| Bitfield | Decoded value |
|---|---|
| Bit 31, sharpness | disabled |
| Bit 26, wait frame done | **1 / enabled** |
| Bits 25:20, source frame ID | 0 |
| Bit 19, wait timeout | **1 / enabled** |
| Bits 18:13, timeout cycles | 31 units (manual: 500,000 cycles per unit) |
| Bits 12:11, software set/reset valid | `01b` |
| Bits 10:5, valid-table index | 0 |

The two following zero writes would clear both 32-bit macro-valid tables under this interpretation. The correspondence with the binary's decoder boot sequence makes the HQV interpretation plausible, **not independently verified on hardware**. This evidence belongs in the video/HQV platform adapter, not the reusable `HParaType=04h` vertex-attribute encoder.

#### CE-94. Common H6/H5S2 3D idle command, with two transports

The retail DLL's `WaitEngIdle_CR` (VA `0x1006363B`) is assigned as the `C3DUtils` wait callback for **both** H6 and H5S2 at `0x10063749`, so the same *code body* is used by the listed VX800/VX855/VX900 dispatch paths.

It tests the context flag at `[context+0x70]` and takes one of two forms:

- Direct-MMIO mode: `mov [MMIO+0x6C], 0x00C00000` (`0x1006364B`).
- Buffered mode: constructs an `FE00` register-write packet specifying offset `0x6C`, value `0x00C00000`, then passes the packet to the command transport (`0x10063653–0x100636A1`). A **separate old-chip `3157h` special case** uses a different register-word encoding (`0xF000001B`); do not apply that special case to H6/H5S2.

VIA's VX800/VX855 command-register descriptions map the value's bits 23 and 22 to **wait 3D idle** and **wait 2D idle**. This matches the common driver operation and strengthens the shared-command catalog. The VX900 public manual's different organization/labels mean that identical CE writes still need hardware testing before claiming identical silicon wait semantics. This common wait should not be confused with the **additional** VX900 H5S2 `VX900_WAITPATCH` packet sequence analyzed earlier.

#### CE-95. Transport-family limit: `FE0A` is outside the simple 3D packet interpreter

The CnM boot routine has a conditional **buffered** transport path that requests a `0x5140`-byte command allocation (`0x10068508`) and uses a literal `0xFE0A0000` marker (`0x1006850D`). The alternative path writes directly to the appropriate MMIO offsets (`MMIO+0x4000/0x4004` for the decoder boot-data aperture and later register pairs).

This is a **new packet family relative to the previously analyzed `C3DUtils::KickOffCmd_Inv` interpreter**, which was mapped chiefly for `FE00–FE07`. It should **not** be decoded using the ordinary FE00/FE03 word-count rules merely because its top byte starts with `FE`. The CE package has multiple command producers and dispatch contexts. A normalized family-wide decoder should dispatch on **subsystem and exact packet family**, and preserve unknown `FE0A` payloads as opaque until its own processing rules are proven.

#### CE-96. Shared lock-object observation and verification status

The same retail binary stores address **`0x100CE7D8`** (`g_3DEngineLock`, a CE `CRITICAL_SECTION`) into:

- `C3DUtils` field `+0x10` at `0x100636B2`;
- `CD3dmProxy` field `+0x04` at `0x10062EA7`.

Thus the two helper objects reference the **same lock object**, not two independent engine locks. However, this does **not** establish whether an outer CE graphics API always acquires the lock before invoking the H6/H5S2 command builder. The earlier observed late `Enter3DEngine` / `Leave3DEngine` calls still protect the builder's final submission region locally, not its whole packet construction. No rendering race or double-lock bug is established without the upstream call-chain and runtime serialization rules.

**Companion audit:** `Chrome9_CE_VX900_CnM_and_Wait_Audit.py` checks the exact SHA-256 retail DLL, relevant linker symbols, VX900 reservation branch, HQV values, decoder memory pointers, firmware image SHA-256 and four decode call edges, both shared idle-wait transports, and the shared CE lock pointer. It is **read-only**, executes no CE code, and touches no GPU. All checks passed against the supplied retail DLL.

**Porting outcome:** split *shared 3D commands* from a *VX900-only video firmware/HQV initialization block*. No justification has been found to add the firmware load or `FE0A` decoder boot commands to the existing VX855 Windows 98 3D driver. For a future VX900 video stack, re-evaluate these commands only with compatible decoder support and correct framebuffer carve-out.


#### VX900 CnM boot stream and Type-04/Type-10 evidence

The retail DLL's VX900 `InitialBootCodeCnM` emits **512 ordered decoder boot-control DWORDs**, numbered `0x0000–0x01FF` in their upper halfwords, through both buffered `FE0A` register/value-pair transport and direct MMIO `0x4004` writes. Every one of the 512 values matches across paths. The sequence begins `0000E40E`, `00010020`, `0002E470`, `0003E190` and ends `01FEE190`, `01FFE190`. The normalized 2,048-byte stream SHA-256 is `0e16679a63d3e76265a3cc4268e81680015631d095cbd4b7c0cf1848cbe341dd`. Full independently verified values and instruction VAs: `Chrome9_CE_VX900_CnM_Bootstrap_512_Words.csv`. This is **not** the separate 100 KiB `FirmWareData` copied to VRAM.

`FE0A`'s buffered header has four DWORDs (`FE0A0000`, computed pair count, `00F60000`, zero). The driver generates logical MMIO `(offset,value)` pairs, then patches the second header word from the pair count and flushes the command buffer. The decoder fields' private meanings remain unknown. A following loop clears 396 DWORD-aligned decoder MMIO offsets `0x5000..0x562C` and writes decoder control / memory pointers at `0x4024`, `0x4018`, `0x4000`, `0x4100`, `0x4104`, and `0x4108`. The runtime `InitialFirmWareCnM` writes further decoder controls (`410C=4`, `4114=0`, `4110=4`, `4170=0`, `4140=0`, `4028=895440`, `402C=1`, `4000=1`, `4124=04000000`, `41BC=100`, `41B4=20`). These are confirmed **video** programming operations, not 3D vertex/shader instructions.

Crucial negative control: `Init3DHWRegs_INV` writes `60..64` after selecting **HParaType 10h**, whereas VX900's varying `60..62` vertex control commands select **HParaType 04h**. The equal address prefixes do not prove equal register purpose. The VX900 Type-04 fields are still undocumented. See main report §§97–101 for exact VAs, header framing, pointer-layout caveats, and 512-row audit evidence. All analysis was read-only.

#### 111. Actionable command-compatibility update and reproducibility artifacts

1. **Shared primitive path remains valid for the scoped blits.** Retain the six/eight-DWORD vertex payload builder and common render-target, texture, pixel-shader, and triangle-state helpers already documented in §§48–106.
2. **Keep VX900 declaration and routing as one mode-dependent operation.** The H5S2 vertex `04h:00h–03h`, input mask `04h:12h`, Type-04 `60h–62h`, and subtype-`31h` VS table change together. Do not mix short declarations with extended program/routing words.
3. **Model effective state, not just a texture count.** The CE extended decision is `(flags & 0x20) || bGdiMultiABlend`; this is particularly important if translating the CE helper into a stateless Windows 98 renderer. Avoid carrying a hidden process-global blend flag across unrelated draws.
4. **Use the branch truth table for further audits.** Four input combinations provide a better static test matrix than merely “one vs two textures.” These tests check software command selection, not the validity of undocumented GPU controls on real hardware.
5. **Do not infer absent commands from literal search alone.** In debug the routing words are assembled via ORs. Compare normalized output words and surrounding Type-04 packet selectors.

**Companion files:** `Chrome9_CE_CrossBuild_VS_Routing_Audit_107_111.py` rehashes both user-supplied DLLs, verifies both 8-/12-DWORD shader tables at their preferred VAs, checks instruction data parity, checks routing-word construction and selection predicates, and exports `Chrome9_CE_Retail_Debug_Vertex_Parity_107_111.csv`. It is read-only, calls `objdump` for disassembly only, and neither loads a CE driver nor writes MMIO. All current checks pass on the two original binaries.

**Source limits:** VX900 Part II, 3D Register chapter, Type-04 description (printed pp.154–159 and reserved `53h–62h` at p.173). The CE group2 `DDI_VIA.dll` binaries supply the instruction and packet evidence; the manuals do not provide a public decode of Type-04 `60h–62h` or subtype-`31h` instructions.


#### CE 6.0 2D screen-copy and dispatch references


#### 127. Evidence classification, reproducibility and next hardware tests

**Proven in this CE snapshot:** identical VX800/VX855 AGP image-pattern emitter; 299/300 instruction agreement in AGP mono; common `0x000A0141` mono GE command across the three; VX900's 32-bit row alignment in the examined mono and patterned software reservation calculations; reuse of `cmSmartRequest` and `cmSmartFlush` by all three. The underlying source invokes different *software* reserve sizes. The source does not supply a runtime command queue trace or pixel-equivalence results.

**Not proven:** byte-identical final *dynamic* command streams, hardware activity on an external AGP bus (the label follows VIA function names), a performance advantage from VX900's smaller reservations, command-buffer overflow in any path, or correct image output for every bpp/rotation/source-origin case.

**Recommended next controlled tests:** (1) instrument the buffer request and actual write/flush length for an 8×8, 32×32 and 33×9 host monochrome transfer; (2) vary source-left-bit offsets 0,1,7,8,15,31; (3) compare keyed and unkeyed 8-bpp host copies; (4) compare native-color transfers at odd byte pitches, 8/16/32-bpp destinations and all four rotations; (5) on real hardware, use bounded timeouts and CPU pixel-reference comparisons. Keep the tests separate for PCI-labelled and AGP-labelled paths.

**Companion reproducible evidence:** `Chrome9_CE_AGP_2D_Comparison_Audit_122_127.py` reads the original CE driver ZIP, verifies the retail DLL SHA-256, disassembles the linker-map function ranges without running any driver code, compares 11 pairs, verifies the shared mono command, and exports `Chrome9_CE_AGP_2D_Command_Compatibility_122_127.csv` with original function addresses, instruction counts, match counts and first differing instructions. Its checks pass against the supplied retail DLL. The comparison is a **static source-to-binary observation**, not hardware validation.


---

See [the Group2 indexed and native host-transfer evidence](#c9-section-128).



<a id="ce-overview"></a>

## Appendix C. CE 6.0 package and driver inventory

*Historical CE 6.0 source section: `VIA_CE_Chrome9_Family_Driver_Analysis.md`. The independent CE 7 package and source identities are in §§365–370.*

### VIA Windows Embedded CE 6.0 — Chrome9 Family Driver Analysis

**Source:** User-supplied `VIA Graphics Driver Group2.msi`  
**Method:** Offline MSI database and CAB extraction; static PE export, linker-map, and disassembly inspection. No binaries executed.

#### Verified package scope

`PUBLIC/COMMON/CATALOG/GFXGrp2.PbcXml` explicitly describes this package as **“VIA Display Driver For VX800,VX855 and VX900”**, with OS version **6.00** and driver/catalog file version **7.06.32.411**. The catalog targets a Windows CE 6.0 Platform Builder integration, not a Windows 98 or Windows XP installer.

The package carries the same platform files under both `PLATFORM/VIACEPC/Drivers/GraphicsGrp2` and `PLATFORM/VIABSP/SRC/Drivers/GraphicsGrp2`; these are duplicate destinations, not separate chipset builds.

#### Extracted build inventory

| Build | DLL size | Exported functions | Symbol evidence |
|---|---:|---|---|
| Debug | 1,483,264 bytes | `DisplayInit`, `DrvEnableDriver`, `HALInit` | `DDI_VIA.pdb`, `DDI_VIA.map`, `DDI_VIA.rel` |
| Retail | 867,840 bytes | `DisplayInit`, `DrvEnableDriver`, `HALInit` | Matching retail `.pdb`, `.map`, `.rel` |

The MSI contains four `DDI_VIA.dll` files, but they reduce to **two distinct SHA-256 hashes**, one per build type. Both are **32-bit x86 Windows CE 6.00** PE DLLs.

The included `sources` file has an empty `SOURCES =` field: **this package does not contain the main graphics-driver C/C++ source code**. The linker maps and PDBs still expose many internal symbol names and compilation object files, making static reconstruction much more practical.

#### Important verification against the VIA manuals and OpenChrome

This is a VIA-authored implementation spanning all three targets. It gives stronger evidence of what VIA deliberately shared or separated than source-name similarity alone.

##### Exact shared addresses in the retail map

These pairs or groups point to the **same linked function address**, strong evidence of shared compiled routines within this build:

| Function or action | Shared symbols | Retail address |
|---|---|---|
| 2D screen-to-screen copy | `vScreenToScreenBltVX800` / `vScreenToScreenBltVX855` | `0x1001A8B1` |
| Solid-color fill | `vSolidFillVX800` / `vSolidFillVX855` | `0x10024CDE` |
| Native transfer | `vNativeXferVX800` / `vNativeXferVX855` | `0x10018AFD` |
| 2D GE register initialization | `init2DGERegVX800` / `init2DGERegVX855` | `0x1001B652` |
| Video overlay flip (PCI) | VX800 / VX855 / VX900 | `0x1002D499` |
| Video command kickoff (PCI) | VX800 / VX855 / VX900 | `0x1003B14A` |
| Capture0 update (PCI) | VX800 / VX855 / VX900 | `0x1007F5FD` |
| HQV wait for flip status | VX800 / VX855 / VX900 | `0x10082028` |

**Caveat:** Equal linker addresses show this executable uses the same code body; they do not prove all possible feature modes behave identically on all silicon revisions.

##### Separate linked functions

These are implemented at different addresses; they should be inspected individually rather than assumed interchangeable:

| Function | VX800 | VX855 | VX900 |
|---|---|---|---|
| `KickOffVX*_PCI` | `0x1003179E` | `0x100371C9` | `0x10039319` |
| `KickOffVX*_AGP` | `0x100321A1` | `0x10037C7C` | `0x10039E11` |
| `KickOffHQVVX*_PCI` | `0x1003291B` | `0x1003846E` | `0x1003A6DA` |
| `InitialVideoVX*` | `0x1002B300` | `0x1002D680` | `0x1002E231` |
| `InitialHQVVX*` | `0x100802FD` | `0x1008205B` | `0x10082ABE` |
| `vScreenToScreenBltVX*` | `0x1001A8B1` | `0x1001A8B1` | `0x10027ED4` |

An explicit comparative audit of the retail linker map found **61** normalized function names present for all three chipsets. Of these, **22** map to the same linked address for all three, while **39** map to two or three different addresses. These counts concern *matched exported linker-map symbol patterns*, **not every function in the driver** and **not 61 hardware commands**.

Different addresses indicate different code bodies, but **do not themselves prove incompatible register encodings**. This qualifies an earlier OpenChrome observation: OpenChrome shares an H6 implementation across the three chips for basic 2D operations, while VIA's own CE driver sometimes uses separate routines.

#### 3D engine evidence — useful but limited

`GFX_VIA.reg` states that the driver can use the **3D engine to shrink/scale TV, HDTV, HDMI, DVI, LCD or DisplayPort output**. It defines `UseShrink` (default **0**, disabled). This is direct evidence that 3D-related routines are used for 2D/video functions, **not proof of a complete hardware 3D gaming driver**.

Important function symbols in the retail map include:

| Function | Purpose suggested by name | Address |
|---|---|---|
| `Init3DHWRegs_H6` | Initialize H6 engine-related 3D state | `0x10063470` |
| `Init3DHWRegs_H5S2` | Initialize H5S2 3D state | `0x10063567` |
| `WaitEngIdle_CR` | Command-regulator engine wait | `0x1006363B` |
| `fn3DBlt_H6` | 3D blit path for H6 | `0x10064503` |
| `fn3DBlt_H5S2` | Alternative 3D blit path | `0x1006548C` |
| `VX900_WAITPATCH` | VX900-named patch to emitted command stream | `0x10066588` |
| `Texture3D_H2` | Earlier engine 3D texture path | `0x100666F8` |
| `Texture3D_H5` | H5 3D texture path | `0x10067255` |
| `Texture3D_H5S2` | H5S2 3D texture path | `0x10067B16` |

##### Specific VX900 lead: `VX900_WAITPATCH`

This is not merely a name in the PDB. Static disassembly of the retail DLL found *direct calls* from:

- `fn3DBlt_H5S2` at `0x10066438`
- `Texture3D_H5S2` at `0x100683F4`

The patch routine (starts `0x10066588`) writes a sequence of command-buffer dwords and uses **16-byte alignment** with the literal padding value `0xCC000000`. Among its emitted values are `0xFE000000`, `0x00000001`, `0x0000006C`, and `0x00800000`, repeated across several packets. These are **facts about its binary output**, not yet decoded hardware semantics. The call sites and the patch's purpose deserve comparison against VX900 Part II command-regulator documentation before any reproduction in your Win98 driver.

The existence of VX900-specific patch logic is a warning **against assuming identical command submission/synchronization solely because the main 3D compositor looks similar**.

#### Extra family code visible but not advertised by the package catalog

The linked driver contains names from other VIA generations, including **CX700, CN896 and CN700**. Their presence is evidence of shared historical implementation in this binary, **not evidence that the MSI installer officially supports all of these chipsets**. The package catalog explicitly lists **VX800, VX855 and VX900**.

#### Proposed next static-research steps

1. **Compare `KickOffVX*_PCI` and `KickOffVX*_AGP` instruction-by-instruction** to recover the actual per-chip command packet writers.
2. **Trace the call graph and triggering conditions for `VX900_WAITPATCH`**, then cross-check its emitted words against the VX900 manual. The two present call sites are promising entry points.
3. **Recover `Init3DHWRegs_H6`, `fn3DBlt_H6`, and `Texture3D_H5S2` register-write streams**. Identify corresponding `HParaType` addresses and test only in a controlled hardware probe.
4. Compare the matching/differing 2D routines to distinguish true hardware changes from alternative software implementations or optimizations.
5. Extract the video/HQV behavior differences, using the symbols that share addresses as a baseline and the chip-specific initializers as the delta.
6. Treat Direct3D Mobile support as an open question: `CD3dmProxy` is present, but this package alone does not establish full hardware triangle/shader acceleration for applications.

#### Reproducibility and limits

All addresses are the **retail** DLL's preferred virtual addresses, with base `0x10000000`, as reported in the included retail linker map. Other builds use different addresses. The ZIP of selected artifacts contains the debug and retail DLLs plus their matching map/PDB/REL files and Platform Builder config. The binaries have **not** been executed on a CE system or actual VIA hardware. Hardware capabilities or register effects beyond the inspected functions remain unverified.

#### Appendix: November 6, 2026 — verified CE binary dispatch and wait-packet analysis

The deeper retail-DLL disassembly established **actual PCI-ID dispatch**: `0x1122` VX800 and `0x5122` VX855 both call `Init3DHWRegs_H6` and use the identical H6 texture and 3D-blit routine pointers; `0x7122` VX900 instead selects `Init3DHWRegs_H5S2`, `Texture3D_H5S2`, and `fn3DBlt_H5S2`. Selector: `C3DUtils::Init`, `0x100636BA–0x10063751`.

Both 3D initializers call the same `Init3DHWRegs_INV` (`0x100630F7`) which clears Type-01/02/04 register groups and initializes Type-03 tables before the 3D-generation-specific words. The short initializer then differs on bit 11 of the first word (`0x000E1804` H6 versus `0x000E1004` H5S2), and H6 zeroes extra addresses `0x14–0x19`.

**Stronger explanation of `VX900_WAITPATCH`:** eight copies of a packet containing register field `0x6C` with payload `0x00800000`, followed by an H5S2 closing packet with `0x6C/0x00FC0000`. The VX800/VX855 manuals call `0x6C` the 2D/3D Wait Control register; bits 23–18 respectively select 3D, 2D, and four DMA-channel idle waits. Another CE function builds `0x6C/0x00C00000` or writes it directly to MMIO according to command-interface mode. This strongly suggests serialization/idle-wait workaround, but the VX900 manual marks that address reserved, so effectiveness remains unconfirmed without hardware.

The H6 and H5S2 3D blit bodies contain similar early packet framing but different midstream literals and an H5S2-specific branch on `bGdiMultiABlend` (`0x100CE7EC`). No runtime behavior is claimed. Full disassembly details, word lists, exact function addresses, and cautions have been added to `VIA_CE_Chrome9_Additional_Reverse_Engineering_Findings.md`.


#### Addendum: decoded CE command packets and corrected texture symbol

The main compatibility report section 47 now decodes CE software packet headers using the DLL's executable interpreter at `0x10063766`. FE00 performs MMIO offset/value writes, FE02 selects a register plus streamed data, and FE03 selects a register plus counted data. The `VX900_WAITPATCH` packet therefore **provably writes MMIO+0x6C = 0x00800000 in the CE software path**. The VX900 hardware interpretation of this legacy wait register remains uncertain because its manual marks the address reserved.

Correction: `0x10067255` is `Texture3D_H5` (not a symbol named `Texture3D_H6`); VX800 and VX855 select it alongside `Init3DHWRegs_H6` and `fn3DBlt_H6`. VX900 selects `Texture3D_H5S2`. The H5S2 path writes **documented-as-reserved Type-03 subtype 0x31** table data selected by operation flags, and differs in other texture-state literals. Full words and binary addresses are provided in the main report.

#### CE dynamic-field comparison: render-target and texture formats (2026-11-06)

Disassembly of both CE texture helpers confirms shared render-target and texture-base formulas: `01h:50h = base >> 8`, `01h:52h = pitch >> 5 | format`, `01h:53h = 0x010C0F`, and Type-02 texture-stage-0 base `>> 8`. VX800/VX855 use the H5 helper and request 640 bytes of command-buffer space; VX900 uses H5S2 and requests 1,200 bytes with additional state/wait packets. The complete disassembly-address evidence, format mapper, and caveats appear in the main report §48 and CE findings §48.

##### Texture-state arithmetic and the H5S2 coordinate bias (2026-11-06)

Both CE texture backends pad texture width to 16 texels, build identical Type-02 texture declarations at addresses `18h/20h/21h/22h/30h`, and use the same integer ceiling-log2 exponent loops. The VX900 H5S2 backend additionally adds the floating constant `786432.0` to six intermediate coordinate values (a possible fixed-subpixel representation), absent from the VX800/VX855 H5 backend. It also shares packet format with H5 while selecting between command-buffer submission and direct MMIO packet interpretation. See compatibility report §50–§51 for checked retail VAs, formulas, proof examples, and limitations.


##### Additional H6/H5S2 3D finding (2026-11-06)

At the binary-code level, both CE 3D blit backends load the same pixel-shader ALU/TAU programs using Type-03 subtypes `20h/21h`, and construct the same subtype-`22h` constant register values. A VX900-only subtype-`31h` upload (8 versus 12 DWORDs) correlates with the number of texture-coordinate sets and may represent two- and three-instruction vertex shader programs; the public VX900 detailed manual marks this interface reserved. H5S2's coordinate-bias reference (`786432.0`) appears in *both* 3D blit and texture paths. Full static address-verified details are in the master Chrome9 report §§55–57 and the CE reverse-engineering report.


---

##### 58–61. Further CE-driver comparisons: shared VS program, C-to-S bias, and per-operation pixel shaders (2026-11-06)

- `Texture3D_H5S2` emits the exact same Type-03 subtype `31h` **8-DWORD program** (`0008014C 00000000 00000000 0008021F / 00084124 00000000 00000000 00080283`) used by single-texture `fn3DBlt_H5S2`. The texture function writes it inline at `0x100681D8–0x10068206`; the blit function copies it from `0x1000870C`. The dual-texture/blend blit adds a third 4-DWORD instruction candidate; instruction semantics remain unpublished.
- Both `Texture3D_H5` and `Texture3D_H5S2` **already program the C-to-S X/Y offset as `786432.0`** (`04:41/42` and `04:44/45`). They set scales for X, Y, and Z to `1.0`. The same X/Y words appear in the 32-DWORD table shared by `fn3DBlt_H6` and `fn3DBlt_H5S2` at `0x100087A0`. This refines the prior claim of a VX900-only bias: the VX900 routines additionally apply `786432.0` in software to emitted vertex coordinates, while the hardware transform state has the constant in both generations; whether both stages execute is unknown.
- `04:47` differs between **texture helpers only**: H5 `0x47400000` (Z offset `0x00400000`, subnormal float ≈ `5.87747e-39`) versus H5S2 `0x47000000` (zero). The H6 and H5S2 **blits both use zero**, so do not treat this as a general chipset-specific 3D rule.
- Pixel shaders differ by *operation*: the CE **texture helper** H5 programs `01:92=0x92010003` (1 TAU, **3 ALU** instructions); H5S2 programs `01:92=0x92010001` (1 TAU, **1 ALU** instruction). This exactly matches their emitted Type-03 subtype-`20h` 12/4-DWORD streams. Their one-word TAU instruction `301001B0` is identical. The H5 last ALU instruction differs from H5S2's only instruction in DWORD 2 (`00F20105` vs `00F20005`). By contrast the CE **3D blit** H6 and H5S2 programs are shared as documented in §55.
- Primary manual basis: VX800/VX855/VX900 Part II Type-04 `40h–52h` clipping-to-screen transform; VX855 Part II Type-01 `92h` Pixel Shader Setting 3. VX900 published detailed shader subtype documentation is incomplete/reserved. No hardware behavior has been tested. Full binary anchors, words, and reproduction procedure: master report §§58–61 and `Chrome9_CE_Command_Verification_58_61.py`.

---

<a id="via-matrix"></a>

## Appendix D. VIA 3D register and bitfield tables

*Preserved source section: `Chrome9_VIA_3D_Register_Bitfield_Compatibility_Matrix.md`.*

### Chrome9 VX800 / VX855 / VX900 — VIA 3D Register & Bitfield Compatibility Audit

**Primary sources:** VIA VX800/VX820 Chrome9 HC3, VX855/VX875 Chrome9 HCM, and VX900 Chrome9 HD *Graphics Programming Manuals*, Part II (3D/Video), supplemented by their Part I PCI/MMIO overviews. **No OpenChrome behavior is used to classify a register in this audit.**

**Date:** 2026-11-05  
**Method:** Extract the actual `HParaType`/sub-address descriptions from the six supplied VIA PDFs, align the VX855 and VX900 individual register blocks, inspect important bitfield differences, and use the VX800’s grouped register tables as the third baseline. All page references below are **PDF page numbers** (cover = page 1), not printed page labels.

#### Status definitions and limitations

- **SAME-TEXT** — after normalizing page furniture, the two newer manuals contain effectively identical wording for that register description. This is documentary agreement, not a hardware guarantee.
- **CLOSE-TEXT** — register describes corresponding functionality, but wording/defaults or details differ; inspect the bitfields before reusing values.
- **REVIEW** — source differences may be substantive or merely formatting/errata; not automatically safe.
- **REENCODE / REDEFINED / BIT-CHANGE** — a register layout, meaning, or specific bit differs.
- **RESERVED900 / UNDOCUMENTED900** — the VX900 manual calls it reserved or does not define the VX855 function; this **does not prove the hardware lacks it**.
- **NEW900** — documented for VX900 but not documented at the corresponding address in VX855.
- **VX800 citation** — VX800 places several addresses in a *grouped* table. An attached VX800 page establishes **address coverage**, not independently confirmed word-for-word bitfield identity. Critical fields have been checked manually.

**Completeness statement:** This is a broad address-level inventory plus a manual bitfield audit of the consequential divergences. The automated similarity columns are *not* a formal proof of every text line, reset value, or every possible bit encoding; a hardware driver should never take an unreviewed “close” row as an unconditional compatibility guarantee.

#### 1. Principal conclusions

1. **VX800 → VX855:** the HC3 and HCM manuals document the same legacy FVF layout, primitive-mode setup, shader register groups, and most command-regulator organization. They are the closest pair for the existing VX855 Windows 98 pipeline, subject to PCI/memory/display differences handled separately.
2. **VX855 → VX900:** most `01h` Z/stencil/alpha/render-target/fog blocks and the `02h` texture-stage/sampler blocks retain recognizably matching register layouts. That creates opportunities for a shared fixed-function state writer **with per-chip masked differences**.
3. **VX900 vertex input is a genuine re-encoding:** `04h:00h–10h` no longer describe the VX855 FVF attribute-order list; the new descriptors specify vertex streams, input registers, offsets, and types. VX900 additionally documents `03h:30h` for 16 stream base/pitch entries.
4. **VX900 legacy shader register documentation is curtailed:** `01h:01h` bit10 is reserved, `01h:90h–9Ah` are reserved, and `03h:20h–22h` are reserved; references to VS/PS exist elsewhere in the same manual. Treat this as an **unresolved interface/documentation change**, not a proven shader-feature removal.
5. **3D command regulator is not bitwise universal:** `10h:04h`, `11h:00h`, branch controls and per-engine fences change. Sharing queue startup as a *concept* is different from sharing every 24-bit control word.
6. **Depth/stencil controls are comparatively stable:** `01h:10h–20h`, `23h–2Ah` retain detailed descriptions for Z, stencil, Fast-Z-Clear and coarse-Z. This provides a consistent test baseline, not proof of identical cache behavior.
7. **Manual errata exist:** VX855 `04h:25h` is titled “Vertexes Number” but enumerates FVF attribute-mask bits; VX900 corrects the title. VX855 texture-location labels at `02h:18h` appear inconsistent.

#### 2. Coverage of 3D command families

| HParaType | Hardware meaning | VX800 HC3 | VX855 HCM | VX900 HD | Recommendation |
|---|---|---|---|---|---|
| `00h` | Primitive vertex/index payload | Vertex/index path documented | Same model | Same model described, different vertex state | Keep transport abstraction; encode vertex payload by device |
| `01h` | Non-texture 3D state | Full grouped tables (`00h–AAh`) | Detailed blocks, including PS `90h–9Ah` | Extensive matching blocks; multiple reserved/changed fields | Share tested Z/stencil/alpha paths, mask per GPU |
| `02h` (`00h–0Fh`) | Texture-stage addressing and control | Documented | Documented | Documented | Core texture state strongly reusable |
| `02h` (`20h–2Fh`) | Texture sample/filter state | Documented | Documented | Documented | Reuse with format/border checks |
| `02h` (`FEh`) | Global texture routing/control | Documented | Documented | Documented | Mostly shared, some VS resource descriptions differ |
| `03h` | Palette/tables and special payload | PS instruction subtypes `20h–22h` | Same PS instruction subtypes | Those PS subtypes reserved; adds `30h` stream base/pitch | Do not share shader uploader |
| `04h` | Vertex declaration and primitive state | Legacy FVF | Legacy FVF | Stream/input-register-based FVF + similar primitive fields | Split vertex declaration writers |
| `08h` | CDC/PPC | Not in supplied VX800 type list | Not in supplied VX855 type list | Explicit PPC/CDC controls | VX900-specific path |
| `10h` | Command regulator controls | Legacy grouping | Similar legacy grouping, extra second queue | New preemption/dispatch interface | Per-generation CR initialization |
| `11h` | Fence, swap and branch controls | Legacy grouping | Similar legacy grouping | HW locks and fence wait mask; branch differences | Per-generation synchronization backend |

#### 3. High-impact changed bits and fields — manually inspected

| Address / bits | VX855 definition | VX900 definition | VX855 PDF page | VX900 PDF page | Driver action |
|---|---|---|---|---|---|
| `01h:01h[20]` | Enable clipping-coordinate → screen-coordinate transformation | Reserved | p.85 | p.97 | VX900: do not replay this VX855 bit. |
| `01h:01h[10]` | Enable pixel shader | Reserved | p.85 | p.97 | Does not alone establish shader hardware capabilities; older interface not documented. |
| `01h:13h[23]` | Use pixel-shader-generated source Z | Reserved | p.88 | p.100 | Shader depth write is not portable as-is. |
| `01h:21h–22h` | Occlusion counter writeback address/location | Not described | p.90 | p.102 | Do not assume VX855 writeback semantics on VX900. |
| `01h:59h/61h/69h[21:20]` | RT1/RT2/RT3 memory location | Reserved | pp.105,107,109 | pp.117,118,120 | Preserve RT0 path separately. |
| `01h:8Ah[23,17]` | Bit23 TX precision; bit17 reserved | Bit23 absent/reserved; bit17 ZTAG ECO | p.114 | p.126 | Unrelated functions occupy the high-bit region. |
| `01h:90h–9Ah` | Detailed pixel-shader settings | Explicitly reserved | pp.115–121 | p.126 | Do not reuse shader state programming. |
| `02h:(00h–0Fh):18h[17:14]` | Face0 label repeated | Face5 label | p.123 | p.129 | Likely text ambiguity in VX855; needs independent verification. |
| `02h:FEh:08h[3:0]` | Textures in vertex buffer | Texture coordinate registers in VS output | p.138 | p.144 | Likely resource accounting distinction. |
| `04h:00h[11:8,6:0]` | Legacy FVF length and mask | Input streams and aggregate data length | p.154 | p.160 | Distinct vertex-data descriptor encoding. |
| `04h:01h–10h` | Packed list of attribute indices | Per-attribute stream, offset, IR, data format | pp.155–158 | pp.160–164 | Must use different encoders. |
| `04h:20h[23]` | Disable point-primitive near clipping | Reserved | p.159 | p.166 | VX900 moves feature to 23h. |
| `04h:21h–22h` | Vertex-buffer address/pitch | Reserved | p.160 | p.166 | VX900 documents stream bases under `03h:30h`. |
| `04h:23h[23:19]` | Reserved high bits | Clipping and idle ECO controls | p.161 | p.167 | Legacy state still largely shared at lower bits. |
| `04h:25h[23:0]` | Incorrect title “Vertexes Number” | Correctly titled FVF mask | p.162 | p.168 | VX855 and VX900 sub-bit descriptions match. Documentation erratum, not proven HW difference. |
| `10h:04h` | Interrupt state buffer controls | CR dispatch controls | p.68 | p.80 | Same address, different purpose. |
| `10h:05h–07h` | Interrupt state/threshold commands | Reserved | p.69 | p.80 | Do not share raw writes. |
| `10h:08h–0Eh` | Not described in VX855 | Preemption and CRMI controls | — | pp.81–82 | VX900-specific functionality. |
| `10h:68h[5]` | Reserved | 2D Header1 format | p.71 | p.84 | New VX900 format selection. |
| `10h:69h,6Ch–6Fh` | Branch / restore controls | Not described at same addresses | pp.71–72 | pp.84–85 | Different fetch/branch management. |
| `10h:70h–74h` | Queue start/end/length/control | Analogous core queue interface | pp.72–73 | pp.85–86 | Matching high-level function; validate individual location codes and control bits. |
| `10h:78h–7Ch` | Separate command queue configuration | Absent | pp.73–74 | p.86 | Not a safe VX900 initialization sequence. |
| `11h:00h` | FIFO depth/critical commands plus locking | Hardware inter-engine lock bit17, other bits reserved | p.75 | p.87 | Do not carry across the legacy low-bit fields. |
| `11h:03h` | Not described | Fence wait mask by engine | — | p.88 | VX900-only synchronization. |
| `11h:68h–6Fh` | First/second branch controls | Traditional branch only 68h–6Bh | pp.79–80 | p.93 | Branch logic requires per-generation implementation. |

##### VX800 cross-check for key high-risk fields

- **`01h:01h[20]` and `[10]`:** VX800 Part II p.70 explicitly documents the same clipping-coordinate transform enable and pixel shader enable as VX855. VX900 reserves them. Thus this is a **VX900 departure**, not a difference already present on VX800.
- **`04h:20h[23]` / `04h:21h–22h`:** VX800 Part II pp.139–140 uses the same legacy near-plane point-clip bit and vertex-buffer base/pitch locations as VX855; VX900 moves or reserves these locations.
- **`04h:24h–26h`:** VX800 pp.140–141 enumerates vertex count and FVF masks in the same legacy address region; VX855 `04h:25h` labeling error is not evidence of different hardware.
- **`03h:20h–22h`:** VX800 p.134 and VX855 p.153 document pixel-shader ALU/TAU instruction upload and constants. VX900 p.159 reserves those subtypes.
- **`10h:70h–7Ch`:** VX800 pp.150–151, VX855 pp.72–74 describe queue parameters in 128-bit units; VX900 pp.85–86 shows `70h–74h` but does not describe the separate `78h–7Ch` queue.
- **`11h:00h`:** VX800 p.152 and VX855 p.75 document FIFO depths plus critical-command handling; VX900 p.87 reserves those low bits and adds bit17 hardware 2D/3D lock.

#### 4. Detailed subsystem analysis

##### 4.1 `00h`: primitive payload versus vertex declaration
VX855 Part II p.83 and VX900 Part II p.95 describe identical high-level transport: type `00h` transmits primitive vertices or vertex indices, then the command regulator assembles and fires primitives. However, the meaning of that payload depends on the `04h` FVF configuration, where VX900 differs. Therefore **shared top-level packet type does not imply byte-for-byte equivalent vertex records.** Check VX800 Part II pp.68–69 for the predecessor description.

##### 4.2 `01h`: Z, stencil, alpha, render targets, fog
**Good sharing candidates:** Z-buffer pitch/cache and Z-test compare controls (`10h–20h`), stencil reference/compare/write controls (`23h–26h`), Fast-Z-Clear (`27h–28h`), coarse-Z (`29h–2Ah`), alpha blending (`33h–3Fh`), fog (`70h–78h`) and primary render target (`50h–53h`). These families have matching register names and usually matching field descriptions in the VX855 and VX900 manuals. The appendix still marks smaller wording differences for review.

**Do not copy unmodified:** the old shader control bits (`01h`, `13h`, `90h–9Ah`), VX855 occlusion-count address at `21h–22h`, secondary render-target memory-location selector bits at `59h/61h/69h`, and `8Ah` high-bit ECO/precision fields. The VX900 manual sometimes reserves a selector even though it otherwise retains the corresponding render target.

##### 4.3 `02h`: per-stage texture and sampler state
The address families are remarkably stable: face-0 level-0 addressing, texture control (`18h/20h–22h/30h`), texture filters (`31h–32h`), borders (`35h–36h`), and global source routing (`FEh:00h–13h`). Much of the VX855/VX900 documentation is identical or nearly identical. Distinguish **the programmed register** from **its interpretation of memory-location encodings**; some VX900 fields additionally label memory-location value `11` as Local Memory Local Frame Buffer (L.L.) while VX855 marks it reserved.

The VX855 `02h:(00h–0Fh):18h` labels for location bits `[17:16]` and `[15:14]` read “Face0”, while VX900 calls them “Face5”. That may be a manual typo but the safe rule is to validate six-face cube texture setup before relying on either label. The global `FEh:08h` bits `[3:0]` also have changed resource-accounting wording relating to the VS output register; the rest of the route control appears analogous.

##### 4.4 `03h`: texture tables, shader uploads and streams
| Subtype | VX800 | VX855 | VX900 | Compatibility |
|---|---|---|---|---|
| `00h` | Texture/video palette | Texture/video palette | Texture/video palette | Comparable; check palette formats |
| `01h–0Fh` | Reserved | Reserved | Reserved | Reserved |
| `10h` | Mip level offset/base table | Mip level offset/base table | Mip level offset/base table | Similar; VX800 text mentions levels 1–11, newer manuals levels 1–8 |
| `11h` | 4×4 filter coefficients | 4×4 filter coefficients | 4×4 filter coefficients | Comparable |
| `14h` | Stipple palette | Stipple palette | Stipple palette | Comparable |
| `15h` | Texture de-gamma table | Texture de-gamma table | Texture de-gamma table | Comparable |
| `17h` | Write-color gamma table | Write-color gamma table | Write-color gamma table | Comparable |
| `20h` | Pixel shader ALU code | Pixel shader ALU code | Reserved | **Incompatible documentation** |
| `21h` | Pixel shader TAU code | Pixel shader TAU code | Reserved | **Incompatible documentation** |
| `22h` | Pixel shader constants | Pixel shader constants | Reserved | **Incompatible documentation** |
| `30h` | Not documented | Not documented | 16 vertex-stream base/pitch entries | **New VX900 interface** |
| `31h–33h` | Not documented | Not documented | Reserved in detailed table, even though CR overview mentions VS payload types | **Documentation conflict** |

**Additional VX900 manual typo:** on the photographed PDF p.166, the reserved header after `04h:20h` is printed **`21h-22F`** (not `21h-22h`). Its surrounding addresses imply the older `21h–22h` base/pitch register group is not defined there, but the range is printed ambiguously; do not silently correct or extrapolate it to a broader memory region.

**Documentation conflict:** the VX900 command-regulator overview names shader-instruction formats that its detailed `03h` register table reserves. This is unresolved from these manuals alone. A working proprietary driver trace or additional silicon documentation would be needed to resolve it; do not infer hardware availability from headings alone.

##### 4.5 `04h`: vertex declarations and primitives
**Entire `04h:00h–10h` block requires a distinct VX900 encoder.** VX800/VX855 use packed legacy FVF attributes (up to 51 indexed attributes in a sequence) and an FVF vertex length. VX900 uses stream IDs, per-attribute offsets, input registers, data types, and a resource/word-count field; `03h:30h` supplies stream base/pitch. This is the main structural split for a unified driver.

**Remaining primitive controls are largely recognizable**, such as `04h:23h` shading, primitive types and culling, `04h:24h` count, `04h:25h–26h` attribute mask, and `04h:2Fh` default values. However, moved clipping control bits and new ECO flags in `23h` mean even this region needs a per-chip mask. `04h:40h–52h` clipping-to-screen conversion constants have broadly corresponding layouts; the VX900 manual includes explicit notes that scaling/offset must also be written to VS constants c256/c257, which cannot be treated as a validated recipe for the VX855.

##### 4.6 `10h`/`11h`: CR/queue/fence/branch
Queue base/end/length and queue-control registers at `10h:70h–74h` remain a common *interface pattern*. All three documents give a **minimum of `0x800` 128-bit units**, i.e. **32 KiB**, for the queue length. This is distinct from the capacity of any specific buffer allocated by software. Direct hardware tests would be needed to settle discrepancies with OpenChrome code; this audit makes no claim about runtime queue length interpretation.

VX900 adds dispatch/preemption/CRMI controls under `10h`, while VX800/VX855 contain legacy interrupt, branch, restore, and second-queue controls. VX900 `11h:00h` changes low bits and introduces hardware 2D/3D locking; `11h:03h` adds wait-by-engine fence flags. Preserve the **API-level intent** of these operations but implement their MMIO command encodings by chipset.

#### 5. Suggested driver design

```c
typedef enum { CHROME9_HC3_VX800, CHROME9_HCM_VX855, CHROME9_HD_VX900 } chrome9_variant;
typedef struct chrome9_ops {
    void (*program_legacy_or_stream_vertex_format)(void *state);
    void (*program_common_3d_state)(void *state);
    void (*program_shader_state)(void *state);
    void (*configure_queue)(void *state);
    void (*fence_2d_3d)(void *state);
} chrome9_ops;
```

**Practical rule:** share code only after comparing (1) register address, (2) bits and default/reserved meanings, (3) memory-location encodings, (4) write ordering, (5) hardware behavior. The same register *title* is not enough. For VX900, all legacy pixel-shader state writes should remain disabled unless a separate validated route is found.

#### 6. Proposed validation sequence

1. Start with **no shader uploads**, direct pretransformed flat triangle, single render target, color-write-only. Capture the actual `01h/02h/04h` words.
2. Add alpha/blending, then texture stage, then sampler filtering/wrap, comparing exact state words where manuals agree.
3. Add linear Z24/Z16 without coarse-Z or fast-Z; then add depth-clear and cache controls separately.
4. Add stencil compare/write with a small 8×8/16×16 boundary matrix and inspect tile behavior.
5. Only after simple 3D works, test the generation-specific vertex declaration and indexed paths. VX900 `03h:30h` must not be initialized with VX855 `04h:21h–22h` values.
6. Queue/fence tests must be separate from rendering tests; verify no commands are submitted when status shows stale/busy or a queue fault.
7. Never enable a bit explicitly labeled **reserved** in the target manual solely because its bit position exists on another device.

---

#### 7. Complete address-level appendix — VX855 compared with VX900, VX800 group coverage

Rows below enumerate the **register blocks explicitly headed** in the newer manuals. A range header may cover more individual words than the single row shown, so this appendix is a searchable *address index*, not a claim that every range word appears as its own heading. VX800 references point to its grouped register tables. Similarity thresholds are mechanical; curated differences above override these labels.

##### HParaType `01h`

| Subtype | Address | VX800 manual | VX855 manual | VX900 manual | Document-level classification |
|---|---:|---|---|---|---|
| `—` | `00h`–`0Fh` | p.69 | p.84 — Enable Setting 1 | p.96 — Enable Setting 1 | **SAME-TEXT** |
| `—` | `01h` | p.69 | p.85 — Enable Setting 2 | p.97 — Enable Setting 2 | **BIT-CHANGE** |
| `—` | `02h` | p.69 | p.86 — Enable Setting 3 | p.98 — Enable Setting 3 | **SAME-TEXT** |
| `—` | `03h`–`0Fh` | p.69 | p.86 — : Reserved (for Enable Setting) | p.98 — : Reserved (for Enable Setting) | **SAME-TEXT** |
| `—` | `10h` | p.71 | p.86 — Z Setting 1 | p.98 — Z Setting 1 | **SAME-TEXT** |
| `—` | `11h` | p.71 | p.86 — Z Setting 2 | p.98 — Z Setting 2 | **REVIEW-TEXT** |
| `—` | `12h` | p.71 | p.87 — Z Setting 3 | p.99 — Z Setting 3 | **CLOSE-TEXT** |
| `—` | `13h` | p.71 | p.88 — Z Setting 4 | p.100 — Z Setting 4 | **BIT-CHANGE** |
| `—` | `14h` | p.71 | p.88 — Z Setting 5 | p.100 — Z Setting 5 | **SAME-TEXT** |
| `—` | `15h` | p.71 | p.88 — Z Setting 6 | p.100 — Z Setting 6 | **SAME-TEXT** |
| `—` | `16h` | p.71 | p.88 — Z Setting 7 | p.100 — Z Setting 7 | **CLOSE-TEXT** |
| `—` | `17h` | p.71 | p.89 — Z Setting 8 | p.101 — Z Setting 8 | **SAME-TEXT** |
| `—` | `18h` | p.71 | p.89 — Z Setting 9 | p.101 — Z Setting 9 | **SAME-TEXT** |
| `—` | `19h` | p.71 | p.89 — Z Setting 10 | p.101 — Z Setting 10 | **SAME-TEXT** |
| `—` | `1Ah` | p.71 | p.89 — Z Setting 11 | p.101 — Z Setting 11 | **SAME-TEXT** |
| `—` | `1Bh` | p.71 | p.89 — Z Setting 12 | p.101 — Z Setting 12 | **SAME-TEXT** |
| `—` | `1Ch` | p.71 | p.89 — Z Setting 13 | p.101 — Z Setting 13 | **SAME-TEXT** |
| `—` | `1Dh` | p.71 | p.89 — Z Setting 14 | p.101 — Z Setting 14 | **CLOSE-TEXT** |
| `—` | `1Eh` | p.71 | p.90 — Z Setting 15 | p.102 — Z Setting 15 | **SAME-TEXT** |
| `—` | `1Fh` | p.71 | p.90 — Z Setting 16 | p.102 — Z Setting 16 | **SAME-TEXT** |
| `—` | `20h` | p.71 | p.90 — Z Setting 17 | p.102 — Z Setting 17 | **SAME-TEXT** |
| `—` | `21h` | p.71 | p.90 — Z Setting 18 | — | **UNDOCUMENTED900** |
| `—` | `22h` | p.71 | p.90 — Z Setting 19 | — | **UNDOCUMENTED900** |
| `—` | `23h` | p.73 | p.91 — Stencil Setting 1 | p.102 — Stencil Setting 1 | **REVIEW-TEXT** |
| `—` | `24h` | p.73 | p.91 — Stencil Setting 2 | p.103 — Stencil Setting 2 | **CLOSE-TEXT** |
| `—` | `25h` | p.73 | p.92 — Stencil Setting 3 | p.103 — Stencil Setting 3 | **CLOSE-TEXT** |
| `—` | `26h` | p.73 | p.92 — Stencil Setting 4 | p.104 — Stencil Setting 4 | **CLOSE-TEXT** |
| `—` | `27h` | pp.74–75 | p.93 — Setting for Fast-Z-Clear 1 | p.104 — Setting for Fast-Z-Clear 1 | **CLOSE-TEXT** |
| `—` | `28h` | pp.74–75 | p.93 — Setting for Fast-Z-Clear 2 | p.105 — Setting for Fast-Z-Clear 2 | **SAME-TEXT** |
| `—` | `29h` | p.76 | p.93 — Setting for Coarse Z Test Function 1 | p.105 — Setting for Coarse Z Test Function 1 | **CLOSE-TEXT** |
| `—` | `2Ah` | p.76 | p.94 — Setting for Coarse Z Test Function 2 | p.106 — Setting for Coarse Z Test Function 2 | **SAME-TEXT** |
| `—` | `33h` | p.77 | p.94 — Alpha Setting 1 | p.106 — Alpha Setting 1 | **CLOSE-TEXT** |
| `—` | `34h` | p.77 | p.95 — Alpha Setting 2 | p.107 — Alpha Setting 2 | **CLOSE-TEXT** |
| `—` | `35h` | p.77 | p.96 — Alpha Setting 3 | p.108 — Alpha Setting 3 | **CLOSE-TEXT** |
| `—` | `36h` | p.77 | p.97 — Alpha Setting 4 | p.109 — Alpha Setting 4 | **CLOSE-TEXT** |
| `—` | `37h` | p.77 | p.98 — Alpha Setting 5 | p.110 — Alpha Setting 5 | **CLOSE-TEXT** |
| `—` | `38h` | p.77 | p.98 — Alpha Setting 6 | p.110 — Alpha Setting 6 | **SAME-TEXT** |
| `—` | `39h` | p.77 | p.98 — Alpha Setting 7 | p.110 — Alpha Setting 7 | **CLOSE-TEXT** |
| `—` | `3Ah` | p.77 | p.99 — Alpha Setting 8 | p.111 — Alpha Setting 8 | **SAME-TEXT** |
| `—` | `3Bh` | p.77 | p.99 — Alpha Setting 9 | p.111 — Alpha Setting 9 | **SAME-TEXT** |
| `—` | `3Ch` | p.77 | p.99 — Alpha Setting 10 | p.111 — Alpha Setting 10 | **SAME-TEXT** |
| `—` | `3Dh` | p.77 | p.99 — Alpha Setting 11 | p.111 — Alpha Setting 11 | **CLOSE-TEXT** |
| `—` | `3Eh` | p.77 | p.99 — Alpha Setting 12 | p.112 — Alpha Setting 12 | **CLOSE-TEXT** |
| `—` | `3Fh` | p.77 | p.100 — Alpha Setting 13 | p.112 — Alpha Setting 13 | **SAME-TEXT** |
| `—` | `40h`–`4Fh` | p.77 | p.100 — : Reserved (for Alpha Setting) | p.112 — : Reserved (for Alpha Setting) | **SAME-TEXT** |
| `—` | `50h` | p.81 | p.100 — Destination Setting – Render Target 0 – | p.112 — Destination Setting – Render Target 0 – | **SAME-TEXT** |
| `—` | `51h` | p.81 | p.100 — Destination Setting – Render Target 0 – | p.112 — Destination Setting – Render Target 0 – | **REVIEW-TEXT** |
| `—` | `52h` | p.81 | p.101 — Destination Setting – Render Target 0 – | p.113 — Destination Setting – Render Target 0 – | **SAME-TEXT** |
| `—` | `53h` | p.81 | p.104 — Destination Setting – Render Target 0 – | p.116 — Destination Setting – Render Target 0 – | **SAME-TEXT** |
| `—` | `54h` | p.81 | p.104 — – 57h: Reserved (for Destination Settin | p.116 — – 57h: Reserved (for Destination Settin | **REVIEW-TEXT** |
| `—` | `58h` | p.85 | p.105 — Destination Setting – Render Target 1 – | p.117 — Destination Setting – Render Target 1 – | **SAME-TEXT** |
| `—` | `59h` | p.85 | p.105 — Destination Setting – Render Target 1 – | p.117 — Destination Setting – Render Target 1 – | **BIT-CHANGE** |
| `—` | `5Ah` | p.85 | p.105 — Destination Setting – Render Target 1 – | p.117 — Destination Setting – Render Target 1 – | **CLOSE-TEXT** |
| `—` | `5Bh` | p.85 | p.106 — Destination Setting – Render Target 1 – | p.117 — Destination Setting – Render Target 1 – | **SAME-TEXT** |
| `—` | `5Ch`–`5Fh` | p.85 | p.106 — : Reserved (for Destination Setting – R | p.118 — : Reserved (for Destination Setting – R | **REVIEW-TEXT** |
| `—` | `60h` | p.89 | p.107 — Destination Setting – Render Target 2 – | p.118 — Destination Setting – Render Target 2 – | **SAME-TEXT** |
| `—` | `61h` | p.89 | p.107 — Destination Setting – Render Target 2 – | p.118 — Destination Setting – Render Target 2 – | **BIT-CHANGE** |
| `—` | `62h` | p.89 | p.107 — Destination Setting – Render Target 2 – | p.119 — Destination Setting – Render Target 2 – | **CLOSE-TEXT** |
| `—` | `63h` | p.89 | p.108 — Destination Setting – Render Target 2 – | p.119 — Destination Setting – Render Target 2 – | **SAME-TEXT** |
| `—` | `64h`–`67h` | p.89 | p.108 — : Reserved (for Destination Setting – R | p.120 — : Reserved (for Destination Setting – R | **REVIEW-TEXT** |
| `—` | `68h` | p.93 | p.109 — Destination Setting – Render Target 3 – | p.120 — Destination Setting – Render Target 3 – | **SAME-TEXT** |
| `—` | `69h` | p.93 | p.109 — Destination Setting – Render Target 3 – | p.120 — Destination Setting – Render Target 3 – | **BIT-CHANGE** |
| `—` | `6Ah` | p.93 | p.109 — Destination Setting – Render Target 3 – | p.120 — Destination Setting – Render Target 3 – | **CLOSE-TEXT** |
| `—` | `6Bh` | p.93 | p.110 — Destination Setting – Render Target 3 – | p.121 — Destination Setting – Render Target 3 – | **SAME-TEXT** |
| `—` | `6Ch`–`6Fh` | p.93 | p.110 — : Reserved (for Destination Setting – R | p.121 — : Reserved (for Destination Setting – R | **REVIEW-TEXT** |
| `—` | `70h` | p.97 | p.111 — Fog Setting 1 | p.122 — Fog Setting 1 | **SAME-TEXT** |
| `—` | `71h` | p.97 | p.111 — Fog Setting 2 | p.122 — Fog Setting 2 | **SAME-TEXT** |
| `—` | `72h` | p.97 | p.111 — Fog Setting 3 | p.122 — Fog Setting 3 | **REVIEW-TEXT** |
| `—` | `73h` | p.97 | p.112 — Fog Setting 4 | p.122 — Fog Setting 4 | **REVIEW-TEXT** |
| `—` | `74h` | p.97 | p.112 — Fog Setting 5 | p.123 — Fog Setting 5 | **SAME-TEXT** |
| `—` | `75h` | p.97 | p.112 — Fog Setting 6 | p.123 — Fog Setting 6 | **SAME-TEXT** |
| `—` | `76h` | p.97 | p.112 — Fog Setting 7 | p.123 — Fog Setting 7 | **SAME-TEXT** |
| `—` | `77h` | p.97 | p.112 — Fog Setting 8 | p.123 — Fog Setting 8 | **SAME-TEXT** |
| `—` | `78h` | p.97 | p.112 — Fog Setting 9 | p.123 — Fog Setting 9 | **SAME-TEXT** |
| `—` | `79h`–`7Fh` | p.97 | p.112 — : Reserved (for Fog Setting) | p.123 — : Reserved (for Fog Setting) | **DIFFERENT-TEXT** |
| `—` | `80h` | p.98 | p.113 — Miscellaneous Setting 1 | p.123 — Miscellaneous Setting 1 | **REVIEW-TEXT** |
| `—` | `81h` | p.98 | p.113 — Miscellaneous Setting 2 | p.124 — Miscellaneous Setting 2 | **SAME-TEXT** |
| `—` | `82h` | p.98 | p.113 — Miscellaneous Setting 3 | p.124 — Miscellaneous Setting 3 | **SAME-TEXT** |
| `—` | `83h` | p.98 | p.113 — Miscellaneous Setting 4 | p.124 — Miscellaneous Setting 4 | **SAME-TEXT** |
| `—` | `84h` | p.98 | p.113 — Miscellaneous Setting 5 | p.124 — Miscellaneous Setting 5 | **SAME-TEXT** |
| `—` | `85h` | p.98 | p.113 — Miscellaneous Setting 6 | p.124 — Miscellaneous Setting 6 | **CLOSE-TEXT** |
| `—` | `86h` | p.98 | p.114 — Miscellaneous Setting 7 - Lower 3 Bytes | p.125 — Miscellaneous Setting 7 - Lower 3 Bytes | **SAME-TEXT** |
| `—` | `87h` | p.98 | p.114 — Miscellaneous Setting 8 - Highest Byte  | p.125 — Miscellaneous Setting 8 - Highest Byte  | **CLOSE-TEXT** |
| `—` | `88h` | p.98 | p.114 — Miscellaneous Setting 9 | p.125 — Miscellaneous Setting 9 | **SAME-TEXT** |
| `—` | `89h` | p.98 | p.114 — Miscellaneous Setting 10 | p.125 — Miscellaneous Setting 10 | **REVIEW-TEXT** |
| `—` | `8Ah` | p.98 | p.114 — Miscellaneous Setting 11 | p.126 — Miscellaneous Setting 11 | **BIT-CHANGE** |
| `—` | `8Bh`–`8Fh` | p.98 | p.115 — : Reserved (for Miscellaneous Setting) | p.126 — : Reserved (for Miscellaneous Setting) | **SAME-TEXT** |
| `—` | `90h`–`9Ah` | p.99 | p.115 — Pixel Shader Setting 1 | p.126 — : Reserved | **RESERVED900** |
| `—` | `91h` | p.99 | p.116 — Pixel Shader Setting 2 | — | **NOT-HEADED-900** |
| `—` | `92h` | p.99 | p.117 — Pixel Shader Setting 3 | — | **NOT-HEADED-900** |
| `—` | `93h` | p.99 | p.117 — Pixel Shader Setting 4 | — | **NOT-HEADED-900** |
| `—` | `94h` | p.99 | p.118 — Pixel Shader Setting 5 | — | **NOT-HEADED-900** |
| `—` | `95h` | p.99 | p.118 — Pixel Shader Setting 6 | — | **NOT-HEADED-900** |
| `—` | `96h` | p.99 | p.119 — Pixel Shader Setting 7 | — | **NOT-HEADED-900** |
| `—` | `97h` | p.99 | p.119 — Pixel Shader Setting 8 | — | **NOT-HEADED-900** |
| `—` | `98h` | p.99 | p.120 — Pixel Shader Setting 9 | — | **NOT-HEADED-900** |
| `—` | `99h` | p.99 | p.120 — Pixel Shader Setting 10 | — | **NOT-HEADED-900** |
| `—` | `9Ah` | p.99 | p.121 — Pixel Shader Setting 11 | — | **NOT-HEADED-900** |
| `—` | `AAh` | p.104 | p.121 — Software Inspection | p.126 — Software Inspection | **SAME-TEXT** |

##### HParaType `02h`

| Subtype | Address | VX800 manual | VX855 manual | VX900 manual | Document-level classification |
|---|---:|---|---|---|---|
| `00h-0Fh` | `00h` | p.105 | p.122 — Face 0 Level 0 Base Address | p.127 — Face 0 Level 0 Base Address | **SAME-TEXT** |
| `00h-0Fh` | `01h` | p.105 | p.122 — Face 1 Level 0 Base Address | p.127 — Face 1 Level 0 Base Address | **SAME-TEXT** |
| `00h-0Fh` | `02h` | p.105 | p.122 — Face 2 Level 0 Base Address | p.127 — Face 2 Level 0 Base Address | **REVIEW-TEXT** |
| `00h-0Fh` | `03h` | p.105 | p.122 — Face 3 Level 0 Base Address | p.128 — Face 3 Level 0 Base Address | **SAME-TEXT** |
| `00h-0Fh` | `04h` | p.105 | p.122 — Face 4 Level 0 Base Address | p.128 — Face 4 Level 0 Base Address | **REVIEW-TEXT** |
| `00h-0Fh` | `05h` | p.105 | p.123 — Face 5 Level 0 Base Address | p.128 — Face 5 Level 0 Base Address | **DIFFERENT-TEXT** |
| `00h-0Fh` | `06h`–`17h` | p.105 | p.123 — : Reserved | p.128 — : Reserved | **DIFFERENT-TEXT** |
| `00h-0Fh` | `18h` | p.105 | p.123 — Texture Control 1 | p.129 — Texture Control 1 | **DOC-DIFF** |
| `00h-0Fh` | `19h`–`1Fh` | p.105 | p.124 — : Reserved (Texture Control) | p.130 — : Reserved (Texture Control) | **SAME-TEXT** |
| `00h-0Fh` | `20h` | p.105 | p.124 — Texture Control 2 | p.130 — Texture Control 2 | **SAME-TEXT** |
| `00h-0Fh` | `21h` | p.105 | p.124 — Texture Control 3 | p.130 — Texture Control 3 | **SAME-TEXT** |
| `00h-0Fh` | `22h` | p.105 | p.124 — Texture Control 4 | p.130 — Texture Control 4 | **SAME-TEXT** |
| `00h-0Fh` | `23h`–`2Fh` | p.105 | p.124 — : Reserved (Texture Control) | p.130 — : Reserved (Texture Control) | **REVIEW-TEXT** |
| `00h-0Fh` | `30h` | p.105 | p.125 — Texture Control 5 | p.131 — Texture Control 5 | **CLOSE-TEXT** |
| `00h-0Fh` | `50h` | — | p.128 — Software Inspection for Texture N – 1st | p.134 — Software Inspection for Texture N – 1st | **REVIEW-TEXT** |
| `00h-0Fh` | `51h` | — | p.129 — Software Inspection for Texture N – 2nd | p.134 — Software Inspection for Texture N – 2nd | **SAME-TEXT** |
| `20h-2Fh` | `00h`–`2Fh` | p.111 | p.130 — : Reserved | p.135 — : Reserved | **SAME-TEXT** |
| `20h-2Fh` | `30h` | p.111 | p.130 — Texture Level Control 1 | p.135 — Texture Level Control 1 | **CLOSE-TEXT** |
| `20h-2Fh` | `31h` | p.111 | p.131 — Texture Filter Control 1 | p.136 — Texture Filter Control 1 | **SAME-TEXT** |
| `20h-2Fh` | `32h` | p.111 | p.132 — Texture Filter Control 2 & Texture Mapp | p.137 — Texture Filter Control 2 & Texture Mapp | **SAME-TEXT** |
| `20h-2Fh` | `33h`–`34h` | p.111 | p.132 — : Reserved | p.137 — : Reserved | **SAME-TEXT** |
| `20h-2Fh` | `35h` | p.111 | p.132 — Texture Border Control 1 | p.137 — Texture Border Control 1 | **CLOSE-TEXT** |
| `20h-2Fh` | `36h` | p.111 | p.133 — Texture Border Control 2 | p.138 — Texture Border Control 2 | **REVIEW-TEXT** |
| `20h-2Fh` | `37h`–`40h` | p.111 | p.133 — : Reserved | p.138 — : Reserved | **SAME-TEXT** |
| `20h-2Fh` | `50h` | p.113 | p.133 — Software Inspection for Texture Sample  | p.138 — Software Inspection for Texture Sample  | **SAME-TEXT** |
| `20h-2Fh` | `51h` | p.113 | p.133 — Software Inspection for Texture Sample  | p.138 — Software Inspection for Texture Sample  | **REVIEW-TEXT** |
| `FEh` | `00h` | p.115 | p.134 — General Texture Attribute Control | p.139 — General Texture Attribute Control | **CLOSE-TEXT** |
| `FEh` | `01h` | p.115 | p.134 — Attribute of Texture 0/1/2 | p.140 — Attribute of Texture 0/1/2 | **CLOSE-TEXT** |
| `FEh` | `02h` | p.115 | p.135 — Attribute of Texture 3/4/5 | p.141 — Attribute of Texture 3/4/5 | **DIFFERENT-TEXT** |
| `FEh` | `03h` | p.115 | p.136 — Attribute of Texture 6/7 | p.142 — Attribute of Texture 6/7 | **DIFFERENT-TEXT** |
| `FEh` | `04h` | p.115 | p.137 — Texture Coordinate Control | p.142 — Texture Coordinate Control | **CLOSE-TEXT** |
| `FEh` | `05h` | p.115 | p.137 — User Defined Clipping Planes Control -  | p.143 — User Defined Clipping Planes Control -  | **DIFFERENT-TEXT** |
| `FEh` | `06h`–`07h` | p.115 | p.138 — : Reserved | p.143 — : Reserved | **DIFFERENT-TEXT** |
| `FEh` | `08h` | p.119 | p.138 — Texture A-H Control 1 | p.144 — Texture A-H Control 1 | **TERM-CHANGE** |
| `FEh` | `09h` | p.119 | p.139 — Texture A-H Control 2 | p.145 — Texture A-H Control 2 | **SAME-TEXT** |
| `FEh` | `0Ah` | p.119 | p.140 — Texture A-H Control 3 | p.146 — Texture A-H Control 3 | **SAME-TEXT** |
| `FEh` | `0Bh` | p.119 | p.141 — Texture A-H Control 4 | p.147 — Texture A-H Control 4 | **SAME-TEXT** |
| `FEh` | `0Ch` | p.119 | p.141 — Texture A-H Control 5 | p.147 — Texture A-H Control 5 | **CLOSE-TEXT** |
| `FEh` | `0Dh` | p.119 | p.142 — Texture A-H Control 6 | p.148 — Texture A-H Control 6 | **SAME-TEXT** |
| `FEh` | `0Eh` | p.119 | p.142 — Texture I/J Control | p.148 — Texture I/J Control | **SAME-TEXT** |
| `FEh` | `0Fh` | p.119 | p.142 — : Reserved | p.148 — : Reserved | **REVIEW-TEXT** |
| `FEh` | `10h` | p.119 | p.143 — Coefficient Setting 1 | p.149 — Coefficient Setting 1 | **SAME-TEXT** |
| `FEh` | `11h` | p.119 | p.143 — Coefficient Setting 2 | p.149 — Coefficient Setting 2 | **SAME-TEXT** |
| `FEh` | `12h` | p.119 | p.143 — Coefficient Setting 3 | p.149 — Coefficient Setting 3 | **SAME-TEXT** |
| `FEh` | `13h` | p.119 | p.143 — Coefficient Setting 4 | p.149 — Coefficient Setting 4 | **CLOSE-TEXT** |

##### HParaType `04h`

| Subtype | Address | VX800 manual | VX855 manual | VX900 manual | Document-level classification |
|---|---:|---|---|---|---|
| `—` | `00h` | p.135 | p.154 — Flexible Vertex Format 1 | p.160 — Flexible Vertex Format and Multiple Str | **REENCODE** |
| `—` | `01h` | p.135 | p.155 — Flexible Vertex Format 2 | p.160 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `02h` | p.135 | p.156 — Flexible Vertex Format 3 | p.162 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `03h` | p.135 | p.156 — Flexible Vertex Format 4 | p.162 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `04h` | p.135 | p.156 — Flexible Vertex Format 5 | p.162 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `05h` | p.135 | p.156 — Flexible Vertex Format 6 | p.162 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `06h` | p.135 | p.156 — Flexible Vertex Format 7 | p.162 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `07h` | p.135 | p.157 — Flexible Vertex Format 8 | p.163 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `08h` | p.135 | p.157 — Flexible Vertex Format 9 | p.163 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `09h` | p.135 | p.157 — Flexible Vertex Format 10 | p.163 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `0Ah` | p.135 | p.157 — Flexible Vertex Format 11 | p.163 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `0Bh` | p.135 | p.157 — Flexible Vertex Format 12 | p.163 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `0Ch` | p.135 | p.158 — Flexible Vertex Format 13 | p.164 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `0Dh` | p.135 | p.158 — Flexible Vertex Format 14 | p.164 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `0Eh` | p.135 | p.158 — Flexible Vertex Format 15 | p.164 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `0Fh` | p.135 | p.158 — Flexible Vertex Format 16 | p.164 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `10h` | p.135 | p.158 — Flexible Vertex Format 17 | p.164 — Flexible Vertex Format and Multiple Str | **DIFFERENT-TEXT** |
| `—` | `11h`–`1Fh` | p.135 | p.159 — : Reserved (For Flexible Vertex Format) | p.164 — : Reserved (Flexible Vertex Format and  | **DIFFERENT-TEXT** |
| `—` | `12h` | p.135 | — | p.165 — Flexible Vertex Format and Multiple Str | **NEW-HEADING-900** |
| `—` | `13h`–`1Fh` | p.135 | — | p.165 — : Reserved | **NEW-HEADING-900** |
| `—` | `20h` | p.139 | p.159 — Vertex Buffer & Primitive Setting 1 | p.166 — Vertex Buffer & Primitive Setting 1 | **BIT-CHANGE** |
| `—` | `21h`–`22h` | p.139 | p.160 — Vertex Buffer & Primitive Setting 2 | p.166 — F: Reserved | **UNLISTED/RESERVED** |
| `—` | `22h` | p.139 | p.160 — Vertex Buffer & Primitive Setting 3 | — | **UNLISTED/RESERVED** |
| `—` | `23h` | p.139 | p.161 — Vertex Buffer & Primitive Setting 4 | p.167 — Vertex Buffer & Primitive Setting 4 | **EXTENDED900** |
| `—` | `24h` | p.139 | p.161 — Vertex Buffer & Primitive Setting 5 | p.168 — Vertex Buffer & Primitive Setting 5 | **REVIEW-TEXT** |
| `—` | `25h` | p.139 | p.162 — Vertex Buffer & Primitive Setting 6 | p.168 — Vertex Buffer & Primitive Setting 6 | **MANUAL-ERRATUM** |
| `—` | `26h` | p.139 | p.164 — Vertex Buffer & Primitive Setting 7 | p.170 — Vertex Buffer & Primitive Setting 7 | **CLOSE-TEXT** |
| `—` | `27h` | p.139 | p.166 — Vertex Buffer & Primitive Setting 8 | p.172 — Vertex Buffer & Primitive Setting 8 | **SAME-TEXT** |
| `—` | `28h` | p.139 | p.166 — Vertex Buffer & Primitive Setting 9 | p.172 — Vertex Buffer & Primitive Setting 9 | **SAME-TEXT** |
| `—` | `29h` | p.139 | p.167 — Vertex Buffer & Primitive Setting 10 | p.174 — Vertex Buffer & Primitive Setting 10 | **CLOSE-TEXT** |
| `—` | `2Ah` | p.139 | p.168 — Vertex Buffer & Primitive Setting 11 | p.175 — Vertex Buffer & Primitive Setting 11 | **SAME-TEXT** |
| `—` | `2Bh` | p.139 | p.168 — Vertex Buffer & Primitive Setting 12 | p.175 — Vertex Buffer & Primitive Setting 12 | **CLOSE-TEXT** |
| `—` | `2Ch` | p.139 | p.169 — Vertex Buffer & Primitive Setting 13 | p.175 — Vertex Buffer & Primitive Setting 13 | **SAME-TEXT** |
| `—` | `2Dh` | p.139 | p.169 — Vertex Buffer & Primitive Setting 14 | p.175 — Vertex Buffer & Primitive Setting 14 | **SAME-TEXT** |
| `—` | `2Eh` | p.139 | p.169 — Vertex Buffer & Primitive Setting 15 | p.175 — Vertex Buffer & Primitive Setting 15 | **CLOSE-TEXT** |
| `—` | `2Fh` | p.139 | p.169 — Vertex Buffer & Primitive Setting 16 | p.176 — Vertex Buffer & Primitive Setting 16 | **DIFFERENT-TEXT** |
| `—` | `30h`–`3Fh` | p.139 | p.170 — : Reserved (for Vertex Buffer & Primiti | p.177 — : Reserved (for Vertex Buffer & Primiti | **SAME-TEXT** |
| `—` | `40h` | p.147 | p.170 — Clipping Window to Screen Window Transf | p.177 — Clipping Window to Screen Window Transf | **REVIEW-TEXT** |
| `—` | `41h` | p.147 | p.170 — Clipping Window to Screen Window Transf | p.177 — Clipping Window to Screen Window Transf | **SAME-TEXT** |
| `—` | `42h` | p.147 | p.170 — Clipping Window to Screen Window Transf | p.177 — Clipping Window to Screen Window Transf | **REVIEW-TEXT** |
| `—` | `43h` | p.147 | p.171 — Clipping Window to Screen Window Transf | p.177 — Clipping Window to Screen Window Transf | **SAME-TEXT** |
| `—` | `44h` | p.147 | p.171 — : | p.177 — : | **SAME-TEXT** |
| `—` | `45h` | p.147 | p.171 — Clipping Window to Screen Window Transf | p.177 — Clipping Window to Screen Window Transf | **REVIEW-TEXT** |
| `—` | `46h` | p.147 | p.171 — Clipping Window to Screen Window Transf | p.178 — Clipping Window to Screen Window Transf | **SAME-TEXT** |
| `—` | `47h` | p.147 | p.171 — Clipping Window to Screen Window Transf | p.178 — Clipping Window to Screen Window Transf | **SAME-TEXT** |
| `—` | `48h` | p.147 | p.171 — Clipping Window to Screen Window Transf | p.178 — Clipping Window to Screen Window Transf | **REVIEW-TEXT** |
| `—` | `49h`–`4Fh` | p.147 | p.171 — : Reserved (for Clipping Window to Scre | p.178 — : Reserved (for Clipping Window to Scre | **REVIEW-TEXT** |
| `—` | `50h` | p.147 | p.172 — Clipping Window to Screen Window Transf | p.178 — Clipping Window to Screen Window Transf | **SAME-TEXT** |
| `—` | `51h` | p.147 | p.172 — Clipping Window to Screen Window Transf | p.178 — Clipping Window to Screen Window Transf | **REVIEW-TEXT** |
| `—` | `52h` | p.147 | p.172 — Clipping Window to Screen Window Transf | p.179 — Clipping Window to Screen Window Transf | **SAME-TEXT** |
| `—` | `53h`–`62h` | — | — | p.179 — : Reserved | **NEW-HEADING-900** |
| `—` | `AAh` | — | p.172 — Software Inspection | p.179 — Software Inspection | **DIFFERENT-TEXT** |

##### HParaType `10h`

| Subtype | Address | VX800 manual | VX855 manual | VX900 manual | Document-level classification |
|---|---:|---|---|---|---|
| `—` | `00h` | p.148 | p.68 — PCI Command Setting | p.80 — PCI Command Setting | **CLOSE-TEXT** |
| `—` | `02h` | p.148 | p.68 — Read Register Back Command Setting | p.80 — Read Register Back Command Setting | **CLOSE-TEXT** |
| `—` | `03h` | p.148 | p.68 — Address for Reading-Back Register | p.80 — Address for Reading-Back Register | **REVIEW-TEXT** |
| `—` | `04h` | p.148 | p.68 — Interrupt State Buffer Control 1 | p.80 — CR Command Dispatch Setting | **REDEFINED** |
| `—` | `05h`–`07h` | p.148 | p.69 — Interrupt State Buffer Control 2 | p.80 — : Reserved | **RESERVED900** |
| `—` | `06h` | p.148 | p.69 — Left Vertex Data Threshold | — | **RESERVED900** |
| `—` | `07h` | p.148 | p.69 — Interrupt Command | — | **RESERVED900** |
| `—` | `08h` | — | — | p.81 — Preemption Command Setting | **NEW900** |
| `—` | `09h` | — | — | p.81 — Preemption Command Setting | **NEW-HEADING-900** |
| `—` | `0Ah` | — | — | p.81 — Preemption Command Setting | **NEW-HEADING-900** |
| `—` | `0Bh` | — | — | p.81 — Preemption Command Setting | **NEW-HEADING-900** |
| `—` | `0Ch` | — | — | p.82 — CRMI Setting | **NEW900** |
| `—` | `0Dh` | — | — | p.82 — CRMI Setting | **NEW-HEADING-900** |
| `—` | `0Eh` | — | — | p.82 — CRMI Setting | **NEW-HEADING-900** |
| `—` | `10h` | p.149 | p.69 — VMR Control | — | **UNDOCUMENTED900** |
| `—` | `60h` | p.149 | p.69 — AGP Command Setting 1 | p.83 — AGP Command | **DIFFERENT-TEXT** |
| `—` | `61h` | p.149 | p.70 — AGP Command Setting 2 | p.83 — AGP Command | **REVIEW-TEXT** |
| `—` | `62h` | p.149 | p.70 — AGP Command Setting 3 | p.83 — AGP Command | **DIFFERENT-TEXT** |
| `—` | `63h` | p.149 | p.70 — AGP Command Setting 4 | p.83 — AGP Command | **REVIEW-TEXT** |
| `—` | `64h` | p.149 | p.70 — AGP Command Setting 5 | p.83 — AGP Command | **DIFFERENT-TEXT** |
| `—` | `65h` | p.149 | p.70 — AGP Command Setting 6 | p.84 — AGP Command | **DIFFERENT-TEXT** |
| `—` | `66h` | p.149 | p.70 — AGP Command Setting 7 | p.84 — AGP Command | **DIFFERENT-TEXT** |
| `—` | `67h` | p.149 | p.71 — AGP Command Setting 8 | p.84 — AGP Command | **REVIEW-TEXT** |
| `—` | `68h` | p.149 | p.71 — AGP Command Setting 9 | p.84 — AGP Command | **DIFFERENT-TEXT** |
| `—` | `69h` | p.150 | p.71 — Branch Command Setting | — | **UNDOCUMENTED900** |
| `—` | `6Ch` | p.150 | p.71 — Restore Command Setting 1 | — | **UNDOCUMENTED900** |
| `—` | `6Dh` | p.150 | p.71 — Restore Command Setting 2 | — | **NOT-HEADED-900** |
| `—` | `6Eh` | p.150 | p.72 — Restore Command Setting 3 | — | **NOT-HEADED-900** |
| `—` | `6Fh` | p.150 | p.72 — Restore Command Setting 4 | — | **NOT-HEADED-900** |
| `—` | `70h` | p.150 | p.72 — CMDQ Setting 1 | p.85 — CMDQ Setting | **DIFFERENT-TEXT** |
| `—` | `71h` | p.150 | p.72 — CMDQ Setting 2 | p.85 — CMDQ Setting | **REVIEW-TEXT** |
| `—` | `72h` | p.150 | p.72 — CMDQ Setting 3 | p.85 — CMDQ Setting | **CLOSE-TEXT** |
| `—` | `73h` | p.150 | p.72 — CMDQ Setting 4 | p.85 — CMDQ Setting | **REVIEW-TEXT** |
| `—` | `74h` | p.150 | p.73 — CMDQ Setting 5 | p.86 — CMDQ Setting | **DIFFERENT-TEXT** |
| `—` | `78h` | p.150 | p.73 — CMDQ Setting 6 | — | **UNDOCUMENTED900** |
| `—` | `79h` | p.150 | p.73 — CMDQ Setting 7 | — | **NOT-HEADED-900** |
| `—` | `7Ah` | p.150 | p.73 — CMDQ Setting 8 | — | **NOT-HEADED-900** |
| `—` | `7Bh` | p.150 | p.74 — CMDQ Setting 9 | — | **NOT-HEADED-900** |
| `—` | `7Ch` | p.150 | p.74 — CMDQ Setting 10 | — | **NOT-HEADED-900** |

##### HParaType `11h`

| Subtype | Address | VX800 manual | VX855 manual | VX900 manual | Document-level classification |
|---|---:|---|---|---|---|
| `—` | `00h` | p.152 | p.75 — CR’s Miscellaneous Setting | p.87 — E3/E2 Lock Setting | **BIT-CHANGE** |
| `—` | `02h` | — | — | p.87 — : Reserved | **NEW-HEADING-900** |
| `—` | `03h` | — | — | p.88 — Fence Command Setting | **NEW900** |
| `—` | `04h` | p.152 | p.75 — Fence Command 1 | p.88 — Fence Command Setting | **DIFFERENT-TEXT** |
| `—` | `05h` | p.152 | p.75 — Fence Command 2 | p.88 — Fence Command Setting | **DIFFERENT-TEXT** |
| `—` | `06h` | p.152 | p.76 — Fence Command 3 | p.88 — Fence Command Setting | **DIFFERENT-TEXT** |
| `—` | `07h` | p.152 | p.76 — Fence Command 4 | p.89 — Fence Command Setting | **REVIEW-TEXT** |
| `—` | `08h` | p.153 | p.76 — Save Command 1 | p.90 — , Bit [23:4]) | **DIFFERENT-TEXT** |
| `—` | `09h` | p.153 | p.76 — Save Command 2 | p.90 — , Bit [7:0], | **DIFFERENT-TEXT** |
| `—` | `0Ah` | p.153 | — | p.90 — : Reserved | **NEW-HEADING-900** |
| `—` | `0Bh` | p.153 | p.77 — Save Command 3 | p.90 — , Bit [0]) > T11A09[8] ( | **DIFFERENT-TEXT** |
| `—` | `10h` | p.154 | p.77 — Frame Buffer Automatic Swapping 1 | p.91 — Frame Buffer Automatic Swapping Setting | **DIFFERENT-TEXT** |
| `—` | `11h` | p.154 | p.77 — Frame Buffer Automatic Swapping 2 | p.91 — Frame Buffer Automatic Swapping Setting | **CLOSE-TEXT** |
| `—` | `12h` | p.154 | p.77 — Frame Buffer Automatic Swapping 3 | p.91 — Frame Buffer Automatic Swapping Setting | **REVIEW-TEXT** |
| `—` | `13h`–`17h` | p.154 | p.77 — : Reserved (for Frame Buffer Automatic  | p.91 — : Reserved | **DIFFERENT-TEXT** |
| `—` | `18h` | p.154 | p.77 — Frame Buffer Automatic Swapping 4 | p.91 — Frame Buffer Automatic Swapping Setting | **DIFFERENT-TEXT** |
| `—` | `19h` | p.154 | p.78 — Frame Buffer Automatic Swapping 5 | p.91 — Frame Buffer Automatic Swapping Setting | **REVIEW-TEXT** |
| `—` | `1Ah` | p.154 | p.78 — Frame Buffer Automatic Swapping 6 | p.92 — Frame Buffer Automatic Swapping Setting | **REVIEW-TEXT** |
| `—` | `1Bh`–`2Fh` | p.154 | p.78 — : Reserved (for Frame Buffer Automatic  | p.92 — : Reserved | **DIFFERENT-TEXT** |
| `—` | `30h` | p.154 | p.78 — Frame Buffer Automatic Swapping 7 | p.92 — Frame Buffer Automatic Swapping Setting | **DIFFERENT-TEXT** |
| `—` | `31h` | p.154 | p.78 — Frame Buffer Automatic Swapping 8 | p.92 — Frame Buffer Automatic Swapping Setting | **CLOSE-TEXT** |
| `—` | `32h` | p.154 | p.78 — Frame Buffer Automatic Swapping 9 | p.92 — Frame Buffer Automatic Swapping Setting | **REVIEW-TEXT** |
| `—` | `33h`–`34h` | p.154 | p.78 — : Reserved (for Frame Buffer Automatic  | p.92 — : Reserved | **DIFFERENT-TEXT** |
| `—` | `68h` | p.155 | p.79 — First Branch Command Setting 1 | p.93 — Traditional Branch Command Setting | **BRANCH-CHANGE** |
| `—` | `69h` | p.155 | p.79 — First Branch Command Setting 2 | p.93 — Traditional Branch Command Setting | **DIFFERENT-TEXT** |
| `—` | `6Ah` | p.155 | p.79 — First Branch Command Setting 3 | p.93 — Traditional Branch Command Setting | **DIFFERENT-TEXT** |
| `—` | `6Bh` | p.155 | p.79 — First Branch Command Setting 4 | p.93 — Traditional Branch Command Setting | **DIFFERENT-TEXT** |
| `—` | `6Ch` | — | p.79 — : Reserved (for Second Branch Command S | — | **UNDOCUMENTED900** |
| `—` | `6Dh` | — | p.80 — Second Branch Command Setting 1 | — | **NOT-HEADED-900** |
| `—` | `6Eh` | — | p.80 — Second Branch Command Setting 2 | — | **NOT-HEADED-900** |
| `—` | `6Fh` | — | p.80 — Second Branch Command Setting 3 | — | **NOT-HEADED-900** |
| `—` | `AAh` | p.155 | p.80 — SW Inspection (R/W) 1 | — | **UNDOCUMENTED900** |
| `—` | `ABh` | p.155 | p.80 — SW Inspection (R/W) 2 | — | **NOT-HEADED-900** |

**Appendix classification counts (by explicit block heading, not by silicon register):** SAME-TEXT 82, DIFFERENT-TEXT 49, CLOSE-TEXT 38, REVIEW-TEXT 36, NOT-HEADED-900 21, NEW-HEADING-900 10, BIT-CHANGE 8, UNDOCUMENTED900 8, RESERVED900 4, NEW900 3, UNLISTED/RESERVED 2, DOC-DIFF 1, TERM-CHANGE 1, REENCODE 1, EXTENDED900 1, MANUAL-ERRATUM 1, REDEFINED 1, BRANCH-CHANGE 1. The counts do not imply validated silicon compatibility.

#### 8. Sources and navigation

- `vx800_2.pdf`, *VX800 / VX820 Chrome9 HC3, Part II*, Nov. 19 2008. 3D register material begins around PDF p.56, Type01 around p.69, Type02 around p.105, Type03 around p.124, Type04 around p.135, Type10 around p.148, Type11 around p.152.
- `vx855_2.pdf`, *VX855/VX875 Chrome9 HCM, Part II*, July 29 2009. CR p.57 onward; Type01 p.84 onward, Type02 p.122 onward, Type03 p.144 onward, Type04 p.154 onward.
- `vx900_2.pdf`, *VX900 Chrome9 HD, Part II*, Jan. 19 2011. CR p.75 onward; Type01 p.96 onward, Type02 p.127 onward, Type03 p.150 onward, Type04 p.160 onward, Type08 p.180 onward.
- `vx800_1.pdf`, `vx855_1.pdf`, `vx900_1.pdf` supply the Part I PCI/MMIO interface and 2D register layout. This audit focuses on Part II and does not repeat the earlier report’s display-engine and PCI BAR matrix.

**Important caution:** this is a documentary comparison. No code has been run on VX800 or VX900 hardware, and no test results are claimed for either platform.

---

<a id="openchrome"></a>

## Appendix E. OpenChrome emitted-command tables

*Preserved source section: `Chrome9_OpenChrome_Shared_Command_Matrix.md`.*

### OpenChrome shared hardware-command matrix — VX800 / VX855 / VX900

**Scope:** Compare *actual commands emitted or direct MMIO/VGA writes* by the uploaded `xf86-video-openchrome-master(4).zip`, not general kernel capabilities, driver bugs, or theoretical family resemblance. Cross-reference VIA VX800 HC3, VX855 HCM, and VX900 HD programming manuals. Reviewed November 5, 2026.

#### Interpretation of the evidence

- **Identical emitted sequence:** The same function or common branch writes the same register numbers and bitfield encodings for all three chipsets. Dynamic values (pitch, base address, coordinates, format, colors) naturally vary.
- **Shared limited recipe:** A command stream is shared in OpenChrome, but only for the modes exercised by that source (such as 2D fills and 3D compositing).
- **Not proven equivalent:** A common source function does *not* show that undocumented 3D vertex-buffer, shader, tiling, depth-clear, or recovery modes have identical semantics.
- Paths and line numbers below refer to `xf86-video-openchrome-master/src/` in the *uploaded snapshot*.

#### A. Confirmed common 2D commands — all three chips

`via_exa.c:903–914` assigns **the same H6 callbacks** to VX800, VX855 and VX900: `viaExaPrepareSolid_H6`, `viaExaSolid_H6`, `viaExaPrepareCopy_H6`, `viaExaCopy_H6`, `viaExaDoneSolidCopy_H6`, `viaAccelMarkSync_H6`.

| Command / MMIO offset | Exact common writer or formula | Operation | Source |
|---|---|---|---|
| `0x04` `VIA_REG_GEMODE_M1` | current `tdc->mode` | Set 8/16/32-bpp 2D mode | `via_exa_h6.c:176–183`, `254–264` |
| `0x14` `VIA_REG_DSTBASE_M1` | `dstOffset >> 3` | Destination base address (8-byte units) | same |
| `0x1C` `VIA_REG_SRCBASE_M1` | `srcOffset >> 3` | Source base address (8-byte units) | `via_exa_h6.c:254–264` |
| `0x08` `VIA_REG_PITCH_M1` | fill `(dstPitch >> 3) << 16`; copy `((dstPitch >> 3) << 16) \| (srcPitch >> 3)` | Source/destination pitches | `via_exa_h6.c:176–183`, `251–263` |
| `0x10` `VIA_REG_DSTPOS_M1` | `(dstY << 16) \| dstX` | Destination start | same |
| `0x18` `VIA_REG_SRCPOS_M1` | `(srcY << 16) \| srcX` | Source start | `via_exa_h6.c:260` |
| `0x0C` `VIA_REG_DIMENSION_M1` | `((height - 1) << 16) \| (width - 1)` | Rectangle dimensions | `via_exa_h6.c:181,262` |
| `0x58` `VIA_REG_MONOPATFGC_M1` | solid color | Fixed-color pattern for solid fill | `via_exa_h6.c:182` |
| `0x00` `VIA_REG_GECMD_M1` | `VIA_GEC_BLT \| VIA_GEC_FIXCOLOR_PAT \| ROP` for fill; `VIA_GEC_BLT \| ROP \| direction` for copy | **Launch fill / copy** | `via_exa_h6.c:157,183,212–216,263` |
| `0x48` `VIA_REG_KEYCONTROL_M1` | H6 key/plane-mask state | Transparency and byte-mask state | `via_exa_h6.c:83–98` |

`OUT_RING_H1` converts these writes to the same **Header-1** packet format (`via_dmabuffer.h:55–73`). The source uses identical 2D engines even though other chip-specific display controls exist. Source constants are in `via_regs.h:146–171`.

##### Exact shared solid-fill command sequence

```c
OUT_RING_H1(VIA_REG_GEMODE_M1, tdc->mode);
OUT_RING_H1(VIA_REG_DSTBASE_M1, dstOffset >> 3);
OUT_RING_H1(VIA_REG_PITCH_M1, (dstPitch >> 3) << 16);
OUT_RING_H1(VIA_REG_DSTPOS_M1, (y1 << 16) | (x1 & 0xFFFF));
OUT_RING_H1(VIA_REG_DIMENSION_M1, ((h - 1) << 16) | (w - 1));
OUT_RING_H1(VIA_REG_MONOPATFGC_M1, tdc->fgColor);
OUT_RING_H1(VIA_REG_GECMD_M1, tdc->cmd);
```

**Source:** `via_exa_h6.c:164–185`. The 2D setup and final command write are the same across all three. Parameters vary per rectangle. The code does not use the VX900-specific 2D AlphaBLT opcode in this shared fill/copy path.

#### B. Common command transport, queue programming and status

| Hardware interface | OpenChrome behavior shared by VX800/VX855/VX900 | Source |
|---|---|---|
| Direct 3D parameter transport | `0x43C` (`VIA_REG_TRANSET`) selects parameter type; `0x440` (`VIA_REG_TRANSPACE`) carries command/data words | `via_regs.h:193–196`, `via_exa.c:55–63` |
| 3D/video register area | Starts at `0x400`; OpenChrome reads `0x400` as engine status | `via_regs.h:193–210` |
| Direct queue initialization | `MMIO 0x41C = 0x00100000`; `0x420 = 0x680A0000`, then `0x02000000` | `via_ums.c:285–290`, selected `:436–447` |
| Queue programming | At `0x41C` and `0x420`, transmit `0x72...`, `0x70...`, `0x71...`, `0x73...`, `0x74301001`, `0` (with dynamic buffer-address fields) | `via_ums.c:351–370`, selected `:449–461` |
| Queue disable | `0x41C = 0x00100000`; `0x420 = 0x74301000` | `via_ums.c:375–401` |
| Idle polling | Read `0x400` and poll the same H5 mask: command regulator `0x10`, 2D `0x02`, 3D `0x00001FE1` | `via_exa.c:465–479`; defines `via_regs.h:204–208` |
| Before direct Header-1 writes | Wait for the shared command-regulator/2D busy masks | `via_exa.c:75–82` |

For all three, OpenChrome sets queue length with `0x73000000 | (VIA_VQ_SIZE >> 3)` (`VIA_VQ_SIZE = 256*1024`, in `via_driver.h:101`). This is an **identical formula**, not an independently validated unit interpretation. The earlier reported discrepancy with the manuals' queue-length units remains unresolved; avoid treating the source formula alone as hardware proof.

**Important for the VX855 ring work:** `0x43C/0x440` is the *parameter-send interface*, not a verified queue read pointer. The queue setup is performed through `0x41C/0x420`. To demonstrate actual consumption, correlate status transitions and output/completion, not register readback assumptions.

#### C. Shared, restricted 3D commands from OpenChrome compositing

All three chipsets are wired to the same `viaExaCheckComposite_H6`, `viaExaPrepareComposite_H6`, `viaExaComposite_H6` in `via_exa.c:943–952`. These functions call the **same** `via_3d.c` state- and vertex-emission implementation. This is hardware-accelerated XRender-style compositing, *not* OpenChrome's general Direct3D/OpenGL game renderer.

| Command / type+subaddress | Shared OpenChrome purpose | Source |
|---|---|---|
| Header-2 `HC_ParaType_NotTex = 0x01`, subaddresses `0x40/0x41/0x42` | Render-target base low/high, format, pitch and location | `via_3d.c:400–408`, constants `via_3d_reg.h:613–615` |
| Type `0x01`, `HC_SubA_HABLRFCa`, `HABLRFCb`, `HABLCsat`, `HABLCop`, `HABLAsat`, `HABLAop` | Alpha-blending coefficients/operators | `via_3d.c:410–419` |
| Type `0x01`, `HROP=0x44`, `HFBBMSKL=0x43` | 3D ROP/plane-mask state | `via_3d.c:421–452`, `via_3d_reg.h:613–618` |
| Type `0x01`, `HSolidCL`, `HPixGC` | Constant color and alpha | `via_3d.c:448–452` |
| Type `0x01`, `HEnable` | Enable writes, blending and texture mapping | `via_3d.c:455–471` |
| Type `0x02`, texture subtype 0/1 | Texture format, location, base address, pitch, width/height, wrap, texture blend | `via_3d.c:474–529` |
| Type `0x01`, `HClipTB`, `HClipLR` | Clipping rectangle | `via_3d.c:538–549` |
| Type `0x00`, `OUT_RING_SubA(0xEC, ...)`, `OUT_RING_SubA(0xEE, ...)` | Direct vertex/primitive setup and fire | `via_3d.c:314–385` |
| Type `0x00`, raw vertex words | Six vertices per textured rectangle: `X`, `Y`, `W`, and optional `(S,T)` per active texture | `via_3d.c:332–378` |

The compositor programs the **same** two-texture path on all three. Its simple direct-vertex path avoids needing to establish that the newer VX900 *indexed vertex-buffer* mappings (`HParaType 04h`) are identical to VX855's legacy FVF settings—they are not. The cross-platform similarity is strongest for these **narrow direct-draw recipes**.

The shared H2 packet generation is in `via_dmabuffer.h:72–97`. Shader instruction loading, hardware T&L, depth/stencil clearing, multi-stream vertex fetch, and full gaming OpenGL are **not demonstrated** by these simple compositor commands.

#### D. Identical display writes versus differing values

| Shared or divergent | Actual evidence | Source |
|---|---|---|
| **Same sequencer restore commands** | Restore `SR14`, `SR68–SR6F` for framebuffer/GTI across all three | `via_display.c:871–888` |
| **Same register family for CRTC start** | `CR0C`, `CR0D`, `CR48`; write **CR34 last** as start-address commit | `via_display.c:2126–2154` |
| **Same VGA restore interface** | Shared CRTC restore of `CR30`, `CR31`, `CR36`, `CR37`, `CR43`, `CR45`, etc. | `via_display.c:2040–2118` |
| **Not the same FIFO values** | VX800: `SR17=5F`, `SR16=26`, `SR18=26`, `SR22=10`; VX855/VX900: `C7`, `90`, `90`, `28` | `via_display.c:1799–1820` |
| **VX900 special write** | On VX900 only, initialization includes `MMIO 0x60 = 0` | `via_ums.c:430–434` |

Do **not** treat identical register addresses with differing values as a fully identical configuration command. The `CR34` write-last rule is particularly useful because it agrees with the prior VX855 display-address commit investigations.

#### E. What this establishes for a shared Windows 98 backend

**Strongest candidates for exactly shared encoders:** H6 2D fills/copies, ROPs, clip writes, basic 2D mode/pitch/coordinates, common Header-1 packet structure, common restricted texture/render-target/blend/clipping emitters, and direct-vertex compositing setup. These have direct code-level evidence rather than only manual similarity.

**Share with validation / caveats:** queue enable and disable words, 3D reset-state sequence, status polling, and scanout commit ordering. Common source code is a *recipe*, not a substitute for testing a complete hardware queue cycle on each revision.

**Keep chipset-specific:** PCI BAR/framebuffer discovery, display FIFO numeric values, extra VX900 display/2D initialization, legacy FVF vs VX900 `HParaType=04h` multiple-stream encoding, and all advanced 3D modes whose command equivalence is not established by OpenChrome.

##### Narrow practical comparison plan

1. Take one identical, basic **H6 2D fill** command packet and compare its register writes/visible pixels on VX800, VX855 and VX900.
2. Repeat with **screen copy**, **ROP** and overlap directions.
3. Compare exactly the **same limited 3D state packet** (`type 01h` + one `type 02h` texture) and one direct textured quad packet (`type 00h`). Avoid vertex-buffer and shader claims.
4. Log queue setup and status before/after each packet, including whether it was submitted by direct MMIO or actual buffered command fetching.
5. For every command, record whether the *same encoding* renders identically; label any hardware-revision exceptions. Keep non-OpenChrome modes out of the initial comparison.

##### Source list

- `via_exa.c` — cross-chip callback grouping, busy polling, command flush.
- `via_exa_h6.c` — exact 2D fill/copy command words.
- `via_3d.c` and `via_3d_reg.h` — common restricted 3D state/texture/vertex emitters.
- `via_ums.c` — shared queue initialization and chipset exceptions.
- `via_dmabuffer.h` and `via_regs.h` — H1/H2 packet encoding and MMIO offsets.
- `via_display.c` — common scanout commands and chipset-specific FIFO values.

*Confidence tags: "identical source recipe" means verified in uploaded source; "hardware equivalence" remains a separate empirical question until executed on each platform.*

---

#### F. Independent re-check of the shared-command claims (2026-11-05)

**Method.** Reopened the supplied `xf86-video-openchrome-master(4).zip`, inspected the actual callback assignments and command emitters, and cross-checked relevant register descriptions against the supplied VIA VX800 HC3, VX855 HCM and VX900 HD Part I/II PDFs. Here, **source-identical** means the *same code emits the same opcode/register bit packing*, not that every silicon revision has been experimentally exercised. The uploaded OpenChrome snapshot is not a hardware test log.

| Earlier claim | Re-check outcome | Precisely supported scope |
|---|---|---|
| H6 solid fill and screen copy identical across all three | **Confirmed** | `via_exa.c:903–914` assigns one H6 implementation; `via_exa_h6.c:140–265` emits identical 2D register words. |
| 2D ROP / geometry / direction encodings identical | **Confirmed for the H6 fill/copy path** | Common ROP macros, `0x00` command, `0x08` pitch, `0x10/0x18` coordinates, `0x0C` dimensions; overlapping copies set DECX/DECY and adjust endpoints. |
| Shared Header-1 packet construction | **Confirmed** | `via_dmabuffer.h:55–70`; the packet writer is common. |
| Common engine-idle polling | **Confirmed source recipe only** | `via_exa.c:472–479` selects H5 status masks for the same three chipsets; this is not a proof of register-state timing on every GPU. |
| Shared VQ setup/disable | **Confirmed, conditional** | `via_ums.c:436–463` selects the same PCIe-style queue setup for all three **when VQStart != 0**; otherwise it disables VQ. See queue-size discrepancy below. |
| 3D render-target, blend, texture, clipping and direct-vertex quad packets | **Confirmed for EXA compositing** | `via_exa.c:943–952` installs common H6 callbacks if compositing is enabled; `via_3d.c:280–548` has no VX800/VX855/VX900-dependent branch. |
| 3D hardware shader, indexed buffers or depth-clear commands identical | **NOT established** | OpenChrome's `via_3d.c` emits direct XRender-style 3D state/vertices, not the full Win98 Direct3D/OpenGL API implementation; VX900 Type 04h vertex inputs are different in the manuals. |
| Primary scanout address commit ordering shared | **Confirmed** | `via_display.c:2126–2154`: write CR0C, CR0D, CR48 (for all three), then CR34 last. |
| FIFO numeric values shared on all three | **False; matrix already warned** | IGA1 VX800 differs from VX855/VX900; new evidence also identifies *three different IGA2 FIFO recipes*. |
| Different pixel depths supported in same shared 2D copy | **Not established; explicitly rejected by OpenChrome** | `via_exa_h6.c:200–202` returns false if source and destination bits per pixel differ. VX855/VX900 CFC is a *separate documented mechanism* not employed by this EXA copy recipe. |

##### F.1 Actual shared draw packet, with compatibility limits

The H6 2D fill uses exactly seven Header-1 writes (`0x04,0x14,0x08,0x10,0x0C,0x58,0x00`), and the copy uses eight (`0x04,0x1C,0x14,0x08,0x18,0x10,0x0C,0x00`); cf. `via_exa_h6.c:176–183,254–263`. The common preparation also rejects unsupported pitches and some pixmap formats. For copy, an 8-byte-aligned destination pitch is required, and a source pitch divisible by four is required (`:201–208`). This is *not* evidence that all legal engine pitch encodings are implemented by OpenChrome.

The H6 EXA composite path installs `viaExaCheckComposite_H6`, `viaExaPrepareComposite_H6`, and `viaExaComposite_H6`, but only when `!pVia->noComposite` (`via_exa.c:943–952`). Its common state/quad writer (`via_3d.c`) does not branch on these chipsets. This strongly validates identical **issued commands**, though not success on every device or interchangeability with Type-04h hardware-vertex programming.

#### G. Newly confirmed family differences and exceptions

These are *additional* source-level exceptions beyond the initial command matrix. Where available, the source is corroborated by the VIA manuals.

| # | Difference | VX800 | VX855 | VX900 | Exact source / reason |
|---|---|---|---|---|---|
| 1 | PCI device ID | `1106:1122` | `1106:5122` | `1106:7122` | `via_regs.h:79–81`; `via_driver.c:140–142`; Part I PCI tables. |
| 2 | Framebuffer mapping in OpenChrome | PCI region/BAR0 | PCI region/BAR0 | **PCI region/BAR2** | `via_ums.c:229–233`. The graphics MMIO mapping stays in PCI region/BAR1: `via_ums.c:105`. Initialization cannot share one fixed FB BAR index. |
| 3 | VGA CRTC protection bit | Clear **CR47 bit 0** | Clear **CR47 bit 4** | Clear **CR47 bit 4** | `via_display.c:605–645`. These are *different register commands*, despite similar display logic. |
| 4 | IGA1 horizontal display/blanking extension | No VX900 extension | No VX900 extension | **CR45 bits 1 and 2** for extra active/blank-start bits | `via_display.c:1338–1372`; max mode validation `:1574–1584` permits 4096 horizontal active/blank start on VX900 vs 2048 on older chips. |
| 5 | IGA2 horizontal extension | Legacy fields | Legacy fields | Additional **CR55 bit 7** for active width and **CR6B bit 0** for blank-start | `via_display.c:2400–2436`; mode validation `:2607–2617`. |
| 6 | IGA1 FIFO programming | `SR17=5F`, `SR16=26`, `SR18=26`, `SR22=10` | `C7,90,90,28` | `C7,90,90,28` | `via_display.c:1799–1820`. Identical register addresses do not imply identical tuning values. |
| 7 | **IGA2 FIFO programming** (new finding) | **CR68 high B0; low 04; CR94 low 0x10 or 0x20**; see full source | **CR68 high 80; low 08; CR94 low 08** | **CR68 high 70; low 08; CR94 low 08** | `via_display.c:2829–2879`, with additional values in CR92/94/95. VX855 and VX900 have **different IGA2 depth** despite the same IGA1 values. |
| 8 | Expanded IGA2 CRTC restore block | no `CRF6–CRFC` block | restore `CRF6–CRFC` | restore `CRF6–CRFC`, **plus `CRFD`** | `via_display.c:2960–3041`. This is a register-command difference, not just register defaults. |
| 9 | LCD panel power sequence | Software power sequence for LVDS1/LVDS2 (source notes HW issue) | Hardware primary sequence + LVDS1 | Same as VX855 | `via_fp.c:341–362`. |
| 10 | Panel downscaling disable | No VX900-only CR89 write | No VX900-only CR89 write | Clear **CR89 bit 0** | `via_fp.c:718–729`. |
| 11 | HQV source-data-offset register map | `hqv_cme_regs[]` / older register offsets | `hqv_cme_regs_409[]` / alternate offsets | Same `_409` map as VX855 | `via_xv_overlay.c:61–82,320–340`. Thus the same *semantic* HQV operation can write **different addresses** depending on family. |
| 12 | HQV extended initialization | Basic common controls | Extra destination offsets, background, `HQV_EXTENDED_CONTROL`, subpicture scale writes | Same plus **sharpness/decoder handshake write** | `via_xv_overlay.c:2118–2147`. VX800 must not blindly copy VX855 extended writes. |
| 13 | Additional graphics control write | No special `0x60=0` in this branch | No special `0x60=0` | **`MMIO 0x60 = 0`** on init | `via_ums.c:417–434`. The VX900 Part I register summary marks `0x60–0x9F` reserved. **Source/manual disagreement:** don't infer what this zero write does or copy VX855's legacy `0x60` control bits to VX900. |
| 14 | 2D color-format conversion (CFC, `0x30`) | Not documented in VX800 Part I table | Documented | Documented | VX855 Part I PDF p. 84 limits CFC to ROP `CC` and excludes 8 bpp. VX900 Part I PDF p. 107 defines 16-/32-bpp color formats and lists an 8-bpp source-depth code. It omits those VX855 restriction notes. Actual VX900 8-bpp conversion remains unresolved. OpenChrome H6 copy does not use CFC. |
| 15 | 2D AlphaBLT operation | Not in VX800 GE command list | Not in VX855 GE command list | **GE command `0011`** and additional constant-alpha controls | VX900 Part I, 2D GE command/mode description. OpenChrome uses common BLT, not this VX900-specific AlphaBLT recipe. |
| 16 | Advanced vertex-input format | Older FVF scheme | Legacy FVF scheme | **Type 04h input-register/buffer mapping**, Type 03h subtype 30h stream bases | VIA Part II descriptions. Same `04h:10h` address has a *different format* on VX900; OpenChrome's limited direct-vertex compositing bypasses this incompatibility. |

**Practical distinction:** Differences #3–#13 are actual **different OpenChrome writes or selections**. Differences #14–#16 are **documented hardware differences** that OpenChrome does not exercise in its shared compositing path. The source/manual discrepancy in #13 remains unresolved.

#### H. Re-check of the command-queue length and another important condition

`via_driver.h:101` defines `VIA_VQ_SIZE = 256*1024` bytes. `via_ums.c:545–551` requests that allocation and obtains start/end offsets. `via_ums.c:351–369` then programs `HParaType=10h, subaddress 73h` using `VIA_VQ_SIZE >> 3` (32,768). **All three VIA Part II manuals say this length field is in units of 128 bits = 16 bytes** (VX800: Type 10h/73h; VX855 Type 10h/73h; VX900 Type 10h/73h).

- `256 KiB / 8 = 32,768` — OpenChrome's programmed number.
- `32,768 * 16 = 524,288 bytes = 512 KiB` — literal manual interpretation.
- `256 KiB / 16 = 16,384` — the count expected from a 256-KiB length with 128-bit units.

The discrepancy is **confirmed in text and source**, but its effects are **not** established. Possible explanations include a manual error, hidden hardware unit interpretation, or OpenChrome expecting a different queue-length interpretation in this mode. Do not call it a proven overrun without testing. A safe experiment is to test read-only status and normal known-good command completion before changing queue setup; compare the same workload with the known-good and alternative values using guard allocations, timeouts and a recoverable test system.

Furthermore, identical queue setup is **conditional**: the code selects it only when `VQStart != 0`; otherwise it executes queue disable. `via_ums.c:449–464`. Presence of this function in a source tree does not establish that every shipped configuration actually enables the ring.

#### I. Revised programming reuse boundary

**Reuse the command encoder without changing bit packing (source-proven limited scope):** H6 BLT fill/copy, 2D ROPs and rectangle fields, Header-1 transport, `0x43C/0x440` parameter transmission, restricted XRender-style Type-01/02 state, simple Type-00 textured-quad payloads, and the CR0C/CR0D/CR48/CR34 scanout-address commit order.

**Reuse code structure with per-chip numeric values and checks:** PCI BAR discovery, IGA1/IGA2 FIFO, CRTC protection, wider VX900 timings, panel power controls, HQV register selectors, additional VX900 control writes, and virtual queue sizing/polling.

**Do not assume common commands from this evidence:** programmable shader loading, vertex shader/T&L, advanced VX900 vertex fetch and multiple streams, tiled depth clear, stencil behavior, recovery from stuck DMA/ring, arbitrary-format 2D conversion, and VX900-specific AlphaBLT. These need separate manual mappings and hardware tests.

##### Source-linked test matrix additions

1. Compare **same BLT** with identical pixel format, aligned pitches and dimensions on the three GPUs. Ensure no format conversion is silently assumed; record the actual `0x00` command word and ROP.
2. Compare **same Type-00/01/02 composite** using 0, 1 and 2 textures with identical formats and buffer layout. This tests only OpenChrome's implemented subset, not Type-04 FVF.
3. Log **VQ disabled versus enabled**, queue length word, allocated allocation size and engine-idle status; do not infer a read-pointer from `0x43C`.
4. Validate **IGA1 and IGA2 separately**, including VX855 vs VX900's differing secondary FIFO settings.
5. Test VX900's **extended timing and display paths** separately; first use a conservative, known-supported mode to avoid attributing blank output to engine commands.
6. Check **HQV address tables** before reusing video playback sequences. The VX800's register-map selector differs from the VX855/VX900 selector.

---
