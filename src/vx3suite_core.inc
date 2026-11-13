/* VX3SUITE core: test table, request builders, per-pixel models and result
 * evaluation. Platform neutral (no Windows calls) so it can be unit-tested on
 * a host with a mock driver. Copyright (c) 2026, MIT.
 *
 * The front end provides: static int dev_call(uint32_t select);
 *   - copies nothing: it sends RQ to the driver (probe 20, reserved = select
 *     | recover flag) and leaves the reply in RP. Returns 1 if the driver
 *     answered with a valid reply, 0 otherwise.
 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "vx855_3d.h"

#define ST_NOTRUN 0
#define ST_PASS 1
#define ST_FAIL 2
#define ST_HANG 3
#define ST_SKIP 4
#define MAXT 256
#define N2D 40

enum {G_SYS=0,G_FILL,G_COPY,G_HROT,G_TEXT,G_PAT,G_KEY,G_HOST,G_TEX,G_RT,G_DEPTH,G_GOU,G_STEN,G_FOG,G_TROT,G_BLEND,G_REACH,NGROUPS};
static const char *GNAME[NGROUPS]={
 "System","2D fills","2D copy+clip","2D host rotate","2D rotated text","2D pattern+line","2D keys+text","2D host data",
 "3D texture","3D 16bpp RT","3D depth","3D Gouraud","3D stencil","3D fog","3D tex rotate","3D blend/alpha","3D reach"};

typedef struct suite_test {
 int group,status,has_sw,img,xm;
 uint32_t sw;
 char name[88];
 char detail[240];
} suite_test;

static suite_test T[MAXT];
static int NT;
static uint32_t IMG_B[N2D][4096],IMG_A[N2D][4096],IMG_E[N2D][4096];
static uint32_t XM[MAXT][32];
static int NIMG;
static vx3d_request RQ;
static vx3d_report RP;
static int g_dead;
static char g_dead_why[160];
static void (*g_hook)(int);
static char g_sysinfo[1024];

static int dev_call(uint32_t select);

static int t_add(int group,const char *name) {
 suite_test *t;
 if(NT>=MAXT) return MAXT-1;
 t=&T[NT]; memset(t,0,sizeof(*t)); t->group=group; t->img=-1;
 strncpy(t->name,name,sizeof(t->name)-1);
 return NT++;
}
static char g_inflight[120]; /* v10.09: set while a driver call is running, so a crash leaves a trace in VX3SUITE.TXT */
static void t_set(int i,int status,const char *detail) {
 T[i].status=status;
 strncpy(T[i].detail,detail,sizeof(T[i].detail)-1); T[i].detail[sizeof(T[i].detail)-1]=0;
 if(g_hook) g_hook(i);
}

/* ---------- ROP3 (same truth-table rule the driver's own models use) ---------- */
static uint32_t rop3(uint32_t rop,uint32_t d,uint32_t sr,uint32_t p) {
 uint32_t k,out=0;
 for(k=0;k<32;k++) {
  uint32_t idx=(((p>>k)&1)<<2)|(((sr>>k)&1)<<1)|((d>>k)&1);
  if((rop>>idx)&1) out|=1UL<<k;
 }
 return out;
}

/* ---------- lab test specification ---------- */
typedef struct lab_spec {
 uint32_t cmd,mode,w,h,sx,sy,dx,dy,fg,bg,flags;
 uint32_t reg[8][2]; int nreg;
 uint32_t data[VX3D_LAB_DATA]; int ndata;
 int kind;
 int bx,by,bw,bh;   /* destination box (model) */
 int ssx,ssy;       /* source origin (model) */
 int hasclip,cx0,cy0,cx1,cy1;
 int ang,W,H;       /* rotation / text parameters */
 int a101,trans;
} lab_spec;
enum {K_FILL=1,K_COPY,K_HOST,K_ROT,K_TEXT};

#define WX 448UL
#define WY 320UL

