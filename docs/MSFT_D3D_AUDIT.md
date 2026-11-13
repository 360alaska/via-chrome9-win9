Historical Microsoft audit snapshot from the preceding delta. For the current
v10.148 review and test results, use ../../AUDIT_REPORT.md.

> Rebase note: this is historical v10.88 analysis. Current v10.103 integration,
> fixes and validation are described in REBASE_REPORT.md and AUDIT_REPORT.md
> at the bundle root. Historical source paths and counts below are not current.

> Historical audit snapshot for 10.88.0.1. The current corrected
> source and fresh results are described in AUDIT_REPORT.md at
> the review bundle root. Full-driver evidence paths below belong
> to the prior full package; they are not all included in this delta.

# Microsoft Direct3D DDI audit - M2 carried forward onto v10.88 (10.88.0.1)

Originally reviewed 20 October 2026 against Microsoft's public legacy Direct3D DDI
documentation and a separately supplied DDK comparison header. This revision
carries forward fixes for concrete interface errors in the initial M2 package.
Rebase changes and fresh evidence are recorded in REBASE_v10_88.md; the original
audit report is retained under history/. It remains an
incomplete experimental driver; this review is not Microsoft certification.
No Windows 98 runtime or physical VX855 execution was available.

The relevant architecture is the Win9x shared DirectDraw/Direct3D HAL and VxD,
using legacy callbacks and DP2. D3D9 application interfaces, WDDM and native
DDI8/DDI9 contracts are not substitutes for that interface. Current Microsoft
pages also describe later OS behavior; those extensions are not advertised.

## Verified mismatches and corrections

| Area | Initial M2 | Audit revision |
| --- | --- | --- |
| Device capabilities [1] | Omitted EXECUTESYSTEMMEMORY and DRAWPRIMTLVERTEX; set DRAWPRIMITIVES2EX | Added the two required legacy bits. Removed the bit denoting DX7 compliance because the implementation is incomplete. Existing floating TL/system-memory bits remain. |
| Extended capabilities [2] | FVFCaps 8, MaxAnisotropy 0, a large unsupported W range | FVFCaps 0, MaxAnisotropy 1, MaxVertexW 0. The parser still skips all eight FVF coordinate sets correctly; none are used for texturing. |
| Vertex-buffer access [3] | Located/validated a nonempty VB before examining commands | Bind external vertices only at the first supported, nonempty primitive that consumes them. State-only calls ignore the unused vertex union. Both user/surface address paths reject 32-bit arithmetic overflow. |
| Invalid contexts [4] | Returned DDERR_INVALIDPARAMS | Return D3DHAL_CONTEXT_BAD in ddrval; the function returns DDHAL_DRIVER_HANDLED. Invalid buffer arguments still return INVALIDPARAMS. |
| Texture-stage validation [5] | Generic unsupported status and a one-pass count even on failure | Return the relevant texture format, operation, argument or operation-count status; success reports one pass. Invalid/lost contexts and unsupported render-state combinations do not report success. |
| Surface namespace/capacity [6] | One fixed 128-entry global array | Independent tables keyed by process and DDLocal identity. Capacity doubles from a shared pool; sparse handle values are supported. Exhaustion returns OUTOFMEMORY and preserves earlier entries. |
| Surface lifetime [6,7] | Unsupported notifications could leave stale mappings; DP2 target keys were not updated; invalidation matched offsets across locals | Handle system-memory disassociation separately from video fpVidMem zero. Successful replacement invalidates old users. DP2 stores logical target identities even when a later token fails. Destruction is scoped to resource identity and local ownership. |
| Attached resources [6] | Only the notified root was registered | Traverse supported video color/depth/flip attachment graphs once, with cycle checks and a bounded work list. Validate/reserve the group before registration. Reject unsupported video resources and swapped color/depth roles. |

The table pool supports at most 32 DDLocals and 4096 surface records in total;
a single table grows through 128, 256, 512, 1024, 2048 and 4096 entries as pool
availability permits. The attachment traversal accepts up to 64 unique video
surfaces and 64 links per node. These are explicit experimental resource
limits, with bounded failure, rather than unlimited allocation claims.
The pool uses shared DLL storage so it does not retain process-private heap
pointers. DestroyDDLocal returns its blocks for reuse and never frees the
application's DirectDraw surfaces. Numeric DDLocal identities are retained
for lookup; the driver does not later dereference a cached DDLocal pointer.

## Existing behavior confirmed against the reviewed contracts

- ContextCreate returns a nonzero handle and keeps state independent. Its
  existing OUTOFCONTEXTS value belongs in ddrval with HANDLED; it was correct
  and was retained. [4,8]
- DP2 command lengths/offsets use bytes; dwVertexLength uses vertices. The
  stride is captured before writing the overlapping HRESULT union. Errors
  identify the first unhandled command relative to the command allocation.
  Execute-buffer state feedback is conditional on the execute-buffer flag. [3]
