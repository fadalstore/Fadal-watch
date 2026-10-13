#include <efi.h>
#include <efilib.h>
#include "page_tables.h"

#define X86_CR0_WP (1ULL << 16)
#define X86_CR4_LA57 (1ULL << 12)
#define X86_EFER_MSR 0xc0000080U
#define X86_EFER_NXE (1ULL << 11)
#define X86_PTE_USER (1ULL << 2)
#define X86_PTE_PWT (1ULL << 3)
#define X86_PTE_PCD (1ULL << 4)
#define X86_PTE_ACCESSED (1ULL << 5)
#define X86_PTE_DIRTY (1ULL << 6)
#define X86_PTE_LARGE_PAT (1ULL << 12)
#define X86_PTE_PS_OR_4K_PAT (1ULL << 7)
#define X86_PTE_GLOBAL (1ULL << 8)
#define X86_PTE_SOFTWARE (0x7ULL << 9)
#define X86_PTE_PKEY (0xfULL << 59)
#define X86_PARENT_PERMISSION_FLAGS (FADAL_X86_PTE_PRESENT | \
    FADAL_X86_PTE_WRITE | X86_PTE_USER | X86_PTE_PWT | X86_PTE_PCD | \
    X86_PTE_ACCESSED | X86_PTE_SOFTWARE | FADAL_X86_PTE_NX)
#define X86_PTE_LEAF_FLAGS (FADAL_X86_PTE_PRESENT | FADAL_X86_PTE_WRITE | \
    X86_PTE_USER | X86_PTE_PWT | X86_PTE_PCD | X86_PTE_ACCESSED | \
    X86_PTE_DIRTY | X86_PTE_GLOBAL | X86_PTE_SOFTWARE | X86_PTE_PKEY | \
    FADAL_X86_PTE_NX)
#define X86_PARENT_FLAGS (X86_PTE_USER | FADAL_X86_PTE_NX | X86_PTE_SOFTWARE)
#define X86_PDPE_1G_ADDRESS_MASK 0x000fffffc0000000ULL
#define X86_PDE_2M_ADDRESS_MASK 0x000fffffffe00000ULL

static UINT64 read_cr0(void) {
    UINT64 value;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(value));
    return value;
}

static void write_cr0(UINT64 value) {
    __asm__ volatile ("mov %0, %%cr0" : : "r"(value) : "memory");
}

static UINT64 read_cr3(void) {
    UINT64 value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

static UINT64 read_cr4(void) {
    UINT64 value;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(value));
    return value;
}

static void flush_page_tables(void) {
    UINT64 current_cr3 = read_cr3();
    __asm__ volatile ("mov %0, %%cr3" : : "r"(current_cr3) : "memory");
}

static void flush_translation(UINT64 virtual_address) {
    __asm__ volatile ("invlpg (%0)" : : "r"((VOID *)(UINTN)virtual_address) : "memory");
}

static UINT64 combine_parent_constraints(UINT64 left, UINT64 right) {
    UINT64 flags = FADAL_X86_PTE_PRESENT;
    if ((left & FADAL_X86_PTE_WRITE) != 0 &&
        (right & FADAL_X86_PTE_WRITE) != 0) flags |= FADAL_X86_PTE_WRITE;
    if ((left & X86_PTE_USER) != 0 && (right & X86_PTE_USER) != 0) {
        flags |= X86_PTE_USER;
    }
    flags |= (left | right) &
             (X86_PTE_PWT | X86_PTE_PCD | X86_PTE_ACCESSED |
              X86_PTE_SOFTWARE | FADAL_X86_PTE_NX);
    return flags;
}

