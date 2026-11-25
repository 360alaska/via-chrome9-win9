/* VXPSP v1.4 (driver .746): T, the per-pixel cost of an ALU instruction (below). VXPSP v1.3 (driver .740; T739B: F-I right on the VX855 and VX800): J, the first step of per-pixel lighting (the speed list's item E):
 * DP3 oC0, t0, c0 with c0 = (0.6, 0.8, 0, 0) in the constant bank (block 22030000), so each pixel's colour is 0.6 u + 0.8 v in red, green
 * and blue alike: the interpolated set dotted with a constant direction in the pixel program, as a normal would be with a light's.
 * VXPSP v1.2 (driver .739, attempt 3 of 3; outside answers 2026-11-22, VIA's CE compiler run on MOV oC0, t0 with this vertex layout):
 * C-E hung because the ALU was never given t0. 01/90 bits 14:0 name the texture registers delivered to the ALU (bit k = t k; XP's bump
 * programs 018001 / 018003 / 018004 / 018005), and the last ALU read of a bank-3 register carries its operand's last-read bit (source A:
 * w2 bit 17), or the slot is never released. New forms:
 *   F  C with 01/90 = 058001 (t0 to the ALU) and w2 = 00F2C005 (A's last-read bit): the two fixes alone
 *   G  F with the vendor compiler's other words: FE/00 = 10 (VX855) / 12 (VX800), FE/01 = 212121, FE/04 = 0, 01/9B = 000001
 *   H  G with t0.yxzw (w2 = 00F2C021): the swizzle on bank 3
 *   I  F with FE/0D = 810000 and FE/10 = 800000 as the driver writes them (the fallback named with the answers)
 * Default forms A B F G H I (C-E hung on T735 and T738: VXPSP CDE runs them again). VXPSP v1.1 (driver .736: forms B-E draw with the driver's 04/23 = CE02; T735 (v1.0, 04/23 = 2, the lab's): B drew the lab's solid
 * orange, C-E hung the engine at their last idle wait (status 701) on the VX800 and VX855). VXPSP v1.0 (driver .734): can the Chrome9's pixel program read an interpolated coordinate set as a value (source bank 3, t0), without a
 * texture fetch? The first step of per-pixel lighting (the speed list's item E: a normal carried in a coordinate set, lit in the pixel
 * program). VIA's PDB (docs/PIXEL_SHADER_ANSWERS.md) gives the ALU source banks 0 temp/TAU, 1 colour (v0 v1), 2 constant, 3 interpolated
 * texcoord; the driver has only ever used banks 0-2. Drawn through the 3D lab's raw path (VX_IOCTL_3D_TEST, 'RAW3': the lab's fixed
 * set-up, then this tool's words) on the 64 x 64 lab target, with the driver's own program set-up (01/90-9B, the program at 20030000,
 * 01/01 CE1540, the 04 vertex layout, FE/00-08):
 *   A  the lab's control: a solid orange triangle (the lab's own set-up, as VXCLIP A)
 *   B  the driver's diffuse program (XP's T13: MOV oC0, v0) on vertices X Y Z W diffuse, red, green and blue corners: the set-up's words
 *      are right if the corners come out red, green and blue
 *   C  MOV oC0, t0 on vertices X Y Z W diffuse u v (set A, 2D), u = x / 64, v = y / 64: red should rise left to right and green top to
 *      bottom (pixel x, y about 4x, 4y) if bank 3 reads the interpolated set
 *   D  C with 01/91 = 1 (one coordinate slot named in the program header, as VIA's one-set programs write it)
 *   E  C with MOV oC0, t0.yxzw (swizzled: green from u, red from v): the swizzle applies to bank 3 as to the others
 * Each form's words; a form that hangs the engine is reset by the lab (the next form is sent with the reset). The VX900 has other vertex
 * words: not run there. Run it last in a boot (PROBE.BAT). VXPSP.LOG beside the program. VXPSP <letters> runs only those forms. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "vx85598.h"
#include "vx855_3d.h"
static FILE *lf;
static void say(const char *f,...) {static char b[2000];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);fputs(b,stdout);if(lf){fputs(b,lf);fflush(lf);}}
static HANDLE dev;static vx3d_request rq;static vx3d_report rp;static DWORD nw;
static void P(DWORD port,DWORD v) {if(nw+2<=VX3D_LAB_DATA){rq.lab_data[nw++]=port;rq.lab_data[nw++]=v;}}
static void T(DWORD t) {P(0x43C,t<<16);}
static void R(DWORD a,DWORD v) {P(0x440,(a<<24)|(v&0xFFFFFFUL));}
static void D(DWORD w) {P(0x440,w);}
static void W43C(DWORD w) {P(0x43C,w);}
static void IDLE(void) {P(0xFFFFFFFFUL,0);}
static DWORD fb(float f) {DWORD b;memcpy(&b,&f,4);return b;}
static void C2S(void) { /* the driver's screen transform: scale 1, offset 786432.0 (XP's) */
 DWORD a=fb(1.0f),b=fb(786432.0f);
 T(4);R(0x40,a&0xFFFFFFUL);R(0x41,b&0xFFFFFFUL);R(0x42,((b>>24)<<8)|(a>>24));
 R(0x43,a&0xFFFFFFUL);R(0x44,b&0xFFFFFFUL);R(0x45,((b>>24)<<8)|(a>>24));
 R(0x46,0x800000UL);R(0x47,0);R(0x48,0x92003FUL);R(0x50,0);R(0x51,0);R(0x52,0x147FFFUL);
}
typedef struct {float x,y;DWORD c;} PV;
static const PV tri[3]={{4,4,0xFFFF0000UL},{60,4,0xFF00FF00UL},{4,60,0xFF0000FFUL}};
/* XP's T13 (MOV oC0, v0) and the same instruction with source A's bank 1 -> 3 (w2 bits 16:14), and t0.yxzw (A's swizzle: w2 7:0, w1 31:28) */
static const DWORD t13[4]={0x01C00530UL,0x301C0053UL,0x00F04005UL,0x000D0820UL};
static DWORD p90=0x58000UL,p9b=0xFFFFF0UL;static int fe_vendor,fe_extra,chip800,p_c0; /* v1.2: F-I; v1.3: p_c0, J's constant */
static void program(const DWORD *w,DWORD h91) {
 DWORD k;
 T(1);R(0x00,0);R(0x01,0xCE1140UL); /* the program off while its words are replaced, as the driver */
 R(0x13,7UL<<16); /* depth ALWAYS: the lab's depth buffer does not decide */
 R(0x90,p90);R(0x91,h91);R(0x92,1);R(0x93,0);for(k=0x94;k<=0x9A;k++)R(k,0);R(0x9B,p9b);
 W43C(0x20030000UL);for(k=0;k<4;k++)D(w[k]);
 if(p_c0){W43C(0x22030000UL);D(fb(0.6f));D(fb(0.8f));D(0);D(0);} /* v1.3: c0 */
 T(1);R(0x01,0xCE1540UL);
}
static void layout(int tex) {
 T(4);
 if(!tex){R(0x00,0xA80005UL);R(0x01,0x020100UL);R(0x02,0x000503UL);R(0x03,0);R(0x04,0);R(0x25,0xF40000UL);R(0x28,0xFD0000UL);}
 else{R(0x00,0xA80007UL);R(0x01,0x020100UL);R(0x02,0x080503UL);R(0x03,0x000009UL);R(0x04,0);R(0x25,0xF4C000UL);R(0x28,0xFDC000UL);}
 R(0x2A,0x3F8000UL);R(0x2B,0x3F8000UL);
 W43C(0xFE020000UL);
 if(!tex)R(0x00,1);else if(fe_vendor){R(0x00,chip800?0x12:0x10);R(0x01,0x212121UL);R(0x04,0);R(0x08,0x000101UL);R(0x09,0);} /* v1.2 G: the vendor compiler's words */
 else{R(0x00,0x11);R(0x01,0x21);R(0x08,0x000101UL);R(0x09,0);} /* FE/00 one slot, FE/01 slot A = set A 2D perspective, FE/08 one set (A 2D) */
 if(tex&&fe_extra){R(0x0D,0x810000UL);R(0x10,0x800000UL);} /* v1.2 I: as vx_native3d_hw.c writes them */
}
static void draw(int tex) {
 int k;C2S();
 {{
 T(4);R(0x23,0xCE02UL);R(0x24,3);T(0); /* v1.1: the driver's triangle mode (04/23 = CE02): T735's B drew the lab's solid colour with 2 */
 for(k=0;k<3;k++){D(fb(tri[k].x));D(fb(tri[k].y));D(fb(0.5f));D(fb(1.0f));D(tri[k].c);if(tex){D(fb(tri[k].x/64.0f));D(fb(tri[k].y/64.0f));}}
 IDLE();}}
 T(1);R(0x01,0x40);W43C(0xFE020000UL);R(0x00,0);R(0x01,0);R(0x08,0); /* the lab's state back */
}
static const char *res(DWORD r) {static const char *const n[]={"not run","ran","NOT READY (use 800 x 600 x 16)","bad register window","no register mapping","BUSY before the test","TIMEOUT"};return r<7?n[r]:"failed";}
static int recover;
static int inside(int x,int y) {return x>=4&&y>=4&&x+y<=62;} /* the triangle (4,4) (60,4) (4,60), a pixel's corner, a margin of one */
static int form(int f) {
 DWORD sw[4];int x,y,lit=0,good=0,bad=0,ok;DWORD got=0;const char *what="";
 memset(&rq,0,sizeof(rq));rq.cb=sizeof(rq);rq.version=VX3D_VERSION;rq.arm=VX3D_ARM;rq.probe=1;rq.lab_cmd=0x33574152UL;rq.reserved=recover?0x100UL:0;nw=0;
 memcpy(sw,t13,sizeof(sw));sw[2]=(sw[2]&~(7UL<<14))|(3UL<<14); /* MOV oC0, t0.xyzw */
 p90=0x58000UL;p9b=0xFFFFF0UL;fe_vendor=0;fe_extra=0;p_c0=0;
 if(f>='F'){p90=0x58001UL;sw[2]|=1UL<<17;} /* v1.2: t0 to the ALU (01/90 bit 0); source A's last-read bit (w2 17): 00F2C005 */
 if(f=='G'||f=='H'){fe_vendor=1;p9b=1;}
 if(f=='I')fe_extra=1;
 switch(f){
  case 'A':what="the lab's control (solid orange)";C2S();T(4);R(0x23,2);R(0x24,3);T(0);for(x=0;x<3;x++){D(fb(tri[x].x));D(fb(tri[x].y));D(fb(0.5f));D(fb(1.0f));D(0xFFE04020UL);}IDLE();break;
  case 'B':what="the driver's diffuse program (MOV oC0, v0): red, green, blue corners";program(t13,0);layout(0);draw(0);break;
  case 'C':what="MOV oC0, t0 (bank 3), set A u = x/64, v = y/64, 01/91 = 0";program(sw,0);layout(1);draw(1);break;
  case 'D':what="C with 01/91 = 1";program(sw,1);layout(1);draw(1);break;
  case 'E':what="MOV oC0, t0.yxzw (swizzled): green from u, red from v";{DWORD s2[4];memcpy(s2,sw,sizeof(s2));
   /* A's swizzle (12 bits, x y z w 3 bits each, from 11:9 down): yxzw = 1 0 2 3 -> 001 000 010 011 = 0x213; its high 8 bits in w2 7:0, low 4 in w1 31:28 */
   s2[2]=(s2[2]&~0xFFUL)|(0x213UL>>4);s2[1]=(s2[1]&0x0FFFFFFFUL)|((0x213UL&15)<<28);program(s2,0);layout(1);draw(1);}break;
  case 'F':what="MOV oC0, t0 with 01/90 = 058001 and A's last-read bit (w2 00F2C005)";program(sw,0);layout(1);draw(1);break;
  case 'G':what="F with the vendor compiler's FE/00, FE/01 = 212121, FE/04 = 0, 01/9B = 1";program(sw,0);layout(1);draw(1);break;
  case 'H':what="G with t0.yxzw (w2 00F2C021): green from u, red from v";sw[2]=(sw[2]&~0xFFUL)|0x21UL;program(sw,0);layout(1);draw(1);break;
  case 'J':what="DP3 oC0, t0, c0 (c0 = 0.6, 0.8, 0, 0): grey 0.6 u + 0.8 v";sw[1]=0x30080053UL;sw[3]=0x00020820UL;p_c0=1;program(sw,0);layout(1);draw(1);break;
  case 'I':what="F with FE/0D = 810000 and FE/10 = 800000";program(sw,0);layout(1);draw(1);break;
  default:return 0;}
 rq.lab_ndata=nw;
 memset(&rp,0,sizeof(rp));ok=DeviceIoControl(dev,VX_IOCTL_3D_TEST,&rq,sizeof(rq),&rp,sizeof(rp),&got,NULL);
 if(ok&&got==sizeof(rp)&&!recover&&rp.fault_latched&&rp.stage==0&&rp.writes==0){rq.reserved=0x100UL;memset(&rp,0,sizeof(rp));got=0;ok=DeviceIoControl(dev,VX_IOCTL_3D_TEST,&rq,sizeof(rq),&rp,sizeof(rp),&got,NULL);say("\n(the engine's fault was latched by an earlier form: reset and sent again)\n");}
 say("\n%c  %s: %lu words\n",f,what,nw/2);
 if(!ok||got!=sizeof(rp)){say("   the VxD refused the call (error %lu)\n",GetLastError());return 0;}
 say("   %s; stage %lu (4 = idle after every word); status after the words %08lX; fault latched %lu\n",res(rp.result),rp.stage,rp.status_solid,rp.fault_latched);
 if(rp.vertex_words!=0xFFFFFFFFUL)say("   a wait failed at word %lu (reason %lu, status %08lX): THE ENGINE HUNG ON THIS FORM\n",rp.vertex_words,rp.wait_failure_reason,rp.wait_failure_status);
 recover=rp.fault_latched!=0;
 if(rp.result==2||rp.stage<2)return 0;
 say("   pixels (x, y): colour as drawn\n");
 {static const int sx[6]={6,30,54,6,6,20},sy[6]={6,6,6,30,54,20};int k;for(k=0;k<6;k++){DWORD c=rp.image[sy[k]*64+sx[k]];say("      (%2d,%2d) %08lX",sx[k],sy[k],c);if(k%3==2)say("\n");}}
 for(y=0;y<64;y++)for(x=0;x<64;x++){DWORD c=rp.image[y*64+x];int in=inside(x,y),r=(int)((c>>16)&255),g=(int)((c>>8)&255),b=(int)(c&255);
  if(c!=VX3D_BACKGROUND)lit++;
  if(!in||x<6||y<6||x+y>58)continue; /* the edges not judged */
  if(f=='A'){if((c&0xFFFFFFUL)==0xE04020UL)good++;else bad++;}
  else if(f=='B'){/* barycentric colour: red at (4,4), green toward x, blue toward y */
   double gx=(x-4)/56.0,gy=(y-4)/56.0;int er=(int)((1-gx-gy)*255),eg=(int)(gx*255),eb=(int)(gy*255);
   if(abs(r-er)<=24&&abs(g-eg)<=24&&abs(b-eb)<=24)good++;else bad++;}
  else if(f=='J'){int e=(int)((0.6*x+0.8*y)/64.0*255.0+0.5);if(e>255)e=255;
   if(abs(r-e)<=12&&abs(g-e)<=12&&abs(b-e)<=12)good++;else bad++;}
  else{int eu=(int)(x*4.0),ev=(int)(y*4.0);if(eu>255)eu=255;if(ev>255)ev=255;
   if(f=='E'||f=='H'){int t=eu;eu=ev;ev=t;}
   if(abs(r-eu)<=12&&abs(g-ev)<=12)good++;else bad++;}}
 say("   %d pixels drawn; inside: %d as expected, %d not\n",lit,good,bad);
 {int pass=good>200&&bad*20<good&&!rp.fault_latched&&rp.vertex_words==0xFFFFFFFFUL;
  say(pass?"   -> AS EXPECTED\n":"   -> NOT as expected\n");return pass;}
}
/* v1.5 (.748): v1.4's T (the same instruction written 2 to 16 times, 01/92 = the count) is removed: T747, VX855: the engine timed out on the
 * 2-instruction form (fault latched, the wait failed at word 71), and the 1-instruction requests took 190 microseconds a triangle (the lab waits
 * after every word), so the method could not have shown a nanosecond a pixel either. */
