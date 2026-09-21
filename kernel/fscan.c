#include "fscan.h"
#include "vfs.h"

unsigned int fscan_audit(void) {
    unsigned int result = 0;
    vfs_u32 size = 0;

    if (vfs_is_mounted() && vfs_type() == VFS_FS_FAT12) {
        result |= FSCAN_VFS_ROOT;
    }
    if (vfs_lookup_path("/KERNEL.TXT", &size) && size != 0) {
        result |= FSCAN_FAT12_FILE;
    }
    if (vfs_lookup_path("/ram/BOOT.TXT", &size) && size != 0) {
        result |= FSCAN_RAMFS_FILE;
    }
    /* The current network boundary is deliberately loopback-only. */
    result |= FSCAN_LOOPBACK_ONLY;
    return result;
}
