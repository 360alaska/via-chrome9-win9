"""probe_subs.py (.658): every request number of the VxD's VX_probe (VXPROBE, VXREG CURSOR, VXTIME) answered by one branch only (.657 gave
the cursor report request 4, which the mode-set history already answered first: VXREG CURSOR read the history)."""
import re,sys
s=open(__file__.rsplit('/',2)[0]+'/src/vxd_vesa.c').read()
a=s.index('void VX_probe(');b=s.index('\n}\n',a)
subs=re.findall(r'sub == (\d+)',s[a:b])
d=sorted({x for x in subs if subs.count(x)>1})
print('VX_probe requests:',' '.join(subs),'; answered twice: '+(' '.join(d) if d else 'none'))
print('FAILED' if d else 'PASS');sys.exit(1 if d else 0)
