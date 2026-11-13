/*
 * Open Watcom generate invalid code when using inline const strings,
 * so this code doesn't work:
 *   void somefunc(){
 *     printf("Hello %s!", "World");
 *   }
 * Workaround is following:
 *   static char str_hello[] = "Hello %s!";
 *   static char str_world[] = "World";
 *   void somefunc(){
 *     printf(str_hello, str_world);
 *   }
 * 
 * And for these string is this file...
 *
 */
#ifdef DBGPRINT

#ifdef VXD_MAIN
#define DSTR(_n, _s) char _n[] = _s
#else
#define DSTR(_n, _s) extern char _n[]
#endif


DSTR(dbg_Device_Init_proc, "Device_Init_proc\n");






DSTR(dbg_destroy, "Driver destroyed\n");

















DSTR(dbg_cursor_empty, "new cursor: empty %X\n");











DSTR(dbg_map_pm16, "map_pm16: vm = %ld, linear = %lX, size = %ld\n");

DSTR(dbg_map_pm16_sel, "map_pm16: selector %lX\n");

DSTR(dbg_map_pm16_qw, "map_pm16: high=%lX, low=%lX\n");






















DSTR(dbg_mouse_invalidate, "MOUSE = invalidate\n");
DSTR(dbg_mouse_load, "MOUSE = load ");
DSTR(dbg_mouse_move, "MOUSE = xy(%ld, %ld)\n");

DSTR(dbg_mouse_status, " valid=%ld, visible=%ld, empty=%ld\n");

DSTR(dbg_mouse_hide, "MOUSE = hide\n");

#undef DSTR

#endif
