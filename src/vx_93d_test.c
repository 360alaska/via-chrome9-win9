/* VX93D v1.6 (driver .604, the VxD unchanged): forms Y, Z and 0, the driver's own draw state. VX900 board, v1.5: all right: the list
 * word 04/20 = 0, the depth test both ways, the strip word 386300. Until now every form drew with 01/01 = 40 (or CE1140): no pixel
 * program. The driver's Direct3D path draws with 01/01 = CE1540, which differs in bit 10, the pixel program's enable (form N hung
 * with it: no program was loaded). These forms send the state set-up of vx_native3d_hw.c's setup() word for word, for the test's
 * target (no blend, fog or stencil): Type 01 00-3F, 50-53, 58-6B, 70-78, 80-8A, the untextured pixel program (01/90-9B, selector
 * 20030000, 01C00530 301C0053 00F04005 000D0820), Type 04 with the VX900's vertex words in place of the VX855's, the FE02 clears, then
 * 01/01 = CE1540 and the driver's primitive word 04/23 = CE02.
 *   Y  an orange triangle (five words a vertex: X Y Z W, colour; the offset by the program)
 *   Z  red, green and blue vertices (the driver's CE02 is Gouraud)
 *   0  with the driver's depth words (01/10-15, D32, LESS, 01/01 bits 3:2): orange at Z 0.25, then blue at Z 0.75: orange must stay
 * VX93D v1.5 (driver .604, the VxD unchanged): forms 6 to 9. VX900 board, v1.4: forms 4 and 5 drew: the vertex program adds the 786432.0
 * from constant 0 (selector 32030000), loaded before or after the program, and Direct3D's vertex goes to the chip unchanged. All of
 * the below are form 4 (eight words a vertex, the offset by the program) with one thing changed:
 *   6  04/20 = 0, the list word this driver writes on the VX855 (CE's 1C5C00 until now)
 *   7  depth: an orange triangle at Z = 0.25, then a blue one over it at Z = 0.75, both with the depth test on (01/01 = 4C, LESS, the
 *      test's 32-bit depth buffer cleared to the far value): orange must stay
 *   8  depth: orange at Z = 0.75, then blue at Z = 0.25: blue must win
 *   9  a strip of four vertices, 04/20 = 386300 (this driver's strip word on the VX855), 04/24 = 4: a square, 2304 pixels
 * VX93D v1.4 (driver .604, the VxD unchanged): forms 4 and 5, the XY offset added by the vertex program. VX900 board, v1.3: forms 1, 2
 * and 3 all drew (Gouraud shading; seven and eight words a vertex, Direct3D's own vertex among them). The instruction words are now
 * read from VIA's own code generator (CE's d3dm_via.dll, debug build, run in an emulator; the supplement, 9.10):
 *   word 0 to 2  the three sources: bits 22:19 class (1 vertex input, 2 temporary, 4 special, 7 viewport, 8 + n/32 constants),
 *                bits 18:14 number, bits 13:11, 10:8, 7:5, 4:2 which part (0 x, 1 y, 2 z, 3 w) goes to x, y, z, w, bit 0 negate
 *   word 3       bits 21:17 operation (4 move, 2 multiply-add, 3 dot product, 20 reciprocal ...), bit 14 clamp to 0..1 (VIA sets
 *                it for the two colours), bits 9:8 class of the result (2 an output, 1 a temporary), bits 7:4 its number, bits 3:0
 *                which parts are written
 *   outputs      0 the position before the screen transform, 1 the position on the screen, 2 diffuse, 3 specular, 6 fog,
 *                7 point size, 8 + n texture set n
 *   constants    selector 32030000 + 4 x register at 43C, then four floats a register
 * VIA's "add r, a, c" is one multiply-add: a, the special 1.0 (00206490), c. So the offset is one instruction:
 *   0008014C 00206490 0040014C 0004021F   output 1 = input 0 x 1 + constant 0, with constant 0 = (786432, 786432, 0, 0)
 *   4  form 3 with the vertices as Direct3D gives them (no 786432.0 added), the offset by the instruction above; the constant is
 *      loaded before the program; the colour instructions with VIA's clamp bit (0008422F, 0008423F)
 *   5  as 4, the constant loaded after the program
 * VX93D v1.3 (driver .604, the VxD unchanged): forms 1 to 3. VX900 board, v1.2: R DREW IN ORANGE: vertex program output 2 is the diffuse
 * colour (outputs 3 to 7 and 0 left the triangle white and did no harm). H drew again after N's hang and the reset. Of CE's table
 * only 04/50-52 are needed (Q drew; O, the clip-to-screen words alone, did not; P, the table less them, did).
 *   1  R with three colours (red, green, blue) and Gouraud shading (04/23 = 004E02)
 *   2  seven words a vertex: X Y Z W, colour, S T (04/00 = 00A80207, 04/03 = 03000242, 04/12 = 7, masks F4C000, 04/60 = 60800002,
 *      04/62 = 62204002 as CE's three-input setting; three instructions: position, colour to output 2, S T to output 8)
 *   3  Direct3D's own vertex, eight words: X Y Z W, colour, specular, S T (04/00 = 00A80308, 04/03 = 030002D1, 04/04 = 04000342,
 *      04/12 = F, masks F6C000, 04/60 = 60800003, 04/62 = 62304003 by CE's rule carried one step on; four instructions, the
 *      specular colour to output 3)
 * The first form of a run is now repeated with the reset when the VxD answers that an earlier run left the fault latched.
 * VX93D v1.2 (driver .604, the VxD unchanged): forms O to X, and H N H. VX900 board, v1.1, from a restart: B C D E G again drew nothing, so
 * the reset after a hang was not the cause; H (B after CE's Type 04 table) DREW the triangle, white, 1176 pixels. I and J (no 786432.0
 * added in software, with and without 01/01 bit 20) drew nothing and K drew as H: the table's own offset is not applied to these
 * vertices and the software's is needed. L (Z = 0.5) and M (01/01 = CE1140) drew; N (CE1540) left the engine busy (status 00000701).
 *   H N H  whether the engine draws again after the reset that follows a hang (N hangs)
 *   O  B with only the table's clip-to-screen words (04/40-48)        P  B with the table less those words
 *   Q  B with only 04/50-52
 *   R to X  a colour: the vertex is X Y Z W and a D3DCOLOR (five words; 04/00 = 00A80105, 04/02 = 020001D1, the mask F40000 = X Y Z W
 *      and the diffuse colour), and the program's second instruction reads IR1 whole (0008414C) and writes output 2, 3, 4, 5, 6, 7, 0
 *      (000802nF). CE's two programs show the position as 0008021F and the two texture sets as 00080283, 00080293: an output number in
 *      bits 7:4 and a write mask in bits 3:0. Which number is the diffuse colour is not in the manual. Orange (E04020) is the answer.
 * VX93D v1.1 (driver .604, the VxD unchanged): forms H to N. VX900 board, v1.0: the VX855's words (A) and OpenChrome's direct form (F) leave
 * the engine busy (status 00080021: it waits for more vertex words); VIA's CE setting (B, C, D, E, G) is taken whole, the engine goes
 * idle, and nothing is drawn. The vertex words are therefore counted right, and what is missing lies after them. v1.0 left out the
 * rest of CE's Type 04 table: the clip-to-screen scale and offset (04/40-48: scale 1.0, offset 786432.0), 04/50-52 and the zeroed
 * 04/04-0D, 21, 22, 26, 29, 2C-2E; with the BIOS's zeros there the scale is 0. H to K send the table, with and without the 786432.0
 * added in software and with the clip-to-screen bit of 01/01 (bit 20) off and on; L is H at Z = 0.5; M and N are H with the enable
 * words of the driver's own draws (01/01 = CE1140) and of CE's (CE1540). With no letters H to N are run.
 * VX93D v1.0 (driver .604): the first triangle on the VX900. The VxD's 3D test endpoint sets up a 64 x 64 32-bit target in its scratch
 * area (video memory at 2 MB, saved and put back), runs its fixed steps (command path, a 2D fill check, the FE block, texture off, the
 * render state) and then sends the words this tool gives it; the target is read back and printed here. Nothing shows on the screen;
 * the desktop must be 800 x 600 x 16 or smaller in bytes (the scratch area must lie beyond the visible screen). Each form asks for the
 * same triangle, (8,8) (56,8) (8,56), about 1150 pixels:
 *   A  the VX855's vertex words (five a vertex, as the driver's 3D code sends them): the control; it must draw on a VX800 / VX855
 *   B  VIA's Windows CE setting for the VX900: input descriptors, the 8-word vertex program, X and Y with 786432.0 added, six words a vertex
 *   C  B without the 786432.0
 *   D  B with the plain primitive words of A (04/20 = 0, 04/23 solid) instead of CE's
 *   E  B with 04/61, 62, 60 in the order VIA's XP driver writes them
 *   F  OpenChrome's direct form (control words EC and EE, X Y W a vertex), which it uses on all three chips
 *   G  B followed by the wait word CE sends after a VX900 draw (6C = 00800000)
 * A form that leaves the engine busy is reported and the engine reset before the next. Log: VX93D.LOG next to the program.
 *   H  B with CE's whole Type 04 table (clip-to-screen scale 1, offset 786432, and the rest)
 *   I  H without the 786432.0 added in software
 *   J  I with 01/01 bit 20 (clip-to-screen) on
 *   K  H with 01/01 bit 20 on
 *   L  H at Z = 0.5
 *   M  H with 01/01 = CE1140 (the driver's draw enable word)
 *   N  H with 01/01 = CE1540 (CE's)
 * VX93D <letters> runs only those forms (VX93D AB). */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "vx85598.h"
