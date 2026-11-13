#!/usr/bin/env python3
import os,pathlib,subprocess,sys
root=pathlib.Path(__file__).resolve().parent.parent
wc=pathlib.Path(sys.argv[1]).resolve()
env=os.environ.copy();env['WATCOM']=str(wc);env['PATH']=str(wc/'binl64')+os.pathsep+str(wc/'binl')+os.pathsep+env['PATH']
inc=['-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-I'+str(root/'gl'/'mesa'/'include')]
b=root/'build';b.mkdir(exist_ok=True)
subprocess.run(['wcc386','-q','-bt=nt','-6s','-sg','-s','-fp6','-ox']+inc+['-fo=vxfeat.obj',str(root/'tools'/'vxfeat.c')],cwd=b,env=env,check=True)
(b/'vxfeat.lnk').write_text('system nt_win\noption quiet\noption stack=1m\nname VXFEAT.EXE\nfile vxfeat.obj\nlibrary opengl32,gdi32,user32,kernel32\n')
subprocess.run(['wlink','@vxfeat.lnk'],cwd=b,env=env,check=True)
(root/'driver'/'VXFEAT.EXE').write_bytes((b/'VXFEAT.EXE').read_bytes())
