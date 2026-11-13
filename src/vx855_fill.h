/* VX855 CPU-scan-converted flat fills (v9.7).
 *
 * The hardware triangle/line/rectangle fill primitives draw nothing on this
 * chip (see PROGRESS.TXT), but a type-0 point with 04/2A = width and
 * 04/2B = height fills an exact w x h pixel rectangle (measured, DOS EXP14
 * and Win98 v9.5/v9.6). These routines turn a triangle or rectangle into the
 * smallest list of such rectangles. Pure integer C with no driver
 * dependencies, so it can be unit-tested on a host.
 *
 * Fill rule: pixel (x,y) is covered when its centre (x+.5,y+.5) is inside the
 * primitive, with the standard top-left tie rule, so triangles that share an
 * edge never overlap and never leave a gap. Coordinates are integer pixels,
 * |coordinate| <= 8192; output is clipped to [0,clip_w) x [0,clip_h).
 * Rectangles cover x0 <= x < x1, y0 <= y < y1. Flat colour only: no
 * interpolation, textures or depth. */
#ifndef VX855_FILL_H
#define VX855_FILL_H
typedef void (*vx_fill_emit)(void *ctx,int32_t xs,int32_t xe,int32_t y,int32_t h);
typedef struct vxf_run {
 void *ctx; vx_fill_emit emit; int32_t xs,xe,y,h;
} vxf_run;
static int32_t vxf_floor_div(int32_t n,int32_t d) { /* d > 0 */
 int32_t q=n/d;
 if((n%d)!=0 && n<0) q--;
 return q;
}
static int32_t vxf_ceil_div(int32_t n,int32_t d) { return -vxf_floor_div(-n,d); }
static void vxf_flush(vxf_run *r) {
 if(r->h>0) r->emit(r->ctx,r->xs,r->xe,r->y,r->h);
 r->h=0;
}
/* Identical consecutive rows merge into one taller rectangle. */
static void vxf_span(vxf_run *r,int32_t xs,int32_t xe,int32_t y) {
 if(r->h>0 && r->xs==xs && r->xe==xe && r->y+r->h==y) {r->h++; return;}
 vxf_flush(r); r->xs=xs; r->xe=xe; r->y=y; r->h=1;
}
static void vx_fill_rect(int32_t x0,int32_t y0,int32_t x1,int32_t y1,
 int32_t clip_w,int32_t clip_h,vx_fill_emit emit,void *ctx) {
 if(x0<0) x0=0;
 if(y0<0) y0=0;
 if(x1>clip_w) x1=clip_w;
 if(y1>clip_h) y1=clip_h;
 if(x1>x0 && y1>y0) emit(ctx,x0,x1,y0,y1-y0);
}
static void vx_fill_triangle(int32_t x0,int32_t y0,int32_t x1,int32_t y1,
 int32_t x2,int32_t y2,int32_t clip_w,int32_t clip_h,vx_fill_emit emit,void *ctx) {
 int32_t vx[3],vy[3],area,t,y,ya,yb,lo,hi,e,a,b,dx,dy,A,ok;
 vxf_run run;
 area=(x1-x0)*(y2-y0)-(y1-y0)*(x2-x0);
 if(!area) return;
 if(area<0) {t=x1;x1=x2;x2=t;t=y1;y1=y2;y2=t;}
 vx[0]=x0;vx[1]=x1;vx[2]=x2;vy[0]=y0;vy[1]=y1;vy[2]=y2;
 ya=vy[0]; yb=vy[0];
 for(e=1;e<3;e++) {if(vy[e]<ya) ya=vy[e]; if(vy[e]>yb) yb=vy[e];}
 if(ya<0) ya=0;
 if(yb>clip_h) yb=clip_h;
 run.ctx=ctx; run.emit=emit; run.h=0; run.xs=run.xe=run.y=0;
 for(y=ya;y<yb;y++) {
  lo=0; hi=clip_w-1; ok=1;
  for(e=0;e<3;e++) {
   a=e; b=(e+1)%3; dx=vx[b]-vx[a]; dy=vy[b]-vy[a];
   /* Edge function at the pixel centre, divided by 2:
    * E = dx*(2y+1-2ay) - dy*(2x+1-2ax) = A - 2*dy*x. Covered when E>0,
    * or E==0 on a top edge (dy==0, dx>0) or a left edge (dy<0). */
   A=dx*(2*y+1-2*vy[a])+dy*(2*vx[a]-1);
   if(!dy) {
    if(!(A>0 || (A==0 && dx>0))) ok=0;
   } else if(dy>0) {
    t=vxf_floor_div(A-1,2*dy); if(t<hi) hi=t;
   } else {
    t=vxf_ceil_div(-A,-2*dy); if(t>lo) lo=t;
   }
  }
  if(ok && hi>=lo) vxf_span(&run,lo,hi+1,y);
 }
 vxf_flush(&run);
}
#endif