static int inclip(const lab_spec *s,int x,int y) {
 return !s->hasclip || (x>=s->cx0 && x<=s->cx1 && y>=s->cy0 && y<=s->cy1);
}
static int ts_bit(int y,int x,int W) {
 int v=(((x*3+y*5+(x>>2)*(y+1))%7)<3);
 if(y==0&&x<2) v=1;
 if(y==1&&x==0) v=1;
 if(y==0&&x==W-1) v=1;
 if(y==7&&x>=W-2) v=0;
 return v;
}
static int ts_dest(int ang,int W,int x,int y) {
 if(ang==0) return ts_bit(y,x,W);
 if(ang==180) return ts_bit(7-y,W-1-x,W);
 if(ang==90) return ts_bit(7-x,y,W);
 return ts_bit(x,W-1-y,W);
}
static void model(const lab_spec *s,const uint32_t *b,uint32_t *e) {
 int x,y;
 memcpy(e,b,4096*4);
 if(s->kind==K_FILL) {
  for(y=0;y<s->bh;y++) for(x=0;x<s->bw;x++) {
   int px=s->bx+x,py=s->by+y;
   if(px<0||py<0||px>63||py>63||!inclip(s,px,py)) continue;
   e[py*64+px]=rop3(s->cmd>>24,b[py*64+px],0,s->fg);
  }
 } else if(s->kind==K_COPY) {
  for(y=0;y<s->bh;y++) for(x=0;x<s->bw;x++) {
   int px=s->bx+x,py=s->by+y,qx=s->ssx+x,qy=s->ssy+y;
   if(px<0||py<0||px>63||py>63||!inclip(s,px,py)) continue;
   e[py*64+px]=b[qy*64+qx];
  }
 } else if(s->kind==K_HOST) {
  for(y=0;y<s->bh;y++) for(x=0;x<s->bw;x++) e[(s->by+y)*64+s->bx+x]=s->data[y*s->bw+x];
 } else if(s->kind==K_ROT) {
  int w=s->W,h=s->H;
  for(y=0;y<h;y++) for(x=0;x<w;x++) {
   uint32_t v=s->data[y*w+x]; int dx,dy;
   if(s->ang==90) {dx=h-1-y; dy=x;}
   else if(s->ang==270) {dx=y; dy=w-1-x;}
   else {dx=w-1-x; dy=h-1-y;}
   e[(s->by+dy)*64+s->bx+dx]=v;
  }
 } else if(s->kind==K_TEXT) {
  for(y=0;y<s->bh;y++) for(x=0;x<s->bw;x++) {
   int px=s->bx+x,py=s->by+y;
   int bit=ts_dest(s->ang,s->W,x,y);
   if(!inclip(s,px,py)) continue;
   if(bit) e[py*64+px]=s->fg; else if(!s->trans) e[py*64+px]=s->bg;
  }
 }
}

static void spec_init(lab_spec *s) {
 memset(s,0,sizeof(*s));
 s->mode=0x300; s->fg=0xFFFF8000UL; s->bg=0xFF004080UL;
}
static void spec_reg(lab_spec *s,uint32_t r,uint32_t v) {
 if(s->nreg<8) {s->reg[s->nreg][0]=r; s->reg[s->nreg][1]=v; s->nreg++;}
}

static void abort_all(const char *why) {
 g_dead=1; strncpy(g_dead_why,why,sizeof(g_dead_why)-1);
}
/* returns 1 if driver replied and engine is usable, 0 if the engine is gone */
static int after_call(int *hung) {
 *hung=0;
 if(RP.result==VX3D_DISABLED) {abort_all("driver disabled the engine after a fault: restart Windows"); *hung=1; return 0;}
 if(RP.fault_latched) *hung=1;
 return 1;
}

