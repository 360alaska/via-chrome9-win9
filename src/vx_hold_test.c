/* VXHOLD v1.2 (driver .551): the hold is always on (ACCEL_D3D_HOLD removed); the log prints the HAL's word, no key named.
 * VXHOLD v1.1 (driver .550): v1.0 locked a pool-DEFAULT texture, which Direct3D 8 refuses for a texture made without D3DUSAGE_DYNAMIC
 * ("texture lock failed" on the board, both settings): the DEFAULT case now refills a system-memory texture and copies it in with
 * UpdateTexture (a blit into video memory). v1.0 also read ACCEL_D3D_HOLD before the device existed (the HAL reads the setting when the
 * device is made: "0 (off)" with the key at 1); it is read after CreateDevice now.
 * VXHOLD v1.0 (driver .550): the HAL keeps its draw packets across DrawPrimitives2 calls (ACCEL_D3D_HOLD, default 1) and sends them
 * before anything that reads, writes or moves what they draw. Direct3D 8 on this DirectX 7 driver makes one DrawPrimitives2 call per
 * vertex buffer, so 64 quads drawn from two vertex buffers in turn are 64 calls; the same quads from one buffer are one call.
 *  1. Lock: the 64 quads in 64 calls against one call, read back through a back-buffer Lock: the same picture.
 *  2. Texture order: a quad with a texture, the texture locked and refilled, a second quad with it: the first quad keeps the old texels
 *     (its held packet went before the Lock). Once with a video-memory texture (pool DEFAULT: the Lock reaches the driver) and once with a
 *     managed one (the runtime updates the driver's copy with a blit).
 *  3. Present: the 64 calls, then Present (a blit to the window), read from the screen: every quad there.
 *  4. Speed: 300 frames of the 64-call scene; ms a frame and the HAL's counts (QueryPhases 316-331).
 * Run it twice for an A/B: as installed (ACCEL_D3D_HOLD 1), then with ACCEL_D3D_HOLD=0 in the driver's registry key and a restart. The
 * log says which. Results to the screen and VXHOLD.LOG beside the program. Run it on the desktop at 16 or 32 bpp; it opens a 256x256
 * window for about half a minute. VXD3DMON must not be running. */
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
static void *dev,*vba,*vbb,*vbt;static U32 bbfmt;static HWND wnd;
static DWORD (WINAPI *hal_phases)(DWORD *,DWORD);
static DWORD ph[332];static int ph_ok;
static void phases(void) {ph_ok=hal_phases&&hal_phases(ph,332)==332;}
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
static int read_screen(U32 *out) { /* the window's client area as GDI sees it after Present */
 HDC wdc=GetDC(wnd),mdc;HBITMAP bm,old;BITMAPINFO bi;void *bits=0;int ok=0;U32 k;
 if(!wdc)return 0;mdc=CreateCompatibleDC(wdc);
 memset(&bi,0,sizeof(bi));bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=WIN;bi.bmiHeader.biHeight=-WIN;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
 bm=CreateDIBSection(wdc,&bi,DIB_RGB_COLORS,&bits,0,0);
 if(mdc&&bm&&bits){old=(HBITMAP)SelectObject(mdc,bm);if(BitBlt(mdc,0,0,WIN,WIN,wdc,0,0,SRCCOPY)){GdiFlush();for(k=0;k<WIN*WIN;k++)out[k]=((U32*)bits)[k]&0xFFFFFFUL;ok=1;}SelectObject(mdc,old);}
 if(bm)DeleteObject(bm);if(mdc)DeleteDC(mdc);ReleaseDC(wnd,wdc);return ok;
}
static int differs(U32 a,U32 b) {int k;for(k=0;k<24;k+=8){int d=(int)((a>>k)&255)-(int)((b>>k)&255);if(d<-12||d>12)return 1;}return 0;}
static int ndiff(const U32 *a,const U32 *b) {int n=0,k;for(k=0;k<WIN*WIN;k++)if(differs(a[k],b[k]))n++;return n;}
static int lit(const U32 *p) {int n=0,k;for(k=0;k<WIN*WIN;k++)if(p[k])n++;return n;}
static U32 ref[WIN*WIN],cur[WIN*WIN];
static void plain_states(void) {rs(136,0);rs(7,0);rs(14,0);rs(27,0);rs(28,0);rs(22,1);((F1)VT(dev,DEV_SETVS))(dev,TLF);((F2P)VT(dev,DEV_SETTEX))(dev,0,0);
 tss(1,2);tss(2,0);tss(4,2);tss(5,0);} /* COLOROP / ALPHAOP SELECTARG1 DIFFUSE (D3DTA_DIFFUSE 0) */
