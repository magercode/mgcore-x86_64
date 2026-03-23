#include "mgcore/commands.h"
#include "mgcore/console.h"
#include "mgcore/fs.h"
#include "mgcore/kernel.h"
#include "mgcore/net.h"
#include "mgcore/pmm.h"
#include "mgcore/safeboot.h"
#include "mgcore/sched.h"
#include "mgcore/swap.h"
#include "mgcore/task.h"
#include "mgcore/types.h"
#include "mgcore/kstring.h"

static int parse_u32(const char *text, size_t *value_out) {
  size_t value = 0;
  const char *p = text;
  if (!text || !*text || !value_out) {
    return 0;
  }
  while (*p) {
    char c = *p++;
    if (c < '0' || c > '9') {
      return 0;
    }
    value = (value * 10U) + (size_t)(c - '0');
  }
  *value_out = value;
  return 1;
}

static void print_mem_line(const char *label, size_t pages) {
  size_t kib = pages * (PMM_PAGE_SIZE / 1024U);
  size_t mib_whole = kib / 1024U;
  size_t mib_frac = (kib % 1024U) * 100U / 1024U;
  console_printf("%s: %u pages (%u KiB, %u.%u MiB)\n",
                 label,
                 (unsigned)pages,
                 (unsigned)kib,
                 (unsigned)mib_whole,
                 (unsigned)mib_frac);
}

static int cmd_help(int argc, char **argv) {
  int count;
  int i;
  const command_def *commands;
  (void)argc;
  (void)argv;
  commands = commands_get_all(&count);
  for (i = 0; i < count; ++i) {
    console_printf("%s - %s\n", commands[i].name, commands[i].help);
  }
  return 0;
}

static int cmd_clear(int argc, char **argv) {
  (void)argc;
  (void)argv;
  console_clear();
  return 0;
}

static int cmd_echo(int argc, char **argv) {
  int i;
  for (i = 1; i < argc; ++i) {
    console_write(argv[i]);
    if (i + 1 < argc) {
      console_putc(' ');
    }
  }
  console_putc('\n');
  return 0;
}

static int cmd_sysinfo(int argc, char **argv) {
  (void)argc;
  (void)argv;
  scheduler_dump();
  return 0;
}

static int cmd_tasks(int argc, char **argv) {
  (void)argc;
  (void)argv;
  task_dump();
  return 0;
}

static int cmd_fsls(int argc, char **argv) {
  (void)argc;
  (void)argv;
  fs_dump_root();
  return 0;
}

static int cmd_meminfo(int argc, char **argv) {
  size_t ram_total = pmm_total_pages();
  size_t ram_used = pmm_used_pages();
  size_t ram_free = pmm_free_pages();
  size_t swap_total = swap_total_pages();
  size_t swap_used = swap_used_pages();
  size_t swap_free = swap_free_pages();
  (void)argc;
  (void)argv;

  console_writeln("memory overview:");
  print_mem_line("  ram.total", ram_total);
  print_mem_line("  ram.used ", ram_used);
  print_mem_line("  ram.free ", ram_free);
  console_printf("  swap.state: %s\n", swap_is_enabled() ? "on" : "off");
  print_mem_line("  swap.total", swap_total);
  print_mem_line("  swap.used ", swap_used);
  print_mem_line("  swap.free ", swap_free);
  return 0;
}

static void swap_print_usage(void) {
  console_writeln("usage: swap status | on [pages] | off | use <pages>");
}

static int cmd_swap(int argc, char **argv) {
  size_t pages;
  int rc;
  if (argc < 2) {
    swap_print_usage();
    return -1;
  }

  if (kstrcmp(argv[1], "status") == 0) {
    console_printf("swap: state=%s total=%u used=%u free=%u pages\n",
                   swap_is_enabled() ? "on" : "off",
                   (unsigned)swap_total_pages(),
                   (unsigned)swap_used_pages(),
                   (unsigned)swap_free_pages());
    return 0;
  }

  if (kstrcmp(argv[1], "on") == 0) {
    pages = 0;
    if (argc >= 3 && !parse_u32(argv[2], &pages)) {
      console_writeln("swap: invalid page count");
      return -1;
    }
    rc = swap_enable(pages);
    if (rc != 0) {
      console_writeln("swap: enable failed (set pages > 0)");
      return -1;
    }
    console_printf("swap: enabled total=%u pages\n", (unsigned)swap_total_pages());
    return 0;
  }

  if (kstrcmp(argv[1], "off") == 0) {
    swap_disable();
    console_writeln("swap: disabled");
    return 0;
  }

  if (kstrcmp(argv[1], "use") == 0) {
    if (argc < 3 || !parse_u32(argv[2], &pages)) {
      console_writeln("swap: invalid page count");
      return -1;
    }
    rc = swap_set_used_pages(pages);
    if (rc == -1) {
      console_writeln("swap: currently off (use 'swap on' first)");
      return -1;
    }
    if (rc == -2) {
      console_writeln("swap: usage exceeds configured capacity");
      return -1;
    }
    console_printf("swap: used=%u free=%u pages\n",
                   (unsigned)swap_used_pages(),
                   (unsigned)swap_free_pages());
    return 0;
  }

  swap_print_usage();
  return -1;
}

static int cmd_eth(int argc, char **argv) {
  (void)argc;
  (void)argv;
  net_dump_adapters();
  return 0;
}

static int cmd_nearby(int argc, char **argv) {
  (void)argc;
  (void)argv;
  net_scan_nearby();
  net_dump_nearby();
  return 0;
}

