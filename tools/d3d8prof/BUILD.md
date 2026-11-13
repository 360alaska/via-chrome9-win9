# D3D8PROF v1.0 (2026-11-07): a d3d8.dll proxy that times every IDirect3DDevice8 call a game makes

Not a modified Microsoft DLL: a proxy with the same five exports (ordinals 1-5) that loads the real D3D8.DLL from the system directory
and hands the game a wrapped IDirect3D8 / IDirect3DDevice8. Each of the device's 97 methods is bracketed with rdtsc (prof_enter /
prof_leave, the return address replaced by prof_epilogue); every call goes to the real device with the same arguments.

Use: copy d3d8.dll into the game's directory; play; quit; read C:\D3D8PROF.TXT (one "F" line a frame at Present: wall, in-d3d8, game,
draws, primitives, draw time, state-call time, Present, Clear; a per-method table at exit). Remove the file to go back to normal.
Read with VXD3DMON: D3D8PROF's draw time minus the HAL's DrawPrimitives2 time is Direct3D 8's own share (its software vertex processing).

Build (no C runtime; clang + lld, cross from Linux):
  clang --target=i686-pc-windows-msvc -O2 -march=i686 -mno-sse -fno-builtin -ffreestanding -nostdlib -fno-stack-protector \
    -fno-exceptions -fno-asynchronous-unwind-tables -c d3d8prof.c -o d3d8prof.obj
  llvm-dlltool -m i386 -k -d kernel32.def -l kernel32.lib      (kernel32.def: LIBRARY kernel32.dll / EXPORTS LoadLibraryA@4 GetProcAddress@8
                                                                GetSystemDirectoryA@8 CreateFileA@28 WriteFile@20 CloseHandle@4
                                                                QueryPerformanceCounter@4 QueryPerformanceFrequency@4 GetTickCount@0 GetCurrentThreadId@0)
  lld-link /dll /nodefaultlib /entry:DllMain@12 /subsystem:windows,4.0 /version:4.0 /def:d3d8.def /out:d3d8.dll /machine:x86 \
    /safeseh:no /opt:noref d3d8prof.obj kernel32.lib
Open Watcom builds it too (wcl386 -bd, the same exports), but the inline asm is GAS syntax: convert to #pragma aux first.

Known limits: a device obtained through QueryInterface bypasses the wrapper (RenderWare does not do this); a second CreateDevice is
handed out unwrapped and logged; one shadow stack (the game's render thread), 64 deep.
Board: ran clean on Windows 98 with GTA3 (2,033 frames, .524): gameplay frames with 200+ draws: wall 100 ms, in d3d8 80, game 20.6;
707 draws and 24,350 primitives a frame (34 triangles a draw), DrawIndexedPrimitive 114 us a call.
