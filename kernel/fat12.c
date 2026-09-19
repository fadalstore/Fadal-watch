#include "fat12.h"

#define FAT12_SECTOR_SIZE 512
#define FAT12_TOTAL_SECTORS 64
#define FAT12_RESERVED_SECTORS 1
#define FAT12_FAT_COUNT 2
#define FAT12_SECTORS_PER_FAT 1
#define FAT12_ROOT_ENTRIES 16
#define FAT12_ROOT_SECTORS ((FAT12_ROOT_ENTRIES * 32 + FAT12_SECTOR_SIZE - 1) / FAT12_SECTOR_SIZE)
#define FAT12_DATA_START (FAT12_RESERVED_SECTORS + FAT12_FAT_COUNT * FAT12_SECTORS_PER_FAT + FAT12_ROOT_SECTORS)
#define FAT12_MAX_CLUSTER (FAT12_TOTAL_SECTORS - FAT12_DATA_START + 1)
#define FAT12_EOC 0x0fff
#define FAT12_FREE 0x0000
#define FAT12_ATTR_ARCHIVE 0x20

typedef unsigned short fat12_u16;

struct fat12_dirent {
    fat12_u8 name[11];
    fat12_u8 attributes;
    fat12_u8 reserved;
    fat12_u8 create_time_tenths;
    fat12_u16 create_time;
    fat12_u16 create_date;
    fat12_u16 access_date;
    fat12_u16 first_cluster_high;
    fat12_u16 write_time;
    fat12_u16 write_date;
    fat12_u16 first_cluster;
    fat12_u32 size;
} __attribute__((packed));

static fat12_u8 volume[FAT12_TOTAL_SECTORS * FAT12_SECTOR_SIZE];
static fat12_u32 last_allocated_clusters;

static void zero_bytes(fat12_u8 *target, fat12_u32 count) {
    for (fat12_u32 index = 0; index < count; index++) {
        target[index] = 0;
    }
}

static void copy_bytes(fat12_u8 *target, const fat12_u8 *source, fat12_u32 count) {
    for (fat12_u32 index = 0; index < count; index++) {
        target[index] = source[index];
    }
}

static fat12_u8 upper_ascii(fat12_u8 value) {
    if (value >= 'a' && value <= 'z') {
        return (fat12_u8)(value - 'a' + 'A');
    }
    return value;
}

static fat12_u8 *fat_table(fat12_u32 copy) {
    return &volume[(FAT12_RESERVED_SECTORS + copy * FAT12_SECTORS_PER_FAT) * FAT12_SECTOR_SIZE];
}

static fat12_u16 fat_get(const fat12_u8 *fat, fat12_u16 cluster) {
    fat12_u32 offset = cluster + cluster / 2;
    fat12_u16 value = (fat12_u16)fat[offset] | ((fat12_u16)fat[offset + 1] << 8);
    if (cluster & 1) {
        return (fat12_u16)(value >> 4) & 0x0fff;
    }
    return value & 0x0fff;
}

static void fat_set(fat12_u8 *fat, fat12_u16 cluster, fat12_u16 value) {
    fat12_u32 offset = cluster + cluster / 2;
    fat12_u16 current = (fat12_u16)fat[offset] | ((fat12_u16)fat[offset + 1] << 8);
    value &= 0x0fff;
    if (cluster & 1) {
        current = (fat12_u16)((current & 0x000f) | (value << 4));
    } else {
        current = (fat12_u16)((current & 0xf000) | value);
    }
    fat[offset] = (fat12_u8)(current & 0xff);
    fat[offset + 1] = (fat12_u8)(current >> 8);
}

static fat12_u32 cluster_offset(fat12_u16 cluster) {
    return (FAT12_DATA_START + cluster - 2) * FAT12_SECTOR_SIZE;
}

static fat12_u16 find_free_cluster(const fat12_u8 *fat) {
    for (fat12_u16 cluster = 2; cluster <= FAT12_MAX_CLUSTER; cluster++) {
        if (fat_get(fat, cluster) == FAT12_FREE) {
            return cluster;
        }
    }
    return 0;
}