#include "vx855_3d.h"
static FILE *lf;
static void say(const char *f,...) {static char b[2000];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);if(lf){fputs(b,lf);fflush(lf);}}
static HANDLE dev;
static vx3d_request rq;static vx3d_report rp;
static DWORD nw;
static void P(DWORD port,DWORD v) {if(nw+2<=VX3D_LAB_DATA){rq.lab_data[nw++]=port;rq.lab_data[nw++]=v;}}
static void T(DWORD t) {P(0x43C,t<<16);}
static void R(DWORD a,DWORD v) {P(0x440,(a<<24)|(v&0xFFFFFFUL));}
static void D(DWORD w) {P(0x440,w);}
static void IDLE(void) {P(0xFFFFFFFFUL,0);}
static DWORD fb(float f) {DWORD b;memcpy(&b,&f,4);return b;}
static const float vx[3]={8.0f,56.0f,8.0f},vy[3]={8.0f,8.0f,56.0f};
static void ce_table(int which) { /* 1 all, 2 only 04/40-48, 3 all but those, 4 only 04/50-52 */
 int k; /* CE 6.0 DDI_VIA.dll, the Type 04 table both its VX855 and VX900 paths copy (VA 100087A0) */
 static const DWORD tab[32]={0x07000000UL,0x06000000UL,0x05000000UL,0x04000000UL,0x0B000000UL,0x0A000000UL,0x09000000UL,0x08000000UL,
  0x22000000UL,0x21000000UL,0x0D000000UL,0x0C000000UL,0x26000000UL,0x2D000000UL,0x2C000000UL,0x29000000UL,
  0x2F582000UL,0x2B3F8000UL,0x2A3F8000UL,0x2E000000UL,0x43800000UL,0x4200493FUL,0x41400000UL,0x40800000UL,
  0x47000000UL,0x46800000UL,0x4500493FUL,0x44400000UL,0x52147FFFUL,0x51000000UL,0x50000000UL,0x4800003FUL};
 for(k=0;k<32;k++){DWORD a=tab[k]>>24;int c2s=a>=0x40&&a<=0x48,v=a>=0x50&&a<=0x52;if(which==1||(which==2&&c2s)||(which==3&&!c2s)||(which==4&&v))D(tab[k]);}
}
static void ce_form(int bias,int plain,int xporder,int wait6c,int table,DWORD z,DWORD enable) {
 static const DWORD prog[8]={0x0008014CUL,0,0,0x0008021FUL,0x00084124UL,0,0,0x00080283UL};int k;float b=bias?786432.0f:0.0f;
 T(4);if(table)ce_table(table);
 R(0x00,0xA80106UL);R(0x01,0x0000C2UL);R(0x02,0x000142UL);R(0x03,0);R(0x12,0x000003UL);
 R(0x20,plain?0:0x1C5C00UL);R(0x25,0xF0C000UL);R(0x28,0xF0C000UL);R(0x2A,0x3F8000UL);R(0x2B,0x3F8000UL);R(0x2F,0x582000UL);
 if(xporder){R(0x61,0x800000UL);R(0x62,0x104001UL);R(0x60,0x800001UL);}else{R(0x60,0x800001UL);R(0x61,0x800000UL);R(0x62,0x104001UL);}
 P(0x43C,0x31030000UL);for(k=0;k<8;k++)D(prog[k]);IDLE();
 T(1);R(1,enable);T(4);R(0x23,plain?2:0x002E02UL);R(0x24,3);T(0);
 for(k=0;k<3;k++){D(fb(vx[k]+b));D(fb(vy[k]+b));D(z);D(0x3F800000UL);D(0);D(0);}
 if(wait6c){IDLE();P(0x6C,0x00800000UL);}
}
static void colour_form(DWORD out) { /* H with a D3DCOLOR in IR1 written to vertex program output 'out' */
 int k;
 T(4);ce_table(1);
 R(0x00,0xA80105UL);R(0x01,0x0000C2UL);R(0x02,0x0001D1UL);R(0x03,0);R(0x12,0x000003UL);
 R(0x20,0x1C5C00UL);R(0x25,0xF40000UL);R(0x28,0xF40000UL);R(0x2A,0x3F8000UL);R(0x2B,0x3F8000UL);R(0x2F,0x582000UL);
 R(0x60,0x800001UL);R(0x61,0x800000UL);R(0x62,0x104001UL);
 P(0x43C,0x31030000UL);D(0x0008014CUL);D(0);D(0);D(0x0008021FUL);D(0x0008414CUL);D(0);D(0);D(0x0008020FUL|(out<<4));IDLE();
 T(1);R(1,0x40);T(4);R(0x23,0x002E02UL);R(0x24,3);T(0);
 for(k=0;k<3;k++){D(fb(vx[k]+786432.0f));D(fb(vy[k]+786432.0f));D(0);D(0x3F800000UL);D(0xFFE04020UL);}
}
static void konst0(void) {P(0x43C,0x32030000UL);D(fb(786432.0f));D(fb(786432.0f));D(0);D(0);}
static void tl_form(int kind,int vsoff) { /* 1 Gouraud, 2 seven words (colour, S T), 3 eight words (colour, specular, S T); vsoff 1, 2: the offset by the program */
 static const DWORD rgb[3]={0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL};int k,n=kind==1?1:kind;float b=vsoff?0.0f:786432.0f;
 T(4);ce_table(1);
 R(0x00,0xA80000UL|((DWORD)n<<8)|(kind==1?5:kind==2?7:8));R(0x01,0x0000C2UL);R(0x02,0x0001D1UL);
 if(kind==1)R(0x03,0);else if(kind==2)R(0x03,0x000242UL);else{R(0x03,0x0002D1UL);R(0x04,0x000342UL);}
 R(0x12,kind==1?3:kind==2?7:0xF);
 R(0x20,0x1C5C00UL);R(0x25,kind==1?0xF40000UL:kind==2?0xF4C000UL:0xF6C000UL);R(0x28,kind==1?0xF40000UL:kind==2?0xF4C000UL:0xF6C000UL);
 R(0x2A,0x3F8000UL);R(0x2B,0x3F8000UL);R(0x2F,0x582000UL);
 R(0x60,0x800000UL|n);R(0x61,0x800000UL);R(0x62,0x004000UL|((DWORD)n<<20)|n);
 if(vsoff==1)konst0();
 P(0x43C,0x31030000UL);
 if(vsoff){D(0x0008014CUL);D(0x00206490UL);D(0x0040014CUL);D(0x0004021FUL);D(0x0008414CUL);D(0);D(0);D(0x0008422FUL);}
 else{D(0x0008014CUL);D(0);D(0);D(0x0008021FUL);D(0x0008414CUL);D(0);D(0);D(0x0008022FUL);}
 if(kind==2){D(0x00088124UL);D(0);D(0);D(0x00080283UL);}
 if(kind==3){D(0x0008814CUL);D(0);D(0);D(vsoff?0x0008423FUL:0x0008023FUL);D(0x0008C124UL);D(0);D(0);D(0x00080283UL);}
 if(vsoff==2)konst0();
 IDLE();
 T(1);R(1,0x40);T(4);R(0x23,kind==1?0x004E02UL:0x002E02UL);R(0x24,3);T(0);
 for(k=0;k<3;k++){D(fb(vx[k]+b));D(fb(vy[k]+b));D(0);D(0x3F800000UL);D(kind==1?rgb[k]:0xFFE04020UL);
  if(kind==3)D(0xFF000000UL);if(kind!=1){D(0);D(0);}}
}
static void v8(float x,float y,DWORD z,DWORD c) {D(fb(x));D(fb(y));D(z);D(0x3F800000UL);D(c);D(0xFF000000UL);D(0);D(0);}
static void form4x(int what) { /* 6 list word 0, 7 and 8 depth, 9 strip */
 DWORD en=(what==7||what==8)?0x4CUL:0x40UL;int k;
 T(4);ce_table(1);
 R(0x00,0xA80308UL);R(0x01,0x0000C2UL);R(0x02,0x0001D1UL);R(0x03,0x0002D1UL);R(0x04,0x000342UL);R(0x12,0xF);
 R(0x20,what==6?0:what==9?0x386300UL:0x1C5C00UL);R(0x25,0xF6C000UL);R(0x28,0xF6C000UL);R(0x2A,0x3F8000UL);R(0x2B,0x3F8000UL);R(0x2F,0x582000UL);
 R(0x60,0x800003UL);R(0x61,0x800000UL);R(0x62,0x304003UL);
 konst0();P(0x43C,0x31030000UL);
 D(0x0008014CUL);D(0x00206490UL);D(0x0040014CUL);D(0x0004021FUL);D(0x0008414CUL);D(0);D(0);D(0x0008422FUL);
 D(0x0008814CUL);D(0);D(0);D(0x0008423FUL);D(0x0008C124UL);D(0);D(0);D(0x00080283UL);IDLE();
 T(1);R(1,en);T(4);R(0x23,0x002E02UL);R(0x24,what==9?4:3);T(0);
 if(what==9){v8(8,8,0,0xFFE04020UL);v8(56,8,0,0xFFE04020UL);v8(8,56,0,0xFFE04020UL);v8(56,56,0,0xFFE04020UL);return;}
 {DWORD z1=what==7?0x3E800000UL:what==8?0x3F400000UL:0;for(k=0;k<3;k++)v8(vx[k],vy[k],z1,0xFFE04020UL);}
 if(what==7||what==8){DWORD z2=what==7?0x3F400000UL:0x3E800000UL;IDLE();T(4);R(0x23,0x002E02UL);R(0x24,3);T(0);for(k=0;k<3;k++)v8(vx[k],vy[k],z2,0xFF2040E0UL);}
}
static void v5(float x,float y,DWORD z,DWORD c) {D(fb(x));D(fb(y));D(z);D(0x3F800000UL);D(c);}
static void driver_form(int what) { /* 'Y' flat orange, 'Z' three colours, '0' depth */
 static const DWORD rgb[3]={0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL};
 DWORD zb=what=='0'?12UL:0,k,m=0x010C0FUL;
 T(1);R(0x00,0);R(0x01,0xCE1140UL|zb);for(k=3;k<=15;k++)R(k,0);R(0x02,2);R(0x02,0);
 R(0x10,zb?0x2060UL:0);R(0x11,zb?0x10000UL:0);R(0x12,0x3C0000UL);R(0x13,zb?0x10000UL:0x70000UL);R(0x14,0xFFFFFFUL);R(0x15,0xFF);
 R(0x16,0);R(0x17,0);R(0x18,0);R(0x19,0x800000UL);R(0x1A,0x800000UL);R(0x1B,0x803FBFUL);R(0x1E,0x800000UL);R(0x1F,0);R(0x20,0x3F00);R(0x21,0);R(0x22,0);
 for(k=0x23;k<=0x3F;k++)if(k<0x28||k>0x2A)R(k,0);
 R(0x28,0);R(0x29,0);R(0x2A,0);R(0x34,0);R(0x35,0);R(0x36,0);R(0x37,0);
 R(0x50,0x2010);R(0x51,0);R(0x52,(0x99UL<<16)|8);R(0x53,m);R(0x5B,m);R(0x63,m);R(0x6B,m);
 R(0x80,64);R(0x81,64);R(0x82,64);R(0x83,64);R(0x84,0);R(0x85,0);R(0x88,0);R(0x89,64);R(0x8A,0);
 for(k=0x58;k<=0x6B;k++)if((k&7)<4)R(k,(k&7)==3?m:0);
 for(k=0x70;k<=0x78;k++)R(k,0);
 for(k=0x90;k<=0x9A;k++)R(k,k==0x90?0x58000UL:k==0x92?1:0);
 R(0x9B,0xFFFFFEUL);P(0x43C,0x20030000UL);D(0x01C00530UL);D(0x301C0053UL);D(0x00F04005UL);D(0x000D0820UL);
 T(4);R(0x00,0xA80105UL);R(0x01,0x0000C2UL);R(0x02,0x0001D1UL);for(k=3;k<=15;k++)R(k,0);
 R(0x10,0);R(0x12,3);R(0x20,0);R(0x21,0);R(0x22,0);R(0x25,0xF40000UL);R(0x26,0);R(0x27,0);R(0x28,0xF40000UL);R(0x29,0);R(0x2F,0x582000UL);R(0x2C,0);R(0x2D,0);R(0x2E,0);
 R(0x40,0x800000UL);R(0x41,0x400000UL);R(0x42,0x493F);R(0x43,0x800000UL);R(0x44,0x400000UL);R(0x45,0x493F);R(0x46,0x800000UL);R(0x47,0);R(0x48,0x3F);
 R(0x50,0);R(0x51,0);R(0x52,0x147FFFUL);
 R(0x60,0x800001UL);R(0x61,0x800000UL);R(0x62,0x104001UL);
 konst0();P(0x43C,0x31030000UL);D(0x0008014CUL);D(0x00206490UL);D(0x0040014CUL);D(0x0004021FUL);D(0x0008414CUL);D(0);D(0);D(0x0008422FUL);
 P(0x43C,0xFE020000UL);for(k=1;k<=14;k++)if(k!=6&&k!=7)R(k,0);R(0,1);R(0,0);
 IDLE();T(1);R(0x01,0xCE1540UL|zb);
 T(4);R(0x23,0xCE02UL);R(0x24,3);T(0);
 for(k=0;k<3;k++)v5(vx[k],vy[k],what=='0'?0x3E800000UL:0,what=='Z'?rgb[k]:0xFFE04020UL);
 if(what=='0'){IDLE();T(4);R(0x23,0xCE02UL);R(0x24,3);T(0);for(k=0;k<3;k++)v5(vx[k],vy[k],0x3F400000UL,0xFF2040E0UL);}
}
/* v1.8 (driver .623): VX93D INDEX. The VX900 draws every indexed packet with its vertices written out in full, because its way of
 * fetching vertices by index was not known. Its manual: 04/20 bit 0 index mode, bit 1 32-bit indices; the buffer's base and pitch in
 * the stream table (parameter type 03, sub-type 30h: base bits 31:10 in 1024-byte units, pitch bits 8:0 in words); each descriptor
 * (04/01..) names its buffer (bits 23:20) and its offset in the vertex (bits 19:12, in words); 04/00 bits 16:15 where the buffer is;
 * 04/2C-2E the largest and smallest index. XP's VX900 driver writes the table as a data block under selector 30030000 + 8 an entry
 * (s3ginvA.dll C7518). Here: the driver's flat orange triangle (form Y), its three vertices written into a buffer in the test arena
 * (VxD port FFFFFFFE) and drawn by index, in the ways the words can be read. Form 0 is the control, the vertices in the command. */
