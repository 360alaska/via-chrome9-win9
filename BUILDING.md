# Building VX85598 (source of v10.114; the text below dates from v10.58, still valid)

Use Open Watcom 2.0, October 1, 2026 build, with Python 3 and a host C compiler:

```sh
python3 build.py --watcom /absolute/path/to/watcom
```

This builds the Win16 NE display DRV, ring-0 LE VxD, Win32 PE DirectDraw DLL,
VX2SYNC, VX2BENCH, and the peer diagnostics. The DRV/VxD/DLL are freshly built;
only the unchanged historical VX98TEST client is retained from fallback-1_2c.
Maps, commands, and output hashes are in verification/BUILD.*. The verified host
is Linux x64. Windows-host portability has not been exercised.

The callback DLL exports undecorated `DriverInit` and uses a fixed shared ABI.
Its context layout is checked against the compiled Win16 constants. The INF
must install VXDDRAW.DLL alongside VX85598.DRV and VX85598.VXD.

For the compiled-code tests on a modern host:

```sh
python3 -m pip install unicorn==2.1.4
python3 verification/verify.py
```

The verification entry checks NE exports/imports/relocations, LE fixups/DDB,
Win98-compatible PE subsystem/import modules, the shared ABI and DLL export,
INF installation, source preservation, and linked instruction execution.
It writes verification/RESULTS.json. Windows services, DirectDraw runtime,
DIBENG imports and GPU behavior are models. This is not hardware validation.

The three warnings in the preserved 3D lab core (unused `geo`, possibly unset
`bsx`/`bsy`) also occur in the unmodified peer baseline. This build does not
claim to repair or validate that laboratory path.

## Historical peer build notes

# Building VX85598 v6.15

The delivered DRV and VxD were compiled from the included source with Open
Watcom 2.0's October 1, 2026 Linux x64 build. No Microsoft DDK installation
was required; VMDisp9x supplies the interface headers and import definitions.

Run on a modern build host:

```sh
python3 build.py --watcom /absolute/path/to/watcom
```

The script builds the VESA frontend/backend, native Chrome9 test module, and common modules.
It preserves the exact tested DRV and GDI client from `fallback-1_2c` after compiling.
Both `VX98TEST.EXE` and `VX3DTEST.EXE` use only Win98 system DLL imports.
It links the NE display exports at the upstream Windows ordinals, applies
the upstream fixlink fixes for NE Windows version 4.0 and LE object flags,
and produces the GUI tool as PE32, Windows GUI subsystem 4.0.

On Linux, GCC builds fixlink. On Windows, the script uses wcl386 for that
host utility. Only the Linux host route has been verified here. A new build
directory is used; legacy S3/VIA binaries are not inputs to this build.

Pinned tools and source:

- VMDisp9x: https://github.com/JHRobotics/vmdisp9x
  commit `767761d0a49e4c9d4c66e906f38e6118b8b44f89`.
- fixlink: https://github.com/JHRobotics/fixlink
  commit `c1fd1d2264995b9fa76985c76ef2bd1396af377a`.
- Open Watcom:
  https://github.com/open-watcom/open-watcom-v2/releases/tag/2026-10-01-Build
  asset `open-watcom-2_0-c-linux-x64`, SHA-256
  `a961f3e02ce27bcd88428345dc57483457a623b6e718b6cee52c48d24ab8065f`.

Use the exact asset above to reproduce the toolchain used here. The compiler
itself is not bundled. BUILD.json records the build inputs and binary hashes.
Compiler warnings and errors were checked; the final build has neither.

Current native build is 00060F00, protocol v24, report size 43,424 bytes.
Both VX85598.VXD and VX3DTEST.EXE were rebuilt. The current build scope
is recorded in verification/README_CURRENT.md and RELEASE_V6_14.json.
Only normal compiler/linker checks were run, as requested.

## Historical verification tools

V6.1 was compiled and linked, with no broad verification rerun, at the user's request. Old reports in history/v6/verification are not v6.2 results.
The SF model tools still embody v6 packet assumptions and need updating
before they can assess v6.2 Header0 controls and its alternate triangle.
The commands below document earlier workflows; they were not run for v6.2.

```sh
python3 verification/validate.py
python3 -m pip install unicorn==2.1.4
python3 verification/exercise.py
python3 verification/exercise16.py
python3 verification/exercise_boundary.py
python3 verification/exercise_lifecycle.py
python3 verification/exercise_startup.py
python3 verification/exercise_dos.py > verification/DOS_HOOK.json
python3 verification/exercise_services.py
python3 verification/exercise_native.py
python3 verification/check_native.py
```

`validate.py` checks the NE segment/entry/import/relocation structures, the
LE DDB and every internal relocation, and the PE32 utility's imports and
subsystem. The executable imports only KERNEL32, USER32 and GDI32; the DRV
imports KERNEL and DIBENG, and the VxD has no external DLL imports.

