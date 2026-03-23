#include "mgcore/cli.h"
#include "mgcore/commands.h"
#include "mgcore/console.h"
#include "mgcore/input.h"
#include "mgcore/kstring.h"

static char g_line[CLI_MAX_LINE];
static char g_last_line[CLI_MAX_LINE];
static int g_has_last_line;

static const char *k_prompt = "[mgcore-x86_64]_# ";

static void print_prompt(void) {
  console_set_color_hex(0x57D6FFU, 0x000000U);
  console_write("[mgcore-x86_64]");
  console_set_color_hex(0xFFD166U, 0x000000U);
  console_write("_# ");
  console_reset_color();
}

static void render_line(size_t length, size_t cursor) {
  console_putc('\r');
  print_prompt();
  console_write_n(g_line, length);
  console_putc(' ');
  console_putc('\r');
  print_prompt();
  console_write_n(g_line, cursor);
}

static int dispatch_line(char *line) {
  char *argv[CLI_MAX_ARGS];
  int argc = 0;
  int command_count = 0;
  const command_def *commands = commands_get_all(&command_count);
  char *cursor = line;
  int i;

  while (*cursor && argc < CLI_MAX_ARGS) {
    while (*cursor == ' ' || *cursor == '\t') {
      ++cursor;
    }
    if (*cursor == '\0') {
      break;
    }
    argv[argc++] = cursor;
    while (*cursor && *cursor != ' ' && *cursor != '\t') {
      ++cursor;
    }
    if (*cursor) {
      *cursor++ = '\0';
    }
  }

  if (argc == 0) {
    return 0;
  }

  for (i = 0; i < command_count; ++i) {
    if (kstrcmp(commands[i].name, argv[0]) == 0) {
      return commands[i].fn(argc, argv);
    }
  }

  console_printf("unknown command: %s\n", argv[0]);
  return -1;
}

void cli_init(void) {
  console_set_color_hex(0xFFFFFFU, 0x000000U);
  console_writeln("");
  console_set_color_hex(0x6EE7B7U, 0x000000U);
  console_writeln("           _______       ___________________________________");
  console_set_color_hex(0xFDE68AU, 0x000000U);
  console_writeln("          /  >    f     Github      : github.com/magercode");
  console_writeln("         |  _   _l      Telegram    : t.me/xigmachat");
  console_set_color_hex(0x93C5FDU, 0x000000U);
  console_writeln("        /` m__x_/");
  console_writeln("       /         |");
  console_writeln("      /   /     /");
  console_writeln("     |    | |  |");
  console_writeln("  /~~~|    | |  |");
  console_writeln("  | (~~\\___\\_)__)");
  console_writeln("  \\___/");
  console_writeln("");
  console_set_color_hex(0x57D6FFU, 0x000000U);
  console_writef("{} {}", "MGCore", "x86_64");
  console_putc('\n');
  console_writeln("");
  console_reset_color();
}

void cli_run(void) {
  size_t length = 0;
  size_t cursor = 0;
  int esc_state = 0;
  (void)k_prompt;
  print_prompt();
  for (;;) {
    char raw;
    unsigned char c;
    if (!console_try_read_char(&raw)) {
      __asm__ volatile ("hlt");
      continue;
    }
    c = (unsigned char)raw;

    if (esc_state == 1) {
      if (c == '[') {
        esc_state = 2;
        continue;
      }
      length = 0;
      cursor = 0;
      g_line[0] = '\0';
      render_line(length, cursor);
      esc_state = 0;
      continue;
    }

    if (esc_state == 2) {
      if (c == 'A') {
        c = INPUT_KEY_CTRL_P;
      } else if (c == 'B') {
        c = INPUT_KEY_CTRL_N;
      } else if (c == 'C') {
        c = INPUT_KEY_CTRL_F;
      } else if (c == 'D') {
        c = INPUT_KEY_CTRL_B;
      }
      esc_state = 0;
    }

    if (c == '\r') {
      continue;
    }
    if (c == INPUT_KEY_ESC) {
      esc_state = 1;
      continue;
    }
    if (c == '\n') {
      g_line[length] = '\0';
      console_putc('\n');
      dispatch_line(g_line);
      if (length > 0) {
        kstrncpy(g_last_line, g_line, sizeof(g_last_line) - 1U);
        g_last_line[sizeof(g_last_line) - 1U] = '\0';
        g_has_last_line = 1;
      }
      length = 0;
      cursor = 0;
      g_line[0] = '\0';
      print_prompt();
      continue;
    }
    if (c == INPUT_KEY_CTRL_C) {
      console_write("^C\n");
      length = 0;
      cursor = 0;
      g_line[0] = '\0';
      print_prompt();
      continue;
    }
    if (c == INPUT_KEY_CTRL_A) {
      cursor = 0;
      render_line(length, cursor);
      continue;
    }
    if (c == INPUT_KEY_CTRL_E) {
      cursor = length;
      render_line(length, cursor);
      continue;
    }
    if (c == INPUT_KEY_CTRL_B) {
      if (cursor > 0) {
        --cursor;
        render_line(length, cursor);
      }
      continue;
    }
    if (c == INPUT_KEY_CTRL_F) {
      if (cursor < length) {
        ++cursor;
        render_line(length, cursor);
      }
      continue;
    }
    if (c == INPUT_KEY_CTRL_P) {
      if (g_has_last_line) {
        kstrncpy(g_line, g_last_line, sizeof(g_line) - 1U);
        g_line[sizeof(g_line) - 1U] = '\0';
        length = kstrlen(g_line);
        cursor = length;
        render_line(length, cursor);
      }
      continue;
    }
    if (c == INPUT_KEY_CTRL_N) {
      length = 0;
      cursor = 0;
      g_line[0] = '\0';
      render_line(length, cursor);
      continue;
    }
    if ((c == '\b' || c == 0x7F) && length > 0) {
      size_t i;
      if (cursor == 0) {
        continue;
      }
      for (i = cursor - 1; i + 1 < length; ++i) {
        g_line[i] = g_line[i + 1];
      }
      --length;
      --cursor;
      g_line[length] = '\0';
      render_line(length, cursor);
      continue;
    }
    if (length + 1 < sizeof(g_line)) {
      size_t i;
      for (i = length; i > cursor; --i) {
        g_line[i] = g_line[i - 1];
      }
      g_line[cursor] = (char)c;
      ++length;
      ++cursor;
      g_line[length] = '\0';
      render_line(length, cursor);
    }
  }
}
