#!/usr/bin/env python3
"""Compile/check portable and linked driver code. Never performs hardware IO."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--asan',action='store_true');p.add_argument('--ref',help='.552: the .551 source tree: its core is built as tests/ref_core.so and the new checks compare against it');a=p.parse_args()
 records=[]
 def run(name,cmd,extra=None):
  env=os.environ.copy();env.update(extra or {})
  r=subprocess.run(list(map(str,cmd)),cwd=ROOT,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  records.append({'name':name,'returncode':r.returncode,'output':r.stdout})
  print(name+': '+('PASS' if not r.returncode else 'FAIL'),flush=True)
  if r.stdout:print(r.stdout[-2200:],flush=True)
  if r.returncode:raise RuntimeError(name+' failed')
 try:
  run('host C library',['gcc','-std=c99','-shared','-fPIC','-O2','src/vx_d3d_core.c','src/vx_native3d_hw.c','-o','tests/core.so'])
  run('the DrawPrimitives2 replay tool (.517)',['gcc','-std=c99','-O2','-Isrc','tests/vx_replay.c','src/vx_d3d_core.c','src/vx_native3d_hw.c','-o','tests/vx_replay'])
  run('DP2/emitter/immediate contracts',[sys.executable,'tests/test_core.py'])
  run('indexed packets: the HAL against its lists, the emitter by index and expanded, the validation (v10.470)',[sys.executable,'tests/test_fetch.py'])
  ref={}
  if a.ref: # .552
   # .559: the packet grew (512 vertices by index, .529-.558: 384): the reference is built from a copy of its sources with that one constant
   # set to this tree's, so its packets have this tree's layout
   import shutil,re
   rs=ROOT/'tests/ref_src';shutil.rmtree(rs,ignore_errors=True);shutil.copytree(Path(a.ref)/'src',rs,ignore=shutil.ignore_patterns('*.obj','*.o','gl','ddk','res'))
   ixv=re.search(r'^#define VX3DN_IX_VERTICES (\d+)',(ROOT/'src/vx_native3d.h').read_text(),re.M).group(1)
   h=rs/'vx_native3d.h';t=h.read_text();t2=re.sub(r'^#define VX3DN_IX_VERTICES \d+','#define VX3DN_IX_VERTICES '+ixv,t,flags=re.M);assert t2!=t or ixv=='384';h.write_text(t2)
   run('the reference core (.551) for the comparisons',['gcc','-std=c99','-shared','-fPIC','-O2',rs/'vx_d3d_core.c',rs/'vx_native3d_hw.c','-o','tests/ref_core.so']);ref={'VX_REF_CORE':str(ROOT/'tests/ref_core.so')}
  run('the answers kept for a state follow the state: one call against a call a command, 3,000 random streams'+(', and against .551' if a.ref else '')+' (.552)',[sys.executable,'tests/test_ep.py','3000'],ref)
  # the captured recordings (g.rec, tests/tnl_544.bin) are not in the public repository: their checks run only when the files are there
  if a.ref and (ROOT/'g.rec').exists():
   run('the recording\'s validated packets for the index check',[ROOT/'tests/vx_replay','g.rec','-q'],{'VX_DUMP':str(ROOT/'tests/ixcheck_packets.bin')})
   run('the VxD\'s index check two a word: the arithmetic, and 20,000 changed packets against .551\'s loop (.552)',[sys.executable,'tests/test_ixcheck.py',ROOT/'tests/core.so',ROOT/'tests/ref_core.so',ROOT/'tests/ixcheck_packets.bin','20000'])
  else:run('the VxD\'s index check two a word: the arithmetic against the loop (.552; --ref adds the built function against .551)',[sys.executable,'tests/test_ixcheck.py'])
  # .553 (.552 changes): the scalar fog's bits, the span constants after a template reset and an FVF change; the packet copy by spans
  run('fog bits and span constants build (.553)',['gcc','-std=c99','-O2','-Isrc','tests/test_opt552.c','src/vx_native3d_hw.c','-o','tests/test_opt552','-lm'])
  run('scalar fog: 200,000 inputs bit for bit; span constants follow the template and the FVF (.553)',[ROOT/'tests/test_opt552'])
  if a.ref:run('packet copies by constant spans: 3,000 random packets, every byte as .551 copies them (.553)',[sys.executable,'tools/compare_cores.py',ROOT/'tests/ref_core.so',ROOT/'tests/core.so'])
  run('the T&L group kernels: the assembled SSE2 bytes against the C kernels, 800 random frames (.544)',['clang','-m32','--target=i686-linux-gnu','-O2','-msse2','-nostdlib','-static','-ffreestanding','-fno-stack-protector','-fno-pic','-no-pie','-Wno-gnu-folding-constant','-Wa,--noexecstack','tests/test_tnl_asm.c','tests/tnl_kernels.s','-o','tests/test_tnl_asm'])
  run('the T&L group kernels, bytes == C',[ROOT/'tests/test_tnl_asm'])
  if (ROOT/'tests/tnl_544.bin').exists():run('the T&L group path against .543, GRID-like scene (.544)',[sys.executable,'tests/bench_tnl.py','tests/core.so','3','tests/tnl_544.bin'])
  run('the T&L cases of .544\'s review (ZBIAS, base 65535, linear fog, the side-plane fallback) through the VxD\'s checks',[sys.executable,'tests/tnl_544_cases.py'])
  run('v4 review: clip overflow and rejected ZRANGE',[sys.executable,'tests/tnl_v4_review_cases.py'])
  run('the pass-through span kernels: the assembled SSE2 bytes against the C reference (.545)',['clang','-m32','--target=i686-linux-gnu','-O2','-msse2','-nostdlib','-static','-ffreestanding','-fno-stack-protector','-fno-pic','-Wa,--noexecstack','tests/test_hal_asm.c','tests/hal_kernels.s','-o','tests/test_hal_asm'])
  run('the pass-through span kernels and the index maximum, bytes == C, 60,000 random cases',[ROOT/'tests/test_hal_asm'])
  if a.asan:
   run('ASan/UBSan build',['gcc','-std=c99','-O1','-g','-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-Isrc','tests/fuzz_dp2.c','src/vx_d3d_core.c','src/vx_native3d_hw.c','-o','tests/fuzz_dp2'])
   run('100000 deterministic sanitizer inputs',[ROOT/'tests/fuzz_dp2'],{'ASAN_OPTIONS':'detect_leaks=0'})
   run('the driver\'s T&L, ASan/UBSan build (.543)',['gcc','-std=c99','-shared','-fPIC','-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=undefined','-fno-omit-frame-pointer','src/vx_d3d_core.c','src/vx_native3d_hw.c','-o','tests/core_asan.so'])
   asan=subprocess.run(['gcc','-print-file-name=libasan.so'],text=True,stdout=subprocess.PIPE).stdout.strip()
   run('the driver\'s T&L: 1000 fuzzed DrawPrimitives2 calls under the sanitizers, every packet through the VxD\'s checks (.543)',[sys.executable,'tests/fuzz_tnl.py',ROOT/'tests/core_asan.so','1000','1'],{'LD_PRELOAD':asan,'ASAN_OPTIONS':'detect_leaks=0'})
   run('cursor converters ASan/UBSan build',['gcc','-std=c99','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-unused-function','-Isrc','tests/test_cursor.c','-o','tests/test_cursor']) # v10.279
   run('cursor converters against exactly sized payloads',[ROOT/'tests/test_cursor'],{'ASAN_OPTIONS':'detect_leaks=0'})
   run('D24S8 blocks build',['gcc','-std=c99','-O1','-g','-fsanitize=address,undefined','-Isrc','tests/test_splane.c','-o','tests/test_splane']) # v10.288
   run('D24S8 blocks keep the next surface 256-aligned',[ROOT/'tests/test_splane'],{'ASAN_OPTIONS':'detect_leaks=0'})
   run('latency distributions build',['gcc','-std=c99','-O1','-g','-fsanitize=address,undefined','-Isrc','tests/test_lat.c','-o','tests/test_lat']) # v10.321
   run('latency buckets and percentiles against a sorted reading',[ROOT/'tests/test_lat'],{'ASAN_OPTIONS':'detect_leaks=0'})
   run('the VxD reads registry values by their type and length',[sys.executable,'tests/test_regread.py']) # v10.319 (audit V2)
   run('the DRV copies cursor rows with the VxD\'s rule',[sys.executable,'-c',"import sys;s=open('src/dibcall.c',encoding='latin-1').read();sys.exit(0 if '(bp == 15 ? cx * 2 : (bp * cx + 7) / 8) * cy' in s else 1)"])
  probe=subprocess.run([sys.executable,'-c','import unicorn'],capture_output=True)
  if not probe.returncode:
   for name,script in [('existing driver/2D and image checks','verify.py'),('linked D3D callbacks','d3d_callbacks.py'),('linked native VxD','native3d.py'),('HAL advertisement','profile.py')]:
    run(name,[sys.executable,'verification/'+script])
   run('D3D follow-up regression probes',[sys.executable,'tests/check_followup.py'])
   run('the display driver\'s own lines and the cursor handshake, in the built VX85598.DRV (v10.507)',[sys.executable,'tests/gdi_cpu_lines.py'])
   run('SSE transform and clip kernels of the built ICD against Mesa\'s order and its C code',[sys.executable,'tests/sse_emu.py']) # v10.359
  else:
   records.append({'name':'linked x86 checks','skipped':True,'reason':'Unicorn not installed.'})
   print('Linked x86 checks SKIPPED: install unicorn.',flush=True)
  run('stack frames under one page (HAL DLL built without probes)',[sys.executable,'tests/check_stack_frames.py']) # v10.237
  run('the built VXGL.DLL with a lost queue and a back buffer that cannot be filled or locked (.573, peer review), and unknown WGL attributes (.575)',[sys.executable,'tests/gl_icd_failures.py'])
  run('the device profile: the three Chrome9 devices, their BARs and what is refused (.579)',[sys.executable,'tests/chip_profile.py'])
  run('the built VXDDRAW.DLL with packets held and the callback lock not to be had: Lock, Blt and Flip answer still drawing (.575, peer review)',[sys.executable,'tests/hold_timeout.py'])
  if a.asan:run('OpenGL ICD on the host: Mesa, back end and core against swrast',[sys.executable,'tests/gl_host.py']) # v10.239
 finally:
  gen={'.o','.obj','.err','.map','.lib','.dll','.exe','.lst','.res','.pyc'} # v10.319 (audit T4): gl/ too, build products left out
  hashes={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(list((ROOT/'src').rglob('*'))+list((ROOT/'gl').rglob('*'))) if p.is_file() and p.suffix.lower() not in gen}
  result={'scope':'Host parser/emitter and synthetic-service compiled x86 checks; no real Win98/runtime/GPU test.','records':records,'source_hashes':hashes}
  (ROOT/'verification/M2_TEST_RUN.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