static void run_lab(int ti,lab_spec *s) {
 int i,hung=0,mism=0,expchg=0,fx=-1,fy=-1,x,y; uint32_t gv=0,wv=0; char d[240];
 uint32_t *B,*A,*E;
 if(g_dead) {t_set(ti,ST_SKIP,g_dead_why); return;}
 if(NIMG>=N2D) {t_set(ti,ST_SKIP,"image store full"); return;}
 memset(&RQ,0,sizeof(RQ));
 RQ.lab_cmd=s->cmd; RQ.lab_mode=s->mode; RQ.lab_w=s->w; RQ.lab_h=s->h;
 RQ.lab_sx=s->sx; RQ.lab_sy=s->sy; RQ.lab_dx=s->dx; RQ.lab_dy=s->dy;
 RQ.lab_fg=s->fg; RQ.lab_bg=s->bg; RQ.lab_flags=s->flags;
 for(i=0;i<s->nreg && i<VX3D_LAB_REGS;i++) {RQ.lab_reg[i][0]=s->reg[i][0]; RQ.lab_reg[i][1]=s->reg[i][1];}
 RQ.lab_nregs=(uint32_t)s->nreg;
 for(i=0;i<s->ndata && i<VX3D_LAB_DATA;i++) RQ.lab_data[i]=s->data[i];
 RQ.lab_ndata=(uint32_t)s->ndata;
 if(!dev_call(21)) {abort_all("driver call failed"); t_set(ti,ST_SKIP,"driver call failed"); return;}
 if(!after_call(&hung)) {t_set(ti,ST_HANG,"engine could not be recovered: restart Windows"); return;}
 if(RP.result==VX3D_TIMEOUT || (RP.sf_x_info[0][1] && !RP.sf_x_info[0][2])) {
  abort_all("2D engine hung and was not recovered: restart Windows");
  t_set(ti,ST_HANG,"2D engine hung, reset failed: restart Windows"); return;
 }
 if(RP.result!=VX3D_PASS) {sprintf(d,"driver result %lu",(unsigned long)RP.result); t_set(ti,ST_FAIL,d); return;}
 B=IMG_B[NIMG]; A=IMG_A[NIMG]; E=IMG_E[NIMG];
 memcpy(B,RP.sf_x_raw,4096*4); memcpy(A,RP.image,4096*4);
 model(s,B,E);
 for(y=0;y<64;y++) for(x=0;x<64;x++) {
  uint32_t a=A[y*64+x],e=E[y*64+x];
  if(e!=B[y*64+x]) expchg++;
  if(a!=e) {mism++; if(fx<0) {fx=x;fy=y;gv=a;wv=e;}}
 }
 T[ti].img=NIMG++;
 if(RP.sf_x_info[0][1]) {
  sprintf(d,"engine hung then reset OK; %d pixels wrong; %d expected to change",mism,expchg);
  t_set(ti,ST_HANG,d); return;
 }
 if(mism==0 && expchg>0) sprintf(d,"all 4096 pixels match the model; %d pixels changed as expected",expchg);
 else if(mism==0) sprintf(d,"model expects no change at all: test is empty");
 else sprintf(d,"%d of 4096 pixels wrong (expected %d to change); first at (%d,%d) got %08lX want %08lX",mism,expchg,fx,fy,(unsigned long)gv,(unsigned long)wv);
 t_set(ti,(mism==0&&expchg>0)?ST_PASS:ST_FAIL,d);
}

