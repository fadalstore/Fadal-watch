#include "vfs.h"
#include "fat12.h"

static struct vfs_driver drivers[VFS_MAX_DRIVERS];
static struct vfs_mount mounts[VFS_MAX_MOUNTS];
static vfs_u32 driver_count;

static vfs_u8 fat12_mount_backend(void) {
    return fat12_mount();
}

static void fat12_unmount_backend(void) {
    /* FAT12 currently has no destructive unmount operation. */
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
    .unmount = fat12_unmount_backend,
    .root_entries = fat12_root_entries_backend,
    .write_file = fat12_write_backend,
    .read_file = fat12_read_backend,
};

static vfs_u8 string_equal(const char *left, const char *right) {
    vfs_u32 index = 0;
    if (left == (const char *)0 || right == (const char *)0) {
        return 0;
    }
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return 0;
        }
        index++;
    }
    return left[index] == right[index];
}

static vfs_u8 valid_mountpoint(const char *mountpoint) {
    if (mountpoint == (const char *)0 || mountpoint[0] != '/') {
        return 0;
    }
    if (mountpoint[1] == '\0') {
        return 1;
    }
    return mountpoint[0] == '/' && mountpoint[1] != '/';
}

static vfs_u32 string_length(const char *value) {
    vfs_u32 length = 0;
    if (value == (const char *)0) {
        return 0;
    }
    while (value[length] != '\0') {
        length++;
    }
    return length;
}

static const struct vfs_driver *find_driver(enum vfs_filesystem_type type) {
    for (vfs_u32 index = 0; index < driver_count; index++) {
        if (drivers[index].type == type) {
            return &drivers[index];
        }
    }
    return (const struct vfs_driver *)0;
}

static struct vfs_mount *find_mount(const char *mountpoint) {
    for (vfs_u32 index = 0; index < VFS_MAX_MOUNTS; index++) {
        if (mounts[index].active && string_equal(mounts[index].mountpoint, mountpoint)) {
            return &mounts[index];
        }
    }
    return (struct vfs_mount *)0;
}

static struct vfs_mount *route_path(const char *path, const char **relative_path) {
    struct vfs_mount *selected = (struct vfs_mount *)0;
    vfs_u32 selected_length = 0;
    vfs_u32 path_length;

    if (path == (const char *)0 || relative_path == (const char **)0) {
        return (struct vfs_mount *)0;
    }
    path_length = string_length(path);

    for (vfs_u32 index = 0; index < VFS_MAX_MOUNTS; index++) {
        struct vfs_mount *candidate = &mounts[index];
        vfs_u32 mount_length;
        vfs_u8 boundary;

        if (!candidate->active || candidate->mountpoint == (const char *)0) {
            continue;
        }
        mount_length = string_length(candidate->mountpoint);
        if (mount_length == 1 && candidate->mountpoint[0] == '/') {
            boundary = 1;
        } else {
            boundary = path_length == mount_length ||
                (path_length > mount_length && path[mount_length] == '/');
        }
        if (path_length < mount_length || !boundary) {
            continue;
        }
        if (mount_length > 1) {
            for (vfs_u32 character = 0; character < mount_length; character++) {
                if (path[character] != candidate->mountpoint[character]) {
                    boundary = 0;
                    break;
                }
            }
            if (!boundary) {
                continue;
            }
        }
        if (mount_length >= selected_length) {
            selected = candidate;
            selected_length = mount_length;
        }
    }

    if (selected == (struct vfs_mount *)0) {
        return (struct vfs_mount *)0;
    }
    if (selected_length == 1 && selected->mountpoint[0] == '/' && path[0] != '/') {
        *relative_path = path;
    } else {
        *relative_path = path + selected_length;
    }
    while (**relative_path == '/') {
        (*relative_path)++;
    }
    return selected;
}

void vfs_init(void) {
    driver_count = 0;
    for (vfs_u32 index = 0; index < VFS_MAX_DRIVERS; index++) {
        drivers[index].type = VFS_FS_NONE;
        drivers[index].name = (const char *)0;
        drivers[index].ops = (const struct vfs_mount_ops *)0;
    }
    for (vfs_u32 index = 0; index < VFS_MAX_MOUNTS; index++) {
        mounts[index].active = 0;
        mounts[index].type = VFS_FS_NONE;
        mounts[index].name = (const char *)0;
        mounts[index].mountpoint = (const char *)0;
        mounts[index].ops = (const struct vfs_mount_ops *)0;
    }
    vfs_register_driver(VFS_FS_FAT12, "FAT12", &fat12_ops);
}

