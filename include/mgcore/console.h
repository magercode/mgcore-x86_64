#ifndef MGCORE_CONSOLE_H
#define MGCORE_CONSOLE_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

void console_init(void);
void console_clear(void);
void console_putc(char c);
void console_write(const char *s);
void console_write_n(const char *s, size_t length);
void console_writeln(const char *s);
void console_printf(const char *fmt, ...);
void console_vprintf(const char *fmt, va_list args);
void console_enqueue_input(char c);
int console_try_read_char(char *out);

#endif
