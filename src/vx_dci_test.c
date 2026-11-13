/* VXDCI v1.2 (driver v10.251; v1.1 with v10.250, v1.0 with v10.249): what DCI gives a program for the screen. VXGL v2.2's check drew to the front buffer of Microsoft's
 * software OpenGL, which faulted in OPENGL32.DLL writing 001F001F (two blue 565 pixels, the front clear) at 806FFBF0. That renderer
 * works on every other display driver, so the fault is ours: it writes the screen directly through DCI (DCIMAN32.DLL, the display driver's DCICOMMAND escape, answered by the DIB engine
 * from our driver's PDevice), where our driver says the screen is. This logs the primary surface DCI describes (size, stride, depth, masks, selector and offset), the
 * selector's base and limit, where the screen's rows land, and whether each part of it can be read and written, and compares a few
 * pixels read through it with GDI's GetPixel. Nothing is written to the screen. Log: VXDCI.TXT. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct {
 DWORD dwSize,dwDCICaps,dwCompression,dwMask[3],dwWidth,dwHeight;LONG lStride;DWORD dwBitCount,dwOffSurface;WORD wSelSurface,wReserved;
 DWORD dwReserved1,dwReserved2,dwReserved3;void *BeginAccess,*EndAccess,*DestroySurface;
} dci_info;
typedef HDC (WINAPI *open_fn)(void);
typedef void (WINAPI *close_fn)(HDC);
typedef int (WINAPI *create_fn)(HDC,dci_info**);
typedef int (WINAPI *begin_fn)(dci_info*,int,int,int,int);
typedef void (WINAPI *end_fn)(dci_info*);
typedef void (WINAPI *destroy_fn)(dci_info*);
static FILE *logf;
/* v1.2: a selector's base and limit */
static void sel_line(const char *what,WORD sel) {
 LDT_ENTRY e;DWORD base,limit;
 if(!sel){fprintf(logf,"   %s: selector 0\n",what);return;}
 if(!GetThreadSelectorEntry(GetCurrentThread(),sel,&e)){fprintf(logf,"   %s: selector %04X not readable (%lu)\n",what,sel,(unsigned long)GetLastError());return;}
 base=e.BaseLow|((DWORD)e.HighWord.Bytes.BaseMid<<16)|((DWORD)e.HighWord.Bytes.BaseHi<<24);
 limit=e.LimitLow|((DWORD)e.HighWord.Bits.LimitHi<<16);if(e.HighWord.Bits.Granularity)limit=(limit<<12)|0xFFF;
 fprintf(logf,"   %s: selector %04X base %08lX limit %08lX\n",what,sel,(unsigned long)base,(unsigned long)limit);
}

