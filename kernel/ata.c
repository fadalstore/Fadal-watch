#include "ata.h"

#define ATA_DATA 0x1f0
#define ATA_SECTOR_COUNT 0x1f2
#define ATA_LBA_LOW 0x1f3
#define ATA_LBA_MID 0x1f4
#define ATA_LBA_HIGH 0x1f5
#define ATA_DRIVE 0x1f6
#define ATA_STATUS 0x1f7
#define ATA_COMMAND 0x1f7
#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_IDENTIFY 0xec
#define ATA_STATUS_ERR 0x01
#define ATA_STATUS_DRQ 0x08
#define ATA_STATUS_DF 0x20
#define ATA_STATUS_BSY 0x80
#define ATA_TIMEOUT 1000000
#define ATA_CACHE_ENTRIES 4

struct ata_cache_entry {
    ata_u8 valid;
    ata_u32 lba;
    ata_u8 data[512];
};
static struct ata_cache_entry ata_cache[ATA_CACHE_ENTRIES];
static ata_u32 ata_cache_next;
static ata_u32 ata_cache_hit_count;

static void outb(unsigned short port, ata_u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
static void outw(unsigned short port, unsigned short value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}
static unsigned short inw(unsigned short port) {
    unsigned short value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static ata_u8 inb(unsigned short port) {
    ata_u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static void ata_delay(void) {
    (void)inb(ATA_STATUS); (void)inb(ATA_STATUS);
    (void)inb(ATA_STATUS); (void)inb(ATA_STATUS);
}
static ata_u8 ata_wait_ready(void) {
    for (ata_u32 attempt = 0; attempt < ATA_TIMEOUT; attempt++) {
        ata_u8 status = inb(ATA_STATUS);
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) return 0;
        if ((status & ATA_STATUS_BSY) == 0 && (status & ATA_STATUS_DRQ) != 0) return 1;
    }
    return 0;
}
static struct ata_cache_entry *ata_cache_find(ata_u32 lba) {
    for (ata_u32 index = 0; index < ATA_CACHE_ENTRIES; index++) {
        if (ata_cache[index].valid && ata_cache[index].lba == lba) return &ata_cache[index];
    }
    return (struct ata_cache_entry *)0;
}
static void ata_cache_store(ata_u32 lba, const ata_u8 *data) {
    struct ata_cache_entry *entry = ata_cache_find(lba);
    if (entry == (struct ata_cache_entry *)0) {
        entry = &ata_cache[ata_cache_next];
        ata_cache_next = (ata_cache_next + 1) % ATA_CACHE_ENTRIES;
    }
    entry->valid = 1;
    entry->lba = lba;
    for (ata_u32 index = 0; index < 512; index++) entry->data[index] = data[index];
}

ata_u8 ata_write_sectors(ata_u32 lba, const ata_u8 *data, ata_u32 sectors) {
    if (sectors == 0 || lba > 0x0fffffff || sectors > 256 || lba + sectors > 0x10000000) return 0;
    for (ata_u32 sector_index = 0; sector_index < sectors; sector_index++) {
        ata_u32 current_lba = lba + sector_index;
        const ata_u8 *sector = data + sector_index * 512;
        ata_delay();
        outb(ATA_DRIVE, (ata_u8)(0xe0 | ((current_lba >> 24) & 0x0f)));
        outb(ATA_SECTOR_COUNT, 1);
        outb(ATA_LBA_LOW, (ata_u8)current_lba);
        outb(ATA_LBA_MID, (ata_u8)(current_lba >> 8));
        outb(ATA_LBA_HIGH, (ata_u8)(current_lba >> 16));
        outb(ATA_COMMAND, ATA_CMD_WRITE_SECTORS);
        if (!ata_wait_ready()) return 0;
        for (ata_u32 word = 0; word < 256; word++) {
            outw(ATA_DATA, (unsigned short)sector[word * 2] |
                ((unsigned short)sector[word * 2 + 1] << 8));
        }
        for (ata_u32 attempt = 0; attempt < ATA_TIMEOUT; attempt++) {
            ata_u8 status = inb(ATA_STATUS);
            if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) return 0;
            if ((status & ATA_STATUS_BSY) == 0) break;
            if (attempt + 1 == ATA_TIMEOUT) return 0;
        }
        ata_cache_store(current_lba, sector);
    }
    return 1;
}

ata_u8 ata_read_sectors(ata_u32 lba, ata_u8 *data, ata_u32 sectors) {
    if (sectors == 0 || lba > 0x0fffffff || sectors > 256 || lba + sectors > 0x10000000) return 0;
    for (ata_u32 sector_index = 0; sector_index < sectors; sector_index++) {
        ata_u32 current_lba = lba + sector_index;
        struct ata_cache_entry *cached = ata_cache_find(current_lba);
        if (cached != (struct ata_cache_entry *)0) {
            for (ata_u32 index = 0; index < 512; index++) data[sector_index * 512 + index] = cached->data[index];
            ata_cache_hit_count++;
            continue;
        }
        ata_delay();
        outb(ATA_DRIVE, (ata_u8)(0xe0 | ((current_lba >> 24) & 0x0f)));
        outb(ATA_SECTOR_COUNT, 1);
        outb(ATA_LBA_LOW, (ata_u8)current_lba);
        outb(ATA_LBA_MID, (ata_u8)(current_lba >> 8));
        outb(ATA_LBA_HIGH, (ata_u8)(current_lba >> 16));
        outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);
        if (!ata_wait_ready()) return 0;
        ata_u8 *sector = data + sector_index * 512;
        for (ata_u32 word = 0; word < 256; word++) {
            unsigned short value = inw(ATA_DATA);
            sector[word * 2] = (ata_u8)value;
            sector[word * 2 + 1] = (ata_u8)(value >> 8);
        }
        ata_cache_store(current_lba, sector);
    }
    return 1;
}

ata_u8 ata_identify(ata_u8 *data) {
    if (data == (ata_u8 *)0) return 0;
    ata_delay(); outb(ATA_DRIVE, 0xe0); outb(ATA_SECTOR_COUNT, 0);
    outb(ATA_LBA_LOW, 0); outb(ATA_LBA_MID, 0); outb(ATA_LBA_HIGH, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    for (ata_u32 attempt = 0; attempt < ATA_TIMEOUT; attempt++) {
        ata_u8 status = inb(ATA_STATUS);
        if (status == 0 || (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) != 0) return 0;
        if ((status & ATA_STATUS_BSY) == 0 && (status & ATA_STATUS_DRQ) != 0) {
            for (ata_u32 word = 0; word < 256; word++) {
                unsigned short value = inw(ATA_DATA);
                data[word * 2] = (ata_u8)value;
                data[word * 2 + 1] = (ata_u8)(value >> 8);
            }
            return 1;
        }
    }
    return 0;
}
ata_u32 ata_cache_hits(void) { return ata_cache_hit_count; }
void ata_cache_reset(void) {
    for (ata_u32 index = 0; index < ATA_CACHE_ENTRIES; index++) ata_cache[index].valid = 0;
    ata_cache_next = 0;
    ata_cache_hit_count = 0;
}
