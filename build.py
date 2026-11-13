#!/usr/bin/env python3
"""Build real Win98 NE/LE binaries with a locally installed Open Watcom."""
import argparse, hashlib, struct, json, os, pathlib, shutil, subprocess, sys

ROOT=pathlib.Path(__file__).resolve().parent
PINNED_URL='https://github.com/open-watcom/open-watcom-v2/releases/download/2026-10-01-Build/open-watcom-2_0-c-linux-x64'
PINNED_SHA256='a961f3e02ce27bcd88428345dc57483457a623b6e718b6cee52c48d24ab8065f'

def main():
 p=argparse.ArgumentParser()
 p.add_argument('--watcom',default=os.environ.get('WATCOM'))
 a=p.parse_args()
 if not a.watcom: p.error('Set WATCOM or use --watcom /path/to/watcom')
 wc=pathlib.Path(a.watcom).resolve()
 env=os.environ.copy(); env['WATCOM']=str(wc)
 env['INCLUDE']=str(wc/'h')+os.pathsep+str(wc/'h'/'win')
 env['PATH']=os.pathsep.join([str(wc/'binl64'),str(wc/'binl'),str(wc/'binnt64'),str(wc/'binnt'),env['PATH']])
 b=ROOT/'build'; b.mkdir(exist_ok=True)
 (ROOT/'driver').mkdir(exist_ok=True)
 src=ROOT/'src'
 (ROOT/'driver').mkdir(exist_ok=True)
 shutil.copyfile(src/'res'/'vx85598.inf',ROOT/'driver'/'VX85598.INF')
 (ROOT/'verification').mkdir(exist_ok=True)
 log=(ROOT/'verification'/'BUILD.LOG').open('w')
 def run(args):
  log.write(' '.join(str(x) for x in args)+'\n'); log.flush()
  r=subprocess.run([str(x) for x in args],cwd=b,env=env,stdout=log,stderr=subprocess.STDOUT)
  if not r.returncode and str(args[0]) in ('wcc','wcc386','wasm'):
   for arg in args:
    if str(arg).startswith('-fo='):
     output=pathlib.Path(str(arg)[4:]);output=output if output.is_absolute() else b/output
     if not output.exists() or not output.stat().st_size:
      log.write('Error: compiler returned success without a nonempty object: '+str(output)+'\n');log.flush()
      print('Compiler produced no object:',output);sys.exit(1)
  if r.returncode:
   log.flush(); lines=(ROOT/'verification'/'BUILD.LOG').read_text().splitlines()
   print('\n'.join(x for x in lines if 'Error!' in x or 'Warning!' in x))
   print('\n'.join(lines[-4:]));sys.exit(r.returncode)
 inc=['-I'+str(wc/'h'/'win'),'-I'+str(wc/'h'),'-I'+str(src),'-I'+str(src/'ddk'),'-I'+str(src/'vmware')]
 flags=['-q','-wx','-s','-zls','-wcd=303','-DDRV_VER_BUILD=597','-DDBGPRINT','-DVX85598_BUILD']
 c16=[x for x in flags if x!='-DDBGPRINT']+['-zu','-zW','-4','-fp3','-DCOM0','-DHWBLT']+inc
 c32=flags+['-mf','-DVXD32','-fpi87','-ei','-oeatxhn','-4s','-fp3','-DVX_MEMLOG','-DCOM0']+inc
 objs16=['dbgprint','dibcall','dddrv','drvlib','enable','init','control','palette','scrsw_vesa','pm16_calls_vesa','modes_vesa']
 objs32=['vxd_main_vesa','vx85598','vx855_3d','vx855_3d_core','vx855_2d','vx855_flip','vx_native3d_hw','vx_native3d_vxd','vx855_edid','dbgprint32','vxd_fbhda','vxd_lib','vxd_vesa','vxd_gtf','vxd_vdd_vesa','vxd_mouse','vxd_mtrr','vxd_wram','vxd_async','vxd_terror','vx_cache']
 for name in objs16: run(['wcc']+c16+['-fo='+name+'.obj',src/(name+'.c')])
 for name in ['dibthunk','sswhook']: run(['wasm','-q','-dHWBLT','-fo='+name+'.obj',src/(name+'.asm')])
 for name in objs32: run(['wcc386']+c32+['-fo='+name+'.obj',src/(name+'.c')])
 for name in ['colortab','config','fonts','fonts120']:
  run(['wcc','-q']+inc+['-fo='+name+'.obj',src/'res'/(name+'.c')])
  run(['wlink','op','quiet','disable','1014,1023','name',name+'.bin','sys','dos','output','raw','file',name+'.obj'])
 run(['wrc','-q','-r','-bt=windows','-fo=VX85598.res','-I'+str(src/'res')]+inc+['-DDRV_VER_BUILD=597',src/'res'/'vesamini.rc'])
 run(['wlib','-b','-q','-n','-fo','-ii','@'+str(src/'ddk'/'dibeng.lbc'),'dibeng.lib'])
 # Keep the upstream NE exports/ordinal contracts exactly as supplied.
 mk=(src/'makefile').read_text()
 base=mk.index('vesamini.drv :')
 start=mk.index('system windows dll initglobal',base)
 end=mk.index('\n<<',start)
 lnk=mk[start:end].replace('name vesamini.drv','name VX85598.DRV').replace('map=vesamini.map','map=VX85598_DRV.map')
 lnk='file dbgprint.obj\n'+lnk
 (b/'drv.lnk').write_text(lnk+'\n')
 run(['wlink','op','quiet,start=DriverInit_','disable','2055','@drv.lnk'])
 run(['wrc','-q','VX85598.res','VX85598.DRV'])
 fix=b/('fixlink.exe' if os.name=='nt' else 'fixlink')
 if os.name=='nt': run(['wcl386','-q','-fe='+str(fix),src/'fixlink'/'fixlink.c'])
 else: run(['gcc','-w',str(src/'fixlink'/'fixlink.c'),'-o',str(fix)])
 run([fix,'-40','VX85598.DRV'])
 lnk='system win_vxd dynamic\noption map=VX85598_VXD.map\noption statics\noption nodefaultlibs\nname VX85598.VXD\n'
 lnk+=''.join('file '+n+'.obj\n' for n in objs32)
 lnk+=''.join("segment '"+n+"' PRELOAD NONDISCARDABLE\n" for n in ['_TEXT','_DATA','CONST','CONST2','_BSS'])
 lnk+='export VXD_DDB.1\n'
 (b/'vxd.lnk').write_text(lnk)
 run(['wlink','op','quiet','@vxd.lnk'])
 run([fix,'-vxd32','VX85598.VXD'])
 # A Win9x HAL is a PE32 DLL; do not compile it with the VxD headers/ABI.
 run(['wcc386','-q','-bt=nt','-bd','-oneatxh','-4s','-s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src/'ddhal32'),'-I'+str(src),'-I'+str(wc/'h'/'nt'/'directx'),'-fo=vx_ddraw.obj',src/'vx_ddraw.c'])
 for name in ['vx_d3d_core','vx_d3d_hal']:
  run(['wcc386','-q','-bt=nt','-bd','-4s','-s','-fp3']+['-obmilrt','-oe=40']+['-dWIN95','-DDRV_VER_BUILD=597','-I'+str(wc/'h'/'nt'/'directx'),'-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src/'ddhal32'),'-I'+str(src),'-fo='+name+'.obj',src/(name+'.c')])
 run(['wasm','-q','-fo=vx_tnl_sse.obj',src/'vx_tnl_sse.asm']) # .544: the T&L group kernels (SSE2 as bytes)
 run(['wasm','-q','-fo=vx_hal_sse.obj',src/'vx_hal_sse.asm']) # .545: the pass-through's span kernels (SSE2 as bytes)
 (b/'ddraw.lnk').write_text('system nt_dll\noption quiet\noption offset=0xA0000000\noption start=\'_DllMain@12\'\noption map=VXDDRAW.map\noption statics\nname VXDDRAW.DLL\nfile vx_ddraw.obj,vx_d3d_core.obj,vx_d3d_hal.obj,vx_tnl_sse.obj,vx_hal_sse.obj\nlibrary kernel32,user32,gdi32\nexport DriverInit=\'_DriverInit@4\'\nexport VXDDABI\nexport VXDBGKEY\nexport VXDBGKEYON\nexport VXOVKEY\nexport VXOVKEYON\nexport VXDD3DABI\nexport VX_D3D_QueryStats=\'_VX_D3D_QueryStats@4\'\nexport VX_D3D_Record=\'_VX_D3D_Record@4\'\nexport VX_D3D_RecordFrame=\'_VX_D3D_RecordFrame@0\'\nexport VX_D3D_QueryTrace=\'_VX_D3D_QueryTrace@8\'\nexport VX_D3D_QueryStates=\'_VX_D3D_QueryStates@8\'\nexport VX_D3D_QueryTSS=\'_VX_D3D_QueryTSS@8\'\nexport VX_D3D_QueryEvents=\'_VX_D3D_QueryEvents@8\'\nexport VX_D3D_QueryDraws=\'_VX_D3D_QueryDraws@8\'\nexport VX_D3D_QueryProfile=\'_VX_D3D_QueryProfile@8\'\nexport VX_D3D_QueryLatency=\'_VX_D3D_QueryLatency@8\'\nexport VX_D3D_QuerySlow=\'_VX_D3D_QuerySlow@8\'\nexport VX_D3D_QuerySlowOps=\'_VX_D3D_QuerySlowOps@8\'\nexport VX_D3D_QuerySlowIo=\'_VX_D3D_QuerySlowIo@8\'\nexport VX_D3D_LastPid=\'_VX_D3D_LastPid@0\'\nexport VX_DD_QuerySlowBlt=\'_VX_DD_QuerySlowBlt@8\'\nexport VX_DD_Reopened=\'_VX_DD_Reopened@0\'\nexport VX_DD_Vram=\'_VX_DD_Vram@8\'\nexport VX_DD_Surface=\'_VX_DD_Surface@12\'\nexport VX_DD_StencilPlane=\'_VX_DD_StencilPlane@12\'\nexport VX_DD_QueryFrames=\'_VX_DD_QueryFrames@8\'\nexport VX_DD_QueryBlts=\'_VX_DD_QueryBlts@8\'\nexport VX_DD_QueryBltSigs=\'_VX_DD_QueryBltSigs@8\'\nexport VX_DD_Present=\'_VX_DD_Present@4\'\nexport VX_D3D_QueryPhases=\'_VX_D3D_QueryPhases@8\'\n')
 run(['wlink','@ddraw.lnk'])
 run([str(fix),'-shared','VXDDRAW.DLL'])
 shutil.copy2(b/'VXDDRAW.DLL',ROOT/'driver'/'VXDDRAW.DLL')
 shutil.copy2(b/'VXDDRAW.map',ROOT/'verification'/'VXDDRAW.map')
 # v10.239: VXGL.DLL, the OpenGL ICD: Mesa 7.0.3 (gl/mesa, MIT) and our back end (gl/vxgl_*.c) with the portable core. Built with
 # -6s -sg (__GRO probes frames over 4 KB: swrast has 100 KB frames; Windows 98 grows a stack only through its guard page). Mesa's
 # objects are kept in build/gl and compiled again when their source or any Mesa header is newer.
 # -bm -DWIN32_THREADS: the multithread C library and Mesa's thread-safe dispatch (a game may draw from a second thread, Quake 3's
 # r_smp); one thread keeps the direct _glapi_Context path. glthread.h's Windows mutexes are spin locks (zero-filled statics).
 gl=ROOT/'gl';mesa=gl/'mesa'/'src'/'mesa';gb=b/'gl';gb.mkdir(exist_ok=True)
 import re as _re
 srcs=[];txt=(mesa/'sources').read_text()
 for var in ['MAIN_SOURCES','GLAPI_SOURCES','MATH_SOURCES','SWRAST_SOURCES','SWRAST_SETUP_SOURCES','TNL_SOURCES','VBO_SOURCES','SHADER_SOURCES','SLANG_SOURCES']:
  m=_re.search(var+r'\s*=\s*\\\n((?:.*\\\n)*.*)',txt);srcs+=_re.findall(r'(\S+\.c)',m.group(1))
 srcs+=['drivers/common/driverfuncs.c']
 ginc=['-I'+str(gl/'mesa'/'include'),'-I'+str(mesa),'-I'+str(mesa/'main'),'-I'+str(mesa/'glapi'),'-I'+str(mesa/'math'),'-I'+str(mesa/'tnl'),'-I'+str(mesa/'shader'),
       '-I'+str(mesa/'shader'/'grammar'),'-I'+str(mesa/'shader'/'slang'),'-I'+str(gl),'-I'+str(wc/'h'/'nt'/'directx'),'-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h')]
 gflags=['wcc386','-q','-bt=nt','-bd','-6s','-sg','-s','-fp6','-ox','-zp4','-w1','-wcd=303','-wcd=202','-wcd=140','-bm','-DWIN32','-D_WIN32','-DWIN32_THREADS','-DBUILD_GL32','-DDRV_VER_BUILD=597']
 hdr=max([f.stat().st_mtime for d in [gl/'mesa'/'include',mesa] for f in d.rglob('*.h')]+[ (ROOT/'build.py').stat().st_mtime ])
 objs=[]
 for f in srcs:
  o=gb/(f.replace('/','_')[:-2]+'.obj');objs.append(o)
  if o.exists() and o.stat().st_size>0 and o.stat().st_mtime>max((mesa/f).stat().st_mtime,hdr):continue
  run(gflags+(['-oe=400'] if f in ('tnl/t_vertex_generic.c','tnl/t_vb_light.c','tnl/t_vb_fog.c') else [])+ginc+['-fo='+str(o),mesa/f]) # v10.483: the emit's inserts, vx_itrunc inlined
 for f in ['vxgl_icd','vxgl_mesa','vxgl_state','vxgl_tris','vxgl_tex','vxgl_batch','vxgl_fp','vxgl_fbo','vxgl_sse','vxgl_ssepass']: # v10.487: vxgl_ssepass
  o=gb/(f+'.obj');objs.append(o);run(gflags+(['-oe=400'] if f=='vxgl_ssepass' else [])+ginc+['-I'+str(src),'-fo='+str(o),gl/(f+'.c')]) # v10.487: the pass's small helpers inlined
 o=gb/'vx_d3d_core.obj';objs.append(o);run(['wcc386','-q','-bt=nt','-bd','-bm','-6s','-s','-fp6','-obmilrt','-oe=40','-dWIN95','-DDRV_VER_BUILD=597','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fo='+str(o),src/'vx_d3d_core.c'])
 objs.append(b/'vx_tnl_sse.obj');objs.append(b/'vx_hal_sse.obj') # .544/.545: vx_d3d_core.c's SSE2 kernels
 o=gb/'vx_native3d_hw_gl.obj'
 exps=[('DrvCopyContext',12),('DrvCreateContext',4),('DrvCreateLayerContext',8),('DrvDeleteContext',4),('DrvDescribeLayerPlane',20),('DrvDescribePixelFormat',16),
       ('DrvGetLayerPaletteEntries',20),('DrvGetProcAddress',4),('DrvRealizeLayerPalette',12),('DrvReleaseContext',4),('DrvSetContext',12),('DrvSetLayerPaletteEntries',20),
       ('DrvSetPixelFormat',8),('DrvShareLists',8),('DrvSwapBuffers',4),('DrvSwapLayerBuffers',8),('DrvValidateVersion',4),('VXGL_FpStats',8),('VXGL_FlatStats',8),('VXGL_FlatStats6',8),('VXGL_FlatStats8',8),('VXGL_FlatStats10',8),('VXGL_ClipStats',8),('VXGL_SsePassStats',8),('VXGL_SsePassStats10',8),('VXGL_SsePassStats16',8),('VXGL_SsePassStats20',8),('VXGL_StallStats',8),('VXGL_Stats500',8),('VXGL_Stats501',8),('VXGL_Stats503',8),('VXGL_Stats505',8),('VXGL_Stats506',8)]
 lnk='system nt_dll initinstance terminstance\noption quiet\noption offset=0x21000000\noption map=VXGL.map\noption statics\noption stack=64k\nname VXGL.DLL\n'+''.join('file '+str(x)+'\n' for x in objs)
 lnk+='library kernel32,user32,gdi32,advapi32\nlibrary '+str(wc/'lib386'/'nt'/'directx'/'dxguid.lib')+'\n'+''.join("export %s='_%s@%d'\n"%(n,n,k) for n,k in exps)
 (b/'vxgl.lnk').write_text(lnk)
 run(['wlink','@vxgl.lnk'])
 shutil.copy2(b/'VXGL.DLL',ROOT/'driver'/'VXGL.DLL')
 for name,source in [('VX3DM1.EXE','vx3dm1.c'),('VXDX7.EXE','vx_dx7_test.c'),('VXFLIPF.EXE','vx_flipf_test.c'),('VXDISCO.EXE','vx_disco.c'),('VXRING.EXE','vx_ring_test.c'),('VXGL.EXE','vx_gl_test.c'),('VXGAME.EXE','vx_game_test.c'),('VXD3DMON.EXE','vx_d3d_mon.c'),('VXDCI.EXE','vx_dci_test.c'),('VXSTEN.EXE','vx_sten_test.c'),('VXTEX.EXE','vx_tex_test.c'),('VXKEY.EXE','vx_key_test.c'),('VXPRES.EXE','vx_pres_test.c'),('VXRTT.EXE','vx_rtt_test.c'),('VXFORMAT.EXE','vx_fmt_test.c'),('VXFLAT.EXE','vx_flat_test.c'),('VXSMPCAL.EXE','vx_smp_cal.c'),('VXRHW.EXE','vx_rhw_test.c'),('VXHOLD.EXE','vx_hold_test.c'),('VXV4.EXE','vx_v4_test.c'),('VXREG.EXE','vx_reg_test.c'),('VXTLP.EXE','vx_tlp_test.c'),('VXEP.EXE','vx_ep_test.c'),('VXNOSYNC.EXE','vx_nosync_test.c'),('VXAB.EXE','vx_ab_test.c'),('VXALT.EXE','vx_alt_test.c'),('VXTIME.EXE','vx_time_test.c'),('VXIDLE.EXE','vx_idle_test.c')]:
  run(['wcl386','-q','-bt=nt','-l=nt','-4s','-fp3','-dWIN95','-I'+str(wc/'h'/'nt'/'directx'),'-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src/'ddhal32'),'-I'+str(src),'-fe='+name,src/source,'kernel32.lib','user32.lib','gdi32.lib','uuid.lib',str(wc/'lib386'/'nt'/'directx'/'dxguid.lib')]+(['opengl32.lib','advapi32.lib'] if name in ('VXGL.EXE','VXGAME.EXE','VXSTEN.EXE','VXTEX.EXE','VXPRES.EXE','VXFLAT.EXE','VXALT.EXE') else []))
  if name=='VXDISCO.EXE' and (src/'res'/'wintundd.ocx').exists(): # v1.3: WinTune's original WinTunDD.ocx rides at the end of the EXE
   ocx=(src/'res'/'wintundd.ocx').read_bytes()
   with open(b/name,'ab') as o:o.write(ocx+b'VXWTOCX1'+struct.pack('<I',len(ocx)))
  shutil.copy2(b/name,ROOT/'driver'/name)
 if (ROOT/'src'/'vx98test.c').exists():
  run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX98TEST.EXE',src/'vx98test.c','user32.lib','gdi32.lib','kernel32.lib'])
  shutil.copy2(b/'VX98TEST.EXE',ROOT/'driver'/'VX98TEST.EXE')
 for name in ['VX85598.DRV','VX85598.VXD']: shutil.copy2(b/name,ROOT/'driver'/name)
 if (src/'vx3dtest.c').exists():
  run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX3DTEST.EXE',src/'vx3dtest.c','user32.lib','gdi32.lib','kernel32.lib'])
  shutil.copy2(b/'VX3DTEST.EXE',ROOT/'driver'/'VX3DTEST.EXE')
 if (src/'vx2dlab.c').exists():
  run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2DLAB.EXE',src/'vx2dlab.c','user32.lib','gdi32.lib','kernel32.lib'])
  shutil.copy2(b/'VX2DLAB.EXE',ROOT/'driver'/'VX2DLAB.EXE')
 if (src/'vx2dvi.c').exists():
  run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2DVI.EXE',src/'vx2dvi.c','user32.lib','gdi32.lib','kernel32.lib'])
  shutil.copy2(b/'VX2DVI.EXE',ROOT/'driver'/'VX2DVI.EXE')
 if (src/'vx3suite.c').exists():
  run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX3SUITE.EXE',src/'vx3suite.c','user32.lib','gdi32.lib','kernel32.lib'])
  shutil.copy2(b/'VX3SUITE.EXE',ROOT/'driver'/'VX3SUITE.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-DDRV_VER_BUILD=597','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2BENCH.EXE',src/'vx2bench.c','user32.lib','gdi32.lib','kernel32.lib'])
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VXGTUNE.EXE',src/'vx_gtune.c','user32.lib','gdi32.lib','kernel32.lib']) # v10.507
 shutil.copy2(b/'VX2BENCH.EXE',ROOT/'driver'/'VX2BENCH.EXE')
 shutil.copy2(b/'VXGTUNE.EXE',ROOT/'driver'/'VXGTUNE.EXE') # v10.507
 run(['wcc','-q','-bt=windows','-ml','-zW','-I'+str(wc/'h'),'-I'+str(wc/'h'/'win'),'-fo=vx_sel16.obj',src/'vx_sel16.c']) # .514: VXSEL, a Win16 program (TOOLHELP)
 run(['wlink','op','quiet','system','windows','name','VXSEL.EXE','file','vx_sel16.obj','library','toolhelp.lib'])
 shutil.copy2(b/'VXSEL.EXE',ROOT/'driver'/'VXSEL.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2FLIP.EXE',src/'vx2flip.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2FLIP.EXE',ROOT/'driver'/'VX2FLIP.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2MODE.EXE',src/'vx2mode.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2MODE.EXE',ROOT/'driver'/'VX2MODE.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2AUTO.EXE',src/'vx2auto.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2AUTO.EXE',ROOT/'driver'/'VX2AUTO.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2D8.EXE',src/'vx2d8.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2D8.EXE',ROOT/'driver'/'VX2D8.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2COL.EXE',src/'vx2col.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2COL.EXE',ROOT/'driver'/'VX2COL.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2TRN.EXE',src/'vx2trn.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2TRN.EXE',ROOT/'driver'/'VX2TRN.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2ROP.EXE',src/'vx2rop.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2ROP.EXE',ROOT/'driver'/'VX2ROP.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2LINE.EXE',src/'vx2line.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2LINE.EXE',ROOT/'driver'/'VX2LINE.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2PRIM.EXE',src/'vx2prim.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2PRIM.EXE',ROOT/'driver'/'VX2PRIM.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2STR.EXE',src/'vx2str.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2STR.EXE',ROOT/'driver'/'VX2STR.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2ASYNC.EXE',src/'vx2async.c','user32.lib','gdi32.lib','kernel32.lib','advapi32.lib'])
 shutil.copy2(b/'VX2ASYNC.EXE',ROOT/'driver'/'VX2ASYNC.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2SDIB.EXE',src/'vx2sdib.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2SDIB.EXE',ROOT/'driver'/'VX2SDIB.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2SDI2.EXE',src/'vx2sdi2.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2SDI2.EXE',ROOT/'driver'/'VX2SDI2.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2DTD.EXE',src/'vx2dtd.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2DTD.EXE',ROOT/'driver'/'VX2DTD.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2BLT.EXE',src/'vx2blt.c','user32.lib','gdi32.lib','kernel32.lib']) # v10.321
 shutil.copy2(b/'VX2BLT.EXE',ROOT/'driver'/'VX2BLT.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2STRX.EXE',src/'vx2strx.c','user32.lib','gdi32.lib','kernel32.lib','advapi32.lib'])
 shutil.copy2(b/'VX2STRX.EXE',ROOT/'driver'/'VX2STRX.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2STR2.EXE',src/'vx2str2.c','user32.lib','gdi32.lib','kernel32.lib','advapi32.lib'])
 shutil.copy2(b/'VX2STR2.EXE',ROOT/'driver'/'VX2STR2.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2CUR.EXE',src/'vx2cur.c','user32.lib','gdi32.lib','kernel32.lib','advapi32.lib'])
 shutil.copy2(b/'VX2CUR.EXE',ROOT/'driver'/'VX2CUR.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2OVL.EXE',src/'vx2ovl.c','user32.lib','gdi32.lib','kernel32.lib','advapi32.lib'])
 shutil.copy2(b/'VX2OVL.EXE',ROOT/'driver'/'VX2OVL.EXE')
 for t in ['VX2GAMMA','VX2SWAP','VX2SAVE','VX2WIDE','VX2OVL8','VX2OPT','VX2OPT2','VX2SPAN8','VX2COL8','VX2TIP','VX2TUNE','VX2FIX','VX2FAST','VX2TOP','VX2KEEP','VX2DKEY']:
  run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe='+t+'.EXE',src/(t.lower()+'.c'),'user32.lib','gdi32.lib','kernel32.lib','ddraw.lib','advapi32.lib'])
  shutil.copy2(b/(t+'.EXE'),ROOT/'driver'/(t+'.EXE'))
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2GDI.EXE',src/'vx2gdi.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2GDI.EXE',ROOT/'driver'/'VX2GDI.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2PAT.EXE',src/'vx2pat.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2PAT.EXE',ROOT/'driver'/'VX2PAT.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2TIME.EXE',src/'vx2time.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2TIME.EXE',ROOT/'driver'/'VX2TIME.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2BARS.EXE',src/'vx2bars.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2BARS.EXE',ROOT/'driver'/'VX2BARS.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2HOLD.EXE',src/'vx2hold.c','user32.lib','gdi32.lib','kernel32.lib'])
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2CONF.EXE',src/'vx2conf.c','user32.lib','gdi32.lib','kernel32.lib'])
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2CYCLE.EXE',src/'vx2cycle.c','user32.lib','gdi32.lib','kernel32.lib'])
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2PROOF.EXE',src/'vx2proof.c','user32.lib','gdi32.lib','kernel32.lib'])
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2MEM.EXE',src/'vx2mem.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2MEM.EXE',ROOT/'driver'/'VX2MEM.EXE')
 run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2READ.EXE',src/'vx2read.c','user32.lib','gdi32.lib','kernel32.lib'])
 shutil.copy2(b/'VX2READ.EXE',ROOT/'driver'/'VX2READ.EXE')
 shutil.copy2(b/'VX2HOLD.EXE',ROOT/'driver'/'VX2HOLD.EXE')
 shutil.copy2(b/'VX2CONF.EXE',ROOT/'driver'/'VX2CONF.EXE')
 shutil.copy2(b/'VX2CYCLE.EXE',ROOT/'driver'/'VX2CYCLE.EXE')
 shutil.copy2(b/'VX2PROOF.EXE',ROOT/'driver'/'VX2PROOF.EXE')
 if (src/'vx2sync.c').exists():
  run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2SYNC.EXE',src/'vx2sync.c','user32.lib','gdi32.lib','kernel32.lib'])
  shutil.copy2(b/'VX2SYNC.EXE',ROOT/'driver'/'VX2SYNC.EXE')
 if (src/'vx2edid.c').exists():
  run(['wcl386','-q','-bt=nt','-l=nt','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2EDID.EXE',src/'vx2edid.c','kernel32.lib','user32.lib'])
  shutil.copy2(b/'VX2EDID.EXE',ROOT/'driver'/'VX2EDID.EXE')
 if (src/'vx2crt.c').exists():
  run(['wcl386','-q','-bt=nt','-l=nt','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VX2CRT.EXE',src/'vx2crt.c','kernel32.lib','user32.lib','gdi32.lib'])
  shutil.copy2(b/'VX2CRT.EXE',ROOT/'driver'/'VX2CRT.EXE')
 if (src/'vxprobe.c').exists(): # v10.333: probes for the open review items
  run(['wcl386','-q','-bt=nt','-l=nt','-4s','-fp3','-dWIN95','-I'+str(wc/'h'/'nt'/'directx'),'-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VXPROBE.EXE',src/'vxprobe.c','kernel32.lib','user32.lib','gdi32.lib','advapi32.lib','opengl32.lib','uuid.lib',str(wc/'lib386'/'nt'/'directx'/'dxguid.lib')])
  shutil.copy2(b/'VXPROBE.EXE',ROOT/'driver'/'VXPROBE.EXE')
 if (src/'vxstate.c').exists(): # v10.331: read-only state snapshot
  run(['wcl386','-q','-bt=nt','-l=nt','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=VXSTATE.EXE',src/'vxstate.c','kernel32.lib','user32.lib','gdi32.lib'])
  shutil.copy2(b/'VXSTATE.EXE',ROOT/'driver'/'VXSTATE.EXE')
 if (src/'clxp3d.c').exists(): # v10.170: XP Direct3D 7 capture cases (runs on the XP machine)
  run(['wcl386','-q','-bt=nt','-l=nt','-4s','-fp3','-I'+str(wc/'h'/'nt'/'directx'),'-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-fe=CLXP3D.EXE',src/'clxp3d.c','kernel32.lib','user32.lib','gdi32.lib','uuid.lib',str(wc/'lib386'/'nt'/'directx'/'dxguid.lib')])
 if (src/'clxpovl.c').exists(): # v10.340: XP overlay probe (V3, constant alpha, 8 bpp key; runs on the XP machine)
  run(['wcl386','-q','-bt=nt','-l=nt_win','-4s','-fp3','-I'+str(wc/'h'/'nt'/'directx'),'-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-fe=CLXPOVL.EXE',src/'clxpovl.c','kernel32.lib','user32.lib','gdi32.lib','advapi32.lib','uuid.lib',str(wc/'lib386'/'nt'/'directx'/'dxguid.lib')])
  shutil.copy2(b/'CLXPOVL.EXE',ROOT/'driver'/'CLXPOVL.EXE')
 if (src/'cxphys.c').exists() and shutil.which('i686-w64-mingw32-gcc'): # v10.171: CLXP3D's read-only XP kernel reader (MinGW-w64)
  run(['i686-w64-mingw32-gcc','-O2','-Wall','-fno-stack-protector','-fno-asynchronous-unwind-tables','-I/usr/i686-w64-mingw32/include/ddk','-D_X86_','-nostdlib','-nostartfiles',
       '-Wl,--subsystem,native','-Wl,--entry,_DriverEntry@8','-Wl,--image-base,0x10000','-Wl,--dynamicbase','-Wl,--disable-nxcompat','-Wl,--major-os-version,5','-Wl,--minor-os-version,1',
       '-Wl,--major-subsystem-version,5','-Wl,--minor-subsystem-version,1','-o','CXPHYS.SYS',src/'cxphys.c','-lntoskrnl','-lgcc'])
  import struct as _st
  d=bytearray((b/'CXPHYS.SYS').read_bytes());co=_st.unpack_from('<I',d,0x3c)[0]+88;_st.pack_into('<I',d,co,0);c=0
  for i in range(0,len(d)&~1,2):c+=d[i]|(d[i+1]<<8);c=(c&0xffff)+(c>>16)
  c=((c&0xffff)+(c>>16))+len(d);_st.pack_into('<I',d,co,c);(b/'CXPHYS.SYS').write_bytes(bytes(d)) # PE checksum
 if (src/'xpdump.c').exists():
  run(['wcl386','-q','-bt=nt','-l=nt','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=XPDUMP.EXE',src/'xpdump.c','kernel32.lib'])
  shutil.copy2(b/'XPDUMP.EXE',ROOT/'driver'/'XPDUMP.EXE')
 if (src/'biosrd.c').exists():
  run(['wcl386','-q','-bt=nt','-l=nt','-4s','-fp3','-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(src),'-fe=BIOSRD.EXE',src/'biosrd.c','kernel32.lib'])
  shutil.copy2(b/'BIOSRD.EXE',ROOT/'driver'/'BIOSRD.EXE')
 # Preserve the peer-proven diagnostic client (the DRV/VxD are freshly built).
 for name in ['VX98TEST.EXE']:
  baseline=ROOT/'fallback-1_2c'/name
  if baseline.exists(): shutil.copy2(baseline,ROOT/'driver'/name)
 for name in ['VX85598_DRV.map','VX85598_VXD.map']: shutil.copy2(b/name,ROOT/'verification'/name)
 files={p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in (ROOT/'driver').glob('*') if p.is_file()}
 # v10.319 (audit T4): the OpenGL ICD's sources (gl/, Mesa included) too; build products in the tree are left out
 gen={'.o','.obj','.err','.map','.lib','.dll','.exe','.lst','.res','.pyc'}
 source_hashes={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(list(src.rglob('*'))+list((ROOT/'gl').rglob('*'))) if p.is_file() and p.suffix.lower() not in gen}
 source_hashes['build.py']=hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest()
 (ROOT/'verification'/'BUILD.json').write_text(json.dumps({'milestone':'VX855 Direct3D M2 rebased on v10.148','source_baseline':'VX855_v10_148_source.zip','upstream_commit':'767761d0a49e4c9d4c66e906f38e6118b8b44f89','fixlink_commit':'c1fd1d2264995b9fa76985c76ef2bd1396af377a','toolchain_url':PINNED_URL,'toolchain_sha256':PINNED_SHA256,'files':files,'source_sha256':source_hashes},indent=2)+'\n')
 # .580: the files as they are installed: one name for the family (the build's own names stay VX85598.* inside, as the tests and the VxD's device name \\.\VX85598 use them)
 for a_,b_ in (('VX85598.DRV','CHROME9.DRV'),('VX85598.VXD','CHROME9.VXD'),('VX85598.INF','CHROME9.INF')): shutil.copy2(ROOT/'driver'/a_,ROOT/'driver'/b_)
 log.close(); print('Built VX85598.DRV and VX85598.VXD from source.')

if __name__=='__main__': main()
