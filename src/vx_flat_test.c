/* VXFLAT v3.5 (driver .510; v3.4 .509; v3.3 .506; v3.2 .505; v3.1 .503; v3.0 .502; v2.9 .501; v2.8 v10.500, v2.7 v10.499; v2.6 v10.498, v2.5 v10.497, v2.4 v10.496, v2.3 v10.495, v2.2 v10.494, v2.1 v10.492, v2.0.1 v10.491, v2.0 v10.490, v1.9 v10.489, v1.8.1 v10.488, v1.7 v10.487, v1.6 v10.486, v1.5 v10.485, v1.4 v10.484, v1.3 v10.483, v1.2 v10.482, v1.1 v10.481, v1.0 v10.480): flat shading on the VX855 OpenGL ICD.
 *
 * v10.480: ACCEL_OPENGL_FLATSTRIP, a flat strip whose vertices share one colour sent to the engine as one strip. v10.481 (board, Lattice's
 * profile): ACCEL_OPENGL_FLATBATCH, a flat strip whose colours differ sent as independent triangles in one call (no Mesa template);
 * ACCEL_OPENGL_EMIT, whole-vertex loops for the ICD's fog and secondary-colour layouts; ACCEL_OPENGL_HWFLAT, the engine's own flat shading
 * (XP's mode word 9E02, each triangle the colours of its first vertex). v10.482: ACCEL_OPENGL_HWFLATSTRIP (v1.2 on the board: rule 1, each
 * strip triangle its first strip vertex's colours). v1.3 (v10.483): HWFLATSTRIP 1 by default, quad strips too; flat triangles, quads, fans
 * and polygons whose colours differ on the engine's flat shading (each triangle turned to start at its provoking vertex); ACCEL_OPENGL_FASTINT,
 * lighting's and fog's float-to-integer steps without the compiler's call. v1.4 (v10.484): ACCEL_OPENGL_RSQRT, the lighting's square roots and
 * divisions by SSE (norsqrt: within one step of each channel of the template); a local viewer in the positional scene (Lattice's); the
 * default configuration run again last, to tell a dip in one scene from a real change. v1.5 (v10.485): ACCEL_OPENGL_LIGHTFAST (one
 * positional light in a straight loop: nolightfast within one step of each channel of the template), ACCEL_OPENGL_CLIPINT (the clip test's
 * compares in integer registers), ACCEL_OPENGL_CLIPSTRIP (a strip with a vertex outside split: its runs inside as strips); a scene of lit
 * strips crossing the window's sides (Lattice's); the template with CLIPSTRIP 0; nohwflat dropped (v1.3, v1.4: as expected). v1.6
 * (v10.486): CLIPINT off by default (clipint tries it on); LIGHTFAST for every light of Mesa's general case: a scene of a positional light,
 * a directional light and a spotlight. v1.7 (v10.487): ACCEL_OPENGL_SSEPASS (a display list's draw in one SSE pass: nossepass, Mesa's
 * stages, within one step of each channel of the template); four scenes drawn from display lists (positional, clipped, three lights (a
 * spotlight: the pass declines), quads with a colour a vertex) and the pass's counts. v1.8 (v10.488): the pass for draws not from a list
 * (ACCEL_OPENGL_SSEIMM: nosseimm) and for spotlights; unlit draws left to Mesa's stages by default (ACCEL_OPENGL_SSEUNLIT: sseunlit tries
 * them); the pass's counts for every scene. v1.8.1: the expected counts: the pass takes Mesa's general lighting's case only (not the
 * directional-light scenes, Mesa's fast lighting, nor a separate specular colour). v1.9 (v10.489): a draw whole outside is not lit by the
 * pass (ACCEL_OPENGL_SSEOUTSIDE: nosseoutside): the scene list, half out (the positional list drawn twice, once beside the window). v2.0
 * (v10.490): the pass's new parts, each with a configuration that turns it off: nossebox (a list's draw whole outside from its bounding
 * box), nosseshine (the specular lookup four lanes at a time: within one step of the template), nossepack (the colour and fog bytes four
 * lanes at a time), nossesnap (the strip path's 1/16-grid words from the pass), nocullrun (with culling on, a strip's triangles one
 * vertex each without strip_fast); a scene list, culled (lit strips folded every six columns, GL_CULL_FACE front: runs ending at the folds
 * and starting again) and the parts' counts. v2.1 (v10.492): the screen saver and the monitor's power saving off while it runs (Lattice, the
 * screen saver, started during v2.0's run), restored after; ssesnap (SSESNAP 0 by default now), nossecache (the per-draw set-up made every
 * draw), nossemask (the lighting's lane compares a lane at a time), flatcopy (a flat strip's colours moved in a copy, v10.491); the kept
 * set-ups counted. v2.2 (v10.494): quick by default (the default, the template, software, the default again and the current driver's new
 * switches off: nossetight, nocullrun2, nofastbind); VXFLAT all runs every configuration; each configuration's switch bits checked. v2.3
 * (v10.495): nossefuse, nosseaccept, nossebox2, nossepartial, nolistbind (the quick run's off configurations now); VXGL_SsePassStats20's
 * counts (draws inside by the box, fused groups, partial set-ups, lists bound by the short path) shown and checked. v2.4 (v10.496): each
 * scene's frames in three runs, the rate the median run's; every frame timed, the slow ones logged with when they came (the gaps
 * between them: a period or a stall). v2.5 (v10.497): nossesnap2, nosseoutfast, nosseearly and nosseimmbox (the quick run); each frame's
 * longest VxD call (VXGL_StallStats) with the slow frames, and each scene's; the pass's grid words checked. v2.6 (v10.498): the children
 * without the ICD's log, sampler and frame log (VXFLAT log keeps them); noalignheap and the layout runs (pad1-7, nal pad1-7: the lit scenes).
 * v2.7 (v10.499): the v2.1 check that the pass's grid words are off by default skipped with SSESNAP2 (on by default); the quick run the
 * default, alignheap (ALIGNHEAP 1, now off by default), template, software, default2. v2.8 (v10.500): two scenes drawn as Quake 2 (textured
 * polygons glBegin/glVertex3fv by texture, the lightmap pass, a lightmap row block replaced each frame, a model of strips and fans) and Quake 3
 * (locked vertex arrays, colours as bytes, each batch drawn twice by glDrawElements(GL_TRIANGLES)) (src/vx_qscenes.h, the host bench's too);
 * notrilist, nocvacache, nofastvtx, nostoresnap, noglapione (the quick run's off configurations now, alignheap only with VXFLAT all); a
 * second thread makes a context of its own current and lets it go before the scenes (Mesa's per-thread dispatch until GLAPIONE takes it
 * back: noglapione draws every scene through it); VXGL_Stats500's counts each scene, checked. v2.9 (v10.501): noubcolor, noprepack,
 * nofastindex and indexed (INDEXED is off by default: the host's count of the driver's work 8% higher with it in the Quake 3 scene, the
 * packets three times as many); nostoresnap again (the flat triangles, quads and fans now take the store's grid words too);
 * VXGL_Stats501's counts (byte colours, prepacked packets, indexed triangles) each scene, checked. v3.0 (.502): prepacked packets expected
 * in the textured scenes only (the plain colour strips and the flat triangles have no shader program, which the 10-word form needs).
 * v3.1 (.503): nogllist, noglwords, nosse2 and nossedefer (the quick run's off configurations now; .501's with VXFLAT all); a scene smooth,
 * culled (lit strips, smooth shading, back faces culled, a quarter of each strip turned away: the lighting left for groups no drawn triangle
 * uses); VXGL_Stats503's counts each scene, checked; the frames read back kept in memory and written after the child's last scene (board,
 * v3.0: one 260-290 ms frame early in the first child, the screen files' writes). v3.2 (.505): glwords1, noglfan, nosserange (the quick
 * run's off configurations now; .503's with VXFLAT all); VXGL_Stats505's counts (vertices by the fixed layouts, polygons in one call), checked.
 * v3.3 (.506): noplan, nominmax2 and indexed (the quick run); the experiments the board turned down removed (clipint, sseunlit, ssesnap,
 * alignheap and the heap layout runs); the ICD's cycles a frame by part in the default (ACCEL_OPENGL_TSCPROF on in every child);
 * VXGL_Stats506's counts, checked.
 * v3.5 (.510): noimmsoa and its .509 table removed with ACCEL_OPENGL_IMMSOA (the board: within the default's own spread); the quick run
 * is the default, the template, software and the default again.
 * v3.4 (.509): noimmsoa (glVertex into the SSE pass's arrays off) in the quick run, with the default, the template, software and the default
 * again (.506's noplan and nominmax2 measured: with VXFLAT all now); indexed gone with ACCEL_OPENGL_INDEXED; VXGL_Stats509's counts (buffers
 * drawn from glVertex's arrays, their vertices, buffers made into words after all, wraps refilled) each scene, checked.
 *
 * "VXFLAT" runs itself once per configuration as a child process with the settings in its environment (VXGL.DLL reads every
 * ACCEL_OPENGL_* key from an environment variable of the same name first): default, nohwstrip, noemit, nofastint, norsqrt, nolightfast, noclipint, noclipstrip,
 * template
 * (v10.479: Mesa's template for every flat primitive) and software (VXGL_HW = 0, the reference). Each child draws eleven scenes, a fixed
 * frame of each read back for the comparison, then the scene moved for N frames (default 100, "VXFLAT n") and timed; and reads the ICD's
 * counts (VXGL_FlatStats8, else VXGL_FlatStats6).
 *
 * Scenes (320 x 240, 48 rows of 126 triangles, 6,048 triangles a frame, flat shading, nothing clipped):
 *  one colour        triangle strips, glColor once a strip: every strip sent whole
 *  quad strips       the same as quad strips
 *  arrays            glDrawElements(GL_TRIANGLE_STRIP) from a vertex array, glColor once a strip: sent whole
 *  lit, one normal   perspective, lighting on (a directional light, colour material), one normal a strip: the lit colours the same
 *  lit, normals      perspective, lighting on, a normal per vertex: the lit colours differ along the strip: the engine's strips
 *  lit, fog          as lit, normals, with linear fog and a separate specular colour: the engine's strips
 *  lit quad strips   lit, fog as quad strips (v1.3)
 *  triangles         GL_TRIANGLES, a colour per vertex (v1.3)
 *  quads             GL_QUADS, a colour per vertex (v1.3)
 *  fans, polygons    GL_TRIANGLE_FAN and GL_POLYGON, a colour per vertex (v1.3)
 *  positional, EXP   lit strips, a positional light with specular and exponential fog (Lattice's kind: Mesa's general lighting) (v1.3)
 *
 * Results: VXFLAT.TXT (frames a second, pixels off the software frame, each configuration against the template pixel for pixel, the
 * primitives a frame by path). MIT, 2026. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#ifndef GL_UNSIGNED_INT
#define GL_UNSIGNED_INT 0x1405
#endif
#define QS_API WINAPI
#include "vx_qscenes.h" /* v2.8 */