vfs_u8 vfs_register_driver(enum vfs_filesystem_type type,
                           const char *name,
                           const struct vfs_mount_ops *ops) {
    if (type == VFS_FS_NONE || name == (const char *)0 ||
        ops == (const struct vfs_mount_ops *)0 || ops->mount == (void *)0 ||
        ops->root_entries == (void *)0 || ops->write_file == (void *)0 ||
        ops->read_file == (void *)0 || find_driver(type) != (const struct vfs_driver *)0 ||
        driver_count >= VFS_MAX_DRIVERS) {
        return 0;
    }
    drivers[driver_count].type = type;
    drivers[driver_count].name = name;
    drivers[driver_count].ops = ops;
    driver_count++;
    return 1;
}

vfs_u8 vfs_mount_driver(enum vfs_filesystem_type type, const char *mountpoint) {
    const struct vfs_driver *driver = find_driver(type);
    struct vfs_mount *slot = (struct vfs_mount *)0;

    if (driver == (const struct vfs_driver *)0 || !valid_mountpoint(mountpoint)) {
        return 0;
    }
    if (find_mount(mountpoint) != (struct vfs_mount *)0) {
        return 0;
    }
    for (vfs_u32 index = 0; index < VFS_MAX_MOUNTS; index++) {
        if (!mounts[index].active) {
            slot = &mounts[index];
            break;
        }
    }
    if (slot == (struct vfs_mount *)0 || !driver->ops->mount()) {
        return 0;
    }
    slot->active = 1;
    slot->type = driver->type;
    slot->name = driver->name;
    slot->mountpoint = mountpoint;
    slot->ops = driver->ops;
    return 1;
}

vfs_u8 vfs_unmount(const char *mountpoint) {
    struct vfs_mount *mount = find_mount(mountpoint);
    if (mount == (struct vfs_mount *)0) {
        return 0;
    }
    if (mount->ops != (const struct vfs_mount_ops *)0 && mount->ops->unmount != (void *)0) {
        mount->ops->unmount();
    }
    mount->active = 0;
    mount->type = VFS_FS_NONE;
    mount->name = (const char *)0;
    mount->mountpoint = (const char *)0;
    mount->ops = (const struct vfs_mount_ops *)0;
    return 1;
}

vfs_u8 vfs_mount_fat12(void) {
    if (find_mount("/") != (struct vfs_mount *)0) {
        return 1;
    }
    return vfs_mount_driver(VFS_FS_FAT12, "/");
}

vfs_u8 vfs_is_mounted(void) {
    return find_mount("/") != (struct vfs_mount *)0;
}

enum vfs_filesystem_type vfs_type(void) {
    struct vfs_mount *root = find_mount("/");
    return root == (struct vfs_mount *)0 ? VFS_FS_NONE : root->type;
}

const char *vfs_filesystem_name(void) {
    struct vfs_mount *root = find_mount("/");
    return root == (struct vfs_mount *)0 ? "none" : root->name;
}

vfs_u32 vfs_root_entries(void) {
    struct vfs_mount *root = find_mount("/");
    return root == (struct vfs_mount *)0 ? 0 : root->ops->root_entries();
}

vfs_u32 vfs_write_path(const char *path, const vfs_u8 *data, vfs_u32 size) {
    const char *relative_path;
    struct vfs_mount *mount = route_path(path, &relative_path);
    if (mount == (struct vfs_mount *)0 || relative_path[0] == '\0') {
        return 0;
    }
    return mount->ops->write_file(relative_path, data, size);
}

vfs_u8 vfs_read_path(const char *path, vfs_u8 *output, vfs_u32 capacity, vfs_u32 *size) {
    const char *relative_path;
    struct vfs_mount *mount = route_path(path, &relative_path);
    if (mount == (struct vfs_mount *)0 || relative_path[0] == '\0') {
        return 0;
    }
    return mount->ops->read_file(relative_path, output, capacity, size);
}

vfs_u8 vfs_lookup_path(const char *path, vfs_u32 *size) {
    static vfs_u8 lookup_buffer[512];
    return vfs_read_path(path, lookup_buffer, sizeof(lookup_buffer), size);
}

vfs_u32 vfs_write_file(const char *name, const vfs_u8 *data, vfs_u32 size) {
    return vfs_write_path(name, data, size);
}

vfs_u8 vfs_read_file(const char *name, vfs_u8 *output, vfs_u32 capacity, vfs_u32 *size) {
    return vfs_read_path(name, output, capacity, size);
}

vfs_u8 vfs_lookup_file(const char *name, vfs_u32 *size) {
    return vfs_lookup_path(name, size);
}
