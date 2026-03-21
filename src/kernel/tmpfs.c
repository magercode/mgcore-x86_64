#include "mgcore/console.h"
#include "mgcore/errno.h"
#include "mgcore/fs.h"
#include "mgcore/kstring.h"

#define TMPFS_DATA_ARENA_SIZE (1024 * 1024)

static vfs_node g_nodes[MGCORE_MAX_NODES];
static uint8_t g_data_arena[TMPFS_DATA_ARENA_SIZE];
static size_t g_data_offset;

static int alloc_node(void) {
  int i;
  for (i = 0; i < MGCORE_MAX_NODES; ++i) {
    if (g_nodes[i].type == VFS_NODE_NONE) {
      g_nodes[i].id = i;
      g_nodes[i].parent_id = -1;
      g_nodes[i].first_child_id = -1;
      g_nodes[i].next_sibling_id = -1;
      g_nodes[i].data = NULL;
      g_nodes[i].size = 0;
      g_nodes[i].capacity = 0;
      g_nodes[i].mode = 0;
      g_nodes[i].device_id = 0;
      return i;
    }
  }
  return -MGCORE_ENOSPC;
}

static uint8_t *alloc_storage(size_t size) {
  uint8_t *ptr;
  size_t aligned = (size + 15) & ~15U;
  if (g_data_offset + aligned > TMPFS_DATA_ARENA_SIZE) {
    return NULL;
  }
  ptr = &g_data_arena[g_data_offset];
  g_data_offset += aligned;
  return ptr;
}

static int lookup_child(int parent_id, const char *name) {
  int child = g_nodes[parent_id].first_child_id;
  while (child >= 0) {
    if (kstrcmp(g_nodes[child].name, name) == 0) {
      return child;
    }
    child = g_nodes[child].next_sibling_id;
  }
  return -MGCORE_ENOENT;
}

static int split_parent(const char *path, char *parent, char *leaf) {
  const char *last = path + kstrlen(path);
  while (last > path && last[-1] == '/') {
    --last;
  }
  while (last > path && last[-1] != '/') {
    --last;
  }
  if (last == path) {
    kstrcpy(parent, "/");
    kstrcpy(leaf, path[0] == '/' ? path + 1 : path);
  } else {
    size_t parent_len = (size_t)(last - path);
    kmemcpy(parent, path, parent_len);
    parent[parent_len] = '\0';
    kstrcpy(leaf, last);
  }
  return 0;
}

static int create_child(int parent_id, const char *name, vfs_node_type type, mode_t mode) {
  int id = alloc_node();
  vfs_node *node;
  if (id < 0) {
    return id;
  }
  node = &g_nodes[id];
  node->type = type;
  node->mode = (uint32_t)mode;
  node->parent_id = parent_id;
  kstrncpy(node->name, name, sizeof(node->name) - 1);
  node->name[sizeof(node->name) - 1] = '\0';
  node->next_sibling_id = g_nodes[parent_id].first_child_id;
  g_nodes[parent_id].first_child_id = id;
  return id;
}

static int resolve_path(const char *path) {
  const char *cursor = path;
  char token[MGCORE_NAME_MAX];
  int current = 0;
  if (!path || path[0] != '/') {
    return -MGCORE_EINVAL;
  }
  if (path[1] == '\0') {
    return 0;
  }
  while (*cursor == '/') {
    ++cursor;
  }
  while (*cursor) {
    size_t len = 0;
    while (*cursor && *cursor != '/' && len + 1 < sizeof(token)) {
      token[len++] = *cursor++;
    }
    token[len] = '\0';
    while (*cursor == '/') {
      ++cursor;
    }
    if (token[0] == '\0' || kstrcmp(token, ".") == 0) {
      continue;
    }
    if (kstrcmp(token, "..") == 0) {
      current = (g_nodes[current].parent_id >= 0) ? g_nodes[current].parent_id : 0;
      continue;
    }
    current = lookup_child(current, token);
    if (current < 0) {
      return current;
    }
  }
  return current;
}

void tmpfs_init(void) {
  kmemset(g_nodes, 0, sizeof(g_nodes));
  g_data_offset = 0;
  g_nodes[0].id = 0;
  g_nodes[0].parent_id = -1;
  g_nodes[0].first_child_id = -1;
  g_nodes[0].next_sibling_id = -1;
  g_nodes[0].type = VFS_NODE_DIR;
  g_nodes[0].mode = 0755;
  kstrcpy(g_nodes[0].name, "/");
}

