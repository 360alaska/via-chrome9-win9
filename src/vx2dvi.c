/* VX2DVI: dual-output lab for the VX855 (Windows 98). Separate from VX2DLAB on purpose.
   Buttons: ROUTING (read-only dump), DVI TEST (4 timed variants on IGA2/DVP1), DVI TUNE (live tuning).
   Safe exit: the window can be closed at any time; a running test is aborted, IGA2/DVP1 are restored, then the program ends. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "vx85598.h"
#include "vx855_3d.h"

#define ID_ROUTE 207
#define ID_DVI 208
#define ID_TUNE 209
#define ID_SW1 210
#define ID_SW2 211
#define ID_A1 212
#define ID_A2 213
#define ID_A3 214
#define ID_STOP 215
#define ID_LK1 216
#define ID_LK2 217
#define ID_LK3 218
#define ID_MODES 219
#define ID_FILT 220
#define ID_FIL2 221
#define ID_FIL3 222
#define ID_FIL4 223

static HANDLE device=INVALID_HANDLE_VALUE,logfile=INVALID_HANDLE_VALUE;
static vx3d_report reply;
static vx3d_request request;
static char status_text[2048]="Starting...";
static unsigned long recover_flag;
static int can_run,busy,abort_req,closing,iga2_live,lbon;
static HWND hmain,hb_route,hb_dvi,hb_tune,hb_sw1,hb_sw2,hb_a1,hb_a2,hb_a3,hb_stop,hb_lk1,hb_lk2,hb_lk3,hb_modes,hb_filt,hb_fil2,hb_fil3,hb_fil4;
static unsigned long fextra;
static int sweeping,answer;
static HFONT font;
static int tune_on,tune_sel;
static int tune_on_p(void) {return tune_on;}

static char lbuf[60000]; static unsigned lbn;
static void log_flush(void) {
 DWORD n;
 if(lbn&&logfile!=INVALID_HANDLE_VALUE) {WriteFile(logfile,lbuf,lbn,&n,NULL); FlushFileBuffers(logfile);}
 lbn=0; lbuf[0]=0;
}
static void logtext(const char *s) {
 unsigned l=(unsigned)strlen(s);
 if(lbn+l>=sizeof(lbuf)-1) log_flush();
 if(l>=sizeof(lbuf)-1) return;
 memcpy(lbuf+lbn,s,l); lbn+=l; lbuf[lbn]=0;
}
static void lb_begin(void) {}
static void lb_end(void) {log_flush();}
static void logf2(const char *format,...) {
 char buf[1024]; va_list a;
 va_start(a,format); _vsnprintf(buf,sizeof(buf)-1,format,a); va_end(a); buf[sizeof(buf)-1]=0;
 logtext(buf);
}
static void redraw(void) {if(hmain&&IsWindow(hmain)) {InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);}}
static void set_buttons(void) {
 int off=busy; if(!hb_route) return;
 EnableWindow(hb_route,!off&&!tune_on_p());
 EnableWindow(hb_dvi,!off&&!tune_on_p());
 EnableWindow(hb_tune,!off);
 EnableWindow(hb_sw1,!off&&!tune_on_p());
 EnableWindow(hb_sw2,!off&&!tune_on_p());
 EnableWindow(hb_fil4,!off&&!tune_on_p()); EnableWindow(hb_fil3,!off&&!tune_on_p()); EnableWindow(hb_fil2,!off&&!tune_on_p()); EnableWindow(hb_filt,!off&&!tune_on_p()); EnableWindow(hb_modes,!off&&!tune_on_p()); EnableWindow(hb_lk3,!off&&!tune_on_p()); EnableWindow(hb_lk1,!off&&!tune_on_p()); EnableWindow(hb_lk2,!off&&!tune_on_p());
 EnableWindow(hb_a1,sweeping); EnableWindow(hb_a2,sweeping); EnableWindow(hb_a3,sweeping); EnableWindow(hb_stop,sweeping);
}
static int call_lab(void) {
 DWORD got=0; unsigned long fl;
 request.cb=sizeof(request); request.version=VX3D_VERSION; request.arm=VX3D_ARM;
 request.reserved=21|(recover_flag?0x100UL:0UL); request.probe=VX3D_SF_PROBE;
 fl=request.lab_flags;
 memset(&reply,0,sizeof(reply));
 if(!DeviceIoControl(device,VX_IOCTL_3D_TEST,&request,sizeof(request),&reply,sizeof(reply),&got,NULL)) {
  sprintf(status_text,"Driver request failed: Windows error %lu.",GetLastError()); return 0;
 }
 if(got!=sizeof(reply)||reply.magic!=VX3D_MAGIC||reply.cb!=sizeof(reply)||reply.version!=VX3D_VERSION||reply.build!=VX3D_BUILD||reply.producer!=VX3D_PRODUCER) {
  strcpy(status_text,"The driver reply does not match this program. Use VX85598.VXD from the same package (v10.18) and restart Windows."); return 0;
 }
 recover_flag=(reply.fault_latched||reply.result==VX3D_DISABLED)?1:0;
 if((fl&2097152UL)&&reply.result==VX3D_PASS&&reply.image[0]==1) iga2_live=1;
 if(fl&4194304UL) iga2_live=0;
 return 1;
}
static void open_device(void) {
 vx_status_t s; DWORD got=0;
 device=CreateFile("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(device==INVALID_HANDLE_VALUE) {sprintf(status_text,"Cannot open VX85598: Windows error %lu.",GetLastError()); return;}
 memset(&s,0,sizeof(s));
 if(!DeviceIoControl(device,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC||s.cb!=sizeof(s)||s.version!=VX_VERSION) {
  strcpy(status_text,"Installed driver has an incompatible status interface."); return;
 }
 memset(&reply,0,sizeof(reply));
 if(!DeviceIoControl(device,VX_IOCTL_3D_PROBE,NULL,0,&reply,sizeof(reply),&got,NULL)||reply.magic!=VX3D_MAGIC||reply.build!=VX3D_BUILD) {
  strcpy(status_text,"Driver build does not match this program. Use VX85598.VXD from the same package and restart Windows."); return;
 }
 if(reply.fault_latched) {strcpy(status_text,"The engine reports a latched fault. Restart Windows."); return;}
 can_run=1;
 sprintf(status_text,"Ready (desktop %lux%lux%lu). ROUTING = read-only dump. DVI TEST = 4 variants, 12 s each (Esc or closing the window aborts and restores). DVI TUNE = live tuning. Log: VX2DVI.TXT next to this program.",s.width,s.height,s.bpp);
}
/* Waits ms while keeping the window alive. Returns early if Esc was pressed, the window is being closed, or WM_QUIT arrives
   (WM_QUIT is re-posted so the main loop still sees it). */
static void pump_wait(unsigned ms) {
 MSG m; DWORD end=GetTickCount()+ms;
 while(!abort_req && (long)(GetTickCount()-end)<0) {
  while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {
   if(m.message==WM_QUIT) {abort_req=1; PostQuitMessage((int)m.wParam); return;}
   if(m.message==WM_KEYDOWN&&m.wParam==VK_ESCAPE) {abort_req=1; continue;}
   TranslateMessage(&m); DispatchMessage(&m);
  }
  Sleep(30);
 }
}