static const char *ok(int r) {return r?"yes":"NO";}
static void rows(const char *what,BYTE *p,DWORD n,LONG stride,DWORD h) {
 DWORD ys[5],k;ys[0]=0;ys[1]=h/4;ys[2]=h/2;ys[3]=h*3/4;ys[4]=h?h-1:0;
 fprintf(logf,"   %s: row bytes %lu\n",what,(unsigned long)n);
 for(k=0;k<5;k++){BYTE *r=p+(LONG)ys[k]*stride;
  fprintf(logf,"     row %4lu at %08lX: readable %s, writable %s\n",(unsigned long)ys[k],(unsigned long)r,ok(!IsBadReadPtr(r,n)),ok(!IsBadWritePtr(r,n)));}
}
int main(void) {
 HMODULE m;open_fn op;close_fn cl;create_fn cr;begin_fn be;end_fn en;destroy_fn de;HDC dc;dci_info *s=0;int rc;
 logf=fopen("VXDCI.TXT","w");if(!logf)return 1;
 fprintf(logf,"VXDCI v1.2 (driver v10.251): the screen as DCI describes it\n");
 {HDC d=GetDC(0);fprintf(logf,"desktop %dx%dx%d\n",GetDeviceCaps(d,HORZRES),GetDeviceCaps(d,VERTRES),GetDeviceCaps(d,BITSPIXEL));ReleaseDC(0,d);}
 m=LoadLibraryA("DCIMAN32.DLL");if(!m){fprintf(logf,"DCIMAN32.DLL not loaded\n");fclose(logf);return 1;}
 op=(open_fn)GetProcAddress(m,"DCIOpenProvider");cl=(close_fn)GetProcAddress(m,"DCICloseProvider");cr=(create_fn)GetProcAddress(m,"DCICreatePrimary");
 be=(begin_fn)GetProcAddress(m,"DCIBeginAccess");en=(end_fn)GetProcAddress(m,"DCIEndAccess");de=(destroy_fn)GetProcAddress(m,"DCIDestroy");
 if(!op||!cl||!cr||!be||!en||!de){fprintf(logf,"DCIMAN32.DLL lacks an export\n");fclose(logf);return 1;}
 dc=op();fprintf(logf,"DCIOpenProvider: %p\n",(void*)dc);if(!dc){fclose(logf);return 1;}
 rc=cr(dc,&s);fprintf(logf,"DCICreatePrimary: %d, surface %p\n",rc,(void*)s);
 { /* v1.2: what our driver did with the request (escape 0x8F73) and where its frame buffer is (VXDDRAW.DLL) */
  DWORD d[10];HDC sd=GetDC(0);HMODULE h;DWORD (WINAPI *vram)(DWORD*,DWORD*);DWORD lin=0,size=0;memset(d,0,sizeof(d));
  if(ExtEscape(sd,0x8F73,0,0,sizeof(d),(LPSTR)d)>0)
   fprintf(logf,"   driver: %lu create(s), DIB engine returned %ld, output %04lX:%04lX first dword %08lX, form %lu (1 structure, 2 pointer), surface %04lX:%04lX had offset %08lX selector %04lX, written selector %04lX offset %08lX\n",
    (unsigned long)d[0],(long)d[1],(unsigned long)(d[2]>>16),(unsigned long)(d[2]&0xFFFF),(unsigned long)d[3],(unsigned long)d[4],(unsigned long)(d[5]>>16),(unsigned long)(d[5]&0xFFFF),
    (unsigned long)d[6],(unsigned long)d[7],(unsigned long)d[8],(unsigned long)d[9]);
  else fprintf(logf,"   driver: no answer to escape 8F73 (older than v10.251)\n");
  ReleaseDC(0,sd);
  if(d[8])sel_line("the screen selector we wrote",(WORD)d[8]);
  h=LoadLibraryA("VXDDRAW.DLL");vram=h?(DWORD (WINAPI *)(DWORD*,DWORD*))GetProcAddress(h,"VX_DD_Vram"):0;
  if(vram&&vram(&lin,&size))fprintf(logf,"   frame buffer (VX_DD_Vram): %08lX, %lu KB\n",(unsigned long)lin,(unsigned long)(size>>10));
 }
 if(rc>=0&&s&&!IsBadReadPtr(s,sizeof(*s))) {
  DWORD base=0,limit=0,bpp=s->dwBitCount,rowbytes;BYTE *flat;int r2;
  fprintf(logf,"   size %lu caps %08lX compression %lu masks %08lX %08lX %08lX\n",(unsigned long)s->dwSize,(unsigned long)s->dwDCICaps,(unsigned long)s->dwCompression,
   (unsigned long)s->dwMask[0],(unsigned long)s->dwMask[1],(unsigned long)s->dwMask[2]);
  fprintf(logf,"   %lux%lu, %lu bpp, stride %ld, offset %08lX, selector %04X, reserved %08lX %08lX %08lX\n",(unsigned long)s->dwWidth,(unsigned long)s->dwHeight,
   (unsigned long)bpp,(long)s->lStride,(unsigned long)s->dwOffSurface,s->wSelSurface,(unsigned long)s->dwReserved1,(unsigned long)s->dwReserved2,(unsigned long)s->dwReserved3);
  sel_line("reserved1's selector",(WORD)(s->dwReserved1>>16));sel_line("reserved2's selector",(WORD)(s->dwReserved2>>16));
  if(s->wSelSurface){LDT_ENTRY e;
   if(GetThreadSelectorEntry(GetCurrentThread(),s->wSelSurface,&e)){
    base=e.BaseLow|((DWORD)e.HighWord.Bytes.BaseMid<<16)|((DWORD)e.HighWord.Bytes.BaseHi<<24);
    limit=e.LimitLow|((DWORD)e.HighWord.Bits.LimitHi<<16);if(e.HighWord.Bits.Granularity)limit=(limit<<12)|0xFFF;
    fprintf(logf,"   selector %04X: base %08lX limit %08lX (%lu KB), the surface needs offset + stride x height = %08lX\n",s->wSelSurface,(unsigned long)base,
     (unsigned long)limit,(unsigned long)((limit+1)>>10),(unsigned long)(s->dwOffSurface+(DWORD)(s->lStride<0?-s->lStride:s->lStride)*s->dwHeight));}
   else fprintf(logf,"   selector %04X: GetThreadSelectorEntry failed %lu\n",s->wSelSurface,(unsigned long)GetLastError());}
  flat=(BYTE*)(base+s->dwOffSurface);rowbytes=s->dwWidth*((bpp+7)/8);
  fprintf(logf,"   flat address (base + offset) %08lX, last byte %08lX\n",(unsigned long)flat,(unsigned long)(flat+(LONG)(s->dwHeight?s->dwHeight-1:0)*s->lStride+rowbytes-1));
  rows("base + offset",flat,rowbytes,s->lStride,s->dwHeight);
  if(s->wSelSurface)rows("offset alone (as a flat pointer)",(BYTE*)s->dwOffSurface,rowbytes,s->lStride,s->dwHeight);
  r2=be(s,0,0,(int)s->dwWidth,(int)s->dwHeight);fprintf(logf,"   DCIBeginAccess (whole screen): %d\n",r2);
  if(r2>=0) {
   static const int px[4][2]={{0,0},{100,10},{500,300},{1000,700}};int k;HDC sd=GetDC(0);
   for(k=0;k<4;k++){DWORD x=(DWORD)px[k][0],y=(DWORD)px[k][1],v=0,g;COLORREF c;BYTE *a;
    if(x>=s->dwWidth||y>=s->dwHeight)continue;
    a=flat+(LONG)y*s->lStride+x*((bpp+7)/8);c=GetPixel(sd,(int)x,(int)y);g=((DWORD)GetRValue(c)<<16)|((DWORD)GetGValue(c)<<8)|GetBValue(c);
    if(bpp==16)g=((DWORD)(GetRValue(c)>>3)<<19)|((DWORD)(GetGValue(c)>>2)<<10)|((DWORD)(GetBValue(c)>>3)<<3); /* v1.1: both as 565 */
    if(!IsBadReadPtr(a,4)){if(bpp==16){WORD w=*(WORD*)a;v=(((w>>11)&31)<<19)|(((w>>5)&63)<<10)|((w&31)<<3);}else v=*(DWORD*)a&0xFFFFFF;}
    fprintf(logf,"   pixel %4lu,%4lu at %08lX: DCI %06lX, GetPixel %06lX%s\n",(unsigned long)x,(unsigned long)y,(unsigned long)a,(unsigned long)v,(unsigned long)g,v==g?"":"  DIFFERENT");}
   ReleaseDC(0,sd);en(s);
  }
  de(s);
 }
 cl(dc);
 fprintf(logf,"done\n");fclose(logf);
 return 0;
}
