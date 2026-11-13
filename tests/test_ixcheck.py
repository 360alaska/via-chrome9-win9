#!/usr/bin/env python3
""".552: the VxD's index check, two indices a word (vx_native3d_hw.c, vx3dn_validate) against the loop it replaces.
Part 1: the arithmetic alone, every 16-bit index in each half of a word for every vertex count 1..0x8000 that matters
        (all of 1..400, the edges, and 2,000 random counts).
Part 2: the built function. tests/vx_replay with VX_DUMP set writes the validated packets of a recording; each is then changed (one
        index, several, the last, an odd count's tail, counts at the edges) and both libraries must give the same answer:
        usage: test_ixcheck.py <new core.so> <reference core.so> <packets.bin> [cases]"""
import ctypes as C,random,sys,struct
def part1():
    rnd=random.Random(552);counts=list(range(1,401))+[0x7FFE,0x7FFF,0x8000]+[rnd.randrange(401,0x8000) for _ in range(2000)]
    for vc in counts:
        K=((0x8000-vc)*0x00010001)&0xFFFFFFFF
        # each half alone decides its bit: test every value of one half against three values of the other
        for other in (0,vc-1,0x7FFF):
            for h in list(range(0,min(vc+3,0x10000)))+[0x7FFF,0x8000,0x8001,0xFFFE,0xFFFF,vc,vc+1,max(vc-1,0)]+[rnd.randrange(0x10000) for _ in range(64)]:
                for x in (h|(other<<16),(h<<16)|other):
                    bad=(x|(((x&0x7FFF7FFF)+K)&0xFFFFFFFF))&0x80008000
                    ref=(x&0xFFFF)>=vc or (x>>16)>=vc
                    if bool(bad)!=ref:raise SystemExit('arithmetic differs: count %d word %08X'%(vc,x))
    print('part 1: the word test agrees with the loop for %d vertex counts'%len(counts))
def part2(new,ref,dump,cases):
    a=C.CDLL(new);b=C.CDLL(ref)
    raw=open(dump,'rb').read();isz,psz=struct.unpack_from('<II',raw,0);info=C.create_string_buffer(raw[8:8+isz],isz)
    pk=[raw[8+isz+k*psz:8+isz+(k+1)*psz] for k in range((len(raw)-8-isz)//psz)]
    for f in (a,b):f.vx3dn_validate.restype=C.c_uint32;f.vx3dn_validate.argtypes=[C.c_char_p,C.c_char_p]
    off_vc,off_vert,off_pre,off_b0=[int(x) for x in open(dump+'.off').read().split()]
    ixp=[p for p in pk if struct.unpack_from('<I',p,off_pre)[0]==2]
    if not ixp:raise SystemExit('no indexed packet in the dump')
    rnd=random.Random(1552);n=same=refused=0
    for p in ixp: # untouched first
        if a.vx3dn_validate(p,info)!=b.vx3dn_validate(p,info):raise SystemExit('an untouched packet differs')
    while n<cases:
        p=bytearray(rnd.choice(ixp));vc=struct.unpack_from('<I',p,off_vc)[0];b0=struct.unpack_from('<I',p,off_b0)[0];ni=b0&0xFFFF
        kind=rnd.randrange(8)
        if kind==0:k=rnd.randrange(ni);struct.pack_into('<H',p,off_vert+2*k,rnd.choice([vc,vc+1,vc-1 if vc else 0,0x7FFF,0x8000,0xFFFF,rnd.randrange(0x10000)]))
        elif kind==1:
            for _ in range(rnd.randrange(1,6)):k=rnd.randrange(ni);struct.pack_into('<H',p,off_vert+2*k,rnd.randrange(0x10000))
        elif kind==2:struct.pack_into('<H',p,off_vert+2*(ni-1),rnd.choice([vc,vc-1 if vc else 0,0xFFFF,0x8000]))
        elif kind==3: # an odd index count (refused as not a multiple of three unless 3 mod 6): the tail alone
            m=rnd.choice([3,9,15,ni-3 if ni>3 else 3]);struct.pack_into('<I',p,off_b0,(b0&0xFFFF0000)|m);struct.pack_into('<H',p,off_vert+2*(m-1),rnd.choice([vc,vc-1 if vc else 0,0xFFFF]))
        elif kind==4:struct.pack_into('<I',p,off_vc,rnd.choice([0,1,2,vc-1 if vc else 0,vc+1,384,385,0x8000,0x8001,0xFFFF,0x10000]))
        elif kind==5:k=rnd.randrange(ni);struct.pack_into('<H',p,off_vert+2*k,vc) # exactly the count: the first index refused
        elif kind==6: # the word after the last index (the pad of an odd count, or the next pair): must not matter
            struct.pack_into('<H',p,off_vert+2*ni,0xFFFF)
        else:
            for k in range(ni):struct.pack_into('<H',p,off_vert+2*k,rnd.randrange(max(vc,1)))
        ra=a.vx3dn_validate(bytes(p),info);rb=b.vx3dn_validate(bytes(p),info);n+=1
        if ra!=rb:raise SystemExit('case %d kind %d: new %d, reference %d (count %d, indices %d)'%(n,kind,ra,rb,vc,ni))
        same+=1;refused+=1 if ra else 0
    print('part 2: %d indexed packets of the recording, %d changed cases, every answer the same (%d refused)'%(len(ixp),same,refused))
part1()
if len(sys.argv)>3:part2(sys.argv[1],sys.argv[2],sys.argv[3],int(sys.argv[4]) if len(sys.argv)>4 else 20000)
