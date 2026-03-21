#include "mgcore/cli.h"
#include "mgcore/commands.h"
#include "mgcore/console.h"
#include "mgcore/kstring.h"

static char g_line[CLI_MAX_LINE];

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
  console_writeln("monitor: input serial/keyboard ready, type 'help'");
  console_writeln("GitHub: github.com/magercode'");
  console_writeln("Tele: t.me/xigmachat'");
}

void cli_run(void) {
  size_t length = 0;
  console_write("[mgcore-x86_64]_# ");
  for (;;) {
    char c;
    if (!console_try_read_char(&c)) {
      __asm__ volatile ("hlt");
      continue;
    }

    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      g_line[length] = '\0';
      console_putc('\n');
      dispatch_line(g_line);
      length = 0;
      console_write("[mgcore-x86_64]_# ");
      continue;
    }
    if ((c == '\b' || c == 0x7F) && length > 0) {
      --length;
      console_write("\b \b");
      continue;
    }
    if (length + 1 < sizeof(g_line)) {
      g_line[length++] = c;
      console_putc(c);
    }
  }
}