static void route_edid(const unsigned char *e) {
 unsigned int i,sum=0,m; char name[16]; unsigned long pc;
 for(i=0;i<128;i++) sum+=e[i];
 if(e[0]!=0||e[1]!=0xFF||e[7]!=0) {logtext("    EDID header is not valid.\r\n"); return;}
 m=((unsigned)e[8]<<8)|e[9];
 logf2("    EDID: manufacturer %c%c%c, product %04X, serial %02X%02X%02X%02X, EDID %u.%u, checksum %s, extension blocks %u\r\n",
  '@'+((m>>10)&31),'@'+((m>>5)&31),'@'+(m&31),e[10]|(e[11]<<8),e[15],e[14],e[13],e[12],e[18],e[19],(sum&255)?"BAD":"ok",e[126]);
 logf2("    input: %s%s\r\n",(e[20]&0x80)?"DIGITAL":"ANALOG",(e[20]&0x80)?"":((e[20]&0x10)?" (blank-to-black)":""));
 pc=(unsigned long)(e[54]|(e[55]<<8))*10UL;
 if(pc) logf2("    first timing: %lu.%02lu MHz, %ux%u active\r\n",pc/1000UL,(pc%1000UL)/10UL,e[56]|((e[58]&0xF0)<<4),e[59]|((e[61]&0xF0)<<4));
 for(i=0;i<4;i++) {const unsigned char *d=e+54+18*i; if(d[0]==0&&d[1]==0&&d[3]==0xFC) {int k; for(k=0;k<13&&d[5+k]!=0x0A&&d[5+k]!=0;k++) name[k]=(char)d[5+k]; name[k]=0; logf2("    monitor name: %s\r\n",name);}}
}
static void do_routing(void) {
 static const char *bn[3]={"bus 1 (SR26, usually the VGA DDC)","bus 2 (SR31, DVI DDC / TMDS)","bus 3 (SR2C)"};
 vx_status_t s; DWORD got=0; unsigned int i,b,k; char line[200];
 if(!can_run) return;
 memset(&s,0,sizeof(s)); DeviceIoControl(device,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL);
 logf2("\r\nROUTING DUMP v10.13: read-only look at the display routing, DACs, and I2C buses. No video memory and no 3D engine are used.\r\n");
 memset(&request,0,sizeof(request)); request.lab_flags=1048576UL;
 sprintf(status_text,"Routing dump running ..."); redraw();
 if(!call_lab()) {logf2("ROUTING call failed: %s\r\n",status_text); return;}
 lb_begin();
 if(reply.result!=VX3D_PASS||reply.image[0]!=1) {logf2("ROUTING: driver result %lu, format %lu\r\n",(unsigned long)reply.result,(unsigned long)reply.image[0]); return;}
 logf2("Memory: BIOS frame-buffer setting (host bridge reg) %lu MiB; VBE TotalMemory %lu MiB; mapped window %lu MiB; mapping asked %lu MiB.\r\n",
  (unsigned long)reply.image[11]>>20,(unsigned long)reply.image[12]>>20,(unsigned long)reply.image[10]>>20,(unsigned long)reply.image[13]>>20);
 logf2("Desktop (IGA1) now: %lux%lu, %lu bpp, pitch %lu; scanout span %08lX..%08lX; PCI id %08lX; misc output %02lX (CRTC at %03lX); SR10 found %02lX\r\n",
  (unsigned long)s.width,(unsigned long)reply.image[6],(unsigned long)reply.image[7],(unsigned long)reply.image[5],(unsigned long)reply.image[8],(unsigned long)reply.image[9],(unsigned long)reply.image[4],(unsigned long)reply.image[1],(unsigned long)reply.image[2],(unsigned long)reply.image[3]);
 {
  const uint32_t *I=reply.image; unsigned long sr44=I[16+0x44],sr45=I[16+0x45],sr46=I[16+0x46],sr4a=I[16+0x4A],sr4b=I[16+0x4B],sr4c=I[16+0x4C];
  unsigned long pdm=sr44|((sr45&3)<<8),pdr=(sr45>>2)&7,pdn=sr46&0x7F,sdm=sr4a|((sr4b&3)<<8),sdr=(sr4b>>2)&7,sdn=sr4c&0x7F;
  logf2("Routing state (decoded with the OpenChrome register map):\r\n");
  logf2("  analog DAC %s (CR47[2]=%lu); analog source IGA%lu (SR16[6]=%lu)\r\n",(I[272+0x47]&4)?"OFF":"on",(I[272+0x47]>>2)&1,((I[16+0x16]>>6)&1)+1,(I[16+0x16]>>6)&1);
  logf2("  DVP1 pad SR1E[5:4]=%lu (3 = on); DVP1 source IGA%lu (CR9B[4]=%lu); DVP1 sync polarity CR9B[6:5]=%lu (1 = negative: bit5 HSYNC, bit6 VSYNC); CR9B[3:0] adjust %lu\r\n",(I[16+0x1E]>>4)&3,((I[272+0x9B]>>4)&1)+1,(I[272+0x9B]>>4)&1,(I[272+0x9B]>>5)&3,I[272+0x9B]&15);
  logf2("  integrated TMDS power-down CRD2[3]=%lu; its source CR99[4]=%lu; CR97 %02lX; DVP1 drive SR65 %02lX\r\n",(I[272+0xD2]>>3)&1,(I[272+0x99]>>4)&1,(unsigned long)I[272+0x97],(unsigned long)I[16+0x65]);
  logf2("  IGA2: CR6A %02lX (enable bit7=%lu, bit6=%lu), CR6B %02lX (screen-off bit2=%lu), CR6C %02lX, CR79 scaling bit0=%lu\r\n",(unsigned long)I[272+0x6A],(I[272+0x6A]>>7)&1,(I[272+0x6A]>>6)&1,(unsigned long)I[272+0x6B],(I[272+0x6B]>>2)&1,(unsigned long)I[272+0x6C],I[272+0x79]&1);
  logf2("  gated clocks SR1B %02lX, PLL power SR2D %02lX\r\n",(unsigned long)I[16+0x1B],(unsigned long)I[16+0x2D]);
  if(pdn&&sdn) logf2("  dot clocks: primary PLL (SR44-46) %lu.%03lu MHz, secondary/IGA2 PLL (SR4A-4C) %lu.%03lu MHz\r\n",14318UL*pdm/(pdn<<pdr)/1000UL,14318UL*pdm/(pdn<<pdr)%1000UL,14318UL*sdm/(sdn<<sdr)/1000UL,14318UL*sdm/(sdn<<sdr)%1000UL);
  logf2("  strap view (SR5A[0]=1): SR12 %02lX, SR13 %02lX (SR13[7:6]=%lu)\r\n",(unsigned long)I[12+2],(unsigned long)I[15],(unsigned long)((I[15]>>6)&3));
 }
 logtext("SR00..FF:\r\n");
 for(i=0;i<256;i+=16) {sprintf(line,"  %02X:",i); logtext(line); for(k=0;k<16;k++) {sprintf(line," %02lX",(unsigned long)reply.image[16+i+k]&0xFF); logtext(line);} logtext("\r\n");}
 logtext("CR00..FF:\r\n");
 for(i=0;i<256;i+=16) {sprintf(line,"  %02X:",i); logtext(line); for(k=0;k<16;k++) {sprintf(line," %02lX",(unsigned long)reply.image[272+i+k]&0xFF); logtext(line);} logtext("\r\n");}
 logtext("PCI config of the VX855 graphics function (dwords 00..FC):\r\n");
 for(i=0;i<64;i+=8) {sprintf(line,"  %02X:",i*4); logtext(line); for(k=0;k<8;k++) {sprintf(line," %08lX",(unsigned long)reply.image[528+i+k]); logtext(line);} logtext("\r\n");}
 for(b=0;b<3;b++) {
  const uint32_t *o=reply.image+600+200*b; unsigned char e[128]; unsigned int n;
  logf2("I2C %s: lines idle at SCL=%lu SDA=%lu%s\r\n",bn[b],(unsigned long)o[0]&1,((unsigned long)o[0]>>1)&1,(o[0]==3)?"":"  (a line is stuck low: bus not usable)");
  if(o[0]!=3) continue;
  n=(unsigned)o[1]; logf2("  devices answering: %u:",n); for(k=0;k<n&&k<32;k++) logf2(" %02lX(w %02lX)",(unsigned long)o[2+k],(unsigned long)o[2+k]*2); logtext("\r\n");
  if(o[35]==0) {for(k=0;k<32;k++) {e[4*k]=(unsigned char)o[40+k]; e[4*k+1]=(unsigned char)(o[40+k]>>8); e[4*k+2]=(unsigned char)(o[40+k]>>16); e[4*k+3]=(unsigned char)(o[40+k]>>24);}
   logtext("  EDID read OK (128 bytes):\r\n"); route_edid(e);
   for(k=0;k<128;k+=16) {unsigned int j; sprintf(line,"      %02X:",k); logtext(line); for(j=0;j<16;j++) {sprintf(line," %02X",e[k+j]); logtext(line);} logtext("\r\n");}
  } else logf2("  EDID: %s\r\n",o[35]==1?"nothing at 0x50 (no monitor DDC on this bus)":"0x50 answered but the read failed");
  if(o[80]) {logf2("  VT1632 (7-bit addr 0x08, 8-bit 0x10) present, regs 00..0F:"); for(k=0;k<16;k++) logf2(" %02lX",(unsigned long)o[81+k]&0xFF); logf2("   [vendor %04lX device %04lX; expect 1106/3192; reg09 bit2 = monitor sense %lu]\r\n",(unsigned long)((o[81]&0xFF)|((o[82]&0xFF)<<8)),(unsigned long)((o[83]&0xFF)|((o[84]&0xFF)<<8)),((unsigned long)o[90]>>2)&1);}
  else logtext("  VT1632 (7-bit 0x08): no answer\r\n");
  if(o[100]) {logtext("  SiI164-type TMDS (addr 0x38) present, regs 00..07:"); for(k=0;k<8;k++) logf2(" %02lX",(unsigned long)o[101+k]&0xFF); logtext("\r\n");}
 }
 sprintf(status_text,"Routing dump done. Send me VX2DVI.TXT."); logtext(status_text); logtext("\r\n"); lb_end(); redraw();
}

