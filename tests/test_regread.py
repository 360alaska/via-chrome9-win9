#!/usr/bin/env python3
"""v10.319 (audit V2): the VxD's RegReadConf (src/vxd_lib.c, taken out as it is) against stub registry calls: REG_DWORD and 4-byte REG_BINARY,
REG_SZ digits within the returned length, no overflow, the caller's default kept on anything else. Built with ASan/UBSan."""
import subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PRE='#include <stdio.h>\n#include <string.h>\ntypedef unsigned int DWORD;typedef int BOOL;typedef unsigned int UINT;typedef unsigned char BYTE;\n#define TRUE 1\n#define FALSE 0\n#define ERROR_SUCCESS 0\n#define REG_SZ 1\n#define REG_BINARY 3\nstatic int qtype;static unsigned char qdata[200];static DWORD qlen;\nDWORD _RegOpenKey(DWORD r,char *p,DWORD *k){*k=1;return 0;}\nDWORD _RegCloseKey(DWORD k){return 0;}\nDWORD _RegQueryValueEx(DWORD k,char *n,DWORD *res,DWORD *t,BYTE *d,DWORD *cb){if(qlen>*cb){*cb=qlen;return 234;}memcpy(d,qdata,qlen);*cb=qlen;*t=qtype;return 0;}\n'
MAIN='\nstatic int t(int type,const void *d,DWORD n,DWORD want_ok,DWORD want){DWORD v=777;BOOL ok;qtype=type;memcpy(qdata,d,n);qlen=n;ok=RegReadConf(0,"p","n",&v);\n if(ok!=(BOOL)want_ok||v!=(want_ok?want:777)){printf("FAIL type %d len %u: ok %d v %u\\n",type,n,ok,v);return 1;}return 0;}\nint main(void){int f=0;DWORD one=1,big=0xFFFFFFFFUL;char sp[64];memset(sp,\' \',64);\n f+=t(4,&one,4,1,1);f+=t(4,&big,4,1,0xFFFFFFFFUL);f+=t(4,&one,2,0,0);f+=t(3,&one,4,1,1);f+=t(3,&one,3,0,0);\n f+=t(1,"1",2,1,1);f+=t(1," 42",4,1,42);f+=t(1,"4294967295",11,1,4294967295UL);f+=t(1,"4294967296",11,0,0);f+=t(1,"",1,0,0);f+=t(1,"abc",4,0,0);\n f+=t(1,sp,64,0,0);f+=t(1,"12",2,1,12);f+=t(7,"x",2,0,0);\n printf("%d failed\\n",f);return f;}\n'
def main():
 s=(ROOT/'src'/'vxd_lib.c').read_text(encoding='latin-1');a=s.index("static union\n{");b=s.index("/**\n * VMM calls wrapers")
 with tempfile.TemporaryDirectory() as d:
  c=Path(d)/'rr.c';c.write_text(PRE+s[a:b]+MAIN);exe=Path(d)/'rr'
  r=subprocess.run(['gcc','-w','-fsanitize=address,undefined',str(c),'-o',str(exe)],capture_output=True,text=True)
  if r.returncode:print(r.stderr);sys.exit(1)
  r=subprocess.run([str(exe)],capture_output=True,text=True);print(r.stdout.strip());sys.exit(r.returncode)
if __name__=='__main__':main()
