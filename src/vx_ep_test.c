/* VXEP v1.0 (driver .552): the answers the HAL keeps for a state (vx_d3d_core.c, ep).
 * From .552 a draw's support check and the pixel program composed for its state are worked out once for each state and used again
 * until a command changes the state (.551 composed the program for every draw token, and again for every template). This program
 * looks for an answer used after its state changed, and times the draws the change is for. Direct3D 8, a 256x256 window, 64 quads in
 * an 8 x 8 grid, each quad its own DrawPrimitive.
 *  1. Support follows the state: before every odd quad STENCILENABLE is set (the depth buffer is D16: the driver leaves such a draw
 *     out), before every even quad it is cleared. Sent as ONE DrawPrimitives2 call (one vertex buffer) and as 64 calls (two vertex
 *     buffers in turn: every call works its answers out afresh). The two pictures must be the same, the even quads drawn and the odd
 *     ones missing. If no quad is missing the runtime did not pass the state on and the case says so (not judged).
 *  2. The program follows the stage: even quads with COLOROP SELECTARG1 DIFFUSE, odd quads with SELECTARG1 TFACTOR (a fixed colour),
 *     one call against 64 calls: the same picture, each cell its own colour.
 *  3. The states changed and changed back between draws (the same state again after another): one call against 64.
 *  4. Speed, 300 frames each: (a) 256 draws with no state between them, (b) 256 draws with a stage state changed before each.
 *     Compare the two times with the same program on .551; the HAL's counts (QueryPhases 332-335) say how often an answer was used again.
 * Results to the screen and VXEP.LOG beside the program. Run it on the desktop at 16 or 32 bpp; it takes about half a minute.
 * VXD3DMON must not be running. The same program runs on .551 (the counts are then not reported): every judged case passes there too. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef unsigned long U32;
#define VT(o,i) (((void**)(*(void***)(o)))[i])
typedef HRESULT (__stdcall *F0)(void*);
typedef HRESULT (__stdcall *F1)(void*,U32);
typedef HRESULT (__stdcall *F2)(void*,U32,U32);
typedef HRESULT (__stdcall *F2P)(void*,U32,void*);
typedef HRESULT (__stdcall *F3P)(void*,U32,U32,void*);
typedef HRESULT (__stdcall *FCREATEVB)(void*,U32,U32,U32,U32,void**);
typedef HRESULT (__stdcall *FBLOCK)(void*,U32,U32,unsigned char**,U32);
typedef HRESULT (__stdcall *FDP)(void*,U32,U32,U32);
typedef HRESULT (__stdcall *FSTREAM)(void*,U32,void*,U32);
enum {D3D_REL=2,D3D_MODE=8,D3D_CREATEDEV=15};enum {DEV_UPDATETEX=29};
enum {DEV_PRESENT=15,DEV_BACKBUF=16,DEV_CREATETEX=20,DEV_CREATEVB=23,DEV_BEGIN=34,DEV_END=35,DEV_CLEAR=36,DEV_SETRS=50,DEV_SETTEX=61,
      DEV_SETTSS=63,DEV_DRAWPRIM=70,DEV_SETVS=76,DEV_SETSTREAM=83};
enum {BUF_LOCK=11,BUF_UNLOCK=12,SURF_LOCK=9,SURF_UNLOCK=10,TEX_LOCK=16,TEX_UNLOCK=17};
typedef struct {U32 w,h,refresh,format;} MODE;
typedef struct {U32 bw,bh,bfmt,bcount,ms,swap;HWND wnd;U32 windowed,autoz,zfmt,flags,rate,interval;} PP;
typedef struct {long pitch;void *bits;} LR;
typedef struct {float x,y,z,w;U32 c;float u,v;} TL; /* FVF 0x144: XYZRHW, DIFFUSE, TEX1 */
#define TLF 0x144
#define WIN 256
#define NQ 64 /* an 8 x 8 grid of 32-pixel cells, each quad 28 x 28 */

