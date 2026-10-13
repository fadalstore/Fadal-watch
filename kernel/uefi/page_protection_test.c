#include <efi.h>
#include <efilib.h>
#include <efidebug.h>

#define QEMU_DEBUG_EXIT_PORT 0x00f4
#define QEMU_TEST_PASS 0x10U
#define QEMU_TEST_FAIL 0x11U
#define QEMU_TEST_SKIP 0x12U
#define PF_TEST_PAGE_SIZE 4096U
#define X86_PAGE_FAULT_VECTOR 14U
#define X86_IDT_PRESENT 0x80U
#define X86_IDT_GATE_TYPE_MASK 0x0fU
#define X86_IDT_INTERRUPT_GATE 0x0eU
#define X86_CR0_WP (1ULL << 16)
#define X86_CR4_LA57 (1ULL << 12)
#define X86_PTE_PRESENT (1ULL << 0)
#define X86_PTE_WRITE (1ULL << 1)
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
#define X86_PTE_NX (1ULL << 63)
#define X86_PTE_ADDRESS_MASK 0x000ffffffffff000ULL
#define X86_PDE_2M_ADDRESS_MASK 0x000fffffffe00000ULL
#define X86_PDPE_1G_ADDRESS_MASK 0x000fffffc0000000ULL
#define X86_PTE_LEAF_FLAGS (X86_PTE_PRESENT | X86_PTE_WRITE | X86_PTE_USER | \
    X86_PTE_PWT | X86_PTE_PCD | X86_PTE_ACCESSED | X86_PTE_DIRTY | \
    X86_PTE_GLOBAL | X86_PTE_SOFTWARE | X86_PTE_PKEY | X86_PTE_NX)
#define X86_PARENT_FLAGS (X86_PTE_USER | X86_PTE_NX | X86_PTE_SOFTWARE)
#define PF_TEST_PHASE_NONE 0U
#define PF_TEST_PHASE_EXECUTE 1U
#define PF_TEST_PHASE_WRITE 2U
#define X86_IDT_BYTES 4096U
#define X86_IDT_ENTRY_COUNT 256U
#define X86_TABLE_SPLIT_MAX 2U

extern UINT32 EFIAPI fadal_test_execute(VOID *entry);
extern VOID EFIAPI fadal_test_execute_recovered(VOID);
extern UINT32 EFIAPI fadal_test_store(volatile UINT8 *target);
extern VOID EFIAPI fadal_test_store_recovered(VOID);

__asm__(
    ".text\n"
    ".p2align 4\n"
    ".globl fadal_test_execute\n"
    ".type fadal_test_execute,@function\n"
    "fadal_test_execute:\n\t"
    "call *%rdi\n\t"
    "movl $1, %eax\n\t"
    "ret\n"
    ".globl fadal_test_execute_recovered\n"
    ".type fadal_test_execute_recovered,@function\n"
    "fadal_test_execute_recovered:\n\t"
    "xorl %eax, %eax\n\t"
    "ret\n"
    ".p2align 4\n"
    ".globl fadal_test_store\n"
    ".type fadal_test_store,@function\n"
    "fadal_test_store:\n\t"
    "movb $0xa5, (%rdi)\n\t"
    "movl $1, %eax\n\t"
    "ret\n"
    ".globl fadal_test_store_recovered\n"
    ".type fadal_test_store_recovered,@function\n"
    "fadal_test_store_recovered:\n\t"
    "xorl %eax, %eax\n\t"
    "ret\n"
);

typedef struct __attribute__((packed)) {
    UINT16 Limit;
    UINT64 Base;
} X86_IDTR;

typedef struct __attribute__((packed)) {
    UINT16 OffsetLow;
    UINT16 Selector;
    UINT8 Ist;
    UINT8 TypeAttributes;
    UINT16 OffsetMiddle;
    UINT32 OffsetHigh;
    UINT32 Reserved;
} X86_IDT_GATE;

typedef struct {
    volatile UINT64 *ParentEntry;
    UINT64 OriginalEntry;
    EFI_PHYSICAL_ADDRESS TablePage;
} X86_TABLE_SPLIT;

