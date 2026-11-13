#!/usr/bin/env python3
""".552: the answers kept for the template's epoch (vx_d3d_core.c: ep_supported, ep_compose).

A draw token's support check and the template's composed program are worked out once for each state and used again until the state
changes. These tests look for an answer used after the state it was made for:

 1. the same random command stream (render states, texture stage states, viewports, textures, draws; half of the states make the
    draw unsupported) sent as ONE DrawPrimitives2 call and as one call a command: the calls drop the same draws and send the same
    packets, byte for byte. In the one-a-command run every answer is worked out afresh (a call ends the epoch).
 2. in the one-a-command run, every draw vx_d3d_supported (the public function, which keeps nothing) refuses on the state at that
    draw is left out.
 3. with VX_REF_CORE=<the .551 tree's tests/core.so>: the same streams through both libraries, the same packets and drops.
 4. (.553) ValidateDevice after a context is initialized again at the same address: a fresh context's answer.

usage: test_ep.py [streams]"""
import ctypes as C,os,random,struct,sys
from pathlib import Path
sys.argv,argv=sys.argv[:1],sys.argv
import test_core as T
U=C.c_uint32
N=int(argv[1]) if len(argv)>1 else 3000
def load(path):
    lib=C.CDLL(str(path))
    lib.vx_d3d_init.argtypes=[C.POINTER(T.Context),C.POINTER(T.Surface),C.POINTER(T.Surface),U]
    lib.vx_d3d_dp2.argtypes=[C.POINTER(T.Context),C.POINTER(T.Input),C.POINTER(T.Services),C.POINTER(U)];lib.vx_d3d_dp2.restype=U
    lib.vx_d3d_supported.argtypes=[C.POINTER(T.Context)];lib.vx_d3d_supported.restype=C.c_int
    lib.vx_d3d_draw_status.argtypes=[C.POINTER(T.Context),C.POINTER(T.Services)];lib.vx_d3d_draw_status.restype=U
    lib.vx_d3d_packet_copy.argtypes=[C.POINTER(T.Packet),C.POINTER(T.Packet)];lib.vx_d3d_packet_copy.restype=None
    return lib
class Run:
    """one context on one library; packets normalized by the core's own copy (the unused tails of the static packet left out)"""
    def __init__(self,lib,depth):
        self.lib=lib;self.ctx=T.Context();self.target=T.Surface(0,256,64,64,1);self.depth=T.Surface(0x10000,256,64,64,depth) if depth else None
        lib.vx_d3d_init(C.byref(self.ctx),C.byref(self.target),C.byref(self.depth) if self.depth else None,7);self.ctx.state.rs[22]=1
        U.in_dll(lib,'vx_d3d_feat').value=T.ALLF&~128
        self.packets=[]
        def submit(_,p):
            q=T.Packet();lib.vx_d3d_packet_copy(C.byref(q),p);self.packets.append(bytes(q));return 0
        def resolve(_,h,p):
            if h==3:p[0]=self.target;return 1
            if h==5:p[0]=T.Surface(0x20000,32,8,8,4);return 1
            if h==6:p[0]=T.Surface(0x20000,16,8,8,6);return 1
            if h==7:p[0]=T.Surface(0x20000,32*4,32,32,4);return 1
            return 0
        def colorkey(_,h,k):return 0
        def unknown(_,p,n):return T.UNPARSED
        def mips(_,h,o,m):return 0
        def faces(_,h,o):return 0
        self.keep=(T.SUBMIT(submit),T.RESOLVE(resolve),T.UNKNOWN(unknown),T.CKEY(colorkey),T.MIPS(mips),T.FACES(faces))
        self.sv=T.Services(None,self.keep[0],self.keep[1],self.keep[2],T.BIND(0),self.keep[3],self.keep[4],self.keep[5])
        self.vb=C.create_string_buffer(b''.join(struct.pack('<4fI2f',x,y,.5,1.,0xff000000|(i+1)*0x112233,x/64,y/64) for i,(x,y) in enumerate([(8,8),(56,8),(8,56),(56,56),(32,16)])))
        self.rstates=(U*256)()
    def dropped(self):return U.in_dll(self.lib,'vx_d3d_dropped').value
    def dp2(self,commands):
        cb=C.create_string_buffer(commands,len(commands))
        inp=T.Input(C.addressof(cb),C.addressof(self.vb),len(commands),len(self.vb)-1,5,28,0x144,0,C.addressof(self.rstates),256)
        err=U(0);rc=self.lib.vx_d3d_dp2(C.byref(self.ctx),C.byref(inp),C.byref(self.sv),C.byref(err));return rc,err.value
