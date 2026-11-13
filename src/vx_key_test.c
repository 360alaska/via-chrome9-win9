/* VXKEY v1.0 (driver v10.276): text surfaces and colour-keyed blits as the DirectX sample framework draws its statistics (Tunnel's frame
 * rate and info lines): an offscreen surface in the desktop's format, filled with the key colour (Blt COLORFILL), text written into it
 * with GDI (GetDC, ExtTextOut opaque on black and transparent), then copied onto another surface with BltFast / Blt and the source
 * colour key. Every step is done twice, on video-memory surfaces (the driver: DIBENG through the surface's DC, the HAL's fills and keyed
 * copies on the engine) and on system-memory surfaces (DirectDraw's own software), and the two compared pixel for pixel, for many
 * widths (odd and even, below and above the HAL's size thresholds). The surfaces are filled with a pattern through Lock first, so a
 * step that leaves pixels unwritten shows. Log VXKEY.TXT; the first failing case of each step writes VXKEY_<step>_HW.BMP / _SW.BMP. */
#define COBJMACROS
#define INITGUID
#define DIRECTDRAW_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <string.h>
/* Open Watcom declares IID_IDirectDraw7 but its dxguid.lib omits it (as VXDX7) */
static const GUID iid_dd7={0x15E65EC0UL,0x3B9C,0x11D2,{0xB9,0x2F,0,0x60,0x97,0x97,0xEA,0x5B}};