/* ---------- 2D tests built from the proven recipes ---------- */
static void plain_fill(const char *name,uint32_t cmd) {
 lab_spec s; int ti=t_add(G_FILL,name);
 spec_init(&s); s.cmd=cmd; s.w=16; s.h=16; s.dx=36; s.dy=36; s.sx=4; s.sy=4;
 s.kind=K_FILL; s.bx=36; s.by=36; s.bw=16; s.bh=16;
 run_lab(ti,&s);
}
static void clip_regs(lab_spec *s,int cx0,int cy0,int cx1,int cy1) {
 s->hasclip=1; s->cx0=cx0; s->cy0=cy0; s->cx1=cx1; s->cy1=cy1;
 spec_reg(s,0x40,((WY+(uint32_t)cy0)<<16)|(WX+(uint32_t)cx0));
 spec_reg(s,0x44,((WY+(uint32_t)cy1)<<16)|(WX+(uint32_t)cx1));
}
static void host_rot(int ang,int w,int h) {
 lab_spec s; char nm[88]; int ti,x,y,bw,bh,ox=30,oy=20;
 sprintf(nm,"Host-fed colour rotate %d (source %dx%d)",ang,w,h);
 ti=t_add(G_HROT,nm);
 spec_init(&s); s.sx=60; s.sy=60; s.flags=1|4;
 s.kind=K_ROT; s.ang=ang; s.W=w; s.H=h; s.bx=ox; s.by=oy;
 if(ang==90) {s.cmd=0xCC00A049UL; s.mode=0x301; s.w=(uint32_t)h; s.h=(uint32_t)w; bw=h; bh=w; s.dx=(uint32_t)(ox+h-1); s.dy=(uint32_t)oy;}
 else if(ang==270) {s.cmd=0xCC006049UL; s.mode=0x303; s.w=(uint32_t)h; s.h=(uint32_t)w; bw=h; bh=w; s.dx=(uint32_t)ox; s.dy=(uint32_t)(oy+w-1);}
 else {s.cmd=0xCC00E049UL; s.mode=0x302; s.w=(uint32_t)w; s.h=(uint32_t)h; bw=w; bh=h; s.dx=(uint32_t)(ox+w-1); s.dy=(uint32_t)(oy+h-1);}
 (void)bw; (void)bh;
 for(y=0;y<h;y++) for(x=0;x<w;x++)
  s.data[y*w+x]=0xFF000000UL|((uint32_t)((x*23+3)&0xFF)<<16)|((uint32_t)((y*41+7)&0xFF)<<8)|(uint32_t)((x+y*w*3+1)&0xFF);
 s.ndata=w*h;
 run_lab(ti,&s);
}
static void text_rot(int ang,int W,int a101,int trans,int cut) {
 static const unsigned long decb[4]={0UL,0x8000UL,0xC000UL,0x4000UL}; /* index ang/90 */
 lab_spec s; char nm[88]; int ti,x,y,bw,bh,sx,sy,cx0,cy0,cx1,cy1,clip=(trans||cut);
 unsigned int nB=(unsigned)((W+7)/8),ndw,i; unsigned char bytes[200];
 sprintf(nm,"Rotated mono text op A: %d deg, %dpx wide, %s%s, align %s",ang,W,trans?"transparent":"opaque",cut?"+cutting clip":(clip?"+box clip":""),a101?"dword":"byte");
 ti=t_add(G_TEXT,nm);
 spec_init(&s); memset(bytes,0,sizeof(bytes));
 if(ang==0||ang==180) {bw=W;bh=8;} else {bw=8;bh=W;}
 if(a101) {unsigned int dpr=(nB+3)/4; ndw=8*dpr; for(y=0;y<8;y++) for(x=0;x<W;x++) if(ts_bit(y,x,W)) bytes[y*dpr*4+x/8]|=(unsigned char)(0x80>>(x%8));}
 else {ndw=(8*nB+3)/4; for(y=0;y<8;y++) for(x=0;x<W;x++) if(ts_bit(y,x,W)) bytes[y*nB+x/8]|=(unsigned char)(0x80>>(x%8));}
 if(ang==0) {sx=4;sy=4;} else if(ang==180) {sx=4+bw-1;sy=4+bh-1;} else if(ang==90) {sx=4+bw-1;sy=4;} else {sx=4;sy=4+bh-1;}
 if(cut) {cx0=4+1+bw/6; cx1=4+bw-2-bw/6; cy0=4+1+bh/6; cy1=4+bh-2-bh/6;} else {cx0=4;cx1=4+bw-1;cy0=4;cy1=4+bh-1;}
 s.cmd=0xCC000000UL|(a101?0xA0000UL:0x20000UL)|0x140UL|decb[ang/90]|(clip?0x1000UL:0UL)|(trans?0x400UL:0UL)|0xAUL;
 s.mode=0x300UL|(uint32_t)(ang/90); s.w=(uint32_t)bw; s.h=(uint32_t)bh; s.dx=4; s.dy=4; s.sx=0; s.sy=0;
 s.flags=4|1;
 for(i=0;i<ndw && i<VX3D_LAB_DATA;i++) s.data[i]=(uint32_t)bytes[4*i]|((uint32_t)bytes[4*i+1]<<8)|((uint32_t)bytes[4*i+2]<<16)|((uint32_t)bytes[4*i+3]<<24);
 s.ndata=(int)ndw;
 spec_reg(&s,0x08,320UL<<16);
 spec_reg(&s,0x10,((WY+(uint32_t)sy)<<16)|(WX+(uint32_t)sx));
 if(clip) clip_regs(&s,cx0,cy0,cx1,cy1);
 s.kind=K_TEXT; s.ang=ang; s.W=W; s.a101=a101; s.trans=trans; s.bx=4; s.by=4; s.bw=bw; s.bh=bh;
 run_lab(ti,&s);
}
static void run_2d_basic(void) {
 lab_spec s; int ti,x,y;
 plain_fill("PATCOPY solid fill F0002001",0xF0002001UL);
 plain_fill("BLACKNESS fill 00002001",0x00002001UL);
 plain_fill("WHITENESS fill FF002001",0xFF002001UL);
 plain_fill("DSTINVERT 55002001",0x55002001UL);
 plain_fill("PATINVERT 5A002001 (colour in reg 58)",0x5A002001UL);
 /* clipped fill */
 ti=t_add(G_FILL,"Clipped PATCOPY fill (clip window bit 12, inclusive limits)");
 spec_init(&s); s.cmd=0xF0003001UL; s.w=40; s.h=24; s.dx=8; s.dy=24; s.sx=4; s.sy=4;
 s.kind=K_FILL; s.bx=8; s.by=24; s.bw=40; s.bh=24; clip_regs(&s,14,28,33,41);
 run_lab(ti,&s);
 /* copies */
 ti=t_add(G_COPY,"SRCCOPY screen-to-screen CC000001");
 spec_init(&s); s.cmd=0xCC000001UL; s.w=16; s.h=16; s.sx=4; s.sy=4; s.dx=36; s.dy=36;
 s.kind=K_COPY; s.bx=36; s.by=36; s.bw=16; s.bh=16; s.ssx=4; s.ssy=4;
 run_lab(ti,&s);
 ti=t_add(G_COPY,"SRCCOPY overlapping, both decrement bits, far-corner start (CC00C001)");
 spec_init(&s); s.cmd=0xCC00C001UL; s.w=24; s.h=24; s.sx=4+23; s.sy=4+23; s.dx=12+23; s.dy=12+23;
 s.kind=K_COPY; s.bx=12; s.by=12; s.bw=24; s.bh=24; s.ssx=4; s.ssy=4;
 run_lab(ti,&s);
 ti=t_add(G_COPY,"Clipped SRCCOPY (CC001001, clip window)");
 spec_init(&s); s.cmd=0xCC001001UL; s.w=24; s.h=16; s.sx=4; s.sy=4; s.dx=30; s.dy=30;
 s.kind=K_COPY; s.bx=30; s.by=30; s.bw=24; s.bh=16; s.ssx=4; s.ssy=4; clip_regs(&s,36,34,48,40);
 run_lab(ti,&s);
 /* host-fed colour blit */
 ti=t_add(G_HOST,"Host-fed colour blit CC002041, incrementing window addresses (12x8)");
 spec_init(&s); s.cmd=0xCC002041UL; s.w=12; s.h=8; s.dx=24; s.dy=40; s.sx=60; s.sy=60; s.flags=1|2;
 s.kind=K_HOST; s.bx=24; s.by=40; s.bw=12; s.bh=8;
 for(y=0;y<8;y++) for(x=0;x<12;x++) s.data[y*12+x]=0xFF000000UL|((uint32_t)(x*19+5)<<16)|((uint32_t)(y*29+9)<<8)|(uint32_t)(x*7+y*11+3);
 s.ndata=96;
 run_lab(ti,&s);
 /* host-fed rotation */
 host_rot(90,10,6); host_rot(180,10,6); host_rot(270,10,6);
 host_rot(90,6,10); host_rot(180,6,10); host_rot(270,6,10);
 /* rotated text */
 {static const int angs[4]={0,90,180,270}; int k;
  for(k=0;k<4;k++) {
   text_rot(angs[k],32,1,0,0);
   text_rot(angs[k],33,1,0,0);
   text_rot(angs[k],24,0,1,1);
  }}
}

