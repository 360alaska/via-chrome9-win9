#!/usr/bin/env python3
"""Build the bundled Mesa/OSMesa and run the same bounds pixels as VXFEAT.

This exercises CPU Mesa only. It does not execute the Win98 ICD or VX855 HAL.
--mesa-source ../VX855_v10_355 runs the new suite against the previous source.
"""
import argparse
import concurrent.futures
import hashlib
import json
import pathlib
import re
import subprocess

root = pathlib.Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mesa-source', type=pathlib.Path, default=root)
parser.add_argument('--build-dir', type=pathlib.Path, default=root/'build/host_bounds')
args = parser.parse_args()
source_root = args.mesa_source.resolve()
mesa = source_root/'gl/mesa/src/mesa'
build = args.build_dir.resolve()
build.mkdir(parents=True, exist_ok=True)
source = (mesa/'sources').read_text()
files = []
for var in ['MAIN_SOURCES','GLAPI_SOURCES','MATH_SOURCES','SWRAST_SOURCES',
            'SWRAST_SETUP_SOURCES','TNL_SOURCES','VBO_SOURCES','SHADER_SOURCES','SLANG_SOURCES']:
    match = re.search(var+r'\s*=\s*\\\n((?:.*\\\n)*.*)', source)
    files += re.findall(r'(\S+\.c)', match.group(1))
files += ['drivers/common/driverfuncs.c','drivers/osmesa/osmesa.c']
include = [source_root/'gl',source_root/'gl/mesa/include',mesa,mesa/'main',
           mesa/'glapi',mesa/'math',mesa/'tnl',mesa/'shader',
           mesa/'shader/grammar',mesa/'shader/slang']
flags = ['gcc','-std=gnu99','-O1','-g','-fPIC','-fno-strict-aliasing','-w',
         '-D_GNU_SOURCE'] + ['-I'+str(p) for p in include]
headers = hashlib.sha256()
for directory in [source_root/'gl/mesa/include',mesa,source_root/'gl']:
    paths = directory.glob('*.h') if directory == source_root/'gl' else directory.rglob('*.h')
    for path in sorted(paths):
        headers.update(str(path).encode())
        headers.update(path.read_bytes())
recipe = headers.digest() + json.dumps(flags).encode()

def compile_one(name):
    obj = build/(name.replace('/','_')[:-2]+'.o')
    stamp = obj.with_suffix('.sha256')
    key = hashlib.sha256(recipe+(mesa/name).read_bytes()).hexdigest()
    if obj.exists() and obj.stat().st_size and stamp.exists() and stamp.read_text() == key:
        return obj
    result = subprocess.run(flags+['-c',str(mesa/name),'-o',str(obj)],capture_output=True,text=True)
    if result.returncode:
        raise RuntimeError(name+'\n'+result.stdout+result.stderr)
    if not obj.exists() or not obj.stat().st_size:
        raise RuntimeError('Compiler produced no object: '+str(obj))
    stamp.write_text(key)
    return obj

with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
    objects = list(pool.map(compile_one,files))
library = build/'libOSMesaVX.so'
subprocess.run(['gcc','-shared','-o',str(library)]+[str(p) for p in objects]+['-lm'],check=True)
test = build/'depth_bounds_osmesa'
subprocess.run(['gcc','-O1','-g','-I'+str(source_root/'gl/mesa/include'),
                str(root/'tests/depth_bounds_osmesa.c'),'-L'+str(build),'-lOSMesaVX',
                '-Wl,-rpath,'+str(build),'-lm','-o',str(test)],check=True)
print('Host Mesa source:',source_root,flush=True)
print('OSMesa modules:',len(objects),'; no Win98/GPU execution',flush=True)
subprocess.run([str(test)],check=True)
