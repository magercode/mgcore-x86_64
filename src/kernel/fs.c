#include "mgcore/console.h"
#include "mgcore/errno.h"
#include "mgcore/fs.h"
#include "mgcore/kstring.h"
#include "mgcore/syscall.h"
#include "mgcore/task.h"

#define O_CREAT 64
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

enum mgcore_device_id {
  DEV_NONE = 0,
  DEV_CONSOLE = 1,
  DEV_TTY = 2,
  DEV_NULL = 3,
  DEV_ZERO = 4,
};

extern void tmpfs_init(void);
extern int tmpfs_lookup_path(const char *path);
extern int tmpfs_create_path(const char *path, vfs_node_type type, mode_t mode);
extern int tmpfs_remove_path(const char *path, int want_dir);
extern vfs_node *tmpfs_get_node(int node_id);
extern ssize_t tmpfs_read_node(int node_id, void *buffer, size_t count, size_t offset);
extern ssize_t tmpfs_write_node(int node_id, const void *buffer, size_t count, size_t offset);
extern void tmpfs_dump_root(void);

static vfs_file g_open_files[128];

static void normalize_path(const char *path, char *out) {
  const char *source = path;
  size_t out_len = 0;

  if (!path || path[0] == '\0') {
    kstrcpy(out, "/");
    return;
  }

  if (path[0] != '/') {
    const char *cwd = task_current()->cwd;
    kstrcpy(out, cwd);
    out_len = kstrlen(out);
    if (out_len > 1 && out[out_len - 1] != '/') {
      out[out_len++] = '/';
      out[out_len] = '\0';
    }
  }

  while (*source && out_len + 1 < MGCORE_PATH_MAX) {
    if (*source == '/' && out_len > 0 && out[out_len - 1] == '/') {
      ++source;
      continue;
    }
    out[out_len++] = *source++;
  }
  out[out_len] = '\0';
}

static int alloc_open_file(void) {
  size_t i;
  for (i = 0; i < MGCORE_ARRAY_SIZE(g_open_files); ++i) {
    if (!g_open_files[i].used) {
      g_open_files[i].used = 1;
      g_open_files[i].refs = 1;
      g_open_files[i].offset = 0;
      return (int)i;
    }
  }
  return -MGCORE_ENFILE;
}

static int attach_fd(task *proc, int open_id) {
  int fd;
  for (fd = 3; fd < MGCORE_MAX_FDS; ++fd) {
    if (!proc->fds[fd]) {
      proc->fds[fd] = &g_open_files[open_id];
      return fd;
    }
  }
  g_open_files[open_id].used = 0;
  return -MGCORE_EMFILE;
}

static vfs_file *lookup_file(int fd) {
  task *proc = task_current();
  if (fd < 0 || fd >= MGCORE_MAX_FDS) {
    return NULL;
  }
  return proc->fds[fd];
}

static void init_device_node(const char *path, int device_id) {
  int node_id = tmpfs_create_path(path, VFS_NODE_CHARDEV, 0666);
  vfs_node *node;
  if (node_id < 0) {
    node_id = tmpfs_lookup_path(path);
  }
  node = tmpfs_get_node(node_id);
  if (node) {
    node->device_id = device_id;
  }
}

static int fill_stat_from_node(vfs_node *node, struct mgcore_stat *statbuf) {
  if (!node || !statbuf) {
    return -MGCORE_EINVAL;
  }
  kmemset(statbuf, 0, sizeof(*statbuf));
  statbuf->st_ino = (uint64_t)(node->id + 1);
  statbuf->st_mode = node->mode;
  statbuf->st_size = (int64_t)node->size;
  statbuf->st_nlink = 1;
  statbuf->st_blksize = 4096;
  statbuf->st_blocks = (int64_t)((node->size + 511) / 512);
  return 0;
}

