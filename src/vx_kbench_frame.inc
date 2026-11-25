/* vx_kbench_frame.inc (VXKBENCH v2.0, driver .768): the frames VXKBENCH's cases run on, apart from the tool so that
 * tests/test_kbench_frame.c can run every row once on the host with the board's kernel bytes (the rows must not fault, and the rows of
 * one job must draw the same thing: a comparison of two kernels given different work says nothing).
 * The includer has tnk_frame (vx_d3d_tnl_soa.inc with VX_TNK_FRAME_ONLY), memset, and gives the arrays' memory to kb_arrays():
 * KB_BLOCK bytes on a 64-byte line (the tnq_ kernels use aligned operands on the staged and clip arrays). */
#define NV 64
#define KB_FL (((NV+8)*4+63)&~63)                 /* a float array of NV + 8, on 64-byte lines */
#define KB_REC ((NV+8)*40)
#define KB_VB ((NV+16)*32)                        /* the range's source: 32-byte vertices (tnq_pos reads whole groups of four and four bytes past a position) */
#define KB_BLOCK (8*KB_FL+2*64+2*128+KB_REC+KB_VB+64)
static float *sx,*sy,*sz,*cx,*cy,*cz,*cw,*rw;static uint8_t *cc,*vcb,*rec,*vbuf;
static uint32_t kb_seed;static uint32_t kb_rnd(void) {kb_seed=kb_seed*1664525UL+1013904223UL;return kb_seed;}
static float kb_frnd(float lo,float hi) {return lo+(hi-lo)*(float)(kb_rnd()>>8)/16777216.0f;}
static void kb_splat(float *o,float v) {o[0]=o[1]=o[2]=o[3]=v;}
static uint32_t kb_bits(float f) {union {float f;uint32_t n;} u;u.f=f;return u.n;}
/* the arrays in the block, and the staged positions with the same positions as 32-byte vertices (x y z first) */
static void kb_arrays(uint8_t *blk) {
 uint32_t k;uint8_t *p=blk;
 sx=(float*)(void*)p;p+=KB_FL;sy=(float*)(void*)p;p+=KB_FL;sz=(float*)(void*)p;p+=KB_FL;cx=(float*)(void*)p;p+=KB_FL;cy=(float*)(void*)p;p+=KB_FL;cz=(float*)(void*)p;p+=KB_FL;cw=(float*)(void*)p;p+=KB_FL;rw=(float*)(void*)p;p+=KB_FL;
 cc=p;p+=128;vcb=p;p+=128;rec=p;p+=KB_REC;vbuf=p;
 kb_seed=12345;
 for(k=0;k<NV+8;k++){sx[k]=kb_frnd(-1.5f,1.5f);sy[k]=kb_frnd(-1.5f,1.5f);sz[k]=kb_frnd(-1.5f,1.5f);}
 memset(vbuf,0,KB_VB);
 for(k=0;k<NV+8;k++){float *v=(float*)(void*)(vbuf+k*32);v[0]=sx[k];v[1]=sy[k];v[2]=sz[k];}
}
/* the frame every case starts from: a perspective view of vertices in front of the eye, all inside the screen (640x480, depth 0 .. 1); a
 * rotation for world x view and for the normals; one directional light; a point light at the eye's side.
 * old 0: as the driver makes it since .768 (R1): the window folded into the matrix (its x column times 320, its y column times -240), the
 *        window's centre in vp[0] vp[2], the planes and the reciprocal's constants in the q fields;
 * old 1: as .544-.767 made it for tnk_pos: clip coordinates, the viewport's centre and scale in vp[0..5]. The same picture. */
