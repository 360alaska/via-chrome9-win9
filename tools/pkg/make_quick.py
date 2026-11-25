#!/usr/bin/env python3
"""make_quick.py (.704): QUICK.BAT, the targeted test run of one build: the sanity steps every build gets (VXREG's build check, VXTLP's
picture checks, VXNOSYNC, CLXP3D) and then the steps named for what the build changed. ALL.BAT stays in the package for the full run.
usage: make_quick.py BUILD OUT.BAT STEP...   (steps: GRIDVP, GRIDAB, PURAB, HB, REUSE, IPAB, TWAB, GLAB, SMP, FEAT, PROJ, RHW, PROF, TEAR, FILL, TRIAB, GDI, GL, FXAB, TFAB, TRAB, PSP, KB, POSAB, PST, CULLAB, VBAB, VBQ, VBOX1, NOMON, GLCG, IXOSK, GLPROF, EDID)"""
import sys,re
B,out,extra=sys.argv[1],sys.argv[2],sys.argv[3:]
T='T'+B;L=[]
a=L.append
def step(name):a('echo %s>> %s\\STEPS.TXT'%(name,T))
def clean(globs):
 for g in globs:a('if exist %s del %s'%(g,g))
def keep(globs):
 for g in globs:a('if exist %s copy %s %s >nul'%(g,g,T));a('if exist %s del %s'%(g,g))
BMPS=['VXT?_?.BMP','VXT??_?.BMP','VXTL7*.BMP','VXTL1?A.BMP','VXTL1?B.BMP','VXTLC_?.BMP','VXTLF??.BMP','6M*.BMP','6P*.BMP','6N*.BMP','6Q*.BMP','6W*.BMP'] # .717: 6M's pictures
a('@echo off')
a('rem Driver .%s: the targeted run (QUICK.BAT), about %d minutes. ALL.BAT in this folder is the full run.'%(B,10+sum({'GRIDVP':5,'GRIDAB':10,'PURAB':8,'HB':25,'REUSE':1,'IPAB':22,'TWAB':22,'GLAB':40,'SMP':35,'FEAT':12,'PROJ':4,'RHW':6,'PROF':12,'TEAR':1,'FILL':1,'TRIAB':12,'GDI':3,'GL':5,'FXAB':22,'TFAB':22,'TRAB':22,'KB':1,'VBAB':30,'VBQ':15,'NOMON':5,'GLCG':6,'VBOX1':1,'IXOSK':30,'GLPROF':4,'EDID':1,'PSP':1,'POSAB':8,'PST':1,'CULLAB':16}.get(x,0) for x in extra)))
a('rem Every build: VXREG (the build), VXTLP (the pictures), VXNOSYNC, CLXP3D. This build also: %s.'%(', '.join(extra) or 'nothing more'))
a('rem Install .%s (CHROME9.INF) and restart first. Send the folder %s.'%(B,T))
for f in ['VXREG.EXE','VXTLP.EXE','CLXP3D.EXE','VXNOSYNC.EXE','ALLON.REG']+(['VXREUSE.EXE'] if 'REUSE' in extra else [])+(['VXRHW.EXE','VXDX7.EXE','VXD3DMON.EXE'] if 'RHW' in extra else [])+(['VXD3DMON.EXE','HB\\HarborBench.exe','HB\\HB_HW.INI'] if 'VBAB' in extra or 'VBQ' in extra or 'NOMON' in extra else [])+(['VXGL4.DLL','VXGL6.DLL','HB\\HB_GL.INI','HB\\HarborBench.exe'] if 'GLCG' in extra else [])+(['HB\\HarborBench.exe','HB\\HB_HW.INI'] if 'IXOSK' in extra else [])+(['VXD3DMON.EXE','GLPROF.REG','GLPROF0.REG','VXGL.MAP','HB\\HB_GL.INI','HB\\HarborBench.exe'] if 'GLPROF' in extra else [])+(['VXPROBE.EXE'] if 'EDID' in extra else [])+(['IP0.REG'] if 'IPAB' in extra else [])+(['FX0.REG','HB\\HarborBench.exe','HB\\HB_HW.INI'] if 'FXAB' in extra else [])+(['TF0.REG','HB\\HarborBench.exe','HB\\HB_HW.INI'] if 'TFAB' in extra else [])+(['TR0.REG','HB\\HarborBench.exe','HB\\HB_HW.INI'] if 'TRAB' in extra else [])+(['TW0.REG'] if 'TWAB' in extra else [])+(['GL0.REG','HB\\HarborBench.exe','HB\\HarborBench.ini'] if 'GLAB' in extra else [])+(['HB\\HarborBench.exe','HB\\HB_HW.INI'] if {'IPAB','TWAB'}&set(extra) else [])+(['VXD3DMON.EXE','HB\\HarborBench.exe','HB\\HB_HW.INI'] if 'PURAB' in extra or 'HB' in extra else [])+(['VXD3DMON.EXE','HB\\HarborBench.exe','HB\\HarborBench.ini'] if {'SMP','PROF'}&set(extra) else [])+(['VXCLIP.EXE'] if 'FEAT' in extra else [])+(['VX2GDI.EXE'] if 'GDI' in extra else [])+(['VXGL.EXE'] if 'GL' in extra else [])+(['VXPSP.EXE'] if 'PSP' in extra else [])+(['TRI0.REG','HB\\HarborBench.exe','HB\\HB_HW.INI'] if 'TRIAB' in extra else [])+(['VXD3DMON.EXE'] if 'PROJ' in extra else []):
 a('if not exist %s goto nofiles'%f)
