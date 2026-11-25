import re
#!/usr/bin/env python3
"""gtf_cases.py (E782): the VxD's GTF timings in whole numbers (src/vxd_gtf.c gtf_calc) against the timings the floating-point
formula it replaced gave for twenty modes (taken from that code on the host before it was deleted; the pixel clock within 1 part
in a million), the refresh rate's bounds (0 means 60, at most 240), and no float or double left in the function.
usage: gtf_cases.py [root]"""
import sys,subprocess,tempfile,os
from pathlib import Path
ROOT=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parent.parent
REF=[
 (640,480,60,800,656,720,497,481,484,23856000),
 (640,480,75,816,664,728,502,481,484,30722400),
 (800,600,60,1024,832,912,622,601,604,38215680),
 (800,600,85,1056,840,928,630,601,604,56548796),
 (1024,768,60,1344,1080,1184,795,769,772,64108796),
 (1024,768,75,1360,1080,1192,802,769,772,81804000),
 (1152,864,75,1552,1224,1352,902,865,868,104992808),
 (1280,720,60,1664,1336,1472,746,721,724,74480640),
 (1280,800,60,1680,1344,1480,828,801,804,83462400),
 (1280,1024,60,1712,1360,1496,1060,1025,1028,108883200),
 (1366,768,60,1800,1440,1584,795,769,772,85860000),
 (1440,900,60,1904,1520,1672,932,901,904,106471672),
 (1600,1200,60,2160,1704,1880,1242,1201,1204,160963200),
 (1680,1050,60,2256,1784,1968,1087,1051,1054,147136320),
 (1920,1080,60,2576,2040,2248,1118,1081,1084,172798064),
 (1920,1200,60,2592,2048,2256,1242,1201,1204,193155840),
 (848,480,60,1056,864,952,497,481,484,31489922),
 (1280,960,85,1744,1376,1512,1008,961,964,149425920),
 (320,200,70,352,304,336,209,201,204,5149760),
 (1400,1050,75,1896,1496,1648,1096,1051,1054,155851200),
]
src=(ROOT/'src'/'vxd_gtf.c').read_text(encoding='latin-1')
fn=src[src.index('void gtf_calc('):]
assert 'float' not in fn and 'double' not in fn,'floating point in gtf_calc'
pre="""#include <string.h>
#include <stdio.h>
typedef unsigned DWORD;typedef unsigned short uint16;typedef unsigned char uint8;typedef unsigned uint32;
#pragma pack(push,1)
typedef struct vesa_crtc_info{uint16 HorizontalTotal,HorizontalSyncStart,HorizontalSyncEnd,VerticalTotal,VerticalSyncStart,VerticalSyncEnd;uint8 Flags;uint32 PixelClock;uint16 RefreshRate;uint8 Reserved[40];}vesa_crtc_info_t;
#pragma pack(pop)
#define VESA_CRTC_FLAG_HSYC_NEG 4
#define VESA_CRTC_FLAG_VSYC_POS 0
"""
main="""
int main(void){unsigned w,h,f;vesa_crtc_info_t i;while(scanf("%u %u %u",&w,&h,&f)==3){gtf_calc(w,h,f,&i);printf("%u %u %u %u %u %u %u %u\\n",i.HorizontalTotal,i.HorizontalSyncStart,i.HorizontalSyncEnd,i.VerticalTotal,i.VerticalSyncStart,i.VerticalSyncEnd,i.PixelClock,i.RefreshRate);}return 0;}
"""
d=tempfile.mkdtemp();c=os.path.join(d,'g.c');e=os.path.join(d,'g');open(c,'w').write(pre+fn+main)
subprocess.run(['gcc','-O1','-w','-o',e,c],check=True)
q=[r[:3] for r in REF]+[(1024,768,0),(1024,768,999),(1024,768,60),(1024,768,240)]
out=subprocess.run([e],input=''.join('%u %u %u\n'%r for r in q),capture_output=True,text=True,check=True).stdout.split('\n')
bad=0
for r,l in zip(REF,out):
 g=[int(x) for x in l.split()]
 if tuple(g[:6])!=r[3:9] or abs(g[6]-r[9])*1000000>r[9] or g[7]!=r[2]*100:print('WRONG',r,g);bad+=1
z=[[int(x) for x in out[len(REF)+k].split()] for k in range(4)]
if z[0]!=z[2] or z[0][7]!=6000:print('WRONG: 0 Hz is not 60 Hz',z[0],z[2]);bad+=1
if z[1]!=z[3] or z[1][7]!=24000:print('WRONG: 999 Hz is not held to 240',z[1],z[3]);bad+=1
# E784: the standard (DMT) timings the test rate uses: each row gives its own rate, and its sync lies inside the blanking, on whole characters
v=(ROOT/'src'/'vxd_vesa.c').read_text(encoding='latin-1');blk=v[v.index('vx_dmt_r[] = {')+14:];blk=blk[:blk.index('};')];rows=0
for m in re.finditer(r'\{([^{}]+)\}',blk):
 x=[int(t.strip().replace('UL','')) for t in m.group(1).split(',')];w,h,hz,ht,hss,hse,vt,vss,vse,khz=x[:10];rows+=1
 if not(w<=hss<hse<ht and h<=vss<vse<vt and abs(khz*1000.0/(ht*vt)-hz)<0.9 and ht%8==0 and hss%8==0 and hse%8==0):print('WRONG standard timing',x);bad+=1
if rows<18:print('WRONG: standard timings not found');bad+=1
print('gtf: %d modes against the float formula\'s timings, the rate\'s bounds: %d wrong'%(len(REF),bad))
sys.exit(1 if bad else 0)
