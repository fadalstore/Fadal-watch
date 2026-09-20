#ifndef FADAL_RAMDISK_H
#define FADAL_RAMDISK_H
typedef unsigned char ramdisk_u8;
typedef unsigned int ramdisk_u32;
#define RAMDISK_SECTOR_SIZE 512
#define RAMDISK_SECTORS 32
void ramdisk_init(void);
ramdisk_u8 ramdisk_read(ramdisk_u32 lba, ramdisk_u8 *data, ramdisk_u32 sectors);
ramdisk_u8 ramdisk_write(ramdisk_u32 lba, const ramdisk_u8 *data, ramdisk_u32 sectors);
ramdisk_u32 ramdisk_checksum(void);
#endif
