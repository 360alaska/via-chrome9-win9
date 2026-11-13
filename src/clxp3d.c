/* CLXP3D v2.6: Windows XP Direct3D 7 case runner for the VX855 command-ring capture. MIT, 2026.
 * Each case is one small piece of fixed-function state drawn once into a 256x192 video-memory target, flushed (the target is
 * locked and read back, which makes XP's driver submit and finish the draw), and saved as CLXP3Dnn.BIN (256x192 X8R8G8B8, rows
 * top-down). Everything is logged to CLXP3D.TXT. The baseline state is restored after every case, so a capture of one case holds
 * only that case's state changes.
 *
 * v2.0: the command-ring capture is built in (see capture_init): every case writes Tnn.RNG next to its BIN. Run as Administrator,
 * inpout32.dll and CXPHYS.SYS next to the EXE (v2.2).
 * Usage:
 *   CLXP3D                     all cases with the built-in capture
 *   CLXP3D probe               capture setup only (CLXP3D_PROBE.TXT), no drawing
 *   CLXP3D all nocapture       pixels only
 *   CLXP3D nn                  case nn only
 *   CLXP3D all pause           all cases; before and after each case it stops and asks you to take the PRE / END snapshot
 *   CLXP3D nn pause            the same for one case
 *   CLXP3D all "command"       all cases; runs "command" before and after each case, with {T} replaced by the case tag (T05)
 *                              and {P} by PRE or END, e.g.  CLXP3D all "RINGCAP.EXE {T}_{P}.RNG"
 *   CLXP3D from nn [nocapture] cases nn to the end (v2.4: CLXP3D from 68 runs the v2.4 and v2.5 cases, from 116 only v2.5's, from 138 only v2.6's)
 *   CLXP3D list                list the cases
 * A case the XP driver does not support (format or capability missing) is logged as SKIPPED and drawn as nothing. */