static int parse_cpio_hex(const char *text, size_t len, uint32_t *out) {
  uint32_t value = 0;
  size_t i;
  for (i = 0; i < len; ++i) {
    char c = text[i];
    value <<= 4;
    if (c >= '0' && c <= '9') value |= (uint32_t)(c - '0');
    else if (c >= 'a' && c <= 'f') value |= (uint32_t)(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') value |= (uint32_t)(c - 'A' + 10);
    else return -1;
  }
  *out = value;
  return 0;
}

void fs_init(void) {
  tmpfs_init();
  tmpfs_create_path("/dev", VFS_NODE_DIR, 0755);
  tmpfs_create_path("/bin", VFS_NODE_DIR, 0755);
  tmpfs_create_path("/tmp", VFS_NODE_DIR, 0777);
  tmpfs_create_path("/etc", VFS_NODE_DIR, 0755);
  init_device_node("/dev/console", DEV_CONSOLE);
  init_device_node("/dev/tty", DEV_TTY);
  init_device_node("/dev/null", DEV_NULL);
  init_device_node("/dev/zero", DEV_ZERO);
  console_writeln("fs: tmpfs root mounted");
}

int fs_mount_initramfs(const void *archive, size_t size) {
  const uint8_t *cursor = (const uint8_t *)archive;
  const uint8_t *limit = cursor + size;

  while (cursor + 110 <= limit) {
    uint32_t mode = 0;
    uint32_t file_size = 0;
    uint32_t name_size = 0;
    const char *header = (const char *)cursor;
    const char *name;
    const uint8_t *data;
    char path[MGCORE_PATH_MAX];
    int node_id;

    if (kstrncmp(header, "070701", 6) != 0) {
      return -MGCORE_EINVAL;
    }

    parse_cpio_hex(header + 14, 8, &mode);
    parse_cpio_hex(header + 54, 8, &file_size);
    parse_cpio_hex(header + 94, 8, &name_size);

    name = (const char *)(cursor + 110);
    if (name_size == 0 || cursor + 110 + name_size > limit) {
      return -MGCORE_EINVAL;
    }
    if (kstrcmp(name, "TRAILER!!!") == 0) {
      return 0;
    }
    if (kstrcmp(name, ".") == 0) {
      cursor += (110 + name_size + 3) & ~3U;
      cursor += (file_size + 3) & ~3U;
      continue;
    }

    normalize_path(name, path);
    data = cursor + ((110 + name_size + 3) & ~3U);
    if (data + file_size > limit) {
      return -MGCORE_EINVAL;
    }

    if ((mode & 0170000) == 0040000) {
      tmpfs_create_path(path, VFS_NODE_DIR, mode);
    } else {
      node_id = tmpfs_create_path(path, VFS_NODE_FILE, mode);
      if (node_id >= 0 && file_size != 0) {
        tmpfs_write_node(node_id, data, file_size, 0);
      }
    }

    cursor = data + ((file_size + 3) & ~3U);
  }
  return 0;
}

int fs_open_path(const char *path, int flags, int mode) {
  char resolved[MGCORE_PATH_MAX];
  int node_id;
  int open_id;
  normalize_path(path, resolved);
  node_id = tmpfs_lookup_path(resolved);
  if (node_id < 0 && (flags & O_CREAT)) {
    node_id = tmpfs_create_path(resolved, VFS_NODE_FILE, (mode_t)mode);
  }
  if (node_id < 0) {
    return node_id;
  }

  open_id = alloc_open_file();
  if (open_id < 0) {
    return open_id;
  }
  g_open_files[open_id].node_id = node_id;
  g_open_files[open_id].flags = flags;
  return attach_fd(task_current(), open_id);
}

ssize_t fs_read_fd(int fd, void *buffer, size_t count) {
  vfs_file *file = lookup_file(fd);
  vfs_node *node;
  size_t i;

  if (fd == 0 && !file) {
    char *out = (char *)buffer;
    for (i = 0; i < count; ++i) {
      if (!console_try_read_char(&out[i])) {
        return (ssize_t)i;
      }
    }
    return (ssize_t)count;
  }
  if (!file) {
    return -MGCORE_EBADF;
  }

  node = tmpfs_get_node(file->node_id);
  if (!node) {
    return -MGCORE_EBADF;
  }

  if (node->type == VFS_NODE_CHARDEV) {
    if (node->device_id == DEV_ZERO) {
      kmemset(buffer, 0, count);
      return (ssize_t)count;
    }
    if (node->device_id == DEV_NULL || node->device_id == DEV_CONSOLE || node->device_id == DEV_TTY) {
      char *out = (char *)buffer;
      for (i = 0; i < count; ++i) {
        if (!console_try_read_char(&out[i])) {
          return (ssize_t)i;
        }
      }
      return (ssize_t)count;
    }
  }

  i = (size_t)tmpfs_read_node(file->node_id, buffer, count, file->offset);
  if ((ssize_t)i >= 0) {
    file->offset += i;
  }
  return (ssize_t)i;
}

ssize_t fs_write_fd(int fd, const void *buffer, size_t count) {
  vfs_file *file = lookup_file(fd);
  vfs_node *node;
  if ((fd == 1 || fd == 2) && !file) {
    console_write_n((const char *)buffer, count);
    return (ssize_t)count;
  }
  if (!file) {
    return -MGCORE_EBADF;
  }
  node = tmpfs_get_node(file->node_id);
  if (!node) {
    return -MGCORE_EBADF;
  }
  if (node->type == VFS_NODE_CHARDEV) {
    if (node->device_id == DEV_NULL) {
      return (ssize_t)count;
    }
    if (node->device_id == DEV_CONSOLE || node->device_id == DEV_TTY) {
      console_write_n((const char *)buffer, count);
      return (ssize_t)count;
    }
  }
  return tmpfs_write_node(file->node_id, buffer, count, file->offset);
}

int fs_close_fd(int fd) {
  task *proc = task_current();
  vfs_file *file = lookup_file(fd);
  if (!file) {
    return (fd >= 0 && fd <= 2) ? 0 : -MGCORE_EBADF;
  }
  if (--file->refs <= 0) {
    kmemset(file, 0, sizeof(*file));
  }
  proc->fds[fd] = NULL;
  return 0;
}

off_t fs_lseek_fd(int fd, off_t offset, int whence) {
  vfs_file *file = lookup_file(fd);
  vfs_node *node;
  if (!file) {
    return -MGCORE_EBADF;
  }
  node = tmpfs_get_node(file->node_id);
  if (!node) {
    return -MGCORE_EBADF;
  }
  if (whence == SEEK_SET) file->offset = (size_t)offset;
  else if (whence == SEEK_CUR) file->offset += (size_t)offset;
  else if (whence == SEEK_END) file->offset = node->size + (size_t)offset;
  else return -MGCORE_EINVAL;
  return (off_t)file->offset;
}

int fs_stat_path(const char *path, void *stat_out) {
  char resolved[MGCORE_PATH_MAX];
  normalize_path(path, resolved);
  return fill_stat_from_node(tmpfs_get_node(tmpfs_lookup_path(resolved)), (struct mgcore_stat *)stat_out);
}

int fs_fstat_fd(int fd, void *stat_out) {
  vfs_file *file = lookup_file(fd);
  if (!file) {
    return -MGCORE_EBADF;
  }
  return fill_stat_from_node(tmpfs_get_node(file->node_id), (struct mgcore_stat *)stat_out);
}

int fs_mkdir_path(const char *path, mode_t mode) {
  char resolved[MGCORE_PATH_MAX];
  normalize_path(path, resolved);
  return tmpfs_create_path(resolved, VFS_NODE_DIR, mode);
}

int fs_rmdir_path(const char *path) {
  char resolved[MGCORE_PATH_MAX];
  normalize_path(path, resolved);
  return tmpfs_remove_path(resolved, 1);
}

int fs_unlink_path(const char *path) {
  char resolved[MGCORE_PATH_MAX];
  normalize_path(path, resolved);
  return tmpfs_remove_path(resolved, 0);
}

int fs_chdir_path(const char *path) {
  char resolved[MGCORE_PATH_MAX];
  int node_id;
  vfs_node *node;
  normalize_path(path, resolved);
  node_id = tmpfs_lookup_path(resolved);
  if (node_id < 0) {
    return node_id;
  }
  node = tmpfs_get_node(node_id);
  if (!node || node->type != VFS_NODE_DIR) {
    return -MGCORE_ENOTDIR;
  }
  kstrncpy(task_current()->cwd, resolved, sizeof(task_current()->cwd) - 1);
  task_current()->cwd[sizeof(task_current()->cwd) - 1] = '\0';
  return 0;
}

const char *fs_getcwd(void) {
  return task_current()->cwd;
}

void fs_dump_root(void) {
  tmpfs_dump_root();
}
