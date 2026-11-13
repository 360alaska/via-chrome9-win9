# Scope and reference points

The Win98 architecture remains a Win16 display driver, VxD hardware owner and
shared PE32 callback DLL. No WDDM interface is used.

The supplied v10.148 is authoritative for this rebase. Its vx855_3d_core.c contains the later
CPU-port/Gouraud/Z experiments and is unchanged. The new emitter extracts a
bounded app-facing subset instead of running the large diagnostic laboratory
against runtime-owned memory.

The supplied VIA Chrome9 HCM Part II manual provides parameter types, render
target and pixel/vertex state, and command-regulator descriptions. The supplied
VX855/VX875 system manual describes MMIO/clocks. OpenChrome was checked as a
transport/engine reference; its older UniChrome via_3d.c is not a drop-in HCM
Direct3D pipeline.

Microsoft primary references checked:

The focused follow-up review and contract-by-contract findings are in
`MSFT_D3D_AUDIT.md` (historical snapshot). The current review is
`AUDIT_REPORT.md` at the bundle root. Neither report establishes DX7 compliance.

- [D3DHAL_CALLBACKS](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3dhal_callbacks)
- [D3dDrawPrimitives2](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/nc-d3dhal-lpd3dhal_drawprimitives2cb)
- [DRAWPRIMITIVES2DATA](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3dhal_drawprimitives2data)
- [GLOBALDRIVERDATA](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3dhal_globaldriverdata)
- [DP2COMMAND](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3dhal_dp2command)

Independent narrow declarations are compared with the legacy section of
[vmhal9x's pinned DDK comparison header](https://github.com/JHRobotics/vmhal9x/blob/8bc480e4d7a93bbf208526aa3b9c64079092487b/d3dhal_ddk.h).
That header is not redistributed. The unknown-parser callback notification
carries a function address, not a pointer to a stored pointer.

This branch fills the named legacy callbacks and includes a DX7 acceptance-test
client. That client has not been executed on Win98 in this review.
Textures/points/lines and broad compatibility are pending. The runtime can still reject this cap profile. Real-machine device
creation and pixel output are the next proof obligations.

- [VIDEOMEMORYINFO / Win98 VIDMEMINFO allocation alignment](https://learn.microsoft.com/en-us/windows/win32/api/ddrawint/ns-ddrawint-videomemoryinfo)

The supplied XP 6.14.10.293 DLL comparison is recorded in
XP_DRIVER_COMPARISON.md at the review bundle root, with exact hashes
and a bounded CPU-only reproduction helper. It does not establish
Win98 runtime acceptance or physical GPU equivalence.

Additional Microsoft references checked during the v10.139 follow-up:

- [DP2CLEAR](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3dhal_dp2clear)
- [DD_GETDRIVERINFODATA](https://learn.microsoft.com/en-us/windows/win32/api/ddrawint/ns-ddrawint-dd_getdriverinfodata)
- [DP2INDEXEDTRIANGLELIST](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3dhal_dp2indexedtrianglelist)

The current parser preserves the zero-count CLEAR record size, COMPUTERECTS
behavior, byte/vertex units, error offsets and runtime parser-hook convention.
The follow-up corrects validation of an unused depth field in color-only CLEAR.
Short GetDriverInfo writes still report the bytes written. Edge flags do not add
wireframe support to this solid-only profile. No full DX7 conformance is claimed.