int main(int argc,char **argv) {
 char path[MAX_PATH],*s;const char *forms=argc>1?argv[1]:"ABFJ";int k;static char good[12];DWORD in=0x75,o[8],got=0;HANDLE t;
 GetModuleFileNameA(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXPSP.LOG");lf=fopen(path,"w");
 say("VXPSP v1.5 (driver .748): the pixel program reading an interpolated coordinate set (source bank 3) on the 64 x 64 lab target\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE){say("FAILED: the VxD is not open\n");if(lf)fclose(lf);return 2;}
 t=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);memset(o,0,sizeof(o));
 if(t!=INVALID_HANDLE_VALUE){DeviceIoControl(t,0x85500022UL,&in,4,o,32,&got,0);CloseHandle(t);}
 say("the VxD's build: .%lu; the device 1106:%04lX\n",o[1],o[2]);
 chip800=o[2]==0x1122;
 if(o[2]==0x7122){say("the VX900 has other vertex words (its vertex program): not run\n");if(lf)fclose(lf);return 0;}
 for(k=0;forms[k];k++){int f=forms[k]>='a'&&forms[k]<='z'?forms[k]-32:forms[k];if(f>='A'&&f<='J')good[f-'A']=(char)form(f);}
 say("\nSUMMARY:");for(k=0;forms[k];k++){int f=forms[k]>='a'&&forms[k]<='z'?forms[k]-32:forms[k];if(f>='A'&&f<='J')say(" %c %s;",f,good[f-'A']?"as expected":"NOT");}say("\n");
 if(good[1]&&(good[2]||good[3]||good[5]||good[6]||good[8]))say("bank 3 reads the interpolated set: per-pixel lighting (item E) can take its normal from a coordinate set\n");
 else if(!good[1])say("the program set-up (B) is not right on the lab target: C-E say nothing yet\n");
 if(good[9])say("the pixel program dots an interpolated set with a constant: per-pixel N.L works (item E)\n");
 if(lf)fclose(lf);CloseHandle(dev);return 0;
}