EFI_STATUS fadal_x86_enable_nx(VOID) {
    UINT32 eax = 0x80000000U;
    UINT32 ebx;
    UINT32 ecx;
    UINT32 edx;
    UINT32 low;
    UINT32 high;

    __asm__ volatile ("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    if (eax < 0x80000001U) return EFI_UNSUPPORTED;
    eax = 0x80000001U;
    __asm__ volatile ("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    if ((edx & (1U << 20)) == 0) return EFI_UNSUPPORTED;

    eax = X86_EFER_MSR;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(eax));
    if ((low & X86_EFER_NXE) == 0) {
        low |= X86_EFER_NXE;
        eax = low;
        __asm__ volatile ("wrmsr" : : "c"(X86_EFER_MSR), "a"(eax), "d"(high) : "memory");
    }
    return EFI_SUCCESS;
}

void fadal_x86_enable_write_protect(VOID) {
    write_cr0(read_cr0() | X86_CR0_WP);
}

static EFI_STATUS allocate_table_page(EFI_BOOT_SERVICES *boot_services,
                                      FADAL_X86_PAGE_TABLE_CONTEXT *context,
                                      EFI_PHYSICAL_ADDRESS *page) {
    EFI_STATUS status;

    if (context->SplitCount >= FADAL_X86_MAX_SPLITS) return EFI_OUT_OF_RESOURCES;
    status = uefi_call_wrapper(boot_services->AllocatePages, 4,
                               AllocateAnyPages, EfiLoaderData, 1, page);
    return status;
}

static EFI_STATUS split_large_mapping(EFI_BOOT_SERVICES *boot_services,
                                      FADAL_X86_PAGE_TABLE_CONTEXT *context,
                                      volatile UINT64 *entry, UINTN shift,
                                      UINT64 virtual_address,
                                      UINT64 ancestor_constraints) {
    EFI_STATUS status;
    UINT64 old_entry = *entry;
    EFI_PHYSICAL_ADDRESS table_page = 0;
    volatile UINT64 *table;
    UINT64 common_flags = old_entry & X86_PTE_LEAF_FLAGS;
    UINT64 previous_cr0;
    UINTN index;

    if ((old_entry & X86_PTE_PKEY) != 0) return EFI_UNSUPPORTED;
    status = allocate_table_page(boot_services, context, &table_page);
    if (EFI_ERROR(status)) return status;
    table = (volatile UINT64 *)(UINTN)table_page;
    previous_cr0 = read_cr0();
    write_cr0(previous_cr0 & ~X86_CR0_WP);

    if (shift == 30U) {
        UINT64 base = old_entry & X86_PDPE_1G_ADDRESS_MASK;
        UINT64 large_pat = old_entry & X86_PTE_LARGE_PAT;
        for (index = 0; index < 512; index++) {
            table[index] = (base + ((UINT64)index << 21)) |
                           common_flags | X86_PTE_PS_OR_4K_PAT | large_pat;
        }
    } else if (shift == 21U) {
        UINT64 base = old_entry & X86_PDE_2M_ADDRESS_MASK;
        UINT64 pat = (old_entry & X86_PTE_LARGE_PAT) != 0 ?
                     X86_PTE_PS_OR_4K_PAT : 0;
        for (index = 0; index < 512; index++) {
            table[index] = (base + ((UINT64)index << 12)) |
                           common_flags | pat;
        }
    } else {
        write_cr0(previous_cr0);
        uefi_call_wrapper(boot_services->FreePages, 2, table_page, 1);
        return EFI_INVALID_PARAMETER;
    }

    context->Splits[context->SplitCount].ParentEntry = entry;
    context->Splits[context->SplitCount].OriginalEntry = old_entry;
    context->Splits[context->SplitCount].TablePage = table_page;
    context->SplitCount++;
    *entry = table_page |
             (combine_parent_constraints(old_entry, ancestor_constraints) &
              X86_PARENT_PERMISSION_FLAGS);
    flush_page_tables();
    flush_translation(virtual_address);
    write_cr0(previous_cr0);
    return EFI_SUCCESS;
}

EFI_STATUS fadal_x86_walk_page(EFI_BOOT_SERVICES *boot_services,
                               FADAL_X86_PAGE_TABLE_CONTEXT *context,
                               UINT64 virtual_address,
                               FADAL_X86_PAGE_MAPPING *mapping) {
    UINT64 cr3;
    volatile UINT64 *pml4;
    volatile UINT64 *pdpt;
    volatile UINT64 *pd;
    volatile UINT64 *pt;
    volatile UINT64 *entry;
    UINT64 ancestor_constraints;
    EFI_STATUS status;

    if (boot_services == NULL || context == NULL || mapping == NULL ||
        (virtual_address & (FADAL_X86_PAGE_SIZE - 1U)) != 0) {
        return EFI_INVALID_PARAMETER;
    }
    if ((read_cr4() & X86_CR4_LA57) != 0) return EFI_UNSUPPORTED;

    cr3 = read_cr3();
    pml4 = (volatile UINT64 *)(UINTN)(cr3 & FADAL_X86_PTE_ADDRESS_MASK);
    entry = &pml4[(virtual_address >> 39) & 0x1ffU];
    if ((*entry & FADAL_X86_PTE_PRESENT) == 0) return EFI_NOT_FOUND;
    ancestor_constraints = *entry;

    pdpt = (volatile UINT64 *)(UINTN)(*entry & FADAL_X86_PTE_ADDRESS_MASK);
    entry = &pdpt[(virtual_address >> 30) & 0x1ffU];
    if ((*entry & FADAL_X86_PTE_PRESENT) == 0) return EFI_NOT_FOUND;
    if ((*entry & X86_PTE_PS_OR_4K_PAT) != 0) {
        status = split_large_mapping(boot_services, context, entry, 30U,
                                     virtual_address, ancestor_constraints);
        if (EFI_ERROR(status)) return status;
    }

    ancestor_constraints = combine_parent_constraints(ancestor_constraints, *entry);
    pd = (volatile UINT64 *)(UINTN)(*entry & FADAL_X86_PTE_ADDRESS_MASK);
    entry = &pd[(virtual_address >> 21) & 0x1ffU];
    if ((*entry & FADAL_X86_PTE_PRESENT) == 0) return EFI_NOT_FOUND;
    if ((*entry & X86_PTE_PS_OR_4K_PAT) != 0) {
        status = split_large_mapping(boot_services, context, entry, 21U,
                                     virtual_address, ancestor_constraints);
        if (EFI_ERROR(status)) return status;
    }

    pt = (volatile UINT64 *)(UINTN)(*entry & FADAL_X86_PTE_ADDRESS_MASK);
    entry = &pt[(virtual_address >> 12) & 0x1ffU];
    if ((*entry & FADAL_X86_PTE_PRESENT) == 0 ||
        (*entry & FADAL_X86_PTE_ADDRESS_MASK) !=
            (virtual_address & FADAL_X86_PTE_ADDRESS_MASK)) {
        return EFI_UNSUPPORTED;
    }

    mapping->VirtualAddress = virtual_address;
    mapping->Entries[0] = &pml4[(virtual_address >> 39) & 0x1ffU];
    mapping->Entries[1] = &pdpt[(virtual_address >> 30) & 0x1ffU];
    mapping->Entries[2] = &pd[(virtual_address >> 21) & 0x1ffU];
    mapping->Entries[3] = entry;
    for (UINTN level = 0; level < 4; level++) {
        mapping->OriginalEntries[level] = *mapping->Entries[level];
    }
    return EFI_SUCCESS;
}

EFI_STATUS fadal_x86_check_page_permissions(
    const FADAL_X86_PAGE_MAPPING *mapping,
    UINT64 set_leaf_flags,
    UINT64 clear_leaf_flags) {
    UINTN level;
    if (mapping == NULL || mapping->Entries[3] == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if ((*mapping->Entries[3] & X86_PTE_PKEY) != 0) {
        return EFI_UNSUPPORTED;
    }
    for (level = 0; level < 3; level++) {
        UINT64 parent = *mapping->Entries[level];
        if ((set_leaf_flags & FADAL_X86_PTE_WRITE) != 0 &&
            (parent & FADAL_X86_PTE_WRITE) == 0) {
            return EFI_UNSUPPORTED;
        }
        if ((clear_leaf_flags & FADAL_X86_PTE_NX) != 0 &&
            (parent & FADAL_X86_PTE_NX) != 0) {
            return EFI_UNSUPPORTED;
        }
    }
    return EFI_SUCCESS;
}

EFI_STATUS fadal_x86_set_page_permissions(FADAL_X86_PAGE_MAPPING *mapping,
                                          UINT64 set_leaf_flags,
                                          UINT64 clear_leaf_flags) {
    UINT64 previous_cr0 = read_cr0();
    EFI_STATUS status = fadal_x86_check_page_permissions(mapping,
                                                         set_leaf_flags,
                                                         clear_leaf_flags);
    if (EFI_ERROR(status)) return status;

    write_cr0(previous_cr0 & ~X86_CR0_WP);
    *mapping->Entries[3] = (*mapping->Entries[3] | set_leaf_flags) &
                            ~clear_leaf_flags;
    flush_page_tables();
    flush_translation(mapping->VirtualAddress);
    write_cr0(previous_cr0);
    return EFI_SUCCESS;
}

void fadal_x86_restore_leaf_permissions(FADAL_X86_PAGE_MAPPING *mappings,
                                        UINTN mapping_count) {
    UINT64 previous_cr0 = read_cr0();
    UINTN index;

    write_cr0(previous_cr0 & ~X86_CR0_WP);
    for (index = 0; index < mapping_count; index++) {
        *mappings[index].Entries[3] = mappings[index].OriginalEntries[3];
    }
    flush_page_tables();
    write_cr0(previous_cr0);
}

void fadal_x86_restore_splits(EFI_BOOT_SERVICES *boot_services,
                              FADAL_X86_PAGE_TABLE_CONTEXT *context) {
    UINT64 previous_cr0;
    UINTN count;

    if (boot_services == NULL || context == NULL || context->SplitCount == 0) return;
    previous_cr0 = read_cr0();
    write_cr0(previous_cr0 & ~X86_CR0_WP);
    count = context->SplitCount;
    while (count > 0) {
        FADAL_X86_TABLE_SPLIT *split = &context->Splits[count - 1];
        *split->ParentEntry = split->OriginalEntry;
        count--;
    }
    flush_page_tables();
    while (context->SplitCount > 0) {
        FADAL_X86_TABLE_SPLIT *split = &context->Splits[context->SplitCount - 1];
        uefi_call_wrapper(boot_services->FreePages, 2, split->TablePage, 1);
        context->SplitCount--;
    }
    write_cr0(previous_cr0);
}

void fadal_x86_restore_page(EFI_BOOT_SERVICES *boot_services,
                            FADAL_X86_PAGE_TABLE_CONTEXT *context,
                            FADAL_X86_PAGE_MAPPING *mapping) {
    UINT64 previous_cr0 = read_cr0();
    UINTN level;

    write_cr0(previous_cr0 & ~X86_CR0_WP);
    for (level = 0; level < 4; level++) {
        *mapping->Entries[level] = mapping->OriginalEntries[level];
    }
    flush_page_tables();
    flush_translation(mapping->VirtualAddress);
    if (context != NULL) {
        UINTN count = context->SplitCount;
        while (count > 0) {
            FADAL_X86_TABLE_SPLIT *split = &context->Splits[count - 1];
            *split->ParentEntry = split->OriginalEntry;
            count--;
        }
        flush_page_tables();
        while (context->SplitCount > 0) {
            FADAL_X86_TABLE_SPLIT *split = &context->Splits[context->SplitCount - 1];
            uefi_call_wrapper(boot_services->FreePages, 2, split->TablePage, 1);
            context->SplitCount--;
        }
    }
    write_cr0(previous_cr0);
}
