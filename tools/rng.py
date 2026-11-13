import struct,sys
def load(p):
    d=open(p,'rb').read();o=0
    h=struct.unpack_from('<8I',d,0);o=32
    sr=struct.unpack_from('<8I',d,o);o+=32
    pre=struct.unpack_from('<128I',d,o);o+=512
    end=struct.unpack_from('<128I',d,o);o+=512
    pages={}
    while o<len(d):
        mg,a,x,n=struct.unpack_from('<4I',d,o);o+=16
        data=d[o:o+n];o+=n
        pages.setdefault(a,{})[mg]=(x,data)
    return h,sr,pre,end,pages
def words(pages,lo,hi,src=0x54524147):
    out=[]
    a=lo
    while a<hi:
        pg=a&~0xFFF;e=pages.get(pg,{}).get(src)
        if not e or not e[1]: out.append(None)
        else: out.append(struct.unpack_from('<I',e[1],a-pg)[0])
        a+=4
    return out
if __name__=='__main__':
    h,sr,pre,end,pages=load(sys.argv[1])
    lo=int(sys.argv[2],16) if len(sys.argv)>2 else pre[0xF]
    hi=int(sys.argv[3],16) if len(sys.argv)>3 else end[0xF]
    print('case',h[1],'range %08X..%08X'%(h[2],h[3]),'43C %08X->%08X'%(pre[0xF],end[0xF]))
    for k,a in enumerate(sorted(pages)[:3]): print(' page %08X'%a,{('%08X'%m):('%08X'%v[0],len(v[1])) for m,v in pages[a].items()})
    w=words(pages,lo,hi)
    for i in range(0,len(w),8):
        print('%08X: '%(lo+i*4)+' '.join('--------' if x is None else '%08X'%x for x in w[i:i+8]))
