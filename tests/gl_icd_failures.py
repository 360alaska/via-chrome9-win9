#!/usr/bin/env python3
"""Linked VXGL.DLL fault injection. DirectDraw COM and the VxD are modeled."""
import re
import struct
import sys
from pathlib import Path

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP


def pack(*words):
    return struct.pack('<%dI' % len(words), *words)


def word(blob, at=0):
    return struct.unpack_from('<I', blob, at)[0]


class ICD:
    def __init__(self, root):
        b = (root / 'driver/VXGL.DLL').read_bytes()
        header = word(b, 60)
        opt = header + 24
        base = word(b, opt + 28)
        self.u = Uc(UC_ARCH_X86, UC_MODE_32)
        self.u.mem_map(base, (word(b, opt + 56) + 4095) & ~4095)
        self.u.mem_write(base, b[:word(b, opt + 60)])
        sections = opt + struct.unpack_from('<H', b, header + 20)[0]
        for i in range(struct.unpack_from('<H', b, header + 6)[0]):
            v, n, off = struct.unpack_from('<3I', b, sections + i * 40 + 12)
            if n:
                self.u.mem_write(base + v, b[off:off+n])
        self.sym = {name: int(addr, 16) for addr, name in re.findall(
            r'^([0-9a-fA-F]{8})[s+*]* +([^\s]+)', (root / 'build/VXGL.map').read_text(), re.M)}
        self.u.mem_map(0x10000, 0x400000)
        for n in ('vxgl_engine_lock', 'vxgl_engine_unlock'):
            self.u.mem_write(self.sym[n], b'\xc3')

    def read(self, address):
        return word(self.u.mem_read(address, 4))

    def write(self, address, *values):
        self.u.mem_write(address, pack(*values))

    def arg(self, i):
        return self.read(self.u.reg_read(UC_X86_REG_ESP) + 4 * (i + 1))

    def hook(self, addr, fn, stdcall=0):
        if isinstance(addr, str):
            addr = self.sym[addr]

        def intercept(u, _, _size, _data):
            sp = u.reg_read(UC_X86_REG_ESP)
            u.reg_write(UC_X86_REG_EAX, fn())
            u.reg_write(UC_X86_REG_ESP, sp + 4 * (1 + stdcall))
            u.reg_write(UC_X86_REG_EIP, self.read(sp))

        self.u.hook_add(UC_HOOK_CODE, intercept, None, addr, addr)

    def call(self, fn, *args):
        sp, end = 0x200000, 0x10000
        self.write(sp, end, *args)
        self.u.reg_write(UC_X86_REG_ESP, sp)
        self.u.emu_start(self.sym[fn], end, count=1_000_000)
        assert self.u.reg_read(UC_X86_REG_EIP) == end, fn
        assert self.u.reg_read(UC_X86_REG_ESP) == sp + 4, fn
        return self.u.reg_read(UC_X86_REG_EAX)


