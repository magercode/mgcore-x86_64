#include "mgcore/console.h"
#include "mgcore/elf.h"
#include "mgcore/errno.h"
#include "mgcore/fs.h"
#include "mgcore/kstring.h"
#include "mgcore/sched.h"
#include "mgcore/signal.h"
#include "mgcore/syscall.h"
#include "mgcore/task.h"
#include "mgcore/vmm.h"

#define AT_FDCWD -100

static long copy_uname(struct mgcore_utsname *name) {
  if (!name) {
    return -MGCORE_EFAULT;
  }
  kmemset(name, 0, sizeof(*name));
  kstrcpy(name->sysname, "MGCORE");
  kstrcpy(name->nodename, "mgcore");
  kstrcpy(name->release, "0.1");
  kstrcpy(name->version, "linux-like syscall scaffold");
  kstrcpy(name->machine, "x86_64");
  return 0;
}

#define UNIMPLEMENTED(name) \
  long name(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) { \
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; \
    return -MGCORE_ENOSYS; \
  }

long sys_read(uint64_t fd, uint64_t buffer, uint64_t count, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a3; (void)a4; (void)a5;
  return fs_read_fd((int)fd, (void *)(uintptr_t)buffer, (size_t)count);
}

long sys_write(uint64_t fd, uint64_t buffer, uint64_t count, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a3; (void)a4; (void)a5;
  return fs_write_fd((int)fd, (const void *)(uintptr_t)buffer, (size_t)count);
}

long sys_open(uint64_t path, uint64_t flags, uint64_t mode, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a3; (void)a4; (void)a5;
  return fs_open_path((const char *)(uintptr_t)path, (int)flags, (int)mode);
}

long sys_openat(uint64_t dirfd, uint64_t path, uint64_t flags, uint64_t mode, uint64_t a4, uint64_t a5) {
  (void)a4; (void)a5;
  if ((int64_t)dirfd != AT_FDCWD) {
    return -MGCORE_ENOSYS;
  }
  return fs_open_path((const char *)(uintptr_t)path, (int)flags, (int)mode);
}

long sys_close(uint64_t fd, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
  return fs_close_fd((int)fd);
}

long sys_stat(uint64_t path, uint64_t statbuf, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a2; (void)a3; (void)a4; (void)a5;
  return fs_stat_path((const char *)(uintptr_t)path, (void *)(uintptr_t)statbuf);
}

long sys_fstat(uint64_t fd, uint64_t statbuf, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a2; (void)a3; (void)a4; (void)a5;
  return fs_fstat_fd((int)fd, (void *)(uintptr_t)statbuf);
}

long sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a3; (void)a4; (void)a5;
  return fs_lseek_fd((int)fd, (off_t)offset, (int)whence);
}

long sys_mmap(uint64_t address, uint64_t length, uint64_t prot, uint64_t flags, uint64_t fd, uint64_t offset) {
  uintptr_t region;
  (void)address; (void)flags; (void)fd; (void)offset;
  region = vmm_reserve_range(&task_current()->mm, (size_t)length, (int)prot);
  return region ? (long)region : -MGCORE_ENOMEM;
}

long sys_mprotect(uint64_t address, uint64_t length, uint64_t prot, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)address; (void)length; (void)prot; (void)a3; (void)a4; (void)a5;
  return 0;
}

long sys_munmap(uint64_t address, uint64_t length, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a2; (void)a3; (void)a4; (void)a5;
  return vmm_unmap(&task_current()->mm, (uintptr_t)address, (size_t)length);
}

long sys_brk(uint64_t address, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  address_space *mm = &task_current()->mm;
  (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
  if (address == 0) {
    return (long)mm->brk_current;
  }
  if (address < mm->brk_base) {
    return (long)mm->brk_current;
  }
  mm->brk_current = (uintptr_t)address;
  return (long)mm->brk_current;
}

long sys_rt_sigaction(uint64_t signum, uint64_t act, uint64_t oldact, uint64_t sigsetsize, uint64_t a4, uint64_t a5) {
  (void)signum; (void)act; (void)oldact; (void)sigsetsize; (void)a4; (void)a5;
  return 0;
}

long sys_signal(uint64_t signum, uint64_t handler, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)handler; (void)a2; (void)a3; (void)a4; (void)a5;
  return signum < 32 ? 0 : -MGCORE_EINVAL;
}

long sys_ioctl(uint64_t fd, uint64_t request, uint64_t argp, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)fd; (void)request; (void)argp; (void)a3; (void)a4; (void)a5;
  return 0;
}

long sys_sched_yield(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
  return 0;
}

long sys_nanosleep(uint64_t req, uint64_t rem, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  const struct mgcore_timespec *ts = (const struct mgcore_timespec *)(uintptr_t)req;
  uint64_t sleep_ticks = 1;
  (void)rem; (void)a2; (void)a3; (void)a4; (void)a5;
  if (ts) {
    sleep_ticks = (uint64_t)(ts->tv_sec * MGCORE_HZ) + (uint64_t)(ts->tv_nsec / (1000000000 / MGCORE_HZ));
    if (sleep_ticks == 0) {
      sleep_ticks = 1;
    }
  }
  task_sleep_until(scheduler_uptime_ticks() + sleep_ticks);
  return 0;
}