a('if not exist %windir%\\SYSTEM\\D3D8.DLL goto nodx') # .715: a fresh Windows 98 (DirectX 6.1) skips every Direct3D 7/8 test and HarborBench scene
a('start /w VXREG.EXE');a('find "build: .%s" VXREG.LOG >nul'%re.sub(r'[^0-9]','',B)) # .737B: a letter for a changed build (the owner, 2026-11-22); VXREG's number is plain
a('if errorlevel 1 goto wrongbuild') # .794: this sat inside the comment above from .737B to .769B, so no run checked the installed build (found by the other line, E768)
a('if not exist %s\\nul md %s'%(T,T));a('if exist %s\\STEPS.TXT del %s\\STEPS.TXT'%(T,T));a('regedit /s ALLON.REG')
a('copy VXREG.LOG %s\\REG.LOG >nul'%T)
clean(BMPS)
step('1 VXTLP');a('echo 1. VXTLP: the picture checks, 2-3 minutes.');a('start /w VXTLP.EXE')
a('copy VXTLP.LOG %s\\VXTLP.LOG >nul'%T);a('del VXTLP.LOG');keep(BMPS)
if 'GRIDVP' in extra:
 step('2 GRID VP');a('echo 2. VXTLP GRID VP: every GRID scene through a viewport at (40, 40) on both devices (the guard band across a clip edge that is not 0), about 5 minutes.')
 a('start /w VXTLP.EXE GRID VP');a('copy VXTLP.LOG %s\\GRIDVP.LOG >nul'%T);a('del VXTLP.LOG');clean(['VX?_??.CSV']);keep(['VXT?_?.BMP','VXT??_?.BMP','VXTLC_?.BMP'])
if 'GRIDAB' in extra:
 for k,reg in enumerate(['','']): # .739: two runs (the guard band is no longer a switch)
  step('3 GRID HW %d%s'%(k+1,' '+reg if reg else ''))
  if reg:a('regedit /s %s'%reg)
  a('start /w VXTLP.EXE GRID HW');a('copy VXTLP.LOG %s\\GRIDAB_%d.LOG >nul'%(T,k+1));a('del VXTLP.LOG')
  if reg:a('regedit /s ALLON.REG')
  clean(['VX?_??.CSV']+BMPS)
if 'PURAB' in extra:
 a('copy HB\\HarborBench.ini HB\\HB_MAIN.INI >nul');a('copy HB\\HB_HW.INI HB\\HarborBench.ini >nul')
 for tag,reg in [('PUR','')]: # .739: no band-off run
  step('4 profile HarborBench pursuit %s'%(reg or ''))
  if reg:a('regedit /s %s'%reg)
  a('if exist HB\\results.txt del HB\\results.txt');a('cd HB');a('start /w ..\\VXD3DMON.EXE 120 RUN HarborBench.exe /fullscreen /ram /scene=pursuit');a('cd ..')
  if reg:a('regedit /s ALLON.REG')
  a('if exist HB\\VXD3DMON.TXT copy HB\\VXD3DMON.TXT %s\\PROF_%s.TXT >nul'%(T,tag));a('if exist HB\\VXD3DMON.TXT del HB\\VXD3DMON.TXT')
  a('if exist HB\\results.txt copy HB\\results.txt %s\\HBP_%s.TXT >nul'%(T,tag));a('if exist HB\\results.txt del HB\\results.txt');a('if exist HB\\frames.csv del HB\\frames.csv')
 a('copy HB\\HB_MAIN.INI HB\\HarborBench.ini >nul')
if 'FEAT' in extra: # .716: the two VX855-only features tried on this chip: the engine's own clipping (VXCLIP), the tear flip judged by
 # eye (VXNOSYNC TEAR ASK), the guard band switched on live for VXTLP's pictures and GRID through the viewport, then off again
 step('4f VXCLIP');a('echo 4f. VXCLIP: the engine clipping probe, about 15 seconds; the screen is not touched.')
 a('if exist VXCLIP.LOG del VXCLIP.LOG');a('start /w VXCLIP.EXE');a('if exist VXCLIP.LOG copy VXCLIP.LOG %s\\VXCLIP.LOG >nul'%T);a('if exist VXCLIP.LOG del VXCLIP.LOG')
 step('4g VXNOSYNC TEAR ASK');a('echo 4g. VXNOSYNC TEAR ASK: the tear flip, full screen: answer each question by what you saw (thin tear lines are fine;')
 a('echo     black flashes or a jumping or doubled red bar are not). About a minute.');a('pause')
 a('if exist VXNOSYNC.LOG del VXNOSYNC.LOG');a('start /w VXNOSYNC.EXE TEAR ASK');a('if exist VXNOSYNC.LOG copy VXNOSYNC.LOG %s\\TEAR.LOG >nul'%T);a('if exist VXNOSYNC.LOG del VXNOSYNC.LOG')
 step('4h guard band on: VXTLP, GRID VP');a('echo 4h. The guard band switched on for this chip: VXTLP pictures and GRID VP, about 8 minutes.')
 a('if exist VXGB.LOG del VXGB.LOG');a('start /w VXREG.EXE GBAND ON')
 a('start /w VXTLP.EXE');a('copy VXTLP.LOG %s\\GBVXTLP.LOG >nul'%T);a('del VXTLP.LOG')
 a('start /w VXTLP.EXE GRID VP');a('copy VXTLP.LOG %s\\GBGRIDVP.LOG >nul'%T);a('del VXTLP.LOG')
 a('start /w VXREG.EXE GBAND OFF');a('if exist VXGB.LOG copy VXGB.LOG %s\\VXGB.LOG >nul'%T);clean(['VX?_??.CSV'])
