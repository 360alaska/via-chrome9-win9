#!/usr/bin/env python3
"""Differentially check packet copies against the rebuilt original core.so.

python tools/compare_cores.py /path/original/tests/core.so tests/core.so
Includes unused destination bytes, zero/oversized counts and both layouts.
"""
import ctypes as C
import pathlib
import random
import sys

root = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tests'))
from test_core import Packet

libs = [C.CDLL(str(pathlib.Path(p).resolve())) for p in sys.argv[1:]]
if len(libs) != 2:
    raise SystemExit('supply original and replacement core.so')
for lib in libs:
    lib.vx_d3d_packet_copy.argtypes = [C.POINTER(Packet), C.POINTER(Packet)]
    lib.vx_d3d_packet_copy.restype = None
rng = random.Random(552)
for n in range(3000):
    s = Packet.from_buffer_copy(rng.randbytes(C.sizeof(Packet)))
    s.vertex_count = rng.choice([0, 1, 191, 192, 193, 383, 384, 385, 0xffffffff])
    s.pre_on = n % 3
    s.bursts.n = rng.randrange(70)
    s.bursts.b[0] = rng.getrandbits(32)
    s.runs.n = rng.randrange(100)
    initial = rng.randbytes(C.sizeof(Packet))
    out = [Packet.from_buffer_copy(initial), Packet.from_buffer_copy(initial)]
    for lib, d in zip(libs, out):
        lib.vx_d3d_packet_copy(C.byref(d), C.byref(s))
    if bytes(out[0]) != bytes(out[1]):
        raise SystemExit('packet copy differs in case %d' % n)
print('3000 randomized packet copies: all bytes identical to original')
