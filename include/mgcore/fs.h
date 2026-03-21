#ifndef MGCORE_FS_H
#define MGCORE_FS_H

#include "mgcore/types.h"

#include <stdint.h>

#define MGCORE_PATH_MAX 256
#define MGCORE_NAME_MAX 32
#define MGCORE_MAX_NODES 256

typedef enum vfs_node_type {
  VFS_NODE_NONE = 0,
  VFS_NODE_FILE,
  VFS_NODE_DIR,
  VFS_NODE_CHARDEV,
} vfs_node_type;

typedef struct vfs_node {
  int id;
  int parent_id;
  int first_child_id;
  int next_sibling_id;
  vfs_node_type type;
  char name[MGCORE_NAME_MAX];
  uint8_t *data;
  size_t size;
  size_t capacity;
  uint32_t mode;
  int device_id;
} vfs_node;

typedef struct vfs_file {
  int used;
  int refs;
  int node_id;
  size_t offset;
  int flags;
} vfs_file;

void fs_init(void);
int fs_mount_initramfs(const void *archive, size_t size);
int fs_open_path(const char *path, int flags, int mode);
ssize_t fs_read_fd(int fd, void *buffer, size_t count);
ssize_t fs_write_fd(int fd, const void *buffer, size_t count);
int fs_close_fd(int fd);
off_t fs_lseek_fd(int fd, off_t offset, int whence);
int fs_stat_path(const char *path, void *stat_out);
int fs_fstat_fd(int fd, void *stat_out);
int fs_mkdir_path(const char *path, mode_t mode);
int fs_rmdir_path(const char *path);
int fs_unlink_path(const char *path);
int fs_chdir_path(const char *path);
const char *fs_getcwd(void);
void fs_dump_root(void);

#endif