if 'PROJ' in extra: # .717: VXTLP PROJ; .718: 6M, 6P (deep floors, a floor across the near plane) and 6N (a Dragothic-like water scene), then
 # Dragothic recorded from 4 s into its own drawing (VXD3DMON RECAT=G4: counted from its first heavy second), the water's bad moment
 step('4p VXTLP PROJ');a('echo 4p. VXTLP PROJ: projected textures, deep floors and a water scene against their references (6W: its own), about two minutes.')
 a('start /w VXTLP.EXE PROJ');a('copy VXTLP.LOG %s\\PROJ.LOG >nul'%T);a('del VXTLP.LOG');keep(['6M*.BMP','6P*.BMP','6N*.BMP','6Q*.BMP','6W*.BMP'])
 step('4q Dragothic recorded');a('if exist C:\\VXD3D.REC del C:\\VXD3D.REC')
 a('echo 4q. VXD3DMON starts 3DMark 2001 SE. Run Game 2 Dragothic only (Low Detail is fine) and let it finish; do not touch this')
 a('echo     Watch the water: are the flashing blue and orange triangles gone?')
 a('echo     window. It records 40 frames from 4 seconds into Dragothic (the low water). Then close 3DMark.');a('pause')
 a('start /w VXD3DMON.EXE 150 REC=40 RECAT=G4 RUN C:\\Program Files\\MadOnion.com\\3DMark2001 SE\\3DMark2001SE.exe')
 a('if exist VXD3DMON.TXT copy VXD3DMON.TXT %s\\DRAG.TXT >nul'%T);a('if exist C:\\VXD3D.REC copy C:\\VXD3D.REC %s\\DRAG.REC >nul'%T)
if 'RHW' in extra: # .722: rhw NaN / infinity drawn as rhw 0 is, and FOGFLAT: VXDX7 --enum (the caps, with Hellbender's device test),
 # VXRHW (NaN and +infinity against rhw 1, table fog and mixed still left out), then Hellbender under VXD3DMON
 step('4r VXDX7 --enum');a('echo 4r. VXDX7 --enum: the devices and their caps, a few seconds.')
 a('if exist VXDX7.TXT del VXDX7.TXT');a('start /w VXDX7.EXE --enum');a('if exist VXDX7.TXT copy VXDX7.TXT %s\\ENUM.TXT >nul'%T)
 step('4t VXRHW');a('echo 4t. VXRHW: rhw 0, NaN and infinite primitives against rhw 1, under a minute (a small window).')
 a('if exist VXRHW.LOG del VXRHW.LOG');a('start /w VXRHW.EXE');a('if exist VXRHW.LOG copy VXRHW.LOG %s\\RHW.LOG >nul'%T);keep(['VXRHW_?.BMP'])
 step('4u Hellbender');a('echo 4u. VXD3DMON starts Hellbender (C:\\HELLBEND). Play about a minute in hardware mode: is there a fog box, is the fog')
 a('echo     there, is anything missing? Then quit the game.');a('pause')
 a('if exist C:\\HELLBEND\\HELLBEND.EXE start /w VXD3DMON.EXE 300 RUN C:\\HELLBEND\\HELLBEND.EXE')
 a('if exist VXD3DMON.TXT copy VXD3DMON.TXT %s\\HELL.TXT >nul'%T)
if 'PROF' in extra: # .724: one profile of the current build for ranking the next speed work: VXD3DMON's sampler over HarborBench's
 # whole run (its Direct3D and OpenGL scenes, the driver's T&L and the runtime's), about 12 minutes
 step('4v profile: VXD3DMON 600 SAMPLE, HarborBench (all scenes)');a('echo 4v. VXD3DMON profiles HarborBench (all scenes), about 12 minutes. Do not touch the machine.')
 a('if exist HB\\results.txt del HB\\results.txt');a('cd HB');a('start /w ..\\VXD3DMON.EXE 600 SAMPLE RUN HarborBench.exe /fullscreen /ram');a('cd ..')
 a('if exist HB\\VXD3DMON.TXT copy HB\\VXD3DMON.TXT %s\\PROF.TXT >nul'%T);a('if exist HB\\VXD3DMON.TXT del HB\\VXD3DMON.TXT')
 a('if exist HB\\results.txt copy HB\\results.txt %s\\PROFHB.TXT >nul'%T);a('if exist HB\\results.txt del HB\\results.txt');a('if exist HB\\frames.csv del HB\\frames.csv')
