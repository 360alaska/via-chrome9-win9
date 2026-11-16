/* VXREG v1.98 (driver .637). VXREG v1.97 (driver .636). VXREG v1.96 (driver .635: the ring 256 KB on the VX800). VXREG v1.95 (driver .634: the ring 512 KB on the VX800 and VX855). VXREG v1.94 (driver .633: the ring 1024 KB on the VX900 only). VXREG v1.93 (driver .632: ACCEL_VX900_WAITS gone, ACCEL_STRETCH3D 1 by default). VXREG v1.92 (driver .631). VXREG v1.91 (driver .630). VXREG v1.90 (driver .629: the two lines of builds joined: DirectX 5 render states and the driver's T&L on by default, from the other line's .600 and .601). VXREG v1.89 (driver .628: the seam cover, the 3D-engine stretch and the supersampling area printed; the ring 1024 KB by default). VXREG v1.88 (driver .627: the VX900's packets drawn by index). VXREG v1.87 (driver .626: the ring's size printed for every chip; sizes 2048 and 4096). VXREG v1.86 (driver .625: the ring's size printed; the counters' line says since when they count: VXD3DMON reads and clears them). VXREG v1.85 (driver .623: ACCEL_VX900_WAITS, under test, printed; ACCEL_D3D_TEXRUNS no longer read). VXREG v1.84 (driver .621: ACCEL_D3D_TEXRUNS, a test switch, is read again). VXREG v1.83 (driver .620: the counts of the blank wait in the ring). VXREG v1.82 (driver .619: VXREG RESET, the engine reset as after a hang with every sequencer, CRT and display register read before and after, and two questions about the pointer). VXREG v1.81 (driver .618: the device line no longer calls the VX900's 3D engine 'under test'). VXREG v1.80 (driver .617: the VX900's test switch ACCEL_VX900_3D is gone; a value left in the registry is listed for VXREG CLEAN). VXREG v1.79 (driver .616: the count of wraps that waited for the fetcher's lap, in place of .615's). VXREG v1.78 (driver .615: the count of pause addresses written again after a jump). VXREG v1.77 (driver .614: CE's wait words on the VX900, on or off and their count). VXREG v1.76 (driver .613: the register list is 200h-2FCh as it says (v1.75 went on to 5FCh) and is read through a mapping made for it: v1.75's list on a clean start was not the registers). VXREG v1.75 (driver .612: the display registers 200h-2FCh and 120Ch are listed). VXREG v1.74 (driver .611: the build number). VXREG v1.73 (driver .610: the build number). VXREG v1.72 (driver .609: the build number). VXREG v1.71 (driver .608: the build number). VXREG v1.70 (driver .607, the driver unchanged: the device line names the VX900's transport from the switch's value instead of a fixed text, and the ring's counters since the VxD loaded are shown: packets fetched and sent by the registers, XP's ring's starts, kicks and wraps, and whether a ring was given up as hung). VXREG v1.69 (driver .607: the build number; ACCEL_VX900_3D's value is shown: 1 register writes only, 3 the command fetcher and ring too, and the VxD's fetch switch read back). VXREG v1.68 (driver .606: the build number). VXREG v1.67 (driver .605: the build number; ACCEL_VX900_3D, the VX900's 3D engine and Direct3D under test, is a value the driver reads: its setting is reported, and on the VX900 the 3D engine must be used exactly when it is 1). VXREG v1.66 (driver .604: the build number). VXREG v1.65 (driver .603: the build number; ACCEL_VX900_OVERLAY is no longer a value the driver reads). VXREG v1.64 (driver .602: ACCEL_VX900_2D is gone (the VX900's 2D engine confirmed on the board), ACCEL_VX900_OVERLAY, a feature under test, is a value the driver reads; the overlay offered or not is reported). VXREG v1.63 (driver .601: the 2D engine and the 3D engine reported apart (TEXDBG 75 word 4: bit 0 the 3D engine, bit 1 the 2D engine: the VX900 has its 2D engine in use now); ACCEL_VX900_2D, a feature under test, is a value the driver reads). VXREG v1.62 (driver .600: the build number). VXREG v1.61 (driver .599: the build number). VXREG v1.60 (driver .598: the build number it expects from the VxD, left at 597 in v1.59). VXREG v1.59 (driver .598: Direct3D must not be offered on a device whose engines are not used (the VX900 for now): WRONG if it is). VXREG v1.44 (driver .583: the build number). VXREG v1.43 (driver .582: the build number). VXREG v1.42 (driver .581: the DirectX version, whether DirectDraw has the HAL, and where DriverInit ended). VXREG v1.41 (driver .580: the build number; the files are CHROME9.DRV / CHROME9.VXD / CHROME9.INF now). VXREG v1.40 (driver .579: the build number; the VX800 / VX820 (1106:1122) and VX900 (1106:7122) devices beside the VX855's; it prints the device, its BARs and whether the engines are used). VXREG v1.39 (driver .578: the build number). VXREG v1.38 (driver .577: the build number). VXREG v1.37 (driver .576: the build number, and the Flip callback's counts: run it after a game to see whether the game's flips carry DONOTWAIT and how many were refused). VXREG v1.36 (driver .575: the build number; the horizontal store of the driver's lines is permanent, bit 2 of the GDI options is gone). VXREG v1.35 (driver .574: the build number; the OpenGL driver's primitives by index are permanent, VXGL_Indexed is gone). VXREG v1.34 (driver .573: the build number; the OpenGL driver's primitives by index are a feature under test with no registry value (VXGLIX turns them off and on through VXGL_Indexed). VXREG v1.33 (driver .572: the culled pass-through is permanent, bit 29 of VX_D3D_QueryPhases does nothing again; .570-.571's presents in the ring and their TEXDBG BC F3 / F4 / F6 are gone: F6 must not answer; v1.31-v1.32 were .570-.571's). VXREG v1.30 (driver .569: the build number only). VXREG v1.29 (driver .568: the build number only). VXREG v1.28 (driver .567: the build number; bit 29 of VX_D3D_QueryPhases is a test switch again, vx_d3d_cullpass: named, not failed). VXREG v1.27 (driver .566: the build number only). VXREG v1.26 (driver .565: the build number only). VXREG v1.25 (driver .564: the template table and its switch are gone: bit 29 does nothing; the installed names are reported, not counted wrong, since copying the files does not change them). VXREG v1.24 (driver .563: the build number only). VXREG v1.23 (driver .562: the build number; the table's switch is left as it was found, off by default). VXREG v1.22 (driver .561: the build number; a count of 1 with the switch bits set must write one word). VXREG v1.21 (driver .560: the build number only). VXREG v1.20 (driver .559: the build number; the test switch is back for the template table (QueryPhases bit 29 sets it from bit 24, word 343 reads it): it must follow and is left on). VXREG v1.19 (driver .558: the test switch of QueryPhases (bit 29) does nothing; the installed driver's manufacturer is VIA and its provider 360ALASKA). VXREG v1.18 (driver .557: the build number only). VXREG v1.17 (driver .556: the build number only). VXREG v1.16 (driver .555: ACCEL_D3D_IX16 is no longer read: XP's index form always). VXREG v1.15 (driver .554: ACCEL_D3D_IX16, the index bursts in XP's form, a feature under test, default 0). VXREG v1.14 (driver .553: the build number only). VXREG v1.13 (driver .552: the build number only). VXREG v1.12 (driver .543: the build number only). VXREG v1.11 (driver .542: the build number only). VXREG v1.10 (driver .541: the build number only). VXREG v1.9 (driver .540: the build number only). VXREG v1.8 (driver .539: the build number only). VXREG v1.7 (driver .538: the build number only). VXREG v1.6 (driver .537: the build number only). VXREG v1.5 (driver .536: ACCEL_D3D_TNL, the driver's T&L, replaces .534's ACCEL_D3D_TLPROBE). VXREG v1.4 (driver .535: the build number only). VXREG v1.3 (driver .534: ACCEL_D3D_TNL read, the T&L probe, a feature under test). VXREG v1.2 (driver .533: VX_D3D_Switch gone from VXDDRAW.DLL). VXREG v1.1 (driver .531: ACCEL_D3D_IX384 no longer read, VX_D3D_Switch answers no id). v1.0 (driver .530): what .530 changes, on the board.
 * 1. The registry: .530 reads only NOEDIDHIGHRES, VIDMEM_MB, EDID_MODES, CUSTOM_TIMING, TIMING_1152_CVT, ACCEL_D3D_LOG, ACCEL_D3D_SAMPLE,
 *    ACCEL_DEBUG_LOG, ACCEL_PROFILE (VxD), VXGL_LOG, VXGL_FRAMELOG, VXGL_SAMPLE and ACCEL_OPENGL_TSCPROF (VXGL) from
 *    HKLM\Software\VX85598. Every other value there is listed as ignored; VXREG CLEAN deletes them (the driver's own values stay).
 * 2. The switches: VX_D3D_Switch answers no id (.531). The VxD's build is .531 (TEXDBG 75).
 * 3. The removed TEXDBG switch words change nothing: the burst switch (73) and the posted-write switch (79) are sent and the VxD's
 *    answer to them is the default reply (all zero).
 * Results to the screen and VXREG.LOG beside the program. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <ddraw.h>
static FILE *logf;static int fails;static DWORD chip_engines=1; /* v1.59: TEXDBG 75 word 4, whether the device's 3D engine is used (v1.63: bit 0) */
static void say(const char *f,...) {static char b[1024];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);fputs(b,stdout);if(logf){fputs(b,logf);fflush(logf);}}
static const char *const kept[]={"NOEDIDHIGHRES","VIDMEM_MB","EDID_MODES","CUSTOM_TIMING","TIMING_1152_CVT","ACCEL_D3D_LOG","ACCEL_D3D_SAMPLE",
 "ACCEL_DEBUG_LOG","ACCEL_PROFILE","VXGL_LOG","VXGL_FRAMELOG","VXGL_SAMPLE","ACCEL_OPENGL_TSCPROF","ACCEL_D3D_TNL","ACCEL_D3D_RINGKB","ACCEL_D3D_SEAM","ACCEL_STRETCH3D",0}; /* v1.89: .628's two, under test (.629: ACCEL_D3D_SSAA gone with supersampling) */ /* v1.86: ACCEL_D3D_RINGKB (.625, a tuning value under test) */  /* v1.85: ACCEL_VX900_WAITS (.623, under test); ACCEL_D3D_TEXRUNS (.621-.622) gone again */ /* v1.80: ACCEL_VX900_3D (.605-.616) is gone: confirmed on the board with .616 */ /* v1.65: .602's ACCEL_VX900_OVERLAY and .601's ACCEL_VX900_2D are gone: both confirmed on the board */ /* v1.16: ACCEL_D3D_IX16 (.554) gone in .555 */ /* v1.3: ACCEL_D3D_TNL (.534) */