long sys_getpid(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
  return task_current_pid();
}

long sys_fork(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  task *child;
  (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
  child = task_spawn_user(task_current()->name);
  return child ? child->pid : -MGCORE_ENOMEM;
}

long sys_execve(uint64_t path, uint64_t argv, uint64_t envp, uint64_t a3, uint64_t a4, uint64_t a5) {
  elf_image image;
  int status;
  (void)argv; (void)envp; (void)a3; (void)a4; (void)a5;
  status = elf_probe_path((const char *)(uintptr_t)path, &image);
  if (status < 0) {
    return status;
  }
  console_printf("execve: %s entry=%p\n", (const char *)(uintptr_t)path, (void *)image.entry);
  return 0;
}

long sys_exit(uint64_t status, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
  task_exit((int)status);
  return 0;
}

long sys_wait4(uint64_t pid, uint64_t status_ptr, uint64_t options, uint64_t rusage, uint64_t a4, uint64_t a5) {
  size_t i;
  (void)pid; (void)options; (void)rusage; (void)a4; (void)a5;
  for (i = 1; i < MGCORE_MAX_TASKS; ++i) {
    task *candidate = task_find((pid_t)i);
    if (candidate && candidate->ppid == task_current_pid() && candidate->state == TASK_ZOMBIE) {
      if (status_ptr) {
        *(int *)(uintptr_t)status_ptr = candidate->exit_code << 8;
      }
      return candidate->pid;
    }
  }
  return -MGCORE_ECHILD;
}

long sys_kill(uint64_t pid, uint64_t signum, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a2; (void)a3; (void)a4; (void)a5;
  return signal_send((pid_t)pid, (int)signum);
}

long sys_uname(uint64_t name_ptr, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
  return copy_uname((struct mgcore_utsname *)(uintptr_t)name_ptr);
}

long sys_getcwd(uint64_t buffer, uint64_t size, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  const char *cwd = fs_getcwd();
  size_t length = kstrlen(cwd) + 1;
  (void)a2; (void)a3; (void)a4; (void)a5;
  if (!buffer || size < length) {
    return -MGCORE_EINVAL;
  }
  kmemcpy((void *)(uintptr_t)buffer, cwd, length);
  return (long)buffer;
}

long sys_chdir(uint64_t path, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
  return fs_chdir_path((const char *)(uintptr_t)path);
}

long sys_mkdir(uint64_t path, uint64_t mode, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a2; (void)a3; (void)a4; (void)a5;
  return fs_mkdir_path((const char *)(uintptr_t)path, (mode_t)mode);
}

long sys_rmdir(uint64_t path, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
  return fs_rmdir_path((const char *)(uintptr_t)path);
}

long sys_unlink(uint64_t path, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
  return fs_unlink_path((const char *)(uintptr_t)path);
}

long sys_gettimeofday(uint64_t tv_ptr, uint64_t tz_ptr, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  struct mgcore_timeval *tv = (struct mgcore_timeval *)(uintptr_t)tv_ptr;
  uint64_t ms = scheduler_uptime_ms();
  (void)tz_ptr; (void)a2; (void)a3; (void)a4; (void)a5;
  if (!tv) {
    return -MGCORE_EFAULT;
  }
  tv->tv_sec = (int64_t)(ms / 1000);
  tv->tv_usec = (int64_t)((ms % 1000) * 1000);
  return 0;
}

long sys_reboot(uint64_t magic1, uint64_t magic2, uint64_t cmd, uint64_t arg, uint64_t a4, uint64_t a5) {
  (void)magic1; (void)magic2; (void)cmd; (void)arg; (void)a4; (void)a5;
  console_writeln("reboot: requested (stub)");
  return 0;
}

UNIMPLEMENTED(sys_lstat)
UNIMPLEMENTED(sys_poll)
UNIMPLEMENTED(sys_pread64)
UNIMPLEMENTED(sys_pwrite64)
UNIMPLEMENTED(sys_readv)
UNIMPLEMENTED(sys_writev)
UNIMPLEMENTED(sys_access)
UNIMPLEMENTED(sys_pipe)
UNIMPLEMENTED(sys_mremap)
UNIMPLEMENTED(sys_msync)
UNIMPLEMENTED(sys_mincore)
UNIMPLEMENTED(sys_madvise)
UNIMPLEMENTED(sys_dup)
UNIMPLEMENTED(sys_dup2)
UNIMPLEMENTED(sys_socket)
UNIMPLEMENTED(sys_connect)
UNIMPLEMENTED(sys_clone)
UNIMPLEMENTED(sys_vfork)
UNIMPLEMENTED(sys_fcntl)
UNIMPLEMENTED(sys_creat)
UNIMPLEMENTED(sys_link)
UNIMPLEMENTED(sys_readlink)