static fat12_u8 make_short_name(const char *source, fat12_u8 target[11]) {
    fat12_u32 index = 0;
    fat12_u32 target_index = 0;
    for (fat12_u32 slot = 0; slot < 11; slot++) {
        target[slot] = ' ';
    }
    while (source[index] != '\0' && source[index] != '.' && target_index < 8) {
        target[target_index++] = upper_ascii((fat12_u8)source[index++]);
    }
    if (source[index] == '.') {
        index++;
    }
    target_index = 8;
    while (source[index] != '\0' && target_index < 11) {
        target[target_index++] = upper_ascii((fat12_u8)source[index++]);
    }
    return source[index] == '\0';
}

void fat12_format(void) {
    zero_bytes(volume, sizeof(volume));
    volume[0] = 0xeb;
    volume[1] = 0x3c;
    volume[2] = 0x90;
    copy_bytes(&volume[3], (const fat12_u8 *)"FADAL12 ", 8);
    volume[11] = 0x00;
    volume[12] = 0x02;
    volume[13] = 0x01;
    volume[14] = FAT12_RESERVED_SECTORS;
    volume[16] = FAT12_FAT_COUNT;
    volume[17] = FAT12_ROOT_ENTRIES;
    volume[19] = FAT12_TOTAL_SECTORS;
    volume[21] = 0xf0;
    volume[22] = FAT12_SECTORS_PER_FAT;
    volume[24] = 1;
    volume[26] = 1;
    volume[38] = 0x29;
    volume[43] = 'F';
    volume[44] = 'A';
    volume[45] = 'D';
    volume[46] = 'A';
    volume[47] = 'L';
    volume[48] = ' '; 
    volume[49] = 'F';
    volume[50] = 'A';
    volume[51] = 'T';
    volume[52] = '1';
    volume[53] = '2';
    volume[54] = ' '; 
    volume[55] = ' '; 
    volume[56] = ' '; 
    volume[510] = 0x55;
    volume[511] = 0xaa;
    for (fat12_u32 copy = 0; copy < FAT12_FAT_COUNT; copy++) {
        fat12_u8 *fat = fat_table(copy);
        fat[0] = 0xf0;
        fat[1] = 0xff;
        fat[2] = 0xff;
    }
    last_allocated_clusters = 0;
}

fat12_u32 fat12_write_file(const char *name, const fat12_u8 *data, fat12_u32 size) {
    fat12_u8 short_name[11];
    if (!make_short_name(name, short_name) || size > (FAT12_MAX_CLUSTER - 1) * FAT12_SECTOR_SIZE) {
        return 0;
    }
    fat12_u8 *fat = fat_table(0);
    fat12_u32 root_offset = (FAT12_RESERVED_SECTORS + FAT12_FAT_COUNT * FAT12_SECTORS_PER_FAT) * FAT12_SECTOR_SIZE;
    struct fat12_dirent *entry = (struct fat12_dirent *)0;
    for (fat12_u32 index = 0; index < FAT12_ROOT_ENTRIES; index++) {
        struct fat12_dirent *candidate = (struct fat12_dirent *)(volume + root_offset + index * sizeof(struct fat12_dirent));
        if (candidate->name[0] == 0x00 || candidate->name[0] == 0xe5) {
            entry = candidate;
            break;
        }
    }
    if (entry == (struct fat12_dirent *)0) {
        return 0;
    }

    fat12_u32 clusters_needed = (size + FAT12_SECTOR_SIZE - 1) / FAT12_SECTOR_SIZE;
    fat12_u16 first_cluster = 0;
    fat12_u16 previous_cluster = 0;
    fat12_u32 copied = 0;
    for (fat12_u32 index = 0; index < clusters_needed; index++) {
        fat12_u16 cluster = find_free_cluster(fat);
        if (cluster == 0) {
            return 0;
        }
        if (first_cluster == 0) {
            first_cluster = cluster;
        }
        if (previous_cluster != 0) {
            fat_set(fat, previous_cluster, cluster);
        }
        fat_set(fat, cluster, FAT12_EOC);
        fat12_u32 chunk = size - copied;
        if (chunk > FAT12_SECTOR_SIZE) {
            chunk = FAT12_SECTOR_SIZE;
        }
        copy_bytes(volume + cluster_offset(cluster), data + copied, chunk);
        if (chunk < FAT12_SECTOR_SIZE) {
            zero_bytes(volume + cluster_offset(cluster) + chunk, FAT12_SECTOR_SIZE - chunk);
        }
        copied += chunk;
        previous_cluster = cluster;
    }
    for (fat12_u32 copy = 1; copy < FAT12_FAT_COUNT; copy++) {
        copy_bytes(fat_table(copy), fat, FAT12_SECTORS_PER_FAT * FAT12_SECTOR_SIZE);
    }
    zero_bytes((fat12_u8 *)entry, sizeof(struct fat12_dirent));
    copy_bytes(entry->name, short_name, sizeof(entry->name));
    entry->attributes = FAT12_ATTR_ARCHIVE;
    entry->first_cluster = first_cluster;
    entry->size = size;
    last_allocated_clusters = clusters_needed;
    return clusters_needed;
}