static void stream(void *vb) {((FSTREAM)VT(dev,DEV_SETSTREAM))(dev,0,vb,sizeof(TL));}
static void draw_one(void) {((F0)VT(dev,DEV_BEGIN))(dev);clear(0xFF000000UL);plain_states();stream(vba);((FDP)VT(dev,DEV_DRAWPRIM))(dev,4,0,NQ*2);((F0)VT(dev,DEV_END))(dev);}
static void draw_many(void) {U32 q;((F0)VT(dev,DEV_BEGIN))(dev);clear(0xFF000000UL);plain_states();
 for(q=0;q<NQ;q++){stream(q&1?vbb:vba);((FDP)VT(dev,DEV_DRAWPRIM))(dev,4,q*6,2);} /* another vertex buffer each quad: a DrawPrimitives2 call each */
 ((F0)VT(dev,DEV_END))(dev);}
static void counts(const char *what,const DWORD *a) {
 if(!ph_ok){say("   %s: the HAL's counts not readable (QueryPhases)\n",what);return;}
 say("   %s: hold %lu; calls ended holding %lu, packets held %lu, held packets sent by a later call %lu, by a callback %lu (Lock %lu, Blt %lu, Flip %lu, DestroySurface %lu); dropped %lu refused/busy, %lu unsent; lock waits timed out %lu\n",
  what,ph[330],ph[316]-a[316],ph[317]-a[317],ph[318]-a[318],ph[320]-a[320],ph[322]-a[322],ph[323]-a[323],ph[324]-a[324],ph[325]-a[325],ph[319]-a[319],ph[321]-a[321],ph[329]-a[329]);
}
static DWORD before[332];
static void mark(void) {phases();memcpy(before,ph,sizeof(ph));}
/* 1 */
static void test_lock(void) {
 int d,l0,l1;
 say("1. the 64 quads in 64 calls against one call, read back through a back-buffer Lock\n");
 draw_one();if(!read_bb(ref)){say("   back buffer lock failed\n");fails++;return;}present();
 mark();draw_many();if(!read_bb(cur)){say("   back buffer lock failed\n");fails++;return;}phases();present();
 d=ndiff(ref,cur);l0=lit(ref);l1=lit(cur);
 say("   one call: %d pixels lit; 64 calls: %d lit; %d differ: %s\n",l0,l1,d,d||l0<NQ*700?"WRONG":"ok");if(d||l0<NQ*700)fails++;
 counts("   ",before);
}
/* 2 */
static int fill_tex(void *t,U32 c) {LR lr;U32 x,y;
 if(((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(t,TEX_LOCK))(t,0,&lr,0,0)){say("   texture lock failed\n");return 0;}
 for(y=0;y<64;y++){U32 *row=(U32*)((unsigned char*)lr.bits+y*lr.pitch);for(x=0;x<64;x++)row[x]=c;}
 ((F1)VT(t,TEX_UNLOCK))(t,0);return 1;}
static U32 cell(const U32 *p,U32 q) {return p[((q>>3)*32+16)*WIN+(q&7)*32+16];}
static void *mk_tex(U32 pool) {void *t=0;
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,DEV_CREATETEX))(dev,64,64,1,0,22,pool,&t)||!t){say("   CreateTexture 64x64 failed (pool %lu)\n",pool);return 0;}
 return t;}
/* pool DEFAULT (0): the texels go in by UpdateTexture from a system-memory copy (a blit into video memory); MANAGED (1): locked directly */
static int set_tex(void *t,void *sys,U32 c) {
 if(!sys)return fill_tex(t,c);
 if(!fill_tex(sys,c))return 0;
 if(((HRESULT(__stdcall*)(void*,void*,void*))VT(dev,DEV_UPDATETEX))(dev,sys,t)){say("   UpdateTexture failed\n");return 0;}
 return 1;}
