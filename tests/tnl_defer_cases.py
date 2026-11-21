#!/usr/bin/env python3
"""tnl_defer_cases.py (.718): the T&L's deferred side planes. When the scratch is short tn_draw clips near and far only (mask 07) and leaves
the sides to the per-primitive path; .716 and before held a new near-plane vertex beyond a side at the screen's edge with the off-screen
point's depth, rhw, coordinates and alpha (an outside review reproduced it: a corner 32 pixels off). This runs tnl_guard_cases.py's scenes
(guard band against none, rasterised and compared) on a core built with a small scratch (TN_MAX given at build), so that path is taken,
and fails unless it was taken and every scene matched.
usage: tnl_defer_cases.py core_small.so [scenes] [seed]"""
import sys,os,ctypes as C
HERE=os.path.dirname(os.path.abspath(__file__))
src=open(os.path.join(HERE,'tnl_guard_cases.py')).read();i=src.rindex('sys.exit(main())');src=src[:i]+'RC=main()'+src[i+16:]
ns={'__file__':os.path.join(HERE,'tnl_guard_cases.py'),'__name__':'guard_mod'};exec(compile(src,'tnl_guard_cases.py','exec'),ns)
d=(C.c_uint32*4).in_dll(ns['lib'],'vx_d3d_tn_dfr')
print('deferred side planes: %d draws (triangles), %d (lines); new vertices beyond an x edge %d, a y edge %d'%tuple(d))
if not d[0]:print('FAIL: the small scratch never deferred the side planes (the test did not reach the path)');sys.exit(1)
if not (d[2] or d[3]):print('FAIL: no new vertex beyond an edge in the deferred path (the case .716 got wrong was not made)');sys.exit(1)
sys.exit(ns['RC'])
