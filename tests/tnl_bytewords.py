#!/usr/bin/env python3
"""tnl_bytewords.py (.707): the T&L's four-places-a-word helpers (src/vx_d3d_tnl.inc: tn_w4, tn_hasb, tn_nz4, tn_orand), taken from the
source as they are, against byte-by-byte answers: every word made of 0, 1, 0x7E, 0x7F, 0x80, 0xFF, 0x3F, 2, then 5 million random words,
and tn_orand over random runs of 0-37 bytes at every alignment. Built for the host (64-bit unsigned long) and for i686 (32-bit, as the
board's Watcom build), so a product that overflows only one of them shows."""
import os,re,subprocess,sys,tempfile
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src=open(os.path.join(ROOT,'src','vx_d3d_tnl.inc'),encoding='latin-1').read()
i=src.index('#ifdef __WATCOMC__\nstatic uint32_t tn_w4(');j=src.index('\n}\n',src.index('static void tn_orand('))+3 # .708: from the #ifdef (the board's one-load form under Watcom; memcpy here)
C=r'''#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
'''+src[i:j]+r'''
static uint32_t st=12345;static uint32_t rnd(void){st^=st<<13;st^=st>>17;st^=st<<5;return st;}
int main(void){unsigned long bad=0,i;static const uint8_t v[]={0,1,0x7E,0x7F,0x80,0xFF,0x3F,2};uint8_t buf[64];
 for(i=0;i<4096+5000000;i++){uint8_t b[4];uint32_t w;int h=0,n=0,k;
  if(i<4096){b[0]=v[i&7];b[1]=v[(i>>3)&7];b[2]=v[(i>>6)&7];b[3]=v[(i>>9)&7];}else{w=rnd();memcpy(b,&w,4);}
  w=tn_w4(b);for(k=0;k<4;k++){h|=b[k]==0x7F;n+=b[k]!=0;}
  if(tn_hasb(w,0x7F7F7F7FUL)!=h||tn_nz4(w)!=(uint32_t)n)bad++;}
 for(i=0;i<200000;i++){uint32_t n=rnd()%38,off=rnd()%8,k,o=0,a=0xFF,po,pa;
  for(k=0;k<n+8;k++)buf[off+k]=(rnd()&3)?(uint8_t)(rnd()&0x7F):v[rnd()&7];
  for(k=0;k<n;k++){o|=buf[off+k];a&=buf[off+k];}
  tn_orand(buf+off,n,&po,&pa);if(po!=o||pa!=a)bad++;}
 printf("byte words: %lu wrong\n",bad);return bad!=0;}
'''
rc=0
with tempfile.TemporaryDirectory() as d:
 c=os.path.join(d,'t.c');open(c,'w').write(C)
 for flags,name in ((['-m64'],'host'),(['-m32'],'i686')):
  exe=os.path.join(d,'t_'+name)
  r=subprocess.run(['gcc','-std=c99','-O2',*flags,c,'-o',exe],capture_output=True,text=True)
  if r.returncode:
   if name=='i686':print('i686 build not available here:',r.stderr.strip().splitlines()[-1:]);continue
   print(r.stderr);sys.exit(1)
  out=subprocess.run([exe],capture_output=True,text=True);print(name+':',out.stdout.strip());rc|=out.returncode
print('PASS' if rc==0 else 'FAIL');sys.exit(rc)
