#ifndef FADAL_UEFI_PAGE_TABLES_H
#define FADAL_UEFI_PAGE_TABLES_H

#include <efi.h>

#define FADAL_X86_PAGE_SIZE 4096ULL
#define FADAL_X86_MAX_SPLITS 64U
#define FADAL_X86_PTE_PRESENT (1ULL << 0)
#define FADAL_X86_PTE_WRITE (1ULL << 1)
#define FADAL_X86_PTE_NX (1ULL << 63)
#define FADAL_X86_PTE_ADDRESS_MASK 0x000ffffffffff000ULL

typedef struct {
    volatile UINT64 *ParentEntry;
    UINT64 OriginalEntry;
    EFI_PHYSICAL_ADDRESS TablePage;
} FADAL_X86_TABLE_SPLIT;

typedef struct {
    FADAL_X86_TABLE_SPLIT Splits[FADAL_X86_MAX_SPLITS];
    UINTN SplitCount;
} FADAL_X86_PAGE_TABLE_CONTEXT;

typedef struct {
    UINT64 VirtualAddress;
    volatile UINT64 *Entries[4];
    UINT64 OriginalEntries[4];
} FADAL_X86_PAGE_MAPPING;

EFI_STATUS fadal_x86_enable_nx(VOID);
void fadal_x86_enable_write_protect(VOID);
EFI_STATUS fadal_x86_walk_page(EFI_BOOT_SERVICES *boot_services,
                               FADAL_X86_PAGE_TABLE_CONTEXT *context,
                               UINT64 virtual_address,
                               FADAL_X86_PAGE_MAPPING *mapping);
EFI_STATUS fadal_x86_check_page_permissions(
    const FADAL_X86_PAGE_MAPPING *mapping,
    UINT64 set_leaf_flags,
    UINT64 clear_leaf_flags);
EFI_STATUS fadal_x86_set_page_permissions(FADAL_X86_PAGE_MAPPING *mapping,
                                          UINT64 set_leaf_flags,
                                          UINT64 clear_leaf_flags);
void fadal_x86_restore_leaf_permissions(FADAL_X86_PAGE_MAPPING *mappings,
                                        UINTN mapping_count);
void fadal_x86_restore_splits(EFI_BOOT_SERVICES *boot_services,
                              FADAL_X86_PAGE_TABLE_CONTEXT *context);
void fadal_x86_restore_page(EFI_BOOT_SERVICES *boot_services,
                            FADAL_X86_PAGE_TABLE_CONTEXT *context,
                            FADAL_X86_PAGE_MAPPING *mapping);

#endif