#define IX_VB 0x0020E000UL
static const char *res(DWORD r);
static void VBW(DWORD w) {P(0xFFFFFFFEUL,w);}
static const char *index_name(int v) {
 static const char *n[9]={"0 control: the vertices in the command (form Y)","1 the table's entries 0 and 1 as two words under selector 30030000",
  "2 entry 0 under 30030000, entry 1 under 30030008","3 entry 0 under 30030000, entry 1 under 30030001","4 entry 0 under 30030000, entry 1 under 30030004",
  "5 as 1, buffer 1 named in the colour's descriptor","6 as 2, buffer 1 named in the colour's descriptor","7 as 1, all sixteen entries written","8 as 1, 32-bit indices"};
 return n[v];
}
/* v1.9 (.624). Board, v1.8 on .623: by index the triangle's shape was right (1176 pixels, the same bounds) with one table entry, two
 * 16-bit indices a word or 32-bit indices; its colour was not: 03EF03EF, the desktop's pixels at the start of video memory. So each
 * vertex input has its own table entry (the manual's "base address for data n"): entry 1, never written, was 0. These forms write
 * entry 1 too, in the ways its place can be read. Each form's buffer is at its own place (the arena + E000h + form x 400h), and the
 * arena is filled again before every form, so an entry left by an earlier form points at filler, not at these vertices. */
