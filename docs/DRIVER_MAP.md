# VX855 Win98 driver: interfaces, command order, fallback and recovery (.551/.552)

Read from the source while building .552. One paragraph a component and the file to open for it. Nothing here changed between .551
and .552 except what REVIEW_552.md lists.

## Components

| Binary | Kind | Source | Talks to |
| --- | --- | --- | --- |
| VX85598.DRV | 16-bit NE display driver | src/enable.c, dibcall.c, control.c, init.c, dddrv.c, modes_vesa.c, scrsw_vesa.c | GDI above, DIBENG for everything it does not draw itself, the VxD below (pm16_calls_vesa.c) |
| VX85598.VXD | ring-0 LE VxD | src/vxd_main_vesa.c, vxd_main.c (IOCTLs), vx855_2d.c, vx855_flip.c, vx855_edid.c, vx_native3d_vxd.c, vx_native3d_hw.c | the engine (MMIO, sequencer ports, frame buffer) |
| VXDDRAW.DLL | 32-bit PE, DirectDraw and Direct3D HAL | src/vx_ddraw.c, vx_d3d_hal.c, vx_d3d_core.c (+ .inc), vx_tnl_sse.asm, vx_hal_sse.asm | DirectDraw/Direct3D runtime above, the VxD by DeviceIoControl |
| VXGL.DLL | 32-bit PE, OpenGL ICD | gl/vxgl_*.c, gl/mesa (Mesa 7.0.3), src/vx_d3d_core.c | opengl32.dll above, the VxD by DeviceIoControl |

Shared by ring 3 and ring 0: `src/vx_native3d.h` (the packet, 27,260 bytes, and the result codes) and `src/vx_native3d_hw.c` (the
validation and the emitter; the host tests compile the same file).

## GDI (VX85598.DRV)

