/* VX2CONF v1.0: automatic confirmation of the display-start fix (v10.63). No keys needed.
 * For the six modes (640x480, 800x600, 1024x768 at 16 and 32 bpp) it runs the sequence that used to shift the desktop:
 *   A) a DirectDraw flip chain at 640x480x16, then SetDisplayMode to the test mode
 *   B) a DirectDraw flip chain at the test mode, then SetDisplayMode to the same mode
 * and draws a test card (white frame, a coloured marker in each corner, 8 colour bars, a cross, a label "SHOT nn").
 * Each card waits for you: take your screenshot, then click GOOD, BAD, SKIP or QUIT (keys G, B, S, Esc also work). The label tells which shot it is.
 * Per card the program logs the display start registers (CR0C/0D/34/48), the pitch registers and the driver flip state, and saves the
 * frame buffer content as VX2CONF_nn.BMP (half size). The scan-out position itself cannot be read back by software, so the screenshots are the proof for that part.
 * Command line: "from=11" starts at that shot (default 1). The desktop mode is restored after every card by default (a run that switched DirectDraw modes back to back crashed DDHELP); "norestore" turns that off; "fresh" releases and re-creates the DirectDraw object after every card. A crash is caught and logged with the faulting module and address.
 * Files written in the current folder: VX2CONF.TXT (every value and a PASS/FAIL table) and VX2CONF_01..24.BMP. */
