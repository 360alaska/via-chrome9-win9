#!/usr/bin/env python3
"""Reproduce the v10.178 review checks after the Watcom build.

Writes command output, return codes and hashes. Seven open regression probes
intentionally fail in the reviewed candidate and are recorded as such.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output-dir',type=Path,required=True)
    p.add_argument('--xp',type=Path);a=p.parse_args();a.output_dir.mkdir(parents=True,exist_ok=True)
    rows=[]
    def run(name,cmd,expected=0,env=None):
        e=os.environ.copy();e.update(env or {})
        r=subprocess.run([str(c) for c in cmd],cwd=ROOT,env=e,text=True,
                         stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        rows.append(dict(name=name,command=[str(c) for c in cmd],returncode=r.returncode,
                         expected_returncode=expected,passed=r.returncode==expected,output=r.stdout))
        print(name+': '+('PASS' if r.returncode==expected else 'FAIL'),flush=True)
        if r.returncode!=expected:print(r.stdout[-2400:],flush=True)
    cc=['gcc','-std=c99']
    production=['src/vx_d3d_core.c','src/vx_native3d_hw.c']
    run('host production library',cc+['-shared','-fPIC','-O2',*production,'-o','tests/core.so'])
    run('portable D3D contracts',[sys.executable,'tests/test_core.py'])
    flags=['-O1','-g','-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-Isrc']
    for source,name in [('fuzz_dp2','100000 original sanitizer inputs'),
                        ('audit_stress_v178','200000 structured inputs and 2000 exact clears')]:
        run(source+' sanitizer build',cc+flags+['tests/'+source+'.c',*production,'-o','tests/'+source])
        run(name,[ROOT/'tests'/source],env={'ASAN_OPTIONS':'detect_leaks=0'})
    for source,name in [('verify','loader ABI and 2D regressions'),
                        ('d3d_callbacks','linked D3D callbacks'),
                        ('native3d','linked native VxD'),('profile','HAL advertisement')]:
        run(name,[sys.executable,'verification/'+source+'.py'])
    run('existing D3D follow-up probes',[sys.executable,'tests/check_followup.py'])
    run('fixed defect probes',[sys.executable,'tests/audit_v178.py','--fixes-only',
        '--output',a.output_dir/'FIXES_ONLY.json'])
    run('full audit INCLUDING seven known open probes',
        [sys.executable,'tests/audit_v178.py','--output',a.output_dir/'AUDIT_AFTER.json'],expected=1)
    if a.xp:
        run('bounded original XP MIN/MAG mapping',[sys.executable,'tests/audit_xp_filter_v178.py',
            '--xp',a.xp,'--output',a.output_dir/'XP_FILTER.json'])
    for source in production:
        run('GCC analyzer '+source,cc+['-O0','-fanalyzer','-Wno-analyzer-too-complex','-Isrc',
            '-c',source,'-o',a.output_dir/(Path(source).stem+'.o')])
    result=dict(scope=__doc__,records=rows,
        source_sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in sorted((ROOT/'src').rglob('*')) if p.is_file()},
        binaries_sha256={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest()
                        for n in ['driver/VX85598.DRV','driver/VX85598.VXD','driver/VXDDRAW.DLL','tests/core.so']})
    (a.output_dir/'VALIDATION.json').write_text(json.dumps(result,indent=2)+'\n')
    known=json.loads((a.output_dir/'AUDIT_AFTER.json').read_text())['checks']
    assert sum(r['group']=='unresolved' and not r['passed'] for r in known)==7
    assert all(r['passed'] for r in known if r['group']=='fix')
    return int(any(not r['passed'] for r in rows))

if __name__=='__main__':raise SystemExit(main())
