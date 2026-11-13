#!/usr/bin/env python3
"""v10.239: the OpenGL ICD's Mesa side on the host. Mesa 7.0.3 (gl/mesa) and our back end (gl/vxgl_mesa.c, vxgl_state.c, vxgl_tris.c)
with the core are built with gcc and ASan/UBSan against gl/host's Windows shims; gl/host/vxgl_host.c draws VXGL.EXE's scenes
(src/vx_gl_scenes.h) through the engine path into a model of the engine (packets checked by the VxD's validation, then rasterized by
Direct3D's rules) and through swrast, and compares them. Colour 16/depth 16 and colour 32/depth 24/stencil 8. Mesa's objects are kept in
build/hostgl and compiled again when their source or any Mesa header is newer."""
import os,re,subprocess,sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
M=ROOT/'gl'/'mesa'/'src'/'mesa'
OUT=ROOT/'build'/'hostgl'
INC=['-I'+str(x) for x in [ROOT/'gl'/'host',ROOT/'gl',M,M/'main',M/'glapi',M/'math',M/'tnl',M/'shader',M/'shader'/'grammar',M/'shader'/'slang',ROOT/'gl'/'mesa'/'include',ROOT/'src']]
SAN=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
# v10.322: Mesa itself built with AddressSanitizer too (its reads of freed memory are caught, not only ours: a GLSL program freed after its
# shaders at context deletion); objects in their own directory
MSAN=['-fsanitize=address','-fno-omit-frame-pointer']
def cc(src,obj,extra):
 r=subprocess.run(['gcc','-c','-O1','-g','-w','-std=gnu99']+extra+INC+[str(src),'-o',str(obj)],capture_output=True,text=True)
 if r.returncode:raise SystemExit('compile %s:\n%s'%(src,r.stderr[-3000:]))
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 txt=(M/'sources').read_text();srcs=[]
 for var in ['MAIN_SOURCES','GLAPI_SOURCES','MATH_SOURCES','SWRAST_SOURCES','SWRAST_SETUP_SOURCES','TNL_SOURCES','VBO_SOURCES','SHADER_SOURCES','SLANG_SOURCES']:
  m=re.search(var+r'\s*=\s*\\\n((?:.*\\\n)*.*)',txt);srcs+=re.findall(r'(\S+\.c)',m.group(1))
 srcs+=['drivers/common/driverfuncs.c']
 hdr=max([f.stat().st_mtime for d in [ROOT/'gl'/'mesa'/'include',M] for f in d.rglob('*.h')]+[Path(__file__).stat().st_mtime])
 jobs=[];objs=[]
 for f in srcs:
  o=OUT/('a_'+f.replace('/','_')[:-2]+'.o');objs.append(o)
  if not(o.exists() and o.stat().st_mtime>max((M/f).stat().st_mtime,hdr)):jobs.append((M/f,o,MSAN))
 for f in ['gl/vxgl_mesa.c','gl/vxgl_state.c','gl/vxgl_tris.c','gl/vxgl_tex.c','gl/vxgl_batch.c','gl/vxgl_fp.c','gl/vxgl_fbo.c','gl/vxgl_ssepass.c','gl/host/vxgl_host.c','src/vx_d3d_core.c','src/vx_native3d_hw.c']:
  o=OUT/('v_'+Path(f).stem+'.o');objs.append(o);jobs.append((ROOT/f,o,SAN))
 o=OUT/'v_vx_sten_test.o';objs.append(o);jobs.append((ROOT/'src'/'vx_sten_test.c',o,SAN+['-DVXSTEN_HOST'])) # v10.264: VXSTEN's cases
 o=OUT/'v_vx_tex_test.o';objs.append(o);jobs.append((ROOT/'src'/'vx_tex_test.c',o,SAN+['-DVXTEX_HOST'])) # v10.266: VXTEX's cases
 o=OUT/'v_glreplay.o';objs.append(o);jobs.append((ROOT/'src'/'glreplay.c',o,SAN+['-DGR_HOST','-Wno-cast-function-type'])) # v10.342: GLREPLAY on the host model
 o=OUT/'v_q2menu.o';objs.append(o);jobs.append((ROOT/'src'/'q2menu.c',o,SAN+['-DQ2MENU_HOST'])) # v10.342: Quake 2 3.14's menu
 with ThreadPoolExecutor(os.cpu_count() or 4) as ex:list(ex.map(lambda j:cc(*j),jobs))
 exe=OUT/'vxglhost'
 r=subprocess.run(['gcc']+SAN+[str(o) for o in objs]+['-lm','-o',str(exe)],capture_output=True,text=True)
 if r.returncode:raise SystemExit('link:\n'+r.stderr[-3000:])
 fails=0
 for cb,zb,ixd in [(16,16,'1'),(32,24,'1'),(32,24,'0'),(16,16,'glix'),(32,24,'glix')]: # .573: the ICD's indexed packets on (its default), and once off (the plain form's paths); glix: VXGLIX's own sequence (the board tool's scenes and order: v1.0 went to the board without it and failed for the tool's faults)
  env=os.environ.copy();env.update({'VXGL_CBITS':str(cb),'VXGL_ZBITS':str(zb),'VXGL_INDEXED':'1' if ixd=='glix' else ixd,'UBSAN_OPTIONS':'print_stacktrace=1'})
  if ixd=='glix':env['VXGL_GLIX']='1'
  r=subprocess.run([str(exe)],cwd=ROOT,env=env,capture_output=True,text=True)
  print(r.stdout,end='')
  err=[l for l in r.stderr.splitlines() if l.strip()] # anything from the sanitizers fails the test
  if err:print('\n'.join(err[-60:]))
  if r.returncode or err:fails+=1
 sys.exit(1 if fails else 0)
if __name__=='__main__':main()
