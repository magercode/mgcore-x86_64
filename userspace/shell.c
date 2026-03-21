#include <unistd.h>

static void puts_fd(int fd, const char *s) {
  size_t len = 0;
  while (s[len]) ++len;
  write(fd, s, len);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  puts_fd(1, "MGCORE shell: static userspace ELF placeholder\n");
  puts_fd(1, "MGCORE shell: command parsing to be wired after full exec/user-mode bring-up\n");
  return 0;
}
