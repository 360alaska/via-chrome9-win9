/* v10.307: DXT1 / DXT3 / DXT5 decode and encode built into VXGL.DLL, in place of the external libtxc_dxtn that Mesa 7.0.3 loads
 * (texcompress_s3tc.c includes this when USE_EXTERNAL_DXTN_LIB is 0). Same entry points and data layout as libtxc_dxtn:
 *  - fetch: srcRowStride in texels, blocks in rows of ((srcRowStride + 3) / 4), 8 bytes a block (DXT1) or 16 (DXT3, DXT5); out GLchan[4];
 *  - compress: srccomps 3 or 4 GLchans a pixel, rows width pixels apart; dest rows dstRowStride bytes apart (at least the packed row).
 * Decode as the S3TC spec: 565 widened by repeating the top bits; DXT1 colour 3 is black, transparent for RGBA_DXT1 when c0 <= c1;
 * DXT3 / DXT5 colours always the four-colour form. Encode: the endpoints from the block's principal axis (power iteration), one
 * least-squares refinement, each texel to the nearest of the four colours; DXT1 with alpha below 128 takes the three-colour form;
 * DXT5 alpha the eight-value form between the block's minimum and maximum. */
#ifndef VXGL_DXTN_H
#define VXGL_DXTN_H
#include <math.h>
#include <stdlib.h>

static void dxtn_565(unsigned v,int *c) {int r=(v>>11)&31,g=(v>>5)&63,b=v&31;c[0]=(r<<3)|(r>>2);c[1]=(g<<2)|(g>>4);c[2]=(b<<3)|(b>>2);}
static unsigned dxtn_pack(const double *c) {
 int r=(int)(c[0]*31.0/255.0+0.5),g=(int)(c[1]*63.0/255.0+0.5),b=(int)(c[2]*31.0/255.0+0.5);
 if(r<0)r=0;if(r>31)r=31;if(g<0)g=0;if(g>63)g=63;if(b<0)b=0;if(b>31)b=31;
 return (unsigned)((r<<11)|(g<<5)|b);
}
/* the four colours of a colour block (four = 1: always the four-colour form). v10.309 (review): the divisions by 3, 7 and 5 are
 * reciprocal multiplies, exact over the whole range they see (checked for every sum up to 3*255, 7*255 and 5*255) */
static void dxtn_palette(const GLubyte *blk,int four,int p[4][4]) {
 unsigned c0=blk[0]|(blk[1]<<8),c1=blk[2]|(blk[3]<<8);int k;
 dxtn_565(c0,p[0]);dxtn_565(c1,p[1]);p[0][3]=p[1][3]=255;
 if(four||c0>c1){for(k=0;k<3;k++){p[2][k]=((2*p[0][k]+p[1][k])*43691)>>17;p[3][k]=((p[0][k]+2*p[1][k])*43691)>>17;}p[2][3]=p[3][3]=255;}
 else {for(k=0;k<3;k++){p[2][k]=(p[0][k]+p[1][k])>>1;p[3][k]=0;}p[2][3]=255;p[3][3]=0;}
}
static void dxtn_colour(const GLubyte *blk,int four,int i,int j,GLchan *out,int rgbonly) {
 int p[4][4],code=(blk[4+(j&3)]>>((i&3)*2))&3;
 dxtn_palette(blk,four,p);
 out[0]=(GLchan)p[code][0];out[1]=(GLchan)p[code][1];out[2]=(GLchan)p[code][2];out[3]=(GLchan)(rgbonly?255:p[code][3]);
}
static const GLubyte *dxtn_block(GLint stride,const GLubyte *data,GLint i,GLint j,int bs) {return data+((size_t)((stride+3)/4)*(size_t)(j/4)+(size_t)(i/4))*(size_t)bs;}
static void vxgl_fetch_rgb_dxt1(GLint stride,GLubyte *data,GLint i,GLint j,GLvoid *out) {dxtn_colour(dxtn_block(stride,data,i,j,8),0,i,j,(GLchan*)out,1);}
static void vxgl_fetch_rgba_dxt1(GLint stride,GLubyte *data,GLint i,GLint j,GLvoid *out) {dxtn_colour(dxtn_block(stride,data,i,j,8),0,i,j,(GLchan*)out,0);}
static void vxgl_fetch_rgba_dxt3(GLint stride,GLubyte *data,GLint i,GLint j,GLvoid *out) {
 const GLubyte *b=dxtn_block(stride,data,i,j,16);int n=(j&3)*4+(i&3),a=(b[n/2]>>((n&1)*4))&15;
 dxtn_colour(b+8,1,i,j,(GLchan*)out,1);((GLchan*)out)[3]=(GLchan)(a*17);
}
static int dxtn_alpha(const GLubyte *b,int code) {
 int a0=b[0],a1=b[1];
 if(code==0)return a0;if(code==1)return a1;
 if(a0>a1)return (((8-code)*a0+(code-1)*a1)*9363)>>16;
 if(code==6)return 0;if(code==7)return 255;
 return (((6-code)*a0+(code-1)*a1)*13108)>>16;
}
static void vxgl_fetch_rgba_dxt5(GLint stride,GLubyte *data,GLint i,GLint j,GLvoid *out) {
 const GLubyte *b=dxtn_block(stride,data,i,j,16);int n=(j&3)*4+(i&3),bit=n*3,byte=2+bit/8,sh=bit&7,code;
 code=((b[byte]|((byte+1<8)?(b[byte+1]<<8):0))>>sh)&7;
 dxtn_colour(b+8,1,i,j,(GLchan*)out,1);((GLchan*)out)[3]=(GLchan)dxtn_alpha(b,code);
}

