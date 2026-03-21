#include <unistd.h>

static void puts_fd(int fd, const char *s) {
  size_t len = 0;
  while (s[len]) ++len;
  write(fd, s, len);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  puts_fd(1, "MGCORE init: bootstrapped userspace placeholder\n");
  puts_fd(1, "MGCORE init: intended next step is exec /bin/shell\n");
  return 0;
}