#define W 320
#define H 240
#define NSCENE 22
#define NCFG 50
#define CRSQ 4
#define CLF 5
#define CSP 7
#define CSI 8
#define CSO 9
#define CBOX 10
#define CSH 11
#define CPACK 12
#define CCULL 13
#define CCACHE 14
#define CMASK 15
#define CFCOPY 16
#define CTIGHT 17
#define CCR2 18
#define CFB 19
#define CFUSE 20
#define CACC 21
#define CBOX2 22
#define CPART 23
#define CLB 24
#define CIMB 25
#define CSN2 26
#define COUTF 27
#define CEARLY 28
#define CTRI 29
#define CCVA 30
#define CFVX 31
#define CSSN 32
#define CGLA 33
#define CUB 34
#define CPRE 35
#define CFIX 36
#define CIMM 37
#define CGLL 38 /* v3.1 */
#define CGLW 39
#define CSSE2 40
#define CDEFR 41
#define CGW1 42 /* v3.2 */
#define CFAN 43
#define CRNG 44
#define CPLN 45 /* v3.3 */
#define CMM2 46
#define CTPL 47
#define CSOFT 48
#define CDEF2 49
#define STRIPS 48
#define SEG 64 /* 2 x SEG vertices a strip: 2 x SEG - 2 triangles */
static const char *scene_name[NSCENE]={"one colour","quad strips","arrays","lit, one normal","lit, normals","lit, fog","lit quad strips","triangles","quads","fans, polygons","positional, EXP","clipped, lit","three lights","list, positional","list, clipped","list, 3 lights","list, quads","list, half out","list, culled","q2 world","q3 arrays","smooth, culled"};
static const struct {const char *name,*env;} cfg[NCFG]={
 {"default","ACCEL_OPENGL_SWAPINTERVAL=0"}, /* v1.3: HWFLATSTRIP 1 by default since v10.483 */
 {"nohwstrip","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_HWFLATSTRIP=0"}, /* v10.482's default: flat strips whose colours differ as triangles */
 {"noemit","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_EMIT=0"},
 {"nofastint","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_FASTINT=0"}, /* v1.3: the compiler's float-to-integer call in lighting and fog */
 {"norsqrt","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_RSQRT=0"}, /* v1.4: the x87's square roots and divisions in lighting */
 {"nolightfast","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_LIGHTFAST=0"}, /* v1.5: Mesa's general lighting loop */
 {"noclipstrip","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_CLIPSTRIP=0"}, /* v1.5: a clipped strip a triangle at a time (Mesa's clipping table) */
 {"nossepass","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSEPASS=0"}, /* v1.7: display lists through Mesa's stages (the x87's arithmetic) */
 {"nosseimm","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSEIMM=0"}, /* v1.8: draws not from a list through Mesa's stages */
 {"nosseoutside","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSEOUTSIDE=0"}, /* v1.9: a draw whole outside lit by the pass all the same (v10.488) */
 {"nossebox","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSEBOX=0"}, /* v2.0: a draw whole outside found from its vertices only (v10.489) */
 {"nosseshine","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSESHINE=0"}, /* v2.0: the specular lookup a lane at a time on the x87 (v10.489) */
 {"nossepack","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSEPACK=0"}, /* v2.0: the colour and fog bytes a lane at a time (v10.489) */
 {"nocullrun","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_CULLRUN=0"}, /* v2.0: with culling on, each strip triangle through strip_fast (v10.489) */
 {"nossecache","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSECACHE=0"}, /* v2.1: the pass's set-up made every draw (v10.491) */
 {"nossemask","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSEMASK=0"}, /* v2.1: the lighting's lane compares a lane at a time (v10.491) */
 {"flatcopy","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_FLATNOCOPY=0"}, /* v2.1: a flat strip's colours moved in a copy (v10.491) */
 {"nossetight","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSETIGHT=0"}, /* v2.2: the clip masks a lane at a time (v10.493) */
 {"nocullrun2","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_CULLRUN2=0"}, /* v2.2: a culling run's start through strip_culled (v10.493) */
 {"nofastbind","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_FASTBIND=0"}, /* v2.2: Mesa's import of the float arrays (v10.493) */
 {"nossefuse","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSEFUSE=0"}, /* v2.3: the pass's kernels one at a time (v10.495) */
 {"nosseaccept","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSEACCEPT=0"}, /* v2.3: the clip compares in a draw the box has inside (v10.495) */
 {"nossebox2","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSEBOX2=0"}, /* v2.3: the box's eight corners (v10.490's test; no inside, so no SSEACCEPT) */
 {"nossepartial","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSEPARTIAL=0"}, /* v2.3: a lit set-up made whole when only the tracked material changed (v10.495) */
 {"nolistbind","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_LISTBIND=0"}, /* v2.3: a list's 32 arrays copied every draw (v10.495) */
 {"nosseimmbox","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSEIMMBOX=0"}, /* v2.4: a draw not from a list without a box (v10.496; v2.5: by SSE since v10.497) */
 {"nossesnap2","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSESNAP2=0"}, /* v2.5: the strip path's snapw a vertex (v10.497) */
 {"nosseoutfast","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSEOUTFAST=0"}, /* v2.5: the whole-outside test through the store (v10.497) */
 {"nosseearly","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSEEARLY=0"}, /* v2.5: a draw the pass leaves copied first (v10.497) */
 {"notrilist","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_TRILIST=0"}, /* v2.8: GL_TRIANGLES a triangle at a time through Mesa's template (v10.500) */
 {"nocvacache","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_CVACACHE=0"}, /* v2.8: a locked range transformed again for each draw (v10.500) */
 {"nofastvtx","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_FASTVTX=0"}, /* v2.8: glVertex's copy as Mesa's loop (v10.500) */
 {"nostoresnap","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_STORESNAP=0"}, /* v2.8: the grid words by snapw a vertex a triangle (v10.500) */
 {"noglapione","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_GLAPIONE=0"}, /* v2.8: per-thread dispatch kept after the second thread's context (v10.500) */
 {"noubcolor","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_UBCOLOR=0"}, /* v2.9: byte colour arrays made floats and back (v10.501) */
 {"noprepack","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_PREPACK=0"}, /* v2.9: the packets' vertices as arrays (v10.501) */
 {"nofastindex","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_FASTINDEX=0"}, /* v2.9: Mesa's index scan and a block a rebase (v10.501) */
 {"(unused)",0}, /* v3.5: slot 37 free (v2.9-v3.3 indexed, v3.4 noimmsoa: both removed from the driver) */
 {"nogllist","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_GLLIST=0"}, /* v3.1: a triangle-list run's triangles each through primitive() (.503) */
 {"noglwords","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_GLWORDS=0"}, /* v3.1: the prepacked words by fast_vertex_pre (.503) */
 {"nosse2","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSE2=0"}, /* v3.1: the pass's colour words a lane at a time (.503) */
 {"nossedefer","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSEDEFER=0"}, /* v3.1: every group lit (.503) */
 {"glwords1","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_GLWORDS=1"}, /* v3.2: the recipe only, no fixed layouts (.505) */
 {"noglfan","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_GLFAN=0"}, /* v3.2: a polygon's strips each through vx_d3d_gl_strip (.505) */
 {"nosserange","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSERANGE=0"}, /* v3.2: the pass's range checks a lane at a time (.505) */
 {"noplan","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_PLAN=0"}, /* v3.3: each draw's triangle checks made again (.506) */
 {"nominmax2","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_MINMAX2=0"}, /* v3.3: the index scan in C (.506) */
 {"template","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_FLATSTRIP=0;ACCEL_OPENGL_FLATBATCH=0;ACCEL_OPENGL_HWFLAT=0;ACCEL_OPENGL_CLIPSTRIP=0"},
 {"software","ACCEL_OPENGL_SWAPINTERVAL=0;VXGL_HW=0"},
 {"default2","ACCEL_OPENGL_SWAPINTERVAL=0"}}; /* v1.4: the default again, last */
static const char *env_keys[]={"ACCEL_OPENGL_SWAPINTERVAL","ACCEL_OPENGL_FLATSTRIP","ACCEL_OPENGL_FLATBATCH","ACCEL_OPENGL_EMIT","ACCEL_OPENGL_HWFLAT","ACCEL_OPENGL_HWFLATSTRIP","ACCEL_OPENGL_FASTINT","ACCEL_OPENGL_RSQRT","ACCEL_OPENGL_LIGHTFAST","ACCEL_OPENGL_CLIPINT","ACCEL_OPENGL_CLIPSTRIP","ACCEL_OPENGL_SSEPASS","ACCEL_OPENGL_SSEIMM","ACCEL_OPENGL_SSEUNLIT","ACCEL_OPENGL_SSEOUTSIDE","ACCEL_OPENGL_SSEBOX","ACCEL_OPENGL_SSESHINE","ACCEL_OPENGL_SSEPACK","ACCEL_OPENGL_SSESNAP","ACCEL_OPENGL_CULLRUN","ACCEL_OPENGL_SSECACHE","ACCEL_OPENGL_SSEMASK","ACCEL_OPENGL_FLATNOCOPY","ACCEL_OPENGL_SSETIGHT","ACCEL_OPENGL_CULLRUN2","ACCEL_OPENGL_FASTBIND","ACCEL_OPENGL_SSEFUSE","ACCEL_OPENGL_SSEACCEPT","ACCEL_OPENGL_SSEBOX2","ACCEL_OPENGL_SSEPARTIAL","ACCEL_OPENGL_LISTBIND","ACCEL_OPENGL_SSEIMMBOX","ACCEL_OPENGL_SSESNAP2","ACCEL_OPENGL_SSEOUTFAST","ACCEL_OPENGL_SSEEARLY","ACCEL_OPENGL_ALIGNHEAP","ACCEL_OPENGL_TRILIST","ACCEL_OPENGL_CVACACHE","ACCEL_OPENGL_FASTVTX","ACCEL_OPENGL_STORESNAP","ACCEL_OPENGL_GLAPIONE","ACCEL_OPENGL_UBCOLOR","ACCEL_OPENGL_PREPACK","ACCEL_OPENGL_FASTINDEX","ACCEL_OPENGL_INDEXED","ACCEL_OPENGL_GLLIST","ACCEL_OPENGL_GLWORDS","ACCEL_OPENGL_SSE2","ACCEL_OPENGL_SSEDEFER","ACCEL_OPENGL_GLFAN","ACCEL_OPENGL_SSERANGE","VXFLAT_PAD","VXGL_LOG","VXGL_SAMPLE","VXGL_FRAMELOG","VXGL_HW",0};
/* v2.2: the quick run (the default): the default, the template, software and the default again, and the off configurations of the switches
 * the current driver added (v2.4, v10.496: SSEIMMBOX; v2.3, v10.495: SSEFUSE, SSEACCEPT, SSEBOX2, SSEPARTIAL, LISTBIND); "VXFLAT all" runs every configuration */
static int quick_cfg(int ci) {return ci==0||ci==CTPL||ci==CSOFT||ci==CDEF2;} /* v3.5 (.510): no OpenGL switch new; v3.4: .509's IMMSOA */
static int use_cfg(int all,int ci) {return cfg[ci].env&&(all||quick_cfg(ci));} /* v3.5: a free slot never runs */ /* v3.3: .506's switches and indexed (clipped polygons now inside its packets); v3.2: .505's (with VXFLAT all now) */ /* v2.9: v10.501's switches and nostoresnap; v10.500's others with VXFLAT all */ /* v2.8: v10.500's switches off; alignheap only with VXFLAT all */ /* v2.7: the layout runs only with VXFLAT all */ /* v2.6: v10.498's ALIGNHEAP and the layout runs */
static unsigned long sw_bits[NCFG];static int have_sw[NCFG];
static double runs[NCFG][NSCENE][3],fmed[NCFG][NSCENE],fmaxt[NCFG][NSCENE];static int nslow[NCFG][NSCENE],have_u[NCFG][NSCENE];static double slow_at[NCFG][400];static int nslow_at[NCFG]; /* v2.4 */
static unsigned long ts5[NCFG][NSCENE][7],g5[NCFG][4],vs5[NCFG][NSCENE][4];static int have_v[NCFG][NSCENE]; /* v2.9 */
static unsigned long xs5[NCFG][NSCENE][5];static int have_x[NCFG][NSCENE]; /* v3.1 */
static unsigned long ys5[NCFG][NSCENE][2];static int have_y[NCFG][NSCENE]; /* v3.2 */
static unsigned long zs5[NCFG][NSCENE][10];static int have_z[NCFG][NSCENE]; /* v3.3 */
static unsigned long ts5_unused;static int have_t[NCFG][NSCENE],have_g[NCFG],have_q[NCFG]; /* v2.8: VXGL_Stats500 a scene; the side thread's (made current, entered, left, switch bits); the lock entry points */
static unsigned long vxlong[NCFG][NSCENE];static int nslow_list,sl_cfg[64],sl_sc[64],sl_fi[64];static double sl_ms[64],sl_at[64];static unsigned long sl_vx[64],sl_code[64],sl_busy[64]; /* v2.5 */ /* v2.2: VXGL_SsePassStats16's switch bits a configuration */
typedef unsigned long (__stdcall *flat_fn)(unsigned long *,unsigned long);
static int sse16,sse20;typedef unsigned long (__stdcall *stall_fn)(unsigned long *,unsigned long);static stall_fn stall_stats; /* v2.5: VXGL_StallStats */ /* v2.0: VXGL_SsePassStats16 ([10] box, [11] snapped vertices taken, [12] culling run triangles, [13] specular lanes by SSE) */
static flat_fn flat_stats,clip_stats,sse_stats; /* v1.7: VXGL_SsePassStats: [0] draws by the pass, [1] their vertices, [3] list draws left to the pipeline, [6] the last reason bits */static int stats8,stats10; /* v1.5: VXGL_ClipStats: [0] strips split, [1] runs as strips, [2] their triangles, [3] triangles alone, [4] clipped */ /* v1.4: VXGL_FlatStats10: [8] of [4] the engine's strips, [9] their triangles */ /* VXGL_FlatStats8: [0] strips sent whole, [1] their triangles, [2] strips whose colours differ, [3] theirs, [4] of them batched or
 * as the engine's strips, [5] theirs, [6] other flat primitives on the engine's flat shading, [7] their triangles (VXGL_FlatStats6: the first six) */