static FILE *logf;static int fails;
static void say(const char *f,...) {static char b[2048];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);fputs(b,stdout);if(logf){fputs(b,logf);fflush(logf);}}
static void *dev,*vba,*vbb;static U32 bbfmt;static HWND wnd;
static DWORD (WINAPI *hal_phases)(DWORD *,DWORD);
static DWORD ph[336];static int ph_ok;
static void phases(void) {ph_ok=hal_phases&&hal_phases(ph,336)==336;}
static HRESULT clear(U32 color) {return ((HRESULT(__stdcall*)(void*,U32,void*,U32,U32,float,U32))VT(dev,DEV_CLEAR))(dev,0,0,3,color,1.0f,0);}
static void rs(U32 s,U32 v) {((F2)VT(dev,DEV_SETRS))(dev,s,v);}
static void tss(U32 s,U32 v) {((F3P)VT(dev,DEV_SETTSS))(dev,0,s,(void*)v);}
static void present(void) {((HRESULT(__stdcall*)(void*,void*,void*,void*,void*))VT(dev,DEV_PRESENT))(dev,0,0,0,0);}
static U32 qcolor(U32 q) {static const U32 c[8]={0xFFFF4040UL,0xFF40FF40UL,0xFF4040FFUL,0xFFFFFF40UL,0xFFFF40FFUL,0xFF40FFFFUL,0xFFC0C0C0UL,0xFFFF9020UL};return c[(q+q/8)&7];}
static void quad(TL *v,U32 q,U32 c) {float x=(float)((q&7)*32+2),y=(float)((q>>3)*32+2),s=28.0f;int k;
 static const float px[6]={0,1,0,1,1,0},py[6]={0,0,1,0,1,1};
 for(k=0;k<6;k++){v[k].x=x+px[k]*s;v[k].y=y+py[k]*s;v[k].z=0.5f;v[k].w=1.0f;v[k].c=c;v[k].u=px[k];v[k].v=py[k];}}
static int make_vb(void **vb,int textured) {
 unsigned char *p;U32 q;TL v[NQ*6];
 if(((FCREATEVB)VT(dev,DEV_CREATEVB))(dev,sizeof(v),0,TLF,1,vb)||!*vb){say("CreateVertexBuffer failed\n");return 0;}
 for(q=0;q<NQ;q++)quad(v+q*6,q,textured?0xFFFFFFFFUL:qcolor(q));
 if(((FBLOCK)VT(*vb,BUF_LOCK))(*vb,0,0,&p,0)){say("vertex buffer lock failed\n");return 0;}
 memcpy(p,v,sizeof(v));((F0)VT(*vb,BUF_UNLOCK))(*vb);return 1;
}
static int read_bb(U32 *out) {
 void *bb=0;LR lr;int ok=0;U32 x,y;
 if(((HRESULT(__stdcall*)(void*,U32,U32,void**))VT(dev,DEV_BACKBUF))(dev,0,0,&bb)||!bb)return 0;
 if(!((HRESULT(__stdcall*)(void*,LR*,void*,U32))VT(bb,SURF_LOCK))(bb,&lr,0,0x10)){
  for(y=0;y<WIN;y++){unsigned char *row=(unsigned char*)lr.bits+y*lr.pitch;
   for(x=0;x<WIN;x++){if(bbfmt==23){unsigned short p=((unsigned short*)row)[x];out[y*WIN+x]=((U32)(p>>11)<<19)|((U32)((p>>5)&63)<<10)|((U32)(p&31)<<3);}else out[y*WIN+x]=((U32*)row)[x]&0xFFFFFFUL;}}
  ((F0)VT(bb,SURF_UNLOCK))(bb);ok=1;}
 ((F0)VT(bb,D3D_REL))(bb);return ok;
}
static int differs(U32 a,U32 b) {int k;for(k=0;k<24;k+=8){int d=(int)((a>>k)&255)-(int)((b>>k)&255);if(d<-12||d>12)return 1;}return 0;}
static int ndiff(const U32 *a,const U32 *b) {int n=0,k;for(k=0;k<WIN*WIN;k++)if(differs(a[k],b[k]))n++;return n;}
static int lit(const U32 *p) {int n=0,k;for(k=0;k<WIN*WIN;k++)if(p[k])n++;return n;}
static U32 ref[WIN*WIN],cur[WIN*WIN];
static void plain_states(void) {rs(136,0);rs(7,0);rs(14,0);rs(27,0);rs(28,0);rs(22,1);rs(52,0);rs(60,0xFF2080C0UL);((F1)VT(dev,DEV_SETVS))(dev,TLF);((F2P)VT(dev,DEV_SETTEX))(dev,0,0);
 tss(1,2);tss(2,0);tss(4,2);tss(5,0);} /* COLOROP / ALPHAOP SELECTARG1 DIFFUSE (D3DTA_DIFFUSE 0) */