static FILE *logf;static LPDIRECTDRAW7 dd;static int bpp,failures,dumped[8];
static const char *step_name[4]={"fill","text","BltFast key","Blt key"};
static LPDIRECTDRAWSURFACE7 make(DWORD w,DWORD h,DWORD mem) {
 DDSURFACEDESC2 sd;LPDIRECTDRAWSURFACE7 s=0;HRESULT r;
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;sd.dwWidth=w;sd.dwHeight=h;
 sd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|mem;
 r=IDirectDraw7_CreateSurface(dd,&sd,&s,0);
 if(r!=DD_OK){fprintf(logf,"   CreateSurface %lux%lu %s: %08lX\n",w,h,mem==DDSCAPS_VIDEOMEMORY?"video":"system",(unsigned long)r);return 0;}
 return s;
}
/* the surface's pixels (w x h, desktop format) into out (one DWORD a pixel); its pitch into *pitch; 1 when locked */
static int read_px(LPDIRECTDRAWSURFACE7 s,DWORD w,DWORD h,DWORD *out,LONG *pitch) {
 DDSURFACEDESC2 sd;DWORD x,y;
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
 if(IDirectDrawSurface7_Lock(s,0,&sd,DDLOCK_WAIT|DDLOCK_READONLY,0)!=DD_OK)return 0;
 for(y=0;y<h;y++)for(x=0;x<w;x++){const BYTE *p=(const BYTE*)sd.lpSurface+y*sd.lPitch;out[y*w+x]=bpp==16?((const WORD*)p)[x]:((const DWORD*)p)[x]&0xFFFFFFUL;}
 *pitch=sd.lPitch;IDirectDrawSurface7_Unlock(s,0);return 1;
}
/* every byte of every row (the whole pitch) set to a pattern, so pixels a step does not write keep it */
static void garbage(LPDIRECTDRAWSURFACE7 s,DWORD h,BYTE v) {
 DDSURFACEDESC2 sd;DWORD y;
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
 if(IDirectDrawSurface7_Lock(s,0,&sd,DDLOCK_WAIT|DDLOCK_WRITEONLY,0)!=DD_OK)return;
 for(y=0;y<h;y++)memset((BYTE*)sd.lpSurface+y*sd.lPitch,v,(size_t)sd.lPitch);
 IDirectDrawSurface7_Unlock(s,0);
}
static void fill(LPDIRECTDRAWSURFACE7 s,DWORD c) {
 DDBLTFX fx;memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dwFillColor=c;
 IDirectDrawSurface7_Blt(s,0,0,0,DDBLT_COLORFILL|DDBLT_WAIT,&fx);
}
static HFONT font;
static void text(LPDIRECTDRAWSURFACE7 s,DWORD w,DWORD h,const char *t,int opaque) {
 HDC dc;RECT r;
 if(IDirectDrawSurface7_GetDC(s,&dc)!=DD_OK){fprintf(logf,"   GetDC failed\n");return;}
 SelectObject(dc,font);SetTextColor(dc,RGB(255,255,0));SetBkColor(dc,RGB(0,0,0));SetBkMode(dc,opaque?OPAQUE:TRANSPARENT);
 SetRect(&r,0,0,(int)w,(int)h);ExtTextOutA(dc,0,0,opaque?ETO_OPAQUE:0,&r,t,(UINT)strlen(t),0);
 IDirectDrawSurface7_ReleaseDC(s,dc);
}
static void bmp(const char *name,const DWORD *px,DWORD w,DWORD h) {
 FILE *f=fopen(name,"wb");BYTE hd[54];DWORD x,y,pad=(4-(w*3)%4)%4;
 if(!f)return;memset(hd,0,54);hd[0]='B';hd[1]='M';*(DWORD*)(hd+2)=54+(w*3+pad)*h;*(DWORD*)(hd+10)=54;*(DWORD*)(hd+14)=40;*(LONG*)(hd+18)=(LONG)w;*(LONG*)(hd+22)=-(LONG)h;*(WORD*)(hd+26)=1;*(WORD*)(hd+28)=24;
 fwrite(hd,1,54,f);
 for(y=0;y<h;y++){for(x=0;x<w;x++){DWORD c=px[y*w+x];BYTE b,g,r;
   if(bpp==16){r=(BYTE)(((c>>11)&31)<<3);g=(BYTE)(((c>>5)&63)<<2);b=(BYTE)((c&31)<<3);}else{r=(BYTE)(c>>16);g=(BYTE)(c>>8);b=(BYTE)c;}
   fputc(b,f);fputc(g,f);fputc(r,f);}
  for(x=0;x<pad;x++)fputc(0,f);}
 fclose(f);
}
static DWORD hwbuf[512*64],swbuf[512*64];
/* the video and system copies of a w x h surface compared; 1 when they are equal */
static int compare(int step,const char *what,LPDIRECTDRAWSURFACE7 hw,LPDIRECTDRAWSURFACE7 sw,DWORD w,DWORD h) {
 LONG hp=0,sp=0;DWORD k,n=0,first=0;
 if(!read_px(hw,w,h,hwbuf,&hp)||!read_px(sw,w,h,swbuf,&sp)){fprintf(logf,"   %s %-11s lock failed\n",what,step_name[step]);failures++;return 0;}
 for(k=0;k<w*h;k++)if(hwbuf[k]!=swbuf[k]){if(!n)first=k;n++;}
 if(!n)return 1;
 fprintf(logf,"   %s %-11s %lu of %lu pixels differ (pitch %ld video, %ld system), first at %lu,%lu: %06lX against %06lX  FAIL\n",what,step_name[step],n,w*h,(long)hp,(long)sp,first%w,first/w,hwbuf[first],swbuf[first]);
 failures++;
 if(!dumped[step]){char nm[40];dumped[step]=1;sprintf(nm,"VXKEY_%d_HW.BMP",step);bmp(nm,hwbuf,w,h);sprintf(nm,"VXKEY_%d_SW.BMP",step);bmp(nm,swbuf,w,h);}
 return 0;
}
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcA(h,m,w,l);}
int main(void) {
 static const DWORD ws[]={31,32,33,63,64,65,75,99,100,101,127,128,129,147,150,199,200,201,255,256,257,301,333};
 static const DWORD hs[]={13,16,20};
 static const char *texts[2]={"625.00 fps  200000 tps","256x256x32 VX855 HAL"};
 HMODULE lib;HRESULT (WINAPI *create)(GUID*,LPVOID*,REFIID,IUnknown*);WNDCLASSA wc;HWND win;HDC d;DWORD i,j,o,cases=0;
 logf=fopen("VXKEY.TXT","w");if(!logf)return 1;setvbuf(logf,0,_IONBF,0);
 fprintf(logf,"VXKEY v1.0 (driver v10.276): GDI text surfaces and colour-keyed blits, video memory against system memory\n");
 d=GetDC(0);bpp=GetDeviceCaps(d,BITSPIXEL);fprintf(logf,"desktop %dx%dx%d\n",GetDeviceCaps(d,HORZRES),GetDeviceCaps(d,VERTRES),bpp);ReleaseDC(0,d);
 if(bpp!=16&&bpp!=32){fprintf(logf,"needs a 16 or 32 bpp desktop\n");fclose(logf);return 1;}
 lib=LoadLibraryA("ddraw.dll");create=lib?(HRESULT (WINAPI *)(GUID*,LPVOID*,REFIID,IUnknown*))GetProcAddress(lib,"DirectDrawCreateEx"):0;
 if(!create||create(0,(LPVOID*)&dd,&iid_dd7,0)!=DD_OK){fprintf(logf,"no DirectDraw 7\n");fclose(logf);return 1;}
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="VXKEY";RegisterClassA(&wc);
 win=CreateWindowA("VXKEY","VXKEY",WS_OVERLAPPEDWINDOW,0,0,64,64,0,0,wc.hInstance,0);
 IDirectDraw7_SetCooperativeLevel(dd,win,DDSCL_NORMAL);
 font=CreateFontA(12,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,VARIABLE_PITCH,"Arial");
 for(i=0;i<sizeof(ws)/sizeof(ws[0]);i++)for(j=0;j<sizeof(hs)/sizeof(hs[0]);j++)for(o=0;o<2;o++) {
  DWORD w=ws[i],h=hs[j];LPDIRECTDRAWSURFACE7 hs_=make(w,h,DDSCAPS_VIDEOMEMORY),ss=make(w,h,DDSCAPS_SYSTEMMEMORY),hd=make(400,40,DDSCAPS_VIDEOMEMORY),sd=make(400,40,DDSCAPS_SYSTEMMEMORY);
  char what[48];DDCOLORKEY key;DDSURFACEDESC2 q;RECT r;int ok=1;
  sprintf(what,"%3lux%-2lu %s",w,h,o?"opaque     ":"transparent");cases++;
  if(!hs_||!ss||!hd||!sd){failures++;goto next;}
  garbage(hs_,h,0xA5);garbage(ss,h,0xA5);garbage(hd,40,0x5A);garbage(sd,40,0x5A);
  fill(hs_,0);fill(ss,0);ok&=compare(0,what,hs_,ss,w,h);
  text(hs_,w,h,texts[(i+j)&1],o);text(ss,w,h,texts[(i+j)&1],o);ok&=compare(1,what,hs_,ss,w,h);
  key.dwColorSpaceLowValue=key.dwColorSpaceHighValue=0;IDirectDrawSurface7_SetColorKey(hs_,DDCKEY_SRCBLT,&key);IDirectDrawSurface7_SetColorKey(ss,DDCKEY_SRCBLT,&key);
  fill(hd,bpp==16?0x001F:0x0000FF);fill(sd,bpp==16?0x001F:0x0000FF);SetRect(&r,0,0,(int)w,(int)h);
  IDirectDrawSurface7_BltFast(hd,7,5,hs_,&r,DDBLTFAST_SRCCOLORKEY|DDBLTFAST_WAIT);IDirectDrawSurface7_BltFast(sd,7,5,ss,&r,DDBLTFAST_SRCCOLORKEY|DDBLTFAST_WAIT);
  ok&=compare(2,what,hd,sd,400,40);
  {RECT dr;SetRect(&dr,20,10,20+(int)w,10+(int)h);fill(hd,bpp==16?0x07E0:0x00FF00);fill(sd,bpp==16?0x07E0:0x00FF00);
   IDirectDrawSurface7_Blt(hd,&dr,hs_,&r,DDBLT_KEYSRC|DDBLT_WAIT,0);IDirectDrawSurface7_Blt(sd,&dr,ss,&r,DDBLT_KEYSRC|DDBLT_WAIT,0);
   ok&=compare(3,what,hd,sd,400,40);}
  if(ok){memset(&q,0,sizeof(q));q.dwSize=sizeof(q);IDirectDrawSurface7_GetSurfaceDesc(hs_,&q);fprintf(logf,"   %s pass (pitch %ld)\n",what,(long)q.lPitch);}
 next:
  if(hs_)IDirectDrawSurface7_Release(hs_);if(ss)IDirectDrawSurface7_Release(ss);if(hd)IDirectDrawSurface7_Release(hd);if(sd)IDirectDrawSurface7_Release(sd);
 }
 fprintf(logf,"%lu cases, failures=%d\n",cases,failures);
 IDirectDraw7_Release(dd);DestroyWindow(win);fclose(logf);
 return failures;
}
