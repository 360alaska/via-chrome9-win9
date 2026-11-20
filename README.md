# VIA Chrome9 display driver for Windows 98 (VX800 / VX855 / VX900)

A from-scratch Windows 98 display driver for the VIA VX800, VX855 and VX900 integrated graphics: accelerated 2D, DirectDraw, Direct3D and OpenGL from one set of files. VIA never shipped a Windows 9x driver for these chips.

Current build: **.706**: tested on the VX855 / VX875; checks on the VX800 and VX900 are pending (see [Checking an install](#checking-an-install)).

**Download:** [`VIA_Chrome9_706.zip`](downloads/VIA_Chrome9_706.zip) (the driver) and [`VIA_Chrome9_706_tools.zip`](downloads/VIA_Chrome9_706_tools.zip) (optional test tools).

**Support the project:** if this driver is useful to you, you can [donate through PayPal](https://www.paypal.com/donate/?hosted_button_id=45HAEA4K4KTXU).

## Supported hardware

| Chipset | Graphics | PCI ID | What works |
|---|---|---|---|
| VX800 / VX820 | Chrome9 HC3 | 1106:1122 | 2D, DirectDraw, Direct3D, OpenGL, video overlay |
| VX855 / VX875 | Chrome9 HCM | 1106:5122 | 2D, DirectDraw, Direct3D, OpenGL, video overlay |
| VX900 | Chrome9 HD | 1106:7122 | 2D, DirectDraw, Direct3D, OpenGL, video overlay |

**VX900 boards need a boot fix first.** Stock Windows 98 freezes at its start-up logo on the VX900, before any display driver loads. The fix is a patched `VDD.VXD`, published separately: [vx900-win9x-bootfix](https://github.com/360alaska/vx900-win9x-bootfix). Apply it, confirm the desktop comes up on the standard VGA driver, then install this driver.

## Where these chips sit in VIA's line-up

![VIA integrated graphics lineage: the UniChrome and Chrome9 families, showing which chips have a Windows 98 driver from VIA and which this driver covers](chip-lineage.png)

VIA's integrated graphics come in two families. This driver covers three members of the newer one.

**UniChrome family** (DirectX 7 class, older 2D engine), oldest first:

1. UniChrome: CLE266, KM400, KN400
2. UniChrome Pro: CN700, P4M800 Pro, K8M800
3. UniChrome Pro II: CX700, VX700

**Chrome9 family** (DirectX 9 class, vertex processing in hardware), oldest first:

1. Chrome9 HC: CN896, P4M900. A cousin of the chips below: the same 3D core, but an older 2D engine with a different register layout.
2. Chrome9 HC3: VX800, VX820. **Supported.**
3. Chrome9 HCM: VX855, VX875. **Supported.**
4. Chrome9 HD: VX900. **Supported**, with the boot fix above. The driver was tested on a Gigabyte M7V90PI board; the boot fix on that board and on an HP t510.

The last three are close relatives: they share the newer 2D engine and the same display core, which is why one set of driver files serves all three. As platforms, the CN896 followed the UniChrome Pro boards and the VX800 followed the CX700 and VX700.

**How the chips compare.** The upper rows come from published specifications, and several are unverified; the lower rows are measured results. The memory figures are theoretical peaks, shared with the processor.

| | CX700 | CN896 | VX800 | VX855 | VX900 |
|---|---|---|---|---|---|
| Graphics core | UniChrome Pro II | Chrome9 HC | Chrome9 HC3 | Chrome9 HCM | Chrome9 HD |
| 3D class | DirectX 7, no hardware T&L | DirectX 9, shader model 2.0 | DirectX 9, shader model 2.0 | DirectX 9, shader model 2.0 | DirectX 9, shader model 2.0 |
| Peak fill rate | about 200 Mpixels/s | about 500 | similar (not published; 250 MHz engine clock) | about 500 (not published) | similar (not published) |
| Shared memory, peak | DDR2-533, about 4.3 GB/s (unverified) | DDR2-667, about 5.3 GB/s | DDR2-667, about 5.3 GB/s | DDR2-800, about 6.4 GB/s | DDR3-1066, about 8.5 GB/s |
| Video decode | MPEG-2 | MPEG-2, partial H.264 | MPEG-2, MPEG-4, WMV9, VC-1 | H.264, VC-1, MPEG-2/4 | ChromotionHD 2.0 |
| 3DMark05, published results | needs DirectX 9 hardware | about 300 to 350 | none found | none found | about 700 to 760 |
| 3DMark06, published results | needs DirectX 9 hardware | about 100 to 125 | none found | none found | about 290 to 300 |
| WinTune Direct3D, Windows 98, 640x480 16-bit | not tested | 90.9 (VIA's driver) | 233.9 (this driver) | 417 (this driver) | not tested |
| Quake II demo (OpenGL), Windows 98, this driver | not supported | not supported | about 90% of the VX855 | 34 fps | 54 fps |
| **3D uplift over the chip before it** | baseline | **about +150%** (fill rate on paper) | **about +157%** (WinTune Direct3D) | **about +78%** (WinTune Direct3D) | **about +59%** (Quake II demo, with a faster processor) |

The 3DMark and WinTune rows are measurements, not specifications, and the uplift row is worked out from them: each chip against the one in the column to its left. The CN896-to-VX800 step compares VIA's driver with this one on different boards, so part of that gain is the driver, not the chip. The 3DMark figures are typical stock results from the HWBOT database, run under Windows XP or later on different processors (the CN896 results mostly on much faster desktop processors than the VX900 ones). The WinTune row is from the Benchmarks section below: three different boards, and VIA's driver on the CN896. The Quake II row is the game's own demo on three boards with this driver; the VX900 board has a 1.2 GHz Nano against the VX855 board's 1.0 GHz Eden, so part of that step is the processor.

What that means for Windows 98 games:

- **The big step is from UniChrome to any Chrome9 chip:** about 2.5 times the fill rate, plus hardware vertex processing.
- **The Chrome9 chips are the same class on paper but not the same speed.** Published 3DMark results put the VX900 at more than twice the CN896, and on this driver the VX855 scores about 1.8 times the VX800 in WinTune's Direct3D and OpenGL tests, and the VX900 board runs the Quake II demo about 1.6 times as fast as the VX855 board.
- **The processor often matters more than the graphics chip.** A VIA Nano is roughly 1.5 to 2 times a C7 or Eden at the same clock, and games of this era lean heavily on the processor.

The specification rows are there to set expectations. This driver's own measured figures are in the Benchmarks section.

**Other chips.** The CN896 and P4M900 are not supported. A port would be possible, since the 3D core and most display registers carry over, but it needs a second 2D register layout, an older pixel-clock encoding and per-chip display FIFO values. VIA's own Windows 98 driver exists for that chip, and it is the one measured in the benchmarks.

## What it does

- **2D (GDI):** engine fills, screen copies, pattern and hatch fills, lines, text, 1-bit and colour bitmap blits, ROP blits that read the screen, with solid, dithered and bitmap brushes. Anything the engine cannot draw exactly goes to Windows' own DIB engine, so the picture is always the one Windows would have drawn.
- **DirectDraw:** hardware blits, stretches (by the 3D engine), colour keys, page flipping through the 3D command ring with or without vertical sync, and a video overlay with scaling.
- **Direct3D:** a DirectX 7 hardware device (textures, blending, fog, depth and stencil, bump mapping), with the driver's own transform and lighting (SSE2, four vertices at a time, point, spot and specular lights) for programs that ask for hardware T&L, and the texture settings of DirectX 5 and older programs honoured. On the VX855 / VX875 the 3D engine clips triangles that cross the screen edge (a guard band), so the processor does not.
- **OpenGL:** an installable client driver built on Mesa 7.0.3, with the chip's 3D engine doing the drawing and Mesa's software path covering what the chip cannot.
- **Display modes:** 8, 16 and 32 bits per pixel; the driver sets its own mode timings and reads the monitor's EDID.
- **Laptop panels:** a panel on the chip's second display unit gets the pointer, page flips and vertical sync (reported working on a Samsung NC20).

## Requirements

- Windows 98 (Second Edition recommended).
- **DirectX 7 or later.** Tested with DirectX 9.0c. On DirectX 6.1 the desktop and 2D acceleration work, but DirectDraw and Direct3D are not hardware accelerated and OpenGL falls back to Microsoft's software renderer.
- A VX900 board needs the boot fix described under Supported hardware.
- More than 512 MB of memory needs the usual Windows 98 precautions (a memory limit in `SYSTEM.INI` or HimemX); that is Windows, not this driver.

## Installing

1. Unzip the driver files into one folder: `CHROME9.INF`, `CHROME9.DRV`, `CHROME9.VXD`, `VXDDRAW.DLL`, `VXGL.DLL`.
2. In Device Manager, open the display adapter, choose **Update Driver**, then **Have Disk**, and point it at that folder.
3. Pick the entry for your chip and restart when asked.

To update an existing install, replace the same five files and restart. All five must come from the same build.

## Files

| File | What it is |
|---|---|
| `CHROME9.DRV` | The 16-bit display driver Windows loads |
| `CHROME9.VXD` | The virtual device: mode setting, the 2D engine, the 3D command path |
| `VXDDRAW.DLL` | DirectDraw and Direct3D |
| `VXGL.DLL` | OpenGL |
| `CHROME9.INF` | Installation script |

## Checking an install

The test set is a separate download (`downloads/VIA_Chrome9_706_tools.zip`) and needs no games. Unpack it and double-click `RUNALL.BAT`; it runs the tests in order in a minimised window and gathers their reports in `tools\LOGS`. Do not move the mouse while it runs. VX2FLIP ends with a message box: press Enter.

| Tool | What it checks |
|---|---|
| VXREG | The build that is loaded, the chip found, DirectX version, whether DirectDraw uses the hardware |
| VX2GDI | 2D drawing against Windows' own, pixel for pixel, at 16, 32 and 8 bits |
| VXGTUNE | Lines, fills, text and bitmap blits: pixels and speed |
| VX2FLIP, VXFLIPF | DirectDraw blits, colour keys, page flips and their timing |
| VXDX7 | Direct3D |
| VXGL, VXSTEN, VXTEX | OpenGL scenes, stencil and textures, with the engine against software |
| VX2OVL | The video overlay (run by hand) |
| VXLEG | Direct3D as DirectX 5 programs use it; run it after an old game to list the settings that game sent (run by hand) |
| VXHOLLOW | Drawing with the hollow brush, as installers' banners use it (run by hand, at 16 and 32 bits) |
| VXPANEL | A laptop panel: which display unit drives it, the mode list, the pointer and page flips (run by hand) |

A report that ends in PASS or `failures=0` is good. `LOGS\RUNALL.TXT` names the last test started if one hangs.

`REGRESS.TXT` in the tools lists every change since build .694 that has so far run only on VX855 boards, with the test that checks it. VX800 and VX900 owners: a `RUNALL.BAT` report from your board is welcome.

## Settings

The driver needs none. These registry values exist under `HKEY_LOCAL_MACHINE\Software\VX85598`:

- logging switches for the OpenGL and Direct3D parts (`VXGL_LOG`, `ACCEL_D3D_LOG` and their sample-rate values), off by default
- `NOEDIDHIGHRES`, for monitors whose EDID should not limit the mode list, and `EDID_MODES` (0 only what the EDID lists, 1 the default, 2 every mode the BIOS has)
- `ACCEL_D3D_TNL`: 0 turns the driver's transform and lighting off, so Direct3D does it for every program (default 1)
- features still under test, all on by default: `ACCEL_D3D_GUARDBAND` (the guard band, VX855 / VX875 only; 0 off), `ACCEL_FLIP_TEAR` (a flip without vertical sync shown from the next scan line, VX855 / VX875 only; 0 waits for the next frame), `ACCEL_3D_RINGFLIP` (page flips through the 3D command ring; 0 the processor) and `ACCEL_D3D_TNFASTIX` (meshes wholly on screen sent to the engine in one block)
- tuning values: `ACCEL_D3D_RINGKB` (the 3D command buffer's size; default 256 KB on the VX800, 512 on the VX855, 1024 on the VX900), `ACCEL_STRETCH3D` (stretched blits: 0 the processor, 1 the 3D engine, 2 the 3D engine with bilinear filtering; default 1) and `ACCEL_D3D_SEAM` (a second, shifted draw that covers one-pixel gaps between meshes; default off)

`VXREG` lists what the loaded driver reads.

## Benchmarks

WinTune 98 results at 640x480, 16-bit, in millions of pixels per second (higher is better):

![WinTune results](benchmarks.png)

| Machine and driver | Video (2D) | Direct3D | OpenGL |
|---|---|---|---|
| CN896 (Chrome9 HC), VIA's driver | 48.2 | 90.9 | 36.4 |
| VX800 (Chrome9 HC3), this driver | 112.4 | 233.9 | 81.1 |
| VX855 (Chrome9 HCM), this driver | 120 | 417 | 150 |
| Intel 852/855GM, Intel's driver | 136.7 | 126.1 | 109.1 |

The VIA rows run from the oldest Chrome9 chip to the newest; the Intel machine is listed last for reference.

How to read this: these are four different computers with different processors, and WinTune's video scores depend on the processor as well as the graphics chip and driver. Against VIA's own driver on the older CN896, both boards on this driver are ahead in all three tests. The Intel machine has the highest 2D score. In Direct3D both boards on this driver are well ahead of it. In OpenGL the VX855 is ahead of it and the VX800 behind. So the table shows roughly where this driver stands beside the chip makers' own Windows 98 drivers for neighbouring hardware; it is not a controlled comparison, and there is no vendor driver for the VX800 or VX855 to compare against directly. The VX900 has not been run through WinTune yet; its Quake II demo figure is in the comparison table above.

## Known limits

- DirectX 6.1 and earlier: no hardware DirectDraw or Direct3D (see Requirements).
- Blits from an 8-bit DIB section with a ROP are drawn by Windows, not the engine.
- The video overlay has not been tested at 256 colours on the VX800.
- A second monitor is not set up by the driver. On the desktop boards it mirrors the first display on its own; an independent second display (extended desktop) is not supported. A laptop's own panel is handled.
- VX900: Windows 98 needs the boot fix before it starts at all (see Supported hardware).

## Building from source

The source builds on Linux with Open Watcom:

```
python3 build.py --watcom <path to Open Watcom>
```

The driver files land in `driver/`, map files in `build/`. `python3 verification/verify.py` runs the compiled-code checks. `BUILDING.md` has the details.

## Documentation

In the `docs` folder of the source:

- `COOKBOOK.md`: "VX855 Win98: building every proven function from scratch". How every part of the driver works, the rules found on real hardware, and the approaches that failed.
- `VX900_SUPPLEMENT.md`: what differs on the VX900 (device and memory layout, 2D engine, command transport, 3D vertex input, overlay) and where each fact comes from.
- `Chrome9_HC3_HCM_HD_programming_research.md`: programming research across the VX800, VX855 and VX900, drawn from VIA's manuals, OpenChrome and analysis of VIA's Windows CE and XP drivers.
- `VX800_VX855_VX900_XP_comparison.md`: how VIA's XP drivers differ between the three chips, with Windows CE cross-checks.

## Licence and sources

See `LICENSE` for the licence terms and `SOURCES.md` for the references the driver was built from. The OpenGL part builds on Mesa 7.0.3, which carries its own licence in `gl/mesa/COPYING`.

## Donate

This driver is free. If you want to support the work: https://www.paypal.com/donate/?hosted_button_id=45HAEA4K4KTXU