- Unknown-command notification retains the function address passed by the
  runtime, and unrecognized tokens go through the parser hook when supplied.
  GetDriverInfo limits writes to the expected-size buffer. [3,9]
- CALLBACKS3 leaves Clear2/reserved members NULL and matches each advertised
  callback flag to a populated pointer. This follows the documented DX7+
  table; removing the compliance bit does not establish compatibility with
  every older runtime path. [10]
- A zero-count DP2 clear advances over its reserved RECT without reading the
  undefined rectangle. Normalized depth is converted to the supported D32
  mask. COMPUTERECTS handling is only an optional later-runtime extension. [11]

## Evidence in this package

Both advertisement variants were rebuilt from the audited sources with Open
Watcom. The source hashes and binary hashes are recorded in each BUILD.json.

| Check | Result | Evidence |
| --- | --- | --- |
| Portable parser/emitter/immediate contracts | 43 passed | D3D_CORE.json |
| Existing compiled driver/GDI/DirectDraw checks | 1531 passed | RESULTS.json |
| Linked Direct3D callbacks with synthetic services | 44 passed | D3D_CALLBACKS.json |
| Linked native VxD checks | 19 passed | NATIVE3D.json |
| DDK/SDK comparison | 17 structure sizes, 11 DP2 offsets, 11 status values matched | D3D_ABI.json |
| Deterministic sanitizer stress | 100,000 inputs, no sanitizer errors | no-d3d/M2_TEST_RUN.json |
| Default-off / opt-in advertisement | Both matched their build flag | D3D_PROFILE.json in each variant |

All evidence paths above are relative to `verification/`. The tests execute
portable C and compiled x86 instructions; Windows services, runtime-issued
callbacks and MMIO are synthetic. They verify the exercised contracts and
control flow. They cannot prove GPU rasterization or runtime acceptance.

## Remaining blockers and unverified behavior

1. Real Win98 enumeration, HAL device creation and DP2/legacy callback routing
   have not run. The revised caps may change runtime negotiation. Start with
   VXDX7.EXE query, then --triangle, then --draw; retain the HRESULT and logs.
2. Texturing, texture formats/cache transfers, perspective UV interpolation,
   points, lines and immediate primitive tokens remain unimplemented. There
   is no full DirectX 6/7 DDI or game-compatibility claim.
3. State blocks remain a limited eight-block subset. Fog, stencil, alpha
   testing, most blend pairs and broad rendering-state coverage are missing.
   Unsupported draw states are rejected instead of silently approximated.
4. Color/depth handle registration and flip graph traversal do not establish
   working Direct3D flip presentation. Mipmaps, cube maps and stereo remain
   unsupported. Real attachment/notification ordering still needs testing.
5. Integer/fractional edge rules, top-left coverage, shading, RGB565 rounding,
   depth accuracy and blending require real pixel comparison. A plausible
   command trace cannot establish those results. Legacy Clear color packing
   also needs a legacy-runtime check, separately from the DP2 clear contract.
6. Mode-generation checks and fault containment exist, but complete device
   recreation, recovery, multi-process behavior and alt-tab/reset sequencing
   are unqualified on hardware.

## Microsoft references

1. [D3DDEVICEDESC_V1](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3ddevicedesc_v1)
2. [D3DHAL_D3DEXTENDEDCAPS](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3dhal_d3dextendedcaps)
3. [D3dDrawPrimitives2](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/nc-d3dhal-lpd3dhal_drawprimitives2cb) and [DRAWPRIMITIVES2DATA](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3dhal_drawprimitives2data)
4. [Return codes for Direct3D driver callbacks](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/return-codes-for-direct3d-driver-callbacks)
5. [D3dValidateTextureStageState](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/nc-d3dhal-lpd3dhal_validatetexturestagestatecb)
6. [D3dCreateSurfaceEx](https://learn.microsoft.com/en-us/windows/win32/api/ddrawint/nc-ddrawint-pdd_createsurfaceex)
7. [D3dDestroyDDLocal](https://learn.microsoft.com/en-us/windows/win32/api/ddrawint/nc-ddrawint-pdd_destroyddlocal)
8. [D3dContextCreate](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/nc-d3dhal-lpd3dhal_contextcreatecb)
9. [DD_GETDRIVERINFODATA](https://learn.microsoft.com/en-us/windows/win32/api/ddrawint/ns-ddrawint-dd_getdriverinfodata)
10. [D3DHAL_CALLBACKS3](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3dhal_callbacks3)
11. [D3DHAL_DP2CLEAR](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dhal/ns-d3dhal-_d3dhal_dp2clear)

The independent DDK comparison header's pinned source is recorded in
DDI_REFERENCES.md. It is not shipped as a Microsoft certification artifact.