static int is_kept(const char *n) {int k;for(k=0;kept[k];k++)if(!lstrcmpiA(n,kept[k]))return 1;return 0;}
static int is_driver_own(const char *n) {return !strncmp(n,"Driver",6)||!strcmp(n,"DevLoader")||!strcmp(n,"MinorVersion");}
static int has_dev(const char *t) {char u[256];int k;for(k=0;t[k]&&k<255;k++)u[k]=(char)toupper((unsigned char)t[k]);u[k]=0;return strstr(u,"VEN_1106&DEV_5122")!=0||strstr(u,"VEN_1106&DEV_1122")!=0||strstr(u,"VEN_1106&DEV_7122")!=0;} /* v1.40: the three devices of .579 */
static void inf_names(void) { /* v1.19 (.558): what Device Manager shows: the class key's ProviderName, the device key's Mfg */
 HKEY k,k2;DWORD i;char n[128],v[256],id[256];DWORD sz,ty;int seenp=0,seenm=0;
 if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"System\\CurrentControlSet\\Services\\Class\\Display",0,KEY_READ,&k)==ERROR_SUCCESS){
  for(i=0;RegEnumKeyA(k,i,n,sizeof(n))==ERROR_SUCCESS;i++){if(RegOpenKeyExA(k,n,0,KEY_READ,&k2)!=ERROR_SUCCESS)continue;
   sz=sizeof(id);id[0]=0;if(RegQueryValueExA(k2,"MatchingDeviceId",0,&ty,(BYTE*)id,&sz)==ERROR_SUCCESS&&has_dev(id)){
    sz=sizeof(v);v[0]=0;RegQueryValueExA(k2,"ProviderName",0,&ty,(BYTE*)v,&sz);seenp++;
    say("Display\\%s: provider \"%s\"%s\n",n,v,strcmp(v,"360ALASKA")?"  (not this build's name, 360ALASKA: the files were copied over an older install; one install of this INF through Device Manager sets it)":"");}
   RegCloseKey(k2);}
  RegCloseKey(k);}
 if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"Enum\\PCI",0,KEY_READ,&k)==ERROR_SUCCESS){
  for(i=0;RegEnumKeyA(k,i,n,sizeof(n))==ERROR_SUCCESS;i++){DWORD j;char m[128];if(!has_dev(n)||RegOpenKeyExA(k,n,0,KEY_READ,&k2)!=ERROR_SUCCESS)continue;
   for(j=0;RegEnumKeyA(k2,j,m,sizeof(m))==ERROR_SUCCESS;j++){HKEY k3;if(RegOpenKeyExA(k2,m,0,KEY_READ,&k3)!=ERROR_SUCCESS)continue;
    sz=sizeof(v);v[0]=0;RegQueryValueExA(k3,"Mfg",0,&ty,(BYTE*)v,&sz);seenm++;
    say("Enum\\PCI\\%s\\%s: manufacturer \"%s\"%s\n",n,m,v,strcmp(v,"VIA")?"  (not this build's name, VIA: as above)":"");RegCloseKey(k3);}
   RegCloseKey(k2);}
  RegCloseKey(k);}
 if(!seenp){say("the display class key of the Chrome9 device: WRONG: not found\n");fails++;}
 if(!seenm){say("the device key of the Chrome9 device: WRONG: not found\n");fails++;}
}
/* v1.42 (.581; VX800 board: DirectDraw ran without the HAL, and only the tools that need DirectDraw 7 would have said why) */
static void dd_state(void) {
 HKEY k;char v[64]="not recorded";DWORD n=sizeof(v)-1,t=0;HMODULE dd,h;LPDIRECTDRAW d=0;HRESULT hr;
 HRESULT (WINAPI *create)(GUID*,LPDIRECTDRAW*,IUnknown*);
 if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"Software\\Microsoft\\DirectX",0,KEY_READ,&k)==ERROR_SUCCESS){if(RegQueryValueExA(k,"Version",0,&t,(BYTE*)v,&n)!=ERROR_SUCCESS||t!=REG_SZ)strcpy(v,"not recorded");RegCloseKey(k);}
 dd=LoadLibraryA("DDRAW.DLL");
 say("DirectX: version %s (4.06 = DirectX 6, 4.07 = 7, 4.08 = 8, 4.09 = 9); DirectDrawCreateEx (DirectDraw 7) %s\n",v,dd&&GetProcAddress(dd,"DirectDrawCreateEx")?"there":"NOT THERE: DirectDraw, Direct3D and OpenGL acceleration need DirectX 7 or later (the desktop is accelerated on any; OpenGL programs run on Microsoft's software renderer); VXDX7, VXFLIPF, VXGL, VXSTEN, VXTEX need it too");
 if(!dd||!(create=(HRESULT (WINAPI*)(GUID*,LPDIRECTDRAW*,IUnknown*))GetProcAddress(dd,"DirectDrawCreate"))){say("DirectDraw: WRONG: DDRAW.DLL not usable\n");fails++;return;}
 hr=create(0,&d,0);
 if(hr!=DD_OK||!d){say("DirectDrawCreate: WRONG: %08lX\n",(unsigned long)hr);fails++;}
 else{DDCAPS c;memset(&c,0,sizeof(c));c.dwSize=sizeof(c);hr=IDirectDraw_GetCaps(d,&c,0);
  if(hr!=DD_OK){c.dwSize=sizeof(DDCAPS_DX5);hr=IDirectDraw_GetCaps(d,&c,0);}
  if(hr!=DD_OK){say("DirectDraw GetCaps: WRONG: %08lX\n",(unsigned long)hr);fails++;}
  else{int nohw=(c.dwCaps&0x02000000UL)!=0;say("DirectDraw: the HAL is %s; caps %08lX, surface caps %08lX, video memory %lu KB in all, %lu KB free\n",nohw?"NOT USED (WRONG: emulation only)":"used",c.dwCaps,c.ddsCaps.dwCaps,c.dwVidMemTotal>>10,c.dwVidMemFree>>10);if(nohw)fails++;say("   the video overlay: %s\n",(c.dwCaps&0x800UL)?"offered":"not offered");}} /* v1.64 */
 h=GetModuleHandleA("VXDDRAW.DLL");
 if(h){DWORD (WINAPI *qp)(DWORD*,DWORD)=(DWORD (WINAPI*)(DWORD*,DWORD))GetProcAddress(h,"VX_D3D_QueryPhases");
  DWORD (WINAPI *qt)(DWORD*,DWORD)=(DWORD (WINAPI*)(DWORD*,DWORD))GetProcAddress(h,"VX_D3D_QueryTrace");static DWORD f[366],tr[64];
  if(qp&&qp(f,366)==366){static const char *const why[8]={"","no context from the display driver","the shared block is not this build's (size) or has no frame buffer record","the VxD did not answer","the VxD's bring-up is not complete (stage, failure or VGA mode)","the VxD's 2D record is not this build's","the depth is not 8, 16 or 32","the frame buffer's address differs between the display driver and the VxD"};
   say("DriverInit (the HAL's start, called by DirectDraw): %lu call(s); the last %s%s; the VxD then: stage %lu, failure %lu, VGA mode %lu, %lu bits\n",f[363],f[364]==0x100?"was accepted":f[363]?"was REFUSED: ":"-",f[364]>=1&&f[364]<=7?why[f[364]]:"",f[365]&255,(f[365]>>8)&255,(f[365]>>16)&255,f[365]>>24);
   if(!f[363]||f[364]!=0x100){say("   WRONG: DirectDraw has not started the HAL\n");fails++;}}
  else{say("QueryPhases words 363-365 (DriverInit): WRONG: not answered\n");fails++;}
  if(qt&&qt(tr,64)>=8){say("the display driver's DirectDraw record: built %lu time(s); DirectDraw's answer to the last %s; Direct3D %s; caps given %08lX, surface caps %08lX; the Direct3D HAL started %lu time(s)\n",tr[4],tr[4]?((tr[5]&1)?"accepted":"REFUSED"):"-",(tr[5]&0x100)?"offered":"not offered",tr[6],tr[7],tr[1]);
  if(!chip_engines&&(tr[5]&0x100)){say("   WRONG: Direct3D is offered although the device's 3D engine is not used\n");fails++;}}} /* v1.59 */
 if(d)IDirectDraw_Release(d);
}
/* v1.82 (.619): VXREG RESET. The engine is reset as the driver does after a hang (TEXDBG BC C00), with the sequencer registers, the CRT
 * registers, MMIO 200-2FC, 120C and 400 read before and after (BC A00.., B00..). Every register that differs is listed, then the
 * person at the screen says whether the pointer is right, at once and again after its shape has changed. Log: VXRESET.LOG. */
