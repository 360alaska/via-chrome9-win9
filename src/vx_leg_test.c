/* VXLEG v1.1 (driver .601; v1.1: whether the driver offers its transform and lighting).
 * v1.0 (driver .600): the texture states of the Direct3D interfaces before DirectX 6, drawn through IDirect3DDevice2 (DirectX 5)
 * the way Tomb Raider 2 / 3, Final Reality or PC Player D3D Bench draw: TLVERTEX triangles, the texture given by TEXTUREHANDLE, filtering
 * by TEXTUREMAG / TEXTUREMIN, the texture operation by TEXTUREMAPBLEND, addressing by TEXTUREADDRESS, colour keys by COLORKEYENABLE,
 * vertex fog from the specular alpha. Each case draws a 64 x 64 square into an off-screen 3D surface and reads pixels back.
 * It also lists the hardware device as the old interface describes it (the capability words a DirectX 5 program checks), and the
 * render state ids the driver has received since it was loaded (run it after a game to see what that game sent).
 * Nothing is shown on the screen. Desktop at 16 or 32 bits. Log: VXLEG.TXT. */
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static FILE *lf; static int fails,cases;
static void say(const char *f,...) {static char b[2048];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);if(lf){fputs(b,lf);fflush(lf);}}
static const GUID iid_d3d2={0x6AAE1EC1UL,0x662A,0x11D0,{0x88,0x9D,0x00,0xAA,0x00,0xBB,0xB7,0x6A}};
static const GUID iid_hal={0x84E63DE0UL,0x46AA,0x11CF,{0x81,0x6F,0x00,0x00,0xC0,0x20,0x15,0x6E}};
static const GUID iid_tex2={0x93281502UL,0x8CF8,0x11D0,{0x89,0xAB,0x00,0xA0,0xC9,0x05,0x41,0x29}};
static LPDIRECTDRAW dd; static LPDIRECTDRAWSURFACE rt; static IDirect3D2 *d3; static IDirect3DDevice2 *dv; static IDirect3DViewport2 *vp;
static DWORD bpp; static int have_hal;