/* ---------- VxD-side 2D lists (driver models, per-op match counts) ---------- */
typedef struct xop {int op; int group; const char *name;} xop;
static const xop XOPS19[4]={
 {21,G_HOST,"Host colour blit CC002041, constant window address"},
 {22,G_HOST,"Host colour blit CC002041, incrementing addresses"},
 {13,G_HOST,"Host mono bitmap CC0A0141, dword aligned"},
 {23,G_HOST,"Host mono bitmap CC0A0141, incrementing addresses"}};
static const xop XOPS21[8]={
 {0,G_PAT,"Mono 8x8 pattern fill, opaque (F0000A01)"},
 {1,G_PAT,"Mono pattern, transparent background (F0010A01)"},
 {2,G_PAT,"Colour pattern from pattern RAM (F0400801)"},
 {3,G_PAT,"Mono pattern with X3/Y2 start offset"},
 {4,G_PAT,"Bresenham line, shallow (F0002005)"},
 {5,G_PAT,"Bresenham line, steep, register pattern (bit 11)"},
 {6,G_PAT,"Bresenham line, X decrement, mono pattern (bits 11+9)"},
 {7,G_PAT,"Vertical line, last pixel disabled (bit 20)"}};
static const xop XOPS22[4]={
 {8,G_KEY,"Source colour-key copy (key in reg 50, 48=4000)"},
 {10,G_KEY,"Destination colour-key copy (key in reg 4C, 48=8000)"},
 {25,G_KEY,"Host text CC021542, clip window + transparent mono"},
 {9,G_KEY,"Colour pattern from frame buffer (F0000001)"}};

static void run_xlist(uint32_t select,const xop *ops,int n) {
 int i,k,hung=0,ti[8]; char d[240];
 for(i=0;i<n;i++) ti[i]=t_add(ops[i].group,ops[i].name);
 if(g_dead) {for(i=0;i<n;i++) t_set(ti[i],ST_SKIP,g_dead_why); return;}
 memset(&RQ,0,sizeof(RQ));
 if(!dev_call(select)) {abort_all("driver call failed"); for(i=0;i<n;i++) t_set(ti[i],ST_SKIP,"driver call failed"); return;}
 if(!after_call(&hung)) {for(i=0;i<n;i++) t_set(ti[i],ST_HANG,"engine could not be recovered: restart Windows"); return;}
 for(i=0;i<n;i++) {
  int found=-1;
  for(k=0;k<8;k++) if(RP.sf_x_info[k][7] && RP.sf_x_info[k][0]==(uint32_t)ops[i].op) {found=k; break;}
  if(found<0) {t_set(ti[i],ST_FAIL,"driver did not report this operation"); continue;}
  {
   uint32_t chg=RP.sf_x_info[found][3],mat=RP.sf_x_info[found][4],err=RP.sf_x_info[found][5],exp=RP.sf_x_info[found][6];
   int h=(int)RP.sf_x_info[found][1],rec=(int)RP.sf_x_info[found][2],ok;
   memcpy(XM[ti[i]],RP.sf_x_cls[found],32*4); T[ti[i]].xm=1;
   sprintf(d,"%lu of %lu expected pixels match, %lu wrong, %lu changed%s",(unsigned long)mat,(unsigned long)exp,(unsigned long)err,(unsigned long)chg,h?(rec?"; HUNG, reset OK":"; HUNG, reset FAILED"):"");
   ok=(!h && err==0 && exp>0 && mat==exp);
   t_set(ti[i],h?ST_HANG:(ok?ST_PASS:ST_FAIL),d);
   if(h && !rec) abort_all("2D engine hung and was not recovered: restart Windows");
  }
 }
}