# render states the support check and the template read, with values on both sides of each test
RS={7:(0,1,2,3),8:(0,1,2,3,4),9:(0,1,2,3,4),14:(0,1,2),15:(0,1,2),19:(1,2,5,6,0,99),20:(1,2,5,6,0,99),22:(0,1,2,3,4),23:(0,1,4,8,9),24:(0,128,255,256),
    25:(0,1,5,8,9),26:(0,1),27:(0,1,2),29:(0,1),41:(0,1),52:(0,1),168:(0,7,15,16),47:(0,1,16),34:(0,0xFF8040),28:(0,1),137:(0,1),136:(0,1)}
TSS={0:(0,5,6,7,99),1:(1,2,3,4,5,7,8,10,13,17,22,24,26,99),2:(0,1,2,3,4,0x12,7),3:(0,1,2,3,4),4:(1,2,3,4,5,7,13,99),5:(0,1,2,3,4),6:(0,1,2,3),11:(0,1,2),
     12:(1,2,3),16:(1,2),17:(1,2),24:(0,2,0x103)}
def stream(rnd):
    out=[]
    for _ in range(rnd.randrange(4,40)):
        k=rnd.randrange(100)
        if k<34:
            pairs=[]
            for _ in range(rnd.randrange(1,4)):i=rnd.choice(list(RS));pairs.append((i,rnd.choice(RS[i])))
            out.append(('s',T.rs(*pairs)))
        elif k<58:
            ent=[]
            for _ in range(rnd.randrange(1,4)):st=rnd.choice((0,0,0,1,1,2));i=rnd.choice(list(TSS));ent.append(struct.pack('<HHI',st,i,rnd.choice(TSS[i])))
            out.append(('s',T.cmd(25,len(ent),b''.join(ent))))
        elif k<62:
            x=rnd.choice((0,0,8));y=rnd.choice((0,0,8));out.append(('s',T.cmd(28,1,T.pack(x,y,rnd.choice((64-x,32,16)),rnd.choice((64-y,32,16))))))
        elif k<82:out.append(('d',T.cmd(18,1,struct.pack('<H',rnd.choice((0,1,2))))))
        else:out.append(('d',T.cmd(26,2,struct.pack('<H',0)+struct.pack('<6H',0,1,2,1,3,2))))
    return out
def one(lib,s,depth,split):
    r=Run(lib,depth);d0=r.dropped();res=[];sup=[]
    if split:
        for kind,c in s:
            ok=1 if (not r.ctx.lost and lib.vx_d3d_supported(C.byref(r.ctx))) else 0;d1=r.dropped()
            res.append(r.dp2(c))
            if kind=='d':sup.append((ok,r.dropped()-d1))
    else:res.append(r.dp2(b''.join(c for _,c in s)))
    return res,r.packets,r.dropped()-d0,sup,bytes(r.ctx.state)
