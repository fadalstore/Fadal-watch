#include "ramdisk.h"
static ramdisk_u8 ramdisk_storage[RAMDISK_SECTORS * RAMDISK_SECTOR_SIZE];
void ramdisk_init(void) {
    for (ramdisk_u32 index = 0; index < sizeof(ramdisk_storage); index++) ramdisk_storage[index] = 0;
}
ramdisk_u8 ramdisk_read(ramdisk_u32 lba, ramdisk_u8 *data, ramdisk_u32 sectors) {
    if (data == (ramdisk_u8 *)0 || sectors == 0 || lba >= RAMDISK_SECTORS || sectors > RAMDISK_SECTORS - lba) return 0;
    for (ramdisk_u32 index = 0; index < sectors * RAMDISK_SECTOR_SIZE; index++) data[index] = ramdisk_storage[lba * RAMDISK_SECTOR_SIZE + index];
    return 1;
}
ramdisk_u8 ramdisk_write(ramdisk_u32 lba, const ramdisk_u8 *data, ramdisk_u32 sectors) {
    if (data == (const ramdisk_u8 *)0 || sectors == 0 || lba >= RAMDISK_SECTORS || sectors > RAMDISK_SECTORS - lba) return 0;
    for (ramdisk_u32 index = 0; index < sectors * RAMDISK_SECTOR_SIZE; index++) ramdisk_storage[lba * RAMDISK_SECTOR_SIZE + index] = data[index];
    return 1;
}
ramdisk_u32 ramdisk_checksum(void) {
    ramdisk_u32 checksum = 0;
    for (ramdisk_u32 index = 0; index < sizeof(ramdisk_storage); index++) checksum = (checksum << 5) - checksum + ramdisk_storage[index];
    return checksum;
}