static int cmd_connect(int argc, char **argv) {
  int rc;
  if (argc < 2) {
    console_writeln("usage: connect <net0|default|up>");
    return -1;
  }
  rc = net_connect_target(argv[1]);
  if (rc == -2) {
    console_writeln("connect: no ethernet adapter detected");
    return -1;
  }
  if (rc == -3) {
    console_writeln("connect: gateway unresolved, run 'mgctl net scan' and retry");
    return -1;
  }
  if (rc == -4) {
    console_writeln("connect: invalid target (use net0/default/up)");
    return -1;
  }
  if (rc != 0) {
    console_writeln("connect: invalid target");
    return -1;
  }
  console_printf("connect: connected to %s\n", net_connected_name());
  return 0;
}

static int cmd_ping(int argc, char **argv) {
  size_t count = 4;
  if (argc < 2) {
    console_writeln("usage: ping <target> [count]");
    return -1;
  }
  if (argc >= 3 && !parse_u32(argv[2], &count)) {
    console_writeln("ping: invalid count");
    return -1;
  }
  return net_ping(argv[1], count);
}

static void mgctl_usage(void) {
  console_writeln("mgctl usage:");
  console_writeln("  mgctl status");
  console_writeln("  mgctl net tools|adapters|scan|nearby|connect <net0|default|up>");
  console_writeln("  mgctl power shutdown|reboot");
  console_writeln("  mgctl power safe status|reboot|shutdown");
}

static int cmd_mgctl(int argc, char **argv) {
  if (argc < 2) {
    mgctl_usage();
    return -1;
  }

  if (kstrcmp(argv[1], "status") == 0) {
    console_printf("mgctl: adapters=%u connected=%s safeboot=%s\n",
                   (unsigned)net_adapter_count(),
                   net_connected_name() ? net_connected_name() : "none",
                   safeboot_get_action() == SAFEBOOT_ACTION_SHUTDOWN ? "shutdown" : "reboot");
    return 0;
  }

  if (kstrcmp(argv[1], "net") == 0) {
    if (argc < 3) {
      mgctl_usage();
      return -1;
    }
    if (kstrcmp(argv[2], "tools") == 0) {
      net_dump_driver_tools();
      return 0;
    }
    if (kstrcmp(argv[2], "adapters") == 0) {
      net_dump_adapters();
      return 0;
    }
    if (kstrcmp(argv[2], "scan") == 0) {
      net_scan_nearby();
      net_dump_nearby();
      return 0;
    }
    if (kstrcmp(argv[2], "nearby") == 0) {
      net_dump_nearby();
      return 0;
    }
    if (kstrcmp(argv[2], "connect") == 0) {
      if (argc < 4) {
        console_writeln("mgctl net connect: target required (net0/default/up)");
        return -1;
      }
      return cmd_connect(2, &argv[2]);
    }
    mgctl_usage();
    return -1;
  }

  if (kstrcmp(argv[1], "power") == 0) {
    if (argc < 3) {
      mgctl_usage();
      return -1;
    }
    if (kstrcmp(argv[2], "safe") == 0) {
      if (argc < 4) {
        mgctl_usage();
        return -1;
      }
      if (kstrcmp(argv[3], "status") == 0) {
        console_printf("mgctl: safe boot policy=%s\n",
                       safeboot_get_action() == SAFEBOOT_ACTION_SHUTDOWN ? "shutdown" : "reboot");
        return 0;
      }
      if (kstrcmp(argv[3], "reboot") == 0) {
        safeboot_set_action(SAFEBOOT_ACTION_REBOOT);
        console_writeln("mgctl: safe boot policy set to reboot");
        return 0;
      }
      if (kstrcmp(argv[3], "shutdown") == 0) {
        safeboot_set_action(SAFEBOOT_ACTION_SHUTDOWN);
        console_writeln("mgctl: safe boot policy set to shutdown");
        return 0;
      }
      mgctl_usage();
      return -1;
    }
    if (kstrcmp(argv[2], "shutdown") == 0) {
      console_writeln("mgctl: shutdown requested");
      kernel_halt();
    }
    if (kstrcmp(argv[2], "reboot") == 0) {
      console_writeln("mgctl: reboot requested");
      kernel_reboot();
    }
    mgctl_usage();
    return -1;
  }

  mgctl_usage();
  return -1;
}

static int cmd_shutdown(int argc, char **argv) {
  (void)argc;
  (void)argv;
  console_writeln("shutdown: powering down (halt)");
  kernel_halt();
}

static int cmd_reboot(int argc, char **argv) {
  (void)argc;
  (void)argv;
  console_writeln("reboot: restarting system");
  kernel_reboot();
}

static const command_def k_commands[] = {
  {"help", "list built-in monitor commands", cmd_help},
  {"clear", "clear VGA/serial console", cmd_clear},
  {"echo", "echo arguments", cmd_echo},
  {"sysinfo", "show scheduler uptime", cmd_sysinfo},
  {"tasks", "show current task table", cmd_tasks},
  {"fsls", "list root tmpfs entries", cmd_fsls},
  {"meminfo", "show physical memory and swap usage", cmd_meminfo},
  {"swap", "manage swap: status|on [pages]|off|use <pages>", cmd_swap},
  {"eth", "show detected ethernet controllers", cmd_eth},
  {"nearby", "scan and show live network status", cmd_nearby},
  {"connect", "bring up net0: connect <net0|default|up>", cmd_connect},
  {"ping", "ping <ipv4> [count]", cmd_ping},
  {"mgctl", "system utility: net/power status and control", cmd_mgctl},
  {"shutdown", "halt the machine", cmd_shutdown},
  {"reboot", "reboot the machine", cmd_reboot},
};

const command_def *commands_get_all(int *count_out) {
  if (count_out) {
    *count_out = (int)MGCORE_ARRAY_SIZE(k_commands);
  }
  return k_commands;
}
