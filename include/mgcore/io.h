#ifndef MGCORE_IO_H
#define MGCORE_IO_H

#include <stdint.h>

static inline void io_out8(uint16_t port, uint8_t value) {
  __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t io_in8(uint16_t port) {
  uint8_t value;
  __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
  return value;
}

static inline void io_wait(void) {
  io_out8(0x80, 0);
}

static inline void io_out16(uint16_t port, uint16_t value) {
  __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void io_out32(uint16_t port, uint32_t value) {
  __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint64_t read_cr3(void) {
  uint64_t value;
  __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
  return value;
}

static inline void write_cr3(uint64_t value) {
  __asm__ volatile ("mov %0, %%cr3" : : "r"(value) : "memory");
}

static inline void cpu_halt(void) {
  __asm__ volatile ("hlt");
}

#endif
