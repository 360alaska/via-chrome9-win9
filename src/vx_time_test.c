/* VXTIME v3.3 (driver .791; board, E789 on a Dell E2210 and a Dell SE2722H: every rate right, but a formula timing at a rate with no VESA
 * standard for its size is taken by an LCD on VGA for another mode, too wide or too narrow): only what the driver should offer is shown,
 * a rate at a size where a VESA standard timing exists (std[] below, the VxD's tables) and 60 Hz for every size.
 * VXTIME v3.2 (driver .789): 45 seconds a step (the owner); on a device whose own timing is off (the VX900) it is switched on for the run. VXTIME v3.1 (driver .787B): 30 seconds a step (the owner: time to try the monitor's auto adjust before answering). v3.0 (driver .787), written again from nothing at the owner's word: the driver's own mode timing on the monitor.
 * Full screen in each mode, with five nested one-pixel borders (white on the outermost pixels, then red 8, green 16, yellow 32 and
 * cyan 64 pixels in), so a border that is cut off or shifted can be judged against the ones inside it, and a centre cross.
 * Each size from 640x480 that Windows offers at the desktop's depth, at 60, 65, 70, 72 and 75 Hz, 45 seconds a step:
 *   Y the picture is right, N it is not, no key in 45 seconds = "not answered", Esc ends the run.
 * The VxD holds the test rate for every mode set while it is set (VX_probe sub 5, 0x100 | Hz; 0x100 clears; it forgets it after 20 s):
 * Windows sets a mode more than once a change, and v2's one-shot rate was gone after the first, so every v2 step showed the BIOS's
 * timing. Before a step is shown the VxD is asked whether its own timing is the one programmed: a step where it is not is not shown and
 * not asked about (logged as REFUSED). The rate is measured on the vertical retrace. The timing is the VESA standard one where the size
 * and rate have one, the GTF formula elsewhere; the screen says which.
 * VXTIME HI: only the sizes above 1024x768. VXTIME 75 (50-120): that one rate. Log: VXTIME.LOG. The desktop's mode is put back at the end. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "vx85598.h"
#define MAXSIZE 24
static FILE *logf;
static HANDLE vxd = INVALID_HANDLE_VALUE;
static char line1[64], line2[64], line3[64];
static volatile int answer, stop;

static void out(const char *f, ...)
{
	char b[1024]; va_list a;
	va_start(a, f); _vsnprintf(b, sizeof(b) - 1, f, a); b[sizeof(b) - 1] = 0; va_end(a);
	if(logf) { fputs(b, logf); fflush(logf); }
}
static int ask(DWORD sub, DWORD arg, DWORD *o)
{
	DWORD in[2], got = 0;
	in[0] = sub; in[1] = arg; memset(o, 0, 64 * 4);
	return vxd != INVALID_HANDLE_VALUE && DeviceIoControl(vxd, VX_IOCTL_PROBE, in, 8, o, 64 * 4, &got, NULL) && got == 64 * 4 && o[0] == VX_MAGIC;
}
static void frame(HDC dc, int inset, int w, int h, COLORREF c)
{
	HPEN p = CreatePen(PS_SOLID, 1, c), old = (HPEN)SelectObject(dc, p);
	HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
	Rectangle(dc, inset, inset, w - inset, h - inset); /* the last column and row drawn are w - inset - 1 and h - inset - 1 */
	SelectObject(dc, ob); SelectObject(dc, old); DeleteObject(p);
}
static LRESULT CALLBACK proc(HWND hw, UINT m, WPARAM wp, LPARAM lp)
{
	if(m == WM_KEYDOWN)
	{
		if(wp == 'Y') answer = 1; else if(wp == 'N') answer = 2; else if(wp == VK_ESCAPE) stop = 1;
		return 0;
	}
	if(m == WM_ERASEBKGND) return 1;
	if(m == WM_PAINT)
	{
		PAINTSTRUCT ps; HDC dc = BeginPaint(hw, &ps); RECT r, t; HBRUSH b; HFONT f, of; HPEN p, op; int w, h, y;
		GetClientRect(hw, &r); w = r.right; h = r.bottom;
		b = CreateSolidBrush(RGB(40, 40, 40)); FillRect(dc, &r, b); DeleteObject(b);
		frame(dc, 0, w, h, RGB(255, 255, 255));
		frame(dc, 8, w, h, RGB(255, 0, 0));
		frame(dc, 16, w, h, RGB(0, 255, 0));
		frame(dc, 32, w, h, RGB(255, 255, 0));
		frame(dc, 64, w, h, RGB(0, 255, 255));
		p = CreatePen(PS_SOLID, 1, RGB(128, 128, 128)); op = (HPEN)SelectObject(dc, p);
		MoveToEx(dc, w / 2, 64, NULL); LineTo(dc, w / 2, h - 64); MoveToEx(dc, 64, h / 2, NULL); LineTo(dc, w - 64, h / 2);
		SelectObject(dc, op); DeleteObject(p);
		f = CreateFontA(18, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, 0, 0, 0, 0, "Arial"); of = (HFONT)SelectObject(dc, f);
		SetBkMode(dc, OPAQUE); SetBkColor(dc, RGB(40, 40, 40));
		y = h / 2 - 78;
		t = r; t.top = y; SetTextColor(dc, RGB(255, 255, 255)); DrawTextA(dc, line1, -1, &t, DT_CENTER | DT_TOP | DT_SINGLELINE);
		t = r; t.top = y + 24; DrawTextA(dc, line2, -1, &t, DT_CENTER | DT_TOP | DT_SINGLELINE);
		t = r; t.top = y + 48; SetTextColor(dc, RGB(200, 200, 200)); DrawTextA(dc, line3, -1, &t, DT_CENTER | DT_TOP | DT_SINGLELINE);
		t = r; t.top = y + 84; DrawTextA(dc, "borders: white edge, red 8, green 16,", -1, &t, DT_CENTER | DT_TOP | DT_SINGLELINE);
		t = r; t.top = y + 104; DrawTextA(dc, "yellow 32, cyan 64 pixels in", -1, &t, DT_CENTER | DT_TOP | DT_SINGLELINE);
		t = r; t.top = y + 136; SetTextColor(dc, RGB(255, 255, 0)); DrawTextA(dc, "Y = right   N = not right   Esc = stop", -1, &t, DT_CENTER | DT_TOP | DT_SINGLELINE);
		SelectObject(dc, of); DeleteObject(f);
		EndPaint(hw, &ps);
		return 0;
	}
	return DefWindowProcA(hw, m, wp, lp);
}
/* messages for ms; with keys: until Y, N or Esc */
static void wait_ms(DWORD ms, int keys)
{
	DWORD end = GetTickCount() + ms; MSG m;
	for(;;)
	{
		while(PeekMessageA(&m, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
		if(keys && (answer || stop)) return;
		if((long)(end - GetTickCount()) <= 0) return;
		Sleep(10);
	}
}
/* the sizes and rates above 60 Hz with a VESA standard timing (as vx_dmt_r in vxd_vesa.c: keep the two the same) */
static int has_std(DWORD w, DWORD h, DWORD hz)
{
	static const unsigned short std[][3] = { {640,480,72},{640,480,75},{800,600,72},{800,600,75},{1024,768,70},{1024,768,75},{1152,864,75},{1280,768,75},
		{1280,800,75},{1280,1024,75},{1440,900,75},{1600,1200,65},{1600,1200,70},{1600,1200,75},{1680,1050,75} };
	unsigned i;
	if(hz == 60) return 1;
	for(i = 0; i < sizeof(std) / sizeof(std[0]); i++) if(std[i][0] == w && std[i][1] == h && std[i][2] == hz) return 1;
	return 0;
}
static LONG set_mode(DWORD w, DWORD h, DWORD bpp)
{
	DEVMODEA dm;
	memset(&dm, 0, sizeof(dm)); dm.dmSize = sizeof(dm);
	dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL; dm.dmPelsWidth = w; dm.dmPelsHeight = h; dm.dmBitsPerPel = bpp;
	return ChangeDisplaySettingsA(&dm, CDS_FULLSCREEN);
}
/* the refresh rate from the VxD's count of retrace edges (sub 2: [2] edges, [3] ms, [4] ticks first to last edge, [5] ticks in all) */
static double retrace_hz(void)
{
	DWORD o[64];
	if(!ask(2, 0, o) || o[2] < 3 || !o[3] || !o[5]) return 0;
	return (double)(o[2] - 1) * ((double)o[4] / (double)o[5]) * 1000.0 / (double)o[3];
}
static void regs(const char *who, const DWORD *d)
{
	unsigned char b[20]; int i;
	for(i = 0; i < 20; i++) b[i] = (unsigned char)(d[i / 4] >> ((i & 3) * 8));
	out("      %s: line %lu, sync %lu-%lu; frame %lu, sync at %lu; misc %02X; raw", who,
		(((DWORD)b[0] | ((DWORD)((b[15] >> 3) & 1) << 8)) + 5) * 8, ((DWORD)b[4] | ((DWORD)((b[13] >> 4) & 1) << 8)) * 8, (DWORD)(b[5] & 31) * 8,
		((DWORD)b[6] | ((DWORD)(b[7] & 1) << 8) | ((DWORD)((b[7] >> 5) & 1) << 9) | ((DWORD)(b[14] & 1) << 10)) + 2,
		((DWORD)b[8] | ((DWORD)((b[7] >> 2) & 1) << 8) | ((DWORD)((b[7] >> 7) & 1) << 9) | ((DWORD)((b[14] >> 1) & 1) << 10)), b[18]);
	for(i = 0; i < 20; i++) out(" %02X", b[i]);
	out("\n");
}
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
	static const DWORD all_rates[5] = { 60, 65, 70, 72, 75 };
	DWORD nsteps = 0, rates[5], nrates = 5, size[MAXSIZE][2], nsize = 0, bpp, o[64], i, k, r;
	int was_off = 0, hi_only = 0, shown = 0, right = 0, wrong = 0, silent = 0, refused = 0, badrate = 0;
	DEVMODEA dm, desk; WNDCLASSA wc; char msg[400];
	(void)prev; (void)show;
	memcpy(rates, all_rates, sizeof(rates));
	logf = fopen("VXTIME.LOG", "w");
	out("VXTIME v3.3 (driver .793): the driver's own timing where a VESA standard exists (and 60 Hz everywhere), full screen with five borders, 45 seconds a step\n");
	if(cmd && (strstr(cmd, "HI") || strstr(cmd, "hi"))) hi_only = 1;
	if(cmd) { int v = atoi(cmd); if(v >= 50 && v <= 120) { rates[0] = (DWORD)v; nrates = 1; } }
	vxd = CreateFileA("\\\\.\\VX85598", 0, 0, NULL, OPEN_EXISTING, 0, NULL);
	if(!ask(5, 0x100, o)) { out("FAILED: the VxD does not answer (not this driver)\n"); goto done; }
	if(!o[3]) { ask(5, 1, o); was_off = 1; out("the driver's own timing is off on this device (the BIOS's is used, as on the VX900): switched on for this run, off again at the end\n"); }
	if(!ask(5, 0x100 | 60, o) || o[7] != 60) { out("FAILED: this VxD takes no test rate (a driver before .787)\n"); goto done; }
	ask(5, 0x100, o);
	memset(&desk, 0, sizeof(desk)); desk.dmSize = sizeof(desk); EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &desk); bpp = desk.dmBitsPerPel;
	for(i = 0; nsize < MAXSIZE; i++)
	{
		memset(&dm, 0, sizeof(dm)); dm.dmSize = sizeof(dm);
		if(!EnumDisplaySettingsA(NULL, i, &dm)) break;
		if(dm.dmBitsPerPel != bpp || dm.dmPelsWidth < 640 || dm.dmPelsHeight < 480) continue;
		if(hi_only && dm.dmPelsWidth <= 1024 && dm.dmPelsHeight <= 768) continue;
		for(k = 0; k < nsize; k++) if(size[k][0] == dm.dmPelsWidth && size[k][1] == dm.dmPelsHeight) break;
		if(k == nsize) { size[nsize][0] = dm.dmPelsWidth; size[nsize][1] = dm.dmPelsHeight; nsize++; }
	}
	for(i = 0; i < nsize; i++) for(k = i + 1; k < nsize; k++) if(size[k][0] * size[k][1] < size[i][0] * size[i][1])
	{ DWORD a = size[i][0], b = size[i][1]; size[i][0] = size[k][0]; size[i][1] = size[k][1]; size[k][0] = a; size[k][1] = b; }
	out("desktop %lux%lu, %lu bits; %lu size(s), %lu rate(s)\n", desk.dmPelsWidth, desk.dmPelsHeight, bpp, nsize, nrates);
	if(!nsize) { out("NOT RUN: no size to test\n"); goto done; }
	{ DWORD a, b2, cnt = 0; for(a = 0; a < nsize; a++) for(b2 = 0; b2 < nrates; b2++) if(nrates == 1 || has_std(size[a][0], size[a][1], rates[b2])) cnt++; nsteps = cnt; }
	sprintf(msg, "%lu steps of up to 45 seconds (%lu sizes; 60 Hz, and the higher rates that have a VESA standard timing).\n\nEach step fills the screen with five borders: white on the very edge, then red, green, yellow and cyan further in.\n"
		"Press Y if all of them are there and steady, N if not. There is time to try the monitor's auto adjust first. A dark screen: wait 45 seconds, or press N.\nEsc stops.\n\nStart?", nsteps, nsize);
	if(MessageBoxA(NULL, msg, "VXTIME", MB_OKCANCEL | MB_TOPMOST | MB_SETFOREGROUND) != IDOK) { out("NOT RUN: cancelled\n"); goto done; }
	memset(&wc, 0, sizeof(wc)); wc.lpfnWndProc = proc; wc.hInstance = inst; wc.lpszClassName = "VXTIME3"; wc.hCursor = LoadCursorA(0, IDC_ARROW); RegisterClassA(&wc);

	for(k = 0; k < nsize && !stop; k++) for(r = 0; r < nrates && !stop; r++)
	{
		const DWORD w = size[k][0], h = size[k][1], hz = rates[r]; DWORD q[64], other = k ? 0 : 1; LONG rc; HWND win; double got; const char *said;
		if(nrates > 1 && !has_std(w, h, hz)) continue; /* v3.3 (a single rate asked for on the command line is shown whatever it is) */
		/* Windows does not set the mode it is in: every step comes from another size (or, with one size only, from the desktop's) */
		ask(5, 0x100, q);
		if(nsize > 1) set_mode(size[other][0], size[other][1], bpp); else ChangeDisplaySettingsA(NULL, 0);
		wait_ms(500, 0);
		ask(5, 0x100 | hz, q);
		rc = set_mode(w, h, bpp);
		wait_ms(800, 0);
		ask(5, 0x100 | hz, q); /* (read; the rate set again, which also starts its 20 seconds again) */
		if(rc != DISP_CHANGE_SUCCESSFUL) { out("%4lux%-4lu %3lu Hz: Windows refused the mode (%ld)\n", w, h, hz, rc); continue; }
		if(q[4] != 1)
		{
			out("%4lux%-4lu %3lu Hz: REFUSED by the driver (%s; clock asked %lu kHz): not shown\n", w, h, hz, q[4] == 2 ? "no clock setting or timing" : "its timing was not programmed", q[9]);
			regs("the BIOS's", &q[16]);
			refused++; continue;
		}
		sprintf(line1, "%lu x %lu   %lu Hz", w, h, hz);
		sprintf(line2, "driver's timing: %s", q[8] == 3 ? "formula" : "VESA standard");
		sprintf(line3, "step %d", shown + refused + 1);
		answer = 0;
		win = CreateWindowExA(WS_EX_TOPMOST, "VXTIME3", "VXTIME", WS_POPUP | WS_VISIBLE, 0, 0, (int)w, (int)h, 0, 0, inst, 0);
		if(win) { SetForegroundWindow(win); SetFocus(win); }
		wait_ms(2200, 0); got = retrace_hz();
		if(!answer && !stop) wait_ms(42000, 1);
		ask(5, 0x100 | hz, q); /* still the driver's timing at the end of the step? */
		if(win) DestroyWindow(win);
		said = answer == 1 ? "right" : answer == 2 ? "NOT RIGHT" : "not answered";
		shown++; if(answer == 1) right++; else if(answer == 2) wrong++; else silent++;
		out("%4lux%-4lu %3lu Hz: %s; measured %.2f Hz; clock asked %lu kHz; PLL SR44 %02lX SR45 %02lX SR46 %02lX; seen: %s\n", w, h, hz,
			q[8] == 3 ? "formula" : "VESA standard", got, q[9], q[6] & 255, (q[6] >> 8) & 255, (q[6] >> 16) & 255, said);
		regs("the BIOS's", &q[16]); regs("programmed", &q[24]);
		if(q[4] != 1) { out("      WRONG: the driver's timing was no longer the one programmed at the end of the step\n"); badrate++; }
		else if(got < (double)hz - 1.5 || got > (double)hz + 1.5) { out("      WRONG: the measured rate is not the one asked for\n"); badrate++; }
	}
	ask(5, 0x100, o);
	if(was_off) ask(5, 0, o); /* before the desktop's mode is set again: that set is the BIOS's */
	if(nsize > 1 && shown + refused) { set_mode(size[(desk.dmPelsWidth == size[0][0] && desk.dmPelsHeight == size[0][1]) ? 1 : 0][0], size[(desk.dmPelsWidth == size[0][0] && desk.dmPelsHeight == size[0][1]) ? 1 : 0][1], bpp); wait_ms(500, 0); }
	ChangeDisplaySettingsA(NULL, 0); wait_ms(1000, 0);
	out("the desktop's mode put back%s\n", stop ? " (stopped with Esc)" : "");
	out("%s: %d shown: %d right, %d NOT right, %d not answered; %d refused by the driver; %d with a wrong rate\n",
		!shown ? "NOT RUN" : (wrong || badrate || refused) ? "FAILED" : silent ? "PASSED WHERE ANSWERED" : "PASSED", shown, right, wrong, silent, refused, badrate);
	sprintf(msg, "Done: %d right, %d not right, %d not answered, %d refused by the driver.\nSend VXTIME.LOG.", right, wrong, silent, refused);
	MessageBoxA(NULL, msg, "VXTIME", MB_OK | MB_TOPMOST | MB_SETFOREGROUND);
done:
	if(was_off) { DWORD z[64]; ask(5, 0, z); }
	if(logf) fclose(logf);
	if(vxd != INVALID_HANDLE_VALUE) CloseHandle(vxd);
	return 0;
}