static HRESULT CALLBACK dev_cb(GUID *g,LPSTR desc,LPSTR name,LPD3DDEVICEDESC hal,LPD3DDEVICEDESC hel,LPVOID tag) {
 (void)hel;(void)tag;(void)desc;
 if(g&&!memcmp(g,&iid_hal,sizeof(GUID))&&hal&&hal->dwFlags) {
  const D3DPRIMCAPS *t=&hal->dpcTriCaps; have_hal=1;
  say("the hardware device as IDirect3D2 describes it (\"%s\"): size %lu flags %08lX devcaps %08lX\n",name?name:"",(unsigned long)hal->dwSize,(unsigned long)hal->dwFlags,(unsigned long)hal->dwDevCaps);
  say("   triangles: misc %08lX raster %08lX zcmp %08lX src %08lX dst %08lX acmp %08lX shade %08lX texture %08lX filter %08lX blend %08lX address %08lX\n",
   (unsigned long)t->dwMiscCaps,(unsigned long)t->dwRasterCaps,(unsigned long)t->dwZCmpCaps,(unsigned long)t->dwSrcBlendCaps,(unsigned long)t->dwDestBlendCaps,(unsigned long)t->dwAlphaCmpCaps,
   (unsigned long)t->dwShadeCaps,(unsigned long)t->dwTextureCaps,(unsigned long)t->dwTextureFilterCaps,(unsigned long)t->dwTextureBlendCaps,(unsigned long)t->dwTextureAddressCaps);
  say("   hardware transform and lighting in the device caps: %s\n",(hal->dwDevCaps&0x10000UL)?"yes":"no");
  say("   what a DirectX 5 program reads from them: bilinear filter %s, vertex fog %s, table fog %s, colour-keyed textures %s, alpha textures %s, alpha blending %s, MODULATEALPHA %s\n",
   (t->dwTextureFilterCaps&2)?"yes":"NO",(t->dwRasterCaps&0x80)?"yes":"NO",(t->dwRasterCaps&0x100)?"yes":"NO",(t->dwTextureCaps&8)?"yes":"NO",(t->dwTextureCaps&4)?"yes":"NO",
   (t->dwSrcBlendCaps&0x10)&&(t->dwDestBlendCaps&0x20)?"yes":"NO",(t->dwTextureBlendCaps&8)?"yes":"NO");
 }
 return D3DENUMRET_OK;
}
static HRESULT CALLBACK tf_cb(LPDDSURFACEDESC d,LPVOID ctx) {
 const DDPIXELFORMAT *f=&d->ddpfPixelFormat; (void)ctx;
 say("   texture format: flags %08lX, %lu bits, masks %08lX %08lX %08lX alpha %08lX%s\n",(unsigned long)f->dwFlags,(unsigned long)f->dwRGBBitCount,(unsigned long)f->dwRBitMask,(unsigned long)f->dwGBitMask,(unsigned long)f->dwBBitMask,
  (f->dwFlags&DDPF_ALPHAPIXELS)?(unsigned long)f->dwRGBAlphaBitMask:0UL,(f->dwFlags&DDPF_FOURCC)?" (FOURCC)":(f->dwFlags&(DDPF_PALETTEINDEXED8|DDPF_PALETTEINDEXED4))?" (palette)":"");
 return DDENUMRET_OK;
}
/* a texture in video memory: w x h, RGB565 (alpha 0) or ARGB4444 (alpha 1), filled by fn(x,y) which gives 0xAARRGGBB */
typedef DWORD (*texel_fn)(int,int);
static IDirect3DTexture2 *texture(int w,int h,int alpha,texel_fn fn,int key,DWORD keycolour,LPDIRECTDRAWSURFACE *keep_sys,LPDIRECTDRAWSURFACE *keep_vid) {
 DDSURFACEDESC s; LPDIRECTDRAWSURFACE sys=NULL,vid=NULL; IDirect3DTexture2 *ts=NULL,*tv=NULL; HRESULT hr; int x,y; DDCOLORKEY ck;
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT; s.dwWidth=w; s.dwHeight=h;
 s.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT); s.ddpfPixelFormat.dwFlags=DDPF_RGB|(alpha?DDPF_ALPHAPIXELS:0); s.ddpfPixelFormat.dwRGBBitCount=16;
 if(alpha) {s.ddpfPixelFormat.dwRBitMask=0x0F00; s.ddpfPixelFormat.dwGBitMask=0x00F0; s.ddpfPixelFormat.dwBBitMask=0x000F; s.ddpfPixelFormat.dwRGBAlphaBitMask=0xF000;}
 else {s.ddpfPixelFormat.dwRBitMask=0xF800; s.ddpfPixelFormat.dwGBitMask=0x07E0; s.ddpfPixelFormat.dwBBitMask=0x001F;}
 s.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_SYSTEMMEMORY;
 hr=IDirectDraw_CreateSurface(dd,&s,&sys,NULL); if(FAILED(hr)) {say("   texture (system memory): %08lX\n",(DWORD)hr); return NULL;}
 s.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY|DDSCAPS_ALLOCONLOAD;
 hr=IDirectDraw_CreateSurface(dd,&s,&vid,NULL); if(FAILED(hr)) {say("   texture (video memory): %08lX\n",(DWORD)hr); IDirectDrawSurface_Release(sys); return NULL;}
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s);
 if(SUCCEEDED(IDirectDrawSurface_Lock(sys,NULL,&s,DDLOCK_WAIT,NULL))) {
  for(y=0;y<h;y++) for(x=0;x<w;x++) {
   DWORD c=fn(x,y),r=(c>>16)&255,g=(c>>8)&255,b=c&255,a=c>>24; WORD v=alpha?(WORD)(((a>>4)<<12)|((r>>4)<<8)|((g>>4)<<4)|(b>>4)):(WORD)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
   ((WORD*)((BYTE*)s.lpSurface+y*s.lPitch))[x]=v;
  }
  IDirectDrawSurface_Unlock(sys,NULL);
 }
 if(key) {ck.dwColorSpaceLowValue=ck.dwColorSpaceHighValue=keycolour; IDirectDrawSurface_SetColorKey(sys,DDCKEY_SRCBLT,&ck); IDirectDrawSurface_SetColorKey(vid,DDCKEY_SRCBLT,&ck);}
 if(FAILED(IDirectDrawSurface_QueryInterface(sys,&iid_tex2,(void**)&ts))||FAILED(IDirectDrawSurface_QueryInterface(vid,&iid_tex2,(void**)&tv))) {say("   IDirect3DTexture2 not given\n"); if(ts) IDirect3DTexture2_Release(ts); IDirectDrawSurface_Release(sys); IDirectDrawSurface_Release(vid); return NULL;}
 hr=IDirect3DTexture2_Load(tv,ts); if(FAILED(hr)) say("   IDirect3DTexture2::Load: %08lX\n",(DWORD)hr);
 IDirect3DTexture2_Release(ts); *keep_sys=sys; *keep_vid=vid; return tv;
}
static DWORD pixel(int x,int y) { /* the render target's pixel as 0x00RRGGBB, 0xFFFFFFFF when it cannot be read */
 DDSURFACEDESC s; DWORD v=0xFFFFFFFFUL; memset(&s,0,sizeof(s)); s.dwSize=sizeof(s);
 if(FAILED(IDirectDrawSurface_Lock(rt,NULL,&s,DDLOCK_WAIT|DDLOCK_READONLY,NULL))) return v;
 if(bpp==16) {WORD p=((WORD*)((BYTE*)s.lpSurface+y*s.lPitch))[x]; DWORD r=(p>>11)&31,g=(p>>5)&63,b=p&31; v=(((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|((b<<3)|(b>>2));}
 else v=((DWORD*)((BYTE*)s.lpSurface+y*s.lPitch))[x]&0xFFFFFFUL;
 IDirectDrawSurface_Unlock(rt,NULL); return v;
}
static void clear(DWORD rgb) {
 DDBLTFX f; memset(&f,0,sizeof(f)); f.dwSize=sizeof(f);
 f.dwFillColor=bpp==16?(((rgb>>19)&31)<<11)|(((rgb>>10)&63)<<5)|((rgb>>3)&31):rgb;
 IDirectDrawSurface_Blt(rt,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f);
}
static void rs(DWORD id,DWORD v) {HRESULT hr=IDirect3DDevice2_SetRenderState(dv,(D3DRENDERSTATETYPE)id,v); if(FAILED(hr)) say("   SetRenderState(%lu, %lu): %08lX\n",id,v,(DWORD)hr);}
/* a 64 x 64 square at 16,16 with texture coordinates 0..umax, the diffuse and specular colours given */
static void square(float umax,DWORD diffuse,DWORD specular) {
 D3DTLVERTEX v[4]; int i; HRESULT hr; static const float px[4]={16,80,16,80},py[4]={16,16,80,80};
 memset(v,0,sizeof(v));
 for(i=0;i<4;i++) {v[i].sx=px[i]; v[i].sy=py[i]; v[i].sz=0.5f; v[i].rhw=1.0f; v[i].color=diffuse; v[i].specular=specular; v[i].tu=(i&1)?umax:0.0f; v[i].tv=(i&2)?umax:0.0f;}
 hr=IDirect3DDevice2_BeginScene(dv); if(FAILED(hr)) {say("   BeginScene: %08lX\n",(DWORD)hr); return;}
 hr=IDirect3DDevice2_DrawPrimitive(dv,D3DPT_TRIANGLESTRIP,D3DVT_TLVERTEX,v,4,D3DDP_DONOTCLIP|D3DDP_DONOTUPDATEEXTENTS); if(FAILED(hr)) say("   DrawPrimitive: %08lX\n",(DWORD)hr);
 IDirect3DDevice2_EndScene(dv);
}
static int near_(DWORD p,DWORD want,int tol) {
 int k; if(p==0xFFFFFFFFUL) return 0;
 for(k=0;k<3;k++) {int a=(int)((p>>(k*8))&255),b=(int)((want>>(k*8))&255); if(a-b>tol||b-a>tol) return 0;}
 return 1;
}
static void check(const char *what,DWORD got,DWORD want,int tol) {
 int ok=near_(got,want,tol); cases++; if(!ok) fails++;
 say("   %s %s: read %06lX, expected about %06lX\n",ok?"PASS":"FAIL",what,got&0xFFFFFFUL,want);
}
/* texel functions */
static DWORD t_quad(int x,int y) {return (y<1)?((x<1)?0xFFFF0000UL:0xFF00FF00UL):((x<1)?0xFF0000FFUL:0xFFFFFFFFUL);} /* 2 x 2: red green / blue white */
static DWORD t_grey(int x,int y) {(void)x;(void)y;return 0xFF808080UL;}
static DWORD t_white(int x,int y) {(void)x;(void)y;return 0xFFFFFFFFUL;}
static DWORD t_key(int x,int y) {(void)y;return x<4?0xFFFF00FFUL:0xFF00FF00UL;} /* 8 x 8: left half magenta (the key), right half green */
static DWORD t_half(int x,int y) {(void)y;return x<4?0x00FF0000UL:0xFFFF0000UL;} /* 8 x 8 with alpha: left half alpha 0, right half alpha 1, red */

static void defaults(D3DTEXTUREHANDLE h) {
 rs(D3DRENDERSTATE_TEXTUREHANDLE,h); rs(D3DRENDERSTATE_ZENABLE,0); rs(D3DRENDERSTATE_CULLMODE,D3DCULL_NONE); rs(D3DRENDERSTATE_SHADEMODE,D3DSHADE_GOURAUD);
 rs(D3DRENDERSTATE_TEXTUREMAG,D3DFILTER_NEAREST); rs(D3DRENDERSTATE_TEXTUREMIN,D3DFILTER_NEAREST); rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_MODULATE);
 rs(D3DRENDERSTATE_TEXTUREADDRESS,D3DTADDRESS_WRAP); rs(D3DRENDERSTATE_ALPHABLENDENABLE,0); rs(D3DRENDERSTATE_COLORKEYENABLE,0); rs(D3DRENDERSTATE_FOGENABLE,0);
 rs(D3DRENDERSTATE_SPECULARENABLE,0); rs(D3DRENDERSTATE_TEXTUREPERSPECTIVE,1); rs(D3DRENDERSTATE_DITHERENABLE,0); rs(D3DRENDERSTATE_ALPHATESTENABLE,0);
}
static void run_tests(void) {
 LPDIRECTDRAWSURFACE s1=NULL,v1=NULL,s2=NULL,v2=NULL,s3=NULL,v3=NULL,s4=NULL,v4=NULL,s5=NULL,v5=NULL; IDirect3DTexture2 *tq,*tg,*tw,*tk,*th; D3DTEXTUREHANDLE hq=0,hg=0,hw=0,hk=0,hh=0; DWORD p;
 tq=texture(2,2,0,t_quad,0,0,&s1,&v1); tg=texture(8,8,0,t_grey,0,0,&s2,&v2); tw=texture(8,8,0,t_white,0,0,&s3,&v3); tk=texture(8,8,0,t_key,1,0xF81F,&s4,&v4); th=texture(8,8,1,t_half,0,0,&s5,&v5);
 if(!tq||!tg||!tw||!tk||!th) {say("a texture could not be made: the drawing cases are left out\n"); fails++; goto out;}
 IDirect3DTexture2_GetHandle(tq,dv,&hq); IDirect3DTexture2_GetHandle(tg,dv,&hg); IDirect3DTexture2_GetHandle(tw,dv,&hw); IDirect3DTexture2_GetHandle(tk,dv,&hk); IDirect3DTexture2_GetHandle(th,dv,&hh);
 say("texture handles %lu %lu %lu %lu %lu\n",(unsigned long)hq,(unsigned long)hg,(unsigned long)hw,(unsigned long)hk,(unsigned long)hh);

 say("1. filtering (a 2 x 2 texture of red, green / blue, white on 64 x 64 pixels, clamped):\n");
 defaults(hq); rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_DECAL); rs(D3DRENDERSTATE_TEXTUREADDRESS,D3DTADDRESS_CLAMP);
 clear(0); square(1.0f,0xFFFFFFFFUL,0xFF000000UL);
 check("TEXTUREMAG NEAREST, in the red quarter",pixel(24,24),0xFF0000,16); check("TEXTUREMAG NEAREST, in the white quarter",pixel(72,72),0xFFFFFF,16);
 p=pixel(47,24); check("TEXTUREMAG NEAREST, just left of the middle: still pure red",p,0xFF0000,16);
 rs(D3DRENDERSTATE_TEXTUREMAG,D3DFILTER_LINEAR); rs(D3DRENDERSTATE_TEXTUREMIN,D3DFILTER_LINEAR);
 clear(0); square(1.0f,0xFFFFFFFFUL,0xFF000000UL);
 p=pixel(47,18); check("TEXTUREMAG LINEAR, between red and green: a mixture",p,0x808000,56);
 p=pixel(18,47); check("TEXTUREMAG LINEAR, between red and blue: a mixture",p,0x800080,56);

 say("2. TEXTUREMAPBLEND (a grey 808080 texture, diffuse red FF0000):\n");
 defaults(hg);
 rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_DECAL); clear(0); square(1.0f,0xFFFF0000UL,0xFF000000UL); check("DECAL: the texture alone",pixel(48,48),0x808080,16);
 rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_MODULATE); clear(0); square(1.0f,0xFFFF0000UL,0xFF000000UL); check("MODULATE: texture x diffuse",pixel(48,48),0x800000,16);
 rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_COPY); clear(0); square(1.0f,0xFFFF0000UL,0xFF000000UL); check("COPY: the texture alone",pixel(48,48),0x808080,16);
 rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_ADD); clear(0); square(1.0f,0xFF400000UL,0xFF000000UL); check("ADD: texture + diffuse 400000",pixel(48,48),0xC08080,20);

 say("3. alpha (a white texture without alpha, diffuse alpha one half, blending SRCALPHA / INVSRCALPHA over blue):\n");
 defaults(hw); rs(D3DRENDERSTATE_ALPHABLENDENABLE,1); rs(D3DRENDERSTATE_SRCBLEND,D3DBLEND_SRCALPHA); rs(D3DRENDERSTATE_DESTBLEND,D3DBLEND_INVSRCALPHA);
 rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_MODULATE); clear(0x0000FF); square(1.0f,0x80FFFFFFUL,0xFF000000UL); check("MODULATE, texture without alpha: the diffuse alpha blends",pixel(48,48),0x8080FF,20);
 rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_MODULATEALPHA); clear(0x0000FF); square(1.0f,0x80FFFFFFUL,0xFF000000UL); check("MODULATEALPHA: the diffuse alpha blends",pixel(48,48),0x8080FF,20);
 rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_DECAL); clear(0x0000FF); square(1.0f,0x80FFFFFFUL,0xFF000000UL); check("DECAL, texture without alpha: opaque",pixel(48,48),0xFFFFFF,16);
 say("   (a red ARGB4444 texture, left half alpha 0, right half alpha 1, diffuse opaque white, over blue):\n");
 defaults(hh); rs(D3DRENDERSTATE_ALPHABLENDENABLE,1); rs(D3DRENDERSTATE_SRCBLEND,D3DBLEND_SRCALPHA); rs(D3DRENDERSTATE_DESTBLEND,D3DBLEND_INVSRCALPHA);
 rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_MODULATE); clear(0x0000FF); square(1.0f,0xFFFFFFFFUL,0xFF000000UL);
 check("MODULATE, texture with alpha: alpha 0 leaves the blue",pixel(24,48),0x0000FF,16); check("MODULATE, texture with alpha: alpha 1 is red",pixel(72,48),0xFF0000,20);

 say("4. colour key (a texture magenta on the left, green on the right, the key magenta, over blue):\n");
 defaults(hk); rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_MODULATE);
 clear(0x0000FF); square(1.0f,0xFFFFFFFFUL,0xFF000000UL); check("COLORKEYENABLE 0: the key colour is drawn",pixel(24,48),0xFF00FF,16);
 rs(D3DRENDERSTATE_COLORKEYENABLE,1); clear(0x0000FF); square(1.0f,0xFFFFFFFFUL,0xFF000000UL);
 check("COLORKEYENABLE 1, MODULATE: the key colour leaves the blue",pixel(24,48),0x0000FF,16); check("COLORKEYENABLE 1, MODULATE: the rest is green",pixel(72,48),0x00FF00,16);
 rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_DECAL); clear(0x0000FF); square(1.0f,0xFFFFFFFFUL,0xFF000000UL);
 check("COLORKEYENABLE 1, DECAL: the key colour leaves the blue",pixel(24,48),0x0000FF,16);
 rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_MODULATE); rs(D3DRENDERSTATE_SPECULARENABLE,1); clear(0x0000FF); square(1.0f,0xFFFFFFFFUL,0xFF000000UL);
 check("COLORKEYENABLE 1, MODULATE with SPECULARENABLE: the key colour leaves the blue",pixel(24,48),0x0000FF,16);

 say("5. vertex fog (a grey texture, fog colour white, the fog factor in the specular alpha):\n");
 defaults(hg); rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_DECAL); rs(D3DRENDERSTATE_FOGENABLE,1); rs(D3DRENDERSTATE_FOGCOLOR,0xFFFFFF); rs(D3DRENDERSTATE_FOGTABLEMODE,D3DFOG_NONE);
 clear(0); square(1.0f,0xFFFFFFFFUL,0xFF000000UL); check("fog factor 255 (no fog): the texture",pixel(48,48),0x808080,16);
 clear(0); square(1.0f,0xFFFFFFFFUL,0x00000000UL); check("fog factor 0 (all fog): the fog colour",pixel(48,48),0xFFFFFF,16);
 clear(0); square(1.0f,0xFFFFFFFFUL,0x80000000UL); check("fog factor 128: half way",pixel(48,48),0xC0C0C0,24);
 rs(D3DRENDERSTATE_TEXTUREHANDLE,0); clear(0); square(1.0f,0xFF000000UL,0x00000000UL); check("no texture, fog factor 0: the fog colour",pixel(48,48),0xFFFFFF,16);

 say("6. TEXTUREADDRESS (the 2 x 2 texture with coordinates 0..2):\n");
 defaults(hq); rs(D3DRENDERSTATE_TEXTUREMAPBLEND,D3DTBLEND_DECAL);
 rs(D3DRENDERSTATE_TEXTUREADDRESS,D3DTADDRESS_WRAP); clear(0); square(2.0f,0xFFFFFFFFUL,0xFF000000UL); check("WRAP: the pattern repeats (red again at the right)",pixel(56,20),0xFF0000,16);
 rs(D3DRENDERSTATE_TEXTUREADDRESS,D3DTADDRESS_CLAMP); clear(0); square(2.0f,0xFFFFFFFFUL,0xFF000000UL); check("CLAMP: the edge texel continues (green at the right)",pixel(72,20),0x00FF00,16);