fat12_u32 fat12_free_clusters(void) {
    const fat12_u8 *fat = fat_table(0);
    fat12_u32 free = 0;
    for (fat12_u16 cluster = 2; cluster <= FAT12_MAX_CLUSTER; cluster++) {
        if (fat_get(fat, cluster) == FAT12_FREE) {
            free++;
        }
    }
    return free;
}

fat12_u32 fat12_last_allocated_clusters(void) {
    return last_allocated_clusters;
}

const fat12_u8 *fat12_volume(void) {
    return volume;
}

fat12_u32 fat12_volume_sectors(void) {
    return FAT12_TOTAL_SECTORS;
}

fat12_u8 *fat12_volume_buffer(void) {
    return volume;
}

fat12_u8 fat12_read_file(const char *name, fat12_u8 *output, fat12_u32 capacity, fat12_u32 *size) {
    fat12_u8 short_name[11];
    if (!make_short_name(name, short_name)) {
        return 0;
    }
    fat12_u32 root_offset = (FAT12_RESERVED_SECTORS + FAT12_FAT_COUNT * FAT12_SECTORS_PER_FAT) * FAT12_SECTOR_SIZE;
    struct fat12_dirent *entry = (struct fat12_dirent *)0;
    for (fat12_u32 index = 0; index < FAT12_ROOT_ENTRIES; index++) {
        struct fat12_dirent *candidate = (struct fat12_dirent *)(volume + root_offset + index * sizeof(struct fat12_dirent));
        fat12_u8 matches = 1;
        for (fat12_u32 character = 0; character < sizeof(candidate->name); character++) {
            if (candidate->name[character] != short_name[character]) {
                matches = 0;
                break;
            }
        }
        if (matches) {
            entry = candidate;
            break;
        }
    }
    if (entry == (struct fat12_dirent *)0 || entry->size > capacity) {
        return 0;
    }
    const fat12_u8 *fat = fat_table(0);
    fat12_u32 copied = 0;
    fat12_u32 remaining = entry->size;
    fat12_u16 cluster = entry->first_cluster;
    while (remaining > 0 && cluster >= 2 && cluster <= FAT12_MAX_CLUSTER) {
        fat12_u32 chunk = remaining > FAT12_SECTOR_SIZE ? FAT12_SECTOR_SIZE : remaining;
        copy_bytes(output + copied, volume + cluster_offset(cluster), chunk);
        copied += chunk;
        remaining -= chunk;
        if (remaining == 0) {
            break;
        }
        cluster = fat_get(fat, cluster);
    }
    if (remaining != 0) {
        return 0;
    }
    *size = entry->size;
    return 1;
}
