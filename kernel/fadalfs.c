#include "fadalfs.h"

#define FADALFS_INODES_PER_BLOCK (FADALFS_BLOCK_SIZE / sizeof(struct fadalfs_inode))
#define FADALFS_BITMAP_BITS (FADALFS_BLOCK_SIZE * 8U)

static void fs_zero(unsigned char *buffer) {
    unsigned int index;
    for (index = 0; index < FADALFS_BLOCK_SIZE; index++) buffer[index] = 0;
}

static unsigned int fs_checksum(const struct fadalfs_superblock *super) {
    const unsigned char *bytes = (const unsigned char *)super;
    unsigned int sum = 0;
    unsigned int index;
    for (index = 0; index < sizeof(*super) - sizeof(super->checksum); index++) {
        sum = (sum << 5) - sum + bytes[index];
    }
    return sum;
}

static unsigned int fs_valid(const struct fadalfs *fs) {
    if (fs == (const struct fadalfs *)0 || !fs->mounted || fs->device == (struct fadal_blockdev *)0) return 0;
    if (fs->super.magic != FADALFS_MAGIC || fs->super.version != FADALFS_VERSION ||
        fs->super.block_size != FADALFS_BLOCK_SIZE || fs->super.root_inode != FADALFS_ROOT_INODE ||
        fs->super.data_start < FADALFS_DATA_START || fs->super.data_start >= fs->super.total_blocks ||
        fs->super.total_blocks > FADALFS_BITMAP_BITS || fs->super.total_blocks > fs->device->sector_count ||
        fs->super.checksum != fs_checksum(&fs->super)) return 0;
    return 1;
}

static unsigned int fs_bitmap_get(const unsigned char *bitmap, unsigned int block) {
    return (bitmap[block / 8U] >> (block % 8U)) & 1U;
}

static void fs_bitmap_set(unsigned char *bitmap, unsigned int block, unsigned int used) {
    unsigned char mask = (unsigned char)(1U << (block % 8U));
    if (used) bitmap[block / 8U] |= mask;
    else bitmap[block / 8U] &= (unsigned char)~mask;
}

unsigned int fadalfs_format(struct fadal_blockdev *device) {
    struct fadalfs_superblock super;
    unsigned char buffer[FADALFS_BLOCK_SIZE];
    unsigned int block;
    unsigned int total;
    if (device == (struct fadal_blockdev *)0 || !device->online) return FADALFS_INVALID;
    total = device->sector_count;
    if (total <= FADALFS_DATA_START || total > FADALFS_BITMAP_BITS) return FADALFS_INVALID;

    fs_zero(buffer);
    super.magic = FADALFS_MAGIC;
    super.version = FADALFS_VERSION;
    super.block_size = FADALFS_BLOCK_SIZE;
    super.total_blocks = total;
    super.free_blocks = total - FADALFS_DATA_START;
    super.inode_count = FADALFS_MAX_INODES;
    super.root_inode = FADALFS_ROOT_INODE;
    super.data_start = FADALFS_DATA_START;
    super.bitmap_block = FADALFS_BITMAP_BLOCK;
    super.inode_start = FADALFS_INODE_START;
    super.inode_blocks = FADALFS_INODE_BLOCKS;
    super.checksum = fs_checksum(&super);
    *(struct fadalfs_superblock *)buffer = super;
    if (blockdev_write(device, 0, buffer) != BLOCKDEV_OK) return FADALFS_IO;

    fs_zero(buffer);
    for (block = 0; block < FADALFS_DATA_START; block++) fs_bitmap_set(buffer, block, 1);
    if (blockdev_write(device, FADALFS_BITMAP_BLOCK, buffer) != BLOCKDEV_OK) return FADALFS_IO;
    fs_zero(buffer);
    for (block = 0; block < FADALFS_INODE_BLOCKS; block++) {
        if (blockdev_write(device, FADALFS_INODE_START + block, buffer) != BLOCKDEV_OK) return FADALFS_IO;
    }

    {
        struct fadalfs_inode root;
        unsigned int index;
        for (index = 0; index < sizeof(root); index++) ((unsigned char *)&root)[index] = 0;
        root.inode_id = FADALFS_ROOT_INODE;
        root.mode = FADALFS_MODE_DIR | 0755U;
        root.uid = 0;
        root.gid = 0;
        if (blockdev_read(device, FADALFS_INODE_START, buffer) != BLOCKDEV_OK) return FADALFS_IO;
        ((struct fadalfs_inode *)buffer)[0] = root;
        if (blockdev_write(device, FADALFS_INODE_START, buffer) != BLOCKDEV_OK) return FADALFS_IO;
    }
    return FADALFS_OK;
}

unsigned int fadalfs_mount(struct fadalfs *fs, struct fadal_blockdev *device) {
    unsigned char buffer[FADALFS_BLOCK_SIZE];
    if (fs == (struct fadalfs *)0 || device == (struct fadal_blockdev *)0 || !device->online) return FADALFS_INVALID;
    fs->device = device;
    fs->mounted = 0;
    if (blockdev_read(device, 0, buffer) != BLOCKDEV_OK) return FADALFS_IO;
    fs->super = *(struct fadalfs_superblock *)buffer;
    if (fs->super.checksum != fs_checksum(&fs->super)) return FADALFS_CORRUPT;
    fs->mounted = 1;
    if (!fs_valid(fs)) {
        fs->mounted = 0;
        return FADALFS_CORRUPT;
    }
    return FADALFS_OK;
}

