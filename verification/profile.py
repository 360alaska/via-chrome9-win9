#!/usr/bin/env python3
"""Check actual linked DLL advertisement against the selected build variant."""
import json
from directdraw import HAL
from validate import ROOT

def main():
    # v10.165: the VxD's ACCEL_D3D defaults to 1, so the linked HAL advertises Direct3D in either build variant.
    expected = True
    h = HAL()
    # Fixed offsets belong to the independently checked VMDAHAL_t ABI.
    global_data = h.word(h.context + 19160)
    callbacks = h.word(h.context + 19164)
    get_info = h.word(h.context + 616 + 15*4)
    if expected:
        assert global_data and callbacks
        assert get_info == h.sym['_VX_D3D_GetDriverInfo@4']
        assert h.word(callbacks + 4) == h.sym['_VX_D3D_Create@4']
        global_size = h.word(global_data)
        # v10.172: six texture formats in XP's driver's order (DDSURFACEDESC, 108 bytes each)
        assert h.word(global_data + global_size - 8) == 13  # v10.199: + L8, DXT1, DXT3, DXT5; v10.208: + V8U8, L6V5U5, X8L8V8U8
        fmts = h.word(global_data + global_size - 4)
        want = [(0x41, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000), (0x40, 32, 0xFF0000, 0xFF00, 0xFF, 0), (0x40, 16, 0xF800, 0x7E0, 0x1F, 0),
                (0x41, 16, 0x7C00, 0x3E0, 0x1F, 0x8000), (0x41, 16, 0xF00, 0xF0, 0xF, 0xF000), (0x40, 16, 0x7C00, 0x3E0, 0x1F, 0),
                (0x20000, 8, 0xFF, 0, 0, 0), (4, 0, 0, 0, 0, 0), (4, 0, 0, 0, 0, 0), (4, 0, 0, 0, 0, 0),
                (0x80000, 16, 0xFF, 0xFF00, 0, 0), (0xC0000, 16, 0x1F, 0x3E0, 0xFC00, 0), (0xC0000, 32, 0xFF, 0xFF00, 0xFF0000, 0)]
        fourcc = [0]*7 + [0x31545844, 0x33545844, 0x35545844] + [0]*3
        for k, (flags, bits, r, g, b, alpha) in enumerate(want):
            d = fmts + 108*k
            assert h.word(d) == 108 and h.word(d + 104) == 0x1000 and h.word(d + 72 + 4) == flags and h.word(d + 72 + 12) == bits
            assert [h.word(d + 72 + 16 + 4*i) for i in range(4)] == [r, g, b, alpha] and h.word(d + 72 + 8) == fourcc[k]
        caps = h.word(global_data + 16)
        required = 0x10 | 0x400 | 0x1 | 0x40
        assert caps & required == required  # MS-mandated legacy bits.
        assert caps & 0x8000  # v10.164: DRAWPRIMITIVES2EX, required by DDRAW.DLL 4.09 with CreateSurfaceEx.
        assert caps & 0x200 and not caps & (0x20 | 0x80 | 0x10000 | 0x100)  # v10.169: TEXTUREVIDEOMEMORY
        tri_tex = h.word(global_data + 4 + 100 + 32)  # hwCaps.dpcTriCaps.dwTextureCaps
        assert tri_tex & 0x7 == 0x7 and tri_tex & 0x4000 and h.word(global_data + 4 + 44 + 32) == tri_tex  # v10.456: ACCEL_D3D_MIPCAP's D3DPTEXTURECAPS_MIPMAP (lines copy the triangle caps)
        # No execute/TL video memory or HWTL claim.
        assert h.word(h.context + 19168) == 0x423000  # Win16 bridge: 3DDEVICE|ZBUFFER|TEXTURE (v10.169) | MIPMAP (v10.199)
        assert h.word(h.context + 19172) == 0x500    # v10.177: DDBD_16|DDBD_32, not the surface format enum.
    else:
        assert (global_data, callbacks, get_info) == (0, 0, 0)
        assert bytes(h.u.mem_read(h.context + 19168, 24)) == bytes(24)
    result = {'scope': __doc__ + ' Synthetic loader/services; not runtime acceptance.',
              'expected_experimental': expected, 'passed': True,
              'global_data': hex(global_data), 'callbacks': hex(callbacks),
              'GetDriverInfo': hex(get_info)}
    (ROOT/'verification/D3D_PROFILE.json').write_text(json.dumps(result, indent=2)+'\n')
    print('Linked DLL advertisement matches '+('default-on' if expected else 'default-off')+' build.')

if __name__ == '__main__':
    main()
