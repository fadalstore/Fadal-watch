#include "vfs.h"
#include "fat12.h"

static struct vfs_mount root_mount;

static vfs_u8 fat12_mount_backend(void) {
    return fat12_mount();
}

static vfs_u32 fat12_root_entries_backend(void) {
    return fat12_root_entry_count();
}

static vfs_u32 fat12_write_backend(const char *name, const vfs_u8 *data, vfs_u32 size) {
    return fat12_write_file(name, data, size);
}

static vfs_u8 fat12_read_backend(const char *name, vfs_u8 *output, vfs_u32 capacity, vfs_u32 *size) {
    return fat12_read_file(name, output, capacity, size);
}

static const struct vfs_mount_ops fat12_ops = {
    .mount = fat12_mount_backend,
    .root_entries = fat12_root_entries_backend,
    .write_file = fat12_write_backend,
    .read_file = fat12_read_backend,
};

void vfs_init(void) {
    root_mount.active = 0;
    root_mount.type = VFS_FS_NONE;
    root_mount.name = (const char *)0;
    root_mount.ops = (const struct vfs_mount_ops *)0;
}

vfs_u8 vfs_mount_fat12(void) {
    if (!fat12_ops.mount()) {
        return 0;
    }
    root_mount.active = 1;
    root_mount.type = VFS_FS_FAT12;
    root_mount.name = "FAT12";
    root_mount.ops = &fat12_ops;
    return 1;
}

vfs_u8 vfs_is_mounted(void) {
    return root_mount.active && root_mount.ops != (const struct vfs_mount_ops *)0;
}

enum vfs_filesystem_type vfs_type(void) {
    return vfs_is_mounted() ? root_mount.type : VFS_FS_NONE;
}

const char *vfs_filesystem_name(void) {
    return vfs_is_mounted() ? root_mount.name : "none";
}

vfs_u32 vfs_root_entries(void) {
    if (!vfs_is_mounted()) {
        return 0;
    }
    return root_mount.ops->root_entries();
}

vfs_u32 vfs_write_file(const char *name, const vfs_u8 *data, vfs_u32 size) {
    if (!vfs_is_mounted()) {
        return 0;
    }
    return root_mount.ops->write_file(name, data, size);
}

vfs_u8 vfs_read_file(const char *name, vfs_u8 *output, vfs_u32 capacity, vfs_u32 *size) {
    if (!vfs_is_mounted()) {
        return 0;
    }
    return root_mount.ops->read_file(name, output, capacity, size);
}

vfs_u8 vfs_lookup_file(const char *name, vfs_u32 *size) {
    static vfs_u8 lookup_buffer[512];
    if (!vfs_is_mounted() || size == (vfs_u32 *)0) {
        return 0;
    }
    return root_mount.ops->read_file(name, lookup_buffer, sizeof(lookup_buffer), size);
}
