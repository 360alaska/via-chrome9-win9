#!/bin/sh
# .534: builds D3D8PROF (out/D3D8PROF/d3d8.dll) and the caps probe D3D8TLP (out/D3D8TLP/d3d8.dll) from d3d8prof.c (clang + lld, no C runtime)
set -e
cd "$(dirname "$0")"
printf 'LIBRARY kernel32.dll\nEXPORTS\n LoadLibraryA@4\n GetProcAddress@8\n GetSystemDirectoryA@8\n CreateFileA@28\n WriteFile@20\n CloseHandle@4\n QueryPerformanceCounter@4\n QueryPerformanceFrequency@4\n GetTickCount@0\n GetCurrentThreadId@0\n' > kernel32.def
llvm-dlltool -m i386 -k -d kernel32.def -l kernel32.lib
for v in D3D8PROF D3D8TLP; do
 d=""; [ $v = D3D8TLP ] && d=-DTLPROBE
 mkdir -p out/$v
 clang --target=i686-pc-windows-msvc -O2 -march=i686 -mno-sse -fno-builtin -ffreestanding -nostdlib -fno-stack-protector \
  -fno-exceptions -fno-asynchronous-unwind-tables -Wall -Wno-unused-function $d -c d3d8prof.c -o out/$v/d3d8prof.obj
 lld-link /dll /nodefaultlib /entry:DllMain@12 /subsystem:windows,4.0 /version:4.0 /def:d3d8.def /out:out/$v/d3d8.dll /machine:x86 \
  /safeseh:no /opt:noref out/$v/d3d8prof.obj kernel32.lib >/dev/null
 rm -f out/$v/d3d8prof.obj out/$v/d3d8.lib out/$v/d3d8.exp
done
rm -f kernel32.lib kernel32.def
