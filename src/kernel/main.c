#include "mgcore/cli.h"
#include "mgcore/console.h"
#include "mgcore/elf.h"
#include "mgcore/fs.h"
#include "mgcore/interrupt.h"
#include "mgcore/io.h"
#include "mgcore/kernel.h"
#include "mgcore/keyboard.h"
#include "mgcore/multiboot2.h"
#include "mgcore/mouse.h"
#include "mgcore/net.h"
#include "mgcore/pmm.h"
#include "mgcore/safeboot.h"
#include "mgcore/sched.h"
#include "mgcore/signal.h"
#include "mgcore/swap.h"
#include "mgcore/syscall.h"
#include "mgcore/task.h"
#include "mgcore/vmm.h"

extern uintptr_t __kernel_start;
extern uintptr_t __kernel_end;

static const struct multiboot2_tag_module *find_initramfs_module(const struct multiboot2_info *info) {
  const struct multiboot2_tag *tag;
  for (tag = multiboot2_first_tag(info); tag->type != MULTIBOOT2_TAG_END; tag = multiboot2_next_tag(tag)) {
    if (tag->type == MULTIBOOT2_TAG_MODULE) {
      return (const struct multiboot2_tag_module *)tag;
    }
  }
  return NULL;
}

uintptr_t kernel_image_start(void) {
  return (uintptr_t)&__kernel_start;
}

uintptr_t kernel_image_end(void) {
  return (uintptr_t)&__kernel_end;
}

void kernel_halt(void) {
  for (;;) {
    __asm__ volatile ("cli; hlt");
  }
}

void kernel_reboot(void) {
  int i;
  __asm__ volatile ("cli");
  for (i = 0; i < 100000; ++i) {
    if ((io_in8(0x64) & 0x02u) == 0) {
      break;
    }
  }
  io_out8(0x64, 0xFE);
  for (;;) {
    __asm__ volatile ("hlt");
  }
}

void kernel_panic(const char *message) {
  pid_t suspect = task_find_likely_memory_leaker();
  console_printf("\nPANIC: %s\n", message);
  safeboot_trigger("kernel panic", suspect);
}

void kernel_main(uint32_t multiboot_magic, uintptr_t multiboot_info_ptr) {
  const struct multiboot2_info *info = (const struct multiboot2_info *)multiboot_info_ptr;
  const struct multiboot2_tag_module *module;

  console_init();
  console_writeln("MGCORE x86_64 bootstrap");

  if (multiboot_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
    kernel_panic("invalid Multiboot2 magic");
  }

  console_printf("kernel image: %p - %p\n", (void *)kernel_image_start(), (void *)kernel_image_end());
  pmm_init(info, kernel_image_start(), kernel_image_end());
  swap_init(pmm_total_pages());
  vmm_init();
  task_init();
  scheduler_init();
  interrupt_init();
  keyboard_init();
  mouse_init();
  syscall_init();
  signal_init();
  fs_init();
  elf_init();
  net_init();

  module = find_initramfs_module(info);
  if (module) {
    int mount_status = fs_mount_initramfs((const void *)(uintptr_t)module->mod_start,
                                          (size_t)(module->mod_end - module->mod_start));
    console_printf("initramfs: module %p-%p mount=%d\n",
                   (void *)(uintptr_t)module->mod_start,
                   (void *)(uintptr_t)module->mod_end,
                   mount_status);
  } else {
    console_writeln("initramfs: no module present");
  }

  task_create_kernel("kidle");
  task_spawn_user("init");

  console_writeln("interrupts: enabling timer + serial monitor");
  interrupt_enable();
  net_startup_connect();
  cli_init();
  cli_run();
  kernel_halt();
}