static void do_dvitest(void) {
 static const char *vn[6]={"1: monitor native mode (control, known good)","2: 1280x720, PLL as OpenChrome computes it (dm 83 dn 2)","3: 1280x720, PLL multiplied by 2 (same frequency, dm 166 dn 4)","4: 1280x720, PLL multiplied by 3 (same frequency, dm 249 dn 6)","5: 1280x720, PLL dtz = 3 (what the BIOS uses)","6: 1280x720, DVP1 adjust 7 instead of 15"};
 static const unsigned long vt[6]={0xB7FUL,0xB7FUL,0xB7FUL|(1UL<<17),0xB7FUL|(2UL<<17),0xB7FUL|(1UL<<14),0xB77UL};
 static const unsigned long vv[6]={5UL,1UL,1UL,1UL,1UL,1UL};
 unsigned int v,k; char line[300]; SYSTEMTIME t;
 if(!can_run) return;
 logf2("\r\nDVI TEST v10.17: the second display controller (IGA2) shows a colour-bar picture on the digital output (DVP1, VT1632 transmitter) while the VGA desktop stays on IGA1; 12 seconds per variant, everything restored after each. Variant 1 is the monitor's native mode (known good); variants 2-6 are 1280x720 60 Hz with different dot-clock PLL / pad settings, to find out why 720p cuts out.\r\nPicture: 8 colour bars, grey ramp, 1-pixel black/white stripes (left) and solid grey (right) at the bottom, 1-pixel white border, corner squares red (top-left), green (top-right), blue (bottom-left), white (bottom-right).\r\n");
 for(v=1;v<=6;v++) {
  memset(&request,0,sizeof(request)); request.lab_flags=2097152UL; request.lab_sy=vv[v-1]; request.lab_dx=vt[v-1];
  sprintf(status_text,"DVI test variant %u: setting up ...",v); redraw();
  if(!call_lab()) {logf2("DVI TEST call failed: %s\r\n",status_text); can_run=0; return;}
  GetLocalTime(&t);
  if(reply.result!=VX3D_PASS||reply.image[0]!=1) {
   logf2("Variant %u NOT applied: result %lu status %lu (2 = memory too small, 3 = memory check failed, 5 = monitor EDID unreadable or mode unusable); usable memory %lu MiB\r\n",v,(unsigned long)reply.result,(unsigned long)reply.image[0],(unsigned long)reply.image[2]>>20);
   sprintf(status_text,"DVI test stopped: variant %u not applied (status %lu).",v,(unsigned long)reply.image[0]); redraw(); return;
  }
  logf2("Variant %s applied at %02u:%02u:%02u.%03u; picture at %08lX (usable memory %lu MiB)\r\n",vn[v-1],t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,(unsigned long)reply.image[1],(unsigned long)reply.image[2]>>20);
  if(reply.image[190]) logf2("  native mode from the monitor EDID: %lu.%02lu MHz, %lux%lu (htotal %lu, hsync %lu-%lu, vtotal %lu, vsync %lu-%lu), sync %s%s; PLL dm %lu dn %lu dr %lu = %lu.%03lu MHz\r\n",(unsigned long)reply.image[191]/1000UL,((unsigned long)reply.image[191]%1000UL)/10UL,(unsigned long)reply.image[192],(unsigned long)reply.image[196],(unsigned long)reply.image[195],(unsigned long)reply.image[193],(unsigned long)reply.image[194],(unsigned long)reply.image[199],(unsigned long)reply.image[197],(unsigned long)reply.image[198],(reply.image[200]&1)?"H-":"H+",(reply.image[200]&2)?" V-":" V+",(unsigned long)reply.image[201],(unsigned long)reply.image[202],(unsigned long)reply.image[203],(unsigned long)reply.image[204]/1000UL,(unsigned long)reply.image[204]%1000UL);
  if(reply.image[208]) logf2("  PLL written: dm %lu dn %lu dr %lu (multiplier %lu)\r\n",(unsigned long)reply.image[205],(unsigned long)reply.image[206],(unsigned long)reply.image[207],(unsigned long)reply.image[208]);
  logtext("  CR before:"); for(k=0;k<35;k++) {sprintf(line," %02lX",(unsigned long)reply.image[100+k]&0xFF); logtext(line);}
  logtext("\r\n  CR after: "); for(k=0;k<35;k++) {sprintf(line," %02lX",(unsigned long)reply.image[16+k]&0xFF); logtext(line);}
  if(reply.image[149]&1) {
   logf2("\r\n  VT1632 regs 00..0F before:"); for(k=0;k<16;k++) logf2(" %02lX",(unsigned long)reply.image[170+k]&0xFF);
   logf2("\r\n  VT1632 regs 00..0F after init (%s):",(reply.image[149]&2)?"writes acknowledged":"WRITES NOT ACKNOWLEDGED"); for(k=0;k<16;k++) logf2(" %02lX",(unsigned long)reply.image[150+k]&0xFF);
   logf2("\r\n  VT1632 monitor sense (reg 09 bit 2) before %lu, after %lu; reg 08 bit 0 (power) before %lu, after %lu",((unsigned long)reply.image[170+9]>>2)&1,((unsigned long)reply.image[150+9]>>2)&1,(unsigned long)reply.image[170+8]&1,(unsigned long)reply.image[150+8]&1);
  } else logf2("\r\n  VT1632 did NOT answer on I2C bus 2");
  logtext("\r\n  (CR list: 50-5F 62 63 64 65 66 67 6A 6B 6C 79 97 99 9B A3 D2 68 92 94 95)   SR before 16/1B/1E/2D/4A/4B/4C/65:"); for(k=0;k<8;k++) {sprintf(line," %02lX",(unsigned long)reply.image[140+k]&0xFF); logtext(line);}
  logtext("   after:");
  for(k=0;k<8;k++) {sprintf(line," %02lX",(unsigned long)reply.image[64+k]&0xFF); logtext(line);}
  logtext("\r\n");
  sprintf(status_text,"DVI TEST variant %u of 6 is ON for 12 s: look at the digital output (capture) NOW. The VGA desktop should stay normal.",v);
  redraw(); MessageBeep(MB_ICONASTERISK);
  pump_wait(12000);
  memset(&request,0,sizeof(request)); request.lab_flags=4194304UL;
  if(!call_lab()) {logf2("DVI TEST restore call failed: %s\r\n",status_text); can_run=0; return;}
  GetLocalTime(&t);
  logf2("Variant %u restored at %02u:%02u:%02u.%03u (result %lu)\r\n",v,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,(unsigned long)reply.result);
  sprintf(status_text,"Variant %u restored.",v); redraw();
  if(abort_req) {logtext("DVI TEST aborted by the user (everything restored).\r\n"); break;}
  pump_wait(2500);
 }
 sprintf(status_text,"DVI test finished. Tell me which variant (if any) showed a picture on the digital output, and send VX2DVI.TXT.");
 logtext(status_text); logtext("\r\n"); redraw();
}

