#ifndef FADAL_RAMFS_H
#define FADAL_RAMFS_H

#include "vfs.h"

vfs_u8 ramfs_mount(void);
void ramfs_unmount(void);
vfs_u32 ramfs_root_entries(void);
vfs_u32 ramfs_write_file(const char *name, const vfs_u8 *data, vfs_u32 size);
vfs_u8 ramfs_read_file(const char *name, vfs_u8 *output, vfs_u32 capacity, vfs_u32 *size);

#endif
