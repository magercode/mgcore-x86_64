#include "mgcore/console.h"
#include "mgcore/io.h"
#include "mgcore/kstring.h"

#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)
#define SERIAL_PORT 0x3F8
#define CONSOLE_INPUT_CAPACITY 128

static size_t g_row;
static size_t g_col;
static uint8_t g_color = 0x07;
static char g_input_buffer[CONSOLE_INPUT_CAPACITY];
static size_t g_input_read_index;
static size_t g_input_write_index;

static int input_buffer_is_empty(void) {
  return g_input_read_index == g_input_write_index;
}

static int input_buffer_is_full(void) {
  return ((g_input_write_index + 1U) % CONSOLE_INPUT_CAPACITY) == g_input_read_index;
}

static int input_buffer_pop(char *out) {
  if (!out || input_buffer_is_empty()) {
    return 0;
  }
  *out = g_input_buffer[g_input_read_index];
  g_input_read_index = (g_input_read_index + 1U) % CONSOLE_INPUT_CAPACITY;
  return 1;
}

static void serial_init(void) {
  io_out8(SERIAL_PORT + 1, 0x00);
  io_out8(SERIAL_PORT + 3, 0x80);
  io_out8(SERIAL_PORT + 0, 0x03);
  io_out8(SERIAL_PORT + 1, 0x00);
  io_out8(SERIAL_PORT + 3, 0x03);
  io_out8(SERIAL_PORT + 2, 0xC7);
  io_out8(SERIAL_PORT + 4, 0x0B);
}

static int serial_is_transmit_empty(void) {
  return (io_in8(SERIAL_PORT + 5) & 0x20) != 0;
}

static int serial_has_input(void) {
  return (io_in8(SERIAL_PORT + 5) & 0x01) != 0;
}

static void serial_putc(char c) {
  while (!serial_is_transmit_empty()) {
  }
  io_out8(SERIAL_PORT, (uint8_t)c);
}

static void vga_scroll(void) {
  size_t row;
  size_t col;
  for (row = 1; row < VGA_HEIGHT; ++row) {
    for (col = 0; col < VGA_WIDTH; ++col) {
      VGA_MEMORY[(row - 1) * VGA_WIDTH + col] = VGA_MEMORY[row * VGA_WIDTH + col];
    }
  }
  for (col = 0; col < VGA_WIDTH; ++col) {
    VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = ((uint16_t)g_color << 8) | ' ';
  }
  g_row = VGA_HEIGHT - 1;
}

static void vga_putc(char c) {
  if (c == '\n') {
    g_col = 0;
    ++g_row;
    if (g_row >= VGA_HEIGHT) {
      vga_scroll();
    }
    return;
  }

  VGA_MEMORY[g_row * VGA_WIDTH + g_col] = ((uint16_t)g_color << 8) | (uint8_t)c;
  ++g_col;
  if (g_col >= VGA_WIDTH) {
    g_col = 0;
    ++g_row;
    if (g_row >= VGA_HEIGHT) {
      vga_scroll();
    }
  }
}

void console_init(void) {
  serial_init();
  console_clear();
}

void console_clear(void) {
  size_t i;
  g_row = 0;
  g_col = 0;
  g_input_read_index = 0;
  g_input_write_index = 0;
  for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
    VGA_MEMORY[i] = ((uint16_t)g_color << 8) | ' ';
  }
}

void console_putc(char c) {
  if (c == '\n') {
    serial_putc('\r');
  }
  serial_putc(c);
  vga_putc(c);
}

void console_write(const char *s) {
  while (s && *s) {
    console_putc(*s++);
  }
}

void console_write_n(const char *s, size_t length) {
  size_t i;
  for (i = 0; i < length; ++i) {
    console_putc(s[i]);
  }
}

void console_writeln(const char *s) {
  console_write(s);
  console_putc('\n');
}

void console_vprintf(const char *fmt, va_list args) {
  char buffer[512];
  kvsnprintf(buffer, sizeof(buffer), fmt, args);
  console_write(buffer);
}

void console_printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  console_vprintf(fmt, args);
  va_end(args);
}

void console_enqueue_input(char c) {
  if (input_buffer_is_full()) {
    return;
  }
  g_input_buffer[g_input_write_index] = c;
  g_input_write_index = (g_input_write_index + 1U) % CONSOLE_INPUT_CAPACITY;
}

int console_try_read_char(char *out) {
  if (!out) {
    return 0;
  }

  if (serial_has_input()) {
    console_enqueue_input((char)io_in8(SERIAL_PORT));
  }

  return input_buffer_pop(out);
}
