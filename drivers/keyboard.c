#include "mgcore/keyboard.h"

#include "mgcore/console.h"
#include "mgcore/input.h"
#include "mgcore/io.h"

#include <stddef.h>
#include <stdint.h>

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_SCANCODE_RELEASE 0x80
#define PS2_SCANCODE_EXTENDED 0xE0
#define SCANCODE_LEFT_SHIFT 0x2A
#define SCANCODE_RIGHT_SHIFT 0x36
#define SCANCODE_CTRL 0x1D
#define SCANCODE_ESC 0x01
#define SCANCODE_ARROW_UP 0x48
#define SCANCODE_ARROW_DOWN 0x50
#define SCANCODE_ARROW_LEFT 0x4B
#define SCANCODE_ARROW_RIGHT 0x4D

static int g_shift_active;
static int g_ctrl_active;
static int g_extended_sequence;

struct keymap_entry {
  char normal;
  char shifted;
};

static const struct keymap_entry g_keymap[128] = {
  [0x02] = {'1', '!'},
  [0x03] = {'2', '@'},
  [0x04] = {'3', '#'},
  [0x05] = {'4', '$'},
  [0x06] = {'5', '%'},
  [0x07] = {'6', '^'},
  [0x08] = {'7', '&'},
  [0x09] = {'8', '*'},
  [0x0A] = {'9', '('},
  [0x0B] = {'0', ')'},
  [0x0C] = {'-', '_'},
  [0x0D] = {'=', '+'},
  [0x0E] = {'\b', '\b'},
  [0x0F] = {'\t', '\t'},
  [0x10] = {'q', 'Q'},
  [0x11] = {'w', 'W'},
  [0x12] = {'e', 'E'},
  [0x13] = {'r', 'R'},
  [0x14] = {'t', 'T'},
  [0x15] = {'y', 'Y'},
  [0x16] = {'u', 'U'},
  [0x17] = {'i', 'I'},
  [0x18] = {'o', 'O'},
  [0x19] = {'p', 'P'},
  [0x1A] = {'[', '{'},
  [0x1B] = {']', '}'},
  [0x1C] = {'\n', '\n'},
  [0x1E] = {'a', 'A'},
  [0x1F] = {'s', 'S'},
  [0x20] = {'d', 'D'},
  [0x21] = {'f', 'F'},
  [0x22] = {'g', 'G'},
  [0x23] = {'h', 'H'},
  [0x24] = {'j', 'J'},
  [0x25] = {'k', 'K'},
  [0x26] = {'l', 'L'},
  [0x27] = {';', ':'},
  [0x28] = {'\'', '"'},
  [0x29] = {'`', '~'},
  [0x2B] = {'\\', '|'},
  [0x2C] = {'z', 'Z'},
  [0x2D] = {'x', 'X'},
  [0x2E] = {'c', 'C'},
  [0x2F] = {'v', 'V'},
  [0x30] = {'b', 'B'},
  [0x31] = {'n', 'N'},
  [0x32] = {'m', 'M'},
  [0x33] = {',', '<'},
  [0x34] = {'.', '>'},
  [0x35] = {'/', '?'},
  [0x39] = {' ', ' '},
};

static char translate_scancode(uint8_t scancode) {
  struct keymap_entry entry;
  if (scancode >= sizeof(g_keymap) / sizeof(g_keymap[0])) {
    return '\0';
  }

  entry = g_keymap[scancode];
  if (entry.normal == '\0') {
    return '\0';
  }

  return g_shift_active ? entry.shifted : entry.normal;
}

static char translate_extended_scancode(uint8_t scancode) {
  switch (scancode) {
    case SCANCODE_ARROW_UP:
      return (char)INPUT_KEY_CTRL_P;
    case SCANCODE_ARROW_DOWN:
      return (char)INPUT_KEY_CTRL_N;
    case SCANCODE_ARROW_LEFT:
      return (char)INPUT_KEY_CTRL_B;
    case SCANCODE_ARROW_RIGHT:
      return (char)INPUT_KEY_CTRL_F;
    default:
      return '\0';
  }
}

static char apply_ctrl_translation(char translated) {
  if (!g_ctrl_active) {
    return translated;
  }

  if (translated >= 'a' && translated <= 'z') {
    return (char)(translated - 'a' + 1);
  }
  if (translated >= 'A' && translated <= 'Z') {
    return (char)(translated - 'A' + 1);
  }
  if (translated == '[') {
    return (char)INPUT_KEY_ESC;
  }
  return translated;
}

void keyboard_init(void) {
  g_shift_active = 0;
  g_ctrl_active = 0;
  g_extended_sequence = 0;
  while ((io_in8(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) != 0) {
    (void)io_in8(PS2_DATA_PORT);
  }
  console_writeln("keyboard: PS/2 input ready");
}

void keyboard_handle_irq(void) {
  uint8_t scancode;
  char translated;

  if ((io_in8(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) == 0) {
    return;
  }

  scancode = io_in8(PS2_DATA_PORT);
  if (scancode == PS2_SCANCODE_EXTENDED) {
    g_extended_sequence = 1;
    return;
  }

  if ((scancode & (uint8_t)PS2_SCANCODE_RELEASE) != 0) {
    scancode &= (uint8_t)~PS2_SCANCODE_RELEASE;
    if (scancode == SCANCODE_LEFT_SHIFT || scancode == SCANCODE_RIGHT_SHIFT) {
      g_shift_active = 0;
    } else if (scancode == SCANCODE_CTRL) {
      g_ctrl_active = 0;
    }
    if (g_extended_sequence) {
      g_extended_sequence = 0;
    }
    return;
  }

  if (scancode == SCANCODE_LEFT_SHIFT || scancode == SCANCODE_RIGHT_SHIFT) {
    g_shift_active = 1;
    return;
  }
  if (scancode == SCANCODE_CTRL) {
    g_ctrl_active = 1;
    if (g_extended_sequence) {
      g_extended_sequence = 0;
    }
    return;
  }

  if (g_extended_sequence) {
    translated = translate_extended_scancode(scancode);
    g_extended_sequence = 0;
  } else if (scancode == SCANCODE_ESC) {
    translated = (char)INPUT_KEY_ESC;
  } else {
    translated = translate_scancode(scancode);
    translated = apply_ctrl_translation(translated);
  }

  if (translated != '\0') {
    console_enqueue_input(translated);
  }
}
