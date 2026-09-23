#ifndef FADAL_FADALFS_H
#define FADAL_FADALFS_H

#include "blockdev.h"

typedef unsigned long long fadalfs_u64;

#define FADALFS_MAGIC 0x534c4446U
#define FADALFS_VERSION 1U
#define FADALFS_BLOCK_SIZE 512U
#define FADALFS_BITMAP_BLOCK 1U
#define FADALFS_INODE_START 2U
#define FADALFS_INODE_BLOCKS 16U
#define FADALFS_DATA_START (FADALFS_INODE_START + FADALFS_INODE_BLOCKS)
#define FADALFS_ROOT_INODE 1U
#define FADALFS_MAX_INODES 128U
#define FADALFS_MODE_DIR 0x4000U
#define FADALFS_MODE_FILE 0x8000U

#define FADALFS_OK 0U
#define FADALFS_INVALID 1U
#define FADALFS_IO 2U
#define FADALFS_CORRUPT 3U
#define FADALFS_NOSPACE 4U
#define FADALFS_NOTFOUND 5U
#define FADALFS_EXISTS 6U
#define FADALFS_PERM 7U

struct fadalfs_superblock {
    unsigned int magic;
    unsigned int version;
    unsigned int block_size;
    unsigned int total_blocks;
    unsigned int free_blocks;
    unsigned int inode_count;
    unsigned int root_inode;
    unsigned int data_start;
    unsigned int bitmap_block;
    unsigned int inode_start;
    unsigned int inode_blocks;
    unsigned int checksum;
};

struct fadalfs_inode {
    unsigned int inode_id;
    unsigned int mode;
    unsigned int uid;
    unsigned int gid;
    unsigned int size;
    unsigned int first_block;
    unsigned int block_count;
    unsigned int flags;
    fadalfs_u64 created_at;
    fadalfs_u64 modified_at;
    unsigned char reserved[16];
};

struct fadalfs {
    struct fadal_blockdev *device;
    struct fadalfs_superblock super;
    unsigned char mounted;
};

unsigned int fadalfs_format(struct fadal_blockdev *device);
unsigned int fadalfs_mount(struct fadalfs *fs, struct fadal_blockdev *device);
unsigned int fadalfs_sync(struct fadalfs *fs);
unsigned int fadalfs_inode_read(struct fadalfs *fs, unsigned int inode_id,
                                 struct fadalfs_inode *inode);
unsigned int fadalfs_inode_write(struct fadalfs *fs, const struct fadalfs_inode *inode);
unsigned int fadalfs_alloc_block(struct fadalfs *fs, unsigned int *block);
unsigned int fadalfs_free_block(struct fadalfs *fs, unsigned int block);
unsigned int fadalfs_read_block(struct fadalfs *fs, unsigned int block,
                                 unsigned char *buffer);
unsigned int fadalfs_write_block(struct fadalfs *fs, unsigned int block,
                                  const unsigned char *buffer);

#endif