/* v10.16: live tuning of the digital output. Left/Right select a parameter, Up/Down change it (applied at once), G/B log "good"/"bad", Esc restores everything. */
static const char *tp_name[12]={"mode (1=720p 2=1080p 3=monitor native)","DVP1 adjust CR9B[3:0]","clock drive SR65[3:2]","data drive SR65[1:0]","VT1632 EDGE","VT1632 DSEL","VT1632 BSEL","OpenChrome FIFO","HSYNC inverted","VSYNC inverted","PLL dtz = 3","PLL multiplier (1-4, same frequency)"};
static int tp_val[12]={3,15,3,1,1,1,0,1,0,0,0,1};
static const int tp_max[12]={3,15,3,3,1,1,1,1,1,1,1,4};
static const int tp_min[12]={1,0,0,0,0,0,0,0,0,0,0,1};
static unsigned long tune_bits(void) {
 return (unsigned long)tp_val[1]|((unsigned long)tp_val[2]<<4)|((unsigned long)tp_val[3]<<6)|((unsigned long)tp_val[4]<<8)|((unsigned long)tp_val[5]<<9)|((unsigned long)tp_val[6]<<10)|((unsigned long)tp_val[7]<<11)|((unsigned long)tp_val[8]<<12)|((unsigned long)tp_val[9]<<13)|((unsigned long)tp_val[10]<<14)|((unsigned long)(tp_val[11]-1)<<17);
}
static void tune_status(const char *note) {
 char b[1500]; int i; unsigned n=0;
 n+=sprintf(b+n,"DVI TUNE %s  (Left/Right = select, Up/Down = change, G = log GOOD, B = log BAD, Esc = restore and stop)\r\n",note);
 for(i=0;i<12;i++) n+=sprintf(b+n,"%s%s = %d\r\n",i==tune_sel?"> ":"  ",tp_name[i],tp_val[i]);
 strcpy(status_text,b); set_buttons(); redraw();
}
static void tune_apply(int full) {
 SYSTEMTIME t;
 memset(&request,0,sizeof(request)); request.lab_flags=2097152UL; request.lab_sy=(unsigned long)(tp_val[0]==3?5:tp_val[0]);
 request.lab_dx=tune_bits()|(full?0UL:(1UL<<15))|(1UL<<16);
 if(!call_lab()||reply.result!=VX3D_PASS||reply.image[0]!=1) {
  logf2("TUNE apply failed (call result %lu status %lu)\r\n",(unsigned long)reply.result,(unsigned long)reply.image[0]); tune_on=0; tune_status("STOPPED (apply failed)"); return;
 }
 GetLocalTime(&t);
 logf2("TUNE %02u:%02u:%02u.%03u %s: mode %d adj %d clk %d data %d edge %d dsel %d bsel %d fifo %d hneg %d vneg %d dtz3 %d pllk %d\r\n",t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,full?"full":"light",tp_val[0],tp_val[1],tp_val[2],tp_val[3],tp_val[4],tp_val[5],tp_val[6],tp_val[7],tp_val[8],tp_val[9],tp_val[10],tp_val[11]);
}
static void tune_stop(void) {
 if(!tune_on) return;
 tune_on=0;
 memset(&request,0,sizeof(request)); request.lab_flags=4194304UL; (void)call_lab();
 logf2("TUNE ended and restored.\r\n"); tune_status("ENDED (registers restored)");
}
static void do_tune(void) {
 if(!can_run) return;
 if(tune_on) {tune_stop(); return;}
 logf2("\r\nDVI TUNE v10.17 started. Keys: Left/Right select, Up/Down change, G good, B bad, Esc end.\r\n");
 tune_on=1; tune_sel=0; tune_status("starting ...");
 tune_apply(1);
 if(tune_on) tune_status("running"); 
 SetFocus(hmain);
}
static void tune_key(WPARAM k) {
 int fullchg=0;
 if(!tune_on) return;
 if(k==VK_ESCAPE) {tune_stop(); return;}
 if(k==VK_LEFT) {tune_sel=(tune_sel+11)%12; tune_status("running"); return;}
 if(k==VK_RIGHT) {tune_sel=(tune_sel+1)%12; tune_status("running"); return;}
 if(k=='G'||k=='B') {logf2("TUNE MARK %s: mode %d adj %d clk %d data %d edge %d dsel %d bsel %d fifo %d hneg %d vneg %d dtz3 %d pllk %d\r\n",k=='G'?"GOOD":"BAD",tp_val[0],tp_val[1],tp_val[2],tp_val[3],tp_val[4],tp_val[5],tp_val[6],tp_val[7],tp_val[8],tp_val[9],tp_val[10],tp_val[11]); tune_status(k=='G'?"marked GOOD":"marked BAD"); return;}
 if(k==VK_UP||k==VK_DOWN) {
  int v=tp_val[tune_sel]+(k==VK_UP?1:-1);
  if(v>tp_max[tune_sel]) v=tp_min[tune_sel]; if(v<tp_min[tune_sel]) v=tp_max[tune_sel];
  tp_val[tune_sel]=v;
  fullchg=(tune_sel==0||tune_sel==10||tune_sel==11);
  tune_status("applying ...");
  tune_apply(fullchg);
  if(tune_on) tune_status("running");
 }
}

/* Waits (no timeout) until the user answers: returns 1 = good, 2 = poor sync, 3 = sync ok but bad colour / odd gradient (buttons or keys 1/2/3).
   Esc, the STOP button, closing the window or WM_QUIT set abort_req and return 0. */