static EFI_PHYSICAL_ADDRESS gTestPage;
static BOOLEAN gPageAllocated;
static BOOLEAN gIdtInstalled;
static BOOLEAN gMappingPrepared;
static UINT64 gOriginalCr0;
static volatile UINT64 *gLeafPte;
static UINT64 gOriginalLeafPte;
static volatile UINT64 *gPathEntries[4];
static UINT64 gOriginalPathEntries[4];
static X86_TABLE_SPLIT gTableSplits[X86_TABLE_SPLIT_MAX];
static UINTN gTableSplitCount;
static volatile UINT32 gPhase;
static volatile UINT32 gExpectedFaults;
static volatile UINT32 gUnexpectedFaults;
static volatile UINT32 gStoreReturned;
static volatile UINT32 gCodeReturned;
static volatile UINT64 gExpectedCr2;
static volatile UINT64 gLastCr2;
static volatile UINT64 gLastErrorCode;
static volatile UINT64 gRecoveryRip;
static X86_IDTR gOriginalIdtr;
static X86_IDT_GATE gTestIdt[X86_IDT_ENTRY_COUNT]
    __attribute__((aligned(4096)));

static UINT64 read_cr0(void) {
    UINT64 value;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(value));
    return value;
}

static void write_cr0(UINT64 value) {
    __asm__ volatile ("mov %0, %%cr0" : : "r"(value) : "memory");
}

static UINT64 read_cr2(void) {
    UINT64 value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
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

static void flush_translation(UINT64 virtual_address) {
    __asm__ volatile ("invlpg (%0)" : : "r"((VOID *)(UINTN)virtual_address) : "memory");
}

static void flush_page_tables(void) {
    UINT64 current_cr3 = read_cr3();
    __asm__ volatile ("mov %0, %%cr3" : : "r"(current_cr3) : "memory");
}

static __attribute__((noreturn)) void qemu_test_exit(UINT32 code) {
    __asm__ volatile ("outl %0, %1" : : "a"(code),
                      "Nd"((UINT16)QEMU_DEBUG_EXIT_PORT));
    for (;;) __asm__ volatile ("hlt");
}

static void load_idtr(const X86_IDTR *idtr) {
    __asm__ volatile ("lidt %0" : : "m"(*idtr) : "memory");
}

static UINT64 page_fault_stub_address(void);
void EFIAPI fadal_page_fault_dispatch(UINT64 *exception_frame)
    __attribute__((used, noinline));

/* This EFI-only stub preserves the interrupted x64 register/SIMD state. */
__attribute__((naked, used)) static void page_fault_stub(void) {
    __asm__ volatile (
        "cld\n\t"
        "pushq %rax\n\t"
        "pushq %rcx\n\t"
        "pushq %rdx\n\t"
        "pushq %rbx\n\t"
        "pushq %rbp\n\t"
        "pushq %rsi\n\t"
        "pushq %rdi\n\t"
        "pushq %r8\n\t"
        "pushq %r9\n\t"
        "pushq %r10\n\t"
        "pushq %r11\n\t"
        "pushq %r12\n\t"
        "pushq %r13\n\t"
        "pushq %r14\n\t"
        "pushq %r15\n\t"
        "movq %rsp, %r12\n\t"
        "subq $528, %rsp\n\t"
        "andq $-16, %rsp\n\t"
        "movq %rsp, %r13\n\t"
        "fxsave64 (%r13)\n\t"
        "leaq 120(%r12), %rdi\n\t"
        "subq $32, %rsp\n\t"
        "call fadal_page_fault_dispatch\n\t"
        "fxrstor64 (%r13)\n\t"
        "movq %r12, %rsp\n\t"
        "popq %r15\n\t"
        "popq %r14\n\t"
        "popq %r13\n\t"
        "popq %r12\n\t"
        "popq %r11\n\t"
        "popq %r10\n\t"
        "popq %r9\n\t"
        "popq %r8\n\t"
        "popq %rdi\n\t"
        "popq %rsi\n\t"
        "popq %rbp\n\t"
        "popq %rbx\n\t"
        "popq %rdx\n\t"
        "popq %rcx\n\t"
        "popq %rax\n\t"
        "addq $8, %rsp\n\t"
        "iretq\n\t"
    );
}

static EFI_STATUS install_test_idt(void) {
    X86_IDTR test_idtr;
    X86_IDT_GATE *fault_gate;
    UINTN bytes;
    UINTN index;
    UINT64 handler_address = page_fault_stub_address();

    __asm__ volatile ("sidt %0" : "=m"(gOriginalIdtr));
    bytes = (UINTN)gOriginalIdtr.Limit + 1;
    if (bytes > sizeof(gTestIdt) ||
        bytes < (X86_PAGE_FAULT_VECTOR + 1) * sizeof(X86_IDT_GATE)) {
        return EFI_UNSUPPORTED;
    }
    for (index = 0; index < X86_IDT_ENTRY_COUNT; index++) {
        gTestIdt[index].OffsetLow = 0;
        gTestIdt[index].Selector = 0;
        gTestIdt[index].Ist = 0;
        gTestIdt[index].TypeAttributes = 0;
        gTestIdt[index].OffsetMiddle = 0;
        gTestIdt[index].OffsetHigh = 0;
        gTestIdt[index].Reserved = 0;
    }
    for (index = 0; index < bytes / sizeof(X86_IDT_GATE); index++) {
        gTestIdt[index] = ((const X86_IDT_GATE *)(UINTN)gOriginalIdtr.Base)[index];
    }

    fault_gate = &gTestIdt[X86_PAGE_FAULT_VECTOR];
    if ((fault_gate->TypeAttributes & X86_IDT_PRESENT) == 0 ||
        fault_gate->Selector == 0 ||
        (fault_gate->TypeAttributes & X86_IDT_GATE_TYPE_MASK) !=
            X86_IDT_INTERRUPT_GATE) {
        return EFI_UNSUPPORTED;
    }
    fault_gate->OffsetLow = (UINT16)(handler_address & 0xffffU);
    fault_gate->OffsetMiddle = (UINT16)((handler_address >> 16) & 0xffffU);
    fault_gate->OffsetHigh = (UINT32)(handler_address >> 32);
    fault_gate->Reserved = 0;

    test_idtr.Limit = gOriginalIdtr.Limit;
    test_idtr.Base = (UINT64)(UINTN)gTestIdt;
    load_idtr(&test_idtr);
    gIdtInstalled = TRUE;
    return EFI_SUCCESS;
}

static UINT64 page_fault_stub_address(void) {
    return (UINT64)(UINTN)page_fault_stub;
}

void EFIAPI fadal_page_fault_dispatch(UINT64 *exception_frame) {
    UINT64 error_code = exception_frame[0];
    UINT64 fault_address = read_cr2();

    gLastCr2 = fault_address;
    gLastErrorCode = error_code;
    if (gPhase == PF_TEST_PHASE_WRITE && fault_address == gExpectedCr2 &&
        (error_code & 0x17U) == 0x03U) {
        gExpectedFaults++;
    } else {
        gUnexpectedFaults++;
    }
    if (gRecoveryRip != 0) {
        exception_frame[1] = gRecoveryRip;
    } else {
        gUnexpectedFaults++;
    }
}

static EFI_STATUS allocate_table_page(EFI_PHYSICAL_ADDRESS *page) {
    EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages, 4,
                                           AllocateAnyPages, EfiLoaderData,
                                           1, page);
    if (EFI_ERROR(status)) return status;
    if (gTableSplitCount >= X86_TABLE_SPLIT_MAX) {
        uefi_call_wrapper(BS->FreePages, 2, *page, 1);
        return EFI_OUT_OF_RESOURCES;
    }
    return EFI_SUCCESS;
}

