#ifndef __WMG_DEBUG_H
#define __WMG_DEBUG_H

#if __cplusplus
extern "C" {
#endif

extern int wmg_debug_level;
extern int wmg_debug_show_keys;
extern int wmg_debug_timestap;

#ifdef __GNUC__
#define PRINTF_FORMAT(a,b) __attribute__ ((format (printf, (a), (b))))
#define STRUCT_PACKED __attribute__ ((packed))
#else
#define PRINTF_FORMAT(a,b)
#define STRUCT_PACKED
#endif

enum {
	MSG_ERROR=0, MSG_WARNING, MSG_INFO,MSG_DEBUG, MSG_MSGDUMP, MSG_EXCESSIVE
};

#ifdef CONFIG_NO_STDOUT_DEBUG

#define wmg_printf(args...) do { } while (0)
#define wmg_debug_open_file(p) do { } while (0)
#define wmg_debug_close_file() do { } while (0)

#else
int wmg_debug_open_file(const char *path);
void wmg_debug_close_file(void);
void wmg_debug_open_syslog(void);
void wmg_debug_close_syslog(void);
void wmg_set_debug_level(int level);
int wmg_get_debug_level();


#ifdef CONFIG_DEBUG_FUNCTION_LINE
#define wmg_printf(level,fmt,arg...) \
	wmg_print(level,"<%s:%u>:" fmt "",__FUNCTION__,__LINE__,##arg)
#else
#define wmg_printf(level,fmt,arg...) \
	wmg_print(level,fmt,##arg)
#endif /*CONFIG_DEBUG_FUNCTION_LINE*/

void wmg_print(int level, const char *fmt, ...)
PRINTF_FORMAT(2, 3);

#endif/* CONFIG_NO_STDOUT_DEBUG */

#if __cplusplus
};  // extern "C"
#endif

#endif