static void index_form(int v) {
 DWORD k,m=0x010C0FUL,vb=IX_VB+(DWORD)(v&7)*0x400UL,entry=vb|5UL,id=(v==5||v==6)?0x100000UL:0;
 if(v==0){driver_form('Y');return;}
 P(0xFFFFFFFDUL,(vb-IX_VB)/4);
 for(k=0;k<3;k++){VBW(fb(vx[k]));VBW(fb(vy[k]));VBW(0);VBW(0x3F800000UL);VBW(0xFFE04020UL);}
 T(1);R(0x00,0);R(0x01,0xCE1140UL);for(k=3;k<=15;k++)R(k,0);R(0x02,2);R(0x02,0);
 R(0x10,0);R(0x11,0);R(0x12,0x3C0000UL);R(0x13,0x70000UL);R(0x14,0xFFFFFFUL);R(0x15,0xFF);
 R(0x16,0);R(0x17,0);R(0x18,0);R(0x19,0x800000UL);R(0x1A,0x800000UL);R(0x1B,0x803FBFUL);R(0x1E,0x800000UL);R(0x1F,0);R(0x20,0x3F00);R(0x21,0);R(0x22,0);
 for(k=0x23;k<=0x3F;k++)if(k<0x28||k>0x2A)R(k,0);
 R(0x28,0);R(0x29,0);R(0x2A,0);R(0x34,0);R(0x35,0);R(0x36,0);R(0x37,0);
 R(0x50,0x2010);R(0x51,0);R(0x52,(0x99UL<<16)|8);R(0x53,m);R(0x5B,m);R(0x63,m);R(0x6B,m);
 R(0x80,64);R(0x81,64);R(0x82,64);R(0x83,64);R(0x84,0);R(0x85,0);R(0x88,0);R(0x89,64);R(0x8A,0);
 for(k=0x58;k<=0x6B;k++)if((k&7)<4)R(k,(k&7)==3?m:0);
 for(k=0x70;k<=0x78;k++)R(k,0);
 for(k=0x90;k<=0x9A;k++)R(k,k==0x90?0x58000UL:k==0x92?1:0);
 R(0x9B,0xFFFFFEUL);P(0x43C,0x20030000UL);D(0x01C00530UL);D(0x301C0053UL);D(0x00F04005UL);D(0x000D0820UL);
 T(4);R(0x00,0xA80105UL);R(0x01,0x0000C2UL);R(0x02,id|0x004000UL|0x0001D1UL);for(k=3;k<=15;k++)R(k,0);
 R(0x10,0);R(0x12,3);R(0x20,0);R(0x21,0);R(0x22,0);R(0x25,0xF40000UL);R(0x26,0);R(0x27,0);R(0x28,0xF40000UL);R(0x29,0);R(0x2F,0x582000UL);R(0x2C,0);R(0x2D,0);R(0x2E,0);
 R(0x40,0x800000UL);R(0x41,0x400000UL);R(0x42,0x493F);R(0x43,0x800000UL);R(0x44,0x400000UL);R(0x45,0x493F);R(0x46,0x800000UL);R(0x47,0);R(0x48,0x3F);
 R(0x50,0);R(0x51,0);R(0x52,0x147FFFUL);
 R(0x60,0x800001UL);R(0x61,0x800000UL);R(0x62,0x104001UL);
 konst0();P(0x43C,0x31030000UL);D(0x0008014CUL);D(0x00206490UL);D(0x0040014CUL);D(0x0004021FUL);D(0x0008414CUL);D(0);D(0);D(0x0008422FUL);
 P(0x43C,0x30030000UL);D(entry);
 if(v==1||v==5||v==8)D(entry);
 else if(v==7)for(k=1;k<16;k++)D(entry);
 else{P(0x43C,v==3?0x30030001UL:v==4?0x30030004UL:0x30030008UL);D(entry);}
 P(0x43C,0xFE020000UL);for(k=1;k<=14;k++)if(k!=6&&k!=7)R(k,0);R(0,1);R(0,0);
 IDLE();T(1);R(0x01,0xCE1540UL);
 T(4);R(0x2C,2);R(0x2D,0);R(0x2E,0);R(0x20,v==8?3UL:1UL);R(0x23,0xCE02UL);R(0x24,3);T(0);
 if(v==8){D(0);D(1);D(2);}else{D(0x00010000UL);D(2);}
 IDLE();T(4);R(0x20,0);R(0x2C,0);
}
static int index_run(void) {
 DWORD got=0,ok;int v,recover=0,good[9],any=0;
 say("VX93D INDEX (v1.9): the flat orange triangle (8,8) (56,8) (8,56) drawn by index from a vertex buffer in the test arena (video offset %08lX + form x 400h), eight ways of writing the table's second entry; 0 is the control\n",IX_VB);
 for(v=0;v<9;v++){int x,y;DWORD ch=0,orange=0,minx=64,miny=64,maxx=0,maxy=0;good[v]=0;
  memset(&rq,0,sizeof(rq));rq.cb=sizeof(rq);rq.version=VX3D_VERSION;rq.arm=VX3D_ARM;rq.probe=1;rq.lab_cmd=0x33574152UL;rq.reserved=recover?0x100UL:0;
  nw=0;index_form(v);rq.lab_ndata=nw;memset(&rp,0,sizeof(rp));got=0;
  ok=DeviceIoControl(dev,VX_IOCTL_3D_TEST,&rq,sizeof(rq),&rp,sizeof(rp),&got,NULL);
  if(ok&&got==sizeof(rp)&&!recover&&rp.fault_latched&&rp.stage==0&&rp.writes==0){rq.reserved=0x100UL;memset(&rp,0,sizeof(rp));got=0;ok=DeviceIoControl(dev,VX_IOCTL_3D_TEST,&rq,sizeof(rq),&rp,sizeof(rp),&got,NULL);}
  say("\n%s: %lu words\n",index_name(v),rq.lab_ndata/2);
  if(!ok||got!=sizeof(rp)){say("   the VxD refused the call (error %lu, %lu bytes)\n",GetLastError(),got);continue;}
  say("   %s; stage %lu (4 = idle after every word); words sent %lu; status after the words %08lX; fault latched %lu\n",res(rp.result),rp.stage,rp.vertex_vertices,rp.status_solid,rp.fault_latched);
  if(rp.vertex_words!=0xFFFFFFFFUL)say("   a wait failed at word %lu (reason %lu, status %08lX)\n",rp.vertex_words,rp.wait_failure_reason,rp.wait_failure_status);
  recover=rp.fault_latched!=0;
  if(rp.result==2||rp.result==11||rp.stage<2)continue;
  for(y=0;y<64;y++)for(x=0;x<64;x++){DWORD c=rp.image[y*64+x];if(c==VX3D_BACKGROUND)continue;ch++;if((c&0xFFFFFFUL)==0xE04020UL)orange++;
   if((DWORD)x<minx)minx=x;if((DWORD)x>maxx)maxx=x;if((DWORD)y<miny)miny=y;if((DWORD)y>maxy)maxy=y;}
  say("   the target: %lu pixels changed, %lu of them orange",ch,orange);if(ch)say(", within (%lu,%lu)-(%lu,%lu); (16,16) = %08lX, (40,40) = %08lX",minx,miny,maxx,maxy,rp.image[16*64+16],rp.image[40*64+40]);say("\n");
  if(ch&&!(ch>=1000&&ch<=1300&&orange>=1000)){say("   picture, every second pixel (# orange, + other, . untouched):\n");
   for(y=0;y<64;y+=2){char row[40];for(x=0;x<64;x+=2){DWORD c=rp.image[y*64+x];row[x/2]=c==VX3D_BACKGROUND?'.':(c&0xFFFFFFUL)==0xE04020UL?'#':'+';}row[32]=0;say("      %s\n",row);}}
  if(ch>=1000&&ch<=1300&&orange>=1000&&minx>=7&&miny>=7&&maxx<=57&&maxy<=57&&!rp.fault_latched){good[v]=1;say("   -> THE TRIANGLE IS THERE, in its colour\n");if(v)any=1;}
  else if(ch)say("   -> something was drawn, not the triangle asked for\n");else say("   -> nothing was drawn\n");
 }
 say("\n");
 if(!good[0])say("RESULT: the control did not draw: the test itself is at fault, the index forms say nothing\n");
 else if(!any)say("RESULT: no index form drew the triangle\n");
 else{say("RESULT: drawn by index in form(s)");for(v=1;v<9;v++)if(good[v])say(" %d",v);say("\n");}
 return 0;
}
/* v1.7 (driver .613): VX93D HANG. Board, .612, GTA3 on the VX900: fetches hang in the middle of a group's vertex words, the same
 * packet again after the engine's reset. The triangles around the fetcher's position at two of those hangs (vx_93d_hang.h, from
 * VXD3DMON's record) are sent here one at a time and eight at a time, with the driver's state and its 10-word vertices, moved by whole
 * pixels into the 64 x 64 target (the fractions kept: the vertex program's + 786432 leaves 1/16 of a pixel either way), without fog
 * and with the fog words of the hung group: which of them, if any, stops the engine by itself. */