if 'SMP' in extra: # .714: the sampler under load, three times (the VX800 freeze: VXD3DMON SAMPLE with HarborBench's full run); STEPS.TXT
 # names the run that was going when a machine stops
 for k in (1,2,3):
  step('4s sampler run %d: VXD3DMON 600 SAMPLE, HarborBench (all scenes)'%k)
  a('if exist HB\\results.txt del HB\\results.txt');a('cd HB');a('start /w ..\\VXD3DMON.EXE 600 SAMPLE RUN HarborBench.exe /fullscreen /ram');a('cd ..')
  a('if exist HB\\VXD3DMON.TXT copy HB\\VXD3DMON.TXT %s\\SMP_%d.TXT >nul'%(T,k));a('if exist HB\\VXD3DMON.TXT del HB\\VXD3DMON.TXT')
  a('if exist HB\\results.txt copy HB\\results.txt %s\\HBS_%d.TXT >nul'%(T,k));a('if exist HB\\results.txt del HB\\results.txt');a('if exist HB\\frames.csv del HB\\frames.csv')
if 'TEAR' in extra: # .727: the tear flip judged by eye on this chip (on every chip from .725)
 step('4g VXNOSYNC TEAR ASK');a('echo 4g. VXNOSYNC TEAR ASK: the tear flip, full screen: answer each question by what you saw (thin tear lines are fine;')
 a('echo     black flashes or a jumping or doubled red bar are not). About a minute.');a('pause')
 a('if exist VXNOSYNC.LOG del VXNOSYNC.LOG');a('start /w VXNOSYNC.EXE TEAR ASK');a('if exist VXNOSYNC.LOG copy VXNOSYNC.LOG %s\\TEAR.LOG >nul'%T);a('if exist VXNOSYNC.LOG del VXNOSYNC.LOG')
if 'FILL' in extra: # .727: VXTLP FILL, the texture fill rate with one and two units (the VX800's multitexture question)
 step('4i VXTLP FILL');a('echo 4i. VXTLP FILL: the texture fill rate with one and two units, about a minute (a small window).')
 a('start /w VXTLP.EXE FILL');a('copy VXTLP.LOG %s\\FILL.LOG >nul'%T);a('del VXTLP.LOG')
if 'TRIAB' in extra: # .727: the triangle-loop kernel (ACCEL_D3D_TNTRI) on, off, on, off in this one session: GRID 6E and 6K (the driver's
 # T&L, 20 s each) and HarborBench pursuit
 for k,reg in enumerate(['','TRI0.REG','','TRI0.REG']):
  for sc in ('6E','6K'):
   step('4c GRID %s HW TRIAB %d%s'%(sc,k+1,' '+reg if reg else ''))
   if reg:a('regedit /s %s'%reg)
   a('start /w VXTLP.EXE GRID %s HW'%sc);a('copy VXTLP.LOG %s\\TRI%s_%d.LOG >nul'%(T,sc,k+1));a('del VXTLP.LOG')
   if reg:a('regedit /s ALLON.REG')
   clean(['VX?_??.CSV']+BMPS)
 a('copy HB\\HarborBench.ini HB\\HB_MAIN.INI >nul');a('copy HB\\HB_HW.INI HB\\HarborBench.ini >nul')
 for k,reg in enumerate(['','TRI0.REG','','TRI0.REG']):
  step('4d HarborBench pursuit TRIAB %d%s'%(k+1,' '+reg if reg else ''))
  if reg:a('regedit /s %s'%reg)
  a('if exist HB\\results.txt del HB\\results.txt');a('cd HB');a('start /w HarborBench.exe /fullscreen /ram /scene=pursuit');a('cd ..')
  if reg:a('regedit /s ALLON.REG')
  a('if exist HB\\results.txt copy HB\\results.txt %s\\TRIPUR_%d.TXT >nul'%(T,k+1));a('if exist HB\\results.txt del HB\\results.txt');a('if exist HB\\frames.csv del HB\\frames.csv')
 a('copy HB\\HB_MAIN.INI HB\\HarborBench.ini >nul')
if 'GDI' in extra: # .728: the GDI paths the review touched (BitBlt / StretchBlt entry, the host blit's range check, the palette):
 # VX2GDI draws every case with the new paths off and on at 640x480x16 and x32 and compares the screens
 step('4j VX2GDI');a('echo 4j. VX2GDI: GDI drawing from system memory, both ways, compared pixel for pixel; the desktop changes mode a few times. About 3 minutes.')
 a('if exist VX2GDI.TXT del VX2GDI.TXT');a('start /w VX2GDI.EXE');a('if exist VX2GDI.TXT copy VX2GDI.TXT %s\\VX2GDI.TXT >nul'%T);a('if exist VX2GDI.RES copy VX2GDI.RES %s\\VX2GDI.RES >nul'%T)
if 'GL' in extra: # .728: the OpenGL ICD's scenes (the depth renderbuffer now 16 bits a pixel, the clear's ATI shader enable, w = 0)
 step('4k VXGL 1');a('echo 4k. VXGL: the OpenGL scenes against the ICD without the engine, a few minutes (a small window).')
 a('if exist VXGL.TXT del VXGL.TXT');a('start /w VXGL.EXE 1');a('if exist VXGL.TXT copy VXGL.TXT %s\\VXGL.TXT >nul'%T);keep(['VXGL_??_*.BMP'])
