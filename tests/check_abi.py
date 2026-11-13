#!/usr/bin/env python3
"""Compare independent driver declarations with a supplied legacy DDK header.

The Microsoft DDK header is not redistributed here. Development used the
legacy section of the pinned d3dhal_ddk.h listed in docs/DDI_REFERENCES.md.
"""
import argparse,json,os
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--watcom',required=True);p.add_argument('--reference',type=Path,required=True);a=p.parse_args()
 wc=Path(a.watcom).resolve();env=os.environ.copy();env['WATCOM']=str(wc);env['PATH']=str(wc/'binl64')+os.pathsep+str(wc/'binl')+os.pathsep+env['PATH']
 build=ROOT/'build';build.mkdir(exist_ok=True)
 pairs=[('vx_ddi_desc','D3DDEVICEDESC_V1'),('vx_ddi_global','D3DHAL_GLOBALDRIVERDATA'),('vx_ddi_create','D3DHAL_CONTEXTCREATEDATA'),('vx_ddi_destroy','D3DHAL_CONTEXTDESTROYDATA'),('vx_ddi_dp2','D3DHAL_DRAWPRIMITIVES2DATA'),('vx_ddi_validate','D3DHAL_VALIDATETEXTURESTAGESTATEDATA'),('vx_ddi_callbacks','D3DHAL_CALLBACKS'),('vx_ddi_callbacks2','D3DHAL_CALLBACKS2'),('vx_ddi_callbacks3','D3DHAL_CALLBACKS3'),('vx_ddi_extended','D3DHAL_D3DEXTENDEDCAPS7')]
 pairs += [('vx_ddi_renderstate','D3DHAL_RENDERSTATEDATA'),('vx_ddi_renderprimitive','D3DHAL_RENDERPRIMITIVEDATA'),('vx_ddi_getstate','D3DHAL_GETSTATEDATA'),('vx_ddi_scene','D3DHAL_SCENECAPTUREDATA'),('vx_ddi_drawone','D3DHAL_DRAWONEPRIMITIVEDATA'),('vx_ddi_drawindexed','D3DHAL_DRAWONEINDEXEDPRIMITIVEDATA'),('vx_ddi_clear','D3DHAL_CLEARDATA')]
 code='#include "vx_d3d_hal.h"\n#include <d3d.h>\n#include '+json.dumps(str(a.reference.resolve()))+'\n#include <stddef.h>\n'
 for i,(ours,theirs) in enumerate(pairs):code+=f'typedef char size_{i}[sizeof({ours})==sizeof({theirs})?1:-1];\n'
 for field in ['dwhContext','dwFlags','dwVertexType','lpDDCommands','dwCommandOffset','dwCommandLength','dwVertexOffset','dwVertexLength','lpdwRStates','ddrval','dwErrorOffset']:
  code+=f'typedef char offset_{field}[offsetof(vx_ddi_dp2,{field})==offsetof(D3DHAL_DRAWPRIMITIVES2DATA,{field})?1:-1];\n'
 code+='typedef char error1[VX_D3D_INVALID==DDERR_INVALIDPARAMS?1:-1];\ntypedef char error2[VX_D3D_UNSUPPORTED==DDERR_UNSUPPORTED?1:-1];\ntypedef char error3[VX_D3D_UNPARSED==D3DERR_COMMAND_UNPARSED?1:-1];\n'
 values=[('VX_D3D_CONTEXT_BAD','D3DHAL_CONTEXT_BAD'),('VX_D3D_WRONGTEXTUREFORMAT','D3DERR_WRONGTEXTUREFORMAT'),('VX_D3D_COLOROP','D3DERR_UNSUPPORTEDCOLOROPERATION'),('VX_D3D_COLORARG','D3DERR_UNSUPPORTEDCOLORARG'),('VX_D3D_ALPHAOP','D3DERR_UNSUPPORTEDALPHAOPERATION'),('VX_D3D_ALPHAARG','D3DERR_UNSUPPORTEDALPHAARG'),('VX_D3D_TOOMANYOPS','D3DERR_TOOMANYOPERATIONS'),('VX_D3D_CONFLICTINGSTATE','D3DERR_CONFLICTINGRENDERSTATE')]
 for i,(ours,theirs) in enumerate(values):code+=f'typedef char status_{i}[{ours}=={theirs}?1:-1];\n'
 (build/'abi_compare.c').write_text(code)
 cmd=['wcc386','-q','-bt=nt','-dWIN95','-dDIRECTDRAW_VERSION=0x0700','-dDIRECT3D_VERSION=0x0700','-I'+str(ROOT/'src'),'-I'+str(ROOT/'src'/'ddhal32'),'-I'+str(wc/'h'/'nt'/'directx'),'-I'+str(wc/'h'/'nt'),'-I'+str(wc/'h'),'-fo=abi_compare.obj','abi_compare.c']
 r=subprocess.run(cmd,cwd=build,env=env,text=True,capture_output=True)
 result={'scope':'32-bit compile-time layout/value comparison, not runtime or Microsoft certification.','reference':str(a.reference),'types':pairs,'dp2_offsets_checked':11,'hresults_checked':3+len(values),'returncode':r.returncode,'output':r.stdout+r.stderr}
 (ROOT/'verification'/'D3D_ABI.json').write_text(json.dumps(result,indent=2)+'\n')
 print(result['output'] or f'{len(pairs)} structure sizes, 11 DP2 offsets and {3+len(values)} status values match the supplied legacy DDK/SDK declarations.')
 raise SystemExit(r.returncode)
if __name__=='__main__':main()
