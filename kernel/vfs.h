#ifndef FADAL_VFS_H
#define FADAL_VFS_H

typedef unsigned char vfs_u8;
typedef unsigned int vfs_u32;

enum vfs_filesystem_type {
    VFS_FS_NONE = 0,
    VFS_FS_FAT12 = 1
};

struct vfs_mount_ops {
    vfs_u8 (*mount)(void);
    vfs_u32 (*root_entries)(void);
    vfs_u32 (*write_file)(const char *name, const vfs_u8 *data, vfs_u32 size);
    vfs_u8 (*read_file)(const char *name, vfs_u8 *output, vfs_u32 capacity, vfs_u32 *size);
};

struct vfs_mount {
    vfs_u8 active;
    enum vfs_filesystem_type type;
    const char *name;
    const struct vfs_mount_ops *ops;
};

void vfs_init(void);
vfs_u8 vfs_mount_fat12(void);
vfs_u8 vfs_is_mounted(void);
enum vfs_filesystem_type vfs_type(void);
const char *vfs_filesystem_name(void);
vfs_u32 vfs_root_entries(void);
vfs_u32 vfs_write_file(const char *name, const vfs_u8 *data, vfs_u32 size);
vfs_u8 vfs_read_file(const char *name, vfs_u8 *output, vfs_u32 capacity, vfs_u32 *size);
vfs_u8 vfs_lookup_file(const char *name, vfs_u32 *size);

#endif
