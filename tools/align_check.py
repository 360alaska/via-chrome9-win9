"""align_check.py (.752): the build check for the arrays the SSE kernels write 16 bytes at a time. Reads the link maps (option statics lists
static arrays) and fails when a listed array is off its 64-byte line. T749-T751: with them 8 bytes off, the VX900's position kernel
took 49% longer in the frame; the VX855 (C7) showed 1-3%. Also lists every other array of 4 KB or more with its alignment, for the audit."""
import re,sys
from pathlib import Path
B=Path(__file__).resolve().parent.parent/'build'
MUST=['tn_sx','tn_sy','tn_sz','tn_cx','tn_cy','tn_cz','tn_cw','tn_cc','tn_vc','tn_buf']
bad=0
for m in ('VXDDRAW.map','VXGL.map'):
 t=(B/m).read_text(errors='replace')
 sym=[(int(a,16),n) for a,n in re.findall(r'^([0-9a-f]{8})[ +*s]*\s+(\w+)\s*$',t,re.M)]
 d=dict((n.strip('_'),a) for a,n in sym)
 for n in MUST:
  if n not in d:print(m,n,'NOT IN THE MAP');bad+=1
  elif d[n]%64:print(m,n,'is %d bytes off a 64-byte line'%(d[n]%64));bad+=1
 if '-v' in sys.argv:
  s=sorted(sym)
  for (a,n),(a2,_) in zip(s,s[1:]):
   if a2-a>=4096 and n.strip('_') not in MUST and not n.startswith('_tnk'):print('  %s %-28s %7d bytes, %2d off a 16-byte line'%(m,n,a2-a,a%16))
print('align_check:', 'FAIL' if bad else 'PASS (%d arrays on their lines in both DLLs)'%len(MUST))
sys.exit(1 if bad else 0)
