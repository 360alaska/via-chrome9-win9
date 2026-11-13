/* v10.321: vx_lat.h buckets and percentiles against a brute-force reading */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vx_lat.h"
static int cmp(const void *a,const void *b){uint32_t x=*(const uint32_t*)a,y=*(const uint32_t*)b;return x<y?-1:x>y;}
int main(void){
 uint32_t v,i,s[5000];vx_lat l;int bad=0,k,t;
 /* every bucket's range: lower end and upper end land in it, the next value in the next bucket */
 for(i=0;i<VX_LAT_B;i++){uint32_t u=vx_lat_upper(i);if(vx_lat_bucket(u)!=i){printf("upper %u -> %u\n",i,vx_lat_bucket(u));bad++;}
  if(i+1<VX_LAT_B&&vx_lat_bucket(u+1)!=i+1){printf("next %u\n",i);bad++;}}
 if(vx_lat_upper(VX_LAT_B-1)!=0xFFFFFFFFUL){printf("top\n");bad++;}
 for(v=1;v<1000000;v=v*3/2+1)if(vx_lat_upper(vx_lat_bucket(v))<v||(vx_lat_bucket(v)>=4&&vx_lat_upper(vx_lat_bucket(v))>v+v/4+1)){printf("v %u\n",v);bad++;}
 srand(7);
 for(t=0;t<200;t++){int n=1+rand()%5000;memset(&l,0,sizeof(l));
  for(k=0;k<n;k++){s[k]=(uint32_t)rand()>>(rand()%31);if(t&1)s[k]|=(uint32_t)rand()<<20;vx_lat_add(&l,s[k]);}
  qsort(s,n,4,cmp);
  {static const uint32_t pm[3]={500,950,990};int j;for(j=0;j<3;j++){uint32_t need=(uint32_t)(((uint64_t)n*pm[j]+999)/1000),truth=s[(need?need:1)-1],got=vx_lat_pct(&l,pm[j]);
    if(vx_lat_bucket(got)!=vx_lat_bucket(truth)&&got!=l.max){printf("pct %d n %d pm %u truth %u got %u\n",t,n,pm[j],truth,got);bad++;}
    if(got<truth){printf("low %d\n",t);bad++;}}}
  if(l.max!=s[n-1]||l.n!=(uint32_t)n){printf("max/n\n");bad++;}}
 printf("%s\n",bad?"FAIL":"ok");return bad!=0;
}
