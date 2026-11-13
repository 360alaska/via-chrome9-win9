# safe in-place editing: read, transform, write a temp file, then replace (a failed edit leaves the file as it was)
import os
def edit(p,pairs,count=1):
    s=open(p).read()
    for a,b in pairs:
        n=s.count(a)
        if n!=count: raise SystemExit(f"{p}: {n} matches for {a[:80]!r}")
        s=s.replace(a,b)
    t=p+'.tmp';open(t,'w').write(s);os.replace(t,p)
def transform(p,f):
    s=open(p).read();s2=f(s);t=p+'.tmp';open(t,'w').write(s2);os.replace(t,p)
