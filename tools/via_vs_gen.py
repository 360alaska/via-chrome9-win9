# Runs the vertex shader code generator of VIA's CE Direct3D Mobile driver (debug build) in Unicorn on small programs and prints the
# hardware instruction words: the source of the VX900 instruction layout (the supplement, 9.10). Usage: python3 tools/via_vs_gen.py d3dm_via.dll
import struct,sys
from unicorn import *
from unicorn.x86_const import *
D=open(sys.argv[1],'rb').read()  # d3dm_via.dll, the debug build (894,464 bytes) from VIA Graphics Driver Group2.msi; not part of this tree
BASE=0x10000000
def build():
    u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(BASE,0x200000)
    u.mem_write(BASE+0x1000,D[0x400:0x400+0xd0800]); u.mem_write(BASE+0xd2000,D[0xd0c00:0xd0c00+0x2a00])
    u.mem_map(0x20000000,0x100000)  # ctx, outdesc
    u.mem_map(0x30000000,0x100000)  # stack
    return u
STOP=0x30080000
def call(u,fn,ecx,args,trace=None):
    sp=0x30070000
    for a in reversed(args): sp-=4; u.mem_write(sp,struct.pack('<I',a))
    sp-=4; u.mem_write(sp,struct.pack('<I',STOP))
    u.reg_write(UC_X86_REG_ESP,sp); u.reg_write(UC_X86_REG_ECX,ecx)
    u.emu_start(fn,STOP,count=5000000)
    return u.reg_read(UC_X86_REG_EAX)
def hooks(u,log):
    def code(u,addr,size,ud):
        if addr==0x10019fa6:   # memset(dst,val,n), cdecl
            sp=u.reg_read(UC_X86_REG_ESP); ret,dst,val,n=struct.unpack('<4I',u.mem_read(sp,16))
            u.mem_write(dst,bytes([val&255])*n); u.reg_write(UC_X86_REG_EAX,dst); u.reg_write(UC_X86_REG_ESP,sp+4); u.reg_write(UC_X86_REG_EIP,ret)
        elif addr==0x10015790: # assert
            sp=u.reg_read(UC_X86_REG_ESP); ret=struct.unpack('<I',u.mem_read(sp,4))[0]; log.append('ASSERT from %08X'%ret)
            u.reg_write(UC_X86_REG_ESP,sp+4); u.reg_write(UC_X86_REG_EIP,ret)
        elif 0x10019f00<=addr<0x1001a000 and addr not in (0x10019fa6,):
            sp=u.reg_read(UC_X86_REG_ESP); ret=struct.unpack('<I',u.mem_read(sp,4))[0]; log.append('import thunk %08X from %08X'%(addr,ret))
            u.reg_write(UC_X86_REG_EAX,0); u.reg_write(UC_X86_REG_ESP,sp+4); u.reg_write(UC_X86_REG_EIP,ret)
    u.hook_add(UC_HOOK_CODE,code,begin=0x10015790,end=0x1001a000)
T={'r':0,'v':1,'c':2,'a':3,'oPos':(4,0),'oFog':(4,1),'oPts':(4,2),'oD':5,'oT':6}
def dst(name,mask=15):
    if name in ('oPos','oFog','oPts'): t,n=T[name]
    else:
        import re; m=re.match(r'([a-zA-Z]+)(\d+)',name); t=T[m.group(1)]; n=int(m.group(2))
    return n|(t<<11)|(mask<<16)
def src(name,swz='xyzw',neg=0):
    import re; m=re.match(r'([a-zA-Z]+)(\d+)',name); t=T[m.group(1)]; n=int(m.group(2))
    s=sum('xyzw'.index(c)<<(2*i) for i,c in enumerate((swz+swz[-1]*4)[:4]))
    return n|(t<<11)|(s<<16)|(neg<<24)
def compile_(prog):
    u=build(); log=[]; hooks(u,log)
    CTX=0x20000000; OUT=0x20010000; THIS=0x20040000
    for i,ins in enumerate(prog):
        w=list(ins)+[0]*(6-len(ins)); u.mem_write(CTX+0x16a4+0x18*i,struct.pack('<6I',*w))
    u.mem_write(CTX+0x2ea4,struct.pack('<I',len(prog))); u.mem_write(CTX+0x39fc,struct.pack('<I',OUT))
    try: call(u,0x10095220,THIS,[CTX,0])
    except UcError as e: log.append('STOPPED %s at %08X'%(e,u.reg_read(UC_X86_REG_EIP)))
    n=struct.unpack('<I',u.mem_read(OUT+0x424,4))[0]
    words=struct.unpack('<%dI'%(4*max(n,1)),u.mem_read(OUT+0x42c,16*max(n,1)))
    hdr=struct.unpack('<12I',u.mem_read(OUT,48))
    return n,words,hdr,log,u
if __name__=='__main__':
    progs={'mov oPos,v0; mov oD0,v1; mov oT0.xy,v2':[(1,dst('oPos'),src('v0')),(1,dst('oD0'),src('v1')),(1,dst('oT0',3),src('v2'))],
           'add oPos,v0,c0; mov oD0,v1; mov oD1,v2; mov oFog.x,v3.w; mov oT1.xy,v4':[(2,dst('oPos'),src('v0'),src('c0')),(1,dst('oD0'),src('v1')),(1,dst('oD1'),src('v2')),(1,dst('oFog',1),src('v3','wwww')),(1,dst('oT1',3),src('v4'))],
           'mov r0,v0; add r0.xy,r0,c33; mov oPos,r0':[(1,dst('r0'),src('v0')),(2,dst('r0',3),src('r0'),src('c33')),(1,dst('oPos'),src('r0'))]}
    for name,p in progs.items():
        n,w,hdr,log,u=compile_(p)
        print(name); print('  instructions',n,'log',log[:6])
        for k in range(n): print('   %08X %08X %08X %08X'%w[4*k:4*k+4])
        print('  desc head',' '.join('%08X'%x for x in hdr))
