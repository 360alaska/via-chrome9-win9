#!/usr/bin/env python3
"""Regressions for float overflow during clipping and atomic ZRANGE updates."""
import ctypes as C
import struct

from test_core import ALLF, Contract, INVALID, U, cmd, lib, pack, rs


def context():
    test = Contract("test_tnl")
    test.setUp()
    for name in ("vx_d3d_tnl", "vx_d3d_indexed", "vx_d3d_ixpass",
                 "vx_d3d_prepack", "vx_d3d_v8"):
        U.in_dll(lib, name).value = 1
    U.in_dll(lib, "vx_d3d_feat").value = ALLF
    test._stages([(7, 4, 2, 0, 2, 2, 0, 0)])
    return test


def vertices(points):
    return b"".join(
        struct.pack("<3fI2f", x, y, .5, 0xffffffff, u, .5)
        for x, y, u in points
    )


def draw(test, data, setup=b""):
    test.calls.clear()
    command = rs((137, 0), (28, 0), (22, 1)) + setup + test._itl([(0, 1, 2)])
    assert test.run_dp2(command, data, stride=24, fvf=0x142) == (0, 0)
    assert len(test.calls) == 1
    packet = test.calls[0]
    assert packet.pre_on == 2
    error = lib.vx3dn_validate(C.byref(packet), C.byref(test.info))
    assert error == 0, (error, U.in_dll(lib, "vx3dn_bad_line").value)
    stride = 8 if packet.bursts.b[0] & 0x04000000 else 10
    return [tuple(packet.pre_w[i:i+stride]) for i in range(0, packet.vertex_count*stride, stride)]


def test_clipped_uv():
    largest = struct.unpack("<f", pack(0x7f7fffff))[0]
    words = draw(context(), vertices([(-.5, -.5, -largest), (.5, .5, -largest),
                                      (1.5, -.5, largest)]))
    assert sorted(w[6] for w in words) == [0, 0x7effffff, 0xff7fffff, 0xff7fffff]


def test_clip_distance():
    data = vertices([(-.5, -.5, .1), (.5, .5, .3), (1.5, .25, .5)])

    def project(s):
        matrix = struct.pack("<16f", s, 0, 0, 0, 0, s, 0, 0,
                             0, 0, s, 0, 0, 0, 0, s)
        return draw(context(), data, cmd(36, 1, pack(3) + matrix))

    ordinary = project(1.)
    scaled = project(2.**127)
    # Homogeneous scaling changes only RHW. The right-plane crossing's
    # distance span overflows binary32 in the scaled case.
    assert [w[:3] + w[4:] for w in ordinary] == [w[:3] + w[4:] for w in scaled]
    assert all(w[3] == 0x00400000 for w in scaled)


def test_invalid_zrange():
    test = context()
    data = vertices([(-.5, -.5, 0), (.5, -.5, .5), (0., .5, 1.)])

    def zrange(*values):
        return cmd(32, len(values) // 2, struct.pack("<%df" % len(values), *values))

    valid = draw(test, data, zrange(.125, .625))
    assert all(w[2] == 0x3ec00000 for w in valid)
    for invalid in (zrange(float("nan"), 1.), zrange(float("inf"), 1., 0., 1.)):
        test.calls.clear()
        assert test.run_dp2(invalid, data, stride=24, fvf=0x142) == (INVALID, 0)
        assert not test.calls
        assert draw(test, data) == valid
    assert all(w[2] == 0x3f000000 for w in draw(test, data, zrange(0., 1.)))


if __name__ == "__main__":
    for test in (test_clipped_uv, test_clip_distance, test_invalid_zrange):
        test()
        print(f"{test.__name__} PASS")