def main():
    new=load(Path(__file__).resolve().parent/'core.so');ref=load(os.environ['VX_REF_CORE']) if os.environ.get('VX_REF_CORE') else None
    rnd=random.Random(552);draws=drops=packets=refcmp=0
    for n in range(N):
        s=stream(rnd);depth=rnd.choice((0,3,12,13))
        a_res,a_pk,a_drop,_,a_state=one(new,s,depth,0)
        b_res,b_pk,b_drop,b_sup,b_state=one(new,s,depth,1)
        nd=sum(1 for k,_ in s if k=='d')
        bad=[x for x in b_res if x[0]]
        if not bad:
            if a_res[0][0]:raise SystemExit('stream %d: the single call failed %08X, the split calls did not'%(n,a_res[0][0]))
            if a_state!=b_state:raise SystemExit('stream %d: the states differ after the two runs'%n)
            if a_pk!=b_pk:raise SystemExit('stream %d: packets differ between one call (%d) and a call a command (%d)'%(n,len(a_pk),len(b_pk)))
            if a_drop!=b_drop:raise SystemExit('stream %d: %d draws left out in one call, %d a call a command'%(n,a_drop,b_drop))
            # a draw vx_d3d_supported refuses is left out (a supported one may still be: a texture that does not resolve, a bad vertex)
            for k,(ok,dd) in enumerate(b_sup):
                if not ok and not dd:raise SystemExit('stream %d: draw %d is unsupported and was not left out'%(n,k))
            draws+=nd;drops+=sum(1 for ok,dd in b_sup if not ok);packets+=len(a_pk)
        if ref:
            r_res,r_pk,r_drop,_,r_state=one(ref,s,depth,0)
            if (r_res,r_pk,r_drop,r_state)!=(a_res,a_pk,a_drop,a_state):raise SystemExit('stream %d: the reference library differs (answers %s / %s, packets %d / %d, left out %d / %d)'%(n,r_res,a_res,len(r_pk),len(a_pk),r_drop,a_drop))
            refcmp+=1
    # .553: ValidateDevice (vx_d3d_draw_status with a resolver -> the template's validation build) comes outside a DrawPrimitives2
    # call. After a call has left its answers behind, the context is initialized again in place (a new context at a destroyed one's
    # address) and given other stages directly; its answer must be that of a context at another address with the same state.
    vals=0;rnd2=random.Random(1553)
    for n in range(600):
        r=Run(new,0);s=[x for x in stream(rnd2)]+[('d',T.cmd(18,1,struct.pack('<H',0)))];r.dp2(b''.join(c for _,c in s))
        new.vx_d3d_init(C.byref(r.ctx),C.byref(r.target),None,7);r.ctx.state.rs[22]=1
        q=Run(new,0)
        for ctx in (r.ctx,q.ctx):
            pass
        sets=[(rnd2.choice((0,0,1)),i,rnd2.choice(TSS[i])) for i in rnd2.sample(list(TSS),rnd2.randrange(0,5))]
        for st,i,v in sets:r.ctx.state.tss[st][i]=v;q.ctx.state.tss[st][i]=v
        if r.ctx.state.tss[0][0]:r.ctx.state.rs[1]=r.ctx.state.tss[0][0];q.ctx.state.rs[1]=q.ctx.state.tss[0][0]
        a=new.vx_d3d_draw_status(C.byref(r.ctx),C.byref(r.sv));b=new.vx_d3d_draw_status(C.byref(q.ctx),C.byref(q.sv))
        if a!=b:raise SystemExit('validation %d: a context initialized again in place answers %08X, a fresh one %08X'%(n,a,b))
        if ref:
            z=Run(ref,0)
            for st,i,v in sets:z.ctx.state.tss[st][i]=v
            if z.ctx.state.tss[0][0]:z.ctx.state.rs[1]=z.ctx.state.tss[0][0]
            c=ref.vx_d3d_draw_status(C.byref(z.ctx),C.byref(z.sv))
            if c!=a:raise SystemExit('validation %d: %08X, the reference library %08X'%(n,a,c))
        vals+=1
    print('%d validations after a context was initialized again in place: the answers of a fresh context'%vals+(' and of the reference library' if ref else ''))
    ep=(U*4).in_dll(new,'vx_d3d_ep')
    print('%d streams: %d draws, %d left out as unsupported, %d packets; one call and a call a command agree, and with vx_d3d_supported'%(N,draws,drops,packets))
    print('composes done %d, used again %d; support answers worked out %d, used again %d'%tuple(ep))
    if ref:print('%d streams the same through the reference library (answers, error offsets, packets, drops, final state)'%refcmp)
    if not drops or not packets or not ep[3]:raise SystemExit('the streams did not exercise both answers')
main()