/* one colour block from 16 texels (RGBA); use[k] = 0 leaves texel k out of the fit (transparent in the three-colour form) */
static void dxtn_encode_colour(const int px[16][4],const int *use,int three,GLubyte *out) {
 double mean[3]={0,0,0},cov[6]={0,0,0,0,0,0},ax[3]={1,1,1},lo=1e30,hi=-1e30,e0[3],e1[3];int k,n=0,it,idx[16];unsigned c0,c1,bits=0;int p[4][4];
 for(k=0;k<16;k++)if(use[k]){mean[0]+=px[k][0];mean[1]+=px[k][1];mean[2]+=px[k][2];n++;}
 if(!n){out[0]=out[1]=out[2]=out[3]=0;if(three){out[2]=1;} /* c0 0 <= c1 1: three-colour, every texel colour 3 (transparent) */
  for(k=4;k<8;k++)out[k]=three?0xFF:0;return;}
 for(k=0;k<3;k++)mean[k]/=n;
 for(k=0;k<16;k++)if(use[k]){double r=px[k][0]-mean[0],g=px[k][1]-mean[1],b=px[k][2]-mean[2];cov[0]+=r*r;cov[1]+=r*g;cov[2]+=r*b;cov[3]+=g*g;cov[4]+=g*b;cov[5]+=b*b;}
 /* v10.319 (audit G4): the power iteration starts on the axis of largest variance (its column of cov is never 0 unless the block is one
  * colour); (1, 1, 1) is orthogonal to red against green, and such a block came out one muddy colour */
 ax[0]=ax[1]=ax[2]=0.0;if(cov[0]>=cov[3]&&cov[0]>=cov[5])ax[0]=1.0;else if(cov[3]>=cov[5])ax[1]=1.0;else ax[2]=1.0;
 for(it=0;it<8;it++){double x=cov[0]*ax[0]+cov[1]*ax[1]+cov[2]*ax[2],y=cov[1]*ax[0]+cov[3]*ax[1]+cov[4]*ax[2],z=cov[2]*ax[0]+cov[4]*ax[1]+cov[5]*ax[2],m=fabs(x);
  if(fabs(y)>m)m=fabs(y);if(fabs(z)>m)m=fabs(z);if(m<1e-9)break;ax[0]=x/m;ax[1]=y/m;ax[2]=z/m;}
 for(k=0;k<16;k++)if(use[k]){double t=(px[k][0]-mean[0])*ax[0]+(px[k][1]-mean[1])*ax[1]+(px[k][2]-mean[2])*ax[2];if(t<lo)lo=t;if(t>hi)hi=t;}
 for(k=0;k<3;k++){e0[k]=mean[k]+ax[k]*hi;e1[k]=mean[k]+ax[k]*lo;}
 for(it=0;it<2;it++) {
  c0=dxtn_pack(e0);c1=dxtn_pack(e1);
  if((three&&c0>c1)||(!three&&c0<c1)){unsigned t=c0;double d[3];int z;c0=c1;c1=t;for(z=0;z<3;z++){d[z]=e0[z];e0[z]=e1[z];e1[z]=d[z];}} /* e0 stays colour 0 */
  out[0]=(GLubyte)c0;out[1]=(GLubyte)(c0>>8);out[2]=(GLubyte)c1;out[3]=(GLubyte)(c1>>8);
  dxtn_palette(out,!three,p);
  for(k=0;k<16;k++){int m,best=0;long bd=0x7FFFFFFFL;
   if(!use[k]){idx[k]=3;continue;}
   for(m=0;m<(three?3:4);m++){long d=(long)(px[k][0]-p[m][0])*(px[k][0]-p[m][0])+(long)(px[k][1]-p[m][1])*(px[k][1]-p[m][1])+(long)(px[k][2]-p[m][2])*(px[k][2]-p[m][2]);if(d<bd){bd=d;best=m;}}
   idx[k]=best;}
  if(it)break;
  /* least squares: each texel = (1 - w) e0 + w e1 with w from its index; solve for e0, e1 */
  {double a=0,b=0,cc=0,x0[3]={0,0,0},x1[3]={0,0,0},det;
   for(k=0;k<16;k++){double w;int m;if(!use[k])continue;
    w=idx[k]==0?0:idx[k]==1?1:three?0.5:idx[k]==2?1.0/3:2.0/3;
    a+=(1-w)*(1-w);b+=(1-w)*w;cc+=w*w;for(m=0;m<3;m++){x0[m]+=(1-w)*px[k][m];x1[m]+=w*px[k][m];}}
   det=a*cc-b*b;
   if(fabs(det)>1e-9){int m;for(m=0;m<3;m++){e0[m]=(cc*x0[m]-b*x1[m])/det;e1[m]=(a*x1[m]-b*x0[m])/det;}}
   else break;}
 }
 if(!three&&c0==c1)for(k=0;k<16;k++)idx[k]=0; /* one colour: index 0 (c0 > c1 cannot hold) */
 for(k=0;k<16;k++)bits|=(unsigned)idx[k]<<(k*2);
 out[4]=(GLubyte)bits;out[5]=(GLubyte)(bits>>8);out[6]=(GLubyte)(bits>>16);out[7]=(GLubyte)(bits>>24);
}
static void dxtn_encode_alpha5(const int px[16][4],GLubyte *out) {
 int lo=255,hi=0,k,v[8];unsigned long long bits=0;
 for(k=0;k<16;k++){if(px[k][3]<lo)lo=px[k][3];if(px[k][3]>hi)hi=px[k][3];}
 out[0]=(GLubyte)hi;out[1]=(GLubyte)lo;
 for(k=0;k<8;k++)v[k]=dxtn_alpha(out,k);
 for(k=0;k<16;k++){int m,best=0,bd=1000;if(hi!=lo)for(m=0;m<8;m++){int d=abs(px[k][3]-v[m]);if(d<bd){bd=d;best=m;}}bits|=(unsigned long long)best<<(k*3);}
 for(k=0;k<6;k++)out[2+k]=(GLubyte)(bits>>(k*8));
}
static void vxgl_tx_compress_dxtn(GLint comps,GLint width,GLint height,const GLchan *src,GLenum fmt,GLubyte *dest,GLint dstRowStride) {
 int bs=(fmt==GL_COMPRESSED_RGB_S3TC_DXT1_EXT||fmt==GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)?8:16,bx,by,rowbytes=((width+3)/4)*bs;
 if(dstRowStride<rowbytes)dstRowStride=rowbytes;
 for(by=0;by<(height+3)/4;by++)for(bx=0;bx<(width+3)/4;bx++) {
  int px[16][4],use[16],k,three=0;GLubyte *o=dest+(size_t)by*dstRowStride+(size_t)bx*bs;
  for(k=0;k<16;k++){int x=bx*4+(k&3),y=by*4+(k>>2);const GLchan *s;if(x>=width)x=width-1;if(y>=height)y=height-1;
   s=src+((size_t)y*width+x)*comps;px[k][0]=s[0];px[k][1]=s[1];px[k][2]=s[2];px[k][3]=comps>3?s[3]:255;use[k]=1;}
  if(fmt==GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)for(k=0;k<16;k++)if(px[k][3]<128){use[k]=0;three=1;}
  if(bs==8){dxtn_encode_colour(px,use,three,o);continue;}
  if(fmt==GL_COMPRESSED_RGBA_S3TC_DXT3_EXT){for(k=0;k<8;k++){int a0=(px[k*2][3]*15+127)/255,a1=(px[k*2+1][3]*15+127)/255;o[k]=(GLubyte)(a0|(a1<<4));}}
  else dxtn_encode_alpha5(px,o);
  dxtn_encode_colour(px,use,0,o+8);
 }
}
#endif