out:
 rs(D3DRENDERSTATE_TEXTUREHANDLE,0);
 if(tq) IDirect3DTexture2_Release(tq); if(tg) IDirect3DTexture2_Release(tg); if(tw) IDirect3DTexture2_Release(tw); if(tk) IDirect3DTexture2_Release(tk); if(th) IDirect3DTexture2_Release(th);
 if(v1) IDirectDrawSurface_Release(v1); if(s1) IDirectDrawSurface_Release(s1); if(v2) IDirectDrawSurface_Release(v2); if(s2) IDirectDrawSurface_Release(s2);
 if(v3) IDirectDrawSurface_Release(v3); if(s3) IDirectDrawSurface_Release(s3); if(v4) IDirectDrawSurface_Release(v4); if(s4) IDirectDrawSurface_Release(s4);
 if(v5) IDirectDrawSurface_Release(v5); if(s5) IDirectDrawSurface_Release(s5);
}
static void received(const char *when) {
 HMODULE m=LoadLibraryA("VXDDRAW.DLL"); DWORD (WINAPI *q)(DWORD *,DWORD); DWORD o[24],i,n=0,got;
 static const struct {DWORD id; const char *name;} L[]={{1,"TEXTUREHANDLE"},{3,"TEXTUREADDRESS"},{5,"WRAPU"},{6,"WRAPV"},{17,"TEXTUREMAG"},{18,"TEXTUREMIN"},{21,"TEXTUREMAPBLEND"},{27,"ALPHABLENDENABLE"},{28,"FOGENABLE"},
  {34,"FOGCOLOR"},{35,"FOGTABLEMODE"},{36,"FOGTABLESTART"},{37,"FOGTABLEEND"},{38,"FOGTABLEDENSITY"},{39,"STIPPLEENABLE"},{41,"COLORKEYENABLE"},{43,"BORDERCOLOR"},{44,"TEXTUREADDRESSU"},{45,"TEXTUREADDRESSV"},{15,"ALPHATESTENABLE"},{29,"SPECULARENABLE"},{26,"DITHERENABLE"},{4,"TEXTUREPERSPECTIVE"},{2,"ANTIALIAS"},{22,"CULLMODE"},{7,"ZENABLE"}};
 say("render states the driver has received, %s:\n",when);
 if(!m) {say("   VXDDRAW.DLL cannot be loaded\n"); return;}
 q=(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(m,"VX_D3D_QueryLegacy");
 memset(o,0,sizeof(o)); got=q?q(o,24):0;
 if(got<16) {say("   the driver does not answer (older than .600)\n"); FreeLibrary(m); return;}
 if(got>=17) say("   hardware transform and lighting offered by the driver: %s (ACCEL_D3D_TNL=0 in the registry turns it off)\n",(o[16]&1)?"yes":"no");
 say("   ids:"); for(i=0;i<256;i++) if(o[i>>5]&(1UL<<(i&31))) {say(" %lu",i); n++;} say(n?"\n":" none\n");
 say("   of the old texture and fog states:"); for(i=0;i<sizeof(L)/sizeof(L[0]);i++) if(o[L[i].id>>5]&(1UL<<(L[i].id&31))) say(" %s",L[i].name); say("\n");
 say("   old texture states written through to stage 0: %lu; the last TEXTUREMAG %lu, TEXTUREMIN %lu, TEXTUREMAPBLEND %lu, TEXTUREADDRESS %lu; MODULATE given the diffuse alpha %lu times, the texture's %lu times\n",o[8],o[9],o[10],o[11],o[12],o[13],o[14]);
 FreeLibrary(m);
}
typedef HRESULT (WINAPI *CREATE)(GUID *,LPDIRECTDRAW *,IUnknown *);
int main(void) {
 HMODULE mod; CREATE create; HRESULT hr; DDSURFACEDESC s; D3DVIEWPORT2 v; HDC dc; SYSTEMTIME t;
 lf=fopen("VXLEG.TXT","a"); GetLocalTime(&t);
 say("\n==== VXLEG v1.1 (driver .601) %04u-%02u-%02u %02u:%02u:%02u ====\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
 dc=GetDC(NULL); bpp=dc?(DWORD)GetDeviceCaps(dc,BITSPIXEL):0; if(dc) ReleaseDC(NULL,dc);
 say("desktop %d x %d, %lu bits\n",GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),bpp);
 received("before this program drew anything (what the programs run since the last restart sent)");
 if(bpp!=16&&bpp!=32) {say("The desktop must be at 16 or 32 bits.\n"); printf("Set the desktop to 16 or 32 bit colour.\n"); if(lf) fclose(lf); return 1;}
 mod=LoadLibraryA("DDRAW.DLL"); create=mod?(CREATE)GetProcAddress(mod,"DirectDrawCreate"):NULL;
 if(!create||FAILED(hr=create(NULL,&dd,NULL))) {say("DirectDrawCreate failed\n"); if(lf) fclose(lf); return 1;}
 IDirectDraw_SetCooperativeLevel(dd,NULL,DDSCL_NORMAL);
 hr=IDirectDraw_QueryInterface(dd,&iid_d3d2,(void**)&d3); if(FAILED(hr)) {say("IDirect3D2: %08lX\n",(DWORD)hr); goto out;}
 IDirect3D2_EnumDevices(d3,dev_cb,NULL);
 if(!have_hal) {say("IDirect3D2 lists no hardware device: a DirectX 5 program gets a software device\n"); fails++; goto out;}
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; s.dwWidth=128; s.dwHeight=128; s.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_3DDEVICE|DDSCAPS_VIDEOMEMORY;
 hr=IDirectDraw_CreateSurface(dd,&s,&rt,NULL); if(FAILED(hr)) {say("the off-screen 3D surface: %08lX\n",(DWORD)hr); fails++; goto out;}
 hr=IDirect3D2_CreateDevice(d3,&iid_hal,rt,&dv); if(FAILED(hr)) {say("IDirect3D2::CreateDevice(HAL): %08lX\n",(DWORD)hr); fails++; goto out;}
 say("texture formats the device lists:\n"); IDirect3DDevice2_EnumTextureFormats(dv,tf_cb,NULL);
 hr=IDirect3D2_CreateViewport(d3,&vp,NULL); if(FAILED(hr)) {say("CreateViewport: %08lX\n",(DWORD)hr); fails++; goto out;}
 IDirect3DDevice2_AddViewport(dv,vp);
 memset(&v,0,sizeof(v)); v.dwSize=sizeof(v); v.dwWidth=128; v.dwHeight=128; v.dvClipX=-1.0f; v.dvClipY=1.0f; v.dvClipWidth=2.0f; v.dvClipHeight=2.0f; v.dvMinZ=0.0f; v.dvMaxZ=1.0f;
 hr=IDirect3DViewport2_SetViewport2(vp,&v); if(FAILED(hr)) say("SetViewport2: %08lX\n",(DWORD)hr);
 hr=IDirect3DDevice2_SetCurrentViewport(dv,vp); if(FAILED(hr)) say("SetCurrentViewport: %08lX\n",(DWORD)hr);
 run_tests();
 received("after this program's cases");
out:
 say("%s: %d of %d cases failed\n",fails?"FAILED":"PASSED",fails,cases);
 if(vp) IDirect3DViewport2_Release(vp); if(dv) IDirect3DDevice2_Release(dv); if(rt) IDirectDrawSurface_Release(rt); if(d3) IDirect3D2_Release(d3); if(dd) IDirectDraw_Release(dd);
 printf("%s: %d of %d cases failed. See VXLEG.TXT.\n",fails?"FAILED":"PASSED",fails,cases);
 if(lf) fclose(lf);
 return fails?1:0;
}
