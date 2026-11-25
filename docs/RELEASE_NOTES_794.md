## .794: the last build of the original line before the rebase

This is the final build of the driver line that ran from .723 to .769B. After this build the project is rebased onto the faster line that replaced this one. This build is published so that this line's design and its code stay available. **For everyday use, take the main branch's release when it comes out.**

**Download:** `VIA_Chrome9_794_full.zip` below, the driver and the test tools in one zip. Install `CHROME9.INF` from the zip's top folder. The `tools` folder is optional.

Supported chips: VX800 / VX820 (Chrome9 HC3), VX855 / VX875 (Chrome9 HCM), VX900 (Chrome9 HD). VX900 boards still need [the boot fix](https://github.com/360alaska/vx900-win9x-bootfix) first.

### What's new in .794

- **3DMark 2001 no longer stops at Game 1's loading screen with hardware T&L.** The driver had created vertex buffers itself since .758. Direct3D 8 keeps a buffer locked until it is first drawn, and a buffer the driver made holds Windows' 16-bit system lock while it is locked. The driver now leaves vertex buffers to Direct3D, as it did up to .757. This costs about 1% of speed on the VX855 in the HarborBench pursuit scene.
- **The test scripts check the driver again.** QUICK.BAT and ALL.BAT are meant to stop when the installed driver isn't the build they came with. Since .737B that check had never run.

### This line since .723, in short

**Speed:** HarborBench pursuit, with no monitor and in milliseconds a frame:

| Chip | .748 | .768 |
|---|---|---|
| VX855 | 268.6 | 237.5 |
| VX900 | 125.3 | 110.3 |

The gains came from:
- the driver's T&L kernels, chosen per processor and timed on each board (VXKBENCH);
- depth fog done on the chip's fog unit;
- the T&L set-up kept between draws that don't change it;
- indexed vertices copied and checked in one pass, and written straight to the engine's queue.

**Pictures:**
- 3DMark 2001's Environment Bump Mapping is drawn right on all three chips, including the VX900's own form.
- Texture wrapping (D3DRS_WRAP) works.
- A third texture stage gets its own coordinate set.

**Stability:**
- A crashed full-screen game no longer keeps every later full-screen program from starting.
- A 3D engine recovery no longer turns 3D off until the next boot.
- Each round of outside code reviews ended with its confirmed bugs fixed.

**Display:**
- The 640x480 sync fix.
- The monitor's EDID is read at boot again.
- If the monitor was off or switched away at boot, the larger modes appear without a restart once it is read later.
- Mode timings are worked out without floating point.

### Known differences from the main branch

The main branch is faster. It has hardware T&L and a DirectX 8 interface, and it was tested on the boards after many more fixes. This line keeps its own design:
- the driver's own T&L in the HAL;
- the vertex-buffer code, kept here but off by default (VXREG VBUF ON turns it on live for testing);
- its own test tools and switches.

### Source

The source for this build is on the `pre-rebase-794` branch. To build it, see BUILDING.md (Open Watcom, then `python3 build.py`).