#include "vx_93d_hang.h"
static const char *res(DWORD r);
#define HANG_N (sizeof(hang_tri)/sizeof(hang_tri[0]))
static float uf(DWORD u) {float f;memcpy(&f,&u,4);return f;}
static void hang_form(int first,int count,int fog) {
 static const DWORD prog[20]={0x0008014CUL,0x00206490UL,0x0040014CUL,0x0004021FUL,0x0008414CUL,0,0,0x0008422FUL,0x0008814CUL,0,0,0x0008423FUL,0x0008C124UL,0,0,0x00080283UL,0x00090124UL,0,0,0x00080293UL};
 static const DWORD fogw[9]={0x000030UL,0x2D22D2UL,0x0002D2UL,0x0041F0UL,0,0x00000DUL,0x000076UL,0x00437AUL,0x0FE000UL};
 DWORD k,m=0x010C0FUL;int t,v;
 T(1);R(0x00,fog?0xF01UL:1);R(0x01,0xCE1140UL);for(k=3;k<=15;k++)R(k,0);R(0x02,2);R(0x02,0);
 R(0x10,0);R(0x11,0);R(0x12,0x3C0000UL);R(0x13,0x70000UL);R(0x14,0xFFFFFFUL);R(0x15,0xFF);
 R(0x16,0);R(0x17,0);R(0x18,0);R(0x19,0x800000UL);R(0x1A,0x800000UL);R(0x1B,0x803FBFUL);R(0x1E,0x800000UL);R(0x1F,0);R(0x20,0x3F00);R(0x21,0);R(0x22,0);
 for(k=0x23;k<=0x3F;k++)if(k<0x28||k>0x2A)R(k,0);
 R(0x28,0);R(0x29,0);R(0x2A,0);
 R(0x50,0x2010);R(0x51,0);R(0x52,(0x99UL<<16)|8);R(0x53,m);R(0x5B,m);R(0x63,m);R(0x6B,m);
 R(0x80,64);R(0x81,64);R(0x82,64);R(0x83,64);R(0x84,0);R(0x85,0);R(0x88,0);R(0x89,64);R(0x8A,0);
 for(k=0x58;k<=0x6B;k++)if((k&7)<4)R(k,(k&7)==3?m:0);
 for(k=0x70;k<=0x78;k++)R(k,fog?fogw[k-0x70]:0);
 for(k=0x90;k<=0x9A;k++)R(k,k==0x90?0x58000UL:k==0x92?1:0);
 R(0x9B,0xFFFFFEUL);P(0x43C,0x20030000UL);D(0x01C00530UL);D(0x301C0053UL);D(0x00F04005UL);D(0x000D0820UL);
 T(4);R(0x00,0xA8040AUL);R(0x01,0x0000C2UL);R(0x02,0x0001D1UL);R(0x03,0x0002D1UL);R(0x04,0x000342UL);R(0x05,0x000442UL);for(k=6;k<=8;k++)R(k,0);
 R(0x12,0x1F);R(0x20,0);R(0x21,0);R(0x22,0);R(0x25,0xF6CC00UL);R(0x28,0xF6CC00UL);R(0x2F,0x582000UL);R(0x2C,0);R(0x2D,0);R(0x2E,0);
 R(0x50,0);R(0x51,0);R(0x52,0x147FFFUL);
 R(0x60,0x800004UL);R(0x61,0x800000UL);R(0x62,0x404004UL);
 konst0();P(0x43C,0x31030000UL);for(k=0;k<20;k++)D(prog[k]);
 P(0x43C,0xFE020000UL);for(k=1;k<=14;k++)if(k!=6&&k!=7)R(k,0);R(0,1);R(0,0);
 IDLE();T(1);R(0x01,0xCE1540UL);
 T(4);R(0x23,0xCE02UL);R(0x24,(DWORD)count*3);T(0);
 for(t=first;t<first+count;t++){const unsigned long *w=&hang_tri[t][1];float mx=uf(w[0]),my=uf(w[1]),sx,sy;
  for(v=1;v<3;v++){if(uf(w[v*10])<mx)mx=uf(w[v*10]);if(uf(w[v*10+1])<my)my=uf(w[v*10+1]);}
  sx=(float)((long)mx-8);sy=(float)((long)my-8);
  for(v=0;v<3;v++){D(fb(uf(w[v*10])-sx));D(fb(uf(w[v*10+1])-sy));for(k=2;k<10;k++)D(w[v*10+k]);}}
}
static int hang_run(void) {
 DWORD got=0,ok;int t,pass,recover=0,bad=0,sent=0;
 say("VX93D HANG: %lu triangles from two hangs, each alone without fog (pass 1) and with the hung group's fog (pass 2), then eight at a time (passes 3 and 4)\n",(unsigned long)HANG_N);
 for(pass=1;pass<=4;pass++){int step=pass<=2?1:8,fog=!(pass&1);
  for(t=0;t<(int)HANG_N;t+=step){int n=t+step<=(int)HANG_N?step:(int)HANG_N-t,x,y;DWORD ch=0;
   while(n>1&&hang_tri[t+n-1][0]!=hang_tri[t][0])n--; /* a burst stays within one hang's triangles */
   memset(&rq,0,sizeof(rq));rq.cb=sizeof(rq);rq.version=VX3D_VERSION;rq.arm=VX3D_ARM;rq.probe=1;rq.lab_cmd=0x33574152UL;rq.reserved=recover?0x100UL:0;
   nw=0;hang_form(t,n,fog);rq.lab_ndata=nw;memset(&rp,0,sizeof(rp));got=0;
   ok=DeviceIoControl(dev,VX_IOCTL_3D_TEST,&rq,sizeof(rq),&rp,sizeof(rp),&got,NULL);
   if(ok&&got==sizeof(rp)&&!recover&&rp.fault_latched&&rp.stage==0&&rp.writes==0){rq.reserved=0x100UL;memset(&rp,0,sizeof(rp));got=0;ok=DeviceIoControl(dev,VX_IOCTL_3D_TEST,&rq,sizeof(rq),&rp,sizeof(rp),&got,NULL);}
   if(!ok||got!=sizeof(rp)){say("pass %d triangle %d: the VxD refused the call (error %lu)\n",pass,t,GetLastError());return 2;}
   sent++;recover=rp.fault_latched!=0;
   for(y=0;y<64;y++)for(x=0;x<64;x++)if(rp.image[y*64+x]!=VX3D_BACKGROUND)ch++;
   if(rp.result!=1||rp.vertex_words!=0xFFFFFFFFUL||rp.fault_latched){bad++;
    say("pass %d, triangle%s %d%s%d (hang %lu's): %s; stage %lu; a wait failed at word %lu (reason %lu, status %08lX); status after the words %08lX; %lu pixels changed; fault latched %lu\n",
     pass,n>1?"s":"",t,n>1?"-":" of ",n>1?t+n-1:(int)HANG_N,hang_tri[t][0],res(rp.result),rp.stage,rp.vertex_words,rp.wait_failure_reason,rp.wait_failure_status,rp.status_solid,ch,rp.fault_latched);
    {int v;for(v=0;v<3*n&&v<6;v++){const unsigned long *w=&hang_tri[t+v/3][1+(v%3)*10];say("      vertex: X %08lX (%.4f) Y %08lX (%.4f) Z %08lX W %08lX colour %08lX specular %08lX\n",w[0],uf(w[0]),w[1],uf(w[1]),w[2],w[3],w[4],w[5]);}}}
   else if(pass<=2&&!(t&7))say("pass %d, triangle %d: ran, %lu pixels changed\n",pass,t,ch);}}
 say("\n%d requests sent\n",sent);
 say(bad?"RESULT: %d request(s) stopped the engine: the triangles are listed above\n":"RESULT: none of these triangles stopped the engine by itself (%d)\n",bad);
 return 0;
}
static void build(int form) {
 int k;nw=0;
 switch(form){
 case 'A':T(1);R(1,0x40);T(4);R(0x23,2);R(0x24,3);T(0);for(k=0;k<3;k++){D(fb(vx[k]));D(fb(vy[k]));D(0x3F000000UL);D(0x3F800000UL);D(0xFFE04020UL);}break;
 case 'B':ce_form(1,0,0,0,0,0,0x40);break;
 case 'C':ce_form(0,0,0,0,0,0,0x40);break;
 case 'D':ce_form(1,1,0,0,0,0,0x40);break;
 case 'E':ce_form(1,0,1,0,0,0,0x40);break;
 case 'F':T(1);R(1,0x40);P(0x43C,0);D(0xEC006800UL);D(0xEE020000UL);for(k=0;k<3;k++){D(fb(vx[k]));D(fb(vy[k]));D(fb(0.05f));}D(0xEE120300UL);D(0xEE120300UL);break;
 case 'G':ce_form(1,0,0,1,0,0,0x40);break;
 case 'H':ce_form(1,0,0,0,1,0,0x40);break;
 case 'I':ce_form(0,0,0,0,1,0,0x40);break;
 case 'J':ce_form(0,0,0,0,1,0,0x100040UL);break;
 case 'K':ce_form(1,0,0,0,1,0,0x100040UL);break;
 case 'L':ce_form(1,0,0,0,1,0x3F000000UL,0x40);break;
 case 'M':ce_form(1,0,0,0,1,0,0xCE1140UL);break;
 case 'N':ce_form(1,0,0,0,1,0,0xCE1540UL);break;
 case 'O':ce_form(1,0,0,0,2,0,0x40);break;
 case 'P':ce_form(1,0,0,0,3,0,0x40);break;
 case 'Q':ce_form(1,0,0,0,4,0,0x40);break;
 case 'R':case 'S':case 'T':case 'U':case 'V':case 'W':colour_form((DWORD)(form-'R'+2));break;
 case 'X':colour_form(0);break;
 case '1':tl_form(1,0);break;
 case '2':tl_form(2,0);break;
 case '3':tl_form(3,0);break;
 case '4':tl_form(3,1);break;
 case '5':tl_form(3,2);break;
 case '6':case '7':case '8':case '9':form4x(form-'0');break;
 case 'Y':case 'Z':case '0':driver_form(form);break;
 }
 rq.lab_ndata=nw;
}
static const char *name(int f) {
 switch(f){case 'A':return "A the VX855's vertex words (control)";case 'B':return "B CE's VX900 setting: descriptors, vertex program, X Y + 786432";
 case 'C':return "C as B without the 786432";case 'D':return "D as B with the plain primitive words";case 'E':return "E as B, 04/61 62 60 in XP's order";
 case 'F':return "F OpenChrome's direct form (EC / EE)";case 'G':return "G as B, then the wait word 6C = 00800000";case 'H':return "H as B with CE's whole Type 04 table (clip-to-screen scale 1, offset 786432)";
 case 'I':return "I as H without the 786432 added in software";case 'J':return "J as I with 01/01 bit 20 (clip-to-screen) on";case 'K':return "K as H with 01/01 bit 20 on";
 case 'L':return "L as H at Z = 0.5";case 'M':return "M as H with 01/01 = CE1140";case 'N':return "N as H with 01/01 = CE1540";
 case 'O':return "O as B with only the table's clip-to-screen words (04/40-48)";case 'P':return "P as B with the table less 04/40-48";case 'Q':return "Q as B with only 04/50-52";
 case 'R':return "R as H with a D3DCOLOR vertex colour, program output 2";case 'S':return "S the colour to output 3";case 'T':return "T the colour to output 4";case 'U':return "U the colour to output 5";
 case 'V':return "V the colour to output 6";case 'W':return "W the colour to output 7";case 'X':return "X the colour to output 0";
 case '1':return "1 as R with red, green and blue vertices and Gouraud shading";case '2':return "2 seven words a vertex: X Y Z W, colour, S T";case '3':return "3 eight words a vertex (Direct3D's): X Y Z W, colour, specular, S T";
 case '4':return "4 as 3 with the vertices unchanged: the program adds the 786432 (constant 0, loaded before the program)";case '5':return "5 as 4, the constant loaded after the program";case '6':return "6 as 4 with 04/20 = 0 (this driver's list word)";
 case '7':return "7 depth: orange at Z 0.25, then blue at Z 0.75 over it; ORANGE must stay";case '8':return "8 depth: orange at Z 0.75, then blue at Z 0.25; BLUE must win";case '9':return "9 a strip of four vertices (04/20 = 386300): an orange square, 2304 pixels";
 case 'Y':return "Y the driver's own state set-up and pixel program (01/01 = CE1540, 04/23 = CE02): an orange triangle";case 'Z':return "Z as Y with red, green and blue vertices";default:return "0 as Y with the driver's depth words: orange at Z 0.25, then blue at Z 0.75; ORANGE must stay";}}
