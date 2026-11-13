/* v10.325 (v10.326: says when the image or the registers were not read; v10.329: turns the VxD's check on first): the hardware cursor read back against what the driver wrote (VX_IOCTL_2D_CTRL 0xFFFFFE1D, 1E, 60 + k, 70 + i; vx855_2d.c).
 * Board, v10.280 and v10.321: blocks / a band that moved with the pointer, each time in a run whose hang recovery failed. Image dwords
 * that differ from what the driver wrote mean video memory was written over; cursor registers that read other than written, or display
 * registers 0x200-0x2FC that changed since the start, mean the cursor unit was changed under the driver. Used by VXDX7 and VXDISCO. */
typedef struct {int have; DWORD st[4],ep[4],dr[64],rg[16][3],vol[2];} vx_curchk; /* vol: display registers that changed between two reads at the start (counters, status) */
static int cc_ctl(HANDLE h,DWORD code,DWORD *o) {DWORD got=0; memset(o,0,16); return h!=INVALID_HANDLE_VALUE&&DeviceIoControl(h,0x85500012UL,&code,4,o,16,&got,0)&&got==16;}
static int curchk_read(HANDLE h,vx_curchk *c) {
 DWORD i,o[4];
 memset(c,0,sizeof(*c));
 if(!cc_ctl(h,0xFFFFFE1DUL,c->st)||!cc_ctl(h,0xFFFFFE1EUL,c->ep)||(c->st[3]>>24)>16||c->ep[3]>4096) return 0; /* older VxD: other answers */
 for(i=0;i<16;i++) {if(!cc_ctl(h,0xFFFFFE60UL+i,o)) return 0; memcpy(c->dr+i*4,o,16);}
 for(i=0;i<(c->st[3]>>24);i++) {if(!cc_ctl(h,0xFFFFFE70UL+i,o)) return 0; c->rg[i][0]=o[0]; c->rg[i][1]=o[1]; c->rg[i][2]=o[2];}
 c->have=1; return 1;
}
/* the start: read twice, 50 ms apart; display registers that changed between the two are left out of later comparisons */
static int curchk_base(HANDLE h,vx_curchk *c) {
 vx_curchk b; DWORD i,o[4]; POINT p;
 /* v10.329: the check is off unless ACCEL_CURSOR_CHECK = 1; 0xFFFFFE1F turns it on, and the cursor is programmed again (so its image and
  * registers are kept) at its next show: the pointer is moved by one pixel and back */
 cc_ctl(h,0xFFFFFE1FUL,o);
 if(GetCursorPos(&p)) {SetCursorPos(p.x+1,p.y); Sleep(30); SetCursorPos(p.x,p.y); Sleep(30);}
 if(!curchk_read(h,c)) return 0;
 Sleep(50); if(!curchk_read(h,&b)) return 1; /* v10.326: the first reading stands */
 for(i=0;i<64;i++) if(b.dr[i]!=c->dr[i]) c->vol[i>>5]|=1UL<<(i&31);
 return 1;
}
static int cc_pos(DWORD r) {return r==0x2D4||r==0x2D8||r==0x2F8||r==0x2FC||r==0x208||r==0x20C;} /* positions: they follow the pointer */
/* one line (more when something differs); returns 1 when the image, a register or a display register differs */
static int curchk_log(FILE *f,const char *when,const vx_curchk *c,const vx_curchk *base) {
 DWORD i,n=c->st[3]>>24; int bad=0,k=0;
 if(!c->have) {fprintf(f,"   cursor %s: the VxD does not answer the cursor check (older than v10.325)\n",when); return 0;}
 fprintf(f,"   cursor %s: mode %lu%s%s%s%s, %lu shapes written, %lu engine resets; image ",when,(unsigned long)((c->st[0]>>8)&3),
  c->st[0]&4?", shown":", hidden",c->st[0]&8?", mono unit":"",c->st[0]&16?", a mode set since the image was written":"",c->st[0]&128?", engine marked lost":"",(unsigned long)c->ep[1],(unsigned long)c->ep[0]);
 if(!(c->st[0]&0x400)) {fprintf(f,"the check is off (HKLM\\Software\\VX85598 ACCEL_CURSOR_CHECK = 1)\n"); return 0;}
 if(!(c->st[0]&1)) fprintf(f,"none written");
 else if(!(c->st[0]&32)) fprintf(f,"not compared (no video memory mapping)"); /* v10.326: 0 differing meant nothing then */
 else {fprintf(f,"%lu of %lu dwords differ from what the driver wrote",(unsigned long)c->st[1],(unsigned long)c->ep[3]);
  if(c->st[1]) {bad=1; fprintf(f," (the first at row %lu, dword %lu; image at %08lX)",(unsigned long)(c->ep[3]==4096?c->st[2]/64:c->st[2]/4),(unsigned long)(c->ep[3]==4096?c->st[2]%64:c->st[2]%4),(unsigned long)c->ep[2]);}}
 if(c->st[0]&64) {fprintf(f,"; registers not read (no register mapping)%s\n",bad?" (CHANGED)":""); return bad;} /* v10.326: no readings, nothing compared */
 fprintf(f,"; %lu cursor registers",(unsigned long)n);
 for(i=0;i<n;i++) if(c->rg[i][1]!=c->rg[i][2]) {
  int same=base&&base->have&&!(base->st[0]&64)&&i<(base->st[3]>>24)&&base->rg[i][0]==c->rg[i][0]&&base->rg[i][1]==c->rg[i][1]&&base->rg[i][2]==c->rg[i][2];
  if(!same&&!cc_pos(c->rg[i][0])) bad=1;
  fprintf(f,"%s %03lX written %08lX reads %08lX%s",k++?",":", read other than written:",(unsigned long)c->rg[i][0],(unsigned long)c->rg[i][1],(unsigned long)c->rg[i][2],same?" (as at the start)":"");
 }
 if(!k) fprintf(f," read as written");
 if(base&&base->have&&base!=c&&!(base->st[0]&64)) {k=0;
  for(i=0;i<64;i++) {DWORD r=0x200+i*4; if(c->dr[i]!=base->dr[i]&&!cc_pos(r)&&!(base->vol[i>>5]&(1UL<<(i&31)))) {bad=1; fprintf(f,"%s %03lX %08lX -> %08lX",k++?",":"; display registers changed since the start:",(unsigned long)r,(unsigned long)base->dr[i],(unsigned long)c->dr[i]);}}
  if(!k) fprintf(f,"; display registers 200-2FC as at the start");
  for(i=0;i<64;i++) if(base->vol[i>>5]&(1UL<<(i&31))) {fprintf(f,"%s %03lX",k>=0?"; changing by themselves, left out:":"",(unsigned long)(0x200+i*4)); k=-1;}
 }
 fprintf(f,"%s\n",bad?" (CHANGED)":"");
 return bad;
}
