/* Fixed-size, read-only diagnostics: no printf or trace-buffer writes in HAL. */
#ifndef VX_D3D_DIAG_H
#define VX_D3D_DIAG_H
#include <stdint.h>
typedef struct vx_d3d_diag {
 uint32_t cb,version,enabled,creates,dp2,legacy_draws,render_states;
 uint32_t last_callback,last_hr,last_opcode,last_offset,native_packets;
 uint32_t dropped,drop_why;
 uint32_t lost_why,lost_value,lost_count;
 uint32_t build,batched,recovered; /* v10.214: the DLL's build number, primitives appended to a batch, engine recoveries seen */
 uint32_t drop_state[8]; /* v10.184: vx_d3d_drop_state */ /* v10.182: why a context was last marked lost (vx_d3d_hal.c note_lost) */ /* v10.177: primitives left out (state the chip cannot draw), and why (vx_d3d_unsupported_why bits) */
} vx_d3d_diag;
#endif