static int step_wait(void) {
 MSG m; answer=0;
 while(!abort_req && !answer) {
  while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {
   if(m.message==WM_QUIT) {abort_req=1; PostQuitMessage((int)m.wParam); return 0;}
   if(m.message==WM_KEYDOWN) {
    if(m.wParam==VK_ESCAPE) {abort_req=1; return 0;}
    if(m.wParam=='1'||m.wParam==VK_NUMPAD1) answer=1; else if(m.wParam=='2'||m.wParam==VK_NUMPAD2) answer=2; else if(m.wParam=='3'||m.wParam==VK_NUMPAD3) answer=3;
    continue;
   }
   TranslateMessage(&m); DispatchMessage(&m);
  }
  Sleep(30);
 }
 return abort_req?0:answer;
}
/* Sweep of DVP1 data/clock phase: DVP1 adjust 15..0 with VT1632 EDGE=1, then again with EDGE=0, at one fixed mode. */
static void do_sweep(unsigned long var) {
 const unsigned long base=0xB7FUL; int e,a,n=0,key; SYSTEMTIME t;
 if(!can_run) return;
 logf2("\r\nDVI SWEEP v1.6 (%s): the mode is applied once, then DVP1 adjust CR9B[3:0] goes 15..0 with VT1632 EDGE=1 and then 15..0 with EDGE=0 (32 steps). Each step waits for your answer: GOOD, POOR SYNC, or SYNC OK but BAD COLOUR. Esc, STOP or closing the window stops and restores.\r\n",var==5?"monitor native mode":"1280x720 60 Hz");
 memset(&request,0,sizeof(request)); request.lab_flags=2097152UL; request.lab_sy=var; request.lab_dx=base;
 sweeping=1; set_buttons(); sprintf(status_text,"SWEEP: applying the mode ..."); redraw();
 if(!call_lab()) {logf2("SWEEP call failed: %s\r\n",status_text); can_run=0; sweeping=0; set_buttons(); return;}
 if(reply.result!=VX3D_PASS||reply.image[0]!=1) {sweeping=0; set_buttons(); logf2("SWEEP NOT applied: result %lu status %lu\r\n",(unsigned long)reply.result,(unsigned long)reply.image[0]); sprintf(status_text,"Sweep stopped: mode not applied (status %lu).",(unsigned long)reply.image[0]); redraw(); return;}
 if(reply.image[208]) logf2("  PLL written: dm %lu dn %lu dr %lu\r\n",(unsigned long)reply.image[205],(unsigned long)reply.image[206],(unsigned long)reply.image[207]);
 for(e=1;e>=0&&!abort_req;e--) for(a=15;a>=0&&!abort_req;a--) {
  n++;
  if(n>1) {
   memset(&request,0,sizeof(request)); request.lab_flags=2097152UL; request.lab_sy=var;
   request.lab_dx=(base&~0x10FUL)|(unsigned long)a|((unsigned long)e<<8)|(1UL<<15)|(1UL<<16);
   if(!call_lab()||reply.result!=VX3D_PASS||reply.image[0]!=1) {logf2("SWEEP step %d apply failed\r\n",n); break;}
  }
  GetLocalTime(&t);
  sprintf(status_text,"SWEEP step %d of 32:  DVP1 adjust %d,  VT1632 EDGE %d.\r\nLook at the Dell, then answer (keys 1 / 2 / 3 also work). It waits for you.",n,a,e); redraw(); MessageBeep(MB_OK);
  key=step_wait();
  if(abort_req) {logf2("SWEEP step %2d  adj %2d  edge %d  stopped before an answer\r\n",n,a,e); break;}
  logf2("SWEEP step %2d  adj %2d  edge %d  %02u:%02u:%02u  %s\r\n",n,a,e,t.wHour,t.wMinute,t.wSecond,key==1?"GOOD":(key==2?"POOR SYNC":"SYNC OK, BAD COLOUR / ODD GRADIENT"));
 }
 memset(&request,0,sizeof(request)); request.lab_flags=4194304UL;
 if(!call_lab()) logf2("SWEEP restore call failed: %s\r\n",status_text);
 logf2("SWEEP finished%s (restored).\r\n",abort_req?" (stopped early)":"");
 sweeping=0; set_buttons(); sprintf(status_text,"Sweep finished and restored. Send me VX2DVI.TXT."); redraw();
}

/* Repeatability of the PLL lock: the same settings are applied from scratch (restore, 3 s of no signal, full apply with PLL reset) 8 times;
   after each you say how the picture looks. If the result changes from attempt to attempt, the lock itself is the random part. */
