/* VX3SUITE: one-button automated function test for the VX855 Win98 driver.
 * Runs every proven 2D and 3D function through the VX85598 VxD, scores each
 * one pass/fail, shows tiles and pictures, and writes VX3SUITE.TXT and BMPs.
 * Copyright (c) 2026, MIT. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vx85598.h"
#include "vx3suite_core.inc"

#define SUITE_VERSION "v10.11"
#define ID_RUN 101
#define ID_CLOSE 102
#define TILE 11
#define PITCH 14
#define GX0 128
#define GY0 36
#define DY0 276

static HANDLE device=INVALID_HANDLE_VALUE;
static vx_status_t st;
static unsigned long recover_flag;
static HWND hwnd,hrun;
static HFONT font;
static int sel=-1,running,done;
static char status_line[300]="Press RUN ALL TESTS (about a minute). Restart Windows first if an earlier test hung.";
static char dirpath[MAX_PATH];
static char *repbuf; static size_t replen,repcap;

static void write_report(void);
static int dev_call(uint32_t select) {
 DWORD got=0; int okc;
 RQ.cb=sizeof(RQ); RQ.version=VX3D_VERSION; RQ.arm=VX3D_ARM;
 RQ.reserved=select|(recover_flag?0x100UL:0UL); RQ.probe=VX3D_SF_PROBE;
 memset(&RP,0,sizeof(RP));
 sprintf(g_inflight,"driver call select %lu (flags %08lX) was started and had not returned",(unsigned long)select,(unsigned long)RQ.lab_flags);
 write_report();
 okc=DeviceIoControl(device,VX_IOCTL_3D_TEST,&RQ,sizeof(RQ),&RP,sizeof(RP),&got,NULL);
 g_inflight[0]=0;
 if(!okc) return 0;
 if(got!=sizeof(RP)||RP.magic!=VX3D_MAGIC||RP.cb!=sizeof(RP)||RP.version!=VX3D_VERSION||RP.build!=VX3D_BUILD||RP.producer!=VX3D_PRODUCER) return 0;
 recover_flag=(RP.fault_latched||RP.result==VX3D_DISABLED)?1:0;
 return 1;
}
static void pump(void) {
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
}
static COLORREF st_color(int s) {
 return s==ST_PASS?RGB(32,176,64):s==ST_FAIL?RGB(208,40,40):s==ST_HANG?RGB(240,150,20):s==ST_SKIP?RGB(110,110,110):RGB(48,48,56);
}
static int tile_rect(int i,RECT *r) {
 int g=T[i].group,k=0,j;
 for(j=0;j<i;j++) if(T[j].group==g) k++;
 r->left=GX0+k*(TILE+4); r->top=GY0+g*PITCH; r->right=r->left+TILE; r->bottom=r->top+TILE;
 return 1;
}
static uint32_t diff_pixel(int img,int idx) {
 uint32_t a=IMG_A[img][idx],e=IMG_E[img][idx],b=IMG_B[img][idx];
 if(a!=e) return 0xFFFF0000UL;
 if(e!=b) return 0xFF00C800UL;
 return 0xFF000000UL|(((a>>2)&0x3F3F3FUL));
}
static void blit(HDC dc,const uint32_t *px,int w,int h,int x0,int y0,int scale) {
 BITMAPINFO bi; memset(&bi,0,sizeof(bi));
 bi.bmiHeader.biSize=sizeof(bi.bmiHeader); bi.bmiHeader.biWidth=w; bi.bmiHeader.biHeight=-h;
 bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32; bi.bmiHeader.biCompression=BI_RGB;
 StretchDIBits(dc,x0,y0,w*scale,h*scale,0,0,w,h,px,&bi,DIB_RGB_COLORS,SRCCOPY);
}
static void paint(HDC dc) {
 int g,i,tot,pass,fail,hang,skip; RECT r; char b[300]; HBRUSH br; uint32_t tmp[4096];
 SetBkMode(dc,TRANSPARENT);
 if(font) SelectObject(dc,font);
 r.left=8; r.top=6; r.right=400; r.bottom=40;
 DrawText(dc,status_line,-1,&r,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
 for(g=0;g<NGROUPS;g++) {
  summary_counts(g,&tot,&pass,&fail,&hang,&skip);
  sprintf(b,"%s %d/%d",GNAME[g],pass,tot);
  r.left=4; r.right=GX0-4; r.top=GY0+g*PITCH-1; r.bottom=r.top+PITCH;
  SetTextColor(dc,(tot && pass==tot)?RGB(0,110,30):(fail||hang)?RGB(170,0,0):RGB(0,0,0));
  DrawText(dc,b,-1,&r,DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
 }
 SetTextColor(dc,RGB(0,0,0));
 for(i=0;i<NT;i++) {
  tile_rect(i,&r);
  br=CreateSolidBrush(st_color(T[i].status)); FillRect(dc,&r,br); DeleteObject(br);
  if(i==sel) {
   RECT o; o.left=r.left-2; o.top=r.top-2; o.right=r.right+2; o.bottom=r.bottom+2;
   br=CreateSolidBrush(RGB(255,255,255)); FrameRect(dc,&o,br); DeleteObject(br);
   br=CreateSolidBrush(RGB(0,0,0)); o.left--; o.top--; o.right++; o.bottom++; FrameRect(dc,&o,br); DeleteObject(br);
  }
 }
 if(sel>=0 && sel<NT) {
  int x=4;
  r.left=4; r.top=DY0-2; r.right=620; r.bottom=DY0+14;
  sprintf(b,"%s: %s",st_name(T[sel].status),T[sel].name);
  DrawText(dc,b,-1,&r,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
  if(T[sel].img>=0) {
   int im=T[sel].img,k;
   blit(dc,IMG_B[im],64,64,x,DY0+16,2);
   blit(dc,IMG_A[im],64,64,x+132,DY0+16,2);
   for(k=0;k<4096;k++) tmp[k]=diff_pixel(im,k);
   blit(dc,tmp,64,64,x+264,DY0+16,2);
   r.left=x; r.right=x+128; r.top=DY0+146; r.bottom=r.top+14; DrawText(dc,"before",-1,&r,DT_LEFT|DT_SINGLELINE);
   r.left=x+132; r.right=x+260; DrawText(dc,"after (hardware)",-1,&r,DT_LEFT|DT_SINGLELINE);
   r.left=x+264; r.right=x+392; DrawText(dc,"red=wrong green=right",-1,&r,DT_LEFT|DT_SINGLELINE);
   r.left=x+400; r.right=624; r.top=DY0+16; r.bottom=DY0+160;
  } else if(T[sel].xm) {
   uint32_t m[512]; int px,py;
   for(py=0;py<16;py++) for(px=0;px<32;px++) {
    uint32_t idx=(uint32_t)(py*32+px),cl=(XM[sel][idx/16]>>((idx%16)*2))&3UL;
    m[py*32+px]=cl==0?0xFF202020UL:cl==1?0xFFFF8000UL:cl==2?0xFF0060C0UL:0xFFC0C0C0UL;
   }
   blit(dc,m,32,16,x,DY0+16,4);
   r.left=x; r.right=x+128; r.top=DY0+82; r.bottom=r.top+30; DrawText(dc,"result map (x4..35, y4..19): grey=unchanged, orange=fg, blue=bg, white=other",-1,&r,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
   r.left=x+140; r.right=624; r.top=DY0+16; r.bottom=DY0+160;
  } else if(T[sel].has_sw) {
   RECT sw; sw.left=x; sw.top=DY0+16; sw.right=x+64; sw.bottom=DY0+80;
   if((T[sel].sw>>24)==0xFF) br=CreateSolidBrush(RGB((T[sel].sw>>16)&0xFF,(T[sel].sw>>8)&0xFF,T[sel].sw&0xFF));
   else br=CreateHatchBrush(HS_DIAGCROSS,RGB(120,120,120));
   FillRect(dc,&sw,br); DeleteObject(br);
   r.left=x; r.right=x+80; r.top=DY0+84; r.bottom=r.top+28; DrawText(dc,"sampled pixel",-1,&r,DT_LEFT|DT_WORDBREAK);
   r.left=x+92; r.right=624; r.top=DY0+16; r.bottom=DY0+160;
  } else {
   r.left=x; r.right=624; r.top=DY0+16; r.bottom=DY0+160;
  }
  DrawText(dc,T[sel].detail,-1,&r,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
 }
}
static void hook(int i) {
 int tot,pass,fail,hang,skip;
 summary_counts(-1,&tot,&pass,&fail,&hang,&skip);
 sel=i;
 sprintf(status_line,"Running... %d tests done: %d pass, %d fail, %d hang. Last: %s",pass+fail+hang+skip,pass,fail,hang,T[i].name);
 InvalidateRect(hwnd,NULL,FALSE); UpdateWindow(hwnd); pump();
}
static void rep_out(const char *s) {
 size_t n=strlen(s);
 if(replen+n+1>repcap) {repcap=(replen+n+1)*2+4096; repbuf=(char*)realloc(repbuf,repcap);}
 if(!repbuf) return;
 memcpy(repbuf+replen,s,n+1); replen+=n;
}
static void make_path(const char *name,char *out) {
 sprintf(out,"%s%s",dirpath,name);
}
static void save_bmp(const char *name,const uint32_t *px,int w,int h) {
 BITMAPFILEHEADER bf; BITMAPINFOHEADER bi; HANDLE f; DWORD n; int y; char p[MAX_PATH+16];
 memset(&bf,0,sizeof(bf)); memset(&bi,0,sizeof(bi));
 bf.bfType=0x4D42; bf.bfOffBits=sizeof(bf)+sizeof(bi); bf.bfSize=bf.bfOffBits+(DWORD)(w*h*4);
 bi.biSize=sizeof(bi); bi.biWidth=w; bi.biHeight=h; bi.biPlanes=1; bi.biBitCount=32; bi.biCompression=BI_RGB;
 make_path(name,p);
 f=CreateFile(p,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(f==INVALID_HANDLE_VALUE) return;
 WriteFile(f,&bf,sizeof(bf),&n,NULL); WriteFile(f,&bi,sizeof(bi),&n,NULL);
 for(y=h-1;y>=0;y--) WriteFile(f,px+y*w,(DWORD)(w*4),&n,NULL);
 CloseHandle(f);
}
static void save_sheet2d(void) {
 int W=4*(3*66+10),H,i,cols=4,rows=(NIMG+cols-1)/cols,k,x,y; uint32_t *px; int n=0;
 if(NIMG<=0) return;
 H=rows*70+4;
 px=(uint32_t*)calloc((size_t)(W*H),4);
 if(!px) return;
 for(i=0;i<NT;i++) if(T[i].img>=0) {
  int im=T[i].img,cx=(n%cols)*(3*66+10)+4,cy=(n/cols)*70+4; n++;
  for(y=0;y<64;y++) for(x=0;x<64;x++) {
   px[(cy+y)*W+cx+x]=IMG_B[im][y*64+x];
   px[(cy+y)*W+cx+66+x]=IMG_A[im][y*64+x];
   px[(cy+y)*W+cx+132+x]=diff_pixel(im,y*64+x);
  }
  for(k=0;k<3*66;k++) px[(cy+66)*W+cx+k]=(T[i].status==ST_PASS)?0xFF20B040UL:0xFFD02828UL;
 }
 save_bmp("VX3SUITE_2D.BMP",px,W,H);
 free(px);
}
static void save_window_bmp(void) {
 RECT r; HDC wdc,mdc; HBITMAP bm,old; BITMAPINFO bi; uint32_t *px; int w,h;
 GetClientRect(hwnd,&r); w=r.right; h=r.bottom;
 if(w<=0||h<=0) return;
 wdc=GetDC(hwnd); mdc=CreateCompatibleDC(wdc); bm=CreateCompatibleBitmap(wdc,w,h); old=(HBITMAP)SelectObject(mdc,bm);
 BitBlt(mdc,0,0,w,h,wdc,0,0,SRCCOPY);
 px=(uint32_t*)calloc((size_t)(w*h),4);
 if(px) {
  memset(&bi,0,sizeof(bi)); bi.bmiHeader.biSize=sizeof(bi.bmiHeader); bi.bmiHeader.biWidth=w; bi.bmiHeader.biHeight=-h;
  bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32; bi.bmiHeader.biCompression=BI_RGB;
  GetDIBits(mdc,bm,0,(UINT)h,px,&bi,DIB_RGB_COLORS);
  save_bmp("VX3SUITE.BMP",px,w,h);
  free(px);
 }
 SelectObject(mdc,old); DeleteObject(bm); DeleteDC(mdc); ReleaseDC(hwnd,wdc);
}
static void write_report(void) {
 HANDLE f; DWORD n; char p[MAX_PATH+16];
 replen=0; if(repbuf) repbuf[0]=0;
 report_emit(rep_out);
 make_path("VX3SUITE.TXT",p);
 f=CreateFile(p,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(f==INVALID_HANDLE_VALUE) return;
 if(repbuf) WriteFile(f,repbuf,(DWORD)replen,&n,NULL);
 CloseHandle(f);
}
static void sys_tests(void) {
 DWORD got=0; int ti,ok; char d[240];
 memset(&st,0,sizeof(st));
 device=CreateFile("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 ti=t_add(G_SYS,"VX85598 driver found, no initialisation failure");
 if(device==INVALID_HANDLE_VALUE) {
  sprintf(d,"cannot open the VX85598 device: Windows error %lu",(unsigned long)GetLastError());
  t_set(ti,ST_FAIL,d); abort_all("the VX85598 driver is not loaded"); return;
 }
 ok=DeviceIoControl(device,VX_IOCTL_STATUS,NULL,0,&st,sizeof(st),&got,NULL) && got==sizeof(st) && st.magic==VX_MAGIC && st.cb==sizeof(st) && st.version==VX_VERSION;
 if(!ok) {t_set(ti,ST_FAIL,"driver status interface is missing or incompatible"); abort_all("incompatible driver"); return;}
 sprintf(d,"stage %lu, failure %lu, video memory window %lu MiB at %08lX",(unsigned long)st.stage,(unsigned long)st.failure,(unsigned long)(st.fb_bytes>>20),(unsigned long)st.fb_phys);
 t_set(ti,st.failure==0?ST_PASS:ST_FAIL,d);
 sprintf(g_sysinfo,"Suite %s. Desktop %lux%lux%lu; video memory window %lu MiB at %08lX (BAR0 %08lX, BAR1 %08lX); driver stage %lu failure %lu.\r\n",
  SUITE_VERSION,(unsigned long)st.width,(unsigned long)st.height,(unsigned long)st.bpp,(unsigned long)(st.fb_bytes>>20),(unsigned long)st.fb_phys,(unsigned long)st.bar0,(unsigned long)st.bar1,(unsigned long)st.stage,(unsigned long)st.failure);
 ti=t_add(G_SYS,"Desktop is 640x480x32 (required by every 2D/3D test)");
 sprintf(d,"desktop is %lux%lux%lu",(unsigned long)st.width,(unsigned long)st.height,(unsigned long)st.bpp);
 if(st.width==640 && st.height==480 && st.bpp==32) t_set(ti,ST_PASS,d);
 else {t_set(ti,ST_FAIL,d); abort_all("set the display to 640x480 with 32-bit colour, then run again");}
 ti=t_add(G_SYS,"3D engine ready, no latched fault");
 memset(&RP,0,sizeof(RP)); got=0;
 if(DeviceIoControl(device,VX_IOCTL_3D_PROBE,NULL,0,&RP,sizeof(RP),&got,NULL) && got==sizeof(RP) && RP.magic==VX3D_MAGIC) {
  sprintf(d,"probe result %lu, engine status %08lX, fault latched %lu",(unsigned long)RP.result,(unsigned long)RP.status_before,(unsigned long)RP.fault_latched);
  if(RP.fault_latched) recover_flag=1;
  t_set(ti,(RP.result==VX3D_NOT_RUN && !RP.fault_latched)?ST_PASS:ST_FAIL,d);
  if(RP.fault_latched) abort_all("the 3D engine has a latched fault from an earlier run: restart Windows");
 } else {t_set(ti,ST_FAIL,"3D probe call failed"); abort_all("3D probe failed");}
}
static void do_run(void) {
 int tot,pass,fail,hang,skip;
 if(running) return;
 running=1; done=0; NT=0; NIMG=0; g_dead=0; g_dead_why[0]=0; sel=-1;
 memset(T,0,sizeof(T));
 EnableWindow(hrun,FALSE);
 g_hook=hook;
 sys_tests();
 run_all_tests(st.fb_bytes);
 g_hook=NULL;
 summary_counts(-1,&tot,&pass,&fail,&hang,&skip);
 sprintf(status_line,"DONE: %d tests, %d PASS, %d FAIL, %d HANG, %d SKIP.%s Click a tile for its picture and numbers. Files: VX3SUITE.TXT, VX3SUITE.BMP, VX3SUITE_2D.BMP.",
  tot,pass,fail,hang,skip,g_dead?" Run stopped (see TXT).":"");
 {int i; sel=-1; for(i=0;i<NT;i++) if(T[i].status==ST_FAIL||T[i].status==ST_HANG) {sel=i; break;}}
 InvalidateRect(hwnd,NULL,FALSE); UpdateWindow(hwnd); pump();
 write_report(); save_sheet2d(); save_window_bmp();
 running=0; done=1; EnableWindow(hrun,TRUE);
}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
 PAINTSTRUCT ps; HDC dc; int i; RECT r; POINT p;
 switch(msg) {
 case WM_CREATE:
  hwnd=w;
  hrun=CreateWindow("BUTTON","RUN ALL TESTS",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,410,6,120,28,w,(HMENU)ID_RUN,GetModuleHandle(NULL),NULL);
  {HWND c=CreateWindow("BUTTON","Close",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,540,6,70,28,w,(HMENU)ID_CLOSE,GetModuleHandle(NULL),NULL);
   if(font) {SendMessage(hrun,WM_SETFONT,(WPARAM)font,TRUE); SendMessage(c,WM_SETFONT,(WPARAM)font,TRUE);}}
  return 0;
 case WM_COMMAND:
  if(LOWORD(wp)==ID_RUN && HIWORD(wp)==BN_CLICKED) do_run();
  if(LOWORD(wp)==ID_CLOSE) DestroyWindow(w);
  return 0;
 case WM_LBUTTONDOWN:
  p.x=(short)LOWORD(lp); p.y=(short)HIWORD(lp);
  for(i=0;i<NT;i++) {
   tile_rect(i,&r); r.left-=2; r.top-=2; r.right+=2; r.bottom+=2;
   if(PtInRect(&r,p)) {sel=i; InvalidateRect(w,NULL,FALSE); break;}
  }
  return 0;
 case WM_ERASEBKGND:
  GetClientRect(w,&r); FillRect((HDC)wp,&r,(HBRUSH)(COLOR_BTNFACE+1)); return 1;
 case WM_PAINT:
  dc=BeginPaint(w,&ps);
  {HDC md; HBITMAP bm,ob; int cw,ch; GetClientRect(w,&r); cw=r.right; ch=r.bottom;
   md=CreateCompatibleDC(dc); bm=CreateCompatibleBitmap(dc,cw,ch); ob=(HBITMAP)SelectObject(md,bm);
   FillRect(md,&r,(HBRUSH)(COLOR_BTNFACE+1));
   paint(md);
   BitBlt(dc,0,0,cw,ch,md,0,0,SRCCOPY);
   SelectObject(md,ob); DeleteObject(bm); DeleteDC(md);}
  EndPaint(w,&ps); return 0;
 case WM_DESTROY: PostQuitMessage(0); return 0;
 }
 return DefWindowProc(w,msg,wp,lp);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show) {
 WNDCLASS cls; MSG msg; char *slash; int autorun=0;
 (void)previous;
 if(command && (strstr(command,"/auto")||strstr(command,"/AUTO")||strstr(command,"-auto"))) autorun=1;
 GetModuleFileName(NULL,dirpath,sizeof(dirpath));
 slash=strrchr(dirpath,'\\'); if(slash) slash[1]=0; else dirpath[0]=0;
 font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
 memset(&cls,0,sizeof(cls)); cls.lpfnWndProc=proc; cls.hInstance=instance;
 cls.hCursor=LoadCursor(NULL,IDC_ARROW); cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1); cls.lpszClassName="VX3SUITE";
 if(!RegisterClass(&cls)) return 1;
 hwnd=CreateWindow(cls.lpszClassName,"VX855 full function test " SUITE_VERSION,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,0,0,640,466,NULL,NULL,instance,NULL);
 if(!hwnd) return 1;
 ShowWindow(hwnd,show); UpdateWindow(hwnd);
 if(autorun) PostMessage(hwnd,WM_COMMAND,MAKEWPARAM(ID_RUN,BN_CLICKED),0);
 while(GetMessage(&msg,NULL,0,0)>0) {TranslateMessage(&msg); DispatchMessage(&msg);}
 if(device!=INVALID_HANDLE_VALUE) CloseHandle(device);
 return 0;
}