if 'REUSE' in extra: # .707
 step('4b VXREUSE');a('echo 4b. VXREUSE: 24 copies of a small Direct3D program one after another, each keeping a file of its own open, under a minute.')
 a('if exist VXREUSE.LOG del VXREUSE.LOG');a('start /w VXREUSE.EXE');a('if exist VXREUSE.LOG copy VXREUSE.LOG %s\\REUSE.LOG >nul'%T);a('if exist VXREUSE.LOG del VXREUSE.LOG')
def ab(tag,reg0,what): # .709 IPAB, .710 TWAB: GRID's driver T&L and pursuit four times each, the switch on, off, on, off, in this one session
 for k,reg in enumerate(['',reg0,'',reg0]):
  step('4c GRID HW %s %d%s'%(tag,k+1,' '+reg if reg else ''))
  if reg:a('regedit /s %s'%reg)
  a('start /w VXTLP.EXE GRID HW');a('copy VXTLP.LOG %s\\%s_%d.LOG >nul'%(T,tag,k+1));a('del VXTLP.LOG')
  if reg:a('regedit /s ALLON.REG')
  clean(['VX?_??.CSV']+BMPS)
 a('copy HB\\HarborBench.ini HB\\HB_MAIN.INI >nul');a('copy HB\\HB_HW.INI HB\\HarborBench.ini >nul')
 for k,reg in enumerate(['',reg0,'',reg0]):
  step('4d HarborBench pursuit %s %d%s'%(tag,k+1,' '+reg if reg else ''))
  if reg:a('regedit /s %s'%reg)
  a('if exist HB\\results.txt del HB\\results.txt');a('cd HB');a('start /w HarborBench.exe /fullscreen /ram /scene=pursuit');a('cd ..')
  if reg:a('regedit /s ALLON.REG')
  a('if exist HB\\results.txt copy HB\\results.txt %s\\%sPUR_%d.TXT >nul'%(T,tag,k+1));a('if exist HB\\results.txt del HB\\results.txt');a('if exist HB\\frames.csv del HB\\frames.csv')
 a('copy HB\\HB_MAIN.INI HB\\HarborBench.ini >nul')
def glab(): # .712: OpenGL's same-state marks (ACCEL_OPENGL_SAMETAG) on, off, on, off in this one session; HarborBench's own INI runs each
 # scene in OpenGL and in Direct3D (software and hardware T&L): the Direct3D runs, untouched by the switch, show the session's drift
 for sc in ('arena','industrial','rain','pursuit'):
  for k,reg in enumerate(['','GL0.REG','','GL0.REG']):
   step('4e HarborBench %s GLAB %d%s'%(sc,k+1,' '+reg if reg else ''))
   if reg:a('regedit /s %s'%reg)
   a('if exist HB\\results.txt del HB\\results.txt');a('cd HB');a('start /w HarborBench.exe /fullscreen /ram /scene=%s'%sc);a('cd ..')
   if reg:a('regedit /s ALLON.REG')
   a('if exist HB\\results.txt copy HB\\results.txt %s\\GLAB_%s_%d.TXT >nul'%(T,sc,k+1));a('if exist HB\\results.txt del HB\\results.txt');a('if exist HB\\frames.csv del HB\\frames.csv')
if 'GLAB' in extra:glab()
if 'IPAB' in extra:ab('IPAB','IP0.REG','the batch in its queue slot')
if 'TWAB' in extra:ab('TWAB','TW0.REG','the T&L code passes four places a word')
if 'FXAB' in extra:ab('FXAB','FX0.REG','an indexed packet copied and checked in one pass') # .732: ACCEL_D3D_IXFUSE
if 'TFAB' in extra:ab('TFAB','TF0.REG','depth vertex fog on the engine\'s fog unit') # .733: ACCEL_D3D_TFOG
if 'TRAB' in extra:ab('TRAB','TR0.REG','the set-up without the words the program set-up writes again') # .733: ACCEL_D3D_TRIM
step('5 VXNOSYNC');a('echo 5. VXNOSYNC: full screen, about a minute; the screen must stay white in passes 1-5.')
a('if exist VXNOSYNC.LOG del VXNOSYNC.LOG');a('start /w VXNOSYNC.EXE');a('if exist VXNOSYNC.LOG copy VXNOSYNC.LOG %s\\VXNOSY.LOG >nul'%T);a('if exist VXNOSYNC.LOG del VXNOSYNC.LOG')
step('5 CLXP3D');a('echo    CLXP3D: the cases from T153, under a minute.')
a('if exist CLXP3D1??.BIN del CLXP3D1??.BIN');a('start /w CLXP3D.EXE from 153 nocapture');a('copy CLXP3D.TXT %s\\CLXP.TXT >nul'%T);a('del CLXP3D.TXT')
keep(['CLXP3D1??.BIN'])
if 'KB' in extra: # .744: the T&L kernels timed on this processor (VXKBENCH), a few seconds; the screen is not touched
 step('5k VXKBENCH');a('echo 5k. VXKBENCH: the T&L kernels timed on this processor, a few seconds.')
 a('if exist VXKBENCH.LOG del VXKBENCH.LOG');a('start /w VXKBENCH.EXE');a('if exist VXKBENCH.LOG copy VXKBENCH.LOG %s\\VXKBENCH.LOG >nul'%T);a('if exist VXKBENCH.LOG del VXKBENCH.LOG')
