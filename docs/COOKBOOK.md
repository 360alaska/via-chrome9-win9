# VX855 Win98: building every proven function from scratch

Nov 8, 2026 · 360ALASKA

Every 2D, 3D and rotation function proven on the VX855 under Windows 98, written as a recipe: the registers, command words and ordering that work, and the forms that hang. Section 9 records the five problems that were never fully solved, with every attempt and its result; 9.7 records the October 2026 review, the decisions taken since and the last measured tuning (v10.138). Section 17 is the D24S8 stencil plane and section 18 the OpenGL ICD (both v10.247); section 19 is the ICD's depth clear, speed work and late features (v10.323-v10.353); section 20 the audit features and exact defaults (v10.354-v10.357); section 23 the Direct3D work for GTA3, the command fetcher's ring and frame timing (v10.367-v10.386); section 24 GTA3's speed work, every achievement and every failed experiment with its numbers, 3DMark2001's render-target textures and the stale-ICD lockup (v10.387-v10.459); section 25 what static analysis of XP's driver found about clears, the stencil plane, Coarse Z, state upload and waits, mapped onto this driver; section 26 the OpenGL ICD's CPU path, Lattice from 572 to 317 ms a frame: flat shading on the engine, the SSE vertex pass and its fused kernels, the strip path's culling run, triangle lists and grid words for game-style draws, three Mesa bugs, the measuring method and what comes next (v10.480-v10.500); section 27 the OpenGL switches measured on the board since then, the ones kept and the ones removed, and glVertex writing the SSE pass's arrays (v10.501-v10.509); section 28 GDI on the CPU: lines, the cursor handshake and DIBs (v10.507-v10.510); section 29 GTA3's Direct3D path: the measuring tools, the engine lock, engine culling, 8-word vertices, state deltas, the register shadows and what each saved (.511-.526); section 30 every feature still left to integrate, with what each needs first; section 31 the .526 board run read in full, the decisions it settles and one refused packet; section 32 how to tell which check refused a packet.

&#91;embedded content: 2D coverage · implemented, software, unsupported\]

What the driver does today. Left: done by the 2D engine or by a tuned CPU loop in the VxD. Middle: left to DIBENG, the software renderer in Windows. Right: the engine cannot do it, or it is not offered to applications. Section 12 lists every registry key that switches these paths.

## 1. Before you start

This is for someone rebuilding the VX855 acceleration from an empty folder. Every recipe here was proven on the real board (Windows 98, 640x480x32, PCI 1106:5122) by drawing into a prefilled window and comparing every pixel against a software model. Anything not proven says so. Register numbers are BAR1 offsets in hex; `01/xx` means 3D render-state register xx (see the 3D foundations section).

**What you need**

- The board running Windows 98 at 640x480x32: desktop pitch 2560 bytes, framebuffer at offset 0, so pixel (x,y) sits at byte `y*2560 + x*4`.
- Ring-0 code that can map BAR1. The lab uses a VxD, with a small ring-0 stack, so keep buffers static and declarations C89.
- A Windows XP machine with VIA's own driver, to capture the register stream of any operation the manual leaves unclear.
- Open Watcom 2.0 to build the VxD and the Win32 test program.

**Hardware access.** Everything is memory-mapped through BAR1 (16 MB). Map at least the first 4 MB, because the 2D host window sits at 0x200000 to 0x3FFFFF.

| BAR1 offset | Block |
| --- | --- |
| 0x000 to 0x0FF | 2D engine registers |
| 0x100 to 0x1FF | 2D colour pattern RAM (64 ports). Never write host data here: it froze the machine |
| 0x400 to 0x7FF | 3D engine. Status at 0x400, command header 0x43C, data 0x440 |
| 0x800 to 0xBFF | Burst area |
| 0x1E00 to 0x1FFF | CBU rotate windows (not needed by any recipe) |
| 0x200000 to 0x3FFFFF | 2D host BitBLT window: host-fed pixel data goes here |

**What the CPU can see of video memory.** The VX855 has no dedicated video RAM: the BIOS carves it out of system RAM at boot, and its size is a BIOS setting (on the test board, a Wyse Cx0 thin client with Phoenix TrustedCore BIOS 1.0G-0407, Setup offers Frame Buffer Size 64, 128, 256 or 512 MB; 512 MB on a 512 MB board leaves no OS memory and the board does not POST). The setting is stored in CMOS byte 0x5E, bits 6:5 (0 = 64 MB, 1 = 128 MB, 2 = 256 MB). At POST the BIOS turns it into a size code n = bits 6:4 of PCI config register 0xA1 on device 0, function 3 of the host bridge, with **video memory = 4 MB << n** (4 = 64 MB, 5 = 128 MB, 6 = 256 MB, 7 = 512 MB), and copies it into VGA SR39 (the size in 4 MB units) and SR68 (an aperture mask). Measured on the 512 MB board, the table below shows the values and what Windows 98 then saw: always 512 MB minus the setting minus about 1.7 MB, so the carve-out is real and exact, and a Windows XP driver that shows 256 MB of video memory is reporting the truth, not echoing a setting. The VGA's BAR0 (the CPU's window onto video memory) is as large as the setting, and BAR1 (the 2D/3D registers) sits just above it. A driver should read the size at load time from SR39 or from register 0xA1, not assume one.

| BIOS setting | Code n (0xA1 bits 6:4) | Register 0xA1 | SR39 | SR68 | BAR0 (CPU window) | Windows 98 total RAM |
| --- | --- | --- | --- | --- | --- | --- |
| 64 MB | 4 | 0xCF | 0x10 | 0xE0 | 64 MB at 0xF8000000 | 446 MB |
| 128 MB | 5 | 0xDF | 0x20 | 0xC0 | 128 MB at 0xF0000000 | 382 MB |
| 256 MB | 6 | 0xED | 0x40 | 0x80 | 256 MB at 0xD0000000 | 254 MB |
| 512 MB | 7 (decoded size 512 MiB) | not dumped | not dumped | not dumped | 512 MB at 0xC0000000 | not measured (2 GB stick) |

**The 4 MiB the lab used to map was a driver constant, not the hardware.** The first lab builds mapped only the first 4 MiB of the window (the constant `VX_FB_LIMIT`, still used for the desktop surfaces, modes and pitch checks), and every BIOSRD dump at 64, 128 and 256 MB reported exactly 4194304 bytes. The mapping limit is now `VX_FB_MAP_LIMIT` (512 MiB) and the mapped size is the size decoded from register 0xA1. Reads or writes beyond the mapped range must still be refused, because an unmapped access page-faults in ring 0. The lab keeps its own arena (0x200000 to 0x210000), command queue (0x220000 to 0x241000) and GART table (0x3BF000 to 0x3D1000) and uses 0x250000 to 0x37C000 for scratch surfaces, so test surfaces at high addresses must stay clear of those. VX\_FB\_LIMIT itself is 64 MiB from v10.73 (it was 16 MiB): the size handed to DirectDraw, the modes and the engine is the larger of the VBE size and the register 0xA1 size, at most that limit (see the 64 MiB row in 8.5).

**Measured reach (VRAM REACH test, v10.04 and v10.05).** The CPU and the 2D engine reach every part of video memory with no wrap-around. At the 128 MB, 256 MB and 512 MB settings the test wrote sentinel dwords with the CPU at offsets from 4 MiB up to the last page of the mapped window, then copied a 32x8 block with the 2D engine to the same offsets and read it back. Results: 128 MB setting, CPU 8 of 8 and engine 6 of 6 up to 127 MiB; 256 MB setting, CPU 12 of 12 and engine 10 of 10 up to 255 MiB; 512 MB setting, CPU 16 of 16 and engine 14 of 14, including 256, 320, 384 and 448 MiB and the last page at 511 MiB. The 256 MiB boundary is the one that would have shown a 25-bit address field wrapping; nothing wrapped, so the 2D engine address field is at least 26 bits. 3D reach (suite v10.11, 512 MB setting): the 3D engine drew into a render target (Gouraud ARGB8888), tested against a depth buffer, and sampled a texture, each placed at ten offsets from 4 MiB to the last 160 KB of the 512 MiB window, and all 30 cases were pixel-exact with results identical to the low-address runs. The render-target base, depth address and texture base fields therefore cover the whole window. Not run: vertices fetched from video memory (every proven recipe sends vertices through the register port), 16bpp targets at high addresses, and the 64, 128 and 256 MB settings for 3D.

**VBE under-reports video memory.** The VBE TotalMemory field returned 33554432 bytes (32 MiB) at the 128 MB, 256 MB and 512 MB settings, so it is not a size source. Read host-bridge register 0xA1 (or SR39) and decode the size from it.

**512 MB setting and a 2 GB stick.** The 512 MB setting leaves nothing for the OS on a 512 MB board, so it only works with more RAM installed. With a 2 GB stick it POSTed and the host-bridge register decoded to 536870912 bytes, with BAR0 at 0xC0000000. Windows 98 did not boot with 2 GB by default; it booted after two SYSTEM.INI changes: `MaxPhysPage=40000` (hex pages, which limits RAM to 1 GB) under \[386Enh\] and `MaxFileCache=262144` (KB) under \[vcache\].

**The helpers every recipe uses** (C, in the style of the lab):

```c
uint32_t rd(uint32_t reg) { return mmio_read(reg); }

void wr(uint32_t reg, uint32_t val) {      /* ordinary register write */
    mmio_write(reg, val);
    (void)rd(0x400);                       /* read status back: drains posted writes */
}

void wr_raw(uint32_t reg, uint32_t val) {  /* bulk host-window data only */
    mmio_write(reg, val);
}

int idle(void) {                           /* 1 = idle, 0 = timeout or hang */
    int stable = 0;
    while (!timed_out()) {
        uint32_t s = rd(0x400);
        if (s == 0xFFFFFFFF) return 0;     /* bus returns all ones: device gone */
        if ((s & 0x1FF7) == 0) { if (++stable == 32) return 1; }
        else stable = 0;
    }
    return 0;
}

void reset_2d(void) {                      /* SR4E = VGA sequencer index 4E */
    uint32_t v = seq_read(0x4E);
    seq_write(0x4E, (v & ~0x0C) | 0x04);   /* bits 3:2 = 01 resets the 2D engine */
    delay_ms(5);
    seq_write(0x4E, v & ~0x0C);            /* bits 3:2 = 00 */
    delay_ms(5);
    wr(0x04, 0x300);                       /* back to 32bpp, no rotation */
}
```

The delay is done by reading 0x400 repeatedly until 5 ms have passed. 3D engine recovery is SR4E bits 5:4 = 01, then always a pulse on SR1A bit 6 (SR4E alone leaves the next textured draw hanging; v10.292).

**Rules every recipe assumes**

1. Program every register first and write the command register (00) last. That write starts the operation.
2. Wait for idle before the CPU reads or writes the framebuffer after an operation. The lab always does.
3. Save the 19 engine registers (04 08 0C 10 14 18 1C 20 24 28 2C 48 4C 50 58 5C 30 40 44) before an operation and restore them afterwards. The boot display driver (a VBE framebuffer driver that hooks only BitBlt) shares the engine.
4. A hang is not recoverable by waiting. Detect it with the idle timeout, call `reset_2d()`, then restore the saved registers.
5. Prove one function at a time in a 64x64 window of the live desktop at (448,320): prefill it with a position-dependent pattern, run the operation, compare every pixel with a model, then restore the window.

### Where the VX855 sits in the VIA/S3 family

&#91;image: VIA/S3 integrated graphics: the UniChrome and Chrome9 families, graphics lineage solid, chipset succession dashed\]

Solid arrows follow graphics lineage, which is what decides how far this driver ports. Dashed arrows follow chipset lineage. So the CN700 and CN896 are chipset brothers, but graphically the CN700 is a grandparent. The CX700 sits in the same position relative to the VX800.

Some placements are less certain than others. The CN896, VX800, VX855 and VX900 graphics placements are solid. The K8M800 entry, the CN700-to-CN896 succession and the H2/H5 2D-engine labels are from the chart author's recollection, not checked against a source. Two notes from this cookbook and its reference library: the HCM has no transform unit (hardware T&L is not possible, 30.3), so "hardware vertex" on the Chrome9 box does not hold for the VX855; and VX800 and VX855 share the H6 3D backend, while VX900 uses H5S2 with different vertex descriptors (vx\_8\_VS\_9\_wince\_re.md).

## 2. 2D engine foundations

**Registers** (BAR1 offsets)

| Reg | Name | Contents |
| --- | --- | --- |
| 00 | Command | Write last. Bit layout below |
| 04 | Mode | Bits 9:8 bits per pixel (00 = 8, 01 = 16, 11 = 32); bits 1:0 rotation angle (0, 90, 180, 270 = 0 to 3) |
| 08 | Pitch | Destination pitch in bits 26:16, source pitch in bits 10:0, both in 8-byte units. 640 pixels at 32bpp = 2560 bytes = 320 units = 0x01400140. Host-fed blits use source pitch 0 |
| 0C | Size | `((height-1) << 16) \| (width-1)` |
| 10 | Destination XY | `(y << 16) \| x` |
| 14 | Destination base | Byte address >> 3 (0 = start of framebuffer) |
| 18 | Source XY | `(y << 16) \| x`. For host-fed rows the low X bits mean something else, see host-fed data |
| 1C | Source base | Byte address >> 3, 128-bit aligned |
| 20 | Pattern | Bits 31:29 Y offset, 28:26 X offset; low bits = address of an 8x8 pattern block in video memory >> 3 |
| 24, 28 | Mono pattern | The 64 pattern bits; 24 also holds the line style mask |
| 2C | Line error | Bresenham error term |
| 30 | CFC | Cleared to 0 by the lab |
| 40, 44 | Clip | Top-left and bottom-right, `(y << 16) \| x`, inclusive |
| 48 | Key control | Bit 15 destination key, bit 14 source key. Key bits live only here |
| 4C | Foreground | Also the destination key and the line colour |
| 50 | Background | Also the source key |
| 58, 5C | Pattern colours | 58 is the fixed colour used by bit 13 |

**Command word (reg 00)**

| Bits | Meaning |
| --- | --- |
| 31:24 | ROP3 |
| 23 | Quick start |
| 22 | Pattern keep |
| 21 | Line is Y-major |
| 20 | Line: do not draw the last pixel |
| 19:17 | Mono row alignment: 000 or 001 byte, 101 dword |
| 16 | Mono pattern transparency |
| 15 | Destination X decrement |
| 14 | Destination Y decrement |
| 13 | Fixed colour pattern |
| 12 | Clip enable |
| 11 | Pattern from registers |
| 10 | Mono source transparency |
| 9 | Pattern is mono |
| 8 | Source is mono |
| 6 | Source comes from the host window (0 = on-screen framebuffer) |
| 3:0 | Operation: 1 BitBLT, 2 text, 5 line, 9 BitBLT with rotate, A text with rotate |

**The common prologue** (32bpp desktop). Every 2D recipe starts with it and then adds its own registers.

```c
void prologue(uint32_t fg, uint32_t bg) {
    wr(0x04, 0x300);                      /* 32bpp, no rotation */
    wr(0x08, (320u << 16) | 320u);        /* 2560-byte pitch both ways */
    wr(0x14, 0);  wr(0x1C, 0);            /* both surfaces start at the framebuffer origin */
    wr(0x30, 0);  wr(0x48, 0);            /* no keys */
    wr(0x58, fg); wr(0x5C, bg);
}
```

**Other surfaces.** To draw into a surface elsewhere in video memory, set 14 (and 1C for a source) to its byte offset >> 3 and the pitch field to its pitch / 8. The pitch must be a multiple of 16 bytes: the field's low bit is not used, so a pitch of 16n + 8 is taken as 16n (VXKEY, v10.277: every keyed copy from a 136, 264 ... 1336-byte pitch came out sheared, and engine fills stopped 8 bytes a row short). The HAL rounds off-screen rows to 16 bytes, and the VxD declines any other pitch (textures keep width x bytes rows for the sampler, so their blits go to the CPU). The 3D scratch target is a 64x64x4 surface at framebuffer offset 0x201000: base 0x40200, pitch field 32 (0x00200000 in reg 08 for a destination).

**Pixel formats.** Colours are 32-bit ARGB when the mode is 32bpp. At 16bpp set 04 bits 9:8 to 01, so the mode word becomes 0x100 | rotation. At 8bpp bits 9:8 are 00 (code 0x000, 1 byte per pixel; measured in v10.75 for fills, copies, colour keys and clipping, rotation not tried). Code 0x200 draws 4-byte pixels like 0x300, so do not use it for 8bpp.

## 3. 2D recipes

Each recipe assumes `prologue()` from the previous section has run, then writes its own registers and finishes with the command write and `idle()`. Colours are 32-bit ARGB.

### Solid fill and ROP3 fills

```c
void fill(int x, int y, int w, int h, uint32_t c) {
    prologue(c, 0);
    wr(0x4C, c);
    wr(0x0C, ((h - 1) << 16) | (w - 1));
    wr(0x10, (y << 16) | x);
    wr(0x00, 0xF0002001);   /* ROP F0 (PATCOPY), bit 13 fixed colour, op 1 */
    idle();
}
```

Other fills change only the top byte of the command: BLACKNESS `0x00002001`, WHITENESS `0xFF002001`, DSTINVERT `0x55002001`, PATINVERT `0x5A002001`. The colour for pattern-based ROPs is in 58.

### Screen-to-screen copy

```c
void copy(int sx, int sy, int dx, int dy, int w, int h) {   /* rectangles do not overlap */
    prologue(0, 0);
    wr(0x0C, ((h - 1) << 16) | (w - 1));
    wr(0x18, (sy << 16) | sx);
    wr(0x10, (dy << 16) | dx);
    wr(0x00, 0xCC000001);   /* ROP CC (SRCCOPY), op 1 */
    idle();
}
```

**Overlapping copy.** Use command `0xCC00C001` (both decrement bits, 15 and 14) and give the source and destination XY of the far corner, meaning the bottom-right pixel of each rectangle: `(sx+w-1, sy+h-1)` and `(dx+w-1, dy+h-1)`. That form was proven. Single-axis variants were not verified separately.

### Clip window

Add bit 12 (0x1000) to any command and set the inclusive limits:

```c
wr(0x40, (top << 16) | left);
wr(0x44, (bottom << 16) | right);   /* the lab uses 0 and 0x01DF027F for the whole screen */
wr(0x00, cmd | 0x1000);
```

### Mono 8x8 pattern fill

```c
prologue(fg, bg);                   /* 58 = foreground, 5C = background */
wr(0x24, pat_lo);  wr(0x28, pat_hi);              /* the 64 pattern bits */
wr(0x20, (off_y << 29) | (off_x << 26));          /* pattern origin offsets, 0 to 7 */
wr(0x0C, size);  wr(0x10, dest_xy);
wr(0x00, 0xF0000A01);               /* bits 11 (from registers) and 9 (mono) */
```

Set bit 16 (`0xF0010A01`) to make the background transparent. Measured on 19 Oct (VX2PAT, 136x128 rectangles at six positions, 32bpp, with register 0x20 = 0): byte k of the 64 pattern bits is the pattern row drawn on the k-th row of the rectangle, counting from the rectangle's top line, not from the screen. Register 0x24 holds bytes 0 to 3 (byte 0 in the low byte) and 0x28 holds bytes 4 to 7. Bit 7 of a byte is the leftmost pixel, and a 1 bit shows the foreground colour in 0x58. X is anchored to the screen (the pixel at screen x uses bit 7 - (x & 7)). Y restarts at byte 0 on every command, so rectangles do not line up vertically unless you rotate the rows yourself.

To anchor Y to the screen as well, rotate the rows by the top line of the rectangle. Here `row[y]` is a pattern row with bit x = pixel x (GDI's layout):

```c
uint32_t m[2] = {0, 0};
for (k = 0; k < 8; k++) {
    uint8_t b = row[(dest_y + k) & 7], r = 0;
    for (j = 0; j < 8; j++) if (b & (1 << j)) r |= 1 << (7 - j);   /* mirror the bits */
    m[k >> 2] |= (uint32_t)r << ((k & 3) * 8);
}
wr(0x24, m[0]);  wr(0x28, m[1]);
```

Proof: with this packing all six test rectangles matched with the pattern at phase 0,0 and normal polarity, including after a plain engine fill and copy, and four clipped fills (clip window in 0x40/0x44, bit 12) were pixel-exact. Not tested: 16bpp, transparent background (bit 16) after the change, patterns with the offsets in 0x20 set. Hazard: the first mono pattern fill after a colour pattern RAM fill drew only the background colour in all three runs (a rectangle at 24,24 each time). It did not happen after a plain fill or copy.

### Colour pattern from pattern RAM

```c
for (i = 0; i < 64; i++) wr(0x100 + i * 4, argb[i]);   /* 8x8 ARGB, row-major */
wr(0x0C, size);  wr(0x10, dest_xy);
wr(0x00, 0xF0400801);               /* bit 22 pattern keep, bit 11 from registers */
```

This is ordinary register writes to the 64 ports, never host data. X is anchored to absolute screen coordinates. Y is not stable: see the next paragraph.

Measured on 19 Oct (VX2PAT, 64 distinct colours, six rectangle positions, 32bpp, in the same order each run): the 8x8 block is read in normal order (row-major, no mirror or transpose) and X follows the screen, but the row where the pattern starts changed between operations (0, then 6, 5, 1, 7, 4 for the six positions) while the same sequence on a freshly started engine gave the same result each time. The engine keeps a hidden row counter that the recipe does not reset. Do not use the pattern RAM for GDI brushes: the driver sends two-colour brushes to the mono pattern fill and multicolour brushes to a colour block in video memory (the next recipe, since v10.77).

### Colour pattern from the framebuffer

Put an 8x8 block of 32bpp pixels (256 bytes) in video memory and set `wr(0x20, block_byte_address >> 3)`, then command `0xF0000001`.

### Bresenham line

```c
void line(int x0, int y0, int x1, int y1, uint32_t c) {
    int ddx = x1 - x0, ddy = y1 - y0, adx = abs(ddx), ady = abs(ddy), n, m;
    uint32_t cmd = 0xF0002005;           /* op 5, bit 13 fixed colour */
    prologue(c, 0);
    wr(0x24, 0xFFFFFFFF);                /* line style: every pixel */
    if (ddx < 0) cmd |= 0x8000;          /* X decrement */
    if (ddy < 0) cmd |= 0x4000;          /* Y decrement */
    if (ady > adx) { cmd |= 0x200000; n = ady; m = adx; }   /* Y-major */
    else           { n = adx; m = ady; }
    wr(0x4C, c);
    wr(0x18, ((2 * m) << 16) | ((2 * (m - n)) & 0x3FFF));
    wr(0x2C, (2 * m - n) & 0x3FFF);
    wr(0x0C, n & 0xFFF);
    wr(0x10, (y0 << 16) | x0);
    wr(0x00, cmd);                       /* add 0x100000 to skip the last pixel */
    idle();
}
```

Both endpoints are drawn unless bit 20 is set. The 0x24 mask must be `0xFFFFFFFF` for a solid line; other masks give a styled line (bit 11 uses the register pattern, bit 9 makes it mono).

**Measured on the Win98 driver** (VX2LINE, v10.89 to v10.90, at 32, 16 and 8bpp; the engine matched a software model of the rules below on every line):

- **End point.** GDI never draws a line's last pixel, so add bit 20 (0x100000); register 0C holds n, the delta along the major axis. A Polyline is the same as its end-exclusive segments.
- **Error term.** The recurrence is: if e >= 0 step the minor axis and add 2(m-n), else add 2m. DIBENG's tie rule depends only on the Y direction: Y increasing starts at e = 2m-n-1, Y decreasing and horizontal lines at e = 2m-n, whatever the major axis and the X direction. The code above is the Y-decreasing form; for Y increasing write 0x2C with the extra -1. The error terms in register 18 (low half) and 2C are 14-bit signed values (mask 0x3FFF); the high half of 18 is 2m.
- **ROP.** The ROP in the command is ignored for 00, FF and 0F (the plain colour in register 4C is drawn), and ROPs that read the destination (5A, 55, A0, FA, 05, 50) hang the engine in every command form tried (the reset recovers it). Use F0 only and compute the colour: R2\_COPYPEN is the pen pixel, R2\_BLACK is 0, R2\_NOTCOPYPEN is the inverted pen pixel masked to the depth. XOR and the other R2 modes stay with DIBENG, and so does R2\_WHITE (at 32bpp DIBENG writes 00FFFFFF, not FFFFFFFF).
- **Clip.** Command bit 12 with clip registers 40 and 44 (inclusive) gives the line cut by the clip rectangle, which is exactly what DIBENG draws (40 of 40 cases at each depth).
- **Through GDI.** 90 sets of random lines through the real driver (5 pens, 5 raster operations, clipped lines, Polylines) were byte-identical to DIBENG at 32, 16 and 8bpp.

**Lines take no pattern, so dashed pens stay on DIBENG.** Measured with VX2DASH and VX2LPAT (v10.92 to v10.93, two runs with a restart between, identical): with command 0x2005 or 0x0005 (bit 2000 clear), bits 8..11 and 16 in every combination, and with 0xA00 plus one of bits 4..7, 17..19, 22..23, and a pattern in registers 24/28 (also 20/30/34/38), every line came out solid (60 of 60 pen pixels; the 0x2005 sweep of bits 8..11 and 16 ran at 32, 16 and 8bpp, everything else at 32bpp only). Command bits 24..27 hang the engine whatever the registers, and so does the register set 1C/3C/54/60 once any extra command bit is set; the reset recovers it. Not tried: bits 6..7 and 17..23 with 0x2005, and the single bits at 16 and 8bpp. The mono pattern of the fills (0x0A01, an 8x8 tile in registers 24/28) does not carry over to lines.

**What DIBENG draws for a dashed pen** (VX2PRIM, identical at 32, 16 and 8bpp, both directions), if you emulate it on the CPU: the pattern advances one step per pixel along the major axis and the end point is not drawn. A style is a cycle of eight units of four pixels (32 pixels): PS\_DASH 0x00FFFFFF, PS\_DOT 0x0F0F0F0F, PS\_DASHDOT 0x00F00FFF, PS\_DASHDOTDOT 0x0F0F0FFF, bit k of the cycle (LSB first) = pixel k pen. Each Output call starts at position 8 for its first segment (a lone PS\_DASH line starts with 16 pen pixels), and each later segment of a Polyline starts at (position at the end of the previous segment + 8) mod 32. Separate LineTo calls restart at 8. OPAQUE mode draws the gaps in the background colour (masked to the depth), TRANSPARENT leaves the destination. Wide pens never reach the driver as wide lines: with a ROP2 that reads the screen GDI sends them as scan bands (OS\_POLYSCANLINE), and wide R2\_COPYPEN lines did not reach the driver at all in VX2WIDE (DIBENG draws them inside its own Output).

**Asynchronous DirectDraw blits work and are on (v10.95/v10.96).** In a DirectDraw full-screen exclusive mode the VxD starts a blit or colour fill (never a line) and returns while the engine draws; the register restore, the sequencer unlock (SR10) and the software-cursor exclusion are held in a pending record and finished by complete\_pending(), which runs first in every later VxD entry: submit, access (every Lock, GetBltStatus, CPU blit), control, lost, busy and the flip control. Gate: the HAL's SetExclusiveMode callback sends control 0xFD000001 (enter) / 0xFD000000 (leave); the gate records exclusive mode whether or not the live switch is on (an earlier version only raised it when async was already on, and VX2ASYNC caught that); any non-DirectDraw request clears it, because GDI calls that fall through to DIBENG cannot be intercepted. A blit goes async only when the switch (registry ACCEL\_ASYNC, live control 0xFC000000|v) and the gate are both on. Control 0xFFFFFE12 reads the counters (blits started, waits that found one running, pending now, flags bit 0 switch / bit 1 gate). VX2ASYNC on hardware (32bpp, twice, the second run after the reboot with the registry value set): 60 random fills and copies including overlapping copies, 150 operations each locked and compared at once, 24 queued operations then GetBltStatus, 30 fill-and-Flip rounds: 0 mismatching pixels, 221 async blits per run, every wait found one running; a 640x400 copy plus 1 ms of CPU work takes 1.43 ms per round synchronously and 1.03 ms asynchronously (the saving is the CPU time that overlaps the engine). VX2ASYNC writes ACCEL\_ASYNC=1 on a full pass, and since v10.103 the default is 1; set it to 0 and reboot to turn the feature off.

**VX2LINE rerun on hardware after the v10.92 line changes: PASS** (T6: 90 sets, 0 differ from DIBENG; 804 lab engine lines, no hangs).

**Stretch measurements (VX2STR v1.0, 640x480 at 32, 16 and 8bpp).** Pixel mapping, with sw the source width, dw the destination width and i the destination pixel: vertical stretches follow the centre rule floor((2i+1)×sw / (2×dw)) for 8->16, 8->24, 8->32, 5->8, 7->16, 10->16 and 8->12 (12->8 also matches it); 15->8 and 15->11 vertically are irregular. Horizontal stretches follow it for 8->16, 8->24, 8->32 and 10->16, but 15->11 matches ceil(i×sw / dw), 8->12 floor(i×sw / dw), and 15->8, 12->8, 5->8 and 7->16 match none of the five candidate rules. The results are identical for StretchDIBits to the screen, StretchBlt memory to screen and memory to memory, and at all three depths, except StretchBlt with source offsets at 16bpp. Timings in ms per frame: 32bpp SetDIBitsToDevice 2.57, StretchDIBits 2x 1.53, 1/2 1.24, from a 24-bit source 3.46 (1:1) / 13.55 (2x) / 13.07 (1/2); 16bpp SetDIBitsToDevice 2.36, StretchDIBits 2x 20.49, 1/2 26.45, from a 24-bit source 9.12 / 17.38 / 14.31, StretchBlt memory 2x to screen 1.36; 8bpp SetDIBitsToDevice 2.54, StretchDIBits 2x 5.46, 1/2 1.46, from a 24-bit source 13.03 / 20.71 / 15.27. A plain C 2x stretch plus SetDIBitsToDevice takes 5.01 (32bpp), 4.22 (16bpp) and 4.01 (8bpp); a plain C 24-bit conversion plus upload about 11 ms. So 16bpp StretchDIBits (about 5x possible) was the one slow case worth a fast path (built in v10.99 to v10.104b: the CPU path below, ACCEL\_STRETCH); 32bpp is already fast, and the 24-bit source cases would gain only a little.

### DIB conversion and stretch on the CPU (StretchDIBits, SetDIBitsToDevice)

The copy engine cannot read system memory, so a bitmap held in memory is converted and written by the CPU. DIBENG does this slowly into a 16bpp screen and for 16-bit sources into a 32bpp screen, and a tight loop beats it by 2x to 5x. Measured at 640x480 (ms per call, DIBENG to CPU loop):

| Source, mode | 16bpp screen | 32bpp screen |
| --- | --- | --- |
| 555, 2x | 21.7 to 5.7 | 17.9 to 7.8 |
| 555, 3x | 18.2 to 3.7 | 14.4 to 5.2 |
| 24-bit, 2x | 18.7 to 6.5 | 14.9 to 7.2 |
| 24-bit, 3x | 16.7 to 4.0 | 13.1 to 4.9 |
| 32-bit, 2x | 19.4 to 6.4 | already fast, left alone |
| 24-bit 1:1 | 9.1 to 6.7 | 3.5, left alone (the loop took 8.3) |
| 32-bit 1:1 | 9.6 to 6.4 | already fast, left alone |
| 555 1:1 | 2.5, left alone (the loop took 3.8) | 16.6 to 10.8 |
| 565 1:1 | not applicable | 16.6 to 12.7 |

**Pixel rules DIBENG follows (VX2STRX, 0 differing pixels over about 1,500 random cases per depth).** A destination pixel i takes source pixel floor(i / k) for a whole-number factor k, per axis. Into a 16bpp screen 24-bit and 32-bit sources are truncated to 565: ((r>>3)<<11) | ((g>>2)<<5) | (b>>3). A 555 source becomes 565 with red and green moved up one bit; the new green low bit is a copy of the old one when the picture is stretched and 0 at 1:1. Into a 32bpp screen a 5-bit value v becomes (v<<3) | (7 if v is odd), a 6-bit one (v<<2) | (3 if v is odd), and 24-bit values pass through. The top byte of a 32bpp pixel is unused: DIBENG leaves old data there, so compare with 0x00FFFFFF and write 0.

```c
/* one source row to one 16bpp destination row, two pixels per 32-bit store */
uint32_t keep = one_to_one ? 0 : 0x00200020;          /* 555 green low bit */
for (i = 0; i < n / 2; i++) {                         /* 555 -> 565 */
    uint32_t w = ((const uint32_t *)src)[i];
    ((uint32_t *)dst)[i] = ((w & 0x7FE07FE0) << 1) | (w & 0x001F001F) | (w & keep);
}
/* 16-bit -> 32bpp: three table lookups a pixel; tr[v] = e5(v)<<16, tg[v] = e5(v)<<8, tb[v] = e5(v) */
dst32[i] = tr[(v >> 10) & 31] | tg[(v >> 5) & 31] | tb[v & 31];
/* enlargement: convert the source row once into a line buffer, repeat each pixel k times, write that line ky times */
```

Use 32-bit stores into the screen; the first version converted pixel by pixel through a function and narrowed 32-bit values to 16-bit stores, which cost 80 ns a pixel and was slower than DIBENG at 1:1.

**What the driver is called with.** StretchDIBits reaches the display driver with the source rectangle's y counted from the bottom of the picture for bottom-up and top-down bitmaps alike, the ROP SRCCOPY, a clip rectangle always given (right and bottom exclusive) and no translation table; mirrored destinations have negative extents. Bottom-up SetDIBitsToDevice and 1:1 StretchDIBits arrive as DibToDevice(X, Y, iScan 0, cScans = height, clip, ...): the whole bitmap at the picture's origin (X and Y may be negative), the clip rectangle doing the cropping, the buffer holding only the lines the clip shows. Top-down bitmaps, including SetDIBitsToDevice with StartScan or cLines, arrive as StretchDIBits. A top-down partial bitmap arrives as a rectangle (for example sy 0, sh 5 of 13 rows) naming rows the buffer does not hold, and DIBENG itself paints garbage for it, so take a top-down bitmap only whole (all rows) and leave every top-down sub-rectangle to DIBENG. Take only whole-number enlargements and 1:1; reductions and mirrored destinations stay with DIBENG.

**Rules for the CPU path.** Lock only the bytes the loop reads (the first and last needed row and column), never start the engine, finish any pending asynchronous command first, hide the software cursor around the writes, and decline (let DIBENG do it) when 3D is busy, the request is malformed or the lock fails. Time every class against DIBENG on the target before enabling it: the CPU path is not always faster, and VX2STRX switches a class on only when its pixels are identical and its time is not worse.

**8-bit palette sources (VX2STRX v2.2).** A colour table in the header (BI\_RGB, up to 256 entries) is turned into a 256-entry table of destination pixels once per call, then each source pixel is one table lookup: the 24-bit rule for the entries (565 truncation into 16bpp, the 24 bits unchanged into 32bpp). Into the 32bpp screen this pays for enlargements (2x 9.1 to 6.8 ms, 3x 9.0 to 4.8 ms at 640x480) and not at 1:1 (DIBENG 3.5 ms, the loop 7.1 ms). Into the 16bpp screen DIBENG is already as fast (2x 2.2 ms, 1:1 3.8 ms) and rounds the colours its own way (blue and the green low bit differ from the 24-bit rule), so leave it alone. DIB\_PAL\_COLORS (index colours) reaches the driver with a non-null translation table (a null one for DIB\_RGB\_COLORS): leave those to DIBENG.

**1-bit, 4-bit and RLE sources (VX2STR2 v1.0, v10.104).** Taken into the 32bpp screen only; into the 16bpp screen DIBENG's palette path is as fast (2x 8.9 ms for 1-bit and 4-bit, under 2 ms per call for RLE). 1-bit and 4-bit rows are unpacked to indices and looked up in the colour table (2 or 16 entries, the same 24-bit rule as above), then written ky times like any other class. RLE8 and RLE4 are decoded by the VxD into a one-byte-per-pixel buffer (at most 262144 pixels, bottom-up bitmaps only), and only streams that write every pixel exactly once are taken: no delta codes, rows ending at their end-of-line code, nothing but end markers after the last pixel, no reads past the stream. Anything else stays with DIBENG. All 12 classes drew pixels identical to DIBENG (56 to 62 random cases per class, the index-colour cases too). Measured at 640x480x32, DIBENG to the CPU path: 1-bit 2x 12.5 to 7.3 ms and 3x 11.3 to 5.1 ms; 4-bit 2x 11.1 to 7.6 ms and 3x 10.1 to 5.2 ms. The 1:1 classes are slower (1-bit 8.8 against 18.0 ms, 4-bit 5.4 against 20.2 ms) and so is every RLE class (DIBENG 3 to 6 ms, decode plus conversion 8 to 17 ms), so those stay off. The driver turns on classes 1, 2, 8 and 16 by default (ACCEL\_STRETCH2 = 27).

**Defaults in the driver (v10.104b; section 12 has the current list).** Registry values under HKLM\\Software\\VX85598 override them, and 0 turns a feature off. ACCEL\_STRETCH = 1605567 (16bpp classes 1, 2, 4, 8, 16, 32; 32bpp classes 128 to 16384; 8-bit classes 524288 and 1048576; the 1:1 classes 64, 32768 and 2097152 stay off because DIBENG is faster), ACCEL\_STRETCH2 = 27 (1-bit and 4-bit classes 1, 2, 8 and 16; the 1:1 classes 4 and 32 and all RLE classes 64 to 2048 stay off), ACCEL\_ASYNC = 1, ACCEL\_LINES = 1, ACCEL\_DASH = 0.

### Source colour-key copy

```c
prologue(0, 0);
wr(0x50, key);  wr(0x48, 0x4000);        /* source key enabled */
/* then size, source XY, destination XY, command 0xCC000001 as for a copy */
```

Source pixels equal to the key are skipped; the destination keeps its old value there.

### Destination colour-key copy and key fill

```c
prologue(0, 0);
wr(0x4C, key);  wr(0x48, 0x8000);        /* destination key enabled */
/* copy: size, source XY, destination XY, command 0xCC000001 */
```

Only destination pixels equal to the key are overwritten by the source. To fill the key-coloured pixels of a box with one colour, use either of these two proven forms (the natural fixed-colour form with bit 13 hangs, see What not to build):

```c
/* form A: mono register pattern of all ones */
wr(0x24, 0xFFFFFFFF);  wr(0x28, 0xFFFFFFFF);
wr(0x48, 0x8000);  wr(0x4C, key);
wr(0x58, fill);    wr(0x5C, fill);
wr(0x0C, size);  wr(0x10, dest_xy);
wr(0x00, 0xF0000A01);

/* form B: key copy from a solid surface of the fill colour (CC000001) */
```

Form A filled 174 of 174 key pixels in the 16x16 test box and touched nothing else.

### Host-fed colour blit

The CPU supplies the source pixels through the host window. The engine takes them from the window as it draws, so source pitch is 0.

```c
wr(0x04, 0x300);
wr(0x08, 320u << 16);                    /* destination pitch only: source pitch 0 */
wr(0x14, 0); wr(0x1C, 0); wr(0x30, 0); wr(0x48, 0);
wr(0x0C, ((h - 1) << 16) | (w - 1));
wr(0x10, (y << 16) | x);
wr(0x00, 0xCC002041);                    /* ROP CC, bit 13, host source (bit 6), op 1 */
for (i = 0; i < w * h; i++) wr_raw(0x200000, pixel[i]);   /* or 0x200000 + 4*i */
idle();
```

Constant and incrementing window addresses both work. At 32bpp one pixel is one dword and rows need no padding. For 16bpp see the rotation section, which has the row rule.

### Host-fed mono bitmap

Same setup, with mono source: 4C is the colour for 1 bits, 50 for 0 bits, and bitmap bytes are MSB-first (the leftmost pixel is bit 7).

```c
wr(0x4C, fg);  wr(0x50, bg);
wr(0x00, 0xCC0A0141);   /* mono source (8) + host (6), dword row alignment (bits 19:17 = 101) */
/* send one dword per row: the first 8 pixels are the low byte of the dword */
```

For a 32x8 box that is 8 dwords. Bit 10 (`0xCC000541`) makes the background transparent.

### Host-fed text

```c
wr(0x24, 0); wr(0x28, 0);
wr(0x40, clip_top_left);  wr(0x44, clip_bottom_right);   /* absolute, inclusive */
wr(0x4C, fg);  wr(0x50, bg);
wr(0x00, 0xCC021542);   /* op 2 text, host, mono transparency, clip, row alignment enabled */
```

Then send the bitmap bytes through the host window as for a mono bitmap. This is the command XP issues for unrotated text; rotated text is in section 4D.

## 4. Rotation recipes

There are two proven routes. The 2D engine rotates a source the CPU feeds through the host window (32bpp and 16bpp). The 3D engine rotates a surface that is already in video memory. The 2D engine's own screen-to-screen rotate hangs, so do not use it. Mono glyph bitmaps rotate through the host window too (4D).

### A. Host-fed 2D rotation, 32bpp

The engine fills the destination box column by column from the row-major host stream. Two consequences decide the recipe: the engine size must be the destination size (swapped for 90 and 270), and the start corner depends on the angle.

| Angle (clockwise) | Mode (04) | Command | Engine size | Start in the destination box |
| --- | --- | --- | --- | --- |
| 90 | 0x301 | 0xCC00A049 (base 0xCC002049 plus X decrement 0x8000) | h wide by w tall | top-right |
| 270 | 0x303 | 0xCC006049 (base plus Y decrement 0x4000) | h wide by w tall | bottom-left |
| 180 | 0x302 | 0xCC00E049 (base plus X and Y decrement 0xC000) | w wide by h tall | bottom-right |

Here w and h are the width and height of the source image. The base command `0xCC002049` is ROP CC, bit 13, host source (bit 6) and operation 9 (BitBLT with rotate).

```c
/* src: w x h pixels, row-major ARGB. (dx,dy) = top-left of the destination box. */
void rotate32(int angle, const uint32_t *src, int w, int h, int dx, int dy) {
    uint32_t cmd = 0xCC002049, mode;
    int bw, bh, sx, sy, i;                    /* destination box size, start corner */
    switch (angle) {
    case 90:  mode = 0x301; cmd |= 0x8000; bw = h; bh = w;
              sx = dx + bw - 1; sy = dy; break;               /* top-right */
    case 270: mode = 0x303; cmd |= 0x4000; bw = h; bh = w;
              sx = dx; sy = dy + bh - 1; break;               /* bottom-left */
    default:  mode = 0x302; cmd |= 0xC000; bw = w; bh = h;
              sx = dx + bw - 1; sy = dy + bh - 1; break;      /* 180: bottom-right */
    }
    wr(0x04, mode);
    wr(0x08, 320u << 16);                     /* destination pitch, source pitch 0 */
    wr(0x14, 0); wr(0x1C, 0); wr(0x30, 0); wr(0x48, 0);
    wr(0x18, 0);                              /* source X = 0 */
    wr(0x0C, ((bh - 1) << 16) | (bw - 1));
    wr(0x10, (sy << 16) | sx);
    wr(0x00, cmd);
    for (i = 0; i < w * h; i++) wr_raw(0x200000, src[i]);
    idle();
    wr(0x04, 0x300);                          /* restore the unrotated mode */
}
```

**Proof.** 27 of 27 shapes matched the model: 10x6, 6x10, 30x20, 20x30, 31x17, 17x31, 1x10, 10x1 and 3x7, plus squares of 5, 12, 16, 24 and 32. Mode 0x301 without the decrement bit gives a diagonal transpose, and with both decrement bits an anti-transpose, which is how the direction bits were found.

**Hazards.** Send every pixel: the engine hangs on short data. Angle 0 or 180 with some other decrement combinations also hangs, so use exactly the three combinations above. The CBU rotate window is not needed.

### B. Host-fed 2D rotation, 16bpp

Same recipe with the bits-per-pixel field set to 01: modes `0x101`, `0x102`, `0x103`. One extra rule makes the row data line up.

- Set source X (register 18) to 0. The low bits of source X (mod 8 at 16bpp, mod 4 at 32bpp) are the number of pixels the engine skips at the start of every row, and with a non-zero value rows are then padded to 16-byte units. With source X = 0 rows are padded only to a whole dword.
- So pack each source row to an even number of pixels (pad an odd width with one unused pixel). A row is `(w + 1) / 2` dwords.
- Send a few spare dwords after the data, because the engine hangs if it is short.
- Destination x does not matter.

**Proof.** 18 of 18 (10x6, 6x10, 30x20, 20x30, 32x32 and 31x17, each at 90, 180 and 270), on the 64x64 scratch surface. The earlier 16bpp failures were all caused by the lab's default source X of 508.

### C. 3D textured rotation of a video-memory surface

Use the 3D engine to draw a textured rectangle whose texture is the source surface, with the texture coordinates rotated. The CPU never touches the pixels. For a destination pixel (x, y) inside the 64x64 window with origin (ox, oy) = (448, 320), let dx = x - ox and dy = y - oy; the source position sampled is:

| Rotation | u | v |
| --- | --- | --- |
| 90 clockwise | ox + dy | oy + 63 - dx |
| 180 | ox + 63 - dx | oy + 63 - dy |
| 270 clockwise | ox + 63 - dy | oy + dx |

The three rectangle vertices are given those coordinates, each divided by 2^exp where exp is log2 of the texture size (6 for a 64-wide texture). Everything else is the texturing recipe in the 3D recipes section.

**Proof.** 30 of 30 cases, zero errors, no half-pixel shift: three rotations by ten texture cases (nearest, bilinear, 2x magnified, clamp, mirror, L8, ABGR, BGRA, alpha with blending). **Not tested:** non-square rectangles, sub-regions of a larger surface, 16bpp textures and timing.

### D. Host-fed mono text, rotated (op A)

The text operation with rotate (op A, the command XP issues for rotated text) works the same way as the colour rotate in 4A: the CPU feeds the glyph bitmap through the host window and the engine draws it turned. The destination box is the bitmap size for 0 and 180 degrees, and the **swapped** size for 90 and 270. The engine size register (0C) takes the destination size, and the start corner and decrement bits follow the same rules as 4A.

| Angle | Mode (04) | Command, opaque | Command, clip + transparency | Decrement | Start corner of the box | Engine size |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | `0x300` | `0xCC02014A` | not tested | none | top-left | w x h |
| 90 cw | `0x301` | `0xCC02814A` | `0xCC02954A` | X | top-right | h x w |
| 180 | `0x302` | `0xCC02C14A` | `0xCC02D54A` (XP's exact word) | X + Y | bottom-right | w x h |
| 270 cw | `0x303` | `0xCC02414A` | `0xCC02554A` | Y | bottom-left | h x w |

```c
/* w x h = glyph bitmap size; bw x bh = destination box; (x0,y0) = its top-left; 32bpp desktop */
bw = (angle == 90 || angle == 270) ? h : w;
bh = (angle == 90 || angle == 270) ? w : h;
wr(0x04, 0x300 | (angle / 90));            /* 0x300, 0x301, 0x302, 0x303 */
wr(0x0C, ((bh - 1) << 16) | (bw - 1));
switch (angle) {                            /* start corner + command */
 case 0:   sx = x0;          sy = y0;          cmd = 0xCC02014A; break;
 case 90:  sx = x0 + bw - 1; sy = y0;          cmd = 0xCC02814A; break;  /* X dec */
 case 180: sx = x0 + bw - 1; sy = y0 + bh - 1; cmd = 0xCC02C14A; break;  /* X+Y dec */
 case 270: sx = x0;          sy = y0 + bh - 1; cmd = 0xCC02414A; break;  /* Y dec */
}
wr(0x10, (sy << 16) | sx);
wr(0x4C, fg);  wr(0x50, bg);
/* clip + transparency: cmd |= 0x1000 | 0x0400, and wr(0x40, box top-left); wr(0x44, box bottom-right) first */
wr(0x00, cmd);
/* then one dword per bitmap row (32-pixel-wide glyphs); first 8 pixels = low byte */
```

**Feed layout (proven).** With alignment 101, each glyph row takes a whole number of dwords: width 33 is two dwords per row, the first 8 pixels are the low byte of the first dword, and bits run most significant first within each byte. With alignment 001, rows are packed byte by byte, each row `(w + 7) / 8` bytes with no padding between rows, and the stream is padded to a dword only at the end. Send exactly the dwords the glyph needs; no spare dwords were needed. At 16bpp the box origin and the clip are in 16-bit pixels, the colours are the low 16 bits of registers 4C and 50, and pitch and base are set as for any 16bpp surface (as in 4A). The clip registers are in destination coordinates, the same as for an unrotated draw.

**Proof.** Three runs. (1) A 12-rung ladder from the proven unrotated text command up to XP's exact word. (2) A 9-rung confirm run straight after a restart (32x8 glyph with 103 set bits, box at (4,4) of the 64x64 window): unrotated control, 180 opaque, XP's exact word, 90 and 270 opaque and with clip plus transparency, 7 of 7 defined cases, every box pixel and the rest of the tile checked. (3) A 192-run sweep: 192 passed, 0 hung. Glyph widths 5, 13, 24, 32 and 33 by height 8, at 0, 90, 180 and 270 degrees, each drawn four ways (opaque; transparent with the clip equal to the box; opaque with a clip cutting through the glyph; transparent with a cutting clip), in three configurations: 32bpp with dword-padded rows (alignment 101), 16bpp with dword-padded rows (widths 13 and 33 only), and 32bpp with byte-packed rows (alignment 001). Every tile pixel was compared with its exact expected colour (32bpp foreground FFFF8000 and background FF004080; 16bpp F81F and 07E0). Transparent draws left the background at its prefill value, and no run had a wrong pixel inside or outside the box. Width 33 crosses a dword boundary, so that case is covered.

**Hazards.** Op A at mode `0x302` without both decrement bits (`CC02014A`, `CC0A014A`) hangs. With the engine size left unswapped at 90 or 270, the engine does not hang but draws a scrambled box. Use the table exactly.

**Not tested:** 8bpp destinations, glyph heights other than 8, widths over 33, and 16bpp at widths other than 13 and 33. The unrotated clip plus transparency form (`CC021542`) is proven in section 3.

## 5. 3D engine foundations

The 3D engine is driven through the same BAR1 window, but not by register offset. Everything goes through two ports: **0x43C** selects a parameter *type* (written as `type<<16`), and **0x440** takes *register writes* for that type (written as `(index<<24) | (value & 0xFFFFFF)`). Vertex data is the exception: after `type 0`, raw dwords go to 0x440 with no index byte.

```c
/* Port helpers. wr() = write BAR1 + drain-read 0x400 (see section 1). */
static void type(uint32_t t)              { wr(0x43C, t << 16); }
static void reg(uint32_t a, uint32_t v)   { wr(0x440, (a << 24) | (v & 0xFFFFFFUL)); }
/* Types 0x10/0x11 (command path) use 0x41C/0x420 instead when path flag 1 is set;
   the proven path used 0x43C/0x440 for everything. */
```

### 5.1 Parameter types used

| Type | Meaning | Used for |
| --- | --- | --- |
| 0x00 | vertex stream | raw vertex dwords to 0x440 |
| 0x01 | render state | blend/alpha/fog (reg 00), enables (01), cache (02), depth buffer (10–13), render target (50–53), shader slots (90–9B), texture-cache strobe |
| 0x02 | texture attributes | base, size, pitch, format (selected through 0x43C = 0x00020000) |
| 0x04 | vertex/primitive setup | vertex layout (00–03), masks (25/28), viewport (40–48), clamp (52), mode (23), count (24) |
| 0x10, 0x11 | command path | disable queues, FIFO defaults |
| 0xFE | front-end config | 16 fixed words (below) |

Selector words written straight to 0x43C (not through `type()`): `0x22030000` = constant bank 0 (then four floats to 0x440), `0x21030000` / `0x20030000` = shader program words, `0x10030000` = sampler constant block, `0xFE020000` = general texture state, `0x00020000` = texture 0 attributes, `0x20020000` = sampler stage 0.

### 5.2 Once per mode-set: bring the engine up

Order matters. Do it after the 2D engine has been proven idle and before any draw.

```c
void init3d(void) {
  /* a) command path: disable both queues, no AGP/DMA */
  type(0x10); reg(0x74,0); reg(0x7C,0);
  reg(0x00,0); reg(0x02,0); reg(0x07,0);
  reg(0x68,0x0A0000);
  type(0x11); reg(0x00,0x248);

  /* b) front-end configuration: 16 fixed words straight to 0x440 */
  static const uint32_t fe[16] = {
    0x000E1004,0x08000249,0x0A0002C9,0x0B0002FB,
    0x0C000000,0x0D0002CB,0x0E000009,0x10000049,
    0x110002FF,0x12000008,0x130002DB,0x20000000,
    0x40008408,0x44000000,0x46000000,0x47000404 };
  type(0xFE);
  for (int i = 0; i < 16; i++) wr(0x440, fe[i]);

  /* c) render state */
  type(0x01);
  reg(0x00,0);              /* blend, fog, alpha test, dither off */
  reg(0x01,0x40);           /* enables: colour path on */
  reg(0x02,2); reg(0x02,0); /* invalidate cache once, release */
  /* depth buffer defaults: base 0x10, pitch 0x11, format 0x12, func 0x13 */
  reg(0x12, 4UL<<16);       /* 32-bit fixed Z */
  reg(0x13, 1UL<<16);       /* LESS */
  reg(0x15,0); reg(0x16,0); reg(0x17,0); reg(0x18,0);
  reg(0x19,0); reg(0x1A,0); reg(0x1B,0);
  reg(0x1E,0x800000); reg(0x1F,0); reg(0x20,0x3F00);   /* near=0 far=1 */
  reg(0x21,0); reg(0x22,0); /* no occlusion write-back */
  reg(0x53,0xC0F);          /* ROP COPY, write all four channels */

  /* d) vertex layout: x,y,z,w,ARGB = 5 dwords */
  type(0x04);
  reg(0x00,5); reg(0x01,0x020100); reg(0x02,0x000503);
  reg(0x10,0); reg(0x20,0); reg(0x21,0); reg(0x22,0);
  reg(0x25,0xF40000); reg(0x26,0); reg(0x27,0);
  reg(0x28,0xF40000); reg(0x29,0);
  reg(0x2F,0x400000);       /* default W = 1 */

  /* e) screen-space (identity) viewport, 04/40..48 */
  reg(0x40,0x800000); reg(0x41,0); reg(0x42,0x3F);
  reg(0x43,0x800000); reg(0x44,0); reg(0x45,0x3F);
  reg(0x46,0x800000); reg(0x47,0); reg(0x48,0x92003F);
  reg(0x50,0); reg(0x51,0); reg(0x52,0x0B45C5);   /* coordinate clamp */
}
```

The proven XP-style native triangle path then changes a handful of these (the "captured bundle"): `04/41=0x400000, 04/42=0x00493F, 04/44=0x400000, 04/45=0x00493F, 04/52=0x147FFF` (a 786432.0 bias on X/Y), `04/00=0xA80005, 04/28=0xFD0000, 04/2F=0x582000, 04/48=0x3F`, `01/01=0xCE1540`, `01/53=0x010C0F`, shader word 9B and program word `0x00F04005`. Use that bundle (section 6.1) for anything that must match XP pixel for pixel.

### 5.3 Pointing the engine at a render target

```c
/* byte_base must be 256-aligned; pitch must be a multiple of 32 bytes. */
void set_target(uint32_t byte_base, uint32_t pitch_bytes, uint32_t fmt /*0x99=ARGB8888*/) {
  type(0x01);
  reg(0x50, byte_base >> 8);
  reg(0x51, 0);                                   /* linear, local framebuffer */
  reg(0x52, (fmt << 16) | (pitch_bytes / 32));
  reg(0x53, 0xC0F);
}
```

To draw into the visible desktop, retarget to the screen surface with its real pitch (at 640 wide, 32bpp the pitch is 2560, so `pitch/32 = 80`). Unlike 2D, 3D cannot start at an arbitrary byte: the target origin must land on a 256-byte boundary, so for an offset window (x0,y0) the proven trick is to put the *base* at the surface origin and let the vertices carry the offset (the depth base is set from `window_byte_offset` the same way: `reg(0x10, (DEPTH - (y0*256 + x0*4)) >> 8)` with depth pitch `(256/32) << 13`).

### 5.4 The draw packet

```c
/* One native triangle from three screen-space (x,y) pairs, constant colour. */
void tri(int x0,int y0,int x1,int y1,int x2,int y2, uint32_t mode /*0xCE02*/) {
  type(0x04); reg(0x23, mode); reg(0x24, 3);   /* primitive mode, vertex count */
  type(0x00);                                  /* vertex stream follows */
  vtx(x0,y0); vtx(x1,y1); vtx(x2,y2);
  idle();                                      /* section 1 */
}
void vtx(int x,int y) {
  /* the engine takes floats: x,y are sent as 2*coordinate half-bits for the
     proven screen path (see half_bits in 5.4) */
  wr(0x440, half_bits(2*x)); wr(0x440, half_bits(2*y));
  wr(0x440, 0x3F000000);  /* z = 0.5 */
  wr(0x440, 0x3F800000);  /* w = 1.0 */
  wr(0x440, 0xFFE04020);  /* colour dword (ignored by the constant-colour shader) */
}
```

`half_bits(n)` is the IEEE-754 single of `n/2` computed without the FPU (the Win98 build uses integer maths): a small normalisation loop, exponent `127+k`, mantissa shifted into place. Vertex count generates the end-of-primitive; there is no separate fire word. Mode `0xCE02` is the proven triangle, `0xCE04` the rectangle.

**Always `idle()` between draws and before touching the surface with the CPU.** Always drain a CPU framebuffer read of the last-written address before enabling GPU access to memory the CPU just filled (see `clear()` in the source: write, then read the final dword back).

## 6. 3D recipes

Each recipe assumes `init3d()` and `set_target()` from section 5 have run. Register values are the proven values from the lab's test matrix. The names in the headings (such as `sf_fog_tests`) are lab test-table names kept for traceability; you do not need them. Where a detail was not pinned down, the text says so.

### 6.1 Constant-colour primitive (proven XP bundle)

This is the baseline every other recipe modifies. Program word 00F04005 outputs the vertex colour (the vertex's colour dword, as the XP capture does). With 00F08005 in its place the shader outputs the bank-0 constant written after selector 22030000 (float R, G, B, A) and the vertex colour is ignored.

```c
void xp_bundle(void) {
  /* the program, while pixel shading is off: 11 setup words in 01/90-01/9A, then raw dwords */
  wr(0x43C, 0x22030000);                       /* bank-0 constant: 4 floats (R,G,B,A) */
  wr(0x440, f2bits(r)); wr(0x440, f2bits(g)); wr(0x440, f2bits(b)); wr(0x440, f2bits(a));
  wr(0x43C, 0x20030000);                       /* program: 4 words */
  wr(0x440, 0x01C00530); wr(0x440, 0x301C0053);
  wr(0x440, 0x00F04005);                       /* 00F04005: the vertex colour; 00F08005: the bank-0 constant */
  wr(0x440, 0x000D0820);
  type(0x01);
  reg(0x1E, 0); reg(0x1F, 0); reg(0x20, 0x7FFF); reg(0x29, 0);   /* captured clamp words */
  reg(0x82, 0x40); reg(0x83, 0x40);
  reg(0x53, 0x010C0F);
  reg(0x9B, 0xFFFFFF);
  reg(0x01, 0xCE1540);       /* enables: caches + perspective correction */
  type(0x04);
  reg(0x00, 0xA80005); reg(0x28, 0xFD0000); reg(0x2F, 0x582000); reg(0x48, 0x3F);
  reg(0x41,0x400000); reg(0x42,0x00493F); reg(0x44,0x400000); reg(0x45,0x00493F);
  reg(0x52, 0x147FFF);
}
/* then: tri(...) with mode 0xCE02, or a rectangle with 0xCE04 (3 vertices) */
```

Rectangle (mode `0xCE04`) takes three vertices: top-left, top-right, bottom-left; the hardware infers the fourth. Triangle fill rule is top-left. The rest of the bundle above is unchanged; only the primitive mode word differs from the triangle (`0xCE02`).

### 6.2 Gouraud shading (`sf_gr_tests`)

Same bundle, but the programme word is `0x00F04005` (outputs the interpolated vertex colour) and each vertex carries its own ARGB in the fifth dword instead of the dummy. Vertex colours stay packed `0xAARRGGBB`.

```c
void vtx_c(int x,int y,uint32_t argb){
  wr(0x440,half_bits(2*x)); wr(0x440,half_bits(2*y));
  wr(0x440,0x3F000000); wr(0x440,0x3F800000); wr(0x440,argb);
}
```

### 6.3 Alpha blending, alpha test, colour mask, ROP

```c
/* type 01 */
reg(0x00, 0x10);   /* blend enable */
reg(0x34, 0x020);  /* blend colour op/factors  (proven for src-alpha / inv-src-alpha) */
reg(0x35, 0x148);  /* blend alpha op/factors */
reg(0x36, 0); reg(0x37, 0);
/* ... draw ... then restore: */
reg(0x00,0); reg(0x34,0); reg(0x35,0); reg(0x36,0); reg(0x37,0);
```

`01/53` = `0xC0F` is COPY with all four channels written; changing the low nibble is the per-channel colour mask and bits above it select the logic op. Only COPY and the channel mask are proven; the logic-op encodings were not mapped. Blend is enabled by `01/00` = `0x10`. The `01/34` and `01/35` pairs proven on a 32bpp desktop window (source colour 0.5, 0.25, 0.75 over a prefilled background, ten cases in all): `0x020` and `0x148` = SRCALPHA / INVSRCALPHA; `0x150` and `0x154` = ONE / ONE (result is the sum, clamped at 255); `0x150` and `0x148` = ONE / INVSRCALPHA; `0x020` and `0x154` = SRCALPHA / ONE; `0x050` and `0x100` = ZERO / SRCCOLOR (destination times source). At 16bpp, SRCALPHA / INVSRCALPHA was run on RGB565 and ARGB4444 (6.8). Other factor encodings were not mapped.

### 6.4 Depth (Z) test and write

```c
reg(0x10, depth_base >> 8);          /* 256-aligned */
reg(0x11, (pitch_bytes/32) << 13);   /* linear Z pitch, cache off */
reg(0x12, 4UL << 16);                /* 32-bit fixed Z  (6<<16 for the stencil layout) */
reg(0x13, func << 16);               /* 0=NEVER 1=LESS 2=EQUAL 3=LEQUAL 4=GREATER 5=NOTEQUAL 6=GEQUAL 7=ALWAYS */
reg(0x01, base_enables | (zwrite ? 0xC : 0x8));   /* Z test on; bit 2 = write */
```

The Z buffer must be cleared by the CPU to `0xFFFFFFFF` first (then read back the last dword to drain). Vertex Z is a float in 0..1 (`0x3E800000` = .25, `0x3F000000` = .5, `0x3F400000` = .75).

### 6.5 Stencil

Switch the depth format to the stencil layout and enable it:

```c
reg(0x12, 6UL << 16);     /* depth+stencil packed */
reg(0x13, 1UL << 16);     /* LESS (proven) */
reg(0x01, base_enables | 0x1C);
type(0x01); reg(0x23,w1); reg(0x24,w2); reg(0x25,w1); reg(0x26,w2);   /* front/back stencil state */
/* restore after: */ reg(0x23,0); reg(0x24,0); reg(0x25,0); reg(0x26,0); reg(0x12,0);
```

`w1 = (ref<<16) | (mask<<8) | wmask` and `w2 = (func<<16) | (sfail<<6) | (zfail<<3) | zpass`, where `mask` is the compare mask, `wmask` the write mask, and `func` uses the same compare codes as the depth test (0 NEVER, 1 LESS, 2 EQUAL, 3 LEQUAL, 4 GREATER, 5 NOTEQUAL, 6 GEQUAL, 7 ALWAYS). The test is `(ref & mask) OP (stored & mask)`, so LESS passes when the reference is below the stored value. The three ops (`sfail`: stencil test fails; `zfail`: stencil passes, depth fails; `zpass`: both pass) use: 0 KEEP, 1 ZERO, 2 REPLACE with ref, 3 INCR clamped at 255, 4 DECR clamped at 0, 5 INVERT, 6 INCR wrapping, 7 DECR wrapping. Ten cases covered replace, equal, both increments and decrements (clamped and wrapping), invert, zero, the fail op, the z-fail op, a compare mask and a write mask. In this layout the Z word holds 24-bit Z in bits 31:8, but the engine does not keep the stencil in bits 7:0: it keeps it in a separate, tiled 8-bit plane that 01/28, 01/29 and 01/2A point at (section 17). The lab's ten cases passed only because the engine read back its own writes, with 28/29/2A left 0, so the plane was at video memory 0, on the visible screen.

### 6.6 Fog (`sf_fog_tests`)

Fog is a per-pixel blend toward the fog colour held in constant bank 0 (A forced to 1.0). Registers (type `01`): `01/00` bit 8 enables fog for render target 0; `01/70` is the mode (`0x12` = linear on Z, equation 0 is the proven form; bit 4 selects Z or W, bit 3 the equation); `01/71` and `01/72` hold the fog colour as 11-bit fixed point (1.0 = `0x400`); `01/73` is the start and `01/77` the end as 15-bit floats (the IEEE bits 30:16); `01/75` and `01/76` hold the mantissa and exponent of `1/(end - start)`. Linear fog was run with start/end of 0.25/0.75, 0/1, 0.5/1 and 0/0.5 (the factor clamps outside the range), with fog disabled as a control, and with red geometry over blue and green fog. Only linear mode on Z (`01/70` = `0x12`) is proven. Equation 1 (`0x1A`) and the W-based and local-fog modes were run as explorations and are not relied on; exp and exp2 were never run, so their encodings are unknown.

Fog accuracy. The hardware agrees with an integer model to within 5 of 255 on any channel: a maximum of 3 on most cases, 5 on start 0.5 / end 1, 0 with fog disabled, and a mean of about 1. Compare fog with a tolerance of about 6, not for exact equality. The differences fall on a one-in-three diagonal lattice, which looks like fixed-point interpolation steps, but the exact rounding rule was not worked out. All ten fog cases pass on that tolerance; the local-fog case is exploratory (no model, it differs by up to 255) and is not counted as proven.

### 6.7 Texturing (the XP-path steps; `kind==8` in `sf_2d_run`)

Six steps, in this order:

1. **Upload.** CPU-write the texels (linear, 256-byte aligned base), then read the last dword back to drain.
2. **General texture state:** `wr(0x43C,0xFE020000); reg(0x00,0x11); reg(0x01,0x21); reg(0x08,0x101); reg(0x04,0);`
3. **Texture 0 attributes:** `wr(0x43C,0x00020000); reg(0x00,base>>8); reg(0x18,0x1100); reg(0x20,(w<<12)|h); reg(0x21,1); reg(0x22,0x66); reg(0x30,fmt);` with `fmt` = `0x990000` ARGB8888 with alpha (proven), `0x130000` and `0xB80000`/`0xE80000` for the other tested formats, `0x980000` no-alpha.
4. **Sampler stage 0:** `wr(0x43C,0x20020000); reg(0x30,0); reg(0x31,filter|0x10000); reg(0x32,addr|0x80); reg(0x35,0); reg(0x36,0);` then the sampler block `wr(0x43C,0x10030000)` followed by eight zero dwords.
5. **Pixel shader (sample t0 → output):** `type(1); reg(0x01, enables & ~0x400); reg(0x90, 0x18000); reg(0x91,1); reg(0x92,0x10001); reg(0x93,1); reg(0x94..0x9A,0); reg(0x9B,0xFFFFFE);` then programme words: `wr(0x43C,0x21030000); wr(0x440,0x301001B0); wr(0x43C,0x20030000); wr(0x440,0x01C00530); wr(0x440,0x301C0053); wr(0x440,0x00F20005); wr(0x440,0x000D0820);`
6. **Draw** with the flexible vertex format: `type(4); reg(0x00,7); reg(0x01,0x020100); reg(0x02,0x080503); reg(0x03,0x09); reg(0x25,0xF4C000); reg(0x28,0xFDC000);` then each vertex is x, y, z, w, colour, **u, v** (7 dwords). Restore the old vertex layout, blend off, and the constant shader afterwards.

UV encoding for the proven screen-space path is the absolute screen position divided by 2^exp (`sf_scaled_bits`), so a 64×64 texture sampled 1:1 puts its origin at the window origin. Texture rotation (section 4C) is only a change of which UV each corner gets.

Filter: `fl = (1<<13)|(1<<10)|(1<<7)|(1<<4)` enables bilinear; 0 is point sampling. Blend with texture alpha is 6.3 around the same draw.

### 6.8 16bpp render targets and dither

Set `01/52`'s format byte to the 16bpp code and halve the pitch in bytes accordingly. Format codes used: `0x89` RGB565, `0x88` RGB555, `0x8A` ARGB1555, `0x8B` ARGB4444, and `0x8D` RGB565 with the 2x2 ordered dither switched on (the pixels are ordinary RGB565; there is no red/blue swap). Ten flat-colour and blend cases ran: (0.5, 0.25, 0.75, 1.0) in all four formats, other colours (0.2, 0.6, 0.9) and mid-grey (0.5, 0.5, 0.5) in RGB565, alpha 0 and 1 in ARGB1555, and SRCALPHA / INVSRCALPHA at alpha 0.5 on RGB565 and ARGB4444. Run the mid-grey case first as a rounding probe against your own model, because per-channel rounding when narrowing to 5 or 6 bits is where a model and the hardware can differ. The `01/00` bit for dither is one dither control; the 0x8D format code and the 01/53 mode, described next, also decide whether pixels are dithered.

Dither, proved by the suite. 01/53 bits 16:15 choose how a colour is narrowed to the target's bits: 00 = table times max (the power-on value 0xC0F), 10 = round times max (XP writes 0x010C0F). In table mode, format 0x8D turns on a 2x2 ordered dither; 0x88, 0x89, 0x8A and 0x8B do not dither. In round mode 0x8D gives plain rounded RGB565, the same as 0x89. The dither adds a threshold T before truncating: out = floor(f \* max + T / 2^(8 - bits)), where max is 2^bits - 1 and T = {{0,4},{6,2}}\[y&1\]\[x&1\] in 8-bit LSB units (x and y counted from an even origin; the lab window at 448,320 is even). For (0.5, 0.25, 0.75) in RGB565 the four pixels are 79F7 (even x, even y), 8217 (odd x, even y), 8238 (even x, odd y) and 7A17 (odd x, odd y), and all 2304 pixels of the 48x48 rectangle matched. The same colour without dither is a flat 79F7 in table mode and a flat 8217 in round mode.

On the Direct3D HAL (board, v10.202-v10.205) the mode alone did not dither: 01/00 bit 0, the dither enable for render target 0 (HCM p84), must be set too. With bit 0, format 0x89 dithers in table mode; without it neither 0x89 nor 0x8D did. Details in 16.13.

### 6.9 The CPU-scan fill alternative (proven, used by the Win98 build)

Where a flat fill is all that is needed, the hardware rectangle path is avoided and the CPU walks the edges (`vx855_fill.h`), emitting one **type-0 point** per run of identical rows:

```c
/* one span [xs, xs+w) x [y, y+h) as a point: size and centre as half_bits floats */
type(0x04); reg(0x2A, half_bits(2*w) >> 8); reg(0x2B, half_bits(2*h) >> 8); reg(0x24, 1);
type(0x00);
wr(0x440, half_bits(2*xs + w)); wr(0x440, half_bits(2*y + h));   /* x, y: the span's centre */
wr(0x440, 0x3F000000); wr(0x440, 0x3F800000); wr(0x440, colour); /* z 0.5, w 1.0, ARGB */
idle();
```

This is what makes triangle and rectangle fills pixel-exact against XP (cases 16–25 of the matrix). It is also the safest fallback if a hardware-rasterised edge is ever off by a pixel.

### 6.10 The automated suite (VX3SUITE.EXE) and what it proves

One program runs every proven function and checks it: v10.11 on the Wyse Cx0 with the 512 MB setting gave 198 tests, 198 PASS, 0 FAIL, 0 HANG. It writes VX3SUITE.TXT (a line per test), VX3SUITE.BMP (the window) and VX3SUITE\_2D.BMP (before, after and difference for the 2D cases), and `/auto` runs without clicking. 2D tests draw into the 64x64 window at 448,320 and compare every pixel with a software model of the recipe; 3D tests use the driver's own per-case measurement and pass only with zero errors (fog uses the tolerance in 6.6).

Coverage that passed, by group: system 4 (driver found, 640x480x32, engine ready, video memory reach across the whole 512 MiB), 2D fills 6, copy and clip 3, host rotate 6, rotated text 12, pattern and line 8, keys and text 4, host data 5, 3D texture 10, 16bpp render targets 20 (the plain and the XP 01/53 variants), depth 20, Gouraud 10, stencil 10, fog 10, texture rotate 30, blend and alpha 10, 3D reach 30 (render target, depth buffer and texture each at ten offsets up to 511 MiB).

Not covered: mono expansion from screen memory, fixed-colour fill with destination key, screen-to-screen rotate, CBU rotate windows and 16bpp host rotate. Also not run: vertex buffers fetched from video memory, 16bpp targets at high addresses, and 3D at the 64, 128 and 256 MB BIOS settings. Three problems appeared while building it, and none was a hardware fault. Two were wrong expectations: format 0x8D was assumed to swap red and blue (it dithers, see 6.8), and fog was held to a tolerance of 4 when the hardware is within 5 (see 6.6). A third was a bug in the test code itself: a loop that counted distinct pixel values kept reading past an 8-entry array on the ring-0 stack and page-faulted (Windows fatal exception 0E in the VxD) only when the stack happened to sit near a page edge. Bound every scan in ring-0 measurement code and keep large tables in static memory, not on the stack.

If the machine dies mid-run, the suite writes VX3SUITE.TXT before every driver call, with an UNFINISHED line naming the call that was in flight, so the file shows where it stopped. After a fault the display can stay frozen while the network still answers: the engine is wedged, and only a hard reset clears it.

## 7. What not to build

Every item here hung the engine on the Win98 board (or returned nothing usable) and was closed after systematic variation. A driver must route around them with the CPU or one of the proven forms in sections 3–4. Each has a full write-up with its attempts and results in section 9.

| Function | Result | Use instead |
| --- | --- | --- |
| Mono expansion with the **source in screen memory** (op 1 with bit 8 and a screen-memory source) | No correct output in about 70 variants (v9.87 to v10.00). Most hang with the same state every time: status 2, reason 2, reg 0C = 0x0007001F, reg 10 = box start. Feeding host data to the same commands sometimes completes, but with scrambled output that depends on history and timing. | Host-fed mono (`CC0A0141`), or CPU-expand the bitmap to colour and use host-fed colour (`CC002041`). |
| Fixed-colour (bit 13) **with** destination key | Hangs. | Dest-key fill via `F0000A01` (form A) or a dest-key copy from a solid surface (form B), section 3. |
| **Screen-to-screen rotate** (op 9 with a screen source) | Hangs. | Host-fed rotate (section 4A/4B), or the 3D textured rotation (section 4C). |
| Rotation angle 0/180 with some decrement-bit combinations (including mono text op A at mode 302 without X+Y dec) | Hangs. | Use the exact table in section 4A (90 = X dec, 270 = Y dec, 180 = X+Y dec) and nothing else. |
| **CBU rotate windows** (BAR1 0x1E00–0x1FFF) | Registers read back as written (real reads, upper bits included), but no effect was found on 2D writes or reads, on CPU framebuffer access, or on a screen-to-screen copy with the XP whole-screen window at base 0 (tests v9.64 to v9.92). Role unknown. | Do not program them. Do not rely on them for rotation. Save and restore the +00 control if a test must touch them. |

Two rules that fall out of this:

- A hang is recovered only by the sequences in section 1 (2D: SR4E bits 3:2; 3D: SR4E bits 5:4, then pulse SR1A bit 6). Never probe a new command word in a driver that has no recovery path.
- Reserved or write-only bits are not an invitation. A command word that decodes plausibly in the table (section 2) is *not* evidence it works: the mono-text-rotate word decoded cleanly and still hung until the decrement bits and start corner were right (section 4D).

## 8. Wiring it into a Win98 display driver

**Status of this section:** the native driver described below is built and running on the Wyse board (first at v10.65; v10.353 now): VX85598.DRV (16-bit), VX85598.VXD, VXDDRAW.DLL (the 32-bit DirectDraw and Direct3D HAL), VXGL.DLL (the OpenGL ICD) and VX85598.INF. Accelerated GDI fills and copies, DirectDraw blits with colour key, flip chains and vertical blank are proven on hardware at 640x480 to 1024x768, 8, 16 and 32 bits. Where something is untested it says so.

### 8.1 What the driver is today

The board runs a native driver built on the VMDisp9x skeleton, with no shadow framebuffer. The 16-bit DRV hands mode setting to the VxD; the VxD owns the 2D engine, the display start address and EDID; a 32-bit HAL DLL, loaded by DDHELP, gives DirectDraw hardware blits, colour key, page flips, vertical blank and overlays, and is the Direct3D HAL (sections 15-17); VXGL.DLL is the OpenGL ICD (sections 18-19). The VxD also runs the hardware cursor, gamma and SaveScreenBitmap. Every acceleration is on by default (the exceptions are listed in section 12: ACCEL\_DASH, ACCEL\_CR3, ACCEL\_3D\_COARSEZ, ACCEL\_D3D\_STATE\_DIFF, ACCEL\_D3D\_BATCH\_END and the diagnostics), each with a registry DWORD under `HKLM\Software\VX85598` that turns it off.

### 8.2 The order that worked

1. Boot and mode setting in the VxD, with the mode list filtered by the monitor's EDID (800x600 when there is no EDID; anything above 1024x768 stays hidden without one unless a registry key allows it).
2. GDI hooks for the proven forms: fills, screen-to-screen copies with overlap handling, the ROP fills, mono expansion, two-colour and multicolour 8x8 brushes, hatches, 1-bit bitmap brushes, solid brushes (including PATINVERT at 16bpp), every ROP3, the ExtTextOut opaque rectangle and solid one-pixel pen lines. Added later (v10.105 to v10.138, 8.5 and 9.7): ROP blits from memory bitmaps and DIB sections through a staging block, filled shapes as scan bands, SaveScreenBitmap, the hardware cursor, the gamma ramp, and ROPs that read the screen at any size. Everything else falls through to the DIB engine unchanged.
3. The DirectDraw HAL: same-size video-to-video blits on the engine (everything else on the CPU), source and destination colour key, page flips and vertical blank; since v10.106 the video overlay (YUY2, YV12/I420, RGB, shrink, keys, bob, over 8, 16 and 32bpp desktops) and since v10.123 the primary gamma ramp.
4. 3D last, only through a HAL-only Direct3D layer built from sections 5 and 6. Built: the Direct3D HAL (sections 15-17) and the OpenGL ICD (section 18).

### 8.3 Rules the driver must obey

- **Idle before every CPU framebuffer access.** After any accelerated operation, the driver must poll idle (section 1) before the DIB engine or the CPU touches the destination, and before the next op reads a surface the CPU just wrote.
- **Always set the full prologue** (section 2 `prologue()`) per operation. The engine keeps state between commands; skipping registers inherited from the previous op was the source of several early false results.
- **Never write host data to 0x100–0x1FF**; host data goes only to the 0x200000 window.
- **Cap the supported surfaces.** The engine is used on the primary surface and on video-memory DirectDraw surfaces at 8, 16 or 32 bits per pixel (reg 04 bits 9:8: 0 = 8bpp, 1 = 16bpp, 3 = 32bpp). 8bpp is supported from v10.71, and the engine does 8bpp fills, copies, colour blocks and lines (v10.76 to v10.90, see 8.5). 16bpp rotation has the extra row-padding rules from section 4B.
- **Bind the DRV's words by selector:offset, never by a linear address.** The VxD reads the DRV's wEnabled and the PDEVICE's deFlags. A 16-bit global block keeps its selector but can move in linear memory, and after an OpenGL run the VxD read 0 at the old address and declined every GDI request (reason 0204) until a restart. The DRV passes 16:16 pointers; the VxD maps each selector's current base with \_SelectorMapFlat (system VM) at every entry (v10.274; control query FFFFFE1C counts the moves).
- **Keep a hang watchdog.** Idle timeout → the recovery sequence from section 1 → repaint the affected rectangle with the DIB engine. A driver that cannot recover is worse than no acceleration.

### 8.4 Not yet proven — do not assume

| Question | State |
| --- | --- |
| Batching several ops and waiting once (instead of idle after each) | Proven (v10.132 to v10.138, VX2OPT, VX2OPT2, VX2TUNE): one idle read after a single operation, and no wait at all between the rectangles of a band batch, gave identical pixels and cut about 8 us per command; see 9.7. |
| Interleaving 2D ops with CPU framebuffer writes (the real GDI pattern) | Proven at 32bpp on the desktop (VX2SYNC, 800x600): random sequences of CPU writes interleaved with engine fills and copies matched software drawing exactly (489 fills and 260 copies in one run, zero mismatches), including copies in every overlap direction. 16bpp is proven too (v10.67g, VX2PROOF at 800x600x16, both parts, zero mismatches). |
| Throughput of 3D textured rotation versus the CPU | Not timed. Only host-fed 2D rotation was timed (about 50 MB/s, bus-bound, far below the 540 to 2000 MB/s the CPU writes to the framebuffer, see 8.5). |
| 3D engine used from a Win98 D3D HAL | Built and proven (sections 15-18). |
| Role of the CBU windows | Unknown. |
| Rotated mono text at 8bpp, glyph heights other than 8, widths over 33 | Not tested. Widths 5 to 33 by height 8, 32bpp and 16bpp, all four angles are proven (4D). |
| GDI engine path at 16bpp | Proven at 800x600x16 (v10.67g, VX2PROOF stage A, 8.5). Colour and hatch brushes with more than two colours used the DIB engine until v10.77; they are on the engine now (8.5). |
| 8bpp (GDI or DirectDraw) | Supported from v10.71 for 640x480, 800x600 and 1024x768 (GDI on the DIB engine, DirectDraw with palettes, blits on the CPU). Since v10.76 the 2D engine also does fills and same-size copies at 8bpp (depth code 0x000, measured in v10.75); stretch, mirror and system-memory blits stay on the CPU. |
| DirectDraw overlays, GetScanLine, lost-surface recovery, engine stretch | Overlays built and proven (v10.106 to v10.115, 8.5); GetScanLine built (v10.73, registry SCANLINE, on by default from v10.263); lost-engine recovery built (v10.75), and lost surfaces restore after Alt-Tab (VX2SWAP, v10.123). Engine stretch not built: stretch and mirror run on the CPU. |
| GDI paths still on DIBENG (v10.90, updated v10.138) | StretchBlt (not same-size), Pixel, ScanLR, FastBorder, Strblt, glyph drawing, dashed pens, wide R2\_COPYPEN pens and COPYPEN shapes, line R2 modes other than COPYPEN, BLACK and NOTCOPYPEN, alpha blending; StretchDIBits and DibToDevice only for the classes where DIBENG is as fast. Stretch, mirror and alpha need the 3D engine. The bitmap uploads (DibToDevice, StretchDIBits) are limited by how fast the CPU writes across the bus, which a copy engine does not change; the CPU conversion paths are under "DIB conversion and stretch on the CPU". |

Accelerated 2D is on by default. Anything a recipe above does not cover falls back to the DIB engine (GDI) or to the HAL's own CPU path (DirectDraw), so a declined operation still draws.

### 8.5 Measured 19 to 20 Oct

These come from the native 2D driver (v10.60 to v10.138) on the Wyse board, 640x480, 800x600 and 1024x768 at 8, 16 and 32 bits (also 1280x720 and 1280x1024 at 16 and 32). They add to 8.3 and 8.4.

| Finding | Detail |
| --- | --- |
| DirectDraw strips flip flags the HAL does not advertise | Set DDCAPS2\_FLIPNOVSYNC (0x00400000) and DDCAPS2\_FLIPINTERVAL (0x00200000) in dwCaps2, or DDFLIP\_NOVSYNC and the interval flags never reach the driver. With them set a no-vsync flip took 1.0 ms. |
| Page flip | Move the display start through CR0D, CR0C, CR48 and CR34 (IGA1). The address unit is 2 bytes (byte offset >> 1), so no BIOS calibration is needed. Vertical blank measured 60.2 Hz. Primary at offset 0, back buffers one frame apart. The write order matters; see the CR34 row below. |
| CPU reads of video memory are slow | About 30 MB/s with dword loads, 61 MB/s with 64-bit MMX loads (VX2READ, see the rows at the end of this table). A stretch that reads each source pixel several times from video memory took 43 ms; copying the source rectangle to system memory first, then stretching, took 21.9 ms. |
| Mode switches | 640x480, 800x600 and 1024x768 at 16 and 32 bits switched through DirectDraw SetDisplayMode with working blits and flip chains. 8bpp works from v10.71 (see the 8bpp rows at the end of this table); the original runs covered 16 and 32 bits only. |
| GDI hook | Screen-to-screen copies in all overlap directions, fills, the ROP fills (BLACKNESS, WHITENESS, DSTINVERT, PATINVERT) and mono expansion matched software. Windows 9x realises a brush's origin when it is selected: set the origin with SetBrushOrgEx (and UnrealizeObject) before SelectObject, or the origin is ignored. |
| Mono pattern brushes | The GDI hook sends two-colour 8x8 brushes to the mono pattern fill with the row rotation above. Colour brushes stayed with the DIB engine until v10.77 (hidden Y state, 4D); the colour block fill in this table replaced that. |
| Solved: display start shifted after a mode change | Colour bars appeared wrapped and shifted after a flip chain followed by a mode set, although every readable register held the right value. Cause: CR34 was written second. See the CR34 row below. Fixed in v10.63 and confirmed on a real monitor. |
| Open: SetDisplayMode hang | One exclusive-mode SetDisplayMode(640,480,32) hung and corrupted the desktop; the same call worked in other runs. Not seen since v10.65. The DDHELP crash described below was a memory overrun in the 16-bit driver and may have been the cause, but that was not proven. |
| Probably fixed (v10.73): GDI acceleration switched off | In one boot the driver's enabled flag was 0, so every GDI draw fell back to the DIB engine, while DirectDraw blits still used the engine. A fresh boot did not show it. Probable cause, found in the source in v10.73 and not reproduced: RestoreDesktopMode (the routine the display driver runs when the system VM comes back) cleared the BUSY flag but never set the enabled word again; only SwitchToFgnd did. It now sets the word to 1 at the end. A diagnostic reports the binding (bound, enabled word, BUSY flag, requests that arrived while disabled), and VX2AUTO fails a desktop mode if GDI is bound but switched off. Across the 9 desktop modes of the v10.73 to v10.76 runs the enabled word was always set and no request arrived while it was 0. |
| Fixed: GDI pattern brushes | In the fresh-boot VX2SYNC run, brush tests drew no engine commands while fills and copies did. v10.61 added the mono pattern brush hook (see the Mono pattern brushes row) and brushes now drive the engine at 32bpp. 16bpp pattern and solid fills are hardware-tested since v10.67g; colour and hatch brushes with more than two colours went to the DIB engine until v10.77 (8.5 has the colour block fill that replaced that). |
| CR34 commits the display start | On these VIA chips the whole 26-bit start address is latched when CR34 is written. Write CR0D, CR0C, CR48 (mask 0x1F), then CR34 last. With CR34 second, the registers read back right but the chip scanned out an address built from stale low bytes. After every mode set commit start 0 the same way, and when a flip chain ends. Extended CRTC registers need SR10 bit 0 open while you write them. |
| DirectDraw HAL lifecycle | DDHELP loads VXDDRAW.DLL and calls DriverInit; that is normal, and the process named in a crash dialog is only the one running when the fault hit. A callback can run in a process that never initialised the DLL's private data, so each one re-binds from the shared context, and the handle to the VxD is per process (reopen when the process id changes). A process that moved the display start resets it when it leaves exclusive mode and when the DLL detaches. |
| Crash after about 21 mode sets (fixed) | DDHELP faulted inside the 16-bit DRV. The cause was a 480-byte trace buffer in the DRV's data segment that kept appending value digits after it was full, so each further DirectDraw mode set wrote past the end of the segment. GDI-only mode cycling never hit it. How it was found: the Details box (shown after Close on the Ignore/Close dialog) gives the module and offset; the linker map names the function and the faulting instruction's bytes show the store address. Fixed in v10.65 by removing the trace buffer, and confirmed on hardware: the full 12-card VX2CONF run now finishes. Rule: the 16-bit driver has one 64 KB data segment, so keep no unchecked buffers in it. |
| Capped logs mislead | A HAL log that stopped after 400 events made it look as if later calls never reached the driver, which pointed the search the wrong way. Use a rolling log while debugging and ship with none: v10.65 does no file logging. |
| 16bpp brush layout (measured, v10.67) | At 16bpp DIBENG realizes a brush with depth byte 20, not 16 (32 at 32bpp). Flags 0x80 = dithered colour brush: BrushBits hold the 8x8 converted RGB565 pixels. Flags 0x81 = solid (COLORSOLID): dp32FgColor holds the raw COLORREF (0x00BBGGRR), and BrushBits\[0\] and \[1\] hold the converted physical pixel; only those first two words are filled. At 32bpp the colour field is the raw COLORREF and the driver swaps red and blue. |
| 16bpp colour conversion | DIBENG converts a COLORREF to RGB565 by scaling each channel, (r\*31/255)<<11 \| (g\*63/255)<<5 \| b\*31/255, not by shifting: 00682070 is 68EC, not 710D. Measured on 32 solid colours: the scaled value equals DIB\_ColorInfo and the pixel actually drawn. Dithered brushes do not follow the formula (00881C08 draws 08D0, the formula gives 00D0). |
| Never hand the engine a raw brush colour | Using the low word of dp32FgColor as RGB565 painted the desktop grey red and teal maroon after a 32 to 16 bit mode switch, and gave PATINVERT 2475 wrong pixels. Rule: accept only depth byte 20 at 16bpp and 32 at 32bpp; for a COLORSOLID brush at 16bpp use BrushBits\[0\] (decline if \[0\] and \[1\] differ); two-colour dithered brushes use the mono pattern fill; everything else goes to the DIB engine. Fixed in v10.67g and confirmed on hardware. |
| GDI engine path at 16bpp (proven) | VX2PROOF stage A at 800x600x16, v10.67g, in two parts (conservative copies, every overlap direction): fills, BLACKNESS, WHITENESS, DSTINVERT, PATINVERT, 24 copy offsets, mixed sequence, brushes and clipped GDI gave 0 failing tests and 0 engine timeouts. PATINVERT with a solid brush runs on the engine with the right pixels (colour 00682070, the case that failed: engine ops +1, 0 mismatches). The DirectDraw soak (40 mode changes with flip chains) also passed. |
| Proving an op ran on the hardware | Compare the pixels with a software reference and also read the engine's op counter before and after each draw: a pass with no engine ops only proves the DIB fallback is right. To see what DIBENG passes to the driver, a diagnostic DRV records every brush fill and returns it through private escape 0x8F71 (use ExtEscape with an output size; Escape drops the output buffer). That source is kept in diagnostics/brush\_trace, not in the release. |
| Debug printing costs time | The 16-bit DRV had debug printing compiled in with no output (COM0), so every Control() call still formatted a string. v10.67g builds it without. The VxD keeps its in-memory log; its per-event calls are commented out. |
| CPU speed to video memory (VX2READ, v10.69) | The framebuffer mapping is write-combining. CPU writes: rep stosd fill 1943 MB/s, rep movsd copy from system memory 542 MB/s, MMX 64-bit copy 487 MB/s (MMX does not help writes). CPU reads: dword loads 30 MB/s, rep movsd 32 MB/s, MMX 64-bit loads 61 MB/s. Two runs on the Wyse board agreed within 2%. This replaces the earlier claim that the CPU writes about 50 MB/s: that figure was the host-fed engine path, not the CPU. |
| Cached video memory is not usable (v10.69) | Switching the video memory MTRR to write-back reads at 340 to 521 MB/s (8.5 times the best write-combining read) but the CPU cache goes stale against the 2D engine in both directions: after an engine fill the CPU still saw old data in 4640 to 5584 of 51200 dwords, and an engine copy started right after CPU writes read old data (only 7168 to 7488 of 51200 dwords held the CPU's data). Writes at write-back are slower too (fill 720 to 792 MB/s against 1943). Cache type 3 (hardware coherent) is refused by the MTRR service with status C000000D. At the write-combining setting both coherence tests are exact (51200 of 51200 dwords). Keep write-combining and read with MMX. |
| MMX reads in the DirectDraw HAL (v10.70) | Copies out of video memory use 64-bit MMX loads in 32-byte blocks (only when CPUID reports MMX, EMMS at the end), then rep movsd, then a byte tail, which doubles the read speed to 61 MB/s. Used for the forward unkeyed rectangle copy and for the stretch source read-back. Backward (overlapping) and colour-keyed copies still use the plain path. |
| 8bpp modes and the engine (v10.71) | VBE modes 0x101, 0x103 and 0x105 (640x480, 800x600, 1024x768) are memory model 4 (packed pixel) with no colour masks. GDI runs on DIBENG in software, and in v10.71 to v10.75 the 2D engine was not used at 8bpp (engine requests were declined and the HAL finished blits on the CPU with 1-byte pixels; from v10.76 the engine does fills and same-size copies, see the 8bpp engine rows at the end of this table) (fill colours and colour keys are palette indices). Windows offers a depth only when it is listed under MODES\\8\\w,h in the INF or the registry; VX8BPP.REG adds those keys to an existing install. The engine's 8bpp colour depth code in register 4 was measured in v10.75 (below). |
| 8bpp palettes and the DAC (v10.71) | Write the DAC with interrupts off: PEL mask 0x3C6 = 0xFF, start index on 0x3C8, then three bytes per entry on 0x3C9. Components are 6 bits unless the BIOS DAC is 8-bit; this board's DAC is 8 bits (measured). To read, set the index on 0x3C7, not 0x3C8. Keep a shadow copy of the GDI palette and write it back when a DirectDraw palette is detached or destroyed, when exclusive mode ends, and at process exit. The HAL's palette callbacks decline outside 8bpp because DriverInit can run once and a game may switch to 8bpp later. The software cursor draws palette indices at 8bpp. The DAC read-back (v10.72) returns the bits per component and all 256 raw entries, so a test can compare them with what DirectDraw or GDI set. |
| Unattended test (VX2AUTO, v10.72) | Run on the Wyse board with no clicks: 13 DirectDraw modes (blits, GDI checks, flip chain, and at 8bpp the DAC compared after palette attach, SetEntries, detach and re-attach) and 9 desktop switches through ChangeDisplaySettings (driver status, GetDeviceCaps, solid fills, PATINVERT twice, overlapping screen copies in both directions, system palette against the DAC). Result: 0 failed checks, all 256 DAC entries matched at all three 8bpp sizes. The v10.72 cursor probe read the screen with GetPixel, which cannot see the software cursor, so it always warned. Since v10.74 it checks the cursor state (position, arrow shape, show count), moves the cursor across the screen for a visual check, and reports the pixels as not verifiable; the cursor itself still has to be judged by eye. v10.74 added a surface wider than the screen to every mode, v10.73 a video memory capacity test and a scan line counter sample, v10.76 an engine-operation check at 8bpp (v1.3). In a 3-3-2 test palette, grey RGB(40,40,40) becomes olive because blue has only 2 bits; that is the test, not the driver. |
| Up to 64 MiB of video memory through DirectDraw (v10.73) | VX\_FB\_LIMIT was 16 MiB and is 64 MiB now. The size handed to DirectDraw, the modes and the 2D engine is the larger of the VBE TotalMemory and the size decoded from host-bridge register 0xA1 (4 MiB << n), at most 64 MiB; the mapping is up to VX\_FB\_MAP\_LIMIT (512 MiB). The MTRR is left alone, so the BIOS write-combining range stays. Registry VIDMEM\_MB (0 = all, minimum 8) lowers it. Measured on the Wyse board (VX2AUTO, v10.74): 64 MB handed to DirectDraw and 128 MB mapped, DirectDraw reports 64336 KB; 62 offscreen surfaces of 1024 KB (62 MB) were created, each filled with its own colour and read back with 0 wrong pixels, spanning about 61 MB of the frame buffer, and the next CreateSurface failed with out of video memory. An engine fill, a copy between two surfaces about 61 MB apart and a copy from the lowest to the highest surface each reached the engine and gave the right pixels. |
| Surfaces wider than the screen (v10.74) | Without DDCAPS2\_WIDESURFACES (0x00001000) in dwCaps2, DirectDraw refuses an offscreen surface wider than the display with E\_INVALIDARG (80070057) before the HAL is called: 1024x256 at 640x480x32 failed while 640, 512, 256 and 128 wide worked. Set the flag after every other dwCaps2 assignment (the 3D caps assign the field with =). The HAL's own limit stays: pitch at most 0x3FF8 bytes. With the flag, a surface 1.5 times the screen width (960 wide on 640, up to 1920 wide on 1280) passed fill and readback on both sides of the screen width and blits out of and into its right-hand part, at 8, 16 and 32bpp in all 13 modes. |
| Scan line counter and GetScanLine (v10.73) | The register window (BAR1) offset 0x1290 holds the scan line in bits 31:16. 2000 samples in 18 ms saw lines 0 to 767 of a 768-line mode (764 distinct values), one step backwards per frame, and 58 samples past the last visible line, all inside the vertical blank. The status port's blank bit covers only part of the blank interval, so a counter past the last visible line is also reported as blank (DDERR\_VERTICALBLANKINPROGRESS). The HAL offers GetScanLine only when the registry value SCANLINE is 1 (the default from v10.263; 0 before) and the counter answers; the callback adds one pointer to the shared HAL structure, so the DRV, VxD and DLL must come from the same build. |
| Lost 2D engine and recovery (v10.75) | The engine is lost when a command timed out and the reset did not bring it back. It used to stay lost until reboot: every DirectDraw call returned DDERR\_SURFACELOST, GDI stayed fenced (BUSY flag set, enabled word 0), and the display driver's Enable failed because the VxD's bind returned LOST. Now one reset is tried per successful mode set, also when a CPU access request arrives and at Enable. If the engine answers, the fences are removed and the next operation starts it again; if not, it stays lost. Enable succeeds either way (a still-lost engine means DIB engine, no acceleration). An engine switched off after a timeout whose reset worked is initialised again after the next mode set. The HAL's CPU blit paths (stretch, mirror, system memory) refuse with SURFACELOST while the engine is lost, because it may still be writing to the frame buffer. Tested against the engine model only; a real hang has not been forced. Diagnostic: 2D control 0xFFFFFE11 returns recoveries, retries, state and the fence flag. |
| 8bpp engine depth code (VX2D8, v10.75) | Method: with the desktop at 640x480x32, the 2D lab draws into its 64x64 dword window with register 04 set to a candidate code and the window is read back as bytes. Point the destination and source bases (registers 14 and 1C, units of 8 bytes) at the window so row 0 of the operation is window row 0, and give coordinates in pixels of the depth under test. Result: code 0x000 writes 1 byte per pixel, 0x100 two, 0x300 four, and 0x200 behaves as 32bpp. At 0x000, 19 of 19 checks matched a model over the whole window: fills of odd sizes and starts, ROP fills 00, FF, 55, 5A and F0, copies in every overlap direction, source and destination colour key with an 8-bit compare, and clipping. The same suites at 0x100 and 0x300 ran as controls in the same run and passed. The pitch registers and base units are the same as at 16 and 32bpp. |
| 8bpp engine acceleration (v10.76) | The VxD accepts 8bpp fills and copies: register 04 = 0x000, colour mask 0xFF, colours and keys are palette indices. Mono and colour pattern operations stayed off at 8bpp in this build (not measured then); colour blocks, hatches and 1-bit bitmap brushes at 8bpp followed in v10.77 to v10.89. Registry ACCEL\_8BPP (default 1) turns it off. GDI: same-size screen copies, BLACKNESS, WHITENESS, DSTINVERT, and PATCOPY and PATINVERT with a solid brush. For a solid brush (COLORSOLID in dp8BrushFlags) the driver requires all 64 bytes of dp8BrushBits to be equal and sends that byte as the colour, as at 16bpp; the depth byte of an 8bpp brush is not checked (not measured), and anything else stays with DIBENG. DirectDraw: same-size video copies, fills and colour keys use the engine; stretch, mirror and system memory stay on the CPU. Hardware result (VX2AUTO v1.3): all 13 DirectDraw modes and 9 desktop modes passed, solid fills had 0 wrong pixels at 640x480, 800x600 and 1024x768 at 8bpp, and 2 engine operations were counted during the GDI tests in each 8bpp desktop mode. |
| Colour pattern fill from a block in video memory (v10.77, VX2COL) | Command F0000001 reads an 8x8 block from video memory (ROP F0, or 5A for PATINVERT); register 20 = block address >> 3 \| (y&7) << 29 \| (x&7) << 26. The block is row-major, 16-byte aligned and read from the rectangle's top-left corner, and the offset bits add to the block column and row. The driver keeps a 4 KB block at the very end of video memory, outside what DirectDraw is given, and writes the brush's 64 realized pixels there before each fill; offsets (dx&7, dy&7) reproduce DIBENG's screen-aligned brush. This replaced the colour pattern RAM and its hidden row counter. At 8bpp the row within a pair swaps for odd block columns; with the x offset set to x0&7 that term is zero. |
| Hatch brushes (v10.78 to v10.82) | DIBENG hands the driver realized pixels: the hatch colour on the lines and the background colour elsewhere, brush origin already applied. In TRANSPARENT mode it paints the background colour too (the tile never shows the destination), so a hatch is drawn like an opaque brush in every mode. The engine's own transparent mono fill (command bit 16, F0010A01) works (VX2TRN: 1 pixels take the foreground, 0 pixels are left alone, nothing outside) but is not what DIBENG does for hatches. BS\_HOLLOW and hatches without a valid mask go to DIBENG. |
| Every ROP3 on the engine (v10.84, VX2ROP) | All 256 ROP3 codes were run at 32, 16 and 8bpp against a software ROP3. Copies: a ROP that uses the source and not the pattern is the plain copy (rop\|0001); one that uses both takes the pattern as a fixed colour (command bit 13, registers 4C/50 beside 58/5C). Fills and pattern fills (mono and colour block): every ROP that does not use the source. Colour keys stay with SRCCOPY. D (AA) draws nothing. A source that is not the screen, a ROP code with extra high bits, or a non-solid brush with a pattern+source ROP go to DIBENG; hatches and 1-bit bitmap brushes use only PATCOPY and PATINVERT on the engine. The DirectDraw HAL does the same for DDBLT\_ROP between video surfaces (source ROPs as copies, BLACKNESS, WHITENESS and DSTINVERT as fills). |
| ExtTextOut opaque rectangle (v10.85, v10.86) | GDI sends the opaque rectangle with options 0x0100, not ETO\_OPAQUE 0x0002; treat both as opaque. The rectangle (cut by the clip rectangle and the screen) is an engine fill with the draw mode's background colour, then DIBENG draws the text. Pass GDI's original options (0x0100) on with an empty rectangle, not cleared options: with cleared options DIBENG used a different text routine and about 500 glyph pixels per case differed. |
| Solid brushes at 8bpp (v10.87, v10.88) | A true solid 8bpp brush (flags 0x81) has the palette index in its first four realized bytes and stale bytes after them; the dithered colour (flags 0x80) fills all 64. Require only the first four bytes equal and send that byte as the colour. The earlier rule (all 64 bytes equal) declined every true solid brush. |
| 1-bit bitmap brush, transparent mode (v10.89) | DIBENG paints the text colour for 0 bits and the background colour for 1 bits in TRANSPARENT mode as well, so the engine's opaque mono pattern fill (16 and 32bpp) or colour block (8bpp) gives exactly its pixels. Hardware: 24 of 24 fills on the engine, 0 differences at all nine desktop modes (VX2AUTO v2.7). |
| GDI lines on the engine (v10.90, VX2LINE) | Output (OS\_POLYLINE) is hooked: a solid one-pixel pen with R2\_COPYPEN, R2\_NOTCOPYPEN or R2\_BLACK draws each segment as an engine line (recipe and DIBENG's rules under Bresenham line in section 3). Everything else (other R2 modes, styled or wide pens, points outside the screen, zero-length segments, an empty clip) goes to DIBENG, and a declined segment of a Polyline goes alone. Threshold 32 pixels along the major axis (registry ACCEL\_LINE\_MIN), switch ACCEL\_LINES. Each engine call waits for idle, so the CPU is not free during a line and short lines are no faster than DIBENG. Hardware: 90 of 90 sets identical to DIBENG at 32, 16 and 8bpp. |
| Comparing the engine with DIBENG on the screen (VX2AUTO v2.4, VX2LINE v2.2) | A reference bitmap made by copying the screen to memory is not an identity at 8bpp (colours are translated) and made the 8bpp results vary from run to run. Instead paint two identical regions side by side on the screen, 312 pixels apart: the distance must be a multiple of 8, or the 8x8 dither of a non-pure colour is out of phase and nearly every 8bpp pixel differs (310 apart did exactly that). Draw the first with the engine on and the second with it off (DIBENG in software), read both back with one copy and compare. 2D control (VX\_IOCTL\_2D\_CTRL): 2 = engine off, 5 = engine on and ignore the size thresholds, 0xFFFFFFFE = query, 0xFFFFFFFF = registry defaults; the operation counter in the reply proves the engine ran. |
| Hardware cursor (v10.105, passed on the board: 300 mouse moves took 141.8 ms with the software cursor, 7.6 ms with the hardware cursor; inverting cursors on the legacy mono unit from v10.258, ACCEL\_HWCURSOR\_INV default 8 (v10.128's variant 9 passed on black only and drew the inverting pixels white); 8bpp from v10.262) | Primary display's register set, BAR1 offsets: 0x2E4 and 0x120C inverse colour (0x00FFFFFF), 0x2E8 (0x0D000D0F), 0x2EC (0), control 0x2F0 (0x36000004 plus bit 0 = on; the key mode 0x060F00F0 below is used since v10.129), image address 0x2F4, position 0x2F8, origin offset 0x2FC. Image: 64x64 pixels, 32-bit ARGB (alpha 0 = transparent), 16 KB at an address in video memory that is a multiple of 1 KB, kept in a block at the end of video memory. Windows hands over AND/XOR masks; build the ARGB image from them. Windows 98 never hands over a cursor larger than 32x32 (DIBENG's Inquire reports 32x32); cursors at 8bpp use the hardware cursor too from v10.262 (the XOR mask's palette indices looked up in the GDI palette; the image made again after a palette change). Inverting pixels (AND 1, XOR white: the I-beam, the size cursors), first judged on the board in v10.128 as variant 9 (VX2CUR2 v1.4, judged on black only; v10.257 found it draws the inverting pixels white, so the I-beam vanished over white): control 0x060F00F0 (64x64, RGB32, constant alpha FF, bits 29, 28 and 2 clear: no per-pixel alpha, no blending), transparent key 0x00FF00FF in 0x2EC and in every transparent image pixel, inverting pixels written as 0x00FFFFFF, and BOTH 0x2E4 (OpenChrome V327\_HI\_INVTCOLOR, 'reserved' in the manual) and 0x120C set to 0x00FFFFFF; opaque pixels are their own colour, and an opaque colour equal to a key moves one step in green (white becomes FFFEFF). Primary register set only. What fails: in the alpha-blended mode 0x36000004 the colour keys are never compared (an inverting pixel shows its own colour or, with alpha 0, nothing); in key mode with the inverse key FF00FE in 0x120C and 0x2E4 left white, the inverting pixels show magenta, with or without blending enable, alpha FF pixels, or alpha from the pixel (VX2CUR2 v1.1 to v1.3, variants 1 to 7). The chip's legacy mono cursor (0x2D0 to 0x2E0, 64x64x2: per row 8 XOR bytes then 8 AND bytes, bit 7 leftmost, from Linux viafb) is variant 8, and it is what the driver uses from v10.258: AND 1 XOR 1 inverts. Every shape with inverting pixels goes to it, colour shapes too, in the shape's darkest and brightest colours (v10.331); its 1 KB pattern sits right after the primary surface, because 0x2D0 takes the address in bits 25:10. Registry ACCEL\_HWCURSOR\_INV: 0 (default from v10.357: such shapes to the software cursor, exact), 8 (the default v10.258-v10.356; any other non-zero value is read as 8), 0 = inverting shapes to the software cursor; live control 0xF5000000\|v. Hide the cursor at every mode set. Registry ACCEL\_HWCURSOR: 1 (default) primary set, 2 second display's set, 0 off. |
| SaveScreenBitmap, menu save-under (v10.124; VX2SAVE PASS 7 of 7; ACCEL\_HOST bit 8, on by default since v10.125) | Export SaveScreenBitmap as ordinal 92 and set RC\_SAVEBITMAP at 16 and 32 bpp as well as 8. SSB\_SAVE (1) copies the rectangle into a 4 MB save block in video memory, reserved below the 2 MB staging block and not given to DirectDraw, with the engine's screen-to-screen copy; SSB\_RESTORE (2) copies it back only when the rectangle and the mode are the same; SSB\_DISCARD (4) drops it. Anything refused returns 0 and USER redraws on its own. On the board at 1024x768x32: menus of 4, 12 and 28 items, 0 differing pixels after closing with the save-under off and on, 3 saves and 3 restores, 475.3 to 414.9 ms per 28-item menu open and close (the menu animation dominates). Counters: live control 0xFFFFFE1B. |
| Video overlay, V1 unit (v10.106 to v10.107, passed raw and through DirectDraw on the board) | Registers, BAR1 offsets: V1\_CONTROL 0x230, fetch count 0x234, buffer addresses 0x238, 0x248 and 0x254, stride 0x23C, window top-left 0x240 ((left<<16)\|top), bottom-right 0x244 (right-1, bottom-1), zoom 0x24C, mini 0x250, FIFO 0x258, colour key 0x220 and 0x264, source height and count 0x26C ((height<<16)\|(width-1)), YCbCr to RGB matrix 0x284 and 0x288, compose 0x298 (fire bit 0x80000000, load at vertical blank 0x10000000; wait until the fire bits clear). YUY2 only. Settings that worked: control 0x00050081; start = offset + top\*pitch + ((left\*2) & \~15); stride = pitch \| (pitch<<15); fetch = ((width\*2 rounded up to 16)/16 + 1, at least 4) << 20; zoom X = ((srcW\*0x800/dstW) & 0x7FF)<<16 \| 0x80000000 with mini \|= 2; zoom Y = ((srcH\*0x400/dstH)-1) & 0x3FF \| 0x8000 with mini \|= 5 (enlargement only, shrinking needs the HQV scaler); FIFO 31 \| 29<<24 \| 16<<8; matrix 0x13000DED and 0x13171000 (without them the colours are wrong); destination colour key = 24-bit RGB \| 0x40000000 written to both 0x220 and 0x264, compose \|= 0x21. Three traps: a flip shows only when all three buffer addresses change (a select bit in 0x254 does nothing); an update without a key must write 0 to 0x220 and 0x264 or the old key stays in force and the picture disappears; the overlay request reply size must match between the VxD and the HAL DLL (a mismatch made the HAL quietly offer no overlay). At 16bpp the key is a 5-6-5 pixel expanded to 24 bits with zero low bits. At 8bpp (v10.130, VX2OVL8 5 of 5) the key is the palette index in bits 7:0, with no DAC lookup; two indexes with the same colour key separately. Registry ACCEL\_OVERLAY: bit 1 = the primary display's register set, 0 = off (default 961 with the extensions in the next row). |
| Video overlay extensions: HQV shrink and RGB, planar YV12/I420, source colour key, bob (v10.108, each passed raw on the board with VX2OVL RAW, and all of them, plus RGB32 and bob, through DirectDraw in v10.115; ACCEL\_OVERLAY default 961) | ACCEL\_OVERLAY mask bits: 1 register set, 64 HQV (shrink and RGB sources), 128 planar, 256 source key, 512 bob; default 961 = 1+64+128+256+512, mask bit 1024 is constant alpha (unsupported, off) and 2048 enables the diagnostic register probe commands, registry values above 4095 mean 0. HQV scaler registers, BAR1 offsets: control 0x3D0 (format RGB32 0, RGB16 0x20000000, RGB15 0x30000000, YUV422 0x80000000, YUV420/NV12 0xC0000000; plus ENABLE 0x08000000, TRIPLE 0x04000000, FLIP\_STATUS 1; SW\_FLIP 0x10, FLIP\_ODD 0x20, IDLE 8, SUBPIC\_FLIP 0x8000), source Y address 0x3D4, chroma address 0x3D8 (NV12 only), three destination buffers 0x3EC, 0x3F0 and 0x3FC (2 MB each), destination stride 0x3F4, source stride 0x3F8, scale controls 0x3B0 (horizontal) and 0x3B4 (vertical), filter 0x3E4, window registers 0x370 to 0x38C and 0x3CC (= ((srcRight-1)<<16)\|(srcBottom-1)), and 0x394, 0x3E0, 0x3E8 = 0, 0x3B8 = 0x0643212C, offset 0x1CC. For the HQV path V1\_CONTROL adds 0x02000000 (V1 reads the HQV output) and the RGB format bits (RGB32 4, RGB15 8, RGB16 0xC; YUV needs none), the V1 start addresses are the HQV destination buffers plus the crop offset, and the HQV control register carries the source format. First picture: wait for 0x3D0 idle, write 0x3D0 without and then with SW\_FLIP, wait for SUBPIC\_FLIP to clear, write V1\_CONTROL, fire compose. Later pictures: write 0x3D0 = control \| 1. Flip: wait for SW\_FLIP to clear, write the new 0x3D4 (and 0x3D8), then 0x3D0 = (old & \~0x20) \| 0x10 \| 1. Hide: clear ENABLE. Needs a block of video memory at the end of the framebuffer (the driver takes 8 MB below the cursor block: 3 x 2 MB buffers and 2 MB of NV12 staging). Shrinking works up to 32x; RGB at 1:1 also goes through the HQV. Planar trap: V1\_CONTROL value 0x10 (OpenChrome's V1\_YCbCr420) is YUV411 on this chip (the manual: stream data format bits 4:2 = 100), which is why a YV12 picture shown that way came out squeezed to a quarter of its width; and the V1 unit's own planar mode puts the Cr start address at 0x2F0, which is also the hardware cursor control. So planar video must go through the HQV, as OpenChrome does: it wants NV12, a Y plane plus one plane of interleaved U,V pairs (U first), the chroma plane address in 0x3D8. YV12 holds V before U and I420 U before V, so the driver interleaves the two planes with the CPU into a staging area and alternates between two staging areas. Strides for planar: V1 stride = pitch<<1, HQV destination stride = pitch<<1, HQV source stride = ((pitch>>1)<<16) \| pitch \| 0x10000; V1 fetch counts 16-bit pixels; control 0xC0000000. Source colour key (V1 alone, YUY2): 0x228 = 24-bit (Y<<16 \| U<<8 \| V) \| 0x40000000 with bit 31 clear, 0x22C = the upper limit, compose bit 1 (compose \|= 3 with a destination key too), and mini &= \~7 because the hardware interpolates wrongly next to a chroma key; it worked on the board (a keyed bar became see-through) but not together with the HQV. Bob (V1 alone): control \|= 0x00400000, with interleaved fields also 0x01000000 and half the source height, the odd field is bit 31 (0x80000000) of 0x254 (the manual: V1 Play Odd/Even Field Control; bit 27 is an address bit), toggled at every flip; confirmed on the board with a picture of two interleaved fields (even lines bars, odd lines reversed bars): the overlay alternated cleanly at each flip. Trap: the first build used bit 27 and the test never flipped, so it could not show the error. Another trap: the chroma key enable (bit 30 of 0x228) stays on until it is cleared, so a plain update or a hide must write 0 to 0x228 and 0x22C or the keyed colour stays see-through. Bob through the HQV (with a shrink, or planar) also passed on the board: 0x3D0 bit 17 field to frame, bit 18 frame to field (set both for an interleaved frame, only bit 17 for a source that holds one field, then the vertical scale source height is doubled), bit 5 selects the field to play and is toggled at every HQV flip (the flip writes (old & \~0x20) \| field<<5 \| 0x10 \| 1). HQV bit 16 (OpenChrome's HQV\_DEINTERLACE) is reserved on this chip, and 0x394 = 0 selects line-average bob (bit 6 = 1 is line duplication). Constant alpha: the manual describes 0x210 (Alpha Window Control: bits 15:8 constant alpha factor, default FF; bits 1:0 = 00 constant, 01 alpha stream, 10 graphics stream; blending is Alpha\*VID + (1-Alpha)\*GRA) and an alpha window rectangle in 0x208 and 0x20C (valid while 0x260 bit 0, the hardware icon enable of the second display, is 0), but constant alpha never blended on the board in any form, so it is documented as UNSUPPORTED on the V1 overlay (mask bit 1024, off by default; the VX2OVL ALPHA probes use the REGW/REGR commands behind mask bit 2048). Tried, with 0x210 latching and reading back every time: the factor alone; the factor with the alpha window set to the overlay's window; DirectFB's UniChrome value 0x00080000 \| factor<<8 (constant mode, alpha stream expire field 8) with a V1 fire and no window; constant, alpha stream and graphics stream select; colour key on and off; through the HQV; 0x298 bit 20 cleared (it reads 1 at power-up, which the manual calls V3 on top; cleared, the picture looked different but transparency was never seen); 0x260 bit 2 set (second display HI blending). With the alpha window set the picture did not change at all. 0x298 also reads FF00 in bits the manual calls reserved. Not checked: 0x2A0 bit 0 (V3 enable), SR2E bits 1:0 (V3/V4 playback clocks), a controlled graphics stream alpha source, and the sequence the XP driver uses. Auto flip is DirectDraw video-port switching (not needed, refused); no mirror control is documented (a CPU mirrored copy would work); bob together with RGB is refused (RGB sources are progressive). Colour note: the YCbCr matrix values are OpenChrome's defaults (see above) and matched the plain display path when compared side by side; a remote KVM capture lifts black by about 25, so judge colours against a row of plain display colours on the same screen. Through DirectDraw (the HAL, VXDDRAW.DLL) the board passed every overlay feature in v10.115 (VX2OVL v1.3): picture, enlargement, destination key, flip, move, hide, YV12, shrink, RGB565, RGB32, source colour key (DDOVER\_KEYSRCOVERRIDE: the YUY2 key pixel Y0 U Y1 V becomes the 24-bit Y U V key) and bob with interleaved fields. Shrink was first refused by DirectDraw itself with 80070057 (E\_INVALIDARG) before the HAL ran, because the caps said overlays cannot shrink: dwMinOverlayStretch was 1000 (that means no shrinking; 63 allows down to 1/16, the HQV limit) and DDFXCAPS\_OVERLAYSHRINKX/Y were missing. Bob: without a video port, DirectDraw accepts DDOVER\_BOB only when dwCaps2 has DDCAPS2\_CANFLIPODDEVEN (0x00002000; the DDK: bob without a video port by handling DDFLIP\_ODD and DDFLIP\_EVEN in Flip). Without it the runtime returned 80070057 for BOB, BOB \| INTERLEAVED and BOB \| INTERLEAVED \| BOBHARDWARE before the HAL ran, even with CANBOBINTERLEAVED, CANBOBNONINTERLEAVED and CANBOBHARDWARE set. With it (dwCaps2 006A7030), BOB \| INTERLEAVED is accepted, and the application picks the field of each new buffer with Flip(DDFLIP\_ODD or DDFLIP\_EVEN): the HAL passes the flag to the VxD, which shows that field (0x254 bit 31 on V1, 0x3D0 bit 5 on the HQV); a flip without a field flag toggles the field. Passed on the board: ODD, EVEN, ODD, EVEN flips alternated cleanly. RGB32 on a 32 bit desktop was refused by the HAL itself (80004001, zero refusals in the VxD): a surface in the desktop's own pixel format has no DDRAWISURF\_HASPIXELFORMAT flag and its ddpfSurface is not filled, so the HAL must take the desktop format in that case, for UpdateOverlay and for the pitch in CreateSurface (v10.114; passed on the board in v10.115). Deploy trap: the first run of that fix still failed because the board had an older VXDDRAW.DLL; VX2OVL logs the size of the DRV, DLL and VxD it finds, so check those sizes first. The VxD overlay QUERY returns the reason of the last refused request, which VX2OVL logs after a failed UpdateOverlay. |
| Gamma ramp (v10.123; VX2GAMMA PASS 7 of 7, VX2SWAP: in force again after Alt-Tab and a DOS box; ACCEL\_GAMMA default 1) | Proven on the board (v10.123): at 16 and 32 bpp every ramp from SetDeviceGammaRamp is loaded into the DAC (PEL mask FF, index 0, the high byte of each WORD, red, green, blue per entry) and CR33 bit 7 is set; CR33 read back A6 (bit 7 on, the other bits kept) and 26 at 8 bpp. Identity, darker (2.2), brighter (0.45), red only and a fade to black and back looked right and read back the same; DirectDraw's IDirectDrawGammaControl (exclusive 640x480x16) reaches the same path; at 8 bpp the bit is cleared and the palette stays. Any ramp is taken: the inherited software gamma refused dark ramps, so fades to black failed. The table is loaded again after every mode set and on the return from a DOS box, and the bit is cleared while a DOS box is shown; a ramp never set leaves the BIOS state alone. Before v10.123: the driver advertises DDCAPS2\_PRIMARYGAMMA (dwCaps2 bit 0x20000) and GDI C1\_GAMMA\_RAMP (enable.c), and DDIGammaRamp in dibcall.c reaches FBHDA\_gamma\_set / FBHDA\_gamma\_get in vxd\_fbhda.c. That code, inherited from VMDisp9x, only stores the 3 x 256 WORD ramp and turns it into one number (hda->gamma, from the entry at 128, or at 64 for the ID Software quirk where entry 128 is 0xFFFF); nothing writes it to the hardware, so SetGammaRamp fades and brightness do nothing on screen. Recipe to build, from OpenChrome via\_display.c (viaIGA1SetGamma): IGA1 gamma correction on = CR33 bit 7 (3X5.33\[7\]; SR16 bit 7 is the CLE266/KM400 location only); IGA2 is CR6A bit 1. With gamma on at 16 and 32 bpp, the DAC holds the gamma table: load it through the same ports as the 8bpp palette (PEL mask 0x3C6 = 0xFF, index 0x3C8, three bytes per entry on 0x3C9, the DAC is 8 bits on this board), using the high byte of each ramp WORD. At 8 bpp the DAC is the palette, so gamma stays off. Reload the table after every mode set and on return from a DOS box. Built as planned: registry ACCEL\_GAMMA (default 1), test VX2GAMMA (grey ramp; darker, brighter and red-only ramps; a fade to black; yes/no each, judged against the reference row because of the KVM black lift). |
| GDI drawing from system memory: memory bitmap blits, DIB-section ROPs, filled shapes (v10.116 to v10.122; VX2GDI v1.5 on the board: 0 differing pixels at 16 and 32bpp; ACCEL\_HOST default 22 then, 30 since v10.125) | Rule: the engine cannot read system memory and the CPU reads video memory at about 30 MB/s, so the win is ROPs that read the screen. Staging route (VxD op XF\_STAGE): the CPU writes the source into a 2 MB staging block in video memory (reserved below the HQV block, not given to DirectDraw; a 1-bit source is expanded to the two colours there), flushes the write-combining buffers (locked xchg, then a read of the last dword), and the engine finishes with its proven screen-to-screen ROP3 copy (source base 0x1C = block >> 3, source pitch in reg 08; ROPs with a pattern term use the fixed colour, bit 13, from a solid brush). Bands of at most 2 MB. Measured DIBENG to staging: sprite (1-bit mask SRCAND + bitmap SRCINVERT) 7.13 to 1.68 ms at 16bpp, 14.25 to 1.87 ms at 32bpp; 1-bit SRCAND/SRCPAINT/SRCINVERT/DSna 11.80 to 4.32, 23.67 to 4.28 ms; DIB sections 20.30 to 5.59, 41.89 to 12.18 ms; brush ROPs from memory bitmaps 21.21 to 1.40 ms (32bpp). Plain SRCCOPY of a colour bitmap stays with DIBENG (it only writes, at CPU speed). Sources as GDI hands them: a 1-bit GDI bitmap is a BITMAP (pdType 0, top-down, bmSegmentIndex 0 for one segment), 0 bits take the draw mode's text colour and 1 bits its background colour (both physical); a compatible colour bitmap is a DIB engine bitmap (deType RP, row y at deBitsSelector:deBitsOffset + y \* deDeltaScan); a DIB section never reaches BitBlt: GDI calls StretchDIBits (1:1, the ROP as given; sy counts from the bottom for both row orders), 32-bit sources on the 32bpp screen and 5-6-5 BITFIELDS sources on the 16bpp screen are taken, 5-5-5 stays with DIBENG. 1-bit SRCCOPY through the host window (CC0A0141 plus clip bit 12, rows padded to 32 pixels and clipped back) is pixel exact but no faster (0.72 to 1.22 ms at 16bpp), so it is off (ACCEL\_HOST bit 1). Filled shapes: GDI does not send OS\_SCANLINES; Polygon, Ellipse, RoundRect, Pie and wide pens with ROPs that read the screen arrive as OS\_POLYSCANLINE (style 5) between OS\_BEGINNSCAN (80) and OS\_ENDNSCAN (81), and lpPoints is a list of wCount GDI region scans: WORD count, top, bottom (exclusive), count x values in pairs (first pixel, one past the last), count again. Each pair is a rectangle: one engine fill per rectangle (op XF\_SPANS with rectangle entries; mono pattern rows rotated to the rectangle's first line, colour patterns with the 0x20 offset bits), the brush and the draw mode's ROP2 as a ROP3 that ignores S. Only ROP2s that read the screen go to the engine (XOR, NOT, MASK...): 817 rectangles, 20.84 to 10.46 ms at 16bpp, 40.49 to 10.59 ms at 32bpp (with the shorter waits of v10.132 to v10.138 about 3 times faster again, 9.7). ROP2s that only write stay with DIBENG: ACCEL\_HOST bit 32 sends them to the engine, pixel exact but 3 to 7 times slower, so it is off. A brush whose 64 realized pixels are one colour counts as solid even without COLORSOLID (16bpp solid brushes were seen without it). |
| Text through a 1-bit buffer and the host window (v10.116 to v10.121): tried, removed in v10.122 | Idea: DIBENG draws the string into a 1-bit buffer (its own glyphs, spacing and clipping), the engine expands it to the screen. What was learned: a hand-built 1-bit DIB engine device makes DIBENG draw nothing (blank desktop text in v10.118) and hangs it with the System font; the working target is what GDI itself hands DIBENG for text into a 1-bit memory bitmap: a GDI BITMAP (type 0, word-aligned rows, bmWidthPlanes = size, bmlpPDevice = the display PDevice, bmScanSegment = rows per 64 KB) with mono physical colours carrying 0x40 in the top byte (black 0x40000000, white 0x40FFFFFF; dibeng.h GREY\_BIT). Text into a 1-bit DIB section never passes through the driver. TrueType text with font smoothing is drawn by DIBENG in several passes with background modes 3, 4 and 5 (gray levels; FONTINFO dfType 0x0400), which a 1-bit buffer cannot hold; raster fonts have dfType 0. FONTINFO and TEXTXFORM are byte-packed: read dfPixHeight (offset 22) and dfMaxWidth (27) at their GDI offsets when the driver is built with 2-byte packing. Why it was dropped: even where it was pixel exact (DrawText) it was slower than DIBENG drawing the text itself (DIBENG 0.2 to 0.3 ms a string; TextOut 2.05 to 2.17, opaque 1.37 to 1.80, ETO\_OPAQUE 1.63 to 2.31, DrawText 0.25 to 0.49 ms at 32bpp), and long strings lost their tail from about x=430 (not chased). A safety rule worth keeping for any similar path: an empty buffer for a string with visible characters is a failure, never blank text. |

## 9. Open problems and dead ends

Five problems were pursued hard. One is solved (9.4), three are closed with a working replacement (9.1 to 9.3), and one is parked with its role unknown (9.5). No recipe in sections 3 to 6 depends on any of them. 9.7 records the review of 22 October 2026, which found working routes for 9.2 and 9.3, and the decisions taken since. Each entry says what was wanted, every attempt with its result, what the evidence suggests, and what to use instead, so you can judge whether to try again.

**How the attempts were run.** The lab is a VxD plus a Win32 test program on the Win98 board (640x480x32). The build labels in the tables (v9.87 and so on) only order the rounds. Each test works in a 64x64 window of the live desktop at (448,320). The window is prefilled with a known pattern (a 16x16 picture at (4,4) with red, green, blue and yellow corner markers and a white F, and a position-dependent gradient everywhere else), the operation runs, every pixel is read back and compared with a model, and the window is restored. Mono tests use a 32x8 bitmap whose rows carry recognisable bytes (a moving single bit plus bit 0 in the first byte, then `AA`, `0F`, and the row number times `0x11`), foreground `FFFF8000`, background `FF004080`, and a box at (4,4). When the engine hangs, the lab records its state before resetting it (status register 0x400, the wait-reason code the idle test returns, size register 0C and destination XY register 10), then recovers it with the sequence in section 1. A hang is therefore a result, not a crash. XP's own driver was the ground truth: its register stream was captured for operations the manual leaves unclear.

### 9.1 Mono expansion from screen memory

**Wanted:** keep a 1-bit bitmap (a glyph or pattern) in video memory and have the 2D engine expand it into foreground and background colours at a destination box, with no per-pixel CPU work. **Command:** a BitBLT with source mono (bit 8) and the source on screen (bit 6 clear), base `CC000101`, source address in register 1C (byte address >> 3), source pitch in register 08 bits 10:0 (8-byte units). **Result: closed.** About 70 variants over six rounds produced no correct output. XP's driver does not use this path either.

| Round | Variants | Result |
| --- | --- | --- |
| 1 | 8: `CC000101` at source pitch 1, 2, 4, 8, 0; `CC020101` and `CC0A0101` (alignment bits); text `CC000102` | 8 of 8 hung, no pixel changed |
| 2 | 18: source location bits (register 08 bits 13:12) 00, 01, 11, each with plain, clip, source transparency, alignment and text forms | 18 of 18 hung |
| 3 | 15: bitmap at byte strides 16, 256, 512, 1024, 2560 with matching pitch, three command forms | 15 of 15 hung |
| 4 | Hypothesis: the engine always takes mono bits from the host window. The same screen-source commands (`CC0A0101`, `CC020101`, `CC000101`, `CC020102`, `CC0A0102`) with host data written right after the command, at source pitch 32 and 0, feeding every second dword, with the VRAM rows held constant, after a 2D reset, and with a prefilled scratch | `CC020102` and its variants hung 15 of 15. Some runs completed, but with scrambled output. The proven host-fed control `CC0A0141` was always correct |
| 5 | Unique-word decode: every VRAM dword is `3C000000 + k` and every host dword is `81000000 + j` (palindromic tags, because the first choice `55` and `AA` are bit reversals of each other and decoded ambiguously), so each output row can be traced to its source in either bit order | Completed runs mix VRAM dword 0 with host dwords 5 and 6 in fixed row slots; other rows come out `FFFFFFFF` |
| 6 | Stepped feed: status read after the command and after every host dword, feeding stops when the engine is idle | The control needs all 8 dwords and goes idle only after the 8th. The screen-source forms never go idle early: they hang, or finish after all 8 |

**Hang state** (identical every time): status 06000002 (also seen as 06010002 and 06000012), wait reason 2, register 0C = `0007001F` (the full 32x8 size still pending), register 10 = `014401C4` (the box start). The engine accepts the command and stalls before writing its first pixel, whatever the stride, pitch or command bits. Status bit 0x10000 flickers after host dwords 2 and 6.

**Other observations.** The same command with the same data hangs on one run and completes on another, so the outcome depends on history and timing. A scan of the 4 MiB the lab's CPU could see then found no stale host-tagged words in video memory, so the scrambled output is not a leftover host buffer. One completed screen-source run (pitch 64, round 6) came out as: row 0 = VRAM dword 0, rows 1, 2, 3, 5, 6 = host dwords 5, 6, 3, 5, 6, rows 4 and 7 = `FFFFFFFF`; other runs of the identical command hung.

**Use instead:** host-fed mono (`CC0A0141`) or host-fed text (`CC021542`), both proven. For a cache of glyphs, a 3D textured rectangle sampling a prebuilt L8 glyph atlas is a candidate (L8 texturing is proven, 10 of 10 cases; it has not been tried as a glyph path). **Untried, none promising:** a non-zero source XY, a source base inside the desktop region, an 8bpp or 16bpp destination, and a source above 4 MiB (the lab's CPU mapping then; the driver maps up to 512 MiB from v10.04, so this is no longer blocked).

### 9.2 Fixed-colour fill with destination key

**Wanted:** paint a fill colour only over the pixels in a box that equal a key colour (recolour one colour inside a region). **Natural form:** the fixed-colour fill `F0002001` (bit 13) with register 48 bit 15 (destination key) and the key colour in register 4C. That hangs the engine. **Result: solved by a workaround; the hang itself is unexplained and treated as a hardware limit.**

Test: 16x16 box at (4,4) of the 64x64 window, key `FF203060` (the box background), fill `FFFF8000`. PASS means all 174 key-coloured pixels become the fill colour and every other pixel is untouched.

| Form tried | Result |
| --- | --- |
| Destination-key copy (`CC000001`) from a solid scratch surface of the fill colour | Pass, 174 of 174 |
| `F0000A01` (mono register pattern, all ones) with dest key | Pass, 174 of 174 |
| `F0002001` (bit 13) with dest key | Hang |
| `F0002001` with the key colour also in the pattern foreground (register 58) | Hang |
| `F0002A01` (bits 13, 11, 9), `F0002801` (bits 13, 11) | Hang |
| `F0012001` (bit 13 plus mono transparency, bit 16) | Hang |
| `F0402001` (bit 13 plus pattern keep, bit 22) | Hang |
| `F0003001` (bit 13 plus clip) | Hang |
| `AA002001` (ROP AA) | No hang, no pixel changed (AA is a no-op ROP here) |
| `CC002001` (ROP CC) | No hang, but all 174 key pixels were overwritten with other, unrelated colours |

The first sweep could not tell unchanged pixels from pixels turned into another colour; the test was changed to count unchanged, filled and wrong pixels separately.

**Working recipe** (this is the form in section 3): command `F0000A01`, registers 24 and 28 = `FFFFFFFF`, 48 = `0x8000`, 4C = key colour, 58 and 5C = fill colour. Alternative: a destination-key copy (`CC000001`) from a solid surface of the fill colour. **Avoid:** bit 13 together with a destination key in any form (hangs), and bit 13 with ROP CC (overwrites the key pixels with garbage). **Open question:** why bit 13 alone works (fixed-colour fills are proven) and bit 13 plus the key does not. **Untried:** the byte-mask rectangle-fill mode (register 48 bits 31:28) with bit 13, and 8bpp or 16bpp destinations.

### 9.3 Screen-to-screen rotate

**Wanted:** rotate a surface that is already in video memory with the 2D engine, without the CPU touching pixels (a rotated display, or an off-screen bitmap rotate). **Command:** the rotate BitBLT (op 9, mode bits 1:0 = angle) with the source in video memory (bit 6 clear). **Result: closed.** It hangs in every form tried. The same operation with a host-fed source works (section 4A), and the 3D engine covers the video-memory case (section 4C).

**Attempts.** The lab's early presets covered the screen-source rotate with the source base in video memory at 90, 180 and 270: 639 runs across the early rounds, every one a hang. The engine state at the hang was not captured for these runs, as it later was for the mono work in 9.1, so there is no hang signature to compare.

**What XP does** is the strongest clue. Its rotate blits are all host-fed (source pitch 0): mode `0x302` for 180 degrees, decrement bits set, command `CC00E049` for colour. XP never issues a screen-to-screen rotate, so the hardware may simply not support it. **Related hang family:** rotation angle 0 or 180 combined with some decrement-bit combinations also hangs, with or without host data (section 7).

**Why it stays closed.** XP never uses the form, rotation brings no speed advantage over the CPU (host-fed rotation measured about 50 MB/s; CPU writes to the framebuffer run at 540 to 2000 MB/s, see 8.5), and the video-memory case has a working route. **Use instead:** the 3D textured rectangle with rotated texture coordinates (30 of 30 cases, zero errors), or a shadow copy of the surface kept in system memory and pushed through the host-fed rotate.

**A lead worth rechecking.** The rotated mono text hang (9.4) turned out to be missing decrement bits and the wrong start corner, not a hardware refusal. There is no record that the early screen-source presets all used the exact decrement bits and start corner from section 4A, so a repeat with those rules, capturing the hang state, would settle whether this one is a true limit. Nothing depends on it.

### 9.4 Rotated mono text (op A): solved

**Wanted:** draw a mono glyph bitmap rotated by 90, 180 or 270 degrees with the 2D engine's text-with-rotate operation, the command XP issues for rotated text (`CC02D54A`). **Starting point:** issuing XP's exact word from the lab hung the engine every time, even with host data supplied, while the colour rotate (`CC00E049`, op 9) already worked. **Result: solved.** The hang came from leaving out the decrement bits and the start corner that the colour rotate needs. The recipe and its proof are in section 4D.

**How it was found.** A ladder of 12 rungs began at the proven unrotated text command `CC021542` and added one ingredient at a time up to `CC02D54A`, then repeated with dword alignment (bits 19:17 = 101). Host-fed 32x8 bitmap with 103 set bits, box at (4,4).

| Rung | Command, mode | Result |
| --- | --- | --- |
| 0 to 2 | `CC021542`, `CC020142`, `CC02014A` at mode 300 | Completed, unrotated (op A works unrotated) |
| 3 | `CC02014A` at mode 302 (op A, 180 degrees, no decrement bits) | Hung |
| 4 | `CC02C14A`, mode 302 (X+Y decrement, start at the bottom-right corner) | Completed, 180 degrees, 103 of 103 foreground pixels |
| 5 to 7 | `CC02D14A` (+clip), `CC02C54A` (+transparency), `CC02D54A` (XP's exact word) | Completed, 180 degrees |
| 8 | `CC0A014A` at mode 300 (dword alignment) | Completed, unrotated |
| 9 | `CC0A014A` at mode 302 (no decrement bits) | Hung |
| 10, 11 | `CC0AC14A`, `CC0AD54A` (dword alignment, decrement bits) | Completed, 180 degrees |

The only hangs were the two rungs without both decrement bits; XP's exact word completed. The command word decoded cleanly all along, which is why a plausible decode proves nothing (section 7). A confirm run straight after a restart (so no earlier hang or reset could have helped) then passed 90, 180 and 270, opaque and with clip plus transparency. Using an unswapped engine size at 90 or 270 does not hang but draws a scrambled box, so the size must be the destination size. Finally the 192-run sweep in 4D covered widths 5 to 33, both row layouts, 16bpp, and clips cutting through the glyph: all passed.

**Lesson for the other niggles:** a hang that looks like the engine refusing a command can be a missing direction bit or a wrong start corner. Check the 4A rules before calling a form dead.

### 9.5 CBU rotate windows

**What they are:** BAR1 offsets 0x1E00 to 0x1FFF hold 16 rotate windows, 0x20 bytes apart. XP programs window 0 when it rotates the whole screen, then issues host-fed rotate blits. **Result: parked; their role is unknown.** In every Win98 test they had no effect on 2D engine writes (fills and host blits), 2D engine reads, or CPU framebuffer reads and writes, and rotation works with them off, so no recipe uses them.

**Register layout** (from the hardware manual): +00 control (bit 0 enable, bits 2:1 bpp, bits 4:3 type 01 = 90, 10 = 180, 11 = 270, bit 5 tile flag, bit 6 rotate with tile, bit 7 tile mode), +04 base, +08 end, +0C source pitch reciprocal (floor(2^22 / width)), +10 pitch, +14 size (height << 16 | width). The manual calls them write-only; in the lab they read back exactly what was written, upper bits included, so the reads are real.

**XP's capture** (whole-screen 180 degree rotation at 800x600x32): window 0 control `0x15` (enabled, 32bpp, type 180), base 0, end `0x1D4C00`, reciprocal `0x147A`, pitch `0x03200320`, size `0x02580320`. XP disables it with control `0x14`, then `0`. The 640x480 equivalent is end `0x12C000`, reciprocal `0x1999`, pitch `0x02800280`, size `0x01E00280`. XP itself refused to rotate 90 or 270 at 800x600, which is not a hardware shape limit, since non-square host-fed rotation works at any size.

**Attempts.**

| Test | Result |
| --- | --- |
| Six presets replaying XP's 180 values, and a host sweep with the window off, with the 640x480 geometry as XP, and with swapped geometry for 90 and 270 | Fills and host-fed blits behaved the same in all three |
| CPU writes of a 64x64 image through a 90, 180 or 270 window (control `0x0D`, `0x15`, `0x1D`) over a scratch surface, then linear readback; also the reverse (write with the window off, read through it) | No rotation of CPU access |
| 2D engine pointed at the window as destination and as source | No effect |
| Whole-screen case: XP's window at base 0 with full 640x480x32 geometry over the live desktop, full-screen 2D copies to two surfaces (window off and on) and CPU reads of both | Clean negative: no effect on CPU reads, 2D copies, fills or host rotates |

**Method problems found (all fixed, and worth knowing):** the lab's CPU mapping covered only the first 4 MiB of video memory then (up to 512 MiB from v10.04) (a driver constant, not a measured hardware limit), so a test surface at 0x600000 read back as unmapped (surfaces now sit at 0x250000 and 0x400000; the engine worked on the 0x400000 surface, which the CPU mapping cannot read); probing offsets at 8 MB and 16 MB page-faulted in the VxD, so reads and writes beyond mapped video memory are now bounds-checked. **Limits of the tests:** window control values `0x0D`, `0x15` and `0x1D` only (no tile bits 5 to 7), and nobody looked at what the monitor shows (every check read memory back and never looked at scan-out).

**Other drivers.** The Linux OpenChrome driver (xf86-video-openchrome) has no use of the CBU registers and no hardware rotation: its screen rotation option is a software shadow framebuffer. It gives no hint of what the windows do.

**Hypothesis (unverified):** the windows belong to the display side, rotating the front buffer for a rotated screen, rather than to 2D drawing. It fits the XP capture, where the window covers the whole screen (end = pitch times height), and it fits drawing being unaffected in every test.

### 9.6 If you pick one up

In order of how likely they are to teach something:

1. **CBU windows (9.5):** enable a window and look at what the monitor shows, since scan-out was never checked; write a small register-poking tool for XP to enable the window there and read the screen as a positive control; capture XP at a resolution where it does rotate 90 or 270 and compare the window fields.
2. **Screen-to-screen rotate (9.3):** repeat it with the exact decrement bits and start corners of section 4A and capture the engine state at the hang. 9.4 shows that a rule like this can hide behind a hang.
3. **Destination-key fixed fill (9.2):** try the byte-mask rectangle-fill mode (register 48 bits 31:28) with bit 13, and 8bpp and 16bpp destinations.
4. **Mono from screen memory (9.1):** only if a new idea appears; the untried list is short and none of it looks promising. Host-fed mono and text cover every use.

Open questions about using these recipes inside a driver (batching, sync with CPU writes, 3D throughput, a D3D HAL) are in section 8.4.

### 9.7 Review of 22 October 2026: corrections, leads, decisions and measured tuning

An outside review of v10.108 against this cookbook, the manual and OpenChrome corrected two of the open problems above: both have working routes, neither has a Windows 98 caller. Three of its suggestions were measured in v10.132 to v10.138 (VX2OPT, VX2OPT2, VX2TUNE) and are on by default; the rest are recorded here with the decisions taken since.

| Item | Status | Recipe or next step |
| --- | --- | --- |
| Fixed-colour fill with a destination key (9.2) | Possible, not wired in | All-ones mono pattern instead of the hanging fixed-colour bit 13: 0x24 = 0x28 = 0xFFFFFFFF, 0x48 = 0x00008000 (destination key on), 0x4C = key, 0x58 = 0x5C = fill colour, 0x0C size, 0x10 position, command 0xF0000A01 last. 174 of 174 key pixels replaced at 32bpp; 8 and 16bpp and the alpha byte not qualified. Only DirectDraw (DDBLT\_COLORFILL with DDBLT\_KEYDEST) would use it. |
| Screen-to-screen rotation (9.3) | Works through 3D, direct 2D unproven | The textured rectangle of 4C rotates a video-memory source (30 of 30, square 64x64 only; non-square, sub-rectangles, 16bpp textures and speed not tested). Direct 2D lead: the old lab presets (CC000009, modes 301 to 303) lack the decrement bits and start corner later proven for host rotation. Untested candidates for a 16x8 source at 32bpp: 90 degrees CC00A009, destination 8x16, start (dx+7, dy); 180 CC00E009, start (dx+15, dy+7); 270 CC006009, start (dx, dy+15). No Windows 98 caller needs either. |
| Mono expansion from video memory (9.1) | Parked | About 70 failed variants; Linux viafb only feeds mono data from the host. Host-fed mono works; cached colour bitmaps plus screen copies are the alternative. Reopen only with a new hardware trace or documented prerequisite. |
| CBU rotate windows (9.5) | Parked | Sixteen 0x20-byte blocks at 0x1E00 to 0x1FFF; base and end in bytes (low 4 bits read-only), pitch and size in pixels, 11-bit fields where 0 = 2048, the reciprocal from the pitch in pixels. Next step needs a positive control: capture the windows while XP rotation works and find which traffic they translate. |
| Text and 1-bit SRCCOPY from system memory | Closed on speed | Host-fed mono SRCCOPY pixel exact but no faster (v10.117, ACCEL\_HOST bit 1 off); text through a 1-bit buffer slower than DIBENG and lost string ends (removed v10.122). The review's checklist (one command per rectangle, raw register writes, rows padded to 32 pixels) matches what was built. |
| Wide lines with R2\_COPYPEN | Closed (v10.130, v10.131) | Wide pens never reach the driver as bands: DIBENG draws them inside its own Output (4.7 ms for the VX2WIDE line set at 16bpp). COPYPEN shapes as bands were pixel exact at 8, 16 and 32bpp but 3 to 7 times slower (9.6 to 31.8 ms at 16bpp): ACCEL\_HOST bit 32 exists, off by default. |
| 8bpp pattern brushes | Done since v10.77 to v10.89 | Colour patterns, dithers, hatches and 1-bit bitmap brushes as 8bpp index tiles. |
| Overlay over an 8bpp desktop | Proven (v10.130, VX2OVL8 5 of 5) | Destination key = the palette index in 0x220 and 0x264 bits 7:0 with bit 30; no DAC lookup; video formats and strides unchanged. |
| Section 8.4, partly stale | Corrected | Interleaving CPU and engine writes is settled by VX2SYNC (11.1). "Several operations, one wait" is settled: ACCEL\_SPAN\_WAIT 5 and ACCEL\_OP\_WAIT 1 (v10.138). 3D rotation throughput against the CPU is still not timed. |
| Shorter engine waits, screen-reading ROPs, fill threshold (v10.132 to v10.138, VX2OPT, VX2OPT2, VX2TUNE) | Proven, on by default | The 32 stable idle reads cost about 8 us per command. ACCEL\_SPAN\_WAIT 5: no wait between the rectangles of a band batch (one idle read already took XOR shapes from 6.60 to 2.13 ms at 16bpp and 6.68 to 2.24 ms at 32bpp; no wait is about 15% faster again and was identical in the VX2TUNE soak of 540 random shape sets at 8, 16 and 32bpp). ACCEL\_FILL\_MIN 2304 (48x48, VX2TUNE): PATCOPY below that stays with DIBENG, which is faster under 32x32 at every depth; tall narrow fills (8x256) favour the engine, so ACCEL\_FILL\_TALL (v10.142, default 256 from v10.143) sends a fill below the area threshold to the engine when it is at least that many rows high (VX2FAST: the engine wins every narrow fill from 256 rows at 8, 16 and 32bpp). ACCEL\_OP\_WAIT 1: one idle read after a single fill, copy or line (SRCCOPY 32x32 39.1 -> 30.7 us at 32bpp); staging, host and save paths keep the full wait. ACCEL\_DREAD\_ANY 1: ROPs that read the screen skip the size thresholds (PATINVERT 96x96 2024 -> 45 us at 32bpp, 16x16 79 -> 31 us). ACCEL\_STR\_FUSE 1: 1-bit DIBs into 32bpp expand through a nibble table (2x 5.91 -> 4.92 ms, DIBENG 9.96). The phase profile is off unless armed (0xF4000001). Write-only ROPs on bands (ACCEL\_HOST bit 32) stay off: COPYPEN shapes 4.74 ms against DIBENG 1.97 ms. |
| 8bpp colour-pattern shapes (v10.134 to v10.137, VX2SPAN8, VX2COL8, VX2TIP) | On the engine by default; minor known difference from DIBENG | Single 8bpp colour-pattern fills match DIBENG at every width, height and X/Y phase (7680 cells). Band batches draw exactly the shape's region (GetRegionData rectangles drawn with DIBENG PatBlt: 0 differing pixels), but DIBENG's own Polygon shifts the pattern one pixel sideways on its 2-3 pixel spans, so the tip and corner pixels of a patterned (dithered, hatched or pattern-brush) shape at 8bpp can differ from DIBENG by a few pixels (12 on the VX2TIP star). Not the wait (same with the full idle) and not the tile or registers (same when rewritten per rectangle). Possible dependency: anything that compares 8bpp patterned shapes with DIBENG pixel for pixel. Registry ACCEL\_COL8\_BANDS: 1 (default) engine, about 2.5 times faster; 0 hands these batches to DIBENG for an exact match. Solid colours and 16/32bpp are unaffected. |
| DPMS (monitor standby, suspend, off) | Not supported for now (decision, 22 October 2026) | The driver blanks the screen, and the Wyse board draws about 7 W, which is good enough. Recipe for later (from research): Win98 mini-VDD slots 54 SET\_MONITOR\_POWER\_STATE and 55 GET\_MONITOR\_POWER\_STATE\_CAPS, written only when DispatchTableLength > 55, as \_\_cdecl C callbacks (DEVNODE, state), not the register-based VDDFUNC wrappers; CM states D0 1, D1 2, D2 4, D3 8; analog output CR36 bits 5:4 (00 on, 10 standby, 20 suspend, 30 off, other bits kept), CR47 bit 2 DAC off only for off; VBE 4F10 through VESA\_SUPPORT for legacy callers; monitor sleep only, never PCI D3. |
| Hardware cursors larger than 32x32 | Not applicable | The DRV forwards Inquire to DIBENG, which reports a 32x32 cursor, so Windows 98 never hands the driver a larger shape; reporting 64x64 would make USER stretch every standard 32x32 cursor to twice the size. |

## 10. Second display on the DVI-I port (UNCONFIRMED, tabled)

**Status: tabled, all findings questionable.** A second picture (IGA2 to the DVI-D side) works at the monitor's native mode and at 78.75 MHz and above. Below that it is unreliable. The cause is not known. It may be this board's DVP1 and VT1632 settings, or it may be the DVI-I to VGA + DVI-D splitter, which was never ruled out. Treat every number below as a lead, not a recipe.

**Setup tested.** One DVI-I port, a DVI-I to VGA + DVI-D splitter. VGA stays on IGA1. IGA2 drives DVP1, which feeds an external VT1632 transmitter (I2C bus 2 = SR31, 7-bit address 0x08). Monitors: Dell E2210 (native 1680x1050, 146.25 MHz) and Dell E2014H (native 1600x900, 97.75 MHz). Tests ran from the new VX2DVI.EXE with the test VxD; they need no more than the 64 MiB frame-buffer setting.

**Path and registers (as used, unconfirmed beyond these two monitors).**

- CR9B bit 4 = 1 selects IGA2 as the DVP1 source. CR9B bits 3:0 are the data/clock phase adjust. CR9B bits 6:5 set sync polarity (1 = negative). SR1E bits 5:4 = 11 turns the pads on. CR6A = C0 enables IGA2. SR65 sets pad drive.
- IGA2 pixel clock: SR4A (dm low byte), SR4B (dm bits 9:8 | dr << 2 | dtz bit 0 << 7), SR4C (dn | dtz bit 1 << 7). Fout = 14.318 MHz x dm / (dn << dr). Pulse SR40 bit 2 to reset. The OpenChrome rule (VCO at least 300 MHz) was used to pick dr.
- VT1632 (SiI164-compatible): reg 08 = 0x31 | 0x02 (EDGE) | 0x08 (DSEL) | 0x04 (BSEL); OpenChrome init is reg 08 = 3B, 09 = 20, 0A = 00, 0C = 00. Reads of regs 0B to 0F return FF and are not reliable.

**What was observed.**

| Dot clock | E2210 | E2014H | Note |
| --- | --- | --- | --- |
| 78.75 MHz and above (up to 146 MHz native) | good | good | CR9B adjust 13 with EDGE 1, or EDGE 0 at 108/135 MHz; VT1632 reg 0C = 00 |
| 65 and 74.25 MHz | good, reg 0C = 00 | poor with reg 0C steps 0 to 8 | no filter step helped on the E2014H |
| 40 and 49.5 MHz | good only with reg 0C = 01 | not retested after the filter work | reg 0C = 01 breaks 65 MHz and up on the E2210 |
| 25.2 and 28.3 MHz | fail | fail | every combination tried failed, and XP was also unstable at these clocks |

- At 146 MHz, flipping EDGE shifts the good adjust window by 8 steps (one step is about 0.43 ns). Pictures with rotated colour channels or swapped nibbles mean the wrong half-phase.
- A monitor that has just been fed a bad signal keeps going to standby for several seconds. Sweeps that apply settings back to back are misleading. Only from-scratch attempts (restore, about 3 s of no signal, apply, wait for an answer) are reliable.
- A 32-bit overflow in the PLL search gave 469 MHz for a 40 MHz mode. It is fixed in the driver and the lab.
- VRAM size was judged irrelevant to this problem.

**Open questions.**

- Is the splitter the cause? A direct DVI-D connection, or another splitter, has not been tried.
- Is the 65 to 74.25 MHz failure on the E2014H a limit of this TMDS path, of the PLL post-divider at dr = 3 or 4, or of that monitor's receiver? The E2210 locks the same modes.
- What does the real VIA driver under Windows XP program at 1024x768 and 1280x720? XPDUMP.EXE (register dump through inpout32.dll) was built to find out; it has not been run on real XP.
- Never tried: SR65 drive strength and VT1632 reg 0A deskew at 65 to 74 MHz on the E2014H; the HDMI capture dongle and a DVI-to-HDMI display as sinks; 1080p (LOCK 1080P exists in the lab, untested on those sinks).

**If this is resumed.** Start the secondary mode list at 800x600 or above and use the settings in the table. Run XPDUMP under XP first, then try a different splitter or a direct connection before more register sweeps.

## 11. Win98 driver hardware results: ROPs, EDID, mode timing

Findings from building the VMDisp9x-based native driver (16-bit DRV plus 32-bit VxD) on the Wyse Cx0. Each item is labelled **proven** (run on the hardware, with the test named) or **unconfirmed**.

### 11.1 Fill ROPs and overlapping copy (proven)

Test: VX2SYNC at 640x480x32, 0 failures. CPU writes and engine operations were interleaved, and 400-operation mixed sequences were run.

| Operation | ROP byte | Notes |
| --- | --- | --- |
| PATCOPY (solid) | F0 | Solid fill, colour in 0x4C |
| BLACKNESS | 00 | No colour needed |
| WHITENESS | FF | No colour needed |
| DSTINVERT | 55 | Inverts the destination |
| PATINVERT | 5A | Solid brush XORed onto the destination |

Command word for every fill: `cmd = (rop << 24) | 0x2001`, after the usual prologue (0x04 = 0x300, pitch/8 fields in 0x08, 0x14, 0x1C, 0x30, 0x48 = 0, 0x58 foreground, 0x5C background). Size goes in 0x0C as `((h-1)<<16)|(w-1)`, position in 0x10 as `(y<<16)|x`.

Screen-to-screen copy overlaps correctly in every direction when the direction bits follow the geometry: set `0x8000` (decx) if `dx > sx` and `0x4000` (decy) if `dy > sy`, on top of `0xCC000001`. Tested with 24 offsets per case and in mixed sequences.

### 11.2 Decision: host-fed bitmaps and glyphs not hooked in the Win98 driver

The host-fed colour blit, mono bitmap and text recipes above work, but the driver does not use them. The CPU would still push every pixel through the MMIO window, and text reaches the driver through ExtTextOut inside the DIB engine, so a hook would be high risk with no clear gain. The accelerated paths in the driver were then solid fills with the ROPs above and screen-to-screen copy. (Superseded in part: v10.116 to v10.122 built and measured host-fed and staged paths, 8.5 'GDI drawing from system memory'; ROP blits from memory stayed, host-fed SRCCOPY and text were dropped on speed.) Each can be turned off with a DWORD under HKLM Software VX85598 (ACCEL\_2D, ACCEL\_COPY\_ALL; both default to on).

### 11.3 EDID over bit-banged I2C (proven for DVI, not readable on the VGA path)

The chip has no usable I2C controller here, so the driver bit-bangs the DDC lines through sequencer registers. Unlock SR10 bit 0 around every access.

| Bus | Register | Enable / mask | What hangs on it |
| --- | --- | --- | --- |
| 2 | SR31 | 0x01 / 0x31 | DVI DDC, and the VT1632 transmitter at 7-bit address 0x08 |
| 1 | SR26 | 0x01 / 0x31 | VGA DDC |
| 3 | SR2C | 0xC0 / 0xF0 | Third port; read it without setting bit 0 |

Bit meanings in the bus register: write SCL = 0x20, write SDA = 0x10, read SCL = 0x08, read SDA = 0x04.

**Proven.** A Dell E2014H on a straight DVI cable reads correctly on bus 2 when the driver starts: EDID 1.3, native 1600x900, range 56-76 Hz and 30-83 kHz, 170 MHz maximum clock. Devices answering on the bus: 08 37 3A 49 50 59 (the VT1632 is the one at 08).

**Not readable.** Anything on the VGA path gave silence at 0x50 on all three buses: the Spider KVM, a direct VGA monitor, and a DVI-I-to-VGA adapter without a splitter. The BIOS call (INT 10h AX=4F15h, BL=01) also fails (AX=014F; one earlier run returned 0000 but no data). The KVM is not the cause. The likely cause is that the monitor, cable or port has no DDC path to the chip, which is unproven. Windows XP's driver lists 800x600 to 1280x1024 on the same adapter without an EDID, so a fallback list is the right design: with no EDID the Win98 driver offers the modes up to 1024x768 (NOEDIDHIGHRES = 1 or EDID\_MODES = 2 offer every BIOS mode up to 1920x1200 and the synthetic sizes, 11.9); the default 800x600 comes from the INF either way.

With an EDID present, a mode is offered only if it is within the monitor's ranges, and an unlisted mode is capped at the native size so a 1600x900 panel is not offered 1920x1080. Registry EDID\_MODES: 1 default, 0 hides the extra modes, 2 also offers modes the filter held back.

### 11.4 BIOS VBE mode list (proven)

VBE 3.0, 39 listed modes. Linear 16 and 32 bpp modes the driver can use:

| Size | 16 bpp | 32 bpp |
| --- | --- | --- |
| 640x480 | 0x111 | 0x112 |
| 800x600 | 0x114 | 0x115 |
| 1024x768 | 0x117 | 0x118 |
| 1280x768 | 0x17A | 0x17B |
| 1280x800 | 0x1B7 | 0x1B8 |
| 1280x960 | 0x14F | 0x16A |
| 1280x1024 | 0x11A | 0x11B |
| 1368x768 | 0x19A | 0x19B |
| 1440x900 | 0x212 | 0x213 |
| 1600x1200 | 0x215 | 0x216 |
| 1680x1050 | 0x12C | 0x12D |
| 1920x1080 | 0x167 | 0x168 |
| 1920x1200 | 0x148 | 0x149 |

Of the 8 bpp (memory model 4) modes, the driver uses 0x101, 0x103 and 0x105 (640x480, 800x600, 1024x768) as base modes from v10.71; the others are skipped (the extras scan takes memory model 6 at 16 or 32 bpp). The driver's VX\_FB\_LIMIT was 4 MB then and rejected 32 bpp from 1280x960 up (and 1920x1200 at 16 bpp); it is 64 MiB since v10.73 (11.9, 8.5).

### 11.5 Display timing problem above 1024x768 (proven)

With the BIOS, every mode above 1024x768 keeps the 1024x768 timing and only the pitch registers change. The monitor shows the top-left 1024x768 of the desktop, so the bottom and right are cut off. Evidence: VX2CRT at 1280x1024x16 and at 1024x768x16 give identical timing (total 1344x806, 768 lines, 65.146 MHz), with only the line offset register differing (320 against 256, in 8-byte units). Windows XP, which works at 1280x1024, programs standard VESA 60 Hz timings:

| Mode (32 bpp) | Totals | Pixel clock | Sync | Pitch |
| --- | --- | --- | --- | --- |
| 1024x768 | 1344 x 806 | 65.146 MHz | -/- | 4096 |
| 1280x1024 | 1688 x 1066 | 108.1 MHz | +/+ | 5120 |

XP's 1280x1024 PLL setting is dm 151, dn 5, dr 2 (primary and secondary alike), and the DVI transmitter chip (VT1632) answers on bus 2.

Register decode (OpenChrome map): horizontal total = (CR00 | CR36 bit 3 << 8) + 5 characters; horizontal displayed = CR01 + 1 characters; vertical total = (CR06 | CR07 bit 0 << 8 | CR07 bit 5 << 9 | CR35 bit 0 << 10) + 2; vertical displayed = (CR12 | CR07 bit 1 << 8 | CR07 bit 6 << 9 | CR35 bit 2 << 10) + 1; line offset = CR13 | CR35 bits 7:5 << 8, in 8-byte units; dot clock = 14318 x dm / (dn << dr) kHz with dm = SR44 | (SR45 & 3) << 8, dr = (SR45 >> 2) & 7, dn = SR46 & 0x7F. Characters are 8 pixels wide.

### 11.6 Custom CRTC block through VBE 4F02 (proven not to work)

Tested (driver v10.44): passing a VBE 3.0 CRTC info block with 4F02 bit 11 set made no difference. The BIOS kept the 1024x768 timing, so VX2CRT still showed 1344x806 at 65.146 MHz. The driver therefore programs the timing itself (11.7). Registry CUSTOM\_TIMING (default 1) now gates that direct programming; 0 leaves the BIOS timing alone.

A side note for lab code: the 3D lab request path refuses reads when pitch x height exceeds the 3D arena (result 2, VX3D\_NOT\_READY), so any register dump needs its own ioctl rather than that path.

### 11.7 Programming IGA1 timing directly (proven, v10.57)

After the BIOS sets a mode above 1024x768, the driver rewrites the IGA1 registers. It always programs them, because the BIOS leaves some timing registers untouched, so reading them back to decide is unreliable (the result depended on the previous mode). Verified on a Dell E2014H over VGA with VX2CRT dumps: 1280x1024 gives 1688x1066 at 107.98 MHz and fills the panel.

| Field | Registers (index) |
| --- | --- |
| Horizontal total | CR00, and CR36 bit 3 = bit 8; value = pixels / 8 - 5 |
| Horizontal display | CR01 = width / 8 - 1 (round the width up) |
| Blank start | CR02 = width / 8 - 1 |
| Blank end | CR03 bits 4:0, CR05 bit 7, CR33 bit 5; value = total chars - 1 (7 bits) |
| Sync start | CR04, and CR33 bit 4 = bit 8 (needed from 2048 pixels, 1920x1200) |
| Sync end | CR05 bits 4:0 |
| Vertical total | CR06, CR07 bits 0 and 5, CR35 bit 0; value = lines - 2 |
| Vertical sync start | CR10, CR07 bits 2 and 7, CR35 bit 1; value = start line - 1 |
| Vertical sync end | CR11 bits 3:0; value = (end line - 1) and 15. Clear CR11 bit 7 first to unlock CR00 to 07 |
| Vertical display and blank start | CR12 and CR15; CR07 bits 1/6 and 3, CR09 bit 5, CR35 bits 2 and 3; value = height - 1 |
| Vertical blank end | CR16 = (vertical total value + 1) and 255 |
| Line offset | Already set by the BIOS (CR13 and CR35 bits 7:5); leave it |
| Fetch count | SR1C and SR1D bits 1:0 = width x bytes per pixel / 16 + 4 (XP 1280x1024x32 gives 324) |
| Sync polarity | Misc output bits 7 (vertical negative) and 6 (horizontal negative) |

Set SR01 bit 5 (screen off) while writing and clear it afterwards. SR10 bit 0 must be set (it was already set in the dumps).

**Dot clock.** f = 14318 x dm / (dn << dr) kHz with dm = SR44 plus SR45 bits 1:0 as bits 9:8, dr = SR45 bits 4:2, dn = SR46 bits 6:0. XP and the BIOS also set the top bit of SR45 and SR46 (written as 0x80 and 0x80 | dn). Known good: XP 432 MHz VCO (dm 151, dn 5, dr 2 = 108.1 MHz), BIOS 521 MHz (dm 182, dn 5, dr 3 = 65.146 MHz). Search dn 2 to 12, VCO between 300 and 600 MHz (the 300 floor is an assumption, needed for 79.5 and 83.5 MHz; it worked), searching dr from 3 down to 0 and dn from 2 up, and keep the first setting with the smallest error (so within one dr the lowest dn wins); if the best error is over 2% the search fails and the BIOS timing stays. A 400 MHz floor left 1280x768 and 1280x800 without a setting, so the driver silently left the BIOS timing in place.

### 11.8 Which timings the monitor accepts (measured on the E2014H, VGA input)

| Mode | What happened | Fix |
| --- | --- | --- |
| 1280x1024 | DMT timing, 1688x1066 at 108 MHz +/+: correct | None |
| 1280x768, 1280x800 | Correct once the PLL search found a setting | VCO floor 300 MHz |
| 1280x960 with DMT timing | Screen shows a 1600x900 window of the desktop: side bars and about 60 rows cropped. The DMT 1280x960 timing (1800x1000, 108 MHz, +/+) is the same as the 1600x900 timing, and the monitor picks its native mode | Send the 1280x1024 timing (1688x1066) with 960 lines; the monitor scales it, with a thin bar under the bottom |
| 1600x900 | Monitor detected 1600x900 from 1800x1000 at 108 MHz +/+ | Same timing, used for the real 1600x900 mode |

A GTF timing leaves the picture shifted (the monitor expects the DMT back porch); the driver has a fixed timing for every size it offers above 1024x768: DMT for 1280x1024, 1280x960, 1280x800, 1280x768, 1280x720, 1360x768, 1366x768, 1368x768, 1440x900, 1600x900, 1600x1200, 1680x1050, 1920x1080 and 1920x1200, and for 1152x864 CVT 60 Hz (81.75 MHz, from v10.307; TIMING\_1152\_CVT = 0 the 75 Hz DMT). GTF is only a fallback. Sizes other than 1280x1024, 1280x960, 1280x800 and 1280x768 were checked only for the mode list and (on a TV) 1920x1080.

### 11.9 Mode list and limits (proven)

The BIOS offers 39 modes (11.4). The driver adds three things:

- **Video memory limit.** VX\_FB\_LIMIT was 4 MB, which blocked 32 bpp from 1280x960 up and capped the 16 bpp slider at 1600x1200. It was raised to 16 MB (1920x1200x32 needs 9.2 MB), and to 64 MiB in v10.73. The 3D lab arena sits near the top of video memory and is unaffected.
- **Sizes the BIOS lacks.** 1600x900, 1280x720, 1152x864, 1360x768 and 1366x768 are offered on top of a BIOS mode with the same or wider pitch (1600x1200, 1280x768, 1280x960, 1368x768); the direct timing sets the real size.
- **Mode count.** The list cap went from 24 to 44 for the BIOS modes; the store holds 48 (the synthetic sizes up to 46, EDID\_MODES = 2's held-back modes up to 48).

Without a readable EDID, sizes above 1024x768 are hidden unless the registry DWORD NOEDIDHIGHRES under HKLM Software VX85598 is 1 (EDID\_MODES = 2 also offers everything). With an EDID the monitor's own limits decide.

### 11.10 Windows 98 slider limit

Display Properties stopped at 1600x1200 even though the driver offered 36 modes (VX2EDID showed all of them). Setting the monitor class value MaxResolution = "1920,1200" (HKLM System CurrentControlSet Services Class Monitor 0000 and 0001) lifted it, tested to 1920x1080 on a TV. The v10.57 INF writes it. The INF's MODES keys must also list every size at 16 and 32 bit (18 sizes from 640x480 to 1920x1200).

### 11.11 Open items

- Picture position was checked with VX2PROOF stage C on three monitors: all 11 stage C modes display correctly on at least one of them; 1920x1200 was on none and is assumed good. An old 1080p TV over VGA crops and stretches 1280x720, 1366x768 and 1600x900 (about 125 bottom lines are lost at 1280x720, with a vertical stretch). The driver's timings are within 6 pixels and its clocks within 0.04% of the spec, so that is the TV's scaler. Without an EDID, set NOEDIDHIGHRES to 1 to see these sizes.
- FIFO and fetch thresholds were copied from the 1280x1024 setup; high modes may need more.
- Drawing speed at 16 bpp over VGA looked fine on the direct monitor. The GDI engine path at 16 bpp is hardware-proven (8.5), including solid fills and PATINVERT.

## 12. Registry keys

The keys of the display, mode and 2D paths, by place; 12.7 adds the later VxD keys and the Direct3D and OpenGL families, which sections 15-19 describe. The VxD keys are REG\_DWORD values under HKLM\\Software\\VX85598 and are read once when the VxD loads, so a change needs a reboot. A missing key means the default. Since v10.103 every acceleration whose test program passed on the hardware is on by default; a key only overrides it. The test programs (VX2STRX, VX2ASYNC and others) write the keys for you when a class passes, and say "restart Windows" in their .RES file.

### 12.1 Memory, display and mode keys (VxD)

| Key | Default | Meaning |
| --- | --- | --- |
| FORCE\_SOFTWARE | 1 | 1 keeps the DRV's OPENGL\_GETINFO from offering the ICDs inherited from the VMware fork (SVGA3D, VirGL, QEMU 3dfx); VXGL.DLL is offered either way. It does not touch Direct3D, whose HAL is ACCEL\_D3D (default 1). |
| FORCE\_QEMU3DFX | 0 | 1 sets the QEMU 3dfx pass-through flag in the shared framebuffer header; emulator use only. |
| GAMMA\_QUIRK | 0 | Gamma-ramp format. 0 detects it from the ramp (the middle entry 0xFFFF picks mode 1, otherwise mode 2); 1 or 2 forces one. |
| VIDMEM\_MB | 0 | 0 hands DirectDraw all video memory up to VX\_FB\_LIMIT. A value of 8 or more (and below 4096) lowers the amount handed to DirectDraw. |
| OVERLAY\_n\_SIZE | 0 | n = 1 up to one less than FBHDA\_OVERLAYS\_MAX. Megabytes reserved for overlay n from the end of video memory. |
| DIRECT\_VRAM | 1 | 1 draws straight into the framebuffer; 0 returns to the old shadow-copy path. |
| VRAM\_WC | 1 | 1 maps the framebuffer write-combining; 0 maps it uncached. |
| EDID\_MODES | 1 | 1 offers the modes the monitor reports. 0 hides the extra modes. 2 offers every BIOS mode up to 1920x1200 whatever the monitor says. |
| NOEDIDHIGHRES | 0 | Without a readable EDID, sizes above 1024x768 are hidden. 1 shows them anyway. |
| CUSTOM\_TIMING | 1 | 1 applies the timing fix needed above 1024x768; 0 uses the BIOS timing as it is. |

### 12.2 2D acceleration keys (VxD)

| Key | Default | Meaning |
| --- | --- | --- |
| ACCEL\_2D | 1 | Master switch. 0 leaves every 2D call to DIBENG. |
| ACCEL\_16BPP | 1 | Allows engine operations at 16 bpp. |
| ACCEL\_8BPP | 1 | Allows engine operations at 8 bpp. |
| ACCEL\_COPY\_ALL | 1 | 1 lets the engine do overlapping copies (it picks the direction); 0 leaves them to DIBENG. |
| ACCEL\_FILL\_MIN | 2304 | Smallest fill, in pixels, that goes to the engine; smaller fills stay with DIBENG. 48x48 from VX2TUNE (v10.138; was 16384). Fills and copies whose ROP reads the screen ignore it (ACCEL\_DREAD\_ANY). |
| ACCEL\_COPY\_MIN | 256 | Smallest copy, in pixels, that goes to the engine. |
| ACCEL\_LINE\_MIN | 32 | Smallest line (pixels) that goes to the engine. |
| ACCEL\_LINES | 1 | 1 draws solid lines with the engine line command. |
| ACCEL\_DASH | 0 | 1 tries dashed pens on the engine. Stays 0: the engine drew a solid line in all 32 variants tried. |
| ACCEL\_DASH\_CMD | 0xA00 | Line command word used when ACCEL\_DASH is 1 (experiment only). |
| ACCEL\_DASH\_TCMD | 0x10A00 | Same, for the transparent-background variant (experiment only). |
| ACCEL\_DASH\_FMT | 0 | Bit field for the experiment: bit 0 the engine takes register bit 31 first, bit 1 pen colour for 0 bits, bit 2 register 28 gets the same value as register 24, bits 8 to 12 pattern index of the first pixel. |
| ACCEL\_PATTERN | 1 | Mono 8x8 pattern brushes on the engine. |
| ACCEL\_COLORBRUSH | 1 | Colour 8x8 pattern brushes; needs ACCEL\_PATTERN as well. |
| ACCEL\_CLIP | 1 | Uses the engine clip window for clipped drawing. |
| ACCEL\_DDRAW | 1 | Runs DirectDraw fills and copies through the engine. |
| ACCEL\_ASYNC | 1 | Starts DirectDraw blits without waiting; the next lock or blit waits. On by default since v10.103. |
| ACCEL\_STRETCH | 1605567 | Bit mask (24 bits) of the StretchDIBits and DibToDevice conversion classes done on the CPU. Default 0x187FBF. See the DIB conversion section for the bits. Bits 64, 32768 and 2097152 stay off because DIBENG is faster there. |
| ACCEL\_STRETCH2 | 27 | Bit mask of the 1-bit, 4-bit, RLE8 and RLE4 conversion classes into the 32bpp screen; bit = 1 << (kind\*3 + shape), kinds 1-bit, 4-bit, RLE8, RLE4, shapes both factors 2 or more, one factor 1, 1:1. Default 0x1B (classes 1, 2, 8, 16). Live control 0xFA000000 \| mask. The 1:1 and all RLE classes stay off because DIBENG is faster there. |
| ACCEL\_HOST | 30 | Bit mask of the GDI paths from system memory and the menu save-under (v10.116 to v10.130): 2 mono bitmaps staged, 4 colour bitmaps and DIB sections staged, 8 SaveScreenBitmap, 16 filled shapes as scan bands; 1 host-fed 1-bit SRCCOPY (off: no faster) and 32 bands with ROP2s that only write (off: slower). Live control 0xF7000000 \| mask. |
| ACCEL\_HOST\_MIN | 256 | Smallest area, in pixels, for the staged and band paths. |
| ACCEL\_MONO\_MIN | 4096 | Smallest area for host-fed 1-bit SRCCOPY (when ACCEL\_HOST bit 1 is on). |
| ACCEL\_OP\_WAIT | 1 | After a single engine fill, copy or line: 1 one idle read, 0 the full 32 stable reads (v10.133). Live 0xF1000000 \| v. |
| ACCEL\_DREAD\_ANY | 1 | 1 sends fills and copies whose ROP reads the screen to the engine at any size, past ACCEL\_FILL\_MIN and ACCEL\_COPY\_MIN (v10.133). Live 0xF0000000 \| v. |
| ACCEL\_SPAN\_WAIT | 5 | Wait between the rectangles of one band batch: 0 full idle, 1 none, 2 one idle read, 3 four reads; +4 applies it to colour-pattern rectangles too (v10.132 to v10.138). Live 0xF2000000 \| v. |
| ACCEL\_STR\_FUSE | 1 | 1-bit DIBs into the 32bpp screen expand straight to screen pixels through a nibble table (v10.132). Live 0xF3000000 \| v. |
| ACCEL\_COL8\_BANDS | 1 | 8bpp colour-pattern band batches on the engine; 0 hands them to DIBENG for an exact match with DIBENG's Polygon (v10.137, 9.7). Live 0xEF000000 \| v. |
| ACCEL\_HWCURSOR | 1 | Hardware cursor: 1 the primary display's register set, 2 the second display's set, 0 the software cursor. |
| ACCEL\_HWCURSOR\_INV | 0 | How inverting cursor pixels are drawn: 8 the legacy mono cursor (0x2D0-0x2E0, AND 1 XOR 1 inverts), used for every shape with inverting pixels, colour ones in their darkest and brightest colours (v10.258, v10.331); any other non-zero value is read as 8; 0 (the default from v10.357, exact) sends such shapes to the software cursor. Variant 9, v10.128's key mode, drew inverting pixels white. Live 0xF5000000 \| v. |
| ACCEL\_OVERLAY | 961 | Video overlay mask: 1 register set, 64 HQV (shrink and RGB), 128 planar, 256 source key, 512 bob; 1024 constant alpha (unsupported) and 2048 diagnostic register commands stay off; 0 turns the overlay off. |
| ACCEL\_GAMMA | 1 | 1 loads SetDeviceGammaRamp ramps into the DAC at 16 and 32bpp (v10.123). Live 0xF6000000 \| v. |
| ACCEL\_CR3 | 0 | 1 reloads CR3 (a whole TLB flush) at the start of every DRV call into the VxD; 0 skips it. Default 0 since v10.141 (about 4.2 us a call; VX2CR3 found no difference in 1150 rounds). Set 1 only on CPUs hit by the Windows 9x TLB invalidation bug (section 14.1). Live 0xEE000000 \| v. |
| ACCEL\_FILL\_TALL | 256 | Fills at least this many rows high go to the engine even below ACCEL\_FILL\_MIN (DIBENG's cost grows with the row count); 0 turns it off. 256 measured by VX2FAST at 8, 16 and 32bpp (v10.142/143). Live 0xED000000 \| rows. |
| ACCEL\_PREFILTER | 1 | 1 publishes the size thresholds in the FBHDA block so the DRV keeps fills, opaque text rectangles and lines the VxD would decline for their size without the trip into the VxD (v10.142; about 6 us saved per such call). Live 0xEC000000 \| v. |
| ACCEL\_KEEP | 1 | 1 keeps the engine open between operations: opened and its 19 registers saved once, handed back at mode sets, DOS boxes, flips, the 3D side, unbind and exit (v10.144; default 1 since v10.145; section 14.3). 0 opens, saves, restores and closes on every operation. Live 0xEB000000 \| v. |

### 12.3 Flip and scan-line keys (VxD)

| Key | Default | Meaning |
| --- | --- | --- |
| FLIP | 1 | 1 flips by changing the display start address; 0 turns flipping off. |
| FLIP\_BIOS | 1 | 0 forbids the per-flip BIOS fallback. |
| FLIP\_CAL | 0 | 1 brings back the BIOS calibration step at the first flip. |
| SCANLINE | 1 | 1 offers DirectDraw GetScanLine. |

### 12.4 system.ini \[display\] keys (16-bit driver)

| Key | Default | Meaning |
| --- | --- | --- |
| dpi | 96 | Logical pixels per inch reported to GDI. |
| x\_resolution, y\_resolution | from the mode | Force the starting size. |
| bpp | from the mode | Force the starting colour depth. |
| refreshRate | 0 | 0 lets the driver choose. |
| IgnoreRegistry | 0 | 1 ignores the registry mode and uses these keys only. |
| palettized | 1 | Read at 8 bpp only; 0 turns the palette off. |

### 12.5 INF and mode keys

| Key | Value | Meaning |
| --- | --- | --- |
| DEFAULT\\Mode | "16,800,600" | First mode after install (bpp, width, height). |
| RefreshRate | -1 | Optimal refresh rate. |
| DDC | 0 | Windows DDC probing off (the original VMware INF has 1); the driver reads the EDID itself. |
| `MODES\<bpp>\<w>,<h>` | (empty) | One key per offered mode. VX8BPP.REG adds the 8 bpp keys. |
| Monitor MaxResolution | "1920,1200" | Largest size the generic monitor entries allow (Monitor\\0000 and 0001). |

### 12.6 Keys inherited from the VMware fork (not used here)

The SVGA code (vxd\_svga.c) reads these, but it is not part of the VX855 build, so they do nothing: WRAMSize, and under Software\\vmdisp9x\\svga: VRAMLimit, RGB565bug, CommandBuffers, HWVersion, PreferFIFO, NoMultisample, RegMultisample, AsyncMOBs, HWCursor, NoScreenAccel.

### 12.7 Later VxD keys, Direct3D and OpenGL

HKLM\\Software\\VX85598, DWORD; the VxD's are read when it loads (restart), VXGL.DLL's when it loads.

| Key | Default | Effect |
| --- | --- | --- |
| TIMING\_1152\_CVT | 1 | 1152x864 with the CVT 60 Hz timing (81.75 MHz, v10.307); 0 the 75 Hz DMT timing |
| ACCEL\_DCI | 1 | The DRV corrects DCI's primary surface (screen selector, offset, DCI\_1632\_ACCESS, v10.250); 0 leaves it as DIBENG made it |
| ACCEL\_MOUSE\_FAST | 1 | Hardware-cursor moves write only the position registers, outside the frame-buffer access bracket (v10.253) |
| ACCEL\_FILL\_TALL | 256 | A fill below ACCEL\_FILL\_MIN at least this many rows high goes to the engine (v10.142-v10.143) |
| ACCEL\_PROFILE | 0 | Diagnostic: the VxD's 3D profile (TSC readings, latency distributions, register-access counts); test programs turn it on when they measure (v10.329) |
| ACCEL\_CURSOR\_CHECK | 0 | Diagnostic: the cursor's kept image and register shadow (v10.325); VXDX7 and VXDISCO turn it on through 0xFFFFFE1F |
| ACCEL\_DEBUG\_LOG | 0 | Diagnostic: 1 keeps the VxD's memory log after start-up (VX\_IOCTL\_LOG); 0 empties it after start-up (v10.329) |
| ACCEL\_D3D | 1 | The Direct3D HAL (v10.165); 0 off, 3 the DirectX 5-style HAL (section 15) |
| ACCEL\_D3D\_LOG | 0 | Diagnostic: C:\\VXD3D.LOG |
| ACCEL\_3D\_CLEAR | 3 | Engine clears of colour and depth (sections 15, 17) |
| ACCEL\_3D\_COARSEZ | 0 | Coarse Z off: slower than none on this board (section 17) |
| ACCEL\_D3D\_STENCIL\_ZW | 2 | Both stencil-plane bits on every stencil draw (section 17) |
| ACCEL\_D3D\_IDLE\_READS | 8 | Idle status reads in a row that count as idle (16.16) |
| ACCEL\_D3D\_STATE\_DIFF, ACCEL\_D3D\_BATCH\_END | 0 | Off by default (16.18) |
| Other ACCEL\_D3D\_\* and ACCEL\_3D\_\* switches | 1 | Each feature of sections 15-17 (MIP, ANISO, UNIT2C, CKEY, POSTED, FETCH, STRIP\_HW, VFAST, STATE\_CACHE, CGROUPS, SINK, ASYNC, BURST, RECOVER, CLEAR\_ASYNC and the stage features); 0 turns it off |
| ACCEL\_OPENGL | 1 | VXGL.DLL accepts contexts; 0 makes opengl32 use Microsoft's renderer |
| ACCEL\_OPENGL\_HW | 1 | 0 keeps the ICD but draws in software (read again at each context) |
| ACCEL\_OPENGL\_\* features | 1 | Each engine feature of sections 18-19; exceptions ACCEL\_OPENGL\_BATCH 32, ACCEL\_OPENGL\_EXTLEN 0, ACCEL\_OPENGL\_ZLOCK 0 |
| VXGL\_LOG, VXGL\_FRAMELOG, VXGL\_SAMPLE | 0 | Diagnostics: C:\\VXGL.LOG, the per-frame lines, the sampling profile (19.3) |

## 13. Known bugs and limits (v10.148)

Everything below is known and deliberately left as it is; each row says when it applies. They come from an outside source review of v10.103 (F, C and P numbers are that review's) and from this project's own tests. The review's other findings were fixed in v10.139: pending asynchronous work finished before mode sets, DOS boxes and binds (F01); no recursive wait on the framebuffer semaphore from the cursor (F02); the 15/16/24-bit colour-cursor converters (F03); cursor size checks in the DRV and the VxD (F04); cursor exclusions released after an engine loss (F05); empty-cursor detection (P10); RGB-only CPU colour keys at 32bpp (F07); the depth-fill, arithmetic-stretch and 'YUV2' capabilities removed and READSCANLINE added (F09); one HAL device handle per process (F11); MTRR, selector, span and flip-request hardening (F16 to F19); and the build number (DRV 139, INF 10.139). The CR3 reload on every DRV call (P03) is now switchable (ACCEL\_CR3) so its cost can be measured.

Updated for v10.148: P02 (GetBltStatus), P07 (register save and restore) and P09 (timing programming) are fixed; three rows are new at the end (DIBENG's 4-bit top byte, the DirectDraw destination-key quirk, the kept engine context). Section 14 records the speed work of v10.140 to v10.148.

| ID | Area | What happens | When it applies | Status |
| --- | --- | --- | --- | --- |
| F06 | Software cursor and page flips | The software cursor is saved, drawn and restored in buffer 0, not in the buffer being scanned out after a DirectDraw flip; it can vanish or mark a back buffer. | Only when the software cursor is in use (ACCEL\_HWCURSOR = 0 or ACCEL\_HWCURSOR\_INV = 0, 15 or 24 bpp, or the shadow framebuffer) and an exclusive game flips with the system cursor visible. The hardware cursor (default) follows scanout. | Open, low priority |
| F08 | DirectDraw blit effects | DDBLTFX\_NOTEARING is accepted without waiting for a blank; DDBLTFX\_ARITHSTRETCHY is accepted and drawn point-sampled. | Applications that pass these hints. Both are hints; the arithmetic-stretch capability is no longer advertised. | Accepted |
| F10 | Flip and vertical-blank timeouts | A flip or blank wait that times out returns DD\_OK; GetFlipStatus always reports done. | Only if the blank wait times out (a stopped CRTC, a mode change mid-flip). The address is programmed either way. | Accepted: returning errors would break games for no visible gain |
| P02 | GetBltStatus | Waits for a pending asynchronous blit instead of returning DDERR\_WASSTILLDRAWING. | Exclusive mode with ACCEL\_ASYNC 1. Correct but loses CPU/engine overlap for callers that poll. | Fixed in v10.142: a status-only VxD question answers busy, GetBltStatus(DDGBS\_ISBLTDONE) returns DDERR\_WASSTILLDRAWING (VX2FAST: 675 such answers in 40 rounds, every pixel right) |
| 9.7 | 8bpp colour-pattern shapes | Tip and corner pixels of a dithered, hatched or pattern-brush shape at 8bpp can differ from DIBENG's Polygon by a few pixels (the engine matches the region exactly; DIBENG shifts the pattern on its 2-3 pixel spans). | 8bpp desktops; pixel-exact comparisons with DIBENG. ACCEL\_COL8\_BANDS 0 restores an exact match. | Known difference, by decision |
| C01 | Surface destruction during asynchronous work | No explicit drain of a pending blit when DirectDraw frees a surface; the next VxD entry finishes it. | Exclusive mode with ACCEL\_ASYNC 1. Not reproduced. | Watch |
| C02 | Flips after an engine loss | A flip (a display-address change) is refused with LOST; whether address-only operations should be allowed after a loss is undefined. | After an unrecovered engine fault. | Policy, unchanged |
| C03 | 3D diagnostic fault | After a latched fault of the 3D lab backend, CPU framebuffer access is not fenced the way a 2D loss is. | Only when the 3D lab interface is used. | Diagnostic only |
| C04 | INF monitor workaround | MaxResolution 1920,1200 is written to monitor instances 0000 and 0001, whatever monitor is attached (DDC,0 goes under the adapter's DEFAULT key, 12.5). | Install time. | Accepted |
| F13 | 8bpp shadow framebuffer | The shadow blitter has no 8-bit writer, so nothing reaches the screen. | DIRECT\_VRAM 0 with an 8bpp desktop only. | Fixed in v10.330 |
| F14 | Shadow swaps | The swap checks only the first byte of the frame against the allocation, and FBHDA\_swap reports success when the swap was refused. | DIRECT\_VRAM 0 only. | Fixed in v10.330 |
| (new) | Shadow mode after an engine loss | With the exclusions now released after a loss (F05), the shadow path's FBHDA\_access\_end copies the shadow to the screen even while the engine is lost. | DIRECT\_VRAM 0 and an unrecovered engine fault. | Non-default, open |
| F15 | Overlay reservations | Registry OVERLAY\_n\_SIZE values are multiplied and summed without overflow or capacity checks. | Only when OVERLAY\_n\_SIZE is set. | Non-default, open |
| F20 | Synthetic modes | 1600x900, 1280x720 and the other sizes built on a donor BIOS mode stay listed when CUSTOM\_TIMING is 0, and then show the donor's timing; a failed timing rewrite is not reported. | CUSTOM\_TIMING 0 only. | Non-default, open |
| P04 | Shadow storage | 8 MiB of page-fixed RAM and the shadow timer are kept even with direct VRAM rendering. | Always; costs RAM, not speed. | Open, low priority |
| P05 | Palette loading | A full GDI palette is 256 separate VxD calls. | 8bpp palette changes. | Open, low priority |
| P06 | CPU stretch staging | The DirectDraw CPU stretch stages the whole source rectangle even when the destination is clipped. | Clipped or reduced CPU stretch blits. | Open, low priority |
| P07 | Register save and restore | Every synchronous engine operation saves and restores 19 registers. | Always; part of the per-operation cost. | Fixed in v10.145: ACCEL\_KEEP saves and restores once per ownership period (14.3) |
| P08 | GDI capture wrappers | The diagnostic capture wrappers run their prologue on every wrapped call, even when capture is off. | Always, small. | Open, low priority |
| P09 | Timing programming | Interrupts stay off during the whole timing selection and PLL search at a mode set. | Mode sets above 1024x768. | Fixed in v10.142: the timing and the PLL are worked out before interrupts go off, the PLL search visits 88 candidates (14.5) |
| (build) | Debug output | The VxD is built with DBGPRINT and VX\_MEMLOG: dbg\_printf calls format into a 32 KB memory log with interrupts off per character. Since v10.329 the log records start-up only: after start-up it is emptied and nothing more is formatted unless ACCEL\_DEBUG\_LOG = 1 (a start-up that fails keeps its log). | Start-up only by default (v10.329); with ACCEL\_DEBUG\_LOG = 1, mostly mode sets and initialisation, not the drawing paths. | Accepted for now |
| (enable.c) | GDI metric fields | dpEHiWin is not set (dpELoWin is written twice) and some Y viewport extents derive from X. | Legacy mapping modes (MM\_LOENGLISH and friends); no drawing failure seen. | dpEHiWin fixed in v10.302; the Y extents open, low priority |
| (DIBENG) | 4-bit DIBs into 32bpp | DIBENG leaves a nonzero top byte (apparently a neighbouring pixel's blue byte) in half the pixels of 4-bit StretchDIBits and SetDIBitsToDevice; the driver writes 00. Every colour bit matches. | 32bpp desktops; programs that read the unused byte back. VX2TOP (v10.143). | Known difference, by decision (Microsoft specifies rgbReserved as 0) |
| (runtime) | DirectDraw destination key | The Windows 98 runtime hands DDBLT\_KEYDEST to the HAL as DDBLT\_KEYDESTOVERRIDE with the effects structure's key left 0..0; the HAL now takes the surface's DDCKEY\_DESTBLT key then. An application that deliberately overrides with key 0 on a surface that also has a different destination key of its own would get the surface's key. | Destination-key blits. Fixed in v10.147 (VX2DKEY: 42 of 42 cases). | Fixed; the corner case accepted |
| (keep) | Kept engine context | Between operations the sequencer stays unlocked and the 2D registers hold the last command's values; anything that uses the 2D engine without going through a hand-back point (14.3) would see them. | Only foreign 2D users outside mode sets, full-screen DOS boxes, flips, the 3D side and exit; none known. ACCEL\_KEEP 0 restores the per-operation hand-back. | Watch |

Other limits are by design and documented where they apply: text, dashed pens, wide COPYPEN pens and plain memory-bitmap SRCCOPY stay with DIBENG (measured, 9.7); cursors larger than 32x32 (Windows 98 never hands one over) use the software cursor; DPMS is not supported for now (9.7); overlay constant alpha never blended (8.5).

## 14. Speed work, v10.140 to v10.148

Small GDI operations (menus, list items, toolbar buttons, text backgrounds, short lines) cost more in fixed overhead than in drawing. This round removed most of that overhead. Every change has a registry switch back to the old behaviour (section 12.2), and every one was checked pixel for pixel on the Wyse.

| Change | Measured on the Wyse | Test |
| --- | --- | --- |
| No CR3 reload on DRV calls (ACCEL\_CR3 0) | 26.5 to 22.3 us per 32x32 PATINVERT (640x480x32) | VX2FIX, VX2CR3 |
| DRV keeps requests the VxD would decline (ACCEL\_PREFILTER 1) | 16x16 fill 19.4 to 13.6 us, opaque text 90x16 32.1 to 26.3 us, 20-pixel line 16.9 to 11.0 us (32bpp) | VX2FAST |
| Tall fills to the engine (ACCEL\_FILL\_TALL 256) | 8x256 at 8bpp 94 us in DIBENG, 52 on the engine; engine wins every narrow fill from 256 rows at 8, 16 and 32bpp | VX2TUNE, VX2FAST |
| Kept engine context (ACCEL\_KEEP 1) | 32x32 fill 21.4 to 8.9 us, 64x64 PATINVERT 28.1 to 15.7, 48x48 copy 26.2 to 13.8, 700 mixed operations 24.9 to 20.2 ms (1024x768x32) | VX2KEEP |

### 14.1 The CR3 reload and the Windows 9x TLB bug

The DRV's entry into the VxD (VXD\_API\_entry) reloaded CR3, a whole TLB flush, on every call. It came from VMDisp9x with the comment "TLB bug here? Looks like..."; its \_PageAllocate has a disabled flush with "There is probably another TLB bug in VMM". The bug behind it is real: Windows 9x's VMM changes a valid page-table entry and uses the new mapping without INVLPG or a CR3 write (Henry Wong, "Windows 9x TLB Invalidation Bug", 2015). Processors whose page walks are not coherent (AMD Zen 2 and later, recent Intel, and virtual machines on them) can then use the old translation and crash. The real fix there is patcher9x's VMM patch; a flush in a display driver only helps for addresses the driver touches after its own entry.

The Wyse's VIA processor is not affected. VX2CR3 ran 1150 rounds with the reload off (mode switches, DOS boxes, DirectDraw sessions, 4 million random operations with memory decommitted and recommitted under the driver): 16,848 of 16,848 scenes drawn from remapped pages identical to the same scenes with the reload on, no hang. ACCEL\_CR3 is 0 by default since v10.141; set it to 1 on an affected CPU.

### 14.2 Where a small engine operation's time goes

VX2FAST profiled one 64x64 engine fill at 32bpp (v10.142, before the kept context): 28.3 us in all.

| Phase | us |
| --- | --- |
| Entry checks | 1.7 |
| Engine open (sequencer unlock and checks, port I/O) | 4.9 |
| Idle check | 0.6 |
| Save 19 registers, cursor exclusion | 5.3 |
| Program the command | 0.7 |
| Start and wait | 4.7 |
| Restore 19 registers | 0.8 |
| Close (sequencer lock), cursor | 2.3 |
| Outside the VxD (GDI, DIBENG's checks, the DRV, the call in and back) | 7.3 |

Open, save, restore and close were about 12.5 us of the 28: the kept context removes them.

### 14.3 Kept engine context (ACCEL\_KEEP)

The first operation opens the engine (unlocks the sequencer, sets SR1A bit 3 and SR1E bit 0) and saves the 19 registers 04, 08, 0C, 10, 14, 18, 1C, 20, 24, 28, 2C, 30, 40, 44, 48, 4C, 50, 58, 5C once. Later operations skip all of that. This is safe because every command writes each register it uses:

- the prologue writes 04, 08, 14, 1C, 20, 30, 48, 58 and 5C;
- copies write 18, plus 4C and 50 for keys and fixed-pattern ROPs (gated by register 48 and command bits);
- lines write 18, 24, 2C, 4C, and 28 and 50 for styled lines;
- fills write 4C, mono patterns 24 and 28, colour patterns 20;
- clipping writes 40 and 44 and is enabled by command bit 12 only;
- host transfers and band batches write their own complete set.

The context is handed back (the saved registers written back, the sequencer lock restored) by VX2D\_sync, which the mode set, the DOS box switch (VESA\_HIRES\_disable), the flip control and Windows exit call; by VX2D\_busy, which the 3D side calls before it touches the engine; by a bind or unbind; by the live switch going off; by an engine timeout or loss; and before e\_first and lost\_recover. A context left open across a mode generation change is dropped without writing the old registers back. A pending asynchronous DirectDraw blit completes as before (its cursor exclusions are released) without closing the context. Nothing is held between calls: no critical section, no semaphore, interrupts on.

Proof: the emulated engine (verify.py, keep.py: 160 random operations per depth identical with 0 and 1, hand-back restores the foreign values) and VX2KEEP on the board: 640x480 at 8, 16 and 32bpp and 1024x768x32, each run with ACCEL\_KEEP 0 and 1 (3 x 700 random GDI operations of every accelerated kind, fixed transitions such as clipped to unclipped and mono pattern to plain fill, a DOS box, a DirectDraw session, the live switch toggled): all 12 captures byte-identical, the 19 registers after the hand-back equal those before.

### 14.4 Size decisions in the DRV (ACCEL\_PREFILTER) and tall fills

The VxD publishes its size thresholds in the FBHDA block at bind and after every control call: res0 = 'VX' << 16 with bit 0 force and bit 1 'screen-reading ROPs ignore the thresholds', res1 the fill minimum, res2 the line minimum, res3 the copy minimum, rem2 the tall-fill rows. The DRV applies the same rule before building the trip: a fill, opaque text rectangle or line the VxD would decline for its size goes straight to DIBENG. The VxD can only decline more than the DRV (its clipping only shrinks the area), so nothing that should reach the engine is kept back. With no 'VX' tag the DRV sends everything as before.

Tall fills: an area threshold cannot tell 8x256 (engine wins: 52 against 94 us) from 256x8 (DIBENG wins: 37 against 41 us), because DIBENG's cost grows with the row count. Fills at least ACCEL\_FILL\_TALL rows high go to the engine below ACCEL\_FILL\_MIN. VX2FAST measured widths 1 to 32 and heights 48 to 400 with solid and hatched brushes: the engine wins every case from 256 rows at 8, 16 and 32bpp (128 rows lost at 8bpp: 1x128 26 against 31 us).

### 14.5 Smaller changes

- Hardware cursor: the position registers are not rewritten when the cursor is shown where it already is (VX2FAST: 0 writes during 2000 fills with the cursor still, 20 for 20 moves).
- Software cursor (8bpp): composes from the background it has just saved instead of reading video memory a second time; transparent pixels are not written.
- 1-bit and 4-bit StretchDIBits convert 2 or 16 palette entries instead of 256.
- YV12/I420 overlay: the chroma interleave reads four U and four V bytes per pair of dword reads (VX2OVL passed).
- GetBltStatus: a status-only VxD question (VX\_IOCTL\_2D\_ACCESS action 3) answers VX2D\_BUSY (4) while an asynchronous blit runs; after 100 ms, or when the engine is idle, the blit is completed as before.
- Mode sets: VX\_fix\_timing works out the timing and the PLL before interrupts go off. The PLL search visits, per dn and dr, only q = floor(khz \* (dn << dr) / 14318) and q + 1 clamped to the legal dm range (f grows by at least 149 kHz a step), 88 candidates instead of about 6500; it picks the same dm, dn and dr as the full search for every clock from 10 to 400 MHz. Ties go to the first minimum in dr 3..0, dn 2..12 order.

### 14.6 Two quirks found on the way

DirectDraw destination key: the Windows 98 runtime hands DDBLT\_KEYDEST to the HAL as DDBLT\_KEYDESTOVERRIDE with bltFX.ddckDestColorkey 0..0, while the surface's LCL holds the key (ddckCKDestBlt, flag DDRAWISURF\_HASCKEYDESTBLT). DDBLT\_KEYSRC arrives as DDBLT\_KEYSRCOVERRIDE with the key filled in. From v10.147 the HAL takes the surface's key when the override key is empty and the surface has one (VX2DKEY: 42 of 42 cases at 8, 16 and 32bpp, engine and CPU path). The HAL can record what it is handed on keyed blits (exports VXDBGKEY, and VXDBGKEYON to switch it on; off by default from v10.148).

Unused top byte at 32bpp: the driver's 1-, 4- and 8-bit StretchDIBits match DIBENG in every colour bit and write 00 in the top byte. DIBENG writes 00 for 1-bit and 8-bit stretches, copies rgbReserved in its 1:1 path, and leaves a neighbouring pixel's blue byte in half the pixels of a 4-bit image, in its own drawing too (VX2TOP). The driver keeps 00.

## 15. Direct3D through DirectX, v10.149 to v10.198

The board's runtime is DirectX 9.0c (DDRAW.DLL 4.09.00.0904). The native 3D endpoint (VX\_IOCTL\_3DN\_INFO 0x85500020, SUBMIT 0x85500021) was already proven by VX3DM1; this stage put a Direct3D HAL in front of it (VXDDRAW.DLL: vx\_d3d\_hal.c, vx\_d3d\_core.c, vx\_d3d\_legacy.inc).

### 15.1 Result

ACCEL\_D3D=1, VXDX7 --draw (v10.164): DirectDraw keeps the hardware HAL, Direct3D enumerates "Direct3D HAL" (devcaps 0008A451: DRAWPRIMITIVES2 + DRAWPRIMITIVES2EX), and on the GPU: flat provoking vertex PASS, Gouraud interpolation PASS, D32 near triangle rejects a later far one PASS, native triangle counter PASS. 2D unchanged with 3D on (VX2AUTO, VX2DKEY 42/42, VX2FAST, VX2KEEP, VX2OVL pass, twice). v10.165 made 3D the default.

### 15.2 Registry

| Value | Default | Meaning |
| --- | --- | --- |
| ACCEL\_D3D | 1 | 1 DirectX 6/7 HAL (DrawPrimitives2, CreateSurfaceEx). 3 DirectX 5-style HAL (no GetDriverInfo; RenderState/RenderPrimitive on runtime-allocated execute buffers). 0 off. Read by the VxD at boot, answered to the HAL through control query 0xFFFFFE50 (out\[1\] = 'VXD3'), so VXDDRAW.DLL imports only kernel32/user32/gdi32. Enter it as Decimal in regedit. |

### 15.3 What DirectDraw 9.0c checks (read from DDRAW.DLL, driver-object setup BAABB5C0-BAABBC15)

After every positive GetDriverInfo answer that carries a callback table:

- dwSize a multiple of 4 and at least the expected size; dwActualSize at least that size.
- BAAB915F, per function slot: a function without its flag bit fails; a flag bit whose function is NULL or fails KERNEL32 IsBadCodePtr fails.
- D3DCallbacks2 (BAABB95A): DrawOnePrimitive, DrawOneIndexedPrimitive and DrawPrimitives all present or all absent. A DrawPrimitives2 HAL offers SetRenderTarget and Clear only (flags 3).
- Miscellaneous2 with CreateSurfaceEx (BAABB726): the global data's hwCaps.dwDevCaps must include D3DDEVCAPS\_DRAWPRIMITIVES2EX (0x8000).
- D3DCallbacks3 when dwDevCaps has DRAWPRIMITIVES2: ValidateTextureStageState and DrawPrimitives2 required; then D3DParseUnknownCommandCallback must answer DD\_OK; ZPixelFormats becomes required if Clear2 is offered.
- Any failure jumps to BAABBC15: the D3D data is freed and the whole driver object is marked DDRAWI\_NOHARDWARE. Every DirectDraw object is then emulation only (dwCaps 02000000, HEL surface caps 40200000) and DxDiag shows DirectDraw and Direct3D Not Available. A refused 3D answer takes 2D acceleration with it.

In the order DirectX asks: D7B70EE0 (40 bytes), MiscellaneousCallbacks EFD60CC0, Miscellaneous2 406B2F00, VideoPortCallbacks EFD60CC1, MotionComp B1122B40, ColorControl EFD60CC2, D3DCallbacks2 0BA584E1, D3DCallbacks3 DDF41230, ParseUnknownCommand 2E04FFA0, KernelCallbacks 80863800, ZPixelFormats 93869880, D3DExtendedCaps 7DE41F80, then FFAA7540 and 3B8A0466. Refuse the ones not offered with DDHAL\_DRIVER\_HANDLED + DDERR\_CURRENTLYNOTAVAIL.

### 15.4 Traps on the way

| Symptom | Cause | Fix |
| --- | --- | --- |
| HAL dropped, NOHARDWARE, after the first positive GetDriverInfo answer (v10.149 to v10.163) | 15.3: DrawOne/DrawOneIndexed without DrawPrimitives; CreateSurfaceEx without DRAWPRIMITIVES2EX | v10.164 |
| CreateSurface(3DDEVICE) 80004001 (DDERR\_UNSUPPORTED) | the shared context's ddpf, read by the HAL's format checks, was never filled | v10.152: the DRV copies vmiData.ddpfDisplay into it at every HAL build |
| CreateDevice 80004005, ContextCreate DDERR\_INVALIDPARAMS (DX5-style path) | without CreateSurfaceEx the runtime passes ContextCreate IDirectDrawSurface interfaces (DDRAWI\_DDRAWSURFACE\_INT), not local objects | v10.153/155: each pointer checked: a local object's lpSurfMore is NULL or starts with a small dwSize, an interface's first word is its vtable |
| RenderPrimitive refused, buffer pointer on the application's stack (DX5-style path) | with no execute-buffer callbacks the runtime passes temporary objects; for vertices even with them | v10.159 execute buffers (PLEASEALLOC\_USERMEM, as vmhal9x); v10.161 memory taken from lpLcl->lpGbl->fpVidMem only, vertex range from the triangles' indices |
| Gouraud drawn flat on the DX5-style path | not solved (the DX6/7 path is the one in use) | open |
| A flip draws into the screen | DirectDraw swaps the chain's video memory at each flip, the HAL cached the target address at ContextCreate | v10.166: target and depth memory re-read from their objects at every draw |

Diagnosis that paid off: VXDX7's dump of the runtime's own driver object (no-hardware flag, caps, D3D pointers), the HAL's GetDriverInfo trace (export VX\_D3D\_QueryTrace) and reading DDRAW.DLL itself. Outside guesses (texture formats, execute-buffer caps, MiscellaneousCallbacks) were each tested by a registry bit and none was the cause; JHRobotics' vmhal9x (a working Win98 HAL on the same vmdisp9x base) was the reference for execute buffers.

### 15.5 Flip chains (v10.166 to v10.168, proven)

VXDX7 --flip on the board, v10.168: 640x480 at 16 and 32 bpp, primary + 1 back buffer (3DDEVICE) + D32. Each of four check frames read back from the front buffer after Flip is the frame just rendered (PASS at both depths); 240 animated frames run at 60 frames/s at 16 bpp and 30 at 32 bpp (the CPU clear of 640x480x32 takes about 17 ms).

- PRIMARYSURFACE|FLIP|COMPLEX|3DDEVICE is accepted when the display start can move. The primary's description has no size: the display's is used. Back buffers get the scanout pitch. Z buffers and non-primary chains stay refused.
- On a flip DirectDraw 7 swaps the chain's CreateSurfaceEx handles together with their memory, then sends D3DDP2OP\_SETRENDERTARGET with the back buffer's new handle. So a handle keeps the memory it was registered with; only a target bound by object at ContextCreate (no handle yet) follows its object's fpVidMem. Following the handle's object (v10.166/167) drew every second frame into the buffer being shown.

### 15.6 Textures in the HAL (v10.169 to v10.176)

VXDX7 --texture on the board (v10.175/176, 32 bpp): point sampling, WRAP, MIRROR, CLAMP, bilinear across an edge, a 64x64 texture loaded by Blt from system memory, 256x256, X8R8G8B8, R5G6B5, A1R5G5B5, A4R4G4B4, X1R5G5B5 and texture alpha into SRCALPHA/INVSRCALPHA all pass. XP's blend programs (16.3) give XP's exact pixels: MODULATE red x 808080 = 800000, MODULATE2X x 606060 = C00000, MODULATE4X x 303030 = C00000, ADD + 404040 = FF4040, ADDSIGNED + A0A0A0 = FF2020, MODULATE by TFACTOR 40C080 = 400000.

**Runtime trap (WRAP, v10.170-v10.175):** the DirectX 7 runtime keeps its own copy of D3DTSS\_ADDRESS and drops a write equal to it. After a program sets ADDRESSU/ADDRESSV to CLAMP, its copy of ADDRESS is still the default WRAP, so SetTextureStageState(ADDRESS, WRAP) never reaches DrawPrimitives2 and the stage stays on CLAMP. The record trace (VX\_D3D\_QueryTSS) showed the runtime's only address records: ADDRESS=1, ADDRESSU=1, ADDRESSV=1 (defaults), then ADDRESSU=3, ADDRESSV=3. Set ADDRESSU/ADDRESSV; D3DTSS\_ADDRESS (12) still arrives unsplit when it really changes and sets both. XP runs the same runtime.

| Part | What it does |
| --- | --- |
| Formats | A8R8G8B8, X8R8G8B8, R5G6B5, A1R5G5B5, A4R4G4B4, X1R5G5B5 in video memory, in XP's order (TEXTUREVIDEOMEMORY); power-of-two sides 1 to 1024 (to 2048, non-square, with L8 and DXT1/3/5, from v10.199); rows exactly width x bytes per texel (the sampler takes no pitch); dwTextureAlign 256 |
| CanCreate / Create | video textures of those formats (mip chains from v10.199, cube maps from v10.215); a system-memory texture stays the runtime's, its pitch from its own format |
| Blt into a texture | done on the CPU with the texel size (DirectDraw's own code cannot reach video memory): system copy to texture, colour fill, point stretch, mirror, source key |
| Lock / Unlock | waits for the engine, as render targets do |
| DrawPrimitives2 | texture handle in TSS 0 TEXTUREMAP, MAG/MINFILTER, ADDRESSU/V; D3DTSS\_ADDRESS (12) arrives unsplit and sets both U and V; texture coordinates from the FVF (TEXCOORDINDEX), clipped with the triangle |
| Packet | texture surface, filter, address codes and one (u,v) pair per vertex after the vertices; the VxD still accepts the original packet size |
| Hardware | general texture state as XP (FE02 00=11 01=21 08=010102 0D=810000 10=800000), texture 0 attributes, sampler 0, then XP's pixel program for the stage state (16.3, generated from the captures by tools/gen\_xp\_programs.py) and XP's 10-dword vertex layout (16.4); plain untextured diffuse keeps the 5-word path |
| Sampler address codes | 1 clamp, 2 wrap, 3 mirror, each axis proven on the board (16.6) |

### 15.7 Tests

- VX3DM1: native endpoint query; --draw 50 frames on the primary.
- VXDX7: device enumeration, the runtime's driver object, the HAL trace; --triangle flat and Gouraud into a 256x192 target, read back; --draw adds the D32 test; --flip 640x480 at 16 and 32 bpp, four check frames read from the front buffer after Flip, 240 timed frames.
- VXDX7 --texture: caps and formats; an 8x8 four-quadrant A8R8G8B8 texture written by Lock, point-sampled; coordinates 0..2 under WRAP, MIRROR and CLAMP; bilinear across a quadrant edge; 64x64 loaded by Blt from system memory; 256x256; X8R8G8B8; texture alpha 0x80 into SRCALPHA/INVSRCALPHA; MODULATE logged.
- CLXP3D (Windows XP, section 16): 68 Direct3D 7 cases with the command-ring capture built in.

### 15.7 Blending, alpha test, depth formats, clears and compatibility (v10.177/178)

Every D3DBLEND factor, every alpha-test function, and Z16 / D24S8 / D32 depth pass on the board (VXDX7 --raster, v10.177, 1024x768x32). Clears run on the 2D engine at 70 us per 256x192 X8R8G8B8 clear. DxDiag's Direct3D 7 and 8 tests pass.

| Feature | Recipe | Board result |
| --- | --- | --- |
| Blend factors | 01/34 = 10000 \| colour code << 4, 35 = 100 \| code << 2, 36 = 10400 \| alpha code << 4, 37 = 200 \| code << 2. Colour codes: 0 src colour, 1 dest colour, 2 src alpha, 3 dest alpha, 5 zero, 8 src alpha saturate, + 0x10 inverse (ONE = 0x15). Alpha codes: 1 src alpha, 2 dest alpha, 9 zero, + 0x10 inverse. BOTHSRCALPHA / BOTHINVSRCALPHA = the SRCALPHA/INVSRCALPHA pair. SRCALPHASAT is a source factor only. Caps SrcBlend 1FFF, DestBlend 3FF | 22 pairs against computed results: all pass (within 1 per channel) |
| Alpha test | 01/00 \|= 0x100000 (bit 20), 01/33 = (D3DCMP - 1) << 12 \| ref x 1024 / 255; AlphaCmpCaps FF | 9 cases (NEVER to ALWAYS, ref 0x90, alpha 80/90/A0): all pass |
| Z16 | 01/12 = 380004, 14 = FFFFFF, 2 bytes a pixel; clear value = the 32-bit fraction >> 16 | near/far, far-then-near, Z cleared to 0.5: pass |
| D24S8 | 01/12 = 3E0004, 14 = FFFF00 (the stencil byte left alone), 15 = FF; clear value & FFFFFF00, stencil 0; stencil not enabled | same checks: pass |
| Depth caps | ZBufferBitDepth DDBD\_16 \| DDBD\_32; ZPixelFormats Z16, D24S8, D32 in XP's order | EnumZBufferFormats lists all three |
| Clears | VX3DN\_SUBMIT of a clear packet goes to VX2D\_submit as a solid fill (ROP F0) at the desktop depth: a 16-bit surface on the 32-bit desktop as pixel pairs (even edges), a 32-bit one on the 16-bit desktop when both halves are equal; the rest, and whatever the 2D side declines (ACCEL\_FILL\_MIN, busy), goes to the CPU clear. The 2D wait reads the same status (0x400 & 1FF7) as the 3D emitter. VX2D\_sync after the fill hands the kept 2D context back. Registry ACCEL\_3D\_CLEAR (default 3 since v10.180): bit 0 = 2D-engine clears, bit 1 = D24S8 partial clears as two ROP fills (15.8) | partial clear with odd edges exact; 200 clears of 256x192 in 14 ms |

**Do not fail a batch for an unsupported state.** A DrawPrimitives2 that returns an error fails the application's EndScene (DxDiag's Direct3D 7 test on v10.176: 0x887602FB, D3DERR\_SCENE\_END\_FAILED, because its cube sets DITHERENABLE). Since v10.177 the core ignores states the chip need not reproduce (DITHERENABLE, ANTIALIAS, EDGEANTIALIAS, STIPPLEDALPHA, STIPPLEENABLE, RANGEFOGENABLE, CLIPPLANEENABLE), drew FOGENABLE and COLORKEYENABLE without the effect, and left out a primitive it cannot draw (stencil, an unsupported stage state). Later versions draw them: fog from v10.195-v10.196, the colour key from v10.197, stencil on D24S8 from v10.201 and dither from v10.205, each with an ACCEL\_D3D\_\* switch; a stencil draw without a D24S8 buffer is still left out, counting it (HAL diagnostics dropped / drop\_why). ValidateDevice still reports such a state.

**DX5/6-era device checks.** v10.178 reports raster caps ZTEST | SUBPIXEL, misc + MASKZ, shade + specular flat/Gouraud and the legacy texture blend caps DECAL MODULATE DECALALPHA MODULATEALPHA COPY ADD (the runtime maps TEXTUREMAPBLEND to stage 0). VXDX7 --enum lists the HAL through IDirect3D3, IDirect3D2 and IDirect3D on the desktop and at 640x480 in 8, 16 and 32 bpp; the HAL device renders at 16 and 32 bpp. WinTune 98's Direct3D workload still says "Could not find any compatible devices": DirectX 9 has no d3dref.dll, not a driver fault (15.8).

**Toolchain trap.** Open Watcom's dxguid.lib has IID\_IDirect3D2 wrong (6AAE1EC1-622A-..., the real one is 6AAE1EC1-662A-11D0-889D-00AA00BBB76A): QueryInterface returns E\_NOINTERFACE. Define the GUID in the source.

### 15.8 Video-memory alignment, audit fixes, D24S8 clears, WinTune (v10.179 to v10.181)

Every 3D surface must start on a 256-byte boundary, and DirectDraw's linear heap does not provide that by itself: since v10.179 the driver rounds every video-memory block. An external audit of v10.178 found seven real defects (all fixed) and four open families (one fixed since). WinTune 98's Direct3D test cannot run under DirectX 9 on any card: its first test needs the Null device, which DirectX 9 only offers with the SDK's d3dref.dll.

| Topic | Recipe or finding | Since |
| --- | --- | --- |
| Heap alignment | DirectDraw's linear heap packs blocks end to end and ignores dwZBufferAlign / dwOffscreenAlign. A 292x223 X8R8G8B8 back buffer (264032 bytes) put the Z buffer after it off the 256-byte boundary, and CreateSurfaceEx refused it (DDERR\_UNSUPPORTED, VXDX7 --wintune). Fix: VXCreate gives every video-memory block DDHAL\_PLEASEALLOC\_BLOCKSIZE = pitch x height rounded up to 256, and the heap starts on a 256-byte boundary (dddrv.c). Registry ACCEL\_HEAP\_ALIGN = 0 turns it off (VxD query FFFFFE50 out\[2\]). | v10.179 |
| Format codes | The internal X1R5G5B5 code and VX3DN\_D16 were both 12: a Z16 texture and an RGB555 Z buffer were accepted. X1R5G5B5 is 14. | v10.180 |
| MIN / MAG filter | Independent: sampler 31 enlargement bits 13 and 7, shrinking bits 10 and 4 (HCM p125). Point/point 010000, linear/linear 012490, MAG linear only 012080, MIN linear only 010410. Packet filter 2 = MAG linear, 3 = MIN linear. | v10.180 |
| XRGB target | Render target X8R8G8B8 = format 98 (no alpha, destination alpha reads 1.0), not 99 (A8R8G8B8): register 52 = 98xxxx as XP writes (HCM p95). | v10.180 |
| NULL texture | NULL texture with COLORARG1 = TEXTURE ends the stage before the unused arguments, alpha or stage 1 are judged: the draw is diffuse. | v10.180 |
| Lost surfaces | An object-bound target or depth whose memory became invalid, misaligned or out of range marks the context lost (it drew into the old address). | v10.180 |
| Legacy buffers | Execute/TL base + offset is checked for 32-bit wrap before the readable check. | v10.180 |
| D24S8 clears | DP2 CLEAR takes D3DCLEAR\_STENCIL; the value rides in the packet's clear\_flags bits 15:8. Z+stencil = one solid fill. Z-only keeps the stencil byte and stencil-only keeps Z: two 2D fills with ROPs that read the destination, D AND \~mask (ROP A0, fg = \~mask), then D OR value (ROP FA). The CPU path reads, merges and writes. No stencil storage (D16/D32): a stencil clear does nothing. | v10.180 |
| Texture op caps | 0x18FF from the SDK names (DISABLE to ADDSIGNED, BLENDDIFFUSEALPHA 0x800, BLENDTEXTUREALPHA 0x1000). The earlier 0xCFF claimed ADDSMOOTH (0x400) and left out BLENDTEXTUREALPHA. | v10.181 |

**WinTune 98's Direct3D test (WinTunDD.ocx, disassembled).** It is the DirectX 6 SDK framework (D3DEnum + CD3DFramework). The Null test (DiscoNull) picks the device whose name is exactly "Null device", with no fallback, and the Primary test (our HAL) runs only after it. WinTune creates EnumNullDevice = 1 under HKLM\\Software\\Microsoft\\Direct3D\\Drivers only if the value is missing. DirectX 9's D3DIM700.DLL (4.09.00.0902) honours that value but then loads d3dref.dll and asks it for GetRefHalProvider, for the Null device and the reference rasterizer alike. d3dref.dll ships with the DirectX SDK, not the runtime, so neither device appears and WinTune says "Could not find any compatible devices". Not the driver. VXDX7 --wintune repeats WinTune's enumeration and windowed set-up step by step. Attaching a Z buffer to a windowed primary fails with DDERR\_NOEXCLUSIVEMODE for Microsoft's RGB device too.

**Toolchain trap, more GUIDs.** Open Watcom's dxguid.lib also lacks a correct IID\_IDirectDraw4 and IID\_IDirect3DRGBDevice. Define 9C59509A-39BD-11D1-8C4A-00C04FD930C5 and A4665C60-2673-11CF-A31A-00AA00B93356 in the source. DirectDrawEnumerateA returns an HRESULT, not a BOOL.

**Open from the audit.** Closed in v10.198: stage-0 programs are built from the decoded instruction fields (16.3), so every argument, the alpha operation independently of the colour one, specular on every program and the COMPLEMENT / ALPHAREPLICATE modifiers XP was captured on are drawn. Checked and correct: fan flat shading uses vertex i+1, and the DX5-style Clear callback is D3DHAL\_CLEARDATA (DWORD depth, no stencil).

### 15.9 DirectX 3/5 programs, fog, colour key, built programs (v10.182 to v10.198)

Tunnel (DirectX 5 SDK, execute buffers) and WinTune 98's HAL triangle test run on the board; Tunnel is fastest on Ramp, then HAL, then RGB. WinTune's sphere test runs slowly and flashes; per-triangle submission was the cost. Batching (v10.214), the command fetcher and the later work in 16.18 took the sphere test from 128 s to 52 s.

| Build | Finding | Fix |
| --- | --- | --- |
| v10.188 | D3DIM.DLL 4.07.00.0700 (the DirectX 3/5/6 interfaces) sizes its TL buffer at every Execute to max(count, hwCaps.dwMaxVertexCount) x 32 bytes + 1 KB; 65535 made every Execute fail with E\_OUTOFMEMORY | dwMaxVertexCount = 4096 |
| v10.188 | D3DIM passes the application's execute buffer straight to DrawPrimitives2; ops it does not map (STATELIGHT 7, PROCESSVERTICES 9, EXIT 11) come back as D3DERR\_COMMAND\_UNPARSED, which is the normal protocol | none needed |
| v10.190 | D3DDP2OP\_TRIANGLEFAN\_IMM (23): a DWORD of edge flags, then n + 2 inline vertices of the current FVF; clipped polygons use it (Tunnel's "RenderScene failed") | drawn as a fan from the inline vertices |
| v10.194 | An untextured program (T13, T14) sent with the textured header hung the 3D engine (stage 3), the 2D engine was then marked lost and the display froze (WinTune) | untextured header 90 = 058000, 91 = 0, 92 = count, 93 = 0, FE/00 = 1 |
| v10.195/196 | Fog (16.7); Tunnel's fog proven | 01/00 F00, fog caps |
| v10.197/198 | Colour key (16.7) and the program builder (16.3) | packet 1912 bytes: VXD and DLL ship together |

HAL event log (v10.185 on): a ring of 16 plus C:\\VXD3D.LOG, one line per event, flushed: `#n t= pid= cb= hr= why= a b c`. Callbacks: 1 ContextCreate (why = reason), 2 DrawPrimitives2 (why = opcode, a = error offset), 5 Validate, 6 CreateSurfaceEx, 7-10 a packet the VxD refused (result, offsets, first vertex, state), 11 fog (why = register 70 | program count << 8, a b c = the three vertices' specular). VxD last\_result for a lost draw carries the stage in bits 15:8: 1 busy before the packet, 2 after the state, 3 + n after triangle n.

Registry: ACCEL\_D3D\_CKEY (default 1; 0 draws keyed textures without the key).

## 16. The Windows XP driver's 3D command stream (CLXP3D, captured 2026-10-22)

Windows XP's own VIA driver (Chrome9 HCM IGP, 6.14.0010.0293, DirectX 9.0c, 800x600x32, 256 MB frame buffer) was captured drawing 68 Direct3D 7 cases, one state change each. Every capture holds the exact words the driver queued, so each recipe below is XP's, read from the stream; the pixel results (CLXP3Dnn.BIN) are the reference images.

### 16.1 How the capture works

- XP feeds the 3D engine from a command buffer in GPU address space. MMIO (BAR1, F0000000 here) 0x43C is the driver's write cursor; 0x440 = 0x43C - 0x40 and 0x45C = 0x43C - 0x10 move with it. 0x43C points just past the last word written.
- GPU addresses go through a page table. SR6A/SR6B/SR6C hold its location as a **frame-buffer offset**: offset = (SR6C bit 0) << 28 | SR6B << 20 | SR6A << 12 (0FF80000 on XP, the top 512 KB of 256 MB). Read it through BAR0 (D0000000 here): physical DFF80000. The Win98 lab programs the same registers with its own table in video memory.
- Entry n (4 bytes) maps GPU page n: physical address = (entry & 0x000FFFFF) << 12; the top byte is 01 for a used entry (01005DA5 = page 05DA5000). Unused entries read FFFFFFFF. XP's command buffer sat around GPU 0F1D0000-0F240000, in ordinary RAM.
- The fetcher's registers (type 0x10 through 0x41C/0x420: 60-68) and XP's ring start, kick, wrap and one-shot sequences, read from s3ginv.dll: 16.17.
- Per case: flush (lock the target), read MMIO 0x400-0x5FC, draw, lock the target again (the driver submits and waits), read MMIO again, dump the GPU range between the two cursors through the table. XP advanced 0x400 to 0x1200 bytes per case and nothing while idle.
- On XP, \\Device\\PhysicalMemory maps RAM only: MMIO and BAR0 fail with C00000F4, inpout32's MapPhysToLin fails the same way, and inpout32's 32-bit port calls return FFFFFFFF (PCI configuration unreachable). CLXP3D therefore installs CXPHYS.SYS for the run, a read-only kernel reader (MmMapIoSpace, one IOCTL 0x00226004), and takes BAR0/BAR1 from the registry (Enum\\PCI\\VEN\_1106&DEV\_5122...\\Control\\AllocConfig).

### 16.2 Stream format

| Header | Meaning |
| --- | --- |
| FE020440 0000043C hhhh0000 CC000000, then words | register writes: header hhhh0000 to 0x43C (type in bits 31:16), then each word is reg << 24 \| value (24 bits) to 0x440, until the next FE word |
| FE03043C nnnnnnnn hhhhhhhh CC000000, then n words | a block of n data words under header hhhhhhhh (programs, constants, vertices), padded with CC000000 |
| FE000000 nnnnnnnn 00000000 CC000000, then n pairs | 2D engine register writes (offset, value) |
| FE020420 0000041C 00300000 CC000000, then CCC00008 | padding / fence between batches |
| FE010000 ... | data fed to a 2D host-data command (seen in Z clears) |

Block headers seen: 20030000 ALU program, 21030000 TAU (texture-fetch) instructions, 2203xxxx constants (index in the low word, 4 words per constant), 1003xxxx sampler blocks (8 zero words each), 1203/1403/1503/0003 tables (init only), 00000000 vertices. XP only re-sends state that changed, so case 0 (T00) holds the full setup.

### 16.3 Pixel programs for texture blending (stage 0)

A program is 1 to 3 instructions of 4 words, sent as block 20030000 after the control registers: 01/90 = 018000, 01/91 = 000001, **01/92 = 01000n (n = instruction count)**, 01/93 = 000001, 01/94..9A = 0, 01/9B = a tag XP lowers by one for every new program (FFFFFF, FFFFFE, ...). Textured programs add TAU block 21030000 = 301001B0. One instruction with write mask F (word 2 = 00Fxxxxx) does colour and alpha; otherwise an RGB instruction (mask 7, word 2 = 007xxxxx) comes first and an alpha instruction (mask 8, 008xxxxx) second. Word 2 bits 15:8 are the A source register and bit 17 marks the A operand's last read (16.12: B has w1 bit 21, C w0 bit 25) (the field encoding below, decoded from T68-T115, replaces the earlier reading of these bits as source selects). Word 3 bits 14:13 scale the result (1 = x2, 2 = x4); 1C20 instead of 0820 writes a temporary for the next instruction.

| Case | Stage 0 state | Program (instructions separated by /) |
| --- | --- | --- |
| T00 | colour and alpha = texture | 01C00530 301C0053 00F20005 000D0820 |
| T13 | colour and alpha = diffuse | 01C00530 301C0053 00F04005 000D0820 |
| T02, T03 | MODULATE texture x diffuse (or CURRENT), colour and alpha | 0140B6D0 30040053 00F20005 00000820 |
| T01 | colour MODULATE texture x diffuse, alpha = texture (the DirectX 7 default stage) | 0140B6D0 50040055 00700005 00000820 / 01C00530 B01C0053 008200B6 040D0820 |
| T07 | MODULATE2X | as T01 with word 3 of the first instruction 00002820 |
| T08 | MODULATE4X | as T01 with word 3 of the first instruction 00004820 |
| T06 | ADD texture + diffuse | 00400550 50140924 00700005 00000820 / alpha as T01 |
| T09 | ADDSIGNED | 00400500 01140924 00700005 00100820 / alpha as T01 |
| T10 | BLENDTEXTUREALPHA (texture, diffuse) | 00006DB0 00040050 00700005 00110820 / alpha as T01 |
| T11 | BLENDDIFFUSEALPHA (texture, diffuse) | 00406DB0 00040050 00700005 00110820 / alpha as T01 |
| T12 | MODULATE texture x TFACTOR | 0140B6D0 50082055 00700005 00000820 / alpha as T01; TFACTOR as constant block 22030008 = r g b a floats (0x40,0xC0,0x80,0xFF gave 3E808081 3F40C0C2 3F008081 3F800000) |
| T04 | colour = texture, alpha = diffuse | 01C00530 501C0053 00720005 000D0820 / 01C00530 B01C0053 008040B6 040D0820 |
| T05 | colour = diffuse, alpha = texture | 01C00530 501C0053 00704005 000D0820 / 01C00530 B01C0053 008200B6 040D0820 |
| T14 | diffuse + specular, no texture | 00410550 50140924 00704005 00000820 / 01C00530 B01C0053 008040B6 040D0820 |
| T15 | MODULATE texture x diffuse + specular | 0140B6D0 50040055 00700005 00001C20 / 00410550 50140924 00720C05 00000820 / 01C00530 B01C0053 008200B6 040D0820 (01/92 = 010003) |
| T24 | stage 0 texture, stage 1 MODULATE texture 1 x CURRENT | 01C00530 301C0053 00F20005 000D0C20 / 0140B6D0 5000C055 00720105 00000820 / 01C00530 B01C0053 00820CB6 040D0820; TAU 301001B0 311111B1; 01/91 = 3, 01/92 = 020003, 01/93 = 2 |
| T25 | stage 0 MODULATE diffuse, stage 1 ADD texture 1 | 0140B6D0 50040055 00700005 00001C20 / 020C0550 50140924 00720105 00000820 / 01C00530 B01C0053 008200B6 040D0820; TAU as T24 |
| T45 | texture colour key (COLORKEYENABLE) | 6 instructions (24 words): 80000550 00140924 0070AB05 00000C20 / 000C0558 5000C055 00700C05 00090C20 / 01C00530 5008024D 00F00C05 00021C20 / 800C0550 00140924 00708100 00000420 / 01C00530 301C0053 00820C05 040D0420 / 01C00530 301C0053 00F20005 000D0820; TAU 201001B0 314001B1; constants 22030000 = 0.5 1 2 4 / 1/512 0 0 0, 220300AC = 1 0 1 1 |
| T62, T64 | lines, points (no texture) | 01C00530 301C0053 00F04005 000D0820 with 01/90 = 048000, 01/91 = 0, 01/92 = 000001, 01/93 = 0 |

Pixel results (reference): MODULATE of red texel and 808080 diffuse = 800000; ADD of red and 404040 = FF4040; MODULATE2X with 606060 = C00000; MODULATE4X with 303030 = BF0000; ADDSIGNED with A0A0A0 = FF2121; MODULATE by TFACTOR 40C080 on white = 40C080.

**Instruction encoding (decoded from CLXP3D T00-T15, T68-T115; v10.198 builds every program from it).** Each instruction computes A x B + C. The driver rebuilds 60 captured programs word for word from their Direct3D state, and the built programs pass on the board (v10.198).

| Field | Where |
| --- | --- |
| C register, swizzle | word 0 bits 23:16, bits 15:4 |
| B register, swizzle | word 1 bits 19:12, bits 11:0 |
| A register, swizzle | word 2 bits 15:8; swizzle = (word 2 bits 7:0) << 4 \| word 1 bits 31:28 |
| Write mask | word 2 bits 23:20: 7 colour, 8 alpha, F both |
| Last texture read | word 2 bit 17 = A's last read of a texture or temporary; word 1 bit 21 = B's, word 0 bit 25 = C's (16.12) |
| Destination | word 3 bits 11:8: 8 output, C temporary (bit 12 also set when C is a real register) |
| Scale, alpha | word 3 bit 13 x2, bit 14 x4; bit 26 on alpha instructions |

Registers: 00 texture, 40 diffuse (CURRENT at stage 0), 41 specular, 82 constant c2 (TFACTOR), C0 none, 0C temporary. Swizzles are four 3-bit selectors: 0-3 x y z w, 4 one, 5 zero; 055 = rgb, 053 = rgba, 6DB = alpha replicated, 924 = 1, B6D = 0.

| Op (arg1 X, arg2 Y) | Colour instruction | From |
| --- | --- | --- |
| SELECTARG | 01C00530 501C0053 0070**X**05 000D0820 | T82, T05 |
| X \| COMPLEMENT | 0140B6D0 02140924 00700005 00100820 | T105 |
| X \| ALPHAREPLICATE | 01C00530 D01C0053 0070006D 000D0820 | T107 |
| MODULATE (2X, 4X: word 3 + 2000, 4000) | 0140B6D0 50040055 00700005 00000820 | T01, T12, T114 |
| MODULATE X, Y \| COMPLEMENT | 2140B6D0 00040050 00700005 00100820 | T104, T111 |
| MODULATE X \| ALPHAREPLICATE, Y | 0140B6D0 D0040055 0070006D 00000820 | T106 |
| MODULATE X \| COMPLEMENT, Y \| ALPHAREPLICATE | 0140B6D0 020406DB 00700005 00100820 | T109 |
| MODULATE X, Y \| ALPHAREPLICATE | 0140B6D0 500406DD 00700005 00000820 | T112 |
| ADD | 00400550 50140924 00700005 00000820 | T06, T115 |
| ADD X, Y \| COMPLEMENT | 00400502 00140924 00700005 00100820 | T110 |
| ADDSIGNED (2X: + 2000) | 00400500 01140924 00700005 00100820 | T09, T97 |
| SUBTRACT | 80000550 00140924 00704005 00000820 | T96 |
| ADDSMOOTH | 00000500 02040050 00700005 00100820 | T98 |
| BLENDDIFFUSEALPHA (and CURRENTALPHA) | 00406DB0 00040050 00700005 00110820 | T11, T100 |
| BLENDTEXTUREALPHA | 00006DB0 00040050 00700005 00110820 | T10 |
| BLENDFACTORALPHA | 00826DB0 00040050 00700005 00110820 | T99 |
| MODULATEALPHA\_ADDCOLOR (and BLENDTEXTUREALPHAPM, MODULATEINVALPHA\_ADDCOLOR, T138, T139: 00000500 B2040050 0070006D 00100820) | 00000550 D0040055 0070006D 00000820 | T101 |
| MODULATECOLOR\_ADDALPHA | 00006DD0 50040055 00700005 00000820 | T102 |

Arguments go in by register: MODULATE takes X in A and Y in B; ADD takes X in A and Y in C (B is the constant 1); SUBTRACT takes Y in A and X in C. Commutative ops put the texture first, as XP does.

**Alpha instruction** = the colour template with mask 8, word 3 + 04000000, and each swizzle moved to alpha: 055, 053 and 6DD become B6B; 050 becomes 6DB; constants stay. This reproduces all six captured alpha forms (T04, T05, T79, T84, T108, T113).

**Fold.** The same op and arguments for colour and alpha (SELECT, MODULATE, ADD, no modifiers) is one instruction: mask F, 055 swizzles become 053 (T00, T02, T13, T86).

**Specular.** The colour instruction writes the temporary, then 00410550 50140924 00720C05 00000820 adds specular (C = 41) and the alpha instruction follows (T15, T89-T93). A plain SELECT feeds the add directly: 00410550 50140924 0070**X**05 00000820 (T14, T94, T95). T88 and T91 are XP's own folded variants.

**Colour key.** XP puts five key-test instructions in front of any program without a temporary (T68-T77): 92 = 020000 | count, 93 = 3, 95 = 010, TAU 201001B0 314001B1. With specular (T73) it lays the program out differently.

DOTPRODUCT3 is decoded in 16.12. Not decoded: YUY2 (T152). Bump mapping (T146-T148) and the T73 key-plus-specular layout are in 16.14, cube maps (T150) in 16.15.

### 16.4 Vertex layout

XP sends every vertex as 10 dwords in a block with header 00000000, whatever the FVF: **X Y Z W diffuse specular u0 v0 u1 v1**, W = the vertex's rhw (0.25 on the far edge of the perspective floor, T19), so perspective correction comes from W. The draw is 04/24 = vertex count (6 for two triangles) then the block. Layout registers (type 04), as XP sets them:

| Reg | XP value | Win98 lab path today |
| --- | --- | --- |
| 04/00 | A8000A (10 attributes) | A80005 untextured, A80007 textured |
| 04/01 | 020100 | 020100 |
| 04/02 | 060503 | 000503 / 080503 |
| 04/03 | 0C0908 | - / 09 |
| 04/04 | 00000D | - |
| 04/23 | 00CE02 triangles, 00CE01 lines, 001E00 points | 00CE02 |
| 04/25 | F6CC00 | F40000 / F4C000 |
| 04/28 | FFC000 one texture, FFCC00 two textures, FF0000 lines | FD0000 / FDC000 |
| 04/2A, 04/2B | 3F8000, 3F8000 (1.0 in the 24-bit float format) | never written |
| 04/2F | 582000 | 582000 |
| 04/40..48 | 800000 400000 00493F 800000 400000 00493F 800000 000000 00003F (lines: 46 = 7FFF00; points: 41/44 = 0, 42/45 = 3F) | the triangle values |
| 04/50..52 | 000000 000000 147FFF | same |

### 16.5 Texture and sampler registers

| Group | Registers (XP values) |
| --- | --- |
| General texture state (type FE02) | 00 = 000011 one texture, 000021 + 01 = 002521 for two; T00 full: 00=11 01=21 08=010102 0D=810000 10=800000, the rest 0 |
| Texture n attributes (type n02: 02, 102, ...) | 00 = base >> 8; 18 = 001100; 20 = height << 12 \| width; 21 = 000001; 22 = log2 height << 4 \| log2 width (builds before v10.199 had these swapped, so only square textures worked) (+ bit 15 for DXT); 30 = format |
| Sampler n (type 20n2: 2002, 2102) | 30 = 000000 (mipmaps: 000140; LOD bias +1: 020140); 31 = filter \| 010000; 32 = address \| 80 (mipmaps: 090092); 35 = 0; 36 = 0 |
| Filter (31) | point 010000; bilinear and mip point 012491; mip linear 012492; texture 1 bilinear 012490; anisotropic 020820 |
| Address (32) | U in bits 2:0, V in bits 5:3: 1 clamp (089), 2 wrap (092), 3 mirror |
| Sampler block | 10030000 + 8 words per texture draw, zero without mip levels (the level offsets otherwise, 16.5) (T00 sent 16 blocks, 10030000 to 10030078) |

Texture format codes (texture register 30): A8R8G8B8 990000, X8R8G8B8 980000, R5G6B5 890000, A1R5G5B5 8A0000, A4R4G4B4 8B0000, X1R5G5B5 880000, L8 130000, DXT1 290000, DXT3 2A0000, DXT5 2B0000 (DXT also sets bit 15 of register 22: 008033). Mipmaps (32x32, 6 levels): 00 = base >> 8, 20 = 020020, 22 = 000055; the per-level addresses were not in that stream. The driver puts them in block 10030000 from v10.199: each level's offset from level 0 in 256-byte units, 9 levels at most.

### 16.6 WRAP: solved (v10.176)

The sampler's address codes are right and the hardware wraps: on the board, forced sampler 32 = 00008A (U wrap, V clamp) and 000091 (U clamp, V wrap) give exactly the expected pixels, both on a 0..2 gradient quad and on patches whose four vertices share one (u,v). The Win98 failure was the test's state never reaching the driver (15.6, runtime trap), not a register.

Measured along the way (v10.174/175 experiment, 8x8 A8R8G8B8, point sampling): texture register 22 bit 15 (HTXnPower2), XP's 12030000 table and FE 0B = 0C (wrap correction S/T) change nothing for this case. FE 0D with a texture-A enable (000001, 030001) turns every out-of-range sample into the first texel: leave 0D at 810000. Code 0 is BORDER, not wrap.

### 16.7 Blending, alpha test, fog, shading

With blending on, XP sets 01/00 = 0000FF (otherwise 00000F) and writes 01/34-37 (01/38-3F = 0):

| Case | Source / destination | 01/34 | 01/35 | 01/36 | 01/37 |
| --- | --- | --- | --- | --- | --- |
| T04, T43 | SRCALPHA / INVSRCALPHA (BOTHSRCALPHA gives the same) | 010020 | 000148 | 010410 | 000244 |
| T37 | ONE / ONE | 010150 | 000154 | 010590 | 000264 |
| T38 | DESTCOLOR / ZERO | 010010 | 000114 | 010420 | 000224 |
| T39 | ZERO / SRCCOLOR | 010050 | 000100 | 010490 | 000204 |
| T40 | INVDESTCOLOR / INVSRCCOLOR | 010110 | 000140 | 010520 | 000244 |
| T41 | DESTALPHA / INVDESTALPHA | 010030 | 00014C | 010420 | 000248 |
| T42 | SRCALPHASAT / ONE | 010080 | 000154 | 010590 | 000264 |

| Feature | Case | XP registers |
| --- | --- | --- |
| Alpha test GREATEREQUAL 0x90 | T44 | 01/00 = 10000F (bit 20 on), 01/33 = 006242 |
| Vertex fog (factor in specular alpha) | T16 | 01/00 = 000F0F, 01/71 = 181202, 01/72 = 000101 |
| Table fog LINEAR, start 0.2, end 0.8 | T17 | 01/00 = 000F0F, 01/70 = 000032, 73 = 003E4C, 75 = 00000A, 76 = 00007F, 77 = 003F4C, 78 = 100000 |
| Table fog EXP, density 2 | T18 | 01/00 = 000F0F, 01/70 = 000034 |
| Fog defaults (T00) |  | 01/70 = 000030, 71-73 = 0, 75 = 0, 76 = 00007F, 77 = 003F80, 78 = 0FE000 |
| Flat shading | T48 | 01/1E = 0, 01/1F = 0, 01/20 = 007FFF |
| Dithering | T49 | 01/53, 5B, 63, 6B = 000C0F (T00: 010C0F); 01/00 = 0F on every draw (bits 3:0 = dither enable, RT0-RT3; 16.13) |
| Culling, wireframe | T65, T66 | no register change: XP culls and builds the edges itself and sends triangles or lines |

**Fog, decoded (external review of s3ginv.dll's fog conversions and HCM pp105-106; every value below reproduces the T16-T19 captures).** The fog colour is in the stream: 10-bit fixed point per channel, 1.0 = 0x400 (c x 1024 / 255), 01/71 = G << 12 | B, 01/72 = R. 406080 gives 71 = 181202, 72 = 000101. Earlier versions of this section said the colour was absent; that was wrong.

| Register | Meaning |
| --- | --- |
| 01/00 bits 8-11 | Fog enable per render target (RT0 = bit 8). Fog off = clear these; 70 stays as it is (T19). |
| 70 bit 7 | Fog in the pixel shader; clear for the fixed fog unit. |
| 70 bit 6 | Use the separate fog attribute as the coordinate. |
| 70 bit 5 | Vertex fog factor from Color2 (specular) alpha. |
| 70 bit 4 | Z (set) or W (clear). |
| 70 bit 3 | Reverse the fog equation. |
| 70 bits 2:0 | 0 vertex, 2 LINEAR, 4 EXP, 5 EXP2 (not the D3D enum). XP: 30, 32, 34, 35. |
| 73, 77 | Start and end: (bits(float) >> 16) & 7FFF (0.2 = 3E4C, 0.8 = 3F4C). |
| 75, 76 | 1 / (end - start): 75 = four fraction bits (q >> 19) & F, 76 = biased exponent (q >> 23) & FF; 1/0.6 gives 0A, 7F. |
| 78 | Density: ((bits(density) >> 18) & 1FFF) << 8; 1.0 = 0FE000, 2.0 = 100000. |
| 74 | Reserved. |

Vertex fog needs the specular DWORD's alpha routed to Color2 (the 10-dword layout already carries it). Proven on the board (v10.196): Tunnel's vertex fog (LINEAR 6 to 11, black, set through STATELIGHT, D3DIM puts the factor in specular alpha) draws on the HAL as on RGB. Fog enable is 01/00 bits 11:8 = F00 as XP writes it (000F0F); the HAL advertises FOGVERTEX, FOGTABLE and FOGGOURAUD. Plain diffuse with vertex fog runs the T13 program so the specular DWORD is sent. EXP2 (T129), W fog (T133) and fog on other programs (T130-T135) are captured, not yet compared.

**Texture colour key (T45) is a pixel program, not a register.** Checkerboard 8x8 A8R8G8B8 with magenta FFFF00FF texels, SetColorKey(SRCBLT, FFFF00FF), COLORKEYENABLE: XP draws the keyed texels as the clear colour (rejected), 01/00 stays 00000F. Program: 90 = 018000, 91 = 1, 92 = 020006, 93 = 3, 95 = 010, 9B = FFFFED; 20030000 = 24 words (6 instructions: 80000550 00140924 0070AB05 00000C20 / 000C0558 5000C055 00700C05 00090C20 / 01C00530 5008024D 00F00C05 00021C20 / 800C0550 00140924 00708100 00000420 / 01C00530 301C0053 00820C05 040D0420 / 01C00530 301C0053 00F20005 000D0820); TAU 21030000 = 201001B0 314001B1; constants 22030000 = 0.5 1 2 4 1/512 0 0 0 and 220300AC = 1 0 1 1 (the key as floats; RGBA or BGRA unresolved, magenta reads the same). Every other program has 92 = 010000 | count, 93 = 1, one TAU word and constants only at 22030008. The program's schedule (HCM pp109-112, external review): 92 = 020006 = two TAU and six ALU instructions; 93 = 3 switches TAU to ALU after TAU 0 and TAU 1; 95 = 010 switches ALU to TAU after ALU 4. So TAU0, ALU0-4 (key match and a dependent discard), TAU1, ALU5 (the plain texture output, byte for byte). Constant headers address DWORDs: 22030000 = c0 (0.5, 1, 2, 4) and c1 (1/512, 0, 0, 0); 220300AC = c43 (0xAC / 4), the key. XP's key-bounds code uses key -/+ 1/512 in RGB order. Settled by T68-T77: c43 is R, G, B, A (key 2182C6 gives 0x21/255, 0x82/255, 0xC6/255); alpha is not compared (a key given with alpha 00, T69, and an R5G6B5 key with alpha 0, T74, both key); 16-bit keys are each channel over its own maximum (R5G6B5 2418 = 4/31, 32/63, 24/31). The first five instructions are the key test and go in front of any program without a temporary (16.3). The driver has drawn the key since v10.197 (v10.198: every program but specular); proven on the board (v10.198, VXDX7 --texture): key 2182C6 keyed and its reverse C68221 drawn on A8R8G8B8 (key alpha FF and 00) and R5G6B5, key off draws both.

### 16.8 Depth and stencil

| Case | XP registers (type 01) |
| --- | --- |
| T50 Z16, ZFUNC LESS, write on | 01 = CE374C, 10 = 003306, 11 = 020000, 12 = 380004, 13 = 010000, 16 = 0, 17 = 000080, 29 = 018010, 2A = 003487, 50 = 003001, 52 = 980020 |
| T52 D24S8, stencil REPLACE then NOTEQUAL / INCR | 01 = CE375C, 11 = 040000, 12 = 3E0004, 13 = 030000, 14 = FFFF00, 15 = 0000FF, 23 = 01FFFF, 24 = 070002, 25 = 01FFFF, 26 = 070002, 28 = 003607, 29 = 418410, 2A = 0036C8 |
| T53 ZBIAS 8 (on Z16) | 14 = FFFFFF, 15 = 0000FF, 16 = 03126F, 17 = 0000BA |
| T54 ZFUNC GREATER, write off, Z cleared to 0.5 | 01 = CE1548, 13 = 040000, 14 = 008000, 15 = 000080, 16 = 03126F, 17 = 00003A |

01/28 is the stencil plane's offset >> 8, 01/29 its pitch >> 4 with the Coarse Z bits, and 01/2A the Coarse Z storage's base >> 8, which XP puts 256 past the plane's end (section 17). In T52 the plane starts 256 bytes after the Z words (28 = 003607 with Z at 330600 in this capture). With Z16 XP still writes 29 = 018010 and 2A = Z end + 256: no plane, and the Coarse Z storage right after the Z words.

### 16.9 Clears

XP clears with the 2D engine, not the 3D engine. Target (T55, T56): 2D writes 04 = 00000300, 48 = 2000, 58 = 4C = colour, 14 = 00060020, 08 = 00800000, 0C = (height - 1) << 16 | (width - 1), 10 = y << 16 | x, 00 = F0002001, 48 = 0; then 6C = 00400000 and a second small fill (00 = F0002001, 58 = a fence count). Z (T57, T58): 00 = CC002041 with 0C = 0005007F and the clear value fed through an FE01 host-data packet (Z16 0.5: 20002000 words; D24S8: 10001000 and FFFFFFFF words). The Win98 driver does the same since v10.177 (15.7): 70 us for 256x192x32, against 17 ms at 640x480x32 for the CPU clear.

### 16.10 What XP's driver offers (CLXP3D.TXT)

- Device caps 000BB6F3; texture caps 00000D4F (perspective, power of two, alpha, ...); filter caps 0703073F; address caps 00000017 (wrap, mirror, clamp, independent U/V; no border); raster caps 003373B1 (fog table and vertex, ...); every blend factor (1FFF); 8 stages, 8 textures, texture ops 00FFFFFF; anisotropy 16; textures 1x1 to 2048x2048.
- Texture formats: X1R5G5B5, R5G6B5, X8R8G8B8, A1R5G5B5, A8R8G8B8, A4R4G4B4, L8 (flags 00020000; listed here earlier as A8), YUY2, DXT1 to DXT5, V8U8, L6V5U5, X8L8V8U8. No palettized format, no A8, no A8L8 (T136 skipped: A8 not offered).
- Depth formats: Z16, D24S8 (32 bits), D32. No plain 24-bit Z.

**Later capture runs (CLXP3D v2.4 to v2.6, 2026-10-23)**, decoded with tools/dec.py, in captures/clxp3d\_v24, \_v25 and \_v26:

| Cases | What | Used |
| --- | --- | --- |
| T68-T77 | colour key with other programs, asymmetric key, alpha-00 key, R5G6B5 | v10.198 |
| T78-T87 | colour and alpha set independently | v10.198 |
| T88-T95 | specular on every program | v10.198 |
| T96-T103 | SUBTRACT, ADDSIGNED2X, ADDSMOOTH, BLENDFACTOR/CURRENTALPHA, MODULATEALPHA\_ADDCOLOR, MODULATECOLOR\_ADDALPHA, DOTPRODUCT3 | v10.198; DOTPRODUCT3 v10.206 |
| T104-T115 | COMPLEMENT, ALPHAREPLICATE, SPECULAR as an argument | v10.198 |
| T116-T125 | two stages: light map, ADD, MODULATE2X, BLENDCURRENTALPHA, stage-1 alpha, shared coordinates, specular, no texture 1, SUBTRACT, colour key | v10.206 (16.12) |
| T126-T128 | 1024x1024, 2048x2048, 2048x8 textures | v10.199 |
| T129-T135 | fog EXP2, on MODULATE, untextured vertex and table, W fog, with specular, with colour key | not yet |
| T137 | L8 with MODULATE | v10.199 |
| T138-T145 | BLENDTEXTUREALPHAPM, MODULATEINVALPHA\_ADDCOLOR, MODULATEINVCOLOR\_ADDALPHA, DOTPRODUCT3 forms, PREMODULATE | v10.206 (16.12) |
| T146-T148 | bump mapping: two TAU words with a dependent read (201001B0 311001B1), bump texture format code 38 | v10.210-v10.211 (16.14) |
| T149 | three stages | v10.206 (16.12) |
| T150 | cube map: texture registers 00-05 = the six face bases, 18 = 001300 | v10.215 (16.15) |
| T151 | projected texture: TAU 307001A0 | v10.208 (16.14) |
| T152 | YUY2: format 300001, sampler 35 = 200040, 36 = 200000 | not yet |

### 16.11 Open

- YUY2 textures (T152; needs a CLXP3D capture with real YUY2 data). Coordinate set 1, colour key with a program, bump mapping and projected textures are done (16.14), cube maps too (16.15).
- Batching: done (192-vertex packets from v10.235, 32 packets a call from v10.275; 16.18).
- Whether the 01/9B program tag must change for the engine to load a new program (XP always changes it; the Win98 path lowers it on every new program since v10.172).
- The DDHELP fault in DIBENG.DLL seen after WinTune died (board, v10.177).

### 16.12 Two and three texture stages, DOT3, register last reads (v10.206)

The composer rebuilds all 16 two-stage, three-stage and new-op captures word for word, and 16 of 17 stage checks gave XP's pixels on the board (v10.206, VXDX7 --texture). The one failure was coordinate set 1, solved in v10.209 (04/28 needs FFCC00 when a unit reads set 1; 16.14).

| Item | Encoding |
| --- | --- |
| Header | 01/90 = 018000; 91 = unit mask (1, 3, 7); 92 = units << 16 \| ALU count; 93 = 1, 2, 4 for 1, 2, 3 units (HCM p110: switch TAU to ALU after TAU n) |
| TAU block 21030000 | one word per unit: 301001B0 + u x 01011001 (t0 301001B0, t1 311111B1, t2 321221B2) |
| FE02/00 | units << 4 \| 1 (HCM p128: number of textures in 7:4, bit 0 clears the cache) |
| FE02/01 | byte u = 21 \| set << 2: dimension 1:0 (01 = S, T), source 4:2 (0 TextureA, 1 TextureB), perspective 6:5 (01) |
| Units 1 and 2 | texture registers at header (u << 24) \| 00020000, sampler (u + 20h) << 24 \| 020000, mip block 10030000 + 8u |
| Vertex | XP's 10 dwords X Y Z W diffuse specular u0 v0 u1 v1; 04/25 = F6CC00 (TextureA and B S, T); attributes 08/09 = TextureA S/T, 0C/0D = TextureB S/T (HCM p155-156) |
| Temporaries | w3\[11:4\] = 82 output, C2 r0 (register 0C), C6 r1 (0D) |
| Saturate | w3 bit 12 on a write to a temporary, except a SELECT and DOT3's signed expansions; DP3 sets it |
| Last reads | w2 bit 17 = A, w1 bit 21 = B, w0 bit 25 = C: on the instruction that reads a texture register (00-03) or temporary (0C-0F) for the last time in the program, once per register per instruction (A first). The one-stage program follows the same rule from v10.303. Before that it always marked A, whichever operand read the texture. MODULATE(TEXTURE \| COMPLEMENT, DIFFUSE) reads the texture as B, so the mark landed on the diffuse colour: the draw came out as garbage across each triangle and left the engine wrong for the next draw (VXGL scene 27, GL\_BLEND on LUMINANCE and INTENSITY: 9630 pixels; 0 with the rule). TEXDBG 0x40-0x44 move the mark for experiments: as sent, last operand, every operand, first operand, A always |
| DOT3 | r0 = arg2 x 1 + c22.y, x2 (00962490 30140924 00F0rr05 00002C20); r1 the same on arg1 (dest 2C60); out.rgba = dp3(r1, r0) (01C00530 3000C053 00F00D05 00020820; w3 bit 17 = dot product) |
| c22 | 22030058 = 3EA2F984 BF000000 3EE8BA2E 00000000 (y = -0.5) |
| BLENDTEXTUREALPHAPM, MODULATEINVALPHA\_ADDCOLOR | 00000500 B2040050 0070006D 00100820 (A = texture or arg1 alpha, complemented; B arg2; C arg1) |
| MODULATEINVCOLOR\_ADDALPHA | 00006DB0 02040050 00700005 00100820 |
| Caps | 3 blend stages, 3 textures, FVF caps 2 (two coordinate sets), op caps 0x9FFFFF (0xFFFFFF from v10.208, with the bump operations) |

The layout rule: each stage's colour and alpha stay pending until something reads them. A stage that reads CURRENT first writes the pending instruction to r0 (colour, then alpha). The last stage writes the output, and a value already in a register is copied out with a SELECT. SELECTARG(CURRENT) adds nothing. Same op and arguments for colour and alpha fold into one rgba instruction. Stage k's texture register is its unit number. PREMODULATE selects arg1, and the next stage's texture multiplies CURRENT.

The last-read rule matches the 55 stage-0 cases and every v2.5/v2.6 capture except bump mapping and DOT3 with TFACTOR (T141, which uses other registers). An instruction may read one constant, so a TFACTOR argument to DOT3 is copied to the temporary first.

Solved in v10.209 (16.14): unit 1 on TEXCOORDINDEX 1 sampled set 0's coordinates on the board, while the registers match the manual and XP. XP's captures always had equal sets. v10.207 adds TEXDBG 0x54 (FE/01, units, unit 1's set, vertex 0's uv0 and uv1 as ring 0 sent them) and 0x51 (the two sets swapped in the vertex).

### 16.13 Lines, points, dither, stencil, ZBIAS (v10.201-v10.205)

All proven on the board: VXDX7 --raster on v10.205 had 0 failures.

| Feature | How it is drawn |
| --- | --- |
| Lines (T62) | 04/23 = 00CE01, 04/24 = 2 per line, 01/85 = 800000; list and strip proven |
| Points | each point is a one-pixel square of two triangles (8 points gave exactly 8 pixels). D3DHAL\_DP2POINTS is {wCount, wVStart}: reading it reversed drew nothing in v10.201-v10.203, so no point register set was ever exercised. XP's point mode (04/23 = 001E00, no 01/85, 04/20 and 04/40..48 written) is untested |
| Dither | 01/00 bit 0 = dither enable for RT0 (bits 3:1 for RT3..RT1, HCM p84); 01/01 bit 0 is the line pattern. 01/53 bits 16:15 = 00 table x (2^n - 1), 01 table, 10 round x (2^n - 1), 11 round |
| Stencil (T52) | 01/01 \| 10; 23 = 25 = ref << 16 \| mask << 8 \| write mask; 24 = 26 = func << 16 \| fail << 6 \| zfail << 3 \| pass (D3D value - 1); D24S8 only; with Z off the depth test runs as ALWAYS without writes; v10.245-v10.247: also 01/17 = 80, 01/01 \| 2200 and the stencil plane in 01/28-2A (section 17) |
| ZBIAS | in the core: z -= bias / 4096; proven on Z16, D24S8 and D32 |

Dither on the board (7C7C7C on RGB565): bit 0 with 000C0F mixes green 7D and 79 while red and blue stay 7B; 008C0F also moves red and blue to 84; without bit 0 the block is flat. The pattern is not a plain 2x2, because rows 73 and 75 differ. XP writes 01/00 = 0F on every draw and switches dither with 01/53 alone.

At 16 bpp the clear colour 406080 reads back as 426184, so pixel checks compare within 8 per channel. The C:\\VXD3D.LOG event log is off unless ACCEL\_D3D\_LOG = 1 (DWORD, Decimal).

### 16.14 Coordinate set 1, colour key with a program, bump, projected, hang recovery (v10.208-v10.213)

All of this passes on the board: VXDX7 --texture 0 failures on v10.211 and v10.213, Tunnel works on v10.213. Each row is what the hardware needs, with the board result that proved it.

| Feature | What the hardware needs | Board evidence |
| --- | --- | --- |
| Coordinate set 1 | 04/28 (vertex parameter mask, HCM p160) bits 11:10 = TextureB S, T present: FFCC00 when a unit reads set 1 (XP T24), FFC000 otherwise. A cleared bit means the parameter takes its default (0) | v10.208: set 1 read (0, 0); v10.209 with FFCC00: white at both points |
| Colour key + specular (T73) | Key test in ALU 0 and 2-5, the program's first instruction writes C0 at ALU 1, the rest read C0. 90=018001 91=3 93=3 95=020, TAU 301001B0 314011B1, c0/c1 = 0.5 1 2 4, 1/512, key in c43 | v10.209 with 314001B1 (T45's word, which follows 201001B0): engine hung, status 0E001701. v10.210 with 314011B1: pass |
| Colour key + second stage (T125) | As T73 with 93=6, TAU 301001B0 211111B1 314011B1 | v10.210: pass |
| Bump (T146-T148) | XP's words; TAU 201001B0 311001B1 (BUMPENVMAP) or 301001B0 311011B1 (LUMINANCE); matrix c11 at 2203002C, LSCALE/LOFFSET c35 at 2203008C; formats V8U8 38, L6V5U5 39, X8L8V8U8 3A. The dependent read takes its base coordinates from TextureB whatever FE/01 says: unit 1 goes on the B slot (FE/01 byte 1 = 25, 04/28 = FFCC00) even when the app uses set 0 | v10.210: set 0 read 0 + du dv (B masked). v10.211: all six cases pass |
| Projected (T151) | 9-dword vertex X Y Z W diffuse specular s t q: 04/00 A80009, 04/03 0A0908, 04/25 F6E000, 04/28 FFE000, FE/01 byte 0 = 22, TAU 307001A0 | v10.208: (96,48) green, (96,112) white |

**A TAU word that does not match the schedule hangs the engine.** Unit 1 with 311011B1 on an ordinary two-unit draw (v10.208) hung at status 06000F01; so did the colour key's 314001B1 after 301001B0. Take TAU words only from a capture of the same layout.

**Hang recovery works inside one boot.** On a draw timeout the VxD pulses SR4E bits 5:4 = 01 (the 3D engine reset), then always SR1A bit 6, 5 ms each way, and waits up to 250 ms for 0x400 & 1FF7 = 0. SR4E alone brings the engine back to idle, but the next textured draw hangs again, every time, at 16 and 32 bpp (bits 5:4 = 11, or bit 4 twice: the same); with SR1A after it the next draw is right (v10.292, VXDX7 2 of 2). Board, v10.210: 06000F01 to 06000000 after both pulses, device kept. The hung draw is dropped, never replayed, and the next program gets a new 01/9B tag. With asynchronous draws a hang is found when the next draw retires it: fx\_start then resets the engine and that draw's group is built again on the reset engine (v10.281), or it is lost with the hung one. The VxD keeps a summary of each packet of the group in flight and of the last group that did not finish (TEXDBG E0 + n; v10.291). Registry ACCEL\_D3D\_RECOVER = 0 keeps the old fence until reboot; TEXDBG F1 selects the older form (SR1A only when still busy).

**The HAL DLL must keep every stack frame under 4 KB.** VXDDRAW.DLL is built without stack probes (Watcom -s); the draw function's 7496-byte frame (v10.208's larger packet) broke Tunnel while VXDX7 passed. With the clip buffers, program and packet static (every caller holds callback\_lock) the largest frame is 3064 bytes and Tunnel works (v10.213).

Registry switches (HKLM\\Software\\VX85598, DWORD, default 1, 0 = off): ACCEL\_D3D\_RECOVER, BUMP, PROJECTED, CKEYPS, STAGES, NEWOPS, DITHER, POINTS (all prefixed ACCEL\_D3D\_).

### 16.15 Cube maps, batching, burst (v10.214-v10.216)

Board, v10.215 (VXDX7 v2.38): cube map passes with and without the burst; 40 triangles drew right in 3 draw packets (37 appended to a batch); Tunnel 4x faster on v10.216.

| Feature | What the hardware needs | Board evidence |
| --- | --- | --- |
| Cube map (T150) | 02/00..05 = each face's offset >> 8 in the order +X -X +Y -Y +Z -Z; 02/18 = 1300; FE/08 = 010201; FE/01 byte 0 = 22; 9-dword s t r vertex (r in the q slot): 04/00 A80009, 04/25 F6E000, 04/28 FFE000; TAU 301001B0; no mip chain. Caps: GUID\_DDMoreSurfaceCaps answered with dwCaps2 FE00, texture caps + 800 (CUBEMAP) | v10.215: six faces right, burst on and off |
| Batching | A draw packet holds up to 192 vertices (48 before v10.235), which go to the engine in bursts of at most 48; from v10.236 a packet also holds triangle strips (16.18). Consecutive draws with the same state are appended; the batch is sent before every state command (DP2 8, 25, 28, 39), a render-target change or clear (41, 42), a command the runtime parses, and at the end of the call. A failed batch reports the offset of the command that started it | v10.215: 3 packets for 40 triangles |
| Burst | One 04/23 write with 04/24 = vertex count per triangle-list packet instead of one per triangle | v10.216: Tunnel 4x faster |

A cube map on unit 1 or 2, or with a second unit, is left out (drop reason 1000). Registry switches: ACCEL\_D3D\_CUBE, ACCEL\_D3D\_BATCH, ACCEL\_D3D\_BURST (DWORD, default 1).

### 16.16 WinTune 98's Disco scene: inline vertices, device loss, speed (v10.217-v10.221)

WinTunDD.ocx's sphere test (DirectX 6 interfaces through D3DIM.DLL, D3DVERTEX lit by the runtime, no Z buffer, three 108x108 grids and a 40000-triangle sphere, about 80000 triangles a frame) drew dark walls and no sphere. VXDISCO rebuilds the scene and compares the HAL with the RGB device; `VXDISCO --wintune` runs the original OCX with the driver's counters logged.

**TRIANGLEFAN\_IMM and LINELIST\_IMM vertices start on the next DWORD.** The runtime sends polygons it clipped itself as op 23 right after INDEXEDTRIANGLELIST2 data, which ends 2 bytes past a DWORD boundary when its count is even. The vertex data starts at (command + 4 \[+ 4 edge flags\] + 3) & \~3, as in the DDK reference rasterizer; the next command follows the vertices. Read 2 bytes early, every clipped wall polygon failed (DDERR\_INVALIDPARAMS at offsets 2 mod 4) with the rest of its batch. Board, v10.219: walls, sphere and cube match the RGB device (0.1% of pixels differ on the full frame).

**A packet the VxD refuses after an earlier packet of the same call was drawn loses the device.** The HAL cannot replay the call, so it marks the context lost and every later batch fails (WinTune drew about two frames, then nothing). The HAL must never send what vx3dn\_validate refuses: a vertex non-finite after clipping or with rhw 0 as a float drops its primitive; -0.0 is written as +0.0 (validate compares IEEE encodings, where 80000000 is above every limit); a texture coordinate that is not finite as a float is sent as 0. A VxD BUSY answer (nothing reached the engine) is waited out in the HAL, up to 500 ms.

**Speed.** Board, v10.220, full frame: 4071 ms on the HAL, 514 ms on the RGB device. The HAL spent 2.9 s, 42 us a triangle, redoing the program, texture lookups and packet header for every triangle; the VxD spent 840 ms on 4342 packets (741 engine writes and 96 status reads each); the submit IOCTLs added 160 ms. Engine writes are posted since v10.220 (a status read after every write was about 600 extra uncached reads a packet; x86 uncached writes stay in order and the idle wait's read flushes them). Since v10.221 the per-state work is built once and kept until a state command, and each triangle only reads, clips and appends its vertices.

Registry switches: ACCEL\_D3D\_POSTED (default 1, 0 = a status read after every engine write), ACCEL\_D3D\_IDLE\_READS (idle status reads in a row that count as idle, default 8 from v10.222; 32 in v10.221).

### 16.17 XP's command fetcher: how its driver feeds the engine (read from the driver code; what ran on Win98 is in 16.18)

On the same board XP draws VXDISCO's frame in 157 ms, against 2.5 s for v10.221 writing every word through the register port; WinTune runs at 6 to 10 fps there. XP's CPU writes commands into memory and the engine fetches them. Everything below is read from VIA's XP driver 6.14.10.0293 (dc06emb.inf: miniport S3gIGPm.sys, display driver S3gIGP.dll, which loads the core s3ginv.dll); addresses are those files' virtual addresses. This section is what the code says; 16.18 is what ran on the board. The lab's earlier fetch experiments (vx855\_3d\_core.c, probes 18-19 and the v6-v25 SF ring) cite s3ginv BC770-BCA94, the one-shot routine below, and never confirmed a fetch; the ring mode below may not have been tried.

**Transport.** Every fetcher register is written through the command-regulator port: 0x41C = 0x00100000 (type 0x10), then words reg << 24 | value (24 bits) to 0x420. s3ginv checks the chip ID first and takes this path for 5122 (VX855), 1122, 2122, 4122, 5124, 7122 and 8122.

| Register (type 0x10) | Meaning |
| --- | --- |
| 60 | buffer start, GPU address bits 23:0; bit 0 set in ring mode for the chips listed above |
| 61 | buffer start bits 31:24 |
| 62 | buffer end bits 23:0 |
| 63 | buffer end bits 31:24 |
| 64 | pause address bits 23:0 (the engine fetches up to here) |
| 65 | pause address bits 31:24; the one-shot form adds 0x200 |
| 66, 67 | wrap point bits 23:0 and 31:24 (initialised to FFFFFFF0) |
| 68 = 0A0001 | start the fetcher (the cookbook's init3d writes 0A0000: off) |

**Batch end and padding.** A batch ends 16-byte aligned with CC000000 words, then FE020420 0000041C 00300000 CC000000, then CCC0000n DDD0000n pairs (n identifies the caller: 1 stop, 2 restart, 3 wrap, 4 kick; 1 and 2 are written as CCC/DDD pairs, the wrap and the kick write CCC0000n alone) up to the alignment mask: 0xFF (256 bytes) for 5122 and 8122, 0x1F for 3371, 3225 and 3230, 0x7F otherwise. The pause address is the padded end minus 0x10, as a GPU address (CPU pointer - CPU base + GPU base).

**Ring start** (s3ginv 0xBB370). The ring is \[+1B8\] CPU base, \[+1BC\] GPU base, size \[+1C4\] bytes (4 MB in the miniport). If 0x444 bit 31 is set (a ring is running), the CPU position is taken from 0x43C (it reads back the engine's current GPU position), padded with the stop pairs, and 64 / 65 (65 | 0x200) are written; then the driver waits until 0x400 & 0x101FF3 = 0. Then at the ring start it pads with the restart pairs and writes 60 (start, | 1 on 5122), 61, 62 (end), 63, 64 / 65 (pause), 66 / 67 = FFFFFFF0, 68 = 0A0001. After that it sets SR3F bits 5:4 through the MMIO sequencer window (0x83C4 index, 0x83C5 data); 0xBB990 decides whether a ring restart is needed by comparing 0x478 or 0x464 (& FFFFFFF0) with the ring's GPU base.

**Kick** (s3ginv 0xBC5xx, ending at 0xBC76A). Append the commands, pad with the kick words to the mask, then 41C = 0x100000, 420 = 64 | pause bits 23:0, 420 = 65 | pause bits 31:24. Three register writes a batch.

**Wrap** (s3ginv 0xBBC10). Pad at the current position with the wrap words (CCC00003); 66 / 67 = that end minus 0x10 (where the engine jumps back), the position returns to the ring start, and 64 / 65 = the ring start minus 0x10 (nothing new to fetch yet); the next kick moves the pause forward from there.

**One-shot buffer** (s3ginv 0xBC770-0xBCA94). Pad to 32 bytes with FE020420 0000041C 00300000 CC000000 CCCCCCC0 DDD00000 ...; wait until 0x400 bit 4 is 0; write 60 (start, no bit 0), 61, 62 (end), 63, 64 (padded end minus 4), 65 | 0x200, 68 = 0A0001.

**Idle and status.** Idle: 0x400 & 0x101FF3 = 0 (the register path's 1FF7 plus bit 20). 0x444 bit 31: ring running. 0x43C: the engine's current fetch position (CLXP3D saw it just past the last word written, with 0x440 = 0x43C - 0x40 and 0x45C = 0x43C - 0x10).

**Where the buffer lives.** In system RAM, mapped into GPU address space by the page table at frame-buffer offset 0FF80000 (SR6A 80, SR6B FF, SR6C 00; 16.1). XP's miniport sets the table up at boot: XPDUMP shows SR00-FF identical after boot and after 3D (only CR4B, CR60 and CR61 change, independently of 3D). The miniport imports MmAllocatePagesForMdl, MmAllocateContiguousMemorySpecifyCache and VideoPortGetAgpServices.

**A second, older sequence in the miniport** (S3gIGPm 0x2CC9E-0x2CEA1, in an IOCTL path): the ring at \[+F8\] GPU / \[+FC\] CPU, 4 MB (end = start + 3FFFFF), first words F210F110 00100000 then CC000000 DDD00000 pairs to 256 bytes; then through the 3D port 0x43C = 0x00100000 and to 0x440: 62 (start bits 31:24 | (end bits 31:24) << 8), 60 (start bits 23:0), 61 (end bits 23:0), 64 (pause bits 31:24), 63 (pause bits 23:0 | 2), and 62 again | 0x800000. This is the VIA Linux DRM's via\_cmdbuf\_start layout (CLE266-era HAGPBstL / HAGPBendL / HAGPCMNT / HAGPBpL / HAGPBpH); which chips XP sends through it is not known.

**Done:** VXRING v1.2 to v1.4 (v10.225 to v10.227) ran the one-shot form from video memory on the board, and the driver has drawn through it since v10.228 (16.18). The ring form drew a 2D fill once 68 = 0A0001 was written again after the kick (VXRING v1.2 test 8), but the driver does not use it. The system-memory buffer behind the page table has not been tried.

### 16.18 The fetcher and fast draws on Win98 (v10.225 to v10.236)

Every Direct3D draw packet now goes to the engine through the command fetcher. VXDISCO's full frame went from 2532 ms (v10.221, every word through the register port) to 511 ms (v10.235), about the RGB software device's time, and 3.3 times XP's 157 ms. Everything here is proven on the board, except where a paragraph says it is not.

**The fetched draw that works** (VXRING v1.3 and v1.4, then the driver from v10.228). This is the one-shot form, from video memory.

- The command block is 256 KB of video memory, reserved below the save block and not given to DirectDraw. Each packet starts 256-byte aligned and moves forward through the block.
- The words use XP's forms. A register header (type byte 01, 02, 04 or FE) is sent as FE020440 0000043C header CC000000, followed by the register words. A data header (type byte 03, with 0 = vertices) is sent as FE03043C n header CC000000, followed by the n words.
- After the commands comes XP's batch end, FE020420 0000041C 00300000 CC000000, padded with CC000000.
- Start: write 0x41C = 00100000 again, then the 0x420 words 60, 61, 62, 63, 64, 65 | 0x200 and 68 = 0A0001.
- Then wait for idle (0x400 & 101FF3 = 0, read ACCEL\_D3D\_IDLE\_READS times in a row, at most 100 ms).
- Stop: 0x41C = 00100000, then 68 = 0A0000.
- The command-regulator blocks (types 0x10 and 0x11) are not needed: a fetched draw on a cold engine drew without them. While a draw is in flight they must be left out. A capture over 32 KB goes through the registers instead.

**Trap: the 0x41C latch.** 3D register writes move the header latch that the 0x420 words go through. Without 0x41C written again just before the start words, nothing draws (VXRING v1.4 test 16). With it, state through the registers plus fetched vertices also drew (tests 12 and 13, which drew nothing in v1.3).

**Asynchronous draws** (v10.231, ACCEL\_D3D\_ASYNC). On an offscreen target, when SR1A bit 3 and SR1E bit 0 are already set, a fetched draw returns right after the start. Four things retire it first (the idle wait, then the stop): the next draw, a clear, VX3DN\_busy (every DirectDraw Lock and Blt asks it) and VX2D\_access. A retire that does not see idle within 100 ms resets the engine. A draw to the primary still waits.

**State cache** (v10.232, ACCEL\_D3D\_STATE\_CACHE). The cache compares everything in the packet except the vertex count, the per-vertex arrays and the runs. When that matches the last packet sent, only the primitive set-up and the vertices go out. It is forgotten whenever memory the engine reads may change: every TEXDBG call, VX3DN\_busy, a clear done by the 2D engine, a register-path draw, a refused capture, the 3D lab IOCTL. In VXDISCO, 4339 of 4341 packets went without state, and the words per frame dropped from 3.52 M to 2.15 M.

**Batches** (v10.275, VX\_IOCTL\_3DN\_SUBMITN 0x85500024). Up to 32 offscreen draw packets in one call: one entry, one sequencer open and close, the packets' words built one after another into a fetch group (each still its register-path stream), XP's batch end once, one start per group. A group is started when the next packet might not fit (3200 words kept free, 32 KB a fetch) and the next group is built while it runs. A refused packet stops the batch after the ones before it are sent. VXGL queues 32 packets a call (ACCEL\_OPENGL\_BATCH; 16 before v10.315); Direct3D's core queues a DrawPrimitives2 call's packets (ACCEL\_D3D\_SUBMITN). ACCEL\_D3D\_BATCH\_END 1 puts the batch end between packets too. Board (VXTEX): a bind and a tiny triangle 94 to 69 microseconds, 5.3 packets a start.

**State diff** (v10.276, ACCEL\_D3D\_STATE\_DIFF). The VxD keeps the last value of every type 1 and type 4 register written by a fetched stream, the draw's own writes included. A write before the vertices whose register already holds the value is left out, as XP sends state only when it changes. Never left out: the registers written as sequences or strobes within one packet (type 1: 00 01 02 34-37 5B 63 6B 90-9B; type 4: 00 01 02 20). Type 2, 3, FE and the command-regulator blocks always go whole; a header whose writes are all left out is not sent. The values are forgotten with the state cache. 2 = type 1 only, 0 = off. A small untextured packet: 219 words, then 94 for the same state. Default 0 (v10.287): on this CPU the compare costs more than the words it saves (VXTEX v10.276: 58% of state writes left out, yet building a packet took 43.8 microseconds instead of 28.4). Fewer words only pay when they are never made (constant groups, below).

**Direct sink** (v10.282, ACCEL\_D3D\_SINK, default 1). When the capture opens a fetch block, the emitter stores each word straight into the command block and the capture takes the count back before it looks at its own. REG and DAT are inline from v10.287: one bounds test and one store a word. The words and their order are the register path's. VxD instructions for a textured-state packet, counted: 23.2k by the old path, 9.26k with the sink.

**Constant groups** (v10.282, ACCEL\_D3D\_CGROUPS, default 1). The runs of the state set-up that never change are not made at all once a whole set-up has gone out in this state generation: type 1: 03-0F, 15, 19-22, 27, 2B-32, 38-3F, 51, 58-5A, 60-62, 68-6A, 84, 88, 8A; type 4: 05-10, 21, 22, 26, 27, 29, 2C-2F, 50-52. No other write sets them. A fuzz run of random DrawPrimitives2 streams found no grouped register written with another value (28219 packets, 4.7 million writes). With the sink: 8.85k instructions a packet.

**Asynchronous clears** (v10.282, ACCEL\_3D\_CLEAR\_ASYNC, default 1). Depth is filled first and the target's 2D fill is left running when nothing comes after it (offscreen target, no stencil plane); the next 3D submit, a Lock or any 2D call finishes it first. A clear to the primary waits. VXTEX's 256x256 clears: 155 to 117 microseconds.

**The command block is write-combining**, like the frame buffer's start (page bits A67; TEXDBG C7 reads its MTRR type and times it). 16 KB of CPU writes take 16629 cycles: 985 MB/s, about 4 cycles a word, so a 439-word packet costs about 2 microseconds to store (v10.286). The rest of a packet's time is the CPU building it.

**Where a bind goes now** (board, v10.291-v10.294, VXTEX at 16 bpp): a bind and a tiny triangle 61.8 microseconds (69.4 at v10.275, 85 with the state diff on). Of VXGL's state translation, about 30.6 is the send of the batch drawn with the old state and 9 its own work; the core costs about 1.7 microseconds a primitive. Profiles: TEXDBG C8 and C9 (the VxD's parts and the emitter's phases), VXGL.LOG.

**Where the time went.** The engine was never the limit: fetch on and off took about the same time, and halving the idle reads changed nothing. The time was CPU work for each call, each packet and each word:

- The HAL opened and closed the VxD for every call. v10.234 keeps a handle per process, and the calls' own cost fell from 139 ms to 11 ms a frame.
- The emitter made a call for each word. v10.233 hands over a vertex at a time.
- Packets: v10.235 holds 192 vertices a packet (was 48), still sent to the engine in bursts of at most 48. Each burst has its own 04/23 and 04/24, the size proven since v10.216.
- Copying and checking: v10.236 copies only the used part of a packet, uses rep movsd and repe cmpsd, and checks each triangle on its IEEE bit patterns. Counted in instructions, that took a 48-vertex packet from 16337 to 8282 VxD instructions, and fast\_triangle from 419 to 312 HAL instructions a triangle.

**Trap: Windows 98 reuses process IDs** (v10.230). The DirectDraw HAL's per-process VxD handle, keyed by process ID, was handed to a new process that got a dead process's ID. Tunnel's second run then failed with SURFACELOST, and could not load a texture. The fix: after a failed call, the HAL opens a new handle and retries once. The board confirmed it: 2 reopens, 0 failures.

**Primitive topology** (XP captures in clxp3d\_v24):

- 04/20 selects the topology: 0 for a list (T61), 386300 for a strip (T59: 04/24 = 8 vertices, 6 triangles), 1C5C00 for a fan (T60). 04/23 is 00CE02 for all three.
- XP expands an indexed triangle list into its triangles (T61 sends 6 vertices).
- XP's CCW cull writes 04/23 = 00CF02 (bit 0x100, T65). The driver keeps 00CE02 and culls in the HAL, so the engine never culls.

**Triangle strips** (v10.236; board, v10.240: VXDISCO passed every set with 1199 strips, 224-228 ms a frame against 245 with strips sent as triangles). With batching on (ACCEL\_D3D\_STRIPS), the HAL turns shared edges into strips. A packet carries up to 32 runs, each a list or a strip. A triangle that shares an edge with the list triangle just before it makes that triangle a strip head: the head is reordered as the unshared vertex followed by the two shared ones, and the new vertex is added. Each later triangle on the strip's last edge adds one vertex. While the strip is only the head plus one vertex, a triangle on the head's other shared edge swaps the head's two shared vertices instead (the same two triangles). That way a grid row (a b c), (c b d), (c d e) continues as a b c d e. WinTune's walls, floor and sphere bands come out at about one vertex a triangle instead of three.

The emitter tracks the value in 04/20. Set-up writes 0, a strip burst writes 386300 when the value is not already 386300, and a list burst after a strip writes 0 again; this holds across packets sent without state. A strip longer than 48 vertices goes in bursts of 48 that repeat the last two vertices. That assumes each 04/24 starts a new strip, as it starts a new list; the board confirmed it (VXDISCO, v10.240). If the engine draws strips wrong, ACCEL\_D3D\_STRIP\_HW = 0 makes the VxD send each strip's triangles as list bursts. VXDISCO v2.5 times both settings and compares each with the RGB device.

| Build tested | Tunnel fps | VXDISCO frame | What changed |
| --- | --- | --- | --- |
| v10.221 | 100 | 2532 ms | every word through the register port |
| v10.228 | about 300 | 1033 ms (fetch off 996) | the fetcher |
| v10.230 |  | 1095 ms (fetch off 1052) | handle reopen |
| v10.231 | 370 | 998 ms | asynchronous draws |
| v10.232 | 350-588 | 978 ms | state cache |
| v10.233 |  | 858 ms | a vertex a call to the capture |
| v10.234 | 454-625 | 676 ms | per-process handle |
| v10.235 | about the same | 511 ms | 192-vertex packets |
| XP (same board) |  | 157 ms |  |

WinTune's sphere test took 128 s (score 3.94) on v10.221, 57 s (8.9) on v10.228 and 52-53 s (9.6-9.8) on v10.232. The RGB device's frame times varied from 508 to 780 ms between runs, so compare a run's HAL time only with the RGB time from the same run. On v10.234 the split of the frame was: VxD submits 377 ms, the HAL's own work 165 ms, the runtime about 123 ms.

### 16.19 Blend equations, constant colour, interpolation, result scale, border colour, mip chains (v10.304-v10.306)

All proven on the board by v10.306: VXGL scenes 27 and 32-36 draw within 1 pixel of the software path (most 0), VXTEX and VXDX7 pass, no engine reset. The registers come from the HCM; none of them was in an XP capture except the interpolation op.

| Function | Registers and words | Notes |
| --- | --- | --- |
| Blend equation (HCM pp. 88-91) | Cout = FCa x Ca op FCb x Cb. 01/34 = 10000 \| Ca << 10 \| colour factor << 4; 01/35 = op << 14 \| Cb << 8 \| factor << 2; 36 and 37 the same for alpha | Ca, Cb: 0 source, 1 destination. op: 0 add, 1 subtract, 2 max, 3 min. SUBTRACT = source - destination. REVERSE\_SUBTRACT: Ca = the destination with its own factor, Cb = the source with its own. MIN and MAX use no factors (ONE: colour 15, alpha 19) |
| Constant blend colour (HCM pp. 92-93) | Factor code 5, + 10 inverse. FCa's constant: R 39\[22:12\], G 3A\[22:12\], B 3A\[10:0\]. FCb's: R 3C\[22:12\], G 3D\[22:12\], B 3D\[10:0\]. 11 bits, 1.0 = 400 (c x 1024 / 255) | ZERO and ONE are code 5 with the constant 0, so every set-up writes 38-3F as 0 and only a constant factor fills them. The constant's alpha as a factor = that alpha in R, G and B |
| Interpolation, one instruction (XP T10, T11, T119) | 00006DB0 00040050 00700005 00110820 (w3 op 11): A x C + B x (1 - C), A = arg1, B = arg2, C = the factor, swizzles 050 | A complemented factor swaps A and B. Alpha form: mask 8, w3 bit 26, swizzles 6DB. One constant register an instruction |
| Result scale | w3 bits 14:13: 1 = x2, 2 = x4 | Any op that does not scale already (not MODULATE2X / 4X, ADDSIGNED2X). A scaled pass-through becomes an instruction |
| Maximum mip level (HCM p124) | Sampler 02/30 bits 11:6 = the chain's last level, at most 8 | Block 10030000 holds the offsets of levels 1-8 (HCM p138). It was XP's 000140 (5) for every chain, so 7- to 9-level chains never sampled their last levels |
| Border colour (HCM pp. 125-126) | Address code 0 in 32 (0 border, 1 clamp, 2 wrap, 3 mirror). Sampler 35 = R\[23:12\] G\[11:0\], 36 = B\[23:12\] A\[11:0\], s1.10 | A filtered sample that touches a border texel returns the border colour, so OpenGL's CLAMP\_TO\_BORDER goes to the engine only with NEAREST |
| Mip chains on units 1 and 2 | As unit 0: 30 = bias << 16 \| maximum level << 6, 31 bits 1:0 the mip filter, 32 \| 090000, the level offsets in block 10030000 + 8u |  |
| Texture blits | A texture in the desktop's pixel format whose pitch is a multiple of 16 bytes is an ordinary 2D surface | Narrower textures stay with DirectDraw |

**The form that hangs: a temporary written after the output.** v10.304 and v10.305 built LERP as two instructions: r0 = arg2 x (1 - arg0) (XP's T104 form), then arg0 x arg1 + r0. The colour LERP alone drew right (temporary, colour out, alpha out). GL\_BLEND on INTENSITY, which LERPs both colour and alpha, ran temporary, colour out, temporary, alpha out: the engine hung on every draw and the quad never appeared. Moving the alpha's temporary from r0 to r1 changed nothing. Each of that program's other forms passed on its own: the T104 alpha form to the output (black-environment INTENSITY) and to a temporary, TFACTOR in B of an alpha instruction, a temporary read in C, and op 11 under the alpha mask with TFACTOR in A (VXDX7 v2.76). Write every temporary before the first output write, colour then alpha last, as XP does. The single-instruction LERP draws scene 27's quad in 6 ms (128 ms while it hung).

**Registry** (HKLM\\Software\\VX85598, DWORD, default 1, 0 = off): ACCEL\_D3D\_BORDER, ACCEL\_D3D\_UNITMIP, ACCEL\_D3D\_TEXBLT; ACCEL\_OPENGL\_BLENDOP, \_SCALE, \_LERP, \_LONGMIP, \_UNITMIP, \_BORDER (section 18).

**Tests.** VXGL scenes 32 (blend equations, constant colour), 33 (scaled combine, interpolation by a colour), 34 (border colour), 35 (512 chain, MAX\_LEVEL 2), 36 (unit-1 mipmaps), 27 (GL\_BLEND on every format); VXDX7 v2.76's four alpha cases.

### 16.20 Polygon stipple, DXT mip chains, the third texture unit (v10.307-v10.314)

All proven on the board by VXGL, VXDX7 and their probes; the driver's recipe follows each.

**Polygon stipple** (VXGL v3.5's probe on v10.307; on the engine from v10.308). 01/01 bit 1 turns it on; bit 0 does nothing for polygons. Block 14030000 holds the 32x32 pattern as 32 words (XP's set-up sends 32 zeros): word k is the target's row k counted from its top, bit b its column b counted from its left. The pattern is anchored to the render target's own top-left corner, not the window or the primitive. Send the block before 01/01 in every stippled draw's set-up and never set bit 1 on a clear.

- OpenGL: engine row r = bitrev32(PolygonStipple\[(h + 31 - r) & 31\]) for a target h rows tall (GL counts rows from the bottom and puts the leftmost pixel in a byte's top bit).
- The engine stipples lines and points too, which GL's polygon stipple must not: a batch with lines, points or unfilled polygons goes to software while it is on.

**DXT mip chains** (VXDX7 v2.79 and v2.80, v10.310). For DXT1, DXT3 and DXT5 the sampler ignores the level offsets in block 10030000. It places level n right after level n - 1, starting at level 0's base, at level n - 1's row bytes times its block rows. A row is max(32, blocks across x 8) bytes for DXT1 (x 16 for DXT3 and DXT5); block rows are max(1, height / 4).

| 32x32 DXT1 level | 0 | 1 | 2 | 3 | 4 | 5 |
| --- | --- | --- | --- | --- | --- | --- |
| Bytes | 512 | 128 | 64 | 32 | 32 | 32 |
| Offset from level 0 | 0 | 200 | 280 | 2C0 | 2E0 | 300 |

The proof: with every level in its own 256-byte block and the unused bytes painted one colour a region, level 2 drew the colour painted at +280 and level 3 the one at +2C0. A8R8G8B8 chains do take the offsets sent: levels 4 and 5 drew their own colours with the bytes around them painted.

- The driver gives a DXT chain's top level one block laid out as above (ACCEL\_D3D\_DXTCHAIN). The lower levels keep their own blocks as the CPU's copies, written packed, and are copied in with rows spread to 32 bytes on Unlock and on a DXT Blt.

**The third texture unit** (VXDX7 v2.81's probe, v10.312; fixed in v10.313). Unit 2 gets coordinates only when the vertex declares a third coordinate set. Without it (XP's 10-word vertex) unit 2 reads (0, 0), whatever FE/01 says. That is why three-unit draws came out black or in the wrong quadrant before.

| Register | Two units (XP) | Three units |
| --- | --- | --- |
| 04/00 | A8000A (10 attributes) | A8000C (12) |
| 04/04 | 00000D | 11100D (third set S 10, T 11, after set B's T 0D) |
| 04/25 | F6CC00 | F6CCC0 |
| 04/28 | FFC000, FFCC00 with set B | FFCCC0 |
| FE/01 byte 2 | (none) | 21 = set A, 25 = set B |
| 01/93 | 1, 2 | 4 (XP's T149; 7 draws nothing) |

- The third set's words are never read; send a copy of the set unit 2 uses. FE/01 byte 2 picks unit 2's set exactly as bytes 0 and 1 do for units 0 and 1. Source 2 (byte 29) did not read the third set: results changed from draw to draw (a seam on the diagonal, stripes).
- Registry (VxD) ACCEL\_D3D\_UNIT2C = 0 goes back to the 10-word vertex; (ICD) ACCEL\_OPENGL\_UNITS3 = 0 draws three units in software.

**1152x864 at 60 Hz** (v10.307) is the CVT timing: 81.75 MHz, 1520 x 897 total, hsync 1216-1336 negative, vsync 867-871 positive. TIMING\_1152\_CVT = 0 gives the 75 Hz DMT timing back.

**Traps found v10.309-v10.314.**

- The DRV's data segment is FIXED in its .def, yet its linear address moves once after boot (the VxD counts it, v10.274). Read its base (DPMI 0006) at every request handed to the VxD. v10.309 kept the first base: after the move every GDI request was read from the old address and declined (reason 0300), so the desktop drew in software (text scroll slow) until v10.314.
- DirectDraw sets no HASPIXELFORMAT on a surface in the desktop's format. A test for a texture's format must take the desktop's when the flag is absent (TEXBLT, v10.309).
- Test programs that switch the engine off for a comparison run must do it per process (VXGL\_HW = 0 in the environment, read at CreateContext), not in the registry: a run stopped in that part left ACCEL\_OPENGL\_HW = 0 for every later program (v10.310).

**Registry** (HKLM\\Software\\VX85598, DWORD, default 1): ACCEL\_OPENGL\_PSTIPPLE, ACCEL\_OPENGL\_S3TC, ACCEL\_OPENGL\_UNITS3, ACCEL\_D3D\_DXTCHAIN, ACCEL\_D3D\_UNIT2C, TIMING\_1152\_CVT.

**Tests.** VXGL scenes 37 (three texture units, reported quad by quad), 38 (S3TC, a DXT1 chain to 1x1) and 39 (polygon stipple). VXDX7: the DXT1 chain at levels 0-3 with the painted-bytes probe, the A8R8G8B8 chain's levels 4 and 5, and the three-unit probe (TEXDBG 0x58 + n).

### 16.21 Occlusion counter, LOD clamps (v10.315-v10.316)

The engine counts the pixels that pass the depth and stencil tests and writes the count to video memory. From v10.316 OpenGL's occlusion queries (GL\_ARB\_occlusion\_query, GL 1.5) use it. The board counted exactly: 10000 for a 100x100 quad, 5000 with half of it hidden.

**The counter** (HCM Part 2, pp. 83-84):

- 01/1C and 01/1D hold the count. Writing 0 to both resets it.
- 01/21 and 01/22 hold the write-back address in 4-byte units: 01/21 = (offset >> 2) & FFFFFF, 01/22 = offset >> 26. Bits 23:22 of 01/22 = 00 selects local video memory.
- Writing 01/21 and then 01/22 makes the engine write the 32-bit count to that address. Write both back to 0 afterwards.
- The constant group must not write 01/1C or 01/1D. If it does, every state change resets the count in the middle of a query.

**Sequence** (VX3DN\_query, IOCTL 0x85500025, op 0 = begin, op 1 = end):

1. Retire the group in flight, open the sequencer, check the clock lock (SR3C bit 4), wait for the engine to go idle.
2. Begin: type 1 header (43C = 00010000), then 1C000000 and 1D000000.
3. End: write a marker FFFFFFFF to the last 16 bytes of the command block and read it back (the read drains the write-combining buffer). Write 01/21, then 01/22, wait for idle, and poll the marker for up to 5 ms. Then write 21000000 and 22000000.
4. A marker still at FFFFFFFF means no count arrived: the VxD answers unsupported.

**OpenGL.** BeginQuery flushes the batch and resets the counter; EndQuery flushes and reads it, and the result adds the count. If the VxD declines, drawing inside the query goes to software, which counts as before. The counter is the chip's: from v10.322 it is one context's at a time, put aside for other threads' draws (16.23).

**Trap: the IOCTL allow list.** vxd\_main.c accepts only the control codes on its list. A new IOCTL not added there fails with error 50 before its case runs. v10.315's query failed that way and every query fell back to software.

**LOD clamps.** MIN\_LOD at 0 or below changes nothing (the engine never samples above the base level). A whole-numbered MAX\_LOD n equals a last level of BaseLevel + n, so the driver sends n + 1 levels. MIN\_LOD above 0, MAX\_LOD below 1, and a fractional MAX\_LOD below the last level stay in software. The sampler's 30 word has min-level and max-level fields (bits 5:0, 11:6) that could cover MIN\_LOD; not tried.

**Registry** (HKLM\\Software\\VX85598, DWORD): ACCEL\_OPENGL\_QUERY (default 1), ACCEL\_OPENGL\_LODCLAMP (default 1), ACCEL\_OPENGL\_BATCH (packets per VxD call, default 32).

**Tests.** VXGL's occlusion check (full and half-hidden quad) and scene 40 (LOD clamps). The test programs turn the engine off for their reference pass with the environment variable VXGL\_HW=0, never a registry key: an interrupted run once left ACCEL\_OPENGL\_HW = 0 behind.

### 16.22 NPOT and 3D textures, cube chains, colour tables, the front buffer (v10.317-v10.319)

All proven on the board in v10.318-v10.319 (VXGL scenes 41-48 pass, VXTEX 15 passes). Each is on by default with a key that sends it back to software.

**3D textures** (VXGL 48, VXTEX 15). HCM p117-118: 02/18 base mode 2 (volume), 02/21 the depth, 02/22 bits 11:8 log2 of the depth, the R address in sampler 32 bits 8:6 and the R filters in bits 21:19 (enlarge) and 18:16 (shrink), 001 = linear. The slices sit one after another, each pitch x height bytes. Power-of-two sides, depth up to 512 (the VxD accepts 2048), no mip chain (the HCM has none for volumes). Packet layout 3: face\[0\] the depth, face\[1\] the slice step, face\[2\] the R address; the vertex carries s t r with q\[\] = r.

- **Trap (v10.317 drew every slice as slice 0):** 02/22 bit 15 (width and height powers of two) must be set for a volume, as XP's ICD (s3gIGPgl.dll) sets it, and no level offsets are written. HCM p117: with bit 15 the sampler takes the offset mode for its addresses. The probe (TEXDBG 0x60 + n): bit 15 alone, bit 15 with the slices in the level offsets, and bit 15 with base mode 0 all drew right; without it every slice read slice 0.

**Cube maps with mip chains.** The six faces in one block sharing one level layout; the level table gives each level's offset from its face's base. A cube whose faces do not share the layout keeps level 0 only.

**Non-power-of-two and rectangle textures** (v10.317). A padded power-of-two copy, edges replicated, the texture coordinates scaled in a pipeline stage before the engine's (rectangle coordinates are texels). Clamp wraps only and no chain in this form (v10.321 adds the sampler's own NPOT mode, 16.23).

**Other texture forms.** Linear CLAMP\_TO\_BORDER: a one-texel ring of the border colour around the copy (nearest on a POT texture keeps the sampler's border). FXT1 and any compressed format the engine cannot read: decoded into a copy. DEPTH\_COMPONENT textures: read as luminance, intensity or alpha (DEPTH\_TEXTURE\_MODE), sampled with the magnification filter as Mesa does. Colour tables: applied to the copy (nearest only until v10.321). MIN\_LOD above 0 and fractional MAX\_LOD: the chain cut and its base moved to the levels Mesa would pick. GL\_DOT3\_RGB: the dot product into colour only (the expansion and DOT3 instructions write R, G, B: word 2 bits 23:20 = 0111), alpha from the alpha combiner.

**Front buffer.** Drawing into the front when it is the only buffer drawn: the front surface, shown at glFlush or glFinish.

**Direct3D fast paths.** ACCEL\_D3D\_FASTPL: points and lines inside the viewport go into the packet straight from the vertex buffer (the same words as the clip path). ACCEL\_D3D\_VFAST: the VxD builds XP's 10-word and the 5- and 7-word vertices in plain loops: vertex words 28.4 microseconds a packet (33.4 before), 48.1 a packet in all (52.9).

**Audit fixes worth remembering (v10.319).** Mesa's swizzle\_copy read 4 bytes of a 3-byte pixel past an exact-sized RGB image. The DXT encoder's power iteration must start on the axis of largest variance, not (1, 1, 1). A HAL callback lock held by a process that died answered WASSTILLDRAWING to everyone until reboot: the lock is now its holder's token, new at each DLL attach (Windows 98 gives a restarted program the ended one's process ID). RegReadConf honours the returned type and length.

**Registry** (HKLM\\Software\\VX85598, DWORD, default 1): ACCEL\_OPENGL\_NPOT, ACCEL\_OPENGL\_FXT1, ACCEL\_OPENGL\_DEPTHTEX, ACCEL\_OPENGL\_CTABLE, ACCEL\_OPENGL\_DOT3RGB, ACCEL\_OPENGL\_CUBEMIP, ACCEL\_OPENGL\_TEX3D, ACCEL\_OPENGL\_FRONT, ACCEL\_D3D\_FASTPL, ACCEL\_D3D\_CUBEMIP, ACCEL\_D3D\_VOLUME, ACCEL\_D3D\_VFAST.

**Tests.** VXGL scenes 41-48 (NPOT and rectangle, border, FXT1 / RGB\_DXT1, colour tables and depth textures, LOD clamps, DOT3\_RGB, cube mip chain, 3D texture), speed was 49 then (57 from v10.321, 62 from VXGL v4.5); the front-buffer check draws a triangle into the front only. VXTEX 15: a 3D texture's four slices.

### 16.23 Render to texture, fragment programs, smoothing, a second unit with a cube or 3D texture (v10.321-v10.322)

Board, v10.321: VXGL passes but scene 56 (render to texture, 6.22% apart, a Mesa bug fixed in v10.322, below); VXTEX, VXSTEN and VX2BLT pass. Board, v10.322: every program passes, VXGL 56 0.03% (20 edge pixels).

**Render to texture** (EXT\_framebuffer\_object, vxgl\_fbo.c, ACCEL\_OPENGL\_RTT). Colour attachments are X8R8G8B8 (an RGB texture level or an RGB renderbuffer), depth is D16 with room after it for its Coarse Z storage. The engine draws into the texture's copy (vxgl\_tex\_rtt) or a dedicated surface; RGBA and other formats keep Mesa's wrapper (software).

- No y flip on a framebuffer object: its row 0 is GL's row 0, a texture's first row. vxgl\_wy / vxgl\_sy convert, the viewport matrix drops the flip, and front-facing is decided on the unflipped area.
- The texture's image is read back where the engine drew before Mesa reads or changes it: glGetTexImage, glTexSubImage, glGenerateMipmapEXT, swrast sampling, a copy made again (a vnew mask per level, read through StoreImage from BGRA).
- After drawing into a texture the VxD's state cache starts over (vxgl\_engine\_wait): the texture cache is only known to be fresh when the sampler state is written again.
- swrast's spans into an attachment hold the engine lock from span start to finish (one lock for the run of rows).
- **Trap (board, v10.321 scene 56):** Mesa validated the read framebuffer before glCopyTexSubImage / glCopyTexImage only for the pixel-transfer state, not \_NEW\_BUFFERS. After an attachment switch \_ColorReadBuffer still pointed at the old attachment's renderbuffer wrapper, which the switch had freed: the copy read freed memory. teximage.c now updates the state for \_NEW\_BUFFERS too.

**Fragment programs and shaders as texture stages** (vxgl\_fp.c, ACCEL\_OPENGL\_FPROG). ARB programs, GLSL compiled by Mesa and ATI fragment shaders: each colour is turned into a polynomial over the inputs and matched to a stage cascade that computes the same result; per-pixel math the stages cannot express stays in software; from v10.353 one KIL that is an alpha test and a program's fog option run on the engine (19.5). A term is dropped only when it is exactly 0 (v10.328; the earlier rules are in 19.5); v10.321 dropped any term under 1e-6, so MUL by 1e-7 then 1e7 drew black.

**Antialiased points, lines and polygons** (ACCEL\_OPENGL\_SMOOTH): drawn pixel for pixel as swrast, its coverage carried in the vertex alpha. A stage that does not carry the vertex alpha: software. With the alpha test on: software, except untextured points and lines from v10.353 (19.6). The point radius bounds keep swrast's 0.7071.

**A cube map or 3D texture beside a second unit** (ACCEL\_OPENGL\_UNIT3D, ACCEL\_D3D\_UNIT3D). Packet stages.s3: bit 0 set 0 carries a third coordinate (q\[\]), bit 1 set 1 does (r1\[\]); ulayout / uface describe unit 1's cube or volume.

| Register | Value |
| --- | --- |
| FE/08 (each set's dimension) | 2, or 0x200 for a 3D set 0 (else 0x100), or 0x20000 for a 3D set 1 (else 0x10000) |
| FE/01 unit 1 | S, T, R from its set: (0x22 \| set << 2) << 8 |
| 02/u 00-05 | a cube's six faces >> 8; 02/u 18 = 0x1300 (cube) or 0x1200 (volume) |
| 02/u 21, 22 | a volume's depth; log2 depth << 8 and bit 15 |
| 04/00-04 | X Y Z W diffuse specular, then S\_A T\_A \[R\_A\] S\_B T\_B \[R\_B\] = codes 08 09 0A 0C 0D 0E, three to a register |
| 04/25, 04/28 | 0xF60000 / 0xFF0000 plus a bit at 23 - code for each coordinate carried |

**Non-power-of-two textures that repeat or mirror, and their chains** (ACCEL\_OPENGL\_NPOTWRAP): the sampler's NPOT mode, 02/22 bit 15 clear, rows aligned to 32 bytes. Only DXT sets bit 15 regardless.

**Smaller additions.** Colour tables filtered linearly when each component of a 256-entry table is affine (ACCEL\_OPENGL\_CTABLE). Points with GL\_VERTEX\_PROGRAM\_POINT\_SIZE: the program's size per vertex (ACCEL\_OPENGL\_POINTS). Mesa enable.c dropped glEnable(DEPTH\_TEST / STENCIL\_TEST) with no such buffer bound, and glEnable(GL\_TEXTURE\_\*) right after binding a new framebuffer object: GL keeps the state, swrast tests only buffers that are there.

**Occlusion counts per context** (v10.322; 16.21 had one count for the chip). VX\_IOCTL\_3DN\_QUERY takes an owner token beside the op: 0 begin (another owner's query is answered BUSY and counts in software), 1 end, 2 put aside, 3 taken back, 4 given up. A draw from another thread (Get\_Cur\_Thread\_Handle) puts the owner's count aside first; the owner's next draw sets it to 0 again, so the others' samples are not in it. The ICD puts it aside at MakeCurrent away and takes it back when current again.

**Mesa traps fixed in v10.322.** free\_shared\_state freed GLSL shaders and programs in hash order: a program freed after its shader read the freed shader to detach it; the programs now give their shaders back first. \_mesa\_HashWalk unlocked where it should lock. Both found only with Mesa itself under AddressSanitizer.

**Measurement and capture.** Latency distributions (p50, p95, p99, max): the HAL's DrawPrimitives2, submit IOCTL and busy retries (VX\_D3D\_QueryLatency); the VxD's 3D lock, idle wait, wait for the draw before and submit (TEXDBG 0x64-0x67, 0x68 clears). The HAL keeps draw records only with ACCEL\_D3D\_LOG = 1 or from the first VX\_D3D\_QueryDraws. The DRV's blit capture (ExtEscape 8F72, command 5) records screen-destination BitBlt, StretchBlt, StretchDIBits and SetDIBitsToDevice with who drew them (VX2BLT).

**Open: the engine after VXDX7's hang recovery.** v10.321: the draw after the recovery read 000000 (v10.319: white) with VXDX7.TXT otherwise identical line for line; VXDISCO run after it hung 28 groups in its first scenes and none later. The emitter's words for that draw are unchanged from v10.319. VXDX7 v2.83 reads what the hung draw left, the clear alone, the draw and the same draw again, and repeats the test with reset form 2 (TEXDBG F2: the 3D reset, then the 2D engine's reset SR4E bits 3:2 = 01 at once instead of at the 2D side's next operation). VXDISCO v2.16 prints the recoveries and a hung group's packets per scene. Board, v10.322 and v10.323: the draw after the recovery read white with both reset forms and VXDISCO left no group unfinished; the fault did not show, so it is not proven fixed. Form 0 stays the default. The band that follows the mouse (v10.325) is taken as the same fault: the chip left wrong after a failed recovery.

**Registry** (HKLM\\Software\\VX85598, DWORD, default 1): ACCEL\_OPENGL\_RTT, ACCEL\_OPENGL\_FPROG, ACCEL\_OPENGL\_SMOOTH, ACCEL\_OPENGL\_UNIT3D, ACCEL\_D3D\_UNIT3D, ACCEL\_OPENGL\_NPOTWRAP, ACCEL\_OPENGL\_CTABLE, ACCEL\_OPENGL\_POINTS, ACCEL\_OPENGL\_QUERY.

**Tests.** VXGL v4.4 scenes 49-56 (vertex-program point sizes, antialiasing, ARB fragment programs, ATI shaders, GLSL shaders, NPOT repeat, a second unit with a cube or 3D texture, render to texture; speed is 57, 62 from VXGL v4.5).

## 17. The D24S8 stencil plane and Coarse Z (v10.242-v10.299)

The Chrome9 keeps a D24S8 surface's stencil in its own 8-bit plane, not in the Z word's low byte. The plane is tiled in 8x8-pixel tiles; 01/28 says where it is and 01/29 its pitch. 01/01 bits 13 and 9, with 01/29 and 01/2A, also run Coarse Z, a 16-byte record per 8x8 tile kept in storage of its own (below). Coarse Z draws right with the conservative merging rule (the optimized rule XP's T50 uses lets a farther primitive pass), but on this board every setting that runs its test is slower than no Coarse Z, so the driver leaves it off from v10.299 (below). Proven on the board in v10.247: every Direct3D stencil test (VXDX7 --stencil, --splane) and every OpenGL stencil scene (VXGL v2.1, 0 pixels off; scene B 19) pass, and nothing reaches the screen.

**Registers** (type 01, written for every stencil draw after the 23-26 stencil words; XP T52 values for Z at 300500, pitch 1024, 256x192):

| Register | Value | T52 |
| --- | --- | --- |
| 01/28 | plane offset >> 8 | 003306 |
| 01/29 | 018000 \| 400000 \| (plane pitch >> 4) \| 400 \| 1000. HCM: bits 8:0 are also the Coarse Z pitch in 32-byte units (the same number: a row of Coarse Z records is twice the plane pitch), bit 15 resets the Coarse Z tags (the engine clears it), 400 and 1000 are Coarse Z retest and conservative merging (the lab had called them plane write and read) | 418410 (ALWAYS REPLACE), 419410 (NOTEQUAL INCR) |
| 01/2A | the Coarse Z storage's base >> 8; XP puts the storage 256 past the plane's end | 0033C7 |
| 01/17 | 000080 | 000080 |
| 01/01 | the usual enables \| 10 (stencil); 2200 = Coarse Z test (2000) and write (0200), on every draw that tests and writes depth with LESS or LESSEQUAL while the surface's records are trusted (v10.295, as XP) | CE375C |

On Z16 without stencil XP writes 29 = 018010 (no 400000, no plane bits) and 2A = (Z end + 256) >> 8 (T50: 003487), its Coarse Z storage right after the Z words. The driver does the same from v10.295: a Z16 surface's block holds its storage too (align256(pitch x h + 256) + storage size). With 28-2A left 0 the engine still draws stencil, into a plane at video memory 0: the visible screen.

**Where the plane goes.** 256 bytes past the Z words, rounded up to 256; its pitch is the width rounded up to 16; its size is pitch x the height rounded up to 8.

```c
plane   = ((z_off + z_pitch * h + 255) & ~255) + 256;
pitch   = (w + 15) & ~15;
rows    = (h + 7) & ~7;
size    = pitch * rows;
end     = plane + size;
cz      = (end + 256) & ~255;          /* 2A = cz >> 8: the Coarse Z storage */
cz_size = 2 * pitch * (rows >> 3);     /* 16 bytes an 8x8 tile; 12 KB at 256x192 */
block   = (cz + cz_size + 255) & ~255; /* one allocation from z_off to here */
/* byte of pixel x,y: 8x8 tiles of 64 bytes, rows of 8 inside a tile */
at(x,y) = (y >> 3) * pitch * 8 + (x >> 3) * 64 + (y & 7) * 8 + (x & 7);
```

Board proof (VXDX7 --splane, 256 wide, plane at 001C8100): the engine wrote exactly one byte per one-pixel REPLACE, and a CPU byte was found by EQUAL at exactly one pixel, both by that rule (80,72 -> +4A80, 200,150 -> +9670, 37,53 -> +312D, 255,191 -> +BFFF). The layout is the same with either 01/29 setting.

**Coarse Z.** The Chrome9 HCM manual (OGPM Part 2, pp. 79-80 and 87-88) documents it: 01/01 bit 13 enables the Coarse Z test and bit 9 its writes; 01/2A is the storage's base in 256-byte units; 01/29 bits 8:0 its pitch in 32-byte units and bit 15 its tag reset; 01/27 bit 0 fast Z clear; 01/11 bits 12 and 11 enable and clear a reading-Z cache. The storage holds one 16-byte record per 8x8 tile. The manual names no Z compression. 01/29 in full (p87-88): bits 23 and 11 turn a REJECT into ReTEST (the fine test) for clockwise and counter-clockwise primitives, 22 and 10 a PASS; 21:20 the storage's memory (00 local); 17 inverse mode (larger Z nearer, for GREATER and GREATEREQUAL); 16 write-back mode (1: the old record goes back to memory when its cache cell is taken); 15 the tag reset (cleared by the chip); 13:12 the merging rule (00 optimized, 01 conservative updating MIN and MAX, 10 conservative updating MIN only); 9 every result ReTEST; 8:0 the pitch in 32 bytes. 01/1E-20 are the pixel Z clip (far, near: pixels outside are dropped before the depth test), 01/19-1B the Z bias clamps and a precision flag, 01/1C-1D the occlusion count. XP's stencil draws (T52 418410, 419410) set 400400, so a PASS is tested again; the second also has 1000.

**How XP clears it** (T52: Z at 300500, plane 330600, storage 33C700). A 2D fill of the 12 KB of storage with FFFFFFFF, then 01/29 = 008000 (tag reset), then 018010, then the Z fill (FFFFFF00) and the plane fill (00). For a clear depth of 0.25 (T58) it uploads the record FFFFFFFF FFFFFFFF 10001000 10001000 for every tile instead of the FFFFFFFF fill. Clearing only Z and the plane leaves stale records, and a tag reset alone does too.

**What went wrong, v10.243-v10.293.** The driver copied 2200 and 2A from T52 for stencil draws, but its block ended at the plane. The records went to whatever DirectDraw put next and were never set; in VXGL that was the window's first texture, at exactly the 2A address. Stencil draws tested depth and stencil against a texture's bytes, 8x8 tiles at a time, and wrote records over the texture. Symptom (VXSTEN case 2): the far rectangle read stencil 00 in whole 8x8 tiles after boot, VXTEX, VXDX7 or a depth change, never straight after a VXSTEN run, whose own draws had left usable records. Changed nothing: constant groups off, asynchronous clears off, engine-drawn clears, 01/19 low each draw, a pulse of 01/28 bit 23 (a CX700 Z-dirty reset; on the HCM 01/28 is the plane base).

**Fix and proof, v10.294.** Coarse Z off for every draw, its storage reserved in the D24S8 block (code above) and checked by the HAL, and by the VxD when it is on. Board, 800x600x16: VXSTEN case 2 0 wrong after boot, after VXTEX and after VXDX7. With Coarse Z back on (TEXDBG D5) after VXDX7, the first fresh context failed shadow volumes with 512 pixels (eight 8x8 tiles) wrong; the next two passed. Turning it on for speed needs XP's clear at every Z clear: fill the storage (FFFFFFFF, or the depth's record), then reset the tags.

**How the driver runs it, v10.295-v10.298.** A clear of the whole D16 or D24S8 surface to the far plane fills its storage with FFFFFFFF, by the 2D engine as a surface of the desktop's depth (CPU writes when the fill is declined), and trusts the records. The next Coarse Z draw resets the tags (01/29 = 008000, then the draw's value). Draws that test and write depth with LESS or LESSEQUAL get 01/01 | 2200, 01/17 = 80, 01/29 = 018000 | 1000 | pitch / 16 (the stencil plane's value when stenciled) and 01/2A = the storage / 256. The records are dropped, and Coarse Z stays off for that surface until its next far clear, by: a clear of part of it or to another depth (XP uploads per-tile records for those: 0.5 = 20002000, 0.25 = 10001000; not done), a draw that writes depth without Coarse Z (another test, or stencil with a z-fail op other than KEEP), a Lock for writing or a destroy (the HAL sends VX\_IOCTL\_2D\_ACCESS with the range), swrast's depth writes in the ICD (the depth renderbuffer's writers note them; the end of swrast's batch sends the same IOCTL), a draw into the storage, an engine reset (the tags are reset again) and a mode change. The VxD's state cache compares packets, not Coarse Z: a draw whose Coarse Z bit or pending tag reset differs from what the engine holds gets its whole set-up (v10.296; after a CPU clear the next draw had gone out as vertices only).

**The merging rule, v10.298.** With XP's T50 setting (01/29 = 018010: optimized merging), the board drew farther primitives over nearer ones wherever Coarse Z was on, inside one packet and in interior tiles too, while the depth the engine wrote was right. VXGL v2.8 drew the depth scenes once for each change, with Coarse Z on (wrong pixels against the ICD without the engine; -1 untextured full-window quads front to back, -2 textured triangles, the far one over the near one):

| Change | depth | fog | offset | depth by sw | sphere | -1 | -2 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| none (018010, as T50) | 10868 | 12544 | 2300 | 15310 | 4663 | 65536 | 10764 |
| XP's 01/1E-20; test only (2000); the tags reset every draw; untextured as T50 (10 words) | the same |  |  |  |  |  |  |
| write only (0200); 29 \| 400400; 29 \| 0200; 29 bits 13:12 = 01 | 10 | 20 | 0 | 23 | 4 | 0 | 0 |
| Coarse Z off | 10 | 20 | 0 | 23 | 4 | 0 | 0 |

The conservative rule is the only right one that keeps both coarse decisions (400400 never accepts, 0200 never decides, 0200 in 01/01 never tests). It was the driver's in v10.298 (ACCEL\_3D\_COARSEZ 1). XP's T50 drew its two quads right with the optimized rule; what it does differently is not known.

**Speed, v10.298-v10.299: off.** VXGL v3.0 timed 40 runs of 12 full-window untextured quads drawn front to back (the case Coarse Z is for: 11 of every 12 hidden) with each setting, all drawing right except the optimized rule:

| Setting | ms |
| --- | --- |
| Coarse Z off | 73 |
| off, with the reading-Z cache (01/11 bit 12, cleared by bit 11 first) | 73 |
| records written, no test (01/01 0200 only) | 73 |
| conservative merging; + write-back mode 0; + the reading-Z cache; MIN-only merging (13:12 = 10); + PASS retested (400400); + every result retested (0200) | 112-117 |
| optimized merging + PASS retested | 120 |
| optimized merging (wrong pixels) | 185 |

Writing the records costs nothing; the coarse test itself costs about 55% here and gains nothing, so the hidden quads are not being thrown away early (possibly the records read back from shared system memory; not shown). VXTEX's textured full-window quads take the same time on and off (2.83 ms). The driver leaves Coarse Z off (ACCEL\_3D\_COARSEZ 0, the default from v10.299); the Coarse Z code was removed from the driver in v10.430 (24.5; XP's side of it is in 25.3), and the reading-Z cache stays off too. From v10.301 Coarse Z runs on triangles only; the HCM says it is not for lines. Experiments 1 and 4 leave the tag reset out of the draws' 01/29; they took 110 ms like every other variant, against 70 ms with Coarse Z off. Until v10.302, glFinish locked the depth surface, which made the VxD drop the records at every glFinish; it now waits for the engine instead.

**Clears at 16 bpp, v10.296.** The Z words' low byte is not used beside the plane, so a D24S8 Z clear writes bits 23:16 there too: a clear to 1.0 or 0.0 is then two equal 16-bit halves, which the 2D engine fills at 16 bpp (it declined FFFFFF00, so every such clear was CPU writes, 2.2 ms at 256x256). The storage fill follows by the 2D engine.

**Read and write bits.** With the bits set only as each draw needs (write if an op is not KEEP and the write mask is not 0; read if the test is not ALWAYS or an op reads), OpenGL's EQUAL draws that read without writing found nothing (VXGL scenes A, B, C, E). Direct3D's same orders passed. With both bits on for every stencil draw, everything passes. The driver now sets both (ACCEL\_D3D\_STENCIL\_ZW = 2, the default). XP was only ever captured with the write bit on. Why read-only fails under OpenGL is open. Coarse Z was on over unset storage then, so retest with ACCEL\_D3D\_STENCIL\_ZW 1 on v10.294 or later before trusting this.

**The driver's side.**

- VXDDRAW.DLL allocates every D24S8 Z buffer as one block of align256(pitch x h) + 256 + plane size + 256 + Coarse Z size, rounded up to 256, whatever VX\_HEAP\_ROUND says, so neither the plane nor the Coarse Z storage lands on another surface. A block only a multiple of 128 long (v10.287 and before) put every surface DirectDraw packed after it 128 off the sampler's 256-byte alignment, and CreateSurfaceEx refused them.
- Lock copies the plane into the Z words' low bytes and Unlock copies them back (not after DDLOCK\_READONLY), so applications still see standard D24S8. The copies use the tiled addresses.
- VX\_DD\_StencilPlane(surface, &offset, &pitch) gives the ICD the plane and marks the surface (lpGbl->dwReserved1) so Lock and Unlock leave it alone; the ICD's software stencil reads and writes the plane directly.
- Stencil clears also fill the plane. A clear of the whole surface is a 2D fill of the plane as a surface of the desktop's depth (RGB565 or X8R8G8B8, pitch / 2 or pitch / 4 pixels by the rows rounded to 8; v10.248-v10.249; the tiling does not matter when every byte gets the value). Only a refused fill (SKIP) falls back to the CPU; a fill that fired and failed is a lost device, never replayed. Because the plane holds the stencil, a Z clear writes whole Z words in one fill and a stencil-only clear writes no Z words (v10.249). XP does the same with an 8bpp fill (pitch 256 at 330600 in T52). A partial (scissored) clear, or a refused fill, is CPU writes over the tiles with byte masks at the edges.
- The VxD's validation refuses a stencil draw or clear whose plane is outside the frame buffer or overlaps the target, and, with Coarse Z on, one whose storage does.

**Attempts that led here** (newest first):

| Build | Tried | Result |
| --- | --- | --- |
| v10.299 | speed experiments: write-back mode 0, the reading-Z cache, MIN-only merging, retest bits, the optimized rule with PASS retested | all right but 112-120 ms against 73 off: Coarse Z off by default |
| v10.298 | Coarse Z on with the conservative merging rule (01/29 bits 13:12 = 01) | the rule that drew every depth scene right in v10.297's experiments |
| v10.297 | Coarse Z off by default; nine experiments (TEXDBG D8 + n, D7) run by VXGL v2.8 | write only, PASS retested, all retested and conservative merging right; the rest unchanged |
| v10.296 | the state cache fixed, LESS/LESSEQUAL only, swrast's depth writes drop the records, the storage filled by the 2D engine | the same five VXGL depth scenes wrong, the same counts; right with it off |
| v10.295 | XP's rule: 2200 on every depth-writing draw after a far clear filled the storage | VXSTEN and VXTEX pass; five VXGL depth scenes wrong (farther over nearer) |
| v10.294 | Coarse Z (01/01 2200) off; its storage reserved after the plane | VXSTEN passes after boot, VXTEX and VXDX7; turned back on, the 8x8 tiles return |
| v10.293 | 01/19 low each draw; 01/28 bit 23 pulsed | no change |
| v10.290-v10.292 | fresh-context probes: the run's order, swaps, waits, constant groups and asynchronous clears off, engine-drawn clears | none brought the 8x8 tiles or took them away |
| v10.247 | both plane bits on for every stencil draw | all OpenGL and Direct3D stencil tests pass |
| v10.246 | the 8x8 tiled layout everywhere (HAL copies, clears, ICD) | Direct3D passes; OpenGL fails with read-only draws, passes with both bits |
| v10.245 | 28/29/2A as XP; plane after the Z words, linear rows | the band is gone; CPU and engine disagree on each pixel's byte |
| v10.244 | Z write bit with 01/14 = 0, or 008000 | depth written, stencil still not found |
| v10.243 | 01/17 = 80; 01/01 \| 2200 | 2200 stopped the screen reads; stencil still not found |
| v10.242 | Z write bit for stencil draws without Z writes | only wrote depth |
| v10.241 | none (28-2A = 0) | a dark band at the top of the desktop from each engine stencil write; EQUAL tests read the screen's bytes |

**Lessons.** A stencil test that only reads back the engine's own writes proves nothing about where they go: test CPU-written stencil against the engine, and watch the top of the screen (rows 0-35) for writes to video memory 0. Find a layout with one-pixel writes and one-byte reads, not with shapes. Decode every bit copied from XP against the manual: 2200, copied in v10.243 because it stopped the screen reads, turned on Coarse Z, whose storage nobody had reserved. A fault that depends on what ran before is memory nobody set: look for an address the engine uses that no allocation covers. A capture shows one setting that worked for XP's own draws, not the setting for every draw: after three builds of copying T50, one board run of nine documented variants (each drawn, read back and counted against the software path) found the rule. Sweep the manual's bits on the board before guessing a fourth time.

**Tests.** VXDX7 --stencil (the CPU's 3 in each byte of the word, engine REPLACE, own stencil, screen top; modes 2 and 1); VXDX7 --splane (one-pixel writes, one-byte reads, REPLACE/EQUAL orders including OpenGL's; uses VX\_DD\_StencilPlane for raw bytes); VXGL.EXE scenes A-E. Registry ACCEL\_D3D\_STENCIL\_ZW (HKLM\\Software\\VX85598, DWORD): 2 (default) both plane bits, 1 bits as each draw needs, 0 no plane (draws on the screen; diagnosis only). TEXDBG B2-B4 set 0-2 at run time, B7 reads. ACCEL\_3D\_COARSEZ (DWORD; removed in v10.430 with TEXDBG D4-DF): 0 (default from v10.299) off, 1 Coarse Z as above, 2 not on alpha-tested or colour-keyed draws; TEXDBG D4-D6 set 0-2, CE \[4\] reads it and \[5..7\] the storage fills by the 2D engine and the CPU and the clears that dropped records. TEXDBG D8 + n (D7 for 8) are the board experiments (vx3dn\_cz\_dbg, from v10.299: 1 write-back mode 0, 2 01/01 0200 only, 3 the reading-Z cache, Coarse Z on or off, 4 MIN-only merging, 5 29 | 400400, 6 29 | 0200, 7 the optimized rule, 8 the optimized rule with 400400; v10.297's were 1 XP's 01/1E-20, 3 2000 only, 4 the tags reset every draw, 8 untextured draws as T50); VXGL v3.0 runs them on its depth scenes and times each, VXTEX times 20 full-window quads with Coarse Z on and off.

## 18. OpenGL ICD: VXGL.DLL (v10.239-v10.300)

The driver has its own OpenGL ICD, VXGL.DLL. Mesa 7.0.3 does the API, transform, lighting and clipping, and the engine draws through the same core and VxD packets as Direct3D, textures included from v10.268. Board, v10.247 (VXGL v2.1, 256x256 at 16 bpp): 24 scenes, every judged one within 1% of the ICD's own software path, failures 0; the 12288-triangle lit sphere 50.5 ms a frame (engine off 78.7, Microsoft's software 28.6). Board, v10.275 (VXTEX v1.3, 32 bpp): all 18 texture cases pass and match the software path pixel for pixel; textured fill 235 million pixels a second (software 0.92); a bind and a tiny triangle 69 microseconds; small textured quads with a bind every 8, 17.3 microseconds a quad. Board, v10.294 (VXTEX v1.5, 16 bpp): every case passes, no pixel more than 17 apart; textured fill 260 million pixels a second; a bind and a tiny triangle 61.8 microseconds; small quads 16.4. VXSTEN passes after boot, after VXTEX and after VXDX7. Board, v10.299 (VXGL v3.0, 16 bpp): all 28 scenes within 1% of the software path (multipass GL\_EQUAL shown, not judged), wide lines and large points 0 pixels, logic ops 0, colour masks 47; the lit sphere 33 ms a frame. Board, v10.303 (VXGL v3.3, 32 bpp): all 33 scenes within 1% of the software path, GL\_BLEND 0 pixels (9630 before the one-stage last-read fix, 16.12); the lit sphere 33.4 ms a frame.

Board, v10.306 (VXGL v3.4, VXTEX v1.6, VXDX7 v2.76, 32 bpp): every program passes, no engine reset; all 37 VXGL scenes within 1% of the software path, scenes 27 and 32-34 and 36 0 pixels, 35 1 pixel (its 1x1 level); the lit sphere 33.4 ms a frame.

**How Windows finds it.** VX85598.DRV answers escape 0x1101 (OPENGL\_GETINFO) with the name VX855GL. opengl32.dll looks that up under HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\OpenGLdrivers (VX855GL = VXGL.DLL, set by the INF or VXGL.REG) and calls the Drv\* entry points. VXGL.DLL is based at 0x21000000.

**How it is built.**

- Mesa 7.0.3 (MIT), only the parts the DRI drivers use: main, glapi, math, tnl, vbo, swrast, swrast\_setup, shader. GL\_VERSION 2.1, 2634-character GL\_EXTENSIONS.
- The back end (gl/vxgl\_mesa.c, vxgl\_state.c, vxgl\_tris.c, modelled on the unichrome DRI driver) turns Mesa's vertices and state into the Direct3D core's (vx\_d3d\_gl\_begin/prim/flush/state/clear), which builds the VxD's packets.
- DirectDraw (VXDDRAW.DLL) makes the back buffer and depth buffer in video memory; SwapBuffers is a blit to the window through a clipper. A double-buffered window's front buffer is its own surface (v10.249): made the first time it is drawn or read, filled from the window the first time after each swap, drawn by the engine when the front is the only draw buffer (ACCEL\_OPENGL\_FRONT, v10.317; FRONT\_AND\_BACK from v10.340, 19.8) and otherwise by swrast, and shown at glFlush / glFinish when drawn (it shared the back buffer's memory before).
- Surface offsets come from VX\_DD\_Surface (DirectDraw's fpVidMem less the frame buffer's base); the stencil plane from VX\_DD\_StencilPlane.
- What the engine cannot draw goes to Mesa's software rasterizer on the same surfaces, locked with DDLOCK\_NOSYSLOCK; the lock waits for the engine first.
- Textures (gl/vxgl\_tex.c): a texture an engine draw uses gets a video-memory copy (A8R8G8B8, X8R8G8B8 for RGB) made through DirectDraw as the HAL makes Direct3D's; the units' environments become Direct3D texture stage state, so the core composes the pixel program as for Direct3D (services resolve and mips find the surfaces). From v10.302 the upload copies Mesa's RGBA and RGB images by reading their rows directly: 0.02 microseconds a texel on the board, against 0.16 through FetchTexelc, which other formats still use. A copy made just now needs no batch flush.
- swrast keeps its DirectDraw locks between batches and lets them go before anything else uses the surfaces (ACCEL\_OPENGL\_LOCKKEEP, v10.267: a lock is four DirectDraw calls and a wait for the engine).
- Offscreen draw packets wait in a queue (gl/vxgl\_batch.c) and go 32 to a VxD call (ACCEL\_OPENGL\_BATCH; 16 in v10.275, 32 from v10.315). The queue is sent before a clear or any lone packet, swrast's access, every kept-lock release (swap, blit, front buffer, glFinish, context switch, thread exit, surfaces made again or freed), glFlush, and a texture written or freed.
- The core's batch belongs to the context that drew it (vxgl\_batch\_ctx, v10.288). Every flush goes through that context, so its window's kept swrast locks are let go; a context starting to draw sends another's batch first; SwapBuffers with no current context sends it; a broken context sends its batch before it is detached.
- Texture copies (v10.287-v10.288): a copy is freed only after the core's batch and the queue are sent, under the engine lock. A copy lost with a mode change is restored with each level's offset and pitch read again from the HAL, then uploaded again. A copy that found no video memory, or a level that could not be locked, stays failed until the texture, its levels or the free memory change; it is not tried again at every state change.

**What draws where.**

| Path | Features |
| --- | --- |
| Engine | clears (colour, depth, stencil, scissored), smooth and flat triangles, quads, strips, fans, polygons, lines, points, depth test, blending (destination alpha reads 1; from v10.304 every equation and the constant colour, 16.19), alpha test, culling, polygon offset, unfilled polygons, two-sided lighting, per-vertex fog, separate specular, stencil (two-sided too, per triangle, from v10.263), scissor, dither, textures (below); from v10.295 logic ops (01/53 bits 11:8 the op as a truth table of source 1100 and destination 1010: all 16 right on the board), colour masks (01/53 bits 3:0 are B 1, G 2, R 4, A 8, the pixel's byte order: v10.296), wide lines and large points (v10.296: swrast's exact pixels, a rectangle per run of a line's major axis); from v10.300 textured one-pixel lines, line stipple, attenuated points and point sprites; from v10.308 polygon stipple (16.20) |
| Software (swrast) | the texture features listed below, the smooth points, lines and polygons and the fragment programs and shaders the engine cannot draw (16.23, 19.5, 19.6), feedback and selection, accumulation (system memory), DrawPixels, CopyPixels, ReadPixels |

Pixel formats follow the desktop (16 or 32 bpp): double-buffered with depth 16, depth 24 + stencil 8, or no depth; single-buffered with depth 16 or 24 + stencil 8; accumulation 64. VXGL.LOG names each fallback by its bit: 0001 texture, 0002 state the core refuses, 0004 logic op, 0008 colour mask, 0010 blend, 0020 polygon stipple with ACCEL\_OPENGL\_PSTIPPLE = 0, or smooth polygons, 0040 draw buffer, 0080 feedback/select, 0100 no video memory surfaces, 0200 programs, 0400 ACCEL\_OPENGL\_HW = 0, 0800 textured lines or points or q other than 1, 1000 an occlusion query counting in software, 2000 lines, points or unfilled polygons under polygon stipple, 4000 a point or line condition of this draw (v10.341, decided per draw), 8000 a depth notice to the VxD not yet through (v10.342).

**Textures on the engine:** 1D and 2D, power-of-two sizes up to 2048, no border; RGBA, RGB, LUMINANCE, LUMINANCE\_ALPHA, ALPHA, INTENSITY and paletted (the palette applied when the copy is made, made again when the palette changes); REPLACE, MODULATE, DECAL and ADD; all filters; mipmaps on the first unit (BaseLevel to \_MaxLevel, nine levels at most, the full chain to 1x1: the engine reads a missing level at offset 0); REPEAT, CLAMP, CLAMP\_TO\_EDGE and MIRRORED\_REPEAT; LOD bias; two units. Rows are max(32, width x 4) bytes, the copy 256-byte aligned. From v10.295 the engine also draws GL\_COMBINE (REPLACE, MODULATE x1/x2/x4, ADD, ADD\_SIGNED x1/x2, SUBTRACT, INTERPOLATE by an alpha, DOT3\_RGBA; CONSTANT is one TFACTOR for all units), cube maps on the only unit without mipmaps, and projective coordinates on the only flat unit (q from texgen or the texture matrix; from v10.300 from the vertices too); from v10.300 GL\_BLEND with a black or white environment colour (MODULATE by the texture's complement, ADDSMOOTH). From v10.304 (16.19) also GL\_BLEND with any colour and INTERPOLATE by a colour (one interpolation instruction, the only unit), the RGB and alpha scales, CLAMP\_TO\_BORDER with NEAREST filters, chains shorter or longer than the full one (the engine's last level is 8: a 512 texture's 1x1 is not reached), and mipmaps on the second unit. From v10.317 (16.22) and v10.321 (16.23) the engine also draws DOT3\_RGB, 3D textures, cube maps with mipmaps and beside another unit, CLAMP\_TO\_BORDER with linear filters, MIN\_LOD above 0 and fractional MAX\_LOD, depth textures, colour tables and non-power-of-two sizes; software keeps shadow compare (the sampler has no depth compare and its stage arithmetic is 8 bits). From v10.307 S3TC (DXT1, DXT3, DXT5 copies on the engine, 16.20), from v10.313 three units (16.20), from v10.315 whole-level LOD clamps (16.21), from v10.353 MIRROR\_CLAMP\_TO\_EDGE (19.7). GL\_CLAMP filtered linearly blends in the border colour as GL does by default from v10.357 (20.9); ACCEL\_OPENGL\_CLAMP 1 clamps to the edge texel on the engine instead (as the cards of the time did).

**Registry** (HKLM\\Software\\VX85598, DWORD):

| Value | Default | Effect |
| --- | --- | --- |
| ACCEL\_OPENGL | 1 | 0: DrvValidateVersion refuses; opengl32 uses Microsoft's renderer |
| ACCEL\_OPENGL\_HW | 1 | 0: keep the ICD, draw everything with swrast (read at each context creation) |
| ACCEL\_OPENGL\_BATCH | 32 | draw packets a VxD call (at most 32); 0 or 1: one a call |
| ACCEL\_OPENGL\_LOCKKEEP | 1 | 0: swrast locks and unlocks the surfaces around every batch |
| ACCEL\_OPENGL\_CLAMP | 0 | 0 (default from v10.357): GL\_CLAMP filtered linearly exact, on the engine through a ring copy while the coordinates stay in 0..1, otherwise software (20.9); 1: the engine clamps to the edge texel |
| ACCEL\_OPENGL\_EXTLEN | 0 | bytes of GL\_EXTENSIONS, the 0 included (0: Mesa's whole 2634-character string; classic names first when trimmed) |
| VXGL\_LOG | 0 | 1: write C:\\VXGL.LOG (formats, surfaces, the first packets and swaps, fallbacks, per-context counts; at each context's end the profile: submits, the VxD's parts, the fetcher, the queue, the state diff, VXGL's own parts, and the 2D engine's state) |
| ACCEL\_D3D\_STENCIL\_ZW | 2 | the stencil plane bits (section 17) |
| ACCEL\_OPENGL\_COMBINE, \_CUBE, \_PROJECTED | 1 | 0: GL\_COMBINE, cube maps, projective coordinates in software (v10.295) |
| ACCEL\_OPENGL\_LOGICOP, \_COLORMASK, \_WIDE | 1 | 0: logic ops, partial colour masks, wide lines and large or textured points in software (v10.295) |
| ACCEL\_OPENGL\_LINES, \_POINTS | 1 | 0: textured one-pixel lines and line stipple; attenuated points and point sprites in software (v10.300) |
| ACCEL\_OPENGL\_BLENDENV, \_QVERTEX | 1 | 0: GL\_BLEND with a black or white colour; q in the vertices in software (v10.300) |
| ACCEL\_3D\_COARSEZ | 0 | Coarse Z (section 17): right with 1, but slower here |
| ACCEL\_OPENGL\_BLENDOP, \_SCALE, \_LERP | 1 | 0: blend equations and the constant colour; RGB and alpha scales; GL\_BLEND with other colours and INTERPOLATE by a colour in software (v10.304, 16.19) |
| ACCEL\_OPENGL\_UNITMIP, \_BORDER (ACCEL\_OPENGL\_LONGMIP: 20.8, default 0 from v10.357) | 1 | 0: mipmaps on the second unit; CLAMP\_TO\_BORDER in software (v10.304, 16.19) |

**Traps that cost a build each.**

- On Windows 98 a DirectDraw Lock returns an alias of video memory (fpAliasedVidMem), not the frame buffer's own address. Lock address less the base put every surface "outside video memory" (v10.240); use the HAL's fpVidMem (VX\_DD\_Surface).
- GetModuleHandle("VXDDRAW.DLL") finds nothing: DirectDraw maps the HAL in the shared arena without listing it in the process. Use LoadLibrary.
- Mesa 7.0.3 leaves \_glapi\_check\_multithread to the driver; without it one thread's MakeCurrent changes another's dispatch. Call it at every MakeCurrent, and flush queued vertices (FLUSH\_VERTICES) before switching.
- Mesa locks zero-filled static CRITICAL\_SECTIONs it never initialises, which Windows 98's kernel32 does not survive: glthread.h's mutexes are spin locks here.
- Windows 98 reuses window handles: know a window by its handle plus a property set at SetPixelFormat.
- A surface the lock cannot map (lost after a mode change or Alt-Tab) gets system memory in its place, and the surfaces are made again at the next validate; release surfaces only after the engine has finished with them.
- Mesa patches: glheader.h (stdint.h for Watcom), s\_blend.c (\_BLENDAPI), glprocs.h (no dispatch stubs under Watcom), vbo\_exec\_api.c (inputs\[\] loop bounded), glthread.h (spin locks), colormac.h (PACK\_COLOR\_8888 shift unsigned).
- A texture's copy changes under queued or batched draws: send the core's batch (vxgl\_hw\_flush) and the queue before a texture is written or freed, and build the core's template again after an upload (c->tex\_changed), or a draw samples the old copy (v10.268).
- Surfaces made again at the same size and offset after a mode change left the core on the old generation, and every draw was refused STALE until the next swap. Follow a count of remakes, not the offset (v10.288).
- StretchDIBits returns 0 when it showed nothing. Treat 0 and GDI\_ERROR as failures and keep the front buffer to show at the next glFlush or glFinish; still report SwapBuffers as done, because games stop on a failed swap (v10.287).
- A guard that counts memory given back must not count its own clean-up: two failing textures set each other off at every state change (v10.288).
- glFinish waited by locking the back and depth surfaces, and the depth lock dropped Coarse Z's records each time. Wait for the engine instead: VX\_IOCTL\_2D\_ACCESS action 0 (v10.302).
- A single-buffered window's surfaces were checked only by glViewport, so an application that never called it again stayed on lost surfaces. Check them at glFlush and glFinish as a swap does (v10.302).
- With no alpha bits in the target, glBlendFuncSeparate's alpha factors change nothing. Judge only the RGB equation and factors, or the draw goes to software for nothing (v10.302).
- Translate the state only for the groups it reads (VXGL\_STATE\_MASK). Before, every glRotate between draws cost a full translation: a 2 KB compare and a lost check per texture (v10.302).
- At a wide line's ends, the outer half pixel can extrapolate rhw to 0 or below. Give that edge the end's own values (v10.301).
- The first texture surface after boot takes about 80 ms to make. Compare upload times only between runs with the same program order (v10.302).

**Measured differences.** The engine's depth for a plane is 0 to 5 steps below the exact value at 24 bits; swrast's is spread both ways. A multipass GL\_EQUAL across the engine and swrast therefore fails (24% of pixels): programs that mix paths for one surface need the same path for both passes. OpenGL's point rule is applied in the core (a point on a pixel edge goes to the pixel OpenGL chooses). The engine dithers at 16 bpp when GL\_DITHER is on (OpenGL's default), swrast does not. X8R8G8B8 buffers have no alpha bits, so the span functions read alpha as 1.0 (v10.249; the stored high byte was returned before). Texture sampling (VXTEX probes, v10.270): bilinear weights are 1/32 steps rounded to nearest; the mip LOD fraction is in 1/32 steps, one below exact; VXTEX allows 10 for bilinear and 16 between levels. With dither off the engine converts to 565 by truncation in every setting of 01/53 bits 16:15 (they change nothing); swrast rounds to nearest, so the two can be two 5-bit steps (17) apart, which VXTEX v1.5 allows at 16 bpp (v10.291).

**Tests.** VXGL.EXE draws each scene three times (engine, Microsoft's renderer, the ICD with the engine off), judges the engine against the third, writes VXGL.TXT, BMPs for failing scenes, watches screen rows 0-35, and measures depth rounding; "VXGL n" runs the speed scene n frames. VXSTEN.EXE (stencil, v10.264) and VXTEX.EXE (textures, v10.266) check cases against worked-out answers on the engine, the engine off and Microsoft's renderer, then the engine against the software path pixel for pixel; VXTEX also times binds, fill and small quads and, from v1.3, checks the 2D engine before and after each run. VXSTEN v1.7 also runs cases 0, 1, 2 and 7 in fresh contexts (as the run, with engine-drawn clears, with a tested draw after each clear, and with Coarse Z on) and prints any fetch group that did not finish. Run it after boot, after VXTEX and after VXDX7: faults that depend on what ran before (section 17) show only there.

**Open.** The cost of a bind (61.8 microseconds, split in 16.18); EXT\_stencil\_two\_side (Mesa 7.0.3 cannot offer it beside OpenGL 2.0's separate stencil); a retest of stencil draws that only read the plane (section 17); why Coarse Z is slower than none on this board (section 17).

**Hardware T&L (side research, VXTLP7–VXTLP11, October 2026): CPU T&L stays the default.** Pixel shaders can do the vertex math. The test used six R32F source planes, up to 192 vertices a batch, four coordinate passes and one Lambert lighting pass. The GPU-made vertices fed the indexed draw with no CPU reads, and every numeric, image and guard check passed. It is still about three times slower than the CPU. At 192 vertices the best GPU path took 1664 µs (affine) and 1752 µs (perspective) a batch, against 559 and 570 µs on the CPU. No hardware T&L is advertised.

**What carries over from that work:**

- A bulk copy into the frame buffer uploaded 32 KiB in about 19 µs. Per-word callbacks took 533 µs.
- Replaying a prepared register stream cut a batch's wall time by 32–42% against building it live each time.
- A synchronous fetch per draw through a command buffer was 8–12% slower than direct register writes.
- Not tried: fewer producer passes, reusable GPU-fetched command buffers, larger unique-vertex layouts.

## 19. OpenGL: the engine's depth clear, speed, and the late features (v10.323-v10.353)

This section is what Quake 2 3.14 taught the ICD. It covers depth clears the engine must draw itself, where a frame's time goes on the 1 GHz C7, and the recipes that took the timedemo from 9 to 17 frames a second. It also covers five cases that left software in v10.353: smooth points and lines with the alpha test, a fragment program's KIL as the alpha test, a program's fog option, and GL\_MIRROR\_CLAMP\_TO\_EDGE magnified and mipmapped.

**Board results.**

- v10.323 (1024x768x32): every test program passes. The lit sphere (VXGL scene 57; 62 from VXGL v4.5) drew in 22.0 ms a frame; Microsoft's software OpenGL took 26.3 ms.
- Quake 2 3.14 timedemo 1, 640x480: 9 fps (v10.347), 9.6 (v10.348), 12 (v10.350), 17 (v10.351).
- Heavy particle frames fell from 90 ms to 34-47 ms; plain frames fell from 18 ms to 15 ms.
- Steady scenes ran at 52 fps before this work and 67 fps after it.

### 19.1 Depth clears the engine sees (v10.343-v10.348)

**Symptom.** Quake 2 3.14 with gl\_ztrick 0 clears the depth buffer every frame. On the engine, only the first frame's world landed. Later world draws were lost, while the HUD, the damage flash and the menu fade (all without the depth test) kept drawing over a frozen picture. The following all worked:

- gl\_ztrick 1, which never clears depth
- software
- a recorded trace of the game played back with the engine finishing every frame

The per-frame log (VXGL\_FRAMELOG, 19.3) showed every frame the same. The depth clear went to the engine at 640x480 with z 1.0 and the depth mask on. The world then drew on the engine with the test on, LEQUAL, range 0-1, the same depth and target surfaces, and no software or fallback. CPU clears (ACCEL\_3D\_CLEAR 0) with the VxD's state cache off changed nothing.

**Cause.** A depth clear written behind the 3D engine's back is not seen by the engine's following draws, whether a 2D-engine fill or CPU writes made it. A clear the 3D engine draws itself is seen. The 2D fill and the 3D draws use the same surface at the same offset, so it is not an address error. Which part of the engine holds the old depth was not identified; Coarse Z is off by default (section 17), and locking the surface or waiting for the engine around the fill (ACCEL\_OPENGL\_ZLOCK, below) was the test for that.

**Recipe (ACCEL\_OPENGL\_ZCLEAR3D, default 1 from v10.346).** Draw the clear as a quad over the whole window in the 3D engine:

- Draw one GL\_QUADS through Mesa's execute table (ctx->Exec, so a display list being compiled does not record it). Set identity projection and modelview, viewport 0,0 to the buffer's width and height, and depth range 0-1. Put the vertices at x, y = -1 and 1 and z = clear depth x 2 - 1, clamped to -1..1.
- Depth: test on, function ALWAYS. Writes come from the application's depth mask, which must be on for a depth clear.
- Stencil (D24S8 and a write mask that is not 0): test on, StencilFunc(ALWAYS, clear value, 0xFF), StencilOp REPLACE x 3. The application's stencil write mask applies, as glClear's does.
- Colour of the same glClear: only when all four channels are written and there is one draw buffer. Draw the quad in the clear colour, so no 2D fill follows it. Otherwise set the colour mask all off (19.2). A colour-only glClear stays the 2D fill.
- glClear's rules: the scissor and the write masks apply; nothing else does. Turn off the alpha test, culling, fog, lighting, blend, polygon stipple, polygon offset, polygon smoothing, logic op, every clip plane, every texture target on every unit, fragment and vertex programs, ATI fragment shaders, the colour sum and two-sided stencil. Set polygon mode to FILL.
- Save and restore cheaply. Push only GL\_ENABLE\_BIT, plus GL\_STENCIL\_BUFFER\_BIT when stencil is cleared. Keep and set back yourself the matrix mode, the active texture unit, viewport and depth range, the depth function, the polygon modes, the colour mask or current colour, and ctx->ErrorValue (the application's glGetError sees only its own errors).
- The fragment program and colour sum enables are not in Mesa 7.0's enable group, so set them back by hand. v10.346 left them off after a clear.
- Pushing GL\_ALL\_ATTRIB\_BITS, as v10.345-v10.347 did, costs about 180,000 instructions a clear: the pop binds every texture object and sets every light again, and forces a full state translation with texture checks. Quake 2's clear time fell from 1.39 to 0.19 ms a frame without it.
- Use the 2D fill instead when:
  - the attribute or a matrix stack is full
  - an occlusion query is running (the quad would count)
  - a GLSL program is current
  - a framebuffer object is bound
  - the engine is off, or the draw buffer or render mode already sends drawing to software

With it, Quake 2 draws right with gl\_ztrick 0 and 1, and the timedemo is the same either way. ACCEL\_OPENGL\_ZCLEAR3D = 0 returns to the 2D fill. ACCEL\_OPENGL\_ZLOCK (bits: 1 wait for the engine after the clear, 2 lock and unlock the depth surface after it, 4 the same for the back surface, 8 wait before it, 16 lock and unlock depth before it) stays as a diagnostic for the 2D fill path.

### 19.2 Colour mask all off (v10.346-v10.347)

With every colour channel masked off, such as depth-only passes or the depth clear above, set the engine's channel write mask (render state 168) to 0. Turn blending off, with the blend equation ADD. Before, the ICD blended ZERO / ONE, which makes the engine read the colour for nothing. On the board, Quake 2 draws right with it and the channel mask all off writes no colour. Default on from v10.347; ACCEL\_OPENGL\_NOCOLOR = 0 returns to blend ZERO / ONE. The equation must be ADD: MIN, MAX and the subtractions kept from the application's blend still wrote colour (v10.330).

### 19.3 Measuring a frame (v10.343-v10.349)

Three diagnostics, all off by default (HKLM\\Software\\VX85598, DWORD 1 turns each on; VXGL.LOG is written with VXGL\_LOG = 1; VXGL\_FRAMELOG turns it on by itself, VXGL\_SAMPLE needs VXGL\_LOG = 1 as well):

- **VXGL\_LOG: the frame report (v10.347).** Every 200 swaps and at the context's end, VXGL.LOG gets one line. It gives frames, seconds and frames a second, then a frame's milliseconds in four parts:
  - pipeline runs, with the VxD's submits inside them; these also hold the state translations and the core's primitives
  - clears
  - the swap's blit
  - outside VXGL: the application and Mesa's front end (glVertex and the like, glTexImage's own copy)

  It also gives a frame's pipeline runs, primitive calls, state translations and texture uploads. Time is read with RDTSC and converted with a 20 ms calibration against QueryPerformanceCounter.
- **VXGL\_SAMPLE: the sampling profile (v10.349).** A thread suspends the thread that made the context about a thousand times a second, reads its EIP with GetThreadContext and resumes it. For each 200-frame window slower than 30 ms a frame (four at most) VXGL.LOG gets each module's share of the samples and VXGL.DLL's 120 busiest addresses, as RVAs to 32 bytes. Resolve the RVAs against the build's VXGL.map (VXGL.DLL is based at 0x21000000). Static functions are not in the map's public list, so use the disassembler's labels for the object (Watcom wdis) to name them.
- **VXGL\_FRAMELOG: one line a frame (v10.343).** This covers a context's first 400 frames. Each line gives the engine's triangles and clears, software primitives, the fallback bits and the depth-dirty mark. Each clear gives the engine flags, area, depth value, depth mask, depth surface and what software cleared. The first depth-tested draw after a clear gives engine or software, function, mask, range, depth surface and target. This log is what showed the gl\_ztrick 0 frames identical (19.1).

**What the first measurements showed (Quake 2 timedemo, v10.348).** A heavy frame took 75 ms, of which 57 were in pipeline runs: about 2,500 triangles at about 10 microseconds each on the core's per-triangle path, plus 9 ms of VxD submits. Another 17 ms were outside VXGL, and the engine waited on itself 2 ms a frame. The profile then found (v10.350): VXGL.DLL had 84% of the drawing thread. Of VXGL's samples, 40% were in the core's 1/16-pixel snap and 30% in Watcom's float-to-integer helper \_\_CHP. Most of the rest was the smooth points' rectangles, because Quake 2 3.14 draws its particles as smooth points.

### 19.4 Speed recipes (v10.323-v10.352)

Each recipe draws the same pixels as before, and only the cost changes. The exception is the snap, which moves the engine's edges onto software's.

- **Strips through the core (v10.323, ACCEL\_OPENGL\_STRIPS).** Send a triangle strip or a quad strip from the plain path as one strip run (vx\_d3d\_gl\_strip): one vertex and one bounds check a triangle. Decide the template, culling and the run once, not again for every triangle. Send a quad strip pair-swapped (v1 v0 v3 v2 ...) to get the same triangles and diagonal as Mesa's quads. Loose quads had gone as two triangles that did not continue the run, so a packet held 32 runs of 2 triangles. The lit sphere sent 192 packets a frame with 3 vertices a triangle; with strips it sends 66 packets at about 1 vertex a triangle. About 4.5 microseconds of VxD bookkeeping a packet is what remains (packets hold 192 vertices at most).
- **Mesa's untextured emitter (v10.323).** A hardwired vertex emitter (t\_vertex\_generic.c) for the ICD's position-plus-colour layout, which went through the generic per-attribute emitter.
- **Snap to swrast's grid (v10.324, ACCEL\_OPENGL\_SNAP).** swrast snaps each triangle's x + 0.5 and y - 0.5 to 1/16 of a pixel (s\_tritemp.h, SUB\_PIXEL\_BITS 4). The core snaps a GL triangle's coordinates the same way in the engine's coordinates: x down, y down for a framebuffer object and up for the window's flipped rows. This applies on the fast path, in strips and to clipped triangles. Lines, points and Direct3D keep their coordinates; the ICD's wide-line, point and smoothing quads already sit on half pixels. Without the snap, an edge between a pixel centre and the grid point fell on the other side (scene 56's red triangle). Pixels apart from software fell: scene 1 from 21 to 10, scene 6 from 8 to 0, scene 7 from 16 to 3, scene 56 from 19 to 3.
- **Points and lines decide only their own draws (v10.341).** Quake 2 3.14 finds GL\_EXT\_point\_parameters, then enables GL\_POINT\_SMOOTH, and keeps GL\_ALPHA\_TEST on for the whole game. Smooth points with the alpha test were software, and that condition sent every draw to software, including the world's triangles, so the picture was dark. Conditions that only concern points (smoothing with the alpha test, attenuation or sprites, sizes) or only lines (smoothing, stipple, width) now send only the draws that contain points or lines to software (fallback bit VXGL\_FB\_PRIMSTATE, decided per draw). Polygons drawn as points or lines count as points or lines. Polygon smoothing still sends everything.
- **Mesa draws unclipped primitives with the driver's table (v10.348, tnl/t\_vb\_render.c).** In a vertex buffer holding any clipped vertex, Mesa 7.0 drew every primitive through its clipping table, a triangle at a time. OR the clip bits per primitive: a primitive none of whose vertices is clipped goes through the driver's own table.
- **Polygons, fans and quads as short strips (v10.348).** A fan's triangles (0 k k+1), (0 k+1 k+2), (0 k+2 k+3) are the strip k k+1 0 k+2 k+3, three triangles a strip, with the same diagonals as Mesa's fan. GL\_POLYGON, fans and the polygons the clipper cut go this way. A quad a b c d goes as a b d c, Mesa's two triangles. Flat shading (Quake 2's default) is kept when all of a primitive's vertices have one colour, since the provoking vertex then does not matter. These had gone through the core's general path one triangle at a time. Measured on a recorded Quake trace: 448 million instructions in the driver and Mesa before, 325 million after the three v10.348 changes.
- **Float to integer without \_\_CHP (v10.350-v10.352).** Watcom compiles (int)x and (unsigned)x as a call to \_\_CHP, which sets the FPU's rounding mode to truncate and back for each conversion. That costs tens of cycles on the C7 and was 30% of VXGL's samples. Do the conversions on the float's bits instead:
  - snapw (the 1/16-pixel snap): an integer rounding on the encoding, with the highest set bit found by BSR (a Watcom #pragma aux "bsr eax,eax"). The result float is built from its bits as n / 2048 with n a multiple of 128, which is exact. NaN, infinity and |x| >= 2^20 keep the double path.
  - u8r: (uint32\_t)(d + 0.5) on a double's encoding, for the core's colour and specular bytes.
  - ub\_trunc: the smooth rectangles' and wide lines' alpha bytes, by truncation on the float's encoding.

  Each was compared with the old code for all 2^32 float encodings (snapw both directions, with Watcom's 32-bit long and x87's integer indefinite). u8r was compared for every float from 0 to 255, each rounding boundary and its neighbours, and 10^8 random doubles. No input differed.
- **Rectangles as 4-vertex strips (v10.350).** The smooth and wide primitives' rectangles go as one strip of the same two triangles, in either winding, not as two triangles.
- **The core's clipper (v10.352; VXDDRAW.DLL's Direct3D shares it).** Compute each vertex's distance to a plane once, not once for each end of its edges. Skip a plane no vertex is outside of, which copied the polygon unchanged. Step to the next vertex without a division (no %). The output is the same, with 42% fewer distance calls on Quake's trace.

**Edge vertices.** Mesa clips to GL's window, whose x = 0 and y = 0 are the engine's -0.5. Every polygon cut at the window's left or top edge therefore arrives with vertices at -0.5. The fast path and the VxD take only 0 and above, so these go to the core's general path, which clips them. This is not changed: the VxD refuses negative coordinates.

### 19.5 Fragment programs (v10.326-v10.353, vxgl\_fp.c, ACCEL\_OPENGL\_FPROG)

The translator turns each result component into a polynomial over the program's inputs and searches for a cascade of the engine's texture stages that computes it (16.23). These rules decide when it may say yes.

**Rounding.** swrast computes in float32, so the translator must keep what float32 keeps.

- Drop a term only when it is exactly 0. Three earlier rules each drew black where swrast drew colour:
  - dropping terms under 1e-6 of the largest (v10.322)
  - dropping terms within a bound on their rounding error (v10.326)
  - letting small residues pass as 0 everywhere (v10.327)

  The failing cases: SUB 1 - 0.99999994 times 1e7 is 98 red in swrast; 0.1 added ten times, minus 1, times 1e6 is 30 red.
- When matching a result against a stage (p\_eq), each coefficient must be within 2e-4, and the differences summed must be within 1/512. Nineteen terms of 0.000199 each passed the first test while they added up to 0.0038.
- Two clamped expressions are the same atom only when their polynomials are exactly the same. clamp(2c) and clamp(2c + 0.0001) are two atoms.
- Leave a clamp out only when the value is exactly within 0..1. A value 1e-5 over, scaled up later, drew 128 red against swrast's black.
- A coefficient that overflows or is not a number leaves the program in software. Work out FLR and FRC per component, on constants only.
- Known limit: a polynomial gives up float32's evaluation order. (D + 2^24) - 2^24 is D on the engine and 0 in swrast. Five attempts at this are written up in VX855\_FP\_Rounding\_Attempts.md; such programs draw differently.

**A constant quotient (v10.340).** When a residue is a constant multiple of the divisor, the translator solves it. An LRP by a program.local or program.env parameter between a texel and the colour then runs on the engine as a LERP with TFACTOR.

**The memo (v10.335-v10.337).** A translation took 370-656 microseconds, so two programs drawn alternately ran at 49 ms a frame.

- Keep sixteen memo entries. Key each by a 64-bit value made from two independent word hashes, and compare the program's address and instruction count as well. Hash only the local and environment parameters the instructions name.
- A hit then costs 10.5 microseconds, down from 69, and the alternating pair runs at 7.3 ms a frame.
- Give the whole stage search one budget of 20,000 steps. The largest successful search over every test program took 3,073. A program that runs out of budget is not searched again for other parameter values (a negative memo keyed without them). One program whose parameter changed every quad had taken 4.1 s a translation.

**KIL as the engine's alpha test (v10.353).** ARB's KIL kills a fragment when any component of its operand is below 0. Capture one KIL; a second KIL, KIL\_NV or GLSL discard stays in software.

- A constant component below 0 kills everything: the alpha test is NEVER. A constant component at 0 or above kills nothing and is ignored.
- Every other component must be the same polynomial P = a x (result alpha) + b, with a > 0. Find a from the first non-constant term of P that matches a term of the alpha, subtract a x alpha and require a constant remainder b.
- The fragment is kept when alpha >= -b / a. Set the engine's alpha test on, GREATEREQUAL, with the reference byte the smallest at or above 255 x (-b / a).
- The reference must lie in (0, 1\]. The engine's alpha is clamped to 0..1 and a program's is not, so 0 and below cannot be expressed.
- The engine has one alpha test. When GL\_ALPHA\_TEST is on as well, the draw goes to software.

**A program's fog option (v10.353).** OPTION ARB\_fog\_linear, \_exp or \_exp2 fogs the result in that mode whether or not GL\_FOG is on. Mesa's tnl makes the per-vertex fog factors only when GL\_FOG is on, and in GL\_FOG's mode. For the engine's draw, set ctx->Fog.Enabled and ctx->Fog.Mode to the option's around \_tnl\_run\_pipeline and put them back after it. No state is validated in between, and the pipeline's stages read them as they run. The engine applies the factors as vertex fog (the specular alpha) with fog render state 28 on. Before, this case went to software unless GL\_FOG was on in the same mode.

### 19.6 Smooth points and lines with the alpha test (v10.353, ACCEL\_OPENGL\_SMOOTHATEST)

The engine draws smooth points and lines pixel for pixel as swrast, with the coverage carried in the vertex alpha (16.23). The engine's alpha test would then test alpha x coverage. swrast tests the fragment's alpha before it multiplies in the coverage, so the engine's test is wrong for these primitives.

**Recipe.** Make the test per primitive in the ICD and draw the ones that pass with the engine's alpha test off.

- It applies only when nothing changes the alpha between the vertex and the test: no texture, fragment program or ATI shader.
- A point uses its own alpha, after its distance-attenuation fade.
- A line needs both ends to have the same alpha.
- A primitive that fails the application's test (function and reference, as glAlphaFunc) is not drawn. One that passes is drawn as before.
- Around the pipeline run, set render state 15 (alpha test enable) to 0 and back after it. Flush the batch before each change.
- Do this only when the vertex buffer holds points and lines only, polygon mode is FILL, and for smooth lines lighting is off (lit smooth points are fine). GL\_FRONT\_AND\_BACK drawing is also excluded.

These go to software as before:

- lines whose ends' alphas differ, and lit lines
- unfilled polygons
- a mix of primitive types in one draw
- textured smooth points and lines with the test
- smooth polygons with the test

ACCEL\_OPENGL\_SMOOTHATEST = 0 sends all of them to software.

This is what Quake 2 3.14's particles are: smooth points with the alpha test on.

### 19.7 Mirror clamp to edge (v10.353, ACCEL\_OPENGL\_MIRRORCLAMP)

GL\_MIRROR\_CLAMP\_TO\_EDGE\_EXT mirrors the texture once about 0 and then clamps: |s| clamped to the edge. The sampler has mirrored repeat and clamp but not this.

**Recipe.** On a side with this wrap, make the engine's copy twice as wide: the mirror image first, then the texels. Clamp it to its edge and map s' = (s + 1) / 2.

- s' = (s + 1) / 2 is linear in s, so perspective, every filter, the mip levels and the LOD carry over unchanged. Each level of the copy is the same mirrored double of the matching level.
- In the texture coordinate stage this is scale 0.5 and bias 0.5 on that side.
- Use it on power-of-two sides of up to 1024, so the double fits the sampler's 2048. Rectangles, cube maps and volumes go to software.
- GL\_MIRROR\_CLAMP\_EXT and ATI\_texture\_mirror\_once's enums of the same values also use it. MIRROR\_CLAMP\_EXT filtered linearly follows GL\_CLAMP's policy (ACCEL\_OPENGL\_CLAMP): clamped to the edge on the engine, or software with ACCEL\_OPENGL\_CLAMP = 0.

ACCEL\_OPENGL\_MIRRORCLAMP = 0 draws these in software.

### 19.8 Front and back together, alpha blend, the depth notice (v10.340-v10.342)

- **glDrawBuffer(GL\_FRONT\_AND\_BACK)**, or two draw buffers FRONT\_LEFT | BACK\_LEFT. Run the pipeline into the back buffer, then again into the front with the line stipple counter reset to where it began, then make the back the target again. Use it only when depth writes are off, stencil is off and no occlusion query is running; otherwise draw in software. With depth writes on, the second pass would see the depth the first wrote within the same draw. It would be exact only with the target and state switched per triangle, which is not done.
- **Separate alpha blend factors or equation.** No VXGL pixel format has destination alpha, so the alpha result is never stored. The engine's colour blend already gives the same pixels, so nothing goes to software for it.
- **Feedback and select modes** draw no pixels.
- **The depth notice (v10.342).** When swrast writes depth, the ICD tells the VxD the surface changed. If that notice fails, the engine's Coarse Z records (ACCEL\_3D\_COARSEZ) may still hold the old depth. A depth-tested draw then stays in software (fallback bit VXGL\_FB\_ZNOTICE) and the notice is tried again at every draw until it goes through. A framebuffer object's depth renderbuffer with a failed notice is not drawn by the engine until then.

### 19.9 Mesa 7.0.3 fixes (v10.322-v10.353)

The changes made to the ICD's copy of Mesa 7.0.3. The bug fixes were found where the engine and swrast disagreed and swrast was the one wrong.

- **swrast/s\_span.c (v10.353).** swrast defers texturing until after the depth test. With a fragment program containing KIL, or GLSL discard, the deferral ran the program after the depth test and Z write, so killed fragments wrote their depth. Do not defer when the program uses KIL (UsesKill), as later Mesa does.
- **tnl/t\_vb\_fog.c (v10.353).** With a program's fog option and GL\_FOG off, the fog stage did not run and the program fogged with coordinate 0 (no fog). Run the stage when FragmentProgram.\_Current->FogOption is not GL\_NONE.
- **tnl/t\_vb\_render.c (v10.348).** One clipped vertex sent the whole vertex buffer through the clip table (19.4).
- **tnl/t\_vertex\_generic.c (v10.323).** A hardwired emitter for position plus colour.
- **swrast/s\_aalinetemp.h (v10.334).** A smooth stippled line's run ended at its last set bit's start, and a one-bit run stayed open, so pattern 0x0001 drew the whole line. A run now ends at the end of its last bit's unit and closes at the first clear bit. The ICD's own smooth lines (vxgl\_tris.c aa\_line) follow the same rule.
- **main/teximage.c (v10.322).** glCopyTexImage and glCopyTexSubImage validated the read framebuffer only for pixel-transfer state. After an attachment switch they read the freed wrapper. Validate for \_NEW\_BUFFERS as well.
- **main/context.c free\_shared\_state, main/hash.c (v10.322).** A program freed after its shader read the freed shader, and \_mesa\_HashWalk unlocked where it should lock.
- **main/enable.c (16.23).** glEnable of DEPTH\_TEST or STENCIL\_TEST with no such buffer bound, and of GL\_TEXTURE\_\* right after binding a framebuffer object, was dropped.

### 19.10 Registry, tests, not done

**Registry** (HKLM\\Software\\VX85598, DWORD; read once when VXGL.DLL loads; ACCEL\_OPENGL\_HW again at each context):

| Key | Default | Effect |
| --- | --- | --- |
| ACCEL\_OPENGL\_ZCLEAR3D | 1 | Depth and stencil clears drawn by the 3D engine (19.1); 0: 2D fill |
| ACCEL\_OPENGL\_NOCOLOR | 1 | Colour mask all off: engine channel mask 0, no blend (19.2); 0: blend ZERO / ONE |
| ACCEL\_OPENGL\_STRIPS | 1 | Strips, fan and quad strips, 4-vertex rectangles (19.4); 0: triangles |
| ACCEL\_OPENGL\_SNAP | 1 | Triangles on swrast's 1/16-pixel grid (19.4); 0: unsnapped |
| ACCEL\_OPENGL\_SMOOTHATEST | 1 | Smooth points and lines with the alpha test on the engine (19.6) |
| ACCEL\_OPENGL\_MIRRORCLAMP | 1 | Mirror clamp to edge on the engine (19.7) |
| ACCEL\_OPENGL\_FPROG | 1 | Fragment programs, with KIL and fog options (19.5) |
| ACCEL\_OPENGL\_ZLOCK | 0 | Diagnostic: waits and locks around a 2D depth clear (19.1) |
| VXGL\_LOG | 0 | VXGL.LOG, with the frame report (19.3) |
| VXGL\_SAMPLE | 0 | Sampling profile into VXGL.LOG (19.3) |
| VXGL\_FRAMELOG | 0 | One VXGL.LOG line a frame, first 400 frames (19.3) |

**Tests.**

- Quake 2 3.14 demo, timedemo 1 at 640x480, with gl\_ztrick 0 and 1; the picture must match ACCEL\_OPENGL\_HW = 0.
- GLTRACE.DLL records a program's OpenGL calls with their data (Quake 2: +set gl\_driver gltrace; Scroll Lock records). GLREPLAY.EXE plays the record with the engine and in software and names the first frame and draw where they part.
- Q2REF drives Quake 2's own ref\_gl.dll without the game, engine against software, gl\_ztrick 0 and 1. Q2MENU replays Quake 2 3.14's calls with the menu, console, pause, damage flash and mouse release (15 cases).
- VXGL.EXE's scenes against the ICD's own software path, as in section 18. VXGL v4.5 adds scenes 57-61 for the five cases of 19.5-19.7: KIL as the alpha test (a green quad behind shows where fragments were killed), ARB\_fog\_exp2 with GL\_FOG off and GL\_FOG\_MODE LINEAR, smooth points with the alpha test, and MIRROR\_CLAMP\_TO\_EDGE magnified and mipmapped. The fog scene draws each primitive at one distance, because the engine's fog is per vertex and swrast's per pixel. Microsoft's renderer lacks those extensions and draws only the cleared window for the program and mirror scenes. The lit sphere moves from scene 57 to scene 62.
- The float conversions of 19.4 were each compared with the code they replace over every 32-bit float encoding.

**Not done.**

- GL\_FRONT\_AND\_BACK with depth writes (19.8).
- Shadow compare: the sampler has no depth compare and its stages are 8 bits.
- Edge vertices at -0.5 on the fast path (19.4).
- Hardware T&L: slower than the CPU (above).

## 20. Audit features and exact defaults (v10.354-v10.357)

v10.354 to v10.356 came from an outside review of the v10.353 source (tested on the board); v10.357 merges them and adds the exact defaults, GL\_CLAMP on a ring copy and pbuffers. Board, v10.356 with VXFEAT 1.2: 40 checks pass, 0 fail, 1 skipped (public DirectDraw alpha blits, which the runtime does not implement).

### 20.1 Depth bounds (EXT\_depth\_bounds\_test)

Mesa advertises the extension and swrast tests it, but the engine has no bounds test, and Mesa 7.0.3's swrast tested it only in its colour-index path.

- While the test is on and a depth buffer exists, every draw goes to software (fallback bit 10000, VXGL\_FB\_DEPTHBOUNDS).
- swrast/s\_span.c tests the bounds in `_swrast_write_rgba_span` before the stencil and depth writes and the query count. The test reads the **stored** depth, not the fragment's, and a rejected fragment clears its bit in the span mask, so partial spans stay right with the ordinary depth test off.
- The bounds are converted with the exact integer depth maximum in double arithmetic (a D32 maximum in float rounds up to 2^32).
- The fast paths that skip per-fragment tests (simple textured triangles, DrawPixels, CopyPixels, the query-only triangle) are not taken while the test is on (DEPTH\_BOUNDS\_BIT).
- The enable and the bounds are saved and restored with the enable and depth attribute groups (mtypes.h, attrib.c). The engine's depth-clear quad (19.1) turns the test off around itself.
- Watcom's glprocs.h had zeroed some dispatch stubs to dodge decoration problems; `gl_dispatch_stub_748(GLclampd, GLclampd)` needs its exact `GLAPIENTRY` signature so glDepthBoundsEXT resolves (`_gl_dispatch_stub_748@16`).

### 20.2 Direct3D MAXMIPLEVEL

Texture-stage state 20 is applied to the level actually sampled (apply\_maxmip, vx\_d3d\_core.c): the chain is validated, the requested base clamped to the levels there are, and the packet's offset, width, height, pitch and the subordinate level offsets rebased to it (shared-layout cube faces too). Coordinates stay normalized; LOD bias stays separate. Units 1 and 2 take it too. A volume or a cube layout that cannot be rebased consistently is reported as conflicting state.

### 20.3 Anisotropy on every unit

OpenGL now advertises EXT\_texture\_filter\_anisotropic (maximum 16) and takes the texture object's MaxAnisotropy as the native minification filter (D3D filter 3 and the ratio). Direct3D units 1 and 2 carry their own ratios in the packet's existing `umip` field, bits 20:16 (0 keeps the old ratio of 1), and the emitter programs each sampler's ratio and filter. The board confirmed the advertisement and a request of 8 read back; the image quality of the native filter has not been compared.

### 20.4 Engine clear rectangles and occlusion queries

- `vx_d3d_clear_draw` turns a depth or stencil clear of the shared clear path (Direct3D's DrawPrimitives2 clear and the legacy HAL Clear) into a self-contained six-vertex engine rectangle: untextured diffuse shader, depth ALWAYS with writes, colour mask as the clear asks, stencil ALWAYS / REPLACE. It does not depend on the application's render state. Colour-only clears keep their fill. This is 19.1's finding applied outside OpenGL's window clear. Registry ACCEL\_3D\_CLEAR3D (default 1; 0 the clear packet as before).
- The rectangle carries `VX3DN_DRAW_CLEAR` (0x80000000) in its packet's clear\_flags. The VxD puts the occlusion count aside for marked draws and resumes on the next application draw; marked packets go alone, not in a batch. Internal DirectDraw copy rectangles use the same mark.
- A hardware query whose count cannot be read reports GL\_OUT\_OF\_MEMORY and a visible result of 0xFFFFFFFF (it was a ready, too-small count).
- Board: a clear alone counts 0; a visible 96x96 rectangle 9216; a bounds-rejected draw 0. An RGB + D16 framebuffer object cleared repeatedly without a readback in between draws right.

### 20.5 Direct3D validation

ValidateDevice resolves the real surfaces, mip chains, keys and layouts through the same template decisions a draw makes, and reports combinations whose texture colour key cannot be inserted. ACCEL\_D3D\_STRICT = 1 (default 0) makes a draw that would be left out return an error instead (diagnosis; the default keeps v10.177's rule that a DrawPrimitives2 error fails EndScene).

### 20.6 WGL swap control, pixel formats and pbuffers

- **WGL\_EXT\_swap\_control** (v10.354): the interval belongs to the window; default 1, 0 allowed, values above 4 clamped to 4, negative refused (ERROR\_INVALID\_DATA), no current context ERROR\_DC\_NOT\_FOUND. The swap waits for vertical blank through DirectDraw before its blit (a copy, not a flip). Board: eight swaps 14 ms at interval 0, 264 ms at 1. An application that wants to run uncapped sets 0.
- **WGL\_ARB\_pixel\_format** (v10.357): wglChoosePixelFormatARB and wglGetPixelFormatAttribiv / fvARB over the five formats of section 18. Bit counts match as minimums, everything else exactly; what the ICD has none of (alpha, stereo, aux buffers, multisampling, overlays, colour index, GDI drawing) matches only a request for 0.
- **WGL\_ARB\_pbuffer** (v10.357): a pbuffer is a hidden popup window of its size (class VXGLPbuffer, CS\_OWNDC, allowed past the screen's size through WM\_GETMINMAXINFO) whose DC gets the pixel format through GDI's SetPixelFormat, so opengl32 accepts it at wglMakeCurrent. The ICD gives it the same video-memory buffers and engine drawing as any window; nothing reaches the screen. Up to 2048 x 2048 (WGL\_PBUFFER\_LARGEST\_ARB clamps). wglQueryPbufferARB answers width, height and lost (never).

### 20.7 DirectDraw

- **Alpha blits are not available.** The runtime refuses every alpha Blt (80070057) and its alpha flags are documented as unimplemented; v10.354's claim of the caps was withdrawn in v10.355. The RGB565 / XRGB8888 constant-alpha arithmetic stays in VXDDRAW.DLL unused by the public API.
- **Native stretch and mirror** (VX\_D3D\_Blit, ACCEL\_DDRAW\_3DBLT, default 0): an SRCCOPY stretch or mirror between video-memory surfaces of one format as an engine textured rectangle, checked for formats, texture pitch, clipped rectangles and separate allocations; primary sources, aliased allocations, keys, alpha and other ROPs keep the CPU path. Not yet qualified on the board: the passing stretch and mirror comparisons were the CPU path.
- **Destination key 0** (ACCEL\_DDRAW\_EXACTKEY, default 0): 1 honours an application's explicit override of 0 instead of v10.147's reading of it as the surface's key.

### 20.8 Exact defaults (v10.357)

The defaults are now the exact behaviour; each approximation is one registry value away (profiles/APPROX\_GL.REG sets all three).

| Key | Default | Exact behaviour | Approximation (old default) |
| --- | --- | --- | --- |
| ACCEL\_OPENGL\_CLAMP | 0 | GL\_CLAMP filtered linearly takes in the border colour: on the engine through a ring copy while the coordinates stay in 0..1 (20.9), otherwise software | 1: the engine clamps to the edge texel |
| ACCEL\_OPENGL\_LONGMIP | 0 | A chain longer than nine levels from its base (512 and up, full) goes to software; shorter chains stay on the engine | 1: such chains on the engine, their smallest levels never sampled |
| ACCEL\_HWCURSOR\_INV | 0 | Shapes with inverting or colour-XOR pixels drawn by the software cursor | 8: the mono unit, two colours |

### 20.9 GL\_CLAMP filtered linearly on the engine (v10.357)

GL clamps s to 0..1 and the linear filter then takes in the border colour at the edges: half of it at s = 0 and s = 1, and the same half for any s beyond. Make the copy with a one-texel ring of the border colour around the texels (the copy linear CLAMP\_TO\_BORDER already uses), clamp to the copy's edge, and map s' = (s x sw + 1) / w, w the copy's width. For every s in 0..1 the engine then blends the ring and the edge texel by GL's weights. Beyond 0..1 the ring goes on to all border colour where GL stays at half, so:

- Each draw's vertex coordinates on a ringed side are checked (vxgl\_tex\_vertices\_ok): all in 0..1, no texgen on that side, an identity texture matrix, not projective. Interpolation and Mesa's clipping keep the pixels inside 0..1. Otherwise the draw goes to software (fallback bit 0800).
- Not for mipmapped textures (a ring copy has no chain), rectangles, cube maps or volumes, nor with a colour table: those go to software under the exact default.

### 20.10 Profiles, checks, what remains

**Profiles** (profiles/\*.REG; restart after importing): NORMAL (the defaults), APPROX\_GL (the three approximations), EXACT\_GL (the exact three, now the defaults), LEGACY\_CLEAR (ACCEL\_3D\_CLEAR3D 0), STRICT\_D3D, EXACT\_DESTKEY, EXPERIMENTAL\_DDRAW\_3D (native stretch), LOGGING / LOGGING\_OFF.

**Feature bits** (the VxD's feature word handed to the HAL): 19 CLEAR3D (on), 20 STRICT, 21 EXACTKEY, 22 DDRAW\_3DBLT, 23 GUARD (v10.358, 21.3) (off; the word starts with them off before the registry is read, v10.357). Default mask FF0FFFFF.

**Board checks.** VXFEAT.EXE (tools/vxfeat.c, built by tools/build\_features.py): depth bounds (stored against incoming depth, attribute restore, mixed spans, points and lines, texture and pixel-copy rejection, query-only triangles), the long mip chain, query and clear exclusion, repeated framebuffer-object clears, anisotropy readback, swap-control timing, and the four stretch / mirror comparisons. VXGL v4.6 adds a GL\_CLAMP scene (in and out of 0..1) and a pbuffer check.

**Keep the four driver files together.** The packet stays 10,324 bytes and VX3DN\_ABI 0x00010000, but the producer and the VxD must both know the anisotropy bits and the clear mark.

**Remaining.** Native stretch and mirror need their own pixel comparisons before they go on. Native mip levels 9-11 need a sampler recipe from the board (block 10030000 holds eight level offsets). Destination-alpha visuals and wider framebuffer-object formats need a board probe of the engine writing and blending the alpha byte of an A8R8G8B8 target. Overlay auto-flip, mirror, a second overlay and the HQV key combinations wait for the register recipes CLXPOVL records on XP (v10.340). Not offered by the hardware: multisampling, stereo, colour-XOR cursors on the hardware unit, public DirectDraw alpha. Decided against: hardware T&L (slower than the CPU), DPMS (not supported for now), Coarse Z (slower than none).

## 21. Gaming: speed switches and the tools that measure them (v10.358)

The exact defaults of section 20 cost speed in some games. Each trade is one switch; VXGAME measures every one of them on the board in one run, and VXD3DMON finds the geometry a Direct3D game loses.

### 21.1 Per-process settings

VXGL.DLL reads every registry value it uses from an environment variable of the same name first (decimal, or hexadecimal after 0x), for that process only. A test program sets the variables and starts a child: no registry change, no restart, nothing left behind if the run stops (as VXGL\_HW, 18).

### 21.2 The switches

| Key | Default | Effect |
| --- | --- | --- |
| ACCEL\_OPENGL\_SWAPINTERVAL | 1 | A window's swap interval until the application sets one (20.6). 0 runs uncapped; 1 caps the frame rate at the refresh |
| ACCEL\_OPENGL\_FASTPOINTS | 0 | 1: a smooth point is one square of its size with its own alpha and no edge coverage (as the cards of the time drew GL\_POINT\_SMOOTH): one quad instead of a rectangle per row plus one per coverage step. Particle-heavy frames take less CPU; the pixels are not swrast's |
| ACCEL\_OPENGL\_SNAP | 1 | 0: triangles not snapped to swrast's 1/16-pixel grid (19.4): less CPU, edges a pixel apart from software here and there |
| ACCEL\_OPENGL\_CLAMP, \_LONGMIP | 0 | 1: the approximations of 20.8 |
| ACCEL\_3D\_GUARD | 0 | 1: vertices at -0.5 on the window's left and top edges taken by the engine (21.3) |

profiles/GAMING.REG sets swap interval 0, SNAP 0, FASTPOINTS 1, CLAMP 1, LONGMIP 1 and HWCURSOR\_INV 8; NORMAL.REG returns to the defaults.

### 21.3 The window's edges (ACCEL\_3D\_GUARD)

Mesa clips to GL's window, whose x = 0 and y = 0 are the engine's -0.5, so every polygon cut at the left or top edge arrives with vertices at -0.5. The VxD's check and the core's fast paths take only coordinates of 0 and above, so these polygons went through the core's general path (copied, clipped again at 0): 6-9% of the drawing thread in Quake 2.

- With the switch on, a coordinate from -0.5 to -0.0 (float bits 0x80000000 to 0xBF000000) where the clip edge is 0 is accepted by the VxD's check (vx3dn\_validate) and by the core's triangle and strip fast paths. The engine's clip rectangle still bounds the pixels: the pixel centres are the engine's integers, and -1 is outside it.
- Registry ACCEL\_3D\_GUARD (VxD), or its control word: 0xFFFFFE31 on, 0xFFFFFE30 off, 0xFFFFFE32 reads (out\[1\] = 'GRD1'). It is bit 23 of the VxD's feature word; the HAL takes the word at DriverInit and VXGL.DLL asks the VxD when a process loads it, so the producer and the check always agree (a packet the VxD refused would be lost).
- Off by default until the board shows the engine draws such vertices right (VXGAME's guard row).

### 21.4 VXGAME.EXE

Five game-like scenes at 320x240, each read back once and then animated and timed (150 frames by default; "VXGAME n"):

- **world**: a Quake 2 corridor: a wall texture with a GL\_CLAMP lightmap on unit 1 (coordinates in 0..1), a GL\_CLAMP sky, glBegin / glEnd, depth cleared every frame, polygons cut at the window's edges.
- **particles**: the world plus 1500 smooth points with the alpha test and blending.
- **q3arrays**: Quake 3 style: vertex arrays with glLockArraysEXT and glDrawElements, a lightmap pass blended GL\_DST\_COLOR / GL\_ZERO with depth EQUAL, a 24-bit depth and 8-bit stencil format when offered, and the gamma ramp set and put back (the log says whether both worked).
- **hud**: 600 alpha-tested font quads from four textures, a bind every five.
- **bigtex**: 1024 and 512 textures with full mip chains, minified (ACCEL\_OPENGL\_LONGMIP).

Each of eight configurations runs in its own child process with its settings in the environment: exact (the defaults), snap0, fastpts, approx, gaming, vsync, software (the reference) and guard (the VxD's switch turned on for that child and off after; run no other OpenGL program meanwhile). All but vsync run with swap interval 0. VXGAME.TXT holds frames a second and pixels off the software frame (a channel more than 16 apart) for every scene and configuration, BLANK where a frame holds nothing but the clear colour (Quake 3's symptom), and each child's renderer, pixel format and gamma result.

### 21.5 VXD3DMON.EXE

Start it, then the Direct3D game. VXDDRAW.DLL's data is shared across processes, so its counters are the game's. Every primitive the HAL leaves out (state the engine cannot draw: missing geometry) is logged with the time, the reason bits of vx\_d3d\_unsupported\_why (surfaces, fill or shade mode, Z, cull, alpha test, blend pair, stencil, colour mask, viewport, stage state, texture not samplable, points/lines, cube map) and the stage state at the last one; also lost contexts, engine recoveries, and once a second the DrawPrimitives2 calls and engine packets. A key, or "VXD3DMON n" seconds, stops it; VXD3DMON.TXT holds the record. Read-only.

### 21.6 SSE vertex transform (v10.359)

Mesa's software T&L spends most of its CPU time multiplying every vertex by the model-view-projection matrix, testing it against the six clip planes and dividing by w. VXGL.DLL (vxgl\_sse.c) replaces those entries of Mesa's tables with SSE code; everything else (lighting, texture coordinates, fog, emit) stays Mesa's C code.

| Mesa table entry | What it does | Used for |
| --- | --- | --- |
| `_mesa_transform_tab[2/3/4][MATRIX_GENERAL]` | points by a general 4x4 matrix | every vertex: object to clip coordinates |
| `_mesa_transform_tab[3][MATRIX_3D]` | x, y, z by a 3D matrix (w left as it was) | eye coordinates when lighting or fog needs them |
| `_mesa_clip_tab[4]` | clip mask, then 1/w and x/w, y/w, z/w for the vertices inside | the projected vertices |
| `_mesa_clip_np_tab[4]` | clip mask only | when the driver needs no projected copy |

Rules the kernels keep:

- Mesa's arithmetic order, ((m0·x + m4·y) + m8·z) + m12 (m12·w for 4-component points), each step rounded to single precision. The x87 C code rounds once at the store, so a coordinate can differ from it in the last bit or two of the float; the clip masks never differ.
- The clip mask comes from w < c and w < −c, which set the same bits as Mesa's −c + w < 0 and c + w < 0 for every value, infinities and NaN included. Clipped vertices get (0, 0, 0, 1); the or-mask and and-mask follow Mesa's rule (the and-mask is 0 unless every vertex is clipped).
- 1/w is a true divide (divss), never the approximate reciprocal.
- The thread's MXCSR is saved, set to 0x1F80 (round to nearest, all exceptions masked, no flush-to-zero, no denormals-are-zero) and restored around each call, so a program's own SSE settings neither change the results nor are changed.
- SSE1 instructions only. The kernels go into the tables once per process, after Mesa's one-time set-up in context creation.

When it is on: CPUID function 1 must report SSE (EDX bit 25) and FXSR (bit 24), and CR4.OSFXSR (bit 9) must be set, meaning the system saves the XMM registers at a task switch. CR4 can only be read at ring 0, so the VxD answers it: VX2D control IOCTL 0x85500012 with the word 0xFFFFFE34 returns CR4, the tag 'SSE1' (0x31455353), CPUID 1 EDX and ECX. VXGL's log line at load names the state and both words.

| Key | Default | Effect |
| --- | --- | --- |
| `ACCEL_OPENGL_SSE` | 1 | 0 = Mesa's C transform and clip test |

profiles/NOSSE.REG turns it off; NORMAL.REG and GAMING.REG set it to 1. VXGAME v1.1 has a nosse row (ACCEL\_OPENGL\_SSE = 0): its frames a second against the exact row show what the kernels gain per scene. The software reference row also runs without them, and VXGAME.TXT's header says whether the SSE transform is on for this machine. The gain is limited to the transform share of a frame: rasterizer feeding, emit and the engine's own time are unchanged.

### 21.7 Not built yet

- **Page flipping for fullscreen OpenGL.** Swaps are blits. A flip needs a flip chain on the primary, which DirectDraw gives only to the owner of an exclusive-mode window (the game, not the ICD), or the display start moved under GDI; both need a design and board work of their own.
- **Destination alpha** for games that blend with it: a board probe of the engine writing and blending an A8R8G8B8 target's alpha byte first.

## 22. Quake 2 work: v10.360 to v10.366

Quake 2's 800x600 timedemo went from 15 fps (v10.359) to 28 fps (v10.366), with no triangle defects. The changes are below in the order they were made; each says what was wrong, the fix and how it was proven. Board figures are from the Wyse Cx0 at 1000 MHz, 800x600x32.

### 22.1 One-frame streaks: clipped polygons and strip joining

**Symptom.** A band of wrong triangles for one frame when a wall crossed the screen edge, with SSE on or off.

**Cause.** The core joins triangles into strips by vertex index: a new triangle whose first two indices equal the last two of the run adds one vertex. Mesa's clipper writes each clipped polygon's new vertices into the same scratch slots past the vertex buffer's end, so a later clipped polygon reuses indices that the open run still holds, and the core joined it to stale geometry.

**Fix.** Forget the open strip run (and the list being joined) before every clipped primitive: `vx_d3d_gl_unjoin()` from Mesa's `clipped_line`, `fast_clipped_poly`, `render_clipped_poly` and `render_clipped_line`. The host test has a clip check: perspective views of a box crossing every edge, each clipped polygon alone and together, against swrast.

### 22.2 Vsync and the swap interval

**Halved frame rate.** The vsync wait waited for the end of the blank and then the start of the next, so every swap waited for the second blank after the frame (VXGAME 29 fps at 60 Hz in every scene; Quake 2 15). Fix: one wait for the start of a blank (DirectDraw `WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN)`) per interval, after the engine has finished.

**Default interval 0.** WinTune 98's OpenGL fill test measured 17 MPixels/s with interval 1 and 106 with 0: a benchmark that swaps often waits a blank each time. A window's interval is `ACCEL_OPENGL_SWAPINTERVAL` (default 0) until the application sets one with `wglSwapIntervalEXT`; Quake 2's `gl_swapinterval` works. Single-buffered and front-buffer presents never wait.

### 22.3 Packets, batches and fetch groups

- A packet holds at most 192 vertices and 64 bursts (strip or list runs; 10452 bytes). With 32 bursts Quake 2's packets were cut by the run limit, not the vertex limit; 64 cut 26% of packets and gave the same frame rate.
- The ICD queues up to 32 packets per VxD call (`VX_IOCTL_3DN_SUBMITN`). The VxD builds them into fetch groups in a 256 KB write-combining command block; one group runs while the next is built.
- `ACCEL_D3D_FETCH_KB` (default 32, 16 to 80) is the most a group holds. The cap is a third of the block: a group that would pass the block's end starts again at its beginning, so the group still running must have begun at least two group sizes earlier. 48, 64 and 80 KB all gave 28 fps.

### 22.4 Clears

OpenGL's colour clear goes to the 2D engine's fill; its depth clear is a depth-only 3D quad. Two defaults changed:

- `ACCEL_3D_CLEAR3D` (feature bit 19) default 0 from v10.362: Direct3D's and the core's clears are 2D fills again. With the 3D clear rectangle as default, WinTune's OpenGL test fell from 186 (v10.340) to 17 (v10.361) together with the vsync wait of 22.2.
- `ACCEL_OPENGL_ZCLEAR3D`: 1 (default) = the depth (and stencil) clear as a 3D quad with depth test ALWAYS, colour writes off, the colour by the 2D fill; 2 = the colour in the same quad too; 0 = the 2D fill for depth as well.

Board, v10.366: WinTune OpenGL 127, Direct3D 278.

### 22.5 Open: Quake 2 freezes with a 2D-engine depth clear

**Symptom.** With `ZCLEAR3D` 0 and Quake 2's `gl_ztrick 0` (a depth clear every frame), only the first frame's world lands; later frames draw the HUD over a frozen world. GLREPLAY, which waits for the engine and locks the surfaces after every frame, draws the same stream correctly. The depth-only 3D quad (22.4) avoids it and is the default.

**What was tried** (all on the board, none cured it; tagged at the first depth-tested draw after a 2D depth fill):

| Version | Setting | What it does | Result |
| --- | --- | --- | --- |
| v10.363 | ACCEL\_3D\_ZFIX 1 | 01/29 = 008000 (Coarse Z tag reset) | still frozen |
| v10.363 | ACCEL\_3D\_ZFIX 2 | 01/11 with bit 11 set, then the Z pitch again (reading-Z cache clear) | still frozen |
| v10.363 | ACCEL\_3D\_ZFIX 4 | 01/10 = 0, then the depth base again | still frozen |
| v10.363 | ACCEL\_3D\_ZFIX 7 | all three | still frozen |
| v10.344 | ACCEL\_OPENGL\_ZLOCK 1 / 8 | wait for the engine after / before the clear | still frozen |
| v10.344 | ACCEL\_OPENGL\_ZLOCK 2 / 16 | DirectDraw Lock and Unlock of the depth surface after / before | still frozen |

Ruled out: waiting, CPU locking, the Z tag and the two depth caches the HCM names. Untried: a 2D fill of the depth surface followed by one 3D draw with depth writes on and the colour off before the frame, and comparing the depth words the 2D fill writes with the 3D quad's (D24S8 low byte, Coarse Z records).

### 22.6 Display FIFO and the stripes under heavy fills

Horizontal stripes appeared during WinTune's OpenGL fill test after a registry profile was imported without restarting; after a restart they did not come back with either FIFO setting, and the test ran at full speed. The BIOS leaves SR16 20, SR17 7F, SR18 60, SR22 10. OpenChrome's VX855 values (SR17 C7 = 400 entries, SR16 and SR18 thresholds 0x90, SR22 expire 0x28) were written live by VXFIFO.EXE (`VXFIFO on` writes 90 C7 D0 28, SR18 keeping its top bits; `off` puts back the first values). No difference was seen, so they are not applied at mode set.

### 22.7 Where Quake 2's time goes

About half is the engine drawing pixels and half the CPU. Board timedemo (`timedemo 1`, demo1): 28 fps at 800x600x32, 34 at 640x480, 31 on a 16-bit desktop at 800x600. Bigger fetch groups (22.3) and 16-bit textures (22.8) changed nothing.

The CPU side, from the ICD's own sampler (`VXGL_SAMPLE` 1, about 1 kHz; the log lists module shares and the busiest VXGL.DLL offsets, resolved against VXGL.map and the objects' disassembly):

| Part | Share of samples |
| --- | --- |
| The VxD building and starting packets (KERNEL32 in the list: the IOCTL) | 19-24% |
| The core's strip path: strip\_fast, fast\_vertex, snapw, fast\_v | about 17% |
| Mesa's immediate-mode calls (vbo) | about 8% |
| Mesa's vertex emit (insert\_4ub\_4f\_bgra\_4, viewport) | about 7% |
| SSE clip test | about 4% |

Mesa's emit already runs its hard-wired path for this layout (position 4f viewport, colour 4ub BGRA, 2f texture), and its float-to-byte conversion is the IEEE form, so there is little left there. `fast_vertex` (one vertex into the packet) now reads the template's fields into locals once: Watcom reloads a global after every store through a pointer, and every store here is into the packet.

### 22.8 16-bit textures (ACCEL\_OPENGL\_TEX16)

A texture whose base format is RGB (no alpha) is copied to video memory as R5G6B5, the sampler's format 6 (proven through Direct3D). Default 1; 0 = X8R8G8B8 as before.

- Rounding to the nearest: r5 = (v × 249 + 1014) >> 11 and g6 = (v × 253 + 505) >> 10, exact for every v in 0-255 (checked against round(v × 31 / 255) and round(v × 63 / 255)).
- Surface: a DirectDraw texture with `DDPF_RGB`, 16 bits, masks F800 / 07E0 / 001F. The HAL gives it the sampler's pitch: max(32, width × 2) bytes; a block copy's rows are rounded up to 32 bytes.
- Not used for cube maps, 3D textures, padded or converted copies (borders, colour tables, depth textures) or a framebuffer object's texture, which must be X8R8G8B8, the engine's target format.
- Result: 28 fps with or without it. Texture bandwidth is not what limits Quake 2. VXTEX turns it off for its own run (`VXGL_TEX16=0`) because its answers are 8 bits a channel.

### 22.9 Texture uploads without waiting: the copy ring (ACCEL\_OPENGL\_TEXRING)

**The wait.** Every DirectDraw `Lock` of a video surface waits for the engine to finish everything (the HAL's Lock calls the VxD's access wait first), and an upload into a copy the queued packets still read must send the queue first. Quake 2 rewrites its dynamic lightmaps several times a frame: each upload cost about 4.4 ms.

**The ring.** A texture rewritten with the same shape gets up to 3 more copies (default 3, 0 = off), 2D only, at most 256 KB a copy, not DXT, not a framebuffer object's texture. Each upload:

1. Sends the core's open packet into the queue (its offsets are the old copy's).
2. Takes a spare copy that no queued or running packet reads, or allocates one; the old copy becomes a spare, marked with the packet count at this moment.
3. If none is free: sends the queue (no idle wait) and tries the oldest again; still none: the old send-and-lock path.
4. Writes the texels without Lock, at the video memory's linear address (`VX_DD_Vram`, the base the HAL's own CPU writes use) plus the level's offset, rows of the sampler's pitch.

**The fence.** Three counters: packets handed over, packets given to the VxD, packets finished. A VxD call that takes a packet first retires the fetch group still running (one group at most in flight), so every packet given before that call is finished; an engine wait or a Lock finishes everything. A spare is free once finished has reached its mark.

**Proof.** Host test: Quake 2's pattern (72 sub-image uploads with draws in between) against swrast, with a model that keeps the last call's packets running; removing the fence on purpose gave 66 writes into copies still read and 50160 wrong pixels. Board, Quake 2: 473 uploads into a free copy, 4 after sending the queue, 3 into a new copy, 1 waited; 28 fps steady with it, 25-28 without.

**Unproven.** Whether the sampler's texture cache can hold a reused copy's old texels: the copies rotate, and no artefact has been seen.

### 22.10 Single-buffered windows: the 3D Pipes dissolve

**Symptom.** After Quake 2 had run, the 3D Pipes screensaver (preview and full screen) opened by dissolving from Quake's menu, with Quake no longer running.

**How Pipes starts.** Pixel format 4 (single-buffered). It clears depth over the whole window, then clears small tiles (3x3 in the preview, 18x18 full screen) in a scattered order with a `glFlush` after each.

**Cause.** A single-buffered window draws into an offscreen surface (the "back" surface), shown at each `glFlush`. A new surface held whatever video memory last had there (Quake's frames), and every flush blitted the whole surface over the window, so the tiles not yet cleared showed the old pixels.

**Fix (VXGL 3.73).**

1. A new surface starts as the screen's pixels in the window (a blit from the primary) when it is single-buffered; when that copy cannot be made (another desktop depth, the window not on the primary, a failed blit), or for a double-buffered back buffer, it starts black (a colour fill). A front buffer whose copy from the window fails starts black too, instead of being marked loaded with old memory.
2. A single-buffered surface and a front buffer show only what was drawn into them since they were last shown: the union of the clip rectangles of the packets that write colour (a draw with any colour channel enabled, or a colour clear), plus the whole surface after any software-rasterizer lock. Pipes' depth-only quad writes no colour and shows nothing. A swap still shows the whole back buffer.
3. With nothing new, a flush shows nothing.

**Check.** Pipes now fades from black, and it does the same on Microsoft's software OpenGL (`ACCEL_OPENGL` = 0, VXGL declines the pixel format), so black is Pipes' own start. GDI drawing into a single-buffered window now stays until OpenGL draws over it, as when an ICD draws into the window itself.

### 22.11 Registry keys and tools (v10.360 to v10.366)

All under HKLM\\Software\\VX85598, DWORD, read when a process loads VXGL.DLL (the VxD's at boot).

| Key | Default | Meaning |
| --- | --- | --- |
| ACCEL\_OPENGL\_SWAPINTERVAL | 0 | A window's swap interval until the application sets one |
| ACCEL\_3D\_CLEAR3D | 0 | 1 = Direct3D's and the core's clears as a 3D rectangle |
| ACCEL\_OPENGL\_ZCLEAR3D | 1 | 0 = 2D depth fill, 1 = depth-only 3D quad, 2 = colour in the quad too |
| ACCEL\_3D\_ZFIX | 0 | Experiment, VxD: bits 1, 2, 4 of 22.5 |
| ACCEL\_OPENGL\_ZLOCK | 0 | Experiment: bits 1, 2, 4, 8, 16 (waits and locks around a depth clear) |
| ACCEL\_D3D\_FETCH\_KB | 32 | VxD: the most a fetch group holds, 16-80 KB |
| ACCEL\_OPENGL\_TEX16 | 1 | RGB textures as R5G6B5 (environment VXGL\_TEX16 overrides it for one process) |
| ACCEL\_OPENGL\_TEXRING | 3 | Spare copies for rewritten textures, 0-3 |
| ACCEL\_OPENGL\_SSE | 1 | SSE transform and clip (21.6) |
| VXGL\_SAMPLE | 0 | 1 = the CPU sampler's report in VXGL.LOG (needs VXGL\_LOG 1) |

VXGL.LOG at process exit also gives: frame timings in parts (the swap split into settling and the blit), packets per call, why each packet ended (vertex limit, state change, swap or flush, texture upload), the texture uploads' parts and the ring's counts. Tools: VXFIFO.EXE (22.6), VXGAME v1.2, VXTEX v1.8.

## 23. Direct3D work for GTA3, the command fetcher's ring and frame timing: v10.367 to v10.386

GTA3 (Direct3D 8 on the DirectX 7 HAL) was the test game: 2-7 fps at v10.366, faster since, with hitches still open (23.6). Its draws cost about 85 microseconds each in the VxD; the command fetcher's ring removes the wait for the draw before (68 µs against 94 µs a packet) but still hangs once at the menu, so the driver turns the ring off after its first hang. Board figures are from the Wyse Cx0 at 1000 MHz.

### 23.1 Direct3D fixes (v10.367-v10.372)

- **A bad vertex leaves only its primitives out.** A vertex that is not finite or has rhw <= 0 drops the primitives that use it (drop reason 2000); the call succeeds and draws the rest. It used to fail the whole DrawPrimitives2 call with DDERR\_INVALIDPARAMS (GTA3's LINELIST and TRIANGLEFAN), so the rest of the batch was lost. An index past the vertices is still a malformed call, and the engine never gets such a vertex.
- **CopyRects into a texture.** A Blt from a render target or back buffer (video memory, not a texture) into a texture of the same format is the 2D engine's copy into the texture's offset and pitch, the CPU finishing what the engine declines (same size, SRCCOPY, no key or mirror). GTA3 did hundreds a minute and got E\_NOTIMPL. Mismatched formats are logged once (VXD3D.LOG cb=31) and stay unsupported.
- **Draws on the primary are batched** (`ACCEL_D3D_PRIMBATCH`, default 1). In a flip chain Flip swaps the surfaces' memory, so every other frame is drawn at offset 0; each of its packets used to go alone. The VxD excludes the cursor from the first such packet to the end of the batch and starts the last group synchronously.
- **Busy is back-pressure, not a loss.** When the VxD took part of a queue and answers busy, the rest is sent again. An error after every packet was taken no longer replays the last one (it would draw twice).
- **Render states nothing reads** (WRAP0-7, CLIPPING, LIGHTING and the other vertex-processing states the runtime forwards) no longer end the batch or rebuild the draw template. Phong shading draws as Gouraud, a W-buffer as a Z buffer, and ZBIAS runs in the fast vertex path.
- **CLEAR as XP's S3gIGP.dll parses it** (v10.372): a clear with no rectangles is 16 bytes (header 20 + 16 per rectangle); depth and stencil bits are dropped when there is no depth surface; a rectangle past the surface is clamped; a finite depth outside 0-1 is clamped. Each of these used to fail the call.
- **The cursor bracket only when needed** (v10.371). A 3D draw takes the frame-buffer access bracket (cursor off, draw finished before it comes back) only when the software cursor is or may be drawn on the shown surface and the draw overlaps it (`VESA_draw_needs_exclusion`). It was every draw at offset 0, which made every other frame synchronous.
- **DirectDraw blits:** a blit the engine declines part-way waits for the engine before the CPU finishes it; an overlapping clipped copy or stretch goes through a kept staging copy (up to 1 MB) instead of NOTHANDLED; out of memory for staging answers DDERR\_OUTOFMEMORY with nothing drawn.

### 23.2 The command fetcher's ring: registers and proven behaviour

The fetcher reads command words from video memory. All its registers are written through 0x420 after 0x41C = 00100000, the register number in the top byte and 24 bits of value below. Addresses are offsets in video memory.

| Register | Meaning |
| --- | --- |
| 60 / 61 | Ring start, low 24 bits / high byte. Bit 0 of 60 is XP's GART mode on 5122: leave it clear in video memory |
| 62 / 63 | Ring end (the block's limit) |
| 64 / 65 | Pause: the fetcher runs up to this address and waits. 65 bit 9 (0x200) = one-shot stop at the pause |
| 66 / 67 | Wrap point. XP's start writes 66 = FFFFF0, 67 = 0000FF (none); its wrap kick sets it to the old tail - 16 |
| 68 | 0A0001 = start at register 60; 0A0000 = stop |

**One-shot form** (the ring-off path): the group's words, XP's batch end (`FE020420 0000041C 00300000 CC000000`), padding pairs `CCCCCCC0 DDD00000` to 32 bytes, pause = end - 4 with 65 bit 9, 60-65, 68 = 0A0001, then wait for idle and stop.

**Ring form** (XP s3ginv.dll): a start block of the batch end and `CCC00002 DDD00002` to 256 bytes, with 60-67 and 68 = 0A0001 written once. Each group is then **kicked**: its words, the batch end, 16 bytes of `CCC00004` (so the pause never falls inside the batch end), `CCC00004` to the next 256 bytes, then 0x41C, 64, 65 with pause = end - 0x10. **68 is not written again.**

Proven by VXRING (a test tool that builds the words in a DirectDraw surface and starts the fetcher itself):

- A group kicked while the fetcher still runs the one before is drawn, 2D and 3D, whole.
- Writing 68 = 0A0001 again on a kick restarts from the ring start and redraws every group kicked before (register 440 rose 360 for five kicks of 120 vertices). This broke GTA3 and Quake 2 in v10.374; XP's kick fixed it.
- The status (0x400) reads busy about 0.5 µs after a kick and idle (07000000, 0x400 & 0x101FF3 = 0) once the fetcher has caught up at its pause.
- No words past the pause are run during a run: a trap written past the pause before the kick stayed untouched.
- 32 groups kicked back to back, finished by the idle rule (8 idle reads, then the stop at once): none lost.
- Register 440 counts vertices drawn.
- Not usable in video-memory mode: 0x43C (XP's read pointer) and 0x494 (XP's fence) stay 0; a type-0x11 fence in the stream (`FE020420 0000041C 00110000`, then 06 / 07) never reaches 0x494, though a direct register write does; 0x400 bit 17 and 0x41C bit 31 never move; 0x418 is the constant 00008020.
- A 2D group of 200 back-to-back fills loses its last fill, one-shot too: a 2D quirk, not the ring.

### 23.3 Stop and restart: the read pointer stays where its last words ended

After a stop (68 = 0A0000) the fetcher's read pointer stays where its last words ended. Writing 64/65 for a new start kicks it from there, and it runs the words past the old position until the 68 = 0A0001 write moves it to the new start, about the time of the register writes in between. Old words therefore run at every start that is not at the old position.

VXRING 41-48 (v10.381): the buffer filled with no-op blocks; run 1 = 8 groups drawn; a trap of 32 fills, one per 256 bytes, past run 1's last pause; the stop; run 2 started and one group kicked.

| Test | Variant | Trap cells run |
| --- | --- | --- |
| 41 | Trap before the stop, restart at the buffer's middle | 5 |
| 42 | Trap written after the stop | 5 |
| 43 | Trap overwritten with no-op blocks before the stop | 0 |
| 44 | Restart at the buffer's start | 4 |
| 45 | Restart at the old position (the last pause + 0x10) | 0 |
| 46 | As 41, the new group a 3D triangle | 6 |
| 47 | Stop, no restart | 0 |
| 48 | Restart in the one-shot form at the middle | 2 |

What this means: the pointer is read again from memory (42), not a queue filled before the stop; no-op words there are harmless (43); a restart at the old position runs only its own start block (45); the stop alone runs nothing (47). The one-shot form has the same exposure (48): with the ring off, the up to 224 bytes between a group's 32-byte end and the next group's 256-byte start were run on every group.

No-op words that are safe to run: the ring's kick padding blocks (`FE020420 0000041C 00300000 CC000000`, then `CCC00004` to 256 bytes) after a ring group, and the pairs `CCCCCCC0 DDD00000` after a one-shot group.

**Not yet run on the board** (VXRING v2.2, `VXRING wrap`): 49 = 41 with the start written 60-63, 66, 67, 68 and only then 64/65 (does the stray run go away?); 50-52, XP's wrap without a stop: group A two blocks before the buffer's end, a trap past it, group B at the start, 66/67 = A's pause, then 64/65 = start - 16 and a kick to B (50), 64/65 = B's pause at once (51), or 50 without the kick (52); then a group D kicked as usual to see whether the ring goes on.

### 23.4 The driver's ring (ACCEL\_D3D\_RING) and its open hang

With the ring on (default 1) a fetch group no longer waits for the one before. The first group writes the start block and starts the ring (60-67, 68 = 0A0001); every group is kicked as in 23.2. The wait comes only when something needs the engine idle: CPU access to a surface, the 2D engine, clears, the register path, a status query that finds it idle, a synchronous group (the cursor on the shown surface) and the block's wrap. That wait (`fx_retire`) waits for idle and the fence, then stops the fetcher; the next group starts the ring again.

**The fence** (v10.383, `ACCEL_D3D_RINGFENCE` 1). Every ring group ends with a 1x1 2D solid fill into the command block's last 256 bytes, kept out of the groups, in the colour of the group's 16-bit sequence number. The CPU reads it back uncached: the last group the fetcher consumed. The wait needs the engine idle and the fence at the last group kicked (up to 100 ms); an idle reading before the fence arrives is counted and waited out. The 13 2D registers the fill writes (04 08 14 1C 30 48 58 5C 20 4C 0C 10 40) are read at the ring's start and written back after each stop.

**The gap overwrite** (v10.382, `ACCEL_D3D_GAPWIPE` 1, ring and one-shot alike). The VxD keeps where the fetcher's last words end (a kick's pause + 0x10, a one-shot group's pause + 4). Before each start, the words from there to the new start (at a wrap, towards the block's end; at most 8 KB) are overwritten with the no-op words of 23.3. The start's register writes, 0x41C to 68, run with interrupts off. After an engine reset or a VXRING run the position is not known and nothing is overwritten (counted).

**Ring off after its first hang** (`ACCEL_D3D_RING_HANGOFF` 1). A ring group that never goes idle is reset, and the ring stays off until restart; the one-shot path carries on. Before this, one hang was followed by many: 85 in 98 s in v10.382, each reset dropping that group's draws, so the game kept flipping between two stale frames (4, 5, 4, 5...) until it was closed.

**The hang record.** The last 64 ring groups kicked are kept: sequence, first after a start, synchronous, packets, words, vertices, start, pause, time, register 440, and the first packet's summary, target and depth. At the first hang the status, 440, the fence, the time since the last kick, how that ring started, the groups and all the words (up to 512 each) of the group after the fence and of the fence's group are kept (TEXDBG 0xBC, printed by VXD3DMON).

**Board results.**

| Version | Ring | Result in GTA3 |
| --- | --- | --- |
| v10.374 | 68 written on every kick | Loading screen in pieces, Quake 2 and the desktop broken: every kick redrew the groups before it |
| v10.378 | XP's kick | 179 engine recoveries, the menu flashing |
| v10.382 | XP's kick, gap overwrite | 68.2 µs a packet (94.4 ring off), no wait for the draw before; 85 hangs in 98 s, 86 starts at an unknown position |
| v10.384-v10.385 | Fence, ring off after a hang | One hang, then the ring off; no stale-frame loop |

**Open: the first hang is repeatable.** Twice (v10.384 and v10.385) it was the 7th ring group since boot, at the menu, with the same words: a 1-packet group of 256 words and 6 vertices carrying a full state set-up (the type 0xFE general block, the type 1 block with the pixel program disabled while it is replaced), kicked about 1 ms after group 6 while group 6 still ran. It is the second group of its ring, which started at the old position with no wrap, so the stop/restart of 23.3 is not involved. The fence read 6: group 6 was consumed, group 7 never finished, status 01000021 / 01000031. Groups 3 and 5, also second groups with full set-ups, drew. The one-shot path never starts a group before the one before is idle, which is the difference. v10.386 records the first packet's summary and both groups' full words to find what in group 7 needs the engine idle.

### 23.5 Flips without the blank wait (FLIP\_ASYNC)

A flip must wait for the engine to finish the frame. It also used to wait, after writing the start address, for the next blank to begin; with `FLIP_ASYNC` (default 1, v10.384) it returns once the address is written. The surface it moved away from (the old front) is still scanned until the next blank, so:

- A 3D draw or clear into it (VX3DN\_submit, VX3DN\_submit\_batch), a DirectDraw Blt into it or a write Lock of it waits until the display has taken the flip (`VX_flip_front_wait`; the HAL asks through flip command 6, VXF\_FRONT, only while a flip may be pending).
- The next flip waits for it before writing the address again (a new address would replace the old one unseen).
- GetFlipStatus(DDGFS\_ISFLIPDONE) answers DDERR\_WASSTILLDRAWING until then.
- Taken by the display = a blank seen since the address was written (it is only written outside a blank), the scan-line counter (0x1290, bits 31-16) below the line read then, or a frame plus 2 ms gone by. A NOVSYNC flip drops the pending one; flip intervals above 1 keep the old wait.

**GTA3 does not wait for the blank.** Of 8708 flips only 2 took this path: the game's flips are NOVSYNC or not synced. In each flip the engine wait averaged 0.09 ms (longest 3.6) and the blank wait 0.01 ms. The flip is not where its hitches are.

### 23.6 Where GTA3's time goes

The hitches are spent in our DrawPrimitives2. Each draw packet costs 80-95 µs in the VxD (about 1000 command words):

| Part of a packet (ring off) | µs |
| --- | --- |
| Building its command words (emitter: state set-up 7.5, texture set-up 4.1, program 2.7, primitive set-up and vertices 13.6) | 29.3 |
| Waiting for the draw before | 34.9 |
| Copying in and validation | 10.5 |

The ring removes the wait for the draw before (23.4: 68.2 µs a packet against 94.4).

Frame parts by HAL callback (v10.385; time between flips):

| Frames | Count | ms each | DrawPrimitives2 | Blt | Flip | Outside the driver |
| --- | --- | --- | --- | --- | --- | --- |
| All | 4748 | 19.5 | 11.4 | 0.1 | 0.2 | 7.7 |
| 75 ms and more | 486 | 157.2 | 88.5 | 1.4 | 0.3 | 66.9 |

Lock, Unlock and surface create/destroy were under 0.1 ms a frame. The longest frames spent 961, 1565 and 684 ms in DrawPrimitives2; some long frames are mostly outside the driver (857 of 861 ms). **Open:** whether the long frames are the ones with the most draws; v10.386 counts DrawPrimitives2 calls a frame.

### 23.7 Registry keys, debug codes and tools (v10.367 to v10.386)

All under HKLM\\Software\\VX85598, DWORD, read by the VxD at boot (FLIP\_ASYNC too).

| Key | Default | Meaning |
| --- | --- | --- |
| ACCEL\_D3D\_PRIMBATCH | 1 | Draws on the primary's memory batched like offscreen ones (feature bit 24) |
| ACCEL\_D3D\_RING | 1 | The fetcher kept running as XP's ring; 0 = one group at a time (one-shot) |
| ACCEL\_D3D\_RINGFENCE | 1 | The ring's 2D fence per group, waited for before a stop |
| ACCEL\_D3D\_RING\_HANGOFF | 1 | The ring off until restart after its first hang |
| ACCEL\_D3D\_GAPWIPE | 1 | Overwrite the words between the last ones run and a new start with no-ops |
| ACCEL\_D3D\_BATCH\_END | 0 | 1 = the one-shot form even with the ring on |
| FLIP\_ASYNC | 1 | The flip returns without waiting for the blank |

TEXDBG codes (VX\_IOCTL\_3DN\_TEXDBG): B8 / B9 a batch's parts (B9 clears); BA / BB the ring's starts, kicks, wraps and the gap overwrite's counts (BB clears); BC with an index in the second dword, the ring's first-hang record (0-1 header, 2-65 groups, 0x80 fence counters, 0x81 the two groups' word counts, 0x100+ the groups' first packets, 0x200+ the words); BE / BF the flip timing (BF clears). CA / CB are the constant groups' switch: never use them for counters.

Tools: VXD3DMON v1.9 (watches the HAL while a game runs: primitives left out, contexts lost, the VxD's per-packet parts, the ring's counts and first hang, flip timing, frame parts by callback through VXDDRAW.DLL's `VX_DD_QueryFrames`); VXRING v2.2 (the fetcher probe: `fill`, `3d`, `append`, `kick`, `long`, `xp`, `stale`, `backlog`, `restart`, `wrap`, or one test number).

## 24. GTA3 speed work: XP's ring in system memory, the frame's waits, the per-triangle path (v10.387 to v10.478)

This section replaces the open hang of 23.4. The ring in video memory was never made reliable and is gone; the fetcher now runs XP's own ring in GART-mapped system memory, which has not hung on the board. After that the work moved, in order, from the engine waits a frame contains to the CPU cost of each packet and finally of each triangle. Every result below was measured on the board (VX855, C7 at 1000 MHz) in GTA3 unless it says otherwise. 24.5 lists everything that was tried and taken out, so none of it is tried again blind.

| Version | Change | Measured result |
| --- | --- | --- |
| v10.404 | XP's ring in system memory, XP's start form | 154,705 groups in 111 s, 0 hangs, every fence consumed |
| v10.405 | No program-change barrier in that ring | 69.5 µs a packet (90.5 with it), slow frames 157 ms (177) |
| v10.410 | No wait at a wrap (XP's space check) | 63.3 → 60.0 µs a packet, longest DrawPrimitives2 call 28 ms (400-500 before) |
| v10.411 | Groups staged in cached memory, copied with MOVNTI, one SFENCE | the cache flush (7.5 µs a packet) gone; 60.0 → 58.2 |
| v10.412-413 | Prefetch, one-pass block switch, four-word copy; VDIRECT dropped | 51.3 µs a packet |
| v10.416-419 | Depth clears, flips and clears in the ring, no retire at a ring flip | 2,902 ms of old-front waits a session gone; flips no longer wait for the blank |
| v10.432 | The HAL's DLL locked in memory | DrawPrimitives2 calls no longer stop for page-ins of the HAL's own code |
| v10.440 | (outside the driver) disk out of MS-DOS compatibility mode, Trails off | gameplay median 9.9 fps (8.8 in v10.437) |
| v10.441-442 | Prepacked vertices | DrawPrimitives2 6.83 → 6.31 µs a triangle; in-run A/B −0.22 µs a triangle |
| v10.443-449 | PAIRLIST, VRECIPE, FASTMATCH, FASTCLOCK, ONECOPY | in-run A/B: vertex writes −61 cycles a triangle, matching −12, the VxD −2.5 µs a packet; ONECOPY one copy of each vertex fewer |
| v10.450 | TEXONLY: a packet that differs from the last only in its textures sends the texture units only | in-run A/B: the VxD −9.2 µs a packet; 1,059 → 951 command words a packet |
| v10.451-452 | Every switch measured in one run; all seven kept on | the VxD 41.0 µs a packet with all on (52.9 in v10.449, 94.4 one-shot in v10.385) |
| v10.453 | RINGBLT: the back buffer's copy into a texture (Trails) written into the ring after the draws | Blt −2.5 ms a frame (61 turns, \[2.50, 2.53\]); CPU waits for the ring 8.7 s → 0.9 s a run; no Blt of 10 ms or more |
| v10.455 | FASTVV: the vertex checks' bounds once a call, one integer test a vertex | vertex checks 556 → about 300 cycles a triangle; in-run A/B −0.235 µs a triangle |

All of these are on by default (24.7), each with a registry key to turn it off; 24.4 has the run that measured every one of them at once. A vertex now passes the CPU three times instead of four: written once in XP's 10-word form by the HAL, copied once into the command block, copied once into the ring.

### 24.1 XP's ring in GART system memory (ACCEL\_D3D\_XPRING, v10.402-v10.411)

XP never runs this chip's ring in video memory: its s3ginv.dll sets register 60 bit 0 (GART mode) for the 5122, and its restart drain and space check need 43C (the read pointer) and 444 (bit 31 = the fetcher paused), which only work in system-memory mode (23.2 found them dead in video memory). The recipe below is XP's, rebuilt from s3ginv.dll and S3gIGPm.sys. Fetcher registers are written as in 23.2 (0x41C = 00100000, then 0x420 = register << 24 | 24 bits).

**Page table.** 64 KB in video memory (4 KB aligned) below the command block. Sequencer registers (unlock SR10 bit 0 first): SR6C bit 7 (the table's write protection) cleared while the entries are written; SR6A = offset >> 12, SR6B = offset >> 20, SR6C bit 0 = offset >> 28; then SR6F |= 0x80 (the translation-cache flush) and poll until bit 7 clears by itself (it does on the board, in well under 10 ms; if it does not, do not start the ring). Entries: page frame number | 0x01000000; unused entries 0x80000000. **Leave bit 25 (snoop) clear**: the engine reads memory, not the CPU's caches (24.5).

**Ring.** 64 fixed pages (256 KB) of system memory from `_PageAllocate(PG_SYS, PAGEFIXED)`, physical addresses from `_CopyPageTable` (each must be present, above 1 MB and distinct), mapped at GPU address 0x100000 (entries 0x100-0x13F). Entries 0xFF and 0x140-0x143 point at a guard page filled with no-op blocks. Fill the ring with no-op blocks before the first start.

**Before every start: XP's engine initialisation** (s3ginv 0x110E86): 68 = 0A0000, then 02 = 0. This was the whole difference between v10.402 (both starts stayed idle: 43C 0, 444 000801FF) and v10.404 (ran first time).

**Start** (s3ginv 0xBB370), interrupts off for the register writes:

1. If the fetcher was left paused in the ring (444 bit 31 set), write at its read position (43C, read until two reads agree) XP's batch end `FE020420 0000041C 00300000 CC000000` and `CCC00001 DDD00001` pairs to the next 256 bytes, run it with 64/65 = its end − 0x10 and 65 bit 9 (0x200, one-shot pause), and wait for idle.
2. At the ring's start: the batch end and `CCC00002 DDD00002` pairs to 256 bytes.
3. 60 = 0x100000 | 1, 61 = 0; 62/63 = the ring's end − 0x10; 64/65 = 0x100000 + 256 − 0x10; 66 = FFFFF0, 67 = 0000FF; 68 = 0A0001.
4. The first start of a boot must reach its pause inside the ring (43C between 0x100000 and the end, the engine idle) within 100 ms; if not, GART fetches do not work and the driver stays on the one-shot path. On the board XP's form passed first time.

**Kick** (s3ginv 0xBC6CE): the group's words, the batch end, `CCC00004` to the next 256 bytes, then 0x41C and 64/65 = that end − 0x10. 68 is never written again while the ring runs.

**Wrap** (s3ginv 0xBBC10): the wrap point is one block before the ring's end (66/67 = 0x13FEF0 for this ring; v10.403's wrap point on the end itself made the fetcher stop after every wrap). Write the batch end and `CCC00003` up to it, 66/67 = that point − 0x10, 64/65 = the ring's start − 0x10, then the next group at the start. Never restart the ring because 444 bit 31 reads clear with nothing in flight: XP never does, and it was harmless (5,576 wraps).

**No wait at a wrap** (v10.410, XP's space check s3ginv 0xBBF3D-0xBC16B): after a wrap and until the next full retire, check each new group's room (plus 1 KB of read-ahead) against 43C and wait only when the fetcher is still inside it or 43C is outside the ring. 15,900 checks, 0 waits.

**Writing the words** (v10.411, ACCEL\_D3D\_XPRING\_NT, needs SSE2): build each group in a cached staging buffer, copy it into the ring with MOVNTI four words a step, one SFENCE before the kick's register writes. No ring line may be written both ways: the start block, wrap padding and restart pad keep plain stores and CLFLUSH + MFENCE (WBINVD without CLFLUSH) in lines of their own. Without SSE2: plain stores and CLFLUSH before each kick (7.5 µs a packet).

**When to stop.** The ring is not stopped between groups or at a wrap; only before the fetcher is driven another way (one-shot, register path, queries, a mode set) and after a hang (68 = 0A0000). The fence of 23.4 (ACCEL\_D3D\_RINGFENCE) and the gap overwrite (ACCEL\_D3D\_GAPWIPE) stay. A hang turns the ring off for the session; drawing goes on one-shot.

**The ring in video memory is removed** (v10.406, with ACCEL\_D3D\_RING, RING\_PARK, RING\_PROBE, RING\_REKICK, RING\_KICKGAP / KICKWAIT, RING\_PROGWAIT and RING\_HANGOFF): see 24.5.

### 24.2 The frame's waits removed, and fixes found on the way (v10.390-v10.435)

With the ring running, every remaining wait was something that forced the CPU to retire the ring (wait for it to drain) before going on. Each fix below removed one cause, found by counting the retires by cause.

| Key (default) | What it does | Board result |
| --- | --- | --- |
| ACCEL\_D3D\_FRONTZ (1), v10.416 | With FLIP\_ASYNC (23.5), only a packet that writes colour waits for the old front to leave the screen; a depth/stencil-only clear or a draw with no colour channel written goes at once | 315 waits, 2,902 ms a session, gone |
| ACCEL\_FLIP\_RING (1), v10.417 | A vsync flip of one frame goes into the ring as XP's (s3ginv 0xB6B50): 2D register 6C = 1 (the engine waits for IGA1's blank), then the command regulator's 10 = 10000000 \| address & FFFFF8, 11 = 11000000 \| address >> 24, 12 = 12000008, `CC000099`; CR32 bit 0 set once before. A Lock, Blt, clear or the next flip waits for the flip's fence; a fetched draw on the old front needs no wait. NOVSYNC, an address not 8-aligned, a stopped ring, a reset or a mode set: CPU flip with CR32 bit 0 cleared first. A ring flip whose fence does not come in 100 ms turns ring flips off for the session | every flip in the ring (1,394), none declined, picture right |
| ACCEL\_3D\_RINGCLEAR (1), v10.418 | A whole-surface clear from 0,0 (depth and/or colour, no stencil, no masked word) is the same 2D solid fill written into the ring after the draws and the flip, with the fence's registers | 379 clears of 14.5 ms each gone; 1,797 clears in the ring, none declined |
| (no key), v10.419 | A flip that goes into the ring syncs only the 2D side (a pending async blit, a kept context) and does not retire the ring | 2,542 retires a session (6.8 ms each) gone; frames 48.3 ms on average |
| ACCEL\_D3D\_WCFLUSH (1), v10.390 | SFENCE (when CPUID reports SSE) and a read of the last word written through the frame-buffer mapping before any fetcher start or kick, as VIA's Linux DRM does | write-combined words always reach memory before the fetcher reads them |
| ACCEL\_LOCK\_HAL (1), v10.432/435 | Each Direct3D context create asks the VxD (VX\_IOCTL\_LOCKMEM 0x85500028) to lock VXDDRAW.DLL's whole image (SizeOfImage from its PE header) with `_LinPageLock`, a page at a time, all or nothing; a handle's close drops its hold, the last one unlocks. Ask only on the process's kept handle (a temporary handle's close would release at once) | DrawPrimitives2 calls had stood still up to 450 ms with 400-900 page-ins a second while the HAL's own code and packet queue were read back |
| ACCEL\_D3D\_RSSYNC (1, feature bit 25), v10.428 | At the end of each DrawPrimitives2 call the pixel-pipeline render states are taken from the runtime's array (lpdwRStates) when it disagrees with the context and changed since the last call | the runtime records a SetRenderState after EndScene that never reaches DrawPrimitives2: a whole frame drawn with Z writes off (VXTCBENCH, 12 of 12 tests now clean) |
| ACCEL\_ASYNC\_PRESENT (1), v10.424-426 | VXGL's swap blit to the primary runs on the 2D engine asynchronously; while it runs bit 15 of the display driver's wEnabled word is set, and DIBENG's BeginAccess hook and forwarders finish it first (DRV call 0x1136) | OpenGL SwapBuffers 1.45 → 1.00 ms; GDI drawing right after a swap no longer painted over |
| ACCEL\_D3D\_DITHER32 (0), v10.415 | DITHERENABLE clears bit 16 of 01/53, 5B, 63, 6B only on a 16-bit target; a 32-bit target is not dithered. 1 = dither 32-bit too (the old look) | GTA3's flat menu areas at 32 bpp showed a 4x4 pattern another card does not draw |
| ACCEL\_D3D\_RINGBLT (1), v10.453 | A render target's copy into a texture (Direct3D 8's CopyRects, GTA3's Trails: about one 640x480 back-buffer copy a frame) is written into the ring after the draws as the 2D engine's screen-to-screen copy, a group of its own followed by the ring's fence: 2D registers 04 depth code, 08 = (dst pitch / 8) << 16 \| src pitch / 8, 14 = dst base / 8, 1C = src base / 8, 30 48 58 5C 20 = 0, 18 = sy << 16 \| sx, 0C = (h − 1) << 16 \| w − 1, 10 = dy << 16 \| dx, then 00 = CC000001. A clip rectangle is applied by moving the copy (no clip registers). The ring's saved 2D registers include 18, so a retire puts it back. The driver's state cache starts over after it (the next set-up strobes the texture cache). Taken only for SRCCOPY, the same format at the screen's depth, distinct surfaces, 8-byte aligned bases and pitches, no software cursor on either surface; anything else, or the ring not running, takes the 2D path after a retire. The HAL marks such copies VX2D\_RT. Trails checked on the board: right | Blt −2.51 ms a frame \[2.50, 2.53\] (61 turns), no wait moved to Flip or Lock; whole frame +2.9 ms \[−8.5, +12.4\]: the right way, not yet beyond the scenery's noise |

Also fixed: the primary at offset 0 is described to the 2D engine from the display's own pitch and size (FBHDA), not DirectDraw's record, which keeps the old size after a mode change and back (v10.423: 32 ms software swaps after a Direct3D test in the same process); a declined GDI request and a destroyed surface retire a 3D draw still in flight first (v10.426).

### 24.3 The per-packet and per-triangle CPU path (v10.412-v10.455)

Once the waits were gone, GTA3's frame time was the CPU building packets: about 5.7 µs of driver time a triangle (HAL 3.0, VxD 2.7), 1,075 command words a packet (vertices 60%, set-ups 25%, heads and padding 15%), and each vertex handled four times (v10.441). The HAL's triangle fast path, sampled one triangle in 16 with the TSC (v10.445, cycles a triangle at 1000 MHz):

| Part | Cycles |
| --- | --- |
| Vertex writes | 525 |
| Opening the batch slot (0.7% of triangles also send the batch before: about 35,400 cycles each) | 308 |
| Strip and pair matching | 243 |
| Fetch and index range checks | 219 |
| Area and cull (doubles: cheap on the C7) | 112 |
| The rest | 112 |
| **Total** | **1,518** |

Each change below keeps the fetched words identical (checked word for word against the old path); each has a registry switch.

**Prefetch and the one-pass block switch** (v10.412, ACCEL\_D3D\_PREFETCH, ACCEL\_D3D\_FASTBLK). While one packet is built the next one's ranges are prefetched (SSE); the validation's vertex and texture-coordinate tests run in one loop with a coordinate inside the clip tested in one compare; the state set-up's words go through a local pointer and its block switch is done in one pass (about 40 blocks a packet). Without the prefetch, copy-in 3.2 → 5.7 µs a packet; without the block switch, state set-up 10.3 → 11.1, texture 4.1 → 5.0, program 3.5 → 4.0.

**Prepacked vertices** (v10.441, ACCEL\_D3D\_PREPACK, feature bit 26). The packet carries a vertex block already in XP's 10-word vertex form (the words the emitter would build). The HAL writes a triangle draw's vertices there once (fast path, general path, the strips' reorder, point squares) when the template has an XP program, at most two texture units, no projected, cube or volume layout and no third coordinates; lines, the chip's points and OpenGL keep the six arrays. The VxD checks every vertex as before (x y z rhw against the clip, u0 v0 finite when textured, u1 v1 finite with two units) and the block's form, and the emitter copies the words into the command block with one `rep movsd`. Results: DrawPrimitives2 6.83 → 6.31 µs a triangle between two runs; in-run A/B −0.22 µs a triangle (vertex writes −120 cycles).

**PAIRLIST** (v10.442, feature bit 27). 30% of GTA3's triangles started a strip, most of them a lone pair that then cost a strip head and padding. Now a triangle sharing an edge with the list triangle before starts a strip only when the token's next triangle goes on with it (holds the new vertex and one of the shared two); a lone pair stays in the list run. With no next triangle known (end of a token or chunk) it behaves as before. Strip starts 30% → 7% of triangles, heads and padding 145 → 64 words a packet, the emitter's vertex part 11.1 → 7.4 µs a packet. In-run A/B: about neutral in v10.448 (VxD −0.11 µs a triangle, the HAL's extra vertex writes +62 cycles against matching −61); with every switch measured in one run (v10.451, 22 turns) the VxD −6.8 µs a packet \[4.8, 8.4\], the HAL's extra vertex writes (+62 cycles) and saved matching (−68) still cancelling. Kept on.

**VRECIPE** (v10.445, feature bit 29). The prepacked vertex is written from a recipe worked out once per template and vertex format (colour, specular, texture offsets, units), and a list triangle's three vertices in one call with the recipe in locals. Only without ZBIAS and the OpenGL snap. In-run A/B: vertex writes 520 → 453 cycles a triangle (v10.447), −61 again in a later run (v10.448).

**FASTMATCH** (v10.445, feature bit 30). Before the strip and pair matching, test first the only cases it can act on (a strip's last vertex held, or exactly two indices of the list triangle before); otherwise go straight to the list. In-run A/B: matching 238 → 222 cycles a triangle (v10.447), −12 in v10.448, −5.7 \[−1.3, +12.8\] in the all-switch run: small, never worse; kept on.

**FASTCLOCK** (v10.448-449, ACCEL\_D3D\_FASTCLOCK). On Windows 98 the VMM's `Get_System_Time` also reads the timer chip (slow port I/O). Time stamps that only need tick resolution read the VMM's time word instead (`Get_System_Time_Address`, read once at init; the word is the time at the last tick, so a limit counted from it can last at most one tick longer): the kick's in-flight start time, the 100 ms limits of the in-flight checks, a one-shot group's start, the submit calls' start. With it on, a slow call's thread run time (`Get_Thread_Exec_Time`, another timer-chip read) is not read. Short hardware waits keep `Get_System_Time`, since the word moves only once a tick. In-run A/B: the VxD −2.5 µs a packet; the "recorder" part of the ring's time (2.7 µs a packet) had been this clock, not the recorder's stores.

**ONECOPY** (v10.449, ACCEL\_D3D\_ONECOPY). In a batch through the fetcher, a prepacked packet's vertex block is not copied into the VxD's copy of the packet: the emitter copies it straight from the caller's packet into the command block (`rep movsd`) and then tests the copy with the validation's vertex tests. A failure refuses the packet before its group is started, exactly as the validation did (the packets before it are drawn and the device goes on). With it, a vertex passes the CPU three times. In-run A/B (v10.449, v10.451): copy-in −4.0 µs a packet \[−4.4, −3.5\] and validation 4.5 → 2.3, but the VxD's whole packet only −0.3 to −0.9 with ranges across zero: the reads of the caller's vertices moved into the emitter's copy (the copy is limited by memory reads, not by how many copies there are). Neutral overall; kept on.

**TEXONLY** (v10.450, ACCEL\_D3D\_TEXONLY). 60% of the state-cache misses were packets that differed from the last only in their textures, yet each sent the whole set-up (state 6.7, texture 3.8, program 2.5 µs a packet). The general set-up (`setup()`), the program (`xp_setup`) and the vertex layout read none of the texture fields (the units' surfaces, filters, addressing, mip chains, cube faces, borders), and the engine keeps the last packet's values, so such a packet sends only:

1. the texture-cache strobe, as the full set-up writes it: 0x43C = FE020000, FE02 registers 01-0E except 06 and 07 = 0, then 00 = 1, 00 = 0;
2. each unit's words: the FE02 general texture state (00, 01, 08, 04, 0D, 10), the unit's attributes (0x43C = 00020000 | u << 24), sampler (20020000 | u << 24, with the border colour) and level block (10030000 + 8u).

The default program and 7-word layout that the full texture set-up writes after the units are left out (unchanged). The test is the whole-state cache's own compare (every byte of the packet but the vertex count, the per-vertex arrays and the runs) with the texture fields' bytes left out, so any other difference sends everything, and so does a new state generation (an engine event, a Lock, a clear in the ring). The full set-up also writes the texture units with the pixel shader on, so nothing new is asked of the engine. Checked by a register model: after every packet of a sequence with texture-only and depth-test changes, every register and data block holds the same value with TEXONLY on and off. In-run A/B (v10.451): the VxD −9.2 µs a packet \[2.6, 10.1\]; 890,436 of 1.99 M packets went texture-only; 1,059 → 951 command words a packet. TEXDBG BC B2 / B3 / B4: off, on, read (with the packets sent so and the misses tested).

**FASTVV** (v10.455, ACCEL\_D3D\_FASTVV, feature bit 28). Before the triangles of a primitive token are built, every vertex they use is checked once a call (a bit per index): x, y and z finite, rhw positive and finite, and the whole vertex inside the vertex data. The old check (`valid_vertex`) worked out the bounds for each vertex again, with a 32-bit division, looked the vertex size up and tested the four values one by one with a float compare: 556 cycles a triangle, about 318 a vertex checked. Now the first index whose whole vertex is not inside the data is worked out once a call (`(vertex_bytes − size) / stride + 1`, capped by the vertex count; 0 when the size, the pointer or the stride rule fails), and below it a vertex passes on one integer test of its four words: `((x & 0x7F800000) != 0x7F800000) & (same for y, z) & (rhw − 1 < 0x7F7FFFFF)`. The last term holds exactly for rhw positive, not 0 and finite (−0.0, negatives, denormal negatives, infinities and NaN all fail). Anything that fails, and any index at or past the limit, goes to the old check, which counts the reason: the same decisions, packets and counters (checked on 600 random meshes of raw bit patterns and special values, strides 20-32, cut data and counts above the data). Nothing is read before its index is below the limit. Result: the vertex checks 556 → about 300 cycles a triangle; in-run A/B −0.235 µs a triangle \[0.217, 0.257\] in the vertices phase.

### 24.4 Measuring: what misled, and the in-run A/B method (v10.431-v10.449)

**The instruments**, all read by VXD3DMON while the game runs and all costing nothing when off:

- DrawPrimitives2 in exclusive phases (VXDDRAW.DLL `VX_D3D_QueryPhases`, TSC): the callback lock, commands and states, vertices, primitives into packets, queue copies, VxD calls, waits; per phase time, entries and calls in duration buckets.
- The HAL's geometry counters (triangles, source vertices, fast-path outcomes, vertices written by each path, triangles sharing 0-3 indices with the one before, why batches were sent) and the fast path's parts, one triangle in 16 (24.3's table).
- The VxD's command words by part (state, texture and program set-up, vertex words, heads and padding) and the set-ups that repeat the last packet's words.
- A 1 ms sampling profiler in the VxD (an asynchronous time-out re-armed from its own callback): the engine's busy bit and what the processor runs, per frame class (gameplay under 75 ms, gameplay 75 ms and more, other), with the thread's own run time beside its sampled share, and the stretches the render thread spends off the processor.

**What misled, and the rule each taught.**

- *Bookkeeping that leaks.* The sampler's "thread inside the VxD" flag was saved and restored around each call; two threads' overlapping calls left it set, and later the call that made a thread the render thread left its depth at 1, so 60-94% of gameplay frames read "in the VxD otherwise" (v10.437-438). Fixed by a per-thread table entered and left by each call, and by measuring the render thread's time inside on the way in and out as a check. Rule: print an independent measure beside every sampled share.
- *Different runs are different scenery.* PAIRLIST looked like −10% DrawPrimitives2 time between two runs (v10.443); inside one run it was neutral. Between runs the fast path's parts moved by as much as any switch could (vertex writes 525 vs 615 cycles a triangle on two routes). Rule: never compare switches across runs.
- *Means are pulled by single hitches.* One sampled triangle that met a stall moved a period's slot-opening and DrawPrimitives2 averages. Rule: medians of periods, and judge a change by the counter of the part it touches, not by whole-call totals (their spread was about 1 µs a triangle).

*Open: a sampler that disagrees with the clock.* In v10.449-v10.451 the sampler put 63-66% of slow gameplay frames "in the VxD otherwise", while the render thread's time measured on the way in and out of the VxD was 25-26% (36 of 143 ms), and DrawPrimitives2's own phases agree with the measurement. A late tick is charged whole to wherever it lands (42 s of a 525 s run were late), which explains only part of it. Until it is found, use the measured VxD time and the DrawPrimitives2 phases, not that sampled share.

**The in-run A/B** (VXD3DMON v6.4 and later, on by default; `NOAB` turns it off). Every speed switch is flipped live: the HAL's through VXDDRAW.DLL `VX_D3D_Switch(id, value)` (0 VRECIPE, 1 FASTMATCH, 2 FASTVV, 3 PREPACK, 4 PAIRLIST; value 0 or 1 sets, 0xFFFFFFFF reads), the VxD's through TEXDBG BC codes (A8 FASTCLOCK, AB ONECOPY, B2 TEXONLY, B5 RINGBLT: the code turns it off, +1 on, +2 reads). In v6.8 ten settings rotate every 2 seconds through the whole run: all on, then each of the nine switches off with the rest on (a turn is 20 s; about 30 turns in a 600 s watch), so every setting sees the same mix of scenery. Each 2-second period is worked out on its own (periods under 2,000 triangles dropped). Per setting it reports the median of its periods; per switch, the paired difference of its off period less all on in the same turn, as the median with its 25th-75th percentile range and the number of turns: a positive difference is what the switch saves. The switches found at the start are put back at the end. Twenty or more turns gave stable answers on the board; a change whose range straddles zero is called neutral. Each switch is measured with the others on, so an interaction shows as part of its figure (ONECOPY only acts while PREPACK is on). VXD3DMON v6.0-v6.3 tested two switches at a time against both off (AB=VF, OC, PL, CL, TX); the all-switch run replaced them and v6.5 removed them.

**One switch alone** (VXD3DMON v6.7 and later, `AB=<name>`, e.g. `AB=RINGBLT`): that switch off and on with the rest on, 2 s each, so a turn is 4 s and a 600 s watch gives up to 150 pairs (periods under 2,000 triangles, menus and loading, are dropped: 61 in the board's run). Use it for a change too small for the all-switch run. Since v6.6-v6.7 the A/B also reports per frame: the frame's ms, the Blts', the Flip's and the Locks' ms, so a wait that only moves from one call to another shows.

*Sampled profiling misses the cache.* The fast path's sampling code runs for one triangle in 16, so it is out of the instruction cache each time: v10.453's chain read 551-618 cycles for "the call into the fast path", about 20 instructions, and every sampled part is inflated a little. Use the per-call phases (every triangle, no sampling) for the size of a part, and the samples only to compare settings.

### 24.5 Failed experiments (all removed from the source)

Each was built, measured on the board and taken out with its switches, debug codes and tests (v10.406, v10.430, v10.449). Do not rebuild one without new evidence against the reason given.

| Experiment | Idea | Why it failed (board numbers) |
| --- | --- | --- |
| The ring in video memory (v10.374-v10.405) and its guards: RING\_REKICK (write a kick again), RING\_PARK (park instead of stop), RING\_PROBE (a no-op group before the first start), RING\_KICKGAP / KICKWAIT (space the kicks), RING\_PROGWAIT (an idle engine before a pixel-program change), the write-combining flush | Keep the fetcher running in video memory as XP's ring | A repeatable hang at a menu group (status 01000031, register 440 2) that no guard removed; the program barrier cost 90.5 against 69.5 µs a packet. The hung group's words replayed 750 times in 15 forms (VXRING) never hung; after one hang the engine reset leaves later groups hanging at random, so nothing after a hang can be bisected. XP never uses this mode for the 5122 (24.1). The write-combining flush stays (24.2), the rest is gone |
| ACCEL\_D3D\_XPRING\_SNOOP (v10.408) | Page entries with bit 25 (XP's miniport sets it for cached pages): no cache flush | The first group was read to its end (43C 00100600) but none of it ran: the fetcher reads memory, not the CPU's cache. Replaced by MOVNTI staging (24.1) |
| ACCEL\_D3D\_VDIRECT (v10.412) | Vertex runs written with MOVNTI straight into the ring at their place | 61.2 µs a packet on, 59.1 off: the ring's write time moved into the vertex loop and the next set-up |
| ACCEL\_D3D\_STATE\_DIFF (v10.287) | Skip set-up words equal to the last packet's, from a diff table | Cost more than it saved |
| Per-group state cache (v10.429, counted only) | Skip the set-up groups that did not change | The emitter's setup() writes every group's defaults, so a skipped group would be left at those defaults; only a texture-only change could be cached, and GTA3's texture set-up differs from the last packet's 99.75% of the time (v10.441). Not built |
| ACCEL\_3D\_COARSEZ (v10.299) | The chip's coarse Z | Correct but slower |
| ACCEL\_3D\_ZFIX, ACCEL\_OPENGL\_ZLOCK (v10.344, v10.363) | Quake 2 depth-clear freeze candidates | None was the cause |
| ACCEL\_D3D\_BATCH\_END | The one-shot group form even with the ring on | A diagnostic for the old ring only |
| ACCEL\_D3D 3, the DirectX 5-style HAL (execute buffers) | A second Direct3D interface | Never drew on the board; 3 now means 1 |
| ACCEL\_D3D\_QDIRECT (v10.443-v10.448) | Build each batch directly in the HAL's queue slot, so its copy into the queue disappears | HAL building plus queue copies 2.11 µs a triangle on, 2.08 off; in-run A/B: the queue copy 0.22 → 0.02 but building 1.84 → 1.97 (a cold queue slot cost what the copy saved) |
| ACCEL\_D3D\_FUSEDVAL (v10.443-v10.448) | Check the prepacked vertices word by word while copying them in, and skip the validation's vertex loops | Copy-in and validation 12.2-12.4 µs a packet on, 10.9 off; in-run A/B: copy-in 6.5 → 9.7, validation 4.7 → 2.3: a C loop is slower than `rep movsd` plus a separate check. ONECOPY (24.3) keeps `rep movsd` and checks after |
| ACCEL\_D3D\_PFVV (v10.464, another session's) | Prefetch every index's vertex six indices ahead in the vertex-check loop | Slower in every A/B turn (vertices -0.058 us a triangle, \[-0.065, -0.027\], v10.465, the slower bench): 81.6 M of 91.2 M index checks were already answered by the checked-vertex bitmap, so nine prefetches in ten were for vertices already in the cache. Removed in v10.467; PFVV2 (below) prefetched only the unchecked ones and lost too |
| ACCEL\_D3D\_PFVV2 (v10.468-.526) | Before a chunk's vertex checks, prefetch only the vertices not yet checked | A loss in 3 of 4 GTA3 A/B runs (-0.25 to +0.15 us a triangle, .523-.526) |
| ACCEL\_D3D\_FASTVV (v10.454-.526) | In the list path's vertex checks, one integer test of x, y, z and rhw below the last whole vertex instead of the full bounds work | A small loss in every GTA3 A/B run (-0.10 to -0.20 us a triangle). The indexed path keeps the same test, where it is the only check (ix\_check) |
| ACCEL\_D3D\_VRECIPE (v10.445-.526) | Write the prepacked vertex words from a recipe worked out once per template and vertex format, held in locals (with a fixed-offset form for FVF 0x1C4) | No consistent saving over four GTA3 A/B runs (+0.22 to -0.15 us a triangle). The recipe stays for the OpenGL ICD's grid words, where it does save |
| ACCEL\_D3D\_FASTMATCH (v10.445-.526) | Test first whether the strip and pair matching can apply to a triangle, to skip its loops | Never saved in four GTA3 A/B runs (-0.001 to -0.27 us a triangle) |
| ACCEL\_D3D\_FASTCHK (.526) | Screen the prepacked vertices branch-free (every test ORed), run the exact loop only for a flagged packet | Validation 8.34 us a packet on, 6.15 off: the exact loop already ran fast on these vertices; 3 of 625,820 GTA3 packets flagged |
| ACCEL\_D3D\_HWCULL (.515-.528) | Let the engine cull indexed packets (01/01 bit 7 inside the set-up's own 01/01, 04/23 bit 8 for counter-clockwise) instead of the HAL's per-triangle area test | No saving in two GTA3 A/B runs: GTA3 draws its indexed lists with cull none, so no packet was culled. Every caller already culls on the CPU. Removed in .529. |
| ACCEL\_D3D\_TEXSHADOW (.526) | Write a texture unit's registers only when their value changed, as the type-01 shadow does for the state | Saved 7 words a set-up, but the compares cost more: GTA3 A/B 0.61 us a packet faster without. The type-01 shadow stays (it saves 47.8 words and 2.9 us a packet) |

Also abandoned: building VXD3DMON's sampled shares without an independent measure beside them (24.4), and comparing switches between runs (24.4).

**Removed in .530 (33.9), with their code and tests.** Measured losses: CLEAR3D, clears drawn by the 3D engine (WinTune 139 against 278 MPixels/s with the 2D fill); DDBLT3D, DirectDraw blits by the 3D engine; DITHER32. Unsupported or never finished: the CR3 reload; styled lines in hardware (the dash pattern; styled pens go to DIBENG, which draws them correctly); the inverting cursor variants, the monochrome cursor unit and the cursor check; the flip's BIOS form and FLIP\_CAL's calibration (the direct flip is the only form); EXTLEN (the trimmed OpenGL extension string); FASTPOINTS; STRICT and KEYZERO; GAMMA\_QUIRK; FORCE\_SOFTWARE, FORCE\_QEMU3DFX and the VM ICDs; ACCEL\_OPENGL and ACCEL\_OPENGL\_HW (the ICD always loads and draws with the hardware). Diagnostics off by default: the emitter's probes (tex\_experiment, lr\_rewrite, the swap), the VxD's reset forms (SR1A is always pulsed), VXDX7's wrap experiment, C3 probe and hang tests, and the tools VX2DASH, VX2CUR2, VX2CR3, VXFIFO, VX2LPAT.

### 24.6 Outside the driver: what the machine itself cost

- **The disk in MS-DOS compatibility mode.** The sampler kept finding GTA3's render thread off the processor in V86 mode (6.0 s of a run, 8.3 s for other threads). Windows 98's IOS.LOG showed the disk running through real-mode INT 13h because a hook in the master boot record made IOS refuse its protected-mode driver; with the hook removed the disk runs in 32-bit mode, the render thread's stretches off the processor fell to 4.2 s with none of it in V86, and gameplay went from a median of 8.8 to 9.9 fps (v10.437 → v10.440, Trails off as well). Check IOS.LOG and Device Manager's performance page ("File system" and "Virtual memory" both 32-bit) before measuring anything.
- **Paging.** With 254 MB and the swap file over 110 MB, 400-1000 page-ins a second stopped DrawPrimitives2 calls for hundreds of milliseconds in the HAL's own code: ACCEL\_LOCK\_HAL (24.2).
- **GTA3's Trails setting.** With Trails on the game Blts the whole screen each frame and the engine waits for the CPU's accesses 7.3 s a run; with it off, 0.2 s.
- **What is left** (v10.440, slow gameplay frames, 133 ms each): DrawPrimitives2 81.4 ms (its VxD calls about 41), outside the driver 50.9 ms (the game's own code and D3D8.DLL); the engine busy 34% of those frames. The remaining driver cost is the CPU path of 24.3.

**What is left, v10.455** (home bench, all frames, 99 ms each; DrawPrimitives2's exclusive phases): DrawPrimitives2 65 ms a frame: primitives into packets (the HAL) about 1.76 µs a triangle, VxD calls 1.85 µs a triangle (43.7 µs a packet, of it about 12.5 the vertex words, a copy of about 3 KB limited by memory reads), vertices 0.40 µs a triangle (0.64 before FASTVV), state into the draw template and buffers the rest; outside the driver's callbacks 33 ms (the game, D3D8). With Trails on, the Blt is 0.3 ms a frame (2.3-2.8 before RINGBLT).

### 24.7 Registry keys, switches, debug codes and tools (v10.387 to v10.478)

**From .530 this table is history:** the driver reads only the values listed in 33.9, and `VX_D3D_Switch` answers only id 16. The rows below record what each switch did and where it ended.

All under HKLM\\Software\\VX85598, DWORD, read by the VxD at boot; 0 turns a feature off. Every speed feature is on by default. The only keys off by default are the opt-in modes of 20.8 and 21.2, ACCEL\_D3D\_DITHER32 (the old look) and the logs.

| Key | Default | Meaning |
| --- | --- | --- |
| ACCEL\_D3D\_XPRING | 1 | XP's ring in GART system memory (24.1); 0 = one-shot groups |
| ACCEL\_D3D\_XPRING\_NT | 1 | Groups staged and copied into the ring with MOVNTI (needs SSE2) |
| ACCEL\_D3D\_RINGFENCE, ACCEL\_D3D\_GAPWIPE | 1 | As 23.4 |
| ACCEL\_D3D\_WCFLUSH | 1 | SFENCE and a read back before a fetcher start or kick |
| ACCEL\_D3D\_FRONTZ | 1 | Only colour writes wait for the old front |
| ACCEL\_FLIP\_RING | 1 | Vsync flips in the ring |
| ACCEL\_3D\_RINGCLEAR | 1 | Whole-surface clears as 2D fills in the ring |
| ACCEL\_LOCK\_HAL | 1 | VXDDRAW.DLL's image locked in memory while Direct3D runs |
| ACCEL\_ASYNC\_PRESENT | 1 | OpenGL swap blits run asynchronously (needs ACCEL\_ASYNC) |
| ACCEL\_D3D\_PREFETCH, ACCEL\_D3D\_FASTBLK | 1 | 24.3 |
| ACCEL\_D3D\_RSSYNC | 1 | Feature bit 25: render states taken back from the runtime's array |
| ACCEL\_D3D\_PREPACK | 1 | Feature bit 26: prepacked vertices |
| ACCEL\_D3D\_PAIRLIST | 1 | Feature bit 27: lone pairs kept in the list run |
| ACCEL\_D3D\_VRECIPE | 1 | Feature bit 29: vertex recipe |
| ACCEL\_D3D\_FASTMATCH | 1 | Feature bit 30: matching only when it can act |
| ACCEL\_D3D\_FASTCLOCK | 1 | Tick-resolution time stamps from the VMM's time word |
| ACCEL\_D3D\_ONECOPY | 1 | A batch's prepacked vertices copied once, into the command block, then checked |
| ACCEL\_D3D\_DITHER32 | 0 | 1 = dither 32-bit targets too |
| ACCEL\_D3D\_TEXONLY | 1 | A packet that differs from the last only in its textures sends the texture units only (24.3) |
| ACCEL\_D3D\_RINGBLT | 1 | A render target's copy into a texture written into the ring after the draws (24.2) |
| ACCEL\_D3D\_FASTVV | 1 | Feature bit 28: the vertex checks' bounds once a call, one integer test a vertex (24.3) |
| ACCEL\_D3D\_RTTEX | 1 | Render-target textures: X8R8G8B8, R5G6B5, A8R8G8B8, one level, power of two (24.8) |
| ACCEL\_D3D\_MIPCAP | 1 | D3DPTEXTURECAPS\_MIPMAP in the texture caps; 0 = the old caps (24.8) |
| ACCEL\_D3D\_TEXHINT | 1 | The HAL remembers each surface handle's slot instead of walking the whole table for every texture lookup (24.10); HAL switch 5 |
| ACCEL\_D3D\_PFVV2 | 1 | Vertices not yet checked in the call prefetched before a chunk's checks (24.10); HAL switch 6 |
| ACCEL\_D3D\_INDEXED | 1 | Indexed packets and the VxD's 1,020 KB vertex area; 0 = neither, the area not reserved (24.12); HAL switch 7 |
| ACCEL\_D3D\_RINGPROG | 1 | A ring retire gives up after 100 ms with neither 43C nor the fence moving, 2 s in all; 0 = 100 ms from the wait's start (24.11) |
| ACCEL\_D3D\_RINGCAP | 1 | In XP's ring a group's first packet's command-regulator blocks go into its words, not to 43C/440 directly (24.11) |
| ACCEL\_D3D\_RTNOWAIT | 1 | A Blt into or out of a render-target texture goes to the VxD without waiting for the engine first; 0 = v10.456's wait (24.14); HAL switch 8 |

Removed since 23.7: ACCEL\_D3D\_RING, ACCEL\_D3D\_RING\_HANGOFF (a hang in XP's ring always turns it off), ACCEL\_D3D\_BATCH\_END and the 24.5 keys. Feature-word bit 28 is unused and kept clear.

**Live switches.** VXDDRAW.DLL exports VX\_D3D\_Switch(id, value): id 0 VRECIPE, 1 FASTMATCH, 2 FASTVV, 3 PREPACK, 4 PAIRLIST, 5 TEXHINT, 6 PFVV2, 7 INDEXED (on without the VxD's vertex area, the VxD draws every indexed packet expanded), 8 RTNOWAIT (24.14); value 0 or 1 sets, 0xFFFFFFFF reads; any other id answers 0xFFFFFFFF. The VxD's own switches are TEXDBG (VX\_IOCTL\_3DN\_TEXDBG) code BC with a subcode: A8 / A9 / AA FASTCLOCK off / on / read, AB / AC / AD ONECOPY off / on / read, B2 / B3 / B4 TEXONLY off / on / read (with the packets sent texture-only and the misses tested), B5 / B6 / B7 RINGBLT off / on / read (with the copies in the ring, not taken, by the 2D engine, declined there and that path's wait), B8 the rest of that census and every CPU-access retire with its time, B9 RINGPROG read (waits that went past 100 ms on progress, the longest wait in ms).

**Counters (TEXDBG BC subcodes).** 82-89 the XP ring's state, starts, kicks, wraps, flush form and first-start registers; 92-94 the ring's time by part; 95 wraps without a wait and space checks; 96 prefetch and block switch; 97 old-front waits left out; 98 / 99 ring flips and declines; 9A ring clears and retires by cause; 9E-A1 command words by part, A2 on and cleared, A3 off; A4 prepacked blocks copied, unpacked, deferred, bytes copied (A5 clears); AE the prepacked vertex copy by part (AF clears it and BB's); BA the ring waits by length (0-1, 2-10, 11-50, 51-100, 101-500, 501+ ms); BB the primitive set-up by part (burst heads, bursts, runs, the time before the first burst, packets); C6 indexed packets (by index, expanded, vertices uploaded, indices, slot refusals, the vertex area's offset), C7 clears. TEXDBG BD 82: the HAL lock's state. VxD IOCTLs: 0x85500027 the ring tape, 0x85500028 the memory lock, 0x85500029 the sampling profiler. VXDDRAW.DLL: VX\_D3D\_QueryPhases (220 words: phases, geometry 148-171, prepacked 172-175, PFVV2 prefetches 177, switches 178, indexed triangles 179, fast path 180-199, the template build's parts 212-219).

**Tools.** VXD3DMON v7.9: run it, then the game; it logs the frame classes, the DrawPrimitives2 phases, geometry, command words, prepacked blocks and copies, texture-only set-ups, the template build by part, the fast path, the primitive set-up by part, indexed packets, the ring and its waits by length, flips and clears, and the all-switch in-run A/B of 24.4 (14 settings since v10.477; NOAB turns it off; AB=INDEXED and the like test one switch alone), the render-target copies, the token loop by part with the profiler's own share, and (v7.7-v7.9) the indexed list loop, the triangles by draw command and path, the retires by cause, and the Blts by kind and by signature with their callers named by module. VXRING v2.8: the fetcher probe of 23.2-23.3; its tape replay and pair tests were for the old ring, and the VxD has answered them (ops 5-12) as bad requests since v10.406. VXRTT v1.1 (v10.456-v10.459): Direct3D 8's view of the driver and render-target textures drawn into and sampled (24.8); v1.3 (v10.478) adds GTA3's per-frame copy into a render-target texture (24.14). VXFORMAT v1.2 (v10.463-v10.477): every Direct3D 8 texture format advertised, created (managed, 7-level, default) and sampled, upload speed, the working-set sweep and the video memory's capacity (24.10). VXFETCH v3.1 (v10.470-v10.475; it loads VXDDRAW.DLL by its full path, as VXD3DMON does, before it makes its device): four scenes (full size, tiny, tiny shuffled, game-like) with INDEXED off and on: ms a frame, the checksums, the DrawPrimitives2 phases, the triangles by path, the VxD's per-packet parts, the retires by cause and the Blts (24.12, 24.13). Each writes its .LOG beside itself.

### 24.8 3DMark2001 SE: render-target textures (v10.456-v10.460, then the original plan)

**Where it stands, v10.459.** P0 is built: `ACCEL_D3D_RTTEX` (default 1; 0 refuses as before) takes a texture that is also a render target (`DDSCAPS_TEXTURE | DDSCAPS_3DDEVICE`): X8R8G8B8, R5G6B5 and (v10.459) A8R8G8B8, one level, power-of-two sides up to 2048, local video memory. CanCreateSurface and CreateSurface treat it as a texture: rows of width × bytes, at least 32, which is a multiple of 32 for every power of two, so the target's pitch rule holds on the same 256-byte base. It is resolved by role: a texture when sampled (as any texture), its target format for ContextCreate, SetRenderTarget and DP2 SETRENDERTARGET (`VX_D3D_surface_target`, a separate resolver the core calls for op 41; A8R8G8B8 is drawn as the 32-bit target). When a render-target texture stops being the target, the HAL sends the draws, then waits for the engine and has the VxD send its whole state with the next draw (`VX_IOCTL_2D_ACCESS` with the surface's range): the OpenGL ICD's proven order for its render-to-texture (`rtt_done`). A Lock waits as for any 3D surface, and a Blt into or out of one waits first. A refused render-target texture goes to the HAL event log (callbacks 32 and 33: caps, pixel format, size, mip count).

**Board, VXRTT v1.0 (v10.456, 800x600x32):** Direct3D 8 reports TextureCaps 00024C07, MIPMAP included (`ACCEL_D3D_MIPCAP`, default 1, adds D3DPTEXTURECAPS\_MIPMAP 0x4000; the runtime passes it through and adds CUBEMAP\_POW2). An X8R8G8B8 64x64 render-target texture: created, drawn into (a blue clear, red and green quarters), sampled onto the back buffer with all four quarters right (corners, orientation, pitch), and 50 rounds of draw-then-sample with a new colour each, 0 wrong. On the 32-bit desktop Direct3D 8 itself refuses R5G6B5, X1R5G5B5 and A1R5G5B5 render targets (CheckDeviceFormat D3DERR\_NOTAVAILABLE: a DirectX 7 driver's render targets follow the display's format), but offers A8R8G8B8 (CheckDeviceFormat 0), which the driver refused until v10.459 with D3DERR\_NOTAVAILABLE.

**Board, 3DMark2001 SE (v10.458, 1024x768x32, Z32, compressed textures):** still `P_D3D::allocateMap - CreateTexture ( for a rendertarget) failed : D3DERR_NOTAVAILABLE`, the same answer as VXRTT's A8R8G8B8. v10.459 takes A8R8G8B8; VXRTT v1.1 tests it the same way, shows whether the texture keeps alpha (a clear's, and a draw's: the engine's 32-bit target may not write alpha), and creates render-target textures from 128 to 2048 square in both 32-bit formats. Not yet run on the board. If 3DMark still fails, `ACCEL_D3D_LOG = 1` and C:\\VXD3D.LOG's callbacks 32/33 name the format and size it asked for.

An earlier error.log named driver 10.365: check the platform line of a 3DMark log before reading anything into it.

**Board, VXRTT v1.1 (v10.459): PASSED.** A8R8G8B8 render-target textures right (four quarters, 50 draw-then-sample rounds); render-target textures created from 128 to 2048 square in X8R8G8B8 and A8R8G8B8; R5G6B5 not offered by Direct3D 8 on the 32-bit desktop (not counted). Alpha: a clear kept it (40), but a draw left 0: drawn as the X8R8G8B8 target (01/52 code 98) the engine does not write alpha. **v10.460:** a colour target format A8R8G8B8 (`VX3DN_ARGB8888` = 30, engine code **99** in 01/52, HCM p95), used for A8R8G8B8 render-target textures, so the engine writes alpha and destination-alpha blending reads it. The desktop's own surface stays X8R8G8B8 or R5G6B5. Not yet on the board: VXRTT's alpha line should read 808080 for a draw.

3DMark2001 SE build 330 stops at its first scene with `P_D3D::allocateMap - CreateTexture ( for a rendertarget) failed : D3DERR_NOTAVAILABLE`. That run's log named display driver 10.365, so check the installed files' versions before the next try. A static audit of the v10.449 source matched the error to a missing feature: **a texture that is also a render target**. The engine side is already proven: the OpenGL ICD renders into X8R8G8B8 textures on the board (16.23, ACCEL\_OPENGL\_RTT, VXGL scene 56). What is missing is the DirectDraw and Direct3D HAL side.

**Where the driver refuses it today:**

| Where | What it does |
| --- | --- |
| `vx_ddraw.c` CanCreateSurface (874-888) | A video-memory texture with DDSCAPS\_3DDEVICE is refused; the 3D-target branch excludes DDSCAPS\_TEXTURE |
| `vx_ddraw.c` CreateSurface (916-948) | The same two exclusions again; the target pitch and the texture pitch are chosen in branches that exclude each other |
| `vx_d3d_hal.c` VX\_D3D\_surface (299-315) | A surface is a texture (`VX3DN_TEX_*` format) or a target (`VX3DN_XRGB8888` / `VX3DN_RGB565`), never both |
| `vx_d3d_core.c` vx\_d3d\_targets\_valid (534-543) | Colour targets only XRGB8888 and RGB565 |
| `vx_d3d_hal.c` SetRenderTarget (1018-1029), resolve (638-646) | Resolves a handle once, as one role, and caches it |

**The plan, in order:**

1. Find the exact request first: a small D3D8 program calling `CheckDeviceFormat(…, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, fmt)` and `CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, fmt, D3DPOOL_DEFAULT)` for X8R8G8B8, R5G6B5 and A8R8G8B8, recording the HRESULTs and whether CanCreateSurface / CreateSurface / CreateSurfaceEx are called. If the runtime refuses before calling the HAL, the cause is in its caps-to-formats conversion, not in surface creation.
2. **P0, dual-use surfaces:** X8R8G8B8 and R5G6B5, local video memory, power-of-two sizes. One allocation with a pitch both paths accept (the texture's exact pitch, at least 32 bytes, and the target's 32-byte pitch and 256-byte base); resolved by role: target format for SetRenderTarget, DP2 SETRENDERTARGET and context create, texture format for sampling; the cache keyed by role as well as handle. Draws into it finished before it is sampled; Lock, loss and destroy handled as for either kind today.
3. **P1, only if 3DMark asks for them:** an A8R8G8B8 render target (the sampler takes A8R8G8B8 today, but the engine was never proven drawing into it); non-power-of-two render-target textures (textures must be powers of two today, VX\_D3D\_texture\_ok); mipmapped or cube-face render targets.
4. **Check, do not just set:** the legacy texture caps (`vx_d3d_hal.c` 1185) carry no D3DPTEXTURECAPS\_MIPMAP (0x4000) bit while the filter caps offer mipmapping. Velocity9x's DX8 driver had 3DMark refuse its first mipmapped texture without that bit; read D3D8's effective GetDeviceCaps before changing it.
5. **P2, later:** sampling more than 9 levels of a 2048 chain (the engine's level block holds levels 1-8); the DX8 driver interface (GetDriverInfo2, its own caps and format list); hardware T&L and shaders (not on this engine's fixed-function path; 3DMark runs those scenes with software T&L or skips them).

**Tests for P0:** draw a red/green/blue checkerboard into a 64x64 X8R8G8B8 render-target texture, switch back to the back buffer and draw a quad sampling it; check corners, orientation and pitch; again with R5G6B5; several target switches, a clear, sampling straight after drawing with work still in the ring; refusals (A8R8G8B8, NPOT, DXT, too large) answered as errors with no handle or memory left behind; GTA3, Quake 2 and the DX7 callback and ring checks unchanged. Then 3DMark2001 SE at 640x480x16, Z16, uncompressed textures, then its 1024x768x32, Z32, compressed set-up.

Already there and not part of this work: D16, D32 and D24S8 depth with stencil; X8R8G8B8 and R5G6B5 targets; A8R8G8B8, RGB565, A1R5G5B5, A4R4G4B4, X1R5G5B5, L8, DXT1/3/5 and bump textures; three stages, DOT3, bump, cube and projected sampling; the DP2 primitive, state, viewport, clear and target commands.

### 24.9 The screen saver lockup: a stale OpenGL ICD (v10.457-v10.458)

**Symptom.** The Really Slick Screensavers Flux and Flocks (and Fieldlines) locked the machine up when the Screen Saver tab of Display Properties was selected; run directly they were fine, and 3D Pipes previewed fine.

**What the tab does** (DESK.CPL 4.10.1998, read from its code): every `*.scr` is loaded into the Control Panel's own process with `LoadLibrary`, its description read with `LoadString(1)` and freed; then the selected saver (SYSTEM.INI \[boot\] SCRNSAVE.EXE) is started with `WinExec("<saver> /p <hwnd>")` on a child window of class `SSDemoParent` in the monitor picture. The Really Slick savers pick the pixel format with the most depth bits (here the double-buffered 24-bit depth + 8-bit stencil one), draw whenever no message waits and end a frame with `wglSwapLayerBuffers`.

**What it was.** C:\\VXGL.LOG showed that in the previews the ICD loaded was VXGL 3.73 of driver v10.422 from **C:\\VXGL.DLL**, a stale copy, not the system directory's. The INF registered the bare name `VXGL.DLL` under OpenGLdrivers, and `LoadLibrary` searches the program's directory and the current directory before the system directory: Pipes lives in the system directory and found the right copy; the Really Slick savers live in C:\\WINDOWS and, started from the Control Panel, found C:\\VXGL.DLL first. The current VxD answered every packet of that build LOST (another packet layout) and it drew everything in software, 2-4 frames a second. Deleting the stray copy fixed it on the board.

**Kept (v10.458):** the INF registers `%11%\VXGL.DLL` (the full path); the ICD's `DrvValidateVersion` asks the VxD its build (TEXDBG 75) and refuses when it is not its own (logged with the DLL's path), so opengl32 falls back to Microsoft's renderer instead of a mismatched ICD. **Removed after the fix (v10.459):** VXSAVER (Flux and Flocks rebuilt in 40 stages, and a step-by-step copy of the tab), the host scenes of both savers, `VXGL_LOG = 2`. Those 40 stages, preview-sized and full screen, both pixel formats, asynchronous present on and off, ran on the board without a lockup: the drawing itself was never at fault.

**Lesson.** When a fault appears only under one launcher, first check which copy of each driver DLL the process loaded (the ICD's log names its build and path), before reading the drawing code.

### 24.10 Per-packet cost, texture lookups and what was ruled out (v10.461-v10.468)

**Where a GTA3 frame goes** (v10.462, main bench, 106 ms on average): the game and the Direct3D 8 runtime about 35 ms (with the runtime's software T&L: the device reports no hardware T&L), the HAL about 37 (primitives into packets 21.8, the template 6.9, vertex checks 5.1, buffers 2.8), the VxD about 22 (45.6 us a packet, about 446 packets a frame), flip and Blt about 4. Waiting for the engine: 0.0 ms a frame; the engine is busy 23-34% of the frame. The frame is CPU-bound in every layer, and no layer dominates: tuning one piece moves the total a few percent at a time.

**The texture lookup (ACCEL\_D3D\_TEXHINT, v10.463).** v10.462 measured the template build by part: 16.3 us a build, 7.6 of it resolving the textures. Each resolve, cube-face and mip lookup walked the whole surface table (about 2,000 surfaces in GTA3, a slot 40 bytes: cache misses). The hint keeps a handle's slot by its low 10 bits and takes it only when the slot is one of the table's and still holds the handle (a table holds a handle in one slot at most), so the result is the walk's. Board, v10.463: a build 11.5 us, the textures 3.8, the template 4.7 ms a frame (6.9). Builds whose state differed from the last only in texture handles: 0%; identical: 2-5%: a texture-only template path would have nothing to do.

**Memory is not the limit.** With a 2 GB module (Windows sees 1,833,300 KB, the least free 1.38 GB), page-ins stayed at about 690 a second and the gameplay median at 7.3 fps (7.9 with 256 MB). The ring's pages are allocated below 4 GB (maximum page 0xFFFFF), so 2 GB is safe for the driver.

**Texture formats and memory (VXFORMAT v1.0, v10.463).** Every format the driver offers is advertised, created (managed, a 7-level chain, default pool) and sampled right: A8R8G8B8, X8R8G8B8, R5G6B5, X1R5G5B5, A1R5G5B5, A4R4G4B4, L8, DXT1/3/5, and the three bump formats (created). A8L8, A8, P8 and A8P8 are not offered (GTA3 converts its palettised textures into its txd.img cache once; the cache is reused). Texture memory 48,128 KB: 378 default-pool 256x256 R5G6B5 textures (47 MB); past it the texture manager re-uploads every frame (100-167 ms a frame). Upload through the draw 234-301 MB/s. In GTA3 no eviction shows (surface creates about 0 a frame, Blt 2.1 ms). The sweep's slope is the useful number: 38-42 us a textured draw whatever the format or size: the per-packet cost.

**State deltas (step 3) ruled out.** The "53% of state set-ups repeat the last packet's words" (v10.465) were the TEXONLY packets: their state set-up is the texture-cache strobe, the same every time (882,383 of them against 854,028 repeats, about 36 words each). Full set-ups that repeat are next to none: the remaining misses differ in fields setup() reads. Not built.

**Ring waits** (v10.467, the slower bench): 15,534 of 0-1 ms, 2,847 of 2-10, 45 of 11-50, none above; the longest 18-25 ms. GTA3 never comes near the 100 ms limit of 24.11.

**The slower bench** (256 MB, the swap file in use): its first vertex copy runs at 382 MB/s against 761 from the cache, so half the copy waits for memory there; on the main bench 735 against 765. Compare runs only within one bench.

**PFVV2 and the burst profile (v10.468), not yet measured on the board.** PFVV2 prefetches only the vertices whose checked bit is clear (the 24.5 PFVV prefetched all). The burst profile splits the VxD's primitive set-up and vertices (13.6 us a packet) into the time before the first burst, the burst heads, the copy and its checks.

### 24.11 Review fixes (v10.465-v10.466)

A review of the HAL, the core and the VxD, each finding checked against the code before it was fixed:

- The stretched Blt's staging block was kept in the DLL's shared data but is one process's memory: kept per process now (8 slots by process ID).
- ACCEL\_D3D\_RINGPROG: a ring retire gave up 100 ms after it began however much fetched work the ring held, so a busy engine could be taken for hung (ring off, engine reset). Now it gives up after 100 ms with neither 43C nor the fence moving, 2 s in all; a lost kick still shows as a hang.
- A hung synchronous ring group was reset twice (in the retire and by the caller): once now (RECOVERED).
- A hang found while waiting for room in the ring's last lap restarted the ring for one more group: that group goes one-shot now.
- The vertex-check cache was set up before the vertex buffer was bound when a command with no primitives came first: set up at the first command with primitives.
- GetBltStatus took the 3D ring's paused fetcher for a busy engine: it uses the waits' idle rule.
- A lab request refused before any work reached the engine no longer latches the fault that refused every 2D, flip, D3D and GL request until a reboot.
- A batch on the primary that ended early left a split-off group running under the redrawn cursor: it is finished first.
- The runtime's parse callback kept per process (16 slots).
- DestroySurface with the HAL's lock held answered WASSTILLDRAWING while DirectDraw freed the surface anyway: the lock is waited for, then the surface queued and forgotten by the next callback before its own work.
- ACCEL\_D3D\_RINGCAP: a group's first packet sent its command-regulator blocks to 43C/440 directly while the ring held its paused fetcher; they go into its words as every later packet's do.
- The fetch position (43C) is taken only when two reads agree; else the lap check waits for the engine.
- OVERLAY\_n\_SIZE of 4096 MB or more, or overlays past half the video memory, are taken as no overlay.
- VX\_IOCTL\_LOCKMEM: another range while another handle holds the lock is refused; 64 holders.
- A surface locked again before its Unlock: DXT packing and D24S8 merging at the first Lock, undone at the last.
- Destroying an overlay surface hides the overlay only when it is the one shown.
- DDCREATEDRIVEROBJECT without an output buffer is refused.

Rule kept from this review: every confirmed bug is fixed, including ones no current path reaches.

### 24.12 Indexed packets: the engine's vertex fetch (v10.469-v10.470)

**Why.** A 2001 driver built commands in user mode at a fraction of a microsecond a triangle and had hardware T&L. This driver expands every indexed triangle into three 10-word vertices in the HAL, then checks and formats them again in the VxD: the per-triangle and per-packet CPU work of 24.10. The engine can instead read vertices by index from video memory (HCM Part 2 p148-162; the v10.317 T&L probe's consumer).

**Proven on the board (VXFETCH v1.0, v10.469).** The driver's own 10-word vertex (X Y Z rhw diffuse specular u0 v0 u1 v1, the layout xp\_setup programs), copied to video memory, drawn by index: the same picture as inline (checksum identical in four passes), 1,564 packets, bursts of 48 indices, no hang. Per burst, type 04: 21 = base & 0xFFFFFF, 22 = 40 << 8 | base >> 24 (the stride is bytes), 2C = the last vertex index, 2D = 2E = 0, 20 = 3 (32-bit indices), 23 = the mode, 24 = the count; then type 0 with one dword an index on 0x440. After the packet write 04/20, 21, 22 and 2C back to 0: setup() writes them only until its constant groups are primed, so an inline packet after an indexed one would keep the fetch's base, stride and bound.

**The production path (ACCEL\_D3D\_INDEXED, v10.470).**

- The packet (no size or ABI change): pre.on = 2; pre.w holds its distinct vertices (at most 192) in the 10-word form; the vertices\[\] array, unused by prepacked packets, holds the 16-bit indices (at most 1,920); one run of VX3DN\_TOPO\_INDEXED (2) whose count is the indices, whole triangles.
- The HAL's fast path maps each triangle's source indices to packet-local ones through a 512-entry table (reset per packet and per vertex buffer), converts a vertex the first time the packet sees it, and appends the indices: no expansion, no strip or pair matching. Any other path (clipping, points, lines, strips) ends the indexed packet first.
- The VxD validates one run, whole triangles, every index below the vertex count, every vertex's words; copies the packet; writes the vertices into a slot of its vertex area (136 slots of 7,680 bytes below the page table block, reserved at boot when ACCEL\_D3D\_INDEXED is 1) and sends the indices.
- Slot reuse: each slot keeps the group that read it (in XP's ring the fence sequence its group will be kicked with, else the one-shot group's number). A slot is given again only when nothing is in flight, the fence has passed it, or another one-shot group has started since; never to the group being built. Without a free slot the packet is drawn with its indices expanded into inline list bursts: never a wait.

**To measure:** VXFETCH v2.1 (INDEXED off and on, the grid in drawing order so the inline path still makes strips; the pictures must MATCH) and VXD3DMON v7.6 (the indexed-packet line, "primitives into packets", the INDEXED A/B row).

**Next (the plan, builds 2 and 3):** the HAL writes the vertex block into a video-memory vertex ring itself and the packet carries only a base and the indices (the VxD does nothing per vertex; slots reused by fence); then one VxD call and one kick per DrawPrimitives2 call.

### 24.13 The indexed list loop (v10.471-v10.476)

An indexed triangle list token (DrawPrimitives2 op 26, and op 3) goes through one tight loop when ACCEL\_D3D\_INDEXED is on. On GTA3 it cut DrawPrimitives2 by 12-19% and VXFETCH's indexed scenes by 20-37%; its real gain is CPU time taken off the C7.

**How the loop works (ix\_list, the HAL's core).**

1. Before the loop, check only the token's largest index against the vertex data, so no index can fail the call part-way. If the draw template is not ready for this context and FVF, build it first (tc\_ready), so the draw's first triangle uses the loop too.
2. Look each source index up in a direct-mapped table of 8,192 entries, each `(generation << 16) | packet index`, with a 16-bit generation; use a hash only outside the window. Place the window at the packet's first triangle's lowest index less 4,096, not at the token's base vertex: with indices far into a large vertex buffer, every lookup otherwise took the hash.
3. Check a vertex (valid, inside the viewport and depth range, rhw positive) once, when it first enters a packet; convert it then.
4. Cull on the raw floats, loaded straight into the x87.
5. Open a packet only after the cull and the checks (an empty packet otherwise). ix\_tri shares the opening (ix\_open) and the table.
6. A triangle whose vertex fails stops the loop: the per-triangle path draws it (bad-vertex drop, general path) and the loop resumes after it.
7. No call, profiler test or geometry count per triangle unless the profile is on.

The packets are word for word those of the per-triangle path (host tests: cull modes, op 3, base vertices, two bases in one call, bad and off-screen vertices, 20,000-vertex buffers, an index past the data).

**Counters.** QueryPhases 207-211 (profile on): loop cycles / 16, triangles, stops, vertices converted, packet-opening cycles / 16. Words 220-235: triangles by draw command (3, 18-22, 26, 23) and path: through the loop, culled, stops, per-triangle inside its tokens, and why (template not on the fast path, or not prepacked), and kept out (INDEXED off, an index past the data). The VxD's indexed packets by part are TEXDBG BC CC / CD (copy-in, validation, the emitter: set-up, slot and vertices, index bursts, registers back; CE clears); retires for a CPU access by cause are BC CA (counts) / CB (time) / CD (causes 6-7).

**Board results.**

| Run | Indexed | Inline | Loop | VxD a packet |
| --- | --- | --- | --- | --- |
| VXFETCH v3.1, full size, grid | 7.6 ms a frame | 10.0 | 570 cycles a triangle, 0.62 new vertices | copy-in 10.0, validation 15.4, emitter 39 us |
| VXFETCH v3.1, tiny, shuffled | 9.7 ms | 12.4 | 1,125 cycles, 2.56 new vertices | copy-in 15.3, validation 8.9, emitter 14.1 us |
| VXFETCH v3.1, game-like (374 draws of 31 triangles) | 40.7 ms (DrawPrimitives2 33.8) | 49.3 (42.8) | 1,150 cycles, 1.79 new vertices | copy-in 6.1, validation 3.9, emitter 9.3 us |
| GTA3, v10.472 | DrawPrimitives2 54.3 ms a frame | 62.9 before the loop (v10.465) | 1,125 cycles, 1.65 new vertices; 44.1 M of 61.1 M triangles through it | 41.3 us (45.8) |

The game-like scene matches GTA3's shape, so it stands in for GTA3 when a scene can't be reproduced in the game. In it the loop is 16 ms a frame, the VxD 10.3 ms and the template 3.8 ms.

**Measurement lessons.**

- A TEXDBG read must not start the VxD's 3D state cache over: v10.476-v10.477 fixed every read and read-and-clear code (BC's reads, BA/BB, B8/B9, C8/C9, 6C-6E, 75-77, A4, A8/A9, AC/AD, B0/B1, BE/BF, C0/C1), which had made the next packet resend its whole state in the middle of a measurement.
- The VxD's times (PT()) read 0 unless its profile is on (TEXDBG 6C on, 6D off, 6E reads).
- A windowed test must size its window for the back buffer's client (AdjustWindowRect): a 512 x 512 back buffer in a 514 x 515 client is a CPU stretch at Present (31 ms a frame).
- Read the ring's wait histogram with BA, never BB, which clears it.

### 24.14 The render-target texture copy without the wait (v10.477-v10.478)

A Blt into or out of a render-target texture no longer waits for the engine first when ACCEL\_D3D\_RTNOWAIT is 1 (the default; HAL switch 8, DriverInit bit 12): the copy goes to the VxD, whose ring orders it after the draws already sent. 0 restores v10.456's wait.

**What GTA3 does.** Once a gameplay frame it copies the back buffer into a 640 x 480 render-target texture through Direct3D 8's CopyRects (GTA3.EXE+EC3D and +AEC53). The Blt arrives with flags 01020000, destination caps 10007000, source caps 1000601C. On v10.476 it took 2.94 ms each; the copy itself, written into the ring (ACCEL\_D3D\_RINGBLT, 24.7), takes 14.7 us. The rest was v10.456's `access(0)`: a drain of every draw in flight before any Blt touching a render-target texture.

**Why it is safe.** The ring copy (VX3DN\_ring\_copy, VX2D\_RT requests) runs in order after the draws that made the back buffer and before the draws that sample the texture. When the ring can't take it (the ring off or dead, another ROP or format, a key, overlapping surfaces, the software cursor's surface, the old 3D path busy), the 2D path first retires the 3D work itself (VX3D\_busy, cause 1), so the order holds either way. A CPU access later still retires the ring as before.

**Proof.** VXRTT v1.2 on the board: with the switch on, one retire a frame fewer (1.0 against 2.0) and the sampled quad right in all 132 frames. The linked test checks the order of IOCTLs: switch on, VX2D\_SUBMIT with no VX2D\_ACCESS before it; switch 0, VX2D\_ACCESS first.

**Testing it.** A test of this wait must not read the back buffer in its timed frames: the read retires everything drawn, so a removed wait only moves to the read. A loop where the engine is the limit shows no gain either. VXRTT v1.3 times 30 frames without a read-back, then checks 3 frames. It runs two loads, no CPU work and 6 ms of CPU work a frame after the copy (a game's logic while the engine draws). It prints the CopyRects call, the HAL's Blt into the texture (class copy to a 3D target, index 9) and Present's copy apart, the retires by cause, and the ring copies (TEXDBG BC B7: in the ring, not taken, by the 2D engine after the wait). VXRTT v1.2's "copy's Blt" averaged every Blt of the pass, Present's included.

**Board results** (v10.477-v10.479). GTA3: the per-frame copy into the render-target texture fell from 2.94 to 0.43 ms a Blt; the in-run A/B with ACCEL\_D3D\_RTNOWAIT off less on gave +2.47 ms a frame \[+2.47, +2.49\] over 12 turns. Of 4,453 copies, 316 were not taken by the ring and went through the 2D engine after a retire (2.46 ms each): still open. VXRTT v1.4: CopyRects 0.36 ms with the switch against 2.82-2.87 ms without (the HAL's Blt 0.030 against 2.47). With 6 ms of CPU work a frame it ran 15.0 against 17.5 ms a frame; without CPU work, 11.4 against 11.5 (the engine is the limit). Every checked frame was right, and all 30 copies a pass went into the ring. Present's windowed copy to the front takes 7.9 ms, because it waits for the frame's draws.

**Fixes in v10.479** (a review of v10.477, each finding confirmed in the source):

- A DP2 SETRENDERTARGET refused after its wait left the render-target texture as the target but cleared its mark, so the next change skipped the wait for draws made into it. The mark now follows the change only once the core has made it.
- That wait's lost device was ignored. Both SetRenderTarget paths now answer DDERR\_SURFACELOST with the target unchanged.
- ACCEL\_2D 0 did not stop a render target's copy into a texture from going into the ring (a 2D engine copy). It is now declined like every other 2D request, and the HAL copies it.
- With a flip possibly pending and the VxD's flip call failing, GetFlipStatus answered DD\_OK. Write Locks and Blts into the old front now answer DDERR\_SURFACELOST.
- TEXDBG's read codes no longer start the 3D state cache over.
- VXRTT v1.4 clears the back buffer before the sampled quad, so its check proves the copy (v1.3's pixel already had the frame's colour).

## 25. What XP's driver code says: clears, the stencil plane, Coarse Z, state upload and waits (static analysis, reports v04-v46)

A series of 43 static reverse-engineering reports (v04-v46, October-November 2026) read VIA's XP driver for this chip to explain the D24S8 stencil fault of section 17 and to compare XP's design with this driver as of v10.386. This section keeps what a builder can use, mapped onto the registers this cookbook already proves, and says where each item stands now.

### 25.1 Sources and how far to trust them

The files are VIA's XP set from dc06emb.inf: S3gIGP.dll (DirectDraw/Direct3D display driver), s3ginv.dll (the "CIL" core: state, command queue, resource layout), S3gIGPm.sys (miniport), s3gIGPgl.dll (OpenGL ICD) and ucb\_32.sys. Addresses below are the files' preferred virtual addresses (relocation moves them at run time). Offsets such as +0xA0 are **software structure offsets** unless this section maps them to a register.

| Kind of claim | What it means here |
| --- | --- |
| Code | Read from XP's instructions: static, never traced running |
| Mapped | Matches a register or value this cookbook proves on the board |
| Derived | Arithmetic from the recovered formulas, not measured on the chip |
| Open | Not established; listed in 25.7 |

Nothing in the reports was run on XP or on the board; host-side tests only checked that the models compute the recovered arithmetic. Anything here becomes a recipe only after a board test, as everywhere else in this cookbook.

### 25.2 XP's path from DrawPrimitives2 to the engine

```text
S3gIGP.dll   DrawPrimitives2 callback 0x2AA20 -> DP2 opcode dispatcher 0x37F60 (96-entry table)
     |        (calls the core through tables installed by cilSetCoreFunctionTables)
s3ginv.dll   handlers and command encoders (e.g. 0x118350)
             queue reserve / submit 0x7D1A0 / 0x7D1F0, queue methods 0xBD440 / 0xBD5C0
             conditional transfer 0xBD9A0 / 0xBD8F0, interpreted transfer 0xBCD30
     |
     v       the ring (section 23-24), or register writes on the interpreted path
```

The core keeps a 0x700-byte device object and a separate 0x12D0-byte device record (pointer at object +0x0C). For PCI 1106:5122 the core picks **backend selector 4** in a 0xB0-stride method table at 0x1FC618; the chosen table (0x1FC8D8) holds the clear method 0xAC9C0 at +0x20, the pitch method 0xB5830 at +0x44 and an initialiser 0xB4D20 at +0x48. Device-family dispatches elsewhere use family **13** for this chip.

**Command packets.** The interpreted transfer 0xBCD30 reads a dword stream whose header words' high half names the packet family, and pads each packet to 16 bytes:

| Family | What the interpreter does |
| --- | --- |
| FE00, FE05, FE0A | (offset, value) pairs: one register write each |
| FE01 | A payload streamed through destination +0x200000, fixed or increasing offset |
| FE02 | The header's control value to its encoded port, then data words until the next recognised header |
| FE03 | A control word, then a counted run of data words to the companion data port |
| FE04 | Repeated writes to one port |
| FE06, FE07 | No index advance seen in that branch (a possible stall in the interpreter, not shown to happen) |

On the FE02 path the control word goes to **0x43C** and the data to **0x440**: the same type-select / register-write ports as section 5 (0x43C = type << 16, 0x440 = index << 24 | value). A typical generic-state header is `FE020440 0000043C 00010000 CC000000`, which selects type 01 (render state) through 0x43C; `CC000000` is alignment padding, the same word that closes this driver's batch end (`FE020420 0000041C 00300000 CC000000`, section 23.2). The FE02 look-ahead classifier 0xBA970 knows FE00-FE07 but not FE0A, and the queue transfer 0xBD9A0 skips or reroutes some ranges by mode: a captured stream is not a plain list of register writes and cannot be replayed blindly.

### 25.3 Clears, the stencil plane and Coarse Z in XP's code

**The clear path.** DP2 CLEAR (opcode 0x2A, S3gIGP 0x39561) → 0x355C0 → the core's clear entry 0x1A3F30, which strips a private bit 0x08000000 and calls the backend's clear method, 0xAC9C0 for backend 4. Clear flags: colour 0x01, depth 0x02, stencil 0x04 (0x08 selects a rectangle-count path); each rectangle is validated, clipped and cleared on its own.

**XP's defaults** (dc06emb.inf): HalcyonEnableFastZClear 0, HalcyonTiledZWEnable 0, HalcyonSeparateStencilEnable 1, HalcyonCoarseZEnable 1. Other configuration can override them at run time.

**Three linked surfaces.** A depth/stencil surface (private format 0x2033 for D24S8) points to a separate 8-bit stencil surface (format 0x0805, at +0x2D4) and to a Coarse Z surface (format 0x2032, at +0x2D0, allocation flag 0x8000). The stencil surface's size is the parent's padded to multiples of 8. They are separate allocations with separate gates: exactly section 17's layout (plane, then Coarse Z storage), which this driver places in one block after the Z words.

**The shadow fields are the type-01 registers (mapped).** XP's renderer keeps a shadow of the state it sends. Every field the reports tied to the stencil plane and Coarse Z sits at shadow offset = 4 × a type-01 register index this cookbook proves, with the same encoding:

| XP shadow field | Encoding in XP's code | Type-01 register (section 17) |
| --- | --- | --- |
| +0x48 | stencil plane mode / format bits (5-6) under a capability gate | 01/12, the depth format (6 << 16 = the stencil layout) |
| +0xA0 | stencil surface's GPU address >> 8, low 24 bits | 01/28, the plane's offset >> 8 |
| +0xA4 | (Coarse Z pitch >> 5) & 0x1FF | 01/29 bits 8:0, the Coarse Z pitch in 32-byte units |
| +0xA8 | Coarse Z surface's GPU address >> 8 | 01/2A, the Coarse Z storage's base >> 8 |

So XP's shadow is very likely an image of the register file, 4 bytes per register, in banks of 32 (25.5). The reports warned against equating the two; four of four matching fields with matching encodings is strong evidence, but it is still an inference.

**Coarse Z initialisation.** XP fills the Coarse Z storage with 16-byte records `FFFFFFFF FFFFFFFF v v`, through the engine (0xAC490, in about 4 KB pieces) or the CPU (0xABC40). This is the code behind section 17's capture: `FFFFFFFF` fill for a far clear, `FFFFFFFF FFFFFFFF 10001000 10001000` for a clear to 0.25. Binding it sends a six-word packet (0xC9510) and dirty-state id 0xCA; nothing in the code is a cache flush beyond 01/29's tag reset.

**Stencil clears.** A rectangle covering the whole padded stencil surface goes through a generic clear/transfer (0x1AE480): an engine fill, as this driver's whole-plane 2D fill. A partial rectangle first waits for the engine (wait selector 9, 25.6), then writes the bytes with the CPU through the surface's CPU pointer (+0xF4) in the 8×8 tile order (25.4), as this driver's scissored stencil clear does. XP's partial clear checks the full-clear test against the **padded** stencil size, so a clear of the whole visible surface whose width is not a multiple of 8 takes the CPU path. Specialised stencil copies use FE00 packets and wait selector 0.

**Where this leaves section 17.** The reports set out to explain the 8×8-tile fault (VXSTEN case 2). Section 17 had already found and fixed it in v10.294: Coarse Z was on (01/01 2200 copied from a capture) over storage nobody had reserved. XP's code agrees on every point: a separate plane, separate Coarse Z storage that must be allocated and initialised with those records before Coarse Z is used, and the same registers. Coarse Z stayed off on this board because it was slower (v10.299); its code was removed in v10.430 (24.5).

### 25.4 Geometry: the tile index, pitches and blocks

**XP's stencil byte address** (the partial clear's CPU loop), for a stencil surface whose stored width `aux_width` is padded to a multiple of 8:

```c
index = (y >> 3) * (aux_width << 3)   /* a row of tiles */
      + (x >> 3) * 64                  /* 8x8 tiles of 64 bytes, left to right */
      + (y & 7) * 8 + (x & 7);          /* rows of 8 inside a tile */
```

This is section 17's `at(x,y)` with one difference: XP multiplies by the stored **width** (`+0x140`), this driver by the plane **pitch**. XP keeps width and pitch in separate fields (pitch in +0x14C / +0x15C, an effective width in +0x150), and its code never uses the pitch in this index.

**XP's pitch rule** (0xB5830): bits per element from the format, `width × bits / 8` rounded up to **32 bytes** in the ordinary case; flag 0x10000 (checked first) and flag 0x8000 select other roundings. This driver rounds the plane pitch to 16 (section 17).

**XP's allocation blocks.** The device record's initial layout tables (record +0x5DC / +0x5E0 / +0x5E4, four selector slots of 24 bytes each) give 256-byte blocks of 32 bytes × 8 rows for every depth. The derived base size is `ceil(width × bpp / 8 / 32) × ceil(height / 8) × 256` (640×480 at 8 bpp: 307,200 bytes; 648×480: 322,560; 640×480 at 32 bpp: 1,228,800); the allocator adjusts it further in some branches, so these are not final sizes. The allocator aligns the GPU offset and moves the CPU pointer by the same amount; a surface keeps both (CPU pointer +0xF4, GPU offset +0xF8; the GPU one is what goes into 01/28 and 01/2A).

**Where the two rules differ (derived).** They agree whenever the width is a multiple of 32, which covers every surface the board tests used (256 wide for the one-pixel proof; 640, 800, 1024 windows). Otherwise they can differ:

| Width | This driver: pitch, tile-row stride | XP: stored width, CPU tile-row stride | XP: pitch |
| --: | --- | --- | --: |
| 640 | 640, 5,120 | 640, 5,120 | 640 |
| 400 | 400, 3,200 | 400, 3,200 | 416 |
| 641 | 656, 5,248 | 648, 5,184 | 672 |
| 648 | 656, 5,248 | 648, 5,184 | 672 |
| 656 | 656, 5,248 | 656, 5,248 | 672 |

What the engine uses as its tile-row stride for such widths is not known: XP's CPU stride (the width padded to 8) and its pitch (rounded to 32) already differ from each other at 648. The test that settles it is section 17's one-pixel proof (VXDX7 --splane: one engine REPLACE pixel, one CPU byte found by EQUAL) on a D24S8 surface 648 and 656 wide. Until then, a stencil plane whose width is not a multiple of 32 is unproven.

### 25.5 State upload: dirty masks and the full replay

**Incremental upload.** XP marks changed shadow dwords in eight 32-bit dirty masks: `shadow offset = group × 0x80 + bit × 4`, so with 25.3's mapping a group is 32 type-01 registers and a bit is one register (stencil mode +0x48 = group 0 bit 18 = 01/12; stencil base +0xA0 = group 1 bit 8 = 01/28). 0xBE3A0 collects the masks, 0xC2E40 copies the marked runs, 0xC2F80 builds the packets, 0x7D1F0 submits, 0xBE000 clears the masks. This is the design this driver's state cache approximates by comparing whole packets (STATE\_DIFF, a word-level diff, cost more than it saved: 24.5).

**Full replay** (0xC0870, taken when the submission mode is 1, 0xC304B). It sends the stencil and Coarse Z fields even when not dirty; for family 13 its leading payload has, in order, +0x9C, +0xA0, +0x48, +0xA8, +0xA4 (01/27, 01/28, 01/12, 01/2A, 01/29 by the mapping). Then three loops of 16 banks each, which match section 5.1's selector words:

| XP control series | Section 5.1 meaning | Words a bank (family 13) | Total |
| --- | --- | --: | --: |
| `0x00020000 \| (i << 24)`, FE02 | texture unit i attributes | 11 | 176 |
| `0x20020000 \| (i << 24)`, FE02 | sampler stage i | 5 | 80 |
| `0x10030000 \| (i × 8)`, FE03 | sampler constant block i | 8 | 128 |

384 data dwords in all, before headers, padding and the other blocks: a 17-dword block with control `0x00040000` (type 04, vertex/primitive set-up), a 23-dword `0x00FE0000` block (type FE, front-end configuration; this driver sends 16 fixed words there), FE03 tail blocks, a 20-dword constant table sent with `FE03043C 00000014 12030000` when a Coarse Z field is non-zero, and an optional 32-dword table with `0x11030000` chosen by a rendering record's mode (4 → table at 0x2EF10, 5 → 0x2CF10; copied to shadow +0x1FE0, enabled by +0x3C60). The meanings of the 0x11 and 0x12 FE03 banks are open. Rendering records are `context + 0x22BC + index × 0xF8` (mode A +0x48, mode B +0x4C, resource +0xE8); three of their fields (+0x34 / +0x38 / +0x3C) are clamped to 1-6 and translated through a six-entry table into a 9-bit state field. None of this is shown to be a stencil or Coarse Z cache control. Do not send the replay or the two tables without knowing what they change.

### 25.6 Waits, mapping and CPU write ordering

**XP's engine waits** are one service (0x1128C0) with a selector:

| Selector | What it does |
| --: | --- |
| 0 | Polls 0x400 until `status & 0x101FF3` = 0 (bounded), then an optional host callback |
| 4 | Polls +0x13D0 until bit 0x10 clears (no timeout of its own) |
| 7 | Polls +0x13D0 until bit 0x8000 clears (no timeout of its own) |
| 9 | The miniport's callback (S3gIGPm 0x313B0, installed at 0x2DC84): `status & 0x101FF3` = 0, polled on a 1 ms timer with a 120-second limit; it can fail, and the partial stencil clear writes its bytes without checking |
| 1-3, 5, 6, 8 | Return at once |

0x101FF3 is the idle mask this driver uses (fetch\_idle, the ring's wait; the engine reset waits for 0x101FF7, adding bit 2). What +0x13D0 holds is not recovered. Do not copy XP's untimed loops or its 120 s limit into a VxD: an engine that never idles must not hold the system (this driver's waits stop at 100 ms and recover).

**Mapping.** S3gIGP.dll maps video memory through EngDeviceIoControl 0x230474 (miniport 0x2BDC0 → VideoPortMapMemory; the CPU address is returned at +0x08). The miniport asks for write combining (VIDEO\_MEMORY\_SPACE\_P6CACHE, flag 8 in extension +0x384) unless capability bit 0x200 at +0x68 is set. Surface memory then comes from a video-memory pool suballocator (core global 0x324F80 → S3gIGP 0x67DF0, free 0x67B70), not a new mapping per surface.

**Write ordering.** A wait for the engine before CPU writes orders the engine's earlier work against them; it does not make the CPU's later write-combined stores visible before the next engine read. XP's partial stencil clear returns without a fence, and none of the three main XP modules contains SFENCE, MFENCE or LFENCE; whatever orders XP's writes (later locked instructions, register writes, memory types) is not shown. This driver does it explicitly: `publish()` (a locked xchg) after CPU writes the engine will read, SFENCE and a read back before every fetcher start or kick (ACCEL\_D3D\_WCFLUSH, 24.2), and MOVNTI + SFENCE or CLFLUSH for the ring (24.1). A CPU read of write-combined memory also drains it, so a test that reads the plane back before drawing can hide an ordering fault.

### 25.7 What the reports proposed for this driver, and where each stands

The last report (v46) compared XP's design with this driver's v10.386 source. It found the idle mask (0x101FF3, several idle reads), the 8×8 plane addressing, `publish()`, the plane registers and Coarse Z (off by default) already in place, and proposed two changes:

| Proposal | What it is | Status |
| --- | --- | --- |
| A: publish after the CPU plane clear | A clear that fills Z with the 2D engine and then writes the stencil plane with the CPU returned without draining the CPU's writes | Done in v10.391 (`publish()` after `vx3dn_plane_clear` in that branch) |
| B: clears and occlusion queries | `VX3DN_submit` prepares a packet for the occlusion count with `q_before_clear()` only for a draw flagged as a clear; a real `VX3DN_CLEAR` goes through `q_before_draw()`, which hands the count back to the query's owner, so an engine-drawn clear on the owner's thread could be counted | Done in v10.456: a `VX3DN_CLEAR`, single or in a batch, goes through `q_before_clear()`; checked (native3d.py): a clear on the query owner's own thread is left out of its count |

The reports' VXSTEN test plan (wait-only, rebind-only, publish-only, read-back runs) was written for the 8×8 stencil fault, which section 17 fixed in v10.294; it is not needed. Two of its ideas still apply: the width test of 25.4, and checking a whole-surface stencil clear at a width that is not a multiple of 8 (641 against 648: the same padded plane, but XP sends 641 down the CPU path because the visible size is smaller than the padded plane).

**Open, not recovered:** what the engine uses as the plane's tile-row stride at widths that are not multiples of 32; the meaning of +0x13D0 (wait selectors 4 and 7); the FE03 banks 0x11 and 0x12 and the two 32-dword tables; FE06 / FE07 handling; the hardware meaning of most full-replay words; why XP's capture T50 drew right with Coarse Z's optimized merging rule when this driver's did not (section 17); XP's cache attributes for surfaces at run time. The reports did not cover texture, shader, rasteriser or OpenGL internals.

## 26. OpenGL on the CPU: Lattice from 572 to 348 ms a frame (v10.480-v10.493)

The OpenGL ICD's per-vertex and per-triangle work on the CPU is what limits lit OpenGL scenes on this board; the engine is idle most of a frame. The test case is the Windows 98 OpenGL screen saver Lattice: flat-shaded, lit triangle strips from display lists (33 vertices a strip, about 1,350 draws a frame), a positional light with a local viewer and specular, exponential fog, back faces culled, textured, half of the strips crossing the window's edges and half the draws wholly outside. Every change below is on by default with a registry value under `HKLM\Software\VX85598` to turn it off, draws the same pixels as the path before it (VXFLAT, 26.1), and was measured on the board.

| Driver | Lattice, ms a frame | What changed |
| --- | --- | --- |
| v10.493 | not yet measured | the pass's bookkeeping tightened; the culling run's area from kept floats; Mesa's inputs bound in place |
| v10.492 | 348 | flat strips without the copy; the per-draw set-up kept; SSE lane masks |
| v10.490 | (not run; v10.491 fixed it) | bounding box, SSE specular, byte packing, culling run |
| v10.489 | 391.6 | a draw wholly outside skipped before lighting; linear fog by SSE |
| v10.488 | 534 | the SSE pass for every draw, spotlights (slower: it lit draws that were wholly outside) |
| v10.486 | 448 | the lighting loop for all of Mesa's general case |
| v10.485 | about 300 (textured run, 3.3 frames a second) | strips split at the window's edge |
| v10.484 | 481 | rsqrtss for the lighting |
| v10.483 | 475 | flat strips as the engine's strips (rule 1), float-to-int without \_\_CHP |
| v10.482 | 543 | the engine's flat shading on by default |
| v10.480 | 538-572 | the starting point (flat strips a triangle at a time through Mesa's flat template) |

The v10.485 run was a different Lattice mode (10 vertices a strip); the others are the default mode.

### 26.1 Measuring: the sampler, VXGL.LOG, VXFLAT and the traps

**The sampler** (`VXGL_SAMPLE` 1). A thread suspends the drawing thread at a fixed rate and records its EIP, up to 32,768 samples a window; VXGL.LOG lists the share by module, then VXGL.DLL's addresses in 32-byte buckets as `VXGL+RVA count`, busiest first. Up to v10.491 only the busiest 120 buckets were listed, which left about 45% of Lattice's samples unnamed and hid Mesa's per-draw cost; from v10.492 every bucket with 2 samples or more is listed (up to 1,500 lines) with the total listed. To name a bucket, take the link map (VXGL.map) and the object order of the link (vxgl.lnk): a module's base is its first symbol in the map less that symbol's offset in the object's disassembly (Watcom `wdis`), and a static function is found by its label in the disassembly. Per-function shares then come straight from the buckets.

**VXGL.LOG at a context's end** carries the counts behind each path: flat strips by kind (VXGL\_FlatStats10), strips split at the edge (VXGL\_ClipStats), the lighting runs by function, Mesa's primitives by kind with their clipped calls, the SSE pass's draws, vertices, draws wholly outside, draws left to Mesa with the last reason bits (VXGL\_SsePassStats16), and each later part's count and switch.

**VXFLAT** (MIT, in the build) runs itself once per configuration as a child process, each with its settings in the environment (VXGL.DLL reads every ACCEL\_OPENGL\_\* value from an environment variable of the same name before the registry). Each child draws 21 scenes at 320x240, 6,048 triangles a frame (strips of one colour, quad strips, arrays, lit strips with one normal or a normal a vertex, lit with fog and separate specular, triangles, quads, fans and polygons with a colour a vertex, a positional light with exponential fog, strips crossing the window, three lights, then the same as display lists, a list drawn twice with one copy beside the window, and folded lit strips with front faces culled; since v2.8 two textured scenes drawn as Quake 2 and Quake 3 draw, src/vx\_qscenes.h). One fixed frame of each scene is read back; then the scene moves for 100 frames in three runs, and its rate is the median run's (runs agree within 0-1%). Every frame is timed; frames over twice the median are listed with their longest VxD call. The children run with VXGL\_LOG, VXGL\_SAMPLE and VXGL\_FRAMELOG off: the ICD's sampler had cost 6-22% in the lit scenes and made results depend on code layout, and the 30 ms frames every 200 swaps were VXGL.LOG's own report. Profile with the sampler, measure without it. The parent compares every configuration with the template (v10.479's path) pixel for pixel: 0 apart is required, except for configurations whose arithmetic is meant to differ, which may differ by at most 1 in a channel. It checks each path's counts against what the scene must produce (for example, every lit list draw by the SSE pass, every unlit one left to Mesa). Each switch gets a configuration that turns it off, so a feature's gain is the default against that configuration.

**Traps.**

- The first configuration's first scenes often run 10-20% slow (a warm-up): VXFLAT runs the default again last and marks scenes more than 10% apart. Compare with the second default run.
- The screen saver (Lattice itself) started during long VXFLAT runs and shared the CPU and the engine with it, skewing the frame rates but not the pictures. From VXFLAT v2.1 the screen saver and the monitor's power saving are off for the run (SystemParametersInfo SPI\_SETSCREENSAVEACTIVE, SPI\_SETLOWPOWERACTIVE 0x55, SPI\_SETPOWEROFFACTIVE 0x56) and put back after.
- Unrelated scenes vary by up to 10% between configurations that draw them the same way, so trust a gain of a few percent only when it repeats across several scenes.
- A count is needed for every new path. v10.490's snapped words were made by the pass but never taken by the strip path; only the count (0 in every scene) showed it.

### 26.2 Flat shading on the engine: strips, rule 1, the colour two on

The engine has its own flat shading: XP's flat triangle mode word **04/23 = 9E02** (CLXP3D T48 against T49's CE02, the only state that differs) colours each triangle from its **first** vertex, every channel and the secondary colour's RGB, while each vertex keeps its own fog. GL colours a flat triangle from its **last** (provoking) vertex. Mesa's flat template copies the provoking colour onto the other two vertices, draws the triangle and puts the colours back, one triangle a call; Lattice spent most of its core time there.

1. **One colour** (ACCEL\_OPENGL\_FLATSTRIP, v10.480): a flat strip whose vertices all carry the first one's colours (primary, and the secondary with its fog byte) goes to the core as one strip. One-colour strips 107 frames a second against 46.
2. **Independent triangles** (ACCEL\_OPENGL\_FLATBATCH, v10.481): a strip whose colours differ goes as independent triangles, 64 a call, each turned to start at its provoking vertex with the same winding (p a b), no colour copies.
3. **The engine's flat shading on** (ACCEL\_OPENGL\_HWFLAT, v10.481, default 1 from v10.482). The core neither copies colours nor makes its own strips under it, because the first-vertex rule was first proven for lists only. The Direct3D HAL never sets it: D3D flat stays the colour copy.
4. **Strips under flat shading** (ACCEL\_OPENGL\_HWFLATSTRIP, default 1 from v10.483). On the board, each strip triangle takes the colours of its **first strip vertex** (rule 1; rule 2, the last strip vertex, drew other pixels). So the vertex at strip position k must carry the colours of position k + 2, the provoking vertex of the triangle that starts at k; the last two positions keep their own (no triangle starts there). A quad strip goes pair-swapped (v1 v0 v3 v2 ...), and position k takes the colours of position (k & \~1) + 2, the quad's last vertex. Lit strips +33 to +36%.
5. **Without the copy** (ACCEL\_OPENGL\_FLATNOCOPY, v10.492). Up to v10.491 the ICD made rule 1's colours in a copy of every vertex (chunks of 512, overlapping by two). Now the core's strip run reads each vertex's colour word from the vertex at k + 2 (or (k & \~1) + 2), and with a secondary colour that vertex's specular RGB too, keeping the fog byte its own. A triangle the run does not take goes through the general path with the store's own colours, turned to start at GL's provoking vertex. A strip sent in chunks joins no run across them: the run's last two vertices took their own colours. +2 to +4.5% on the lit scenes.
6. **Other flat primitives** (v10.483): triangles, quads, fans and polygons, the clipper's polygons too, go to the engine's flat shading as triangle lists, each triangle turned to start at its GL provoking vertex: triangles (j, j-2, j-1); a fan (j, start, j-1); a polygon (start, j-1, j); a quad a b c d as (d, b, c) and (d, a, b). +12 to +13%.

**Trap found on the way** (v10.483): once a batch could hold the ICD's flat strip runs and list triangles together, list vertices added after a run went uncounted and the VxD refused the whole packet. Vertices joining a batch that has runs are now always counted, and a batch's first run counts the list before it.

### 26.3 Mesa's stages made faster

These still apply to every draw the SSE pass (26.4) does not take, and they are where the pass's arithmetic rules come from.

- **Whole-vertex emit** (ACCEL\_OPENGL\_EMIT, v10.481): one loop a vertex for the ICD's layouts with fog and the secondary colour (position, colour, \[secondary\], \[fog\], \[one 2D set\]) in place of Mesa's generic emit, which made one call per attribute. Mesa's emit, lighting and fog are compiled with Watcom inlining (`-oe=400`).
- **Float to integer without \_\_CHP** (ACCEL\_OPENGL\_FASTINT, v10.483). Watcom's float-to-int call sets the FPU to chop and back each time. The specular table lookup (2 a lit vertex), the spot lookup and the exponential fog table now round by the sum f + 1.5×2^23, take the integer from the sum's bits less 0x4B400000, then step back by one where that went past f. This is exact truncation for |f| < 2^22 in any rounding mode; outside that, or for a NaN, the plain cast is used. +4% in the positional scene.
- **Square roots and divisions by SSE** (ACCEL\_OPENGL\_RSQRT, v10.484). The lighting's normalizations and a positional light's distance use rsqrtss and one Newton step, s = ((3 - r²x) r) × 0.5. The attenuation's division uses rcpss and one step, (r + r) - r²x. Both are within 4 units in the last place, and only for x in 1e-30..1e30, so no SSE exception but inexact can occur; otherwise the x87 is used. With no attenuation, the constant's reciprocal replaces the division. It measured no faster on its own, but the pass uses the same steps.
- **One lighting loop** (ACCEL\_OPENGL\_LIGHTFAST, v10.485-v10.486). All of light\_rgba's case (RGBA, no separate specular, one side; any lights, local viewer or not, colour material or not) runs in one straight loop: the light's values in locals, the compares done on the floats' bits, and the same arithmetic in the same order as Mesa. +2%.
- **Strips split at the window's edge** (ACCEL\_OPENGL\_CLIPSTRIP, v10.485, in t\_vb\_render.c). A strip with a vertex outside is split into runs of triangles with no vertex outside, each starting at an even place so winding and parity match the whole strip; each run goes to the strip path. A triangle inside at an odd place before a run goes alone. Every other triangle goes through Mesa's clipping table (dropped when outside one plane, else clipped). The clipper's flat polygons from a split strip are gathered and sent in one call before anything else is drawn. Clipped lit strips +29%.

### 26.4 The SSE pass: one pass a vertex (vxgl\_ssepass.c, v10.487-v10.500)

**Where it sits.** The pass is the first stage of the ICD's tnl pipeline. When it takes a draw, it does the whole vertex side four vertices at a time: transform, clip test, projection, lighting, fog and the vertex emit. It then calls the render stage itself (with BuildVertices made a no-op for that call) and returns GL\_FALSE, so Mesa's vertex, normal, lighting, fog, texgen and texture-matrix stages do not run. Anything it does not take gets GL\_TRUE and Mesa's pipeline as before. Its outputs are exactly what the pipeline leaves for the render stage:

- the clip coordinates in VB->ClipPtr, with room for the clipper's new vertices;
- ClipMask, ClipOrMask and ClipAndMask;
- the engine vertices in the ICD's store: x y z rhw, BGRA bytes, \[pad + fog byte\], 2-float coordinate sets.

So the strip paths, the clipper (whose interpolation reads the store and ClipPtr) and the flat paths work unchanged.

**Inputs in structure-of-arrays form.** A display list's vertices are copied at glEndList (in vbo\_save\_api.c, the node's vx855\_soa) into 16-byte aligned arrays x\[\], y\[\], z\[\], nx\[\]..., r g b a, s\[\] t\[\] for up to three sets. The count is rounded up to four, and the padding lanes repeat the last vertex. A list with any other attribute (weights, secondary colour, fog coordinate, edge flags, materials, generic attributes) gets no copy. A draw not from a list (glBegin/glEnd, glDrawArrays without indices; ACCEL\_OPENGL\_SSEIMM) has its vertex-buffer inputs copied into the same form first. An input given once (stride 0) is spread into every lane for normals, and kept as the pass's constant for colours and coordinate sets.

**What it takes** (else the reason bits, VXGL.LOG):

- the engine drawing (no fallback), no vertex or fragment program, no user clip planes, no attenuated points, no index buffer, the list's node in one vertex buffer, positions of 2 or 3 floats;
- lighting off (only with ACCEL\_OPENGL\_SSEUNLIT 1), or light\_rgba's kind: eye coordinates, one side, no separate specular colour, spotlights allowed, no material a vertex;
- colour material either with the list's one colour (applied once), or with colours that differ, worked out a group at a time (ambient, diffuse, emission tracked; a tracked specular or shininess goes to Mesa);
- fog off, or GL\_FOG from the fragment depth (the engine's vertex fog);
- 2-float coordinate sets with no texgen and identity texture matrices.

Mesa's fast lighting (a directional light, no local viewer) is not this case and stays with Mesa (reason 0x20); a separate specular colour gives 0x220.

**Arithmetic.** The same operations in the same order as the C code they replace, each rounded to single precision by SSE:

- transform ((m0 x + m4 y) + m8 z) + m12;
- clip bits from w < c and w < -c;
- 1/w by a true divide;
- normals by the inverse's rows, normalized by rsqrtps and one Newton step;
- the attenuation's reciprocal by rcpps and one step;
- compares on the floats' bits as light\_vx855.

The x87 keeps intermediates longer, so a colour channel may differ by 1. A group with a lane the SSE steps do not cover is done again by a scalar reference with Mesa's arithmetic: a vertex at a positional light's position, a length outside 1e-30..1e30, a normal of length 1e-20 or less under GL\_NORMALIZE, or a NaN where a compare decides. MXCSR is set to 0x1F80 for the pass, and only SSE1 is used.

**Parts added after the first pass, each with its switch:**

- **Whole outside first** (ACCEL\_OPENGL\_SSEOUTSIDE, v10.489). As Mesa's vertex stage, the clip masks are made first (transform and compares only, a group at a time, stopping at the first group that leaves no plane all vertices are outside of). A draw wholly outside one plane is skipped before material and lighting. Lattice: half of its draws. Without this, the pass was slower than Mesa (534 against 448 ms).
- **Bounding box** (ACCEL\_OPENGL\_SSEBOX, v10.490). The list's position box is kept with its copy (none if a coordinate is not finite or exceeds 1e30). The plane value x - w (and the other five) is affine in the position, so over the box it is least at a corner. Compute each corner's clip coordinates in double. Let E\_r = 8 × 2^-24 × (|m0r| A0 + |m4r| A1 + |m8r| A2 + |m12r|), with A the box's largest |coordinate| per axis, as the bound on the single-precision transform's error for row r. If every corner's value exceeds 2 (E\_j + E\_w), the pass's own compares would find every vertex outside that plane. Otherwise the vertices are tested as before. Lattice: 520,315 of 599,928 outside draws found this way.
- **Specular lookup by SSE** (ACCEL\_OPENGL\_SSESHINE, v10.490). f = n.h × 255; t = (f + 1.5×2^23) - 1.5×2^23, less 1 where t > f, gives floor(f) as a float; floor + 1.5×2^23 gives the integer from its bits less 0x4B400000. The two table entries are gathered a lane at a time; tab\[k\] + (f - floor) × (tab\[k+1\] - tab\[k\]) is computed by SSE. A lane with f of 255 or more uses \_mesa\_pow, as Mesa does. Then c += spec × the light's material specular (0 in other lanes). +2 to +3.6% in the specular scenes.
- **Colour and fog bytes without a branch** (ACCEL\_OPENGL\_SSEPACK, v10.490). UNCLAMPED\_FLOAT\_TO\_UBYTE for four lanes: x = max(v, 0); where x >= 0.99609375 (IEEE\_0996, bits 0x3F7F0000) use 1.0; then x × 255/256 + 32768, whose low byte is the result. A group with a NaN uses the scalar rule (a NaN with a clear sign gives 255 there). The bytes are stored as one BGRA word a vertex. Neutral on the board, exact.
- **Linear fog by SSE** (v10.489): f = clamp((end - |z|) × 1/(end - start), 0, 1), with maxps/minps (a NaN gives 0, as Mesa's CLAMP).
- **Per-draw set-up kept** (ACCEL\_OPENGL\_SSECACHE, v10.492). The matrices' splats are made again only when a matrix changed (memcmp). The material update, the shine tables and the light splats are redone only when their inputs changed. Those inputs are compared as words: the material (all 12 attributes), the model ambient, local viewer, colour-material bits, and each enabled light's colours, position, directions, cut-off, attenuation, spot exponent. Colour material's update after a draw with differing colours forces a new set-up. Lattice keeps 16% of its set-ups (each list sets its own colour).
- **Lighting masks by SSE** (ACCEL\_OPENGL\_SSEMASK, v10.492): att >= 0.001 and n.VP >= 0 by cmpleps and movmskps; a NaN lane goes to the reference.
- **Clip masks from tables** (ACCEL\_OPENGL\_SSETIGHT, v10.493). vk\_clipbits gives six 4-bit nibbles (w < x, w < y, w < z, w < -x, w < -y, w < -z). Each nibble indexes a 16-entry table of words with that plane's bit in byte v for each set lane v. The six words ORed are the four mask bytes, stored as one word. The vertex size and store offsets are held in locals (Watcom re-read them after every byte stored), and coordinate sets are copied as words.

**Parts added v10.495-v10.497** (each exact; each fused kernel is checked bit for bit against the kernels it replaces, on the DLL's own code under an x86 emulator, with ordinary, very wide, special and random-bit inputs):

- **Fused kernels** (ACCEL\_OPENGL\_SSEFUSE, v10.495). One kernel a phase, the values kept in registers: transform, clip compares, projection, viewport and both stores (vk\_front); the normal's transform and normalization; a positional light's vector and distance; n.VP with its masks and the diffuse term; the half vector, its length and n.h; the specular term and the accumulation. The same instructions on the same values in the same order as the separate kernels, including which operand is the destination (that decides a NaN's payload). Board: 3-10% in the list scenes.
- **Box from centre and half sizes** (ACCEL\_OPENGL\_SSEBOX2, v10.495). Per plane, the box's centre value and its half extent with a 2.000001 error margin: wholly outside, wholly inside, or unknown. Replaces the eight-corner test.
- **Inside every plane: no clip compares** (ACCEL\_OPENGL\_SSEACCEPT, v10.495). A draw the box has inside every plane runs vk\_front\_in, which stores masks of 0 and skips the compares. Board: 2.5-8%.
- **Partial set-up** (ACCEL\_OPENGL\_SSEPARTIAL, v10.495). When only the material changed since the last draw (Lattice: each list sets its colour through colour material), only `_mesa_update_material` for the changed materials runs (the shine tables are validated again only if the shininess changed), and the lights' material products are splatted again. Board: up to 5%.
- **List binding kept** (ACCEL\_OPENGL\_LISTBIND, v10.495, vbo\_save\_draw.c). A list draw leaves the 32 arrays as the last list left them; only the slots it overlaid, and a slot whose current value changed size, are copied again. Within the noise on the board.
- **Box for draws not from a list** (ACCEL\_OPENGL\_SSEIMMBOX, v10.496). The positions' box is made as they are copied into the pass's arrays (vk\_box3: min, max, and |p| >= 1e30 or a NaN, four lanes a group), so glBegin/glEnd and vertex-array draws get the outside and inside tests too.
- **Grid words from the pass's registers** (ACCEL\_OPENGL\_SSESNAP2, v10.497). vk\_front keeps the window x and y; vk\_snap2d / vk\_snap2u turn them into the strip path's 1/16-grid words four lanes at a time (26.5's snap steps), a lane out of range by snapw. Unlike SSESNAP, nothing is read back from the store.
- **Whole-outside test in registers** (ACCEL\_OPENGL\_SSEOUTFAST, v10.497). vk\_xfbits transforms and compares without storing; a 6-bit mask carries the planes every vertex so far is outside of. MXCSR is set once a draw.
- **Early decline** (ACCEL\_OPENGL\_SSEEARLY, v10.497). A lit draw not from a list whose lighting the pass does not take (Mesa's fast lighting, two-sided, a separate specular colour, no inverse matrix) is declined before its vertices are copied into the pass's arrays.

**Left off by default:** unlit draws by the pass (ACCEL\_OPENGL\_SSEUNLIT 1; 4% slower than Mesa's unlit path) and the snapped words read back from the store (26.5; SSESNAP2 makes them from the registers instead, on by default).

### 26.5 The strip path and Mesa's draw path: the culling run, snapped words, binding, triangle lists

The core's strip entry (vx\_d3d\_gl\_strip, vx\_d3d\_core.c) takes a strip's vertex indices into the ICD's store. For each triangle, strip\_fast checks the batch template, the fast conditions and each new vertex (inside the viewport and the depth range, rhw > 0: fast\_v). With culling on, it also checks the area and appends one vertex to the engine strip run (or starts a run with three). A triangle it does not take goes through the general path (primitive), and the run ends.

- **No culling** (v10.323): once a run is going, a loop appends one vertex a triangle with only fast\_v, with no strip\_fast call.
- **The culling run** (ACCEL\_OPENGL\_CULLRUN, v10.490). With culling on, the same loop also computes the new triangle's area exactly as strip\_culled does: in double from the floats, in GL's winding of the triangle. A culled triangle, no area, or a vertex the fast path does not take ends the loop, and that triangle goes through strip\_fast as before (which ends the run). Lattice culls back faces, so before this every triangle went through strip\_fast. List, culled +6 to +9%.
- **Kept floats** (ACCEL\_OPENGL\_CULLRUN2, v10.493): the run keeps its last two vertices' x and y as floats, so each triangle loads only its new vertex. The expression and operand order are strip\_culled's, so the decision is the same.
- **Snapped words** (ACCEL\_OPENGL\_SSESNAP, default 0 from v10.492). Under ACCEL\_OPENGL\_SNAP the core puts every GL triangle's x and y on swrast's 1/16 grid (snapw: round half away at 1/2048, then down, or up for y with the window's flip, to 1/16). The pass can make those words four lanes at a time, exactly: a = |x| × 2048; floor(a) by the 1.5×2^23 sum; add 1 where a - floor >= 0.5; put the sign back; × 1/128; floor or ceiling by the same sum; × 1/16. A lane with |x| >= 2040 or a NaN uses snapw itself. The strip path takes them for the draw's own vertices (the clipper's new ones use snapw). Measured slower than snapw on the board in four of five scenes (reading the store back and storing the words costs more than snapw), so it is off. Kept as a setting.
- **Inputs bound in place** (ACCEL\_OPENGL\_FASTBIND, v10.493, t\_draw.c): a float array's GLvector4f is set in bind\_inputs itself (the fields \_tnl\_import\_array sets), with no call for each of the 32 inputs of every draw.

**Parts added v10.500** for draws the pass does not take (found by profiling, on the host with callgrind, two scenes drawn the way Quake 2 and Quake 3 draw: src/vx\_qscenes.h, VXFLAT's "q2 world" and "q3 arrays"). Each is exact: the host check draws both scenes with all of them on and all off and compares every draw packet's words.

- **Triangle lists in one call** (ACCEL\_OPENGL\_TRILIST). GL\_TRIANGLES on the plain render path (smooth shading, no two-sided stencil, no smoothing, not both faces culled) went a triangle at a time through Mesa's render template, the ICD's triangle function, vertex numbers worked back from pointers by a division, and vx\_d3d\_gl\_prim: about a sixth of the Quake 3 scene. The ICD's own entry in the render table gathers 128 triangles' element numbers and calls vx\_d3d\_gl\_trilist, which draws each exactly as vx\_d3d\_gl\_prim(…, 3, join) would. It also takes the runs clip\_elt\_triangles passes between clipped triangles.
- **Grid words for the whole store** (ACCEL\_OPENGL\_STORESNAP). After Mesa builds a draw's vertices (the ICD wraps BuildVertices), the store's x and y are snapped four vertices at a time with the pass's vk\_snap2d / vk\_snap2u (a lane out of range by snapw) and handed to the core as the draw's grid words, taken back at the render's end. The strip path and the triangle path (fast\_triangle now passes each vertex's number) use them for vertex numbers below the draw's count; the clipper's new vertices still use snapw. snapw had been run twice a vertex, again for every triangle that shared it. tests/sse\_emu.py checks the store words against the DLL's snapw for every input class.
- **Locked-array transform kept** (ACCEL\_OPENGL\_CVACACHE, t\_vb\_vertex.c). Inside one glLockArraysEXT, a glDrawArrays or glDrawElements from client arrays (not a buffer object, not glBegin/glEnd) whose positions are the same array (pointer, stride, size, count) under a bit-identical modelview-projection takes the last run's clip coordinates, projected coordinates and clip masks, still in the vertex stage's store. Not with eye coordinates (lighting, fog, texgen), user clip planes or a vertex program. Lock and unlock each bump a generation, so nothing is kept across locks. Quake 3 draws each locked batch once a shader stage.
- **glVertex's copy as words** (ACCEL\_OPENGL\_FASTVTX, vbo\_exec\_api.c). Watcom compiled Mesa's per-vertex copy into the vertex buffer as a loop that re-read the size and both pointers for every float and moved each float through the x87. It is now 32-bit moves four at a time from registers.
- **Dispatch back to the globals** (ACCEL\_OPENGL\_GLAPIONE, glapi.c). The ICD is built with Mesa's thread-safe dispatch (WIN32\_THREADS). Once a second thread makes a context current, Mesa switches every GL call and every GET\_CURRENT\_CONTEXT to TlsGetValue for the rest of the process, even after that thread lets go. opengl32 only sends an ICD's GL calls from a thread with one of its contexts current, so the ICD counts the threads holding a context and, in MakeCurrent under its lock, returns Mesa to the globals when no other thread holds one (the globals set from the calling thread's own values before the switch is cleared). VXGL.LOG says each time it switches.

VXGL\_Stats500 (an export) returns these parts' counts and switch bits: 1 TRILIST, 2 CVACACHE, 4 FASTVTX, 8 STORESNAP, 16 GLAPIONE.

### 26.6 Mesa 7.0.3 bugs found and fixed

The first two are in the vbo module's array binding (vbo\_save\_draw.c for display lists, vbo\_exec\_draw.c for glBegin/glEnd). All were found by the host tests, whose Mesa is built with AddressSanitizer.

- **Stale arrays 28-31** (v10.490). Without a vertex program, `map_vp_none` maps tnl inputs 28-31 to vbo attributes 0-3 (position, weight, normal, colour), but the binding resets only inputs 0-27 to their current values before overlaying the node's arrays. A colour from an earlier display list stayed bound in input 31 for a list without colours. Once that earlier list was deleted, bind\_inputs read its freed buffer object (`BufferObj->Name`), and could map it. Fix: reset 28-31 with the others.
- **The material offset** (v10.493). A vertex's attributes are stored in vbo order (legacy 0-15, generic 16-31, materials 32-43), and the binding steps its data offset by `attrsz[attr]`, the tnl input's size, instead of `attrsz[src]`, the stored attribute's. With two materials a vertex (glMaterial between glBegin and glEnd, in a list or not), the second read the first's place. Fix: step by the stored attribute's size, bind only the 32 tnl inputs, and do not overlay 28-31 without a program (they alias 0-3, past the vertex's last attribute). The host check draws ambient (0.5, 0, 0) and diffuse (0, 0.5, 0) per vertex under a white light along z: 153 128 0 (156 129 0 in a 16-bit frame). With the bug the diffuse would read the ambient's place: red 255, green 0.

* **Blocks freed by the other allocator** (v10.498). Found when Mesa's blocks were made 32-byte aligned (`_mesa_malloc` keeping the offset in the byte before the block), which makes a mismatched free fault. slang\_emit's instruction annotations came from `_mesa_strdup` and were freed with `free`, and its result from `malloc` but freed with `_mesa_free`; slang\_compile's Programs array and vbo\_rebase's temporary indices came from `malloc` and were freed with `_mesa_free`. Fix: Mesa's allocator both ways. The alignment itself (ACCEL\_OPENGL\_ALIGNHEAP) measured no difference on the board and is off by default.

### 26.7 Registry, tried and left off, where the time is now

All under `HKLM\Software\VX85598`, DWORD; an environment variable of the same name overrides it.

| Value | Default | Since | Turns off (0) or on (1) |
| --- | --- | --- | --- |
| ACCEL\_OPENGL\_FLATSTRIP | 1 | v10.480 | a flat strip of one colour as one strip |
| ACCEL\_OPENGL\_FLATBATCH | 1 | v10.481 | flat strips whose colours differ as triangles, 64 a call |
| ACCEL\_OPENGL\_EMIT | 1 | v10.481 | the whole-vertex emit loops |
| ACCEL\_OPENGL\_HWFLAT | 1 | v10.481 | the engine's flat shading (04/23 = 9E02) |
| ACCEL\_OPENGL\_HWFLATSTRIP | 1 | v10.482 | flat strips as engine strips (1 = rule 1; 2 = rule 2, other pixels) |
| ACCEL\_OPENGL\_FASTINT | 1 | v10.483 | float to integer without \_\_CHP |
| ACCEL\_OPENGL\_RSQRT | 1 | v10.484 | rsqrtss / rcpss with a Newton step in lighting |
| ACCEL\_OPENGL\_CLIPSTRIP | 1 | v10.485 | strips split at the window's edge |
| ACCEL\_OPENGL\_LIGHTFAST | 1 | v10.485 | the one lighting loop |
| ACCEL\_OPENGL\_CLIPINT | 0 | v10.485 | 1: the clip test's compares in integer registers (2-7% slower) |
| ACCEL\_OPENGL\_SSEPASS | 1 | v10.487 | the SSE pass |
| ACCEL\_OPENGL\_SSEIMM | 1 | v10.488 | the pass for draws not from a list |
| ACCEL\_OPENGL\_SSEUNLIT | 0 | v10.488 | 1: unlit draws by the pass (4% slower) |
| ACCEL\_OPENGL\_SSEOUTSIDE | 1 | v10.489 | a draw wholly outside skipped first |
| ACCEL\_OPENGL\_SSEBOX | 1 | v10.490 | the bounding box test |
| ACCEL\_OPENGL\_SSESHINE | 1 | v10.490 | the specular lookup by SSE |
| ACCEL\_OPENGL\_SSEPACK | 1 | v10.490 | colour and fog bytes without a branch |
| ACCEL\_OPENGL\_CULLRUN | 1 | v10.490 | the culling run |
| ACCEL\_OPENGL\_SSESNAP | 0 | v10.490 | 1: the pass's snapped words read back from the store (slower; SSESNAP2 replaces it) |
| ACCEL\_OPENGL\_FLATNOCOPY | 1 | v10.492 | rule 1's colours without the copy |
| ACCEL\_OPENGL\_SSECACHE | 1 | v10.492 | the per-draw set-up kept |
| ACCEL\_OPENGL\_SSEMASK | 1 | v10.492 | the lighting masks by SSE |
| ACCEL\_OPENGL\_SSETIGHT | 1 | v10.493 | clip masks from tables, stores in locals |
| ACCEL\_OPENGL\_CULLRUN2 | 1 | v10.493 | the culling run's area from kept floats |
| ACCEL\_OPENGL\_FASTBIND | 1 | v10.493 | Mesa's float inputs bound in place |
| ACCEL\_OPENGL\_SSEFUSE | 1 | v10.495 | the fused kernels |
| ACCEL\_OPENGL\_SSEBOX2 | 1 | v10.495 | the box from centre and half sizes (and inside) |
| ACCEL\_OPENGL\_SSEACCEPT | 1 | v10.495 | no clip compares in a draw the box has inside |
| ACCEL\_OPENGL\_SSEPARTIAL | 1 | v10.495 | the partial set-up when only the material changed |
| ACCEL\_OPENGL\_LISTBIND | 1 | v10.495 | a list's arrays rebound only where they changed |
| ACCEL\_OPENGL\_SSEIMMBOX | 1 | v10.496 | the box for draws not from a list |
| ACCEL\_OPENGL\_SSESNAP2 | 1 | v10.497 | the grid words from the pass's registers |
| ACCEL\_OPENGL\_SSEOUTFAST | 1 | v10.497 | the whole-outside test in registers |
| ACCEL\_OPENGL\_SSEEARLY | 1 | v10.497 | the early decline of lighting the pass does not take |
| ACCEL\_OPENGL\_ALIGNHEAP | 0 | v10.498 | 1: Mesa's blocks 32-byte aligned (no difference) |
| ACCEL\_OPENGL\_TRILIST | 1 | v10.500 | triangle lists 128 a call |
| ACCEL\_OPENGL\_STORESNAP | 1 | v10.500 | the store's grid words four at a time |
| ACCEL\_OPENGL\_CVACACHE | 1 | v10.500 | the locked-array transform kept |
| ACCEL\_OPENGL\_FASTVTX | 1 | v10.500 | glVertex's copy as words |
| ACCEL\_OPENGL\_GLAPIONE | 1 | v10.500 | the dispatch back to the globals |

**Tried and left off.** The clip test in integer registers was slower than SSE compares. Unlit draws by the pass were slower than Mesa's unlit path. The snapped words were slower than snapw. The pass's first form without the outside test was slower than Mesa on Lattice, because it lit draws Mesa skips. Aligning Mesa's heap blocks to 32 bytes changed nothing, aligned or not, with any environment length. Do not turn these on again without new evidence.

**Where the time is** (Lattice, v10.492, 30,300 of 30,561 VXGL samples named):

| Part | Share of samples |
| --- | --- |
| sse\_group, the pass's four-vertex work | 36.3% (a sixth of it the clip masks' assembly, about 13% the per-vertex stores: both in v10.493) |
| the strip path: fast\_vertex\_arrays, strip\_culled, snapw, strip\_fast, fast\_v, strip\_body | about 21% |
| clipped triangles: interpolation, clip\_tri\_4, fast\_triangle, vx855\_clip\_strip, pb\_poly | about 11% (2.36M of 4.27M strip calls clipped) |
| the pass's set-up: sp\_prepare, box\_outside, light\_key | about 4.4% |
| Mesa's per-draw binding: \_tnl\_import\_array, vbo\_bind\_vertex\_list, bind\_inputs and others | about 3.5% |

Lattice since: 332 ms a frame on v10.493, 317-322 ms on v10.496 (both with the sampler on). VXFLAT v2.7 on v10.499, sampler off, frames a second: lit, one normal 65.6; lit, normals 42.1; positional, EXP 39.5; three lights 33.9; list, positional 68.9; list, 3 lights 53.4; list, culled 65.6. Mesa's own per-draw work in Lattice is about 3% (binding 1.2, list binding 0.7, playback 0.3, pipeline 0.3, run\_render 0.5), so bypassing tnl was dropped.

**Host profile of the game-style scenes** (v10.500, gcc -O2, callgrind, the driver's instructions a frame excluding the host's model): the Quake 3 scene 20% fewer than with v10.500's parts off, the Quake 2 scene 3% fewer (FASTVTX's gain is in Watcom's code and does not show here). What remains, by self cost:

| Part | Quake 2 scene | Quake 3 scene |
| --- | --- | --- |
| the core's vertex conversion (fast\_vertex\_arrays) | 5.7% | 8.3% |
| fast\_triangle's matching (list triangles) | under 1% | 11.7% |
| the strip path a polygon (vx\_d3d\_gl\_strip) | 4.2% | under 1% |
| Mesa's emit (emit\_viewport4\_bgra4\_st2) | 3.4% | 2.6% |
| glVertex3fv | 3.3% | none |
| Mesa's clip test | 2.3% | 0.8% |
| the minimum and maximum index scan | none | 1.3% |

**Checked and left as they are:** state translation is under 0.5% of the Quake 2 scene (to confirm in real Quake 2 with VXGL.LOG's timing); glTexSubImage goes through the texture ring without waiting (it copies the whole texture; VXGL.LOG counts any waits); the swap is timed in parts in VXGL.LOG. VXGL.LOG also lists each context's draws in software by fallback bit and by the state behind it: run each game with it on and treat every software draw as a bug.

**Next, in this order** (every one exact and behind its own switch):

1. Prepacked and indexed packets for OpenGL. Done and measured (section 27): prepacked packets are the ICD's default since v10.501; indexed packets were slower on the board at every step and were removed in v10.509 (Direct3D keeps them).
2. The pass writing packet words directly, written once against that form, with the per-vertex path (pass, strip emit, packet) in chunks of 128-256 vertices so each chunk stays in the 128 KB L2. No non-temporal stores: the VxD reads each packet back for its copy-in and validation, so they would only push those reads onto the 100 MHz bus.
3. Lighting deferred: transform, clip-test and project first, cull, then light only the vertices of triangles that remain (the provoking vertex only under GL\_FLAT).
4. glDrawElements without the rebase and split copies, together with unsigned-byte colour arrays passed straight through (the ubyte to float to ubyte round trip returns the original byte); both need an emitter of their own.
5. An SSE2 path (the board's Eden 1000, Esther core, has SSE2 and SSE3; the SSE1 limit was the ICD's own): a CPUID check at load, the SSE1 kernels kept as the fallback. The SSE2 colour bytes (cvtps2dq of f x 255) only after every float in \[0,1) is checked against Mesa's f x 255/256 + 32768 rule. Esther runs 128-bit operations as two 64-bit halves, so the gain is the scalar loops removed, not the width.
6. On the board with the real games (Quake 2, Quake 3, MDK2): the fallback audit and sampler profiles.

The guard band and object-space lighting stay last (not exact).

## 27. OpenGL after section 26: what the board kept, what it removed, and glVertex into the pass's arrays (v10.501-v10.509)

Each switch here was first checked on the host: callgrind counted the driver's instructions, and every draw packet was compared word for word with the switch off. It was then measured on the board with VXFLAT, in frames a second against the switch off, with every scene's pixels compared. A switch the board did not confirm was removed rather than left off.

**Kept (on by default).**

| Switch | Since | What it does | Measured |
| --- | --- | --- | --- |
| ACCEL\_OPENGL\_TRILIST | v10.501 | The plain path's GL\_TRIANGLES runs drawn on the engine | q3 arrays +14% |
| ACCEL\_OPENGL\_CVACACHE | v10.501 | Within one glLockArraysEXT, a draw from the same positions reuses the transform results | +2% |
| ACCEL\_OPENGL\_FASTVTX | v10.501 | glVertex's copy into Mesa's buffer done as words from registers | glBegin scenes +1-1.5% |
| ACCEL\_OPENGL\_GLAPIONE | v10.501 | Mesa stays on its single-thread dispatch after a second thread has held a context | +4-9% once that happens |
| ACCEL\_OPENGL\_STORESNAP | v10.501-v10.502 | The store's grid words made four vertices at a time; the flat triangles, quads, fans and polygons use them too | Strips and q3 arrays +2.5-3%. After the v10.502 fix: triangles +1.4%, quads +4%, fans +5%, list quads +6% |
| ACCEL\_OPENGL\_PREPACK | v10.501 | The ICD's packets use the Direct3D side's prepacked form, 10 words a vertex | +0.5-0.9%, later +1.2% |
| ACCEL\_OPENGL\_FASTINDEX | v10.501 | glDrawElements' index scan takes two indices a step | q3 arrays +0.6%, later +1.8% |
| ACCEL\_OPENGL\_UBCOLOR | v10.501-v10.502 | Four-byte colour arrays go into the vertex as bytes; the round trip returns every byte | +3.6%, after the pass stopped converting them on entry |
| ACCEL\_OPENGL\_GLLIST | v10.503 | A GL\_TRIANGLES run's checks made once a run, in the run's own loop | q3 arrays +4.9% |
| ACCEL\_OPENGL\_GLWORDS = 2 | v10.503-v10.505 | Each vertex's ten words written from a recipe made once a template; the commonest layouts get fixed offsets | +1.1%; 2 adds +1.4-1.5% |
| ACCEL\_OPENGL\_SSEDEFER | v10.503 | Lit, culled draws: positions first, then light only the groups that drawn triangles use | Smooth, culled +15.7% (70% of groups left unlit) |
| ACCEL\_OPENGL\_SSE2 | v10.503 | The pass's colour words packed with SSE2 (the same bytes by construction) | +0.4-0.6% |
| ACCEL\_OPENGL\_GLFAN | v10.505 | A polygon's, fan's or quads' strips sent in one core call | q2 world +0.9% |
| ACCEL\_OPENGL\_SSERANGE | v10.505 | The pass's range checks done four lanes at once | Lit lists +1.4-1.8% |
| ACCEL\_OPENGL\_PLAN | v10.506 | A draw's triangle-run decisions kept and reused by the next draw with the same window vertices and state | q3 arrays +1.6% |
| ACCEL\_OPENGL\_MINMAX2 | v10.506 | glDrawElements' index scan done with SSE2 | Within the spread; host Quake 3 scene 2.1% fewer instructions |
| ACCEL\_OPENGL\_IMMSOA | v10.509 | glVertex writes straight into the SSE pass's arrays (27.1) | Host 1.2-1.4% fewer instructions; VXFLAT v3.4's noimmsoa run |

**Removed (the board turned them down).**

| Switch | Removed | Why |
| --- | --- | --- |
| ACCEL\_OPENGL\_INDEXED | v10.509 | Indexed packets (each vertex converted once, triangles drawn by index) were slower at every step: host work +8%, and on the board 13-14% slower in q3 arrays (v10.501-v10.502). With clipped polygons kept inside the packet (v10.506): q2 world -3.7%, list clipped -7%, clipped and lit -4%. Direct3D's ACCEL\_D3D\_INDEXED stays (GTA3). |
| ACCEL\_OPENGL\_CLAMP | v10.509 | GL\_CLAMP drawn as CLAMP\_TO\_EDGE on the engine: no gain, and it changed pixels. |
| ACCEL\_OPENGL\_CLIPINT | v10.506 | The clip test's compares in integer registers: 2-7% slower. |
| ACCEL\_OPENGL\_SSEUNLIT | v10.506 | Unlit draws through the SSE pass: 4% slower. |
| ACCEL\_OPENGL\_SSESNAP | v10.506 | Grid words made for every group: slower. SSESNAP2 makes them now. |
| ACCEL\_OPENGL\_ALIGNHEAP | v10.506 | Mesa's blocks aligned to 32 bytes: no gain. |

**Not done, with numbers.** A template cache keyed on full state (4 entries) never matched in the Quake 3 scene and cost 2.4% more; a larger one would compare 2 KB of state per lookup, about what a build costs. Keeping each store vertex's words for later uses was slower, because few vertices are used again within a run. A last-result cache for the specular \_mesa\_pow got 0 hits in 15,250 calls.

### 27.1 glVertex into the pass's arrays (ACCEL\_OPENGL\_IMMSOA, v10.509)

Before this switch, glVertex wrote each vertex into Mesa's interleaved buffer. The SSE pass's imm\_build then copied it out again into the pass's own arrays. Now, for a buffer the pass will take, glVertex writes the arrays directly and imm\_build reads them in place.

**Layout.** There are 22 float arrays, one lane per vertex:

| Arrays | Hold |
| --- | --- |
| 0-2 | x, y, z |
| 3-5 | Normal |
| 6-9 | Colour |
| 10 + 2u, 11 + 2u | s, t of texture set u (u = 0-2) |
| 16 + 2u, 17 + 2u | r, q of set u |

At flush, the lanes are padded to a multiple of four. The `_vbo_vx855_imm` struct (count, which arrays are present, and their pointers) is published for imm\_build just before draw\_prims and cleared after it.

**When it is taken.** The decision is made once per buffer, at the first vertex (vert\_count == 0), in vbo\_exec\_vx855\_decide. All of these must hold:

- Lighting is on and the render mode is GL\_RENDER.
- The pass's predictor (`_vbo_vx855_imm_will`, installed by the ICD) says the pass would take the draw. It checks, before any vertex exists: pass on, no fallback, no programs, no user clip planes, no point attenuation, a lighting model the pass handles, and a valid modelview inverse.
- The vertex holds a position of size 2 or 3, a normal of size 3, a colour of size 3 or 4, and coordinates for at most three texture sets, and nothing else.

The predictor is required. Without it, a draw the pass later declines (Mesa's fast lighting, for example) has to make the buffer's words afterwards, which cost 2.66 million more instructions in the host scene. With it, the choice costs about 17,000 instructions over a run of 120 buffers.

**When the buffer's words are made anyway.** vbo\_exec\_vx855\_aos makes the same 32-bit words Mesa would have written, but only where something reads them:

- A flush inside a primitive, because copy\_vertices reads the buffer.
- A draw the pass declines after all: sp\_run calls `_vbo_vx855_imm_aos()` at both of its GL\_TRUE returns.
- The checks before the pipeline that read texture coordinates (a third texture stage, or GL\_CLAMP ring copies).
- Antialiased smooth lines, which read colours.

On a wrap, the decision is made again and the arrays are refilled from the copied vertices that were replayed. A layout upgrade (a new attribute mid-buffer) ends the switch for that buffer. Flush resets the decision.

**Proof.** The host draws eight cases three times: through the buffer, through the arrays, and through the buffer again, with state reset between runs. The arrays run must match a buffer run word for word in every draw packet, and in pixels. The eight cases:

1. Strips with colour material and fog.
2. glVertex2f with glColor3f.
3. A strip of 3000 vertices across a wrap.
4. A texture matrix the pass declines after the copy.
5. A coordinate set that grows mid-buffer.
6. Quads with glColor4ub and glTexCoord1f.
7. Triangles and quads in one buffer, flat and culled.
8. Separate specular, which the predictor turns away (0 buffers).

**Measured on the host** (callgrind, the driver's instructions): lit positional strips 1.2% fewer, the culled lit scene 1.4% fewer. Draws the pass declines are unchanged. VXGL\_Stats509 slots 30-33 count buffers, vertices, words made anyway, and wraps refilled; VXFLAT v3.4 prints them and runs noimmsoa (ACCEL\_OPENGL\_IMMSOA = 0) in its quick run.

### 27.2 Section 26's list, where each item stands

1. Prepacked and indexed packets: prepacked packets have been on since v10.501 (PREPACK). Indexed packets were removed in v10.509 (above).
2. The pass writing packet words: the ten words now come from a recipe made once a template (GLWORDS, v10.503-v10.505). The pass itself does not yet write them.
3. Lighting deferred: done for groups in lit, culled draws (SSEDEFER, v10.503).
4. glDrawElements' copies: the index scan is faster (FASTINDEX, MINMAX2) and byte colours pass straight through (UBCOLOR). The rebased index copy remains, about 2% of the Quake 3 scene; skipping the scan would mean trusting the application's indices to the locked range.
5. SSE2: used for the colour words (v10.503) and the index scan (v10.506), each behind CPUID 1 EDX bit 26. The vertex kernels are still SSE1.
6. The real games on the board: still to do.

## 28. GDI on the CPU: lines, the cursor handshake and DIBs (v10.507-v10.509)

WinTune on the board, before this work: line drawing 0.81 s, DIB blit 0.96 s, filled objects 0.32 s, text 0.32 s. VXGTUNE (new in v10.507) measures the same four categories in the desktop mode. It compares pixels against the .506 path for lines, fills and text, and against DIBENG for DIBs, before timing anything.

### 28.1 Solid lines in the display driver (ACCEL\_CPULINE, ACCEL\_CPULINE\_MAX, ACCEL\_CPULINE\_MAXY)

The 16-bit driver draws solid one-pixel lines itself (Polyline and LineTo) with a 32-bit loop straight into the screen. This covers R2\_COPYPEN, R2\_NOTCOPYPEN, R2\_BLACK and R2\_XORPEN at 8, 16 and 32 bpp. It uses the rule the engine path and DIBENG share (VX2LINE), so the pixels are identical:

```c
/* n = major delta, m = minor delta (both >= 0); the end point is not drawn */
int e = 2*m - n - (dy > 0);           /* one less when Y increases */
for (i = 0; i < n; i++) {
    plot(p);
    if (e >= 0) { p += minor_step; e += 2*(m - n); }
    else        { e += 2*m; }
    p += major_step;
}
```

A segment that leaves the clip rectangle is stepped forward to its visible run. Clipped short segments use 16-bit arithmetic. Styled pens and other ROPs go to the engine or DIBENG as before.

**Length thresholds.** The CPU costs about 50 ns a pixel; the engine costs about 12 us a line (one VxD call and a wait). With every length on the CPU (v10.507), long lines ran at 0.83x, 64-point polylines at 0.82x and clipped lines at 0.65x. A pen-copy segment of at least the threshold length now goes to the engine (the VxD clips it). XOR lines, short lines and the engine's SKIPs stay on the CPU. VXGTUNE v1.1's sweep (ms for 200 lines, 8 pens, random direction) gives a different crossover for each direction:

| Length | Shallow: CPU | Shallow: engine | Steep: CPU | Steep: engine |
| --- | --- | --- | --- | --- |
| 64 | 5.84 | 10.94 | 8.62 | 11.20 |
| 96 | 6.75 | 11.27 | 10.98 | 11.45 |
| 128 | 7.55 | 11.49 | 13.67 | 12.04 |
| 256 | 10.79 | 12.72 | 23.36 | 13.78 |
| 384 | 14.22 | 13.81 | 32.41 | 15.63 |
| 512 | 17.59 | 14.96 | 36.26 | 16.35 |

The defaults (.510) are ACCEL\_CPULINE\_MAX = 368 for shallow lines and ACCEL\_CPULINE\_MAXY = 112 for steep ones. The lengths are published to the driver in sixteens: 0 sends every pen-copy segment to the engine, and 1008 or more keeps them all on the CPU. VXGTUNE v1.2 reports the crossover for each direction on any machine.

**Clipped runs and polylines (.510).** A segment that leaves the clip rectangle no longer steps its hidden pixels. With A = e0 - 2m + 2n, pixel k's minor steps are floor((A + 2mk) / 2n); the clip's major range bounds k directly and its minor range by two divisions, which give the run's first pixel and its error. A polyline whose points all lie inside the clip is drawn in one assembly call (vx\_poly\_run): each segment's set-up in 32-bit registers, its first pixel's address where the last segment ended, no C per segment; it stops before a segment long enough for the engine. Board (VXGTUNE v1.3, .510): lines 1.13x with clipped lines 1.02x (was 0.93x), filled objects 1.21x (Ellipse 1.15x, was 0.88x), DIB 1.61x; crossovers 383 shallow, 107 steep.

Board results with a single threshold of 160 (v10.508): line drawing 1.10x over .506. Short lines 1.42x, horizontal and vertical 1.10x, XOR 1.08x; long, polyline and clipped lines are back to 0.98-1.02x.

### 28.2 No VxD call for the cursor (ACCEL\_GDIFAST)

DIBENG's BeginAccess/EndAccess made two VxD calls per GDI drawing call to exclude the cursor. While the cursor is the hardware one, those calls are now skipped, through a handshake in the FBHDA:

- **rem4** (written by the VxD): `'CU' << 16`, with bit 0 = direct, bit 1 = software cursor drawn, bit 2 = a show was put off.
- **rem3 low word** (written by the driver): its busy count.

If a software cursor show arrives while the driver is drawing (a mouse move at interrupt time), the VxD puts it off. The driver lets it through as soon as its count drops to zero. With a software cursor drawn, the calls are made as before.

Measured: 16x16 fills 1.23x, text 1.02-1.04x. Most of the short-line gain also needs this switch: short lines take 19.3 ms without it against 14.2 ms with it.

### 28.3 DIBs

Measured with VXGTUNE (256x256, ms a call):

| Case | DIBENG | Driver |
| --- | --- | --- |
| 16-bit 1:1, rows straight into the 32bpp screen | 3.46 | 1.97-2.13 |
| 8/16/24-bit StretchDIBits 2x on the engine | 7.4-14.6 | 5.7-6.9 |
| 1/4/8/24-bit 1:1 through the VxD | 0.52-0.88 | 0.90-2.13 (left with DIBENG) |
| 32-bit 1:1 | 0.63 | No driver route |

The DIB category total is 1.62x over DIBENG. A 32-bit DIB onto the 32bpp screen no longer asks the VxD at all (v10.509), which saves about 2 us a call; escape counter 13 counts these.

### 28.4 Turned down and removed

| Feature | Removed | Result |
| --- | --- | --- |
| ACCEL\_CPUSPAN: filled shapes' scan lines (Ellipse, Polygon, RoundRect) as dword stores from the driver | v10.509 | Ellipse 0.75x, Polygon 0.89x, RoundRect 0.86x |
| ACCEL\_DIBHOST: 32-bit 1:1 DIBs through the engine's host port (CC000041, a dword a pixel) | v10.508 | 4.9 ms against DIBENG's 0.64 |
| ACCEL\_DIBSTORE 2: rows through a line buffer with 16-byte non-temporal stores (MOVNTDQ, FXSAVE around the call) | v10.508 | 0.73 ms, still slower than DIBENG |
| ACCEL\_DIBSTORE 0: rows through a line buffer | v10.508 | Slower than rows straight into the screen (now always used) |
| ACCEL\_STRETCH class 4194304: 32-bit 1:1 onto 32bpp in the VxD | v10.508 | Slower than DIBENG |

### 28.5 Controls and counters

| Control | Meaning |
| --- | --- |
| FBHDA rem3 high word (VxD to driver) | `0x8000` \| bit 0 CPULINE \| bit 1 GDIFAST \| (CPULINE\_MAX/16) << 2 \| (CPULINE\_MAXY/16) << 8; each length is 6 bits, 63 = no limit |
| Live set (VxD) | `0xE9000000 \| bits`; `0xE8000000 \| x \| y << 12` for the two lengths |
| Read (VxD) | `0xFFFFFE26`: out\[0\] bits, out\[1\] x \| y << 16, out\[2\] rows drawn straight into the screen, out\[3\] cursor shows put off |
| Driver escape 0x8F74 | The driver's counters: 0-8 lines and accesses, 12 long segments sent to the engine, 13 32-bit DIBs skipped |

VXGTUNE puts the registry values back when it finishes.

## 29. GTA3's Direct3D path (.511-.526)

GTA3's gameplay frame went from 126 ms (.522) to 111.4 ms (.524, D3D8PROF), and the VxD's share of a slow frame from 33.2 to 25.3 ms. What is left is mostly outside the per-packet path: the batch call's entry, Direct3D 8's own vertex processing and GTA3 itself (section 30).

### 29.1 Where a slow frame goes

| Build | Frame | HAL | VxD | Direct3D 8 | GTA3 | Note |
| --- | --- | --- | --- | --- | --- | --- |
| .511 | 7.9 fps median | DrawPrimitives2 61 ms (VxD calls 23, primitives into packets 20) | 40 us a packet: emitter 15.5, copy-in 5.1, validation 4.6, ring 5 | - | - | 38 clears waited 86-194 ms for the system critical section |
| .514 | 7.8 fps median | DrawPrimitives2 68 ms (VxD 25.8, packets 22.7) | - | - | - | 35 clears waited 85-194 ms; KRNL386 (014F) and 003B held the section |
| .522 | 126 ms | 44 ms | 32 ms (23% in calls other than submits) | about 26 ms | about 12 ms | RTC sampler |
| .523 | 126 ms | - | copy-in 5.0, validation 4.7 us an indexed packet | - | - | the profiler's own VxD calls about 13 ms |
| .524 | 111.4 ms (D3D8PROF; was 118.4) | - | 25.3 ms a slow frame; copy-in 3.2, validation 4.9, set-up 9.8 us a packet | 21-24% of the frame (software vertex processing) | - | 195 DrawPrimitives2 calls of 10 ms or more |
| .525 | - | - | SHADOW saved 2.9 us a packet; IXCHK cost 0.9 | - | - | 7.1 us a triangle either way |

Three findings that are not the driver: the game menu's lag (draws take 0.01-0.02 ms a menu frame; the time is GTA3's code and a 16-bit thread at CS 014F); the long DrawPrimitives2 calls (the render thread ran 29 of 327 ms, 0 page-ins: another thread was running); and the critical-section waits of .511-.514 (16-bit KRNL386 code held it, not this VxD, DIBENG or the display driver).

### 29.2 Measuring in a real game

Every speed decision since .511 was taken from GTA3 on the board, with an A/B of the live switches inside the game, then checked word for word on the host.

| Tool | Build | What it gives |
| --- | --- | --- |
| ACCEL\_D3D\_SAMPLE (diagnostic, 0) | .511 | VXDDRAW samples the flipping thread about 1000 times a second; every 600 flips C:\\VXD3D.LOG gets each module's share and VXDDRAW's busiest 32-byte buckets, named from VXDDRAW.map (link maps with OPTION STATICS since .512) |
| Critical-section holder sampling | .512-.514 | while a 3D call waits in cs\_enter, what runs instead, its 24 commonest 16-bit places by CS and IP block (TEXDBG BC D0-D4, D7-DA); escape 0x8F74 input 2 names VX85598.DRV's and DIBENG's selectors; VXSEL names any selector |
| Recorder and replay | .517-.518 | VXD3DMON REC=n writes n frames to C:\\VXD3D.REC (input, state, every service answer); tests/vx\_replay replays it through the core and emitter on the host; -s name=value flips a switch; it hashes the emitted words, the fetched vertices and a register model at every draw |
| RTC sampler | .520-.521 | IRQ 8 virtualized, the RTC's periodic interrupt at 1024 a second (register A rate 6, B PIE, C acknowledged, physical EOI); samples weighted by the time since the last tick; the render thread inside the VxD split by phase. The VMM time-out clock aliased with the 5 ms desktop timer (VXSMPCAL v1.0 failed 10 of 36) and was removed in .524 The sampler itself costs about 0.24% of the time. The watcher around it must stay light: leave Windows' timer at its own rate (no timeBeginPeriod), take the game's module list only when the game is found and once more 15 s later (Toolhelp on Windows 98 stalls other processes), and poll the HAL's counters every 250 ms, not faster. |
| Sampler phases | .520, .526 | 0-9: other calls, submit, copy-in, validation, building, group start, waits, single submits; .526 splits the batch call: 10 entry (lock, busy checks, mapping), 11 sequencer opened, 12 between packets, 13 after them; 2 is the dispatch outside the batch (VX\_SMP\_VERSION 0x521) |
| HAL phase profiler | .524-.525 | the thread's run time (TEXDBG BD 81: run ms, system time, VMM page faults, page-ins from \_GetDemandPageInfo) read on every 16th DrawPrimitives2 call; QueryPhases returns 270 words |

### 29.3 The engine lock (.515-.516)

ACCEL\_PRIVLOCK (1) replaces the system critical section with the engine's own lock (recursive per thread, a VMM semaphore for waiters) in the 2D, 3D and flip paths. Two rules keep it safe. FBHDA\_lock skips its semaphore while the critical section is held, so the engine lock's owner counts as the critical section's owner (vx\_lock\_owned). Where both are held the order is always the critical section first, then the engine lock (VX2D\_stretch, VX2D\_xfer and mode sets claim the critical section first); the 3D paths take the engine lock alone. Counters: TEXDBG BC DB/DC.

### 29.4 Culling on the engine (.515, .519)

ACCEL\_D3D\_IXPASS passes an indexed list's triangles through when cull is none: a run whose indices span the packet's free vertices takes them once, with no per-triangle area or index lookup. A span much wider than the run's triangles (more than 3 vertices a triangle plus 32) ends the pass. The HAL culls every other mode on the CPU. The engine's own cull was tried and removed (24.5); if it is ever tried again, the cull bit (01/01 bit 7, HenBFCull) must go inside the set-up's own 01/01 write. A 01/01 written on its own after the set-up stopped every draw (.518).

### 29.5 Smaller packets and fewer words

**8-word vertices (.522-.523, now always on).** An indexed packet with an XP program, one texture unit and the plain layout carries X Y Z W diffuse specular u v, its run word marked VX3DN\_IX\_V8. The engine draws them from the vertex area at a 32-byte stride with this layout, written only when the engine does not have it already (vx3dn\_lay8, reset by the set-ups and vx3dn\_forget):

```text
04/00 = A80008
04/01..04 = 020100 060503 000908 000000
04/25 = F6C000
04/28 = FFC000
```

A packet that would skip its set-up while the engine holds the 8-word layout gets its whole set-up. Host replay: the same 231,741 fetched vertices; 1,090 words a packet against 1,182. The VxD's copy-in reads 32 bytes a vertex.

**State deltas (.524, now always on).** VX3DN\_STATE\_RANGES (vx\_native3d.h) is the one table of the packet's seven state ranges (bytes 0-108, 3952-3992, 5528-5680, 6448-6680, 8216-8348, 9116-9292, 10320-10452). The HAL compares ranges 1-6 of each queued packet with the one before and marks the same ones in the draw's clear\_depth (VX3DN\_SAME\_TAG 0x5D000000 | bits). The VxD clears the mark on its copy and keeps its validated copy of each marked range. With every range marked and the header unchanged, vx3dn\_validate\_geo checks only the vertex count, runs, indices and vertices. A mark is honoured only when the packet's predecessor in the same call is the one the copy holds. A batch packet's cb and op are checked again on the VxD's copy. Board: 0.34-0.9 us a packet's copy-in; host: 76% of packets marked, 594 bytes a packet not copied.

**Register shadow (.525, always on in .526).** The state set-up's type-01 registers 03-8F, except 29 (the Coarse Z tags), are written only when their value differs from the last set-up's (vx\_native3d\_hw.c, REG1, sh\_v / sh\_m). The shadow is used only while the VxD has primed the engine in this state generation (vx3dn\_sh\_ok, set and cleared around each build). It is forgotten with the rest of the engine state (vx3dn\_forget: a reset, a refused or rebuilt group, a switch) and by an occlusion query. The point mode's 01/85 write drops that register from it; 00-02 and 90-9B always go. Host: 95 words fewer a whole set-up; the register model the same at all 6,549 draws. Board: 2.9 us a packet saved.

**Texture shadow (ACCEL\_D3D\_TEXSHADOW, .526, default 1).** In texture\_setup, each unit's 02/xx after the header u << 24 | 00020000 and its sampler's 30-32 after u << 24 | 20020000 go only when changed (TXS, slots 2u and 2u+1), with the type-01 shadow's validity; not with a texture probe. FE words, borders and level offsets always go. Host: 7 words fewer a texture set-up, the same registers at every draw.

**Fast check (ACCEL\_D3D\_FASTCHK, .526, default 1).** The prepacked vertices are screened first by ORing every test as 0 or 1, with no branch a vertex; the coordinate test leaves out the guard band's exception, so it flags a superset. The exact loop runs only for a flagged packet. Host: none of the replay's 3,117 packets flagged; random spoiled words give the same answer either way.

**Readiness cache (ACCEL\_D3D\_INFOCACHE, .522, now always on).** The readiness answer (VX\_IOCTL\_3DN\_INFO, asked at every DrawPrimitives2 call before) is kept while it said ready for the context's generation and is under 100 ms old; it is asked again after a refused submit and at a HAL rebuild. The VxD still checks each packet's generation.

**Fixed-offset recipe (.526).** The HAL's recipe converter has a fixed-offset path for Direct3D 8's pre-transformed textured vertex (FVF 0x1C4, GTA3's): 23% fewer instructions, the same words.

### 29.6 Every Direct3D speed switch at .526

A baked-in switch no longer reads the registry; its global stays as a test hook (VX\_D3D\_Switch or TEXDBG BC) and leaves VXD3DMON's A/B.

| State | Switches |
| --- | --- |
| Baked in .524 (saved time in all three A/B runs) | PREPACK, INDEXED, TEXONLY, RINGBLT, RTNOWAIT, FASTCLOCK, ONECOPY, V8 |
| Baked in .525 | IXPASS, INFOCACHE, DELTA |
| Baked in .526 | SHADOW (type-01 register shadow) |
| Removed in .527, measured as losses (31.1, 24.5) | TEXSHADOW (BC F5 off, F6 on, F7 read), FASTCHK (BC EC off, ED on, EE read) |
| Settled in .527 by four A/B runs (31.2, 24.5): VRECIPE, FASTMATCH, FASTVV and PFVV2 removed; TEXHINT baked in; HWCULL removed in .529 (idle in GTA3, no saving in two A/B runs) | VRECIPE (switch 0), FASTMATCH (1), FASTVV (2), TEXHINT (5), PFVV2 (6), HWCULL (9) |
| On, not a speed switch | ACCEL\_PRIVLOCK (engine lock) |
| Diagnostic, off | ACCEL\_D3D\_SAMPLE |
| Removed | IXFIT (.515: no change in GTA3), PAIRLIST (.524: -3.9 ms median, lone pairs go out as strips again), IXPF (.524: within noise), the VMM time-out sampler clock (.524: aliased), IXCHK (.526: the word-by-word copy cost 0.9-1.25 us a packet more than the block copy and the check pass together) |

Free or unknown ids (.527): VX\_D3D\_Switch 0, 1, 2, 4, 6 and 12; feature word bits 18 and 27-30 (bit 17 INFOCACHE, 19 V8, 20 DELTA).

### 29.7 Diagnostic codes added since .511

| Code | Meaning |
| --- | --- |
| TEXDBG BC DB / DC | engine lock: entries, waits, longest wait and hold |
| TEXDBG BC D0-D4, D7-DA, D5 | critical-section holder places; D5 clears; D6 the ring cache's work counts |
| TEXDBG BC E5 | 8-word stats (8 words) |
| TEXDBG BC E6 / E7 / E8 | DELTA off / on / read |
| TEXDBG BC E9 / EA / EB | SHADOW off / on / read counts |
| TEXDBG BC EC / ED / EE | .527: the last refused packet, words 0-5 / 6-11 / clear (32.2; were FASTCHK's, removed) |
| TEXDBG BC CC / CE | indexed copy-in / validation times |
| TEXDBG BC C8 / C9 | build times: \[0\] state, \[1\] texture, \[2\] program |
| TEXDBG BC 6C / 6D / 6E | VxD profile clock on / off / read |
| TEXDBG BD 81 | \[2\] run ms, \[3\] system time, \[4\] VMM page faults, \[5\] page-ins |
| QueryPhases (270 words) | 244 infocache, 245 delta, 246-247 cache stats, 248 delta-marked, 249-250 V8, 251-252 delta ranges / all the same, 253-258 page faults by call-time bucket, 259-264 page-ins by bucket, 265-269 sampled calls of 10 ms or more (count, ms, ms ran, faults, page-ins) |

Board tools for this work: VXD3DMON v9.1 (in-game A/B, phases, the hottest pages of D3D8.DLL and GTA3.EXE), D3D8PROF, VXSETUP v1.0 (.526: TEXSHADOW and FASTCHK on against off, six shots a form, their times and counts), VXDELTA v1.1, VXSMPCAL v1.2, VXCULL v1.3, VXSEL.

## 30. Left to integrate (at .526)

The biggest open item is several draws per packet; the next two need board data first (the .526 run with VXD3DMON v9.1, and D3D8.DLL). The features doc of v10.207 (bump, cube maps, projected textures, coordinate set 1, colour key with specular) is done except YUY2 textures.

### 30.1 GTA3 and Direct3D speed

| # | Item | State | Needs first |
| --- | --- | --- | --- |
| 1 | Several draws per packet | built in .528 as texture runs (33.1-33.5); a run for other state changes is not built | a packet form whose draws share one set-up; host replay to prove the same words |
| 2 | The batch call's entry: "lock, set-up and close", a quarter of the VxD's time | measured on .526: 23 us a batch call (31.3); .528 writes the sequencer only when it changes (33.6) | count the register reads and waits in the batch's open and close |
| 3 | Direct3D 8's software vertex processing: 21-24% of the frame | VXD3DMON v9.2 (.527) picks the system D3D8.DLL by its full path and lists its hottest pages | the real C:\\WINDOWS\\SYSTEM\\D3D8.DLL (the one sent is D3D8PROF's wrapper) and VXD3DMON's page report fixed to pick it (31.4) |
| 4 | Decide VRECIPE, FASTMATCH, FASTVV, TEXHINT, PFVV2, HWCULL | done in .527 (24.5) | four runs read (31.2): remove FASTMATCH, FASTVV, PFVV2 and VRECIPE; bake in TEXHINT; HWCULL is idle in GTA3 |
| 5 | Confirm TEXSHADOW and FASTCHK | removed in .527 (24.5) | done: both lose time, remove both (31.1) |
| 6 | Larger packets (past 192 vertices) | not done | changes the layout every caller and test shares |

### 30.2 Direct3D features

| # | Item | Needs first |
| --- | --- | --- |
| 7 | YUY2 textures (CLXP3D T152) | a CLXP3D re-capture with real YUY2 data |
| 8 | D3DRENDERSTATE\_WRAP0-7 (interpolation wrap) | the HCM's 02:FE:0B/0C bits checked against XP on the board |

### 30.3 OpenGL

| # | Item | Needs first |
| --- | --- | --- |
| 9 | Strips whose lit colours differ | the triangles batched with their own colours |
| 11 | GL\_FRONT\_AND\_BACK with depth writes | exact only with the target switched per triangle |

Already built (not open): polygon stipple (16.20) and line stipple (18, ACCEL\_OPENGL\_LINES); S3TC textures (16.20, ACCEL\_OPENGL\_S3TC); a third texture unit (16.20) and a cube map or 3D texture on a second unit (16.23); fragment programs and shaders as texture stages, render to texture (16.23, 19.5); antialiased points, lines and polygons (16.23, 19.6). Where the engine cannot draw one of these, swrast does.

Not possible on this engine: shadow compare (no depth compare in the sampler) and hardware T&L (no transform unit). Coarse Z by default was measured slower.

### 30.4 2D, DirectDraw and modes

| # | Item | Needs first |
| --- | --- | --- |
| 15 | 2D engine at 8bpp; 8bpp modes beyond the three base sizes | the engine's 8bpp colour-depth setting measured with VX2PROOF |
| 16 | Lost-surface hardening | today only DDERR\_SURFACELOST from the existing paths |
| 17 | Stretch: stage only the needed source rows; an index for surface handles | none |
| 19 | V3 overlay, 8-bit desktop colour key, low-res modes, DPMS, dual display | not built; constant overlay alpha is unsupported |

Outside the driver, last noted at v10.208: the DDHELP fault in DIBENG.DLL after WinTune, and Tunnel -systemmemory crashing on a mode switch.

## 31. The .526 board run (2026-11-08)

Both .526 features lose time and should go. Four in-game A/B runs now settle the old switches. The batch call's fixed cost is 23 us a call. GTA3's gameplay frame is 120 ms (8.3 fps), the same as .525. Logs: VXSETUP v1.0, VXD3DMON v9.1, D3D8PROF and VXD3D.LOG, all from GTA3 on .526.

### 31.1 TEXSHADOW and FASTCHK: remove both

| Measure | TEXSHADOW (texture register shadow) | FASTCHK (branch-free vertex screen) |
| --- | --- | --- |
| Picture (VXSETUP, six shots a form) | 0 pixels differ in every form | 0 pixels differ in every form |
| Work saved | 153 words (one texture) and 315 (two) over the grid; 6.9 words a set-up in GTA3 | 3 of 625,820 GTA3 packets flagged, so the exact loop almost never ran |
| VXSETUP time | texture set-up 0.050 ms a frame on, 0.051 off | validation 8.34 us a packet on, 6.15 off: 2.19 us lost |
| GTA3 A/B (switch off less all on, median \[quartiles\]) | VxD -0.61 us a packet \[-2.78, -0.42\]: off is faster | VxD -4.35 us a packet \[-4.64, +1.32\]; validation 6.9 us on against 5.1 off |

The texture shadow's compares and branches cost more than the 7 words a set-up they save; ring writes are cheap. The screen tests every word of every vertex, while the exact loop it was meant to skip already ran fast on these vertices. The type-01 SHADOW (.525) stays: it saves 47.8 words a set-up and 2.9 us a packet.

### 31.2 The undecided switches over four runs

DrawPrimitives2 microseconds a triangle, switch off less all on (positive = the switch saves time).

| Switch | .523 | .524 | .525 | .526 | Reading |
| --- | --- | --- | --- | --- | --- |
| VRECIPE | +0.216 | -0.152 | -0.036 | +0.041 | no consistent saving: remove |
| FASTMATCH | -0.031 | -0.266 | -0.001 | -0.081 | never saves: remove |
| FASTVV | -0.197 | -0.158 | -0.098 | -0.104 | a small loss every run: remove |
| PFVV2 | -0.254 | -0.128 | +0.152 | -0.128 | a loss in 3 of 4: remove |
| TEXHINT | -0.105 | +0.031 | +0.308 | +0.256 | saves in the last three (.525 quartiles all positive): bake in |
| HWCULL | -0.321 | -0.062 | +0.040 | -0.302 | the engine culled 0 GTA3 packets: idle in GTA3; judge it in another game |

The A/B's frame-ms column read negative for every switch (-8 to -17 ms), which only says that the all-on period of each turn ran slower scenes. Use the per-triangle and per-packet columns, never the frame column.

### 31.3 The batch call's fixed cost

The .526 sampler phases split the quarter of the VxD's time that .525 could not place. Render thread inside the VxD (29.1 ms of a 123.5 ms slow frame), by phase:

| Phase | Share | A slow frame | Each |
| --- | --- | --- | --- |
| Building the words (emitter) | 45.8% | 13.3 ms | 27 us a packet |
| Validation | 13.9% | 4.0 ms | 8 us a packet |
| Batch: after the packets (status, sequencer closed, cursor, lock) | 6.8% | 2.0 ms | 13 us a call |
| Dispatch outside the batch | 6.2% | 1.8 ms | 12 us a call |
| Copy-in | 6.1% | 1.8 ms | 3.6 us a packet |
| Batch: between packets (cursor, groups, counts) | 6.0% | 1.7 ms | 3.5 us a packet |
| Starting a group (flush, fence, kick) | 4.5% | 1.3 ms | 5 us a group |
| Batch: sequencer opened, status, fetcher idle | 4.1% | 1.2 ms | 8 us a call |
| Other calls (Lock, flip, query) | 4.1% | 1.2 ms | - |
| Batch entry (lock, busy checks, mapping) | 1.5% | 0.4 ms | 3 us a call |
| Engine and ring waits; single submits | 0.9% | 0.3 ms | - |

About 154 batch calls and 492 packets a slow frame (3.2 packets a call). The open, close and dispatch cost about 23 us a call. Fewer calls is worth about 3.5 ms a slow frame, from holding packets across DrawPrimitives2 calls. Cheaper open and close is worth the same; reads of engine registers are the likely cost (a 2D re-read is 5 us). The emitter still holds almost half: texture set-up is 58 words a packet, and only 1,525 of 642,050 texture set-ups matched the packet before.

### 31.4 Direct3D 8's own time and the DLL

GTA3 creates its device with software vertex processing (D3D8PROF: behaviour 0x20), so Direct3D 8 transforms and lights every vertex on the CPU before the HAL sees it. The RTC sampler puts the render thread in Microsoft's D3D8.DLL (C:\\WINDOWS\\SYSTEM, 1,234,944 bytes) for about 23% of a gameplay frame, about 25 ms of a slow one. That time is already in SSE code: the hottest pages (RVA A4000 5.5% of the frame, AB000 2.8%, A2000, A3000, AA000) are mostly movaps, mulps, addps and shufps, with some SSE2 (cvttps2dq). The DLL's CPU check (RVA 30810) reads CPUID function 1 and sets its SSE path from EDX bit 25 and its SSE2 path from bit 26, with no vendor test, so the Eden gets the same kernels as an Intel CPU. The driver cannot make this part faster. Without a transform unit in the HCM (30.3) the HAL offers no hardware vertex processing, so this quarter of the frame is a floor. A profiler proxy of the same name (D3D8PROF's d3d8.dll) loads beside it: name modules by full path (32).

### 31.5 Frame times across builds

D3D8PROF, gameplay frames (more than 5,000 primitives):

| Build | Frames | Mean ms | Median ms | Primitives a frame | Draw us a primitive |
| --- | --- | --- | --- | --- | --- |
| .523 | 1,452 | 119.4 | 118.3 | 24,585 | 3.86 |
| .524 | 1,121 | 112.7 | 109.8 | 23,993 | 3.68 |
| .525 | 1,187 | 120.3 | 120.5 | 25,114 | 3.78 |
| .526 | 1,227 | 120.3 | 120.7 | 25,408 | 3.75 |

The .525 and .526 runs drew 5-6% more primitives a frame than .524's. Per primitive they were 2-3% slower, which matches IXCHK's loss (.525) and FASTCHK's and TEXSHADOW's (.526). With those three gone, .524's 3.68 us a primitive is the base to beat.

### 31.6 Other findings

- **Dropped primitives.** 13,751 in 178 s (about 0.05% of those drawn), all bad-vertex drops (reason 2000): x, y or z not finite in 32,624 vertex checks, rhw 0 in 16,787. Every run since .519 shows 12,000-20,000. These come in DrawPrimitives2's vertices already transformed. Next: find them in the g.rec replay and check whether those triangles would reach the screen.
- **One refused packet (open bug).** VXD3D.LOG's oldest session is a test tool run before the .524 GTA3 run, most likely VXDELTA v1.0, the run whose two-texture picture differed in every pixel. In that session the VxD refused a one-packet batch as VX3DN\_BAD\_PACKET (event 7: op 1, textured, texture format 4, target format 1, no depth). The HAL then dropped the context, and every DrawPrimitives2 call returned DDERR\_SURFACELOST until the program ended 1.5 s later. The HAL's own checks passed that packet and the VxD's refused it, so the two disagree. Not seen again in VXDELTA v1.1, VXSHADOW, VXSETUP or GTA3. Next: log the VxD's refusal reason with the packet (events 8-10 are written only on the single-submit path), then rerun VXDELTA's toggling.
- **Measuring overhead.** ACCEL\_D3D\_SAMPLE and ACCEL\_D3D\_LOG were on in all three GTA3 runs (VXD3D.LOG). VXD3DMON's own counters cost 0.92 ms a frame plus about 450 cycles a triangle (the geometry counter and clock reads), about 8 ms a slow frame. Times are comparable between runs but slower than the game alone. Turn SAMPLE and LOG off for timing runs; the RTC sampler has replaced SAMPLE.
- **Paging.** About 800 page-ins a second during slow seconds with no swap file in use, so these are file reads (GTA3 streaming). The DrawPrimitives2 calls sampled had almost none (19 in the 1 ms+ class), and the calls of 10 ms or more had 0. The long calls were preemption: the thread ran 52 of 409 ms.
- **Menus.** Other threads' 16-bit code (CS 014F and 003B) took 42.4 ms of an 83 ms menu frame; the driver took about 2.6 ms. Same as .525.
- **Long frames.** The 8 longest frames (377-1,607 ms) were spent outside the driver's callbacks, except 135 ms of surface creation in one.

## 32. Telling which check refused a packet

A refusal that reports only BAD\_PACKET cannot be traced: the validation and the emitter have 61 exits, and the VxD adds its own. Build a refusal record in from the start, on both submit paths (single packets and batches).

1. **Note the line at every exit.** In vx\_native3d\_hw.c every refusal returns through one macro, which costs a single store, and only on the refusal path:

```c
uint32_t vx3dn_bad_line;
#define VX3DN_BAD_HERE (vx3dn_bad_line=__LINE__,VX3DN_BAD_PACKET)
/* every exit: */ return VX3DN_BAD_HERE;
```

2. **Note the VxD's own refusals separately.** These are an empty batch, and a packet whose size or op is wrong (checked on the caller's memory, then again on the copy). Note their line in a second variable, so the record says which file refused.
3. **Keep the last refused packet.** At the submit's exit, a BAD\_PACKET result fills vx3dn\_bad (table below). In a batch, record the copy that was validated, except after the size check on the caller's memory, where only the caller's packet exists. Add the packet's index and the batch's size.
4. **Make it readable.** TEXDBG BC EC returns words 0-5, ED words 6-11, EE clears the record. Count these as reads: they must not make the next packet send its whole state again.
5. **Log it in the HAL.** With ACCEL\_D3D\_LOG = 1, for any refusal on either path, write the packet as the HAL sent it to VXD3D.LOG as callbacks 8-10. For a prepacked packet, log its first words, not the empty vertex array. For a BAD\_PACKET, also write the VxD's record (callbacks 13 and 14).

| Word | Contents |
| --- | --- |
| 0 | Refusals counted |
| 1 | Path << 24 (1 single submit, 2 batch); 1 << 16 + a line in vx\_native3d\_hw.c, or 2 << 16 + a line in vx\_native3d\_vxd.c |
| 2, 3 | The packet's index in the batch; the batch's size |
| 4 | op << 24, texture on << 20, texture format << 12, depth format << 4, target format |
| 5 | Vertex count, pre.on << 16, bursts << 20, validated by its geometry alone (state deltas) << 31 |
| 6 | The state ranges the HAL marked the same as the packet before |
| 7, 8, 9 | Target offset; target width << 16 \| height; texture offset |
| 10, 11 | The first vertex's x and y words |

Callback 13 carries the result, word 1, index << 16 | size, word 5 and word 6. Callback 14 carries words 4, 7, 8 and 9. To test the record, send the VxD an empty draw packet as a single submit, then as the first of a batch of two, then with a wrong size in a batch. Each must be counted once, with the right path, file and index. Then draw a frame and check that it is unchanged.

Rules:

- The HAL's checks and the VxD's must accept exactly the same packets. A packet the HAL passes and the VxD refuses is a bug in one of them, never a fallback. Both take the guard band from the same switch (feature word bit 23). The HAL zeroes non-finite texture coordinates before the VxD sees them.
- The first packet of a batch is always fully validated: state deltas (29.5) apply only from a call's second packet.
- When naming a module from sampled addresses, match it by its full path. A proxy DLL of the same name can be loaded beside the system's, as D3D8PROF's d3d8.dll is in the game's folder.

## 33. Several textures in one indexed packet (texture runs), and the batch call's sequencer writes

A game changes only the texture between most of its draws (GTA3: about two packets in three). A texture run lets one indexed packet carry those draws: the packet's own state is drawn first, then each run swaps the unit-0 texture at a fixed point in the index list. Always on: the Direct3D HAL turns it on in the core; the OpenGL ICD's core leaves it off.

### 33.1 The packet form

- `vx3dn_texrun {at; surface; filter, address_u, address_v; mip}` and `vx3dn_texruns {n; r[15]}` go at the END of the packet, so every older offset and the V1 size (1,072, which subtracts the runs too) stay put. The packet is 27,260 bytes, with the runs at offset 25,816 (33.7).
- Only on an indexed packet (pre.on 2), textured, one unit, no stage layout, no S3 stages (`VX3DN_TEXRUN_FORM`). Anything else with runs.n > 0 is refused.
- `at` is an index position: increasing, a multiple of 3, above 0 and below the index count. Each run's texture goes through the same tex\_ok and mip\_ok as the packet's own.

### 33.2 The HAL's merge rule

- At ix\_open, when the template changed and the open batch is an indexed TEXRUN-form packet with room, compare the two templates' state: ranges 0, 2, 4, 5 and 6 whole, range 3 without the mip chain, tex.enable and mode equal. Equal except the texture → add a run at the current index count; the same texture → no run; a run with no triangles yet is replaced, not added.
- The conversion key (vertex layout offsets, texture coordinate index, program, units, ZBIAS) decides how vertices are converted. A different key starts a new index table in the same packet (ix\_new), or the old table's vertices would be reused with the wrong conversion.
- A texture stage state command (DrawPrimitives2 op 25) normally sends the batch. When a run can be taken it must not: keep the batch open and reset only the template. Do this only with the batch call (submit\_n) and only offscreen or with PRIMBATCH, as the flushing path requires.

### 33.3 Busy retry

The batch now outlives a state change. Before taking the run, keep the batch's state as a queue epoch (cq\_keep\_state) and remember which epoch is the batch's (bq\_ep, reset with each new batch). Detect whether cq\_keep\_state made a new epoch by comparing the epoch count before and after; taking the latest epoch blindly can pick an older one. A WASSTILLDRAWING answer then restores the batch's first state and the retry builds the same packet.

### 33.4 The VxD

- copy\_packet\_d reads runs.n once, caps it at 15 and copies only those runs.
- The group room check is FX\_PACKET\_WORDS + n × 100 words (a run's set-up is under 100).
- The emitter splits each burst at the run boundaries. At each one: type 4, registers 20, 21, 22 and 2C set to 0, the topology forgotten, then the texture cache strobe, the unit's words (texture\_setup with units only) and the texture experiments. Patch the packet's texture and mip from the run, emit, and put them back.
- After a packet with runs, the state cache (fx\_prev) takes the last run's texture and mip, so the next packet's compare is against what the engine holds.

### 33.5 Proving it

Merging packets changes the emitted words, the fetched-vertex hash (it folds each burst's mode once per burst) and the register model, so none of these can prove a merge. Use two hashes that do not depend on where packets and bursts split:

- The index-state hash: every index, with the registers its draw ran with.
- The fetched-by-index hash: every fetched vertex, by index.

Both must be equal with runs on and off over the whole g.rec replay. With runs off, every older hash must reproduce exactly. Then run the same on the built VxD in the emulator, from the vertex area and inline, with the refusals, and with a busy retry that must restore the stage state and make the same packet. VXRUNS on the board: 16 calls a frame cycling 4 textures (and the blend toggled every 4th call), six shots each way, at least 8 runs a frame. g.rec: 573.6 → 321.8 packets a frame, 7% fewer command words.

To test that other state still cuts a packet, count the VxD's packets with runs; it must go up when state changes between draws. Don't use the HAL's count of template changes that cut a packet: a render state (op 8) sends the batch before the next draw opens it, so that count never sees it. VXRUNS v1.0 checked the HAL's count and failed a correct driver.

Board, GTA3 (.528): runs save 11.5 ms a frame (paired median of 10 A/B turns, range +3.5 to +19.7). The VxD drew 186k packets with 515k texture switches inside them. Once runs are on, the most common reason a batch is sent is that 192 vertices are full (160k), ahead of a state change (102k).

### 33.6 The batch call's sequencer writes

The batch call's open writes SR10, SR1A bit 3 and SR1E bit 0 to ungate the 3D engine, and the close puts them back. Read each and write only when the value would change. Read SR3C in the same interrupts-off section as the open, and skip the close when the open changed nothing. Always on; TEXDBG BC FA reads the counts. In the emulator the sequencer must read the same after every batch.

Board, GTA3 (.528): 2.09 of the 3 writes skipped per call, and 70% of closes had nothing to put back. Measured saving: 2.1 us a packet (range +1.4 to +3.5).

### 33.7 Indexed packets of 384 vertices

An indexed packet carries its vertices once in pre.w and its indices in the vertices\[\] array (16-bit, at most 1,920). Raising the vertex limit cuts the batches sent because 192 vertices were full (on .528 the most common reason). Registry ACCEL\_D3D\_IX384 (default 1; VX\_D3D\_Switch 16; the VxD's answer out\[2\] bit 6).

- Only pre.w grows, to 384 x 10 words (VX3DN\_IX\_VERTICES). Every array of 192 (uv, spec, q, r1, uv1, vertices) stays, so a packet that is not indexed keeps 192. The packet becomes 27,260 bytes; the runs move to 25,816.
- The VxD's copy-in reads pre.on once and takes the 384 limit only for the copy that goes the indexed way. Reading it twice lets a caller switch the form between the reads and overflow an array of 192.
- The validation takes up to 384 only when pre.on is 2, and refuses more than 192 with a layout or third coordinates (q and r1 hold 192).
- The HAL's queue copy and the queue's own gate take 384 for an indexed packet. Missing either one silently cuts the packet back to 192 or sends it outside the batch.
- The vertex area keeps slots of 7,680 bytes (192 x 40). A packet with more vertices takes two side by side; both must be free, and the run starts again at slot 0 when it would pass the last slot. With no free pair it is drawn with its indices expanded inline, as before.
- The HAL's hash of source indices grows to 1,024 entries, so it never fills at 384.
- The pass-through now fits wider spans, so a packet can hold up to 32 vertices a run that no index uses. The words drawn are the same.

Proving it: the replay's index-state and fetched-by-index hashes stay the same at 192 and 384, and at 192 the emitted words match the build before. Run the 17x17 grid through the built VxD in the emulator as one 289-vertex packet in two slots and as two packets at 192; check that a two-slot packet at the end of the slots wraps to slot 0. g.rec: 321.8 to 256.8 packets a frame, 3% fewer command words.

### 33.8 After a forget, the next packet sends its whole set-up

`vx3dn_forget` (anything that writes type-01 or texture registers outside the set-up, a mode set, a refused packet, most TEXDBG writes) clears the shadows, and it must also clear `vx3dn_r01`, the record that the last set-up wrote 01/01. Each of the three set-ups sets `vx3dn_r01 = 1` after its REGL 01/01. A vertices-only or textures-only packet is allowed only while `vx3dn_r01` is 1; otherwise it is sent as a full packet:

```c
if((vx3dn_vertices_only||vx3dn_tex_only)&&!vx3dn_r01){vx3dn_vertices_only=0;vx3dn_tex_only=0;}
```

.529 removed `vx3dn_r01` together with HWCULL (whose 01/01 bit it also tracked) and lost this rule: a short packet could follow a forget and draw with registers the engine no longer held. .530 put it back. Before removing any variable, list every rule that reads it, not only the feature it was added for.

### 33.9 The registry policy (.530)

`HKLM\Software\VX85598` holds only: NOEDIDHIGHRES; the display set-up (VIDMEM\_MB, EDID\_MODES, CUSTOM\_TIMING, TIMING\_1152\_CVT); the logs (ACCEL\_D3D\_LOG, ACCEL\_D3D\_SAMPLE, ACCEL\_DEBUG\_LOG, VXGL\_LOG, VXGL\_FRAMELOG, VXGL\_SAMPLE); the feature under test (now ACCEL\_D3D\_IX384, VX\_D3D\_Switch 16); and the two profilers ACCEL\_PROFILE and ACCEL\_OPENGL\_TSCPROF. Every other value is ignored; VXREG lists them and `VXREG CLEAN` deletes them.

- A new feature gets one DWORD, one `VX_D3D_Switch` id and one VXD3DMON A/B entry while it is tested. Once decided, it is baked in at the value the board ran, or removed with its code, its tests, its switch id and its TEXDBG words. Ask the owner before keeping any other switch.
- Removing a switch: fix it at the value the board runs today (its default, not blindly on). When the code behind it is a measured loss, an unsupported feature or a diagnostic that is off by default, delete the code and its tests instead. Numeric tuning values keep their numbers.
- Every removed TEXDBG word gets the default reply (eight zero words); every removed `VX_D3D_Switch` id answers 0xFFFFFFFF. The test tools treat those answers as "not there".
- Development hooks still kept and marked `DELETE WHEN DEVELOPMENT IS FINISHED` in the source: ACCEL\_PROFILE and TEXDBG 6C/6D, BC A2/A3; ACCEL\_OPENGL\_TSCPROF; the 2D live control (VX\_IOCTL\_2D\_CTRL 0x85500012) used by the VX2 tools. Several VX2 tools (VX2CUR, VX2FAST, VX2STRX, VX2STR2, VX2ASYNC, VX2OVL, VX2TUNE) still write registry values the driver no longer reads; they go with the hooks.
- After the removal, the host replay of g.rec must give the same hashes as before (33.7's values): a removal that changes an emitted word changed behaviour.

### 33.10 Primitives whose vertices all have rhw 0

GTA3 draws its car light streaks as line lists of FVF 0x144 (XYZRHW, DIFFUSE, TEX1: 28 bytes, one coordinate pair) at z 0 with rhw 0 on every vertex, untextured, colour from the diffuse only (stage 0 SELECTARG2 DIFFUSE). The VxD refuses a vertex with rhw 0, so up to .530 the HAL left these out (drop reason 2000): 20,167 primitives in a 240-second run.

The rule (.531): a primitive whose vertices **all** have rhw +0 or -0 is drawn with rhw 1 when nothing in the state reads rhw:

- no table fog (FOGENABLE with FOGTABLEMODE set: fog from w);
- no w-buffer (ZENABLE 2).

With the same rhw on every vertex the perspective weights are equal, so any rhw gives the same picture; 1 is the value the engine accepts. A primitive that mixes rhw 0 with other values, or comes in a state that reads rhw, is still left out.

Where it goes: `valid_vertex` answers `VX_VERTEX_RHW0` for rhw ±0 with x, y, z finite (never cached as checked); every place that leaves a primitive out for a bad vertex calls `bad_prim` instead, which sets `rhw0_fix` for that primitive; `vertex()` (the general path's reader) reads rhw 0 as 1.0 while it is set. Every fast path already declines rhw 0 (their tests want rhw positive), so these primitives always take the general path. Clear `rhw0_fix` before each check, at each DrawPrimitives2 call and at the ICD's begin. QueryPhases word 242 counts them.

Test: the same quads and lines with rhw 0 and with rhw 1 must give the same pixels (VXRHW v1.1; v1.0 built 24-byte vertices for this FVF, so Direct3D refused every draw), and the same packet words unbatched (test\_core). Board, GTA3 on .532: 8,527 such primitives drawn, the left-out count down from 20,167 to 8,890 (the rest have x, y or z not finite).

**A texture doesn't stop it (.545).** With one rhw on every vertex, the texture coordinates interpolate the same for any common rhw (affine), so a textured primitive with all rhw 0 is drawn with rhw 1 too. .531 excluded a bound texture for no reason; GTA3's textured coronas were left out (31,244 rhw-0 vertices in a .544 run). Board, VXRHW v1.3: textured quads and lines, rhw 0 against rhw 1, 0 pixels differ, 240 of 240 counted. Non-finite x, y or z still leaves a primitive out: there is no picture to draw.

### 33.11 Template builds: keeping answers that seldom change (tried in .531, removed in .533)

**Failed approach; don't repeat it without a new reason.** A template build is needed whenever the render state changes (GTA3: about 425 a frame, 11.3 µs each on .530). .531 kept two of its parts' answers. On the board (GTA3 on .532) a build still cost the same: composing 2.2 → 1.8 µs, the texture lookups 3.5 → 4.0 µs. There was no saving in the frame, so .533 removed both. What was tried:

- **The composed program.** `vx_d3d_compose` reads only the texture stages up to the first disabled one plus stages 1 and 2 (words 1-31, word 0 only as bound or not), RENDERSTATE 29, its `bound` argument and `vx_d3d_feat`; it starts from a cleared result and keeps no state between calls. Keep 8 entries keyed by exactly those values (least recently used replaced). In g.rec 99% are found again. If the composer is ever changed to read anything else, add it to the key; test\_core checks kept answers against freshly composed ones for states one word apart.
- **The texture answers in the HAL** (resolve, not-a-cube, mip chain, colour key). Each build looked the handle up three or four times, each lookup checking the surface objects with IsBadReadPtr again. Keep them for one DrawPrimitives2 call only: the runtime holds DirectDraw's lock for the call, so no surface is created, destroyed or given new memory inside it except through the commands handed back to the runtime (TEXBLT and the rest, through `unknown`) and SETRENDERTARGET. End the kept answers at each of those and at every call. Keep only successful resolves, so a texture whose memory arrives later is still found. Keep a cube's faces only as "not a cube".

QueryPhases is back to 270 words. The cost of a build is spread over its parts. Any further saving needs fewer builds, not cheaper parts of one.

### 33.12 384 vertices an indexed packet is permanent

From .531 every indexed packet takes up to 384 vertices, and from .533 the limit is the constant `VX3DN_IX_VERTICES` (no variable, no switch). A test that needs several packets uses more vertices than one packet holds, or separate DrawPrimitives2 calls. `VX_D3D_Switch` itself was removed in .533; a feature under test brings back its own switch and A/B, and both go once it is decided.

### 33.13 Rules from the .530 reviews (.532)

- **A ring flip is done only when its fence comes.** If the ring stops first (a hang's retire, a reset, a fresh ring start, the wait's time-out), the flip is lost, not done. Decide from the fence itself. A lost flip's address is written by the CPU (CRTC 0C/0D/48/34), CR32 bit 0 is cleared, and from then on it is treated as a CPU flip just written: the old front stays protected until that flip is taken at a blank. Writing the same address again is harmless if the flip did run. A mode set clears the pending flip, its old front and the lost flag together.
- **RECOVERED means "the engine was reset"; `done` counts the packets that were issued.** A packet that was issued is never sent again, because it may have run, and blending or stencil would apply twice. RECOVERED with nothing taken means the reset came before any packet was issued, so the queue is sent once more (D3D core and ICD queue). If the same answer comes again, the queue is dropped.
- **Validate the whole region the registers name.** A stencil draw or clear on D24S8 programs the plane (01/28-29) and the region after it (01/2A), even though Coarse Z itself is off. Check the whole block, `VX3DN_CZ_END`, against the end of video memory and against the target, with a wrap check. The HAL's `VX_D3D_surface` already bounds the same block.
- **Read the caller's packet words once.** Every value that chooses how much to copy or how to lay it out (vertex count, `pre.on`, the bursts) is read once into ring 0's copy. The copy, the validation and the emitter all use that copy.
- **A BI\_BITFIELDS DIB's masks are at offset 40 in every header form.** For a 40-byte BITMAPINFOHEADER they follow the header; for V2/V3/V4/V5 (52/56/108/124 bytes) they are inside it. Any other size goes to DIBENG.
- **A present that falls back to GDI shows only what DirectDraw would.** For a single-buffered or front present that is the damage rectangle: point the DIB at its first row and give the DIB its height (no source y in a top-down DIB). A new GL surface is the window's pixels or black. If the blit and the colour fill both fail, the CPU clears it; a surface that cannot even be locked is not marked loaded.
- **The surface table grows by what is free, up to doubling.** One block (128 slots) holds any one registration, so failing because the table cannot double is wrong.
- **An overlay flip changes what DestroySurface must protect.** Record the shown offset on every successful overlay flip; a refused flip, which the VxD hides, clears the record.

### 33.14 Transform and lighting: where the work is, and how to find out who would use it (.534)

- **Direct3D 8's software vertex processing is already SIMD on this CPU.** D3D8.DLL 4.09.00.0904's hottest pages in GTA3 (+A4000, +AB000) are packed SSE code with SSE2 (`cvtdq2ps`), `movmskps` clip codes and `rcpps` for 1/w. A driver-side T&L can win only by removing passes over the vertices, never by redoing the same math in SSE.
- **This HAL is a DirectX 7 DDI** (no GetDriverInfo2). Offering D3DDEVCAPS\_HWTRANSFORMANDLIGHT moves every game that picks Direct3D 7's T&L HAL device or Direct3D 8's hardware or mixed vertex processing at once, onto commands 33-36 and 44 (SETMATERIAL, SETLIGHT, CREATELIGHT, SETTRANSFORM, SETCLIPPLANE) and untransformed vertices. There is no per-game opt-in.
- **Command lengths** (a 4-byte header, then n entries): SETMATERIAL and SETTRANSFORM 68 bytes each; CREATELIGHT 4; SETCLIPPLANE 20 (index, plane\[4\]); SETLIGHT 8 (index, data type) plus a 104-byte D3DLIGHT7 when the type is D3DHAL\_SETLIGHT\_DATA (2; ENABLE 0, DISABLE 1).
- **Advertising caps does not change a game's choice after the fact:** a Direct3D 8 game picks its vertex processing at CreateDevice. To learn what it would pick, show it the caps and log the flags. D3D8TLP (the D3D8PROF source built with -DTLPROBE) does this without the driver: the caps show hardware T&L, the flags asked for are logged, and the device is made with software vertex processing so the game draws as before.
- **Direct3D 7 has no such proxy:** DDRAW.DLL is imported by the other DirectX DLLs and exports undocumented internals, so a proxy is a gamble. Use the driver key ACCEL\_D3D\_TLPROBE instead: the HAL offers the caps, reads past the T&L commands, leaves out and counts the draws of untransformed vertices (QueryPhases 270-289), and VXD3DMON v9.8 reports which games chose it. Delete the key and its code once the games are probed.
- **Where repeated work lives on our side** (GTA3 .533, an indexed vertex): the HAL reads the TL vertex and writes 8 words to the queue; the VxD copies them in (6.4 us a packet), validates (8.0), copies to the vertex-area slot (3.5); then the indices go out one register write each (about 56 cycles an index, 10.4 us a packet). Hardware T&L would remove only D3D8's TL write and the HAL's read. The VxD passes are ours, are paid by every Direct3D game, and come first.
- **Shares by function:** use VXD3DMON SAMPLE (the RTC sampler alone) with VXDDRAW.MAP beside it; the HAL's time comes out by function from the map (statics included, `option statics`). The token loop's counters cost 447 of 1,280 cycles only on the triangles they sample (one in 16), so they barely move the shares: GTA3 on .534 gave the HAL 35.8% with them off against 36.5% with them on.
- **Toolhelp on Windows 98 need not list the game's .EXE first.** Pick the game's module by its .EXE extension (VXD3DMON v9.1-9.7 printed D3D8.DLL's pages twice and never the game's).

**GTA3's answer (D3D8TLP, .534):** shown hardware T&L in the caps, it asks CreateDevice for hardware vertex processing (behaviour 0x40). Its draws: 98.5% of primitives untransformed (FVF 0x142 position, diffuse, one texture set; 0x112 position, normal, one set; 0x102), 1.5% pretransformed (0x144). Of the untransformed: vertex fog 98%, lit 25% (1 to 3 directional lights, never point or spot, no specular), texture coordinates generated with a texture transform 7%, Direct3D's clipping off 41%, no vertex blending, no clip planes. Indexed ranges are 1.70 vertices a triangle, the same as the vertices the triangles use: no range waste to recover. A driver T&L for GTA3 needs that set, and a clipper for the 59% with clipping on.

**Check a sampled hot spot's cause before fixing it (.535):** VXD3DMON names a 64-byte line after the function its first byte lies in. Watcom's memset is `__STOSB` (it aligns, then calls `__STOSD`), so the bulk store loop shows up under `__STOSB`, and the time belongs to whichever memset callers are hot. .535 cleared the vertex-check cache by range on the guess that it was the caller; the replay showed that clear was 32 bytes a call, and `__STOSB` stayed at 0.9% in GTA3. Measure a guess on the replay first (count the bytes or calls it would save). Hash lookup tables by a multiplicative hash of the handle (`(h*2654435761)>>20` for 4096 entries), not its low bits, but in GTA3 the hashed surface hints moved `surface_find` only from 1.4% to 1.2%.

**Before optimizing a measured phase, read what it contains.** The VxD's 10.4 us a packet labelled "index bursts" is mostly the texture runs' switches written between the bursts; the index words are already inline stores. A fused copy-and-check of the indexed vertices only moves a cached copy unless the HAL writes the vertex-area slot itself (.526's IXCHK, a check loop inside the copy, lost to `rep movsd` plus a separate check).

### 33.15 The driver's own transform and lighting (.536, ACCEL\_D3D\_TNL)

- **Where it sits:** `src/vx_d3d_tnl.inc`, included by the core before `dp2_body`. A call whose FVF position is XYZ is drawn from a scratch buffer of pretransformed vertices (XYZRHW, DIFFUSE, SPECULAR, one or two sets). Each draw's triangles have their vertex range transformed and appended to it, then the draw goes on as an indexed list (26) into the buffer. Everything after that is the existing pretransformed path.
- **Keep the scratch buffer append-only within a call.** Then one pointer serves the whole call, the indexed packets keep merging across draws, and the vertex-check and index caches stay valid. Restart it, sending the batch first and resetting `ix_vb` and `vv_fresh`, only when it fills or the record size changes.
- **The T&L state lives outside `vx_d3d_context`** (`tnl_of`, per context pointer), so recordings and the host structures keep their sizes. The recorder writes it as record 6 and the replay restores it. Without it, a recording starts after the projection and light data were sent, and its geometry falls outside the far plane.
- **The DX7 DDI command values:** SETTRANSFORM type 1 world, 2 view, 3 projection, 16-23 texture; SETLIGHT data is a D3DLIGHT7 (type 1 point, 2 spot, 3 directional); ZRANGE gives minZ and maxZ for the viewport's z.
- **Clip in clip space before the divide**, at z >= 0 and z <= w only. That removes every w <= 0 vertex. Leave x and y to the guard band and the screen clipper. Interpolate new vertices' colours, coordinates and position before the divide.
- **The pretransformed path reads TEXCOORDINDEX and TEXTURETRANSFORMFLAGS.** For a transformed draw, point each active stage at its own output set with the flags at 0, and put the application's values back before the next command.
- **No normals with LIGHTING on** gives ambient and emissive only, as Direct3D does. Material sources follow COLORVERTEX and DIFFUSE/AMBIENT/EMISSIVEMATERIALSOURCE. A 2D set under a texture transform is the vector (u, v, 1, 0); a generated set is (x, y, z, 1).
- **Direct3D 8's hardware vertex processing on this DDI indexes past `dwVertexLength`** (GTA3 appends to its vertex buffers). With T&L offered, bind every vertex buffer whole, whatever its vertex format, bounded by the surface's size, and accept a dwVertexLength of 0. Binding only untransformed buffers whole left GTA3's pretransformed draws failing with DDERR\_INVALIDPARAMS (.537).
- **Keep the HAL image under 1024 pages**, the VxD's lock limit for ACCEL\_LOCK\_HAL. That is why the scratch buffer holds 16,384 vertices.
- **No C library on the host:** the core uses its own square root (a seed from the float's bits, three Newton steps) and its own e^x.
- **Leave vertex buffers out of the surface table.** In `CreateSurfaceEx`, return DD\_OK for a DDSCAPS\_EXECUTEBUFFER surface before any table work. Direct3D 8's hardware vertex processing creates and releases vertex buffers through CreateSurfaceEx, and their handles can equal a texture's or the render target's. A match invalidates that slot and loses the context: every later DrawPrimitives2 call answers DDERR\_SURFACELOST and the same frame shows over and over (.536, GTA3). This DDI never names a vertex buffer by handle; DrawPrimitives2 passes `lpDDVertex`.
- **No C casts from float to integer in per-vertex code.** Watcom's `__CHP` saves, changes and reloads the x87 control word on every cast (13% of GTA3's frame in .536). For a value in 0..255, add 1.5 x 2^23 through a volatile float and take the low mantissa bits (`tn_round`).
- **Lose a context only when its own target or depth surface object goes.** Match the surface object (`target_key`, `depth_key`), never the handle number. Under hardware vertex processing another surface can carry the render target's handle number while the target lives on; matching the handle lost GTA3's context after 40 s of gameplay (.537). A target destroyed, or made again with another layout, still loses it. Record which path lost it and the handle in the reason's value.
- **Drop a triangle with a non-finite transformed position whole.** Mark the vertex outside both planes and skip clipping: interpolating towards a NaN makes more NaNs, and the core refuses them as bad vertices.
- **Every path that marks a context lost records why.** That includes the core's submit failure after part of the call completed: record it as reason 26, with the VxD's answer, the opcode, and whether packets had completed. In .538, GTA3 went SURFACELOST with no reason counted, because this was the one path that set `lost` silently.
- **Never send an empty draw packet.** Open a new indexed packet only for a triangle an empty packet can hold (index span up to 384), and make the flush drop a draw packet with no vertices or no triangle. .539's `ix_pass` opened one for a triangle wider than any packet; a bad vertex then stopped `ix_list` before it added anything, and the VxD refused the empty packet (line 223, the vertex count), losing GTA3's context. Wide spans come from the T&L: a clipped triangle's new vertices sit at the end of the scratch buffer.
- **Output FVF is C4 plus the set count** (`0xC4 | sets << 8`), stride 24 + 8 per set. `0x1C4` already says one set, so adding the count to it named three sets for two and the core refused the draw.
- **Normals by the inverse transpose, row vectors:** n'(j) = Σᵢ n(i)·C(i,j)/det. Make them unit length only when NORMALIZENORMALS is on. Test with a rotation, not only identity.
- **Stop a non-finite vertex before any per-vertex math**, and never convert a possible NaN to an integer: a NaN fog argument made `tn_exp`'s halving loop run 2³¹ times.
- **Transform only the vertices a draw's triangles use.** Stamp the referenced indices; vertices between the lowest and highest index that no triangle uses can be unwritten buffer memory.
- **Bound TEXTURETRANSFORMFLAGS' count to 0–4 before indexing with it.** Interpolate a projected set as u, v, q across a clip, then divide.
- **The T&L state holds no pointer, and is reset when a context is made.** Keep the context key beside it, so recordings replay on a 64-bit host and a new context at an old address starts fresh.
- **Draw the T&L's triangles into the indexed packet directly** (`tn_ix`), not back through the pretransformed path. Write each vertex's words once, when it first enters a packet. The T&L's records are its own and finite, so they need no source checks (`vv_good`) or span pass (`ix_pass`). A vertex outside the guard band or depth range stops the run, and the per-triangle path clips that triangle on the screen.
- **Light a vertex once, and only when something uses it.** Do the position and clip code for every vertex a triangle references. Do the lighting, fog, texture coordinates and divide when the vertex enters a packet, ends a clipped edge, or goes to the per-triangle path. A vertex used only by dropped triangles is never lit.
- **Keep the scratch buffer dense:** a draw's vertices take its next places in the order its triangles first use them.
- **Convert colour channels with FISTP and an integer clamp** (Watcom `#pragma aux`), not the C cast (`__CHP`) or a store through a volatile. Under FNSAVE's default control word it rounds to nearest, and a NaN gives 0x80000000, which clamps to 0.
- **Write the position words in the first step**, with the divide and viewport. Then the guard-band and depth test runs before lighting, and a vertex entering a packet is lit straight into its packet words and its record at the same time. Do this only when the template takes the record's words unchanged (no ZBIAS, textured, an XP program, the record's layout); otherwise convert as before.
- **SSE kernels go in as bytes in `#pragma aux`:** the HAL's 486/x87 build refuses SSE mnemonics. Assemble them once with `-6` and paste the bytes. Check CPUID leaf 1 EDX bits 25–26 once, and keep the x87 code as the fallback. On the host, write the same operations in the same order with intrinsics, so host tests check the board's arithmetic. Don't use packed memory operands (`mulps xmm,[mem]` needs 16-byte alignment): load with `movups` first.
- **Pack a colour in one go:** multiply by 255, then `cvtps2dq`, `packssdw`, `packuswb`, with the lanes in b, g, r, a order. Rounding is to nearest, the saturation does the clamp, and a NaN becomes 0.
- **Compute material-sourced lighting once per draw:** the base colour (emissive plus ambients) and each light's diffuse×diffuse and ambient×ambient terms. A vertex then just adds its lights' terms. An unlit vertex keeps its diffuse word as it came.
- **Clip at all six planes in clip space**, not only near and far. The engine has no guard band, so a triangle partly off the screen otherwise goes through the core's slow per-triangle screen clipper (27% of GRID's triangles). Pin each new vertex to its plane, hold its screen position inside the viewport, and drop a triangle that is wholly outside any one plane. If a draw's clipped output wouldn't fit the scratch buffer, fall back to near and far only.
- **No small `memcpy` in hot code built with Watcom.** It compiles a 4-byte `memcpy` as REP MOVSD/MOVSB with their set-up. Use the core's unaligned types (`vx_f32a`, `vx_u32a`) and the `vx_fu` union instead.
- **Offer hardware T&L only when SSE is really usable:** FXSR, SSE and SSE2 in CPUID, *and* CR4.OSFXSR (the VxD's control 0xFFFFFE34, read at ring 0). CPUID alone doesn't show that the system saves the XMM registers. Then keep one SSE path, not x87 copies.
- **Look up per-context state once per draw**, not per vertex.

### 33.16 The driver's T&L: the result (a failed approach, kept off)

**Verdict.** On this board a driver T&L on the CPU does not beat Direct3D 8's own software vertex processing, even in its finished form. The code stays in the tree for a future idea, off by default: ACCEL\_D3D\_TNL = 0 (the VxD's default; TNL\_ON.REG sets 1 for a test). With the key at 0 the HAL offers no hardware T&L and every game uses Direct3D's. The pass rule was set before the work began: clearly faster than Direct3D's T&L on the same board, or it comes out of the default path.

**What was built (.536 to .544).** The HAL took the DX7 DDI's T&L commands (SETTRANSFORM, SETLIGHT, CREATELIGHT, SETMATERIAL, SETCLIPPLANE) and untransformed vertices, and did per draw:

- the world, view and projection transform, the clip codes against all six planes, the divide and the viewport;
- directional and point lighting with the material or the vertex colours, vertex fog (linear, exp, exp2, range), generated and transformed texture coordinates;
- clip-space Sutherland-Hodgman clipping at all six planes (a near/far-only fallback when the scratch buffer is full), with new vertices pinned to their plane and held inside the viewport;
- the triangles straight into the indexed packet (`tn_ix`), each vertex's words written once.

.544 (reviewed externally in several rounds) added four-wide SSE2 kernels for the transform and the lighting (src/vx\_tnl\_sse.asm, generated by tools/tnl\_asm.py), attributes finished only for vertices a kept triangle uses, lighting reused for repeated normals, and double-precision fallbacks for overflowing clip interpolations.

**GRID (VXTLP, 30 lit, fogged, textured grids, 34,560 triangles a frame, logging off):**

| Build | Direct3D's T&L | The driver's | Picture |
| --- | --- | --- | --- |
| .542 | 47.7 ms | 160.4 ms | the same |
| .543 | 45.1 ms | 87.8 ms | 0 of 29,597 lit pixels differ |
| .544 | 45.2 ms | 47.5 ms | 0 of 29,597 lit pixels differ |
| .547 | not run | 45.67 ms | 0 wrong |
| .548 | 41.59 ms | 44.46 ms | 0 of 29,597 lit pixels differ |

**GTA3 (.544, the 256 MB bench, VXD3DMON SAMPLE, gameplay):**

|  | T&L on (the driver's) | T&L off (Direct3D's) |
| --- | --- | --- |
| Median frame | 116 ms (8.6 fps) | 103 ms (9.7 fps) |
| 90th percentile | 155 ms | 148 ms |
| 99th percentile | 660 ms | 622 ms |
| Gameplay overall | 7.8 fps | 9.0 fps |
| Slow frames: HAL + Direct3D T&L | 74 + 0 ms (the HAL does both) | 39 + 25 ms |

With T&L on, GTA3's draws really went through it (708,450 draws, 25.8 million triangles, none left out, no context lost, no packet refused). The bench pages hard with 256 MB, so runs vary by about 10%; no run showed the driver ahead.

**Why it cannot win here.**

- Direct3D 8's software path is already SSE code tuned for the processor (33.15). Redoing the same math in SSE removes nothing.
- The kernels were about a tenth of the driver's T&L time. The rest is per draw (set-up, the frame of constants, about 700 draws a GTA3 frame), the clipper, the record-to-packet step and the C around the kernels. Those costs are the driver's own and Direct3D does not pay them twice.
- About 1.4 µs a triangle against Direct3D's 1.0 in GTA3. A direct-emission pass might save a few milliseconds, not the ten needed.

**What it left that every game keeps:** the empty-packet fix and the flush guard (33.15), the context-loss fixes, the ZRANGE check before use (every pair checked before the depth range is committed), and binding vertex buffers whole for the callers that need it.

**If a T&L idea comes back,** start from these numbers: it has to remove a pass over the vertices that Direct3D's path makes, not do Direct3D's math faster. Time it on GRID and GTA3 against Direct3D's T&L on the same board and session before anything else.

**The last round (.545-.548): the gap grew.** .548 added a packed index snapshot (an INDEXEDTRIANGLELIST2 token's indices copied once into the T&L's snapshot with their bounds in the same pass, instead of widened to 32 bits and narrowed back): GRID HW 45.67 → 44.50 ms. In the same session Direct3D's T&L took 41.59 ms against the driver's 44.46: 2.9 ms behind, more than .544's 2.3. The shared pretransformed path (33.17) sped up Direct3D's output by 3.6 ms on GRID; the driver's T&L, which uses less of that path, gained 3.0. Every gain in the shared path helps Direct3D's T&L more than the driver's.

**Not tried, and why:** the fully inside draw shortcut (skip classification and compaction when a draw's clip codes OR to zero) covers about 9 of GRID's 30 draws (10,368 triangles); closing 2.9 ms would need about 280 cycles saved on each, against a few dozen cycles of work removed. Dense vertex loads straight into SSE and cached T&L set-up were judged the same way: each removes less than the gap.

**Nothing in the driver speeds up Direct3D's own lighting:** it runs in D3D8.DLL (packed SSE on this processor) and the driver sees only lit vertices. The driver's levers are what happens to D3D8's output (33.17) and the caps that change how much D3D8 clips; the engine has no guard band, so taking D3D8's clipping into the HAL would only move it to a slower clipper.

### 33.17 The pretransformed indexed path: check spans and ranges once (.545, .546)

Every Direct3D game's draws reach the HAL already transformed. GTA3's go 96% through the indexed list loop (`ix_list`, its draws cull) and the rest through the pass-through (`ix_pass`, cull mode none).

- **Check and convert a span's vertices with SSE2 kernels** (`vx_d3d_span.inc`; `src/vx_hal_sse.asm`, generated by `tools/hal_asm.py`, which reuses `tools/tnl_asm.py`'s assembler). `vxs_check` loads x y z rhw of four vertices with `movdqu`, transposes them, and runs `ix_check`'s tests as biased signed compares: `(a - lo) ^ 0x80000000 > range ^ 0x80000000` is the unsigned guard-band test; z ≤ 1.0 and rhw - 1 < 0x7F7FFFFF the same way. It returns how many leading vertices pass; the C tests the rest. `vxs_conv` writes a span's 10-word records in one pass (-0 to +0 by `pcmpeqd`/`pandn`, coordinates not finite as 0) without a call or the template's fields a vertex. `vxs_max16` finds a token's largest index with `pmaxsw` on the indices xor 0x8000.
- **Keep the C as the reference.** The host and a processor without SSE2 run the C; a 32-bit freestanding test (`tests/test_hal_asm.c`, 60,000 random cases) checks the bytes against it. Gate the kernels on the same 0xFFFFFE34 answer as the T&L (SSE2 and OSFXSR), set once in the HAL.
- **Prove a draw's whole vertex range once.** The token's range base .. base + its largest index, when dense (at most 3 a triangle and 32 more), is checked with `vxs_check` before the loop. If every vertex passes, the loop checks none as they enter packets. The proof holds for the template it was made with. GTA3's recording: 1,987 of 2,394 tokens proven.
- **Drop a wholly off-screen triangle in the loop.** Three valid vertices strictly past the same clip edge (right and bottom exclusive) cover no pixel: count it as culled and go on. Stopping the loop sent it through the slow per-triangle clipper only to draw nothing. GTA3 turns Direct3D's clipping off for 41% of its draws, so its off-screen triangles reach the HAL.
- **Write a burst's indices through a local pointer in the VxD.** `DAT` reloads the global write pointer and end for every word under Watcom.
- **Check each vertex once (.547).** A pass-through span inside a range already proven for the same template is not checked again. GTA3 on .546 checked 39.1 million vertices for 19.2 million converted, because the proof and the span check both ran. Board, VXRHW v1.5: the proven grid's span skipped, one kernel check for the whole grid, 0 pixels differ.
- **Come back to the pass-through after a sparse run (.548).** A run refused as sparse (span wider than 3 a triangle and 32) used to send the rest of the token to the indexed loop (about 22% of GTA3's indexed-list triangles on .547). Let the loop take that run's triangles one by one, then call `ix_pass` again for the rest. On such a return `ix_pass` must begin no new packet: flushing a half-full packet to start a new one made 1.6 more packets a frame on the GTA3 replay, and a packet costs the VxD about 60 µs. With that rule the replay's packets and every hash are unchanged. Test: VXRHW v1.6 (the list's 1,536 vertices by index, triangles paired 100 apart, against the list in order), test\_core.
- **Try the one-test check before `valid_vertex` (.548).** In the per-triangle path's vertex pre-check, a vertex below `vv_lim` that passes `vv_good` (x y z finite, rhw positive and finite) gets `valid_vertex`'s answer without the call; mark it checked. GTA3 on .547: 1,226 cycles a triangle on 675,427 triangles there.

Test (VXRHW v1.3-v1.4, board): the 16×16 grid by index against the same triangles drawn as a plain list, with culling off and on (cull CW, Direct3D's clipping off), on the screen, with vertices past the left edge, and with its left columns wholly off the screen: 0 pixels differ in each; the kernels' counts grow (QueryPhases 302-310, VXD3DMON v10.2). The GTA3 recording's hashes don't change.

### 33.18 Draw packets held across DrawPrimitives2 calls (.550)

Why: the .549 VxD profile of GTA3 put the batch call's own parts (the dispatch, the entry, the open and close of the sequencer, and the work between and after the packets) at 22% of the VxD's 12 ms in a gameplay frame. That came at about 1.6 packets a VxD call, because the HAL sent its queue at the end of every DrawPrimitives2 call. Direct3D 8 on this DirectX 7 driver makes one call per vertex buffer, so GTA3 makes hundreds of calls a frame. Section 31.3 had estimated 3.5 ms a slow frame for this.

Recipe:

- At the end of a call (`cq_hold` in vx\_d3d\_core.c), keep what is queued if it is fewer than CQ\_MAX (32) packets. Then mark those packets' state epochs as having nothing to put back (0xFFFFFFFF), set `cq_epn` to 0 and `cq_held` to the count, and remember the context (`hold_c`).
- At the next call's start (`cq_take`), keep the packets when the context is the same; otherwise send them first under their own context.
- Every command that sent the queue before still sends it: state blocks, the render target, a clear, and every command the runtime parses (TEXBLT is one).
- In the HAL, `VX_D3D_hold_sync` sends the held packets before Lock (but not a Lock of an execute buffer; packets are full copies), Blt, Flip, DestroySurface, SetEntries, and a context's destroy (Destroy, DestroyAll, DestroyDDLocal).
- Only the process that queued the packets can send them, because the VxD handle is per process and the DLL's data is shared (`hold_pid`). The thread inside DrawPrimitives2 (`hold_tid`) does nothing in a nested callback: the core already sends its queue before such commands.
- A DrawPrimitives2 call from another process sends a live process's held packets and drops a dead process's packets. Process detach drops its own.
- Errors: a held packet's call already answered DD\_OK, so the runtime cannot retry it. In `cq_send_body`, if a held packet is refused, or is still busy after the HAL's own 500 ms of waits, drop it (or for busy, the rest of the held packets) and clear the next packet's delta tag (`clear_depth=0`). A loss is reported to the call that sent the packet, at that call's first queued command.
- Always on: the HAL sets vx\_d3d\_hold\_on at its set-up (d3d\_registry). .550's A/B switch, ACCEL\_D3D\_HOLD, was removed in .551 once GTA3 confirmed the gain. VXGL.DLL and the host tests keep the word at 0 unless they set it; the emulated callback harness clears it after DriverInit and sets it per call.
- Counts are in QueryPhases 316–331: the hold counts, the causes, the switch and the packets held now. VXD3DMON v10.7 prints them.

Result on the host (g.rec): 141.4 VxD calls a frame become 9.0, and every hash is unchanged, emitted words included. The flip goes into the ring (v10.417), so the last chunk sent at Flip is not waited for.

Rules:

- Never hold a packet that points into the runtime's memory. Every queued packet is a full copy, which is what makes this safe.
- Send the held packets before anything that reads, writes or moves what they draw. Flip moves surface memory (v10.166), so it must send them first.
- In the emulated tests (verification/d3d\_callbacks.py), the registry answer has bit 22 off for the cases written before .550; the new case turns it on per call (`h.hold`). The emulator cannot run the C library's memmove (it jumped to 0), so the drop path copies with a loop.

Test tool: VXHOLD v1.1. It draws 64 quads from two vertex buffers in turn (64 calls) and checks them against one call: through a Lock, and through Present read from the screen. It checks texture order: the quad drawn before the texture is refilled keeps the old texels. A MANAGED texture is locked directly; a DEFAULT one is refilled by UpdateTexture from a system-memory copy, because Direct3D 8 refuses to lock a DEFAULT texture made without D3DUSAGE\_DYNAMIC. It also times 300 frames. Read ACCEL\_D3D\_HOLD from QueryPhases only after CreateDevice; before that the HAL has not read the setting. Board, .550: VXHOLD v1.1 PASSED with the key at 1 and at 0, all four checks in each run, nothing dropped. The 64-call scene took 3.30 ms a frame with the key at 0 and 2.27 ms at 1 (31% less; v1.0's run gave 3.43 and 2.27).

GTA3 on the board, .550, VXD3DMON v10.7 SAMPLE, the key at 0 against 1, same machine and session:

- Median gameplay frame: 93 ms → 76 ms (9.6 → 11.2 fps median gameplay second). Average 107.0 → 87.4 ms; 90th percentile 130 → 107 ms; 99th 771 → 443 ms.
- The VxD in frames under 75 ms: 11.4 → 9.2 ms; in slower frames 22.5 → 17.3 ms. DrawPrimitives2 time: 39.4 → 34.7 ms a frame.
- Engine lock entries (about one per VxD call): 191,157 → 36,866. The batch call's own phases fell from about 19% to 8% of the VxD; the engine was busy in 52% of the VxD's time (28% before).
- Sends of held packets: 299,479 by the next call; 5,222 by a callback (Flip 2,288, Blt 2,126 (render-target copies), Lock 799, DestroySurface 9); none dropped.
- What leads the VxD now: copy\_packet\_d 19% (the indexed packet's vertex words, +1D0 alone 14.8%), validate\_x 12%, cap\_block 9.5%, setup 8.2%. The copy-in is the next target (the compact queue format).

### 33.19 Recipe: the VxD line profiler and how to read it (.549)

- The RTC sampler (IRQ 8) finds the interrupted ring-0 frame by scanning its own stack, from its ESP up to the thread's client registers (at most 16 KB). It looks for EIP ≥ C0000000, CS 28h and EFLAGS with IF set and the reserved bits clear. Then it counts the frame's 64-byte line. VXD3DMON names the lines from VXVXD.MAP, which must sit beside the EXE.
- To map a hot line to source, rebuild the object with `-d1` and the same flags (the code is byte for byte the same; check with `cmp` on the `wdis` output). Run `wdis -s=<source> -l`, then look at the function's start plus the line's offset, through the next 64 bytes.
- Samples cannot land while interrupts are off. They land on the first instruction after the `popfd`, so an interrupts-off section's whole time shows up there. Example: batch\_impl+370 (3.8%) is the sequencer section's port I/O, not the `test` it points at.
- SAMPLE must turn the VxD's word profiler off (texdbg2 BC A3). VXD3DMON before v10.6 left it on in every SAMPLE run (wp\_part 5.3% of the VxD). The A/B comparisons stayed fair, because both sides carried it, but the absolute times were high.
- The .549 profile (GTA3, the VxD's line samples): validate\_x 11.1%, copy\_packet\_d 10.2% (its +1B0 line 6.6%: the indexed packet's vertex words and indices, `rep movsd`), cap\_block 9.1%, setup 8.8%, vx3dn\_execute 7.1%, batch\_impl 5.5%, va\_write 3.7%, xr\_copy 3.7%, texture\_setup 3.2%.

### 33.20 Answers kept for a state: compose the pixel program once, not once a draw (.552)

**Status: built and tested on the host only; the board A/B against .551 is open (AB\_TEST\_552.md in the .552 tree).**

**The rule.** Anything a draw asks of the state, and not of its vertices, is worked out once for that state and kept until the state changes. The draw template already lives that long: `template_reset` ends it at every state command that changes a value, at the start of every DrawPrimitives2 call, and after every command the runtime parses. Hang every other per-state answer on the same reset.

**What .551 did.** `dp2_body` called `vx_d3d_supported` for every draw token. Its last test, `vx_d3d_texture_status`, ran `vx_d3d_compose` (the whole pixel program) only to read the error word. The template build then composed the same program again. GTA3's recording (g.rec): about 2,130 composes a frame for about 520 distinct states. The phase clocks never showed it, because the support check sits outside the template build they measure.

**Recipe.**

- One static record `ep`: the context, the composer's answer (`vx_d3d_xp`, its result and error word, the `bound` argument), the support verdict, and which of the two are valid. `template_reset` clears the valid bits.
- `ep_compose` returns the kept answer when the context and `bound` match, else composes. `template_body` takes a copy, because it edits its program for the colour key.
- `ep_supported` is `why_states` (every reason bit except 200) plus the kept error word. The lost flag is read at each draw and never kept: a failed send can set it inside an epoch.
- The driver's T&L swaps TEXCOORDINDEX and TEXTURETRANSFORMFLAGS around its draws (`tn_tss`) without a template reset. Give the record a key: 0 for the application's stages, sets + 1 under the override. An answer is used only under the key it was made with.
- Render states listed in `rs_inert` are applied without a reset. Neither the composer nor `why_states` may read one of them.
- Keep the public `vx_d3d_supported`, `vx_d3d_unsupported_why` and `vx_d3d_compose` as they are (ValidateDevice and the ICD call them outside a draw epoch).

**Proving it.** `tests/test_ep.py`: 3,000 random command streams, half of whose states make a draw unsupported, sent as one call and as one call a command. A call ends the epoch, so the second form works every answer out afresh; packets and drops must match byte for byte, and match the .551 core (`tests/run.py --ref`). The test must fail when `template_reset` no longer clears the record. Host, g.rec: every hash unchanged (emitted 13B0CC2A348FF817), core instructions a frame 9.68 to 7.95 million, core time -8% (-11% with the hold). On the board: VXEP v1.0 (64 quads with STENCILENABLE on a D16 depth buffer or COLORARG1 TFACTOR before every odd one, one call against 64; 256 draws a frame timed). QueryPhases 332-335 count composes done and answers used again.

**The index check, two a word (.552, same status).** The VxD checked an indexed packet's indices one at a time: 8 instructions and 2 branches an index in Watcom's code, 46,000 indices a frame in g.rec. An indexed packet has at most 384 vertices, so with K = 0x8000 - count in both halves of a word, `(x | ((x & 0x7FFF7FFF) + K)) & 0x80008000` is non-zero exactly when one of the two indices in x is at or past the count. OR every word's result and test once: 9 instructions for two indices. An odd last index is tested alone; a count of 0 or above 0x8000 takes the old loop. `tests/test_ixcheck.py` checks the arithmetic and 20,000 corrupted packets against .551's loop.

**Failed approach (host, not kept).** A run of the indexed pass-through refused because its span holds a vertex outside the viewport that none of its triangles uses: cutting the run to the triangles whose vertices pass and filling the failing records with a copy of a good vertex kept the by-index and index-state hashes, but made 5 more packets a frame (a span takes every vertex between its lowest and highest index) and 1.4% more core instructions than the per-triangle path. Also not worth building: a template kept across calls (3% of g.rec's builds repeat the last call's state) or within a call (28% repeat, but the textures must be resolved again; .531 measured no gain).

### 33.21 Rules from the .553 merge and audit

**Status: host only, as 33.20. .553 is .552 with the outside .552 changes merged and both audited.**

- **Anything kept for a state ends in `template_reset`, and every writer of that state calls it.** .552 missed three writers: `template_status` (ValidateDevice runs outside a DrawPrimitives2 call and took the last call's composed program), `vx_d3d_init` (a context created at a destroyed one's address, the case .540 already fixed for the T&L state), and the state put back after a failed send in `cq_send_body`. When adding a kept answer, list the writers by grepping for assignments to `state.rs`, `state.tss`, `state.viewport` and whole-struct copies of `c->state`, not by reading the command loop.
- **Test a kept answer from outside the call too.** `tests/test_ep.py` part 4 runs a stream, initializes the context again in place with other stages, and compares ValidateDevice's answer with a context at another address. It fails on .552.
- **Keep the span kernels' constants per template.** `vxs_tc` filled about 40 words of the kernel frame for every span. Key them on the template id and the vertex format (`tc_ready` changes the layout without a new id) and clear the key in `template_reset`, so an id that comes round again cannot match. The span's pointers, counts and strides are still set every time.
- **Copy and clear the packet's state ranges as constant spans**, not through a table of offsets: the compiler then sees each size, and the four single-word ranges become one store each. One macro list feeds both `pk_copy` and `pk_clear`, so they cannot cover different fields.
- **Scale by a power of two in one multiply.** The scalar fog's `while(i<0){r*=0.5;i++;}` ran up to 116 times; `r * 2^i` with the float built from its exponent bits gives the same bits.
- **Keep the build free of warnings.** .551 had 20 outside Mesa and three were defects: a call with seven arguments to a six-argument trace function (the last word was dropped), a `DWORD < 0` test that could never set the capture's top-down bit, and a function called without a prototype. .553 has none, so the next one stands out.
- **A tool reads its mode from its arguments, not from the whole command line.** VXTLP searched `GetCommandLine` for GRID, HW and SW, the program's path included.
- **Checked and left alone: the compiler's flags.** A replay built by Watcom for Linux (`-bt=linux`, the board's flags, the SSE2 kernel objects linked, `vx_d3d_sse2` on) gives the same hashes as the gcc build, and `-oh`, `-ox`, `-6s -fp6` and `-oe=100` all run within 1.5% of the shipped flags. That replay is also the closest host check of what the board runs.

### 33.22 Ruled out without a build: Direct3D's vertex buffers in engine memory

vmhal9x (MIT) owns its vertex and command buffers through the execute-buffer callbacks; this driver zeroes them and Direct3D allocates the buffers in system memory. Taking them over, so that Direct3D's processed vertices land where the engine fetches them, looks like a way to drop the HAL's conversion, the queue copy, the VxD's copy-in and the upload. It is not one on this board:

- Direct3D already offers the swap: in g.rec 485 of 720 DrawPrimitives2 calls carry D3DHALDP2\_SWAPVERTEXBUFFER (flags 0C and 1C), none carries USERMEMVERTICES. Getting the buffers is not the obstacle.
- The HAL must read every vertex (the viewport and depth checks, the cull, the clipper). CPU reads of the write-combining frame buffer run at 30 MB/s with dword code and 61.5 MB/s with MMX (VX2READ, v10.69); a frame's 28,000 vertices of 32 bytes would cost 15 to 30 ms to read back, more than the whole copy chain costs now.
- A cached mapping reads at 340 to 516 MB/s but is stale against the engine in both directions (the same VX2READ run), so the engine cannot fetch what the CPU wrote through it.

What is left of the idea is the write-only form already on the list: check the vertices in system memory, then write them once into the vertex-area slot from the HAL. That needs no callback; it needs a decision about what ring 0 still validates, because the VxD cannot read the slot back at a useful speed either.

**Rule.** Before proposing a board probe, search CHANGES.md and this document for the measurement. This one had been on file since v10.69.

### 33.23 Recipe (.554, under test): the index bursts in XP's form (ACCEL\_D3D\_IX16)

Status: always on from .555 (mode 3 of the table below; the key and the other three forms are gone). Drawn right on the board in all four modes (VXIX16 v1.0 on .554, 32 bpp desktop: 0 pixels differ from mode 0 in the three scenes, counts of up to 960 indices a burst). So the engine takes a `04/24` above 48 in index mode, and `04/20` = 1 is 16-bit indices with the first index in the low half. Measured on 12,800 triangles a frame, modes in turn: mode 0 21.17 ms median / 21.57 ms 95th percentile, mode 1 20.81 / 20.96, mode 2 21.02 / 21.23, mode 3 20.61 / 20.79. That is about 0.5 microseconds a burst head and about 8 nanoseconds an index data word. TEXDBG `BC E4` reads the bursts' counts, `BC E0` clears them.

What XP's driver for the VX855 does (s3ginv.dll from the XP package, static disassembly):

- Indexed triangle list, inline indices: function `0x11B030`. It is chosen at `0x120230` when the index buffer is not user memory and the 2-or-4 field at `[dev+0x45F8]` is 2 or 4; otherwise `0x11A910` expands on the CPU.
- One `04/24` count for the whole draw. The chunk limit is `0x7FFC00 / (3 * index size)` triangles, so in practice never split.
- Head written by `0x118350`: pad to 16 bytes with `CC000000`, `FE020440 0000043C 00040000 CC000000`, `24000000 | index count`, pad, `FE03043C`, data word count, `0`, `CC000000`. Data words = `ceil(indices * size / 4)`.
- The indices are copied as they are (`rep movs`), so 16-bit indices go two a data word, the first in the low half.
- `04/20` bits 1:0, built at `0xD8AA0`: 0 no index mode, **1 for 2-byte indices, 3 for 4-byte indices**.
- `04/21` = base & FFFFFF, `04/22` = stride << 8 | base >> 24, `04/2C` = bound, `04/2D` = lowest index (built at `0xD8C20` and `0xD8B64`). This is how .553 already writes them.
- The second head helper `0x118420` also writes `04/23 = (kept & F10F) | arg << 9` before the count.

What .554 sends with the key:

| Mode | A burst | An index | 04/20 |
| --- | --- | --- | --- |
| 0 | 48 indices | one 32-bit word | 3 |
| 1 | up to 480 data words | one 32-bit word | 3 |
| 2 | 48 indices | 16 bits, two a word | 1 |
| 3 | up to 480 data words (960 indices) | 16 bits, two a word | 1 |

- 480 data words a burst is what an inline burst of 48 ten-word vertices already sends between two waits, so the direct (no ring) path waits as often as before.
- Burst sizes stay multiples of 6: whole triangles, and an even index count except in a run's last burst. An odd burst's last word has 0 above its index.
- Base, stride and bound are still sent before every head, and `04/20, 21, 22, 2C = 0` after the packet.
- On g.rec (GTA3): heads 1,234 to 515 a frame, index data words 46,348 to 23,241 a frame, words a packet 1,930 to 1,817.
- Run-time switch for a tool: TEXDBG `BC E0`..`E3` set the mode and clear the counts, `BC E4` reads mode, heads, data words, indices. Registry: `ACCEL_D3D_IX16` (0 to 3).
- Board tool: VXIX16 (each mode's picture against mode 0's, then median and 95th percentile). Board steps: AB\_TEST\_554.md.

Open point: XP also sets `04/00` bit 16 (bit 15 on one chip variant) when `[dev+0x45F8]` is 4. .553 draws 32-bit indices with that bit clear and the board draws right, so it is left clear in every mode.

### 33.24 Rules from the .554 round

- **A new fast path needs a host test that reaches it.** The emitter's direct sink (`vx3dn_wp`, `vx3dn_wend`, `vx3dn_wform`) is only open inside the VxD. `tests/test_fetch.py` `_trace_sink` opens it on the host (form 3 after a type-0 header, 2 after any other) and takes the stored words before every io word. Use it for any change to a `REG` / `DAT` fast path.
- **Break the code on purpose before trusting a new test.** Seven one-line breaks of the 16-bit paths were each caught by `test_ix16_forms`; a test that passes in 40 ms proves nothing until it has failed once.
- **Count instructions over every function.** `callgrind_annotate` lists only the functions above its threshold; parsing that list undercounted. Use `--threshold=100` (work/irc.py). Corrected core counts on g.rec: .551 10.29 million a frame, .552 8.64, .553 8.49, .554 8.47 (-17.7%, not -19.8%).
- **The host emitter profile is not the VxD's.** On the host `clear_surface`, the vertex writes, `reg_slow` and `dat_slow` are the harness's loops; in the VxD they are the engine, `rep movsd` and the inline sink.
- **Hashes that do not depend on where bursts split** (the replay's by-index hash and index-state hash) are the ones to hold fixed when a change moves burst boundaries; the emitted-words hash is held only for the default mode.
- Search CHANGES.md and this cookbook before proposing a board probe. The 48-index burst was never a measured engine limit: it is the packet size of v10.216 carried forward (v10.235).

### 33.25 Examined and ruled out (.554)

- **A branch-free per-vertex check in the VxD.** Watcom compiles the check of an indexed packet's vertices (x, y against the clip rectangle with the guard band, z, rhw, u, v) to 28 instructions a vertex with every branch going one way for a good vertex. Sign-bit forms without branches need 32 (accumulate, exact loop only on a hit) to 41 instructions. Left as it is.
- **A vertex slot shared between the packets of one call.** Measured on g.rec: 97,804 vertices converted through the span path in 5 frames, 96,608 distinct by call and template (1.2% repeated), 95,327 referenced. GTA3's draws do not share vertices across packets, so there is nothing to save.
- **D3D8.DLL (4.09.00.0904) and the C7.** Its vertex pipeline is chosen by CPUID feature bits only: 3DNow first, then SSE2 (`0x496E40`), then SSE (`0x464280`). The string GenuineIntel is copied in both detection routines but never compared there. The C7 reports SSE2 and no 3DNow, so the runtime's SSE2 pipeline is what the selection asks for. This fits the board result that the driver's T&L lost to the runtime's.
- **DirectX 8-level DDI, on paper.** XP's display DLL does handle commands 45 to 65 (DP2\_opcode\_dispatch.csv), and its vertex buffers are in video memory with the engine fetching by index. With software vertex processing the runtime still transforms, lights and clips; the DX8 commands change where the result is written and how indices arrive. On this driver the copy of indices into the DX7 command buffer is about 46,000 indices a frame, and vertex buffers in engine memory were ruled out in 33.22 (reads of write-combined memory). Estimated gain: below 1 ms of 76. Not worth its board sessions unless 33.22's read problem is solved first.
- **OpenChrome** was not used for any of the above (it has no indexed path for this engine).

### 33.26 Recipe (.555, under test): strips and fans through the span path

Status: host only. Run-time switch `vx_d3d_pass` (default 1; `VX_D3D_QueryPhases` bit 29 sets it from bit 28; words 336-343 are the counts and the switch). No registry value. Remove the switch and its tests of 0 once the board confirms. Board tool: VXPASS.

Why: on GTA3's recorded frames 28% of the triangles did not take the indexed list's span path and cost about six times as much each (about 600 instructions against about 100). Two kinds made up most of them:

- **Indexed strips** (DrawPrimitives2 command 20): 1,101 triangles a frame, 56% of them stitches (an index twice).
- **The clipper's fans** (command 23, the vertices inside the command): 1,210 triangles a frame, 1.5 a fan. Direct3D sends every clipped triangle this way.

How:

- A strip is written out as index triples in the strip's own order (an odd triangle's last two indices swapped), stitches left out, into a static buffer of 1,024 triangles, and handed to the indexed list's code (`ix_list`). Nothing else is new: the list's spans, its per-triangle loop with the cull, its stops. Gate: every index below the vertex data's end (`VXS_MAX16` over the n + 2 indices), at most 1,024 triangles; otherwise the old path, which refuses a bad index whole.
- A fan is passed through when the template is already built for this vertex format and on the fast path into indexed packets, the fill is solid, nothing is culled, and every vertex passes the range tests. Its n + 2 vertices are converted as one span and its triangles added as (j + 1, j + 2, 0). Otherwise the old loop.
- A fan's vertices must not go through `ix_check` or `vxs_span_ok`: those mark `vv_ok`, whose bits are indices of the bound vertex buffer. Use the range test alone (`vxs_imm_ok`). It implies the finite test.
- Do not build the template in the fan's gate. The old loop builds it only when a triangle is drawn, and a build can fail the call.

Result on g.rec: the core's instructions a frame 8.47 to 7.47 million (-11.8%; -27.4% from .551); every index drawn with the same vertex words and registers in the same order as before (the replay's by-index and index-state hashes unchanged).

### 33.27 Recipe (.555): a flip without vsync

The chip takes a new start address at the next blank only. A `DDFLIP_NOVSYNC` flip that writes the address and lets the application go on leaves the old front on the screen for up to a refresh while the next frame is cleared and drawn into it. Board, GTA3 with Frame Sync off on .554: the picture flashed like a strobe. The code was v10.384's and unchanged since.

Rule: carry out a NOVSYNC flip as the one-frame flip. Into the ring when it runs (no wait in the flip call), else the CPU flip that returns once the address is written. Either way the old front is guarded until the display has taken the flip. Keep `DDCAPS2_FLIPNOVSYNC` advertised so an application that asks for it still gets its device. Cost: no more flips than refreshes. Board tool: VXNOSYNC (each frame cleared black, held 6 ms, covered white: the screen must stay white).

### 33.28 Rules and failed approaches from the .555 round

- **Failed: keep a failing span's leading triangles.** When a list run's span check fails, cut the run to the triangles below the first failing vertex and come back after one triangle. On g.rec the failing vertices are spread through the spans (often vertices no triangle uses): 2,167 of 2,416 retries found no prefix, one more packet a frame, 1.8% more instructions. Dropped. Still open: 1,116 good triangles a frame go one by one because their span holds a failing vertex nobody uses.
- **The linked-VxD checks model the engine too.** `verification/native3d.py` runs the built VxD and decodes its ring words; it knew only `04/20` = 3. A change of command form has three models to update: the replay, `tests/test_fetch.py`, and `native3d.py`.
- **Host defaults are not the driver's.** `vx_d3d_ixpass` is 0 in the host library and always 1 in the driver. A test of a path behind it must set it, or it tests nothing.
- **A hash that survives the change proves more than a new reference.** For a change that may drop or send triangles with no area, hash by triangle and leave the ones with no area out (the replay's nzhash). Here the stricter by-index hash also held, because every dropped triangle was a stitch.
- **Before blaming a new build for a board fault, diff the path.** The flash with Frame Sync off appeared on .554; the flip, DirectDraw and VxD flip sources were .551's line for line, and the fault followed from the code's own comment about the blank.

### 33.29 Recipe (.556, under test with 33.26's switch): a proven list token as one run

An indexed list's token whose whole range was proven (every vertex from its base to its largest index passes, the range at most 3 a triangle and 32 more) and that fits the packet is taken as one run. Its largest index is the one `dp2_body` already has from `VXS_MAX16`; its smallest is one pass of compares. This replaces the pass that kept both for every triangle and tested the packet's room each time (about 35 instructions a triangle).

- The run is exactly the one the old pass made, so the command stream is .555's word for word (the replay's emitted-words hash is unchanged). On g.rec 2,072 of 2,394 tokens go this way.
- A run's indices are rewritten two at a time when they are back to back: one 32-bit add of `gain * 0x10001`. It is right for a negative gain too: the multiply leaves gain - 1 in the upper half and the lower add always carries, because a packet index is never negative.
- Core instructions a frame on g.rec, compiled with the usual inlining: .551 10.11 million, .555 7.36, .556 7.22 (-28.6% from .551).

### 33.30 Rules and failed approaches from the .556 round

- **Failed: the span from the token's base.** Taking a proven token's run as base to largest index needs no pass at all and was 3% fewer instructions on GTA3, whose tokens begin at their base. But a draw whose indices begin well above its base converts vertices nobody uses: 289 for 153 in the linked-VxD suite's split grid. An optimisation has to hold for other applications' draws, not only the recorded game's. The smallest index must be found.
- **Not built, from the record: one pass of check and write in the VxD.** It was proposed as the next step with a guess of 3 to 6 ms. The record already had the answer: the whole copy-in of an indexed packet is 5.0 microseconds (.523), about 1.1 ms a frame; v10.444's FUSEDVAL (a fused copy and check) was slower than `rep movsd` and the check after it (12.2 to 12.4 against 10.9 microseconds a packet); v10.449's ONECOPY was neutral for the VxD's whole packet (-0.3). The cost is the first read of the caller's cold vertices, wherever it is done. Driver-owned vertex buffers in locked memory would save at most the HAL's conversion pass on top of that; not worth their board sessions.
- **Search the record before giving an estimate, not only before proposing a probe.** The numbers above were three greps away.
- **A test suite fixture that fails is evidence about other applications.** The split grid was written for another feature; it caught the base-span waste because its second draw starts at index 136.

### 33.31 Board results for 33.26, 33.27 and 33.29 (.556), and what .557 adds

- **Confirmed on the board, always on from .557:** the strips as lists, the fans passed through and the tokens taken whole. VXPASS v1.1 on .556: 0 pixels differ off against on in every scene. GTA3, three runs: nothing refused or lost; in one 120 s run 956,274 fans passed through, 22,385 strips as lists, 566,558 tokens taken whole.
- **Confirmed: the flip without vsync (33.27).** GTA3 with Frame Sync off on .556 from a fresh start: 1,630 flips, all in the ring, 204 of them under 20 ms after the one before, 0 hangs, no strobe.
- **Open: a ring hang in the test tool.** VXNOSYNC v1.0 (each frame: clear, read-lock the back buffer, 6 ms, draw, flip without vsync) hung the ring on its third flip: the fence stood at group 7 and the flip is the first words of group 8. The driver took its 100 ms way out and flipped by the CPU for the rest of that boot, so the screen stayed right. The cause is not known. VXNOSYNC v1.1 runs four passes (no vsync or vsync, without and with the lock) and prints the VxD's flip counts after each, so the log names the combination that hangs it.
- **A tool's own expectation can be the wrong part.** VXPASS v1.1 called scene 2a wrong because nothing was drawn; CULLMODE CCW rightly culls the whole grid. Judge a culled scene by off against on only.

### 33.32 Recipe (.557, under test with the run-time switch): a refused run drawn by the vertices it uses

In live GTA3 a third of the list path's runs are refused because the run's span of vertices holds one that fails the range tests, most often a vertex no triangle of the run uses (223,676 of 694,050 runs in 120 s). Those triangles then went one at a time through `ix_list`'s loop.

`ix_ref` draws such a run by the vertices it uses: a map from source index to packet index over the span, each vertex checked (`ix_check`) and converted (`fast_vertex_pre_w`) once in the order met, nothing written until the checks are done. A triangle that uses a failing vertex ends the run; that one triangle goes to `ix_list`'s loop, which culls it or stops as before and then comes back.

The gain is small: 1.7% of the core's instructions on g.rec, for 0.8 more packets a frame (the run's vertices are not shared with the per-triangle loop's tables). It stays only if VXPASS's speed part or a GTA3 A/B shows it. The earlier attempt on the same triangles is in 33.28 (keeping a failing span's leading triangles: a loss).

### 33.33 Rules from the first board run of 33.32 (.557)

- **GTA3 takes the path and draws right.** One 255 s run: 409,067 runs drawn by the vertices they use (2,813,380 triangles, 5,110,381 vertices), 13,698 of them ended by a failing vertex, 0 primitives left out, 0 ring hangs, 3,958 flips all in the ring. Whether it is faster is not known from one run: the speed part of VXPASS or an alternating A/B decides.
- **A test scene must prove it drew something before it is used as a check.** VXPASS v1.2's scene 5 moved every seventh vertex of the whole grid and reused one index list for ten blocks that start 164 vertices apart; 164 mod 7 is 3, so over the ten blocks every vertex was a moved one somewhere and the tool left out every triangle. It sent draws of 0 triangles. Choose the moved vertices by something that is the same in every block (v1.3: column mod 7 is 3 on odd rows, with an even number of rows a block) and make the tool fail by name when its own triangle count is 0.

### 33.34 Confirmed (.558): the run drawn by the vertices it uses is always on

- Board, .557, VXPASS v1.3 scene 5 (lists whose spans hold far-outside vertices no triangle uses): 0 pixels differ off against on; median 34.94 to 33.83 ms a frame, 95th percentile 35.47 to 34.18 (39,680 triangles a frame). With 33.33's GTA3 run this confirms 33.32: `ix_ref` is unconditional from .558 and the run-time switch (`vx_d3d_pass`, QueryPhases bit 29) is gone. VXPASS is retired with it; the host test keeps the cases.
- **The ring hang of 33.31 did not come back.** VXNOSYNC v1.1 on .557: four passes of 150 flips, all 600 in the ring, no wait ran out. One difference from v1.0 is left untested: v1.0 locked the back buffer in the very first frames after the start. VXNOSYNC v1.2 runs that frame first and last. Until it hangs again with a ring tape, there is nothing to fix in the flip code, and none is changed.

### 33.35 Recipe: the names Device Manager shows

In the INF, the `[Manufacturer]` line's string is the device's manufacturer and `[Version]`'s `Provider=` is the driver's provider; give each its own entry in `[Strings]` (here `Mfg="VIA"`, `Provider="360ALASKA"`). Windows 98 copies them at install into `Enum\PCI\<device>\<instance>\Mfg` and `Services\Class\Display\000n\ProviderName`, so a tool can check the installed names there; they change only when the INF is installed again through Device Manager, not when the files are copied over.

### 33.36 Recipe (.559, under test with a run-time switch): built templates kept in a table

A draw template is built for every state the game draws with: 7.8 microseconds on the board, 360 a GTA3 gameplay frame, most of it asking Direct3D's surface objects (resolve, levels, faces, key). The game comes back to its states: in the recording 1,919 of 2,617 builds are of a state built before.

- **The key is everything the build reads from the context:** its address and generation, the target and depth surfaces, the whole state (2,196 bytes), the feature words. All of it is compared, a word at a time; the hash only picks the place.
- **The answer** is the template's fields and the packet's state ranges packed (988 bytes of a 32 KB packet), not the packet.
- **What the build reads outside the context is guarded by a count of the world.** Count every event that can change a surface object (where it is, its size, format, levels, key, palette): CreateSurface, DestroySurface, SetColorKey, the palette calls, SetExclusiveMode, CreateSurfaceEx, DestroyDDLocal, a context made or ended, and inside a DrawPrimitives2 call every command the runtime parses. Lock, Unlock, Blt and CLEAR change what a surface holds, not the object, and are not counted: a template holds no texel.
- **A flip** swaps the memory of the chain's surfaces. The target and depth are in the key; drop the entries that sample either flipped surface, and count the world when the chain is made of textures.
- **Never kept:** a build with an error or a left-out reason, ValidateDevice's, a state block being recorded, a draw under the driver's T&L, a state with COLORKEYENABLE (the key is read from the object at each build).
- **Off in the ICD** (its textures change without these events); the HAL turns it on.
- 512 entries, two places for each (the alternating back buffers double the states). One place each for 256 lost a third of the finds to another state in the place.

On the host the table costs instructions (a build asks nothing there): the board's A/B decides. VXTPL v1.0: the same frames off and on, two textures released and made again between frames, and a speed part.

### 33.37 Rules from .559

- **An indexed packet holds 512 vertices** (384 from .529). Recording: 267.0 to 251.2 packets a frame, the by-index and area hashes unchanged. Three things move with the constant: the packet's size (32,380 bytes), the T&L's slot table (512 is its limit), and the vertex area: such a packet needs three 7,680-byte slots, and `va_alloc` refused more than two, which would have drawn every packet past 480 vertices expanded. The linked test found it; a constant in the HAL's header is also a rule in the VxD.
- **A packet drawn expanded must fit a fetch group** (8,192 words). One of more than about 790 indices goes through the registers after a retire. This was already so; larger packets make it more likely when no slot is free. The monitor's "expanded (no free slot)" count must stay 0.
- **A reference build for comparisons must share the packet's layout.** Build the old core from a copy of its sources with the layout constants set to the new tree's.
- **Failed approach, do not repeat: texture and program registers written only when changed.** .526 built it for the texture set-up; the board measured the compares dearer than the words saved (0.61 microseconds a packet) and .527 removed it. A program's words equal the last program's in 5% of set-ups. Read the emitter's own comments before proposing a shadow.
- **Failed approach on the same triangles as 33.28 stands**; the map of 33.32 is now stamped by the run instead of cleared.

### 33.38 Rules from .559's fault on the board (.560)

- **The packet's size is also the size of a batch call.** A queue of 32 packets goes to the VxD in one DeviceIoControl. With .559's 32,380-byte packet that is 1,036,160 bytes; with .558's 27,260 it was 872,320. On .559 VXTPL ended with an invalid page fault in KERNEL32: the fault a push, the stack pointer at the bottom of the thread's megabyte of stack, another register still pointing near its top. That is one allocation of about a megabyte, not a recursion. The cause is read from the registers and the two sizes, not seen. .560 sends at most 24 packets a call (`VX3DN_BATCH_SEND`, 777,120 bytes) and a compile-time check keeps a call under 872,320 bytes; the VxD still takes 32.
- **Whatever grows the HAL's image must be checked against the VxD's lock limit.** The VxD refused to lock more than 1,024 pages; .559's image was 1,438, so the HAL was silently not kept in memory. The limit is 2,048 from .560, and VXTPL prints the pages locked (0 is wrong).
- **Reading a fault dialog:** EIP on a push with ESP just under a 64 KB boundary is the stack, whatever module is named. Compare ESP with the other registers that hold stack addresses to find where the stack's top is and how much was used.

### 33.39 Correction to 33.38, and rules from .560 on the board (.561)

- **33.38's first rule read the fault wrongly.** VXTPL's fault was not a megabyte batch on the stack. `VX_D3D_QueryPhases` takes flag bits and a word count in one argument; .559 put the new switch values in bits 24-27 and left them inside the count's mask, so a count of 1 with bit 24 set became 16 million and the HAL wrote all 356 words into the caller's one-word buffer on its stack. The stack was overwritten, not used up. **When flags and a count share a word, mask the count to the bits no flag uses, and test a count of 1 with every flag set against a guarded buffer** (VXREG v1.22 does).
- **A fault dialog alone does not name a cause.** Two readings fitted the registers; the tool's log, written a line at a time, settled it at once (it ended at the first switch-on call). Ask for the log before building on a reading.
- **A switch a tool sets must survive the game's start.** The HAL sets its defaults again whenever Direct3D starts; a default must not overwrite a switch a tool has set (`tplc_switched`).
- The batch limit of 33.38 stays at 24 packets a call only because that is what the board has run with the 32,380-byte packet.
- The lock limit of 33.38 is confirmed: GTA3 on .560, 1,370 pages locked.
- **33.36's table, first board numbers (GTA3 on .560, 99 s):** 226,361 templates found, 220,477 built: half, not the recording's three quarters, because the world changed 24,008 times, 18 a frame. A template request cost 11.4 microseconds against 9.9 for a build on .557 without the table (other scenes). That is no gain. The same-session A/B (VXTPL OFF, a GTA3 run, VXTPL ON, the same run) decides; if it confirms, the table goes, and this section becomes a failed approach.

### 33.40 Rules from the review of .559 to .561 (.562)

- **A table of large entries is slow on this processor even when it finds.** 33.36's entry is 3.3 KB in a 1.7 MB table; a find compares 2 KB and copies 1 KB that are not in the cache, some fifty lines, while a build runs on code and data that are. Count the cache lines a lookup touches, not its instructions; the host's instruction counts cannot see this. The table is off by default from .562 until an A/B on the board shows a gain.
- **A feature under test is off by default until the board shows a gain**, and its tools leave the switch as they found it.
- **When a new early return is put in front of a function, check what the rest of the function did besides its work.** The table's branch returned before the profile's block, so no template build was measured while the table was on.
- **Count why, not only how often.** The world changed 18 times a frame in GTA3 and one counter could not say for what. From .562 the changes are counted by cause and the runtime-parsed commands by code.
- **Known limit of 33.36:** a mip level attached after creation (DirectX 5/6) and a surface marked lost come through no callback; an entry made before stands until the next change of the world.

### 33.41 Rule (.563): what is kept from a surface object is kept only for an interface that cannot change it unseen

33.40's known limit is closed by not using the table where it could be wrong. A DirectX 7, 6 or 5 application can attach a mip level to a texture, set a key or lose a surface through no callback of the driver. A DirectX 8 texture is made whole and changes only through callbacks. The context's DirectDraw object says which interface made it: `DDRAWILCL_DIRECTDRAW8` in `dwLocalFlags`, read once when the context is made. The HAL passes it to the core in the services' flags (`VX_SV_FIXED_OBJECTS`); without it every template is built from the objects, as before .559.

- Prefer this to checking the objects again at each find: a check that walks a mip chain reads as many cache lines as the build it replaces (33.40).
- A recorded call clears the flag, so a recording holds every answer a replay can ask for.
- A tool for the feature must prove its own device is counted as a DirectX 8 one (QueryPhases 365), or the feature is never exercised and the tool passes for nothing.

### 33.42 Failed approach, do not repeat: built templates kept in a table (.559 to .563, removed in .564)

Sections 33.36, 33.39 (its last point), 33.40 and 33.41 describe a table of built draw templates. It is removed. The board's A/B on .563 (VXTPL v1.4, the same frames off and on in turn): 384 template requests a frame, every one found in the table, 12.99 ms a frame with the table against 11.96 without (95th percentile 13.77 against 12.25). GTA3 found half and showed no gain.

- **Why it lost with every request found:** an entry was 3.3 KB (the 2 KB of state as the key, 1 KB of answer) in a 1.7 MB table. A find compared and copied some fifty cache lines the processor did not hold; a build runs on code and data it does. On this processor a lookup is priced by the cache lines it touches.
- **What it cost besides:** a count of every event that can change a surface object, called from every DirectDraw and Direct3D callback; a rule for flips; an interface check so older games could not be served stale entries; 1.7 MB of locked memory. None of it is needed without the table.
- **What would have to be true for a second try:** a key of a few words kept up to date as the states are set (not hashed and compared at each request), an answer of a cache line or two, and a way to keep entries across the 18 changes of the world a frame GTA3 makes. Nothing measured says that is reachable.

### 33.43 Recipe (.564): an indexed packet with no free slot in the vertex area

An indexed packet's vertices go into slots of the vertex area; a slot is free again when the group that read it is done. With no slot free the packet is drawn with its indices expanded inline, ten words an index. From some 790 indices that is more than a fetch group's 8,192 words even alone, and the packet went through the registers with the ring stopped: correct, and many times slower.

The slots are busy only while groups are in flight. So when the expansion does not fit and work is in flight: wait for the work in flight (a retire, the ring left running), then build the packet again; every slot is free and it is drawn by index. Only if it still does not fit does it go through the registers.

- A build that is not kept must leave the counts as they were: the expansion that did not fit was being counted as a packet drawn expanded.
- **Testing a path that depends on the engine being busy:** the linked model lands a fence at once, so the slots were always free and the new path never ran. The model needs a mode where a fence lands only when the VxD waits for it (here: after eight register reads with no write between). Take the fix out and the test must fail: without it two packets go through the registers.
- On the board the path is reached with TEXDBG BC E7 (the area cut to three slots); VXSLOT v1.0 compares the picture with every slot and with three, and reads the waits (BC E6).

### 33.44 Rules from .564 on the board (.565)

- **A new TEXDBG code must be searched for in the dispatch chain before it is used, and its handler tested through the dispatch.** .564 gave VXSLOT's three codes the numbers E6, E7 and E8 on the word of an old comment; the state deltas' reading already answers those earlier in the same `else if` chain, so the new lines were never reached and the tool could not set the slots. The codes are F0 (read), F1 (three slots), F2 (every slot) in 33.43. The linked checks now call the handler through the VxD's own IOCTL dispatch and check the variable it sets; a test that pokes the variable directly proves nothing about the tool's path.
- Confirmed on the board (.564, GTA3 189 s on the slower test bench): no packet expanded, no slot refused, every flip in the ring, the HAL's image locked at 954 pages after the table's removal, template builds measured again.

### 33.45 Confirmed on the board (.565): 33.43's wait for the slots

VXSLOT v1.1, the vertex area cut to three slots: 0 pixels differ from every slot; in four frames 12 packets drawn by index after a wait, none still too large, none expanded, none through the registers. In the speed part 9,487 packets took the wait and none went through the registers. A frame of 51,200 triangles took 34.65 ms with three slots against 22.56 with every slot: that is the price of waiting for each group, paid only when no slot is free, which no game run has yet shown (0 slot refusals in every GTA3 log). The test codes F1 and F2 have done their work and can go; F0's reading stays for the monitor.

### 33.46 Recipe (.566): an A/B between two builds on one machine

A tool that compares two builds must run on both, so it may ask the driver nothing a later build added. VXAB asks only the VxD's build number (TEXDBG 75, there since .531) and names its log by it (VXAB551.LOG, VXAB566.LOG). It times five fixed scenes, each 8 frames to settle and 120 timed, and prints the median and the 95th percentile: indexed lists, indexed strips, fans clipped by Direct3D, 384 quads with a state change before each, and lists whose spans hold unused far-outside vertices.

- Run it on build A, copy build B's four files, restart, run it again, with the same desktop depth and nothing else running. Do each build twice; a difference smaller than the gap between two runs of one build is not a difference.
- The scenes say where a build gained or lost by kind of work. A game's own run (the monitor's median and 95th percentile over the same stretch, alternating the builds) says what a player gets. Both are needed: a fixed scene cannot show what a game's mix does, and a game's run cannot be repeated exactly.
- A score from a benchmark stands for other programs only as far as they do the same kind of work. Before working on one, profile it and name the path the time is in; keep a change only when another program or tool that uses that path gains as well.

### 33.47 Culled indexed lists through the pass-through (.567)

**Finding (board, WinTune Direct3D on .566).** A fast path that is gated on one render state is invisible to every program that sets the other value. The pass-through (33.x, .515-.558) ran only with CULLMODE NONE because GTA3 uses it; Direct3D's default is counter-clockwise, so WinTune (and most DirectX 5 to 7 programs) sent every list triangle through `ix_list`'s per-triangle loop: 792 cycles a triangle, 64 of 82 ms of driver time a frame. Rule: when a monitor log shows a path's counter at 0 for a workload, read the gate before optimizing anything else.

**Recipe.**

1. The chip's cull stays unused (it drew nothing, .518/.529). Cull on the CPU, but per run, not per triangle through the tables.
2. In `ix_pass`, after the run (lo, hi, m) has passed the sparse test and the span check, filter: for each triangle the area from the source vertices with exactly `ix_list`'s expression (doubles) and rule (`area==0 || (cull==2 && area>0) || (cull==3 && area<0)` is left out). Kept indices go to `ixc_keep` relative to the run's lo (16 bits are enough whatever the base).
3. While filtering, stamp the vertices the kept triangles use (`ix_rst`/`ix_rmap`/`ix_rsrc`, the tables of `ix_ref`): this gives the used count nv and the kept span l2..h2 for free.
4. Choose by waste: if `span - nv > span/8 + 8`, convert only the vertices used (one by one, order met) and write `d + ix_rmap[keep]`; otherwise convert the kept span with the span kernel and write `keep + d - l2`. Without this choice the first version sent 43-57% more vertices to the VxD than the loop did (g.rec forced to cull): HAL time bought with VxD copy time.
5. Nothing kept: no packet opened, the run skipped.
6. A culled run whose span holds a failing vertex goes to the loop (`ixp_sparse = m`), never to `ix_ref` (it does not cull).
7. Fans (command 23): all triangles kept, pass through; all culled, left out whole; mixed, the loop.

**Test recipe.**

- Host differential: `vx_d3d_cullpass` 0 against 1 on meshes with rows turned over (runs with nothing kept, runs by the vertices used), one winding, random, an outside vertex, an index base, command 3's 8-byte triangles; assert the counters moved so the test cannot pass by not taking the path; five mutations (rule flipped, each index mapping wrong, fan checks removed) must each fail.
- `vx_replay FILE -c N` forces a cull mode on a recording made with none; the by-index, area and index-state hashes must be equal off and on (they do not depend on where packets split).
- A tool that tests a switch reads it first and puts it back (VXREG v1.28); a tool that only needs a one-word answer must not send bit 29.
- Board: VXCULL v1.0. Its strongest check needs no reference: with every triangle either winding, the CW picture and the CCW picture must share no pixel and together cover the window.

**Measuring on the host.** Wall time of the replay was too noisy to rank a 10% change (the same binary gave 0.79 to 1.02 ms). `valgrind --tool=callgrind` and self cost by function did: build with `-g -fno-inline-functions-called-once`, compare `ix_list + ix_pass + fast_vertex_pre_w + vxs_conv_c`. Inclusive cost is misleading here: `ix_open` sends packets, and the harness's packet copy lands inside it.

**Numbers.** Host, per list triangle: the loop about 200 instructions plus 106 a new vertex; the filter about 85 plus 61 a span vertex. g.rec forced to cull gains only about 10% in these paths (short lists, few shared vertices); a dense mesh like WinTune's gains more. Estimate for WinTune on the board: 8% of a frame, to be replaced by VXCULL's timing.

**Next candidate seen in the same log (not built).** WinTune presents with a windowed Blt to the front buffer: each waits for the 3D work to retire (0.39 ms) and takes 0.85 ms. Putting that Blt in the ring would remove the wait.

### 33.48 Culled runs whose span holds a failing vertex (.568)

**Board result for 33.47 (.567).** VXCULL passed and its own scene gained (35.06 to 33.32 ms), but WinTune did not: 797 cycles a triangle against 792. Only a quarter of its triangles took the new path; 25,826 runs were refused "for a vertex" and fell back to the loop. Step 6 of 33.47 ("a culled run with a failing vertex goes to the loop") was the wrong rule for the workload the change was made for.

**Why real programs have such spans.** With Direct3D's clipping on, the runtime transforms the whole vertex buffer, sends the triangles that cross an edge as fans (command 23) and the rest as index lists into the same buffer. The off-screen vertices stay in the buffer between the ones the lists use. A span check over lo..hi therefore fails for most runs although no triangle sent uses a failing vertex. Cull-none got its answer for this in .557 (`ix_ref`); the culled path needs the same.

**Recipe (replaces step 6 of 33.47).** When the span check fails and a cull mode is set, run the filter with `chk` on:

1. For each triangle, first check the vertices not yet known good in this run (`ixc_ok`, stamped with the run's generation; `ix_check`, the loop's own test).
2. A triangle with a failing vertex: wholly past one clip edge (`ix_off`), leave it out and go on; otherwise end the run before it and hand one triangle to the loop (`ixp_sparse = 1`), which stops there so the per-triangle path clips it. This is the loop's order (check, then area), so the same triangles come out.
3. Kept triangles are always drawn by the vertices used (never the span: it holds vertices the VxD's validation would refuse).
4. Advance by the triangles taken (`mm`), not by the run's m.

**Rules learned.**

- A synthetic scene proves the code, not the gain: VXCULL v1.0's grids had clean spans, so it passed while the target workload mostly missed the path. After a path is added, read its counters in a log of the real workload before quoting a gain. VXCULL v1.1 makes the unused-outside-vertex scene a required scene and times it.
- The monitor's "cycles a triangle" for the indexed list covers the pass-through too and includes packet sends; compare it between builds on the same scene only.
- Equivalent mutations exist: treating "wholly outside" as a stop draws the same picture (the loop then leaves the triangle out itself). Note them instead of forcing a failing test.

**Also seen (.567 board).** Test Drive 5 (DirectX 5/6 era, cull none): 30.8 fps, nothing left out, every list triangle through the pass-through; its HAL time is spread over many small functions (about 10 triangles a run, 2.2 vertices a triangle), no single target.

### 33.49 Failed approach: checking and converting a culled run vertex by vertex (.568), and its replacement (.569)

**Failed (.568, board).** The recipe of 33.48 (check each vertex as a triangle first uses it, stamp it, convert the used vertices one by one) drew correctly but was slower than the loop it replaced: VXCULL v1.1 scene 4, 36.67 to 42.74 ms a frame. Host instruction counts said so too, had they been taken before shipping: 221 a triangle in `ix_pass` against the loop's 192. A path that keeps a stamp per vertex and converts with `fast_vertex_pre_w` is the loop again in two passes; it cannot beat it.

**Rule.** Before a build goes out, count instructions of old path against new on the tool's own scenes (the Python harness under `valgrind --tool=callgrind`, self cost of `ix_list`, `ix_pass`, `fast_vertex_pre_w`, `vxs_conv_c`, `ix_check`). On this code the host count predicted the board's direction every time; the estimate from reasoning did not.

**What makes the pass-through cheap, and must be kept:** nothing per vertex in the triangle loop, and the span converted by the kernel. So for a culled run:

1. Span check failed (`chk`): test every vertex of the span once with `ix_check` into a byte map `ixc_bad`. One linear pass, no stamps.
2. Triangle loop: three byte loads decide "uses a failing vertex" (then `ix_off`: leave out, else end the run); the area; the kept indices relative to lo. Nothing else.
3. Vertices: at least half the run's triangles kept, convert the run's whole span lo..hi with the kernel, then write a kept vertex's words over each failing vertex's slot. This is safe because the conversion is copies only (no arithmetic on the garbage; when `vxs_span_conv` would fall back to arithmetic, tri\_snap or a z bias, use the vertices-used path instead), the engine fetches by index, and the VxD checks only the texture words of prepacked vertices, which the kernel already makes finite. Fewer than half kept: stamps over the kept indices only, the vertices used converted one by one.
4. The culled triangles' vertices ride along with the span (host: +6 to +17% vertices, a few more packets). A wasted vertex costs the VxD about 115 cycles (copy-in and upload); converting one by one costs more than that per used vertex, so the span wins until well over half is waste.

**Host numbers (VXCULL's scenes, list paths, instructions):** clean spans a quarter culled 11.25 to 6.61 million (-41%); spans with unused outside vertices 12.39 to 8.9 million (-28%; .568 was +11%).

**Test additions.** Assert in the differential test that no packet holds a vertex outside the viewport (catches a missing overwrite). Equivalent mutations: forcing the vertices-used path always draws the same picture.

**Board result for 33.49 (.569, VXCULL v1.2, the slower bench).** Every picture the same off and on. Clean spans a quarter culled: 35.68 to 31.12 ms a frame (-4.56). Spans with unused outside vertices: 36.51 to 35.36 ms (-1.15; .568 was +6.06). The host's instruction counts gave the direction of both and of .568's loss, but not the size: -41% and -28% of the list paths' instructions became -13% and -3% of the frame, because the tool's frame also holds the VxD's copy and upload, the engine and the present, and the span path sends more vertices. Use the host count to accept or reject a design, the board's A/B for the number.

**Real workload for 33.49 (.569, WinTune Direct3D, the slower bench, against .567).** Confirmed: 6.4 of 7.16 million drawn triangles through the pass-through (1.69 before), 58,486 runs with a failing vertex in the span handled, none ended early, nothing left out. A frame 198.3 to 189.0 ms (-4.7%); DrawPrimitives2 81.0 to 71.6 ms; primitives into packets 62.9 to 50.2 ms; the HAL's sampled share 68.4 to 56.7 ms. The price, as predicted in step 4: vertices uploaded 6.72 to 8.12 million (+21%), packets 15,554 to 20,258, the VxD calls 14.4 to 16.6 ms and the queue copies 5.0 to 5.8 ms a frame. Net gain about 9 ms a frame. What is left in the HAL is `ix_pass` itself (45% of the HAL's time); outside it, the runtime's own transform (D3DIM.DLL, 29% of the frame) and the windowed present: 20 back-to-front blits a flip, 0.86 ms each plus a 0.39 ms retire of the 3D work before each (about 25 ms of a 189 ms frame), the next candidate.

### 33.50 A windowed present in the ring (.570, under test)

**Why.** A windowed Direct3D program presents with a Blt of its back buffer onto the primary. Through the 2D path that Blt first retires all 3D work in flight, then runs the engine copy with its own waits. WinTune's Direct3D test on .569: 2,002 such copies (634 x 455), 0.86 ms each plus a 0.39 ms retire, about 25 ms of a 189 ms frame, with the engine itself busy only 5% of the time.

**Recipe.** The ring already carries a 2D screen-to-screen copy group for render-target-to-texture copies (.453, `VX3DN_ring_copy`). A present is the same group with the primary as destination:

1. HAL (`VXBlit_impl`): mark the request `VX2D_PRESENT` (512) when it is a copy, SRCCOPY, no key, source surface other than the destination, destination caps PRIMARYSURFACE, and not VXGL's swap (that one keeps its armed asynchronous copy).
2. 2D submit: a request with `VX2D_RT` or `VX2D_PRESENT` is offered to `VX3DN_ring_copy` before the usual path. The flag mask in `validate` grows to 1023.
3. `VX3DN_ring_copy`: accept `VX2D_PRESENT` and the HAL's `VX2D_ASYNC`; for a present require the destination to be the surface shown (`hda->surface`) and the present switch; keep every other refusal (ring not running or dead, depth, alignment, the lab busy, a software cursor on either surface). Write the 13-register group and kick. Count in `vx3dn_pr_stat` (in the ring, not taken, time, empty after the clip).
4. Order on the screen needs nothing new: every later toucher of the frame buffer already retires the ring first (GDI through the 2D path, the minidriver's BeginAccess, Lock, flip), the rule that made ring clears, copies and flips safe.

**Rules.**

- Check a new TEXDBG subcode against the whole else-if chain before using it (33.44): F5 was already a reading; the present uses F3 off, F4 on, F6 read. A linked check calls them through the VxD's IOCTL dispatch.
- A tool must not borrow a subcode to read a counter without reading its handler: BC E0 clears the index-burst counts. Retires by cause are BC CA.
- The linked harness stubs `VESA_draw_needs_exclusion` (`excl_rule`) and `VX3D_busy`; a test of a copy onto offset 0 must set `excl_rule=0`, and cannot count the usual path's retire.
- A source rectangle outside the harness's 64 x 48 source surface fails `validate` and looks like "not taken": check the request before blaming the path.

**Tests.** Linked VxD (209 checks): registers of the group at 32 and 16 bpp, the clip, another destination, the switch, the software cursor. Linked HAL: the flag on a plain copy onto the primary, absent for another destination, a keyed copy, a fill. Board: VXWPRES v1.0 (Direct3D 8 window; frames read back through GDI at once and after 30 ms, the next frame drawn before the copy can have run, a GDI square drawn right after the present by FillRect and SetPixelV, then timing of a light and a heavy frame, off and on).

**Cull switch.** The culled pass-through (33.47 to 33.49) is permanent from .570: `vx_d3d_cullpass`, QueryPhases bit 29 and VXCULL removed; the host test compares against the per-triangle loop (`vx_d3d_ixpass` 0).

### 33.51 Presents in the ring: what .570 got wrong, and .571

**Board, .570.** VXWPRES: shown and order passed, but a square drawn by GDI right after the present was missing 10 times of 10. WinTune: 2,002 presents in the ring and no gain (188.7 against 189.0 ms a frame).

**Wrong statement in 33.50, step 4.** "Every later toucher of the frame buffer already retires the ring" is false for DIBENG. GDI drawing that DIBENG does itself (small fills, SetPixel, anything the 2D path declines early or never sees) reaches the screen with no VxD call at all; with ACCEL\_GDIFAST even the access bracket makes none. The driver only calls the VxD when bit 15 of its wEnabled word is set (v10.425, made for the asynchronous OpenGL present): `VX_SETTLE()` in BeginAccess and the DIBENG forwarders, then OP\_VX\_COMPLETE.

**Rule.** Anything that leaves work for the screen running after its call returns must set that bit, and `VX2D_complete` must finish that work. For a present in the ring: set `ring_present` and the bit when `VX3DN_ring_copy` takes a present; `VX2D_complete` retires the ring (cause 2) and clears both; keep the bit when an asynchronous 2D command completes while a present is still in the ring; clear the flag after every retire the 2D file asks for.

**Where the gain went.** The engine's copy still takes about 1 ms for 634 x 455 at 32 bpp; before, the Blt waited for it (0.86 ms); in the ring the next CPU access pays (WinTune locks 7 times a flip: 689 retires of 1.2 ms). And 13 waits of 100 to 110 ms appeared, every one in a group's opening after a wrap (`xr_open_x`'s space check, `fx_retire`): on .569 each present retired the ring, so a wrap almost never found groups of the old lap unread and its 219 waits were under 1 ms.

**Recipe (.571).** `xr_pr_lap`: set when a present is written, cleared by `xr_retire`. In `xr_open_x`, when a wrap is due and `xr_pr_lap && fx_inflight`, retire first, then write the wrap. Cost: one ordinary retire a lap; the present and the draws behind it still go without a wait.

**Not known.** Why a lap that holds a 2D copy onto the primary stalls the first group after the wrap until about 100 ms (GTA3 runs with overlapping laps and ring flips without it). The 100 ms is where `xr_wait_b` reads the fetcher's position and the fence for its progress test; whether that read lets the fetcher go on is a guess. If .571 still shows waits past 100 ms, the next step is a ring tape of one such wait, not another change.

**Tests.** Linked: bit 15 set by a present and cleared by one settle with one retire; a wrap with a present in flight counted as retired first, a wrap without one written at once (213 checks). Board: VXWPRES v1.1 check 3 is the judge of the order; VXD3DMON's "ring waits past 100 ms" and "retires before a wrap" lines the judge of the stall.

### 33.52 Failed approach: the windowed present in the ring (.570-.571, removed in .572)

**Result.** Correct after .571 (VXWPRES v1.1: every check passed, the GDI square there 10 times of 10), and no gain: a light frame 0.75 to 0.71 ms, a heavy one 66.08 to 66.11 ms; WinTune's Direct3D seconds 4.7 to 5.8 fps as on .569. Removed with its switch, tool and checks; 33.50 and 33.51 stay as the record of how it was built and what it broke.

**Why it could not pay.** The 0.86 ms a present cost in the Blt was the engine's copy itself, which the CPU waited for. Queued in the ring the copy takes the same engine time, and the wait moves to whoever touches the surfaces next (WinTune's Locks: 689 retires of 1.2 ms). Only the 0.39 ms retire before the Blt was real overhead, and a retire per lap had to be put back to avoid the stall after a wrap. Moving a wait is not removing it.

**Rule before the next "move it into the ring" idea.** From the monitor's log, split the cost into (a) CPU work, (b) waiting for engine work that must happen anyway, (c) waiting for unrelated work. Only (a) and (c) can be saved; then check who will wait for (b) instead. The .569 log already had the facts: engine busy 5%, Blt 0.86 ms for 288,512 pixels (about what the engine's copy takes), and a Lock after most presents.

**Kept from the episode.**

- The culled pass-through is permanent (.572).
- DIBENG draws on the screen without a VxD call; bit 15 of the driver's wEnabled and `VX2D_complete` are the only hook (33.51).
- A lap of the ring that holds a 2D copy onto the primary stalled the first group after a wrap for about 100 ms (.570, 13 times); cause not found. Relevant if render-target copies in the ring (.453) ever show the same.

### 33.53 OpenGL: why indexed packets lost in .509, found on the host (.572 tree)

**How to measure the ICD on the host.** `tests/gl_host.py` builds Mesa, the back end and the core with ASan at -O1. For instruction counts build the same objects at -O2 without sanitizers into another directory (copy the script, empty `SAN`/`MSAN`, `-O2 -g -fno-inline-functions-called-once`). Then `VXGL_BENCH=7` (Quake 2's world) or `8` (Quake 3's batches), `VXGL_BENCH_N=5`, `VXGL_BENCH_CORE=1` (prepacked) or `3` (prepacked and indexed); the bench prints packets, triangles and a packet hash, and runs under callgrind.

**Finding.** The ICD sends every triangle in the plain prepacked form: 3 vertices a triangle less what strip joins save, at most 192 vertices a packet. With indexed packets on, the packet count went up, not down: Quake 3 scene 230 to 837 packets, Quake 2 scene 100 to 212. A cut histogram (cause of every packet end) shows why: 607 of the 837 ended because the next primitive needed the other packet form. Only `vx_d3d_gl_trilist` (through `fast_tri_place` and `ix_tri`) can write an indexed packet; `vx_d3d_gl_fan`, `vx_d3d_gl_strip` and the quads write strip bursts into a plain one, and the two forms cannot share a packet. In the Quake 3 scene 1,100 fans sit between the triangle lists; in the Quake 2 scene nearly everything is a fan or polygon (3,220 calls). So .509's "indexed is slower in Quake 2" measured the thrash between forms, not indexed packets.

**What the same run shows is there to gain.** The Quake 3 scene's 23,850 list triangles needed 16,594 vertices by index (0.70 a triangle) against 30,936 in the plain form, and an indexed packet holds 512 vertices and 640 triangles: about 40 packets where the plain form takes 230.

**Rule.** Two packet forms in one stream need every primitive kind writable in both, or one form only. Before judging a packet form by frame time, print why packets end.

**Plan.** Fans, polygons, strips and quads of the ICD written as index triples into the open indexed packet (as Direct3D's strips and clipped fans since .555), then indexed on for the ICD; judged by packets and instructions on scenes 7 and 8 first, then on the board.

### 33.54 OpenGL: every primitive kind by index (.573, feature under test)

**Recipe.** One packet form for a whole draw. A second form that only some primitive kinds can write costs more in cut packets than it saves (33.53).

- Lists (`gl_ix_emit`): find the run's span of store numbers, convert each span vertex once, look it up in the open packet's table by store number (`IX_FIND` / `IX_PUT`) so runs of one draw share vertices, write indices through a slot map. Cull on the CPU with the list path's rule before choosing the span.
- Span much wider than its triangles can share (`sp > nk + 8`: a run between clipped triangles) or elements scattered over a locked array (`hi-lo+1 > 3n+32`): go vertex by vertex through the table (`ix_tri`), never convert the whole span.
- Fans and polygons from glBegin are n store numbers in a row: `gl_ix_fan` converts them in store order with no table and no scans. Without it the Quake 2 scene cost more than the strip path it replaced.
- Strips, quads, quad strips: build their triangles and hand them to `gl_ix_emit`.
- All or nothing per primitive: decide before the first word is written, else fall back to the old path whole.

**Rules found by the host checks (both were picture faults).**

1. A table keyed by store number is only valid while the store is not rewritten. Mesa's per-triangle template (flat colour copy, two-sided colours, polygon offset) changes vertices between triangles of one draw. `vx_d3d_gl_prim` without `join`, `vx_d3d_gl_tris` (the flat batch's own buffer) and anything flat-shaded must keep the plain form (`gl_noix`, `rs[9]==1`, `cs_on`). The ICD already tells the core: `c->join = render_index==0`.
2. The engine starts a triangle's interpolation at its first vertex. Writing a strip's odd triangle with its first two vertices swapped (to keep the winding) moved 8 pixels of the Quake 2 scene by one step at 32 bpp. Cull by winding, but store the triangle in the order the strip gave it (`gix_sw`). 16 bpp hides this; always run the host checks at 32 bpp too.

**Host numbers (5 frames).** Quake 3 scene: vertices written 30,936 -> 20,804, core instructions about 11.4 M -> 6.1 M, packets 230 -> 242. Quake 2 scene: vertices 15,202 -> 15,008, instructions level, packets 100 -> 75.

**Wrong estimate.** I expected about 40 packets for the Quake 3 scene. Packet count follows state changes (about one packet a draw), not vertex room. Count cuts by cause (`vx_d3d_cut_hist`) before estimating.

**Test.** VXGLIX: off and on through `VXGL_Indexed`, pictures compared, then timing off/on in turn. gl\_host.py runs 16 bpp on, 32 bpp on, 32 bpp off.

### 33.55 Peer review of .572: what was taken

- Remembered-decision stacks: when the stack is full, take the path that needs no remembered decision (cursor exclusion past depth 16 took fast begin, slow end).
- A handler whose request and reply may share a buffer copies the request fields before clearing the reply, in every branch (VXF\_FRONT's early branch missed it).
- A deferred wait keeps its answer. The CPU flip after ring flips ran the left-out engine wait and ignored a lost engine.
- A queue that can be sent inside `add` or before a lone packet: check the loss counter again after it, and do not send the lone packet behind a lost queue.
- A surface that could be neither filled nor locked is released, not published with old video memory.
- Rebase two 16-bit indices with one 32-bit add when base + index <= 511.

**Not taken.** WaitForVerticalBlank returning DDERR\_WASSTILLDRAWING on a blank timeout: applications retry on that code and would spin while the monitor sleeps. VXF\_TIMEOUT stays "done".

### 33.56 A board tool's scenes run on the host first (VXGLIX v1.0, failed)

**Rule.** Keep a board tool's scenes in a header (`src/vx_glix_scenes.h`) and run the tool's exact sequence on the host model (`VXGL_GLIX=1`, in `tests/gl_host.py`) before the tool ships. The host gave the board's counts and the board's 8 failures to the number.

VXGLIX v1.0 failed on the board for two faults of its own:

- **A scene that is not repeatable.** The Quake 3 scene's first batches read the arrays its later batches wrote the frame before, so one frame drawn twice is two pictures, in software too. An off/on comparison in one context needs each frame drawn once unjudged first. Check repeatability by running the comparison with the switch the same both times.
- **Scenes the feature does not apply to.** The driver prepacks, and so indexes, only draws with a program, which means textured ones (`VX3DN_PRE_FORM` wants `shader.count`). Untextured strips, fans and grids go the old way. Lit untextured scenes (Lattice's kind) gain nothing from by-index packets.

**Board, .573, VXGLIX v1.1 (by index off -> on, median ms a frame, 320 x 240).** Passed, 0 wrong; 2 pixels of one Quake 3 frame one step apart. Quake 2 scene 9.23 -> 8.58 (-7.0%), Quake 3 scene 13.47 -> 10.91 (-19.0%), clipped culled grid 2.58 -> 2.33 (-9.7%). The 95th percentiles moved the same way. The host's instruction counts gave the direction for all three and the order (Quake 3 most); it had the Quake 2 scene level, the board measured a gain there (fewer packets).

### 33.57 .574: by index permanent; untextured draws left; GDI horizontal store

**By index is permanent** (`VXGL_Indexed` and VXGLIX removed). Ask the cheap refusals (flat shading, `cs_on`) before building a primitive's index triangles: a flat strip built them for nothing (+0.5% instructions on flat lit scenes).

**Untextured draws by index: not built.** How to decide such a case on the host: run the same scene with and without its texture and count instructions a frame.

| Scene | textured plain | textured by index | untextured plain |
| --- | --- | --- | --- |
| strips | 2.029 M | 1.966 M | 1.952 M |
| fans, polygons, quads | 0.881 M | 0.744 M | 0.844 M |

Untextured plain strips are already cheaper than textured indexed ones. Polygons could gain about 12% at most, only when unfogged and untextured, and only by giving them a program (fogged untextured draws already get one), which changes the engine's pixel path.

**GDI horizontal lines (feature under test, GDI option bit 2, VXHLINE).** In the driver's own lines a horizontal pen-copy segment is one `REP STOS`. In 16-bit code the string store needs the address-size prefix to use ECX and EDI: emit the bytes (`F3 67 AA`, `F3 67 AB` is a word store, `F3 67 66 AB` a dword store). At 16 bpp store two pixels a dword from a 4-aligned address. Fill a leftward segment from its last pixel rightward and then set the address to where the loop would leave it; keep the direction in a flag, not in the address (a segment ending at the surface's first pixel has address 0). Host: a 100-pixel line 1,255 -> 623 instructions.

**GDI, looked over and left.** A line call's fixed cost in the driver is about 560 instructions of the 4.4 us the board measures for the whole call; vertical lines write a row a pixel; no DIB route is slower than DIBENG (VXGTUNE .510).

### 33.58 .575: horizontal store confirmed; three rules from the peer review

**Board, .574, VXHLINE (800x600x32, store off -> on).** 0 pixels differ. Horizontal lines: 16 pixels 1.05x, 64 1.16x, 160 1.43x, 352 1.92x; rectangle outlines 1.12x; a Polyline half horizontal 1.18x; random lines and XOR 1.00x. The store is permanent.

**Rules.**

- **A wait that can time out returns its result.** `VX_D3D_hold_sync` gave up after 100 ms and returned void, so Lock, Blt and Flip went on with draws still held. Now the three answer `DDERR_WASSTILLDRAWING` and touch nothing; calls that cannot refuse (DestroySurface, SetEntries) wait 2 s and then go on.
- **A status query touches no register, so it must not forget the register shadow.** `VX2D_access(3)` reached `VX3D_busy`, whose `VX3DN_busy` always calls `vx3dn_forget`. After `VX3DN_poll` has retired what was in flight, ask only whether the engine is stopped (`VX3DN_stopped`). Keep the forget when the query completed a pending blit.
- **An unknown WGL attribute is an error** (`ERROR_INVALID_DATA`), not "no constraint". Attributes of extensions the driver lacks (BIND\_TO\_TEXTURE, sRGB, float) are known with value 0: asking for one matches nothing, asking for none passes.

**Testing a change behind a stubbed call.** The linked VxD harness stubs `VX3D_busy`, so the state generation could not show the fix. Count the stub's calls instead (`busy3d_calls`): the query must not reach it. Always run a new test on the build before the fix to see it fail.

**Flip flags (finding 9): measure before changing.** VXFLIPF times WAIT, DONOTWAIT, NOVSYNC and INTERVAL2 flips and reads what the Flip callback saw (QueryPhases words 356-361).

### 33.59 Flip flags as the Win98 runtime hands them to the HAL (.575 board, .576 fix)

**Measured (VXFLIPF v1.0, DirectDraw 7, full-screen flip chain).**

- `DDFLIP_WAIT` never reaches the Flip callback: waiting is the runtime's loop. A waiting flip arrives as `01000000` (interval 1).
- `DDFLIP_DONOTWAIT` does arrive (`01000020`). `DDFLIP_NOVSYNC` arrives as `00000008`, `INTERVAL2` as `02000000`.
- A flip call costs 0.17 ms; INTERVAL2 33.25 ms (two refreshes). NOVSYNC costs the same as a plain flip.

**Recipe.** In the Flip callback, if the flags have `0x20` and the last flip is still pending (the same VxD question GetFlipStatus asks: `VXF_FRONT` with `0xFFFFFFFF`), answer `DDERR_WASSTILLDRAWING` and change nothing. All other flips wait in the VxD as before.

**Test-tool rule.** With one back buffer, any write to the back buffer after a flip is a write to the old front and waits for the flip. A tool that fills before every flip never sees a pending flip. To meet one, flip twice with nothing drawn between.

**Open gap.** `GetMonitorFrequency` answers 0: the DRV does not fill `dwMonitorFrequency`.

**Board, .576, VXFLIPF v1.1 (a flip, then at once another).** Second WAIT flip: waits, median 6.00 ms. Second DONOTWAIT flip: "still drawing" 40 of 40 in 0.03 ms. Second NOVSYNC flip: waits as a plain one (8.55 ms). The recipe above is confirmed.

### 33.60 DirectDraw's monitor frequency (.577)

`GetMonitorFrequency` reads `DDHALINFO.dwMonitorFrequency`, which the 16-bit display driver fills each time it builds the HAL info (every mode set). Left at 0, DirectDraw reports 0 Hz.

**Recipe.** The VxD answers a new call (`OP_VX_HZ`): the rate the mode was set with when the driver chose the timing (`vesa_set_freq`); otherwise the period of the blank signal already measured for flips (`vb_probe`, three frames in milliseconds), snapped to the nearest usual rate (50, 56, 60, 65, 70, 72, 75, 85, 90, 100, 120); 0 only when there is no blank signal. Millisecond timing over three frames cannot tell 70 from 72 Hz, so the set rate is preferred whenever it is known. The DRV calls it in `buildDDHALInfo`.

### 33.61 Stripping dead code from the display driver and the VxD (.578)

**Recipe, in the order that keeps each step checkable.**

1. **Files no compiler opens.** Run the build under `strace -f -e trace=openat,execve` and keep only opens made by the compiler, assembler, resource compiler and linker processes. The build script itself opens every source file to hash it, so opens by Python do not count. Do not trust "not named in build.py": names built in loops are missed (this deleted files the build needed, and the build failed).
2. **Preprocessor branches never taken.** Resolve only symbols that are the same in every compile of a file: `VBE` off, the other serial ports off, `#if 0`, and `VESA` on in the files compiled only through their `_vesa` wrappers. Check: the VxD built after this step was byte-identical to the one before.
3. **Do not resolve build symbols in a header that host tests also compile.** `vxd_mouse_conv.h` is built by a test without `VX85598_BUILD`; resolving it there broke the test.
4. **Unreachable cases.** A dispatch whose own allow-list rejects a code makes that code's case dead (the device-I/O FBHDA cases). An operation the display driver never sends makes its case in the VxD dead, and the wrapper in the DRV too.
5. **Functions nothing calls.** Take the public names from the link maps, count occurrences over all sources, tests and build files; a name whose only code occurrence is its definition goes, with its prototype. A name used inside a `#define` in a header is in use (`VX_complete` in `VX_SETTLE`). Rebuild and repeat; then remove what the compiler reports as defined and not referenced. Read the warnings from the build log: the build prints them only when it fails.
6. **Licence files and notices of inherited code stay.**

**Result.** DRV 42,302 -> 41,596 bytes, VxD 398,037 -> 390,681; the suite as before.

## 34. One driver for VX800 / VX820, VX855 / VX875 and VX900

### 34.1 The device profile (.579)

| Chipset | PCI device | Graphics | Frame buffer BAR | Register BAR |
| --- | --- | --- | --- | --- |
| VX800 / VX820 | 1106:1122 | Chrome9 HC3 | 10h | 14h, or 18h when the BAR at 10h is 64-bit |
| VX855 / VX875 | 1106:5122 | Chrome9 HCM | 10h | 14h |
| VX900 | 1106:7122 | Chrome9 HD | 18h | 14h |

**Recipe.** Identify by device ID, pick the two BARs from the table, and decode each properly: bit 0 set is I/O (refuse), type `10b` is 64-bit (its upper dword must be 0), other types refuse. Store the two bases with the type bits cleared in the fields the rest of the driver already reads (`vx_status.bar0`, `bar1`); then nothing after identification needs to know which chip it is. VIA's manual puts the VX800's registers at 18h behind a 64-bit frame-buffer BAR, OpenChrome reads region 1; reading the BAR's type satisfies both.

**What is shared.** VX800 and VX855 take the same 2D register sequence in XP's miniport (`+04,+14,+08,+1C,+48,+18,+10,+0C,+00`) and the same 3D back end in XP and CE (H6). So the VX800 runs the VX855's engine code unchanged.

**What is gated by device.**

- **VX900 engines:** its 2D register order (`+04,+34,+38,+30,+2C,+08,+0C,+10,+00`) and vertex input differ. Until it has its own paths: 2D off, 3D "not ready" (no Direct3D, OpenGL in software), frame buffer, modes, cursor and flips only.
- **Video overlay:** VX855 only. The VX800's HQV unit has another register map; the query answers mask 0 so DirectDraw offers no overlay.
- **PLL:** VX855 and VX900 take the multiplier and divider as computed; the VX800 holds each less two (OpenChrome `ViaSetDotclock`). Until a board has shown the driver's own timing on a chip, that chip uses the BIOS's timing.
- **Video ROM check:** strict device number only where the ROM has been read (VX855); elsewhere a VIA x86 ROM.

**Open on the VX800.** XP's queue fields (`7F` on 1122, `FF` on 5122) and bit 0 of register 60 in ring mode have not been compared with what the ring code writes.

**Test.** `tests/chip_profile.py` runs the compiled identification against a modelled PCI configuration space and ROM. On the board, VXREG prints the device, both BARs and whether the engines are used (TEXDBG 75).

### 34.2 First VX800 run (.580) and what it taught (.581)

**Board facts (VX800, 1106:1122 rev 11).** Frame buffer BAR at 10h = D8000000, registers at 14h = DE000000. The VxD reaches bring-up stage 6 with no failure; 2D at 16bpp runs through the engine with the VX855 sequence (VXGTUNE: fills, text, lines, DIB routes all engaged, pixel-exact apart from the tool fault below).

**Rule: check the DirectX version before reading any 3D result.** The test machine had DirectX 6.1 (no `DirectDrawCreateEx`). The Direct3D HAL, the OpenGL ICD (it makes its buffers as DirectDraw 7 surfaces) and VXDX7, VXFLIPF, VXGL, VXSTEN, VXTEX all need DirectX 7 or later. On 6.1 every one of them fails for that reason alone; nothing about the chip can be concluded. VXREG v1.42 prints the DirectX version first.

**Rule: a tool run from a batch file must not share the screen with an open DOS window.** RUNALL.BAT's window stayed open beside the tests; its blinking cursor was read back by VX2GDI and VXGTUNE as 7 differing pixels at x 6 (values exact complements, and "DIBENG drawn a second time: 7 pixels differ" on the same cases). RUNALL now restarts itself minimized (`start /m RUNALL.BAT GO`). Signature to recognise: a small fixed pixel count at one fixed position, complement colours, and the reference path itself not repeatable.

**Bug fixed (.581): OpenGL MakeCurrent with no buffers.** When the window's surfaces could not be made at all (no DirectDraw 7), MakeCurrent logged it and went on; swrast then read a depth and a colour buffer that were never attached and faulted (`_swrast_clear_depth_buffer`, `_swrast_copy_teximage2d`; null + 28h). MakeCurrent now refuses the binding (ERROR\_NOT\_ENOUGH\_MEMORY, the old binding stays) when the window has never had a back buffer. Buffers lost later keep the binding as before.

**Diagnostic added (.581): why DirectDraw has no HAL.** `DriverInit` records calls, the check that refused it (1 no context, 2 shared block size, 3 VxD silent, 4 bring-up not complete, 5 2D record, 6 depth, 7 frame buffer address differs; 0x100 accepted) and the VxD's state then, in QueryPhases words 363-365. VXREG v1.42 prints them with DirectDraw's caps (NOHARDWARE = 02000000), video memory, and the display driver's own DDCREATEDRIVEROBJECT record (built n times, SetInfo accepted or refused).

**Open after .580 on the VX800 (not explained by the logs):** (1) DirectDraw ran without the HAL (caps 0A000000, 1.5 MB video memory in DxDiag) on DirectX 6.1; whether that is the DirectX version or the chip is what VXREG v1.42 decides. (2) VX2GDI at 640x480x32: the new 2D paths drew nothing and handed nothing back (16bpp fine). Compare with the same batch on the VX855 before changing anything.

**Map lookup recipe.** A fault address in VXGL.DLL (base 21000000) or VXDDRAW.DLL (A0000000) is looked up in tools\\VXGL.MAP / VXDDRAW.MAP: the last symbol at or below the address in the address-sorted list.

### 34.3 VX800 with DirectX 9: the VX855 code runs it (.580 board results, .582)

**Result.** With DirectX 9 installed, the unchanged VX855 paths pass on the VX800 (1106:1122): DirectDraw HAL used with 49 MB of video memory; VX2FLIP all checks (flips, vertical blank at 60.1 Hz, stretch and key blits); VXFLIPF (DONOTWAIT refusal, monitor frequency 60 Hz); VXDX7 2 devices, 0 failures; VXGL, VXSTEN, VXTEX 0 failures; VXGTUNE pixel-exact. So the VX800 needs no 2D or 3D sequence of its own: same engine order, same H6 vertex encoder, same ring. Quake II demo runs at about 90% of the VX855's rate (user's figure; not yet broken down).

**The no-HAL result of 34.2 belonged to DirectX 6.1, not to the chip.** Still a gap to close: on a DirectX 6.1 runtime DirectDraw does not take the HAL, and the ICD has no buffers because it asks for DirectDraw 7 surfaces. VXREG v1.42+ on such a machine names the DriverInit check that refused.

**Bug fixed (.582): the display driver's copy of the ACCEL\_HOST mask could stay 0 for good.** `VX2D_bind` (every Enable, so every depth change) clears the VxD's record of where the DRV keeps `vx_host_mask`; the record comes back with the next host request. A copy that was 0 at that moment (a test tool's "all off") makes no request, so no later mask write reached it. Seen as VX2GDI at 640x480x32: "new path drew 0, handed back 0" on all six cases, after the 16bpp phase ended with the mask at 0. Recipe: the DRV sets its copy to 63 after each bind; the VxD checks its own mask before drawing and answers every request with the live value, so one request corrects the copy. Rule: a cached permission that gates the only call able to refresh it must start open, not closed.

**Reading VX2GDI differences.** The test area is the screen's top-left 600x440. A difference outside every rectangle the case draws, or larger than all of them together, is another window on the screen, not the path under test (board: 58955 pixels, first at 272,159, in the sprite case that draws 31280 pixels elsewhere). VX2GDI v2.1 prints the bounding box and draws the new path a second time.

**Open: taskbar gone after Alt-Tab out of full-screen Quake II (OpenGL) on the VX800 at a 640x480x16 desktop**; quitting did not bring it back, a resolution change did. Not yet known whether the VX855 does the same. VXALT v1.0 plays the sequence (plain window, OpenGL window, OpenGL window with mode change; minimized, then closed while minimized) and reports the taskbar's rectangle, visibility, z-order at its own place, the work area and the Start corner's pixels after each step.

### 34.4 VX800: video overlay and the driver's own timing (.583)

**Confirmed on the board with .582:** the ACCEL\_HOST mask fix (34.3) — VX2GDI at 640x480x32 draws through the new paths in all cases, 0 differing pixels. VXALT passed its three runs, so the taskbar lost after Alt-Tab out of Quake II is not reproduced by the driver's own handling of that sequence.

**The VX800's HQV register map (overlay through the scaler).** The V1 overlay unit (0x200-0x2FF), the HQV control and addresses (0x3D0-0x3DC, 0x3EC-0x3FC), the filter (0x3E4) and the new-style scale registers (0x3B0 / 0x3B4) are the VX855's. Four things differ, from VIA's VX800 manual part II and OpenChrome (`hqv_cme_regs`, `SetHQVFetch`):

| Offset | VX800 (HC3) | VX855 (HCM) |
| --- | --- | --- |
| 0x370-0x37C | not there | source crop 1-4 |
| 0x380-0x38C | source data offset 1-4 (write 0, right-1<<16 \| bottom-1, 0, same) | destination crop |
| 0x3CC, 0x394, 0x3E8 | not written | background offset, extended control, subpicture vertical scale (0) |
| 0x3E0 | source line count and bytes fetched a line: (width << n) - 1 << 16 \| lines - 1, n = 0 planar, 1 packed 16-bit, 2 RGB32; lines doubled for a bobbed field | subpicture horizontal scale (0) |

Rule: the same offset number is not the same register across HC3 and HCM; never send one family's 0x3E0 word to the other. The emitter branches on the device id at that one block; `VX_chip_video()` now answers yes for 1122 and 5122. Host check: `verification/v10106.py` runs the same shrink with the device set to 1122. Board test: VX2OVL (through DirectDraw).

**The VX800's primary PLL.** SR44 = (dm - 2) low byte; SR45 = 0x80 | dr << 2 | (dm - 2) bits 9:8; SR46 = (dn - 2) & 0x7F (top bit clear); then SR40 bit 1 set and cleared so the PLL takes the values. The VX855 holds dm and dn unmodified, SR46 with its top bit set, and has run without the SR40 pulse. The CRTC timing registers are common. Until a board has shown it the driver's timing stays off by default on 1122 (BIOS timing); probe sub 5 (`VX_IOCTL_PROBE`) switches it live and reads SR44-46 and SR40 back.

**Recipe: testing a timing change without risking the desktop.** VXTIME sets each offered size above 1024x768 twice for four seconds (BIOS timing, then the driver's), with the screen labelled and a white line on the outermost pixels, measures the refresh on the retrace bit, reads the PLL back, restores the switch and the desktop, and asks one question. A wrong clock costs four seconds of lost picture, not a boot into safe mode.

### 34.5 VX800 overlay and timing: first board results (.583-.585)

**Board, .583 (VX2OVL through DirectDraw, 640x480x16):** picture, enlargement, flip, move, hide, YV12, shrink, RGB565, RGB32, source colour key and bob all right. The HQV block of 34.4 is therefore correct for the VX800. Only the **destination colour key** fails: the key-coloured rectangle stays its own colour, no video in it.

**Destination key at 16 bits, tried so far on the VX800:** low bits zero (the VX855's proven form, F81F -> F800F8): fails. Low bits replicated (FF00FF, .584): fails. Third form in .585: the 5-6-5 pixel value itself in the key register (mask bit 12, `VXO_M_KEYRAW`; OpenChrome writes the X pixel value at every depth). VX2OVL v1.5 tries all three live and logs 0x220, 0x264, 0x298, 0x230, 0x210, 0x228, 0x22C, 0x240 for each, so a fourth failure still leaves the registers to read.

**The VX800 BIOS's own PLL words confirm the dm-2 / dn-2 form.** Read back by VXTIME: 1280x1024 SR44 95, SR45 08, SR46 03 = dm 151, dr 2, dn 5 = 108.1 MHz; 1280x800 SR44 44, SR45 88, SR46 81 = dm 70, dr 2, dn 3 = 83.5 MHz. The BIOS uses both states of the two top bits (SR45 bit 7, SR46 bit 7), so neither is required.

**Tool rule: Windows does not set a mode it is already in.** VXTIME v1.0 set each size twice running (BIOS timing, then the driver's); the second ChangeDisplaySettings did nothing, so the driver's timing never ran and every DRIVER step reported the BIOS's. v1.1 goes back to the desktop's mode before every step. Any A/B that depends on code run at mode-set time must change the mode in between.

### 34.6 VX800 complete: overlay key and own timing proven (.585 board, .586)

**Destination colour key at 16 bits on the VX800 = the 5-6-5 pixel value itself** in 0x220 / 0x264 (with bit 30), no widening. Board, VX2OVL v1.5 with .585 (mask 5057 = 961 | 4096): key right at the first try, and every other step right (picture, enlargement, flip, move, hide, YV12, shrink, RGB565, RGB32, source key, bob). The VX855 needs the widened form with zero low bits. So the key register's format is per device: VX855 24-bit widened, VX800 raw pixel.

**The driver's own timing runs on the VX800.** Board, VXTIME v1.1 with .585: eight sizes (1152x864, 1280x720, 1280x768, 1280x800, 1280x960, 1280x1024, 1360x768, 1366x768), each with the driver's timing used, 59.69-60.08 Hz measured, picture steady with all four edges. PLL words written, for reference: 1280x1024 SR44 B3 SR45 88 SR46 04; 1280x720 SR44 51 SR45 8C SR46 00; 1366x768 SR44 D5 SR45 88 SR46 07 (the BIOS's 1366x768 runs at 58.93 Hz; the driver's at 59.69). From .586 the driver's timing is the default on 1122 as on 5122.

**State of the VX800 (1106:1122) after .586:** 2D, DirectDraw, flips, Direct3D, OpenGL, overlay and own timing all run on the VX855's code with three device branches: the registers' BAR (14h or 18h), the HQV block (34.4), and the PLL words plus the 16-bit key form. Kept for the VX900: probe sub 5 (the live timing switch) and VXTIME.

### 34.7 Niggles carried forward (after .587)

- **DirectX 6.1 runtime (decided 2026-11-11: a caveat for now, not built).** DirectDraw, Direct3D and OpenGL acceleration need DirectX 7 or later; the desktop's 2D acceleration works on any version, and since .581 nothing faults on 6.1. To support 6.1 later: find why DirectDraw does not take the HAL there (VXREG's DriverInit line on a 6.1 install), give the ICD a DirectDraw 4 path for its surfaces, and run Direct3D against the 6.1 runtime.
- **.587 bug check:** a refused MakeCurrent (window with no buffers) had already put the old context's occlusion count aside; it is taken back now. Rule (v10.261, restated): whatever can refuse a new binding either runs before the old one is touched, or undoes what was touched.
- **VX800 destination colour key at 32-bit and 8-bit desktops:** not yet run on the board (16-bit is proven with the raw pixel value). The VX800's 16-bit form differed from the VX855's, so the other depths are not assumed.

**.588: OpenGL on a DirectX older than 7.** `DrvValidateVersion` answers FALSE when DDRAW.DLL has no `DirectDrawCreateEx` (only the export is looked for), exactly as it does for a VxD of another build; opengl32 then draws with Microsoft's own renderer, so the program starts and runs in software instead of failing. Recipe: an ICD that cannot work on a system declines at DrvValidateVersion, not at MakeCurrent — that is the one point where opengl32 still has a fallback. Host check: `tests/gl_icd_failures.py` `validate_case` (with and without the export). On the board's DirectX 6.1 run DxDiag said "DirectDraw: Acceleration Enabled", but that line only means it is not switched off: DirectDraw's caps had NOHARDWARE (02000000) set and video memory was the screen's 1.5 MB, so the HAL was not in use; hardware OpenGL there still needs that solved first (34.7).

### 34.8 OpenGL front buffer lost after a swap (found on the VX800, .588-.590)

**Symptom.** VXGL's check "front clear reads blue" failed about one run in three: black, or the picture just swapped. With the engine and without it, at any desktop size.

**How it was narrowed (each a tool step, keep the method):**

1. Repeat the one check many times in the tool (40, then 30 per variant) instead of rerunning the tool: the rate (1-7 in 30) and the condition (only right after SwapBuffers, never without one) showed at once.
2. Log the wrong value: red = the swapped picture, and still red 30 ms later. So the front holds the window's picture: its fill from the window happened after the clear, or the clear never reached it.
3. Vary the timing in the tool (glFinish or 20 ms after the swap): still fails, so not "the swap's copy still running".
4. Measure the suspected hardware cause in the VxD (VXIDLE, control FFFFFE40: status reads between a 2D command and the first busy read, 2000 copies): busy at read 0 every time, a 256 x 256 copy busy for 283-437 reads. **The VX800's 2D engine shows busy at once; the 32-idle-reads wait does not end early.** Hypothesis discarded by measurement.
5. Switch the driver's features live from the tool: with the swap's copy not asynchronous (FFFFFE22) or all asynchronous copies off (FC000000), 0 wrong in every ICD row. **The cause needs the asynchronous present (v10.424).**

**Design flaw fixed in .590 (first attempt, not yet board-proven).** The front buffer is filled from the window "when next used" after a swap (fstate bit 0). A draw into it set only "drawn" (bit 1). If the fill did not happen before the draw (it failed, or was not tried because the surfaces were locked), the next use did the fill then, over what had been drawn. Now every draw into the front sets both bits. Rule: state that says "to be initialised lazily" must be cleared by the first write, not only by the initialisation.

**Still open.** Why the fill does not happen right after an asynchronous present (VXGL v5.2 turns the ICD's log on for its own process with the environment variable VXGL\_LOG=1 and copies the front-buffer lines into VXGL.TXT). And Microsoft's renderer on this display driver reads the swapped picture where it just cleared, correct 30 ms later: a fill not yet landed when the screen is read directly; separate from the ICD's bug.

**Tool note.** GetPixel on the window reads black under the ICD (not judged). A tool can switch the ICD's registry values for itself with environment variables of the same name (v10.358).

### 34.9 .592: a 2D command must be waited for from its start once the ring-paused idle rule is in use

**Symptom (VX800, VXGL v5.4 on .591).** After SwapBuffers a front-buffer clear sometimes reads back as the swapped picture. The ICD's trace is identical in right and wrong tries (load copy window to front, CPU clear, present, read). Under Microsoft's renderer with every asynchronous path off, a GDI fill after the swap reads as the old picture and as the fill 30 ms later; a 20 ms pause before the clear cures it.

**Cause as coded.** `idle_bits` (v10.402) takes status `0x10` (command regulator only) with 444 bit 31 as idle once `vx3dn_xr_cr_pause` has been set, and it stays set. That is also what the status shows while the regulator still holds a 2D command it has not yet handed to the engine. The synchronous wait is one idle read (`vx2d_op_wait`), so the request returned with the command not yet run; the CPU then wrote or read the surface and the command ran afterwards.

**Rule.** With `vx3dn_xr_cr_pause` set, or right after an asynchronous command was completed in the same request, wait with `idle_started()`: up to 256 reads for the 2D engine's own busy bit (status value 2), then 32 stable idle reads. `complete_pending` the same. Never judge "finished" from an idle read alone in a state where idle and "not started" look the same.

**Checks.** `verification/engine.py`: model engine with `late` = 1, 20, 200 reads of status 0x10 before the command starts; fails on .591 (request returns with the command pending). Board: VXGL v5.5 lines "a 2D copy right after each of 200 swaps" (control FFFFFE45: reads before busy shows, whether the idle test passed before that) and "waits from the command's start" (FFFFFE46: waits, waits where busy never showed).

**Failed approaches.** .590 `fstate|=3` in the ICD (the ICD's order was already right). Desktop size, the asynchronous present alone, a failing refill, Microsoft's renderer: all measured away. VXIDLE's "busy at read 0" was taken standalone, where bit 4 counts as busy, so it did not cover this state.

**Open until the board says so.** Whether 256 reads is enough on VX800 (FFFFFE45's histogram answers it), and the cost for very small GDI operations after OpenGL has run (FFFFFE46 out\[1\] counts waits where busy never showed).

### 34.10 .593: no 2D command is finished on its first idle status read (corrects 34.9)

**Measured (VX800, VXGL v5.5 on .592, three runs, 1200 timed copies after swaps).** The 2D engine's busy bit (status value 2) shows at the second status read after the command, never at the first. Once the first read was `00000000`. `vx3dn_xr_cr_pause` was 0 throughout, so 34.9's "regulator bit read as idle" was not the cause on this board; 34.9's wait helped only because it also covered commands that follow a completed asynchronous one.

**Result of .592.** ICD front clears: 16 and 8 wrong of 60 before, 0 of 60 after in all runs; one wrong left in 1260, in the row where glFinish had already completed the pending copy. Microsoft's renderer: still 1 or 2 wrong per run (GDI fill, nothing pending). Both leftovers are commands that still had the one-read wait (ACCEL\_OP\_WAIT, v10.133).

**Rule.** After writing a 2D command, look for the busy bit first (up to 16 reads), then take 2 idle reads in a row (8 if busy never showed): `idle_from_start()`. Used for every synchronous command and for the span path's one-read wait. Waits that need 32 idle reads in a row were never exposed, because busy shows by the second read. The stronger 34.9 wait stays for commands after a completed asynchronous one.

**Checks.** `verification/engine.py`: first 1, 2 or 8 status reads `00000000` before busy; fails on .592. Board: VXGL v5.6 lists the status values at the first read after the command and control FFFFFE47 (waits, busy never seen in 16 reads, busy later than the second read, time-outs).

**Not explained yet.** In one of the 1200 copies the engine was already busy (status 0002) before the command, after `complete_pending` had seen 32 idle reads; and under Microsoft's renderer a 20 ms pause before the clear always cured it.

**34.10 confirmed on the VX800 (.593, VXGL v5.6, five runs).** 90 front-clear rows, 3150 tries, 0 wrong, in all three sections (ICD with the engine, ICD without, Microsoft's renderer). The status at the first read after a 2D command was `00000010` in 3000 of 3000 timed copies: the command regulator's bit alone, the engine's busy bit (2) from the second read. So on this chip the first read is never the engine's own answer, and `00000010` must never count as idle right after a command (which the ring-paused rule of v10.402 would do if it were ever in use here: 34.9's wait stays). Cost of the rule: of 5698 waits, 2 never saw busy within 16 reads, none saw it later than the second read, no time-outs.

**Still open.** In 3 of about 4200 timed copies the probe found the engine or the regulator busy before it wrote its command, although `complete_pending` had just seen 32 idle reads; in 2 of them the probe's copy then never showed busy. The probe programs without the driver's `idle_fast` check, so the driver's own path is not shown to be affected; the source of that activity is not identified.

**34.10 on the VX855 (.593 RUNALL) and the tuned wait (.594).** The VX855 behaves the same: first status read after a 2D command `00000010` in 200 of 200, busy bit from the second read; front clears 0 wrong; all other tools pass. Of 113,489 waits, 48,248 (42%) never showed busy within 16 reads (short lines and small fills finish before the bit can show) and none showed it later than the second read. Those commands paid 24 reads, about 6 us: VXGTUNE's engine lines up to 48 pixels took 17.4 us against 12 to 15 for longer ones. **Rule, final form:** 4 reads for the busy bit, then 2 idle reads in a row (`idle_from_start`). Do not search longer than the chip needs: the search is paid in full by every command that is already finished.

**Tool note.** VX2GDI and VXGTUNE can report 7 differing pixels at x = 6 with complementary colours (6B6323 against 949CDC): a blinking caret in a window under the test area. VX2GDI's own repeat of DIBENG differs the same way; it is not the driver.

### 34.11 .595: a brush that is not one colour as the pattern of a staged ROP blit (XF\_COLPAT)

**Gap.** A BitBlt from a memory bitmap with a ROP that uses pattern, source and destination (PSDPxax, DSPDxax) was sent to the engine only when the brush was one solid colour (XF\_FIXPAT). A dithered colour (any colour without an exact 5-6-5 or palette value) or a brush made from a colour bitmap went to DIBENG: right pixels, about 11 ms against 1.5 ms for the case in VX2GDI.

**Recipe.** DRV (`vx_try_memblit`): when the ROP uses the pattern and `vx_solid` says no, refuse hatches, 1-bit bitmap brushes and hollow brushes (their colours come from the draw mode; not measured with these ROPs), then build a PATCOPY fill of the same rectangle with `vx_build_only` and copy the built request into `tpl` with flag `VX2D_XF_COLPAT` (64). That reuses the one place that knows how each depth stores realized brush pixels. VxD (XF\_STAGE): `put_block(&tpl)` once, then per band the usual staged copy with the command's fixed-colour bit (0x2000) clear and register 20 = block address >> 3 | ((dy + band row) & 7) << 29 | (dx & 7) << 26, exactly the phase rule of the colour pattern fill (v10.77). Refused: both flags, the flag with a ROP that has no pattern, no brush block (reasons 14, 14, 18).

**Checks.** `verification/v10116.py`: XF\_COLPAT at 16 and 32 bpp, ROPs B8 and E2, three rectangles with different phases, every pixel against the ROP3 with pattern pixel (x & 7, y & 7). VX2GDI v2.3: a new case with a pattern brush from an 8x8 colour bitmap, a brush origin of 3,5, odd positions and sizes, a 9x9 blit, and a dithered colour; run at 16, 32 and 8 bits (the 8-bit phase runs the two brush cases only).

**Assumption until the board confirms it.** That the engine takes the 8x8 block as the pattern operand of a copy that also reads a source; so far it was measured for fills only. The model was extended on that assumption. If VX2GDI shows differing pixels in the new case, the assumption or the phase is wrong, not the tool.

**34.11 on the VX800 (.595, .596) and the second entry point (.597).** Confirmed on the board at 16, 32 and 8 bits: the engine takes the 8x8 block as the pattern of a copy that also reads a source; 0 differing pixels; the new VX2GDI case 53 ms to 1.4 ms (16 bpp), 106 to 2.5 (32), 27 to 0.9 (8).

**Rule: a blit from a DIB section never reaches BitBlt.** GDI hands it to StretchDIBits with the same ROP and brush (capture `VXH_ARM_BLITS`: record XDIB, ROP 00B8074A). Anything added to `vx_try_memblit` must be added to `vx_dib_rop` too, or DIB section sources silently keep the old behaviour. .597: one routine, `vx_colpat`, used by both. At 8 bpp `vx_dib_rop` takes no DIB at all (indices go through the colour table): by design.

**How it was found (keep the method).** A table in the tool, one blit per cell (brush kind x source kind x ROP, + engine / . DIBENG), then the capture escape for one cell: `VXH_ARM` for the driver's brush and 'MBSK' skip records (which exit of `vx_try_memblit`, brush flags, source type), `VXH_ARM_BLITS` for the call that actually arrived. Arm the capture after the test background is drawn: its 32 slots fill with the background's own fills otherwise.

**Tool rules from these runs.** Tools that draw on the screen DC and compare pictures must (1) take the focus with a window of their own, so no caret blinks through (7 pixels, complementary colours), and (2) make that window cover the screen, topmost, never painting or erasing, so no other window repaints a piece of itself between two pictures (a 16x30 block of untouched background differed once).

## 35. VX900 board (Gigabyte M7V90PI-VL): getting Windows 98 to start at all (open, 2026-11-12)

**Board.** VIA Nano U3300 1.2 GHz, VX900 (graphics 1106:7122), AMI BIOS V9F5 09/14/11, 2 GB DDR3 SO-DIMM, an OEM thin-client board (no vendor page, few BIOS options), driven through an IP KVM. Disk controller 1106:9001 in native mode (programming interface 8F), IRQ 11.

**Symptom.** Fresh Windows 98 SE install freezes on the first restart, Safe mode too, with no driver of ours installed. BOOTLOG.TXT ends after the real-mode drivers.

**What is established.**

- Windows does enter protected mode and initialises core drivers in order with success lines (IOS, PAGEFILE, PAGESWAP, PARITY, REBOOT at least).
- BOOTLOG.TXT looks short because Windows writes each line through DOS but the file's length is only recorded at close; ScanDisk finds no lost chain afterwards.
- DOS with EMM386 runs (protected-mode switch and V86 disk access work on this board).
- At rest from DOS: keyboard controller, timer and IRQ 0, interrupt controllers, clock, PCI BIOS, VGA status port (retrace toggles) all normal. No NMI arrives when both classic NMI sources are switched on. The EBDA is at 9F00 with size byte 1 K while base memory ends at 636 K.

**Ruled out (each tested on the board).** Memory size (HimemX /MAX=524288 confirmed by MEM, plus MaxPhysPage / MaxFileCache); USB legacy and KVM virtual media; BIOS CPU options; Patcher9x; `win /d:xvsf`; starting from EMM386; EBIOS (kept out with an INT 15 AH=C1 "not supported" answer: same freeze); NMI from the PARITY driver.

**Rules learned.**

- A resident DOS tracer that writes to B800 goes blind when Windows' display driver (VDD) takes the screen memory: "the last line shown" is not the last line written. Two wrong conclusions came from this (EBIOS, then "freeze after REBOOT").
- `ebios=*ebios` in SYSTEM.INI and deleting `Services\VxD\EBIOS` with real-mode REGEDIT did not keep EBIOS from loading; answering INT 15 AH=C1 with carry did.
- To get the true end of the log, commit the file after every line: a resident INT 21 hook that lets AH=40 through and then issues AH=68 for the same handle (VX9TRACE v1.4). Limit: once IFSMGR takes over file access the hook no longer sees the writes.

**Tools (DOS, sources in the tree under `dos9/`).** VX9PROBE (legacy hardware report), VX9TRACE (log lines and commits, IRQ counts), VX9EBIOS (INT 15 AH=C1 answer), VX9NMI (NMI source test).

**Next.** Read the committed BOOTLOG.TXT after a freeze under VX9TRACE v1.4 for the true last line; then act on that driver (settings line, replacement file in `SYSTEM\VMM32\`, or a resident fix). Fallback: finish setup on the VX800 and move the disk.

**Scope from 2026-11-12: the VX800 / VX855 driver.** The VX900 is shelved until Windows 98 starts on that board (section 35). `CHROME9.INF` lists only 1106:1122 and 1106:5122; the 1106:7122 line and its name string were removed (build number unchanged, .597). The VxD still recognises 7122 and would run it as a plain frame buffer if the driver were forced onto one.

**WinTune 98 reference figures (MPixels/s; different machines, compare within a mode).** 640x480 16-bit: VX800 this driver 112.4 / 233.9 / 81.1 (2D / Direct3D / OpenGL); CN896 with VIA's driver 48.2 / 90.9 / 36.4; Intel 852/855GM with Intel's driver 136.7 / 126.1 / 109.1. 1024x768 32-bit: VX855 this driver (.560) 145.0 / 254.1 / 124.3; the same Intel machine 120.0 / 55.6 / 60.5. No VX855 run at 640x480 16-bit is recorded.

**WinTune, VX855 at 640x480 16-bit (added 2026-11-12, current driver, two passes):** video 120 +/- 14, OpenGL 150 +/- 2.4, Direct3D 417 +/- 0.3 MPixels/s (Direct3D Null driver score 90, Primary driver score 54). This replaces the note above that no such run was recorded. In that mode: Direct3D VX855 417, VX800 234, Intel 855GM 126, CN896 91; OpenGL 150, 81, 109, 36; 2D 120, 112, 137, 48.

## 36. Chip families and what a port to another chip needs

### 36.1 Lineage

**UniChrome family** (DirectX 7 class, H2 2D engine), oldest first: UniChrome (CLE266, KM400, KN400), UniChrome Pro (CN700, P4M800 Pro, K8M800), UniChrome Pro II (CX700, VX700).

**Chrome9 family** (DirectX 9, shader model 2.0, hardware vertex processing), oldest first:

- Chrome9 HC: CN896, P4M900. The cousin: same 3D core, H5 2D engine.
- Chrome9 HC3: VX800, VX820. M1 2D engine. Supported.
- Chrome9 HCM: VX855, VX875. M1 2D engine. Supported.
- Chrome9 HD: VX900. M1 2D engine. Not supported (section 35).

The three M1 chips are the close relatives; one set of binaries serves VX800 and VX855. Platform succession: UniChrome Pro boards were followed by Chrome9 HC, and UniChrome Pro II (CX700/VX700) by Chrome9 HC3.

### 36.2 What a CN896 / P4M900 port would need

Not done and not tested; these notes are from specifications and the Linux driver, so check each on hardware first.

- **Device ID:** believed 1106:3371 (shared with P4M900). Confirm on the board. The graphics core sits behind the chipset's internal AGP bridge, not on the root bus.
- **2D engine:** H5 register layout. Same operations as M1 but different offsets, so the 2D accelerator needs a second register table, not new logic.
- **Pixel clock:** older PLL encoding than VX855/VX900. Needs its own routine.
- **Display FIFO:** depth, threshold and high-threshold values are per chip.
- **Outputs:** more reliance on external transmitters and DVP ports than the single-chip parts.
- **Carries over:** the 3D command stream, most CRTC and sequencer registers, the VxD/DRV split and the DirectDraw/Direct3D/OpenGL layers.

Rough size: about a third of a new driver. VIA's own Windows 98 driver exists for this chip, so the gain is performance, not function.

### 36.3 WinTune reference figures, 640x480 16-bit

Order used in the README and chart: Chrome9 chips oldest first, Intel last.

| Machine and driver | Video (2D) | Direct3D | OpenGL |
| --- | --- | --- | --- |
| CN896, VIA's driver | 48.2 | 90.9 | 36.4 |
| VX800, this driver | 112.4 | 233.9 | 81.1 |
| VX855, this driver | 120 | 417 | 150 |
| Intel 852/855GM, Intel's driver | 136.7 | 126.1 | 109.1 |
