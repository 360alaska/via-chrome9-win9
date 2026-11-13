/* Win98 native VX855 test client. Copyright (c) 2026, MIT. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "vx85598.h"
#include "vx855_3d.h"
#define UI_FIRST VX3D_SF_PROBE
#define UI_COUNT 1

static HANDLE device=INVALID_HANDLE_VALUE,logfile=INVALID_HANDLE_VALUE,binary=INVALID_HANDLE_VALUE;
static char base[MAX_PATH],message[1024];
static vx3d_report reply;
static int have_image, can_run;
static int log_failed;
static DWORD capture_error;
static DWORD sheet[VX3D_PIXELS*UI_COUNT];
static char rows[UI_COUNT][100];
static const char *probe_names[UI_COUNT]={
 "32 cases: v9.4 matrix plus v9.7 fills and v9.8 desktop fills"
};
static const char *short_names[UI_COUNT]={"SF triangle"};
static unsigned long sel_arg;
static const char *matrix_names[VX3D_MATRIX_CASES]={
 "startup FE: three cyan point control",
 "v9.61 native: shallow triangle, unclipped (4,12) (60,20) (20,60)",
 "v9.61 control: three cyan points",
 "v9.61 native: same shallow triangle, reversed winding",
 "v9.61 control: three cyan points",
 "v9.61 native: left-clipped (-8,10) (40,20) (20,60)",
 "v9.61 native: LINE (8,8)-(56,8), two vertices",
 "v9.61 native: top-clipped (10,-8) (60,20) (20,60)",
 "v9.61 control: three cyan points",
 "v9.61 native: GOURAUD triangle, red/green/blue vertices, captured state",
 "v9.61 control: three cyan points",
 "v9.61 native: right-clipped (10,10) (72,24) (20,56), red",
 "v9.61 native: clipped RECTANGLE, three vertices (-8,10) (40,10) (-8,40)",
 "v9.61 native: bottom-clipped (8,10) (56,20) (30,72)",
 "v9.61 final control: three cyan points",
 "v9.7 control: three cyan points (before fills)",
 "v9.61 texture: ARGB nearest 1:1 repeat",
 "v9.61 texture: ARGB nearest 2x magnified",
 "v9.61 texture: ARGB bilinear 1:1",
 "v9.61 texture: ARGB bilinear 2x magnified",
 "v9.61 texture: ARGB nearest clamp (far outside)",
 "v9.61 texture: ARGB nearest mirror",
 "v9.61 texture: L8 format nearest",
 "v9.61 texture: ABGR 32bpp format",
 "v9.61 texture: BGRA 32bpp format",
 "v9.61 texture: alpha texel + SRCALPHA/INVSRCALPHA blend",
 "v9.7 control: three cyan points (after fills)",
 "v9.61 2D engine: mono source expansion (bit 8, bitmap rows 52..59)",
 "v9.61 NATIVE desktop: red rectangle (520,40)-(600,100), window saved and restored",
 "v9.61 NATIVE desktop: GREEN triangle (520,120) (620,180) (520,200), expect FF00FF00",
 "v9.61 NATIVE desktop: BLUE rectangle (587,433)-(633,475), inside the screen, expect FF0000FF",
 "v9.8 final control: three cyan points"
};
static const char *names[]={
 "not run", "PASS: this probe",
 "desktop/VM/framebuffer not ready", "invalid MMIO BAR", "MMIO mapping failed",
 "engine already busy or unreadable", "engine timeout", "writes outside target/depth buffers",
 "solid triangle readback failed", "Gouraud triangle readback failed",
 "Z-buffer comparison failed", "disabled until reboot", "invalid request",
 "engine controls/PLL could not be established or restored", "GPU 2D framebuffer-write check failed",
 "control completion unconfirmed (not a busy timeout)",
 "fetched command execution unconfirmed",
 "SF memory mapping/GTI flush failed", "SF pause/retirement not confirmed"
};
static const char *result_name(DWORD n) {
 return n<sizeof(names)/sizeof(names[0])?names[n]:"unknown result";
}
/* Every formatted destination has an explicit capacity. v6.8's matrix
 * status line overran log_engine_details' 256-byte stack buffer. */