if 'PSP' in extra: # .734: the pixel program reading a coordinate set (bank 3), raw on the lab target: last, as a probe may hang the engine (the VX900 skips it)
 step('6 VXPSP');a('echo 6. VXPSP: the pixel program probe, about 10 seconds; the screen is not touched. Last: a probe may need a restart.')
 a('if exist VXPSP.LOG del VXPSP.LOG');a('start /w VXPSP.EXE');a('if exist VXPSP.LOG copy VXPSP.LOG %s\\VXPSP.LOG >nul'%T);a('if exist VXPSP.LOG del VXPSP.LOG')
if 'PST' in extra: # .746: VXPSP T, the per-pixel cost of an ALU instruction (the VX900 skips it)
 step('6t VXPSP T');a('echo 6t. VXPSP T: the cost of a pixel program instruction, about 10 seconds; the screen is not touched.')
 a('if exist VXPSP.LOG del VXPSP.LOG');a('start /w VXPSP.EXE T');a('if exist VXPSP.LOG copy VXPSP.LOG %s\\VXPSPT.LOG >nul'%T);a('if exist VXPSP.LOG del VXPSP.LOG')
if 'VBAB' in extra or 'VBQ' in extra: # .753: VBQ, the short form (the pictures with the boxes on and checked, pursuit six times)
 # .752: the HAL's vertex buffers and the T&L's cached boxes, set live (VXREG VBUF, VBOX). Last: with VBUF ON DirectDraw asks
 # the HAL to create the applications' vertex buffers, which no build before did; everything above ran as booted (VBUF off).
 def vreg(*cmds):
  for c in cmds:a('start /w VXREG.EXE %s'%c)
 def vtlp(arg,log):
  a('start /w VXTLP.EXE %s'%arg if arg else 'start /w VXTLP.EXE');a('copy VXTLP.LOG %s\\%s >nul'%(T,log));a('del VXTLP.LOG');a('if exist VXTLVBOX.BMP copy VXTLVBOX.BMP %s\\%s.BMP >nul'%(T,log[:-4]));a('if exist VXTLVBOX.BMP del VXTLVBOX.BMP')
 def pur(tag):
  step('7 pursuit %s'%tag)
  a('if exist HB\\results.txt del HB\\results.txt');a('cd HB');a('start /w ..\\VXD3DMON.EXE 120 RUN HarborBench.exe /fullscreen /ram /scene=pursuit');a('cd ..')
  a('if exist HB\\VXD3DMON.TXT copy HB\\VXD3DMON.TXT %s\\VB_%s.TXT >nul'%(T,tag));a('if exist HB\\VXD3DMON.TXT del HB\\VXD3DMON.TXT')
  a('if exist HB\\results.txt copy HB\\results.txt %s\\VBH_%s.TXT >nul'%(T,tag));a('if exist HB\\results.txt del HB\\results.txt');a('if exist HB\\frames.csv del HB\\frames.csv')
 if 'VBQ' in extra:
  a('echo 7. The vertex buffers and the cached boxes, about 15 minutes: VXTLP VBOX twice, then HarborBench pursuit six times')
  a('echo    under VXD3DMON. The screen changes mode when the buffers are switched.')
  a('if exist VXVB.LOG del VXVB.LOG')
  step('7 VBUF ON, VBOX 1: VXTLP VBOX');vreg('VBUF ON','VBOX 1');vtlp('VBOX','VBOX_B1.LOG')
  step('7 VBUF ON, VBOX 2: VXTLP VBOX');vreg('VBOX 2');vtlp('VBOX','VBOX_B2.LOG')
  a('copy HB\\HarborBench.ini HB\\HB_MAIN.INI >nul');a('copy HB\\HB_HW.INI HB\\HarborBench.ini >nul')
  vreg('VBOX 1');pur('B1_1');vreg('VBUF OFF');pur('A_1');vreg('VBUF ON');pur('B1_2');vreg('VBUF OFF');pur('A_2')
  vreg('VBUF ON','VBOX 0');pur('B0_1');vreg('VBOX 2');pur('B2')
  a('copy HB\\HB_MAIN.INI HB\\HarborBench.ini >nul');vreg('VBUF OFF','VBOX 1')
 else:
  a('echo 7. The vertex buffers and the cached boxes, about 30 minutes: VXTLP VBOX five times, VXTLP and GRID with the buffers on,')
  a('echo    then HarborBench pursuit six times under VXD3DMON. The screen changes mode when the buffers are switched.')
  a('echo    If a step stops for over two minutes: restart (the buffers are off again after a restart) and send the folder as it is.')
  a('if exist VXVB.LOG del VXVB.LOG')
  step('7 VBUF OFF: VXTLP VBOX');vreg('VBUF OFF','VBOX 1');vtlp('VBOX','VBOX_A.LOG')
  step('7 VBUF ON, VBOX 1: VXTLP VBOX');vreg('VBUF ON','VBOX 1');vtlp('VBOX','VBOX_B1.LOG')
  step('7 VBUF ON, VBOX 2: VXTLP VBOX');vreg('VBOX 2');vtlp('VBOX','VBOX_B2.LOG')
  step('7 VBUF ON, VBOX 0: VXTLP VBOX');vreg('VBOX 0');vtlp('VBOX','VBOX_B0.LOG')
  step('7 VBUF ON, VBOX 1: VXTLP');vreg('VBOX 1');vtlp('','VBTLP.LOG');clean(BMPS)
  step('7 VBUF ON, VBOX 1: GRID HW');vtlp('GRID HW','VBGRID.LOG');clean(['VX?_??.CSV']+BMPS)
  a('copy HB\\HarborBench.ini HB\\HB_MAIN.INI >nul');a('copy HB\\HB_HW.INI HB\\HarborBench.ini >nul')
  pur('B1_1');vreg('VBOX 0');pur('B0_1');vreg('VBUF OFF','VBOX 1');pur('A_1')
  vreg('VBUF ON');pur('B1_2');vreg('VBUF OFF');pur('A_2');vreg('VBUF ON','VBOX 2');pur('B2')
  a('copy HB\\HB_MAIN.INI HB\\HarborBench.ini >nul')
  step('7 VBUF OFF: VXTLP VBOX again');vreg('VBUF OFF','VBOX 1');vtlp('VBOX','VBOX_A2.LOG')
 a('if exist VXVB.LOG copy VXVB.LOG %s\\VXVB.LOG >nul'%T)
