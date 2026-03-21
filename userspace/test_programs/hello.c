#include <unistd.h>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  static const char message[] = "Hello from MGCORE userspace C program\n";
  write(1, message, sizeof(message) - 1);
  return 0;
}