static void textfmt(char *destination,size_t capacity,const char *format,...) {
 va_list args; int n;
 if(!capacity) return;
 va_start(args,format); n=vsnprintf(destination,capacity,format,args); va_end(args);
 destination[capacity-1]=0;
 if(n<0 || (size_t)n>=capacity) log_failed=1;
}
static int logtext(const char *s) {
 DWORD n,bytes=(DWORD)strlen(s); int ok;
 ok=logfile!=INVALID_HANDLE_VALUE && WriteFile(logfile,s,bytes,&n,NULL) && n==bytes;
 if(!ok) log_failed=1;
 return ok;
}
static char vprefix[8];
static char connect_text[700];
static int open_log(void) {
 char directory[MAX_PATH],name[MAX_PATH],binname[MAX_PATH],*p; unsigned int n;
 if(!GetModuleFileName(NULL,directory,sizeof(directory))) return 0;
 p=strrchr(directory,'\\'); if(!p) return 0; p[1]=0;
 if(strlen(directory)>MAX_PATH-20) return 0;
 for(n=0;n<1000;n++) {
  textfmt(base,sizeof(base),"%sVX3D%s%03u",directory,vprefix,n); textfmt(name,sizeof(name),"%s.TXT",base);
  textfmt(binname,sizeof(binname),"%s.BIN",base);
  logfile=CreateFile(name,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
  if(logfile!=INVALID_HANDLE_VALUE) {
   binary=CreateFile(binname,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
   if(binary!=INVALID_HANDLE_VALUE) return 1;
   capture_error=GetLastError();
   CloseHandle(logfile); logfile=INVALID_HANDLE_VALUE;
   /* This empty TXT was created by this invocation. Existing BIN/TXT
    * captures are never overwritten or removed. Reserve a fresh pair. */
   DeleteFile(name);
   if(capture_error!=ERROR_FILE_EXISTS && capture_error!=ERROR_ALREADY_EXISTS) return 0;
  } else {
   capture_error=GetLastError();
   if(capture_error!=ERROR_FILE_EXISTS && capture_error!=ERROR_ALREADY_EXISTS) return 0;
  }
 }
 return 0;
}
static int write_all(HANDLE f,const void *data,DWORD bytes) {
 DWORD n; return WriteFile(f,data,bytes,&n,NULL) && n==bytes;
}
static int append_capture(void) {
 return binary!=INVALID_HANDLE_VALUE && write_all(binary,&reply,sizeof(reply)) && FlushFileBuffers(binary);
}
static void save_sheet(void) {
 char name[MAX_PATH]; HANDLE f; BITMAPFILEHEADER bf; BITMAPINFOHEADER bi;
 unsigned int y; int ok;
 if(!have_image) return;
 memset(&bf,0,sizeof(bf)); memset(&bi,0,sizeof(bi));
 bf.bfType=0x4D42; bf.bfOffBits=sizeof(bf)+sizeof(bi);
 bf.bfSize=bf.bfOffBits+sizeof(sheet);
 bi.biSize=sizeof(bi); bi.biWidth=VX3D_SIDE*UI_COUNT; bi.biHeight=VX3D_SIDE;
 bi.biPlanes=1; bi.biBitCount=32; bi.biCompression=BI_RGB;
 bi.biSizeImage=sizeof(sheet);
 textfmt(name,sizeof(name),"%s.BMP",base);
 f=CreateFile(name,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
 ok=f!=INVALID_HANDLE_VALUE;
 if(ok) {
  ok=write_all(f,&bf,sizeof(bf)) && write_all(f,&bi,sizeof(bi));
  for(y=VX3D_SIDE;y>0 && ok;y--) ok=write_all(f,sheet+(y-1)*VX3D_SIDE*UI_COUNT,VX3D_SIDE*4*UI_COUNT);
  CloseHandle(f);
 }
 if(!ok) logtext("Contact sheet write failed. Images remain in BIN records.\r\n");
}
static int call(DWORD code,const void *input,DWORD bytes) {
 DWORD got=0,error;
 memset(&reply,0,sizeof(reply)); SetLastError(0);
 if(!DeviceIoControl(device,code,(void*)input,bytes,&reply,sizeof(reply),&got,NULL)) {
  error=GetLastError();
  textfmt(message,sizeof(message),"Driver request %08lX failed: Windows error %lu.\r\n",code,error);
  logtext(message); return 0;
 }
 if(got!=sizeof(reply) || reply.magic!=VX3D_MAGIC ||
    reply.cb!=sizeof(reply) || reply.version!=VX3D_VERSION) {
  textfmt(message,sizeof(message),"Invalid native-test reply: %lu bytes.\r\n",got);
  logtext(message); return 0;
 }
 if(reply.build!=VX3D_BUILD || reply.producer!=VX3D_PRODUCER) {
  textfmt(message,sizeof(message),"Driver build %08lX does not match test build %08lX.\r\nCopy both VX85598.VXD and VX3DTEST.EXE from the same package, then reboot.\r\n",reply.build,(DWORD)VX3D_BUILD);
  logtext(message); return 0;
 }
 return 1;
}
static void log_engine_details(void) {
 static const char *reg[VX3D_SEQ_COUNT]={"SR10","SR1A","SR1E","SR2D","SR3F","SR4E","SR3C"};
 static const char *phase[VX3D_PHASES]={"CR","2D","FE","state","acceptance","draw","post-draw","cleanup"};
 static const char *tag[3]={"CR T11AA","VP T04AA","general T01AA"};
 static const char *state[VX3D_STATE_READS]={"FE00","T01A01","RT base","RT format/pitch","RT mask/ROP","FVF length","FVF XYZ","FVF W/color","input mask","output mask","texture count","user clip planes","color window TB","scissor LR","far clip","X scale low","Y high bytes","Z scale/area"};
 char line[1024];unsigned int i;
 textfmt(line,sizeof(line),"Driver build %08lX; init flags %08lX; PLL polls %lu.\r\n",
  reply.build,reply.init_flags,reply.pll_polls);logtext(line);
 textfmt(line,sizeof(line),"Producer %08lX; failed wait reason %lu; failed wait status %08lX (reason 0 means no wait failed).\r\n",
  reply.producer,reply.wait_failure_reason,reply.wait_failure_status);logtext(line);
 logtext("Sequencer: before / active / restored. Restore flags determine whether restoration ran; zero can be a legitimate register value.\r\n");
 for(i=0;i<VX3D_SEQ_COUNT;i++) {
  textfmt(line,sizeof(line),"%s: %02lX / %02lX / %02lX\r\n",reg[i],reply.seq_before[i],reply.seq_active[i],reply.seq_after[i]);logtext(line);
 }
 textfmt(line,sizeof(line),"2D check: errors %lu, changed %lu, sample %08lX.\r\n",
  reply.blit_errors,reply.blit_changed_pixels,reply.blit_sample);logtext(line);
 textfmt(line,sizeof(line),"Status OR %08lX; 2D %08lX; setup %08lX; solid %08lX.\r\n",
  reply.status_or,reply.status_blit,reply.status_setup,reply.status_solid);logtext(line);
 textfmt(line,sizeof(line),"Probe %lu; isolated %lu; display [%08lX,%08lX); desktop copies %lu -> %lu; held attempts %lu -> %lu.\r\n",
  reply.probe_id,reply.display_isolated,reply.display_begin,reply.display_end,
  reply.display_draws_before,reply.display_draws_after,reply.display_skips_before,reply.display_skips_after);logtext(line);
 textfmt(line,sizeof(line),"Vertex words %lu; vertices %lu; VB stride %lu; checks %08lX (1=2D, 2=solid, 4=Gouraud, 8=depth, 16=point, 32=line, 64=depth only).\r\n",
  reply.vertex_words,reply.vertex_vertices,reply.vertex_stride,reply.checks_run);logtext(line);
 logtext("Init flags: 1 snapshot, 2 enabled, 4 restored, 8 held after unsafe completion, 16 controls mismatch, 32 reset held, 64 programmed, 128 restore mismatch, 256 PLL unlock, 512 FE submitted, 1024 no-texture/clip clear mismatch.\r\n");
 logtext("Phase status is sampled OR, not a completion fence. A missing busy bit does not prove inactivity.\r\n");
 for(i=0;i<VX3D_PHASES;i++) {
  textfmt(line,sizeof(line),"Activity %s: %08lX.\r\n",phase[i],reply.phase_status[i]);logtext(line);
 }
 if(reply.path_flags&16) {
  textfmt(line,sizeof(line),"General state original 00..03: %08lX %08lX %08lX %08lX; 04..07: %08lX %08lX %08lX %08lX.\r\n",reply.general_before[0],reply.general_before[1],reply.general_before[2],reply.general_before[3],reply.general_before[4],reply.general_before[5],reply.general_before[6],reply.general_before[7]);logtext(line);
  textfmt(line,sizeof(line),"General state active   00..03: %08lX %08lX %08lX %08lX; 04..07: %08lX %08lX %08lX %08lX.\r\n",reply.general_after[0],reply.general_after[1],reply.general_after[2],reply.general_after[3],reply.general_after[4],reply.general_after[5],reply.general_after[6],reply.general_after[7]);logtext(line);
 }
 if(reply.path_flags&8) {
  textfmt(line,sizeof(line),"Path flags %lX; acceptance %lX; state matches %05lX/3FFFF. Aligned register lane AND index tag must match.\r\n",reply.path_flags,reply.acceptance,reply.state_matches);logtext(line);
  logtext("Acceptance bits: 1 fence A+B, 2 CR tag A+B, 4 VP tag A+B, 8 general tag A+B, 16 post-draw fence, 32 fence restored. Pixel checks still decide rendering PASS.\r\n");
  textfmt(line,sizeof(line),"Fence original %08lX; A %08lX; B %08lX; post %08lX expected %08lX; restored %08lX.\r\n",reply.fence[0],reply.fence[1],reply.fence[2],reply.fence[3],reply.fence[5],reply.fence[4]);logtext(line);
  for(i=0;i<3;i++) {
   textfmt(line,sizeof(line),"Tag %s: original %08lX; A %08lX; B %08lX; restored %08lX.\r\n",tag[i],reply.marker[i][0],reply.marker[i][1],reply.marker[i][2],reply.marker[i][3]);logtext(line);
  }
  for(i=0;i<VX3D_STATE_READS;i++) {
   textfmt(line,sizeof(line),"Raw state %s: %08lX %08lX %08lX %08lX.\r\n",state[i],reply.state_readback[i][0],reply.state_readback[i][1],reply.state_readback[i][2],reply.state_readback[i][3]);logtext(line);
  }
  for(i=0;i<3;i++) {
   textfmt(line,sizeof(line),"Occlusion %s: %08lX %08lX %08lX %08lX.\r\n",i==0?"original":i==1?"cleared":"post-draw",reply.occlusion[i][0],reply.occlusion[i][1],reply.occlusion[i][2],reply.occlusion[i][3]);logtext(line);
  }
  if(reply.checks_run&(8|64)) {
   textfmt(line,sizeof(line),"Depth pixels changed %lu; raw depth sample %08lX.\r\n",reply.depth_changed_pixels,reply.depth_sample);logtext(line);
  } else logtext("Depth was not measured by this probe.\r\n");
 }
 if(reply.path_flags&512) {
  textfmt(line,sizeof(line),"Queue control %08lX; start %08lX; exclusive end %08lX; length %08lX; backed bytes %lu.\r\n",reply.queue_control,reply.queue_start,reply.queue_end,reply.queue_length,reply.queue_storage_bytes);logtext(line);
  textfmt(line,sizeof(line),"Queue flags %08lX; phase %lu; guard errors %lu; changed queue DWORDs %lu. Queue writes alone do not prove rendering.\r\n",reply.queue_flags,reply.queue_phase,reply.queue_guard_errors,reply.queue_changed_words);logtext(line);
  logtext("Queue flags: 1 raw snapshot, 2 init issued, 4 enable submitted, 8 OFF/idle, 16 known OFF baseline, 32 memory restored, 64 raw mismatch, 128 bad raw tag, 512 shutdown contradiction, 1024 WO observations, 2048 completion unconfirmed, 4096 bank responded.\r\n");
  for(i=0;i<8;i++) {
   textfmt(line,sizeof(line),"CR %02X: original %08lX; active %08lX; disabled %08lX; final %08lX.\r\n",0x70+i,reply.queue_original[i],reply.queue_active[i],reply.queue_disabled[i],reply.queue_restored[i]);logtext(line);
  }
  textfmt(line,sizeof(line),"Other queue raw: %08lX %08lX %08lX %08lX. Queue controls are write-only; raw values are observational.\r\n",reply.queue_other_original[0],reply.queue_other_original[1],reply.queue_other_original[2],reply.queue_other_original[3]);logtext(line);
  textfmt(line,sizeof(line),"Queue fence original %08lX; claim %08lX; shutdown %08lX; restored %08lX. These prove CR traffic, not triangle rendering.\r\n",reply.queue_fence_original,reply.queue_claim_fence,reply.queue_disable_fence,reply.queue_fence_restored);logtext(line);
 }
 if(reply.path_flags&2048) {
  static const char *bank_name[VX3D_PIPELINE_BANKS]={"CR","FE","PE","RC","PS","XE","BE"};
  unsigned int phase,bank;uint32_t *a,*b;
  textfmt(line,sizeof(line),"Pipeline flags %lX; changing enable response mask %02lX; changing color response mask %02lX; records identical to CR %02lX.\r\n",reply.pipeline_flags,reply.pipeline_match[0],reply.pipeline_match[1],reply.pipeline_identical);logtext(line);
  logtext("Audit flags: 1 started, 2 render state reissued, 4 bank 0 reselected, 8 completed, 16 restore contradiction. Masks require two values plus index tags. Identical responses cannot identify a separate engine. None proves rendering.\r\n");
  for(phase=0;phase<2;phase++) for(bank=0;bank<VX3D_PIPELINE_BANKS;bank++) {
   a=reply.pipeline_raw[phase][bank][0];b=reply.pipeline_raw[phase][bank][1];
   textfmt(line,sizeof(line),"Bank %u %s phase %u: enable %08lX %08lX %08lX %08lX; color %08lX %08lX %08lX %08lX.\r\n",bank,bank_name[bank],phase,a[0],a[1],a[2],a[3],b[0],b[1],b[2],b[3]);logtext(line);
  }
 }
 if((reply.path_flags&4096) && reply.probe_id!=VX3D_SF_PROBE) {
  unsigned int p,j;
  textfmt(line,sizeof(line),"Fetched-buffer flags %08lX; phase %lu; GPU source [%08lX,%08lX); source errors %lu; guard errors %lu.\r\n",reply.dma_flags,reply.dma_phase,reply.dma_base,reply.dma_limit,reply.dma_source_errors,reply.dma_guard_errors);logtext(line);
  logtext("Fetch flags: 1 initial cycle cleared, 2 control built, 4 control fence executed, 8 draw built, 16 draw fence executed, 32 control parked, 64 draw parked, 128 source intact, 256 guards intact. Cursor movement alone does not establish packet execution.\r\n");
  logtext("Fetch phases: 1 claim, 2 control built, 3 control launched, 4 control parked, 5 draw built, 6 draw launched, 7 draw parked, 8 scratch restored.\r\n");
  for(p=0;p<2;p++) {
   textfmt(line,sizeof(line),"Buffer %u %s: %lu DWORDs (%lu bytes), stop %08lX; expected fence %08lX, observed %08lX; park fence %08lX.\r\n",p,p?"triangle":"fence control",reply.dma_count[p],reply.dma_count[p]*4,reply.dma_stop[p],reply.dma_expected[p],reply.dma_observed[p],reply.dma_park_fence[p]);logtext(line);
   textfmt(line,sizeof(line),"Buffer %u raw cursor %08lX -> %08lX; fetch status %08lX -> %08lX.\r\n",p,reply.dma_cursor[p][0],reply.dma_cursor[p][1],reply.dma_status[p][0],reply.dma_status[p][1]);logtext(line);
   for(j=0;j<reply.dma_count[p] && j<VX3D_DMA_WORDS;j++) {
    textfmt(line,sizeof(line),"DMA %u %04X: %08lX\r\n",p,j*4,reply.dma_program[p][j]);logtext(line);
   }
  }
  if(!(reply.dma_flags&4)) logtext("The fetched control fence was not confirmed. No conclusion about triangle rasterization can be made.\r\n");
  else if(!(reply.dma_flags&16)) logtext("Control decoded, but draw-buffer completion was not confirmed. Pixel fields are not a completed rendering test.\r\n");
  logtext("Depth was not measured. See SF mapping and actual-submission fields below.\r\n");
 }
 if(reply.probe_id==VX3D_SF_PROBE) {
  static const char *gti[5]={"SR6A","SR6B","SR6C","SR6F","SR7B (preserved)"};
  static const char *row[5]={"prime","inline H0 fill + H2 fence","inline H0 fill + H0 fence","CPU state + inline Header3 vertices","retire"};
  static const unsigned int reg2d[7]={0x04,0x08,0x0C,0x10,0x14,0x48,0x58};
  unsigned int p,j;
  textfmt(line,sizeof(line),"SF flags %08lX; phase %lu; failure phase %lu; host %08lX, %lu bytes; RTSF %08lX, %lu bytes; A60 %08lX.\r\n",reply.sf_flags,reply.sf_phase,reply.sf_failure_phase,reply.sf_host_linear,reply.sf_host_bytes,reply.sf_gart_offset,reply.sf_gart_bytes,reply.sf_location_word);logtext(line);
  textfmt(line,sizeof(line),"SF current PCI command %04lX; memory decode and bus-master bits (0006) must both be enabled.\r\n",reply.sf_pci_command);logtext(line);
  textfmt(line,sizeof(line),"Memory sequencer: SR68 %02lX; SL base SR6D %02lX SR6E %02lX SR6F %02lX. Sampled, not changed by this test.\r\n",reply.sf_memory[0],reply.sf_memory[1],reply.sf_memory[2],reply.sf_gti_before[3]&0x7FUL);logtext(line);
  logtext("SF flags hex: 1 allocated, 2 PTEs validated, 4 GTI snapshot, 8 VRAM snapshot, 10 table written, 20 GTI programmed, 40 host published, 80 primed, 100/200 inline control0 submitted/fenced, 400/800 inline control1 submitted/fenced, 1000/2000 triangle submitted/fenced, 4000 retired, 8000 GTI restored, 10000 safe release, 20000 host freed, 40000 retained, 80000/100000 source/guard checks ran.\r\n");
  for(p=0;p<5;p++) {
   textfmt(line,sizeof(line),"GTI %s: before %02lX; active %02lX; restored %02lX.\r\n",gti[p],reply.sf_gti_before[p],reply.sf_gti_active[p],reply.sf_gti_after[p]);logtext(line);
  }
  for(p=0;p<VX3D_SF_PAGES;p++) {
   textfmt(line,sizeof(line),"Host page %u: CPU %08lX; VMM PTE %08lX; physical %08lX.\r\n",p,reply.sf_host_linear+p*4096UL,reply.sf_host_pte[p],reply.sf_host_pte[p]&0xFFFFF000UL);logtext(line);
   if(p>=1 && p<=16) {
    textfmt(line,sizeof(line),"SF map %08lX -> RTSF entry %08lX = %08lX -> host page %u.\r\n",0x220000UL+(p-1)*4096UL,reply.sf_gart_offset+(0x220UL+p-1)*4,(reply.sf_host_pte[p]>>12)&0x3FFFFFFFUL,p);logtext(line);
   }
  }
  logtext("Every unused RTSF entry maps the owned dummy page 18. CPU cache writeback/invalidation precedes publication. Jump is FFFFFFF0; VX855 padding CCC00002/DDD00002 to 256-byte boundaries.\r\n");
  logtext("The same numeric range in LOCAL VRAM contains dummy commands only, with no fence, fill, or triangle. Fetched control fence IDs are never posted directly by the CPU.\r\n");
  textfmt(line,sizeof(line),"Bare CPU flags %08lX (1 port 41C hit, 2 port 43C hit, 4 restored, 8 all controls stopped, 10 after FE hit, 20 after FE restored); original %08lX; bare restore %08lX.\r\n",reply.cpu_flags,reply.cpu_original,reply.cpu_restored);logtext(line);
  for(p=0;p<2;p++) {
   textfmt(line,sizeof(line),"CPU %s fence expected %08lX; observed %08lX.\r\n",p?"43C/440":"41C/420",reply.cpu_expected[p],reply.cpu_observed[p]);logtext(line);
   textfmt(line,sizeof(line),"MMIO %s clocks: 400 %08lX; 43C %08lX; 444 %08lX; 494 %08lX.\r\n",p?"after":"before",reply.diagnostic_mmio[p][0],reply.diagnostic_mmio[p][1],reply.diagnostic_mmio[p][2],reply.diagnostic_mmio[p][3]);logtext(line);
  }
  textfmt(line,sizeof(line),"CPU control selected %08lX (0 bare, 1 OpenChrome init, 2 earlier v6 baseline, FFFFFFFF none).\r\n",reply.cpu_selected);logtext(line);
  for(p=0;p<2;p++) {
   textfmt(line,sizeof(line),"CPU retry %u %s: flags %08lX (1 attempted, 2 port41C hit, 4 port43C hit, 8 original restored); restore %08lX.\r\n",p+1,p?"earlier v6 CR/FIFO":"OpenChrome PCIe init",reply.cpu_retry_flags[p],reply.cpu_retry_restored[p]);logtext(line);
   for(j=0;j<2;j++) {
    textfmt(line,sizeof(line),"CPU retry %u %s expected %08lX; observed %08lX.\r\n",p+1,j?"43C/440":"41C/420",reply.cpu_retry_expected[p][j],reply.cpu_retry_observed[p][j]);logtext(line);
   }
   textfmt(line,sizeof(line),"CPU retry %u MMIO: 400 %08lX; 43C %08lX; 444 %08lX; 494 %08lX.\r\n",p+1,reply.cpu_retry_mmio[p][0],reply.cpu_retry_mmio[p][1],reply.cpu_retry_mmio[p][2],reply.cpu_retry_mmio[p][3]);logtext(line);
  }
  textfmt(line,sizeof(line),"PCI BAR0 %08lX; BAR1 %08lX; BAR2 %08lX; command/status %08lX.\r\n",reply.diagnostic_pci[0],reply.diagnostic_pci[1],reply.diagnostic_pci[2],reply.diagnostic_pci[3]);logtext(line);
  textfmt(line,sizeof(line),"CPU after FE expected %08lX; observed %08lX. If bare and both initialization retries fail, FE setup and fetching are not attempted.\r\n",reply.cpu_after_fe[0],reply.cpu_after_fe[1]);logtext(line);
  textfmt(line,sizeof(line),"Legacy branch flags %08lX; v9.4 sends no FE83 or branch-enable command (10 means inline fences missed).\r\n",reply.branch_flags);logtext(line);
  textfmt(line,sizeof(line),"Inline execution flags %08lX (1 2D saved, 2/4 controls built, 8/10 sampled, 20/40 color register changed, 80/100 pixel fill passed, 200 2D restored).\r\n",reply.sf_transfer_flags);logtext(line);
  for(p=0;p<2;p++) {
   textfmt(line,sizeof(line),"Inline control %u: color %08lX; register %08lX -> %08lX; changed %lu; pixel errors %lu. Sample flags distinguish unrun zeros.\r\n",p,reply.sf_2d_color[p],reply.sf_2d_before[p],reply.sf_2d_observed[p],reply.sf_2d_changed[p],reply.sf_2d_errors[p]);logtext(line);
  }
  textfmt(line,sizeof(line),"REGDUMP flags %08lX (1 ran, 2 read failed/busy, 4 complete); after case %lu setup (variants 15-17: re-taken inside texture case 16 after its texture/PS state is issued, before the draw). Format: RB id=stage space=subtype+type group=subaddr: four tagged words.\r\n",reply.dump_flags,(unsigned long)VX3D_DUMP_CASE);logtext(line);
  {
   unsigned int di,ds,dg;
   for(di=0;di<VX3D_DUMP_IDS;di++) for(ds=0;ds<VX3D_DUMP_SPACES;ds++) for(dg=0;dg<64;dg++) {
    static const unsigned long sp[VX3D_DUMP_SPACES]={0x0001,0x0004,0xFE02,0x00FE,0x0002,0x2002};
    textfmt(line,sizeof(line),"RB id=%u space=%04lX group=%02X: %08lX %08lX %08lX %08lX\r\n",di,sp[ds],dg*4,
     reply.dump_words[di][ds][dg][0],reply.dump_words[di][ds][dg][1],reply.dump_words[di][ds][dg][2],reply.dump_words[di][ds][dg][3]);logtext(line);
   }
  }
  if(reply.sf_matrix_count) {
   logtext("v9.8 adds cases 28-31: the same fill routine drawing into the live desktop framebuffer (640x480x32, pitch 2560, offset 0). Each desktop case saves a small window of the screen first, retargets RT0 (01/50-52) and opens the clip windows (01/80-83, 01/88-8A) to the XP full range, fills, checks every pixel in the window against the reference (fill colour where covered and on screen, the saved original elsewhere), then writes the saved pixels back. 28 red rectangle, 29 green triangle (tests the G/B channel order of the constant), 30 blue rectangle clipped at the screen edge, 31 final point control. Per-case fill rectangles and milliseconds are logged.\r\n");
   logtext("v9.7 cases 15-27: control 15; cases 16-25 exercise the driver flat-fill routine (vx855_fill.h): a CPU edge walk with the top-left rule on pixel centres, emitting one type-0 point per run of identical rows (04/2A = width, 04/2B = height) through the CPU register path while the ring is paused; each case is scored per pixel against an independent brute-force reference (16/17 same triangle in both windings, 18 skew, 19 sliver, 20 zero area = no pixels, 21 clipped, 22 rectangle, 23 full-target rectangle, 24 red constant upload, 25 cyan restored); controls 26, 27. Fill cases need a flat colour only: no interpolation, textures or depth, and they do not fix the hardware type-2 triangle. RT0 stays C0F outside the v9.4 variant cases 06/10/14. v9.4: explicit XP state block (all 01h/04h registers) before every case; fixed SF transport, target 201000, pitch 8, ARGB8888, five-word vertices and the hardware-proven constant-cyan shader. v9.4: SR2D/SR3F left as the display driver set them. Cases 02-03 texture-0 zeroed, 04-05 cache-clear strobes pulsed, 06-07 RT0 S4, 08-09 enable CE1540, 10-14 all combined. Case 12 is a three-vertex rectangle and case 13 a plain triangle that runs only if it settled. Both the v6.14 four-vertex and v6.15 two-vertex rectangles stalled.\r\n");
   textfmt(line,sizeof(line),"v9.7 fill totals: %lu emitted rectangles, %lu pixels requested.\r\n",reply.sf_fill_spans,reply.sf_fill_pixels);logtext(line);
   textfmt(line,sizeof(line),"Prepared cases %lu; measured mask %08lX; filled-triangle PASS mask %08lX; last submitted %08lX. Case flags hex: 1 built, 2 state issued, 4 submitted, 8 settled, 10 coverage passed, 20 trailing latch, 40 unconfirmed observation, 80 any changed pixel, 100 path control failed/unrun (case still submitted). Built-only cases were not run.\r\n",reply.sf_matrix_count,reply.sf_matrix_observed,reply.sf_matrix_triangle_pass,reply.sf_matrix_last);logtext(line);
   logtext("Controls identify each delivery path. A failed control is flagged 100; its following bounded trial still runs and is not treated as a skipped packet. Pixel coverage, not cursor/fence alone, decides rendering. VB sources and padding are checked as guards; CPU writes vertex data only into source slots, never orange pixels into the target.\r\n");
   logtext("Primitive type 4 is a rectangle experiment, not a confirmed hardware contract. Two-vertex and four-vertex lists stalled the engine (v6.15, v6.14); case 12 sends three vertices (8,8),(56,8),(8,56). Rectangle coverage may pass; only type 2 can set the triangle PASS mask.\r\n");
   if(reply.sf_matrix_triangle_pass) logtext("BMP: full 64x64 target of the first coverage-passed triangle. Overall PASS additionally requires intact guards and confirmed retirement.\r\n");
   else logtext("BMP: full 64x64 target after fill case 16 if it passed, else the 4x4 contact sheet. 4x4 contact sheet, 16x16 cells, cases 00..15 left-to-right then top-to-bottom (cases 16..27 are in the case list only). Each cell projects actual target reads and preserves changed pixels; counts/checks use full 64x64 data. Point controls remain visible even if no triangle draws.\r\n");
   for(p=0;p<reply.sf_matrix_count && p<VX3D_MATRIX_CASES;p++) {
    vx3d_case_report *c=&reply.sf_matrix[p];
    textfmt(line,sizeof(line),"CASE %02u %s: flags %08lX; changed %lu; expected-color pixels %lu; errors %lu; sample %08lX.\r\n",p,matrix_names[p],c->flags,c->changed,c->orange,c->errors,c->sample);logtext(line);
    if(c->fill_rects){textfmt(line,sizeof(line),"CASE %02u fill: %lu rectangles in %lu ms.\r\n",p,c->fill_rects,c->fill_ms);logtext(line);}
    textfmt(line,sizeof(line),"CASE %02u state: T04A23 %06lX; vertices %lu; path %lu (0 inline, 1 CPU 440, 2 CPU sequential, 3 indexed); address %08lX; pause %08lX; %lu DWORDs.\r\n",p,c->mode,c->vertices,c->layout,c->address,c->pause,c->count);logtext(line);
    textfmt(line,sizeof(line),"CASE %02u configured (flag 2 confirms issued; not readback): FE40 %06lX; enable %06lX; vertex parameter mask %06lX; far low %06lX; near low %06lX; far/near high %06lX; Z compare %06lX; T04A20 cycle/mode %06lX.\r\n",p,c->state[0],c->state[1],c->state[2],c->state[3],c->state[4],c->state[5],c->state[6],c->state[7]);logtext(line);
    textfmt(line,sizeof(line),"CASE %02u configured T04A40..A48 (flag 2 confirms issued): %06lX %06lX %06lX %06lX %06lX %06lX %06lX %06lX %06lX.\r\n",p,c->state[8],c->state[9],c->state[10],c->state[11],c->state[12],c->state[13],c->state[14],c->state[15],c->state[16]);logtext(line);
    textfmt(line,sizeof(line),"CASE %02u XP/default state issued with flag 2: T01 A88/A89/A8A %06lX/%06lX/%06lX; T04 A50/A51/A52 %06lX/%06lX/%06lX; A2F %06lX; A00 extra %06lX.\r\n",p,c->state[17],c->state[18],c->state[19],c->state[20],c->state[21],c->state[22],c->state[23],c->state[24]);logtext(line);
    textfmt(line,sizeof(line),"CASE %02u config: FVF words %lu; input mask %06lX; raw stride %lu; index bits %lu; geometry %lu (0 original, 1 transformed, 2 skew, 3 square); VB %08lX; input DWORDs %lu; payload DWORDs %lu; control %08lX; flags %lX (1 XP prefix, 2 reverse, 4 fan, high byte FE profile); FVF A01/A02 %06lX/%06lX.\r\n",p,c->config[0],c->config[1],c->config[2],c->config[3],c->config[4],c->config[5],c->config[6],c->config[7],c->config[8],c->config[9],c->config[10],c->config[11]);logtext(line);
    textfmt(line,sizeof(line),"CASE %02u shader: kind %lu (0 off, 1 C0, 2 diffuse); expected %08lX; native %08lX %08lX %08lX %08lX; C0 float4 %08lX %08lX %08lX %08lX. Flag 2 confirms issue, not readback.\r\n",p,c->shader[0],c->shader[1],c->shader[13],c->shader[14],c->shader[15],c->shader[16],c->shader[17],c->shader[18],c->shader[19],c->shader[20]);logtext(line);
    textfmt(line,sizeof(line),"CASE %02u shader T01 A90..A9A: %06lX %06lX %06lX %06lX %06lX %06lX %06lX %06lX %06lX %06lX %06lX. One ALU, no TAU when PS is active.\r\n",p,c->shader[2],c->shader[3],c->shader[4],c->shader[5],c->shader[6],c->shader[7],c->shader[8],c->shader[9],c->shader[10],c->shader[11],c->shader[12]);logtext(line);
    textfmt(line,sizeof(line),"CASE %02u FE overrides after 16-word startup: %lu words. Profile %lu; flag 2 confirms issue, not readback. Extra zero fields persist into later profiles/final startup control.\r\n",p,c->fe_override[0],c->config[9]>>8);logtext(line);
    for(j=0;j<c->fe_override[0] && j<23;j++) {
     textfmt(line,sizeof(line),"CASE %02u FE override %02u: %08lX\r\n",p,j,c->fe_override[j+1]);logtext(line);
    }
    for(j=0;j<c->config[6] && j<24;j++) {
     textfmt(line,sizeof(line),"CASE %02u vertex-source %02u: %08lX\r\n",p,j,c->input[j]);logtext(line);
    }
    if(p>=1 && p<=14 && c->changed) {
     unsigned int yy,xx;
     textfmt(line,sizeof(line),"CASE %02u coverage map (# = changed pixel, 64x64):\r\n",p);logtext(line);
     for(yy=0;yy<((p>=16&&p<=25)?0:64);yy++) {
      char row[70];
      for(xx=0;xx<64;xx++) row[xx]=(reply.sf_cover[p][(yy*64+xx)/32]>>((yy*64+xx)%32))&1?'#':'.';
      row[64]='\r';row[65]='\n';row[66]=0;logtext(row);
     }
    }
    if(reply.sf_miss[p][0]) {
     unsigned int k,n=reply.sf_miss[p][0];
     textfmt(line,sizeof(line),"CASE %02u mismatches: %u total; first %u (x,y; E = drawn outside model, M = model pixel missing):\r\n",p,n,n>32?32:n);logtext(line);
     for(k=0;k<n && k<32;k++) {
      DWORD v=reply.sf_miss[p][1+k];
      textfmt(line,sizeof(line),"  %lu,%lu %c\r\n",(unsigned long)(v&0xFFFFUL),(unsigned long)((v>>16)&0x7FFFUL),(v&0x80000000UL)?'E':'M');logtext(line);
     }
    }
    if((p>=16 && p<=25)||p==31||p==27) {
     unsigned int yy,xx,ri=(p==31)?10:(p==27)?11:(p-16);
     if(sel_arg>=10) logtext("  16bpp RT case raw layout: [0..7] row 20 pixel pairs x=8..23 (lo=even x); [8] RT format code; [9] colour millis R|G<<10|B<<20; [10] dest prefill pixel; [11] candidate A trunc(f*max) packed R,G,B,A bytes; [12] B round(f*max); [13] C 8-bit then shift; [14] unique inside values; [15] inside pixels equal to first; [16..18] pixels matching A/B/C exactly (of 2304); [19] first inside pixel; [20..23] unique values; [24] outside-rect pixels changed; [25] guard dwords changed; [26] tolerance (+-1) failures.\r\n");
     logtext("  raw pixels at window (4..11,4..11), 8 per row (texture cases: raw[0..7] got row20, [8..15] model(fit), [16..20] fit/score/score00/score22/n, [24..31] model(0,0), [32..47] top row y=0 got (32..39) / model (40..47), [48..63] EMITTED: TAU, ALU x4, A90,A92,A93, variant, tex base,R18,R20,R22,R30,R31,R32):\r\n");
     for(yy=0;yy<8;yy++) {
      textfmt(line,sizeof(line),"  %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX\r\n",(unsigned long)reply.sf_raw[ri][yy*8],(unsigned long)reply.sf_raw[ri][yy*8+1],(unsigned long)reply.sf_raw[ri][yy*8+2],(unsigned long)reply.sf_raw[ri][yy*8+3],(unsigned long)reply.sf_raw[ri][yy*8+4],(unsigned long)reply.sf_raw[ri][yy*8+5],(unsigned long)reply.sf_raw[ri][yy*8+6],(unsigned long)reply.sf_raw[ri][yy*8+7]);logtext(line);
     }
     textfmt(line,sizeof(line),"  2D engine reset attempted %lu, recovered %lu\r\n",(unsigned long)reply.sf_raw[13][ri*2],(unsigned long)reply.sf_raw[13][ri*2+1]);logtext(line);
     if(p==25){logtext("  desktop pixels (0..7,0..7) for reference:\r\n");
      for(yy=0;yy<8;yy++){textfmt(line,sizeof(line),"  %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX\r\n",(unsigned long)reply.sf_raw[12][yy*8],(unsigned long)reply.sf_raw[12][yy*8+1],(unsigned long)reply.sf_raw[12][yy*8+2],(unsigned long)reply.sf_raw[12][yy*8+3],(unsigned long)reply.sf_raw[12][yy*8+4],(unsigned long)reply.sf_raw[12][yy*8+5],(unsigned long)reply.sf_raw[12][yy*8+6],(unsigned long)reply.sf_raw[12][yy*8+7]);logtext(line);}}
     textfmt(line,sizeof(line),"CASE %02u window map 64x64 (. unchanged, # = foreground/op colour, o = background colour, + = other changed pixel):\r\n",p);logtext(line);
     for(yy=0;yy<64;yy++) {
      char row[70];
      for(xx=0;xx<64;xx++) {
       unsigned long cls=(reply.sf_cls[ri][(yy*64+xx)/16]>>(((yy*64+xx)%16)*2))&3UL;
       row[xx]=cls==0?'.':cls==1?'#':cls==2?'o':'+';
      }
      row[64]='\r';row[65]='\n';row[66]=0;logtext(row);
     }
    }
    if(p==9) {
     unsigned int yy,xx,n=0,k,seen=0; DWORD list[64];
     for(yy=0;yy<64;yy++) for(xx=0;xx<64;xx++) {
      DWORD v=reply.sf_gouraud[yy*64+xx];
      if(v==0xFF182430UL) continue;
      for(k=0;k<seen;k++) if(list[k]==v) break;
      if(k==seen && seen<64) list[seen++]=v;
      n++;
     }
     textfmt(line,sizeof(line),"CASE 09 Gouraud: %u non-background pixels, %u distinct colours (first 64 counted).\r\n",n,seen);logtext(line);
     logtext("CASE 09 Gouraud colour grid, every 4th pixel, RRGGBB (.... = background):\r\n");
     for(yy=0;yy<64;yy+=4) {
      char row[200];unsigned int o=0;
      for(xx=0;xx<64;xx+=4) {
       DWORD v=reply.sf_gouraud[yy*64+xx];
       if(v==0xFF182430UL){row[o++]='.';row[o++]='.';row[o++]='.';row[o++]='.';row[o++]='.';row[o++]='.';}
       else o+=wsprintf(row+o,"%06lX",v&0xFFFFFFUL);
       row[o++]=' ';
      }
      row[o++]='\r';row[o++]='\n';row[o]=0;logtext(row);
     }
    }
    textfmt(line,sizeof(line),"CASE %02u status %08lX -> %08lX; cursor %08lX -> %08lX; fetch %08lX -> %08lX; latch %08lX expected %08lX.\r\n",p,c->before[0],c->after[0],c->before[1],c->after[1],c->before[2],c->after[2],c->latch_observed,c->latch_expected);logtext(line);
    for(j=0;j<c->count && j<128;j++) {
     textfmt(line,sizeof(line),"CASE %02u packet %04X: %08lX\r\n",p,j*4,c->program[j]);logtext(line);
    }
   }
   textfmt(line,sizeof(line),"Retirement: pause %08lX; cursor %08lX -> %08lX; fetch %08lX -> %08lX. PASS requires a filled triangle, intact guards and completed retirement.\r\n",reply.sf_pause[4],reply.sf_before[4][1],reply.sf_after[4][1],reply.sf_before[4][2],reply.sf_after[4][2]);logtext(line);
  } else {
  logtext("Primary 3D state is programmed directly by CPU, with RC-cache clear released. The fetched primary contains a legal inspection marker and counted vertices. The first inline triangle uses CMDQ OFF. A settled miss permits the identical inline triangle with XP AGP CMDQ 74101005 enabled, then H0 replay. No branch is used.\r\n");
  textfmt(line,sizeof(line),"CPU 3D state flags %08lX; status %08lX (1 issued, 2 idle, 4/8 CPU tag CR/FE, 10/20 fetched tag CR/FE, 40 trailing H0 latch, 80 RC-clear release issued, 100/200 snapshots ran).\r\n",reply.sf_state_flags,reply.sf_cpu_state_status);logtext(line);
  textfmt(line,sizeof(line),"State markers: CPU expected %04lX, CR observed %04lX; fetched expected %04lX, CR observed %04lX. These inspect writeback paths, not rasterization.\r\n",reply.sf_state_marker[0],reply.sf_state_marker[1],reply.sf_state_marker[2],reply.sf_state_marker[3]);logtext(line);
  for(p=0;p<2;p++) for(j=0;j<2;j++) {
   textfmt(line,sizeof(line),"State tag raw %s / %s: %08lX %08lX %08lX %08lX.\r\n",p?"after fetched vertices":"CPU preload",j?"FE":"CR",reply.sf_state_raw[p][j][0],reply.sf_state_raw[p][j][1],reply.sf_state_raw[p][j][2],reply.sf_state_raw[p][j][3]);logtext(line);
  }
  textfmt(line,sizeof(line),"CPU FVF raw: %08lX %08lX %08lX %08lX. CPU target raw: %08lX %08lX %08lX %08lX.\r\n",reply.sf_state_fvf[0],reply.sf_state_fvf[1],reply.sf_state_fvf[2],reply.sf_state_fvf[3],reply.sf_state_target[0],reply.sf_state_target[1],reply.sf_state_target[2],reply.sf_state_target[3]);logtext(line);
  textfmt(line,sizeof(line),"Trailing H0 latch expected %08lX; observed %08lX. It writes foreground register 58 only, never the target.\r\n",reply.sf_draw_marker_expected,reply.sf_draw_marker_observed);logtext(line);
  textfmt(line,sizeof(line),"XP AGP queue flags %08lX (1 retry built, 2 enable issued, 4 idle, 8 CPU state reissued, 10 trailing latch matched); control %08lX; local [%08lX,%08lX] inclusive; length %08lX in 16-byte units.\r\n",reply.sf_queue_compare_flags,reply.sf_queue_compare_control,reply.sf_queue_compare_start,reply.sf_queue_compare_end,reply.sf_queue_compare_length);logtext(line);
  textfmt(line,sizeof(line),"XP AGP queue status %08lX -> %08lX; retry latch expected %08lX observed %08lX; queue changed DWORDs %lu (not rendering evidence). CPU FVF/target snapshots reflect the latest preload if retry ran.\r\n",reply.sf_queue_compare_status[0],reply.sf_queue_compare_status[1],reply.sf_draw_marker_expected+1,reply.sf_queue_compare_latch,reply.queue_changed_words);logtext(line);
  for(p=0;p<3;p++) {
   textfmt(line,sizeof(line),"Branch source %u: SF %08lX; %lu blocks of 16 bytes.\r\n",p,reply.branch_address[p],reply.branch_blocks[p]);logtext(line);
  }
  for(p=0;p<7;p++) {
   textfmt(line,sizeof(line),"2D register %02X: saved %08lX; restored %08lX.\r\n",reg2d[p],reply.sf_2d_saved[p],reply.sf_2d_restored[p]);logtext(line);
  }
  for(p=0;p<5;p++) {
   if(p==0 || p==4)
    textfmt(line,sizeof(line),"SF %s: pause %08lX; fence observed %08lX (no fence requirement); status %08lX -> %08lX; cursor %08lX -> %08lX; fetch %08lX -> %08lX.\r\n",row[p],reply.sf_pause[p],reply.sf_observed[p],reply.sf_before[p][0],reply.sf_after[p][0],reply.sf_before[p][1],reply.sf_after[p][1],reply.sf_before[p][2],reply.sf_after[p][2]);
   else
    textfmt(line,sizeof(line),"SF %s: pause %08lX; fence expected %08lX, observed %08lX; status %08lX -> %08lX; cursor %08lX -> %08lX; fetch %08lX -> %08lX.\r\n",row[p],reply.sf_pause[p],reply.sf_expected[p],reply.sf_observed[p],reply.sf_before[p][0],reply.sf_after[p][0],reply.sf_before[p][1],reply.sf_after[p][1],reply.sf_before[p][2],reply.sf_after[p][2]);
   logtext(line);
  }
  for(p=0;p<2;p++) {
   textfmt(line,sizeof(line),"SF generated packet %u (%s): %lu DWORDs; GPU start %08lX. Generation is not submission; see SF flags.\r\n",p,p?"triangle":"41C control",reply.dma_count[p],0x220000UL+(p?0xC00UL:0x400UL));logtext(line);
   for(j=0;j<reply.dma_count[p] && j<VX3D_DMA_WORDS;j++) {
    textfmt(line,sizeof(line),"SF packet %u %04X: %08lX\r\n",p,j*4,reply.dma_program[p][j]);logtext(line);
   }
  }
  for(j=0;j<64;j++) {
   textfmt(line,sizeof(line),"SF inline control 1 %04X: %08lX\r\n",j*4,reply.sf_control43c[j]);logtext(line);
  }
  for(j=0;j<reply.branch_control_count && j<VX3D_DMA_WORDS;j++) {
   textfmt(line,sizeof(line),"SF queue-ON inline program %04X: %08lX\r\n",j*4,reply.branch_control[j]);logtext(line);
  }
  for(p=0;p<2;p++) for(j=0;reply.branch_blocks[p] && j<16;j++) {
   textfmt(line,sizeof(line),"Branch payload %u %04X: %08lX\r\n",p,j*4,reply.branch_payload[p][j]);logtext(line);
  }
  textfmt(line,sizeof(line),"Primary CPU-state / fetched-vertex triangle: changed %lu; errors %lu; sample %08lX. Check submission and observation flags.\r\n",reply.sf_primary[0],reply.sf_primary[1],reply.sf_primary[2]);logtext(line);
  textfmt(line,sizeof(line),"Queue-ON inline flags %08lX (1 built, 2 submitted, 4 settled, 8 fenced, 10 pixels passed, 20 unconfirmed pixel observation); %lu DWORDs at 00221C00; pause %08lX.\r\n",reply.sf_branch_flags,reply.branch_control_count,reply.sf_branch_pause);logtext(line);
  textfmt(line,sizeof(line),"Queue-ON inline fence expected %08lX, observed %08lX; changed %lu; errors %lu; sample %08lX.\r\n",reply.sf_branch_expected,reply.sf_branch_observed,reply.sf_branch_changed,reply.sf_branch_errors,reply.sf_branch_sample);logtext(line);
  textfmt(line,sizeof(line),"Queue-ON inline status %08lX -> %08lX; cursor %08lX -> %08lX; fetch %08lX -> %08lX.\r\n",reply.sf_branch_before[0],reply.sf_branch_after[0],reply.sf_branch_before[1],reply.sf_branch_after[1],reply.sf_branch_before[2],reply.sf_branch_after[2]);logtext(line);
  logtext("3D rectangle uses one 16-pixel point at (20,20): same Type01 A50=2010, A52=990008, no triangle assembly. It still uses the vertex front end; a miss does not isolate the backend.\r\n");
  for(p=0;p<2;p++) {
   textfmt(line,sizeof(line),"3D rectangle CMDQ %s: flags %08lX (1 built, 2 submitted, 4 settled, 8 exact pixels passed, 10 unconfirmed observation, 20 state issued, 40 trailing latch); changed %lu; pixel errors %lu; sample %08lX; latch %08lX expected %08lX.\r\n",p?"ON":"OFF",reply.sf_point_flags[p],reply.sf_point_changed[p],reply.sf_point_errors[p],reply.sf_point_sample[p],reply.sf_point_latch[p],reply.sf_draw_marker_expected+2+p);logtext(line);
   textfmt(line,sizeof(line),"3D rectangle CMDQ %s: pause %08lX; status %08lX -> %08lX; cursor %08lX -> %08lX; fetch %08lX -> %08lX.\r\n",p?"ON":"OFF",reply.sf_point_pause[p],reply.sf_point_before[p][0],reply.sf_point_after[p][0],reply.sf_point_before[p][1],reply.sf_point_after[p][1],reply.sf_point_before[p][2],reply.sf_point_after[p][2]);logtext(line);
   for(j=0;j<reply.sf_point_count[p] && j<64;j++) {
    textfmt(line,sizeof(line),"SF rectangle %u %04X: %08lX\r\n",p,j*4,reply.sf_point_program[p][j]);logtext(line);
   }
  }
  logtext("Overall PASS/solid error refers to the triangle. Rectangle outcomes are independent in their own flags and counts; the BMP captures the latest triangle, not the rectangle.\r\n");
  textfmt(line,sizeof(line),"Unconfirmed draw observation flags %08lX (1 inline Header3, 2 queue-ON inline, 4 H0). These target reads do not establish completed rendering.\r\n",reply.sf_draw_observation);logtext(line);
  textfmt(line,sizeof(line),"Alternate Header0 triangle: flags %08lX (1 built, 2 submitted, 4 settled, 8 fenced, 10 pixels passed); %lu DWORDs; pause %08lX.\r\n",reply.sf_alt_flags,reply.sf_alt_count,reply.sf_alt_pause);logtext(line);
  textfmt(line,sizeof(line),"Alternate fence expected %08lX, observed %08lX; changed %lu; errors %lu; sample %08lX.\r\n",reply.sf_alt_expected,reply.sf_alt_observed,reply.sf_alt_changed,reply.sf_alt_errors,reply.sf_alt_sample);logtext(line);
  textfmt(line,sizeof(line),"Alternate status %08lX -> %08lX; cursor %08lX -> %08lX; fetch %08lX -> %08lX.\r\n",reply.sf_alt_before[0],reply.sf_alt_after[0],reply.sf_alt_before[1],reply.sf_alt_after[1],reply.sf_alt_before[2],reply.sf_alt_after[2]);logtext(line);
  for(j=0;j<reply.sf_alt_count && j<VX3D_DMA_WORDS;j++) {
   textfmt(line,sizeof(line),"SF alternate %04X: %08lX\r\n",j*4,reply.sf_alt_program[j]);logtext(line);
  }
  logtext("After clean control pauses, try inline Header3 with CMDQ OFF. A settled pixel miss permits the same Header3 with the XP AGP CMDQ enabled, then H0 replay if still needed. A stalled reader always stops further work. Pixels are captured on a stall as observations, never PASS evidence. Targets are cleared between draws. PASS requires pixels, guards, and retirement.\r\n");
  }
  textfmt(line,sizeof(line),"SF checks: source errors %lu, table errors %lu, guard errors %lu; GTI flush polls %lu.\r\n",reply.sf_source_errors,reply.sf_table_errors,reply.sf_guard_errors,reply.sf_flush_polls);logtext(line);
  {
   static const char *xn[26]={"?","?","?","?","?","?","?","?","source colour key copy SRCCOPY (key in 0x50, 0x48=0x4000)","colour pattern from frame buffer","destination colour key copy SRCCOPY (key in 0x4C, 0x48=0x8000)","mono source expansion from frame buffer CC000101","host mono blit (bit 6+8), constant port 0x100","host mono blit, dword aligned","host mono blit, incrementing ports","host colour copy (bit 6)","host mono, source transparency (bit 10)","host text command 2 (bit 6+8)","bitblt+rotate 90 (mode 0x301)","bitblt+rotate 180 (mode 0x302)","bitblt+rotate 270 (mode 0x303)","XP host colour blit CC002041, constant window address","XP host colour blit CC002041, incrementing window addresses","XP host mono blit CC0A0141 dword aligned, incrementing window addresses","fixed-colour fill F0002001 with destination colour key","XP host text CC021542 with clip window, transparent mono"};
   unsigned int k,yy,xx;
   logtext("EXTRA 2D TESTS (post-phase, isolated; each followed by a 2D engine reset attempt if it hung):\r\n");
   for(k=0;k<8;k++) {
    if(!reply.sf_x_info[k][7]) continue;
    textfmt(line,sizeof(line),"EXTRA %u %s: cmd %08lX; reset attempted %lu, recovered %lu; changed %lu; expected %lu; matched %lu; errors %lu.\r\n",k,xn[reply.sf_x_info[k][0]],(unsigned long)reply.sf_x_info[k][7],(unsigned long)reply.sf_x_info[k][1],(unsigned long)reply.sf_x_info[k][2],(unsigned long)reply.sf_x_info[k][3],(unsigned long)reply.sf_x_info[k][6],(unsigned long)reply.sf_x_info[k][4],(unsigned long)reply.sf_x_info[k][5]);logtext(line);
    logtext("  map of window x 4..35, y 4..19 (. unchanged, # = fg colour, o = bg colour, + = other):\r\n");
    for(yy=0;yy<16;yy++) {
     char row[48];
     for(xx=0;xx<32;xx++) {
      unsigned long cls=(reply.sf_x_cls[k][(yy*32+xx)/16]>>(((yy*32+xx)%16)*2))&3UL;
      row[xx]=cls==0?'.':cls==1?'#':cls==2?'o':'+';
     }
     row[32]='\r';row[33]='\n';row[34]=0;logtext(row);
    }
    logtext("  raw pixels, first 4 rows of 8:\r\n");
    for(yy=0;yy<4;yy++) {
     textfmt(line,sizeof(line),"  %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX\r\n",(unsigned long)reply.sf_x_raw[k][yy*32],(unsigned long)reply.sf_x_raw[k][yy*32+1],(unsigned long)reply.sf_x_raw[k][yy*32+2],(unsigned long)reply.sf_x_raw[k][yy*32+3],(unsigned long)reply.sf_x_raw[k][yy*32+4],(unsigned long)reply.sf_x_raw[k][yy*32+5],(unsigned long)reply.sf_x_raw[k][yy*32+6],(unsigned long)reply.sf_x_raw[k][yy*32+7]);logtext(line);
    }
   }
  }
  if(!(reply.sf_flags&4096)) logtext("Geometry NOT SUBMITTED. Prebuilt packet counts are not GPU draw counts.\r\n");
  if(reply.sf_flags&0x40000) logtext("Completion or mapping restoration is unconfirmed. Backing and clocks are retained; restart Windows before another run.\r\n");
 }
}
static void connect_driver(void) {
 DWORD got=0; vx_status_t s; char line[256];
 strcpy(message,"Connecting to the installed VX85598 driver...");
 device=CreateFile("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(device==INVALID_HANDLE_VALUE) {
  textfmt(message,sizeof(message),"Cannot open VX85598: Windows error %lu.",GetLastError()); return;
 }
 memset(&s,0,sizeof(s));
 if(!DeviceIoControl(device,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL) ||
  got!=sizeof(s) || s.magic!=VX_MAGIC || s.cb!=sizeof(s) || s.version!=VX_VERSION) {
  strcpy(message,"Installed driver has an incompatible status interface."); return;
 }
 textfmt(message,sizeof(message),"Desktop %lux%lux%lu; driver stage %lu, failure %lu.\r\n",
  s.width,s.height,s.bpp,s.stage,s.failure); logtext(message); strncat(connect_text,message,200);
 if(!call(VX_IOCTL_3D_PROBE,NULL,0)) return;
 textfmt(message,sizeof(message),"Probe: %s. Engine status %08lX.\r\n",result_name(reply.result),reply.status_before);
 logtext(message); strncat(connect_text,message,200);
 textfmt(line,sizeof(line),"Connected VxD build %08lX; report %08lX; fault latched %lu.\r\n",reply.build,reply.version,reply.fault_latched);
 logtext(line); strncat(connect_text,line,200);
 if(reply.result==VX3D_NOT_RUN && !reply.fault_latched) {
  can_run=1;
  strcpy(message,"Ready: v9.61. Press one variant button (V0..V9); each makes its own VX3DVn_nnn.TXT/BIN. Restart Windows after a stall.");
 }
 FlushFileBuffers(logfile);
}
static unsigned long recover_flag;
static int startup_pair=1,quiet_box;
static void run_test(HWND window) {
 vx3d_request request; char line[1600],name[MAX_PATH];
 unsigned int i,y,finished=0,passed=0; int ok=1;
 can_run=0;
 strcpy(message,"Run button received. Starting the test...");
 InvalidateRect(window,NULL,TRUE); UpdateWindow(window);
 if(!logtext("Run button received. Captures reserved; flushing before request.\r\n") || !FlushFileBuffers(logfile)) {
  capture_error=GetLastError();
  textfmt(message,sizeof(message),"Cannot write TXT capture: Windows error %lu. No test request was sent.",capture_error);
  goto done;
 }
 if(binary==INVALID_HANDLE_VALUE) {
  strcpy(message,"BIN capture is unavailable. No test request was sent.");
  logtext(message); logtext("\r\n"); goto done;
 }
 logtext("v9.8 SF test: one request, v24 report, ID 20. Fifteen cases compare rasterizer-side state variants (Z, guard band, windows, point size, enables). Keep the proven constant-cyan shader, fixed target, masks and transport. Rectangle coverage never becomes a triangle PASS. Stop after a verified triangle or unsafe completion; guards and retirement remain required.\r\n");
 for(i=0;i<UI_COUNT;i++) {
  textfmt(message,sizeof(message),"Running probe %u/%u: %s",i+1,UI_COUNT,probe_names[i]);
  InvalidateRect(window,NULL,TRUE); UpdateWindow(window);
  textfmt(line,sizeof(line),"\r\nBEGIN PROBE %02u: %s. Flushed BEFORE GPU submission.\r\n",i+UI_FIRST,probe_names[i]);
  if(!logtext(line) || !FlushFileBuffers(logfile)) {ok=0; break;}
  request.cb=sizeof(request); request.version=VX3D_VERSION;
  request.arm=VX3D_ARM; request.reserved=sel_arg|(recover_flag?0x100UL:0UL); request.probe=i+UI_FIRST;
  if(!call(VX_IOCTL_3D_TEST,&request,sizeof(request))) {ok=0; break;}
  if(reply.probe_id!=i+UI_FIRST) {logtext("Mismatched probe ID. Batch stopped.\r\n"); ok=0; break;}
  textfmt(line,sizeof(line),"%s\r\nStage %lu, writes %lu, polls %lu, elapsed %lu ms. Guard %lu; solid %lu; shade %lu; depth %lu. Changed %lu. Samples %08lX / %08lX / %08lX. Latched %lu.\r\n",
   result_name(reply.result),reply.stage,reply.writes,reply.polls,reply.elapsed_ms,
   reply.guard_errors,reply.solid_errors,reply.shade_errors,reply.depth_errors,
   reply.changed_pixels,reply.solid_sample,reply.shade_sample,reply.depth_sample,reply.fault_latched);
  if(!logtext(line)) {ok=0; break;}
  if(recover_flag){textfmt(line,sizeof(line),"3D engine recovery requested before this run: attempted %lu, status before reset %08lX, status after %08lX, recovered %lu.\r\n",(unsigned long)reply.sf_x_info[7][3],(unsigned long)reply.sf_x_info[7][0],(unsigned long)reply.sf_x_info[7][1],(unsigned long)reply.sf_x_info[7][2]);logtext(line);}
  log_engine_details();
  if(!append_capture() || !FlushFileBuffers(logfile)) {ok=0; break;}
  finished++;
  if(reply.result==VX3D_PASS) passed++;
  textfmt(rows[i],sizeof(rows[i]),"%02u %s: %s (%lu %s)",i+UI_FIRST,short_names[i],reply.result==VX3D_PASS?"PASS":"FAIL",(reply.checks_run&64)?reply.depth_changed_pixels:reply.changed_pixels,(reply.checks_run&64)?"Z px":"px");
  if(reply.checks_run&2) {
   for(y=0;y<VX3D_SIDE;y++) memcpy(sheet+y*VX3D_SIDE*UI_COUNT+i*VX3D_SIDE,reply.image+y*VX3D_SIDE,VX3D_SIDE*4);
   have_image=1;
  }
  if(reply.fault_latched || (reply.result!=VX3D_PASS && reply.result!=VX3D_SOLID_FAILURE && reply.result!=VX3D_SHADE_FAILURE && reply.result!=VX3D_DEPTH_FAILURE)) break;
 }
 CloseHandle(binary); binary=INVALID_HANDLE_VALUE;
 save_sheet();
 if(ok) textfmt(message,sizeof(message),"Finished %u/%u probes; %u passed their own checks. %s\r\nNo Direct3D HAL is installed. See TXT for each independent result.",
  finished,UI_COUNT,passed,finished==UI_COUNT?"Batch complete.":"Stopped at a hard failure; reboot before another run.");
 else strcpy(message,"Capture or interface error stopped the batch. Earlier flushed records remain in TXT/BIN. See the TXT report.");
 logtext(message); logtext("\r\n");
 done:
 if(binary!=INVALID_HANDLE_VALUE) {CloseHandle(binary); binary=INVALID_HANDLE_VALUE;}
 FlushFileBuffers(logfile); InvalidateRect(window,NULL,TRUE); UpdateWindow(window);
 textfmt(name,sizeof(name),"%s.TXT",base);
 logtext("Test handler returned; reopen the TXT to see its current contents.\r\n");
 FlushFileBuffers(logfile);
 textfmt(line,sizeof(line),"%s\r\n\r\nReport: %s\r\nClose and reopen an existing Notepad view to refresh it.",message,name);
 if(!quiet_box) MessageBox(window,line,"VX85598 test result",MB_OK|MB_ICONINFORMATION);
}
static void start_variant(HWND w,unsigned int v,int recover) {
 char oldbase[MAX_PATH+8],oldname[MAX_PATH+16];
 recover_flag=(unsigned long)recover;
 strcpy(oldbase,base);
 if(binary!=INVALID_HANDLE_VALUE) {CloseHandle(binary); binary=INVALID_HANDLE_VALUE;}
 if(logfile!=INVALID_HANDLE_VALUE) {FlushFileBuffers(logfile); CloseHandle(logfile); logfile=INVALID_HANDLE_VALUE;}
 if(startup_pair) {
  textfmt(oldname,sizeof(oldname),"%s.TXT",oldbase); DeleteFile(oldname);
  textfmt(oldname,sizeof(oldname),"%s.BIN",oldbase); DeleteFile(oldname);
  startup_pair=0;
 }
 sel_arg=v; textfmt(vprefix,sizeof(vprefix),"V%u_",v);
 if(!open_log()) {
  textfmt(message,sizeof(message),"Cannot reserve a TXT/BIN pair for variant %u. Windows error %lu.",v,capture_error);
  InvalidateRect(w,NULL,TRUE); return;
 }
 logtext("VX85598 v9.61 texture variant run.\r\n"); logtext(connect_text);
 textfmt(message,sizeof(message),"Texture variant %lu. XP's captured shader/texture bundle is the baseline (found by comparing with an XP ring capture; it stops the stall). 0 = full proof run: non-uniform texture, XP vertex path with explicit UVs (absolute screen position / 2^exponent) and output mask, fitted against the model. 1 = bisect without A9B; 2 = old TAU/ALU words; 3 = old texture registers 18/22/30; 4 = old sampler registers 31/32; 5 = no 10030000 upload (1-5 use the uniform texture and the screen-coordinate path). 6 = non-uniform texture on the screen-coordinate path. 7 = old TAU word with the XP ALU word; 8 = XP TAU word with the old ALU word (uniform, screen path). 15 = depth modes EQUAL/ALWAYS and strict-edge cases (cases 16..25 remapped); 18 = fog measurement (cases 16..25: linear Z fog with equation 0/1, start/end variations, disabled control, exploratory local/W modes, colour-field mapping probe; raw layout: [0] 01/70 [1] enable [2] interior checked [3] within +-4 [4] outside changed [5] max err [6] mean err x100 [7] distinct values [8..11] got at (12,12),(44,12),(12,44),(24,24) [12..15] expected RRGGBB [16] got at (50,10) [19] got at (10,50)); 17 = stencil test and operations (cases 16..25, two rectangle passes, 24-bit Z + 8-bit stencil buffer; raw = depth/stencil dwords at the overlap, A-only and B-only probe pixels, then colour, expected colour and expected dword); 16 = Gouraud triangle into the desktop target in 32bpp and each 16bpp format, with rounding or dither (per-case table; raw layout: [0] fmt [1] 01/53 [2] interior checked [3] within tolerance [4] outside changed [5] max err [6] mean err x100 [7] distinct values [8..11] got at (12,12),(44,12),(12,44),(24,24) [12..15] expected RRGGBB); 10..14 = 16bpp render targets; 10 baseline, 11 = 01/53 XP value 010C0F (rounding), 12 = dither enable bit + 01/53 000C0F, 13 = enable bit + 010C0F, 14 = 01/53 008C0F (cases 16..25: RGB565/555/ARGB1555/ARGB4444 flat fills, mid-grey rounding probe, alpha blend into 565 and 4444, BGR565).\r\n",sel_arg); logtext(message);
 can_run=1;
 run_test(w);
}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
 PAINTSTRUCT ps; HDC dc; RECT rect; BITMAPINFO info; char name[MAX_PATH+50]; unsigned int i;
 switch(msg) {
 case WM_CREATE:
  {static const char *vl[23]={"V0 proof (UV)","V1 -A9B","V2 -TAU/ALU","V3 -texregs","V4 -sampler","V5 -upload","V6 screen path","V7 old TAU","V8 old ALU","V10 16bpp RT","V11 dither 53=XP","V12 dither en+tbl","V13 en+XP53","V14 53 mode01","V15 depth EQ/ALW","V16 Gouraud RT","V17 stencil","V18 fog","V19 host data","V20 keys+text","V24 rot 90","V25 rot 180","V26 rot 270"};
   for(i=0;i<23;i++) {
    CreateWindow("BUTTON",vl[i],WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
     16+(int)(i%4)*128,96+(int)(i/4)*32,124,28,w,(HMENU)(110+i),GetModuleHandle(NULL),NULL);
    EnableWindow(GetDlgItem(w,110+(int)i),can_run);
   }}
  CreateWindow("BUTTON","Run V7 then V8 (auto reset)",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
   16,296,300,28,w,(HMENU)150,GetModuleHandle(NULL),NULL);
  EnableWindow(GetDlgItem(w,150),can_run);
  CreateWindow("BUTTON","Close",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
   330,296,85,28,w,(HMENU)101,GetModuleHandle(NULL),NULL);
  return 0;
 case WM_COMMAND:
  if(LOWORD(wp)>=110 && LOWORD(wp)<=132 && HIWORD(wp)==BN_CLICKED && can_run) {
   unsigned int k;
   for(k=0;k<23;k++) EnableWindow(GetDlgItem(w,110+(int)k),FALSE); EnableWindow(GetDlgItem(w,150),FALSE);
   start_variant(w,(LOWORD(wp)-110)>=20?(LOWORD(wp)-110)+4:(LOWORD(wp)-110)<9?(LOWORD(wp)-110):(LOWORD(wp)-109),0);
  }
  if(LOWORD(wp)==150 && HIWORD(wp)==BN_CLICKED && can_run) {
   static const unsigned int list[2]={7,8}; unsigned int k,n; char msg[300];
   for(k=0;k<23;k++) EnableWindow(GetDlgItem(w,110+(int)k),FALSE); EnableWindow(GetDlgItem(w,150),FALSE);
   quiet_box=1;
   for(n=0;n<2;n++) {
    start_variant(w,list[n],n>0);
    if(reply.result==VX3D_DISABLED) {n++; break;}
   }
   quiet_box=0;
   textfmt(msg,sizeof(msg),"Run-all finished after %u variant(s). Each wrote its own VX3DVn_nnn.TXT/BIN. If the last one reported a failed recovery, restart Windows.",n);
   MessageBox(w,msg,"VX85598 run-all",MB_OK|MB_ICONINFORMATION);
  }
  if(LOWORD(wp)==101) DestroyWindow(w);
  return 0;
 case WM_PAINT:
  dc=BeginPaint(w,&ps); SetBkMode(dc,TRANSPARENT);
  rect.left=16; rect.top=12; rect.right=520; rect.bottom=60;
  DrawText(dc,message,-1,&rect,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
  if(0 && have_image) {
   memset(&info,0,sizeof(info)); info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
   info.bmiHeader.biWidth=VX3D_SIDE*UI_COUNT; info.bmiHeader.biHeight=-(LONG)VX3D_SIDE;
   info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
   info.bmiHeader.biCompression=BI_RGB;
   StretchDIBits(dc,16,84,128*UI_COUNT,128,0,0,VX3D_SIDE*UI_COUNT,VX3D_SIDE,sheet,&info,DIB_RGB_COLORS,SRCCOPY);
  }
  for(i=0;i<UI_COUNT;i++) {
   rect.left=16; rect.right=520; rect.top=80+(int)i*16; rect.bottom=rect.top+16;
   DrawText(dc,rows[i],-1,&rect,DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
  }
  rect.left=16; rect.right=600;
  textfmt(name,sizeof(name),"Report: %s.TXT",base);
  rect.top=62; rect.bottom=80;
  DrawText(dc,name,-1,&rect,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
  EndPaint(w,&ps); return 0;
 case WM_DESTROY: PostQuitMessage(0); return 0;
 }
 return DefWindowProc(w,msg,wp,lp);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show) {
 WNDCLASS cls; HWND window; MSG msg;
 (void)previous;
 while(command && *command==' ') command++;
 sel_arg=0;
 if(!open_log()) {
  textfmt(message,sizeof(message),"Cannot reserve a new TXT/BIN capture pair. Windows error %lu. Use a writable folder with room for captures.",capture_error);
  MessageBox(NULL,message,
   "VX85598 native 3D test",MB_OK|MB_ICONSTOP); return 1;
 }
 logtext("VX85598 v9.34 client and native driver. DOS and two driver runs proved the minimal recipe: 786432.0 viewport offset on X and Y plus clamp 04/52 = 147FFF. Cases 1-13 now probe native triangle behaviour: winding, shapes, line, Gouraud, colour upload, rectangle and clipping. The XP-captured triangle state (786432.0 viewport bias, clamp 147FFF, mode 00CE02) in separated steps. DOS proved the bias recipe draws a native triangle. 28 bounded FE-context and rectangle cases with captured overrides and cyan controls.\r\n");
 connect_driver(); memset(&cls,0,sizeof(cls)); cls.lpfnWndProc=proc;
 cls.hInstance=instance; cls.hCursor=LoadCursor(NULL,IDC_ARROW);
 cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1); cls.lpszClassName="VX3DTEST6";
 if(!RegisterClass(&cls)) return 1;
 window=CreateWindow(cls.lpszClassName,"VX85598 v9.81 - texture variants",
  WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
  0,0,545,404,NULL,NULL,instance,NULL);
 if(!window) return 1;
 ShowWindow(window,show); UpdateWindow(window);
 while(GetMessage(&msg,NULL,0,0)>0) {TranslateMessage(&msg); DispatchMessage(&msg);}
 if(binary!=INVALID_HANDLE_VALUE) CloseHandle(binary);
 if(device!=INVALID_HANDLE_VALUE) CloseHandle(device);
 CloseHandle(logfile); return 0;
}