static EFI_STATUS split_large_mapping(volatile UINT64 *entry, UINTN shift) {
    EFI_STATUS status;
    UINT64 old_entry = *entry;
    EFI_PHYSICAL_ADDRESS table_page = 0;
    volatile UINT64 *table;
    UINT64 common_flags = old_entry & X86_PTE_LEAF_FLAGS;
    UINTN index;

    if ((old_entry & X86_PTE_PKEY) != 0) return EFI_UNSUPPORTED;
    status = allocate_table_page(&table_page);
    if (EFI_ERROR(status)) return status;
    table = (volatile UINT64 *)(UINTN)table_page;

    if (shift == 30U) {
        UINT64 base = old_entry & X86_PDPE_1G_ADDRESS_MASK;
        UINT64 large_pat = old_entry & X86_PTE_LARGE_PAT;
        for (index = 0; index < 512; index++) {
            table[index] = (base + ((UINT64)index << 21)) |
                           common_flags | (1ULL << 7) | large_pat;
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
        uefi_call_wrapper(BS->FreePages, 2, table_page, 1);
        return EFI_INVALID_PARAMETER;
    }

    gTableSplits[gTableSplitCount].ParentEntry = entry;
    gTableSplits[gTableSplitCount].OriginalEntry = old_entry;
    gTableSplits[gTableSplitCount].TablePage = table_page;
    gTableSplitCount++;
    *entry = table_page | X86_PTE_PRESENT | X86_PTE_WRITE |
             (old_entry & X86_PARENT_FLAGS);
    flush_page_tables();
    flush_translation(gTestPage);
    return EFI_SUCCESS;
}

static EFI_STATUS prepare_test_mapping(void) {
    UINT64 cr4 = read_cr4();
    UINT64 cr3 = read_cr3();
    UINT64 virtual_address = gTestPage;
    volatile UINT64 *pml4;
    volatile UINT64 *pdpt;
    volatile UINT64 *pd;
    volatile UINT64 *pt;
    volatile UINT64 *entry;
    EFI_STATUS status;

    if ((cr4 & X86_CR4_LA57) != 0) return EFI_UNSUPPORTED;
    pml4 = (volatile UINT64 *)(UINTN)(cr3 & X86_PTE_ADDRESS_MASK);
    entry = &pml4[(virtual_address >> 39) & 0x1ffU];
    if ((*entry & X86_PTE_PRESENT) == 0) return EFI_NOT_FOUND;

    pdpt = (volatile UINT64 *)(UINTN)(*entry & X86_PTE_ADDRESS_MASK);
    entry = &pdpt[(virtual_address >> 30) & 0x1ffU];
    if ((*entry & X86_PTE_PRESENT) == 0) return EFI_NOT_FOUND;
    if ((*entry & X86_PTE_PS_OR_4K_PAT) != 0) {
        status = split_large_mapping(entry, 30U);
        if (EFI_ERROR(status)) return status;
    }

    pd = (volatile UINT64 *)(UINTN)(*entry & X86_PTE_ADDRESS_MASK);
    entry = &pd[(virtual_address >> 21) & 0x1ffU];
    if ((*entry & X86_PTE_PRESENT) == 0) return EFI_NOT_FOUND;
    if ((*entry & X86_PTE_PS_OR_4K_PAT) != 0) {
        status = split_large_mapping(entry, 21U);
        if (EFI_ERROR(status)) return status;
    }

    pt = (volatile UINT64 *)(UINTN)(*entry & X86_PTE_ADDRESS_MASK);
    entry = &pt[(virtual_address >> 12) & 0x1ffU];
    if ((*entry & X86_PTE_PRESENT) == 0 ||
        (*entry & X86_PTE_ADDRESS_MASK) != (gTestPage & X86_PTE_ADDRESS_MASK)) {
        return EFI_UNSUPPORTED;
    }

    gPathEntries[0] = &pml4[(virtual_address >> 39) & 0x1ffU];
    gPathEntries[1] = &pdpt[(virtual_address >> 30) & 0x1ffU];
    gPathEntries[2] = &pd[(virtual_address >> 21) & 0x1ffU];
    gPathEntries[3] = entry;
    for (UINTN level = 0; level < 4; level++) {
        gOriginalPathEntries[level] = *gPathEntries[level];
        *gPathEntries[level] = (*gPathEntries[level] | X86_PTE_WRITE) & ~X86_PTE_NX;
    }
    gLeafPte = entry;
    gOriginalLeafPte = *entry;
    gMappingPrepared = TRUE;
    flush_page_tables();
    flush_translation(gTestPage);
    return EFI_SUCCESS;
}

static void make_test_page_read_only(void) {
    UINT64 current_cr0 = read_cr0();
    write_cr0(current_cr0 & ~X86_CR0_WP);
    *gLeafPte &= ~X86_PTE_WRITE;
    flush_page_tables();
    flush_translation(gTestPage);
    write_cr0(current_cr0);
}

static void cleanup_test_resources(void) {
    UINT64 current_cr0 = read_cr0();
    BOOLEAN changed_tables = gMappingPrepared || gTableSplitCount != 0;
    if (changed_tables) write_cr0(current_cr0 & ~X86_CR0_WP);
    if (gMappingPrepared) {
        *gLeafPte = gOriginalLeafPte;
        for (UINTN level = 0; level < 4; level++) {
            *gPathEntries[level] = gOriginalPathEntries[level];
        }
        flush_page_tables();
        flush_translation(gTestPage);
        gMappingPrepared = FALSE;
    }
    while (gTableSplitCount > 0) {
        X86_TABLE_SPLIT *split = &gTableSplits[gTableSplitCount - 1];
        *split->ParentEntry = split->OriginalEntry;
        flush_page_tables();
        flush_translation(gTestPage);
        uefi_call_wrapper(BS->FreePages, 2, split->TablePage, 1);
        gTableSplitCount--;
    }
    if (changed_tables) write_cr0(current_cr0);
    if (gIdtInstalled) {
        load_idtr(&gOriginalIdtr);
        gIdtInstalled = FALSE;
    }
    if (gPageAllocated && gTestPage != 0) {
        uefi_call_wrapper(BS->FreePages, 2, gTestPage, 1);
        gPageAllocated = FALSE;
    }
}

static __attribute__((noreturn)) void finish_test(UINT32 code,
                                                   const CHAR16 *message) {
    cleanup_test_resources();
    Print(L"PFTEST: %s\r\n", message);
    qemu_test_exit(code);
}

static void execute_test_page(void) {
    gCodeReturned = 0;
    gRecoveryRip = (UINT64)(UINTN)fadal_test_execute_recovered;
    gCodeReturned = fadal_test_execute((VOID *)(UINTN)gTestPage);
    gRecoveryRip = 0;
}

static void attempt_protected_write(volatile UINT8 *target) {
    gStoreReturned = 0;
    gRecoveryRip = (UINT64)(UINTN)fadal_test_store_recovered;
    gStoreReturned = fadal_test_store(target);
    gRecoveryRip = 0;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *system_table) {
    EFI_STATUS status;
    volatile UINT8 *write_target;
    UINT8 original_value;
    UINT64 current_cr0;

    InitializeLib(image, system_table);
    Print(L"FadalOS page-protection self-test\r\n");
    status = install_test_idt();
    if (EFI_ERROR(status)) {
        finish_test(QEMU_TEST_SKIP, L"SKIP - temporary x64 #PF IDT could not be installed");
    }
    Print(L"PFTEST: stage=idtr-installed\r\n");

    status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
                               EfiLoaderCode, 1, &gTestPage);
    if (EFI_ERROR(status)) {
        Print(L"PFTEST: AllocatePages status=%r\r\n", status);
        finish_test(QEMU_TEST_FAIL, L"FAIL - unable to allocate test code page");
    }
    gPageAllocated = TRUE;
    gOriginalCr0 = read_cr0();
    Print(L"PFTEST: stage=code-page-allocated\r\n");

    /* Firmware may protect its page-table pages; temporarily clear CR0.WP
       only while editing the map, then restore the original control state. */
    write_cr0(gOriginalCr0 & ~X86_CR0_WP);
    status = prepare_test_mapping();
    write_cr0(gOriginalCr0);
    if (EFI_ERROR(status)) {
        Print(L"PFTEST: page-table walk status=%r\r\n", status);
        finish_test(QEMU_TEST_SKIP, L"SKIP - unsupported or non-identity x86 page-table map");
    }
    Print(L"PFTEST: stage=pte-ready address=%lx cr3=%lx\r\n",
          (UINT64)gTestPage, read_cr3());

    *(volatile UINT8 *)(UINTN)gTestPage = 0xc3; /* RET instruction */
    *((volatile UINT8 *)(UINTN)(gTestPage + 16)) = 0x5a;
    Print(L"PFTEST: stage=page-initialized\r\n");
    gPhase = PF_TEST_PHASE_EXECUTE;
    execute_test_page();
    gPhase = PF_TEST_PHASE_NONE;
    Print(L"PFTEST: stage=initial-code-execution\r\n");
    if (gUnexpectedFaults != 0 || gCodeReturned == 0) {
        finish_test(QEMU_TEST_FAIL, L"FAIL - writable loader-code page did not execute");
    }

    current_cr0 = read_cr0();
    write_cr0(current_cr0 | X86_CR0_WP);
    make_test_page_read_only();
    Print(L"PFTEST: stage=read-only-set\r\n");
    write_target = (volatile UINT8 *)(UINTN)(gTestPage + 16);
    original_value = *write_target;
    gExpectedCr2 = (UINT64)(UINTN)write_target;
    gPhase = PF_TEST_PHASE_WRITE;
    attempt_protected_write(write_target);
    write_cr0(current_cr0);
    gPhase = PF_TEST_PHASE_NONE;
    Print(L"PFTEST: stage=write-test-returned faults=%u unexpected=%u\r\n",
          gExpectedFaults, gUnexpectedFaults);

    if (gExpectedFaults != 1 || gUnexpectedFaults != 0 ||
        gStoreReturned != 0 || *write_target != original_value ||
        gLastCr2 != gExpectedCr2 || (gLastErrorCode & 0x17U) != 0x03U) {
        finish_test(QEMU_TEST_FAIL, L"FAIL - expected write-protection #PF was not verified");
    }

    execute_test_page();
    if (gUnexpectedFaults != 0 || gCodeReturned == 0) {
        finish_test(QEMU_TEST_FAIL, L"FAIL - protected code page stopped executing");
    }

    Print(L"PFTEST: CR2=%lx error=%lx expected-write-faults=%u\r\n",
          gLastCr2, gLastErrorCode, gExpectedFaults);
    finish_test(QEMU_TEST_PASS, L"PASS - code-page write raised and recovered from #PF");
}
