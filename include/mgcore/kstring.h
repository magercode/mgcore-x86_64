#ifndef MGCORE_KSTRING_H
#define MGCORE_KSTRING_H

#include <stdarg.h>
#include <stddef.h>

size_t kstrlen(const char *s);
int kstrcmp(const char *lhs, const char *rhs);
int kstrncmp(const char *lhs, const char *rhs, size_t count);
char *kstrcpy(char *dst, const char *src);
char *kstrncpy(char *dst, const char *src, size_t count);
void *kmemcpy(void *dst, const void *src, size_t size);
void *kmemmove(void *dst, const void *src, size_t size);
void *kmemset(void *dst, int value, size_t size);
int kmemcmp(const void *lhs, const void *rhs, size_t size);
int kvsnprintf(char *buffer, size_t size, const char *fmt, va_list args);
int ksnprintf(char *buffer, size_t size, const char *fmt, ...);
int kvformat_braces(char *buffer, size_t size, const char *pattern, va_list args);
int kformat_braces(char *buffer, size_t size, const char *pattern, ...);

#endif
