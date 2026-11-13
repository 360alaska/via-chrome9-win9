/* VXS KERNELS BEGIN */
typedef struct {
 /* 0 */ const uint8_t *vb;           /* the span's first vertex */
 /* 4 */ uint32_t stride,n;           /* the vertex stride; the vertices */
 /* 12 */ uint32_t *w;                /* vxs_conv: the first record */
 /* 16 */ uint32_t wstride;           /* bytes from one record to the next (32 or 40) */
 /* 20 */ uint32_t coff;              /* the colour's offset, 0: none (white) */
 /* 24 */ uint32_t smode,soff;        /* specular: 0 none (0), 1 white (0xFF000000), 2 the word at soff */
 /* 32 */ uint32_t t0,v0on;           /* the first set's offset (0: no coordinates, 0 0), its second word read */
 /* 40 */ uint32_t two,t1,v1on;       /* two units: the second set's offset (0: 0 0), its second word read; one unit: the first set again */
 /* 52 */ uint32_t pad0,pad1,pad2;
 /* 64 */ uint32_t xlo,xrange,ylo,yrange; /* GB_IN's: w - lo <= range */
 /* 80 */ uint32_t xg,yg;             /* 0xFFFFFFFF: the guard band also takes 0x80000000..0xBF000000 (tc.guard and lo 0) */
 /* 88 */ uint32_t pad3,pad4;
 /* 96: the kernels' lane constants (the C does not read them): fixed (vxs_init) */
 /* 96 */ uint32_t kbias[4],kgu[4],kz[4],kw[4],kone[4],kneg0[4]; /* 0x80000000; 0xBF000000, 0x3F800000, 0x7F7FFFFF each xor the bias; 1; -0 in lanes 0-2 */
 /* 192: the template's (vxs_tc) */
 /* 192 */ uint32_t kxlo[4],kxr[4],kylo[4],kyr[4],kxg[4],kyg[4]; /* xlo, xrange ^ bias, ylo, yrange ^ bias, xg, yg */
} vxs_frame;
static uint32_t vxs_check_c(const vxs_frame *f) {
 uint32_t i;const uint8_t *p=f->vb;
 for(i=0;i<f->n;i++,p+=f->stride){const uint32_t x=u32(p),y=u32(p+4),z=u32(p+8),w=u32(p+12);
  const int gx=x-f->xlo<=f->xrange||(f->xg&&x>=0x80000000UL&&x<=0xBF000000UL),gy=y-f->ylo<=f->yrange||(f->yg&&y>=0x80000000UL&&y<=0xBF000000UL);
  if(!(gx&&gy&&z<=0x3F800000UL&&w-1UL<0x7F7FFFFFUL))break;}
 return i;
}
static uint32_t vxs_fin(uint32_t u) {return (u&0x7F800000UL)==0x7F800000UL?0:u;}
static void vxs_conv_c(const vxs_frame *f) {
 uint32_t i;const uint8_t *p=f->vb;uint8_t *o=(uint8_t*)f->w;
 for(i=0;i<f->n;i++,p+=f->stride,o+=f->wstride){uint32_t *w=(uint32_t*)(void*)o,x=u32(p),y=u32(p+4),z=u32(p+8),u=0,v=0;
  if(x==0x80000000UL)x=0;if(y==0x80000000UL)y=0;if(z==0x80000000UL)z=0;
  if(f->t0){u=vxs_fin(u32(p+f->t0));if(f->v0on)v=vxs_fin(u32(p+f->t0+4));}
  w[0]=x;w[1]=y;w[2]=z;w[3]=u32(p+12);w[4]=f->coff?u32(p+f->coff):0xFFFFFFFFUL;
  w[5]=f->smode==2?u32(p+f->soff):f->smode==1?0xFF000000UL:0;w[6]=u;w[7]=v;
  if(f->two){uint32_t u1=0,v1=0;if(f->t1){u1=vxs_fin(u32(p+f->t1));if(f->v1on)v1=vxs_fin(u32(p+f->t1+4));}w[8]=u1;w[9]=v1;}
  else{w[8]=u;w[9]=v;}
 }
}
/* the largest of count 16-bit indices (INDEXEDTRIANGLELIST2's token: three a triangle, back to back) */
static uint32_t vxs_max16_c(const uint8_t *p,uint32_t count) {uint32_t m=0,k,a;for(k=0;k<count;k++){a=(uint32_t)p[k*2]|(uint32_t)p[k*2+1]<<8;if(a>m)m=a;}return m;}
/* VXS KERNELS END */