#define COBJMACROS
#define DIRECTDRAW_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <io.h>
#include "vx85598.h"
#include "vx855_flip.h"
static LPDIRECTDRAW dd; static HANDLE dev=INVALID_HANDLE_VALUE; static FILE *lf;
static void logl(const char *fmt,...) {
 if(!lf) return; {va_list a; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a);} fflush(lf); _commit(_fileno(lf));
}
static char stepname[80];
static LONG WINAPI crashfilter(EXCEPTION_POINTERS *e) {
 MEMORY_BASIC_INFORMATION mbi; char mod[260]; DWORD a=(DWORD)e->ExceptionRecord->ExceptionAddress; mod[0]=0;
 if(VirtualQuery((void*)a,&mbi,sizeof(mbi))&&mbi.AllocationBase) GetModuleFileNameA((HMODULE)mbi.AllocationBase,mod,sizeof(mod));
 logl("\nCRASH during \"%s\": exception %08lX at %08lX in %s (module base %08lX, offset %08lX)\n",stepname,(DWORD)e->ExceptionRecord->ExceptionCode,a,mod[0]?mod:"(unknown)",(DWORD)mbi.AllocationBase,a-(DWORD)mbi.AllocationBase);
 logl("  EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX ESI=%08lX EDI=%08lX EBP=%08lX ESP=%08lX\n",e->ContextRecord->Eax,e->ContextRecord->Ebx,e->ContextRecord->Ecx,e->ContextRecord->Edx,e->ContextRecord->Esi,e->ContextRecord->Edi,e->ContextRecord->Ebp,e->ContextRecord->Esp);
 if(e->ExceptionRecord->NumberParameters>=2) logl("  access type %lu address %08lX\n",(DWORD)e->ExceptionRecord->ExceptionInformation[0],(DWORD)e->ExceptionRecord->ExceptionInformation[1]);
 if(lf) {fclose(lf); lf=NULL;}
 return EXCEPTION_EXECUTE_HANDLER;
}
static int modeops;
static void memlog(const char *when) {
 MEMORYSTATUS m; m.dwLength=sizeof(m); GlobalMemoryStatus(&m);
 logl("  mem [%s] mode operations so far %d: physical free %lu KB of %lu KB, page file free %lu KB, virtual free %lu KB, load %lu%%\n",when,modeops,m.dwAvailPhys/1024,m.dwTotalPhys/1024,m.dwAvailPageFile/1024,m.dwAvailVirtual/1024,m.dwMemoryLoad);
}
static void step(const char *n) {strncpy(stepname,n,79); stepname[79]=0; logl("  > %s\n",n);}
static volatile int lastkey,clicked; static volatile POINT clickpt;
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_ERASEBKGND) return 1;
 if(m==WM_SETCURSOR) {SetCursor(LoadCursor(NULL,IDC_ARROW)); return TRUE;}
 if(m==WM_KEYDOWN||m==WM_SYSKEYDOWN||m==WM_CHAR) {lastkey=(int)a; return 0;}
 if(m==WM_LBUTTONDOWN) {clickpt.x=(short)LOWORD(b); clickpt.y=(short)HIWORD(b); clicked=1; return 0;}
 return DefWindowProc(w,m,a,b);
}
static void pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m);DispatchMessage(&m);}}
static void wait_ms(DWORD ms) {DWORD e=GetTickCount()+ms; while(GetTickCount()<e) {pump(); Sleep(20);}}
static DWORD rgb(DWORD bpp,int r,int g,int b) {return bpp==16?(DWORD)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)):(DWORD)((r<<16)|(g<<8)|b);}
static void fill(LPDIRECTDRAWSURFACE s,DWORD c,int x0,int y0,int x1,int y1) {
 RECT r; DDBLTFX f; SetRect(&r,x0,y0,x1,y1); memset(&f,0,sizeof(f)); f.dwSize=sizeof(f); f.dwFillColor=c;
 IDirectDrawSurface_Blt(s,&r,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f);
}
static void label(LPDIRECTDRAWSURFACE s,const char *t,int x,int y,COLORREF fg,COLORREF bg) {
 HDC dc; if(SUCCEEDED(IDirectDrawSurface_GetDC(s,&dc))) {SetBkMode(dc,OPAQUE); SetBkColor(dc,bg); SetTextColor(dc,fg); TextOut(dc,x,y,t,(int)strlen(t)); IDirectDrawSurface_ReleaseDC(s,dc);}
}
static void flipcycle(void) {
 DDSURFACEDESC s; DDSCAPS caps; LPDIRECTDRAWSURFACE prim=NULL,back=NULL; int i; DWORD cols[3];
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT; s.dwBackBufferCount=2; s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
 if(FAILED(IDirectDraw_CreateSurface(dd,&s,&prim,NULL))) {logl("  flip chain: CreateSurface failed\n"); return;}
 memset(&caps,0,sizeof(caps)); caps.dwCaps=DDSCAPS_BACKBUFFER;
 if(FAILED(IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back))) {IDirectDrawSurface_Release(prim); logl("  flip chain: no back buffer\n"); return;}
 cols[0]=0x00DC2828UL; cols[1]=0x0028C83CUL; cols[2]=0x003246E6UL;
 for(i=0;i<6;i++) {
  DDBLTFX f; memset(&f,0,sizeof(f)); f.dwSize=sizeof(f); f.dwFillColor=cols[i%3]; IDirectDrawSurface_Blt(back,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f);
  if(FAILED(IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT))) {logl("  flip %d failed\n",i); break;}
  IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back);
 }
 IDirectDraw_FlipToGDISurface(dd);
 IDirectDrawSurface_Release(prim);
}
typedef struct {DWORD start,pitch_lo,pitch_hi,info_display,info_state,info_shift,info_flips,info_bios,ok;} snapv;
static int snapshot(snapv *v,const char *when) {
 static vx_crt_report rep; DWORD got=0,r[VXF_REPLY_DWORDS]; vxf_request q; int ok=1;
 memset(v,0,sizeof(*v));
 if(dev==INVALID_HANDLE_VALUE||!DeviceIoControl(dev,VX_IOCTL_CRT,NULL,0,&rep,sizeof(rep),&got,NULL)) {logl("  [%s] register dump failed\n",when); return 0;}
 v->start=((DWORD)(rep.cr[0x48]&0x1F)<<24)|((DWORD)rep.cr[0x34]<<16)|((DWORD)rep.cr[0x0C]<<8)|rep.cr[0x0D]; v->pitch_lo=rep.cr[0x13]; v->pitch_hi=rep.cr[0x35];
 q.cb=sizeof(q); q.cmd=VXF_INFO; q.arg=0; q.flags=0; memset(r,0,sizeof(r));
 if(DeviceIoControl(dev,VX_IOCTL_FLIP,&q,sizeof(q),r,4*VXF_REPLY_DWORDS,&got,NULL)) {v->info_display=r[VXF_R_DISPLAY]; v->info_state=r[VXF_R_STATE]; v->info_shift=r[VXF_R_SHIFT]; v->info_flips=r[VXF_R_FLIPS]; v->info_bios=r[VXF_R_BIOS];}
 else {ok=0; logl("  [%s] flip info failed\n",when);}
 v->ok=ok;
 logl("  [%s] CR48=%02X CR34=%02X CR0C=%02X CR0D=%02X start=%lu  CR13=%02X CR35=%02X  flip info: state %lu shift %lu display %lu flips %lu bios %lu\n",when,rep.cr[0x48],rep.cr[0x34],rep.cr[0x0C],rep.cr[0x0D],v->start,rep.cr[0x13],rep.cr[0x35],v->info_state,v->info_shift,v->info_display,v->info_flips,v->info_bios);
 return ok;
}
/* half-size 24-bit BMP of the primary surface as the CPU sees it */
static int savebmp(LPDIRECTDRAWSURFACE s,DWORD w,DWORD h,DWORD bpp,const char *name) {
 DDSURFACEDESC d; FILE *f; DWORD ow=w/2,oh=h/2,row=(ow*3+3)&~3UL,x,y; BITMAPFILEHEADER fh; BITMAPINFOHEADER ih; BYTE *line;
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); if(FAILED(IDirectDrawSurface_Lock(s,NULL,&d,DDLOCK_WAIT,NULL))) return 0;
 f=fopen(name,"wb"); if(!f) {IDirectDrawSurface_Unlock(s,NULL); return 0;}
 memset(&fh,0,sizeof(fh)); memset(&ih,0,sizeof(ih)); fh.bfType=0x4D42; fh.bfOffBits=sizeof(fh)+sizeof(ih); fh.bfSize=fh.bfOffBits+row*oh;
 ih.biSize=sizeof(ih); ih.biWidth=(LONG)ow; ih.biHeight=(LONG)oh; ih.biPlanes=1; ih.biBitCount=24; ih.biSizeImage=row*oh;
 fwrite(&fh,sizeof(fh),1,f); fwrite(&ih,sizeof(ih),1,f); line=(BYTE*)malloc(row);
 for(y=0;y<oh;y++) {
  BYTE *src=(BYTE*)d.lpSurface+(DWORD)(h-1-y*2)*(DWORD)d.lPitch; memset(line,0,row);
  for(x=0;x<ow;x++) {DWORD p=bpp==16?((WORD*)src)[x*2]:((DWORD*)src)[x*2],r,g,b;
   if(bpp==16) {r=(p>>11)<<3; g=((p>>5)&63)<<2; b=(p&31)<<3;} else {r=(p>>16)&255; g=(p>>8)&255; b=p&255;}
   line[x*3]=(BYTE)b; line[x*3+1]=(BYTE)g; line[x*3+2]=(BYTE)r;}
  fwrite(line,row,1,f);
 }
 free(line); fclose(f); IDirectDrawSurface_Unlock(s,NULL); return 1;
}
static void card(LPDIRECTDRAWSURFACE ps,DWORD w,DWORD h,DWORD b,int shot,const char *what) {
 DWORD c[8]; int k,m=40; char t[160]; int W=(int)w,H=(int)h;
 c[0]=rgb(b,255,255,255); c[1]=rgb(b,255,255,0); c[2]=rgb(b,0,255,255); c[3]=rgb(b,0,255,0); c[4]=rgb(b,255,0,255); c[5]=rgb(b,255,0,0); c[6]=rgb(b,0,0,255); c[7]=rgb(b,0,0,0);
 fill(ps,rgb(b,30,30,30),0,0,W,H);
 for(k=0;k<8;k++) fill(ps,c[k],W*k/8,H/3,W*(k+1)/8,H*2/3);
 fill(ps,c[0],W/2-1,0,W/2+1,H); fill(ps,c[0],0,H/2-1,W,H/2+1); /* cross */
 fill(ps,c[0],0,0,W,3); fill(ps,c[0],0,H-3,W,H); fill(ps,c[0],0,0,3,H); fill(ps,c[0],W-3,0,W,H); /* frame */
 fill(ps,rgb(b,255,0,0),3,3,3+m,3+m); fill(ps,rgb(b,0,255,0),W-3-m,3,W-3,3+m); fill(ps,rgb(b,0,0,255),3,H-3-m,3+m,H-3); fill(ps,rgb(b,255,255,0),W-3-m,H-3-m,W-3,H-3);
 label(ps,"TL",8,8,RGB(255,255,255),RGB(255,0,0)); label(ps,"TR",W-36,8,RGB(0,0,0),RGB(0,255,0)); label(ps,"BL",8,H-30,RGB(255,255,255),RGB(0,0,255)); label(ps,"BR",W-36,H-30,RGB(0,0,0),RGB(255,255,0));
 sprintf(t," SHOT %02d   %lux%lux%lu   %s ",shot,w,h,b,what); label(ps,t,60,H/6,RGB(255,255,255),RGB(0,0,0));
 label(ps," All four corner markers and the full white frame must be visible ",60,H/6+24,RGB(255,255,255),RGB(0,0,0));
}
/* returns 1 GOOD, 2 BAD, 3 SKIP, 4 QUIT, 0 no answer. v1.6: only fresh input counts. Everything that arrives in the first 1.5 s is thrown away, the polled
 * keys and the mouse button must first be seen released, and only a new press (an edge) answers. The log names the source of the answer. */
