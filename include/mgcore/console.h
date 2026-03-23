#ifndef MGCORE_CONSOLE_H
#define MGCORE_CONSOLE_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

void console_init(void);
void console_clear(void);
void console_putc(char c);
void console_write(const char *pattern, ...);
void console_writef(const char *pattern, ...);
void console_write_n(const char *s, size_t length);
void console_writeln(const char *pattern, ...);
void console_writelnf(const char *pattern, ...);
void console_set_color_rgb(uint8_t fr, uint8_t fg, uint8_t fb,
                           uint8_t br, uint8_t bg, uint8_t bb);
void console_set_color_hex(uint32_t fg_hex, uint32_t bg_hex);
void console_reset_color(void);
void console_scroll_view(int delta_lines);
void console_view_to_bottom(void);
void console_printf(const char *fmt, ...);
void console_vprintf(const char *fmt, va_list args);
void console_enqueue_input(char c);
int console_try_read_char(char *out);

#endif
