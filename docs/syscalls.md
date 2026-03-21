# MGCORE Syscall Notes

MGCORE memakai nomor syscall `x86_64` yang mengikuti Linux untuk subset awal.

## Implemented / scaffolded

`read`, `write`, `open`, `openat`, `close`, `stat`, `fstat`, `lseek`, `mmap`, `mprotect`, `munmap`, `brk`, `rt_sigaction`, `ioctl`, `sched_yield`, `nanosleep`, `getpid`, `signal`, `fork`, `execve`, `exit`, `wait4`, `kill`, `uname`, `getcwd`, `chdir`, `mkdir`, `rmdir`, `unlink`, `gettimeofday`, `reboot`.

## Unimplemented placeholders returning `-ENOSYS`

`poll`, `pread64`, `pwrite64`, `readv`, `writev`, `access`, `pipe`, `mremap`, `msync`, `mincore`, `madvise`, `dup`, `dup2`, `socket`, `connect`, `clone`, `vfork`, `fcntl`, `creat`, `link`, `readlink`.

## Userspace build

```sh
cmake -S . -B build
cmake --build build
```

Built payloads are copied into the generated initramfs under `/bin` and `/scripts`.