unsigned int fadalfs_sync(struct fadalfs *fs) {
    unsigned char buffer[FADALFS_BLOCK_SIZE];
    if (!fs_valid(fs)) return FADALFS_INVALID;
    fs->super.checksum = fs_checksum(&fs->super);
    fs_zero(buffer);
    *(struct fadalfs_superblock *)buffer = fs->super;
    return blockdev_write(fs->device, 0, buffer) == BLOCKDEV_OK ? FADALFS_OK : FADALFS_IO;
}

unsigned int fadalfs_inode_read(struct fadalfs *fs, unsigned int inode_id,
                                 struct fadalfs_inode *inode) {
    unsigned char buffer[FADALFS_BLOCK_SIZE];
    unsigned int index;
    if (!fs_valid(fs) || inode == (struct fadalfs_inode *)0 || inode_id == 0 || inode_id > FADALFS_MAX_INODES) return FADALFS_INVALID;
    index = (inode_id - 1U) / FADALFS_INODES_PER_BLOCK;
    if (blockdev_read(fs->device, FADALFS_INODE_START + index, buffer) != BLOCKDEV_OK) return FADALFS_IO;
    *inode = ((struct fadalfs_inode *)buffer)[(inode_id - 1U) % FADALFS_INODES_PER_BLOCK];
    return inode->inode_id == inode_id ? FADALFS_OK : FADALFS_NOTFOUND;
}

unsigned int fadalfs_inode_write(struct fadalfs *fs, const struct fadalfs_inode *inode) {
    unsigned char buffer[FADALFS_BLOCK_SIZE];
    unsigned int index;
    if (!fs_valid(fs) || inode == (const struct fadalfs_inode *)0 || inode->inode_id == 0 || inode->inode_id > FADALFS_MAX_INODES) return FADALFS_INVALID;
    index = (inode->inode_id - 1U) / FADALFS_INODES_PER_BLOCK;
    if (blockdev_read(fs->device, FADALFS_INODE_START + index, buffer) != BLOCKDEV_OK) return FADALFS_IO;
    ((struct fadalfs_inode *)buffer)[(inode->inode_id - 1U) % FADALFS_INODES_PER_BLOCK] = *inode;
    return blockdev_write(fs->device, FADALFS_INODE_START + index, buffer) == BLOCKDEV_OK ? FADALFS_OK : FADALFS_IO;
}

unsigned int fadalfs_alloc_block(struct fadalfs *fs, unsigned int *block) {
    unsigned char bitmap[FADALFS_BLOCK_SIZE];
    unsigned int candidate;
    if (!fs_valid(fs) || block == (unsigned int *)0) return FADALFS_INVALID;
    if (blockdev_read(fs->device, fs->super.bitmap_block, bitmap) != BLOCKDEV_OK) return FADALFS_IO;
    for (candidate = fs->super.data_start; candidate < fs->super.total_blocks; candidate++) {
        if (!fs_bitmap_get(bitmap, candidate)) {
            if (fs->super.free_blocks == 0) return FADALFS_NOSPACE;
            fs_bitmap_set(bitmap, candidate, 1);
            if (blockdev_write(fs->device, fs->super.bitmap_block, bitmap) != BLOCKDEV_OK) return FADALFS_IO;
            fs->super.free_blocks--;
            if (fadalfs_sync(fs) != FADALFS_OK) return FADALFS_IO;
            if (blockdev_zero(fs->device, candidate) != BLOCKDEV_OK) return FADALFS_IO;
            *block = candidate;
            return FADALFS_OK;
        }
    }
    return FADALFS_NOSPACE;
}

unsigned int fadalfs_free_block(struct fadalfs *fs, unsigned int block) {
    unsigned char bitmap[FADALFS_BLOCK_SIZE];
    if (!fs_valid(fs) || block < fs->super.data_start || block >= fs->super.total_blocks) return FADALFS_INVALID;
    if (blockdev_read(fs->device, fs->super.bitmap_block, bitmap) != BLOCKDEV_OK) return FADALFS_IO;
    if (!fs_bitmap_get(bitmap, block)) return FADALFS_INVALID;
    fs_bitmap_set(bitmap, block, 0);
    if (blockdev_write(fs->device, fs->super.bitmap_block, bitmap) != BLOCKDEV_OK) return FADALFS_IO;
    fs->super.free_blocks++;
    return fadalfs_sync(fs);
}

unsigned int fadalfs_read_block(struct fadalfs *fs, unsigned int block, unsigned char *buffer) {
    if (!fs_valid(fs) || buffer == (unsigned char *)0 || block < fs->super.data_start || block >= fs->super.total_blocks) return FADALFS_INVALID;
    return blockdev_read(fs->device, block, buffer) == BLOCKDEV_OK ? FADALFS_OK : FADALFS_IO;
}

unsigned int fadalfs_write_block(struct fadalfs *fs, unsigned int block, const unsigned char *buffer) {
    if (!fs_valid(fs) || buffer == (const unsigned char *)0 || block < fs->super.data_start || block >= fs->super.total_blocks) return FADALFS_INVALID;
    return blockdev_write(fs->device, block, buffer) == BLOCKDEV_OK ? FADALFS_OK : FADALFS_IO;
}
