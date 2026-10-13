#include <efi.h>
#include <efilib.h>
#include <efidebug.h>
#include "page_tables.h"

#define QEMU_DEBUG_EXIT_PORT 0x00f4
#define QEMU_TEST_PASS 0x10U
#define QEMU_TEST_FAIL 0x11U
#define QEMU_TEST_SKIP 0x12U
#define X86_PAGE_FAULT_VECTOR 14U
#define X86_IDT_PRESENT 0x80U
#define X86_IDT_GATE_TYPE_MASK 0x0fU
#define X86_IDT_INTERRUPT_GATE 0x0eU
#define X86_CR0_WP (1ULL << 16)
#define PF_TEST_PHASE_NONE 0U
#define PF_TEST_PHASE_EXECUTE 1U
#define PF_TEST_PHASE_WRITE 2U
#define PF_TEST_PHASE_NX 3U
#define X86_IDT_ENTRY_COUNT 256U

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
    "addq $8, %rsp\n\t"
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

static EFI_PHYSICAL_ADDRESS gTestPage;
static BOOLEAN gPageAllocated;
static BOOLEAN gIdtInstalled;
static BOOLEAN gMappingPrepared;
static UINT64 gOriginalCr0;
static FADAL_X86_PAGE_TABLE_CONTEXT gPageTables;
static FADAL_X86_PAGE_MAPPING gTestMapping;
static volatile UINT32 gPhase;
static volatile UINT32 gExpectedFaults;
static volatile UINT32 gExpectedNxFaults;
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
    } else if (gPhase == PF_TEST_PHASE_NX && fault_address == gExpectedCr2 &&
               (error_code & 0x17U) == 0x11U) {
        gExpectedNxFaults++;
    } else {
        gUnexpectedFaults++;
    }
    if (gRecoveryRip != 0) {
        exception_frame[1] = gRecoveryRip;
    } else {
        gUnexpectedFaults++;
    }
}

static EFI_STATUS prepare_test_mapping(void) {
    EFI_STATUS status = fadal_x86_walk_page(BS, &gPageTables,
                                             (UINT64)gTestPage, &gTestMapping);
    if (EFI_ERROR(status)) return status;
    status = fadal_x86_set_page_permissions(&gTestMapping,
                                            FADAL_X86_PTE_WRITE,
                                            FADAL_X86_PTE_NX);
    if (EFI_ERROR(status)) return status;
    gMappingPrepared = TRUE;
    return EFI_SUCCESS;
}

static EFI_STATUS make_test_page_read_only(void) {
    return fadal_x86_set_page_permissions(&gTestMapping, 0,
                                          FADAL_X86_PTE_WRITE);
}

static BOOLEAN parent_permissions_unchanged(void) {
    const UINT64 permission_mask = FADAL_X86_PTE_WRITE | FADAL_X86_PTE_NX;
    UINTN level;
    for (level = 0; level < 3; level++) {
        if (((*gTestMapping.Entries[level] ^
              gTestMapping.OriginalEntries[level]) & permission_mask) != 0) {
            return FALSE;
        }
    }
    return TRUE;
}

static void cleanup_test_resources(void) {
    if (gMappingPrepared) {
        fadal_x86_restore_page(BS, &gPageTables, &gTestMapping);
        gMappingPrepared = FALSE;
    } else if (gPageTables.SplitCount != 0) {
        fadal_x86_restore_splits(BS, &gPageTables);
    }
    write_cr0(gOriginalCr0);
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
    gOriginalCr0 = read_cr0();
    status = fadal_x86_enable_nx();
    if (EFI_ERROR(status)) {
        finish_test(QEMU_TEST_SKIP, L"SKIP - CPU does not support NX page protection");
    }
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
    Print(L"PFTEST: stage=code-page-allocated\r\n");

    status = prepare_test_mapping();
    if (EFI_ERROR(status)) {
        Print(L"PFTEST: page-table walk status=%r\r\n", status);
        finish_test(QEMU_TEST_SKIP, L"SKIP - unsupported or non-identity x86 page-table map");
    }
    if (!parent_permissions_unchanged()) {
        finish_test(QEMU_TEST_FAIL, L"FAIL - leaf setup changed parent RW/NX permissions");
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

    gExpectedCr2 = (UINT64)gTestPage;
    status = fadal_x86_set_page_permissions(&gTestMapping,
                                            FADAL_X86_PTE_NX, 0);
    if (EFI_ERROR(status)) {
        finish_test(QEMU_TEST_SKIP, L"SKIP - parent mapping blocks NX test setup");
    }
    if (!parent_permissions_unchanged()) {
        finish_test(QEMU_TEST_FAIL, L"FAIL - NX update changed parent RW/NX permissions");
    }
    gPhase = PF_TEST_PHASE_NX;
    execute_test_page();
    gPhase = PF_TEST_PHASE_NONE;
    Print(L"PFTEST: NX-test-returned faults=%u unexpected=%u returned=%u CR2=%lx error=%lx\r\n",
          gExpectedNxFaults, gUnexpectedFaults, gCodeReturned,
          gLastCr2, gLastErrorCode);
    if (gExpectedNxFaults != 1 || gUnexpectedFaults != 0 || gCodeReturned != 0 ||
        gLastCr2 != gExpectedCr2 || (gLastErrorCode & 0x17U) != 0x11U) {
        finish_test(QEMU_TEST_FAIL, L"FAIL - NX execution fault was not verified");
    }
    status = fadal_x86_set_page_permissions(&gTestMapping, 0,
                                            FADAL_X86_PTE_NX);
    if (EFI_ERROR(status)) {
        finish_test(QEMU_TEST_FAIL, L"FAIL - unable to restore executable mapping");
    }
    execute_test_page();
    if (gUnexpectedFaults != 0 || gCodeReturned == 0) {
        finish_test(QEMU_TEST_FAIL, L"FAIL - executable mapping did not recover after NX test");
    }
    Print(L"PFTEST: NX error=%lx expected-execute-faults=%u\r\n",
          gLastErrorCode, gExpectedNxFaults);

    current_cr0 = read_cr0();
    write_cr0(current_cr0 | X86_CR0_WP);
    status = make_test_page_read_only();
    if (EFI_ERROR(status)) {
        finish_test(QEMU_TEST_FAIL, L"FAIL - unable to make test code page read-only");
    }
    if (!parent_permissions_unchanged()) {
        finish_test(QEMU_TEST_FAIL, L"FAIL - RO update changed parent RW/NX permissions");
    }
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
    finish_test(QEMU_TEST_PASS,
                L"PASS - NX and read-only page faults were caught and recovered");
}
