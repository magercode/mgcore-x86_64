#include "mgcore/elf.h"
#include "mgcore/errno.h"
#include "mgcore/fs.h"
#include "mgcore/kstring.h"
#include "mgcore/syscall.h"
#include "mgcore/util.h"

#include <stdint.h>

#define ELF_MAGIC "\177ELF"
#define ELFCLASS64 2
#define EM_X86_64 62

typedef struct elf64_header {
  uint8_t ident[16];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint64_t entry;
  uint64_t phoff;
  uint64_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
} elf64_header;

void elf_init(void) {
}

int elf_parse_image(const void *data, size_t size, elf_image *out) {
  const elf64_header *header = (const elf64_header *)data;
  if (!data || size < sizeof(*header)) {
    return -MGCORE_EINVAL;
  }
  if (kmemcmp(header->ident, ELF_MAGIC, 4) != 0 || header->ident[4] != ELFCLASS64) {
    return -MGCORE_ENOEXEC;
  }
  if (header->machine != EM_X86_64) {
    return -MGCORE_ENOEXEC;
  }
  if (out) {
    out->base = data;
    out->size = size;
    out->entry = (uintptr_t)header->entry;
  }
  return 0;
}

int elf_probe_path(const char *path, elf_image *out) {
  int fd;
  struct mgcore_stat statbuf;
  void *buffer;
  int status;
  fd = fs_open_path(path, 0, 0);
  if (fd < 0) {
    return fd;
  }
  status = fs_fstat_fd(fd, &statbuf);
  if (status < 0) {
    fs_close_fd(fd);
    return status;
  }
  buffer = kalloc((size_t)statbuf.st_size);
  if (!buffer) {
    fs_close_fd(fd);
    return -MGCORE_ENOMEM;
  }
  if (fs_read_fd(fd, buffer, (size_t)statbuf.st_size) < 0) {
    fs_close_fd(fd);
    return -MGCORE_EIO;
  }
  fs_close_fd(fd);
  return elf_parse_image(buffer, (size_t)statbuf.st_size, out);
}
