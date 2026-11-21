#!/usr/bin/env python3
"""tnl_xbuild_cases.py (.707): the driver's T&L group path of one build against another's, word for word.

.707 rearranged the lighting kernels (the normals and eye-space position made once a group and kept, the point lights read lbase,
far point lights skipped before their reciprocal square root, tnk_light compiled by clang): the same operations in the same order, so
every diffuse and specular word must be the same bits as the build before. The scenes are tnl_light_cases.py's (point, spot, directional
lights, specular lighting with and without LOCALVIEWER, the colour sources), each also with vertex fog (exp, exp2, linear) and range fog.
usage: tnl_xbuild_cases.py <old core.so> <new core.so> [scenes] [seed]    (exit 1 when any word differs)"""
import sys,os,struct,ctypes as C
HERE=os.path.dirname(os.path.abspath(__file__));sys.path.insert(0,HERE)
old_so,new_so=os.path.abspath(sys.argv[1]),os.path.abspath(sys.argv[2])
SCENES=int(sys.argv[3]) if len(sys.argv)>3 else 200
SEED=int(sys.argv[4]) if len(sys.argv)>4 else 707
src=open(os.path.join(HERE,'tnl_light_cases.py')).read()
src=src[:src.rindex('sys.exit(main())')]
a="(143,1),(28,0)]"
assert a in src
src=src.replace(a,"(143,1),(28,1 if sc.get('fog') else 0)]+([(35,0),(140,sc['fog']),(36,f2u(.3)),(37,f2u(2.5)),(38,f2u(.7)),(48,sc.get('rfog',0))] if sc.get('fog') else [])")
sys.argv=[sys.argv[0],'-',str(SCENES),str(SEED)]
ns={'__name__':'tnl_light_cases_x','__file__':os.path.join(HERE,'tnl_light_cases.py')};exec(compile(src,'tnl_light_cases.py','exec'),ns)
T=ns['T'];libs=[C.CDLL(old_so),C.CDLL(new_so)]
def use(i):T.lib=libs[i];ns['lib']=libs[i]
def main():
 bad=0;words=0;kinds=['spot','spec','src','mix','point']
 for s in range(SCENES):
  kind=kinds[s%len(kinds)];sc=ns['build'](kind);sc['fog']=(0,1,2,3)[(s//5)%4];sc['rfog']=(s//20)&1
  ns['LIDX']=[0,1,2] if s%2==0 else [17,1000,0x7FFFFFFF]
  got=[]
  for i in (0,1):use(i);got.append(ns['run'](sc,1))
  (ra,ga),(rb,gb)=got
  if ra!=rb or (ga is None)!=(gb is None):bad+=1;print('FAIL scene',s,kind,'rc',ra,rb);continue
  if ga is None:continue
  if ga.keys()!=gb.keys():bad+=1;print('FAIL scene',s,kind,'vertex sets differ');continue
  for k in ga:
   words+=2
   if ga[k]!=gb[k]:bad+=1;print('FAIL scene',s,kind,'fog',sc['fog'],'rfog',sc['rfog'],'vertex',k,'old %08X %08X new %08X %08X'%(*ga[k],*gb[k]));break
 print('scenes',SCENES,'words compared',words,'failures',bad);return 1 if bad else 0
sys.exit(main())