/* ---------- 3D variants: ten cases each, scored from the driver's own measurements ---------- */
typedef struct var3d {uint32_t sel; int group; const char *prefix; const char *names[10];} var3d;
static const var3d V3D[]={
 {0,G_TEX,"Texture",{"ARGB nearest 1:1 repeat","ARGB nearest 2x magnified","ARGB bilinear 1:1","ARGB bilinear 2x magnified","ARGB nearest clamp (far outside)","ARGB nearest mirror","L8 format nearest","ABGR 32bpp format","BGRA 32bpp format","alpha texel + SRCALPHA/INVSRCALPHA blend"}},
 {10,G_RT,"16bpp RT",{"RGB565 flat (.5,.25,.75)","RGB555 flat","ARGB1555 flat, A=1","ARGB4444 flat, A=1","RGB565 flat (.2,.6,.9)","RGB565 mid-grey (rounding probe)","RGB565 SRCALPHA/INVSRCALPHA a=.5","ARGB4444 blend a=.5","ARGB1555 flat, A=0","format 0x8D = RGB565 + 2x2 ordered dither"}},
 {11,G_RT,"16bpp RT, XP 01/53",{"RGB565 flat (.5,.25,.75)","RGB555 flat","ARGB1555 flat, A=1","ARGB4444 flat, A=1","RGB565 flat (.2,.6,.9)","RGB565 mid-grey (rounding probe)","RGB565 SRCALPHA/INVSRCALPHA a=.5","ARGB4444 blend a=.5","ARGB1555 flat, A=0","format 0x8D = RGB565, no dither in round mode"}},
 {15,G_DEPTH,"Depth",{"EQUAL, A=B=.5: B wins","EQUAL, A=.5 B=.75: A wins","EQUAL, clear=1: nothing drawn","ALWAYS: B wins","ALWAYS, B nearer: B wins","ALWAYS, Z write off","ALWAYS, clear 0","LESS equal Z: A wins (strict)","GREATER equal Z: A wins (strict)","LEQUAL equal Z, Z write off"}},
 {16,G_GOU,"Gouraud",{"ARGB8888 triangle","RGB565 round (010C0F)","RGB565 (000C0F)","ARGB4444 round","ARGB1555 round","ARGB8888 reversed winding","RGB565 reversed winding","ARGB4444 (000C0F)","ARGB1555 (000C0F)","RGB555 round"}},
 {17,G_STEN,"Stencil",{"ALWAYS REPLACE 7F","EQUAL 55 passes overlap, INCR","INCRSAT keeps FF, INCR wraps","DECRSAT keeps 00, DECR wraps","INVERT 0F->F0","ZERO, B GREATER ref 1","NEVER with fail-op REPLACE","Z-fail op REPLACE, INVERT elsewhere","compare mask 0F, INVERT","write mask 0F, REPLACE FF"}},
 {18,G_FOG,"Fog",{"linear Z, eq 0, .25..75","linear Z, eq 1","start 0 end 1","start .5 end 1 (clamps)","fog disabled (control)","local mode (exploratory)","linear from W (exploratory)","green fog colour","colour-field mapping probe","start 0 end .5 (clamp)"}},
 {24,G_TROT,"Tex rotate 90",{"ARGB nearest 1:1 repeat","ARGB nearest 2x magnified","ARGB bilinear 1:1","ARGB bilinear 2x magnified","ARGB nearest clamp","ARGB nearest mirror","L8 nearest","ABGR 32bpp","BGRA 32bpp","alpha texel blend"}},
 {25,G_TROT,"Tex rotate 180",{"ARGB nearest 1:1 repeat","ARGB nearest 2x magnified","ARGB bilinear 1:1","ARGB bilinear 2x magnified","ARGB nearest clamp","ARGB nearest mirror","L8 nearest","ABGR 32bpp","BGRA 32bpp","alpha texel blend"}},
 {26,G_TROT,"Tex rotate 270",{"ARGB nearest 1:1 repeat","ARGB nearest 2x magnified","ARGB bilinear 1:1","ARGB bilinear 2x magnified","ARGB nearest clamp","ARGB nearest mirror","L8 nearest","ABGR 32bpp","BGRA 32bpp","alpha texel blend"}},
 {27,G_BLEND,"Alpha/blend",{"alpha test ALWAYS (control)","alpha test GREATER .75>ref","alpha test GREATER ref .75 (scaling probe)","alpha test LESS","alpha test NEVER (nothing)","alpha test NOTEQUAL","blend ONE/ONE","blend ONE/INVSRCALPHA","blend SRCALPHA/ONE","blend ZERO/SRCCOLOR"}},
 {28,G_DEPTH,"Depth (original set)",{"LESS A=.25 B=.75: A wins","GREATER clear 0: B wins","LEQUAL equal Z: B wins","LEQUAL B>A: A wins","NOTEQUAL equal Z: A wins","NOTEQUAL B>A: B wins","GEQUAL equal Z: B wins","GEQUAL clear 0: B wins","NEVER: nothing drawn","LESS, Z write off"}},
 {16UL|(1UL<<16),G_REACH,"Render target at",{"4 MiB","8 MiB","16 MiB","32 MiB","64 MiB","128 MiB","256 MiB","384 MiB","448 MiB","511 MiB (last 160 KB)"}},
 {15UL|(2UL<<16),G_REACH,"Depth buffer at",{"4 MiB","8 MiB","16 MiB","32 MiB","64 MiB","128 MiB","256 MiB","384 MiB","448 MiB","511 MiB (last 160 KB)"}},
 {0UL|(3UL<<16),G_REACH,"Texture at",{"4 MiB","8 MiB","16 MiB","32 MiB","64 MiB","128 MiB","256 MiB","384 MiB","448 MiB","511 MiB (last 160 KB)"}}
};
#define NV3D ((int)(sizeof(V3D)/sizeof(V3D[0])))

