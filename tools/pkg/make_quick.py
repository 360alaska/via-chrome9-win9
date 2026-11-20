#!/usr/bin/env python3
"""make_quick.py (.704): QUICK.BAT, the targeted test run of one build: the sanity steps every build gets (VXREG's build check, VXTLP's
picture checks, VXNOSYNC, CLXP3D) and then the steps named for what the build changed. ALL.BAT stays in the package for the full run.
usage: make_quick.py BUILD OUT.BAT STEP...   (steps: GRIDVP, GRIDAB, PURAB, HB)"""
import sys
B,out,extra=sys.argv[1],sys.argv[2],sys.argv[3:]
T='T'+B;L=[]
a=L.append
def step(name):a('echo %s>> %s\\STEPS.TXT'%(name,T))
def clean(globs):
 for g in globs:a('if exist %s del %s'%(g,g))
def keep(globs):
 for g in globs:a('if exist %s copy %s %s >nul'%(g,g,T));a('if exist %s del %s'%(g,g))
BMPS=['VXT?_?.BMP','VXT??_?.BMP','VXTL7*.BMP','VXTL1?A.BMP','VXTL1?B.BMP','VXTLC_?.BMP']
a('@echo off')
a('rem Driver .%s: the targeted run (QUICK.BAT), about %d minutes. ALL.BAT in this folder is the full run.'%(B,10+sum({'GRIDVP':5,'GRIDAB':10,'PURAB':8,'HB':25}.get(x,0) for x in extra)))
a('rem Every build: VXREG (the build), VXTLP (the pictures), VXNOSYNC, CLXP3D. This build also: %s.'%(', '.join(extra) or 'nothing more'))
a('rem Install .%s (CHROME9.INF) and restart first. Send the folder %s.'%(B,T))
for f in ['VXREG.EXE','VXTLP.EXE','CLXP3D.EXE','VXNOSYNC.EXE','ALLON.REG']+(['GB0.REG'] if {'GRIDAB','PURAB'}&set(extra) else [])+(['VXD3DMON.EXE','HB\\HarborBench.exe','HB\\HB_HW.INI'] if 'PURAB' in extra or 'HB' in extra else []):
 a('if not exist %s goto nofiles'%f)
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
step('5 VXNOSYNC');a('echo 5. VXNOSYNC: full screen, about a minute; the screen must stay white in passes 1-5.')
a('if exist VXNOSYNC.LOG del VXNOSYNC.LOG');a('start /w VXNOSYNC.EXE');a('if exist VXNOSYNC.LOG copy VXNOSYNC.LOG %s\\VXNOSY.LOG >nul'%T);a('if exist VXNOSYNC.LOG del VXNOSYNC.LOG')
step('5 CLXP3D');a('echo    CLXP3D: the cases from T153, under a minute.')
a('if exist CLXP3D1??.BIN del CLXP3D1??.BIN');a('start /w CLXP3D.EXE from 153 nocapture');a('copy CLXP3D.TXT %s\\CLXP.TXT >nul'%T);a('del CLXP3D.TXT')
keep(['CLXP3D1??.BIN'])
a('echo done>> %s\\STEPS.TXT'%T);a('echo Done. Send the folder %s.'%T);a('goto end')
a(':wrongbuild');a('echo The driver that is running is not .%s (see VXREG.LOG). Install CHROME9.INF from this package, restart, and run this again.'%B);a('goto end')
a(':nofiles');a('echo The test programs and .REG files of this package must be in the current folder.')
a(':end');a('pause')
open(out,'wb').write(('\r\n'.join(L)+'\r\n').encode('latin-1'))