static const char *res(DWORD r) {
 static const char *const n[]={"not run","ran","NOT READY (the mode's screen reaches the scratch area at 2 MB, or the driver is not in its normal state: use 800 x 600 x 16)","bad register window","no register mapping","BUSY before the test","TIMEOUT: the engine stayed busy","guard words changed (something drew outside the target)","solid","shade","depth","the 3D test is DISABLED (an earlier form left the engine busy and the reset did not free it: restart Windows)","bad request","the engine could not be opened","the 2D fill check failed"};
 return r<sizeof(n)/sizeof(n[0])?n[r]:"?";}
int main(int argc,char **argv) {
 char path[MAX_PATH],*s;const char *forms=argc>1?argv[1]:"4YZ0";DWORD in,o[8],got=0;int k,any=0,recover=0;static char drawn[40],coloured[40];int ndrawn=0,ncol=0;HANDLE t;
 GetModuleFileNameA(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VX93D.LOG");lf=fopen(path,"w");
 say("VX93D v1.9 (driver .624; VX93D INDEX draws by index from a vertex buffer; VX93D HANG replays the triangles of two hangs): one triangle, (8,8) (56,8) (8,56), by several vertex input forms; the 64 x 64 target read back\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE){say("FAILED: the VxD is not open\n");if(lf)fclose(lf);return 2;}
 t=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);in=0x75;memset(o,0,sizeof(o));
 if(t!=INVALID_HANDLE_VALUE){DeviceIoControl(t,0x85500022UL,&in,4,o,32,&got,0);CloseHandle(t);}
 say("the VxD's build: .%lu; the device 1106:%04lX (%s)\n",o[1],o[2],o[2]==0x7122?"VX900":o[2]==0x5122?"VX855":o[2]==0x1122?"VX800":"?");
 if(o[1]<604){say("FAILED: the VxD is older than .604\n");if(lf)fclose(lf);return 2;}
 if(argc>1&&(!strcmp(argv[1],"HANG")||!strcmp(argv[1],"hang"))){hang_run();if(lf)fclose(lf);CloseHandle(dev);return 0;}
 if(argc>1&&(!strcmp(argv[1],"INDEX")||!strcmp(argv[1],"index"))){if(o[1]<624)say("FAILED: VX93D INDEX v1.9 needs driver .624 or later\n");else index_run();if(lf)fclose(lf);CloseHandle(dev);return 0;} /* v1.8 */
 for(k=0;forms[k];k++){int f=forms[k]>='a'&&forms[k]<='z'?forms[k]-32:forms[k],x,y,bit;DWORD ch=0,orange=0,white=0,blue=0,minx=64,miny=64,maxx=0,maxy=0,ok;
  if(!((f>='A'&&f<='Z')||(f>='0'&&f<='9')))continue;bit=f>='A'?f-'A':26+f-'0';
  memset(&rq,0,sizeof(rq));rq.cb=sizeof(rq);rq.version=VX3D_VERSION;rq.arm=VX3D_ARM;rq.probe=1;rq.lab_cmd=0x33574152UL;rq.reserved=recover?0x100UL:0;
  build(f);memset(&rp,0,sizeof(rp));got=0;
  ok=DeviceIoControl(dev,VX_IOCTL_3D_TEST,&rq,sizeof(rq),&rp,sizeof(rp),&got,NULL);
  if(ok&&got==sizeof(rp)&&!recover&&rp.fault_latched&&rp.stage==0&&rp.writes==0){ /* an earlier run left the fault latched: once more, with the reset */
   rq.reserved=0x100UL;memset(&rp,0,sizeof(rp));got=0;ok=DeviceIoControl(dev,VX_IOCTL_3D_TEST,&rq,sizeof(rq),&rp,sizeof(rp),&got,NULL);say("\n(an earlier run had left the engine's fault latched: this form was sent again with the 3D reset)");}
  say("\n%s: %lu words\n",name(f),rq.lab_ndata/2);
  if(!ok||got!=sizeof(rp)){say("   the VxD refused the call (error %lu, %lu bytes)\n",GetLastError(),got);continue;}
  any=1;
  say("   %s; stage %lu (2 set-up done, 3 all words sent, 4 idle after them); words sent %lu; status before %08lX, after set-up %08lX, after the words %08lX, last %08lX, all OR'd %08lX; register writes %lu, status polls %lu, %lu ms\n",
   res(rp.result),rp.stage,rp.vertex_vertices,rp.status_before,rp.status_setup,rp.status_solid,rp.status_after,rp.status_or,rp.writes,rp.polls,rp.elapsed_ms);
  if(rp.vertex_words!=0xFFFFFFFFUL)say("   a wait failed at word %lu (reason %lu: 1 polls, 2 time, 3 all ones, 4 busy at the end; status %08lX)\n",rp.vertex_words,rp.wait_failure_reason,rp.wait_failure_status);
  say("   the 2D fill check: %lu errors, sample %08lX; guards %lu; engine opened: flags %03lX; SR1A %02lX SR1E %02lX SR4E %02lX SR3C %02lX; fault latched %lu\n",rp.blit_errors,rp.blit_sample,rp.guard_errors,rp.init_flags,rp.seq_before[1],rp.seq_before[2],rp.seq_before[5],rp.seq_before[6],rp.fault_latched);
  recover=rp.fault_latched!=0;
  if(rp.result==2||rp.result==11||rp.stage<2)continue;
  for(y=0;y<64;y++)for(x=0;x<64;x++){DWORD c=rp.image[y*64+x];if(c==VX3D_BACKGROUND)continue;ch++;if((c&0xFFFFFFUL)==0xE04020UL)orange++;else if((c&0xFFFFFFUL)==0xFFFFFFUL)white++;else if((c&0xFFFFFFUL)==0x2040E0UL)blue++;
   if((DWORD)x<minx)minx=x;if((DWORD)x>maxx)maxx=x;if((DWORD)y<miny)miny=y;if((DWORD)y>maxy)maxy=y;}
  say("   the target: %lu pixels changed (%lu orange E04020, %lu white, %lu blue 2040E0, %lu other)",ch,orange,white,blue,ch-orange-white-blue);
  if(ch)say(", all within (%lu,%lu)-(%lu,%lu); pixel (16,16) = %08lX, (40,40) = %08lX",minx,miny,maxx,maxy,rp.image[16*64+16],rp.image[40*64+40]);
  say("\n");
  if(ch)say("   pixels (10,10) = %08lX, (52,10) = %08lX, (10,52) = %08lX, (24,24) = %08lX\n",rp.image[10*64+10],rp.image[10*64+52],rp.image[52*64+10],rp.image[24*64+24]);
  if(ch){say("   picture, every second pixel (# orange, W white, B blue, + other, . untouched):\n");
   for(y=0;y<64;y+=2){char row[40];for(x=0;x<64;x+=2){DWORD c=rp.image[y*64+x];row[x/2]=c==VX3D_BACKGROUND?'.':(c&0xFFFFFFUL)==0xE04020UL?'#':(c&0xFFFFFFUL)==0xFFFFFFUL?'W':(c&0xFFFFFFUL)==0x2040E0UL?'B':'+';}row[32]=0;say("      %s\n",row);}}
  if(f=='7'||f=='8'||f=='0'){int good=ch==1176&&(f=='8'?blue:orange)==1176;say(good?"   -> DEPTH RIGHT (%s)\n":"   -> DEPTH WRONG (%s expected on all 1176 pixels)\n",f=='8'?"blue won":"orange kept");if(good){drawn[bit]=1;ndrawn++;};}
  else if(f=='9'){int good=ch==2304&&orange==2304;say(good?"   -> THE STRIP'S SQUARE IS THERE\n":"   -> not the strip's square (2304 orange pixels expected)\n");if(good){drawn[bit]=1;ndrawn++;};}
  else if(ch>=1000&&ch<=1300&&minx>=7&&miny>=7&&maxx<=57&&maxy<=57){say("   -> THE TRIANGLE IS THERE\n");{drawn[bit]=1;ndrawn++;};if(orange>=1000){coloured[bit]=1;ncol++;};}
  else if(ch)say("   -> something was drawn, not the triangle asked for\n");
  else say("   -> nothing was drawn\n");
 }
 say("\n");
 if(!any)say("NOT RUN\n");
 else if(!ndrawn)say("RESULT: no form was right\n");
 else{say("RESULT: right in form(s)");for(k=0;k<36;k++)if(drawn[k])say(" %c",k<26?'A'+k:'0'+k-26);say("\n");
  if(ncol){say("RESULT: in orange, the colour of the vertices, by form(s)");for(k=0;k<36;k++)if(coloured[k])say(" %c",k<26?'A'+k:'0'+k-26);say("\n");}}
 if(lf)fclose(lf);CloseHandle(dev);return 0;
}
