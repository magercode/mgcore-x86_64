#include "sys/syscall.h"

int errno;

static intptr_t normalize_result(intptr_t result) {
  if (result < 0 && result > -4096) {
    errno = (int)(-result);
    return -1;
  }
  return result;
}

intptr_t __syscall0(intptr_t nr) {
  intptr_t ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr) : "rcx", "r11", "memory");
  return normalize_result(ret);
}

intptr_t __syscall1(intptr_t nr, intptr_t a0) {
  intptr_t ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a0) : "rcx", "r11", "memory");
  return normalize_result(ret);
}

intptr_t __syscall2(intptr_t nr, intptr_t a0, intptr_t a1) {
  intptr_t ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a0), "S"(a1) : "rcx", "r11", "memory");
  return normalize_result(ret);
}

intptr_t __syscall3(intptr_t nr, intptr_t a0, intptr_t a1, intptr_t a2) {
  intptr_t ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a0), "S"(a1), "d"(a2) : "rcx", "r11", "memory");
  return normalize_result(ret);
}

intptr_t __syscall6(intptr_t nr, intptr_t a0, intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4, intptr_t a5) {
  intptr_t ret;
  register intptr_t r10 __asm__("r10") = a3;
  register intptr_t r8 __asm__("r8") = a4;
  register intptr_t r9 __asm__("r9") = a5;
  __asm__ volatile ("syscall"
                    : "=a"(ret)
                    : "a"(nr), "D"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8), "r"(r9)
                    : "rcx", "r11", "memory");
  return normalize_result(ret);
}
