#include "mgcore/console.h"
#include "mgcore/io.h"
#include "mgcore/kstring.h"

#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)
#define VGA_CRTC_INDEX_PORT 0x3D4
#define VGA_CRTC_DATA_PORT 0x3D5
#define SERIAL_PORT 0x3F8
#define CONSOLE_INPUT_CAPACITY 128
#define CONSOLE_HISTORY_LINES 512

struct rgb_color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

static const struct rgb_color k_vga_palette[16] = {
  {0x00, 0x00, 0x00}, {0x00, 0x00, 0xAA}, {0x00, 0xAA, 0x00}, {0x00, 0xAA, 0xAA},
  {0xAA, 0x00, 0x00}, {0xAA, 0x00, 0xAA}, {0xAA, 0x55, 0x00}, {0xAA, 0xAA, 0xAA},
  {0x55, 0x55, 0x55}, {0x55, 0x55, 0xFF}, {0x55, 0xFF, 0x55}, {0x55, 0xFF, 0xFF},
  {0xFF, 0x55, 0x55}, {0xFF, 0x55, 0xFF}, {0xFF, 0xFF, 0x55}, {0xFF, 0xFF, 0xFF},
};

static uint16_t g_history[CONSOLE_HISTORY_LINES][VGA_WIDTH];
static size_t g_abs_row;
static size_t g_col;
static size_t g_view_start;
static int g_follow_tail;
static uint8_t g_attr;
static struct rgb_color g_fg;
static struct rgb_color g_bg;

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

static void serial_write(const char *s) {
  while (s && *s) {
    serial_putc(*s++);
  }
}

static void console_write_plain(const char *s) {
  while (s && *s) {
    console_putc(*s++);
  }
}

static size_t history_min_abs_row(void) {
  return (g_abs_row + 1U > CONSOLE_HISTORY_LINES) ? (g_abs_row + 1U - CONSOLE_HISTORY_LINES) : 0U;
}

static size_t viewport_max_start(void) {
  return (g_abs_row >= (VGA_HEIGHT - 1U)) ? (g_abs_row - (VGA_HEIGHT - 1U)) : 0U;
}

static void history_clear_row(size_t abs_row) {
  size_t col;
  size_t slot = abs_row % CONSOLE_HISTORY_LINES;
  for (col = 0; col < VGA_WIDTH; ++col) {
    g_history[slot][col] = ((uint16_t)g_attr << 8) | ' ';
  }
}

static uint16_t *history_cell(size_t abs_row, size_t col) {
  return &g_history[abs_row % CONSOLE_HISTORY_LINES][col];
}

static uint8_t color_nearest_vga_index(struct rgb_color c) {
  uint8_t best = 0;
  uint32_t best_dist = 0xFFFFFFFFU;
  uint8_t i;
  for (i = 0; i < 16; ++i) {
    int dr = (int)c.r - (int)k_vga_palette[i].r;
    int dg = (int)c.g - (int)k_vga_palette[i].g;
    int db = (int)c.b - (int)k_vga_palette[i].b;
    uint32_t dist = (uint32_t)(dr * dr + dg * dg + db * db);
    if (dist < best_dist) {
      best_dist = dist;
      best = i;
    }
  }
  return best;
}

static void console_update_attr(void) {
  uint8_t fg = color_nearest_vga_index(g_fg);
  uint8_t bg = color_nearest_vga_index(g_bg);
  g_attr = (uint8_t)((bg << 4) | fg);
}

static void serial_emit_ansi_color(void) {
  char buffer[96];
  ksnprintf(buffer, sizeof(buffer),
            "\x1b[38;2;%u;%u;%um\x1b[48;2;%u;%u;%um",
            (unsigned)g_fg.r, (unsigned)g_fg.g, (unsigned)g_fg.b,
            (unsigned)g_bg.r, (unsigned)g_bg.g, (unsigned)g_bg.b);
  serial_write(buffer);
}

static void vga_update_hw_cursor(void) {
  size_t cursor_row = VGA_HEIGHT - 1U;
  size_t cursor_col = VGA_WIDTH - 1U;
  if (g_abs_row >= g_view_start && g_abs_row < g_view_start + VGA_HEIGHT) {
    cursor_row = g_abs_row - g_view_start;
    cursor_col = g_col;
  }

  {
    uint16_t position = (uint16_t)(cursor_row * VGA_WIDTH + cursor_col);
    io_out8(VGA_CRTC_INDEX_PORT, 0x0F);
    io_out8(VGA_CRTC_DATA_PORT, (uint8_t)(position & 0xFF));
    io_out8(VGA_CRTC_INDEX_PORT, 0x0E);
    io_out8(VGA_CRTC_DATA_PORT, (uint8_t)((position >> 8) & 0xFF));
  }
}

static void vga_render_view(void) {
  size_t row;
  size_t min_row = history_min_abs_row();

  for (row = 0; row < VGA_HEIGHT; ++row) {
    size_t col;
    size_t abs_row = g_view_start + row;
    if (abs_row < min_row || abs_row > g_abs_row) {
      for (col = 0; col < VGA_WIDTH; ++col) {
        VGA_MEMORY[row * VGA_WIDTH + col] = ((uint16_t)g_attr << 8) | ' ';
      }
    } else {
      size_t slot = abs_row % CONSOLE_HISTORY_LINES;
      for (col = 0; col < VGA_WIDTH; ++col) {
        VGA_MEMORY[row * VGA_WIDTH + col] = g_history[slot][col];
      }
    }
  }

  vga_update_hw_cursor();
}