static int reset_run(void) {
 HANDLE h=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);DWORD in[2],o[8],got,snap[2][198];int w,n,k,diff=0,a1,a2;char path[MAX_PATH],*s;
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXRESET.LOG");logf=fopen(path,"w");
 say("VXREG v1.82 RESET (driver .619): the engine reset as after a hang, the registers before and after\n");
 if(h==INVALID_HANDLE_VALUE){say("the VxD does not open\n");if(logf)fclose(logf);return 2;}
 in[0]=0x75;memset(o,0,sizeof(o));got=0;DeviceIoControl(h,0x85500022UL,in,4,o,32,&got,0);say("the VxD's build: .%lu, the device 1106:%04lX\n",o[1],o[2]);
 if(o[1]<619){say("needs driver .619 or later\n");CloseHandle(h);if(logf)fclose(logf);return 2;}
 if(MessageBoxA(0,"The 3D engine will be reset once, as the driver does after a hang.\nClose every game first.\n\nIs the mouse pointer right NOW (it follows the mouse left-right and up-down)?","VXREG RESET",MB_YESNO|MB_TOPMOST)!=IDYES)say("before the reset the pointer was already WRONG (restart Windows and run this first)\n");
 else say("before the reset: the pointer is right\n");
 in[0]=0xBC;in[1]=0xC00;memset(o,0,sizeof(o));got=0;DeviceIoControl(h,0x85500022UL,in,8,o,32,&got,0);
 say("the reset: %s\n",o[2]==1?"done, the engine idle again":o[2]==2?"not done (the engine busy or its registers not mapped)":o[2]==3?"FAILED (the device is lost; restart Windows)":"no answer");
 if(o[2]!=1){CloseHandle(h);if(logf)fclose(logf);return 2;}
 for(w=0;w<2;w++)for(n=0;n<33;n++){in[0]=0xBC;in[1]=0xA00+w*0x100+n;memset(o,0,sizeof(o));got=0;DeviceIoControl(h,0x85500022UL,in,8,o,32,&got,0);for(k=0;k<6;k++)snap[w][n*6+k]=o[2+k];}
 CloseHandle(h);
 for(w=0;w<2;w++){static const char *nm[2]={"SR","CR"};
  for(k=0;k<256;k++){unsigned b0=(snap[0][w*64+(k>>2)]>>((k&3)*8))&0xFF,b1=(snap[1][w*64+(k>>2)]>>((k&3)*8))&0xFF;if(b0!=b1){say("   differs: %s%02X %02X -> %02X\n",nm[w],k,b0,b1);diff++;}}}
 for(k=0;k<64;k++)if(snap[0][128+k]!=snap[1][128+k]){say("   differs: MMIO %03X %08lX -> %08lX%s\n",0x200+k*4,snap[0][128+k],snap[1][128+k],k==2?" (a counter: expected)":"");diff++;}
 if(snap[0][192]!=snap[1][192]){say("   differs: MMIO 120C %08lX -> %08lX\n",snap[0][192],snap[1][192]);diff++;}
 say("status 400: %08lX before, %08lX after; %d register(s) differ\n",snap[0][193],snap[1][193],diff);
 for(w=0;w<2;w++){say("%s the reset:\n",w?"after":"before");
  for(n=0;n<2;n++){say("   %s 00-FF:\n",n?"CR":"SR");for(k=0;k<256;k+=16){int j;char line[80];char *q=line;for(j=0;j<16;j++)q+=sprintf(q," %02X",(unsigned)((snap[w][n*64+((k+j)>>2)]>>(((k+j)&3)*8))&0xFF));say("      %02X:%s\n",k,line);}}
  say("   MMIO 200-2FC:\n");for(k=0;k<64;k+=8)say("      %03X: %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX\n",0x200+k*4,snap[w][128+k],snap[w][129+k],snap[w][130+k],snap[w][131+k],snap[w][132+k],snap[w][133+k],snap[w][134+k],snap[w][135+k]);}
 a1=MessageBoxA(0,"The engine has been reset.\n\nMove the mouse left-right and up-down.\nIs the pointer right (in place, following both ways)?","VXREG RESET",MB_YESNO|MB_TOPMOST)==IDYES;
 say("after the reset: the pointer is %s\n",a1?"right":"WRONG");
 a2=MessageBoxA(0,"Now move the pointer over a window's edge so it changes to the resize arrows and back.\n\nIs the pointer right NOW?","VXREG RESET",MB_YESNO|MB_TOPMOST)==IDYES;
 say("after its shape changed: the pointer is %s\n",a2?"right":"WRONG");
 if(!a1||!a2)say("the pointer stays wrong until Windows restarts or the display mode changes; send VXRESET.LOG\n");
 if(logf)fclose(logf);return 0;
}
int main(int argc,char **argv) {
 HKEY k;DWORD i,n,t;char name[256];char path[MAX_PATH],*s;int clean=argc>1&&!lstrcmpiA(argv[1],"CLEAN"),ignored=0,del=0;HANDLE vxd;
 if(argc>1&&!lstrcmpiA(argv[1],"RESET"))return reset_run(); /* v1.82 */
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXREG.LOG");logf=fopen(path,"w");
 say("VXREG v1.98 (driver .637): the registry values the driver still reads%s\n",clean?"; CLEAN: the ignored values deleted":"");
 if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"Software\\VX85598",0,KEY_READ|(clean?KEY_SET_VALUE:0),&k)==ERROR_SUCCESS) {
  char gone[64][64];int ng=0;
  for(i=0;;i++){n=sizeof(name);if(RegEnumValueA(k,i,name,&n,0,&t,0,0)!=ERROR_SUCCESS)break;
   if(is_kept(name))say("   read:    %s\n",name);
   else if(!is_driver_own(name)){say("   ignored: %s\n",name);ignored++;if(clean&&ng<64){lstrcpynA(gone[ng],name,64);ng++;}}}
  for(i=0;i<(DWORD)ng;i++)if(RegDeleteValueA(k,gone[i])==ERROR_SUCCESS)del++;
  RegCloseKey(k);
  say("%d value(s) the driver no longer reads%s\n",ignored,clean?"":" (VXREG CLEAN deletes them)");if(clean)say("%d deleted\n",del);
 } else say("HKLM\\Software\\VX85598: not there (nothing set)\n");
 {char dll[MAX_PATH];UINT m=GetSystemDirectoryA(dll,MAX_PATH-16);HMODULE h=0; /* v1.2 (.533): VX_D3D_Switch removed from the HAL */
  if(m&&m<MAX_PATH-16){strcpy(dll+m,dll[m-1]=='\\'?"VXDDRAW.DLL":"\\VXDDRAW.DLL");h=LoadLibraryA(dll);}
  if(!h){say("VXDDRAW.DLL not found\n");fails++;}
  else{int gone=GetProcAddress(h,"VX_D3D_Switch")==0;say("VX_D3D_Switch: %s\n",gone?"gone (no switch is left)":"WRONG: still exported");if(!gone)fails++;
   {DWORD (WINAPI *qp)(DWORD*,DWORD)=(DWORD (WINAPI*)(DWORD*,DWORD))GetProcAddress(h,"VX_D3D_QueryPhases");static DWORD w[356];DWORD a=9,b=9; /* v1.19 (.558) */
    if(!qp){say("VX_D3D_QueryPhases: WRONG: not exported\n");fails++;}
    else{{static DWORD g[8];DWORD k2,over=0;for(k2=0;k2<8;k2++)g[k2]=0xA5A5A5A5UL;qp(g,1UL|0x0F000000UL);for(k2=1;k2<8;k2++)if(g[k2]!=0xA5A5A5A5UL)over++;
      say("a count of 1 with the old switch bits set: %s\n",over?"WRONG: more than one word written":"one word written");if(over)fails++;}
     qp(w,1UL|0x20000000UL);if(qp(w,350)==350)a=w[343];qp(w,1UL|0x30000000UL);if(qp(w,350)==350)b=w[343]; /* v1.33: it switches nothing in either position, so nothing is left changed */
     say("QueryPhases bit 29 (the test switch of .555-.557, .559-.563 and .567-.569): %s\n",!a&&!b?"does nothing (word 343 reads 0 either way)":"WRONG: it still switches");if(a||b)fails++;
     {static DWORD f[363];if(qp(f,363)==363)say("the Flip callback since the driver loaded (.576): %lu calls, %lu with DONOTWAIT, %lu with NOVSYNC, last flags %08lX, last answer %08lX; %lu refused for a flip still pending (DONOTWAIT only)\n",f[356],f[357],f[358],f[360],f[361],f[362]);
      else{say("QueryPhases words 356-362 (the Flip callback's counts): WRONG: not answered\n");fails++;}}}}}}
 inf_names();
 vxd=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);
 if(vxd==INVALID_HANDLE_VALUE){say("the VxD: not open\n");fails++;}
 else{DWORD in,o[8],got=0;
  in=0x75;memset(o,0,sizeof(o));DeviceIoControl(vxd,0x85500022UL,&in,4,o,32,&got,0);say("the VxD's build: .%lu\n",o[1]);if(o[1]!=637){say("   WRONG: not .637 (the driver files of this package are not the ones installed)\n");fails++;}
  say("the device: 1106:%04lX (%s); the frame buffer's BAR at configuration %02lXh = %08lX, the registers' at %02lXh = %08lX; the 2D engine %s, the 3D engine %s; bring-up stage %lu, failure %lu\n",o[2],o[2]==0x1122?"VX800 / VX820, Chrome9 HC3":o[2]==0x5122?"VX855 / VX875, Chrome9 HCM":o[2]==0x7122?"VX900, Chrome9 HD":"WRONG: not one of the three",o[3]&255,o[5],(o[3]>>8)&255,o[6],(o[4]&2)?"used":"not used",(o[4]&1)?(o[2]==0x7122?"used (VX900: always from .617)":"used"):"not used (VX900: ACCEL_VX900_3D is not 1: no Direct3D, OpenGL in software)",o[7]&255,o[7]>>8);
  if(o[2]!=0x1122&&o[2]!=0x5122&&o[2]!=0x7122)fails++;
  chip_engines=o[4]&1;
  {DWORD ir[2],orr[8],gr=0;HANDLE hr2=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);memset(orr,0,sizeof(orr));ir[0]=0xBC;ir[1]=0x442;if(hr2!=INVALID_HANDLE_VALUE){DeviceIoControl(hr2,0x85500022UL,ir,8,orr,32,&gr,0);CloseHandle(hr2);}
   say("   the command ring's size (ACCEL_D3D_RINGKB, a tuning value: 256 (the default on the VX800), 512 (the default on the VX855), 1024 (the default on the VX900), 2048 or 4096; halved when the pages cannot be had): %lu KB\n",orr[2]);}
  {DWORD i5[2],o5[8],g5=0;HANDLE h5=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);memset(o5,0,sizeof(o5));i5[0]=0xBC;i5[1]=0x445;if(h5!=INVALID_HANDLE_VALUE){DeviceIoControl(h5,0x85500022UL,i5,8,o5,32,&g5,0);
    say("   the seam cover (ACCEL_D3D_SEAM, a tuning value: 0 off (the default), 4112 = one pixel right and down; x + 256 * y sixteenths of a pixel): %lu; %lu packet(s) drawn twice, %lu indices sent again\n",o5[2],o5[3],o5[4]);
    memset(o5,0,sizeof(o5));i5[1]=0x446;DeviceIoControl(h5,0x85500022UL,i5,8,o5,32,&g5,0);
    say("   stretching Blts by the 3D engine (ACCEL_STRETCH3D, a tuning value: 0 the CPU, 1 nearest pixel (the default from .632), 2 bilinear): %lu\n",o5[2]);
    CloseHandle(h5);}}
  if(o[2]==0x7122&&!(o[4]&2)){say("   WRONG: the VX900's 2D engine is not used\n");fails++;}
  if(o[2]==0x7122){
   {DWORD i9=0xA3,o9[8];DWORD g9=0;HANDLE h9=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);memset(o9,0,sizeof(o9));if(h9!=INVALID_HANDLE_VALUE){DeviceIoControl(h9,0x85500022UL,&i9,4,o9,32,&g9,0);CloseHandle(h9);}
    say("   the VX900's 3D engine and Direct3D: always used from .617 (no switch); the VxD's command fetch is %s\n",o9[0]==0xA1?"on":o9[0]==0xA2?"off":"not answered");
    if(o9[0]==0xA2){say("   WRONG: the command fetch is off\n");fails++;}
    {DWORD iw[2],ow[8],gw=0;HANDLE hw=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);memset(ow,0,sizeof(ow));iw[0]=0xBC;iw[1]=0x441;if(hw!=INVALID_HANDLE_VALUE){DeviceIoControl(hw,0x85500022UL,iw,8,ow,32,&gw,0);CloseHandle(hw);}
     say("   CE's wait words after a burst of a fetched packet (.614): %lu sent (.632: the last block only, always; no switch)\n",ow[3]);
     {DWORD ii[2],oi[8],gi=0;HANDLE hi=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);memset(oi,0,sizeof(oi));ii[0]=0xBC;ii[1]=0x443;if(hi!=INVALID_HANDLE_VALUE){DeviceIoControl(hi,0x85500022UL,ii,8,oi,32,&gi,0);CloseHandle(hi);}
      say("   packets drawn by index from the vertex area (.627): %lu, with %lu vertices (0 after a game with indexed draws = every one was expanded into the command)\n",oi[2],oi[3]);}
     say("   wraps that first waited for the fetcher to reach the writer's lap (.616): %lu; flips in the ring: not used on the VX900\n",ow[4]);
     say("   the wait for the blank after a CPU flip (.620): %lu block(s) kicked in the ring, %lu packet(s) that did not wait in the CPU; %lu time(s) not queued (the CPU waited)\n",ow[6],ow[5],ow[7]);}
    {DWORD ib[2],r1[8],r2[8];HANDLE hb=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);memset(r1,0,sizeof(r1));memset(r2,0,sizeof(r2));
     if(hb!=INVALID_HANDLE_VALUE){ib[0]=0xBA;ib[1]=0;g9=0;DeviceIoControl(hb,0x85500022UL,ib,4,r1,32,&g9,0);ib[0]=0xBC;ib[1]=0x80;g9=0;DeviceIoControl(hb,0x85500022UL,ib,8,r2,32,&g9,0);CloseHandle(hb);}
     say("   since VXD3DMON last read and cleared them (or since the VxD loaded): %lu packets fetched, %lu sent by the registers instead, %lu fetches that did not finish; XP's ring %s: %lu starts, %lu kicks, %lu wraps; rings given up as hung: %lu; a ring hang on record: %s\n",o9[1],o9[3],o9[4],r1[0]==0xBA?(r1[1]?"on":"off"):"not answered",r1[2],r1[3],r1[4],r2[5],(r2[0]>>8)?"YES":"no");} /* v1.70 */}
   if(!(o[4]&1)){say("   WRONG: the VX900's 3D engine is not used\n");fails++;}} /* v1.80 */
  {static const DWORD w[2]={0x73,0x79};int j,k2;for(j=0;j<2;j++){in=w[j];memset(o,0xEE,sizeof(o));DeviceIoControl(vxd,0x85500022UL,&in,4,o,32,&got,0);
   for(k2=0;k2<8&&!o[k2];k2++);say("TEXDBG %02lX (a removed switch word): %s\n",w[j],k2==8?"the default reply, nothing switched":"WRONG: it still answers");if(k2!=8)fails++;}}
  /* v1.75 (.612): the display registers 200h-2FCh and 120Ch (the hardware cursor's and the video streams'): one run on a clean
   * start and one after a game has left the pointer wrong show which register changed */
  {DWORD in3[2],o3[8],k4,j4;say("display registers 200h-2FCh (four a line) and 120Ch:\n");
   for(k4=0;k4<16;k4++){in3[0]=0xBC;in3[1]=0x400+k4;memset(o3,0,sizeof(o3));got=0;DeviceIoControl(vxd,0x85500022UL,in3,8,o3,32,&got,0);
    if(!(k4&1))say("   %03lX:",0x200+k4*16);for(j4=0;j4<4;j4++)say(" %08lX",o3[2+j4]);if(k4&1)say("\n");}
   in3[0]=0xBC;in3[1]=0x440;memset(o3,0,sizeof(o3));got=0;DeviceIoControl(vxd,0x85500022UL,in3,8,o3,32,&got,0);say("   120C: %08lX\n",o3[2]);}
  {DWORD in2[2],o2[8],k3;in2[0]=0xBC;in2[1]=0xF6;memset(o2,0,sizeof(o2));DeviceIoControl(vxd,0x85500022UL,in2,8,o2,32,&got,0);for(k3=2;k3<8&&!o2[k3];k3++);
   say("TEXDBG BC F6 (.570-.571: presents in the ring, removed): %s\n",k3==8?"no answer, nothing switched":"WRONG: it still answers");if(k3!=8)fails++;}
  CloseHandle(vxd);}
 dd_state();
 say("%s: %d wrong\n",fails?"FAILED":"PASSED",fails);
 if(logf)fclose(logf);
 return fails?2:0;
}