static void stream(void *vb) {((FSTREAM)VT(dev,DEV_SETSTREAM))(dev,0,vb,sizeof(TL));}
#define TFAC 0x2080C0UL
/* the state before quad q in each case */
static void state_for(int test,U32 q) {
 if(test==1)rs(52,q&1); /* STENCILENABLE on a D16 depth buffer: the draw is left out */
 else if(test==2)tss(2,(q&1)?3:0); /* COLORARG1 TFACTOR (3) / DIFFUSE (0) */
 else{ /* 3: both, changed and put back: quads 0,1 plain; 2 factor; 3 left out; then again */
  U32 m=q&3;rs(52,m==3);tss(2,m==2?3:0);
  if(m==1){tss(2,3);tss(2,0);rs(52,1);rs(52,0);}} /* another state and back again before the draw */
}
static U32 expect(int test,U32 q) { /* 0xFFFFFFFF: nothing drawn (black) */
 if(test==1)return (q&1)?0xFFFFFFFFUL:(qcolor(q)&0xFFFFFFUL);
 if(test==2)return (q&1)?TFAC:(qcolor(q)&0xFFFFFFUL);
 return (q&3)==3?0xFFFFFFFFUL:(q&3)==2?TFAC:(qcolor(q)&0xFFFFFFUL);
}
static void scene(int test,int many) {U32 q;
 ((F0)VT(dev,DEV_BEGIN))(dev);clear(0xFF000000UL);plain_states();stream(vba);
 for(q=0;q<NQ;q++){state_for(test,q);if(many)stream(q&1?vbb:vba);((FDP)VT(dev,DEV_DRAWPRIM))(dev,4,q*6,2);} /* many: another vertex buffer each quad, a DrawPrimitives2 call each */
 rs(52,0);tss(2,0);
 ((F0)VT(dev,DEV_END))(dev);}
static U32 cell(const U32 *p,U32 q) {return p[((q>>3)*32+16)*WIN+(q&7)*32+16];}
static DWORD before[336];
static void mark(void) {phases();memcpy(before,ph,sizeof(ph));}
static void counts(void) {
 if(!ph_ok){say("   the HAL's counts (QueryPhases 332-335): not reported (a driver before .552)\n");return;}
 say("   the program composed %lu times, used again %lu; support worked out %lu times, used again %lu\n",ph[332]-before[332],ph[333]-before[333],ph[334]-before[334],ph[335]-before[335]);
}
static void test_follow(int test,const char *what) {
 int d,wrong=0,missing=0;U32 q;
 say("%d. %s: one DrawPrimitives2 call against 64\n",test,what);
 scene(test,1);if(!read_bb(ref)){say("   back buffer lock failed\n");fails++;return;}present();
 mark();scene(test,0);if(!read_bb(cur)){say("   back buffer lock failed\n");fails++;return;}phases();present();
 d=ndiff(ref,cur);
 for(q=0;q<NQ;q++){U32 e=expect(test,q),c=cell(cur,q);if(e==0xFFFFFFFFUL){if(!c)missing++;}else if(differs(c,e))wrong++;}
 if(test!=2&&!missing){ /* the stencil state never reached the driver: nothing here tells an old answer from a new one */
  say("   no quad left out: the runtime did not pass STENCILENABLE on; %d pixels differ between the two forms: %s\n",d,d?"WRONG":"not judged");if(d)fails++;counts();return;}
 {int want=0;for(q=0;q<NQ;q++)if(expect(test,q)==0xFFFFFFFFUL)want++;
  say("   %d pixels differ between the two forms; %d cells the wrong colour; %d of %d quads left out as expected: %s\n",d,wrong,missing,want,(d||wrong||missing!=want)?"WRONG":"ok");
  if(d||wrong||missing!=want)fails++;}
 counts();
}
#define ND 256
static void speed_scene(int change) {U32 q;
 ((F0)VT(dev,DEV_BEGIN))(dev);clear(0xFF000000UL);plain_states();stream(vba);
 for(q=0;q<ND;q++){if(change)tss(2,(q&1)?3:0);((FDP)VT(dev,DEV_DRAWPRIM))(dev,4,((q*37)&63)*6,2);} /* quads out of buffer order: no two draws one after another in the buffer (nothing for the runtime to join) */
 tss(2,0);((F0)VT(dev,DEV_END))(dev);}
