/* Guarded initialization for the three modes measured in VX000.LOG. */
BOOL VESA_init_hw()
{
 static vesa_mode_t storage[48];
 static const WORD wanted[6]={0x111,0x112,0x114,0x101,0x103,0x105}; /* v10.71: 640x480, 800x600, 1024x768 at 16bpp, then at 8bpp (VBE packed-pixel modes) */
 DWORD i, vram_size, vram_lin, map_size, chip_size, chip_n;
 CRS_32 regs;
 vesa_info_block_t *info;
 vesa_mode_info_t *mi;
 vesa_valid=FALSE;
 /* The Win98 DDK marks the global V86 allocator VMM_ICODE. Refuse late
  * loads before touching that service after VMM's init code is discarded. */
 if(VMM_GetSystemInitState()>=0x40000000UL) {
  vx_status.failure=VX_LATE_LOAD;
  dbg_printf("VX85598 refused late load: BIOS buffer allocator is init-only\n");
  return FALSE;
 }
 if(!VX_check_hardware()) return FALSE;
 /* Let VMM reserve this page. Never overwrite an assumed free B0000 page. */
 vesa_buf_v86=_Allocate_Global_V86_Data_Area(P_SIZE,
  GVDAPageAlign | GVDAForceLow | GVDAZeroInit);
 if(!vesa_buf_v86 || vesa_buf_v86>0x100000UL-P_SIZE ||
    (vesa_buf_v86 & (P_SIZE-1))) {
  vx_status.failure=VX_NO_BIOS_BUFFER; return FALSE;
 }
 vesa_buf=(void*)vesa_buf_v86;
 vx_status.bios_buffer=vesa_buf_v86;
 vx_status.stage=3;
 info=(vesa_info_block_t*)vesa_buf;
 mi=(vesa_mode_info_t*)((BYTE*)vesa_buf+MODE_OFFSET);
 memset(info,0,sizeof(*info));
 memcpy(info->VESASignature,"VBE2",4);
 load_client_state(&regs);
 regs.Client_EAX=VESA_CMD_ADAPTER_INFO;
 regs.Client_ES=V86_SEG(vesa_buf_v86);
 regs.Client_EDI=V86_OFF(vesa_buf_v86);
 dbg_printf("VX85598 BIOS 4F00 begin buffer=%lX\n",vesa_buf_v86);
 vesa_bios_V86(&regs);
 vx_status.last_bios_ax=regs.Client_EAX & 0xFFFF;
 if(!VESA_SUCC(regs) || memcmp(info->VESASignature,"VESA",4) ||
    info->VESAVersion<0x200 || !info->TotalMemory) {
  vx_status.failure=VX_BAD_VBE; return FALSE;
 }
 vesa_version=info->VESAVersion;
 vesa_caps=info->Capabilities;
 vx_status.vbe_version=vesa_version;
 vram_size=(DWORD)info->TotalMemory*65536UL;
 vx_vbe_total_bytes=vram_size;
 /* v10.04: host bridge (0:0.3) reg 0xA1 bits 6:4 = BIOS frame buffer size code n, size = 4 MB << n (measured at 64/128/256 MB). */
 chip_n=(VX_pci_read(0x80000300UL,0xA0)>>12)&7UL;
 chip_size=(chip_n>=1)?(4UL*1024UL*1024UL)<<chip_n:0UL;
 vx_chip_fb_bytes=chip_size;
 if(chip_size>vram_size) vram_size=chip_size; /* v10.73: VBE TotalMemory under-reports (32 MiB at 128 MB and up); the size decoded from the host bridge is the real one */
 map_size=vram_size; if(chip_size>map_size) map_size=chip_size;
 if(map_size>VX_FB_MAP_LIMIT) map_size=VX_FB_MAP_LIMIT;
 if(vram_size>VX_FB_LIMIT) vram_size=VX_FB_LIMIT;
 if(map_size<vram_size) map_size=vram_size;
 vesa_modes=storage;
 vesa_modes_cnt=0;
 for(i=0; i<sizeof(wanted)/sizeof(wanted[0]); i++) {
  DWORD pitch;
  memset(mi,0,sizeof(*mi));
  load_client_state(&regs);
  regs.Client_EAX=VESA_CMD_MODE_INFO;
  regs.Client_ECX=wanted[i];
  regs.Client_ES=V86_SEG(vesa_buf_v86+MODE_OFFSET);
  regs.Client_EDI=V86_OFF(vesa_buf_v86+MODE_OFFSET);
  dbg_printf("VX85598 BIOS 4F01 mode=%lX begin\n",(DWORD)wanted[i]);
  vesa_bios_V86(&regs);
  vx_status.last_bios_ax=regs.Client_EAX & 0xFFFF;
  dbg_printf("VX85598 4F01 AX=%lX attrs=%lX %ldx%ldx%ld model=%ld phys=%lX pitch=%ld linear_pitch=%ld\n",
   regs.Client_EAX & 0xFFFF,(DWORD)mi->ModeAttributes,(DWORD)mi->XResolution,
   (DWORD)mi->YResolution,(DWORD)mi->BitsPerPixel,(DWORD)mi->MemoryModel,
   (DWORD)mi->PhysBasePtr,(DWORD)mi->BytesPerScanLine,(DWORD)mi->LinBytesPerScanLine);
  dbg_printf("VX85598 linear RGB masks R=%ld@%ld G=%ld@%ld B=%ld@%ld\n",
   (DWORD)mi->LinRedMaskSize,(DWORD)mi->LinRedFieldPosition,
   (DWORD)mi->LinGreenMaskSize,(DWORD)mi->LinGreenFieldPosition,
   (DWORD)mi->LinBlueMaskSize,(DWORD)mi->LinBlueFieldPosition);
  if(VESA_SUCC(regs) && VX_mode_layout(vesa_version,wanted[i],mi,&pitch) &&
     pitch*mi->YResolution<=vram_size) {
   vesa_mode_t *m=&storage[vesa_modes_cnt++];
   memset(m,0,sizeof(*m));
   m->width=mi->XResolution; m->height=mi->YResolution;
   m->bpp=mi->BitsPerPixel; m->pitch=pitch;
   m->phy=mi->PhysBasePtr; m->mode_id=wanted[i]; m->flags=mi->ModeAttributes;
   dbg_printf("VX85598 accepted mode=%lX %ldx%ldx%ld pitch=%ld\n",
    (DWORD)m->mode_id,m->width,m->height,m->bpp,pitch);
  } else dbg_printf("VX85598 rejected mode=%lX AX=%lX\n",
   (DWORD)wanted[i],regs.Client_EAX & 0xFFFF);
 }
 if(!vesa_modes_cnt || storage[0].mode_id!=0x111) {
  vx_status.failure=VX_NO_MODES; return FALSE;
 }
 vx_base_modes=vesa_modes_cnt;
 /* v10.37: read the monitor EDID, then look through the BIOS mode list for extra linear 16/32bpp modes the monitor can show.
  * Registry EDID_MODES=0 (read later) hides them again. Without an EDID modes up to 1280x1024 are offered. */
 vx_bios_n=0;
 VX_edid_probe();
 if(!vx_edid_ok) {
  /* the BIOS knows which pins its VGA output uses: VBE/DDC, INT 10h AX=4F15h BL=01, controller 0, block 0 */
  BYTE *eb=(BYTE*)vesa_buf+CRTC_OFFSET;
  memset(eb,0,128);
  load_client_state(&regs);
  regs.Client_EAX=0x4F15;
  regs.Client_EBX=0x0001;
  regs.Client_ECX=0;
  regs.Client_EDX=0;
  regs.Client_ES=V86_SEG(vesa_buf_v86+CRTC_OFFSET);
  regs.Client_EDI=V86_OFF(vesa_buf_v86+CRTC_OFFSET);
  vesa_bios_V86(&regs);
  vx_bios_ddc_ax=regs.Client_EAX & 0xFFFF;
  if(VESA_SUCC(regs)) VX_edid_set(eb,4);
  dbg_printf("VX85598 BIOS DDC AX=%lX edid ok=%ld\n",vx_bios_ddc_ax,(DWORD)vx_edid_ok);
 }
 {
  WORD ids[64]; DWORD nid=0, fp=info->VideoModePtr, lin=((fp>>16)&0xFFFFUL)*16UL+(fp&0xFFFFUL), k, j;
  if(lin>=0x400UL && lin<0x10FFE0UL) {
   const WORD *lst=(const WORD*)lin;
   while(nid<64 && lst[nid]!=0xFFFF) {ids[nid]=lst[nid]; nid++;}
  }
  for(k=0;k<nid && vx_bios_n<64;k++) {
   DWORD v=2, w=0, h=0, b=0, at=0, md=0, pitch=0;
   WORD id=ids[k];
   for(j=0;j<vx_base_modes;j++) if(storage[j].mode_id==id) break;
   if(j<vx_base_modes) {v=0; w=storage[j].width; h=storage[j].height; b=storage[j].bpp; at=storage[j].flags; md=6;}
   else {
    memset(mi,0,sizeof(*mi));
    load_client_state(&regs);
    regs.Client_EAX=VESA_CMD_MODE_INFO;
    regs.Client_ECX=id;
    regs.Client_ES=V86_SEG(vesa_buf_v86+MODE_OFFSET);
    regs.Client_EDI=V86_OFF(vesa_buf_v86+MODE_OFFSET);
    vesa_bios_V86(&regs);
    if(VESA_SUCC(regs)) {
     w=mi->XResolution; h=mi->YResolution; b=mi->BitsPerPixel; at=mi->ModeAttributes; md=mi->MemoryModel;
     if((at&0x99)!=0x99 || md!=6) v=3;
     else if(b!=16 && b!=32) v=4;
     else if(w<640 || w>1920 || h<480 || h>1200) v=5;
     else if(!VX_mode_layout(vesa_version,id,mi,&pitch) || pitch*h>vram_size) v=6;
     else {
      for(j=0;j<vesa_modes_cnt;j++) if(storage[j].width==w && storage[j].height==h && storage[j].bpp==b) break;
      if(j<vesa_modes_cnt) v=7;
      else if(vx_edid_ok ? !VX_edid_mode_ok(w,h) : 0) {
       v=vx_edid_ok?8:9;   /* no EDID: offer up to 1280x1024, the most the XP driver shows on the same VGA adapter; EDID_MODES=2 offers the rest */
       if(vx_held_n<24) {   /* kept aside: registry EDID_MODES=2 offers these too */
        vesa_mode_t *m=&vx_held[vx_held_n++];
        memset(m,0,sizeof(*m));
        m->width=w; m->height=h; m->bpp=b; m->pitch=pitch;
        m->phy=mi->PhysBasePtr; m->mode_id=id; m->flags=mi->ModeAttributes;
        if(w>1024 || h>768) m->freqs[0]=60;
       }
      }
      else if(vesa_modes_cnt>=44) v=10;
      else {
       vesa_mode_t *m=&storage[vesa_modes_cnt++];
       memset(m,0,sizeof(*m));
       m->width=w; m->height=h; m->bpp=b; m->pitch=pitch;
       m->phy=mi->PhysBasePtr; m->mode_id=id; m->flags=mi->ModeAttributes;
       if(w>1024 || h>768) m->freqs[0]=60;   /* the BIOS leaves the 1024x768 timing in place for bigger modes, so give it a 60 Hz one */
       v=1;
       dbg_printf("VX85598 extra mode=%lX %ldx%ldx%ld pitch=%ld\n",(DWORD)id,w,h,b,pitch);
      }
     }
    }
   }
   vx_bios_log[vx_bios_n*3+0]=(DWORD)id|(v<<16);
   vx_bios_log[vx_bios_n*3+1]=(w&0xFFFFUL)|(h<<16);
   vx_bios_log[vx_bios_n*3+2]=(b&0xFFUL)|((md&0xFFUL)<<8)|((at&0xFFFFUL)<<16);
   vx_bios_n++;
  }
 }
 /* v10.55/56: sizes the BIOS does not list are offered on top of a BIOS mode of the same or wider pitch; the direct timing code then sets the
  * real active size and timing. No BIOS timing block for these (freqs[0]=0). */
 {
  static const unsigned short syn[][4]={{1600,900,1600,1200},{1152,864,1280,960},{1280,720,1280,768},{1360,768,1368,768},{1366,768,1368,768}};
  unsigned si; int bi;
  for(si=0; si<sizeof(syn)/sizeof(syn[0]); si++) {
   for(bi=0; bi<2; bi++) {
    DWORD bb=bi?32:16;
    int dj=-1, have=0;
    for(i=0;i<vesa_modes_cnt;i++) {
     if(storage[i].width==syn[si][2] && storage[i].height==syn[si][3] && storage[i].bpp==bb) dj=(int)i;
     if(storage[i].width==syn[si][0] && storage[i].height==syn[si][1] && storage[i].bpp==bb) have=1;
    }
    if(dj<0) for(i=0;i<vx_held_n;i++) if(vx_held[i].width==syn[si][2] && vx_held[i].height==syn[si][3] && vx_held[i].bpp==bb) dj=100+(int)i; /* donor held back by the EDID filter */
    if(dj>=0 && !have && vesa_modes_cnt<46 && (!vx_edid_ok || VX_edid_mode_ok(syn[si][0],syn[si][1]))) {
     vesa_mode_t *m=&storage[vesa_modes_cnt++];
     *m=(dj>=100)?vx_held[dj-100]:storage[dj];
     m->width=syn[si][0]; m->height=syn[si][1]; memset(m->freqs,0,sizeof(m->freqs));
    }
   }
  }
 }
 vx_status.mode_count=vesa_modes_cnt;
 vx_status.stage=4;
 vram_phy=vx_status.fb_phys;
 vram_lin=_MapPhysToLinear(vram_phy,map_size,0);
 while((!vram_lin || vram_lin==0xFFFFFFFFUL) && map_size>vram_size) {
  map_size>>=1; if(map_size<vram_size) map_size=vram_size;
  vram_lin=_MapPhysToLinear(vram_phy,map_size,0);
 }
 vx_map_bytes=map_size;
 if(!vram_lin || vram_lin==0xFFFFFFFFUL) {
  vx_status.failure=VX_NO_FB_MAP; return FALSE;
 }
 vx_status.fb_bytes=map_size;
 vx_status.fb_linear=vram_lin;
 if(!wram_init(VX_SHADOW_MB*1024UL*1024UL)) {
  vx_status.failure=VX_NO_SHADOW; return FALSE;
 }
 if(!FBHDA_init_hw()) { vx_status.failure=VX_NO_HDA; return FALSE; }
 memcpy(hda->vxdname,vesa_vxd_name,sizeof(vesa_vxd_name));
 hda->vram_size=vram_size;
 hda->vram_size_bar=vram_size;
 hda->vram_phylin=(void*)vram_lin;
 hda->flags |= FB_VESA_MODES | FB_FORCE_SOFTWARE;
 /* v10.24: draw straight into the BIOS linear frame buffer when every accepted mode uses it. */
 vx_direct=1;
 for(i=0; i<vesa_modes_cnt; i++) if(storage[i].phy!=vx_status.fb_phys) vx_direct=0;
 if(vx_direct) {
  hda->vram_pm32=(BYTE*)vram_lin;
  hda->vram_size_virt=vram_size;
 } else {
  hda->vram_pm32=(BYTE*)wram+wram->regs.s.fbmin;
  hda->vram_size_virt=wram->regs.s.fbmax-wram->regs.s.fbmin;
  hda->flags |= FB_SUPPORT_FLIPING;
 }
 vx_status.shadow_linear=(DWORD)hda->vram_pm32;
 vx_status.shadow_bytes=hda->vram_size_virt;
 vesa_pal=(vesa_palette_entry_t*)((BYTE*)vesa_buf+PAL_OFFSET);
 /* No BIOS PM table, custom refresh, MTRR change or hardware page flipping. */
 conf_hw_double_buf=0;
 conf_mtrr=0;
 wblit.num_changes=0;
 timer_set=async_blit_init(&wblit,VESA_draw);
 vesa_valid=TRUE;
 vx_status.vga_mode=1;
 vx_status.stage=5;
 vx_status.failure=VX_OK;
 dbg_printf("VX85598 ready: mapped=%lX bytes=%ld shadow=%lX bytes=%ld timer=%ld\n",
  vram_lin,vram_size,(DWORD)hda->vram_pm32,hda->vram_size_virt,(DWORD)timer_set);
 return TRUE;
}
