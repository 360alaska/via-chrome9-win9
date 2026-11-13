#!/usr/bin/env python3
"""Compare all point/linear MIN/MAG pairs against a bounded XP CPU fragment.

Pinned supplied DLL, synthetic RAM. No XP runtime or physical GPU execution.
Requires Unicorn. The proprietary DLL is an input and is not redistributed.
"""
import argparse
import hashlib
import json
from pathlib import Path
from check_xp_reference import PE, XP
from unicorn.x86_const import UC_X86_REG_EDX

def main():
    p=argparse.ArgumentParser();p.add_argument('--xp',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    image=PE(a.xp);digest=hashlib.sha256(image.b).hexdigest()
    assert digest=='fec5211548bc2e1100efa8de55b89407717c14997d78e0d3eed333d62a1ca532'
    ctx,shadow,obj,setup=0x02000000,0x02040000,0x02080000,0x02090000
    rows=[]
    for stage in (0,1,7):
        for mag,min_ in [(1,1),(2,1),(1,2),(2,2)]:
            xp=XP(image);xp.put(ctx+0x8c40,shadow);xp.put(ctx+0x8c74,obj)
            xp.put(ctx+0x8c6c,setup);xp.put(0x021f0004,ctx)
            state=ctx+stage*0xf8+0x22bc
            xp.put(state+0x48,mag);xp.put(state+0x4c,min_)
            xp.put(state+0x50,0);xp.put(state+0x60,1)
            reg31=shadow+stage*0xdc+0xed0+0xc4
            xp.put(reg31,0x31000000)
            xp.run(0xcc2d0,0xcc5d5,{UC_X86_REG_EDX:stage})
            actual=xp.get(reg31)
            expected=0x31010000|(0x2080 if mag==2 else 0)|(0x410 if min_==2 else 0)
            rows.append(dict(stage=stage,mag=mag,min=min_,actual=hex(actual),
                             expected=hex(expected),passed=actual==expected))
    a.output.write_text(json.dumps(dict(scope=__doc__,binary_sha256=digest,
        fragment=['0xcc2d0','0xcc5d5 (exclusive)'],checks=rows),indent=2)+'\n')
    assert all(r['passed'] for r in rows),rows
    print(f'{len(rows)} XP MIN/MAG mapping probes passed.')

if __name__=='__main__':main()