def batch_case(root, max_batch, fail):
    d = ICD(root)
    ctx, draw, clear = 0x220000, 0x250000, 0x260000
    d.write(draw, 0x7e7c, 0x10000, 0, 1, 0, 0x10000)
    d.write(clear, 0x7e7c, 0x10000, 0, 2, 0, 0x10000)
    d.write(d.sym['vxgl_batch_max'], max_batch)
    calls = []

    def ioctl():
        code, output = d.arg(0), d.arg(3)
        calls.append(code)
        if code == 0x85500024:
            d.write(output, 4 if fail else 0, 0 if fail else d.arg(2)//0x7e7c)
        else:
            assert code == 0x85500021, hex(code)
            d.write(output, 0)
        return 1

    d.hook('vxd_call', ioctl)
    assert d.call('vxgl_submit', ctx, draw) == 0 and not calls
    result = d.call('vxgl_submit', ctx, draw if max_batch == 2 else clear)
    expected = [0x85500024] if fail or max_batch == 2 else [0x85500024, 0x85500021]
    assert (result, calls) == (4 if fail else 0, expected), (max_batch, fail, result, [hex(c) for c in calls])
    assert d.call('vxgl_submit', ctx, clear) == 0, 'the context must consume a reported batch loss once'


def seed_case(root, fail):
    d = ICD(root)
    fb, dd, dd_vt, surface, surface_vt, pixels = (0x220000 + 0x1000*i for i in range(6))
    # vxgl_fb layout from the Watcom ICD (Mesa 7.0.3, 32-bit packing).
    width, back, fmt, cpu, color_bits, pf = (0x3b8, 0x3c0, 0x3d8, 0x3f0, 0x45c, 0x478)
    create, blt, lock, unlock, release = (0x230000 + 0x100*i for i in range(5))
    d.write(fb + color_bits, 32)
    d.write(fb + pf, 3)  # double-buffered colour only
    d.write(d.sym['vxgl_dd'], dd)
    d.write(dd, dd_vt)
    d.write(dd_vt + 0x18, create)
    d.write(surface, surface_vt)
    for off, fn in ((0x08, release), (0x14, blt), (0x64, lock), (0x80, unlock)):
        d.write(surface_vt + off, fn)
    d.u.mem_write(pixels, b'\xA5' * (32 * 4))
    calls = []

    def surface_create():
        calls.append('create')
        d.write(d.arg(2), surface)
        return 0

    def surface_where():
        assert d.arg(0) == surface
        d.write(d.arg(1), 0x10000, 32, 8, 4)
        d.write(d.arg(2), pixels)
        return 1

    def surface_lock():
        calls.append('lock')
        if fail:
            return 0x887601c2
        sd = d.arg(2)
        d.write(sd + 8, 4, 8, 32)
        d.write(sd + 36, pixels)
        return 0

    def operation(name, result):
        calls.append(name)
        return result

    d.hook('dd_init', lambda: 1)
    d.hook('surface_where', surface_where)
    d.hook(create, surface_create, 4)
    d.hook(blt, lambda: operation('blt', 0x80004005), 6)
    d.hook(lock, surface_lock, 5)
    d.hook(unlock, lambda: operation('unlock', 0), 2)
    d.hook(release, lambda: operation('release', 0), 1)
    rc = d.call('fb_make_surfaces', fb, 8, 4)
    if fail:
        assert (rc, calls) == (0, ['create', 'blt', 'lock', 'release']), (rc, calls)
        assert all(d.read(fb + k) == 0 for k in (width, back, fmt, cpu))
        assert bytes(d.u.mem_read(pixels, 128)) == b'\xA5' * 128
    else:
        assert (rc, calls) == (1, ['create', 'blt', 'lock', 'unlock']), (rc, calls)
        assert d.read(fb + width) == 8 and d.read(fb + back) == surface
        assert bytes(d.u.mem_read(pixels, 128)) == bytes(128)


def import_slot(d, root, wanted):
    """The IAT slot of an imported function (the PE's import directory)."""
    b = (root / 'driver/VXGL.DLL').read_bytes()
    header = word(b, 60)
    opt = header + 24
    base = word(b, opt + 28)
    imp = word(b, opt + 104)
    while True:
        oft, _, _, name, ft = (d.read(base + imp + 4*k) for k in range(5))
        if not name:
            return None
        k = 0
        while True:
            hint = d.read(base + (oft or ft) + 4*k)
            if not hint:
                break
            if not hint & 0x80000000:
                n = bytes(d.u.mem_read(base + hint + 2, 40)).split(b'\0')[0].decode()
                if n == wanted:
                    return base + ft + 4*k
            k += 1
        imp += 20


def wgl_case(root, kind):
    """.575 (peer review of .572, finding 13): wglChoosePixelFormatARB with an attribute that is not one fails with
    ERROR_INVALID_DATA (it answered TRUE with formats); a known attribute no format has matches nothing; a plain list finds formats."""
    d = ICD(root)
    ia, fmts, num, stub = 0x220000, 0x221000, 0x222000, 0x230000
    errors = []
    slot = import_slot(d, root, 'SetLastError')
    assert slot, 'SetLastError import'
    d.write(slot, stub)
    d.hook(stub, lambda: errors.append(d.arg(0)) or 0, 1)
    d.hook('vxgl_screen_bpp', lambda: 32)
    d.write(d.sym['desktop_bpp'], 32)
    lists = {'invalid': [0x2010, 1, 0x7ABC, 1, 0], 'stereo': [0x2010, 1, 0x2012, 1, 0], 'srgb': [0x2010, 1, 0x20A9, 1, 0], 'plain': [0x2010, 1, 0x2011, 1, 0x2022, 16, 0]}
    d.write(ia, *lists[kind])
    d.write(num, 0x55)
    sp, end = 0x200000, 0x10000  # __stdcall (24 bytes of arguments): the fixture's call checks a cdecl return
    d.write(sp, end, 0, ia, 0, 8, fmts, num)
    d.u.reg_write(UC_X86_REG_ESP, sp)
    d.u.emu_start(d.sym['_vx_wgl_choose_pf@24'], end, count=1_000_000)
    assert d.u.reg_read(UC_X86_REG_EIP) == end and d.u.reg_read(UC_X86_REG_ESP) == sp + 28
    rc = d.u.reg_read(UC_X86_REG_EAX)
    got = d.read(num)
    if kind == 'invalid':
        assert rc == 0 and errors == [13], (rc, errors, got)
    elif kind in ('stereo', 'srgb'):
        assert rc == 1 and got == 0 and not errors, (rc, got, errors)
    else:
        assert rc == 1 and got >= 1 and not errors, (rc, got, errors)


def validate_case(root, have_dd7):
    """.588: DrvValidateVersion says no on a DirectDraw without DirectDrawCreateEx (DirectX older than 7), so opengl32 draws with
    Microsoft's renderer; with the export it says yes as before. The VxD's answers are the ready ones in both runs."""
    import re
    d = ICD(root)
    build = int(re.search(r"-DDRV_VER_BUILD=(\d+)", (root / 'build.py').read_text()).group(1))
    stubs = {'LoadLibraryA': 0x230000, 'GetProcAddress': 0x230100, 'FreeLibrary': 0x230200}
    asked = []
    for name, at in stubs.items():
        slot = import_slot(d, root, name)
        assert slot, name + ' import'
        d.write(slot, at)
    d.hook(stubs['LoadLibraryA'], lambda: 0x7000, 1)
    def gpa():
        n = bytes(d.u.mem_read(d.arg(1), 40)).split(b'\0')[0].decode()
        asked.append(n)
        return 0x7100 if (n == 'DirectDrawCreateEx' and have_dd7) else 0
    d.hook(stubs['GetProcAddress'], gpa, 2)
    d.hook(stubs['FreeLibrary'], lambda: 1, 1)
    d.hook('vxgl_log', lambda: 0)
    d.hook('vxgl_vxd_info', lambda: 1)
    def texdbg():
        d.write(d.arg(1), 0x75, build, 0, 0, 0, 0, 0, 0)
        return 1
    d.hook('texdbg', texdbg)
    d.write(d.sym['icd_enabled'], 1)
    d.write(d.sym['vxgl_gone'], 0)
    sp, end = 0x200000, 0x10000
    d.write(sp, end, 2)
    d.u.reg_write(UC_X86_REG_ESP, sp)
    d.u.emu_start(d.sym['_DrvValidateVersion@4'], end, count=1_000_000)
    assert d.u.reg_read(UC_X86_REG_EIP) == end and d.u.reg_read(UC_X86_REG_ESP) == sp + 8
    rc = d.u.reg_read(UC_X86_REG_EAX)
    assert asked == ['DirectDrawCreateEx'] and rc == (1 if have_dd7 else 0), (rc, asked)


if __name__ == '__main__':
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
    failed = 0
    for fn, variants in ((batch_case, [(m, f) for m in (2, 3) for f in (False, True)]),
                         (seed_case, [(False,), (True,)]),
                         (wgl_case, [('invalid',), ('stereo',), ('srgb',), ('plain',)]),
                         (validate_case, [(True,), (False,)])):
        for args in variants:
            try:
                fn(root, *args)
                print('PASS', fn.__name__, args)
            except AssertionError as e:
                failed += 1
                print('FAIL', fn.__name__, args, e)
    sys.exit(bool(failed))