static void do_lock(unsigned long var) {
 const unsigned long base=(var==1UL)?0xB7FUL:0xB7DUL; /* native: adjust 13 (middle of the good window found by the sweep); 720p: adjust 15 */ int n,key,cnt[4]; SYSTEMTIME t;
 if(!can_run) return;
 cnt[0]=cnt[1]=cnt[2]=cnt[3]=0;
 logf2("\r\nLOCK TEST v1.6 (%s, DVP1 adjust %lu, EDGE 1, PLL as OpenChrome): 8 attempts, each = restore, 3 s without signal, apply from scratch. Answer each: GOOD / POOR SYNC / SYNC OK but BAD COLOUR. Give each attempt about 15 s.\r\n",(var==5?"monitor native mode":(var==2?"1920x1080 60 Hz":"1280x720 60 Hz")),base&15UL);
 sweeping=1; set_buttons();
 for(n=1;n<=8&&!abort_req;n++) {
  if(iga2_live) {
   sprintf(status_text,"LOCK TEST attempt %d of 8: restoring, 3 s without signal ...",n); redraw();
   memset(&request,0,sizeof(request)); request.lab_flags=4194304UL;
   if(!call_lab()) {logf2("LOCK restore call failed: %s\r\n",status_text); can_run=0; break;}
   pump_wait(3000); if(abort_req) break;
  }
  memset(&request,0,sizeof(request)); request.lab_flags=2097152UL; request.lab_sy=var; request.lab_dx=base;
  if(!call_lab()) {logf2("LOCK apply call failed: %s\r\n",status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS||reply.image[0]!=1) {logf2("LOCK attempt %d NOT applied: result %lu status %lu\r\n",n,(unsigned long)reply.result,(unsigned long)reply.image[0]); break;}
  GetLocalTime(&t);
  sprintf(status_text,"LOCK TEST attempt %d of 8 is ON. Watch the Dell for about 15 s, then answer (keys 1 / 2 / 3 also work).",n); redraw(); MessageBeep(MB_OK);
  key=step_wait();
  if(abort_req) {logf2("LOCK attempt %d stopped before an answer\r\n",n); break;}
  cnt[key]++;
  logf2("LOCK attempt %d  applied %02u:%02u:%02u.%03u  PLL dm %lu dn %lu dr %lu  %s\r\n",n,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,(unsigned long)reply.image[205],(unsigned long)reply.image[206],(unsigned long)reply.image[207],key==1?"GOOD":(key==2?"POOR SYNC":"SYNC OK, BAD COLOUR / ODD GRADIENT"));
 }
 memset(&request,0,sizeof(request)); request.lab_flags=4194304UL;
 if(!call_lab()) logf2("LOCK final restore call failed: %s\r\n",status_text);
 logf2("LOCK TEST finished%s: good %d, poor sync %d, bad colour %d (restored).\r\n",abort_req?" (stopped early)":"",cnt[1],cnt[2],cnt[3]);
 sweeping=0; set_buttons(); sprintf(status_text,"Lock test finished: good %d, poor sync %d, bad colour %d. Send me VX2DVI.TXT.",cnt[1],cnt[2],cnt[3]); redraw();
}

/* Every standard mode the Dell lists in its EDID. Per mode: first the setting a simple timing model predicts; if that is not GOOD, adjust 13 / EDGE 1
   (proven at native). Every attempt is from scratch; answer each: GOOD / POOR SYNC / SYNC OK but BAD COLOUR. */
typedef struct {unsigned long var; const char *name; int adj,edge;} modeent;
static void do_modes(void) {
 static const modeent me[11]={
  {5,"monitor native 1680x1050 (control)",13,1},
  {6,"640x480 60 Hz (25.2 MHz)",12,0},
  {13,"720x400 70 Hz (28.3 MHz)",12,0},
  {7,"800x600 60 Hz (40 MHz)",12,0},
  {12,"800x600 75 Hz (49.5 MHz)",12,0},
  {8,"1024x768 60 Hz (65 MHz)",10,0},
  {10,"1024x768 75 Hz (78.75 MHz)",9,0},
  {14,"1152x864 75 Hz (108 MHz)",7,0},
  {9,"1280x1024 60 Hz (108 MHz)",7,0},
  {11,"1280x1024 75 Hz (135 MHz)",6,0},
  {1,"1280x720 60 Hz (74.25 MHz, not listed by the Dell)",9,0}};
 int m,tr,key=0,adj,edge; SYSTEMTIME t;
 if(!can_run) return;
 logf2("\r\nMODES TEST v1.12: all standard modes the Dell lists (needs VX85598.VXD v10.23). Per mode up to 2 settings, each = restore, 3 s without signal, apply from scratch. Answer each: GOOD / POOR SYNC / SYNC OK but BAD COLOUR. Give each about 15 s.\r\n");
 sweeping=1; set_buttons();
 for(m=0;m<11&&!abort_req;m++) for(tr=0;tr<2&&!abort_req;tr++) {
  unsigned long dx;
  if(tr==0) {adj=me[m].adj; edge=me[m].edge;} else {if(key==1||(me[m].adj==13&&me[m].edge==1)) break; adj=13; edge=1;}
  dx=(0xB7FUL&~0x10FUL)|(unsigned long)adj|((unsigned long)edge<<8);
  if(iga2_live) {
   sprintf(status_text,"MODES %d of 11 (%s): restoring, 3 s without signal ...",m+1,me[m].name); redraw();
   memset(&request,0,sizeof(request)); request.lab_flags=4194304UL;
   if(!call_lab()) {logf2("MODES restore call failed: %s\r\n",status_text); can_run=0; abort_req=1; break;}
   pump_wait(3000); if(abort_req) break;
  }
  memset(&request,0,sizeof(request)); request.lab_flags=2097152UL; request.lab_sy=me[m].var; request.lab_dx=dx;
  if(!call_lab()) {logf2("MODES apply call failed: %s\r\n",status_text); can_run=0; abort_req=1; break;}
  if(reply.result!=VX3D_PASS||reply.image[0]!=1) {logf2("MODES %2d  %s NOT applied: result %lu status %lu (4 = driver too old for this mode)\r\n",m+1,me[m].name,(unsigned long)reply.result,(unsigned long)reply.image[0]); key=0; break;}
  GetLocalTime(&t);
  sprintf(status_text,"MODES %d of 11: %s, DVP1 adjust %d, EDGE %d (%s setting).\r\nWatch the Dell about 15 s, then answer (keys 1 / 2 / 3 also work).",m+1,me[m].name,adj,edge,tr==0?"first":"second"); redraw(); MessageBeep(MB_OK);
  key=step_wait();
  if(abort_req) {logf2("MODES %d stopped before an answer\r\n",m+1); break;}
  logf2("MODES %2d  %-50s adj %2d edge %d  PLL dm %lu dn %lu dr %lu  %02u:%02u:%02u  %s\r\n",m+1,me[m].name,adj,edge,(unsigned long)reply.image[205],(unsigned long)reply.image[206],(unsigned long)reply.image[207],t.wHour,t.wMinute,t.wSecond,key==1?"GOOD":(key==2?"POOR SYNC":"SYNC OK, BAD COLOUR / ODD GRADIENT"));
 }
 memset(&request,0,sizeof(request)); request.lab_flags=4194304UL;
 if(!call_lab()) logf2("MODES final restore call failed: %s\r\n",status_text);
 logf2("MODES TEST finished%s (restored).\r\n",abort_req?" (stopped early)":"");
 sweeping=0; set_buttons(); sprintf(status_text,"Modes test finished. Send me VX2DVI.TXT."); redraw();
}

/* One from-scratch attempt (restore, 3 s without signal, apply) at a mode with given DVP1 adjust / EDGE and VT1632 filter / deskew values.
   Returns the answer 1/2/3, or 0 when stopped or failed. */
static int fattempt(unsigned long var,const char *name,int adj,int edge,int filt,int desk,const char *tag,int n,int total) {
 unsigned long dx=(0xB7FUL&~0x10FUL)|(unsigned long)adj|((unsigned long)edge<<8)|((unsigned long)filt<<19)|((unsigned long)desk<<23)|fextra;
 SYSTEMTIME t; int key;
 if(abort_req) return 0;
 if(iga2_live) {
  sprintf(status_text,"FILTER test %d of up to %d: restoring, 3 s without signal ...",n,total); redraw();
  memset(&request,0,sizeof(request)); request.lab_flags=4194304UL;
  if(!call_lab()) {logf2("FILTER restore call failed: %s\r\n",status_text); can_run=0; abort_req=1; return 0;}
  pump_wait(3000); if(abort_req) return 0;
 }
 memset(&request,0,sizeof(request)); request.lab_flags=2097152UL; request.lab_sy=var; request.lab_dx=dx;
 if(!call_lab()) {logf2("FILTER apply call failed: %s\r\n",status_text); can_run=0; abort_req=1; return 0;}
 if(reply.result!=VX3D_PASS||reply.image[0]!=1) {logf2("FILTER %s NOT applied: result %lu status %lu (4 = driver too old)\r\n",name,(unsigned long)reply.result,(unsigned long)reply.image[0]); abort_req=1; return 0;}
 GetLocalTime(&t);
 sprintf(status_text,"FILTER test %d: %s, adjust %d, EDGE %d, VT1632 filter step %d, deskew step %d (%s).\r\nGive the Dell about 15 s to lock, then answer (keys 1 / 2 / 3 also work).",n,name,adj,edge,filt,desk,tag); redraw(); MessageBeep(MB_OK);
 key=step_wait();
 if(abort_req) {logf2("FILTER %d stopped before an answer\r\n",n); return 0;}
 logf2("FILTER %2d  %-28s adj %2d edge %d  filter %d (reg0C %02lX)  deskew %d (reg0A %02lX)  extra %08lX  PLL dm %lu dn %lu dr %lu  %02u:%02u:%02u  %s  [%s]\r\n",n,name,adj,edge,filt,(unsigned long)reply.image[210],desk,(unsigned long)reply.image[209],fextra,(unsigned long)reply.image[205],(unsigned long)reply.image[206],(unsigned long)reply.image[207],t.wHour,t.wMinute,t.wSecond,key==1?"GOOD":(key==2?"POOR SYNC":"SYNC OK, BAD COLOUR / ODD GRADIENT"),tag);
 return key;
}
/* Low pixel clocks lose sync completely. Hypothesis: the VT1632's PLL filter (reg 0C) / deskew (reg 0A) need setting. Pass 1: filter steps 0..8 at 800x600 75 Hz (49.5 MHz).
   Pass 2 (only if no GOOD): deskew steps 1..8. If something is GOOD it is confirmed on 800x600 60 Hz, 640x480 and 720x400. */
static void do_filter(void) {
 int f,d,key,n=0,good=-1,lockf=-1,gooddesk=0,usef,used,i;
 static const struct {unsigned long var; const char *name;} ck[3]={{7,"800x600 60 Hz (40 MHz)"},{6,"640x480 60 Hz (25.2 MHz)"},{13,"720x400 70 Hz (28.3 MHz)"}};
 if(!can_run) return;
 logf2("\r\nFILTER TEST v1.12 (needs VX85598.VXD v10.23): VT1632 filter (reg 0C) and deskew (reg 0A) at low pixel clocks. Answer each: GOOD / POOR SYNC / SYNC OK but BAD COLOUR. Give each about 15 s.\r\n");
 sweeping=1; set_buttons();
 for(f=0;f<=8&&!abort_req;f++) {
  key=fattempt(12UL,"800x600 75 Hz (49.5 MHz)",13,1,f,0,"pass 1: filter",++n,30);
  if(key==1) {good=f; break;}
  if(key==3&&lockf<0) lockf=f;
 }
 usef=(good>=0)?good:(lockf>=0?lockf:0); used=0;
 if(good<0&&!abort_req) for(d=1;d<=8&&!abort_req;d++) {
  key=fattempt(12UL,"800x600 75 Hz (49.5 MHz)",13,1,usef,d,"pass 2: deskew",++n,30);
  if(key==1) {gooddesk=d; break;}
 }
 used=gooddesk;
 if((good>=0||gooddesk>0)&&!abort_req) for(i=0;i<3&&!abort_req;i++) fattempt(ck[i].var,ck[i].name,13,1,usef,used,"confirm",++n,30);
 memset(&request,0,sizeof(request)); request.lab_flags=4194304UL;
 if(!call_lab()) logf2("FILTER final restore call failed: %s\r\n",status_text);
 logf2("FILTER TEST finished%s (restored): good filter %d, lock-only filter %d, good deskew %d.\r\n",abort_req?" (stopped early)":"",good,lockf,gooddesk);
 sweeping=0; set_buttons(); sprintf(status_text,"Filter test finished. Send me VX2DVI.TXT."); redraw();
}

/* Second filter test. Found so far: reg 0C = 00 works at 65 MHz and above; reg 0C = 01 (filter enabled, step 0) works at 40 and 49.5 MHz but not at 25.2 / 28.3 MHz.
   Part A: 640x480 with filter steps 2..8. Part B: confirm 720x400 with what worked. Part C: survey whether filter step 1 also works at higher clocks. */
static void do_filter2(void) {
 int f,key,n=0,good=-1,lockf=-1,i,usef;
 static const struct {unsigned long var; const char *name; int filt;} sv[5]={
  {8,"1024x768 60 Hz (65 MHz)",1},{10,"1024x768 75 Hz (78.75 MHz)",1},{1,"1280x720 60 Hz (74.25 MHz)",1},{5,"monitor native 1680x1050",1},{11,"1280x1024 75 Hz (135 MHz)",0}};
 if(!can_run) return;
 logf2("\r\nFILTER TEST 2 v1.12 (needs VX85598.VXD v10.23): lowest clocks, then a survey at higher clocks. Answer each: GOOD / POOR SYNC / SYNC OK but BAD COLOUR. Give each about 15 s.\r\n");
 sweeping=1; set_buttons();
 for(f=2;f<=8&&!abort_req;f++) {
  key=fattempt(6UL,"640x480 60 Hz (25.2 MHz)",13,1,f,0,"part A: filter",++n,16);
  if(key==1) {good=f; break;}
  if(key==3&&lockf<0) lockf=f;
 }
 if(good<0&&lockf>=0&&!abort_req) {
  static const int ad[3]={12,8,4};
  for(i=0;i<3&&!abort_req;i++) {key=fattempt(6UL,"640x480 60 Hz (25.2 MHz)",ad[i],0,lockf,0,"part A: phase",++n,16); if(key==1) {good=lockf; break;}}
 }
 usef=good;
 if(usef>=0&&!abort_req) fattempt(13UL,"720x400 70 Hz (28.3 MHz)",13,1,usef,0,"part B: confirm",++n,16);
 for(i=0;i<5&&!abort_req;i++) fattempt(sv[i].var,sv[i].name,13,1,sv[i].filt,0,sv[i].filt?"part C: filter step 1":"part C: filter 0",++n,16);
 memset(&request,0,sizeof(request)); request.lab_flags=4194304UL;
 if(!call_lab()) logf2("FILTER2 final restore call failed: %s\r\n",status_text);
 logf2("FILTER TEST 2 finished%s (restored): filter for 640x480 = %d, lock-only filter = %d.\r\n",abort_req?" (stopped early)":"",good,lockf);
 sweeping=0; set_buttons(); sprintf(status_text,"Filter test 2 finished. Send me VX2DVI.TXT."); redraw();
}

/* Third test: 25.2 / 28.3 MHz never lock with any filter step. The BIOS drives 25 MHz with dtz = 3 and dn = 5, we use dtz = 1 and dn = 7.
   Try those at 640x480, with filter 0 and 1; if one is GOOD, confirm 720x400. */
static void do_filter3(void) {
 static const struct {unsigned long extra; int filt; const char *tag;} v3[6]={
  {1UL<<14,0,"dtz 3, filter 0"},{1UL<<14,1,"dtz 3, filter 1"},
  {(1UL<<14)|(5UL<<27),0,"dtz 3, dn 5, filter 0"},{(1UL<<14)|(5UL<<27),1,"dtz 3, dn 5, filter 1"},
  {5UL<<27,0,"dtz 1, dn 5, filter 0"},{5UL<<27,1,"dtz 1, dn 5, filter 1"}};
 int i,key,n=0,gi=-1;
 if(!can_run) return;
 logf2("\r\nFILTER TEST 3 v1.12 (needs VX85598.VXD v10.23): 640x480 (25.2 MHz) with the BIOS-style PLL settings. Answer each: GOOD / POOR SYNC / SYNC OK but BAD COLOUR. Give each about 15 s.\r\n");
 sweeping=1; set_buttons();
 for(i=0;i<6&&!abort_req;i++) {
  fextra=v3[i].extra;
  key=fattempt(6UL,"640x480 60 Hz (25.2 MHz)",13,1,v3[i].filt,0,v3[i].tag,++n,8);
  if(key==1) {gi=i; break;}
 }
 if(gi>=0&&!abort_req) {fextra=v3[gi].extra; fattempt(13UL,"720x400 70 Hz (28.3 MHz)",13,1,v3[gi].filt,0,"confirm",++n,8);}
 fextra=0;
 memset(&request,0,sizeof(request)); request.lab_flags=4194304UL;
 if(!call_lab()) logf2("FILTER3 final restore call failed: %s\r\n",status_text);
 logf2("FILTER TEST 3 finished%s (restored): good combination = %s.\r\n",abort_req?" (stopped early)":"",gi>=0?v3[gi].tag:"none");
 sweeping=0; set_buttons(); sprintf(status_text,"Filter test 3 finished. Send me VX2DVI.TXT."); redraw();
}

/* Fourth test: the 50..78 MHz range. 1024x768 60 Hz (65 MHz) and 720p (74.25 MHz) are good with the filter off on one monitor and not on the other.
   Every filter step 0..8 (reg 0C = 00, 01, 03 ... 0F) at each of the two modes. */
static void do_filter4(void) {
 static const struct {unsigned long var; const char *name;} md[2]={{8,"1024x768 60 Hz (65 MHz)"},{1,"1280x720 60 Hz (74.25 MHz)"}};
 int m,f,key,n=0; int ok[2][9];
 if(!can_run) return;
 for(m=0;m<2;m++) for(f=0;f<9;f++) ok[m][f]=0;
 logf2("\r\nFILTER TEST 4 v1.12 (needs VX85598.VXD v10.23): filter steps 0..8 at 65 and 74.25 MHz. Answer each: GOOD / POOR SYNC / SYNC OK but BAD COLOUR. Give each about 15 s.\r\n");
 sweeping=1; set_buttons();
 for(m=0;m<2&&!abort_req;m++) for(f=0;f<9&&!abort_req;f++) {
  key=fattempt(md[m].var,md[m].name,13,1,f,0,"filter sweep",++n,18);
  if(key>0) ok[m][f]=key;
 }
 memset(&request,0,sizeof(request)); request.lab_flags=4194304UL;
 if(!call_lab()) logf2("FILTER4 final restore call failed: %s\r\n",status_text);
 logf2("FILTER TEST 4 finished%s (restored). Summary (G good, s poor sync, c bad colour, . not tested):\r\n",abort_req?" (stopped early)":"");
 for(m=0;m<2;m++) {logf2("  %-30s",md[m].name); for(f=0;f<9;f++) logf2(" step%d=%c",f,ok[m][f]==1?'G':(ok[m][f]==2?'s':(ok[m][f]==3?'c':'.'))); logtext("\r\n");}
 sweeping=0; set_buttons(); sprintf(status_text,"Filter test 4 finished. Send me VX2DVI.TXT."); redraw();
}

static void begin_op(void) {busy=1; abort_req=0; set_buttons();}
static void end_op(HWND w) {busy=0; set_buttons(); log_flush(); if(closing) DestroyWindow(w);}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
 PAINTSTRUCT ps; HDC dc; RECT rc;
 switch(msg) {
 case WM_CREATE:
  hmain=w;
  font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
  hb_route=CreateWindow("BUTTON","ROUTING",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,8,8,70,26,w,(HMENU)ID_ROUTE,GetModuleHandle(NULL),NULL);
  hb_dvi=CreateWindow("BUTTON","DVI TEST",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,82,8,70,26,w,(HMENU)ID_DVI,GetModuleHandle(NULL),NULL);
  hb_tune=CreateWindow("BUTTON","DVI TUNE",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,156,8,70,26,w,(HMENU)ID_TUNE,GetModuleHandle(NULL),NULL);
  hb_sw1=CreateWindow("BUTTON","SWEEP 720P",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,230,8,86,26,w,(HMENU)ID_SW1,GetModuleHandle(NULL),NULL);
  hb_sw2=CreateWindow("BUTTON","SWEEP NATIVE",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,320,8,96,26,w,(HMENU)ID_SW2,GetModuleHandle(NULL),NULL);
  SendMessage(hb_sw1,WM_SETFONT,(WPARAM)font,TRUE); SendMessage(hb_sw2,WM_SETFONT,(WPARAM)font,TRUE);
  hb_lk1=CreateWindow("BUTTON","LOCK 720P",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,420,8,80,26,w,(HMENU)ID_LK1,GetModuleHandle(NULL),NULL);
  hb_lk2=CreateWindow("BUTTON","LOCK NATIVE",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,504,8,96,26,w,(HMENU)ID_LK2,GetModuleHandle(NULL),NULL);
  SendMessage(hb_lk1,WM_SETFONT,(WPARAM)font,TRUE); SendMessage(hb_lk2,WM_SETFONT,(WPARAM)font,TRUE);
  hb_lk3=CreateWindow("BUTTON","LOCK 1080P",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,8,80,96,26,w,(HMENU)ID_LK3,GetModuleHandle(NULL),NULL);
  SendMessage(hb_lk3,WM_SETFONT,(WPARAM)font,TRUE);
  hb_modes=CreateWindow("BUTTON","MODES",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,112,80,80,26,w,(HMENU)ID_MODES,GetModuleHandle(NULL),NULL);
  SendMessage(hb_modes,WM_SETFONT,(WPARAM)font,TRUE);
  hb_filt=CreateWindow("BUTTON","FILTER",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,196,80,80,26,w,(HMENU)ID_FILT,GetModuleHandle(NULL),NULL);
  SendMessage(hb_filt,WM_SETFONT,(WPARAM)font,TRUE);
  hb_fil2=CreateWindow("BUTTON","FILTER 2",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,280,80,80,26,w,(HMENU)ID_FIL2,GetModuleHandle(NULL),NULL);
  SendMessage(hb_fil2,WM_SETFONT,(WPARAM)font,TRUE);
  hb_fil3=CreateWindow("BUTTON","FILTER 3",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,364,80,80,26,w,(HMENU)ID_FIL3,GetModuleHandle(NULL),NULL);
  SendMessage(hb_fil3,WM_SETFONT,(WPARAM)font,TRUE);
  hb_fil4=CreateWindow("BUTTON","FILTER 4",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,448,80,80,26,w,(HMENU)ID_FIL4,GetModuleHandle(NULL),NULL);
  SendMessage(hb_fil4,WM_SETFONT,(WPARAM)font,TRUE);
  hb_a1=CreateWindow("BUTTON","1  GOOD",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,8,42,150,32,w,(HMENU)ID_A1,GetModuleHandle(NULL),NULL);
  hb_a2=CreateWindow("BUTTON","2  POOR SYNC",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,166,42,150,32,w,(HMENU)ID_A2,GetModuleHandle(NULL),NULL);
  hb_a3=CreateWindow("BUTTON","3  SYNC OK, BAD COLOUR",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,324,42,200,32,w,(HMENU)ID_A3,GetModuleHandle(NULL),NULL);
  hb_stop=CreateWindow("BUTTON","STOP",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,532,42,80,32,w,(HMENU)ID_STOP,GetModuleHandle(NULL),NULL);
  SendMessage(hb_a1,WM_SETFONT,(WPARAM)font,TRUE); SendMessage(hb_a2,WM_SETFONT,(WPARAM)font,TRUE); SendMessage(hb_a3,WM_SETFONT,(WPARAM)font,TRUE); SendMessage(hb_stop,WM_SETFONT,(WPARAM)font,TRUE);
  set_buttons();
  SendMessage(hb_route,WM_SETFONT,(WPARAM)font,TRUE); SendMessage(hb_dvi,WM_SETFONT,(WPARAM)font,TRUE); SendMessage(hb_tune,WM_SETFONT,(WPARAM)font,TRUE);
  open_device();
  return 0;
 case WM_COMMAND:
  if(HIWORD(wp)==BN_CLICKED && sweeping) {
   if(LOWORD(wp)==ID_A1) answer=1; else if(LOWORD(wp)==ID_A2) answer=2; else if(LOWORD(wp)==ID_A3) answer=3; else if(LOWORD(wp)==ID_STOP) abort_req=1;
   return 0;
  }
  if(HIWORD(wp)==BN_CLICKED && !busy) {
   if(LOWORD(wp)==ID_ROUTE && !tune_on) {begin_op(); do_routing(); end_op(w);}
   else if(LOWORD(wp)==ID_DVI && !tune_on) {begin_op(); do_dvitest(); end_op(w);}
   else if(LOWORD(wp)==ID_LK1 && !tune_on) {begin_op(); do_lock(1UL); end_op(w);}
   else if(LOWORD(wp)==ID_FIL4 && !tune_on) {begin_op(); do_filter4(); end_op(w);}
   else if(LOWORD(wp)==ID_FIL3 && !tune_on) {begin_op(); do_filter3(); end_op(w);}
   else if(LOWORD(wp)==ID_FIL2 && !tune_on) {begin_op(); do_filter2(); end_op(w);}
   else if(LOWORD(wp)==ID_FILT && !tune_on) {begin_op(); do_filter(); end_op(w);}
   else if(LOWORD(wp)==ID_MODES && !tune_on) {begin_op(); do_modes(); end_op(w);}
   else if(LOWORD(wp)==ID_LK3 && !tune_on) {begin_op(); do_lock(2UL); end_op(w);}
   else if(LOWORD(wp)==ID_LK2 && !tune_on) {begin_op(); do_lock(5UL); end_op(w);}
   else if(LOWORD(wp)==ID_SW1 && !tune_on) {begin_op(); do_sweep(1UL); end_op(w);}
   else if(LOWORD(wp)==ID_SW2 && !tune_on) {begin_op(); do_sweep(5UL); end_op(w);}
   else if(LOWORD(wp)==ID_TUNE) {begin_op(); do_tune(); busy=0; set_buttons(); log_flush();}
  }
  return 0;
 case WM_PAINT:
  dc=BeginPaint(w,&ps);
  SelectObject(dc,font); SetBkMode(dc,TRANSPARENT);
  rc.left=8; rc.top=116; rc.right=632; rc.bottom=400;
  DrawText(dc,status_text,-1,&rc,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
  EndPaint(w,&ps); return 0;
 case WM_CLOSE:
  if(busy) {closing=1; abort_req=1; sprintf(status_text,"Closing: aborting the running test and restoring ..."); redraw(); return 0;}
  tune_stop();
  DestroyWindow(w); return 0;
 case WM_DESTROY:
  hmain=NULL; PostQuitMessage(0); return 0;
 }
 return DefWindowProc(w,msg,wp,lp);
}
int WINAPI WinMain(HINSTANCE inst,HINSTANCE prev,LPSTR cmd,int show) {
 WNDCLASS wc; MSG m; HWND w; char name[MAX_PATH],*p;
 (void)prev;(void)cmd;
 GetModuleFileName(NULL,name,sizeof(name)); p=strrchr(name,'\\'); if(p) strcpy(p+1,"VX2DVI.TXT"); else strcpy(name,"VX2DVI.TXT");
 logfile=CreateFile(name,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(logfile!=INVALID_HANDLE_VALUE) {SetFilePointer(logfile,0,NULL,FILE_END); logtext("\r\n==== VX2DVI session (v1.12, uses VX85598.VXD v10.23) ====\r\n"); log_flush();}
 memset(&wc,0,sizeof(wc));
 wc.lpfnWndProc=proc; wc.hInstance=inst; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
 wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1); wc.lpszClassName="VX2DVI";
 RegisterClass(&wc);
 w=CreateWindow("VX2DVI","VX855 dual-output lab (VX2DVI v1.12)",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_VISIBLE,4,4,640,420,NULL,NULL,inst,NULL);
 if(!w) return 1;
 ShowWindow(w,show); UpdateWindow(w);
 while(GetMessage(&m,NULL,0,0)>0) {
  if(m.message==WM_KEYDOWN && !busy && tune_on) {tune_key(m.wParam); continue;}
  TranslateMessage(&m); DispatchMessage(&m);
 }
 /* belt and braces: never leave IGA2/DVP1 reprogrammed */
 if(iga2_live&&device!=INVALID_HANDLE_VALUE) {memset(&request,0,sizeof(request)); request.lab_flags=4194304UL; (void)call_lab(); logtext("Exit: IGA2 restored.\r\n");}
 log_flush();
 if(device!=INVALID_HANDLE_VALUE) CloseHandle(device);
 if(logfile!=INVALID_HANDLE_VALUE) CloseHandle(logfile);
 ExitProcess(0);
 return 0;
}