static void run_variant(const var3d *v) {
 int k,hung=0,ti[10]; char nm[100],d[240];
 for(k=0;k<10;k++) {sprintf(nm,"%s: %s",v->prefix,v->names[k]); ti[k]=t_add(v->group,nm);}
 if(g_dead) {for(k=0;k<10;k++) t_set(ti[k],ST_SKIP,g_dead_why); return;}
 memset(&RQ,0,sizeof(RQ));
 if(!dev_call(v->sel)) {abort_all("driver call failed"); for(k=0;k<10;k++) t_set(ti[k],ST_SKIP,"driver call failed"); return;}
 if(!after_call(&hung)) {for(k=0;k<10;k++) t_set(ti[k],ST_HANG,"engine could not be recovered: restart Windows"); return;}
 for(k=0;k<10;k++) {
  const vx3d_case_report *c=&RP.sf_matrix[16+k]; int observed=(RP.sf_matrix_observed>>(16+k))&1;
  if(!observed) {
   t_set(ti[k],hung?ST_HANG:ST_FAIL,hung?"3D engine faulted before this case was measured":"case was not measured");
   continue;
  }
  T[ti[k]].has_sw=1; T[ti[k]].sw=c->sample;
  sprintf(d,"changed %lu, matched %lu, errors %lu, sample %08lX%s",(unsigned long)c->changed,(unsigned long)c->orange,(unsigned long)c->errors,(unsigned long)c->sample,RP.guard_errors?"; GUARD ERRORS in this request":"");
  if(v->sel==18 && RP.sf_raw[k][2]) {char e[80]; sprintf(e,"; fog error vs model: max %lu, mean %lu.%02lu of 255 (tolerance 6)",(unsigned long)RP.sf_raw[k][5],(unsigned long)(RP.sf_raw[k][6]/100),(unsigned long)(RP.sf_raw[k][6]%100)); strcat(d,e);}
  t_set(ti[k],(c->errors==0 && !RP.guard_errors)?ST_PASS:ST_FAIL,d);
 }
 if(hung) {abort_all("3D engine faulted: restart Windows before another run");}
}

