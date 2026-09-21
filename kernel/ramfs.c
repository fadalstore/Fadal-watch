#include "ramfs.h"
#include "ramdisk.h"

#define RAMFS_MAGIC 0x534d4152
#define RAMFS_VERSION 1
#define RAMFS_HEADER_SECTOR 0
#define RAMFS_FIRST_SLOT_SECTOR 1
#define RAMFS_SLOT_COUNT 7
#define RAMFS_SLOT_SECTORS 4
#define RAMFS_METADATA_SECTOR 1
#define RAMFS_DATA_SECTORS (RAMFS_SLOT_SECTORS - 1)
#define RAMFS_MAX_FILE_SIZE (RAMFS_DATA_SECTORS * RAMDISK_SECTOR_SIZE)
#define RAMFS_NAME_SIZE 32

struct ramfs_header {
    vfs_u32 magic;
    vfs_u32 version;
    vfs_u32 entries;
    vfs_u32 reserved;
};

struct ramfs_record {
    char name[RAMFS_NAME_SIZE];
    vfs_u32 size;
    vfs_u32 used;
    vfs_u8 reserved[RAMDISK_SECTOR_SIZE - RAMFS_NAME_SIZE - 8];
};

static vfs_u8 mounted;
static vfs_u32 entries;

static void zero_bytes(vfs_u8 *data, vfs_u32 size) {
    for (vfs_u32 index = 0; index < size; index++) {
        data[index] = 0;
    }
}

static vfs_u8 string_equal(const char *left, const char *right) {
    vfs_u32 index = 0;
    if (left == (const char *)0 || right == (const char *)0) {
        return 0;
    }
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return 0;
        }
        index++;
    }
    return left[index] == right[index];
}

static vfs_u32 string_length(const char *value) {
    vfs_u32 length = 0;
    if (value == (const char *)0) {
        return 0;
    }
    while (value[length] != '\0') {
        length++;
    }
    return length;
}

static vfs_u8 valid_name(const char *name) {
    vfs_u32 length = string_length(name);
    return name != (const char *)0 && length > 0 && length < RAMFS_NAME_SIZE;
}

static vfs_u32 metadata_sector(vfs_u32 slot) {
    return RAMFS_METADATA_SECTOR + slot * RAMFS_SLOT_SECTORS;
}

static vfs_u32 data_sector(vfs_u32 slot) {
    return metadata_sector(slot) + 1;
}

static vfs_u8 read_record(vfs_u32 slot, struct ramfs_record *record) {
    return ramdisk_read(metadata_sector(slot), (ramdisk_u8 *)record, 1);
}

static vfs_u8 write_record(vfs_u32 slot, const struct ramfs_record *record) {
    return ramdisk_write(metadata_sector(slot), (const ramdisk_u8 *)record, 1);
}

static vfs_u8 write_header(void) {
    vfs_u8 sector[RAMDISK_SECTOR_SIZE];
    struct ramfs_header *header = (struct ramfs_header *)sector;
    zero_bytes(sector, sizeof(sector));
    header->magic = RAMFS_MAGIC;
    header->version = RAMFS_VERSION;
    header->entries = entries;
    return ramdisk_write(RAMFS_HEADER_SECTOR, sector, 1);
}

static vfs_u8 find_record(const char *name, vfs_u32 *slot, struct ramfs_record *record) {
    for (vfs_u32 index = 0; index < RAMFS_SLOT_COUNT; index++) {
        if (!read_record(index, record)) {
            return 0;
        }
        if (record->used && string_equal(record->name, name)) {
            *slot = index;
            return 1;
        }
    }
    return 0;
}

static vfs_u8 find_free_record(vfs_u32 *slot, struct ramfs_record *record) {
    for (vfs_u32 index = 0; index < RAMFS_SLOT_COUNT; index++) {
        if (!read_record(index, record)) {
            return 0;
        }
        if (!record->used) {
            *slot = index;
            return 1;
        }
    }
    return 0;
}

vfs_u8 ramfs_mount(void) {
    vfs_u8 sector[RAMDISK_SECTOR_SIZE];
    struct ramfs_header *header = (struct ramfs_header *)sector;

    if (!ramdisk_read(RAMFS_HEADER_SECTOR, sector, 1)) {
        mounted = 0;
        return 0;
    }
    if (header->magic != RAMFS_MAGIC || header->version != RAMFS_VERSION ||
        header->entries > RAMFS_SLOT_COUNT) {
        entries = 0;
        mounted = write_header();
        return mounted;
    }
    entries = header->entries;
    mounted = 1;
    return 1;
}

void ramfs_unmount(void) {
    mounted = 0;
}

vfs_u32 ramfs_root_entries(void) {
    return mounted ? entries : 0;
}

vfs_u32 ramfs_write_file(const char *name, const vfs_u8 *data, vfs_u32 size) {
    struct ramfs_record record;
    vfs_u32 slot;
    vfs_u8 existing;

    if (!mounted || !valid_name(name) || data == (const vfs_u8 *)0 ||
        size > RAMFS_MAX_FILE_SIZE) {
        return 0;
    }
    existing = find_record(name, &slot, &record);
    if (!existing && !find_free_record(&slot, &record)) {
        return 0;
    }
    if (!existing) {
        entries++;
    }
    zero_bytes((vfs_u8 *)&record, sizeof(record));
    for (vfs_u32 index = 0; index < string_length(name); index++) {
        record.name[index] = name[index];
    }
    record.size = size;
    record.used = 1;
    for (vfs_u32 sector = 0; sector < RAMFS_DATA_SECTORS; sector++) {
        vfs_u8 buffer[RAMDISK_SECTOR_SIZE];
        vfs_u32 offset = sector * RAMDISK_SECTOR_SIZE;
        vfs_u32 chunk = size > offset ? size - offset : 0;
        if (chunk > RAMDISK_SECTOR_SIZE) {
            chunk = RAMDISK_SECTOR_SIZE;
        }
        zero_bytes(buffer, sizeof(buffer));
        for (vfs_u32 index = 0; index < chunk; index++) {
            buffer[index] = data[offset + index];
        }
        if (!ramdisk_write(data_sector(slot) + sector, buffer, 1)) {
            return 0;
        }
    }
    if (!write_record(slot, &record) || !write_header()) {
        return 0;
    }
    return size;
}

vfs_u8 ramfs_read_file(const char *name, vfs_u8 *output, vfs_u32 capacity, vfs_u32 *size) {
    struct ramfs_record record;
    vfs_u32 slot;
    if (!mounted || !valid_name(name) || output == (vfs_u8 *)0 ||
        size == (vfs_u32 *)0 || !find_record(name, &slot, &record) ||
        record.size > capacity) {
        return 0;
    }
    for (vfs_u32 sector = 0; sector < RAMFS_DATA_SECTORS; sector++) {
        vfs_u8 buffer[RAMDISK_SECTOR_SIZE];
        vfs_u32 offset = sector * RAMDISK_SECTOR_SIZE;
        vfs_u32 chunk = record.size > offset ? record.size - offset : 0;
        if (chunk > RAMDISK_SECTOR_SIZE) {
            chunk = RAMDISK_SECTOR_SIZE;
        }
        if (!ramdisk_read(data_sector(slot) + sector, buffer, 1)) {
            return 0;
        }
        for (vfs_u32 index = 0; index < chunk; index++) {
            output[offset + index] = buffer[index];
        }
    }
    *size = record.size;
    return 1;
}
