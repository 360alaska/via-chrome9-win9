/* Win9x legacy callbacks, included in vx_d3d_hal.c to share context ownership
 * and its single callback lock. Drawing is delegated to the bounded DP2 core.
 * Points, lines, textures and transformed/unlit vertices remain unsupported. */
static context_slot *legacy_context(uint32_t handle,HRESULT *hr) {
 context_slot *c=find(handle);vx3dn_info i={0}; /* v10.367 (audit): read in the failure branch when the INFO call failed */
 *hr=VX_D3D_CONTEXT_BAD;
 if(!c)return 0;
 if(!call(VX_IOCTL_3DN_INFO,0,0,&i,sizeof(i))||!i.ready||
    i.generation!=c->core.generation||c->core.lost) {
  if(!c->core.lost)note_lost((!i.ready)?1:2,i.generation);
  c->core.lost=1;*hr=DDERR_SURFACELOST;return 0;
 }
 follow_targets(c); /* v10.166: flips move the chain's memory */
 if(c->core.lost){*hr=DDERR_SURFACELOST;return 0;}
 *hr=DD_OK;return c;
}
static int readable(const void *p,uint32_t bytes) {
 return (!bytes || (p&&(uint32_t)p<=0xFFFFFFFFUL-bytes&&!IsBadReadPtr(p,bytes)));
}
static const uint8_t *buffer_range(DDRAWI_DDRAWSURFACE_LCL *s,uint32_t at,uint32_t bytes) {
 uint32_t n=surface_bytes(s),base;
 if(!s||!s->lpGbl||at>n||bytes>n-at)return 0;
 base=(uint32_t)s->lpGbl->fpVidMem;
 if(!base||base>0xFFFFFFFFUL-at||!readable((const void*)(base+at),bytes))return 0;
 return (const uint8_t*)(base+at);
}
static const uint8_t *legacy_mem(void *p) {
 LPDDRAWI_DDRAWSURFACE_INT i=(LPDDRAWI_DDRAWSURFACE_INT)p;DDRAWI_DDRAWSURFACE_LCL *l=surface_lcl(p);
 if(!l&&p&&!IsBadReadPtr(i,8))l=i->lpLcl;
 if(!l||IsBadReadPtr(l,8)||!l->lpGbl||IsBadReadPtr(l->lpGbl,sizeof(*l->lpGbl))||!l->lpGbl->fpVidMem)return 0;
 return (const uint8_t*)l->lpGbl->fpVidMem;
}
/* Temporary legacy objects do not always contain allocation dimensions.
 * Retain readable-range validation, but check addition before forming a
 * pointer: IsBadReadPtr cannot detect an offset that already wrapped. */