#define COBJMACROS
#define INITGUID
#define DIRECTDRAW_VERSION 0x0700
#define DIRECT3D_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const GUID iid_dd7={0x15E65EC0UL,0x3B9C,0x11D2,{0xB9,0x2F,0,0x60,0x97,0x97,0xEA,0x5B}};
#define W 256
#define H 192
typedef struct vtx {float x,y,z,rhw;DWORD diffuse,specular;float u0,v0,u1,v1;} vtx;
#define FVF (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEX2)
static FILE *logf;
static IDirectDraw7 *dd;static IDirect3D7 *d3d;static IDirect3DDevice7 *dev;
static IDirectDrawSurface7 *rt;
static HWND window;
static const char *hook;static int pause_mode;
static D3DDEVICEDESC7 caps;
static DDPIXELFORMAT texfmt[32];static int ntexfmt;
static DDPIXELFORMAT zfmt[16];static int nzfmt;
static DWORD f2dw(float f){DWORD d;memcpy(&d,&f,4);return d;}
static int ok(const char *what,HRESULT hr){if(FAILED(hr)){fprintf(logf,"   %s = %08lX\n",what,(unsigned long)hr);fflush(logf);return 0;}return 1;}
static void rs(D3DRENDERSTATETYPE k,DWORD v){IDirect3DDevice7_SetRenderState(dev,k,v);}
static void ts(DWORD st,D3DTEXTURESTAGESTATETYPE k,DWORD v){IDirect3DDevice7_SetTextureStageState(dev,st,k,v);}
/* ---- surfaces ---- */
static DWORD pack(const DDPIXELFORMAT *f,DWORD argb) {
 DWORD m[4],c[4],k,out=0;
 m[0]=f->dwRGBAlphaBitMask;m[1]=f->dwRBitMask;m[2]=f->dwGBitMask;m[3]=f->dwBBitMask;
 c[0]=argb>>24;c[1]=(argb>>16)&255;c[2]=(argb>>8)&255;c[3]=argb&255;
 if(f->dwFlags&DDPF_LUMINANCE){m[1]=f->dwLuminanceBitMask;m[2]=m[3]=0;c[1]=(c[1]*77+c[2]*150+c[3]*29)>>8;}
 if(!(f->dwFlags&DDPF_ALPHAPIXELS))m[0]=0;
 for(k=0;k<4;k++)if(m[k]){DWORD sh=0,bits=0,mm=m[k];while(!(mm&1)){mm>>=1;sh++;}while(mm&1){mm>>=1;bits++;}out|=((c[k]>>(8-bits))<<sh)&m[k];}
 return out;
}
static DWORD unpack_rt(const DDSURFACEDESC2 *s,const BYTE *p) {
 DWORD v=0,bpp=s->ddpfPixelFormat.dwRGBBitCount,r,g,b;
 if(bpp==32){memcpy(&v,p,4);return v&0xFFFFFF;}
 if(bpp==16){WORD w;memcpy(&w,p,2);
  if(s->ddpfPixelFormat.dwGBitMask==0x7E0){r=(w>>11)&31;g=(w>>5)&63;b=w&31;return (((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|(b<<3)|(b>>2);}
  r=(w>>10)&31;g=(w>>5)&31;b=w&31;return (((r<<3)|(r>>2))<<16)|(((g<<3)|(g>>2))<<8)|(b<<3)|(b>>2);}
 return 0;
}
typedef DWORD (*texel_fn)(DWORD w,DWORD h,DWORD x,DWORD y,DWORD level);
static int fill(IDirectDrawSurface7 *s,texel_fn fn,DWORD level) {
 DDSURFACEDESC2 d;DWORD x,y,by;
 memset(&d,0,sizeof(d));d.dwSize=sizeof(d);
 if(!ok("Lock(texture)",IDirectDrawSurface7_Lock(s,0,&d,DDLOCK_WAIT|DDLOCK_WRITEONLY,0)))return 0;
 if(d.ddpfPixelFormat.dwFlags&DDPF_FOURCC) { /* DXT1: one solid colour per 4x4 block (c0, indices 0) */
  /* v1.1: rows of blocks are (width/4)*block bytes (a compressed surface's lPitch is its linear size); DXT2..5 blocks are 16 bytes, alpha first */
  DWORD bs=d.ddpfPixelFormat.dwFourCC==MAKEFOURCC('D','X','T','1')?8:16,row=((d.dwWidth+3)/4)*bs,k;
  for(y=0;y<d.dwHeight;y+=4)for(x=0;x<d.dwWidth;x+=4){
   BYTE *b=(BYTE*)d.lpSurface+(y/4)*row+(x/4)*bs;DWORD c=fn(d.dwWidth,d.dwHeight,x,y,level);
   WORD c565=(WORD)(((c>>8)&0xF800)|((c>>5)&0x7E0)|((c>>3)&31));
   if(bs==16){DWORD a=c>>24;
    if(d.ddpfPixelFormat.dwFourCC==MAKEFOURCC('D','X','T','5')){b[0]=(BYTE)a;b[1]=(BYTE)a;for(k=2;k<8;k++)b[k]=0;}
    else for(k=0;k<8;k++)b[k]=(BYTE)((a>>4)*0x11);
    b+=8;}
   b[0]=(BYTE)c565;b[1]=(BYTE)(c565>>8);b[2]=0;b[3]=0;b[4]=b[5]=b[6]=b[7]=0;
  }
 } else {
  by=d.ddpfPixelFormat.dwRGBBitCount/8;
  for(y=0;y<d.dwHeight;y++)for(x=0;x<d.dwWidth;x++){
   DWORD v=pack(&d.ddpfPixelFormat,fn(d.dwWidth,d.dwHeight,x,y,level));BYTE *p=(BYTE*)d.lpSurface+y*d.lPitch+x*by;
   if(by==4)memcpy(p,&v,4);else if(by==2){WORD w=(WORD)v;memcpy(p,&w,2);}else *p=(BYTE)v;
  }
 }
 IDirectDrawSurface7_Unlock(s,0);return 1;
}
static const DDPIXELFORMAT *find_fmt(DWORD flags,DWORD bits,DWORD r,DWORD g,DWORD b,DWORD a,DWORD fourcc) {
 int k;
 for(k=0;k<ntexfmt;k++){const DDPIXELFORMAT *f=&texfmt[k];
  if(fourcc){if((f->dwFlags&DDPF_FOURCC)&&f->dwFourCC==fourcc)return f;continue;}
  if((f->dwFlags&(DDPF_RGB|DDPF_LUMINANCE|DDPF_ALPHAPIXELS|DDPF_FOURCC))!=flags||f->dwRGBBitCount!=bits)continue;
  if(f->dwRBitMask==r&&f->dwGBitMask==g&&f->dwBBitMask==b&&((flags&DDPF_ALPHAPIXELS)?f->dwRGBAlphaBitMask==a:1))return f;}
 return 0;
}
static const DDPIXELFORMAT *ARGB(void){return find_fmt(DDPF_RGB|DDPF_ALPHAPIXELS,32,0xFF0000,0xFF00,0xFF,0xFF000000UL,0);}
static IDirectDrawSurface7 *texture(const DDPIXELFORMAT *f,DWORD w,DWORD h,DWORD levels,texel_fn fn) {
 DDSURFACEDESC2 d;IDirectDrawSurface7 *t=0,*s,*n;DWORD k;DDSCAPS2 c;
 if(!f){fprintf(logf,"   texture format not offered by the driver\n");return 0;}
 memset(&d,0,sizeof(d));d.dwSize=sizeof(d);d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;d.dwWidth=w;d.dwHeight=h;d.ddpfPixelFormat=*f;
 d.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY;
 if(levels>1){d.dwFlags|=DDSD_MIPMAPCOUNT;d.dwMipMapCount=levels;d.ddsCaps.dwCaps|=DDSCAPS_MIPMAP|DDSCAPS_COMPLEX;}
 if(!ok("CreateSurface(texture)",IDirectDraw7_CreateSurface(dd,&d,&t,0)))return 0;
 s=t;IDirectDrawSurface7_AddRef(s);
 for(k=0;k<levels;k++){
  fill(s,fn,k);
  if(k+1<levels){memset(&c,0,sizeof(c));c.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_MIPMAP;
   if(FAILED(IDirectDrawSurface7_GetAttachedSurface(s,&c,&n))){IDirectDrawSurface7_Release(s);s=0;break;}
   IDirectDrawSurface7_Release(s);s=n;}
 }
 if(s)IDirectDrawSurface7_Release(s);
 return t;
}
/* texel patterns: quadrants red, green, blue, white with alpha FF, C0, 80, 40 */
static DWORD quad(DWORD w,DWORD h,DWORD x,DWORD y,DWORD l){static const DWORD c[4]={0xFFFF0000UL,0xC000FF00UL,0x800000FFUL,0x40FFFFFFUL};(void)l;return c[(x>=w/2?1:0)+(y>=h/2?2:0)];}
static DWORD quad_opaque(DWORD w,DWORD h,DWORD x,DWORD y,DWORD l){return quad(w,h,x,y,l)|0xFF000000UL;}
static DWORD grey(DWORD w,DWORD h,DWORD x,DWORD y,DWORD l){(void)w;(void)h;(void)x;(void)y;(void)l;return 0xFF808080UL;}
static DWORD levels(DWORD w,DWORD h,DWORD x,DWORD y,DWORD l){static const DWORD c[6]={0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL,0xFFFFFF00UL,0xFF00FFFFUL,0xFFFF00FFUL};(void)w;(void)h;(void)x;(void)y;return c[l%6];}
static DWORD keyed(DWORD w,DWORD h,DWORD x,DWORD y,DWORD l){return ((x^y)&1)?0xFFFF00FFUL:quad_opaque(w,h,x,y,l);}
/* ---- drawing ---- */
static void corner(vtx *v,float x,float y,float z,float rhw,DWORD c,float u,float t){memset(v,0,sizeof(*v));v->x=x;v->y=y;v->z=z;v->rhw=rhw;v->diffuse=c;v->specular=0xFF000000UL;v->u0=v->u1=u;v->v0=v->v1=t;}
static int draw(vtx *v,DWORD n){return ok("DrawPrimitive",IDirect3DDevice7_DrawPrimitive(dev,D3DPT_TRIANGLELIST,FVF,v,n,0));}
/* the standard quad (32,32)-(160,160), uv 0..s, colours per corner tl tr bl br */
static int quad_draw(float s,DWORD tl,DWORD tr,DWORD bl,DWORD br,float z) {
 vtx v[6];
 corner(&v[0],32,32,z,1,tl,0,0);corner(&v[1],160,32,z,1,tr,s,0);corner(&v[2],32,160,z,1,bl,0,s);
 v[3]=v[1];corner(&v[4],160,160,z,1,br,s,s);v[5]=v[2];
 return draw(v,6);
}
/* ---- baseline ---- */
static void baseline(void) {
 DWORD s;
 rs(D3DRENDERSTATE_ZENABLE,FALSE);rs(D3DRENDERSTATE_ZWRITEENABLE,FALSE);rs(D3DRENDERSTATE_LIGHTING,FALSE);rs(D3DRENDERSTATE_CULLMODE,D3DCULL_NONE);
 rs(D3DRENDERSTATE_SHADEMODE,D3DSHADE_GOURAUD);rs(D3DRENDERSTATE_SPECULARENABLE,FALSE);rs(D3DRENDERSTATE_FOGENABLE,FALSE);
 rs(D3DRENDERSTATE_FOGTABLEMODE,D3DFOG_NONE);rs(D3DRENDERSTATE_ALPHABLENDENABLE,FALSE);rs(D3DRENDERSTATE_ALPHATESTENABLE,FALSE);
 rs(D3DRENDERSTATE_SRCBLEND,D3DBLEND_ONE);rs(D3DRENDERSTATE_DESTBLEND,D3DBLEND_ZERO);rs(D3DRENDERSTATE_COLORKEYENABLE,FALSE);
 rs(D3DRENDERSTATE_STENCILENABLE,FALSE);rs(D3DRENDERSTATE_DITHERENABLE,FALSE);rs(D3DRENDERSTATE_TEXTUREPERSPECTIVE,TRUE);
 rs(D3DRENDERSTATE_ZFUNC,D3DCMP_LESSEQUAL);rs(D3DRENDERSTATE_TEXTUREFACTOR,0xFF808080UL);
 for(s=0;s<2;s++){
  IDirect3DDevice7_SetTexture(dev,s,0);
  ts(s,D3DTSS_COLOROP,s?D3DTOP_DISABLE:D3DTOP_SELECTARG1);ts(s,D3DTSS_COLORARG1,D3DTA_TEXTURE);ts(s,D3DTSS_COLORARG2,s?D3DTA_CURRENT:D3DTA_DIFFUSE);
  ts(s,D3DTSS_ALPHAOP,s?D3DTOP_DISABLE:D3DTOP_SELECTARG1);ts(s,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);ts(s,D3DTSS_ALPHAARG2,s?D3DTA_CURRENT:D3DTA_DIFFUSE);
  ts(s,D3DTSS_MAGFILTER,D3DTFG_POINT);ts(s,D3DTSS_MINFILTER,D3DTFN_POINT);ts(s,D3DTSS_MIPFILTER,D3DTFP_NONE);
  ts(s,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);ts(s,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);ts(s,D3DTSS_TEXCOORDINDEX,s);
 }
}
static void colorop(DWORD st,DWORD op,DWORD a1,DWORD a2){ts(st,D3DTSS_COLOROP,op);ts(st,D3DTSS_COLORARG1,a1);ts(st,D3DTSS_COLORARG2,a2);}
static void alphaop(DWORD st,DWORD op,DWORD a1,DWORD a2){ts(st,D3DTSS_ALPHAOP,op);ts(st,D3DTSS_ALPHAARG1,a1);ts(st,D3DTSS_ALPHAARG2,a2);}
/* ---- cases ---- */
#define SKIP(why) do{fprintf(logf,"   SKIPPED: %s\n",why);return 0;}while(0)
static IDirectDrawSurface7 *tex0,*tex1,*zcase;
static int use_tex(IDirectDrawSurface7 *t){tex0=t;if(!t)return 0;return ok("SetTexture",IDirect3DDevice7_SetTexture(dev,0,t));}
static int c_replace(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");return quad_draw(1,~0UL,~0UL,~0UL,~0UL,.5f);}
static int c_mod_flat(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");colorop(0,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE);return quad_draw(1,0xFF808080UL,0xFF808080UL,0xFF808080UL,0xFF808080UL,.5f);}
static int c_mod_gouraud(void){if(!use_tex(texture(ARGB(),8,8,1,quad_opaque)))SKIP("texture");colorop(0,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE);alphaop(0,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE);return quad_draw(1,0xFFFFFFFFUL,0x80FF8000UL,0x400080FFUL,0xFF404040UL,.5f);}
static int c_mod_current(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");colorop(0,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_CURRENT);alphaop(0,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_CURRENT);return quad_draw(1,0xC0808080UL,0xC0808080UL,0xC0808080UL,0xC0808080UL,.5f);}
static int c_tex_rgb_diff_a(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");alphaop(0,D3DTOP_SELECTARG1,D3DTA_DIFFUSE,D3DTA_TEXTURE);
 rs(D3DRENDERSTATE_ALPHABLENDENABLE,TRUE);rs(D3DRENDERSTATE_SRCBLEND,D3DBLEND_SRCALPHA);rs(D3DRENDERSTATE_DESTBLEND,D3DBLEND_INVSRCALPHA);return quad_draw(1,0x80FFFFFFUL,0x80FFFFFFUL,0x80FFFFFFUL,0x80FFFFFFUL,.5f);}
static int c_diff_rgb_tex_a(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");colorop(0,D3DTOP_SELECTARG1,D3DTA_DIFFUSE,D3DTA_TEXTURE);
 rs(D3DRENDERSTATE_ALPHABLENDENABLE,TRUE);rs(D3DRENDERSTATE_SRCBLEND,D3DBLEND_SRCALPHA);rs(D3DRENDERSTATE_DESTBLEND,D3DBLEND_INVSRCALPHA);return quad_draw(1,0xFFFFC000UL,0xFFFFC000UL,0xFFFFC000UL,0xFFFFC000UL,.5f);}
static int c_add(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");colorop(0,D3DTOP_ADD,D3DTA_TEXTURE,D3DTA_DIFFUSE);return quad_draw(1,0xFF404040UL,0xFF404040UL,0xFF404040UL,0xFF404040UL,.5f);}
static int c_mod2x(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");colorop(0,D3DTOP_MODULATE2X,D3DTA_TEXTURE,D3DTA_DIFFUSE);return quad_draw(1,0xFF606060UL,0xFF606060UL,0xFF606060UL,0xFF606060UL,.5f);}
static int c_mod4x(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");colorop(0,D3DTOP_MODULATE4X,D3DTA_TEXTURE,D3DTA_DIFFUSE);return quad_draw(1,0xFF303030UL,0xFF303030UL,0xFF303030UL,0xFF303030UL,.5f);}
static int c_addsigned(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");colorop(0,D3DTOP_ADDSIGNED,D3DTA_TEXTURE,D3DTA_DIFFUSE);return quad_draw(1,0xFFA0A0A0UL,0xFFA0A0A0UL,0xFFA0A0A0UL,0xFFA0A0A0UL,.5f);}
static int c_blend_tex_a(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");colorop(0,D3DTOP_BLENDTEXTUREALPHA,D3DTA_TEXTURE,D3DTA_DIFFUSE);return quad_draw(1,0xFF0000FFUL,0xFF0000FFUL,0xFF0000FFUL,0xFF0000FFUL,.5f);}
static int c_blend_diff_a(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");colorop(0,D3DTOP_BLENDDIFFUSEALPHA,D3DTA_TEXTURE,D3DTA_DIFFUSE);return quad_draw(1,0xFF00FF00UL,0x8000FF00UL,0x4000FF00UL,0x0000FF00UL,.5f);}
static int c_tfactor(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");colorop(0,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_TFACTOR);rs(D3DRENDERSTATE_TEXTUREFACTOR,0xFF40C080UL);return quad_draw(1,~0UL,~0UL,~0UL,~0UL,.5f);}
static int c_diffuse_only(void){colorop(0,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_DIFFUSE);alphaop(0,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_DIFFUSE);return quad_draw(1,0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL,0xFFFFFFFFUL,.5f);}
static int specular_quad(void){vtx v[6];
 corner(&v[0],32,32,.5f,1,0xFF800000UL,0,0);corner(&v[1],160,32,.5f,1,0xFF008000UL,1,0);corner(&v[2],32,160,.5f,1,0xFF000080UL,0,1);v[3]=v[1];corner(&v[4],160,160,.5f,1,0xFF404040UL,1,1);v[5]=v[2];
 v[0].specular=0xFF004080UL;v[1].specular=v[3].specular=0xFF800000UL;v[2].specular=v[5].specular=0xFF008000UL;v[4].specular=0xFF404040UL;
 return draw(v,6);}
static int c_spec_diffuse(void){colorop(0,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_DIFFUSE);rs(D3DRENDERSTATE_SPECULARENABLE,TRUE);return specular_quad();}
static int c_spec_tex(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");colorop(0,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE);rs(D3DRENDERSTATE_SPECULARENABLE,TRUE);return specular_quad();}
static int c_fog_vertex(void){vtx v[6];
 if(!use_tex(texture(ARGB(),8,8,1,quad_opaque)))SKIP("texture");rs(D3DRENDERSTATE_FOGENABLE,TRUE);rs(D3DRENDERSTATE_FOGCOLOR,0x00406080UL);
 corner(&v[0],32,32,.5f,1,~0UL,0,0);corner(&v[1],160,32,.5f,1,~0UL,1,0);corner(&v[2],32,160,.5f,1,~0UL,0,1);v[3]=v[1];corner(&v[4],160,160,.5f,1,~0UL,1,1);v[5]=v[2];
 v[0].specular=v[2].specular=v[5].specular=0xFF000000UL;v[1].specular=v[3].specular=v[4].specular=0x00000000UL; /* fog factor in specular alpha: left clear, right fogged */
 return draw(v,6);}
static int c_fog_table(DWORD mode){vtx v[6];float st=.2f,en=.8f,dens=2.f;
 if(!(caps.dpcTriCaps.dwRasterCaps&D3DPRASTERCAPS_FOGTABLE))SKIP("no D3DPRASTERCAPS_FOGTABLE");
 if(!use_tex(texture(ARGB(),8,8,1,quad_opaque)))SKIP("texture");rs(D3DRENDERSTATE_FOGENABLE,TRUE);rs(D3DRENDERSTATE_FOGCOLOR,0x00406080UL);rs(D3DRENDERSTATE_FOGTABLEMODE,mode);
 rs(D3DRENDERSTATE_FOGTABLESTART,f2dw(st));rs(D3DRENDERSTATE_FOGTABLEEND,f2dw(en));rs(D3DRENDERSTATE_FOGTABLEDENSITY,f2dw(dens));
 corner(&v[0],32,32,0,1,~0UL,0,0);corner(&v[1],160,32,1,1,~0UL,1,0);corner(&v[2],32,160,0,1,~0UL,0,1);v[3]=v[1];corner(&v[4],160,160,1,1,~0UL,1,1);v[5]=v[2];
 v[1].z=v[3].z=v[4].z=.999f;return draw(v,6);}
static int c_fog_lin(void){return c_fog_table(D3DFOG_LINEAR);}
static int c_fog_exp(void){return c_fog_table(D3DFOG_EXP);}
static int c_perspective(void){vtx v[6];
 if(!use_tex(texture(ARGB(),8,8,1,quad_opaque)))SKIP("texture");ts(0,D3DTSS_ADDRESSU,D3DTADDRESS_WRAP);ts(0,D3DTSS_ADDRESSV,D3DTADDRESS_WRAP);
 /* a floor receding to the right: near edge rhw 1, far edge rhw 0.25; u and v run 0..4 in object space */
 corner(&v[0],16,16,.2f,1,~0UL,0,0);corner(&v[1],240,72,.9f,.25f,~0UL,4,0);corner(&v[2],16,176,.2f,1,~0UL,0,4);v[3]=v[1];corner(&v[4],240,120,.9f,.25f,~0UL,4,4);v[5]=v[2];
 return draw(v,6);}
static int c_affine(void){rs(D3DRENDERSTATE_TEXTUREPERSPECTIVE,FALSE);return c_perspective();}
static int c_mip(DWORD filter,float s){
 if(!(caps.dpcTriCaps.dwTextureFilterCaps&(D3DPTFILTERCAPS_MIPFPOINT|D3DPTFILTERCAPS_MIPNEAREST)))SKIP("no mipmap filter caps");
 if(!use_tex(texture(ARGB(),32,32,6,levels)))SKIP("mipmapped texture");ts(0,D3DTSS_MIPFILTER,filter);ts(0,D3DTSS_ADDRESSU,D3DTADDRESS_WRAP);ts(0,D3DTSS_ADDRESSV,D3DTADDRESS_WRAP);
 ts(0,D3DTSS_MINFILTER,D3DTFN_LINEAR);ts(0,D3DTSS_MAGFILTER,D3DTFG_LINEAR);return quad_draw(s,~0UL,~0UL,~0UL,~0UL,.5f);}
static int c_mip_point(void){return c_mip(D3DTFP_POINT,16);}
static int c_mip_linear(void){return c_mip(D3DTFP_LINEAR,12);}
static int c_mip_bias(void){ts(0,D3DTSS_MIPMAPLODBIAS,f2dw(1.0f));return c_mip(D3DTFP_POINT,8);}
static int c_two_stage(void){
 if(caps.wMaxSimultaneousTextures<2)SKIP("one texture only");
 if(!use_tex(texture(ARGB(),8,8,1,quad_opaque)))SKIP("texture 0");
 tex1=texture(ARGB(),4,4,1,grey);if(!tex1||!ok("SetTexture(1)",IDirect3DDevice7_SetTexture(dev,1,tex1)))SKIP("texture 1");
 colorop(1,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_CURRENT);alphaop(1,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_CURRENT);ts(1,D3DTSS_TEXCOORDINDEX,1);
 ts(1,D3DTSS_MAGFILTER,D3DTFG_LINEAR);ts(1,D3DTSS_MINFILTER,D3DTFN_LINEAR);return quad_draw(1,~0UL,~0UL,~0UL,~0UL,.5f);}
static int c_two_stage_add(void){
 if(caps.wMaxSimultaneousTextures<2)SKIP("one texture only");
 if(!use_tex(texture(ARGB(),8,8,1,quad_opaque)))SKIP("texture 0");
 tex1=texture(ARGB(),4,4,1,grey);if(!tex1||!ok("SetTexture(1)",IDirect3DDevice7_SetTexture(dev,1,tex1)))SKIP("texture 1");
 colorop(0,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE);colorop(1,D3DTOP_ADD,D3DTA_TEXTURE,D3DTA_CURRENT);alphaop(1,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_CURRENT);ts(1,D3DTSS_TEXCOORDINDEX,1);
 return quad_draw(1,0xFFC0C0C0UL,0xFFC0C0C0UL,0xFFC0C0C0UL,0xFFC0C0C0UL,.5f);}
static int fmt_case(DWORD flags,DWORD bits,DWORD r,DWORD g,DWORD b,DWORD a,DWORD fourcc){
 const DDPIXELFORMAT *f=find_fmt(flags,bits,r,g,b,a,fourcc);if(!f)SKIP("format not offered");
 if(!use_tex(texture(f,8,8,1,quad)))SKIP("texture");return quad_draw(1,~0UL,~0UL,~0UL,~0UL,.5f);}
static int c_565(void){return fmt_case(DDPF_RGB,16,0xF800,0x7E0,0x1F,0,0);}
static int c_1555(void){return fmt_case(DDPF_RGB|DDPF_ALPHAPIXELS,16,0x7C00,0x3E0,0x1F,0x8000,0);}
static int c_4444(void){return fmt_case(DDPF_RGB|DDPF_ALPHAPIXELS,16,0xF00,0xF0,0xF,0xF000,0);}
static int c_555(void){return fmt_case(DDPF_RGB,16,0x7C00,0x3E0,0x1F,0,0);}
static int c_x8(void){return fmt_case(DDPF_RGB,32,0xFF0000,0xFF00,0xFF,0,0);}
static int c_l8(void){int k;for(k=0;k<ntexfmt;k++)if((texfmt[k].dwFlags&DDPF_LUMINANCE)&&texfmt[k].dwRGBBitCount==8&&!(texfmt[k].dwFlags&DDPF_ALPHAPIXELS)){if(!use_tex(texture(&texfmt[k],8,8,1,quad)))SKIP("texture");return quad_draw(1,~0UL,~0UL,~0UL,~0UL,.5f);}SKIP("format not offered");}
static int c_a8l8(void){int k;for(k=0;k<ntexfmt;k++)if((texfmt[k].dwFlags&DDPF_LUMINANCE)&&(texfmt[k].dwFlags&DDPF_ALPHAPIXELS)&&texfmt[k].dwRGBBitCount==16){if(!use_tex(texture(&texfmt[k],8,8,1,quad)))SKIP("texture");
 rs(D3DRENDERSTATE_ALPHABLENDENABLE,TRUE);rs(D3DRENDERSTATE_SRCBLEND,D3DBLEND_SRCALPHA);rs(D3DRENDERSTATE_DESTBLEND,D3DBLEND_INVSRCALPHA);return quad_draw(1,~0UL,~0UL,~0UL,~0UL,.5f);}SKIP("format not offered");}
static int c_dxt1(void){return fmt_case(0,0,0,0,0,0,MAKEFOURCC('D','X','T','1'));}
static int c_dxt3(void){return fmt_case(0,0,0,0,0,0,MAKEFOURCC('D','X','T','3'));}
static int c_dxt5(void){return fmt_case(0,0,0,0,0,0,MAKEFOURCC('D','X','T','5'));}
static int c_pal8(void){int k;for(k=0;k<ntexfmt;k++)if(texfmt[k].dwFlags&DDPF_PALETTEINDEXED8){fprintf(logf,"   driver offers 8-bit palettized textures (not drawn by this tool)\n");return 0;}SKIP("format not offered");}
static int c_blend(DWORD s,DWORD d){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");rs(D3DRENDERSTATE_ALPHABLENDENABLE,TRUE);rs(D3DRENDERSTATE_SRCBLEND,s);rs(D3DRENDERSTATE_DESTBLEND,d);return quad_draw(1,~0UL,~0UL,~0UL,~0UL,.5f);}
static int c_bl_one_one(void){return c_blend(D3DBLEND_ONE,D3DBLEND_ONE);}
static int c_bl_dest_zero(void){return c_blend(D3DBLEND_DESTCOLOR,D3DBLEND_ZERO);}
static int c_bl_zero_src(void){return c_blend(D3DBLEND_ZERO,D3DBLEND_SRCCOLOR);}
static int c_bl_invdest(void){return c_blend(D3DBLEND_INVDESTCOLOR,D3DBLEND_INVSRCCOLOR);}
static int c_bl_destalpha(void){return c_blend(D3DBLEND_DESTALPHA,D3DBLEND_INVDESTALPHA);}
static int c_bl_satur(void){return c_blend(D3DBLEND_SRCALPHASAT,D3DBLEND_ONE);}
static int c_bl_both(void){return c_blend(D3DBLEND_BOTHSRCALPHA,D3DBLEND_ZERO);}
static int c_alphatest(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");rs(D3DRENDERSTATE_ALPHATESTENABLE,TRUE);rs(D3DRENDERSTATE_ALPHAREF,0x90);rs(D3DRENDERSTATE_ALPHAFUNC,D3DCMP_GREATEREQUAL);return quad_draw(1,~0UL,~0UL,~0UL,~0UL,.5f);}
static int c_colorkey(void){DDCOLORKEY k;IDirectDrawSurface7 *t=texture(ARGB(),8,8,1,keyed);
 if(!t)SKIP("texture");k.dwColorSpaceLowValue=k.dwColorSpaceHighValue=pack(ARGB(),0xFFFF00FFUL);
 if(!ok("SetColorKey(texture)",IDirectDrawSurface7_SetColorKey(t,DDCKEY_SRCBLT,&k))){IDirectDrawSurface7_Release(t);SKIP("color key");}
 use_tex(t);rs(D3DRENDERSTATE_COLORKEYENABLE,TRUE);return quad_draw(1,~0UL,~0UL,~0UL,~0UL,.5f);}
static int c_border(void){if(!(caps.dpcTriCaps.dwTextureAddressCaps&D3DPTADDRESSCAPS_BORDER))SKIP("no BORDER address caps");
 if(!use_tex(texture(ARGB(),8,8,1,quad_opaque)))SKIP("texture");ts(0,D3DTSS_ADDRESSU,D3DTADDRESS_BORDER);ts(0,D3DTSS_ADDRESSV,D3DTADDRESS_BORDER);ts(0,D3DTSS_BORDERCOLOR,0xFFFF8000UL);
 return quad_draw(2,~0UL,~0UL,~0UL,~0UL,.5f);}
static int c_aniso(void){if(!(caps.dpcTriCaps.dwTextureFilterCaps&D3DPTFILTERCAPS_MINFANISOTROPIC))SKIP("no anisotropic filter caps");
 ts(0,D3DTSS_MAXANISOTROPY,2);ts(0,D3DTSS_MINFILTER,D3DTFN_ANISOTROPIC);return c_perspective();}
static int c_flat_tex(void){if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");rs(D3DRENDERSTATE_SHADEMODE,D3DSHADE_FLAT);colorop(0,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE);
 return quad_draw(1,0xFF808080UL,0xFFFFFFFFUL,0xFF000000UL,0xFFFFFFFFUL,.5f);}
static int c_dither(void){rs(D3DRENDERSTATE_DITHERENABLE,TRUE);return c_mod_gouraud();}
/* depth / stencil: a depth surface of the requested format attached to the target for this case only */
static int with_z(const DDPIXELFORMAT *f) {
 DDSURFACEDESC2 d;
 int r=1;
 if(!f)return 0;
 IDirect3DDevice7_EndScene(dev); /* the target changes outside a scene */
 memset(&d,0,sizeof(d));d.dwSize=sizeof(d);d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;d.dwWidth=W;d.dwHeight=H;d.ddpfPixelFormat=*f;d.ddsCaps.dwCaps=DDSCAPS_ZBUFFER|DDSCAPS_VIDEOMEMORY;
 if(!ok("CreateSurface(depth)",IDirectDraw7_CreateSurface(dd,&d,&zcase,0))||!ok("AddAttachedSurface(depth)",IDirectDrawSurface7_AddAttachedSurface(rt,zcase))||
    !ok("SetRenderTarget(with depth)",IDirect3DDevice7_SetRenderTarget(dev,rt,0)))r=0;
 IDirect3DDevice7_BeginScene(dev);
 return r;
}
static const DDPIXELFORMAT *zformat(DWORD bits,DWORD stencil){int k;for(k=0;k<nzfmt;k++)if(zfmt[k].dwZBufferBitDepth==bits+stencil&&zfmt[k].dwStencilBitDepth==stencil)return &zfmt[k];return 0;}
static int depth_pair(void){
 rs(D3DRENDERSTATE_ZENABLE,TRUE);rs(D3DRENDERSTATE_ZWRITEENABLE,TRUE);rs(D3DRENDERSTATE_ZFUNC,D3DCMP_LESS);
 if(!ok("Clear(Z)",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_ZBUFFER,0,1,0)))return 0;
 colorop(0,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_DIFFUSE);
 return quad_draw(1,0xFFFF0000UL,0xFFFF0000UL,0xFFFF0000UL,0xFFFF0000UL,.25f)&&quad_draw(1.5f,0xFF0000FFUL,0xFF0000FFUL,0xFF0000FFUL,0xFF0000FFUL,.75f);}
static int c_z16(void){if(!with_z(zformat(16,0)))SKIP("no 16-bit Z format");return depth_pair();}
static int c_z24(void){const DDPIXELFORMAT *f=zformat(24,0);if(!f){int k;for(k=0;k<nzfmt;k++)if(zfmt[k].dwZBufferBitDepth==32&&zfmt[k].dwZBitMask==0xFFFFFF00UL&&!zfmt[k].dwStencilBitDepth)f=&zfmt[k];}
 if(!with_z(f))SKIP("no 24-bit Z format");return depth_pair();}
static int c_stencil(void){
 if(!with_z(zformat(24,8)))SKIP("no 24-bit Z + 8-bit stencil format");
 rs(D3DRENDERSTATE_ZENABLE,TRUE);rs(D3DRENDERSTATE_ZWRITEENABLE,TRUE);
 if(!ok("Clear(Z+stencil)",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0,1,0)))return 0;
 colorop(0,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_DIFFUSE);
 rs(D3DRENDERSTATE_STENCILENABLE,TRUE);rs(D3DRENDERSTATE_STENCILFUNC,D3DCMP_ALWAYS);rs(D3DRENDERSTATE_STENCILREF,1);rs(D3DRENDERSTATE_STENCILMASK,0xFF);rs(D3DRENDERSTATE_STENCILWRITEMASK,0xFF);
 rs(D3DRENDERSTATE_STENCILPASS,D3DSTENCILOP_REPLACE);rs(D3DRENDERSTATE_STENCILFAIL,D3DSTENCILOP_KEEP);rs(D3DRENDERSTATE_STENCILZFAIL,D3DSTENCILOP_KEEP);
 if(!quad_draw(1,0xFF00FF00UL,0xFF00FF00UL,0xFF00FF00UL,0xFF00FF00UL,.5f))return 0;
 rs(D3DRENDERSTATE_ZFUNC,D3DCMP_ALWAYS);rs(D3DRENDERSTATE_STENCILFUNC,D3DCMP_NOTEQUAL);rs(D3DRENDERSTATE_STENCILPASS,D3DSTENCILOP_INCR);
 {vtx v[6];corner(&v[0],8,8,.5f,1,0xFFFF00FFUL,0,0);corner(&v[1],248,8,.5f,1,0xFFFF00FFUL,1,0);corner(&v[2],8,184,.5f,1,0xFFFF00FFUL,0,1);v[3]=v[1];corner(&v[4],248,184,.5f,1,0xFFFF00FFUL,1,1);v[5]=v[2];return draw(v,6);}}
static int c_zbias(void){if(!with_z(zformat(16,0))&&!with_z(zformat(24,0)))SKIP("no Z format");rs(D3DRENDERSTATE_ZBIAS,8);return depth_pair();}
static int c_zfuncs(void){if(!with_z(zformat(16,0)))SKIP("no 16-bit Z format");rs(D3DRENDERSTATE_ZENABLE,TRUE);rs(D3DRENDERSTATE_ZWRITEENABLE,FALSE);rs(D3DRENDERSTATE_ZFUNC,D3DCMP_GREATER);
 if(!ok("Clear(Z 0.5)",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_ZBUFFER,0,.5f,0)))return 0;colorop(0,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_DIFFUSE);
 {vtx v[6];corner(&v[0],32,32,0,1,0xFFFFFF00UL,0,0);corner(&v[1],160,32,1,1,0xFFFFFF00UL,1,0);corner(&v[2],32,160,0,1,0xFFFFFF00UL,0,1);v[3]=v[1];corner(&v[4],160,160,1,1,0xFFFFFF00UL,1,1);v[5]=v[2];return draw(v,6);}}
static int c_clear_target(void){return ok("Clear(target)",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET,0xFF336699UL,1,0));}
static int c_clear_rect(void){D3DRECT r;r.x1=40;r.y1=30;r.x2=200;r.y2=150;return ok("Clear(target, rectangle)",IDirect3DDevice7_Clear(dev,1,&r,D3DCLEAR_TARGET,0xFFCC8844UL,1,0));}
static int c_clear_z(void){if(!with_z(zformat(16,0)))SKIP("no 16-bit Z format");return ok("Clear(target+Z)",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF224466UL,.5f,0));}
static int c_clear_zs(void){if(!with_z(zformat(24,8)))SKIP("no 24-bit Z + 8-bit stencil format");return ok("Clear(Z+stencil)",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0,.25f,0x5A));}
static int prim(D3DPRIMITIVETYPE t,int n){vtx v[8];int k;for(k=0;k<n;k++)corner(&v[k],(float)(20+k*28),(float)(30+(k&1)*90),.5f,1,0xFF000000UL|(0x3F<<(8*(k%3))),0,0);
 colorop(0,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_DIFFUSE);return ok("DrawPrimitive(lines/points)",IDirect3DDevice7_DrawPrimitive(dev,t,FVF,v,n,0));}
static int c_strip(void){colorop(0,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_DIFFUSE);{vtx v[8];int k;for(k=0;k<8;k++)corner(&v[k],(float)(20+(k/2)*60),(float)(30+(k&1)*120),.5f,1,0xFF000000UL|(0xC0<<(8*(k%3))),0,0);
 return ok("DrawPrimitive(strip)",IDirect3DDevice7_DrawPrimitive(dev,D3DPT_TRIANGLESTRIP,FVF,v,8,0));}}
static int c_fan(void){colorop(0,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_DIFFUSE);{vtx v[6];
 corner(&v[0],128,96,.5f,1,0xFFFFFFFFUL,0,0);corner(&v[1],200,96,.5f,1,0xFFFF0000UL,0,0);corner(&v[2],150,30,.5f,1,0xFF00FF00UL,0,0);corner(&v[3],80,40,.5f,1,0xFF0000FFUL,0,0);corner(&v[4],60,130,.5f,1,0xFFFFFF00UL,0,0);corner(&v[5],170,170,.5f,1,0xFF00FFFFUL,0,0);
 return ok("DrawPrimitive(fan)",IDirect3DDevice7_DrawPrimitive(dev,D3DPT_TRIANGLEFAN,FVF,v,6,0));}}
static int c_indexed(void){static WORD ix[6]={0,1,2,1,3,2};vtx v[4];if(!use_tex(texture(ARGB(),8,8,1,quad)))SKIP("texture");
 corner(&v[0],32,32,.5f,1,~0UL,0,0);corner(&v[1],160,32,.5f,1,~0UL,1,0);corner(&v[2],32,160,.5f,1,~0UL,0,1);corner(&v[3],160,160,.5f,1,~0UL,1,1);
 return ok("DrawIndexedPrimitive",IDirect3DDevice7_DrawIndexedPrimitive(dev,D3DPT_TRIANGLELIST,FVF,v,4,ix,6,0));}
static int c_lines(void){return prim(D3DPT_LINELIST,8);}
static int c_linestrip(void){return prim(D3DPT_LINESTRIP,8);}
static int c_points(void){return prim(D3DPT_POINTLIST,8);}
static int c_cull(void){rs(D3DRENDERSTATE_CULLMODE,D3DCULL_CCW);colorop(0,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_DIFFUSE);
 {vtx v[6];corner(&v[0],32,32,.5f,1,0xFFFF0000UL,0,0);corner(&v[1],160,32,.5f,1,0xFFFF0000UL,0,0);corner(&v[2],32,160,.5f,1,0xFFFF0000UL,0,0); /* clockwise: drawn */
  corner(&v[3],100,100,.5f,1,0xFF00FF00UL,0,0);corner(&v[4],100,180,.5f,1,0xFF00FF00UL,0,0);corner(&v[5],240,100,.5f,1,0xFF00FF00UL,0,0); /* counter-clockwise: culled */
  return draw(v,6);}}
static int c_wireframe(void){rs(D3DRENDERSTATE_FILLMODE,D3DFILL_WIREFRAME);{int r=c_diffuse_only();rs(D3DRENDERSTATE_FILLMODE,D3DFILL_SOLID);return r;}}
static int c_wrap_rs(void){if(!use_tex(texture(ARGB(),8,8,1,quad_opaque)))SKIP("texture");ts(0,D3DTSS_ADDRESSU,D3DTADDRESS_WRAP);ts(0,D3DTSS_ADDRESSV,D3DTADDRESS_WRAP);rs(D3DRENDERSTATE_WRAP0,D3DWRAP_U|D3DWRAP_V);
 {vtx v[6];corner(&v[0],32,32,.5f,1,~0UL,.75f,.75f);corner(&v[1],160,32,.5f,1,~0UL,.25f,.75f);corner(&v[2],32,160,.5f,1,~0UL,.75f,.25f);v[3]=v[1];corner(&v[4],160,160,.5f,1,~0UL,.25f,.25f);v[5]=v[2];
  {int r=draw(v,6);rs(D3DRENDERSTATE_WRAP0,0);return r;}}}
/* ---- v2.4: colour key with other programs, colour and alpha ops set independently, specular on every program, the remaining ops,
 * argument modifiers (COMPLEMENT, ALPHAREPLICATE) and SPECULAR as an argument. The 8x8 texture is the four quadrants (alpha FF, C0, 80, 40);
 * the diffuse colour carries alpha A0 and the specular colour 204060, so colour and alpha come out different for every op. Alpha is made
 * visible by SRCALPHA / INVSRCALPHA blending over the clear colour. ---- */
static DWORD kn(DWORD w,DWORD h,DWORD x,DWORD y,DWORD l){(void)h;(void)y;(void)l;return x<w/2?0xFF2182C6UL:0xFFC68221UL;} /* K (the key) left, its reverse N right */
static DWORD kn_mixed(DWORD w,DWORD h,DWORD x,DWORD y,DWORD l){return ((x^y)&1)?0xFF2182C6UL:quad(w,h,x,y,l);}
static int op_draw(DWORD cop,DWORD c1,DWORD c2,DWORD aop,DWORD a1,DWORD a2,int spec,int blend) {
 vtx v[6];int k;
 colorop(0,cop,c1,c2);alphaop(0,aop,a1,a2);rs(D3DRENDERSTATE_TEXTUREFACTOR,0x9050A0F0UL);
 if(spec)rs(D3DRENDERSTATE_SPECULARENABLE,TRUE);
 if(blend){rs(D3DRENDERSTATE_ALPHABLENDENABLE,TRUE);rs(D3DRENDERSTATE_SRCBLEND,D3DBLEND_SRCALPHA);rs(D3DRENDERSTATE_DESTBLEND,D3DBLEND_INVSRCALPHA);}
 corner(&v[0],32,32,.5f,1,0xA0C08040UL,0,0);corner(&v[1],160,32,.5f,1,0xA0C08040UL,1,0);corner(&v[2],32,160,.5f,1,0xA0C08040UL,0,1);v[3]=v[1];corner(&v[4],160,160,.5f,1,0xA0C08040UL,1,1);v[5]=v[2];
 for(k=0;k<6;k++)v[k].specular=0xFF204060UL;
 return draw(v,6);}
static int tq(void){return use_tex(texture(ARGB(),8,8,1,quad));}
static int set_key(IDirectDrawSurface7 *t,DWORD argb,const DDPIXELFORMAT *f){DDCOLORKEY k;k.dwColorSpaceLowValue=k.dwColorSpaceHighValue=(argb&0xFF000000UL)?pack(f,argb):(pack(f,argb|0xFF000000UL)&~f->dwRGBAlphaBitMask);
 return ok("SetColorKey(texture)",IDirectDrawSurface7_SetColorKey(t,DDCKEY_SRCBLT,&k));}
static int keyed_case(const DDPIXELFORMAT *f,texel_fn fn,DWORD key,DWORD cop,DWORD c1,DWORD c2,DWORD aop,DWORD a1,DWORD a2,int spec,int blend) {
 IDirectDrawSurface7 *t;if(!f)SKIP("format not offered");t=texture(f,8,8,1,fn);if(!t)SKIP("texture");
 if(!set_key(t,key,f)){IDirectDrawSurface7_Release(t);SKIP("color key");}
 use_tex(t);rs(D3DRENDERSTATE_COLORKEYENABLE,TRUE);return op_draw(cop,c1,c2,aop,a1,a2,spec,blend);}
#define T D3DTA_TEXTURE
#define D D3DTA_DIFFUSE
#define S D3DTA_SPECULAR
#define F D3DTA_TFACTOR
#define CUR D3DTA_CURRENT
#define CM D3DTA_COMPLEMENT
#define AR D3DTA_ALPHAREPLICATE
static const DDPIXELFORMAT *RGB565(void){return find_fmt(DDPF_RGB,16,0xF800,0x7E0,0x1F,0,0);}
static int c_ck_asym(void){return keyed_case(ARGB(),kn,0xFF2182C6UL,D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,T,D,0,0);}
static int c_ck_alpha0(void){return keyed_case(ARGB(),kn,0x002182C6UL,D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,T,D,0,0);}
static int c_ck_mod_both(void){return keyed_case(ARGB(),kn_mixed,0xFF2182C6UL,D3DTOP_MODULATE,T,D,D3DTOP_MODULATE,T,D,0,1);}
static int c_ck_mod(void){return keyed_case(ARGB(),kn_mixed,0xFF2182C6UL,D3DTOP_MODULATE,T,D,D3DTOP_SELECTARG1,T,D,0,0);}
static int c_ck_add(void){return keyed_case(ARGB(),kn_mixed,0xFF2182C6UL,D3DTOP_ADD,T,D,D3DTOP_SELECTARG1,T,D,0,0);}
static int c_ck_spec(void){return keyed_case(ARGB(),kn_mixed,0xFF2182C6UL,D3DTOP_MODULATE,T,D,D3DTOP_SELECTARG1,T,D,1,0);}
static int c_ck_565(void){return keyed_case(RGB565(),kn,0xFF2182C6UL,D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,T,D,0,0);}
static int c_ck_blend(void){return keyed_case(ARGB(),kn_mixed,0xFF2182C6UL,D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,T,D,0,1);}
static int c_ck_diffa(void){return keyed_case(ARGB(),kn_mixed,0xFF2182C6UL,D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,D,T,0,1);}
static int c_ck_mod2x(void){return keyed_case(ARGB(),kn_mixed,0xFF2182C6UL,D3DTOP_MODULATE2X,T,D,D3DTOP_SELECTARG1,T,D,0,0);}
#define OPC(name,cop,c1,c2,aop,a1,a2,spec) static int name(void){if(!tq())SKIP("texture");return op_draw(cop,c1,c2,aop,a1,a2,spec,1);}
OPC(c_u_mod_da,D3DTOP_MODULATE,T,D,D3DTOP_SELECTARG1,D,T,0)
OPC(c_u_add_mod,D3DTOP_ADD,T,D,D3DTOP_MODULATE,T,D,0)
OPC(c_u_mod2x_d,D3DTOP_MODULATE2X,T,D,D3DTOP_SELECTARG2,T,D,0)
OPC(c_u_bda_d,D3DTOP_BLENDDIFFUSEALPHA,T,D,D3DTOP_SELECTARG1,D,T,0)
OPC(c_u_tex_mod,D3DTOP_SELECTARG1,T,D,D3DTOP_MODULATE,T,D,0)
OPC(c_u_adds_mod,D3DTOP_ADDSIGNED,T,D,D3DTOP_MODULATE,T,D,0)
OPC(c_u_tf_tf,D3DTOP_MODULATE,T,F,D3DTOP_SELECTARG1,F,T,0)
OPC(c_u_bta_d,D3DTOP_BLENDTEXTUREALPHA,T,D,D3DTOP_SELECTARG1,D,T,0)
OPC(c_u_add_add,D3DTOP_ADD,T,D,D3DTOP_ADD,T,D,0)
OPC(c_u_mod4x_mod,D3DTOP_MODULATE4X,T,D,D3DTOP_MODULATE,T,D,0)
OPC(c_s_tex,D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,T,D,1)
OPC(c_s_add,D3DTOP_ADD,T,D,D3DTOP_SELECTARG1,T,D,1)
OPC(c_s_mod2x,D3DTOP_MODULATE2X,T,D,D3DTOP_SELECTARG1,T,D,1)
OPC(c_s_mod_both,D3DTOP_MODULATE,T,D,D3DTOP_MODULATE,T,D,1)
OPC(c_s_tf,D3DTOP_MODULATE,T,F,D3DTOP_SELECTARG1,T,D,1)
OPC(c_s_bda,D3DTOP_BLENDDIFFUSEALPHA,T,D,D3DTOP_SELECTARG1,T,D,1)
OPC(c_s_diff_texa,D3DTOP_SELECTARG1,D,T,D3DTOP_SELECTARG1,T,D,1)
OPC(c_s_tex_diffa,D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,D,T,1)
OPC(c_o_sub,D3DTOP_SUBTRACT,T,D,D3DTOP_SELECTARG1,T,D,0)
OPC(c_o_adds2x,D3DTOP_ADDSIGNED2X,T,D,D3DTOP_SELECTARG1,T,D,0)
OPC(c_o_addsmooth,D3DTOP_ADDSMOOTH,T,D,D3DTOP_SELECTARG1,T,D,0)
OPC(c_o_bfa,D3DTOP_BLENDFACTORALPHA,T,D,D3DTOP_SELECTARG1,T,D,0)
OPC(c_o_bca,D3DTOP_BLENDCURRENTALPHA,T,D,D3DTOP_SELECTARG1,T,D,0)
OPC(c_o_maac,D3DTOP_MODULATEALPHA_ADDCOLOR,T,D,D3DTOP_SELECTARG1,T,D,0)
OPC(c_o_mcaa,D3DTOP_MODULATECOLOR_ADDALPHA,T,D,D3DTOP_SELECTARG1,T,D,0)
OPC(c_o_dot3,D3DTOP_DOTPRODUCT3,T,D,D3DTOP_SELECTARG1,T,D,0)
OPC(c_m_mod_dcm,D3DTOP_MODULATE,T,D|CM,D3DTOP_SELECTARG1,T,D,0)
OPC(c_m_tcm,D3DTOP_SELECTARG1,T|CM,D,D3DTOP_SELECTARG1,T,D,0)
OPC(c_m_mod_tar,D3DTOP_MODULATE,T|AR,D,D3DTOP_SELECTARG1,T,D,0)
OPC(c_m_dar,D3DTOP_SELECTARG1,D|AR,T,D3DTOP_SELECTARG1,T,D,0)
OPC(c_m_alpha_tcm,D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,T|CM,D,0)
OPC(c_m_mod_tcm_dar,D3DTOP_MODULATE,T|CM,D|AR,D3DTOP_SELECTARG1,T,D,0)
OPC(c_m_add_dcm,D3DTOP_ADD,T,D|CM,D3DTOP_SELECTARG1,T,D,0)
OPC(c_m_mod_fcm,D3DTOP_MODULATE,T,F|CM,D3DTOP_SELECTARG1,T,D,0)
OPC(c_m_mod_curar,D3DTOP_MODULATE,T,CUR|AR,D3DTOP_SELECTARG1,T,D,0)
OPC(c_m_alpha_mod_dcm,D3DTOP_SELECTARG1,T,D,D3DTOP_MODULATE,T,D|CM,0)
OPC(c_a_mod_spec,D3DTOP_MODULATE,T,S,D3DTOP_SELECTARG1,T,D,0)
OPC(c_a_add_spec,D3DTOP_ADD,D,S,D3DTOP_SELECTARG1,T,D,0)
/* ---- v2.5: two-stage combinations, large textures, fog with other programs and EXP2 / W fog, A8 ---- */
static int two(DWORD c0,DWORD c01,DWORD c02,DWORD a0,DWORD a01,DWORD a02,DWORD c1,DWORD c11,DWORD c12,DWORD a1,DWORD a11,DWORD a12,int idx1,int spec) {
 if(caps.wMaxSimultaneousTextures<2)SKIP("one texture only");
 if(!tq())SKIP("texture 0");
 tex1=texture(ARGB(),4,4,1,quad_opaque);if(!tex1||!ok("SetTexture(1)",IDirect3DDevice7_SetTexture(dev,1,tex1)))SKIP("texture 1");
 ts(1,D3DTSS_TEXCOORDINDEX,idx1);ts(1,D3DTSS_MAGFILTER,D3DTFG_POINT);ts(1,D3DTSS_MINFILTER,D3DTFN_POINT);
 colorop(1,c1,c11,c12);alphaop(1,a1,a11,a12);
 return op_draw(c0,c01,c02,a0,a01,a02,spec,1);}
static int c_2_lightmap(void){return two(D3DTOP_MODULATE,T,D,D3DTOP_SELECTARG1,T,D, D3DTOP_MODULATE,T,CUR,D3DTOP_SELECTARG2,T,CUR,1,0);}
static int c_2_add(void){return two(D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,T,D, D3DTOP_ADD,T,CUR,D3DTOP_SELECTARG2,T,CUR,1,0);}
static int c_2_mod2x(void){return two(D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,T,D, D3DTOP_MODULATE2X,T,CUR,D3DTOP_SELECTARG2,T,CUR,1,0);}
static int c_2_bca(void){return two(D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,D,T, D3DTOP_BLENDCURRENTALPHA,T,CUR,D3DTOP_SELECTARG2,T,CUR,1,0);}
static int c_2_alpha_mod(void){return two(D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,T,D, D3DTOP_SELECTARG2,T,CUR,D3DTOP_MODULATE,T,CUR,1,0);}
static int c_2_same_uv(void){return two(D3DTOP_MODULATE,T,D,D3DTOP_SELECTARG1,T,D, D3DTOP_MODULATE,T,CUR,D3DTOP_SELECTARG2,T,CUR,0,0);}
static int c_2_spec(void){return two(D3DTOP_MODULATE,T,D,D3DTOP_SELECTARG1,T,D, D3DTOP_MODULATE,T,CUR,D3DTOP_SELECTARG2,T,CUR,1,1);}
static int c_2_cur_diff(void){if(!tq())SKIP("texture");colorop(1,D3DTOP_MODULATE,CUR,D);alphaop(1,D3DTOP_SELECTARG1,CUR,D);return op_draw(D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,T,D,0,1);} /* stage 1 without a texture */
static int c_2_sub(void){return two(D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,T,D, D3DTOP_SUBTRACT,CUR,T,D3DTOP_SELECTARG2,T,CUR,1,0);}
static int c_2_ckey(void){if(caps.wMaxSimultaneousTextures<2)SKIP("one texture only");{IDirectDrawSurface7 *t=texture(ARGB(),8,8,1,kn_mixed);if(!t)SKIP("texture");
 if(!set_key(t,0xFF2182C6UL,ARGB())){IDirectDrawSurface7_Release(t);SKIP("color key");}use_tex(t);rs(D3DRENDERSTATE_COLORKEYENABLE,TRUE);}
 tex1=texture(ARGB(),4,4,1,quad_opaque);if(!tex1||!ok("SetTexture(1)",IDirect3DDevice7_SetTexture(dev,1,tex1)))SKIP("texture 1");ts(1,D3DTSS_TEXCOORDINDEX,1);
 colorop(1,D3DTOP_MODULATE,T,CUR);alphaop(1,D3DTOP_SELECTARG2,T,CUR);return op_draw(D3DTOP_SELECTARG1,T,D,D3DTOP_SELECTARG1,T,D,0,1);}
static int big_tex(DWORD n){if(caps.dwMaxTextureWidth<n||caps.dwMaxTextureHeight<n)SKIP("texture size above the driver's maximum");if(!use_tex(texture(ARGB(),n,n,1,quad)))SKIP("texture");return quad_draw(1,~0UL,~0UL,~0UL,~0UL,.5f);}
static int c_tex1024(void){return big_tex(1024);}
static int c_tex2048(void){return big_tex(2048);}
static int c_tex_2048x8(void){if(caps.dwMaxTextureWidth<2048)SKIP("texture size above the driver's maximum");if(!use_tex(texture(ARGB(),2048,8,1,quad)))SKIP("texture");return quad_draw(1,~0UL,~0UL,~0UL,~0UL,.5f);}
static int fog_on(DWORD mode){rs(D3DRENDERSTATE_FOGENABLE,TRUE);rs(D3DRENDERSTATE_FOGCOLOR,0x00406080UL);rs(D3DRENDERSTATE_FOGTABLEMODE,mode);
 rs(D3DRENDERSTATE_FOGTABLESTART,f2dw(.2f));rs(D3DRENDERSTATE_FOGTABLEEND,f2dw(.8f));rs(D3DRENDERSTATE_FOGTABLEDENSITY,f2dw(2.f));return 1;}
static int fog_ramp(void){vtx v[6];corner(&v[0],32,32,0,1,0xA0C08040UL,0,0);corner(&v[1],160,32,.999f,1,0xA0C08040UL,1,0);corner(&v[2],32,160,0,1,0xA0C08040UL,0,1);v[3]=v[1];
 corner(&v[4],160,160,.999f,1,0xA0C08040UL,1,1);v[5]=v[2];v[0].specular=v[2].specular=v[5].specular=0xFF204060UL;v[1].specular=v[3].specular=v[4].specular=0x00204060UL;return draw(v,6);}
static int c_fog_exp2(void){if(!tq())SKIP("texture");fog_on(D3DFOG_EXP2);colorop(0,D3DTOP_SELECTARG1,T,D);return fog_ramp();}
static int c_fog_mod(void){if(!tq())SKIP("texture");fog_on(D3DFOG_LINEAR);colorop(0,D3DTOP_MODULATE,T,D);return fog_ramp();}
static int c_fog_vtx_untex(void){fog_on(D3DFOG_NONE);colorop(0,D3DTOP_SELECTARG2,T,D);alphaop(0,D3DTOP_SELECTARG2,T,D);return fog_ramp();}
static int c_fog_table_untex(void){fog_on(D3DFOG_LINEAR);colorop(0,D3DTOP_SELECTARG2,T,D);alphaop(0,D3DTOP_SELECTARG2,T,D);return fog_ramp();}
static int c_fog_w(void){vtx v[6];if(!(caps.dpcTriCaps.dwRasterCaps&D3DPRASTERCAPS_WFOG))SKIP("no D3DPRASTERCAPS_WFOG");if(!tq())SKIP("texture");fog_on(D3DFOG_LINEAR);
 rs(D3DRENDERSTATE_FOGTABLESTART,f2dw(1.f));rs(D3DRENDERSTATE_FOGTABLEEND,f2dw(4.f)); /* W = 1/rhw, 1 at the left, 4 at the right */
 corner(&v[0],32,32,.1f,1,~0UL,0,0);corner(&v[1],160,32,.9f,.25f,~0UL,1,0);corner(&v[2],32,160,.1f,1,~0UL,0,1);v[3]=v[1];corner(&v[4],160,160,.9f,.25f,~0UL,1,1);v[5]=v[2];return draw(v,6);}
static int c_fog_spec(void){if(!tq())SKIP("texture");fog_on(D3DFOG_LINEAR);rs(D3DRENDERSTATE_SPECULARENABLE,TRUE);colorop(0,D3DTOP_MODULATE,T,D);return fog_ramp();}
static int c_fog_ckey(void){IDirectDrawSurface7 *t=texture(ARGB(),8,8,1,kn_mixed);if(!t)SKIP("texture");if(!set_key(t,0xFF2182C6UL,ARGB())){IDirectDrawSurface7_Release(t);SKIP("color key");}
 use_tex(t);rs(D3DRENDERSTATE_COLORKEYENABLE,TRUE);fog_on(D3DFOG_LINEAR);colorop(0,D3DTOP_SELECTARG1,T,D);return fog_ramp();}
static int c_a8(void){int k;for(k=0;k<ntexfmt;k++)if((texfmt[k].dwFlags&DDPF_ALPHAPIXELS)&&!(texfmt[k].dwFlags&(DDPF_RGB|DDPF_LUMINANCE))&&texfmt[k].dwRGBBitCount==8){if(!use_tex(texture(&texfmt[k],8,8,1,quad)))SKIP("texture");
 return op_draw(D3DTOP_SELECTARG2,T,D,D3DTOP_SELECTARG1,T,D,0,1);}SKIP("format not offered");}
static int c_l8_mod(void){int k;for(k=0;k<ntexfmt;k++)if((texfmt[k].dwFlags&DDPF_LUMINANCE)&&texfmt[k].dwRGBBitCount==8&&!(texfmt[k].dwFlags&DDPF_ALPHAPIXELS)){if(!use_tex(texture(&texfmt[k],8,8,1,quad)))SKIP("texture");
 return op_draw(D3DTOP_MODULATE,T,D,D3DTOP_SELECTARG1,D,T,0,0);}SKIP("format not offered");}
/* ---- v2.6: the remaining ops, DOTPRODUCT3 forms, bump mapping, three stages, cube map, projected texture ---- */
OPC(c_o_btapm,D3DTOP_BLENDTEXTUREALPHAPM,T,D,D3DTOP_SELECTARG1,T,D,0)
OPC(c_o_miaac,D3DTOP_MODULATEINVALPHA_ADDCOLOR,T,D,D3DTOP_SELECTARG1,T,D,0)
OPC(c_o_micaa,D3DTOP_MODULATEINVCOLOR_ADDALPHA,T,D,D3DTOP_SELECTARG1,T,D,0)
OPC(c_o_dot3_tf,D3DTOP_DOTPRODUCT3,T,F,D3DTOP_SELECTARG1,T,D,0)
OPC(c_o_dot3_both,D3DTOP_DOTPRODUCT3,T,D,D3DTOP_DOTPRODUCT3,T,D,0)
OPC(c_o_dot3_da,D3DTOP_DOTPRODUCT3,T,D,D3DTOP_SELECTARG1,D,T,0)
static int c_premod(void){return two(D3DTOP_PREMODULATE,T,D,D3DTOP_SELECTARG1,T,D, D3DTOP_SELECTARG2,T,CUR,D3DTOP_SELECTARG2,T,CUR,1,0);}
static int c_2_dot3(void){return two(D3DTOP_DOTPRODUCT3,T,D,D3DTOP_SELECTARG1,T,D, D3DTOP_MODULATE,T,CUR,D3DTOP_SELECTARG2,T,CUR,1,0);}
static const DDPIXELFORMAT *bumpfmt(DWORD flags,DWORD bits){int k;for(k=0;k<ntexfmt;k++)if((texfmt[k].dwFlags&(DDPF_BUMPDUDV|DDPF_BUMPLUMINANCE))==flags&&texfmt[k].dwRGBBitCount==bits)return &texfmt[k];return 0;}
static DWORD bumps(DWORD w,DWORD h,DWORD x,DWORD y,DWORD l){(void)h;(void)y;(void)l;return x<w/2?0xFF20E080UL:0xFFE02080UL;} /* du dv (signed) l */
static int bump_case(DWORD op,DWORD flags,DWORD bits){const DDPIXELFORMAT *f=bumpfmt(flags,bits);vtx v[6];
 if(caps.wMaxSimultaneousTextures<2)SKIP("one texture only");if(!f)SKIP("bump format not offered");
 if(!use_tex(texture(f,8,8,1,bumps)))SKIP("bump texture");
 tex1=texture(ARGB(),8,8,1,quad_opaque);if(!tex1||!ok("SetTexture(1)",IDirect3DDevice7_SetTexture(dev,1,tex1)))SKIP("texture 1");
 ts(0,D3DTSS_COLOROP,op);ts(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);ts(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);ts(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);ts(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
 ts(0,D3DTSS_BUMPENVMAT00,f2dw(.5f));ts(0,D3DTSS_BUMPENVMAT01,0);ts(0,D3DTSS_BUMPENVMAT10,0);ts(0,D3DTSS_BUMPENVMAT11,f2dw(.5f));
 ts(0,D3DTSS_BUMPENVLSCALE,f2dw(1.f));ts(0,D3DTSS_BUMPENVLOFFSET,f2dw(.25f));
 colorop(1,D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_CURRENT);alphaop(1,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_CURRENT);ts(1,D3DTSS_TEXCOORDINDEX,1);
 ts(1,D3DTSS_ADDRESSU,D3DTADDRESS_WRAP);ts(1,D3DTSS_ADDRESSV,D3DTADDRESS_WRAP);
 corner(&v[0],32,32,.5f,1,~0UL,0,0);corner(&v[1],160,32,.5f,1,~0UL,1,0);corner(&v[2],32,160,.5f,1,~0UL,0,1);v[3]=v[1];corner(&v[4],160,160,.5f,1,~0UL,1,1);v[5]=v[2];
 return draw(v,6);}
static int c_bump(void){return bump_case(D3DTOP_BUMPENVMAP,DDPF_BUMPDUDV,16);}
static int c_bump_l(void){return bump_case(D3DTOP_BUMPENVMAPLUMINANCE,DDPF_BUMPDUDV|DDPF_BUMPLUMINANCE,16);}
static int c_bump_l32(void){return bump_case(D3DTOP_BUMPENVMAPLUMINANCE,DDPF_BUMPDUDV|DDPF_BUMPLUMINANCE,32);}
static IDirectDrawSurface7 *tex2;
static int c_3_stages(void){
 if(caps.wMaxSimultaneousTextures<3)SKIP("fewer than three textures");
 if(!tq())SKIP("texture 0");
 tex1=texture(ARGB(),4,4,1,quad_opaque);if(!tex1||!ok("SetTexture(1)",IDirect3DDevice7_SetTexture(dev,1,tex1)))SKIP("texture 1");
 tex2=texture(ARGB(),2,2,1,grey);if(!tex2||!ok("SetTexture(2)",IDirect3DDevice7_SetTexture(dev,2,tex2)))SKIP("texture 2");
 colorop(1,D3DTOP_MODULATE,T,CUR);alphaop(1,D3DTOP_SELECTARG2,T,CUR);ts(1,D3DTSS_TEXCOORDINDEX,1);
 colorop(2,D3DTOP_ADD,T,CUR);alphaop(2,D3DTOP_SELECTARG2,T,CUR);ts(2,D3DTSS_TEXCOORDINDEX,0);
 {int r=op_draw(D3DTOP_MODULATE,T,D,D3DTOP_SELECTARG1,T,D,0,1);
  ts(2,D3DTSS_COLOROP,D3DTOP_DISABLE);ts(2,D3DTSS_ALPHAOP,D3DTOP_DISABLE);IDirect3DDevice7_SetTexture(dev,2,0);IDirectDrawSurface7_Release(tex2);tex2=0;return r;}}
typedef struct vtx3 {float x,y,z,rhw;DWORD diffuse,specular;float u,v,w;} vtx3;
#define FVF3 (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEX1|D3DFVF_TEXCOORDSIZE3(0))
static void c3(vtx3 *p,float x,float y,float u,float v,float w){memset(p,0,sizeof(*p));p->x=x;p->y=y;p->z=.5f;p->rhw=1;p->diffuse=~0UL;p->specular=0xFF000000UL;p->u=u;p->v=v;p->w=w;}
static int draw3(vtx3 *v){return ok("DrawPrimitive(3D coordinates)",IDirect3DDevice7_DrawPrimitive(dev,D3DPT_TRIANGLELIST,FVF3,v,6,0));}
static int c_cube(void){
 static const DWORD face[6]={DDSCAPS2_CUBEMAP_POSITIVEX,DDSCAPS2_CUBEMAP_NEGATIVEX,DDSCAPS2_CUBEMAP_POSITIVEY,DDSCAPS2_CUBEMAP_NEGATIVEY,DDSCAPS2_CUBEMAP_POSITIVEZ,DDSCAPS2_CUBEMAP_NEGATIVEZ};
 static const DWORD col[6]={0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL,0xFFFFFF00UL,0xFF00FFFFUL,0xFFFF00FFUL};
 DDSURFACEDESC2 d;IDirectDrawSurface7 *t=0;int k;vtx3 v[6];
 if(!(caps.dpcTriCaps.dwTextureCaps&D3DPTEXTURECAPS_CUBEMAP))SKIP("no cube map caps");
 memset(&d,0,sizeof(d));d.dwSize=sizeof(d);d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;d.dwWidth=d.dwHeight=8;d.ddpfPixelFormat=*ARGB();
 d.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_COMPLEX|DDSCAPS_VIDEOMEMORY;d.ddsCaps.dwCaps2=DDSCAPS2_CUBEMAP|DDSCAPS2_CUBEMAP_ALLFACES;
 if(!ok("CreateSurface(cube map)",IDirectDraw7_CreateSurface(dd,&d,&t,0)))SKIP("cube map");
 for(k=0;k<6;k++){DDSCAPS2 c;IDirectDrawSurface7 *f=0;DDSURFACEDESC2 l;DWORD x,y;memset(&c,0,sizeof(c));c.dwCaps=DDSCAPS_TEXTURE;c.dwCaps2=DDSCAPS2_CUBEMAP|face[k];
  if(k==0){f=t;IDirectDrawSurface7_AddRef(f);}else if(FAILED(IDirectDrawSurface7_GetAttachedSurface(t,&c,&f)))continue;
  memset(&l,0,sizeof(l));l.dwSize=sizeof(l);
  if(SUCCEEDED(IDirectDrawSurface7_Lock(f,0,&l,DDLOCK_WAIT|DDLOCK_WRITEONLY,0))){for(y=0;y<8;y++)for(x=0;x<8;x++)((DWORD*)((BYTE*)l.lpSurface+y*l.lPitch))[x]=col[k];IDirectDrawSurface7_Unlock(f,0);}
  IDirectDrawSurface7_Release(f);}
 use_tex(t);ts(0,D3DTSS_TEXCOORDINDEX,0);
 /* direction +x on the left half, -z on the right half */
 c3(&v[0],32,32,1,.2f,.2f);c3(&v[1],160,32,-.2f,.2f,-1);c3(&v[2],32,160,1,-.2f,.2f);v[3]=v[1];c3(&v[4],160,160,-.2f,-.2f,-1);v[5]=v[2];
 return draw3(v);}
static int c_projected(void){vtx3 v[6];
 if(!tq())SKIP("texture");ts(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_COUNT3|D3DTTFF_PROJECTED);
 /* (u, v, q): q 1 at the left, 2 at the right: the right half samples u/2, v/2 */
 c3(&v[0],32,32,0,0,1);c3(&v[1],160,32,2,0,2);c3(&v[2],32,160,0,1,1);v[3]=v[1];c3(&v[4],160,160,2,2,2);v[5]=v[2];
 {int r=draw3(v);ts(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);return r;}}
static int c_yuy2(void){const DDPIXELFORMAT *f=find_fmt(0,0,0,0,0,0,MAKEFOURCC('Y','U','Y','2'));if(!f)SKIP("YUY2 not offered");if(!use_tex(texture(f,8,8,1,quad)))SKIP("texture");return quad_draw(1,~0UL,~0UL,~0UL,~0UL,.5f);}
#undef T
#undef D
#undef S
#undef F
#undef CUR
#undef CM
#undef AR
typedef struct tcase {const char *name;int (*fn)(void);} tcase;
static const tcase cases[]={
 {"reference: texture SELECTARG1 (the recipe already proven on Win98)",c_replace},
 {"MODULATE texture x flat grey diffuse",c_mod_flat},
 {"MODULATE colour and alpha, per-vertex colours",c_mod_gouraud},
 {"MODULATE texture x CURRENT (stage 0 = diffuse)",c_mod_current},
 {"colour from texture, alpha from diffuse, SRCALPHA blend",c_tex_rgb_diff_a},
 {"colour from diffuse, alpha from texture, SRCALPHA blend",c_diff_rgb_tex_a},
 {"ADD texture + diffuse",c_add},
 {"MODULATE2X",c_mod2x},
 {"MODULATE4X",c_mod4x},
 {"ADDSIGNED",c_addsigned},
 {"BLENDTEXTUREALPHA",c_blend_tex_a},
 {"BLENDDIFFUSEALPHA",c_blend_diff_a},
 {"MODULATE texture x TFACTOR",c_tfactor},
 {"diffuse only (SELECTARG2 DIFFUSE), Gouraud",c_diffuse_only},
 {"specular add, no texture",c_spec_diffuse},
 {"specular add on MODULATE texture",c_spec_tex},
 {"vertex fog (fog factor in specular alpha)",c_fog_vertex},
 {"table fog LINEAR",c_fog_lin},
 {"table fog EXP",c_fog_exp},
 {"perspective-correct textured floor (rhw 1 -> 0.25)",c_perspective},
 {"same floor with TEXTUREPERSPECTIVE off",c_affine},
 {"mipmaps 32x32, 6 levels, MIPFILTER POINT, minified 4x",c_mip_point},
 {"mipmaps, MIPFILTER LINEAR, minified 3x",c_mip_linear},
 {"mipmaps with LOD bias +1",c_mip_bias},
 {"two stages: texture 0, then MODULATE texture 1",c_two_stage},
 {"two stages: MODULATE diffuse, then ADD texture 1",c_two_stage_add},
 {"texture format R5G6B5",c_565},
 {"texture format A1R5G5B5",c_1555},
 {"texture format A4R4G4B4",c_4444},
 {"texture format X1R5G5B5",c_555},
 {"texture format X8R8G8B8",c_x8},
 {"texture format L8",c_l8},
 {"texture format A8L8, SRCALPHA blend",c_a8l8},
 {"texture format DXT1",c_dxt1},
 {"texture format DXT3",c_dxt3},
 {"texture format DXT5",c_dxt5},
 {"texture format 8-bit palettized (logged only)",c_pal8},
 {"blend ONE / ONE",c_bl_one_one},
 {"blend DESTCOLOR / ZERO",c_bl_dest_zero},
 {"blend ZERO / SRCCOLOR",c_bl_zero_src},
 {"blend INVDESTCOLOR / INVSRCCOLOR",c_bl_invdest},
 {"blend DESTALPHA / INVDESTALPHA",c_bl_destalpha},
 {"blend SRCALPHASAT / ONE",c_bl_satur},
 {"blend BOTHSRCALPHA",c_bl_both},
 {"alpha test GREATEREQUAL 0x90",c_alphatest},
 {"texture colour key (COLORKEYENABLE)",c_colorkey},
 {"address BORDER with border colour",c_border},
 {"anisotropic filter on the floor",c_aniso},
 {"flat shading with MODULATE",c_flat_tex},
 {"dithering on, MODULATE Gouraud",c_dither},
 {"16-bit Z: near red, far blue, ZFUNC LESS",c_z16},
 {"24-bit Z",c_z24},
 {"24-bit Z + 8-bit stencil: REPLACE then NOTEQUAL/INCR",c_stencil},
 {"ZBIAS 8",c_zbias},
 {"ZFUNC GREATER, Z write off, Z cleared to 0.5",c_zfuncs},
 {"Clear target",c_clear_target},
 {"Clear target, one rectangle",c_clear_rect},
 {"Clear target + 16-bit Z",c_clear_z},
 {"Clear 24-bit Z + stencil",c_clear_zs},
 {"triangle strip",c_strip},
 {"triangle fan",c_fan},
 {"indexed triangle list",c_indexed},
 {"line list",c_lines},
 {"line strip",c_linestrip},
 {"point list",c_points},
 {"cull CCW",c_cull},
 {"wireframe fill",c_wireframe},
 {"WRAP0 render state (cylindrical wrap)",c_wrap_rs},
 /* v2.4 */
 {"colour key, key 2182C6 (left K = key, right N = 2182C6 reversed), SELECTARG1",c_ck_asym},
 {"colour key, key given with alpha 00 (002182C6)",c_ck_alpha0},
 {"colour key + MODULATE colour and alpha, SRCALPHA blend",c_ck_mod_both},
 {"colour key + MODULATE colour, alpha = texture",c_ck_mod},
 {"colour key + ADD",c_ck_add},
 {"colour key + specular on MODULATE",c_ck_spec},
 {"colour key, R5G6B5 texture",c_ck_565},
 {"colour key + SRCALPHA blend (SELECTARG1)",c_ck_blend},
 {"colour key, colour = texture, alpha = diffuse, SRCALPHA blend",c_ck_diffa},
 {"colour key + MODULATE2X",c_ck_mod2x},
 {"U: colour MODULATE, alpha = diffuse",c_u_mod_da},
 {"U: colour ADD, alpha MODULATE",c_u_add_mod},
 {"U: colour MODULATE2X, alpha = diffuse (SELECTARG2)",c_u_mod2x_d},
 {"U: colour BLENDDIFFUSEALPHA, alpha = diffuse",c_u_bda_d},
 {"U: colour = texture, alpha MODULATE",c_u_tex_mod},
 {"U: colour ADDSIGNED, alpha MODULATE",c_u_adds_mod},
 {"U: colour MODULATE TFACTOR, alpha = TFACTOR",c_u_tf_tf},
 {"U: colour BLENDTEXTUREALPHA, alpha = diffuse",c_u_bta_d},
 {"U: colour ADD, alpha ADD",c_u_add_add},
 {"U: colour MODULATE4X, alpha MODULATE",c_u_mod4x_mod},
 {"specular + texture SELECTARG1",c_s_tex},
 {"specular + ADD",c_s_add},
 {"specular + MODULATE2X",c_s_mod2x},
 {"specular + MODULATE colour and alpha",c_s_mod_both},
 {"specular + MODULATE TFACTOR",c_s_tf},
 {"specular + BLENDDIFFUSEALPHA",c_s_bda},
 {"specular + colour = diffuse, alpha = texture",c_s_diff_texa},
 {"specular + colour = texture, alpha = diffuse",c_s_tex_diffa},
 {"op SUBTRACT",c_o_sub},
 {"op ADDSIGNED2X",c_o_adds2x},
 {"op ADDSMOOTH",c_o_addsmooth},
 {"op BLENDFACTORALPHA (TFACTOR alpha 90)",c_o_bfa},
 {"op BLENDCURRENTALPHA",c_o_bca},
 {"op MODULATEALPHA_ADDCOLOR",c_o_maac},
 {"op MODULATECOLOR_ADDALPHA",c_o_mcaa},
 {"op DOTPRODUCT3",c_o_dot3},
 {"modifier: MODULATE texture x (diffuse | COMPLEMENT)",c_m_mod_dcm},
 {"modifier: SELECTARG1 texture | COMPLEMENT",c_m_tcm},
 {"modifier: MODULATE (texture | ALPHAREPLICATE) x diffuse",c_m_mod_tar},
 {"modifier: SELECTARG1 diffuse | ALPHAREPLICATE",c_m_dar},
 {"modifier: alpha = texture | COMPLEMENT",c_m_alpha_tcm},
 {"modifier: MODULATE (texture | COMPLEMENT) x (diffuse | ALPHAREPLICATE)",c_m_mod_tcm_dar},
 {"modifier: ADD texture + (diffuse | COMPLEMENT)",c_m_add_dcm},
 {"modifier: MODULATE texture x (TFACTOR | COMPLEMENT)",c_m_mod_fcm},
 {"modifier: MODULATE texture x (CURRENT | ALPHAREPLICATE)",c_m_mod_curar},
 {"modifier: alpha MODULATE texture x (diffuse | COMPLEMENT)",c_m_alpha_mod_dcm},
 {"argument SPECULAR: MODULATE texture x specular",c_a_mod_spec},
 {"argument SPECULAR: ADD diffuse + specular",c_a_add_spec},
 /* v2.5 */
 {"two stages: MODULATE diffuse, then MODULATE texture 1 (light map)",c_2_lightmap},
 {"two stages: texture 0, then ADD texture 1",c_2_add},
 {"two stages: texture 0, then MODULATE2X texture 1",c_2_mod2x},
 {"two stages: alpha = diffuse, then BLENDCURRENTALPHA texture 1",c_2_bca},
 {"two stages: stage 1 alpha MODULATE texture 1, colour CURRENT",c_2_alpha_mod},
 {"two stages: light map with both stages on texture coordinates 0",c_2_same_uv},
 {"two stages: light map + specular",c_2_spec},
 {"two stages: stage 1 MODULATE CURRENT x diffuse, no texture 1",c_2_cur_diff},
 {"two stages: CURRENT SUBTRACT texture 1",c_2_sub},
 {"two stages: colour key on texture 0, MODULATE texture 1",c_2_ckey},
 {"texture 1024x1024",c_tex1024},
 {"texture 2048x2048",c_tex2048},
 {"texture 2048x8",c_tex_2048x8},
 {"table fog EXP2",c_fog_exp2},
 {"table fog LINEAR on MODULATE",c_fog_mod},
 {"vertex fog, untextured diffuse",c_fog_vtx_untex},
 {"table fog LINEAR, untextured diffuse",c_fog_table_untex},
 {"W fog LINEAR (start 1, end 4, rhw 1 -> 0.25)",c_fog_w},
 {"table fog LINEAR + specular on MODULATE",c_fog_spec},
 {"table fog LINEAR + colour key",c_fog_ckey},
 {"texture format A8 (alpha only), SRCALPHA blend",c_a8},
 {"texture format L8, MODULATE",c_l8_mod},
 /* v2.6 */
 {"op BLENDTEXTUREALPHAPM",c_o_btapm},
 {"op MODULATEINVALPHA_ADDCOLOR",c_o_miaac},
 {"op MODULATEINVCOLOR_ADDALPHA",c_o_micaa},
 {"op DOTPRODUCT3 texture . TFACTOR",c_o_dot3_tf},
 {"op DOTPRODUCT3 in colour and alpha",c_o_dot3_both},
 {"op DOTPRODUCT3, alpha = diffuse",c_o_dot3_da},
 {"two stages: PREMODULATE, then CURRENT",c_premod},
 {"two stages: DOTPRODUCT3, then MODULATE texture 1",c_2_dot3},
 {"bump map BUMPENVMAP (V8U8), stage 1 texture",c_bump},
 {"bump map BUMPENVMAPLUMINANCE (L6V5U5)",c_bump_l},
 {"bump map BUMPENVMAPLUMINANCE (X8L8V8U8)",c_bump_l32},
 {"three stages: MODULATE diffuse, MODULATE texture 1, ADD texture 2",c_3_stages},
 {"cube map (+X left, -Z right)",c_cube},
 {"projected texture (COUNT3 | PROJECTED, q 1 -> 2)",c_projected},
 {"texture format YUY2",c_yuy2},
};
#define NCASES ((int)(sizeof(cases)/sizeof(cases[0])))

/* ---- v2.0: built-in command-ring capture ----
 * The XP driver feeds the 3D engine from a command buffer in GPU address space; MMIO 0x43C (with 0x440/0x45C) is its moving
 * cursor. GPU addresses go through the chip's GART table, whose physical base the sequencer holds in SR6A/SR6B/SR6C
 * (base = SR6C bit0 <<28 | SR6B <<20 | SR6A <<12, SR6C bit 7 = enabled); entry n (4 bytes) holds the physical page of GPU page n.
 * Around every case the tool reads MMIO 0x400..0x5FC, and after the draw dumps the GPU range the cursor crossed (with margins), both
 * translated through the GART table and, as a cross-check, read as a frame-buffer offset through BAR0. Read only: nothing on the chip
 * is written except the VGA/PCI index registers (restored). Needs Administrator, inpout32.dll (as XPDUMP) and \Device\PhysicalMemory. */
typedef struct {USHORT Length,MaximumLength;PWSTR Buffer;} vx_ustr;
typedef struct {ULONG Length;HANDLE RootDirectory;vx_ustr *ObjectName;ULONG Attributes;PVOID SecurityDescriptor,SecurityQualityOfService;} vx_oattr;
typedef LONG (__stdcall *NtOpenSection_t)(HANDLE *,ULONG,vx_oattr *);
typedef LONG (__stdcall *NtMapViewOfSection_t)(HANDLE,HANDLE,PVOID *,ULONG,ULONG,LARGE_INTEGER *,ULONG *,ULONG,ULONG,ULONG);
typedef LONG (__stdcall *NtUnmapViewOfSection_t)(HANDLE,PVOID);
typedef short (__stdcall *Inp32_t)(short);typedef void (__stdcall *Out32_t)(short,short);
typedef ULONG (__stdcall *DlRdL_t)(ULONG);typedef void (__stdcall *DlWrL_t)(ULONG,ULONG);
typedef PBYTE (__stdcall *MapPhys_t)(PBYTE,DWORD,HANDLE *);typedef BOOL (__stdcall *UnmapPhys_t)(HANDLE,PBYTE);
static Inp32_t pInp;static Out32_t pOut;static DlRdL_t pRdL;static DlWrL_t pWrL;static MapPhys_t pMap;static UnmapPhys_t pUnmap;
static NtMapViewOfSection_t pNtMap;static NtUnmapViewOfSection_t pNtUnmap;static HANDLE physmem;
static int capture_on;static DWORD bar0,bar1,bar0_size,gart_base,gart_on,sr[8];static const BYTE sr_idx[8]={0x68,0x6A,0x6B,0x6C,0x6D,0x6E,0x39,0x1A};
/* read len bytes of physical memory; 0 on failure */
static LONG last_nt;static DWORD last_map;
/* v2.2: XP's \Device\PhysicalMemory covers RAM only (MMIO and the BAR0 frame buffer fail with C00000F4), so reads go through
 * CXPHYS.SYS, a read-only kernel reader installed for the run (demand-start service CXPHYS) and removed afterwards. */
#define CX_IOCTL_READ 0x00226004UL /* CTL_CODE(FILE_DEVICE_UNKNOWN,0x801,METHOD_BUFFERED,FILE_READ_ACCESS) */
static HANDLE cxdev=INVALID_HANDLE_VALUE;static SC_HANDLE scm,svc;static DWORD cx_err;
static void cx_start(void) {
 char path[MAX_PATH],*e;
 GetModuleFileNameA(0,path,sizeof(path));e=strrchr(path,'\\');if(e)strcpy(e+1,"CXPHYS.SYS");
 if(GetFileAttributesA(path)==0xFFFFFFFFUL){cx_err=1;return;}
 scm=OpenSCManagerA(0,0,SC_MANAGER_ALL_ACCESS);if(!scm){cx_err=GetLastError();return;}
 svc=CreateServiceA(scm,"CXPHYS","CXPHYS",SERVICE_ALL_ACCESS,SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,path,0,0,0,0,0);
 if(!svc)svc=OpenServiceA(scm,"CXPHYS",SERVICE_ALL_ACCESS);
 if(!svc){cx_err=GetLastError();return;}
 if(!StartServiceA(svc,0,0)&&GetLastError()!=ERROR_SERVICE_ALREADY_RUNNING){cx_err=GetLastError();return;}
 cxdev=CreateFileA("\\\\.\\CXPHYS",GENERIC_READ,0,0,OPEN_EXISTING,0,0);if(cxdev==INVALID_HANDLE_VALUE)cx_err=GetLastError();
}
static void cx_stop(void) {
 SERVICE_STATUS st;
 if(cxdev!=INVALID_HANDLE_VALUE){CloseHandle(cxdev);cxdev=INVALID_HANDLE_VALUE;}
 if(svc){ControlService(svc,SERVICE_CONTROL_STOP,&st);DeleteService(svc);CloseServiceHandle(svc);svc=0;}
 if(scm){CloseServiceHandle(scm);scm=0;}
}
static int cx_read_phys(DWORD phys,void *out,DWORD len,int cached) {
 DWORD q[4],got;BYTE *o=out;
 while(len){DWORD n=len>65536UL?65536UL:len;q[0]=phys;q[1]=0;q[2]=(n+3)&~3UL;q[3]=(DWORD)cached;
  if(q[2]!=n){DWORD tmp[1];(void)tmp;}
  if(!DeviceIoControl(cxdev,CX_IOCTL_READ,q,16,o,q[2],&got,0)||got!=q[2]){cx_err=GetLastError();return 0;}
  o+=n;phys+=n;len-=n;}
 return 1;
}
static int phys_read_p(DWORD phys,void *out,DWORD len,ULONG prot) {
 if(cxdev!=INVALID_HANDLE_VALUE&&!(len&3)&&!(phys&3))return cx_read_phys(phys,out,len,(prot&PAGE_NOCACHE)?0:1);
 if(physmem&&pNtMap) {
  ULONG tries[2];int t;tries[0]=prot;tries[1]=PAGE_READONLY;
  for(t=0;t<2;t++){
   PVOID base=0;LARGE_INTEGER off;ULONG size;DWORD a=phys&~0xFFFFUL;LONG st;
   off.QuadPart=a;size=(phys-a)+len;size=(size+4095)&~4095UL;
   st=pNtMap(physmem,(HANDLE)-1,&base,0,size,&off,&size,1,0,tries[t]);last_nt=st;
   if(st>=0&&base){memcpy(out,(BYTE*)base+(phys-(DWORD)off.QuadPart),len);pNtUnmap((HANDLE)-1,base);return 1;}
   if(tries[1]==prot)break;
  }
 }
 if(pMap&&pUnmap) {
  HANDLE h=0;DWORD a=phys&~4095UL,size=((phys-a)+len+4095)&~4095UL;PBYTE p=pMap((PBYTE)a,size,&h);last_map=(DWORD)p;
  if(p){memcpy(out,p+(phys-a),len);pUnmap(h,p);return 1;}
 }
 return 0;
}
static int phys_read(DWORD phys,void *out,DWORD len){return phys_read_p(phys,out,len,PAGE_READONLY);}
/* v2.1: the display adapter's memory ranges from the PnP resource list (AllocConfig), when PCI configuration reads give nothing */
static int registry_bars(void) {
 HKEY k,d,i;char name[256],dev[256];DWORD a,b,n;int found=0;
 if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Enum\\PCI",0,KEY_READ,&k))return 0;
 for(a=0;!found&&!RegEnumKeyA(k,a,dev,sizeof(dev));a++) {
  if(_strnicmp(dev,"VEN_1106&DEV_5122",17))continue;
  if(RegOpenKeyExA(k,dev,0,KEY_READ,&d))continue;
  for(b=0;!found&&!RegEnumKeyA(d,b,name,sizeof(name));b++) {
   static BYTE buf[4096];DWORD type,size=sizeof(buf);char sub[300];
   sprintf(sub,"%s\\Control",name);
   if(RegOpenKeyExA(d,sub,0,KEY_READ,&i))continue;
   if(!RegQueryValueExA(i,"AllocConfig",0,&type,buf,&size)&&size>=8) {
    /* CM_RESOURCE_LIST: Count; CM_FULL_RESOURCE_DESCRIPTOR: InterfaceType, BusNumber, then Version(2) Revision(2) Count(4), 16-byte descriptors */
    DWORD count=*(DWORD*)(buf+16),k2,mem=0;
    fprintf(logf,"registry %s\\%s AllocConfig %lu bytes, %lu descriptors:",dev,name,(unsigned long)size,(unsigned long)count);
    for(k2=0;k2<count&&20+k2*16+16<=size;k2++){BYTE *e=buf+20+k2*16;DWORD start=*(DWORD*)(e+4),len=*(DWORD*)(e+12);
     fprintf(logf," type %u %08lX+%08lX",e[0],(unsigned long)start,(unsigned long)len);
     if(e[0]==3){if(len>=0x01000000UL&&len>0x01000000UL&&!bar0)bar0=start;else if(len==0x01000000UL&&!bar1)bar1=start;else if(!mem&&!bar1&&len<0x01000000UL)mem=start;}}
    fprintf(logf,"; ");found=1;
   }
   RegCloseKey(i);
  }
  RegCloseKey(d);
 }
 RegCloseKey(k);(void)n;return found;
}
static int mmio_regs(DWORD *out) {return bar1&&phys_read_p(bar1+0x400,out,0x200,PAGE_READONLY|PAGE_NOCACHE);}
static DWORD pci_rd(DWORD bus,DWORD dev,DWORD fn,DWORD reg) {pWrL(0xCF8,0x80000000UL|(bus<<16)|(dev<<11)|(fn<<8)|(reg&0xFC));return pRdL(0xCFC);}
static BYTE sr_get(BYTE i) {BYTE o=(BYTE)pInp(0x3C4),v;pOut(0x3C4,i);v=(BYTE)pInp(0x3C5);pOut(0x3C4,o);return v;}
static void capture_init(void) {
 HMODULE nt=GetModuleHandleA("ntdll.dll"),io=LoadLibraryA("inpout32.dll");DWORD bus,dev,k;
 fprintf(logf,"capture: ");
 cx_start();
 if(cxdev!=INVALID_HANDLE_VALUE)fprintf(logf,"CXPHYS.SYS running; ");else fprintf(logf,"CXPHYS.SYS not available (%s, error %lu); ",cx_err==1?"file missing next to the EXE":"service",(unsigned long)cx_err);
 if(io){pInp=(Inp32_t)GetProcAddress(io,"Inp32");pOut=(Out32_t)GetProcAddress(io,"Out32");pRdL=(DlRdL_t)GetProcAddress(io,"DlPortReadPortUlong");
  pWrL=(DlWrL_t)GetProcAddress(io,"DlPortWritePortUlong");pMap=(MapPhys_t)GetProcAddress(io,"MapPhysToLin");pUnmap=(UnmapPhys_t)GetProcAddress(io,"UnmapPhysicalMemory");}
 fprintf(logf,"inpout32 %s (ports %s, PCI %s, MapPhysToLin %s); ",io?"loaded":"MISSING",pInp&&pOut?"yes":"no",pRdL&&pWrL?"yes":"no",pMap&&pUnmap?"yes":"no");
 if(nt) {
  NtOpenSection_t open=(NtOpenSection_t)GetProcAddress(nt,"NtOpenSection");
  pNtMap=(NtMapViewOfSection_t)GetProcAddress(nt,"NtMapViewOfSection");pNtUnmap=(NtUnmapViewOfSection_t)GetProcAddress(nt,"NtUnmapViewOfSection");
  if(open&&pNtMap&&pNtUnmap){static WCHAR name[]=L"\\Device\\PhysicalMemory";vx_ustr u;vx_oattr a;LONG st;
   u.Buffer=name;u.Length=(USHORT)(wcslen(name)*2);u.MaximumLength=u.Length+2;memset(&a,0,sizeof(a));a.Length=sizeof(a);a.ObjectName=&u;a.Attributes=0x40;
   st=open(&physmem,4 /* SECTION_MAP_READ */,&a);if(st<0){physmem=0;fprintf(logf,"\\Device\\PhysicalMemory refused (%08lX); ",(unsigned long)st);}
   else fprintf(logf,"\\Device\\PhysicalMemory open; ");}
 }
 if(pRdL&&pWrL) fprintf(logf,"PCI 0:0.0 id %08lX, 1:0.0 id %08lX; ",(unsigned long)pci_rd(0,0,0,0),(unsigned long)pci_rd(1,0,0,0));
 if(pRdL&&pWrL) for(bus=0;bus<4&&!bar1;bus++)for(dev=0;dev<32;dev++){DWORD id=pci_rd(bus,dev,0,0);
  if(id==0x51221106UL||(id&0xFFFF)==0x1106&&((pci_rd(bus,dev,0,8)>>24)==3)){bar0=pci_rd(bus,dev,0,0x10)&~15UL;bar1=pci_rd(bus,dev,0,0x14)&~15UL;
   fprintf(logf,"VGA %08lX at %lu:%lu BAR0 %08lX BAR1 %08lX; ",(unsigned long)id,(unsigned long)bus,(unsigned long)dev,(unsigned long)bar0,(unsigned long)bar1);break;}}
 if(!bar1)registry_bars();
 if(!bar1){bar1=0xF0000000UL;fprintf(logf,"BAR1 assumed F0000000; ");}
 else fprintf(logf,"BAR0 %08lX BAR1 %08lX; ",(unsigned long)bar0,(unsigned long)bar1);
 if(pInp&&pOut) {
  BYTE unlock=sr_get(0x10);pOut(0x3C4,0x10);pOut(0x3C5,1); /* VIA extended sequencer unlock, restored below */
  for(k=0;k<8;k++)sr[k]=sr_get(sr_idx[k]);
  pOut(0x3C4,0x10);pOut(0x3C5,unlock);
  gart_base=((DWORD)(sr[3]&1)<<28)|((DWORD)sr[2]<<20)|((DWORD)sr[1]<<12);gart_on=(sr[3]&0x80)!=0;
  bar0_size=(DWORD)sr[6]<<22;
  fprintf(logf,"SR68 %02X SR6A %02X SR6B %02X SR6C %02X SR6D %02X SR6E %02X SR39 %02X SR1A %02X: GART table %08lX %s, video memory %lu MB; ",
   sr[0],sr[1],sr[2],sr[3],sr[4],sr[5],sr[6],sr[7],(unsigned long)gart_base,gart_on?"enabled":"disabled",(unsigned long)(bar0_size>>20));
 }
 {DWORD r[128];capture_on=mmio_regs(r);
  fprintf(logf,"MMIO read %s (last section status %08lX, MapPhysToLin %08lX)",capture_on?"ok":"FAILED (no capture)",(unsigned long)last_nt,(unsigned long)last_map);
  /* v2.3: SR6A..6C hold a frame-buffer offset (the Win98 lab programs them with its own table in video memory): the table is read through BAR0 */
  if(capture_on&&bar0&&gart_base){DWORD pte=0,w[8],k2,valid=0,t[1024],a=r[0xF];
   for(k2=0;k2<64;k2++)if(phys_read_p(bar0+gart_base+k2*4096UL,t,4096,PAGE_READONLY|PAGE_NOCACHE)){DWORD j;for(j=0;j<1024;j++)if(t[j]!=0xFFFFFFFFUL&&t[j])valid++;}
   fprintf(logf,"; GART table at physical %08lX: %lu used entries in its first 64K",(unsigned long)(bar0+gart_base),(unsigned long)valid);
   if(phys_read_p(bar0+gart_base+(a>>12)*4,&pte,4,PAGE_READONLY|PAGE_NOCACHE)){fprintf(logf,"; entry for 43C page %08lX = %08lX",(unsigned long)(a&~4095UL),(unsigned long)pte);
    if(phys_read(((pte&0x000FFFFFUL)<<12)|(a&0xFC0),w,32))fprintf(logf,"; that page at 43C-offset reads %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX",(unsigned long)w[0],(unsigned long)w[1],(unsigned long)w[2],(unsigned long)w[3],(unsigned long)w[4],(unsigned long)w[5],(unsigned long)w[6],(unsigned long)w[7]);}}
  if(!capture_on){DWORD probe[4];static const DWORD tryb[]={0xF0000000UL,0xF1000000UL,0xF2000000UL,0xE0000000UL,0xFC000000UL,0xFD000000UL,0xFE000000UL,0xDC000000UL,0xDD000000UL};int q;
   for(q=0;q<9;q++)if(phys_read_p(tryb[q]+0x400,probe,16,PAGE_READONLY|PAGE_NOCACHE))fprintf(logf,"; %08lX+400 reads %08lX %08lX %08lX %08lX",(unsigned long)tryb[q],(unsigned long)probe[0],(unsigned long)probe[1],(unsigned long)probe[2],(unsigned long)probe[3]);
   if(phys_read(0x0FF80000UL,probe,16))fprintf(logf,"; RAM 0FF80000 reads %08lX %08lX %08lX %08lX",(unsigned long)probe[0],(unsigned long)probe[1],(unsigned long)probe[2],(unsigned long)probe[3]);
   else fprintf(logf,"; RAM 0FF80000 not readable (status %08lX)",(unsigned long)last_nt);}if(capture_on)fprintf(logf,": 400=%08lX 43C=%08lX 440=%08lX 444=%08lX 45C=%08lX 494=%08lX",
   (unsigned long)r[0],(unsigned long)r[0xF],(unsigned long)r[0x10],(unsigned long)r[0x11],(unsigned long)r[0x17],(unsigned long)r[0x25]);}
 fprintf(logf,"\n");fflush(logf);
}
/* a GPU range through the GART: per page the table entry, then the page bytes */
static DWORD fe_count(const DWORD *w,DWORD n){DWORD k,c=0;for(k=0;k<n;k++)if((w[k]>>24)==0xFE||(w[k]&0xFFFF0000UL)==0x20030000UL||(w[k]&0xFFFF0000UL)==0x10030000UL)c++;return c;}
static void dump_range(FILE *f,DWORD gpu0,DWORD gpu1,DWORD *fe_gart,DWORD *fe_fb) {
 DWORD a,pte,buf[1024],hdr[4];
 for(a=gpu0&~0xFFFUL;a<gpu1;a+=0x1000) {
  pte=0;hdr[0]=0x54524147UL; /* 'GART' */hdr[1]=a;hdr[2]=0;hdr[3]=0;
  if(gart_base&&bar0&&phys_read_p(bar0+gart_base+(a>>12)*4,&pte,4,PAGE_READONLY|PAGE_NOCACHE)){DWORD phys=(pte&0x000FFFFFUL)<<12;hdr[2]=pte;
   if(phys_read(phys,buf,4096)){hdr[3]=4096;fwrite(hdr,4,4,f);fwrite(buf,4,1024,f);*fe_gart+=fe_count(buf,1024);}
   else fwrite(hdr,4,4,f);
   /* second reading of the entry: a physical address with flag bits in the low 12 */
   hdr[0]=0x32545247UL; /* 'GRT2' */hdr[3]=0;
   if((pte&0xFFFFF000UL)!=phys&&phys_read(pte&0xFFFFF000UL,buf,4096)){hdr[3]=4096;fwrite(hdr,4,4,f);fwrite(buf,4,1024,f);}
   else fwrite(hdr,4,4,f);}
  else fwrite(hdr,4,4,f);
  hdr[0]=0x20204246UL; /* 'FB  ' */hdr[2]=bar0?bar0+a:0;hdr[3]=0;
  if(bar0&&(!bar0_size||a<bar0_size)&&phys_read_p(bar0+a,buf,4096,PAGE_READONLY|PAGE_NOCACHE)){hdr[3]=4096;fwrite(hdr,4,4,f);fwrite(buf,4,1024,f);*fe_fb+=fe_count(buf,1024);}
  else fwrite(hdr,4,4,f);
 }
}
static DWORD pre_regs[128],end_regs[128];
static void capture_pre(void){if(capture_on){Sleep(30);mmio_regs(pre_regs);}}
static void capture_end(int n,const char *tag) {
 char name[32];FILE *f;DWORD hdr[8],lo,hi,fg=0,ff=0,k;
 if(!capture_on)return;
 mmio_regs(end_regs);
 lo=pre_regs[0xF];hi=end_regs[0xF];
 for(k=0x10;k<=0x17;k+=7){if(pre_regs[k]&&pre_regs[k]<lo)lo=pre_regs[k];if(end_regs[k]>hi)hi=end_regs[k];}
 if(hi<lo||hi-lo>0x100000UL){DWORD t=lo;lo=hi<lo?hi:lo;hi=t>hi?t:hi;if(hi-lo>0x100000UL)hi=lo+0x100000UL;} /* wrapped or far apart: bounded */
 lo=lo>0x4000UL?lo-0x4000UL:0;hi+=0x8000UL;
 sprintf(name,"%s.RNG",tag);f=fopen(name,"wb");if(!f)return;
 hdr[0]=0x31525843UL; /* 'CXR1' */hdr[1]=(DWORD)n;hdr[2]=lo;hdr[3]=hi;hdr[4]=gart_base|gart_on;hdr[5]=bar0;hdr[6]=bar1;hdr[7]=bar0_size;
 fwrite(hdr,4,8,f);fwrite(sr,4,8,f);fwrite(pre_regs,4,128,f);fwrite(end_regs,4,128,f);
 dump_range(f,lo,hi,&fg,&ff);fclose(f);
 fprintf(logf,"   capture %s: 43C %08lX -> %08lX (440 %08lX -> %08lX, 45C %08lX -> %08lX, 494 %08lX -> %08lX), dumped %08lX..%08lX; command-like words: GART %lu, FB %lu\n",name,
  (unsigned long)pre_regs[0xF],(unsigned long)end_regs[0xF],(unsigned long)pre_regs[0x10],(unsigned long)end_regs[0x10],(unsigned long)pre_regs[0x17],(unsigned long)end_regs[0x17],
  (unsigned long)pre_regs[0x25],(unsigned long)end_regs[0x25],(unsigned long)lo,(unsigned long)hi,(unsigned long)fg,(unsigned long)ff);
 fflush(logf);
}
/* ---- capture hook ---- */
static void snapshot(int n,const char *phase) {
 char cmd[1024],tag[8];const char *s;char *d=cmd;
 sprintf(tag,"T%02d",n);
 if(capture_on){if(phase[0]=='P')capture_pre();else capture_end(n,tag);}
 if(pause_mode){char line[16];printf("Case %s (%s): take the %s snapshot now, then press Enter.\n",tag,cases[n].name,phase);fflush(stdout);fgets(line,sizeof(line),stdin);return;}
 if(!hook)return;
 for(s=hook;*s&&d<cmd+sizeof(cmd)-16;) {
  if(s[0]=='{'&&s[1]=='T'&&s[2]=='}'){strcpy(d,tag);d+=strlen(tag);s+=3;}
  else if(s[0]=='{'&&s[1]=='P'&&s[2]=='}'){strcpy(d,phase);d+=strlen(phase);s+=3;}
  else *d++=*s++;
 }
 *d=0;fprintf(logf,"   hook: %s\n",cmd);fflush(logf);
 {int rc=system(cmd);if(rc)fprintf(logf,"   hook returned %d\n",rc);}
}
/* flush: lock the target (the driver submits and waits for the queued work), optionally saving it */
static int flush(int n) {
 DDSURFACEDESC2 s;DWORD x,y,*row;FILE *f;char name[32];
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
 if(!ok("Lock(target)",IDirectDrawSurface7_Lock(rt,0,&s,DDLOCK_WAIT|DDLOCK_READONLY,0)))return 0;
 if(n>=0){
  sprintf(name,"CLXP3D%02d.BIN",n);f=fopen(name,"wb");row=(DWORD*)malloc(W*4);
  if(f&&row){for(y=0;y<H;y++){for(x=0;x<W;x++)row[x]=unpack_rt(&s,(BYTE*)s.lpSurface+y*s.lPitch+x*(s.ddpfPixelFormat.dwRGBBitCount/8));fwrite(row,4,W,f);}}
  fprintf(logf,"   pixels (40,40)=%06lX (64,64)=%06lX (128,64)=%06lX (64,128)=%06lX (128,128)=%06lX (150,90)=%06lX (220,170)=%06lX\n",
   (unsigned long)unpack_rt(&s,(BYTE*)s.lpSurface+40*s.lPitch+40*(s.ddpfPixelFormat.dwRGBBitCount/8)),
   (unsigned long)unpack_rt(&s,(BYTE*)s.lpSurface+64*s.lPitch+64*(s.ddpfPixelFormat.dwRGBBitCount/8)),
   (unsigned long)unpack_rt(&s,(BYTE*)s.lpSurface+64*s.lPitch+128*(s.ddpfPixelFormat.dwRGBBitCount/8)),
   (unsigned long)unpack_rt(&s,(BYTE*)s.lpSurface+128*s.lPitch+64*(s.ddpfPixelFormat.dwRGBBitCount/8)),
   (unsigned long)unpack_rt(&s,(BYTE*)s.lpSurface+128*s.lPitch+128*(s.ddpfPixelFormat.dwRGBBitCount/8)),
   (unsigned long)unpack_rt(&s,(BYTE*)s.lpSurface+90*s.lPitch+150*(s.ddpfPixelFormat.dwRGBBitCount/8)),
   (unsigned long)unpack_rt(&s,(BYTE*)s.lpSurface+170*s.lPitch+220*(s.ddpfPixelFormat.dwRGBBitCount/8)));
  if(row)free(row);if(f)fclose(f);
 }
 IDirectDrawSurface7_Unlock(rt,0);return 1;
}
static void run_case(int n) {
 int r;DWORD t0;
 fprintf(logf,"== T%02d %s\n",n,cases[n].name);fflush(logf);
 baseline();tex0=tex1=zcase=0;
 IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET,0xFF202020UL,1,0);
 flush(-1);                       /* baseline state and the background clear are submitted before the PRE snapshot */
 snapshot(n,"PRE");
 t0=GetTickCount();
 if(!ok("BeginScene",IDirect3DDevice7_BeginScene(dev)))return;
 r=cases[n].fn();
 ok("EndScene",IDirect3DDevice7_EndScene(dev));
 flush(n);
 fprintf(logf,"   %s, %lu ms\n",r?"drawn":"not drawn",(unsigned long)(GetTickCount()-t0));fflush(logf);
 snapshot(n,"END");
 IDirect3DDevice7_SetTexture(dev,0,0);IDirect3DDevice7_SetTexture(dev,1,0);
 if(tex0)IDirectDrawSurface7_Release(tex0);if(tex1)IDirectDrawSurface7_Release(tex1);
 if(zcase){IDirectDrawSurface7_DeleteAttachedSurface(rt,0,zcase);IDirectDrawSurface7_Release(zcase);zcase=0;
  IDirect3DDevice7_SetRenderTarget(dev,rt,0);}
}
/* ---- setup ---- */
static HRESULT CALLBACK on_texfmt(LPDDPIXELFORMAT f,LPVOID u){(void)u;
 fprintf(logf,"   texture format: flags %08lX fourcc %08lX bits %lu masks %08lX %08lX %08lX alpha %08lX\n",(unsigned long)f->dwFlags,(unsigned long)f->dwFourCC,(unsigned long)f->dwRGBBitCount,
  (unsigned long)f->dwRBitMask,(unsigned long)f->dwGBitMask,(unsigned long)f->dwBBitMask,(unsigned long)f->dwRGBAlphaBitMask);
 if(ntexfmt<32)texfmt[ntexfmt++]=*f;return D3DENUMRET_OK;}
static HRESULT CALLBACK on_zfmt(LPDDPIXELFORMAT f,LPVOID u){(void)u;
 fprintf(logf,"   depth format: flags %08lX depth %lu stencil %lu masks Z %08lX S %08lX\n",(unsigned long)f->dwFlags,(unsigned long)f->dwZBufferBitDepth,(unsigned long)f->dwStencilBitDepth,
  (unsigned long)f->dwZBitMask,(unsigned long)f->dwStencilBitMask);
 if(nzfmt<16)zfmt[nzfmt++]=*f;return D3DENUMRET_OK;}
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProc(w,m,a,b);}
static int setup(void) {
 HRESULT (WINAPI *create)(GUID *,void **,REFIID,IUnknown *);HMODULE lib;WNDCLASSA wc;DDSURFACEDESC2 s;D3DVIEWPORT7 vp;DDCAPS hw,hel;
 lib=LoadLibraryA("DDRAW.DLL");if(!lib){fprintf(logf,"DDRAW.DLL missing\n");return 0;}
 create=(HRESULT (WINAPI *)(GUID *,void **,REFIID,IUnknown *))GetProcAddress(lib,"DirectDrawCreateEx");if(!create){fprintf(logf,"DirectDrawCreateEx missing\n");return 0;}
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="CLXP3D";RegisterClassA(&wc);
 window=CreateWindowA("CLXP3D","CLXP3D",WS_OVERLAPPEDWINDOW,40,40,320,260,0,0,wc.hInstance,0);ShowWindow(window,SW_SHOW);
 if(!ok("DirectDrawCreateEx",create(0,(void**)&dd,&iid_dd7,0))||!ok("SetCooperativeLevel",IDirectDraw7_SetCooperativeLevel(dd,window,DDSCL_NORMAL))||
    !ok("QueryInterface(IDirect3D7)",IDirectDraw7_QueryInterface(dd,&IID_IDirect3D7,(void**)&d3d)))return 0;
 memset(&hw,0,sizeof(hw));hw.dwSize=sizeof(hw);memset(&hel,0,sizeof(hel));hel.dwSize=sizeof(hel);
 if(SUCCEEDED(IDirectDraw7_GetCaps(dd,&hw,&hel)))fprintf(logf,"DirectDraw caps %08lX caps2 %08lX surface caps %08lX Z depths %08lX video memory %lu free %lu\n",(unsigned long)hw.dwCaps,(unsigned long)hw.dwCaps2,
  (unsigned long)hw.ddsCaps.dwCaps,(unsigned long)hw.dwZBufferBitDepths,(unsigned long)hw.dwVidMemTotal,(unsigned long)hw.dwVidMemFree);
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;s.dwWidth=W;s.dwHeight=H;s.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_3DDEVICE|DDSCAPS_VIDEOMEMORY;
 if(!ok("CreateSurface(target)",IDirectDraw7_CreateSurface(dd,&s,&rt,0)))return 0;
 fprintf(logf,"depth formats:\n");IDirect3D7_EnumZBufferFormats(d3d,&IID_IDirect3DHALDevice,on_zfmt,0);
 if(!ok("CreateDevice(HAL)",IDirect3D7_CreateDevice(d3d,&IID_IDirect3DHALDevice,rt,&dev)))return 0;
 memset(&caps,0,sizeof(caps));IDirect3DDevice7_GetCaps(dev,&caps);
 fprintf(logf,"device caps %08lX; triangles: misc %08lX raster %08lX Z %08lX src blend %08lX dest blend %08lX alpha cmp %08lX shade %08lX texture %08lX filter %08lX blend %08lX address %08lX\n",
  (unsigned long)caps.dwDevCaps,(unsigned long)caps.dpcTriCaps.dwMiscCaps,(unsigned long)caps.dpcTriCaps.dwRasterCaps,(unsigned long)caps.dpcTriCaps.dwZCmpCaps,(unsigned long)caps.dpcTriCaps.dwSrcBlendCaps,
  (unsigned long)caps.dpcTriCaps.dwDestBlendCaps,(unsigned long)caps.dpcTriCaps.dwAlphaCmpCaps,(unsigned long)caps.dpcTriCaps.dwShadeCaps,(unsigned long)caps.dpcTriCaps.dwTextureCaps,
  (unsigned long)caps.dpcTriCaps.dwTextureFilterCaps,(unsigned long)caps.dpcTriCaps.dwTextureBlendCaps,(unsigned long)caps.dpcTriCaps.dwTextureAddressCaps);
 fprintf(logf,"   texture %lux%lu..%lux%lu repeat %lu aspect %lu anisotropy %lu, stencil %08lX FVF %08lX texture ops %08lX stages %u simultaneous %u, guard band %g %g %g %g\n",
  (unsigned long)caps.dwMinTextureWidth,(unsigned long)caps.dwMinTextureHeight,(unsigned long)caps.dwMaxTextureWidth,(unsigned long)caps.dwMaxTextureHeight,(unsigned long)caps.dwMaxTextureRepeat,
  (unsigned long)caps.dwMaxTextureAspectRatio,(unsigned long)caps.dwMaxAnisotropy,(unsigned long)caps.dwStencilCaps,(unsigned long)caps.dwFVFCaps,(unsigned long)caps.dwTextureOpCaps,
  (unsigned)caps.wMaxTextureBlendStages,(unsigned)caps.wMaxSimultaneousTextures,caps.dvGuardBandLeft,caps.dvGuardBandTop,caps.dvGuardBandRight,caps.dvGuardBandBottom);
 fprintf(logf,"   lines: raster %08lX shade %08lX\n",(unsigned long)caps.dpcLineCaps.dwRasterCaps,(unsigned long)caps.dpcLineCaps.dwShadeCaps);
 fprintf(logf,"texture formats:\n");IDirect3DDevice7_EnumTextureFormats(dev,on_texfmt,0);
 memset(&vp,0,sizeof(vp));vp.dwWidth=W;vp.dwHeight=H;vp.dvMaxZ=1;ok("SetViewport",IDirect3DDevice7_SetViewport(dev,&vp));
 fflush(logf);return 1;
}
int main(int argc,char **argv) {
 int only=-1,k,nocap=0,from=0;
 if(argc>=2&&!strcmp(argv[1],"list")){for(k=0;k<NCASES;k++)printf("%02d %s\n",k,cases[k].name);return 0;}
 if(argc>=2&&!strcmp(argv[1],"probe")){logf=fopen("CLXP3D_PROBE.TXT","w");if(!logf)return 1;capture_init();cx_stop();fclose(logf);printf("see CLXP3D_PROBE.TXT\n");return 0;}
 if(argc>=3&&!strcmp(argv[1],"from")){from=atoi(argv[2]);argc--;argv++;} /* v2.4: CLXP3D from 68 = the v2.4 cases only */
 else if(argc>=2&&strcmp(argv[1],"all"))only=atoi(argv[1]);
 if(argc>=3){if(!strcmp(argv[2],"pause"))pause_mode=1;else if(!strcmp(argv[2],"nocapture"))nocap=1;else hook=argv[2];}
 if(only>=NCASES){printf("no case %d (CLXP3D list)\n",only);return 1;}
 logf=fopen(only>=0?"CLXP3D_1.TXT":"CLXP3D.TXT","w");if(!logf)return 1;
 fprintf(logf,"CLXP3D v2.6: XP Direct3D 7 capture cases, target %dx%d; %s\n",W,H,pause_mode?"pausing for PRE/END snapshots":hook?"running the capture hook":"built-in ring capture");
 if(!nocap)capture_init();
 if(!setup()){cx_stop();fclose(logf);printf("setup failed, see the TXT\n");return 1;}
 if(capture_on){DWORD i;pre_regs[0]=0;capture_pre();Sleep(200);capture_end(999,"IDLE");(void)i;} /* 200 ms with nothing drawn: the driver's own background traffic */
 for(k=from;k<NCASES;k++)if(only<0||k==only)run_case(k);
 if(capture_on&&gart_base){ /* the whole GART table (256 KB covers 256 MB of GPU space) for offline decoding */
  static DWORD t[16384];FILE *g=fopen("GART.BIN","wb");DWORD k;
  for(k=0;k<4&&g;k++)if(phys_read_p(bar0+gart_base+k*65536UL,t,65536UL,PAGE_READONLY|PAGE_NOCACHE))fwrite(t,1,65536UL,g);
  if(g)fclose(g);fprintf(logf,"GART.BIN: the table at frame-buffer offset %08lX (physical %08lX)\n",(unsigned long)gart_base,(unsigned long)(bar0+gart_base));
 }
 cx_stop();
 fprintf(logf,"done\n");
 if(dev)IDirect3DDevice7_Release(dev);if(rt)IDirectDrawSurface7_Release(rt);if(d3d)IDirect3D7_Release(d3d);if(dd)IDirectDraw7_Release(dd);
 fclose(logf);printf("done, see %s\n",only>=0?"CLXP3D_1.TXT":"CLXP3D.TXT");return 0;
}
