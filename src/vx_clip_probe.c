/* VXCLIP v1.1 (driver .681: every form writes its own clip-to-screen transform and clamp, the driver's by default; O-R). VXCLIP v1.0 (driver .680): does the VX855's engine clip? A probe of the guard band, the clipping engine and the clip-to-screen
 * transform, through the 3D lab's raw path (VX_IOCTL_3D_TEST, 'RAW3': the lab's fixed set-up, then this tool's words) on the 64 x 64
 * test target. Today the driver keeps every vertex inside the screen: a triangle crossing a screen edge is clipped by the CPU (the
 * driver's screen clipper: 27% of GRID's triangles; GTA3 clips 59% of its draws). VIA's manual (VX900 Part II, the same Chrome9 family)
 * describes 01/01 bit 11 Enable Clipping Engine, 01/88-89 the guard band window (left / right, s12), 04/40-48 the clip-to-screen
 * transform and 04/50-52 a screen-coordinate clamp. The driver writes 01/88 = 0, 01/89 = the target's width (a guard band no wider
 * than the screen), 04/40-48 scale 1 and offset 786432.0 (XP's) and 04/52 = 147FFF (no clamp); the lab writes 04/52 = 0B45C5. Each form draws one solid orange triangle (01/86-87, 04/23 = 2) and is compared with
 * the CPU's coverage of the same triangle clipped to the target (pixel (x, y) covered at its corner (x, y), top-left rule: the lab's
 * control triangle (8,8) (56,8) (8,56) is 1,176 pixels on the board, which is that rule).
 *   A  the control, inside the target
 *   B  one vertex at x = -24, the lab's state (as the driver draws today)
 *   C  B with the guard band window widened to -1024 .. +1088 (01/88, 01/89)
 *   D  C with the clipping engine on (01/01 bit 11)
 *   E  B with the clipping engine alone
 *   F  a triangle covering the whole target from (-1000,-1000) (3000,-1000) (-1000,3000), C's state: 4,096 pixels expected
 *   G  F with the clipping engine on
 *   H  a thin triangle from x = -1500 (inside the +/-2048 clamp), C's state
 *   I  a thin triangle from x = -5000, C's state with the driver's 04/52 = 147FFF (no clamp)
 *   J  I with the lab's clamp (04/52 = 0B45C5, +/-2048): where the clamp acts (before or after the transform's offset)
 *   K  clip-space vertices with w = 2 through the clip-to-screen transform (x = 32 x + 32, y = -32 y + 32), 01/01 bit 20 (the old lab's
 *      C2S flag): the control's triangle if the engine divides by w
 *   L  K with the clipping engine on too
 *   M  L with one vertex beyond the left plane (x = -3 w): the engine clips it to the screen?
 *   N  L with one vertex behind the eye (w < 0): the engine clips at the near plane?
 *   O  K without 01/01 bit 20;  P  L with the transform's offset 32 alone (no 786432.0)
 *   Q  A, R  C, with the October lab's screen transform (scale 1, offset 0): is the driver's 786432.0 offset what lets x < 0 through?
 * A form whose words hang the engine is reset by the lab (the next form is sent with the reset); the log says which. Run it last in a
 * boot (PROBE.BAT). VXCLIP.LOG beside the program. VXCLIP <letters> runs only those forms. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "vx85598.h"
#include "vx855_3d.h"
static FILE *lf;
static void say(const char *f,...) {static char b[2000];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);fputs(b,stdout);if(lf){fputs(b,lf);fflush(lf);}}
static HANDLE dev;static vx3d_request rq;static vx3d_report rp;static DWORD nw;
static void P(DWORD port,DWORD v) {if(nw+2<=VX3D_LAB_DATA){rq.lab_data[nw++]=port;rq.lab_data[nw++]=v;}}
static void T(DWORD t) {P(0x43C,t<<16);}
static void R(DWORD a,DWORD v) {P(0x440,(a<<24)|(v&0xFFFFFFUL));}
static void D(DWORD w) {P(0x440,w);}
static void IDLE(void) {P(0xFFFFFFFFUL,0);}
static DWORD fb(float f) {DWORD b;memcpy(&b,&f,4);return b;}
/* a float register split as the manual says: the low 24 bits in lo, the high byte in byte 'hb' of hi (whose other bytes are 'rest') */
static void C2S(float sx,float ox,float sy,float oy) {
 DWORD a=fb(sx),b=fb(ox),c=fb(sy),d=fb(oy);
 T(4);R(0x40,a&0xFFFFFFUL);R(0x41,b&0xFFFFFFUL);R(0x42,((b>>24)<<8)|(a>>24));
 R(0x43,c&0xFFFFFFUL);R(0x44,d&0xFFFFFFUL);R(0x45,((d>>24)<<8)|(c>>24));
 R(0x46,0x800000UL);R(0x47,0);R(0x48,0x92003FUL); /* z: scale 1, offset 0 (the lab's) */
}
typedef struct {float x,y,z,w;} V4;
/* the form's state: guard band (0 lab's), clipping engine, C2S (1: clip space with 01/01 bit 20, 2: clip space without it, 3: as 1
 * without the 786432 offset, 4: screen space with the October lab's identity, offset 0), clamp word (0 = the driver's 147FFF). Every form writes 04/40-48 and 04/50-52 itself: the driver's own
 * transform (scale 1, offset 786432.0: XP's, the snap to sixteenths) for screen-space vertices, so a form differs from the driver's
 * drawing only in what it tests (the lab's set-up leaves 04/40-48 as the last packet left them and writes the clamp 0B45C5) */
