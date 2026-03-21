#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n) {
  size_t i;
  unsigned char *d = (unsigned char *)dst;
  const unsigned char *s = (const unsigned char *)src;
  for (i = 0; i < n; ++i) d[i] = s[i];
  return dst;
}

void *memset(void *dst, int c, size_t n) {
  size_t i;
  unsigned char *d = (unsigned char *)dst;
  for (i = 0; i < n; ++i) d[i] = (unsigned char)c;
  return dst;
}

size_t strlen(const char *s) {
  size_t n = 0;
  while (s[n]) ++n;
  return n;
}