`exercise.py` loads and relocates the delivered VxD and executes its machine
code. Windows allocation/scheduling/semaphore services, PCI I/O and BIOS
responses are modeled. The mode geometries come from VX000.LOG; standard
RGB masks and the ROM are synthetic fixtures. It checks initialization,
failure rejection, all three mode sets, actual shadow-to-framebuffer copies,
partial updates, cursor clipping/erase, access counters, logging bounds,
diagnostic IOCTL sizes and the VxD control entry's stack/register behavior.

`exercise16.py` executes the NE validator, connection checks and DirectDraw
escape with SS different from DS. It runs both numeric real-mode selector
stand-ins and CPU protected mode with synthetic GDT descriptors. Windows
dispatch and selector allocation remain modeled. Backend mode queries
execute the delivered VxD. The original tests caught the phantom-segment
relocation produced by the old code-selector idiom.

`exercise_boundary.py` executes the BIOS wrappers while modeling individual
VMM services. `exercise_lifecycle.py` exercises the real initializer, VDD
registration and descriptor wrappers, and timer initialization, shutdown
and failure behavior. Their BIOS and OS responses remain synthetic.

`exercise_startup.py` executes NE DriverInit/Enable/ReEnable/restore/Disable
with synthetic protected-mode descriptors and modeled KERNEL/DIBENG/VDD
imports. Its API bridge invokes the compiled VxD API handler and BIOS
wrappers. Microsoft DIBENG rendering and Windows' loader are not executed.

Current checks are recorded in verification/FINAL_1_3G.json and CHECK_RUN.json.
They include compiled LE and production-C fault injection, readback evidence,
all 16 probes, capture decoding and the existing display/DOS/timer cases.
The model is synthetic and cannot confirm physical 3D operation.

## Timer correction in 1.3a

`Get_System_Time` now uses a returning VMM call, copies EAX into a C return
value and executes its compiler-generated stack/register epilogue. The 1.3
implementation used a jump-style service in a non-naked C function, leaving
saved EDI/ESI/EBX above the caller return address.

`exercise_services.py` executes the final wrapper instructions and models only
the individual VMM service. It checks timer values, wraparound boundary values,
caller stack integrity and saved registers. `exercise_native.py` likewise
executes the compiled timer and critical-section wrappers instead of replacing
these entire functions with mocks.

To reproduce the defect, extract the original 1.3 ZIP into a separate folder:

```sh
python3 verification/exercise_services.py --baseline /path/to/original-1.3
```

`verification/TIMER_REPRO_1_3.json` records reproduction against the exact old
VxD hash. This reproduces the software error, not the user's full OS state.

## Engine-control correction in 1.3b

The native report is now version 00020000h, 16604 bytes. Its first 96 bytes
and 64x64 image keep their former field order; appended fields identify the
build, sequencer snapshots, initialization flags, independent 2D check,
status observations and PLL polling. Both VxD and VX3DTEST.EXE must be updated.

The native machine-code harness executes the real port I/O instructions.
Its initial MMIO and engine clock controls are disabled, while unrelated
sequencer fields contain nonzero values. It enforces the exact changed
bit masks, preservation of the index register and short IRQ-disabled port
transactions. It models denied writes, held resets, delayed/failed PLL
lock, restoration failure and late busy status. The command model separately
implements bounded 2D rectangle fill and 3D triangle raster checks; it rejects
missing rendering, buffer overruns, timeout and incorrect output.

The 1.3b results are historical. Its physical capture is retained in
verification/hardware_1_3b_2026_10_15; it proves 2D but not 3D.

## VX855 FE correction in 1.3c

The omitted FE block matches the actual PCI 5122 vendor branch. It runs
after 2D and has an independent bounded idle wait. FINAL_1_3C.json identifies
the final files. To reproduce the external oracle:

```sh
python3 verification/vendor_init.py --vendor /path/to/s3ginv_vx855.dll
```

The script requires Unicorn 2.1.4 and the exact DLL hash recorded in
references/VX855_FE.md. The DLL is not bundled or needed to build/run the
driver. It executes chip dispatch and captures writes, not a physical GPU.
The client checks build 00010303 before enabling the Run button.

## Multi-probe tests in 1.3d

After check_native.py builds the host shared library, run:

```sh
python3 verification/check_batch.py
python3 verification/check_capture.py
```

The batch suite executes production C and actual LE across all twelve
probes, dead-engine behavior, two stride interpretations, timeouts/guards,
vertex-source integrity, hold/release failures and real desktop callback
hold/resume/overlap behavior. The capture suite uses real compiled-LE report
bytes with synthetic GPU pixels. FINAL_1_3D.json records current files/counts.
The v3 request/reply sizes are 20/16652; older report sizes in this document
refer to historical 1.3b/c. An optional host parser is available:

```sh
python3 verification/read_batch.py VX3D000.BIN --extract-dir extracted
```

No Python or other runtime is required by the Windows test EXE.

## Command acceptance checks in 1.3e

Build the production-C test library first, then run:

```sh
python3 verification/check_native.py
python3 verification/check_batch.py
python3 verification/check_acceptance.py
python3 verification/check_capture.py
python3 verification/vendor_init.py --vendor /path/to/original/s3ginv_vx855.dll
python3 verification/vendor_readback.py --vendor /path/to/original/s3ginv_vx855.dll
```

Unicorn 2.1.4 executes the compiled VxD and vendor CPU instructions. The
original vendor DLL is optional for external-oracle reproduction and is not
a build input. V4 request/reply sizes are 20/16936 bytes; version 00040000,
build 00010305. Old clients are rejected before MMIO. Historical sections
above describe old protocols and are retained as history.

The acceptance fault suite deliberately models unavailable readback, ignored
3D state, accepted-but-nonrendering commands, port/sync/window requirements
and timeouts during acceptance, synchronization, vertices and post-draw
fencing. It checks that zero-pixel rendering never passes through marker
success, unavailable markers do not turn good pixels into a failure, no
commands follow a timeout, scratch remains GPU-owned, and hold/clocks follow
the established failure policy. These are control/evidence checks, not
validation of any claimed physical unlock sequence.

## Pipeline checks in 1.3f (current)

V5 request/reply sizes are 20/17180, build 00010306. Historical sections above
retain older protocols. Run check_native.py before check_batch.py or
check_acceptance.py to build the host production-C library. Also run
check_readback_layout.py to replay the unmodified 1.3e physical observations.

The acceptance suite models stale user clipping/texture count, failed state
clears, unavailable/wrong-tag readback, dropped state, color-only/depth-only
failure, C2S, and timeouts. The simulated stale-state scenarios demonstrate
deterministic initialization, not the physical cause. Actual LE and production
C must agree. Depth PASS requires GPU-model depth writes and untouched color,
not marker/counter success. Final reports distinguish software execution from
physical capture replay; neither establishes physical 1.3f rendering.

`check_baseline_commands.py --previous /path/to/1.3e` compares the retained
probe-00 production-C stream with the previous source in both normal and
missing-render scenarios. The other probes intentionally change in 1.3f.
The peer review and evidence-based response are included in verification/.


## Matched transport checks in 1.3g (current)

Build first, then run check_native.py to compile the host production-C
library. Run check_transport.py, openchrome_queue.py and check_capture.py.
The actual compiled VxD and production C are paired for queue ownership,
configuration, disabled/enabled rendering hypotheses, timeouts, failed
restoration and guards. The source oracle compiles exact original functions
from the included pinned OpenChrome source. It validates CPU encoding only.

V6 request/report sizes are 20/17360, build 00010307. Requests 00-15 remain
for software regression; the Windows GUI runs only IDs 16/17. Both generated
files must match. The decoder reads v3, v4, v5 and v6 without rewriting the
original captures. Older sections above describe historical release ABIs.

The declared PCI virtual queue is 64 KiB at 00220000; physical backing
reserves twice that plus 4 KiB guard pages. The original source's /8 formula
and HCM manual's /16 description disagree; see PROGRESS.TXT and the review.
Guard checks still reject any write beyond the declared end. This does not
validate queue wrap, concurrency, sustained load or general allocation.

## Added source and fetched-buffer checks

After `check_native.py` builds the host core, run:

```sh
python3 verification/check_batch.py
python3 verification/check_acceptance.py
python3 verification/check_capture.py
python3 verification/check_readback_layout.py
python3 verification/check_baseline_commands.py
python3 verification/check_transport.py
python3 verification/check_pipeline.py
python3 verification/check_dma.py
python3 verification/openchrome_queue.py
python3 verification/openchrome_submission.py
python3 verification/linux_dma_oracle.py
python3 verification/vendor_init.py --vendor /path/to/original/s3ginv.dll
python3 verification/vendor_readback.py --vendor /path/to/original/s3ginv.dll
python3 verification/vendor_dma.py --vendor /path/to/original/s3ginv.dll
```

The original Linux oracle needs GCC `-m32` compilation and GNU binutils; it
uses freestanding fixtures and does not need 32-bit system libraries. Unicorn
executes its resulting CPU code. Original proprietary binaries are optional
verification inputs, not build inputs, and are not redistributed. Hash checks
reject a different vendor DLL. The uploaded OpenChrome source hash and
function executions are recorded separately from the 2008 VIA DMA source.
See references/LINUX_SUBMISSION.md for generation and validation limits.

V6-specific verification: verification/check_sf.py, check_sf_vendor.py and
vendor_ring_audit.py. The latter two require the user-supplied XP DLL and
validate its SHA-256; that proprietary DLL is not a build input or included.
All GPU/Windows services in execution tests are modeled.
