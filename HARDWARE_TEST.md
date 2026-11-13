# v10.58 physical test order

These checks have not been run on the board. Keep the original v10.57 driver
pair available before installing the matching three-file v10.58 runtime.

## First boot and GDI

1. Install with Device Manager / Have Disk from `driver/`, then reboot at
   640×480 or 800×600, 16 bit. Confirm the desktop, cursor, text, and window
   repainting appear correctly. Check red, green, blue, white, and black fills.
2. Run VX2SYNC. It compares native screen operations with software DIBs of the
   same depth, including actual RGB565 DIB sections. It tests solid and ROP
   fills, 24 overlap offsets, mixed CPU/GPU traffic, patterned brushes with
   shifted origins, and clipped fills/copies. Both parts need zero failures.
3. Check its final native counts. Fill and copy counters must increase; pattern
   and clip counters show which additional hooks were exercised. Transparent
   hatches and unsupported patterns may correctly use software.
4. Repeat at 32 bit. Color pattern RAM and 32-bit brush channel order need
   physical confirmation, even though the register recipes were proved before.

## DirectDraw and speed

1. Run VX2BENCH. Confirm video-memory surfaces can be created, not merely an
   emulated DirectDraw object. Its capability dump should exclude flipping,
   alpha, stretching, hardware system-memory blits, and 3D.
2. All five DirectDraw pixel cases must report zero mismatches. Native
   DirectDraw/source-key/destination-key counters must increase. The XRGB
   cases exercise keys with nonzero, differing unused high bytes.
3. Compare median timings for the same operation count with OFF, CONSERVATIVE,
   and ALL. A result explicitly marked NO NATIVE ACCELERATION OBSERVED is a
   fallback measurement, not evidence of a working GPU hook.
4. Try an existing 16/32-bit DirectDraw game that can render without hardware
   page flipping. Games requiring 8-bit palettes or a flip chain are still
   outside this build's supported set. Record title, resolution, depth, and
   whether it uses windowed blits or exclusive fullscreen.

## Modes, monitor, and boot regression checks

- Cold boot, warm reboot, DOS full-screen switch and desktop return; confirm
  cursor and window updates still work. Repeat after a display-mode change.
- Test DVI EDID detection and VGA fallback with the same monitor/cable setup
  used for v10.57. Save VX2EDID output for each connection.
- Check 640×480, 800×600, 1024×768 at 16/32 bit first. Then test the already
  available larger sizes, including 1440×900, 1600×1200, 1680×1050, 1920×1200,
  plus the peer's added modes. Record crop, centering, border, refresh and any
  artifacts during repeated large copies. Save VX2CRT output per problem mode.
- At 1920 wide, stress repeated fill/copy/window movement and look for FIFO
  underrun symptoms. This release preserves FIFO programming; it does not
  claim an untested FIFO fix.

Do not run the separate 3D lab at the same time as these measurements. The
ownership guards prevent shared engine use; they do not make timing results
meaningful with competing desktop or lab work.

## Recovery

If Windows cannot draw the desktop, reboot to Command Prompt Only and copy
**both** original v10.57 files back from `recovery-v10_57/` to the Windows system
directory. Example, if the recovery folder is `C:\VX1057` and Windows is in
`C:\WINDOWS`:

```
COPY C:\VX1057\VX85598.DRV C:\WINDOWS\SYSTEM\VX85598.DRV
COPY C:\VX1057\VX85598.VXD C:\WINDOWS\SYSTEM\VX85598.VXD
```

Adjust those paths for the installation. Reboot. The old driver does not load
VXDDRAW.DLL. Restore the pair together; do not mix driver generations.

For a functioning desktop with an acceleration problem, the existing registry
switch `HKLM\Software\VX85598\ACCEL_2D` (DWORD 0) disables native acceleration
at the next boot. Preserve the prior value for later restoration. A failed
engine reset deliberately stops further primary drawing; reboot and collect
the logs rather than forcing CPU writes while the engine may still be active.
