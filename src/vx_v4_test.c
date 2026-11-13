/* VXV4 v1.0 (driver .532; .533): 16-bit BI_BITFIELDS bitmaps with BITMAPV4HEADER / BITMAPV5HEADER on the board (review of .530, B4).
 * The display driver read the three colour masks at info + biSize, which is right only for the 40-byte header; a V4 (108 bytes) or V5
 * (124) header holds them inside it at offset 40. .532 reads them there. Each header form draws the same 5-6-5 picture (64x48, a colour
 * gradient with a 1-pixel checker) with StretchDIBits SRCCOPY and, at 16 bpp, SRCINVERT over a known background, six times each; the bytes
 * after the V4/V5 header are 5-5-5 masks, so the old code took the wrong format (or skipped). Every pixel is compared with the 5-6-5
 * colour it must show (within 8 a channel). Results to the screen and VXV4.LOG beside the program. Run on the desktop at 16 or 32 bpp. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#define W 64
#define H 48
static FILE *logf;static int fails;
static void say(const char *f,...) {static char b[1024];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);fputs(b,stdout);if(logf){fputs(b,logf);fflush(logf);}}
static unsigned short pix[W*H];
static COLORREF want565(unsigned short p) {unsigned r=p>>11,g=(p>>5)&63,b=p&31;return RGB((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2));}
static COLORREF want555(unsigned short p) {unsigned r=(p>>10)&31,g=(p>>5)&31,b=p&31;return RGB((r<<3)|(r>>2),(g<<3)|(g>>2),(b<<3)|(b>>2));}
#define NF 5
static int f555[NF]; /* the form's own masks (inside the header, or after a 40-byte one) are 5-5-5 */
static int near8(COLORREF a,COLORREF b) {int k;for(k=0;k<24;k+=8){int d=(int)((a>>k)&255)-(int)((b>>k)&255);if(d<-8||d>8)return 0;}return 1;}
static unsigned char hdr[NF][256];static const DWORD hsize[NF]={40,108,124,108,124};
static void make(void) {
 int x,y,k;
 for(y=0;y<H;y++)for(x=0;x<W;x++){unsigned r=(x*31)/(W-1),g=(y*63)/(H-1),b=((x+y)&1)?31:(unsigned)((x^y)&31);pix[y*W+x]=(unsigned short)(r<<11|g<<5|b);}
 /* forms 1-2: 5-6-5 inside the V4/V5 header, 5-5-5 after it; forms 3-4: 5-5-5 inside, 5-6-5 after (the old code took these as 5-6-5) */
 for(k=0;k<NF;k++){BITMAPINFOHEADER *h=(BITMAPINFOHEADER*)hdr[k];DWORD *m=(DWORD*)(hdr[k]+40),*after=(DWORD*)(hdr[k]+hsize[k]);
  memset(hdr[k],0,sizeof(hdr[k]));h->biSize=hsize[k];h->biWidth=W;h->biHeight=-H;h->biPlanes=1;h->biBitCount=16;h->biCompression=BI_BITFIELDS;
  f555[k]=k>=3;
  if(f555[k]){m[0]=0x7C00;m[1]=0x03E0;m[2]=0x001F;}else{m[0]=0xF800;m[1]=0x07E0;m[2]=0x001F;}
  if(k){if(f555[k]){after[0]=0xF800;after[1]=0x07E0;after[2]=0x001F;}else{after[0]=0x7C00;after[1]=0x03E0;after[2]=0x001F;}
   *(DWORD*)(hdr[k]+56)=0x73524742UL;} /* bV4CSType LCS_sRGB */}
}
static int check(HDC dc,int x0,int y0,int inv,COLORREF bg,int form,int *bad) {
 int x,y,n=0;
 for(y=0;y<H;y++)for(x=0;x<W;x++){COLORREF g=GetPixel(dc,x0+x,y0+y),w=f555[form]?want555(pix[y*W+x]):want565(pix[y*W+x]);
  if(inv)w=(w^bg)&0xFFFFFF;
  if(!near8(g,w)){if(n<3)say("      at %d,%d: %06lX, want %06lX\n",x,y,(unsigned long)g,(unsigned long)w);n++;}}
 *bad+=n;return n;
}
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProc(h,m,w,l);}
int main(void) {
 char path[MAX_PATH],*s;WNDCLASS wc;HWND wnd;HDC dc;int bpp,k,r,bad;static const char *nm[NF]={"40-byte, 5-6-5","V4, 5-6-5 inside","V5, 5-6-5 inside","V4, 5-5-5 inside","V5, 5-5-5 inside"};
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXV4.LOG");logf=fopen(path,"w");
 say("VXV4 v1.0 (driver .532; .533): 16-bit BITFIELDS bitmaps with V4/V5 headers (the masks inside the header)\n");
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wp;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="VXV4";wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClass(&wc);
 wnd=CreateWindow("VXV4","VXV4",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,40,40,NF*(W+16)+16,2*(H+16)+40,0,0,wc.hInstance,0);
 UpdateWindow(wnd);Sleep(200);dc=GetDC(wnd);bpp=GetDeviceCaps(dc,BITSPIXEL)*GetDeviceCaps(dc,PLANES);
 say("desktop %d bpp: SRCCOPY%s\n",bpp,bpp==16?" and SRCINVERT (the 16 bpp ROP path)":" (the 32 bpp 5-6-5 path)");
 make();
 for(k=0;k<NF;k++){int t;bad=0;
  for(t=0;t<6;t++){PatBlt(dc,8+k*(W+16),8,W,H,BLACKNESS);GdiFlush();
   r=StretchDIBits(dc,8+k*(W+16),8,W,H,0,0,W,H,pix,(BITMAPINFO*)hdr[k],DIB_RGB_COLORS,SRCCOPY);GdiFlush();
   if(r!=H){say("   %s: StretchDIBits returned %d\n",nm[k],r);bad++;break;}
   check(dc,8+k*(W+16),8,0,0,k,&bad);}
  say("   SRCCOPY   %-24s six shots: %s (%d pixels wrong)\n",nm[k],bad?"WRONG":"right",bad);if(bad)fails++;
  if(bpp==16){bad=0;
   for(t=0;t<6;t++){HBRUSH br=CreateSolidBrush(RGB(0x40,0x80,0xC0)),ob;COLORREF bg;ob=SelectObject(dc,br);PatBlt(dc,8+k*(W+16),H+24,W,H,PATCOPY);SelectObject(dc,ob);DeleteObject(br);GdiFlush();
    bg=GetPixel(dc,8+k*(W+16),H+24);
    r=StretchDIBits(dc,8+k*(W+16),H+24,W,H,0,0,W,H,pix,(BITMAPINFO*)hdr[k],DIB_RGB_COLORS,SRCINVERT);GdiFlush();
    if(r!=H){say("   %s: StretchDIBits SRCINVERT returned %d\n",nm[k],r);bad++;break;}
    check(dc,8+k*(W+16),H+24,1,bg,k,&bad);}
   say("   SRCINVERT %-24s six shots: %s (%d pixels wrong)\n",nm[k],bad?"WRONG":"right",bad);if(bad)fails++;}}
 ReleaseDC(wnd,dc);DestroyWindow(wnd);
 say("%s: %d wrong\n",fails?"FAILED":"PASSED",fails);
 if(logf)fclose(logf);
 return fails?2:0;
}
