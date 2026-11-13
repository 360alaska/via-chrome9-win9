import sys,struct
import os;sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from rng import load,words
def packets(w):
    i=0;out=[]
    while i<len(w):
        x=w[i]
        if x is None: i+=1; continue
        if (x>>24)==0xFE:
            kind=(x>>16)&0xFF
            if kind==3:
                n=w[i+1];hv=w[i+2];data=w[i+4:i+4+n];out.append(('blk',hv,list(data)));i+=4+n
                while i<len(w) and w[i]==0xCC000000: i+=1
                continue
            if kind==2:
                addr=w[i+1];hv=w[i+2];j=i+4;data=[]
                while j<len(w) and w[j] is not None and (w[j]>>24)!=0xFE: 
                    if w[j] not in (0xCC000000,0xCCC00008): data.append(w[j])
                    j+=1
                out.append(('reg' if addr==0x43C else 'w%03X'%addr,hv,data));i=j;continue
            if kind==0:
                n=w[i+1];data=w[i+4:i+4+2*n];out.append(('2d',n,list(data)));i+=4+2*n
                while i<len(w) and w[i] in (0xCC000000,): i+=1
                continue
            out.append(('fe?',x,[]));i+=1;continue
        out.append(('raw',x,[]));i+=1
    return out
def fmt(p):
    k,h,d=p
    if k=='reg':
        t=h>>16
        return 'T%02X/%04X: '%(t,h&0xFFFF)+' '.join('%02X=%06X'%(v>>24,v&0xFFFFFF) for v in d)
    if k=='blk': return 'BLK %08X[%d]: '%(h,len(d))+' '.join('%08X'%v for v in d[:24])+(' ...' if len(d)>24 else '')
    if k=='2d': return '2D: '+' '.join('%02X=%08X'%(d[j],d[j+1]) for j in range(0,len(d)-1,2))
    if k.startswith('w'): return '%s %08X'%(k,h)
    return '%s %08X'%(k,h)
def case_packets(path):
    h,sr,pre,end,pages=load(path)
    w=words(pages,pre[0xF],end[0xF])
    return packets(w)
if __name__=='__main__':
    for p in sys.argv[1:]:
        print('==',p)
        for q in case_packets(p):
            if q[0]=='w41C': continue
            print('  '+fmt(q))
