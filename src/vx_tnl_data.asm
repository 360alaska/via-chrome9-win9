; vx_tnl_data.asm (.751): the T&L working arrays, each on a 64-byte line (tools/tnl_asm.py)
.386
DGROUP group _BSS
_BSS segment page public use32 'BSS'
public tn_sx
tn_sx db 1056 dup(?)
 db 32 dup(?)
public tn_sy
tn_sy db 1056 dup(?)
 db 32 dup(?)
public tn_sz
tn_sz db 1056 dup(?)
 db 32 dup(?)
public tn_cx
tn_cx db 65568 dup(?)
 db 32 dup(?)
public tn_cy
tn_cy db 65568 dup(?)
 db 32 dup(?)
public tn_cz
tn_cz db 65568 dup(?)
 db 32 dup(?)
public tn_cw
tn_cw db 65568 dup(?)
 db 32 dup(?)
public tn_cc
tn_cc db 16392 dup(?)
 db 56 dup(?)
public tn_vc
tn_vc db 16392 dup(?)
 db 56 dup(?)
public tn_buf
tn_buf db 852384 dup(?)
_BSS ends
end
