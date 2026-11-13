# v10.58 verification scope

`RESULTS.json` is the consolidated result for the delivered binaries: 119 VxD,
20 Win16 frontend, and 31 DirectDraw callback checks (170 total), plus loader,
ABI, INF and source-preservation assertions. `BUILD.json` records file hashes;
`BUILD.LOG` and maps record the actual Open Watcom build.

Run `python3 verification/verify.py` after building with unicorn==2.1.4.
The tests execute linked x86 instructions with synthetic Windows/DPMI services,
DIBENG imports and a register-level 2D recipe model. The HAL tests bridge the
compiled PE callback to the compiled LE engine. They do not execute Windows,
Microsoft DIBENG, the real DirectDraw runtime, a BIOS or a GPU.

Physical brush realization/phase and RGB565 colors, Win98 callback loading and
allocation, mode transitions, monitor output, hardware completion/coherency,
key/clip pixels, speed and FIFO behavior must still be checked on the board.
No native page flipping, vblank wait, 8-bit mode or D3D HAL is claimed.