/* ---------- VRAM reach ---------- */
static void run_vramreach(uint32_t fb_bytes) {
 int ti=t_add(G_SYS,"Video memory reach: CPU sentinels + 2D copy across the whole window"),hung=0;
 unsigned int np,nd; char d[240];
 if(g_dead) {t_set(ti,ST_SKIP,g_dead_why); return;}
 memset(&RQ,0,sizeof(RQ)); RQ.lab_flags=524288UL; RQ.lab_sx=fb_bytes;
 if(!dev_call(21)) {abort_all("driver call failed"); t_set(ti,ST_SKIP,"driver call failed"); return;}
 if(!after_call(&hung)) {t_set(ti,ST_HANG,"engine could not be recovered: restart Windows"); return;}
 np=(unsigned)RP.image[0]; nd=(unsigned)RP.image[2];
 if(!np) {t_set(ti,ST_SKIP,"mapped window is not larger than 4 MiB: nothing to test"); return;}
 sprintf(d,"window %lu MiB; CPU %lu of %u offsets, 2D engine %lu of %u offsets%s",(unsigned long)(RP.sf_x_info[5][3]>>20),(unsigned long)RP.image[1],np,(unsigned long)RP.image[3],nd,RP.image[4]?"; ENGINE HUNG":"");
 if(RP.image[4]) {abort_all("2D engine hung during the reach test: restart Windows"); t_set(ti,ST_HANG,d); return;}
 t_set(ti,(RP.image[1]==np && RP.image[3]==nd)?ST_PASS:ST_FAIL,d);
}

/* ---------- whole run ---------- */
static void run_all_tests(uint32_t fb_bytes) {
 int i;
 run_vramreach(fb_bytes);
 run_2d_basic();
 run_xlist(19,XOPS19,4);
 run_xlist(22,XOPS21,8);
 run_xlist(23,XOPS22,4);
 for(i=0;i<NV3D;i++) run_variant(&V3D[i]);
}

/* ---------- summary and report ---------- */
static void summary_counts(int g,int *tot,int *pass,int *fail,int *hang,int *skip) {
 int i; *tot=*pass=*fail=*hang=*skip=0;
 for(i=0;i<NT;i++) if(g<0 || T[i].group==g) {
  (*tot)++;
  if(T[i].status==ST_PASS) (*pass)++;
  else if(T[i].status==ST_FAIL) (*fail)++;
  else if(T[i].status==ST_HANG) (*hang)++;
  else (*skip)++;
 }
}
static const char *st_name(int s) {return s==ST_PASS?"PASS":s==ST_FAIL?"FAIL":s==ST_HANG?"HANG":s==ST_SKIP?"SKIP":"----";}
static void report_emit(void (*out)(const char *)) {
 char line[400]; int g,i,tot,pass,fail,hang,skip;
 out("VX3SUITE full function test, one automated run.\r\n");
 out(g_sysinfo);
 out("\r\nPASS = every pixel matched the model (2D) or the driver's own per-case measurement had zero errors (3D). FAIL = wrong pixels. HANG = the engine hung (recovery status in the line). SKIP = not run.\r\n");
 summary_counts(-1,&tot,&pass,&fail,&hang,&skip);
 sprintf(line,"\r\nTOTAL: %d tests, %d PASS, %d FAIL, %d HANG, %d SKIP.\r\n",tot,pass,fail,hang,skip); out(line);
 if(g_dead) {sprintf(line,"RUN STOPPED: %s\r\n",g_dead_why); out(line);}
 if(g_inflight[0]) {sprintf(line,"UNFINISHED: %s. If this is the last version of the file, Windows crashed or the engine stalled inside that driver call.\r\n",g_inflight); out(line);}
 out("\r\nBY GROUP\r\n");
 for(g=0;g<NGROUPS;g++) {
  summary_counts(g,&tot,&pass,&fail,&hang,&skip);
  if(!tot) continue;
  sprintf(line,"  %-18s %3d tests: %3d pass, %3d fail, %3d hang, %3d skip\r\n",GNAME[g],tot,pass,fail,hang,skip); out(line);
 }
 out("\r\nDETAIL\r\n");
 for(g=0;g<NGROUPS;g++) {
  int first=1;
  for(i=0;i<NT;i++) if(T[i].group==g) {
   if(first) {sprintf(line,"\r\n[%s]\r\n",GNAME[g]); out(line); first=0;}
   sprintf(line,"  %-4s %s -- %s\r\n",st_name(T[i].status),T[i].name,T[i].detail); out(line);
  }
 }
 out("\r\nNot covered by this suite (no proven recipe or known to hang): mono expansion from screen memory, fixed-colour fill with destination key, screen-to-screen rotate, CBU rotate windows, and 16bpp host rotate. 3D reach tests move the render target, depth buffer and texture to offsets from 4 MiB to the last 160 KB of the window. The 3D alpha/blend and original depth groups use driver variants re-enabled in v10.06.\r\n");
}