static const uint8_t *legacy_range(const uint8_t *base,uint32_t at,uint32_t bytes) {
 const uint8_t *p;
 if(!base||(uint32_t)base>0xFFFFFFFFUL-at)return 0;
 p=(const uint8_t*)((uint32_t)base+at);
 return readable(p,bytes)?p:0;
}
static void services(context_slot *c,vx_d3d_services *sv) {
 memset(sv,0,sizeof(*sv)); /* v10.214: colorkey, mips and faces were left uninitialised on this path */
 sv->context=c;sv->submit=native_submit;sv->resolve=resolve;sv->unknown=unknown;sv->bind_vertices=0;
}
static HRESULT legacy_token(context_slot *c,uint8_t op,uint32_t count,
 const uint8_t *payload,uint32_t bytes,const uint8_t *vertices,uint32_t vertex_bytes) {
 uint8_t *commands;vx_d3d_input in;vx_d3d_services sv;uint32_t error,hr;
 if(count>65535||bytes>0x100000UL)return DDERR_INVALIDPARAMS;
 commands=(uint8_t*)GlobalAlloc(GMEM_FIXED,bytes+4);
 if(!commands)return DDERR_OUTOFMEMORY;
 commands[0]=op;commands[1]=0;commands[2]=(uint8_t)count;commands[3]=(uint8_t)(count>>8);
 if(bytes)memcpy(commands+4,payload,bytes);
 memset(&in,0,sizeof(in));in.commands=commands;in.command_bytes=bytes+4;
 in.vertices=vertices;in.vertex_bytes=vertex_bytes;in.vertex_count=vertex_bytes/32;
 in.vertex_stride=32;in.fvf=0x1C4;services(c,&sv);
 hr=vx_d3d_dp2(&c->core,&in,&sv,&error);GlobalFree((HGLOBAL)commands);
 return (HRESULT)hr;
}
DWORD WINAPI VX_D3D_RenderState(vx_ddi_renderstate *p) {
 context_slot *c;const uint8_t *b;uint32_t fp[27];
 if(!p)return DDHAL_DRIVER_NOTHANDLED;
 if(!cb_enter()){p->ddrval=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}
 save_fp(fp);c=legacy_context(p->dwhContext,&p->ddrval);
 if(c) {
  p->ddrval=DDERR_INVALIDPARAMS;
  if(!p->dwCount)p->ddrval=DD_OK;
  else if(p->dwCount<=65535&&(b=legacy_range(legacy_mem(p->lpExeBuf),p->dwOffset,p->dwCount*8))!=0) /* v10.161: as RenderPrimitive */
  {uint32_t j;for(j=0;j<p->dwCount&&rs_count<64;j++){rs_ring[rs_count][0]=((const DWORD*)b)[j*2];rs_ring[rs_count][1]=((const DWORD*)b)[j*2+1];rs_count++;}
   p->ddrval=legacy_token(c,8,p->dwCount,b,p->dwCount*8,0,0);}
 }
 diagnostic.render_states++;record(3,p->ddrval);restore_fp(fp);
 cb_leave();return DDHAL_DRIVER_HANDLED;
}
DWORD WINAPI VX_D3D_RenderPrimitive(vx_ddi_renderprimitive *p) {
 context_slot *c;const uint8_t *b,*v;uint32_t n,fp[27];
 if(!p)return DDHAL_DRIVER_NOTHANDLED;
 if(!cb_enter()){p->ddrval=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}
 save_fp(fp);c=legacy_context(p->dwhContext,&p->ddrval);
 if(c) {
  p->ddrval=DDERR_UNSUPPORTED;n=p->diInstruction.wCount;
  if(p->diInstruction.bOpcode==3&&p->diInstruction.bSize==8) {
   /* v10.161: as vmhal9x reads them: the buffer's memory is lpLcl->lpGbl->fpVidMem of the object passed (for vertices DirectX may pass a temporary
    * object on its stack whose other members are not filled). Sizes come from what the triangles index, each range checked readable. */
   const uint8_t *eb=legacy_mem(p->lpExeBuf),*tb=legacy_mem(p->lpTLBuf);uint32_t j,top=0,vb;
   p->ddrval=DDERR_INVALIDPARAMS;diagnostic.last_opcode=3;
   if(!eb){diagnostic.last_offset=1;probe_buffer(1,p->lpExeBuf);}else if(!tb){diagnostic.last_offset=2;probe_buffer(2,p->lpTLBuf);}
   else if((b=legacy_range(eb,p->dwOffset,n*8))==0)diagnostic.last_offset=4;
   else {
    for(j=0;j<n;j++){uint32_t k;for(k=0;k<3;k++){uint32_t x=((const WORD*)(b+j*8))[k];if(x>top)top=x;}}
    vb=(top+1)*32;v=legacy_range(tb,p->dwTLOffset,vb);
    if(!v)diagnostic.last_offset=5;
    else {
     if(draw_count<8)draw_shade[draw_count]=c->core.state.rs[9];draw_count++;
     p->ddrval=legacy_token(c,3,n,b,n*8,v,vb);
     if(FAILED(p->ddrval)){diagnostic.last_offset=0x10;
      if(!trace_probe[0]){trace_probe[0]=0x10;trace_probe[1]=(DWORD)p->ddrval;trace_probe[2]=n;trace_probe[3]=top;probe_words(trace_probe+4,b,2);probe_words(trace_probe+6,v,6);}}
    }
   }
  }
 }
 diagnostic.legacy_draws++;record(4,p->ddrval);restore_fp(fp);
 cb_leave();return DDHAL_DRIVER_HANDLED;
}
DWORD WINAPI VX_D3D_GetState(vx_ddi_getstate *p) {
 context_slot *c;uint32_t id;
 if(!p)return DDHAL_DRIVER_NOTHANDLED;
 if(!cb_enter()){p->ddrval=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}
 c=legacy_context(p->dwhContext,&p->ddrval);id=(uint32_t)p->ddState.drstRenderStateType;
 if(c){p->ddrval=DDERR_UNSUPPORTED;if(p->dwWhich==4&&id<256){p->ddState.dwArg[0]=c->core.state.rs[id];p->ddrval=DD_OK;}}
 record(5,p->ddrval);cb_leave();return DDHAL_DRIVER_HANDLED;
}
DWORD WINAPI VX_D3D_Scene(vx_ddi_scene *p) {
 context_slot *c;
 if(!p)return DDHAL_DRIVER_NOTHANDLED;
 if(!cb_enter()){p->ddrval=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}
 c=legacy_context(p->dwhContext,&p->ddrval);if(c&&p->dwFlag>1)p->ddrval=DDERR_INVALIDPARAMS;
 record(6,p->ddrval);cb_leave();return DDHAL_DRIVER_HANDLED;
}
DWORD WINAPI VX_D3D_Clear(vx_ddi_clear *p) {
 static vx3dn_packet q; /* v10.237: static, used only under callback_lock (a 9272-byte local stepped over the stack guard) */
 context_slot *c;vx3dn_rect r;uint32_t j,n,color,rc,completed=0,fp[27];
 if(!p)return DDHAL_DRIVER_NOTHANDLED;
 if(!cb_enter()){p->ddrval=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}
 save_fp(fp);
 c=legacy_context(p->dwhContext,&p->ddrval);
 if(c) {
  p->ddrval=DDERR_INVALIDPARAMS;
  if(p->dwNumRects>4096||(p->dwFlags&~3UL)||
     !readable(p->lpRects,p->dwNumRects*16)||((p->dwFlags&2)&&!c->core.depth.format))goto done_clear;
  /* Validate the complete rectangle array before the first clear. */
  for(j=0;j<p->dwNumRects;j++) {
   memcpy(&r,p->lpRects+j,16);
   if(r.left<0||r.top<0||r.right<r.left||r.bottom<r.top||
      (uint32_t)r.right>c->core.target.width||(uint32_t)r.bottom>c->core.target.height)goto done_clear;
  }
  memset(&q,0,sizeof(q));q.cb=sizeof(q);q.abi=VX3DN_ABI;q.arm=VX3DN_ARM;
  q.op=VX3DN_CLEAR;q.generation=c->core.generation;q.target=c->core.target;q.depth=c->core.depth;
  q.z_func=1;q.write_mask=15;q.clear_flags=p->dwFlags;q.clear_depth=p->dwFillDepth;color=p->dwFillColor;
  if(q.target.format==VX3DN_RGB565) {
   uint32_t rr=(color>>11)&31,gg=(color>>5)&63,bb=color&31;
   color=0xFF000000UL|(((rr<<3)|(rr>>2))<<16)|(((gg<<2)|(gg>>4))<<8)|(bb<<3)|(bb>>2);
  }
  q.clear_color=color;p->ddrval=DD_OK;
  if(!q.clear_flags)goto done_clear;
  n=p->dwNumRects?p->dwNumRects:1;
  for(j=0;j<n;j++) {
   if(p->dwNumRects)memcpy(&q.clip,p->lpRects+j,16);else q.clip=c->core.state.viewport;
   rc=vx_d3d_submit_result(&c->core,native_submit(c,&q),completed);
   if(rc){p->ddrval=(HRESULT)rc;break;}
   completed++;
  }
 }
done_clear:
 record(9,p->ddrval);restore_fp(fp);cb_leave();return DDHAL_DRIVER_HANDLED;
}