static FILE *logf;static HDC dc;static HWND win;
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcA(h,m,w,l);}
static double scene_t0; /* v2.4: the child's start (ms): the slow frames' times are from it */
static double now_ms(void){static LARGE_INTEGER f;LARGE_INTEGER t;if(!f.QuadPart)QueryPerformanceFrequency(&f);QueryPerformanceCounter(&t);return (double)t.QuadPart*1000.0/(double)f.QuadPart;}

static GLfloat va[2*SEG*3];static GLushort ix[2*SEG];
static void strip_colour(int s) {glColor3f(0.3f+0.7f*(float)((s*37)%11)/10.0f,0.2f+0.8f*(float)((s*17)%7)/6.0f,0.25f+0.75f*(float)((s*29)%5)/4.0f);}
/* strip s of the grid: x from 8 to 312, its two rows at y0 and y0 + 4 */
static float sx(int k){return 8.0f+(float)k*(304.0f/(float)(SEG-1));}
/* v1.5: the clipped scene's strips: x from -90 to 410, past both sides of the window (odd rows' top vertices 7 further: a triangle inside alone) */
static float cx(int k){return -90.0f+(float)k*(500.0f/(float)(SEG-1));}
static float sy(int s){return 10.0f+(float)s*4.6f;}
static void reset_state(void) {
 glDisable(GL_LIGHTING);glDisable(GL_LIGHT1);glDisable(GL_LIGHT2);glDisable(GL_COLOR_MATERIAL);glDisable(GL_FOG);glLightModeli(0x81F8,0x81F9);glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,0);{static const GLfloat z[4]={0,0,0,1};glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,z);}glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
 glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);glShadeModel(GL_FLAT);glDisableClientState(GL_VERTEX_ARRAY);
 glViewport(0,0,W,H);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,W,0,H,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
}
/* mode 1 a directional light; 2 and linear fog, a separate specular colour; 3 (v1.3) a positional light with specular (in the primary
 * colour) and exponential fog: Mesa's general lighting (light_rgba) and its exponential fog table, Lattice's kind */
static void lights(int mode) {
 static const GLfloat dir[4]={0.3f,0.5f,1.0f,0.0f},dif[4]={0.9f,0.9f,0.9f,1},amb[4]={0.15f,0.15f,0.15f,1},fc[4]={0.2f,0.25f,0.35f,1},sp[4]={0.5f,0.5f,0.5f,1};
 glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glLightfv(GL_LIGHT0,GL_POSITION,dir);glLightfv(GL_LIGHT0,GL_DIFFUSE,dif);glLightfv(GL_LIGHT0,GL_AMBIENT,amb);
 glLightf(GL_LIGHT0,GL_LINEAR_ATTENUATION,0.0f);
 glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);glEnable(GL_COLOR_MATERIAL);
 if(mode==2){
  glEnable(GL_FOG);glFogi(GL_FOG_MODE,GL_LINEAR);glFogf(GL_FOG_START,1.5f);glFogf(GL_FOG_END,3.5f);glFogfv(GL_FOG_COLOR,fc);
  glLightfv(GL_LIGHT0,GL_SPECULAR,sp);glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,sp);glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,10);
  glLightModeli(0x81F8,0x81FA);} /* GL_LIGHT_MODEL_COLOR_CONTROL = GL_SEPARATE_SPECULAR_COLOR: a secondary colour in the vertices */
 if(mode==4){static const GLfloat dir1[4]={-0.4f,0.3f,1.0f,0.0f},pos2[4]={100.0f,150.0f,50.0f,1.0f},sd[3]={0.3f,-0.2f,-1.0f},c1[4]={0.5f,0.4f,0.3f,1},c2[4]={0.3f,0.6f,0.5f,1};
  /* v1.6: and a directional light and a spotlight (Mesa's general lighting with three lights) */
  glEnable(GL_LIGHT1);glLightfv(GL_LIGHT1,GL_POSITION,dir1);glLightfv(GL_LIGHT1,GL_DIFFUSE,c1);glLightfv(GL_LIGHT1,GL_SPECULAR,c1);
  glEnable(GL_LIGHT2);glLightfv(GL_LIGHT2,GL_POSITION,pos2);glLightfv(GL_LIGHT2,GL_DIFFUSE,c2);glLightfv(GL_LIGHT2,GL_SPECULAR,c2);glLightfv(GL_LIGHT2,GL_SPOT_DIRECTION,sd);
  glLightf(GL_LIGHT2,GL_SPOT_CUTOFF,50.0f);glLightf(GL_LIGHT2,GL_SPOT_EXPONENT,4.0f);mode=3;}
 if(mode==3){static const GLfloat pos[4]={160.0f,120.0f,60.0f,1.0f}; /* in the scene's units (the modelview below): over the middle */
  glLightfv(GL_LIGHT0,GL_POSITION,pos);glLightf(GL_LIGHT0,GL_LINEAR_ATTENUATION,0.002f);
  glLightfv(GL_LIGHT0,GL_SPECULAR,sp);glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,sp);glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,20);
  glEnable(GL_FOG);glFogi(GL_FOG_MODE,GL_EXP);glFogf(GL_FOG_DENSITY,0.35f);glFogfv(GL_FOG_COLOR,fc);
  glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,1);} /* v1.4: a local viewer (Lattice's: two more normalizations a vertex) */
}
static void vcol(int s,int k,int f) {glColor3f((float)((s*7+k*3+f)%11)/10.0f,(float)((s*5+k*7+f)%7)/6.0f,(float)((s*3+k*5+f)%5)/4.0f);} /* v1.3: a colour per vertex */
/* v1.3: an ellipse of the row (centre x cx, the row's middle; 3.5 segments by 2): point i of n */
static void ell(float cx,float cy,int i,int n,float a0) {double a=a0+6.2831853*(double)i/(double)n;glVertex2f(cx+(float)(cos(a)*3.45*304.0/(SEG-1)),cy+(float)(sin(a)*1.95));}
static void draw_geom(int sc,int f);
/* v1.7: the display-list scenes (13-16) draw a base scene's geometry (frame 0's) from a list compiled once (glCallList, Lattice's way), the
 * frame's movement outside the list */
static GLuint list_id[6];static const int list_base[6]={10,11,12,8,10,18}; /* v2.0: 18 the culled scene's folded strips (lit as 10) */
static void draw_scene(int sc,int f) {
 int base=sc>=13&&sc<19?list_base[sc-13]:sc;float dx=(float)(f%5),dy=(float)(f%3);int lit=(base>=3&&base<=6)||(base>=10&&base<=12)||base==18;int lm=base==12?4:base>=10?3:base==5||base==6?2:1;
 reset_state();glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
 if(sc==21){ /* v3.1: lit strips (the positional light, as positional, EXP), smooth, back faces culled, a quarter of each strip turned away (SSEDEFER) */
  glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-1,1,-0.75,0.75,1,10);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
  glTranslatef(0,0,-2);glScalef(1.0f/80.0f,1.0f/80.0f,1);glTranslatef(-160,-120,0);lights(3);glTranslatef(dx,dy,0);
  glShadeModel(GL_SMOOTH);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);draw_geom(21,f);glDisable(GL_CULL_FACE);glShadeModel(GL_FLAT);return;}
 if(sc>=19){glClear(GL_DEPTH_BUFFER_BIT);if(sc==19)qs_q2(f);else qs_q3(f);reset_state();glDisable(GL_TEXTURE_2D);return;} /* v2.8: Quake 2's and Quake 3's ways */
 if(lit){ /* perspective: the strips at z -2 fill the window as in the orthographic scenes; w per vertex (Lattice's position layout) */
  glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-1,1,-0.75,0.75,1,10);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
  glTranslatef(0,0,-2);glScalef(1.0f/80.0f,1.0f/80.0f,1);glTranslatef(-160,-120,0);lights(lm);}
 glTranslatef(dx,dy,0);
 if(sc==18){glEnable(GL_CULL_FACE);glCullFace(GL_FRONT);} /* v2.0: the strips wind clockwise: front culled keeps the unfolded parts */
 if(sc>=13){int q=sc==17?0:sc==18?5:sc-13;if(!list_id[q]){list_id[q]=glGenLists(1);glNewList(list_id[q],GL_COMPILE);draw_geom(base,0);glEndList();}glCallList(list_id[q]);
  if(sc==17){glTranslatef(2000.0f,0.0f,0.0f);glCallList(list_id[q]);} /* v1.9: the same list again, whole outside the window */
  return;}
 draw_geom(sc,f);
}
static void draw_geom(int sc,int f) {
 int s,k;
 for(s=0;s<STRIPS;s++){float y0=sy(s),y1=y0+4.0f;
  strip_colour(s+f);
  if(sc==2){
   for(k=0;k<SEG;k++){va[k*6]=sx(k);va[k*6+1]=y0;va[k*6+2]=0;va[k*6+3]=sx(k);va[k*6+4]=y1;va[k*6+5]=0;ix[2*k]=(GLushort)(2*k);ix[2*k+1]=(GLushort)(2*k+1);}
   glVertexPointer(3,GL_FLOAT,0,va);glEnableClientState(GL_VERTEX_ARRAY);glDrawElements(GL_TRIANGLE_STRIP,2*SEG,GL_UNSIGNED_SHORT,ix);glDisableClientState(GL_VERTEX_ARRAY);
   continue;}
  if(sc==7){ /* each segment two triangles */
   glBegin(GL_TRIANGLES);
   for(k=0;k+1<SEG;k++){vcol(s,k,f);glVertex2f(sx(k),y0);vcol(s,k+1,f);glVertex2f(sx(k+1),y0);vcol(s,k+40,f);glVertex2f(sx(k),y1);
    vcol(s,k+21,f);glVertex2f(sx(k+1),y0);vcol(s,k+13,f);glVertex2f(sx(k+1),y1);vcol(s,k+5,f);glVertex2f(sx(k),y1);}
   glEnd();continue;}
  if(sc==8){ /* each segment a quad */
   glBegin(GL_QUADS);
   for(k=0;k+1<SEG;k++){vcol(s,k,f);glVertex2f(sx(k),y0);vcol(s,k+17,f);glVertex2f(sx(k+1),y0);vcol(s,k+9,f);glVertex2f(sx(k+1),y1);vcol(s,k+30,f);glVertex2f(sx(k),y1);}
   glEnd();continue;}
  if(sc==9){ /* nine a row: polygons of 16 vertices (14 triangles) on even rows, fans of a centre and 15 (14 triangles) on odd rows */
   int e,i;for(e=0;e<9;e++){float cx=sx(e*7)+3.5f*304.0f/(SEG-1),cy=y0+2.0f;
    if(s&1){glBegin(GL_TRIANGLE_FAN);vcol(s,e,f);glVertex2f(cx,cy);for(i=0;i<15;i++){vcol(s,e*16+i+1,f);ell(cx,cy,i,16,0.2f);}glEnd();}
    else{glBegin(GL_POLYGON);for(i=0;i<16;i++){vcol(s,e*16+i,f);ell(cx,cy,i,16,0.1f);}glEnd();}}
   continue;}
  glBegin(sc==1||sc==6?GL_QUAD_STRIP:GL_TRIANGLE_STRIP);
  if(sc==3)glNormal3f(0.5f*(float)sin(s*0.7),0.0f,1.0f);
  for(k=0;k<SEG;k++){
   if(sc>=4)glNormal3f(0.6f*(float)sin(k*0.35+s*0.5),0.6f*(float)cos(k*0.2+f*0.1),1.0f);
   {int fold=(sc==18&&((k/6)&1))||(sc==21&&(((k/16)+s)&3)==0); /* v2.0: every other six columns the strip's rows swapped: those triangles wind the other way; v3.1: smooth, culled: a quarter */
   glVertex2f(sc==11?cx(k):sx(k),fold?y1:y0);
   if(sc>=4)glNormal3f(0.6f*(float)sin(k*0.35+s*0.5+0.4),0.6f*(float)cos(k*0.2+f*0.1+0.3),1.0f);
   glVertex2f(sc==11?cx(k)+((s&1)?7.0f:0.0f):sx(k),fold?y0:y1);}}
  glEnd();}
}

