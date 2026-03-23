#include "mgcore/kstring.h"

#include <stdint.h>

size_t kstrlen(const char *s) {
  size_t len = 0;
  while (s && s[len] != '\0') {
    ++len;
  }
  return len;
}

int kstrcmp(const char *lhs, const char *rhs) {
  while (*lhs && (*lhs == *rhs)) {
    ++lhs;
    ++rhs;
  }
  return (unsigned char)*lhs - (unsigned char)*rhs;
}

int kstrncmp(const char *lhs, const char *rhs, size_t count) {
  size_t i;
  for (i = 0; i < count; ++i) {
    unsigned char a = (unsigned char)lhs[i];
    unsigned char b = (unsigned char)rhs[i];
    if (a != b || a == '\0' || b == '\0') {
      return a - b;
    }
  }
  return 0;
}

char *kstrcpy(char *dst, const char *src) {
  char *out = dst;
  while ((*dst++ = *src++) != '\0') {
  }
  return out;
}

char *kstrncpy(char *dst, const char *src, size_t count) {
  size_t i;
  for (i = 0; i < count; ++i) {
    dst[i] = src[i];
    if (src[i] == '\0') {
      break;
    }
  }
  for (; i < count; ++i) {
    dst[i] = '\0';
  }
  return dst;
}

void *kmemcpy(void *dst, const void *src, size_t size) {
  size_t i;
  uint8_t *out = (uint8_t *)dst;
  const uint8_t *in = (const uint8_t *)src;
  for (i = 0; i < size; ++i) {
    out[i] = in[i];
  }
  return dst;
}

void *kmemmove(void *dst, const void *src, size_t size) {
  size_t i;
  uint8_t *out = (uint8_t *)dst;
  const uint8_t *in = (const uint8_t *)src;
  if (out < in) {
    for (i = 0; i < size; ++i) {
      out[i] = in[i];
    }
  } else if (out > in) {
    for (i = size; i != 0; --i) {
      out[i - 1] = in[i - 1];
    }
  }
  return dst;
}

void *kmemset(void *dst, int value, size_t size) {
  size_t i;
  uint8_t *out = (uint8_t *)dst;
  for (i = 0; i < size; ++i) {
    out[i] = (uint8_t)value;
  }
  return dst;
}

int kmemcmp(const void *lhs, const void *rhs, size_t size) {
  size_t i;
  const uint8_t *a = (const uint8_t *)lhs;
  const uint8_t *b = (const uint8_t *)rhs;
  for (i = 0; i < size; ++i) {
    if (a[i] != b[i]) {
      return (int)a[i] - (int)b[i];
    }
  }
  return 0;
}

static void append_char(char *buffer, size_t size, size_t *index, char c) {
  if (*index + 1 < size) {
    buffer[*index] = c;
  }
  ++(*index);
}

static void append_string(char *buffer, size_t size, size_t *index, const char *s) {
  if (!s) {
    s = "(null)";
  }
  while (*s) {
    append_char(buffer, size, index, *s++);
  }
}

static void append_unsigned(char *buffer, size_t size, size_t *index, uint64_t value, unsigned base, int uppercase) {
  char temp[32];
  size_t len = 0;
  const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
  if (value == 0) {
    append_char(buffer, size, index, '0');
    return;
  }
  while (value != 0) {
    temp[len++] = digits[value % base];
    value /= base;
  }
  while (len != 0) {
    append_char(buffer, size, index, temp[--len]);
  }
}

int kvsnprintf(char *buffer, size_t size, const char *fmt, va_list args) {
  size_t index = 0;
  const char *p;

  if (size == 0) {
    return 0;
  }

  for (p = fmt; *p; ++p) {
    if (*p != '%') {
      append_char(buffer, size, &index, *p);
      continue;
    }

    ++p;
    switch (*p) {
      case '%':
        append_char(buffer, size, &index, '%');
        break;
      case 'c':
        append_char(buffer, size, &index, (char)va_arg(args, int));
        break;
      case 's':
        append_string(buffer, size, &index, va_arg(args, const char *));
        break;
      case 'd':
      case 'i': {
        int64_t value = va_arg(args, int64_t);
        if (value < 0) {
          append_char(buffer, size, &index, '-');
          append_unsigned(buffer, size, &index, (uint64_t)(-value), 10, 0);
        } else {
          append_unsigned(buffer, size, &index, (uint64_t)value, 10, 0);
        }
        break;
      }
      case 'u':
        append_unsigned(buffer, size, &index, va_arg(args, uint64_t), 10, 0);
        break;
      case 'x':
        append_unsigned(buffer, size, &index, va_arg(args, uint64_t), 16, 0);
        break;
      case 'X':
        append_unsigned(buffer, size, &index, va_arg(args, uint64_t), 16, 1);
        break;
      case 'p':
        append_string(buffer, size, &index, "0x");
        append_unsigned(buffer, size, &index, (uint64_t)(uintptr_t)va_arg(args, void *), 16, 0);
        break;
      default:
        append_char(buffer, size, &index, '%');
        append_char(buffer, size, &index, *p);
        break;
    }
  }

  buffer[(index < size) ? index : size - 1] = '\0';
  return (int)index;
}

int ksnprintf(char *buffer, size_t size, const char *fmt, ...) {
  int written;
  va_list args;
  va_start(args, fmt);
  written = kvsnprintf(buffer, size, fmt, args);
  va_end(args);
  return written;
}

int kvformat_braces(char *buffer, size_t size, const char *pattern, va_list args) {
  size_t index = 0;
  const char *p = pattern;

  if (size == 0) {
    return 0;
  }

  while (p && *p) {
    if (p[0] == '{' && p[1] == '{') {
      append_char(buffer, size, &index, '{');
      p += 2;
      continue;
    }
    if (p[0] == '}' && p[1] == '}') {
      append_char(buffer, size, &index, '}');
      p += 2;
      continue;
    }
    if (p[0] == '{' && p[1] == '}') {
      const char *arg = va_arg(args, const char *);
      if (arg) {
        append_string(buffer, size, &index, arg);
      } else {
        append_char(buffer, size, &index, '{');
        append_char(buffer, size, &index, '}');
      }
      p += 2;
      continue;
    }
    append_char(buffer, size, &index, *p++);
  }

  buffer[(index < size) ? index : size - 1] = '\0';
  return (int)index;
}

int kformat_braces(char *buffer, size_t size, const char *pattern, ...) {
  int written;
  va_list args;
  va_start(args, pattern);
  written = kvformat_braces(buffer, size, pattern, args);
  va_end(args);
  return written;
}

void *memcpy(void *dst, const void *src, size_t size) { return kmemcpy(dst, src, size); }
void *memmove(void *dst, const void *src, size_t size) { return kmemmove(dst, src, size); }
void *memset(void *dst, int value, size_t size) { return kmemset(dst, value, size); }
int memcmp(const void *lhs, const void *rhs, size_t size) { return kmemcmp(lhs, rhs, size); }
size_t strlen(const char *s) { return kstrlen(s); }