static void test_speed(int change) {
 DWORD t0,t1;int k;
 say("4%c. speed: 300 frames of %d draws in one call, %s\n",change?'b':'a',ND,change?"a stage state changed before each":"no state between them");
 for(k=0;k<20;k++){speed_scene(change);present();}
 mark();t0=GetTickCount();for(k=0;k<300;k++){speed_scene(change);present();}t1=GetTickCount();phases();
 say("   %.2f ms a frame (%lu ms; %.1f microseconds a draw)\n",(double)(t1-t0)/300.0,t1-t0,(double)(t1-t0)*1000.0/(300.0*ND));
 counts();
}
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProc(h,m,w,l);}
int main(void) {
 HMODULE d8;void *(__stdcall *create)(U32);void *d3d;MODE mode;PP pp;WNDCLASS wc;HRESULT hr;char path[MAX_PATH],*s;
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXEP.LOG");logf=fopen(path,"w");
 say("VXEP v1.0 (driver .552): the answers kept for a state (a draw's support, the composed program)\n");
 {char dll[MAX_PATH];UINT n=GetSystemDirectoryA(dll,MAX_PATH-16);HMODULE h=0;
  if(n&&n<MAX_PATH-16){strcpy(dll+n,dll[n-1]=='\\'?"VXDDRAW.DLL":"\\VXDDRAW.DLL");h=LoadLibraryA(dll);}
  hal_phases=h?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(h,"VX_D3D_QueryPhases"):0;}
 d8=LoadLibrary("d3d8.dll");create=d8?(void*(__stdcall*)(U32))GetProcAddress(d8,"Direct3DCreate8"):0;
 if(!create){say("d3d8.dll / Direct3DCreate8 not found (DirectX 8 or later is needed)\n");return 1;}
 d3d=create(220);if(!d3d)d3d=create(120);
 if(!d3d){say("Direct3DCreate8 failed\n");return 1;}
 memset(&mode,0,sizeof(mode));((F2P)VT(d3d,D3D_MODE))(d3d,0,&mode);bbfmt=mode.format;
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wp;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="VXEP";wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClass(&wc);
 {RECT r;r.left=0;r.top=0;r.right=WIN;r.bottom=WIN;AdjustWindowRect(&r,WS_OVERLAPPED|WS_CAPTION,FALSE);
  wnd=CreateWindow("VXEP","VXEP",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,40,40,r.right-r.left,r.bottom-r.top,0,0,wc.hInstance,0);}
 SetWindowPos(wnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
 memset(&pp,0,sizeof(pp));pp.bw=WIN;pp.bh=WIN;pp.bfmt=mode.format;pp.bcount=1;pp.swap=3;pp.wnd=wnd;pp.windowed=1;pp.autoz=1;pp.zfmt=80;pp.flags=1;
 hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,0x20,&pp,&dev);
 say("display format %lu; CreateDevice(HAL, %dx%d windowed): %08lX\n",mode.format,WIN,WIN,(U32)hr);
 if(hr||!dev){((F0)VT(d3d,D3D_REL))(d3d);if(logf)fclose(logf);return 1;}
 phases();say("the HAL's kept-answer counts (QueryPhases 332-335): %s\n",ph_ok?"read (.552 or later)":"not reported (a driver before .552: the cases below are judged the same way)");
 if(make_vb(&vba,0)&&make_vb(&vbb,0)){test_follow(1,"support follows the state (STENCILENABLE on a D16 depth buffer before every odd quad)");
  test_follow(2,"the program follows the stage (COLORARG1 TFACTOR before every odd quad)");
  test_follow(3,"states changed and put back between draws");test_speed(0);test_speed(1);}else fails++;
 if(vba)((F0)VT(vba,D3D_REL))(vba);if(vbb)((F0)VT(vbb,D3D_REL))(vbb);
 ((F0)VT(dev,D3D_REL))(dev);((F0)VT(d3d,D3D_REL))(d3d);DestroyWindow(wnd);
 say("%s: %d wrong\n",fails?"FAILED":"PASSED",fails);
 if(logf)fclose(logf);
 return fails?2:0;
}
