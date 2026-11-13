/* VXSEL v1.0 (driver .514): names 16-bit code selectors for VXD3DMON's "16-bit" lines (a Win16 program: TOOLHELP sees the 16-bit global
 * heap). VXSEL 003B 014F ... writes VXSEL.TXT: each selector's base, limit, owning module and segment number, or that it is not in the
 * global heap (a GDT selector the system keeps). With no arguments it reads the selectors from the first "16-bit XXXX:" lines of VXD3DMON.TXT. */
#include <windows.h>
#include <toolhelp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void one(FILE *f,WORD sel) {
 GLOBALENTRY ge;MODULEENTRY me;DWORD base=GetSelectorBase(sel),lim=GetSelectorLimit(sel);HGLOBAL h;
 fprintf(f,"%04X: base %08lX limit %08lX",sel,base,lim);
 memset(&ge,0,sizeof(ge));ge.dwSize=sizeof(ge);h=(HGLOBAL)GlobalHandle(sel);
 if(h&&GlobalEntryHandle(&ge,(HANDLE)h)){
  memset(&me,0,sizeof(me));me.dwSize=sizeof(me);
  if(ge.hOwner&&ModuleFindHandle(&me,(HMODULE)ge.hOwner))fprintf(f,"; module %s (%s), segment %u, type %u\n",me.szModule,me.szExePath,ge.wData,ge.wType);
  else fprintf(f,"; global heap block, owner %04X (not a module: a task or the system), type %u\n",(unsigned)ge.hOwner,ge.wType);}
 else fprintf(f,"; not in the 16-bit global heap (%s selector: one the system keeps)\n",(sel&4)?"an LDT":"a GDT");
}
int PASCAL WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 FILE *f=fopen("VXSEL.TXT","w");char buf[64];int n=0;(void)hi;(void)hp;(void)show;
 if(!f)return 1;
 fprintf(f,"VXSEL v1.0 (driver .514)\n");
 while(*cmd){unsigned long v;char *e;while(*cmd==' ')cmd++;if(!*cmd)break;v=strtoul(cmd,&e,16);if(e==cmd)break;cmd=e;one(f,(WORD)v);n++;}
 if(!n){FILE *m=fopen("VXD3DMON.TXT","r");char line[400];
  if(m){while(fgets(line,sizeof(line),m)){char *p=strstr(line,"16-bit ");unsigned long v;if(p&&sscanf(p+7,"%lx:",&v)==1&&strchr(p,':')){one(f,(WORD)v);n++;}}fclose(m);}
  if(!n)fprintf(f,"no selectors: VXSEL 003B 014F, or VXD3DMON.TXT in this folder\n");}
 fclose(f);
 sprintf(buf,"VXSEL: %d selector(s), see VXSEL.TXT",n);MessageBox(0,buf,"VXSEL",MB_OK);
 return 0;
}
