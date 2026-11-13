/* Native GPU triangle probe independent of DirectDraw registration. MIT 2026.
 * Default: query only. --draw: five-second fullscreen hardware probe, ESC exits.
 * No offscreen address is borrowed from the runtime's unreserved heap. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "vx_native3d.h"
static HANDLE device=INVALID_HANDLE_VALUE;
static vx3dn_info info;
static FILE *logfile;
static uint32_t frame,failed;
static uint32_t fb(float f) {uint32_t b;memcpy(&b,&f,4);return b;}
static int ioctl(uint32_t c,const void *p,uint32_t n,void *out,uint32_t bytes) {
 DWORD got=0;return DeviceIoControl(device,c,(void*)p,n,out,bytes,&got,0)&&got==bytes;
}
static uint32_t submit(vx3dn_packet *p) {uint32_t r=VX3DN_LOST;if(!ioctl(VX_IOCTL_3DN_SUBMIT,p,sizeof(*p),&r,4))return VX3DN_LOST;return r;}
static void draw(HWND window) {
 vx3dn_packet p;uint32_t rc;float w=(float)info.width,h=(float)info.height;
 if(GetForegroundWindow()!=window)return;
 memset(&p,0,sizeof(p));p.cb=sizeof(p);p.abi=VX3DN_ABI;p.arm=VX3DN_ARM;p.generation=info.generation;
 p.target.pitch=info.pitch;p.target.width=info.width;p.target.height=info.height;p.target.format=info.bpp==16?VX3DN_RGB565:VX3DN_XRGB8888;
 p.clip.right=(int32_t)info.width;p.clip.bottom=(int32_t)info.height;p.z_func=8;p.write_mask=15;
 p.op=VX3DN_CLEAR;p.clear_flags=VX3DN_CLEAR_TARGET;p.clear_color=0xFF101820UL;
 rc=submit(&p);
 if(!rc) {
  p.op=VX3DN_DRAW;p.vertex_count=3;
  p.vertices[0].x=fb(w*0.5F);p.vertices[0].y=fb(h*0.15F);p.vertices[0].color=0xFFFF0000UL;
  p.vertices[1].x=fb(w*0.85F);p.vertices[1].y=fb(h*0.85F);p.vertices[1].color=0xFF00FF00UL;
  p.vertices[2].x=fb(w*0.15F);p.vertices[2].y=fb(h*0.85F);p.vertices[2].color=0xFF0000FFUL;
  p.vertices[0].z=p.vertices[1].z=p.vertices[2].z=fb(0.5F);
  p.vertices[0].rhw=p.vertices[1].rhw=p.vertices[2].rhw=fb(1.0F);
  rc=submit(&p);
 }
 fprintf(logfile,"frame %lu result %lu\n",(unsigned long)frame,(unsigned long)rc);fflush(logfile);
 if(rc){failed=rc;DestroyWindow(window);return;}
 if(++frame>=50)DestroyWindow(window);
}
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_CREATE){SetTimer(w,1,100,0);return 0;}
 if(m==WM_TIMER){draw(w);return 0;}
 if((m==WM_KEYDOWN&&a==VK_ESCAPE)||m==WM_LBUTTONDOWN){DestroyWindow(w);return 0;}
 if(m==WM_PAINT){PAINTSTRUCT p;BeginPaint(w,&p);EndPaint(w,&p);return 0;}
 if(m==WM_DESTROY){KillTimer(w,1);PostQuitMessage(0);return 0;}
 return DefWindowProc(w,m,a,b);
}
int main(int argc,char **argv) {
 WNDCLASSA c;HWND w;MSG m;int draw_test=argc==2&&!strcmp(argv[1],"--draw");
 logfile=fopen("VX3DM1.TXT","w");if(!logfile)return 1;
 fprintf(logfile,"VX855 native 3D milestone 1; real GPU test only with --draw\n");
 device=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);
 if(device==INVALID_HANDLE_VALUE||!ioctl(VX_IOCTL_3DN_INFO,0,0,&info,sizeof(info))){fprintf(logfile,"Native 3D endpoint unavailable. Install the matching milestone VxD.\n");goto fail;}
 fprintf(logfile,"ABI %08lX ready %lu generation %lu %lux%lux%lu pitch %lu mapped %lu primary %lu fault %lu\n",
  (unsigned long)info.abi,(unsigned long)info.ready,(unsigned long)info.generation,(unsigned long)info.width,(unsigned long)info.height,
  (unsigned long)info.bpp,(unsigned long)info.pitch,(unsigned long)info.fb_bytes,(unsigned long)info.primary_bytes,(unsigned long)info.fault);
 if(!draw_test){printf("Query complete: VX3DM1.TXT. Use --draw for the fullscreen native probe.\n");goto done;}
 if(!info.ready||info.width>2048||info.height>2048||(info.pitch&31)){fprintf(logfile,"Mode not ready or not supported by milestone.\n");goto fail;}
 memset(&c,0,sizeof(c));c.lpfnWndProc=proc;c.hInstance=GetModuleHandleA(0);c.lpszClassName="VX3DM1";
 RegisterClassA(&c);w=CreateWindowA("VX3DM1","VX855 native 3D",WS_POPUP|WS_VISIBLE,0,0,info.width,info.height,0,0,c.hInstance,0);
 if(!w)goto fail;SetForegroundWindow(w);SetFocus(w);
 while(GetMessage(&m,0,0,0)>0){TranslateMessage(&m);DispatchMessage(&m);}
 if(failed)fprintf(logfile,"Engine stopped, result %lu. Reboot if display access was fenced.\n",(unsigned long)failed);
 else RedrawWindow(0,0,0,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
done:
 if(device!=INVALID_HANDLE_VALUE&&ioctl(VX_IOCTL_3DN_INFO,0,0,&info,sizeof(info)))
  fprintf(logfile,"Final submitted %lu triangles %lu CPU-clears %lu last-status %08lX last-result %lu fault %lu\n",(unsigned long)info.submitted,(unsigned long)info.triangles,(unsigned long)info.clears,(unsigned long)info.last_status,(unsigned long)info.last_result,(unsigned long)info.fault);
 if(device!=INVALID_HANDLE_VALUE)CloseHandle(device);fclose(logfile);return failed?1:0;
fail:
 if(device!=INVALID_HANDLE_VALUE)CloseHandle(device);fclose(logfile);return 1;
}