int tmpfs_lookup_path(const char *path) {
  return resolve_path(path);
}

int tmpfs_create_path(const char *path, vfs_node_type type, mode_t mode) {
  char parent[MGCORE_PATH_MAX];
  char leaf[MGCORE_NAME_MAX];
  int parent_id;
  int existing;
  if (!path || path[0] != '/' || path[1] == '\0') {
    return -MGCORE_EINVAL;
  }
  existing = resolve_path(path);
  if (existing >= 0) {
    return existing;
  }
  split_parent(path, parent, leaf);
  parent_id = resolve_path(parent);
  if (parent_id < 0) {
    return parent_id;
  }
  if (g_nodes[parent_id].type != VFS_NODE_DIR) {
    return -MGCORE_ENOTDIR;
  }
  return create_child(parent_id, leaf, type, mode);
}

int tmpfs_remove_path(const char *path, int want_dir) {
  int id = resolve_path(path);
  int parent;
  int child;
  if (id <= 0) {
    return -MGCORE_EINVAL;
  }
  if (want_dir && g_nodes[id].type != VFS_NODE_DIR) {
    return -MGCORE_ENOTDIR;
  }
  if (!want_dir && g_nodes[id].type == VFS_NODE_DIR) {
    return -MGCORE_EISDIR;
  }
  if (g_nodes[id].type == VFS_NODE_DIR && g_nodes[id].first_child_id >= 0) {
    return -MGCORE_ENOTEMPTY;
  }
  parent = g_nodes[id].parent_id;
  child = g_nodes[parent].first_child_id;
  if (child == id) {
    g_nodes[parent].first_child_id = g_nodes[id].next_sibling_id;
  } else {
    while (child >= 0 && g_nodes[child].next_sibling_id != id) {
      child = g_nodes[child].next_sibling_id;
    }
    if (child >= 0) {
      g_nodes[child].next_sibling_id = g_nodes[id].next_sibling_id;
    }
  }
  kmemset(&g_nodes[id], 0, sizeof(g_nodes[id]));
  return 0;
}

vfs_node *tmpfs_get_node(int node_id) {
  if (node_id < 0 || node_id >= MGCORE_MAX_NODES || g_nodes[node_id].type == VFS_NODE_NONE) {
    return NULL;
  }
  return &g_nodes[node_id];
}

ssize_t tmpfs_read_node(int node_id, void *buffer, size_t count, size_t offset) {
  vfs_node *node = tmpfs_get_node(node_id);
  size_t available;
  if (!node) {
    return -MGCORE_ENOENT;
  }
  if (node->type != VFS_NODE_FILE) {
    return -MGCORE_EISDIR;
  }
  if (offset >= node->size) {
    return 0;
  }
  available = node->size - offset;
  if (count > available) {
    count = available;
  }
  kmemcpy(buffer, node->data + offset, count);
  return (ssize_t)count;
}

ssize_t tmpfs_write_node(int node_id, const void *buffer, size_t count, size_t offset) {
  vfs_node *node = tmpfs_get_node(node_id);
  size_t end_offset = offset + count;
  uint8_t *new_data;
  if (!node) {
    return -MGCORE_ENOENT;
  }
  if (node->type != VFS_NODE_FILE) {
    return -MGCORE_EISDIR;
  }
  if (end_offset > node->capacity) {
    size_t new_capacity = (end_offset + 255) & ~255U;
    new_data = alloc_storage(new_capacity);
    if (!new_data) {
      return -MGCORE_ENOSPC;
    }
    if (node->data && node->size) {
      kmemcpy(new_data, node->data, node->size);
    }
    node->data = new_data;
    node->capacity = new_capacity;
  }
  kmemcpy(node->data + offset, buffer, count);
  if (end_offset > node->size) {
    node->size = end_offset;
  }
  return (ssize_t)count;
}

void tmpfs_dump_root(void) {
  int child = g_nodes[0].first_child_id;
  while (child >= 0) {
    const vfs_node *node = &g_nodes[child];
    const char *type = node->type == VFS_NODE_DIR ? "dir" :
                       node->type == VFS_NODE_CHARDEV ? "chr" : "file";
    console_printf("%s\t/%s\t%u bytes\n", type, node->name, (unsigned)node->size);
    child = node->next_sibling_id;
  }
}
