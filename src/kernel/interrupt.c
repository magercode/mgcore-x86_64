#include "mgcore/console.h"
#include "mgcore/interrupt.h"
#include "mgcore/io.h"
#include "mgcore/keyboard.h"
#include "mgcore/kernel.h"
#include "mgcore/mouse.h"
#include "mgcore/net.h"
#include "mgcore/sched.h"

#include <stddef.h>
#include <stdint.h>

struct idt_entry {
  uint16_t offset_low;
  uint16_t selector;
  uint8_t ist;
  uint8_t type_attr;
  uint16_t offset_mid;
  uint32_t offset_high;
  uint32_t zero;
} __attribute__((packed));

struct idt_pointer {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed));

extern void *isr_stub_table[];

static struct idt_entry g_idt[256];
static uint64_t g_timer_ticks;

static void idt_set_gate(uint8_t vector, void *handler, uint8_t type_attr) {
  uintptr_t address = (uintptr_t)handler;
  g_idt[vector].offset_low = (uint16_t)(address & 0xFFFF);
  g_idt[vector].selector = 0x08;
  g_idt[vector].ist = 0;
  g_idt[vector].type_attr = type_attr;
  g_idt[vector].offset_mid = (uint16_t)((address >> 16) & 0xFFFF);
  g_idt[vector].offset_high = (uint32_t)((address >> 32) & 0xFFFFFFFF);
  g_idt[vector].zero = 0;
}

static void pic_remap(void) {
  io_out8(0x20, 0x11);
  io_wait();
  io_out8(0xA0, 0x11);
  io_wait();
  io_out8(0x21, 0x20);
  io_wait();
  io_out8(0xA1, 0x28);
  io_wait();
  io_out8(0x21, 0x04);
  io_wait();
  io_out8(0xA1, 0x02);
  io_wait();
  io_out8(0x21, 0x01);
  io_wait();
  io_out8(0xA1, 0x01);
  io_wait();
  /* Unmask IRQ0(timer), IRQ1(keyboard), IRQ2(cascade) on master and IRQ11/IRQ12 on slave. */
  io_out8(0x21, 0xF8);
  io_out8(0xA1, 0xE7);
}

static void pit_program(uint32_t hz) {
  uint16_t divisor = (uint16_t)(1193182U / hz);
  io_out8(0x43, 0x36);
  io_out8(0x40, (uint8_t)(divisor & 0xFF));
  io_out8(0x40, (uint8_t)(divisor >> 8));
}

static void pic_send_eoi(uint8_t vector) {
  if (vector >= 40) {
    io_out8(0xA0, 0x20);
  }
  io_out8(0x20, 0x20);
}

void interrupt_init(void) {
  struct idt_pointer idtr;
  size_t i;

  for (i = 0; i < 45; ++i) {
    idt_set_gate((uint8_t)i, isr_stub_table[i], 0x8E);
  }

  idtr.limit = sizeof(g_idt) - 1;
  idtr.base = (uint64_t)(uintptr_t)&g_idt[0];
  __asm__ volatile ("lidt %0" : : "m"(idtr));

  pic_remap();
  pit_program(MGCORE_HZ);
}

void interrupt_enable(void) {
  __asm__ volatile ("sti");
}

void interrupt_disable(void) {
  __asm__ volatile ("cli");
}

void interrupt_dispatch(struct interrupt_frame *frame) {
  if (frame->vector == 32) {
    ++g_timer_ticks;
    scheduler_tick();
    pic_send_eoi((uint8_t)frame->vector);
    return;
  }

  if (frame->vector == 33) {
    keyboard_handle_irq();
    pic_send_eoi((uint8_t)frame->vector);
    return;
  }

  if (frame->vector == 43) {
    net_handle_irq();
    pic_send_eoi((uint8_t)frame->vector);
    return;
  }

  if (frame->vector == 44) {
    mouse_handle_irq();
    pic_send_eoi((uint8_t)frame->vector);
    return;
  }

  if (frame->vector < 32) {
    console_printf("exception %u err=%u rip=%p\n",
                   (unsigned)frame->vector,
                   (unsigned)frame->error_code,
                   (void *)frame->rip);
    kernel_panic("unhandled processor exception");
  }
}

uint64_t timer_ticks(void) {
  return g_timer_ticks;
}
