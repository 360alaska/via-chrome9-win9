#!/usr/bin/env python3
"""make_quick.py (.704): QUICK.BAT, the targeted test run of one build: the sanity steps every build gets (VXREG's build check, VXTLP's
picture checks, VXNOSYNC, CLXP3D) and then the steps named for what the build changed. ALL.BAT stays in the package for the full run.
usage: make_quick.py BUILD OUT.BAT STEP...   (steps: GRIDVP, GRIDAB, PURAB, HB, REUSE, IPAB, TWAB, GLAB, SMP, FEAT, PROJ, RHW)"""
import sys
B,out,extra=sys.argv[1],sys.argv[2],sys.argv[3:]
T='T'+B;L=[]
a=L.append
def step(name):a('echo %s>> %s\\STEPS.TXT'%(name,T))
def clean(globs):
 for g in globs:a('if exist %s del %s'%(g,g))
def keep(globs):
 for g in globs:a('if exist %s copy %s %s >nul'%(g,g,T));a('if exist %s del %s'%(g,g))
BMPS=['VXT?_?.BMP','VXT??_?.BMP','VXTL7*.BMP','VXTL1?A.BMP','VXTL1?B.BMP','VXTLC_?.BMP','6M*.BMP','6P*.BMP','6N*.BMP','6Q*.BMP','6W*.BMP'] # .717: 6M's pictures
a('@echo off')
a('rem Driver .%s: the targeted run (QUICK.BAT), about %d minutes. ALL.BAT in this folder is the full run.'%(B,10+sum({'GRIDVP':5,'GRIDAB':10,'PURAB':8,'HB':25,'REUSE':1,'IPAB':22,'TWAB':22,'GLAB':40,'SMP':35,'FEAT':12,'PROJ':4,'RHW':6}.get(x,0) for x in extra)))
a('rem Every build: VXREG (the build), VXTLP (the pictures), VXNOSYNC, CLXP3D. This build also: %s.'%(', '.join(extra) or 'nothing more'))
a('rem Install .%s (CHROME9.INF) and restart first. Send the folder %s.'%(B,T))
for f in ['VXREG.EXE','VXTLP.EXE','CLXP3D.EXE','VXNOSYNC.EXE','ALLON.REG']+(['VXREUSE.EXE'] if 'REUSE' in extra else [])+(['VXRHW.EXE','VXDX7.EXE','VXD3DMON.EXE'] if 'RHW' in extra else [])+(['GB0.REG'] if {'GRIDAB','PURAB'}&set(extra) else [])+(['IP0.REG'] if 'IPAB' in extra else [])+(['TW0.REG'] if 'TWAB' in extra else [])+(['GL0.REG','HB\\HarborBench.exe','HB\\HarborBench.ini'] if 'GLAB' in extra else [])+(['HB\\HarborBench.exe','HB\\HB_HW.INI'] if {'IPAB','TWAB'}&set(extra) else [])+(['VXD3DMON.EXE','HB\\HarborBench.exe','HB\\HB_HW.INI'] if 'PURAB' in extra or 'HB' in extra else [])+(['VXD3DMON.EXE','HB\\HarborBench.exe','HB\\HarborBench.ini'] if 'SMP' in extra else [])+(['VXCLIP.EXE'] if 'FEAT' in extra else [])+(['VXD3DMON.EXE'] if 'PROJ' in extra else []):
 a('if not exist %s goto nofiles'%f)
a('if not exist %windir%\\SYSTEM\\D3D8.DLL goto nodx') # .715: a fresh Windows 98 (DirectX 6.1) skips every Direct3D 7/8 test and HarborBench scene
a('start /w VXREG.EXE');a('find "build: .%s" VXREG.LOG >nul'%B);a('if errorlevel 1 goto wrongbuild')
a('if not exist %s\\nul md %s'%(T,T));a('if exist %s\\STEPS.TXT del %s\\STEPS.TXT'%(T,T));a('regedit /s ALLON.REG')
a('copy VXREG.LOG %s\\REG.LOG >nul'%T)
clean(BMPS)
step('1 VXTLP');a('echo 1. VXTLP: the picture checks, 2-3 minutes.');a('start /w VXTLP.EXE')
a('copy VXTLP.LOG %s\\VXTLP.LOG >nul'%T);a('del VXTLP.LOG');keep(BMPS)
if 'GRIDVP' in extra:
 step('2 GRID VP');a('echo 2. VXTLP GRID VP: every GRID scene through a viewport at (40, 40) on both devices (the guard band across a clip edge that is not 0), about 5 minutes.')
 a('start /w VXTLP.EXE GRID VP');a('copy VXTLP.LOG %s\\GRIDVP.LOG >nul'%T);a('del VXTLP.LOG');clean(['VX?_??.CSV']);keep(['VXT?_?.BMP','VXT??_?.BMP','VXTLC_?.BMP'])
