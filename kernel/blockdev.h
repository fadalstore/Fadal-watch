#ifndef FADAL_BLOCKDEV_H
#define FADAL_BLOCKDEV_H

#include "ata.h"

#define BLOCKDEV_SECTOR_SIZE 512U
#define BLOCKDEV_OK 0U
#define BLOCKDEV_INVALID 1U
#define BLOCKDEV_RANGE 2U
#define BLOCKDEV_IO 3U

struct fadal_blockdev {
    ata_u32 start_lba;
    ata_u32 sector_count;
    ata_u8 online;
};

ata_u8 blockdev_bind(struct fadal_blockdev *device, ata_u32 start_lba,
                     ata_u32 sector_count);
ata_u8 blockdev_read(const struct fadal_blockdev *device, ata_u32 block,
                    ata_u8 *buffer);
ata_u8 blockdev_write(const struct fadal_blockdev *device, ata_u32 block,
                     const ata_u8 *buffer);
ata_u8 blockdev_zero(const struct fadal_blockdev *device, ata_u32 block);

#endif