if 'VBOX1' in extra: # .758: the buffers are on at boot: VXTLP VBOX once as booted. .794: the buffers are off at boot, so this is the pictures with the runtime's own buffers
 step('7 VXTLP VBOX');a('echo 7. VXTLP VBOX: vertices rewritten under a cached box, under a minute.')
 a('start /w VXTLP.EXE VBOX');a('copy VXTLP.LOG %s\\VBOX.LOG >nul'%T);a('del VXTLP.LOG');a('if exist VXTLVBOX.BMP copy VXTLVBOX.BMP %s\\VBOX.BMP >nul'%T);a('if exist VXTLVBOX.BMP del VXTLVBOX.BMP')
if 'GLCG' in extra: # .762: the OpenGL ICD built for the Pentium Pro (VXGL.DLL, as installed) and for the standard target (VXGL4.DLL), the file in SYSTEM swapped between runs
 a('echo 9. The OpenGL DLL two ways, HarborBench pursuit in OpenGL four times, about 6 minutes.')
 a('copy HB\\HarborBench.ini HB\\HB_MAIN.INI >nul');a('copy HB\\HB_GL.INI HB\\HarborBench.ini >nul')
 for k,v in enumerate((4,6,4,6)):
  step('9 OpenGL pursuit, VXGL%d.DLL (%d)'%(v,k+1));a('copy /y VXGL%d.DLL %%windir%%\\SYSTEM\\VXGL.DLL >nul'%v);a('fc /b VXGL%d.DLL %%windir%%\\SYSTEM\\VXGL.DLL > %s\\GLCG%d_%d.FC'%(v,T,v,k//2+1))
  a('if exist HB\\results.txt del HB\\results.txt');a('cd HB');a('start /w HarborBench.exe /fullscreen /ram /scene=pursuit');a('cd ..')
  a('if exist HB\\results.txt copy HB\\results.txt %s\\GLCG%d_%d.TXT >nul'%(T,v,k//2+1));a('if exist HB\\results.txt del HB\\results.txt');a('if exist HB\\frames.csv del HB\\frames.csv')
 a('copy /y VXGL4.DLL %windir%\\SYSTEM\\VXGL.DLL >nul');a('copy HB\\HB_MAIN.INI HB\\HarborBench.ini >nul')
if 'IXOSK' in extra: # .768B: IXDIRECT (the VxD, live) and OSK (the HAL, each program's start) both ways, in one order and back again
 # (11 01 10 00 00 10 01 11: each switch's on and off runs are spread alike over the session), VXTLP GRID 6E and 6K through the driver's
 # T&L (the synthetic load), then HarborBench pursuit with no monitor; both on again at the end
 order=[(1,1),(0,1),(1,0),(0,0),(0,0),(1,0),(0,1),(1,1)];oo=lambda v:'ON' if v else 'OFF'
 a('echo 10. IXDIRECT and OSK each on and off: VXTLP GRID 6E and 6K 16 times (20 s each), then HarborBench pursuit 8 times, about 30 minutes.')
 for k,(ix,os_) in enumerate(order):
  for sc in ('6E','6K'):
   step('10 GRID %s HW, IXDIRECT %s, OSK %s (%d)'%(sc,oo(ix),oo(os_),k+1));a('start /w VXREG.EXE IXDIRECT %s'%oo(ix));a('start /w VXREG.EXE OSK %s'%oo(os_))
   a('start /w VXTLP.EXE GRID %s HW'%sc);a('copy VXTLP.LOG %s\\IO%s%d%d_%d.LOG >nul'%(T,sc,ix,os_,k//4+1));a('del VXTLP.LOG');clean(['VX?_??.CSV']+BMPS)
 a('copy HB\\HarborBench.ini HB\\HB_MAIN.INI >nul');a('copy HB\\HB_HW.INI HB\\HarborBench.ini >nul')
 for k,(ix,os_) in enumerate(order):
  step('10 pursuit, no monitor, IXDIRECT %s, OSK %s (%d)'%(oo(ix),oo(os_),k+1));a('start /w VXREG.EXE IXDIRECT %s'%oo(ix));a('start /w VXREG.EXE OSK %s'%oo(os_))
  a('if exist HB\\results.txt del HB\\results.txt');a('cd HB');a('start /w HarborBench.exe /fullscreen /ram /scene=pursuit');a('cd ..')
  a('if exist HB\\results.txt copy HB\\results.txt %s\\IOPUR%d%d_%d.TXT >nul'%(T,ix,os_,k//4+1));a('if exist HB\\results.txt del HB\\results.txt');a('if exist HB\\frames.csv del HB\\frames.csv')
 a('copy HB\\HB_MAIN.INI HB\\HarborBench.ini >nul');a('start /w VXREG.EXE IXDIRECT ON');a('start /w VXREG.EXE OSK ON')
 a('if exist VXVB.LOG copy VXVB.LOG %s\\VXVB.LOG >nul'%T)
if 'GLPROF' in extra: # .768B: the first OpenGL profile with names: HarborBench pursuit in OpenGL under VXD3DMON (VXGL.DLL's lines by function
 # from VXGL.MAP) with the ICD's own sampler and log on (VXGL_LOG, VXGL_SAMPLE: C:\VXGL.LOG, the SSE pass's counts at the context's end)
 a('echo 11. HarborBench pursuit in OpenGL, profiled, about 4 minutes.')
 step('11 OpenGL pursuit profiled');a('regedit /s GLPROF.REG');a('if exist C:\\VXGL.LOG del C:\\VXGL.LOG')
 a('copy HB\\HarborBench.ini HB\\HB_MAIN.INI >nul');a('copy HB\\HB_GL.INI HB\\HarborBench.ini >nul')
 a('if exist HB\\results.txt del HB\\results.txt');a('cd HB');a('start /w ..\\VXD3DMON.EXE 300 RUN HarborBench.exe /fullscreen /ram /scene=pursuit');a('cd ..')
 a('regedit /s GLPROF0.REG');a('copy HB\\HB_MAIN.INI HB\\HarborBench.ini >nul')
 a('if exist HB\\VXD3DMON.TXT copy HB\\VXD3DMON.TXT %s\\GLMON.TXT >nul'%T);a('if exist HB\\VXD3DMON.TXT del HB\\VXD3DMON.TXT')
 a('if exist C:\\VXGL.LOG copy C:\\VXGL.LOG %s\\GLPROF.LOG >nul'%T)
 a('if exist HB\\results.txt copy HB\\results.txt %s\\GLPROF.TXT >nul'%T);a('if exist HB\\results.txt del HB\\results.txt');a('if exist HB\\frames.csv del HB\\frames.csv')
if 'EDID' in extra: # .769: the EDID as read at boot and again now (a bus scan that reads one gives the hidden sizes back), the mode table
 step('12 VXPROBE edid');a('echo 12. VXPROBE edid: the EDID and the mode table, a few seconds.')
 a('if exist VXPROBE.TXT del VXPROBE.TXT');a('start /w VXPROBE.EXE edid');a('if exist VXPROBE.TXT copy VXPROBE.TXT %s\\EDID.TXT >nul'%T)
if 'NOMON' in extra: # .757 / .758: HarborBench pursuit with no monitor, twice (the frame a game sees; the baseline between builds and code generations)
 a('copy HB\\HarborBench.ini HB\\HB_MAIN.INI >nul');a('copy HB\\HB_HW.INI HB\\HarborBench.ini >nul')
 for k in range(2):
  step('8 pursuit, no monitor %d'%(k+1));a('if exist HB\\results.txt del HB\\results.txt');a('cd HB');a('start /w HarborBench.exe /fullscreen /ram /scene=pursuit');a('cd ..')
  a('if exist HB\\results.txt copy HB\\results.txt %s\\NOMON_%d.TXT >nul'%(T,k+1));a('if exist HB\\results.txt del HB\\results.txt');a('if exist HB\\frames.csv del HB\\frames.csv')
 a('copy HB\\HB_MAIN.INI HB\\HarborBench.ini >nul')
a('echo done>> %s\\STEPS.TXT'%T);a('echo Done. Send the folder %s.'%T);a('goto end')
a(':wrongbuild');a('echo The driver that is running is not .%s (see VXREG.LOG). Install CHROME9.INF from this package, restart, and run this again.'%B);a('goto end')
a(':nofiles');a('echo The test programs and .REG files of this package must be in the current folder.');a('goto end')
a(':nodx');a('echo DirectX 8.1 or later is not installed (no D3D8.DLL in the SYSTEM folder): the Direct3D tests and HarborBench would be skipped.');a('echo Install DirectX 9.0c, restart, and run this again.');a('goto end')
a(':end');a('pause')
open(out,'wb').write(('\r\n'.join(L)+'\r\n').encode('latin-1'))