if 'GRIDAB' in extra:
 for k,reg in enumerate(['','GB0.REG','','GB0.REG']):
  step('3 GRID HW %d%s'%(k+1,' '+reg if reg else ''))
  if reg:a('regedit /s %s'%reg)
  a('start /w VXTLP.EXE GRID HW');a('copy VXTLP.LOG %s\\GRIDAB_%d.LOG >nul'%(T,k+1));a('del VXTLP.LOG')
  if reg:a('regedit /s ALLON.REG')
  clean(['VX?_??.CSV']+BMPS)
if 'PURAB' in extra:
 a('copy HB\\HarborBench.ini HB\\HB_MAIN.INI >nul');a('copy HB\\HB_HW.INI HB\\HarborBench.ini >nul')
 for tag,reg in [('PUR',''),('PURG','GB0.REG')]:
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
if 'SMP' in extra: # .714: the sampler under load, three times (the VX800 freeze: VXD3DMON SAMPLE with HarborBench's full run); STEPS.TXT
 # names the run that was going when a machine stops
 for k in (1,2,3):
  step('4s sampler run %d: VXD3DMON 600 SAMPLE, HarborBench (all scenes)'%k)
  a('if exist HB\\results.txt del HB\\results.txt');a('cd HB');a('start /w ..\\VXD3DMON.EXE 600 SAMPLE RUN HarborBench.exe /fullscreen /ram');a('cd ..')
  a('if exist HB\\VXD3DMON.TXT copy HB\\VXD3DMON.TXT %s\\SMP_%d.TXT >nul'%(T,k));a('if exist HB\\VXD3DMON.TXT del HB\\VXD3DMON.TXT')
  a('if exist HB\\results.txt copy HB\\results.txt %s\\HBS_%d.TXT >nul'%(T,k));a('if exist HB\\results.txt del HB\\results.txt');a('if exist HB\\frames.csv del HB\\frames.csv')
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
step('5 VXNOSYNC');a('echo 5. VXNOSYNC: full screen, about a minute; the screen must stay white in passes 1-5.')
a('if exist VXNOSYNC.LOG del VXNOSYNC.LOG');a('start /w VXNOSYNC.EXE');a('if exist VXNOSYNC.LOG copy VXNOSYNC.LOG %s\\VXNOSY.LOG >nul'%T);a('if exist VXNOSYNC.LOG del VXNOSYNC.LOG')
step('5 CLXP3D');a('echo    CLXP3D: the cases from T153, under a minute.')
a('if exist CLXP3D1??.BIN del CLXP3D1??.BIN');a('start /w CLXP3D.EXE from 153 nocapture');a('copy CLXP3D.TXT %s\\CLXP.TXT >nul'%T);a('del CLXP3D.TXT')
keep(['CLXP3D1??.BIN'])
a('echo done>> %s\\STEPS.TXT'%T);a('echo Done. Send the folder %s.'%T);a('goto end')
a(':wrongbuild');a('echo The driver that is running is not .%s (see VXREG.LOG). Install CHROME9.INF from this package, restart, and run this again.'%B);a('goto end')
a(':nofiles');a('echo The test programs and .REG files of this package must be in the current folder.')
a(':nodx');a('echo DirectX 8.1 or later is not installed (no D3D8.DLL in the SYSTEM folder): the Direct3D tests and HarborBench would be skipped.');a('echo Install DirectX 9.0c, restart, and run this again.');a('goto end')
a(':end');a('pause')
open(out,'wb').write(('\r\n'.join(L)+'\r\n').encode('latin-1'))
