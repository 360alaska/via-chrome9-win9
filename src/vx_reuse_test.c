/* VXREUSE v1.0 (driver .707): a program's own handle must survive Direct3D in a process whose ID Windows 98 gave again.
 *
 * T700, T702 and T706 (VX855): VXNOSYNC run after 3DMark lost its log after its first line, though every pass ran. Windows 98 gives a new
 * process the ID of one that has ended, and DirectDraw can call the HAL (VXDDRAW.DLL) in a process the DLL was never attached to, so the
 * HAL's handle slot under that ID still held the ended process's handle value. The first call on it failed, the HAL opened a new handle
 * and then CLOSED the old value: in the new process that number was one of the program's own handles (VXNOSYNC's log file). .707 drops
 * the old value without closing it.
 *
 * This program starts itself 24 times in a row (VXREUSE CHILD n). Each child is the same program doing the same things in the same order,
 * so its handle numbers are the ones the child before it had: it opens a file of its own and keeps it open, makes a Direct3D 8 device
 * (windowed, 64 x 64), clears and presents 20 frames, releases it, and then writes 4 KB to its file and reads the size back. A child
 * whose write fails or whose file is short had its handle closed under it. The parent counts the children, the process IDs given
 * more than once, and the failures. RIGHT: no child lost its handle (on .706 a child after a reused ID could). Results to VXREUSE.LOG.
 * Runs in about 20 seconds; leave the machine alone. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef unsigned long U32;
#define VT(o,i) (((void**)(*(void***)(o)))[i])
typedef HRESULT (__stdcall *F0)(void*);
enum {D3D_REL=2,D3D_MODE=8,D3D_CREATEDEV=15};
enum {DEV_PRESENT=15,DEV_BEGIN=34,DEV_END=35,DEV_CLEAR=36};
typedef struct {U32 w,h,refresh,format;} MODE;
typedef struct {U32 bw,bh,bfmt,bcount,ms,swap;HWND wnd;U32 windowed,autoz,zfmt,flags,rate,interval;} PP;
#define CHILDREN 24
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProc(h,m,w,l);}
static void here(char *path,const char *name) {char *s;GetModuleFileName(0,path,MAX_PATH);s=strrchr(path,'\\');strcpy(s?s+1:path,name);}
/* one line appended to VXREUSE.LOG, the file opened and closed for it (the parent and each child) */
static void logline(const char *t) {char path[MAX_PATH];HANDLE f;DWORD w;here(path,"VXREUSE.LOG");
 f=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(f==INVALID_HANDLE_VALUE)return;
 SetFilePointer(f,0,0,FILE_END);WriteFile(f,t,(DWORD)strlen(t),&w,0);CloseHandle(f);}
