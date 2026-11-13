/* v10.321: latency distributions (HAL and VxD). A sample is TSC cycles; 124 buckets: 0..3 exact, then four per power of two (a bucket
 * spans a quarter of its octave, so a percentile is read to within 25 %). n = samples, sum = cycles / 256, max = the largest sample.
 * vx_lat_pct(l, per_mille) = the upper end of the bucket that holds that share of the samples (never above max). */
#ifndef VX_LAT_H
#define VX_LAT_H
#define VX_LAT_B 124 /* b = 31: buckets 120..123 */
typedef struct {uint32_t n,sum,max,h[VX_LAT_B];} vx_lat;
static uint32_t vx_lat_bucket(uint32_t v) {
 uint32_t b=2;
 if(v<4)return v;
 if(v>>16)b=16;
 if(v>>(b+8))b+=8;
 if(v>>(b+4))b+=4;
 while(b<31&&(v>>(b+1)))b++;
 return 4+(b-2)*4+((v>>(b-2))&3);
}
static uint32_t vx_lat_upper(uint32_t i) {
 uint32_t b,m;
 if(i<4)return i;
 b=(i-4)/4+2;m=(i-4)&3;
 return ((5UL+m)<<(b-2))-1; /* 8 << 29 wraps to 0: 0 - 1 = 0xFFFFFFFF, the top */
}
static void vx_lat_add(vx_lat *l,uint32_t v) {
 l->h[vx_lat_bucket(v)]++;l->n++;l->sum+=(v+128)>>8;if(v>l->max)l->max=v;
}
static uint32_t vx_lat_pct(const vx_lat *l,uint32_t pm) {
 uint32_t need,acc=0,i,u;
 if(!l->n)return 0;
 need=l->n/1000*pm+((l->n%1000)*pm+999)/1000;if(!need)need=1;
 for(i=0;i<VX_LAT_B;i++){acc+=l->h[i];if(acc>=need)break;}
 if(i>=VX_LAT_B)return l->max;
 u=vx_lat_upper(i);return u>l->max?l->max:u;
}
/* six words a distribution: samples, p50, p95, p99, max, sum (cycles / 256) */
static void vx_lat_out(const vx_lat *l,uint32_t *o) {
 o[0]=l->n;o[1]=vx_lat_pct(l,500);o[2]=vx_lat_pct(l,950);o[3]=vx_lat_pct(l,990);o[4]=l->max;o[5]=l->sum;
}
#endif