static void viewport_clamp(void) {
  size_t min_start = history_min_abs_row();
  size_t max_start = viewport_max_start();
  if (g_view_start < min_start) {
    g_view_start = min_start;
  }
  if (g_view_start > max_start) {
    g_view_start = max_start;
  }
}

static void viewport_after_output(void) {
  if (g_follow_tail) {
    g_view_start = viewport_max_start();
  } else {
    viewport_clamp();
  }
}

static void advance_new_line(void) {
  ++g_abs_row;
  g_col = 0;
  history_clear_row(g_abs_row);
}

static void console_write_visible_char(char c) {
  *history_cell(g_abs_row, g_col) = ((uint16_t)g_attr << 8) | (uint8_t)c;
  ++g_col;
  if (g_col >= VGA_WIDTH) {
    advance_new_line();
  }
}

void console_init(void) {
  serial_init();
  console_clear();
}

void console_clear(void) {
  size_t i;
  g_abs_row = 0;
  g_col = 0;
  g_view_start = 0;
  g_follow_tail = 1;
  g_fg = (struct rgb_color){0xC0, 0xC0, 0xC0};
  g_bg = (struct rgb_color){0x00, 0x00, 0x00};
  console_update_attr();
  g_input_read_index = 0;
  g_input_write_index = 0;

  for (i = 0; i < CONSOLE_HISTORY_LINES; ++i) {
    history_clear_row(i);
  }
  history_clear_row(0);
  serial_write("\x1b[0m");
  serial_emit_ansi_color();
  vga_render_view();
}

void console_putc(char c) {
  if (c == '\n') {
    serial_putc('\r');
  }
  serial_putc(c);

  if (c == '\r') {
    g_col = 0;
  } else if (c == '\n') {
    advance_new_line();
  } else if (c == '\b') {
    if (g_col > 0) {
      --g_col;
      *history_cell(g_abs_row, g_col) = ((uint16_t)g_attr << 8) | ' ';
    } else if (g_abs_row > 0) {
      --g_abs_row;
      g_col = VGA_WIDTH - 1U;
      *history_cell(g_abs_row, g_col) = ((uint16_t)g_attr << 8) | ' ';
    }
  } else if (c == '\t') {
    size_t spaces = 4U - (g_col % 4U);
    size_t i;
    for (i = 0; i < spaces; ++i) {
      console_write_visible_char(' ');
    }
  } else if ((uint8_t)c >= 0x20U) {
    console_write_visible_char(c);
  }

  viewport_after_output();
  vga_render_view();
}

static void console_write_braces_v(const char *pattern, va_list args) {
  char buffer[1024];
  kvformat_braces(buffer, sizeof(buffer), pattern, args);
  console_write_plain(buffer);
}

void console_write(const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  console_write_braces_v(pattern, args);
  va_end(args);
}

void console_writef(const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  console_write_braces_v(pattern, args);
  va_end(args);
}

void console_write_n(const char *s, size_t length) {
  size_t i;
  for (i = 0; i < length; ++i) {
    console_putc(s[i]);
  }
}

void console_writeln(const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  console_write_braces_v(pattern, args);
  va_end(args);
  console_putc('\n');
}

void console_writelnf(const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  console_write_braces_v(pattern, args);
  va_end(args);
  console_putc('\n');
}

void console_set_color_rgb(uint8_t fr, uint8_t fg, uint8_t fb,
                           uint8_t br, uint8_t bg, uint8_t bb) {
  g_fg = (struct rgb_color){fr, fg, fb};
  g_bg = (struct rgb_color){br, bg, bb};
  console_update_attr();
  serial_emit_ansi_color();
}

void console_set_color_hex(uint32_t fg_hex, uint32_t bg_hex) {
  console_set_color_rgb((uint8_t)((fg_hex >> 16) & 0xFF),
                        (uint8_t)((fg_hex >> 8) & 0xFF),
                        (uint8_t)(fg_hex & 0xFF),
                        (uint8_t)((bg_hex >> 16) & 0xFF),
                        (uint8_t)((bg_hex >> 8) & 0xFF),
                        (uint8_t)(bg_hex & 0xFF));
}

void console_reset_color(void) {
  console_set_color_hex(0xC0C0C0U, 0x000000U);
}

void console_scroll_view(int delta_lines) {
  size_t min_start;
  size_t max_start;

  if (delta_lines == 0) {
    return;
  }

  min_start = history_min_abs_row();
  max_start = viewport_max_start();
  if (delta_lines > 0) {
    size_t delta = (size_t)delta_lines;
    if (delta > g_view_start - min_start) {
      g_view_start = min_start;
    } else {
      g_view_start -= delta;
    }
  } else {
    size_t delta = (size_t)(-delta_lines);
    if (g_view_start + delta > max_start) {
      g_view_start = max_start;
    } else {
      g_view_start += delta;
    }
  }

  g_follow_tail = (g_view_start == max_start);
  vga_render_view();
}

void console_view_to_bottom(void) {
  g_view_start = viewport_max_start();
  g_follow_tail = 1;
  vga_render_view();
}

void console_vprintf(const char *fmt, va_list args) {
  char buffer[512];
  kvsnprintf(buffer, sizeof(buffer), fmt, args);
  console_write_plain(buffer);
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