/* the child: returns 0 when its own file handle still works after the Direct3D work, 1 when not, 2 when Direct3D could not be used */
static int child(int n) {
 char path[MAX_PATH],line[300],buf[4096];HANDLE own;HMODULE d8;void *(__stdcall *create)(U32);void *d3d,*dev=0;MODE mode;PP pp;WNDCLASS wc;HWND wnd;
 DWORD w=0,size;int k,ok;HRESULT hr;
 here(path,"VXREUSE.TMP");
 own=CreateFileA(path,GENERIC_READ|GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0); /* this child's own handle, kept open */
 if(own==INVALID_HANDLE_VALUE){sprintf(line,"child %2d (process %08lX): its own file could not be made\r\n",n,GetCurrentProcessId());logline(line);return 2;}
 d8=LoadLibrary("d3d8.dll");create=d8?(void*(__stdcall*)(U32))GetProcAddress(d8,"Direct3DCreate8"):0;
 d3d=create?create(220):0;if(!d3d&&create)d3d=create(120);
 if(!d3d){CloseHandle(own);sprintf(line,"child %2d: Direct3D 8 not available\r\n",n);logline(line);return 2;}
 memset(&mode,0,sizeof(mode));((HRESULT(__stdcall*)(void*,U32,MODE*))VT(d3d,D3D_MODE))(d3d,0,&mode);
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wp;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="VXREUSE";RegisterClass(&wc);
 wnd=CreateWindow("VXREUSE","VXREUSE",WS_OVERLAPPED|WS_VISIBLE,20+n*4,20+n*4,80,100,0,0,wc.hInstance,0);
 memset(&pp,0,sizeof(pp));pp.bw=64;pp.bh=64;pp.bfmt=mode.format;pp.bcount=1;pp.swap=1;pp.wnd=wnd;pp.windowed=1; /* D3DSWAPEFFECT_DISCARD, windowed */
 hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,0x20,&pp,&dev);
 if(hr||!dev){((F0)VT(d3d,D3D_REL))(d3d);DestroyWindow(wnd);CloseHandle(own);sprintf(line,"child %2d: CreateDevice failed, %08lX\r\n",n,(U32)hr);logline(line);return 2;}
 for(k=0;k<20;k++){MSG m;while(PeekMessage(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}
  ((F0)VT(dev,DEV_BEGIN))(dev);
  ((HRESULT(__stdcall*)(void*,U32,void*,U32,U32,float,U32))VT(dev,DEV_CLEAR))(dev,0,0,1,0xFF000000UL|(U32)(k*12)<<8,1.0f,0);
  ((F0)VT(dev,DEV_END))(dev);
  ((HRESULT(__stdcall*)(void*,void*,void*,void*,void*))VT(dev,DEV_PRESENT))(dev,0,0,0,0);}
 ((F0)VT(dev,D3D_REL))(dev);((F0)VT(d3d,D3D_REL))(d3d);DestroyWindow(wnd);
 memset(buf,'V',sizeof(buf));
 ok=WriteFile(own,buf,sizeof(buf),&w,0)&&w==sizeof(buf);
 size=GetFileSize(own,0);ok=ok&&size==sizeof(buf);
 sprintf(line,"child %2d (process %08lX, own handle %08lX): %s (written %lu, size %lu)\r\n",n,GetCurrentProcessId(),(U32)own,ok?"its handle still works":"ITS HANDLE WAS CLOSED UNDER IT",w,size);
 logline(line);CloseHandle(own);
 return ok?0:1;
}
int main(int argc,char **argv) {
 char path[MAX_PATH],cmd[MAX_PATH+40],line[300];DWORD pids[CHILDREN];int n,i,j,bad=0,na=0,reused=0;
 if(argc>2&&!lstrcmpi(argv[1],"CHILD"))return child(atoi(argv[2]));
 here(path,"VXREUSE.LOG");DeleteFileA(path);
 logline("VXREUSE v1.0 (driver .707): 24 copies of this program run one after another, each holding a file of its own open across Direct3D 8 work\r\n");
 GetModuleFileName(0,path,MAX_PATH);
 for(n=0;n<CHILDREN;n++){STARTUPINFO si;PROCESS_INFORMATION pi;DWORD code=3;
  memset(&si,0,sizeof(si));si.cb=sizeof(si);sprintf(cmd,"\"%s\" CHILD %d",path,n);pids[n]=0;
  if(!CreateProcess(0,cmd,0,0,FALSE,0,0,0,&si,&pi)){sprintf(line,"child %2d: could not be started (%lu)\r\n",n,GetLastError());logline(line);na++;continue;}
  pids[n]=pi.dwProcessId;WaitForSingleObject(pi.hProcess,30000);GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
  if(code==1)bad++;else if(code!=0)na++;}
 for(i=0;i<CHILDREN;i++)for(j=0;j<i;j++)if(pids[i]&&pids[i]==pids[j]){reused++;break;}
 sprintf(line,"children %d, process IDs given again %d, children whose handle was closed under them %d, children that could not use Direct3D %d\r\n",CHILDREN,reused,bad,na);logline(line);
 sprintf(line,"%s\r\n",bad?"WRONG: a child lost its own handle (the HAL closed it)":na==CHILDREN?"NOT RUN: Direct3D 8 could not be used":reused?"RIGHT: no child lost its handle, IDs were given again":"RIGHT: no child lost its handle (no ID was given again this run: run it again after other programs)");logline(line);
 return bad?1:0;
}
