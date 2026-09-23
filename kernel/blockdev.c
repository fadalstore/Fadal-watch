#include "blockdev.h"

static void blockdev_clear(ata_u8 *buffer) {
    ata_u32 index;
    for (index = 0; index < BLOCKDEV_SECTOR_SIZE; index++) buffer[index] = 0;
}

ata_u8 blockdev_bind(struct fadal_blockdev *device, ata_u32 start_lba,
                     ata_u32 sector_count) {
    if (device == (struct fadal_blockdev *)0 || sector_count == 0) return BLOCKDEV_INVALID;
    if (start_lba > 0x0fffffff || sector_count > 0x10000000U - start_lba) return BLOCKDEV_RANGE;
    device->start_lba = start_lba;
    device->sector_count = sector_count;
    device->online = 1;
    return BLOCKDEV_OK;
}

ata_u8 blockdev_read(const struct fadal_blockdev *device, ata_u32 block,
                     ata_u8 *buffer) {
    ata_u32 lba;
    if (device == (const struct fadal_blockdev *)0 || buffer == (ata_u8 *)0 || !device->online) return BLOCKDEV_INVALID;
    if (block >= device->sector_count) return BLOCKDEV_RANGE;
    lba = device->start_lba + block;
    if (!ata_read_sectors(lba, buffer, 1)) return BLOCKDEV_IO;
    return BLOCKDEV_OK;
}

ata_u8 blockdev_write(const struct fadal_blockdev *device, ata_u32 block,
                      const ata_u8 *buffer) {
    ata_u32 lba;
    if (device == (const struct fadal_blockdev *)0 || buffer == (const ata_u8 *)0 || !device->online) return BLOCKDEV_INVALID;
    if (block >= device->sector_count) return BLOCKDEV_RANGE;
    lba = device->start_lba + block;
    if (!ata_write_sectors(lba, buffer, 1)) return BLOCKDEV_IO;
    return BLOCKDEV_OK;
}

ata_u8 blockdev_zero(const struct fadal_blockdev *device, ata_u32 block) {
    ata_u8 buffer[BLOCKDEV_SECTOR_SIZE];
    blockdev_clear(buffer);
    return blockdev_write(device, block, buffer);
}