static const char *asrc="";
static int ask(RECT *bt) {
 DWORD t0=GetTickCount(),end=t0+180000UL; int v=0,i; POINT pt;
 int pg,pb,ps,pe,pl; /* previous state of G, B, S, Esc and the left button */
 wait_ms(1500); lastkey=0; clicked=0;
 pg=(GetAsyncKeyState('G')&0x8000)!=0; pb=(GetAsyncKeyState('B')&0x8000)!=0; ps=(GetAsyncKeyState('S')&0x8000)!=0; pe=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0; pl=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
 while(GetTickCount()<end&&!v) {
  int g,b,s,e,l; pump();
  if(lastkey=='g'||lastkey=='G') {v=1; asrc="key message";} else if(lastkey=='b'||lastkey=='B') {v=2; asrc="key message";} else if(lastkey=='s'||lastkey=='S') {v=3; asrc="key message";} else if(lastkey==27) {v=4; asrc="key message";}
  else if(clicked) {clicked=0; pt.x=clickpt.x; pt.y=clickpt.y; for(i=0;i<4;i++) if(PtInRect(&bt[i],pt)) {v=i+1; asrc="click message";}}
  if(!v) {
   g=(GetAsyncKeyState('G')&0x8000)!=0; b=(GetAsyncKeyState('B')&0x8000)!=0; s=(GetAsyncKeyState('S')&0x8000)!=0; e=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0; l=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
   if(g&&!pg) {v=1; asrc="polled key";} else if(b&&!pb) {v=2; asrc="polled key";} else if(s&&!ps) {v=3; asrc="polled key";} else if(e&&!pe) {v=4; asrc="polled key";}
   else if(l&&!pl&&GetCursorPos(&pt)) {for(i=0;i<4;i++) if(PtInRect(&bt[i],pt)) {v=i+1; asrc="polled mouse";}}
   pg=g; pb=b; ps=s; pe=e; pl=l;
  }
  Sleep(30);
 }
 logl("  answered after %lu ms by: %s\n",GetTickCount()-t0,v?asrc:"(nobody)");
 return v;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show_) {
 WNDCLASS wc; HWND win; HMODULE mod; typedef HRESULT (WINAPI *CREATE)(GUID*,LPDIRECTDRAW*,IUnknown*); CREATE create;
 static const DWORD W[6]={640,800,1024,640,800,1024},H[6]={480,600,768,480,600,768},B[6]={16,16,16,32,32,32};
 int mi,ab,shot=0,pass=0,fail=0,from=1,quit=0,opt_restore=1,opt_fresh=0; static char res[24][120]; DWORD hold=6000;
 (void)prev;(void)show_; if(cmd&&strstr(cmd,"from=")) from=atoi(strstr(cmd,"from=")+5); if(from<1) from=1; if(cmd&&strstr(cmd,"norestore")) opt_restore=0; if(cmd&&strstr(cmd,"fresh")) opt_fresh=1;
 SetUnhandledExceptionFilter(crashfilter); lf=fopen("VX2CONF.TXT","w"); logl("VX2CONF v1.8 (driver v10.63 expected)\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) logl("cannot open the driver device\n");
 logl("options: from=%d restore=%d fresh=%d\n",from,opt_restore,opt_fresh);
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName="VX2CONF"; RegisterClass(&wc);
 win=CreateWindowEx(WS_EX_TOPMOST,wc.lpszClassName,"VX2CONF",WS_POPUP|WS_VISIBLE,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,hi,NULL);
 SetCursor(LoadCursor(NULL,IDC_ARROW)); {MSG m0; PostMessage(win,WM_NULL,0,0); GetMessage(&m0,NULL,0,0); DispatchMessage(&m0);} /* ends the startup busy cursor */
 mod=LoadLibraryA("DDRAW.DLL"); create=mod?(CREATE)GetProcAddress(mod,"DirectDrawCreate"):NULL;
 if(!win||!create||FAILED(create(NULL,&dd,NULL))||FAILED(IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN))) {MessageBox(NULL,"DirectDraw not available","VX2CONF",MB_OK); return 1;}
 for(mi=0;mi<6&&!quit;mi++) for(ab=0;ab<2&&!quit;ab++) {
  DWORD w=W[mi],h=H[mi],b=B[mi]; LPDIRECTDRAWSURFACE ps=NULL; DDSURFACEDESC d; snapv s1,s2,s3; char what[80],bmp[40]; int good=1,uv=0; RECT bt[4];
  shot++; if(shot<from) continue;
  sprintf(what,ab?"flip chain at the same mode first":"flip chain at 640x480x16 first");
  logl("\nSHOT %02d  %lux%lux%lu  %s\n",shot,w,h,b,what);
  memlog("card start"); step("prepare SetDisplayMode"); modeops++; if(FAILED(IDirectDraw_SetDisplayMode(dd,ab?w:640,ab?h:480,ab?b:16))) logl("  prepare SetDisplayMode failed\n");
  step("flip chain"); flipcycle(); logl("  flip chain done\n");
  modeops++; step("SetDisplayMode to the test mode"); if(FAILED(IDirectDraw_SetDisplayMode(dd,w,h,b))) {logl("  SetDisplayMode failed\n"); sprintf(res[shot-1],"SHOT %02d %lux%lux%lu (%s): SetDisplayMode FAILED",shot,w,h,b,what); fail++; continue;}
  SetCursor(LoadCursor(NULL,IDC_ARROW)); logl("  SetDisplayMode returned\n"); step("register snapshot after mode set"); snapshot(&s1,"after mode set");
  memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
  step("CreateSurface"); if(FAILED(IDirectDraw_CreateSurface(dd,&d,&ps,NULL))) {logl("  CreateSurface failed\n"); sprintf(res[shot-1],"SHOT %02d %lux%lux%lu (%s): CreateSurface FAILED",shot,w,h,b,what); fail++; continue;}
  step("draw card"); card(ps,w,h,b,shot,what);
  {int k,bw=((int)w-160)/4,y0=(int)h*2/3+20,y1=(int)h-70; static const char *nm[4]={" GOOD "," BAD "," SKIP "," QUIT "}; DWORD col[4]; col[0]=rgb(b,0,140,0); col[1]=rgb(b,170,0,0); col[2]=rgb(b,90,90,90); col[3]=rgb(b,40,40,120);
   for(k=0;k<4;k++) {SetRect(&bt[k],80+k*bw+6,y0,80+(k+1)*bw-6,y1); fill(ps,col[k],bt[k].left,bt[k].top,bt[k].right,bt[k].bottom); label(ps,nm[k],bt[k].left+10,bt[k].top+10,RGB(255,255,255),RGB(0,0,0));}
   label(ps," Take your screenshot, then click GOOD / BAD / SKIP / QUIT (or press G B S Esc) ",60,y0-26,RGB(255,255,255),RGB(0,0,0));} snapshot(&s2,"card drawn");
  sprintf(bmp,"VX2CONF_%02d.BMP",shot); logl("  frame buffer saved: %s (%s)\n",bmp,savebmp(ps,w,h,b,bmp)?"ok":"FAILED");
  step("waiting for your answer"); uv=ask(bt); logl("  your answer: %s\n",uv==1?"GOOD":uv==2?"BAD":uv==3?"skipped":uv==4?"quit":"no answer (3 min)"); snapshot(&s3,"after answer");
  IDirectDrawSurface_Release(ps);
  if(!s1.ok||!s2.ok||!s3.ok) good=0;
  if(s1.start||s2.start||s3.start) good=0;
  if(s1.info_display||s2.info_display||s3.info_display) good=0;
  logl("  software verdict: %s\n",good?"PASS (start 0 everywhere)":"FAIL");
  sprintf(res[shot-1],"SHOT %02d %4lux%-4lux%-2lu %-34s software %s, you: %s",shot,w,h,b,what,good?"PASS":"FAIL",uv==1?"GOOD":uv==2?"BAD":uv==3?"skipped":uv==4?"quit":"no answer"); if(good&&uv!=2) pass++; else fail++;
  if(uv==4) quit=1;
  if(opt_restore) {modeops++; step("RestoreDisplayMode"); IDirectDraw_RestoreDisplayMode(dd); wait_ms(500);}
  if(opt_fresh) {step("re-create DirectDraw"); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); IDirectDraw_Release(dd); dd=NULL; wait_ms(500);
   if(FAILED(create(NULL,&dd,NULL))||FAILED(IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN))) {logl("  re-create failed\n"); break;}}
 }
 IDirectDraw_RestoreDisplayMode(dd); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); IDirectDraw_Release(dd); DestroyWindow(win);
 {int i; logl("\nSUMMARY (software side): %d pass, %d fail\n",pass,fail); for(i=0;i<shot;i++) logl("%s\n",res[i]);
  logl("Scan-out position: compare each KVM/camera screenshot with its SHOT number; all four corner markers and the frame must be visible.\n");}
 if(lf) fclose(lf);
 {char msg[200]; sprintf(msg,"Finished: %d pass, %d fail. Send VX2CONF.TXT, the VX2CONF_nn.BMP files and your screenshots.",pass,fail); MessageBox(NULL,msg,"VX2CONF",MB_OK);}
 return 0;
}