static void draw(const V4 *v,int gb,int ce,int c2s,DWORD clamp) {
 int k;
 T(1);R(0x01,0x40UL|(ce?0x800UL:0)|((c2s==1||c2s==3)?0x100000UL:0));
 if(gb){R(0x88,(DWORD)(-1024)&0x1FFFUL);R(0x89,1088);}
 if(c2s==4)C2S(1.0f,0.0f,1.0f,0.0f);else if(c2s)C2S(32.0f,c2s==3?32.0f:786464.0f,-32.0f,c2s==3?32.0f:786464.0f);else C2S(1.0f,786432.0f,1.0f,786432.0f);
 T(4);R(0x50,0);R(0x51,0);R(0x52,clamp?clamp:0x147FFFUL);
 R(0x23,2);R(0x24,3);T(0);
 for(k=0;k<3;k++){D(fb(v[k].x));D(fb(v[k].y));D(fb(v[k].z));D(fb(v[k].w));D(0xFFE04020UL);}
 IDLE();
 T(1);R(0x01,0x40);if(gb){R(0x88,0);R(0x89,64);} /* the lab's state back */
}
/* ---- the CPU's reference: clip in clip space (w > 1e-5, z >= 0, and the four sides when asked), project, rasterize ---- */
static int clip_plane(V4 *in,int n,V4 *out,int pl) {
 int i,m=0;
 for(i=0;i<n;i++){V4 a=in[i],b=in[(i+1)%n];float da,db;
  #define DIST(p) (pl==0?(p).w-1e-5f:pl==1?(p).z:pl==2?(p).w+(p).x:pl==3?(p).w-(p).x:pl==4?(p).w+(p).y:(p).w-(p).y)
  da=DIST(a);db=DIST(b);
  if(da>=0)out[m++]=a;
  if((da>=0)!=(db>=0)){float t=da/(da-db);V4 c;c.x=a.x+t*(b.x-a.x);c.y=a.y+t*(b.y-a.y);c.z=a.z+t*(b.z-a.z);c.w=a.w+t*(b.w-a.w);out[m++]=c;}
  #undef DIST
 }
 return m;
}
static unsigned char ref[64*64];
static void raster(float ax,float ay,float bx,float by,float cx,float cy) {
 int x,y;double ar=(bx-ax)*(double)(cy-ay)-(by-ay)*(double)(cx-ax);
 if(ar==0)return;
 for(y=0;y<64;y++)for(x=0;x<64;x++){
  double e0=(bx-ax)*(double)(y-ay)-(by-ay)*(double)(x-ax),e1=(cx-bx)*(double)(y-by)-(cy-by)*(double)(x-bx),e2=(ax-cx)*(double)(y-cy)-(ay-cy)*(double)(x-cx);
  if(ar<0){e0=-e0;e1=-e1;e2=-e2;}
  /* top-left: an edge whose interior is below it (dy == 0, going the interior's way) or to its right is inclusive */
  {int in=1;double ex[3],ey[3];int k;ex[0]=bx-ax;ey[0]=by-ay;ex[1]=cx-bx;ey[1]=cy-by;ex[2]=ax-cx;ey[2]=ay-cy;
   for(k=0;k<3;k++){double e=k==0?e0:k==1?e1:e2,dx=ar<0?-ex[k]:ex[k],dy=ar<0?-ey[k]:ey[k];int tl=(dy==0&&dx>0)||dy<0;
    if(e<0||(e==0&&!tl))in=0;}
   if(in)ref[y*64+x]=1;}}
}
static int reference(const V4 *v,int clipspace,int sides) {
 V4 a[16],b[16];int n=3,k,p,c=0;memcpy(a,v,3*sizeof(V4));memset(ref,0,sizeof(ref));
 if(clipspace){for(p=0;p<(sides?6:2);p++){n=clip_plane(a,n,b,p);memcpy(a,b,n*sizeof(V4));if(n<3)return 0;}
  for(k=0;k<n;k++){a[k].x=32.0f*a[k].x/a[k].w+32.0f;a[k].y=-32.0f*a[k].y/a[k].w+32.0f;}}
 for(k=1;k+1<n;k++)raster(a[0].x,a[0].y,a[k].x,a[k].y,a[k+1].x,a[k+1].y);
 for(k=0;k<64*64;k++)c+=ref[k];
 return c;
}
static const char *res(DWORD r) {static const char *const n[]={"not run","ran","NOT READY (use 800 x 600 x 16)","bad register window","no register mapping","BUSY before the test","TIMEOUT"};return r<7?n[r]:"failed";}
static int recover;
static int form(int f) {
 static const V4 ctl[3]={{8,8,.5f,1},{56,8,.5f,1},{8,56,.5f,1}},left[3]={{-24,8,.5f,1},{56,8,.5f,1},{8,56,.5f,1}},
  big[3]={{-1000,-1000,.5f,1},{3000,-1000,.5f,1},{-1000,3000,.5f,1}},thin1[3]={{-1500,20,.5f,1},{60,10,.5f,1},{60,40,.5f,1}},
  thin2[3]={{-5000,20,.5f,1},{60,10,.5f,1},{60,40,.5f,1}},
  cs[3]={{-1.5f,1.5f,1,2},{1.5f,1.5f,1,2},{-1.5f,-1.5f,1,2}},side[3]={{-6,1.5f,1,2},{1.5f,1.5f,1,2},{-1.5f,-1.5f,1,2}},
  eye[3]={{-1.5f,1.5f,1,2},{1.5f,1.5f,1,2},{.5f,-.5f,-.6f,-1}};
 const V4 *v=ctl;int cl=0,sides=0,expect,x,y;DWORD ch=0,orange=0,ok,got=0,diff=0;
 const char *what="";
 memset(&rq,0,sizeof(rq));rq.cb=sizeof(rq);rq.version=VX3D_VERSION;rq.arm=VX3D_ARM;rq.probe=1;rq.lab_cmd=0x33574152UL;rq.reserved=recover?0x100UL:0;nw=0;
 switch(f){
  case 'A':what="the control, inside the target";draw(ctl,0,0,0,0);break;
  case 'B':what="one vertex at x = -24, the lab's state (the driver's today)";v=left;draw(left,0,0,0,0);break;
  case 'C':what="B with the guard band window -1024 .. +1088 (01/88, 01/89)";v=left;draw(left,1,0,0,0);break;
  case 'D':what="C with the clipping engine on (01/01 bit 11)";v=left;draw(left,1,1,0,0);break;
  case 'E':what="B with the clipping engine alone";v=left;draw(left,0,1,0,0);break;
  case 'F':what="the whole target from +/-1000, C's state";v=big;draw(big,1,0,0,0);break;
  case 'G':what="F with the clipping engine on";v=big;draw(big,1,1,0,0);break;
  case 'H':what="a thin triangle from x = -1500 (inside the +/-2048 clamp), C's state";v=thin1;draw(thin1,1,0,0,0);break;
  case 'I':what="a thin triangle from x = -5000, the driver's 04/52 = 147FFF (no clamp)";v=thin2;draw(thin2,1,0,0,0x147FFFUL);break;
  case 'J':what="I with the lab's clamp 04/52 = 0B45C5 (+/-2048: after the 786432.0 offset every coordinate is past it)";v=thin2;draw(thin2,1,0,0,0x0B45C5UL);break;
  case 'K':what="clip-space vertices, w = 2, through the clip-to-screen transform (01/01 bit 20): the control's triangle if the engine divides";v=cs;cl=1;draw(cs,1,0,1,0);break;
  case 'L':what="K with the clipping engine on";v=cs;cl=1;draw(cs,1,1,1,0);break;
  case 'M':what="L with a vertex past the left plane (x = -3 w)";v=side;cl=1;sides=1;draw(side,1,1,1,0);break;
  case 'N':what="L with a vertex behind the eye (w = -1)";v=eye;cl=1;draw(eye,1,1,1,0);break;
  case 'O':what="K without 01/01 bit 20 (clip-space vertices, the transform alone)";v=cs;cl=1;draw(cs,1,0,2,0);break;
  case 'P':what="L with the transform's offset 32 alone (no 786432.0)";v=cs;cl=1;draw(cs,1,1,3,0);break;
  case 'Q':what="A with the October lab's transform (scale 1, offset 0: no 786432.0)";draw(ctl,0,0,4,0);break;
  case 'R':what="C with the October lab's transform (offset 0)";v=left;draw(left,1,0,4,0);break;
  default:return 0;}
 rq.lab_ndata=nw;
 expect=reference(v,cl,sides||f=='N');
 memset(&rp,0,sizeof(rp));ok=DeviceIoControl(dev,VX_IOCTL_3D_TEST,&rq,sizeof(rq),&rp,sizeof(rp),&got,NULL);
 if(ok&&got==sizeof(rp)&&!recover&&rp.fault_latched&&rp.stage==0&&rp.writes==0){rq.reserved=0x100UL;memset(&rp,0,sizeof(rp));got=0;ok=DeviceIoControl(dev,VX_IOCTL_3D_TEST,&rq,sizeof(rq),&rp,sizeof(rp),&got,NULL);say("\n(the engine's fault was latched from the form before: this one sent again with the reset)");}
 say("\n%c  %s: %lu words\n",f,what,nw/2);
 if(!ok||got!=sizeof(rp)){say("   the VxD refused the call (error %lu)\n",GetLastError());return 0;}
 say("   %s; stage %lu (4 = idle after every word); status after the words %08lX; fault latched %lu\n",res(rp.result),rp.stage,rp.status_solid,rp.fault_latched);
 if(rp.vertex_words!=0xFFFFFFFFUL)say("   a wait failed at word %lu (reason %lu, status %08lX): THE ENGINE HUNG ON THIS FORM\n",rp.vertex_words,rp.wait_failure_reason,rp.wait_failure_status);
 recover=rp.fault_latched!=0;
 if(rp.result==2||rp.stage<2)return 0;
 for(y=0;y<64;y++)for(x=0;x<64;x++){DWORD c=rp.image[y*64+x];int lit=c!=VX3D_BACKGROUND;if(lit){ch++;if((c&0xFFFFFFUL)==0xE04020UL)orange++;}if(lit!=(ref[y*64+x]!=0))diff++;}
 say("   %lu pixels drawn (%lu orange); the CPU's clipped triangle: %d pixels; %lu pixels differ\n",ch,orange,expect,diff);
 say("   picture, every second pixel (# drawn and expected, + drawn only, o expected only, . neither):\n");
 for(y=0;y<64;y+=2){char row[40];for(x=0;x<64;x+=2){int a=rp.image[y*64+x]!=VX3D_BACKGROUND,b=ref[y*64+x]!=0;row[x/2]=a&&b?'#':a?'+':b?'o':'.';}row[32]=0;say("      %s\n",row);}
 {int good=diff<=(DWORD)(expect/50+8)&&!rp.fault_latched&&rp.vertex_words==0xFFFFFFFFUL;
  say(good?"   -> AS THE CPU CLIPS IT\n":"   -> NOT the CPU's picture\n");return good;}
}
int main(int argc,char **argv) {
 char path[MAX_PATH],*s;const char *forms=argc>1?argv[1]:"ABCDEFGHIJKLMNOPQR";int k;static char good[32];DWORD in=0x75,o[8],got=0;HANDLE t;
 GetModuleFileNameA(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXCLIP.LOG");lf=fopen(path,"w");
 say("VXCLIP v1.1 (driver .681): the guard band, the clipping engine and the clip-to-screen transform on the 64 x 64 lab target\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE){say("FAILED: the VxD is not open\n");if(lf)fclose(lf);return 2;}
 t=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);memset(o,0,sizeof(o));
 if(t!=INVALID_HANDLE_VALUE){DeviceIoControl(t,0x85500022UL,&in,4,o,32,&got,0);CloseHandle(t);}
 say("the VxD's build: .%lu; the device 1106:%04lX\n",o[1],o[2]);
 if(o[2]==0x7122){say("the VX900 has other words for this (its vertex program): not run\n");if(lf)fclose(lf);return 0;}
 for(k=0;forms[k];k++){int f=forms[k]>='a'&&forms[k]<='z'?forms[k]-32:forms[k];if(f>='A'&&f<='R')good[f-'A']=(char)form(f);}
 say("\nSUMMARY:");for(k=0;forms[k];k++){int f=forms[k]>='a'&&forms[k]<='z'?forms[k]-32:forms[k];if(f>='A'&&f<='R')say(" %c %s;",f,good[f-'A']?"as the CPU":"differs");}say("\n");
 if(!good[0])say("the control differs: the reference's rule is wrong, judge the pictures by eye\n");
 /* the DMA question: VIA's older chips have a PCI DMA blit engine with its registers at BAR1 0xE00-0xEFF (OpenChrome's via_dmablit).
  * A read-only dump (VXPROBE sub 7); no register is written. */
 {DWORD q[2],r[64],g=0,at;say("\nengine registers 0xE00-0xEFF (read only; the PCI DMA engine's place on VIA's older chips):\n");
  for(at=0xE00;at<0xF00;at+=62*4){int k;q[0]=7;q[1]=at;memset(r,0,sizeof(r));
   if(!DeviceIoControl(dev,VX_IOCTL_PROBE,q,8,r,256,&g,NULL)||r[1]!=7){say("   the probe call failed (driver older than .680?)\n");break;}
   for(k=0;k<62&&at+k*4<0xF00;k++){if(!(k&7))say("   %03lX:",at+k*4);say(" %08lX",r[2+k]);if((k&7)==7)say("\n");}say("\n");}}
 if(lf)fclose(lf);CloseHandle(dev);return 0;
}
