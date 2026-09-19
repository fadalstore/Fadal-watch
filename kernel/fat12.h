#ifndef FADAL_FAT12_H
#define FADAL_FAT12_H

typedef unsigned char fat12_u8;
typedef unsigned int fat12_u32;

void fat12_format(void);
fat12_u32 fat12_write_file(const char *name, const fat12_u8 *data, fat12_u32 size);
fat12_u32 fat12_free_clusters(void);
fat12_u32 fat12_last_allocated_clusters(void);
const fat12_u8 *fat12_volume(void);
fat12_u32 fat12_volume_sectors(void);
fat12_u8 *fat12_volume_buffer(void);
fat12_u8 fat12_read_file(const char *name, fat12_u8 *output, fat12_u32 capacity, fat12_u32 *size);

#endif