/* v2.8: a second thread that makes a context of its own current on a window of its own, clears it and lets it go (a loader thread, a
 * splash screen): Mesa switches every GL call to per-thread dispatch from then on; v10.500's GLAPIONE goes back to the globals when the
 * first thread binds its context again */
static int side_pf;static volatile int side_ok;
static DWORD WINAPI side_thread(LPVOID u) {
 HWND w;HDC d;HGLRC r;PIXELFORMATDESCRIPTOR p;(void)u;
 w=CreateWindowA("VXFLAT","side",WS_OVERLAPPEDWINDOW,0,0,64,64,0,0,GetModuleHandleA(0),0);if(!w)return 0;d=GetDC(w);
 DescribePixelFormat(d,side_pf,sizeof(p),&p);if(SetPixelFormat(d,side_pf,&p)&&(r=wglCreateContext(d))!=0){
  if(wglMakeCurrent(d,r)){glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glFinish();side_ok=1;wglMakeCurrent(0,0);}wglDeleteContext(r);}
 ReleaseDC(w,d);DestroyWindow(w);return 0;
}
typedef unsigned long (__stdcall *s500_fn)(unsigned long *,unsigned long);static s500_fn s500;static int s501,s503,s505,s506;
/* ------------------------------------------------------------------ the child: one configuration */
static int child(int ci,int frames) {
 WNDCLASSA wc;HGLRC rc;PIXELFORMATDESCRIPTOR p;int pf=0,s,i,n;RECT r;static GLubyte img[W*H*4],shots[NSCENE][W*H*4];static char have_shot[NSCENE];char fn[64];unsigned long fl=0;
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="VXFLAT";wc.style=CS_OWNDC;RegisterClassA(&wc);
 r.left=0;r.top=0;r.right=W;r.bottom=H;AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
 scene_t0=now_ms(); /* v2.4 */
 win=CreateWindowA("VXFLAT",cfg[ci].name,WS_OVERLAPPEDWINDOW|WS_VISIBLE,60,60,r.right-r.left,r.bottom-r.top,0,0,wc.hInstance,0);
 if(!win){fprintf(logf,"%s: no window\n",cfg[ci].name);return 1;}
 dc=GetDC(win);n=DescribePixelFormat(dc,1,sizeof(p),&p);
 for(i=1;i<=n&&!pf;i++){DescribePixelFormat(dc,i,sizeof(p),&p);
  if((p.dwFlags&PFD_SUPPORT_OPENGL)&&(p.dwFlags&PFD_DOUBLEBUFFER)&&!(p.dwFlags&PFD_GENERIC_FORMAT))pf=i;}
 if(!pf){fprintf(logf,"%s: no ICD pixel format\n",cfg[ci].name);return 1;}
 DescribePixelFormat(dc,pf,sizeof(p),&p);if(!SetPixelFormat(dc,pf,&p)){fprintf(logf,"%s: SetPixelFormat failed\n",cfg[ci].name);return 1;}
 rc=wglCreateContext(dc);if(!rc||!wglMakeCurrent(dc,rc)){fprintf(logf,"%s: no context\n",cfg[ci].name);return 1;}
 flat_stats=(flat_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_FlatStats10");stats10=stats8=flat_stats!=0;
 if(!flat_stats){flat_stats=(flat_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_FlatStats8");stats8=flat_stats!=0;}
 if(!flat_stats)flat_stats=(flat_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_FlatStats6");
 if(flat_stats)fl=flat_stats(0,0);
 clip_stats=(flat_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_ClipStats");
 sse_stats=(flat_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_SsePassStats20");sse20=sse_stats!=0; /* v2.3 */
 stall_stats=(stall_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_StallStats");if(stall_stats)stall_stats(0,1); /* v2.5 (its clock measured here, outside the timing) */
 if(!sse_stats)sse_stats=(flat_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_SsePassStats16");sse16=sse_stats!=0;
 if(!sse_stats)sse_stats=(flat_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_SsePassStats10");
 if(!sse_stats)sse_stats=(flat_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_SsePassStats");
 fprintf(logf,"%s: %s, format %d, %s; %s (FLATSTRIP %lu, FLATBATCH %lu, EMIT %lu, HWFLAT %lu, HWFLATSTRIP %lu)\n",cfg[ci].name,(const char*)glGetString(GL_RENDERER),pf,(const char*)glGetString(GL_VERSION),
  stats10?"VXGL_FlatStats10":stats8?"VXGL_FlatStats8 (an older ICD)":flat_stats?"VXGL_FlatStats6 (an older ICD)":"no counts (an older ICD)",fl&1,(fl>>1)&1,(fl>>2)&1,(fl>>3)&1,(fl>>4)&3);
 if(sse16)fprintf(logf,"S %d %lX\n",ci,sse_stats(0,0));
 s500=(s500_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_Stats506");s506=s505=s503=s501=s500!=0; /* v3.5: VXGL_Stats509 gone with IMMSOA */
 if(!s500){s500=(s500_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_Stats506");s506=s505=s503=s501=s500!=0;} /* v3.3 */
 if(!s500){s500=(s500_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_Stats505");s505=s503=s501=s500!=0;} /* v3.2 */
 if(!s500){s500=(s500_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_Stats503");s503=s501=s500!=0;} /* v3.1 */
 if(!s500){s500=(s500_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_Stats501");s501=s500!=0;} /* v2.9 */
 if(!s500)s500=(s500_fn)GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_Stats500"); /* v2.8 */
 {qs_lock_fn lk=(qs_lock_fn)wglGetProcAddress("glLockArraysEXT");qs_unlock_fn ul=(qs_unlock_fn)wglGetProcAddress("glUnlockArraysEXT");qs_init(lk,ul);fprintf(logf,"Q %d %d\n",ci,lk&&ul);}
 {HANDLE t;DWORD id;unsigned long o[30],sw=0;memset(o,0,sizeof(o));side_pf=pf;t=CreateThread(0,0,side_thread,0,0,&id);if(t){WaitForSingleObject(t,INFINITE);CloseHandle(t);}
  wglMakeCurrent(0,0);wglMakeCurrent(dc,rc); /* the first thread binds its context again */
  if(s500)sw=s500(o,1);fprintf(logf,"G %d %d %lu %lu %lX\n",ci,side_ok,o[7],o[8],sw);} /* v2.2: the switch bits (2048 CULLRUN2, 4096 FASTBIND since v10.494) */
 for(s=0;s<NSCENE;s++){double t0,t1;int f;FILE *o;unsigned long st[10]={0,0,0,0,0,0,0,0,0,0},cs[5]={0,0,0,0,0},ss[20]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  draw_scene(s,0);glFinish();glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,img);
  memcpy(shots[s],img,sizeof(img));have_shot[s]=1; /* v3.1: kept in memory, written after the last scene (board, v3.0: a 260-290 ms frame early in the first child: the file's write) */
  SwapBuffers(dc);
  if(flat_stats)flat_stats(st,1);
  if(clip_stats)clip_stats(cs,1);
  if(sse_stats)sse_stats(ss,1);
  if(s500)s500(0,1); /* v2.8 */
  /* v2.4 (board, v2.3: a scene nothing changed 25% apart between two runs of the same configuration): the frames in three runs, the scene's
   * rate the median of the three; each frame's time kept (from before its draw to after its SwapBuffers), the slow ones (more than twice
   * the scene's median frame) logged with their time since the child started, so a period (a timer, a poll) or one long stall shows */
  {static double ft[3000],fst[3000];static unsigned long fvx[3000],fcode[3000],fbusy[3000];unsigned long vxmax=0;double rf[3],med,mx=0,sum=0;int run,nf=0,ns=0,fi=1,k;
   for(run=0;run<3;run++){int fr=run<2?frames/3:frames-2*(frames/3);double a=now_ms(),b;
    for(f=0;f<fr;f++,fi++){MSG m;double t=now_ms();draw_scene(s,fi);SwapBuffers(dc);while(PeekMessageA(&m,0,0,0,PM_REMOVE))DispatchMessageA(&m);if(nf<3000){unsigned long so[6]={0,0,0,0,0,0};fst[nf]=t-scene_t0;ft[nf]=now_ms()-t;if(stall_stats)stall_stats(so,1);fvx[nf]=so[0];fcode[nf]=so[1];fbusy[nf]=so[4]+(so[5]<<16);if(so[0]>vxmax)vxmax=so[0];nf++;}}
    glFinish();b=now_ms();rf[run]=b>a?fr*1000.0/(b-a):0;sum+=b-a;}
   t0=0;t1=sum; /* (the R line's rate below: the median run's) */
   {double a=rf[0],b=rf[1],c=rf[2],m3=a>b?(b>c?b:(a>c?c:a)):(a>c?a:(b>c?c:b));t1=m3>0?frames*1000.0/m3:sum;}
   {static double so[3000];int i,j;for(i=0;i<nf;i++){double v=ft[i];for(j=i;j>0&&so[j-1]>v;j--)so[j]=so[j-1];so[j]=v;}med=nf?so[nf/2]:0;mx=nf?so[nf-1]:0;}
   fprintf(logf,"U %d %d %.2f %.2f %.2f %.3f %.3f %lu",ci,s,rf[0],rf[1],rf[2],med,mx,vxmax); /* v2.5: the scene's longest VxD call (us) */
   for(k=0;k<nf;k++)if(ft[k]>2*med&&ft[k]>med+2.0){ns++;if(ns<=16)fprintf(logf," %d:%.1f@%.0f/%lu/%lX/%lX",k,ft[k],fst[k],fvx[k],fcode[k],fbusy[k]);} /* v2.5: the frame's longest VxD call (us), its code, busy answers (+ 1 ms waits << 16) */
   fprintf(logf," n%d\n",ns);}
  if(flat_stats)flat_stats(st,1);
  if(clip_stats)clip_stats(cs,1);
  if(sse_stats)sse_stats(ss,1);
  fprintf(logf,"R %d %d %.2f %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n",ci,s,frames*1000.0/(t1-t0),st[0],st[1],st[2],st[3],st[4],st[5],st[6],st[7],st[8],st[9],
   cs[0],cs[1],cs[2],cs[3],cs[4],ss[0],ss[1],ss[3],ss[6],ss[8],(unsigned long)glGetError(),ss[10],ss[11],ss[12],ss[13],ss[15],ss[16],ss[17],ss[18],ss[19]); /* v2.3: [16] draws inside by the box, [17] groups by the fused kernels, [18] partial set-ups, [19] lists bound by the short path */
  if(s500){unsigned long o[34];memset(o,0,sizeof(o));s500(o,1);
   if(s506)fprintf(logf,"Z %d %d %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n",ci,s,o[20],o[21],o[22],o[23],o[24],o[25],o[26],o[27],o[28],o[29]); /* v3.3: cycles / 256 by part; plan triangles, SSE2 scans, polygons into indexed packets */
   if(s505)fprintf(logf,"Y %d %d %lu %lu\n",ci,s,o[18],o[19]); /* v3.2: vertices by the fixed layouts, polygons in one call */
   if(s503)fprintf(logf,"X %d %d %lu %lu %lu %lu %lu\n",ci,s,o[13],o[14],o[15],o[16],o[17]); /* v3.1: recipe vertices, run triangles, deferred draws, groups lit, unlit */fprintf(logf,"T %d %d %lu %lu %lu %lu %lu %lu %lu\n",ci,s,o[0],o[1],o[2],o[3],o[4],o[5],o[6]);
   if(s501)fprintf(logf,"V %d %d %lu %lu %lu %lu\n",ci,s,o[9],o[10],o[11],o[12]);} /* v2.8; v2.9: byte colours, converted, prepacked packets, indexed triangles */fflush(logf);}
 wglMakeCurrent(0,0);wglDeleteContext(rc);ReleaseDC(win,dc);DestroyWindow(win);
 for(s=0;s<NSCENE;s++)if(have_shot[s]){FILE *o;sprintf(fn,"VXF%d%02d.RAW",ci,s);o=fopen(fn,"wb");if(o){fwrite(shots[s],1,W*H*4,o);fclose(o);}} /* v3.1 */
 return 0;
}

/* ------------------------------------------------------------------ the parent: every configuration, then the table */
static long raw_diff(int ca,int cb,int s,int tol) { /* pixels differing (any channel more than tol apart); -1 when a frame is missing */
 char fa[32],fb[32];FILE *a,*b;static GLubyte ia[W*H*4],ib[W*H*4];long d=-1;
 sprintf(fa,"VXF%d%02d.RAW",ca,s);sprintf(fb,"VXF%d%02d.RAW",cb,s);a=fopen(fa,"rb");b=fopen(fb,"rb");
 if(a&&b&&fread(ia,1,sizeof(ia),a)==sizeof(ia)&&fread(ib,1,sizeof(ib),b)==sizeof(ib)){int i,ch;d=0;for(i=0;i<W*H;i++)for(ch=0;ch<3;ch++)if(abs(ia[i*4+ch]-ib[i*4+ch])>tol){d++;break;}}
 if(a)fclose(a);if(b)fclose(b);return d;
}
int main(int argc,char **argv) {
 int frames=100,all=0,keeplog=0,ci,s,bad=0,ss_was=-1,lp_was=-1,po_was=-1;static double fps[NCFG][NSCENE];static unsigned long fs[NCFG][NSCENE][29];static int have[NCFG][NSCENE];char self[MAX_PATH],line[512];
 if(argc>2&&!strcmp(argv[1],"/child")){ci=atoi(argv[2]);if(argc>3)frames=atoi(argv[3]);logf=fopen("VXFLAT.LOG","a");if(!logf)return 1;s=child(ci,frames);fclose(logf);return s;}
 {int a;for(a=1;a<argc;a++){if(!stricmp(argv[a],"all"))all=1;else if(!stricmp(argv[a],"log"))keeplog=1;else frames=atoi(argv[a]);}} /* v2.6: VXFLAT [all] [log] [frames] */if(frames<10)frames=10; /* v2.2: VXFLAT [all] [frames] */
 /* v2.1: the screen saver and the monitor's power saving off for the run (a screen saver drawn over the scenes shares the CPU and the
  * engine: v2.0 on the board, Lattice), the settings put back at the end */
 {BOOL v=FALSE;if(SystemParametersInfoA(SPI_GETSCREENSAVEACTIVE,0,&v,0)){ss_was=v?1:0;SystemParametersInfoA(SPI_SETSCREENSAVEACTIVE,FALSE,0,0);}
  v=FALSE;if(SystemParametersInfoA(0x0053,0,&v,0)){lp_was=v?1:0;SystemParametersInfoA(0x0055,FALSE,0,0);} /* SPI_GETLOWPOWERACTIVE, SPI_SETLOWPOWERACTIVE */
  v=FALSE;if(SystemParametersInfoA(0x0054,0,&v,0)){po_was=v?1:0;SystemParametersInfoA(0x0056,FALSE,0,0);}} /* SPI_GETPOWEROFFACTIVE, SPI_SETPOWEROFFACTIVE */
 remove("VXFLAT.LOG");GetModuleFileNameA(0,self,sizeof(self));
 for(ci=0;ci<NCFG;ci++){char cmd[MAX_PATH+40],env[256],*e;STARTUPINFOA si;PROCESS_INFORMATION pi;int k;
  if(!use_cfg(all,ci))continue;
  for(k=0;env_keys[k];k++)SetEnvironmentVariableA(env_keys[k],NULL);
  /* v2.6 (board, v2.5: a frame 20-50 ms long every 200 swaps, VXGL.LOG's frame report): the ICD's log, sampler and frame log off for the children
   * (VXFLAT log keeps the machine's settings) */
  SetEnvironmentVariableA("ACCEL_OPENGL_TSCPROF","1"); /* v3.3: the ICD's cycle counts in every child (the same cost in every configuration) */
  if(!keeplog){SetEnvironmentVariableA("VXGL_LOG","0");SetEnvironmentVariableA("VXGL_SAMPLE","0");SetEnvironmentVariableA("VXGL_FRAMELOG","0");}
  strcpy(env,cfg[ci].env);for(e=strtok(env,";");e;e=strtok(0,";")){char *q=strchr(e,'=');if(q){*q=0;SetEnvironmentVariableA(e,q+1);}}
  sprintf(cmd,"\"%s\" /child %d %d",self,ci,ci==CSOFT?(frames<20?frames:20):frames);memset(&si,0,sizeof(si));si.cb=sizeof(si);
  if(CreateProcessA(0,cmd,0,0,FALSE,0,0,0,&si,&pi)){WaitForSingleObject(pi.hProcess,INFINITE);CloseHandle(pi.hProcess);CloseHandle(pi.hThread);}}
 {int k;for(k=0;env_keys[k];k++)SetEnvironmentVariableA(env_keys[k],NULL);}
 {FILE *l=fopen("VXFLAT.LOG","r");if(l){while(fgets(line,sizeof(line),l)){int a,b;unsigned long x[30];double v;
   if(sscanf(line,"S %d %lX",&a,&x[0])==2&&a>=0&&a<NCFG){sw_bits[a]=x[0];have_sw[a]=1;continue;}
   if(sscanf(line,"T %d %d %lu %lu %lu %lu %lu %lu %lu",&a,&b,&x[0],&x[1],&x[2],&x[3],&x[4],&x[5],&x[6])==9&&a>=0&&a<NCFG&&b>=0&&b<NSCENE){int k;for(k=0;k<7;k++)ts5[a][b][k]=x[k];have_t[a][b]=1;continue;} /* v2.8 */
   if(sscanf(line,"V %d %d %lu %lu %lu %lu",&a,&b,&x[0],&x[1],&x[2],&x[3])==6&&a>=0&&a<NCFG&&b>=0&&b<NSCENE){int k;for(k=0;k<4;k++)vs5[a][b][k]=x[k];have_v[a][b]=1;continue;} /* v2.9 */
   if(sscanf(line,"Z %d %d %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",&a,&b,&x[0],&x[1],&x[2],&x[3],&x[4],&x[5],&x[6],&x[7],&x[8],&x[9])==12&&a>=0&&a<NCFG&&b>=0&&b<NSCENE){int k;for(k=0;k<10;k++)zs5[a][b][k]=x[k];have_z[a][b]=1;continue;} /* v3.3 */
   if(sscanf(line,"Y %d %d %lu %lu",&a,&b,&x[0],&x[1])==4&&a>=0&&a<NCFG&&b>=0&&b<NSCENE){ys5[a][b][0]=x[0];ys5[a][b][1]=x[1];have_y[a][b]=1;continue;} /* v3.2 */
   if(sscanf(line,"X %d %d %lu %lu %lu %lu %lu",&a,&b,&x[0],&x[1],&x[2],&x[3],&x[4])==7&&a>=0&&a<NCFG&&b>=0&&b<NSCENE){int k;for(k=0;k<5;k++)xs5[a][b][k]=x[k];have_x[a][b]=1;continue;} /* v3.1 */
   if(sscanf(line,"G %d %lu %lu %lu %lX",&a,&x[0],&x[1],&x[2],&x[3])==5&&a>=0&&a<NCFG){int k;for(k=0;k<4;k++)g5[a][k]=x[k];have_g[a]=1;continue;}
   if(sscanf(line,"Q %d %lu",&a,&x[0])==2&&a>=0&&a<NCFG){have_q[a]=x[0]!=0;continue;}
   if(line[0]=='U'&&line[1]==' '){double r0,r1,r2,md,mxv;int nn=0; /* v2.4: the three runs, the frames' median and longest, the slow frames (index:ms@time) */
    unsigned long vxm=0;
    if(sscanf(line,"U %d %d %lf %lf %lf %lf %lf %lu%n",&a,&b,&r0,&r1,&r2,&md,&mxv,&vxm,&nn)>=8&&a>=0&&a<NCFG&&b>=0&&b<NSCENE){char *q=line+nn;int fi;double ms,at;unsigned long fv,fc,fb;
     runs[a][b][0]=r0;runs[a][b][1]=r1;runs[a][b][2]=r2;fmed[a][b]=md;fmaxt[a][b]=mxv;have_u[a][b]=1;vxlong[a][b]=vxm;
     while(sscanf(q," %d:%lf@%lf/%lu/%lX/%lX%n",&fi,&ms,&at,&fv,&fc,&fb,&nn)==6){if(nslow_at[a]<400)slow_at[a][nslow_at[a]++]=at;
      if(nslow_list<64){sl_cfg[nslow_list]=a;sl_sc[nslow_list]=b;sl_fi[nslow_list]=fi;sl_ms[nslow_list]=ms;sl_at[nslow_list]=at;sl_vx[nslow_list]=fv;sl_code[nslow_list]=fc;sl_busy[nslow_list]=fb;nslow_list++;}q+=nn;}
     {char *e=strstr(line," n");if(e)nslow[a][b]=atoi(e+2);}}
    continue;}
   if(sscanf(line,"R %d %d %lf %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",&a,&b,&v,&x[0],&x[1],&x[2],&x[3],&x[4],&x[5],&x[6],&x[7],&x[8],&x[9],&x[10],&x[11],&x[12],&x[13],&x[14],&x[15],&x[16],&x[17],&x[18],&x[19],&x[20],&x[21],&x[22],&x[23],&x[24],&x[25],&x[26],&x[27],&x[28],&x[29])==33&&
     a>=0&&a<NCFG&&b>=0&&b<NSCENE){int k;fps[a][b]=v;for(k=0;k<20;k++)fs[a][b][k]=x[k];for(k=0;k<9;k++)fs[a][b][20+k]=x[21+k];have[a][b]=1;}}fclose(l);}} /* v2.3: [25] draws inside by the box, [26] fused groups, [27] partial set-ups, [28] lists bound by the short path */ /* v2.0: [20] box, [21] snapped vertices taken, [22] culling run triangles, [23] specular lanes by SSE; v2.1: [24] set-ups kept */
 if(ss_was>=0)SystemParametersInfoA(SPI_SETSCREENSAVEACTIVE,(UINT)ss_was,0,0); /* v2.1: put back */
 if(lp_was>=0)SystemParametersInfoA(0x0055,(UINT)lp_was,0,0);
 if(po_was>=0)SystemParametersInfoA(0x0056,(UINT)po_was,0,0);
 logf=fopen("VXFLAT.TXT","w");if(!logf)return 1;
 fprintf(logf,"VXFLAT v3.5 (driver .510): %dx%d, 48 rows of 126 triangles a frame (6048 triangles), flat shading (smooth, culled: smooth), %d frames a scene (software 20); %s\n",W,H,frames,all?"every configuration (all)":"quick: the default, the template, software, the default again (VXFLAT all: every configuration)");
 fprintf(logf,"frames a second / pixels off the software frame (a channel more than 16 apart, of %d)\n%-16s",W*H,"");
 for(ci=0;ci<NCFG;ci++)if(use_cfg(all,ci))fprintf(logf,"%11s",cfg[ci].name);fprintf(logf,"\n");
 for(s=0;s<NSCENE;s++){fprintf(logf,"%-16s",scene_name[s]);
  for(ci=0;ci<NCFG;ci++){char c[40];if(!use_cfg(all,ci))continue;if(!have[ci][s])strcpy(c,"-");else sprintf(c,"%.1f/%ld",fps[ci][s],raw_diff(ci,CSOFT,s,16));fprintf(logf,"%11s",c);}
  fprintf(logf,"\n");}
 fprintf(logf,"\neach against the template (v10.479's path), pixels that differ at all (want 0; norsqrt, nolightfast, nossepass, nosseimm and nosseshine: other\narithmetic than the template's: any number, but none more than 1 apart in a channel, the count after the slash):\n%-16s","");
 for(ci=0;ci<CTPL;ci++)if(use_cfg(all,ci))fprintf(logf,"%11s",cfg[ci].name);fprintf(logf,"%11s\n",cfg[CDEF2].name);
 for(s=0;s<NSCENE;s++){fprintf(logf,"%-16s",scene_name[s]);
  for(ci=0;ci<=CDEF2;ci++){long d,d1;char c[32];if(ci>=CTPL&&ci!=CDEF2)continue;if(!use_cfg(all,ci))continue;if(!have[ci][s]){fprintf(logf,"%11s","-");continue;}d=raw_diff(ci,CTPL,s,0);
   if(ci==CRSQ||ci==CLF||ci==CSP||ci==CSI||ci==CSH){d1=raw_diff(ci,CTPL,s,1);sprintf(c,"%ld/%ld",d,d1);if(d1!=0)bad++;}else{sprintf(c,"%ld",d);if(d!=0)bad++;}fprintf(logf,"%11s",c);}
  fprintf(logf,"\n");}
 fprintf(logf,"\na frame: strips sent whole / strips whose colours differ (of them the engine's strips) / triangles from other flat primitives on the engine's flat shading:\n%-16s","");
 for(ci=0;ci<CSOFT;ci++)if(use_cfg(all,ci))fprintf(logf,"%17s",cfg[ci].name);fprintf(logf,"\n");
 for(s=0;s<NSCENE;s++){fprintf(logf,"%-16s",scene_name[s]);
  for(ci=0;ci<CSOFT;ci++){char c[48];if(!use_cfg(all,ci))continue;if(!have[ci][s])strcpy(c,"-");else sprintf(c,"%.0f/%.0f(%.0f)/%.0f",(double)fs[ci][s][0]/frames,(double)fs[ci][s][2]/frames,(double)fs[ci][s][8]/frames,(double)fs[ci][s][7]/frames);fprintf(logf,"%17s",c);}
  fprintf(logf,"\n");}
 fprintf(logf,"expected (default): 48/0(0)/0 in the first four scenes, 0/48(48)/0 in the lit strip scenes (normals, fog, quad strips, positional), 0/0(0)/6048 in triangles, quads, fans and polygons\n");
 if(have[0][0])for(s=0;s<NSCENE;s++){double w=(double)fs[0][s][0]/frames,d=(double)fs[0][s][2]/frames,b=(double)fs[0][s][8]/frames,t=(double)fs[0][s][7]/frames;
  int lit,sok,ok;if(s==19||s==20)continue; /* v2.8: the Quake scenes are judged below */
  lit=(s>=10&&s<=15)||s==17||s==18||s==21;sok=lit?fs[0][s][15]>0&&fs[0][s][17]==0:fs[0][s][15]==0; /* v1.8: every draw of Mesa's general lighting's case by the pass, no other;
   * v1.8.1: the scenes lit with a directional light and no local viewer (Mesa's fast lighting, reason 20) or a separate specular colour (220) are not that case */
  ok=!sok?0:s>=13?1:s<=3?w>47.5:(s>=7&&s<=9)?t>6047.5&&w<0.5&&d<0.5:s==11?(double)fs[0][s][10]/frames>47.5&&(double)fs[0][s][11]/frames>47.5&&fs[0][s][14]>0:w<0.5&&b>47.5;
  if(!ok){bad++;fprintf(logf,"default, %s: not as expected\n",scene_name[s]);}}
 fprintf(logf,"\nthe clipped scene a frame: strips split / runs inside as strips (their triangles) / triangles inside alone / triangles clipped (VXGL_ClipStats):\n");
 for(ci=0;ci<CSOFT;ci++)if(have[ci][11])fprintf(logf,"  %-11s %.0f / %.0f (%.0f) / %.0f / %.0f\n",cfg[ci].name,(double)fs[ci][11][10]/frames,(double)fs[ci][11][11]/frames,(double)fs[ci][11][12]/frames,(double)fs[ci][11][13]/frames,(double)fs[ci][11][14]/frames);
 fprintf(logf,"expected: default 48 strips split a frame, 48 runs or more, some clipped; noclipstrip and template 0 (Mesa's clipping table)\n");
 fprintf(logf,"\nthe SSE pass a frame: draws by it (of them not from a list) / draws left to Mesa's stages (the last reason bits) (VXGL_SsePassStats10):\n%-16s","");
 for(ci=0;ci<CSOFT;ci++)if((ci==0||ci>=CSP)&&(use_cfg(all,ci)))fprintf(logf,"%16s",cfg[ci].name);fprintf(logf,"\n");
 for(s=0;s<NSCENE;s++){fprintf(logf,"%-16s",scene_name[s]);for(ci=0;ci<CSOFT;ci++){char c[48];if(ci&&ci<CSP)continue;if(!use_cfg(all,ci))continue;if(!have[ci][s])strcpy(c,"-");else sprintf(c,"%.0f(%.0f)/%.0f(%lX)",(double)fs[ci][s][15]/frames,(double)fs[ci][s][19]/frames,(double)fs[ci][s][17]/frames,fs[ci][s][18]);fprintf(logf,"%16s",c);}fprintf(logf,"\n");}
 fprintf(logf,"expected: default every draw lit as Mesa's general case by the pass (positional, clipped, three lights, the lit lists; lists and not), the other lit scenes left (reason 20: Mesa's fast lighting, 220: a separate specular colour), every unlit one left (reason 4000); nossepass none by the pass; nosseimm only the lists; sseunlit every draw\n");
 /* v2.0: the pass's v10.490 parts a frame */
 fprintf(logf,"\nthe SSE pass's parts a frame: draws whole outside by the box / vertices the strip path took snapped by the pass / triangles by the culling run / specular lanes by SSE / lit set-ups kept (VXGL_SsePassStats16):\n%-16s","");
 fprintf(logf,"%24s",cfg[0].name);for(ci=CBOX;ci<=CFB;ci++)if(use_cfg(all,ci))fprintf(logf,"%24s",cfg[ci].name);fprintf(logf,"\n");
 for(s=0;s<NSCENE;s++){fprintf(logf,"%-16s",scene_name[s]);for(ci=0;ci<=CFB;ci++){char c[80];if(ci&&ci<CBOX)continue;if(!use_cfg(all,ci))continue;if(!have[ci][s])strcpy(c,"-");
   else sprintf(c,"%.0f/%.0f/%.0f/%.0f/%.0f",(double)fs[ci][s][20]/frames,(double)fs[ci][s][21]/frames,(double)fs[ci][s][22]/frames,(double)fs[ci][s][23]/frames,(double)fs[ci][s][24]/frames);fprintf(logf,"%24s",c);}fprintf(logf,"\n");}
 if(have[0][0]){static const int part_scene[5]={17,13,18,13,13};static const int part_cfg[5]={CBOX,-1,CCULL,CSH,CCACHE};int p; /* v3.3: part 1 (the pass's grid words: SSESNAP2's, its own check below) not here */
  for(p=0;p<5;p++){int sc=part_scene[p],off=part_cfg[p],on=0;if(off<0)continue;
   if(have[on][sc]&&!fs[on][sc][20+p]){bad++;fprintf(logf,"%s, %s: the part %d not used\n",cfg[on].name,scene_name[sc],p);}
   for(s=0;s<NSCENE;s++)if(have[off][s]&&fs[off][s][20+p]){bad++;fprintf(logf,"%s, %s: the part %d used though off\n",cfg[off].name,scene_name[s],p);}}}
 /* v2.3: the v10.495 parts a frame */
 fprintf(logf,"\nthe v10.495 parts a frame: list draws inside every plane by the box (no clip compares) / groups by the fused kernels / set-ups for the material only / lists bound by the short path (VXGL_SsePassStats20):\n%-16s","");
 fprintf(logf,"%22s",cfg[0].name);for(ci=CFUSE;ci<=CIMB;ci++)if(use_cfg(all,ci))fprintf(logf,"%22s",cfg[ci].name);fprintf(logf,"\n");
 for(s=0;s<NSCENE;s++){fprintf(logf,"%-16s",scene_name[s]);for(ci=0;ci<=CIMB;ci++){char c[80];if(ci&&ci<CFUSE)continue;if(!use_cfg(all,ci))continue;if(!have[ci][s])strcpy(c,"-");
   else sprintf(c,"%.0f/%.0f/%.0f/%.0f",(double)fs[ci][s][25]/frames,(double)fs[ci][s][26]/frames,(double)fs[ci][s][27]/frames,(double)fs[ci][s][28]/frames);fprintf(logf,"%22s",c);}fprintf(logf,"\n");}
 if(have[0][0]&&have_sw[0]&&(sw_bits[0]&8192UL)){ /* the default (a driver with the parts): fused groups in every scene the pass draws, lists bound short in every list scene and only
   * there, the box's inside and the partial set-ups in "list, positional"; each part 0 in every scene with its switch off (nossebox2: no inside either) */
  for(s=0;s<NSCENE;s++){int lst=s>=13&&s<=18;
   if(have[0][s]&&fs[0][s][15]&&!fs[0][s][26]){bad++;fprintf(logf,"default, %s: no group by the fused kernels\n",scene_name[s]);}
   if(have[0][s]&&(lst?!fs[0][s][28]:fs[0][s][28]!=0)){bad++;fprintf(logf,"default, %s: lists bound by the short path %lu\n",scene_name[s],fs[0][s][28]);}
   if(have[CFUSE][s]&&fs[CFUSE][s][26]){bad++;fprintf(logf,"nossefuse, %s: fused groups\n",scene_name[s]);}
   if(have[CACC][s]&&fs[CACC][s][25]){bad++;fprintf(logf,"nosseaccept, %s: draws inside\n",scene_name[s]);}
   if(have[CBOX2][s]&&fs[CBOX2][s][25]){bad++;fprintf(logf,"nossebox2, %s: draws inside\n",scene_name[s]);}
   if(have[CPART][s]&&fs[CPART][s][27]){bad++;fprintf(logf,"nossepartial, %s: partial set-ups\n",scene_name[s]);}
   if(have[CLB][s]&&fs[CLB][s][28]){bad++;fprintf(logf,"nolistbind, %s: lists bound by the short path\n",scene_name[s]);}}
  if(have[0][13]&&!fs[0][13][25]){bad++;fprintf(logf,"default, %s: no draw inside by the box\n",scene_name[13]);}
  if(have[0][13]&&!fs[0][13][27]){bad++;fprintf(logf,"default, %s: no partial set-up\n",scene_name[13]);}
  /* v2.4 (v10.496): the draws not from a list inside by their own box: positional, EXP and three lights; none with nosseimmbox in the scenes not from a list */
  if(sw_bits[0]&262144UL){if(have[0][10]&&!fs[0][10][25]){bad++;fprintf(logf,"default, %s: no draw inside by its own box\n",scene_name[10]);}
   if(have[0][12]&&!fs[0][12][25]){bad++;fprintf(logf,"default, %s: no draw inside by its own box\n",scene_name[12]);}
   for(s=0;s<13;s++)if(have[CIMB][s]&&fs[CIMB][s][25]){bad++;fprintf(logf,"nosseimmbox, %s: draws inside\n",scene_name[s]);}}
  /* v2.5 (v10.497): the strip path takes the pass's grid words in every scene the pass draws (the snapped-vertices count), none with nossesnap2 */
  if(sw_bits[0]&524288UL)for(s=0;s<NSCENE;s++){if(have[0][s]&&fs[0][s][15]&&s!=17&&!fs[0][s][21]){bad++;fprintf(logf,"default, %s: no grid words from the pass\n",scene_name[s]);}
   if(have[CSN2][s]&&fs[CSN2][s][21]){bad++;fprintf(logf,"nossesnap2, %s: grid words from the pass\n",scene_name[s]);}}}
 fprintf(logf,"expected: default fused groups in every scene the pass draws, lists bound by the short path in the list scenes only, draws inside and partial set-ups in list, positional; v10.496: draws inside in positional, EXP and three lights (no list); each part 0 with its switch off\n");
 /* v2.2: the switch bits each configuration's driver reported: the default with SSETIGHT, CULLRUN2 and FASTBIND on, each of the three
  * configurations with its own off */
 {static const int sw_cfg[12]={CTIGHT,CCR2,CFB,CFUSE,CACC,CBOX2,CPART,CLB,CIMB,CSN2,COUTF,CEARLY};static const unsigned long sw_bit[12]={1024,2048,4096,8192,16384,32768,65536,131072,262144,524288,1048576,2097152};int p; /* v2.3: the v10.495 bits */
  fprintf(logf,"\nswitch bits (VXGL_SsePassStats16):");for(ci=0;ci<NCFG;ci++)if(have_sw[ci])fprintf(logf," %s %lX",cfg[ci].name,sw_bits[ci]);fprintf(logf,"\n");
  for(p=0;p<12;p++){if(p>=3&&!(sw_bits[0]&8192UL))break; /* a driver before v10.495 */if(p==8&&!(sw_bits[0]&262144UL))break; /* before v10.496 */if(p==9&&!(sw_bits[0]&524288UL))break; /* before v10.497 */
   if(have_sw[0]&&!(sw_bits[0]&sw_bit[p])){bad++;fprintf(logf,"default: switch bit %lX off\n",sw_bit[p]);}
   if(have_sw[sw_cfg[p]]&&(sw_bits[sw_cfg[p]]&sw_bit[p])){bad++;fprintf(logf,"%s: switch bit %lX on\n",cfg[sw_cfg[p]].name,sw_bit[p]);}}}
 fprintf(logf,"expected: default the box in list, half out; the culling run in list, culled; SSE specular in the positional scenes; set-ups kept in the lit scenes; no snapped vertices (ssesnap: in every scene the pass draws); each part 0 with its switch off\n");
 /* v2.4: the runs and the slow frames */
 fprintf(logf,"\nthe three runs of each scene (frames a second; the table above has their median), the spread (the fastest over the slowest, %%), slow frames (more than twice the median frame time) and the longest frame / the median (ms):\n%-16s","");
 for(ci=0;ci<NCFG;ci++)if(use_cfg(all,ci))fprintf(logf,"%26s",cfg[ci].name);fprintf(logf,"\n");
 for(s=0;s<NSCENE;s++){fprintf(logf,"%-16s",scene_name[s]);for(ci=0;ci<NCFG;ci++){char c[64];double lo,hi;if(!use_cfg(all,ci))continue;if(!have_u[ci][s]){fprintf(logf,"%26s","-");continue;}
   lo=runs[ci][s][0];hi=lo;{int k;for(k=1;k<3;k++){if(runs[ci][s][k]<lo)lo=runs[ci][s][k];if(runs[ci][s][k]>hi)hi=runs[ci][s][k];}}
   sprintf(c,"%+.0f%% s%d %.0f/%.1f",lo>0?(hi/lo-1)*100:0,nslow[ci][s],fmaxt[ci][s],fmed[ci][s]);fprintf(logf,"%26s",c);}fprintf(logf,"\n");}
 /* the slow frames' spacing, a configuration at a time (its child's running time): the most common gaps, to 10 ms */
 fprintf(logf,"\nslow frames' spacing (the gaps between successive slow frames in a configuration's run, to 10 ms; the most common three, with how many):\n");
 for(ci=0;ci<NCFG;ci++){int n=nslow_at[ci],i,j,best[3]={-1,-1,-1},bc[3]={0,0,0};static int hist[2000];if(!n||(!use_cfg(all,ci)))continue;memset(hist,0,sizeof(hist));
  for(i=1;i<n;i++){double g=slow_at[ci][i]-slow_at[ci][i-1];int b=(int)(g/10.0+0.5);if(b>=0&&b<2000)hist[b]++;}
  for(j=0;j<3;j++){int bb=-1,bv=0;for(i=0;i<2000;i++){int used=0,k;for(k=0;k<j;k++)if(best[k]==i)used=1;if(!used&&hist[i]>bv){bv=hist[i];bb=i;}}best[j]=bb;bc[j]=bv;}
  fprintf(logf,"  %-12s %d slow frames;",cfg[ci].name,n);for(j=0;j<3;j++)if(best[j]>=0)fprintf(logf," %d ms x%d",best[j]*10,bc[j]);fprintf(logf,"\n");}
 fprintf(logf,"(a gap that repeats is a period: a timer or a poll; a few long frames with no common gap: stalls)\n");
 /* v2.5: each slow frame with the longest VxD call in it (VXGL_StallStats): a VxD call as long as the frame's excess is the driver's wait */
 fprintf(logf,"\nslow frames: configuration, scene, frame, ms (the scene's median), at (ms from the child's start), the frame's longest VxD call (ms) and its control code, busy answers / 1 ms waits:\n");
 {int k;for(k=0;k<nslow_list;k++)fprintf(logf,"  %-12s %-16s %3d %6.1f (%4.1f) @%6.0f  VxD %6.2f ms %08lX  %lu/%lu\n",cfg[sl_cfg[k]].name,scene_name[sl_sc[k]],sl_fi[k],sl_ms[k],fmed[sl_cfg[k]][sl_sc[k]],sl_at[k],sl_vx[k]/1000.0,sl_code[k],sl_busy[k]&0xFFFFUL,sl_busy[k]>>16);}
 fprintf(logf,"the longest VxD call in each scene (ms):\n%-16s","");for(ci=0;ci<NCFG;ci++)if(use_cfg(all,ci))fprintf(logf,"%12s",cfg[ci].name);fprintf(logf,"\n");
 for(s=0;s<NSCENE;s++){fprintf(logf,"%-16s",scene_name[s]);for(ci=0;ci<NCFG;ci++){if(!use_cfg(all,ci))continue;if(have_u[ci][s])fprintf(logf,"%12.2f",vxlong[ci][s]/1000.0);else fprintf(logf,"%12s","-");}fprintf(logf,"\n");}
 /* v2.8: v10.500's parts a frame (VXGL_Stats500) and the second thread's dispatch */
 fprintf(logf,"\nthe v10.500 parts a frame: triangles in one call (TRILIST) / locked-array transforms kept, made (CVACACHE) / vertices with the store's grid words (STORESNAP):\n%-16s","");
 fprintf(logf,"%22s",cfg[0].name);for(ci=CTRI;ci<=CGLA;ci++)if(use_cfg(all,ci))fprintf(logf,"%22s",cfg[ci].name);fprintf(logf,"\n");
 for(s=0;s<NSCENE;s++){fprintf(logf,"%-16s",scene_name[s]);for(ci=0;ci<=CGLA;ci++){char c[80];if(ci&&ci<CTRI)continue;if(!use_cfg(all,ci))continue;if(!have_t[ci][s])strcpy(c,"-");
   else sprintf(c,"%.0f/%.0f,%.0f/%.0f",(double)ts5[ci][s][1]/frames,(double)ts5[ci][s][3]/frames,(double)ts5[ci][s][4]/frames,(double)ts5[ci][s][6]/frames);fprintf(logf,"%22s",c);}fprintf(logf,"\n");}
 fprintf(logf,"the second thread's context (made current / per-thread dispatch entered / left / switch bits):");for(ci=0;ci<NCFG;ci++)if(have_g[ci])fprintf(logf," %s %lu/%lu/%lu/%lX",cfg[ci].name,g5[ci][0],g5[ci][1],g5[ci][2],g5[ci][3]);fprintf(logf,"\n");
 if(have_t[0][0]&&have_g[0]&&(g5[0][3]&1)){static const int sw_c[5]={CTRI,CCVA,CFVX,CSSN,CGLA};int p;
  /* the default: triangles in one call in q3 arrays and triangles (GL_TRIANGLES), kept transforms in q3 arrays, grid words in every unlit scene the pass does not draw;
   * each 0 with its switch off; the dispatch left again after the side thread (none left with noglapione); each configuration's bit off */
  if(have_t[0][20]&&!ts5[0][20][1]){bad++;fprintf(logf,"default, q3 arrays: no triangles in one call\n");}
  if(have_q[0]&&have_t[0][20]&&!ts5[0][20][3]){bad++;fprintf(logf,"default, q3 arrays: no locked-array transform kept\n");}
  for(s=0;s<NSCENE;s++)if(have_t[0][s]&&(s<=2||s==7||s==8||s==9||s==19||s==20)&&!ts5[0][s][6]){bad++;fprintf(logf,"default, %s: no grid words from the store\n",scene_name[s]);}
  for(s=0;s<NSCENE;s++){if(have_t[CTRI][s]&&ts5[CTRI][s][1]){bad++;fprintf(logf,"notrilist, %s: triangles in one call\n",scene_name[s]);}
   if(have_t[CCVA][s]&&ts5[CCVA][s][3]){bad++;fprintf(logf,"nocvacache, %s: transforms kept\n",scene_name[s]);}
   if(have_t[CSSN][s]&&ts5[CSSN][s][6]){bad++;fprintf(logf,"nostoresnap, %s: grid words from the store\n",scene_name[s]);}}
  if(g5[0][0]&&(!g5[0][1]||!g5[0][2])){bad++;fprintf(logf,"default: the second thread's per-thread dispatch entered %lu, left %lu (want both)\n",g5[0][1],g5[0][2]);}
  if(have_g[CGLA]&&g5[CGLA][0]&&g5[CGLA][2]){bad++;fprintf(logf,"noglapione: per-thread dispatch left %lu times\n",g5[CGLA][2]);}
  for(p=0;p<5;p++){if(!(g5[0][3]&(1UL<<p))){bad++;fprintf(logf,"default: switch bit %lX off\n",1UL<<p);}
   if(have_g[sw_c[p]]&&(g5[sw_c[p]][3]&(1UL<<p))){bad++;fprintf(logf,"%s: switch bit %lX on\n",cfg[sw_c[p]].name,1UL<<p);}}}
 fprintf(logf,"expected: default triangles in one call in q3 arrays (triangles: flat shading, its own path), transforms kept in q3 arrays, grid words in the unlit scenes, the dispatch left again after the second thread; each 0 with its switch off\n");
 /* v2.9: v10.501's parts a frame (VXGL_Stats501) */
 fprintf(logf,"\nthe v10.501 parts a frame: draws with byte colours (UBCOLOR; converted after all) / prepacked packets (PREPACK) / triangles in indexed packets (INDEXED):\n%-16s","");
 fprintf(logf,"%22s",cfg[0].name);for(ci=CUB;ci<=CIMM;ci++)if(use_cfg(all,ci))fprintf(logf,"%22s",cfg[ci].name);fprintf(logf,"\n");
 for(s=0;s<NSCENE;s++){fprintf(logf,"%-16s",scene_name[s]);for(ci=0;ci<=CIMM;ci++){char c[80];if(ci&&ci<CUB)continue;if(!use_cfg(all,ci))continue;if(!have_v[ci][s])strcpy(c,"-");
   else sprintf(c,"%.0f(%.0f)/%.0f/%.0f",(double)vs5[ci][s][0]/frames,(double)vs5[ci][s][1]/frames,(double)vs5[ci][s][2]/frames,(double)vs5[ci][s][3]/frames);fprintf(logf,"%22s",c);}fprintf(logf,"\n");}
 if(have_v[0][20]&&have_g[0]&&(g5[0][3]&32)){
  /* the default: byte colours in q3 arrays, none converted; prepacked packets in every engine scene (when the VxD's prepack is on: bit 64); no
   * indexed triangles; each part 0 with its switch off, indexed triangles in q3 arrays with indexed (when the VxD has its vertex area: bit 128) */
  if(!vs5[0][20][0]||vs5[0][20][1]){bad++;fprintf(logf,"default, q3 arrays: byte colours %lu, converted %lu (want some, none)\n",vs5[0][20][0],vs5[0][20][1]);}
  for(s=0;s<NSCENE;s++){if(have_v[0][s]&&(g5[0][3]&64)&&(s==19||s==20)&&!vs5[0][s][2]){bad++;fprintf(logf,"default, %s: no prepacked packet\n",scene_name[s]);} /* v3.0: the textured scenes
   * (v2.9 expected every scene: the 10-word form needs the engine's shader program, which the plain colour strips and the flat triangles do not have) */
   if(have_v[0][s]&&vs5[0][s][3]){bad++;fprintf(logf,"default, %s: indexed triangles though off\n",scene_name[s]);}
   if(have_v[CUB][s]&&vs5[CUB][s][0]){bad++;fprintf(logf,"noubcolor, %s: byte colours\n",scene_name[s]);}
   if(have_v[CPRE][s]&&vs5[CPRE][s][2]){bad++;fprintf(logf,"noprepack, %s: prepacked packets\n",scene_name[s]);}}
  if(!(g5[0][3]&64))fprintf(logf,"note: the VxD's ACCEL_D3D_PREPACK is off: no prepacked packets anywhere\n");
  if(have_g[CUB]&&(g5[CUB][3]&32)){bad++;fprintf(logf,"noubcolor: switch bit 20 on\n");}
  if(have_g[CPRE]&&(g5[CPRE][3]&64)){bad++;fprintf(logf,"noprepack: switch bit 40 on\n");}
  if(g5[0][3]&128){bad++;fprintf(logf,"default: switch bit 80 (INDEXED) on\n");}}
 fprintf(logf,"expected: default byte colours in q3 arrays (none converted), prepacked packets in q2 world and q3 arrays (and the scenes with fog or specular), no indexed triangles (only in indexed, where the VxD has its vertex area); each 0 with its switch off\n");
 /* v3.1 (.503): the new parts a frame and their checks */
 fprintf(logf,"\nthe .503 parts a frame: vertices written from the recipe (GLWORDS) / triangles through the run's own checks (GLLIST) / draws whose lighting waited (SSEDEFER): groups lit, left unlit:\n%-16s","");
 fprintf(logf,"%24s",cfg[0].name);for(ci=CGLL;ci<=CDEFR;ci++)if(use_cfg(all,ci))fprintf(logf,"%24s",cfg[ci].name);fprintf(logf,"\n");
 for(s=0;s<NSCENE;s++){fprintf(logf,"%-16s",scene_name[s]);for(ci=0;ci<=CDEFR;ci++){char c[80];if(ci&&ci<CGLL)continue;if(!use_cfg(all,ci))continue;if(!have_x[ci][s])strcpy(c,"-");
   else sprintf(c,"%.0f/%.0f/%.0f: %.0f, %.0f",(double)xs5[ci][s][0]/frames,(double)xs5[ci][s][1]/frames,(double)xs5[ci][s][2]/frames,(double)xs5[ci][s][3]/frames,(double)xs5[ci][s][4]/frames);fprintf(logf,"%24s",c);}fprintf(logf,"\n");}
 if(have_x[0][20]&&have_g[0]){unsigned long sw=g5[0][3];
  /* the default: run triangles in q3 arrays; recipe vertices in q2 world and q3 arrays (the prepacked scenes: VxD bit 64); deferred draws with groups
   * left unlit in smooth, culled (when the pass draws it); each 0 with its switch off; the switch bits */
  if((sw&256)&&!xs5[0][20][1]){bad++;fprintf(logf,"default, q3 arrays: no triangle through the run's checks\n");}
  if((sw&512)&&(sw&64)){if(!xs5[0][19][0]){bad++;fprintf(logf,"default, q2 world: no vertex from the recipe\n");}if(!xs5[0][20][0]){bad++;fprintf(logf,"default, q3 arrays: no vertex from the recipe\n");}}
  if((sw&2048)&&have[0][21]&&fs[0][21][15]&&(!xs5[0][21][2]||!xs5[0][21][4])){bad++;fprintf(logf,"default, smooth, culled: deferred draws %lu, groups left unlit %lu (want both)\n",xs5[0][21][2],xs5[0][21][4]);}
  for(s=0;s<NSCENE;s++){
   if(have_x[CGLL][s]&&xs5[CGLL][s][1]){bad++;fprintf(logf,"nogllist, %s: triangles through the run's checks\n",scene_name[s]);}
   if(have_x[CGLW][s]&&xs5[CGLW][s][0]){bad++;fprintf(logf,"noglwords, %s: vertices from the recipe\n",scene_name[s]);}
   if(have_x[CDEFR][s]&&xs5[CDEFR][s][2]){bad++;fprintf(logf,"nossedefer, %s: deferred draws\n",scene_name[s]);}}
  {static const unsigned long bit[4]={256,512,1024,2048};static const int off[4]={CGLL,CGLW,CSSE2,CDEFR};int p;
   for(p=0;p<4;p++){if(p!=2&&!(sw&bit[p])){bad++;fprintf(logf,"default: switch bit %lX off\n",bit[p]);}
    if(have_g[off[p]]&&(g5[off[p]][3]&bit[p])){bad++;fprintf(logf,"%s: switch bit %lX on\n",cfg[off[p]].name,bit[p]);}}
   if(!(sw&1024))fprintf(logf,"note: the processor has no SSE2 (or the pass is off): the colour words a lane at a time\n");}}
 fprintf(logf,"expected: default triangles through the run's checks in q3 arrays, vertices from the recipe in q2 world and q3 arrays, deferred draws with groups left unlit in smooth, culled; each 0 with its switch off\n");
 /* v3.2 (.505): the fixed layouts and the one-call polygons a frame */
 fprintf(logf,"\nthe .505 parts a frame: vertices by the fixed layouts (GLWORDS 2) / polygons, fans and quads in one call (GLFAN):\n%-16s","");
 fprintf(logf,"%14s",cfg[0].name);for(ci=CGW1;ci<=CRNG;ci++)if(use_cfg(all,ci))fprintf(logf,"%14s",cfg[ci].name);fprintf(logf,"\n");
 for(s=0;s<NSCENE;s++){fprintf(logf,"%-16s",scene_name[s]);for(ci=0;ci<=CRNG;ci++){char c[40];if(ci&&ci<CGW1)continue;if(!use_cfg(all,ci))continue;if(!have_y[ci][s])strcpy(c,"-");
   else sprintf(c,"%.0f/%.0f",(double)ys5[ci][s][0]/frames,(double)ys5[ci][s][1]/frames);fprintf(logf,"%14s",c);}fprintf(logf,"\n");}
 if(have_y[0][19]&&have_g[0]){unsigned long sw=g5[0][3];
  if((sw&4096)&&(sw&64)&&(!ys5[0][19][0]||!ys5[0][20][0])){bad++;fprintf(logf,"default: no vertex by the fixed layouts in q2 world (%lu) or q3 arrays (%lu)\n",ys5[0][19][0],ys5[0][20][0]);}
  if((sw&8192)&&!ys5[0][19][1]){bad++;fprintf(logf,"default, q2 world: no polygon in one call\n");}
  for(s=0;s<NSCENE;s++){if(have_y[CGW1][s]&&ys5[CGW1][s][0]){bad++;fprintf(logf,"glwords1, %s: vertices by the fixed layouts\n",scene_name[s]);}
   if(have_y[CFAN][s]&&ys5[CFAN][s][1]){bad++;fprintf(logf,"noglfan, %s: polygons in one call\n",scene_name[s]);}}
  {static const unsigned long bit[3]={4096,8192,16384};static const int off[3]={CGW1,CFAN,CRNG};int p;
   for(p=0;p<3;p++){if(!(sw&bit[p])){bad++;fprintf(logf,"default: switch bit %lX off\n",bit[p]);}if(have_g[off[p]]&&(g5[off[p]][3]&bit[p])){bad++;fprintf(logf,"%s: switch bit %lX on\n",cfg[off[p]].name,bit[p]);}}}}
 fprintf(logf,"expected: default vertices by the fixed layouts in q2 world and q3 arrays (and other textured or lit, fogged scenes), polygons in one call in q2 world; each 0 with its switch off\n");
 /* v3.3 (.506): where the ICD's time goes on the board (ACCEL_OPENGL_TSCPROF, on in every child): the CPU's thousand cycles a frame in
  * the ICD's parts, the default's; and the .506 parts' counts */
 fprintf(logf,"\nthe ICD's time a frame, thousand cycles (the default): pipeline runs (a draw's ICD side) / triangle-list runs / strips, polygons, fans / the SSE pass's transform and lighting / Mesa's vertex emit / packets to the VxD / swaps:\n");
 for(s=0;s<NSCENE;s++){double k=256.0/1000.0/frames;if(!have_z[0][s])continue;
  fprintf(logf,"%-16s %8.0f %8.0f %8.0f %8.0f %8.0f %8.0f %8.0f\n",scene_name[s],zs5[0][s][0]*k,zs5[0][s][1]*k,zs5[0][s][2]*k,zs5[0][s][3]*k,zs5[0][s][4]*k,zs5[0][s][5]*k,zs5[0][s][6]*k);}
 fprintf(logf,"\nthe .506 parts a frame: triangles from the last draw's plan (PLAN) / index scans by SSE2 (MINMAX2):\n%-16s","");
 fprintf(logf,"%16s",cfg[0].name);for(ci=1;ci<CTPL;ci++)if(use_cfg(all,ci))if(ci==CPLN||ci==CMM2)fprintf(logf,"%16s",cfg[ci].name);fprintf(logf,"\n");
 for(s=0;s<NSCENE;s++){fprintf(logf,"%-16s",scene_name[s]);for(ci=0;ci<CTPL;ci++){char c[48];if(ci&&ci!=CPLN&&ci!=CMM2)continue;if(!use_cfg(all,ci))continue;if(!have_z[ci][s])strcpy(c,"-");
   else sprintf(c,"%.0f/%.0f",(double)zs5[ci][s][7]/frames,(double)zs5[ci][s][8]/frames);fprintf(logf,"%16s",c);}fprintf(logf,"\n");}
 if(have_z[0][20]&&have_g[0]){unsigned long sw=g5[0][3];
  if((sw&32768)&&!zs5[0][20][7]){bad++;fprintf(logf,"default, q3 arrays: no triangle from the plan\n");}
  if((sw&65536)&&!zs5[0][20][8]){bad++;fprintf(logf,"default, q3 arrays: no index scan by SSE2\n");}
  for(s=0;s<NSCENE;s++){if(have_z[CPLN][s]&&zs5[CPLN][s][7]){bad++;fprintf(logf,"noplan, %s: triangles from the plan\n",scene_name[s]);}
   if(have_z[CMM2][s]&&zs5[CMM2][s][8]){bad++;fprintf(logf,"nominmax2, %s: index scans by SSE2\n",scene_name[s]);}}
  {static const unsigned long bit[2]={32768,65536};static const int off[2]={CPLN,CMM2};int p;
   for(p=0;p<2;p++){if(!(sw&bit[p])&&!(p==1)){bad++;fprintf(logf,"default: switch bit %lX off\n",bit[p]);}if(have_g[off[p]]&&(g5[off[p]][3]&bit[p])){bad++;fprintf(logf,"%s: switch bit %lX on\n",cfg[off[p]].name,bit[p]);}}
   if(!(sw&65536))fprintf(logf,"note: the processor has no SSE2: the index scan in C\n");}}
 fprintf(logf,"expected: default triangles from the plan and SSE2 index scans in q3 arrays; each 0 with its switch off\n");
 fprintf(logf,"\nthe default run first and again last (frames a second, each the median of three runs; a scene more than 10%% apart is marked):\n");
 for(s=0;s<NSCENE;s++)if(have[0][s]&&have[CDEF2][s]){double a=fps[0][s],b=fps[CDEF2][s],r=a>b?a/b:b/a;fprintf(logf,"%-16s %6.1f %6.1f%s\n",scene_name[s],a,b,r>1.10?"  <- apart":"");}
 fprintf(logf,"\n%s\n",bad?"FAILED: see the lines above":"PASSED");
 for(ci=0;ci<NCFG;ci++)for(s=0;s<NSCENE;s++){char fn[32];sprintf(fn,"VXF%d%02d.RAW",ci,s);remove(fn);}
 fprintf(logf,"\nsettings:\n");for(ci=0;ci<NCFG;ci++)if(use_cfg(all,ci))fprintf(logf,"  %-10s %s\n",cfg[ci].name,cfg[ci].env);
 {FILE *l=fopen("VXFLAT.LOG","r");fprintf(logf,"\nchildren:\n");if(l){while(fgets(line,sizeof(line),l))if(line[0]!='R'||line[1]!=' ')fputs(line,logf);fclose(l);}}
 fclose(logf);
 MessageBoxA(0,"VXFLAT done: results in VXFLAT.TXT","VXFLAT",MB_OK);
 return 0;
}