static void test_texture(U32 pool) {
 void *t,*sys=0;U32 a,b;int ok;
 say("2. texture order, pool %s: quad 0 with the texture red, the texture refilled green%s, quad 9 with it\n",pool?"MANAGED":"DEFAULT",pool?"":" (UpdateTexture from system memory)");
 t=mk_tex(pool);if(!t){fails++;return;}
 if(!pool){sys=mk_tex(2);if(!sys){fails++;((F0)VT(t,D3D_REL))(t);return;}}
 if(!set_tex(t,sys,0xFFFF0000UL)){fails++;goto out;}
 mark();
 ((F0)VT(dev,DEV_BEGIN))(dev);clear(0xFF000000UL);plain_states();((F2P)VT(dev,DEV_SETTEX))(dev,0,t);tss(1,2);tss(2,2);tss(4,2);tss(5,2); /* SELECTARG1 TEXTURE */
 stream(vbt);((FDP)VT(dev,DEV_DRAWPRIM))(dev,4,0,2);
 ((F0)VT(dev,DEV_END))(dev);
 if(!set_tex(t,sys,0xFF00FF00UL)){fails++;goto out;}
 ((F0)VT(dev,DEV_BEGIN))(dev);stream(vbt);((FDP)VT(dev,DEV_DRAWPRIM))(dev,4,9*6,2);((F0)VT(dev,DEV_END))(dev);
 if(!read_bb(cur)){say("   back buffer lock failed\n");fails++;goto out;}phases();present();
 a=cell(cur,0);b=cell(cur,9);ok=!differs(a,0xFF0000UL)&&!differs(b,0x00FF00UL);
 say("   quad 0 %06lX (red expected), quad 9 %06lX (green expected): %s\n",a,b,ok?"ok":"WRONG");if(!ok)fails++;
 counts("   ",before);
out:
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F0)VT(t,D3D_REL))(t);if(sys)((F0)VT(sys,D3D_REL))(sys);
}
/* 3 */
static void test_present(void) {
 int d,l;MSG m;
 say("3. the 64 calls, then Present, read from the screen\n");
 draw_one();present();Sleep(50);while(PeekMessage(&m,0,0,0,PM_REMOVE))DispatchMessage(&m);
 if(!read_screen(ref)){say("   screen read failed\n");fails++;return;}
 mark();draw_many();present();phases();Sleep(50);while(PeekMessage(&m,0,0,0,PM_REMOVE))DispatchMessage(&m);
 if(!read_screen(cur)){say("   screen read failed\n");fails++;return;}
 d=ndiff(ref,cur);l=lit(cur);
 say("   %d pixels lit on the screen, %d differ from the one-call picture: %s\n",l,d,d||l<NQ*700?"WRONG":"ok");if(d||l<NQ*700)fails++;
 counts("   ",before);
}
/* 4 */
static void test_speed(void) {
 DWORD t0,t1;int k;
 say("4. speed: 300 frames of the 64-call scene\n");
 for(k=0;k<20;k++){draw_many();present();}
 mark();t0=GetTickCount();for(k=0;k<300;k++){draw_many();present();}t1=GetTickCount();phases();
 say("   %.2f ms a frame (%lu ms)\n",(double)(t1-t0)/300.0,t1-t0);
 counts("   ",before);
}
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProc(h,m,w,l);}
int main(void) {
 HMODULE d8;void *(__stdcall *create)(U32);void *d3d;MODE mode;PP pp;WNDCLASS wc;HRESULT hr;char path[MAX_PATH],*s;
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXHOLD.LOG");logf=fopen(path,"w");
 say("VXHOLD v1.2 (driver .551): draw packets held across DrawPrimitives2 calls\n");
 {char dll[MAX_PATH];UINT n=GetSystemDirectoryA(dll,MAX_PATH-16);HMODULE h=0;
  if(n&&n<MAX_PATH-16){strcpy(dll+n,dll[n-1]=='\\'?"VXDDRAW.DLL":"\\VXDDRAW.DLL");h=LoadLibraryA(dll);}
  hal_phases=h?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(h,"VX_D3D_QueryPhases"):0;}
 d8=LoadLibrary("d3d8.dll");create=d8?(void*(__stdcall*)(U32))GetProcAddress(d8,"Direct3DCreate8"):0;
 if(!create){say("d3d8.dll / Direct3DCreate8 not found (DirectX 8 or later is needed)\n");return 1;}
 d3d=create(220);if(!d3d)d3d=create(120);
 if(!d3d){say("Direct3DCreate8 failed\n");return 1;}
 memset(&mode,0,sizeof(mode));((F2P)VT(d3d,D3D_MODE))(d3d,0,&mode);bbfmt=mode.format;
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wp;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="VXHOLD";wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClass(&wc);
 {RECT r;r.left=0;r.top=0;r.right=WIN;r.bottom=WIN;AdjustWindowRect(&r,WS_OVERLAPPED|WS_CAPTION,FALSE);
  wnd=CreateWindow("VXHOLD","VXHOLD",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,40,40,r.right-r.left,r.bottom-r.top,0,0,wc.hInstance,0);}
 SetWindowPos(wnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
 memset(&pp,0,sizeof(pp));pp.bw=WIN;pp.bh=WIN;pp.bfmt=mode.format;pp.bcount=1;pp.swap=3;pp.wnd=wnd;pp.windowed=1;pp.autoz=1;pp.zfmt=80;pp.flags=1;
 hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,0x20,&pp,&dev);
 say("display format %lu; CreateDevice(HAL, %dx%d windowed): %08lX\n",mode.format,WIN,WIN,(U32)hr);
 if(hr||!dev){((F0)VT(d3d,D3D_REL))(d3d);if(logf)fclose(logf);return 1;}
 phases();say("the HAL's QueryPhases: %s; packets held across calls: %s\n",ph_ok?"read":"not readable (a driver before .550?)",ph_ok?(ph[330]?"1 (on)":"0 (off)"):"?");
 if(make_vb(&vba,0)&&make_vb(&vbb,0)&&make_vb(&vbt,1)){test_lock();test_texture(0);test_texture(1);test_present();test_speed();}else fails++;
 if(vba)((F0)VT(vba,D3D_REL))(vba);if(vbb)((F0)VT(vbb,D3D_REL))(vbb);if(vbt)((F0)VT(vbt,D3D_REL))(vbt);
 ((F0)VT(dev,D3D_REL))(dev);((F0)VT(d3d,D3D_REL))(d3d);DestroyWindow(wnd);
 say("%s: %d wrong\n",fails?"FAILED":"PASSED",fails);
 if(logf)fclose(logf);
 return fails?2:0;
}
