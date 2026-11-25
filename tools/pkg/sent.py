"""sent.py (.739): the builds already presented or sent to the owner (tools/pkg/SENT.TXT, one build a line: 738B, 739, ...).
A number is frozen once sent (the owner, 2026-11-21/22): a change after that takes a letter (737B), anything beyond a B a new number.
guard(build) stops a package script that would make a zip under a number already sent; mark(build) records one once it is sent."""
import pathlib,re,sys
F=pathlib.Path(__file__).with_name('SENT.TXT')
def sent():return set(F.read_text().split()) if F.exists() else set()
def guard(build):
 b=build.lstrip('.').upper()
 if b in sent():sys.exit('build .%s was already sent: a change takes %s'%(b,'a new number' if re.search(r'[A-Z]$',b) else 'a letter (.%sB)'%b))
 m=re.match(r'^(\d+)([A-Z])$',b)
 if m and m.group(2)>'B':sys.exit('build .%s: anything beyond a B is a new build number'%b)
def mark(build):
 b=build.lstrip('.').upper()
 if b not in sent():F.write_text(''.join(x+'\n' for x in sorted(sent()|{b})))
if __name__=='__main__':
 (mark if sys.argv[1]=='mark' else guard)(sys.argv[2])
