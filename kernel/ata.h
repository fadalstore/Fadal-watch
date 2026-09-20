#ifndef FADAL_ATA_H
#define FADAL_ATA_H

typedef unsigned char ata_u8;
typedef unsigned int ata_u32;

ata_u8 ata_write_sectors(ata_u32 lba, const ata_u8 *data, ata_u32 sectors);
ata_u8 ata_read_sectors(ata_u32 lba, ata_u8 *data, ata_u32 sectors);
ata_u8 ata_identify(ata_u8 *data);
ata_u32 ata_cache_hits(void);
void ata_cache_reset(void);

#endif