static void base_frame(tnk_frame *f,int old) {
 static const float expc[7]={1.0f,0.6931471805599453f,0.2402265069591007f,0.0555041086648216f,0.0096181291076285f,0.0013333558146428f,0.0001540353039338f};
 static const float wv[16]={0.8f,0.0f,0.6f,0, 0.0f,1.0f,0.0f,0, -0.6f,0.0f,0.8f,0, 0.1f,-0.2f,5.0f,1};
 static const float ldir[2][3]={{0.3f,0.5f,-0.81f},{-0.6f,0.2f,-0.77f}},lcol[2][3]={{0.7f,0.6f,0.5f},{0.2f,0.3f,0.4f}};
 float m[16];uint32_t k,i,q;
 memset(f,0,sizeof(*f));
 kb_splat(f->kone,1.0f);kb_splat(f->k255,255.0f);f->kr255=1.0f/255.0f;kb_splat(f->khalf,0.5f);kb_splat(f->kl2e,1.4426950408889634f);kb_splat(f->kneg80,-80.0f);
 for(k=0;k<7;k++)kb_splat(f->kexpc[k],expc[k]);
 for(k=0;k<4;k++){((uint32_t*)(void*)f->kinf)[k]=0x7F800000UL;((uint32_t*)(void*)f->ksign)[k]=0x80000000UL;}
 kb_splat(f->klog[0],1.0f/3);kb_splat(f->klog[1],1.0f/5);kb_splat(f->klog[2],1.0f/7);kb_splat(f->klog[3],1.0f/9);kb_splat(f->ksqrt2,1.4142135623730951f);
 for(k=0;k<4;k++){f->kff[k]=0xFFUL;f->kmant[k]=0x007FFFFFUL;f->k127[k]=127;f->kmask24[k]=0x00FFFFFFUL;f->k7f[k]=0x7F;f->kvc[0][k]=1;f->kvc[1][k]=2;f->kvc[2][k]=4;f->kvc[3][k]=8;
  f->kb[0][k]=1;f->kb[1][k]=2;f->kb[2][k]=8;f->kb[3][k]=16;f->kb[4][k]=32;f->kb[5][k]=64;}
 /* clip = eye x projection (w = z, x and y scaled by 1.5, z to 0..w for near 1, far 100) */
 for(k=0;k<4;k++){m[k*4]=wv[k*4]*1.5f;m[k*4+1]=wv[k*4+1]*1.5f;m[k*4+2]=wv[k*4+2]*(100.0f/99.0f)-(k==3?100.0f/99.0f:0.0f);m[k*4+3]=wv[k*4+2];}
 if(old){kb_splat(f->vp[0],320.0f);kb_splat(f->vp[1],320.0f);kb_splat(f->vp[2],240.0f);kb_splat(f->vp[3],-240.0f);kb_splat(f->vp[4],0.0f);kb_splat(f->vp[5],1.0f);kb_splat(f->vp[6],0.0f);}
 else{for(k=0;k<16;k+=4){m[k]*=320.0f;m[k+1]*=-240.0f;} /* (the depth range 0 .. 1: the z column times 1) */
  kb_splat(f->vp[0],320.0f);kb_splat(f->vp[2],240.0f);} /* (vp[1] vp[3] vp[5]: not read; vp[4] the depth range's start, vp[6] zero: both 0) */
 for(k=0;k<16;k++){kb_splat(f->m[k],m[k]);kb_splat(f->e[k],wv[k]);}
 for(k=0;k<3;k++)for(i=0;i<3;i++)kb_splat(f->nm[k*3+i],wv[k*4+i]);
 /* R1's constants (tn_soa_prepare_b): the side planes at the viewport's half sizes, the far plane, the depth range's end; 2 and 1.5 for the
  * Newton steps; tnq_pos's inside test (a word inside when (word - lo) <= lim, kept ready for the signed compare) */
 kb_splat(f->qtwo,2.0f);kb_splat(f->q15,1.5f);kb_splat(f->qws,1.0f);
 kb_splat(f->qg[0],320.0f);kb_splat(f->qg[1],240.0f);kb_splat(f->qg[2],1.0f);kb_splat(f->qg[3],1.0f);kb_splat(f->qv[0],320.0f);kb_splat(f->qv[1],240.0f);
 for(k=0;k<4;k++){const uint32_t y=k&1,l0=kb_bits(0.0f),h0=kb_bits(y?480.0f:640.0f),l1=y?1UL:kb_bits(0.0f),h1=y?0x7F7FFFFFUL:kb_bits(1.0f);
  f->qlo[0][k]=l0+0x80000000UL;f->qlim[0][k]=(h0-l0)^0x80000000UL;f->qlo[1][k]=l1+0x80000000UL;f->qlim[1][k]=(h1-l1)^0x80000000UL;
  f->qvlo[k]=0.0f;f->qvhi[k]=y?480.0f:640.0f;f->qk12[k]=1UL<<(k&1);f->qk48[k]=4UL<<(k&1);}
 f->qsrc=vbuf;f->qdst=rec;f->qn=NV;f->qsstride=32;f->qdstride=32;
 f->lit=1;f->haveN=1;f->nnorm=0;f->nl=1;f->fogmode=0;f->stride=32;f->first=0;f->n=NV;
 for(k=0;k<2;k++)for(i=0;i<3;i++){kb_splat(f->ld[k][i],ldir[k][i]);kb_splat(f->lc[k][i],lcol[k][i]);}
 for(i=0;i<4;i++){kb_splat(f->base[i],i==3?1.0f:0.1f);kb_splat(f->lbase[i],i==3?1.0f:0.1f);}
 /* tnq_lit4 (R1): the same two lights and base colour as tnk_light's, as tn_soa_qcol gives them: the colours times 255, the alpha's byte, the
  * specular word; each direction taken through the normal matrix into the mesh's own space (tn_qlights: L'[i] = the matrix's row i . L), so
  * N.L' is the eye-space kernel's N.L */
 f->qnl=1;f->qnnorm=0;f->qvcol=0;
 for(k=0;k<2;k++)for(i=0;i<3;i++){kb_splat(f->ql[k][i],wv[i*4]*ldir[k][0]+wv[i*4+1]*ldir[k][1]+wv[i*4+2]*ldir[k][2]);kb_splat(f->qc[k][i],lcol[k][i]*255.0f);}
 for(i=0;i<3;i++)kb_splat(f->qbase[i],0.1f*255.0f);
 for(k=0;k<4;k++){f->qalpha[k]=255;f->qspec[k]=0xFF000000UL;}
 f->sx=sx;f->sy=sy;f->sz=sz;f->cx=cx;f->cy=cy;f->cz=cz;f->cw=cw;f->rw=rw;f->cc=cc;f->rec=rec;f->vc=0;
 for(k=0;k<8;k++){for(i=0;i<3;i++)kb_splat(f->pp[k][i],i==2?4.0f+(float)k:(float)k-3.0f);
  kb_splat(f->pr[k][0],20.0f);kb_splat(f->pr[k][1],1.0f);kb_splat(f->pr[k][2],0.1f);kb_splat(f->pr[k][3],0.01f);
  for(q=0;q<4;q++)for(i=0;i<3;i++){f->pdc[k][i][q]=0.5f;f->pac[k][i][q]=0.05f;}}
 f->pamb=255;f->pskip=0;f->npl=0;
 kb_seed=777; /* v2.0: the lanes' numbers the same for every row of a case (v1.2's rule), whichever row's frame is made */
 for(k=0;k<4;k++){f->gx[k]=kb_frnd(-1,1);f->gy[k]=kb_frnd(-1,1);f->gz[k]=kb_frnd(-1,1);f->gnx[k]=kb_frnd(-1,1);f->gny[k]=kb_frnd(-1,1);f->gnz[k]=kb_frnd(-1,1);f->gdc[k]=0xFFFFFFFFUL;f->gsc[k]=0;}
}
/* the cases: cs 0 positions, 64 staged vertices inside the screen; 9 the same, 4 vertices (the call's fixed part); 10 as 0 with the guard
 * band's viewport flags on; 18 the positions of a 64-vertex range read in place (tnq_pos: no clip codes); 1 one directional light;
 * 16 one, normals normalized; 17 two; 2 two, normals normalized; 11 one with exponential fog; 3 / 12 / 13 one, two, one with fog, the
 * normals given (after a point-light kernel); 4 one point light in range; 5 one out of range; 6 four in range; 7 four, two out of range;
 * 14 two point lights, one a spot light; 15 the same with specular lighting (power 20) */