- Exports the standard display ordinals (BitBlt 1 … SetAttribute 18 and the rest of `src/makefile`'s list, kept as upstream VMDisp9x has them).
- Own code: Enable / Disable / ReEnable (mode set through the VxD), BitBlt, StretchBlt, StretchDIBits, Output, ExtTextOut, Control
  (escapes, DirectDraw's DCICOMMAND), the cursor (MoveCursor, CheckCursor: the hardware cursor).
- Fallback: any call or raster operation the engine path does not take goes to DIBENG unchanged. A fired engine operation is never
  replayed in software.
- Recovery: an engine time-out turns acceleration off (`VX2D_device_lost`); a failed recovery stops CPU and cursor access to the primary.

## VxD

- Control: `W32_DEVICEIOCONTROL` (DIOC_OPEN, DIOC_CLOSEHANDLE, and the 0x855000xx codes below), the VDD and mode services of the
  mini-VDD.
- IOCTLs: STATUS 01, LOG 02, 3D_PROBE 10, 3D_TEST 11, 2D_CTRL 12, EDID 13, CRT 14, 2D_SUBMIT 15, 2D_STATS 16, 2D_ACCESS 17, FLIP 18,
  CACHE 19, CACHE_SET 1A, DAC 1B, OVL 1C, 3DN_INFO 20, 3DN_SUBMIT 21, 3DN_TEXDBG 22, RING 23, 3DN_SUBMITN 24, 3DN_QUERY 25, PROBE 26,
  TAPE 27, LOCKMEM 28, SAMPLE 29 (`src/vx_native3d.h`, `src/vx85598.h`).

### A batch of draw packets (3DN_SUBMITN, `batch_impl` in vx_native3d_vxd.c), in order

1. Enter the critical section. The 3D lab busy or a fault: LOST or BUSY. The 2D engine busy: BUSY.
2. Map the MMIO block (once). Interrupts off: unlock the sequencer (SR10 bit 0), read SR1A / SR1E / SR4E; a 2D operation in flight
   (SR1A & 42h, SR4E & 30h) answers BUSY; set SR1A bit 3 and SR1E bit 0 when clear; read SR3C (clock locked, else BUSY). Interrupts on.
3. For each packet: size and opcode checked on the caller's memory; copied in by ranges (`copy_packet_d`: header always, state ranges
   unless the HAL marked them the same as the packet before, then only the vertices and indices in use); size and opcode checked again
   on the copy; validated (`vx3dn_validate`, or the geometry alone when every state range was kept); the next packet prefetched.
4. A draw on the primary's memory excludes the cursor until the batch ends.
5. The packet is built into the open fetch group (`fx_build`, which runs the emitter below into the command block). A group that is
   full is started and a new one opened. A packet too large for any group is drawn alone through the registers.
6. The last group is started (asynchronously unless the cursor is excluded). The sequencer registers are put back only if they were changed.

### The emitter (`vx3dn_execute` in vx_native3d_hw.c), for one packet

1. Wait for idle (status 400h, 8 stable reads, 100 ms limit: LOST).
2. State, one of: the whole set-up (`setup` type-01 registers through the shadow, `texture_setup`, `xp_setup`), or the texture strobe
   and the texture units alone (only a texture changed), or nothing (the engine holds this state). After `vx3dn_forget` (reset,
   refused or rebuilt group) the next packet always sends the whole set-up.
3. Wait for idle. Primitive registers (lines 01/85, points 04/40-48).
4. An indexed packet: a slot of the vertex area (`va_get`), the vertices written there, the 8-word layout (04/00-04, 25, 28) if the
   engine has the 10-word one; then for each burst of at most 960 indices (.555: XP's form): 04/21, 22, 2C, 2D, 2E, 04/20 = 1, 04/23 mode, 04/24 count,
   type 0, the indices two a word (the first in the low half), wait for idle. A texture run change between bursts: 04/20, 21, 22, 2C = 0, the strobe, the unit's
   words. At the end 04/20, 21, 22, 2C = 0. No slot free: the same triangles as inline 10-word list bursts.
5. Other packets: bursts of lists and strips (`burst_head`: type 4, 04/20 on a topology change, 04/23, 04/24, type 0, the vertices),
   wait for idle after each.

### Results and recovery

`VX3DN_OK`, `BAD_PACKET` (refused before anything is sent; the refusing line kept for TEXDBG), `UNSUPPORTED`, `BUSY` (nothing sent;
the HAL retries), `LOST` (the engine did not go idle: `engine_reset`, then `RECOVERED` with the state forgotten, or the device faulted
and 2D acceleration lost), `STALE` (mode generation changed). A started group that hangs is rebuilt from its first packet after the reset.

## DirectDraw (vx_ddraw.c)

- Callbacks: CanCreateSurface, CreateSurface, DestroySurface, Lock, Unlock, Blt, Flip, GetBltStatus, GetFlipStatus, SetColorKey,
  SetExclusiveMode, WaitForVerticalBlank, UpdateOverlay, SetOverlayPosition.
- Before Lock (not of an execute buffer), Blt, Flip, DestroySurface, SetEntries and a context's destroy the held draw packets are sent
  (`VX_D3D_hold_sync`): nothing reads or moves a surface with draws for it still queued.
- Fallback: a blit form the engine does not take is answered NOTHANDLED and DirectDraw's HEL does it.

## Direct3D (vx_d3d_hal.c + vx_d3d_core.c)

- Callbacks: ContextCreate, ContextDestroy, ContextDestroyAll, SceneCapture, RenderState, RenderPrimitive, GetState, SetRenderTarget,
  Clear, ValidateTextureStageState, DrawPrimitives2, CreateSurfaceEx, GetDriverState, DestroyDDLocal.
- A DrawPrimitives2 call (`VX_D3D_DP2` -> `dp2_impl` -> `vx_d3d_dp2`): the callback lock (held: WASSTILLDRAWING); the VxD's info
  (kept 100 ms) and the mode generation (changed: SURFACELOST); the targets re-read from their surfaces; then the command loop.
- The command loop (`dp2_body`): a state command that changes nothing is skipped; any other state command resets the draw template
  and sends the open batch (a texture-only change can stay in the batch as a texture run). A draw: the support check (.552: kept for
  the state, `ep_supported`), the vertex buffer bound once a call, the template built for the state (`template_body`: the pixel
  program composed, the textures resolved, the packet's state filled), then the vertices into the open packet (`bq`): indexed lists
  through `ix_list` / `ix_pass` (spans checked and converted by the SSE2 kernels), other triangles through `fast_triangle`, anything
  the fast forms decline through `primitive` (clipping in the HAL).
- Order to the VxD: `bq` (the open packet) -> `cq` (up to 32 packets, each tagged with the state ranges equal to the packet before)
  -> one SUBMITN. The queue is held across calls (.550) and goes when full, at a clear, render-target change, state block, parsed
  command, and from the DirectDraw callbacks above.
- Fallbacks: a draw whose state the chip cannot do is left out and counted (`vx_d3d_dropped`, reason bits in `vx_d3d_unsupported_why`),
  the call still succeeds, ValidateDevice reports it. A primitive with a non-finite vertex is left out. A vertex outside the viewport
  stops the fast loop for that triangle and the general path clips it.
- Recovery: BUSY with nothing taken answers WASSTILLDRAWING at the command of the first packet not taken, with the state that packet
  was made with put back (`cq_snap`). BUSY after some were taken sends the rest again. RECOVERED drops that primitive and goes on. A
  held packet refused or still busy after 500 ms is dropped and counted. LOST marks the context lost (SURFACELOST until re-created).
- Transform and lighting: the runtime's (default). The driver's own (`vx_d3d_tnl.inc`, `vx_d3d_tnl_soa.inc`) only with
  ACCEL_D3D_TNL = 1; with it off hardware T&L is not offered.

## OpenGL (VXGL.DLL)

- Exports: the ICD's Drv* set (DrvCreateContext, DrvSetContext, DrvSwapBuffers, DrvDescribePixelFormat, DrvSetPixelFormat, … and the
  VXGL_*Stats queries; the list is in build.py).
- Mesa does the API, transform, lighting and clipping; `gl/vxgl_tris.c`, `vxgl_state.c`, `vxgl_tex.c`, `vxgl_batch.c` hand triangles,
  strips and state to the shared core (`vx_d3d_gl_*`), which builds the same packets Direct3D does. `vxgl_ssepass.c` is the SSE pass
  over Mesa's vertices.
- Fallback: a state the engine cannot draw sets a fallback bit and Mesa's software rasterizer draws that primitive into the same surface.

## Shared core entry points (vx_d3d_core.h)

`vx_d3d_init`, `vx_d3d_dp2`, `vx_d3d_draw_primitive`, `vx_d3d_flush`, `vx_d3d_hold_send` / `vx_d3d_hold_drop`, `vx_d3d_supported` /
`vx_d3d_unsupported_why` / `vx_d3d_draw_status` (ValidateDevice), `vx_d3d_compose` (the pixel program for a state), `vx_d3d_state_adopt`,
the ICD's `vx_d3d_gl_*`, and the services the caller supplies (submit, submit_n, resolve, bind_vertices, colorkey, mips, faces, target).
