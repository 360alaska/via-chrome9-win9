/* VX2PAT v1.2: (driver v10.60 expects every monochrome fill to match layout 0, phase 0,0, polarity normal, with and without a clip rectangle,
 * and in any order relative to colour fills; colour fills are sent straight to the engine only to study them)
 * VX2PAT v1.1: finds out exactly how the 2D engine lays out its 8x8 pattern fills (colour and monochrome), by sending requests
 * straight to the driver and reading the screen back. For each test rectangle it works out which of 8 pattern layouts (mirrors and
 * transposes), which phase (where the pattern starts relative to the screen corner), and for monochrome which polarity reproduces
 * every pixel. Writes VX2PAT.TXT. Needs ACCEL_PATTERN = 1 (otherwise the driver declines the request and the log says so).
 * v1.1: no display-mode change at all (v1.0 hung inside SetDisplayMode). It draws on the live desktop at its current depth and
 * repaints the desktop afterwards. To test 16 bits per pixel, set the desktop to 16 bit colour first. Esc aborts. */
#define COBJMACROS
#define DIRECTDRAW_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <io.h>
#include <stdarg.h>
#include <string.h>
#include "vx85598.h"
#include "vx855_2d.h"
static HANDLE dev=INVALID_HANDLE_VALUE;
static FILE *logfile;
static HWND win;
static LPDIRECTDRAW dd;
static DWORD depth,sw,sh,pitch,aborted,declined,problems; static int monoonly;
static void logline(const char *fmt,...) {
 va_list ap; va_start(ap,fmt); vfprintf(logfile,fmt,ap); va_end(ap); fflush(logfile); _commit(_fileno(logfile));
}
static void pump(void) {
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1;
}
static void wait_ms(DWORD ms) {DWORD end=GetTickCount()+ms; while(GetTickCount()<end&&!aborted) {pump(); Sleep(20);}}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_CLOSE) {aborted=1;return 0;}
 if(m==WM_ERASEBKGND) return 1;
 return DefWindowProc(w,m,a,b);
}
static DWORD maskv(void) {return depth==16?0xFFFFUL:0xFFFFFFUL;}
static int lock_surf(LPDIRECTDRAWSURFACE s,DDSURFACEDESC *d) {
 memset(d,0,sizeof(*d)); d->dwSize=sizeof(*d);
 return SUCCEEDED(IDirectDrawSurface_Lock(s,NULL,d,DDLOCK_WAIT,NULL));
}
static DWORD getpx(const DDSURFACEDESC *d,int x,int y) {
 BYTE *p=(BYTE*)d->lpSurface+(DWORD)y*d->lPitch;
 return depth==16?(DWORD)((WORD*)p)[x]:(((DWORD*)p)[x]&0xFFFFFFUL);
}
static void putpx(const DDSURFACEDESC *d,int x,int y,DWORD v) {
 BYTE *p=(BYTE*)d->lpSurface+(DWORD)y*d->lPitch;
 if(depth==16) ((WORD*)p)[x]=(WORD)v; else ((DWORD*)p)[x]=v;
}
static vx2d_request q;
static int submit(DWORD *rc) {
 DWORD got=0; *rc=99;
 return DeviceIoControl(dev,VX_IOCTL_2D_SUBMIT,&q,sizeof(q),rc,4,&got,NULL)&&got==4;
}
static void mkreq(DWORD op,int x,int y,int w,int h) {
 memset(&q,0,sizeof(q)); q.cb=sizeof(q); q.abi=VX2D_ABI; q.op=op; q.flags=VX2D_DDRAW; q.bpp=depth;
 q.dst.offset=0; q.dst.pitch=pitch; q.dst.width=sw; q.dst.height=sh; q.src=q.dst;
 q.dx=x; q.dy=y; q.width=w; q.height=h; q.rop=0xF0;
}
static int vmap(int v,int a,int b) { /* pattern cell used at phase position (a,b) for layout variant v */
 switch(v) {
  case 0: return b*8+a;
  case 1: return b*8+(7-a);
  case 2: return (7-b)*8+a;
  case 3: return (7-b)*8+(7-a);
  case 4: return a*8+b;
  case 5: return a*8+(7-b);
  case 6: return (7-a)*8+b;
  default: return (7-a)*8+(7-b);
 }
}
static const char *vname[8]={"normal","mirrored left-right","mirrored up-down","rotated 180","transposed","transposed + up-down","transposed + left-right","transposed rotated 180"};
#define RWID 136
#define RHGT 128
#define MX 16
#define MY 16
#define MW 200
#define MH 160
static short obs[RHGT*RWID]; /* what each pixel shows: colour pattern cell index, or mono fg(1)/bg(0); -1 = something else */
static DWORD pat[64];
static const BYTE rowbits[8]={0x9D,0x31,0x6A,0xC5,0x0F,0xE2,0x58,0xB7};
static void clearregion(LPDIRECTDRAWSURFACE s,DWORD v) {
 DDSURFACEDESC d; int x,y;
 if(!lock_surf(s,&d)) return;
 for(y=MY;y<MY+MH;y++) for(x=MX;x<MX+MW;x++) putpx(&d,x,y,v&maskv());
 IDirectDrawSurface_Unlock(s,d.lpSurface);
}
/* fit the observed rectangle to a layout; mono!=0 also tries both polarities. Logs the matching combinations. */
static int fit(const char *what,int rx,int ry,int mono) {
 int v,px,py,pol,x,y,found=0,pols=mono?2:1;
 for(v=0;v<8;v++) for(py=0;py<8;py++) for(px=0;px<8;px++) for(pol=0;pol<pols;pol++) {
  int ok=1;
  for(y=0;y<RHGT&&ok;y++) for(x=0;x<RWID;x++) {
   int idx=vmap(v,(rx+x+px)&7,(ry+y+py)&7),want;
   if(mono) want=((rowbits[idx>>3]>>(idx&7))&1)^pol; else want=idx;
   if(obs[y*RWID+x]!=want) {ok=0; break;}
  }
  if(ok) {
   if(found<6) logline("    MATCH: layout %d (%s), phase %d,%d (pattern cell 0 sits %d,%d pixels from the screen corner)%s\n",v,vname[v],px,py,(8-px)&7,(8-py)&7,mono?(pol?", polarity inverted (1 bits show the BACKGROUND colour)":", polarity normal (1 bits show the foreground colour)"):"");
   found++;
  }
 }
 if(!found) {
  int bad=0,other=0,first=-1;
  for(y=0;y<RHGT;y++) for(x=0;x<RWID;x++) {if(obs[y*RWID+x]<0) other++;}
  (void)bad; (void)first;
  logline("    NO MATCH for %s at %d,%d: %d pixels hold a value that is not in the pattern. Observed top-left 16x4 (%s):\n",what,rx,ry,other,mono?"1 = foreground, 0 = background, . = other":"cell index, .. = other");
  for(y=0;y<4;y++) {
   char line[200]; int n=0;
   for(x=0;x<16;x++) {int o=obs[y*RWID+x]; if(mono) n+=sprintf(line+n,"%c",o<0?'.':(o?'1':'0')); else {if(o<0) n+=sprintf(line+n,".. "); else n+=sprintf(line+n,"%02d ",o);}}
   logline("      %s\n",line);
  }
  problems++;
 }
 return found;
}
static void casecolour(LPDIRECTDRAWSURFACE s,int dx,int dy) {
 DWORD rc; DDSURFACEDESC d; int x,y,i,stray=0,rx=MX+8+dx,ry=MY+8+dy;
 clearregion(s,0xFFFFFFUL);
 mkreq(VX2D_COLOR,rx,ry,RWID,RHGT); memcpy(q.pattern,pat,sizeof(pat)); q.fg=q.bg=pat[0];
 logline("  colour pattern fill at screen %d,%d (size %dx%d)\n",rx,ry,RWID,RHGT);
 if(!submit(&rc)) {logline("    the driver ioctl failed\n"); problems++; return;}
 if(rc!=VX2D_DONE) {logline("    engine result %lu (1 = done, 0 = declined: is ACCEL_PATTERN set to 1 and the driver reloaded?)\n",rc); declined++; return;}
 if(!lock_surf(s,&d)) {logline("    lock failed\n"); problems++; return;}
 for(y=MY;y<MY+MH;y++) for(x=MX;x<MX+MW;x++) {
  DWORD v=getpx(&d,x,y);
  if(x>=rx&&x<rx+RWID&&y>=ry&&y<ry+RHGT) {
   short o=-1; for(i=0;i<64;i++) if((pat[i]&maskv())==v) {o=(short)i; break;}
   obs[(y-ry)*RWID+(x-rx)]=o;
  } else if(v!=(0xFFFFFFUL&maskv())) stray++;
 }
 IDirectDrawSurface_Unlock(s,d.lpSurface);
 if(stray) {logline("    %d pixels OUTSIDE the rectangle were changed\n",stray); problems++;}
 fit("colour pattern",rx,ry,0);
}
static void casemono(LPDIRECTDRAWSURFACE s,int dx,int dy) {
 DWORD rc,fg=depth==16?0xF800UL:0x00C83214UL,bg=depth==16?0x07FFUL:0x001464DCUL,clr=maskv(); DDSURFACEDESC d; int x,y,stray=0,rx=MX+8+dx,ry=MY+8+dy;
 clearregion(s,clr);
 mkreq(VX2D_MONO,rx,ry,RWID,RHGT); q.fg=fg; q.bg=bg;
 for(y=0;y<8;y++) q.mono[y/4]|=(DWORD)rowbits[y]<<((3-(y&3))*8); /* the same packing the driver's GDI hook uses */
 logline("  monochrome pattern fill at screen %d,%d (size %dx%d), foreground %08lX background %08lX\n",rx,ry,RWID,RHGT,fg,bg);
 if(!submit(&rc)) {logline("    the driver ioctl failed\n"); problems++; return;}
 if(rc!=VX2D_DONE) {logline("    engine result %lu (1 = done, 0 = declined: is ACCEL_PATTERN set to 1 and the driver reloaded?)\n",rc); declined++; return;}
 if(!lock_surf(s,&d)) {logline("    lock failed\n"); problems++; return;}
 for(y=MY;y<MY+MH;y++) for(x=MX;x<MX+MW;x++) {
  DWORD v=getpx(&d,x,y);
  if(x>=rx&&x<rx+RWID&&y>=ry&&y<ry+RHGT) obs[(y-ry)*RWID+(x-rx)]=(v==(fg&maskv()))?1:(v==(bg&maskv()))?0:-1;
  else if(v!=clr) stray++;
 }
 IDirectDrawSurface_Unlock(s,d.lpSurface);
 if(stray) {logline("    %d pixels OUTSIDE the rectangle were changed\n",stray); problems++;}
 fit("monochrome pattern",rx,ry,1);
}
static void casemonoclip(LPDIRECTDRAWSURFACE s,int dx,int dy,int cl,int ct,int cr,int cb) {
 DWORD rc,fg=depth==16?0xF800UL:0x00C83214UL,bg=depth==16?0x07FFUL:0x001464DCUL,clr=maskv(); DDSURFACEDESC d; int x,y,bad=0,rx=MX+8+dx,ry=MY+8+dy,n=0;
 clearregion(s,clr);
 mkreq(VX2D_MONO,rx,ry,RWID,RHGT); q.fg=fg; q.bg=bg; q.flags|=VX2D_CLIP; q.left=rx+cl; q.top=ry+ct; q.right=rx+cr; q.bottom=ry+cb;
 for(y=0;y<8;y++) q.mono[y/4]|=(DWORD)rowbits[y]<<((3-(y&3))*8);
 logline("  clipped monochrome fill at screen %d,%d size %dx%d, clip %d,%d to %d,%d (right/bottom exclusive)\n",rx,ry,RWID,RHGT,q.left,q.top,q.right,q.bottom);
 if(!submit(&rc)) {logline("    the driver ioctl failed\n"); problems++; return;}
 if(rc!=VX2D_DONE) {logline("    engine result %lu (1 = done, 0 = declined)\n",rc); declined++; return;}
 if(!lock_surf(s,&d)) {logline("    lock failed\n"); problems++; return;}
 for(y=MY;y<MY+MH;y++) for(x=MX;x<MX+MW;x++) {
  DWORD v=getpx(&d,x,y),want;
  if(x>=q.left&&x<q.right&&y>=q.top&&y<q.bottom&&x>=rx&&x<rx+RWID&&y>=ry&&y<ry+RHGT) want=((rowbits[y&7]>>(x&7))&1)?(fg&maskv()):(bg&maskv()); else want=clr;
  if(v!=want) {if(bad<4) logline("    pixel %d,%d is %08lX, expected %08lX\n",x,y,v,want); bad++;}
  n++;
 }
 IDirectDrawSurface_Unlock(s,d.lpSurface);
 if(bad) {logline("    FAIL: %d of %d pixels differ from the expected screen-anchored pattern\n",bad,n); problems++;} else logline("    OK: all %d pixels as expected (clip respected, pattern anchored to the screen)\n",n);
}
static void plainops(void) { /* a plain engine fill and a copy, as GDI would send them before a pattern fill */
 DWORD rc; mkreq(VX2D_FILL,40,40,136,128); q.fg=0x00123456UL&maskv(); q.rop=0xF0;
 if(!submit(&rc)||rc!=VX2D_DONE) logline("  plain fill: result %lu\n",rc); else logline("  plain engine fill done\n");
 mkreq(VX2D_COPY,60,50,100,100); q.sx=40; q.sy=40; q.rop=0xCC;
 if(!submit(&rc)||rc!=VX2D_DONE) logline("  plain copy: result %lu\n",rc); else logline("  plain engine copy done\n");
}
static void testdesktop(const vx_status_t *st) {
 HRESULT hr; LPDIRECTDRAWSURFACE prim=NULL; DDSURFACEDESC d; static const int offs[][2]={{0,0},{1,0},{0,1},{3,5},{7,7},{5,2}}; int i;
 depth=st->bpp; sw=st->width; sh=st->height; pitch=st->video_pitch;
 logline("\n== desktop %lux%lux%lu pitch %lu (no mode change) ==\n",sw,sh,depth,pitch);
 if(depth!=16&&depth!=32) {logline("this test needs a 16 or 32 bit desktop\n"); problems++; return;}
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
 hr=IDirectDraw_CreateSurface(dd,&d,&prim,NULL); logline("CreateSurface(primary): %08lX\n",(DWORD)hr);
 if(FAILED(hr)||!prim) {problems++; return;}
 {DDSURFACEDESC l; if(lock_surf(prim,&l)) {logline("primary lock: pitch %ld, bits %lu\n",(long)l.lPitch,l.ddpfPixelFormat.dwRGBBitCount); if((DWORD)l.lPitch!=pitch) {logline("pitch differs from the driver's; stopping\n"); problems++; IDirectDrawSurface_Unlock(prim,l.lpSurface); IDirectDrawSurface_Release(prim); return;} IDirectDrawSurface_Unlock(prim,l.lpSurface);}}
 logline("MONOCHROME pattern: asymmetric 8x8 bitmap, bytes %02X %02X %02X %02X %02X %02X %02X %02X (bit 0 = leftmost pixel in the request)\n",rowbits[0],rowbits[1],rowbits[2],rowbits[3],rowbits[4],rowbits[5],rowbits[6],rowbits[7]);
 logline("-- monochrome first, before any colour fill --\n");
 for(i=0;i<6&&!aborted;i++) {casemono(prim,offs[i][0],offs[i][1]); wait_ms(150);}
 if(depth==32&&!monoonly) {
  logline("-- a plain fill and a plain copy, then monochrome again --\n");
  plainops();
  for(i=0;i<6&&!aborted;i++) {casemono(prim,offs[i][0],offs[i][1]); wait_ms(150);}
  logline("-- colour fills, then monochrome again (does a colour fill disturb the next monochrome fill?) --\n");
  logline("COLOUR pattern (32 bits per pixel): 64 distinct colours\n");
  for(i=0;i<6&&!aborted;i++) {casecolour(prim,offs[i][0],offs[i][1]); wait_ms(150);}
  for(i=0;i<6&&!aborted;i++) {casemono(prim,offs[i][0],offs[i][1]); wait_ms(150);}
 }
 logline("-- monochrome with a clip rectangle --\n");
 casemonoclip(prim,0,0,10,7,100,90); wait_ms(150);
 casemonoclip(prim,3,5,0,0,50,128); wait_ms(150);
 casemonoclip(prim,7,7,33,41,136,60); wait_ms(150);
 casemonoclip(prim,1,0,5,3,6,125); wait_ms(150);
 IDirectDrawSurface_Release(prim);
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show_) {
 WNDCLASS wc; vx_status_t s; DWORD got,desk,i,j; HMODULE mod; HRESULT hr; char text[300];
 typedef HRESULT (WINAPI *CREATE)(GUID*,LPDIRECTDRAW*,IUnknown*);
 (void)prev;(void)show_;
 logfile=fopen("VX2PAT.TXT","w"); if(!logfile) return 1;
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE||!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC) {logline("The VX85598 driver is not active.\n"); goto cleanup;}
 monoonly=cmd&&strstr(cmd,"mono")!=NULL; desk=s.bpp; logline("VX2PAT v1.4 / driver v10.61\nDesktop %lux%lux%lu\n",(DWORD)s.width,(DWORD)s.height,desk);
 for(i=0;i<64;i++) {pat[i]=(((i*37UL+11)&255)<<16)|(((i*91UL+5)&255)<<8)|((i*53UL+17)&255); if(pat[i]==0xFFFFFFUL) pat[i]^=0x010101UL;}
 for(i=0;i<64;i++) for(j=i+1;j<64;j++) if(pat[i]==pat[j]) logline("internal: pattern colours %lu and %lu are equal\n",i,j);
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2PAT"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClass(&wc);
 win=CreateWindowEx(0,wc.lpszClassName,"VX2PAT",WS_POPUP,0,0,1,1,NULL,NULL,hi,NULL);
 if(!win) goto cleanup;
 mod=LoadLibraryA("DDRAW.DLL"); if(!mod) goto cleanup;
 {CREATE create=(CREATE)GetProcAddress(mod,"DirectDrawCreate"); if(!create) goto cleanup; hr=create(NULL,&dd,NULL); logline("DirectDrawCreate: %08lX\n",(DWORD)hr); if(FAILED(hr)) {dd=NULL; goto cleanup;}}
 hr=IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); logline("SetCooperativeLevel(normal): %08lX\n",(DWORD)hr); if(FAILED(hr)) goto cleanup;
 testdesktop(&s);
 logline("\nrepainting the desktop\n");
 RedrawWindow(NULL,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
 logline("\nSUMMARY: %lu requests declined by the engine, %lu problems\n",declined,problems);
cleanup:
 if(dd) IDirectDraw_Release(dd);
 if(win) DestroyWindow(win);
 if(dev!=INVALID_HANDLE_VALUE) CloseHandle(dev);
 logline("Finished%s.\n",aborted?" (aborted)":""); fclose(logfile);
 sprintf(text,"Finished. Send me VX2PAT.TXT.%s",declined?" (The engine declined some requests: set ACCEL_PATTERN to 1 and reboot first.)":"");
 MessageBox(NULL,text,"VX2PAT",MB_OK);
 return 0;
}