static void set_case(tnk_frame *f,int cs,int old) {
 uint32_t k,i,q;base_frame(f,old);
 if(cs==9)f->n=4;
 if(cs==10){f->vc=vcb;if(!old){kb_splat(f->qg[0],640.0f);kb_splat(f->qg[1],480.0f);}} /* (R1: with the guard band the side planes lie at twice the viewport; .765 halved the matrix: the bench's old frame leaves it, as v1.1-v1.9 did) */
 if(cs==2||cs==12){f->nl=2;f->nnorm=cs==2;}
 if(cs==16)f->nnorm=1;
 if(cs==17)f->nl=2;
 f->qnl=f->nl;f->qnnorm=f->nnorm;
 if(cs==11||cs==13){f->fogmode=1;kb_splat(f->fog[0],0.05f);}
 if(cs==3||cs==12||cs==13){f->nready=1;for(k=0;k<4;k++){f->nv[0][k]=0.3f;f->nv[1][k]=0.4f;f->nv[2][k]=-0.86f;}}
 if(cs>=4&&cs<=7){f->nl=0;f->npl=cs>=6?4:1;if(cs==5)kb_splat(f->pr[0][0],0.5f);if(cs==7){kb_splat(f->pr[1][0],0.5f);kb_splat(f->pr[3][0],0.5f);}}
 if(cs==14||cs==15){f->nl=1;f->npl=2;f->nspot=1;f->pspot[1]=1;f->pmode[1]=1;
  kb_splat(f->psd[1][0],0.0f);kb_splat(f->psd[1][1],0.0f);kb_splat(f->psd[1][2],1.0f);kb_splat(f->pcone[1][0],0.2f);kb_splat(f->pcone[1][1],0.8f);kb_splat(f->pcone[1][2],1.0f);
  if(cs==15){f->spec=1;f->lview=0;kb_splat(f->spow,20.0f);f->spmode=3;f->spint=20;kb_splat(f->spcut,0.0f);
   for(k=0;k<16;k++)for(i=0;i<3;i++)for(q=0;q<4;q++)f->sdc[k][i][q]=0.3f;}}
}
