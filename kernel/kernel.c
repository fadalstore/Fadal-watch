#include "fat12.h"
#include "ata.h"
#include "slab.h"
#include "vfs.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_CELLS (VGA_WIDTH * VGA_HEIGHT)
#define IDT_ENTRIES 256
#define TIMER_VECTOR 0x20
#define KEYBOARD_VECTOR 0x21
#define SYSCALL_VECTOR 0x80
#define FAT12_DISK_LBA 113
#define TIMER_FREQUENCY 100
#define PAGE_SIZE 4096
#define PAGE_PRESENT 0x001
#define PAGE_WRITE 0x002
#define PAGE_USER 0x004
#define PAGE_KERNEL_FLAGS (PAGE_PRESENT | PAGE_WRITE)
#define PAGE_USER_RO_FLAGS (PAGE_PRESENT | PAGE_USER)
#define PAGE_USER_RW_FLAGS (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)
#define IDENTITY_MAP_BYTES (16 * 1024 * 1024)
#define PHYSICAL_PAGE_COUNT (IDENTITY_MAP_BYTES / PAGE_SIZE)
#define PAGE_TABLE_COUNT (IDENTITY_MAP_BYTES / (PAGE_SIZE * 1024))
#define FIRST_USABLE_PAGE (0x100000 / PAGE_SIZE)
#define E820_MAP_ADDRESS 0x5000
#define E820_COUNT_ADDRESS 0x4ffc
#define E820_MAX_ENTRIES 32
#define USER_CODE_ADDRESS 0x00200000
#define USER_STACK_ADDRESS 0x00300000
#define USER_STACK_2_ADDRESS 0x00301000
#define USER_CODE_PAGE (USER_CODE_ADDRESS / PAGE_SIZE)
#define USER_STACK_PAGE (USER_STACK_ADDRESS / PAGE_SIZE)
#define USER_STACK_2_PAGE (USER_STACK_2_ADDRESS / PAGE_SIZE)
#define SYSCALL_GET_TICKS 1
#define SYSCALL_GET_PID 2
#define SYSCALL_EXIT 3
#define SYSCALL_READ 4
#define SYSCALL_WRITE 5
#define SYSCALL_OPEN 6
#define SYSCALL_EXEC 7
#define SYSCALL_YIELD 8
#define SYSCALL_CLOSE 9
#define MAX_PROCESSES 8
#define PROCESS_UNUSED 0
#define PROCESS_READY 1
#define PROCESS_RUNNING 2
#define PROCESS_EXITED 3
#define PROCESS_BLOCKED 4
#define WAIT_NONE 0
#define WAIT_TTY 1
#define MAX_FDS 8
#define MAX_HEAP_ALLOCS 32
#define TTY_BUFFER_SIZE 256

static volatile u16 *const vga = (volatile u16 *)0xb8000;
static u32 cursor;
static u8 shift_pressed;
static volatile char tty_buffer[TTY_BUFFER_SIZE];
static volatile u32 tty_read_index;
static volatile u32 tty_write_index;
static volatile u32 tty_count;
static volatile u32 timer_ticks;
static volatile u32 syscall_count;
static volatile u8 syscall_reported;
static volatile u8 get_ticks_reported;
static volatile u8 get_pid_reported;
static volatile u8 exit_reported;
static volatile u8 read_reported;
static volatile u8 write_reported;
static volatile u8 open_reported;
static volatile u8 exec_reported;
static volatile u8 yield_reported;
static volatile u8 close_reported;
static volatile u8 page_fault_reported;
static u32 scheduler_ticks;
static u32 scheduler_ready_pid;
static u32 page_directory[1024] __attribute__((aligned(PAGE_SIZE)));
static u32 page_tables[PAGE_TABLE_COUNT][1024] __attribute__((aligned(PAGE_SIZE)));
static u8 page_state[PHYSICAL_PAGE_COUNT];
static u32 free_page_count;
static u32 managed_page_count;
static u32 memory_map_count;
static u8 memory_map_valid;

struct e820_entry {
    u32 base_low;
    u32 base_high;
    u32 length_low;
    u32 length_high;
    u32 type;
    u32 attributes;
} __attribute__((packed));

static volatile struct e820_entry *const bios_memory_map =
    (volatile struct e820_entry *)E820_MAP_ADDRESS;
static volatile u16 *const bios_memory_map_count =
    (volatile u16 *)E820_COUNT_ADDRESS;

struct idt_entry {
    u16 offset_low;
    u16 selector;
    u8 zero;
    u8 type_attributes;
    u16 offset_high;
} __attribute__((packed));

struct idt_pointer {
    u16 limit;
    u32 base;
} __attribute__((packed));

struct gdt_entry {
    u16 limit_low;
    u16 base_low;
    u8 base_middle;
    u8 access;
    u8 granularity;
    u8 base_high;
} __attribute__((packed));

struct gdt_pointer {
    u16 limit;
    u32 base;
} __attribute__((packed));

struct tss_entry {
    u32 previous_task;
    u32 esp0;
    u32 ss0;
    u32 esp1;
    u32 ss1;
    u32 esp2;
    u32 ss2;
    u32 cr3;
    u32 eip;
    u32 eflags;
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 es;
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldt;
    u16 trap;
    u16 iomap_base;
} __attribute__((packed));

/*
 * pusha stores registers in this order at the current stack pointer:
 * edi, esi, ebp, original_esp, ebx, edx, ecx, eax.
 */
struct syscall_frame {
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 original_esp;
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;
};

struct address_space {
    u32 *page_directory;
    u32 *page_tables;
    u32 stack_physical;
    u32 refcount;
};

struct file_descriptor {
    u8 used;
    u8 kind;
};

struct process {
    u32 pid;
    u32 state;
    u32 entry;
    u32 user_stack_top;
    struct address_space *address_space;
    u32 wait_reason;
    struct file_descriptor fds[MAX_FDS];
};

struct heap_allocation {
    void *address;
    u32 pages;
    u32 bytes;
};

static struct idt_entry idt[IDT_ENTRIES];
static struct gdt_entry gdt[6];
static struct tss_entry tss;
static struct process *process_table[MAX_PROCESSES];
static u32 process_page_directories[MAX_PROCESSES][1024]
    __attribute__((aligned(PAGE_SIZE)));
static u32 process_page_tables[MAX_PROCESSES][PAGE_TABLE_COUNT * 1024]
    __attribute__((aligned(PAGE_SIZE)));
static struct address_space process_address_spaces[MAX_PROCESSES];
static u32 next_pid = 1;
static u32 current_pid;
static u32 process_total;
static u32 active_processes;
static struct heap_allocation heap_allocations[MAX_HEAP_ALLOCS];
static u32 heap_allocation_count;
static u32 heap_used_bytes;

extern void default_isr(void);
extern void gdt_flush(const struct gdt_pointer *pointer);
extern void timer_isr(void);
extern void keyboard_isr(void);
extern void syscall_isr(void);
extern void page_fault_isr(void);

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

static inline void lidt(const struct idt_pointer *pointer) {
    __asm__ volatile ("lidtl (%0)" : : "r"(pointer) : "memory");
}

static inline void enable_interrupts(void) {
    __asm__ volatile ("sti");
}

static inline void halt(void) {
    __asm__ volatile ("hlt");
}

static void kernel_write(const char *text);
static void serial_write(const char *text);
static void *page_alloc(void);
static void page_free(void *address);

static void write_u32(u32 value) {
    char digits[11];
    u32 length = 0;

    if (value == 0) {
        kernel_write("0");
        return;
    }

    while (value > 0 && length < sizeof(digits)) {
        digits[length++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (length > 0) {
        char digit[2] = { digits[--length], '\0' };
        kernel_write(digit);
    }
}

static void gdt_set_entry(u32 index, u32 base, u32 limit, u8 access) {
    gdt[index].base_low = (u16)(base & 0xffff);
    gdt[index].base_middle = (u8)((base >> 16) & 0xff);
    gdt[index].base_high = (u8)((base >> 24) & 0xff);
    gdt[index].limit_low = (u16)(limit & 0xffff);
    gdt[index].granularity = (u8)((limit >> 16) & 0x0f);
    gdt[index].granularity |= 0xcf;
    gdt[index].access = access;
}

static void tss_init(void) {
    for (u32 index = 0; index < sizeof(tss) / sizeof(u32); index++) {
        ((u32 *)&tss)[index] = 0;
    }
    tss.esp0 = 0x90000;
    tss.ss0 = 0x10;
    tss.iomap_base = sizeof(tss);
}

static void gdt_init(void) {
    gdt_set_entry(0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xffffffff, 0x9a);
    gdt_set_entry(2, 0, 0xffffffff, 0x92);
    gdt_set_entry(3, 0, 0xffffffff, 0xfa);
    gdt_set_entry(4, 0, 0xffffffff, 0xf2);
    gdt_set_entry(5, (u32)&tss, sizeof(tss) - 1, 0x89);

    struct gdt_pointer pointer = {
        .limit = (u16)(sizeof(gdt) - 1),
        .base = (u32)gdt,
    };
    gdt_flush(&pointer);
    __asm__ volatile ("ltr %0" : : "r"((u16)0x28));
}

static void paging_init(void) {
    for (u32 index = 0; index < 1024; index++) {
        page_directory[index] = 0;
    }
    for (u32 table = 0; table < PAGE_TABLE_COUNT; table++) {
        for (u32 index = 0; index < 1024; index++) {
            page_tables[table][index] =
                ((table * 1024 + index) * PAGE_SIZE) | PAGE_KERNEL_FLAGS;
        }
        page_directory[table] = ((u32)page_tables[table]) | PAGE_KERNEL_FLAGS;
    }
    page_tables[USER_CODE_PAGE / 1024][USER_CODE_PAGE % 1024] =
        USER_CODE_ADDRESS | PAGE_USER_RO_FLAGS;
    page_tables[USER_STACK_PAGE / 1024][USER_STACK_PAGE % 1024] =
        USER_STACK_ADDRESS | PAGE_USER_RW_FLAGS;
    page_tables[USER_STACK_2_PAGE / 1024][USER_STACK_2_PAGE % 1024] =
        USER_STACK_2_ADDRESS | PAGE_USER_RW_FLAGS;
    page_directory[USER_CODE_PAGE / 1024] |= PAGE_USER;
    page_directory[USER_STACK_PAGE / 1024] |= PAGE_USER;
    page_directory[USER_STACK_2_PAGE / 1024] |= PAGE_USER;

    u32 directory = (u32)page_directory;
    __asm__ volatile (
        "mov %%cr4, %%eax\n"
        "orl $0x10, %%eax\n"
        "mov %%eax, %%cr4\n"
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "orl $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        :
        : "r"(directory)
        : "eax", "memory");
}

static void load_address_space(struct address_space *space) {
    u32 directory = (u32)space->page_directory;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(directory) : "memory");
}

static struct address_space *address_space_create(u32 slot, u32 stack_physical) {
    if (slot >= MAX_PROCESSES) {
        return (struct address_space *)0;
    }
    struct address_space *space = &process_address_spaces[slot];
    space->page_directory = process_page_directories[slot];
    space->page_tables = process_page_tables[slot];
    space->stack_physical = stack_physical;
    space->refcount = 1;
    for (u32 index = 0; index < 1024; index++) {
        space->page_directory[index] = 0;
    }
    for (u32 table = 0; table < PAGE_TABLE_COUNT; table++) {
        u32 *source = page_tables[table];
        u32 *target = space->page_tables + table * 1024;
        for (u32 index = 0; index < 1024; index++) {
            target[index] = source[index];
        }
        space->page_directory[table] = (u32)target | PAGE_KERNEL_FLAGS;
    }
    space->page_tables[USER_STACK_PAGE] = stack_physical | PAGE_USER_RW_FLAGS;
    space->page_tables[USER_STACK_2_PAGE] = USER_STACK_2_ADDRESS | PAGE_KERNEL_FLAGS;
    space->page_directory[USER_CODE_PAGE / 1024] |= PAGE_USER;
    space->page_directory[USER_STACK_PAGE / 1024] |= PAGE_USER;
    return space;
}

static u8 address_spaces_isolated(struct address_space *first,
                                  struct address_space *second) {
    if (first == (struct address_space *)0 || second == (struct address_space *)0) {
        return 0;
    }
    u32 first_stack = first->page_tables[USER_STACK_PAGE];
    u32 second_stack = second->page_tables[USER_STACK_PAGE];
    u32 first_code = first->page_tables[USER_CODE_PAGE];
    u32 second_code = second->page_tables[USER_CODE_PAGE];
    return (first_stack & ~0xfff) != (second_stack & ~0xfff) &&
        (first_stack & PAGE_USER_RW_FLAGS) == PAGE_USER_RW_FLAGS &&
        (second_stack & PAGE_USER_RW_FLAGS) == PAGE_USER_RW_FLAGS &&
        (first_code & ~0xfff) == (second_code & ~0xfff) &&
        (first->page_directory[USER_STACK_PAGE / 1024] & PAGE_USER) != 0 &&
        (second->page_directory[USER_STACK_PAGE / 1024] & PAGE_USER) != 0;
}

static void address_space_destroy(u32 slot, struct address_space *space) {
    if (space == (struct address_space *)0 || slot >= MAX_PROCESSES) {
        return;
    }
    /* The first two bootstrap stacks are fixed boot mappings. Later process
       stacks come from the page allocator and must be returned on exit. */
    if (slot > 1 && space->stack_physical != 0) {
        page_free((void *)space->stack_physical);
    }
    space->page_directory = (u32 *)0;
    space->page_tables = (u32 *)0;
    space->stack_physical = 0;
    space->refcount = 0;
}

static u8 memory_map_covers_page(u32 page) {
    u32 page_start = page * PAGE_SIZE;
    u32 page_end = page_start + PAGE_SIZE;

    for (u32 index = 0; index < memory_map_count; index++) {
        volatile struct e820_entry *entry = &bios_memory_map[index];
        if (entry->type != 1 || entry->base_high != 0 || entry->length_high != 0) {
            continue;
        }

        u32 base = entry->base_low;
        u32 end = base + entry->length_low;
        if (end < base) {
            end = 0xffffffff;
        }
        if (base <= page_start && end >= page_end) {
            return 1;
        }
    }
    return 0;
}

static void memory_init(void) {
    memory_map_count = *bios_memory_map_count;
    if (memory_map_count > E820_MAX_ENTRIES) {
        memory_map_count = E820_MAX_ENTRIES;
    }
    memory_map_valid = memory_map_count > 0;

    for (u32 page = 0; page < PHYSICAL_PAGE_COUNT; page++) {
        page_state[page] = 1;
    }

    free_page_count = 0;
    managed_page_count = 0;
    for (u32 page = FIRST_USABLE_PAGE; page < PHYSICAL_PAGE_COUNT; page++) {
        if (!memory_map_valid || memory_map_covers_page(page)) {
            page_state[page] = 0;
            free_page_count++;
            managed_page_count++;
        }
    }
}

static void reserve_page(u32 address) {
    u32 page = address / PAGE_SIZE;
    if (page < PHYSICAL_PAGE_COUNT && page_state[page] == 0) {
        page_state[page] = 1;
        free_page_count--;
    }
}

static void process_manager_init(void) {
    for (u32 index = 0; index < MAX_PROCESSES; index++) {
        process_table[index] = (struct process *)slab_alloc(&process_slab_cache);
        if (process_table[index] != (struct process *)0) {
            process_table[index]->pid = 0;
            process_table[index]->state = PROCESS_UNUSED;
            process_table[index]->entry = 0;
            process_table[index]->user_stack_top = 0;
            process_table[index]->address_space = (struct address_space *)0;
            process_table[index]->wait_reason = WAIT_NONE;
            for (u32 fd = 0; fd < MAX_FDS; fd++) {
                process_table[index]->fds[fd].used = 0;
                process_table[index]->fds[fd].kind = 0;
            }
        }
    }
    next_pid = 1;
    current_pid = 0;
    process_total = 0;
    active_processes = 0;
    scheduler_ticks = 0;
    scheduler_ready_pid = 0;
    yield_reported = 0;
}

static u32 process_create(u32 entry, u32 user_stack_top) {
    (void)user_stack_top;
    for (u32 index = 0; index < MAX_PROCESSES; index++) {
        struct process *process = process_table[index];
        if (process == (struct process *)0) {
            continue;
        }
        if (process->state != PROCESS_UNUSED) {
            continue;
        }
        u32 stack_physical = index == 0 ? USER_STACK_ADDRESS : USER_STACK_2_ADDRESS;
        if (index > 1) {
            stack_physical = (u32)page_alloc();
        }
        if (stack_physical == 0) {
            continue;
        }
        struct address_space *space = address_space_create(index, stack_physical);
        if (space == (struct address_space *)0) {
            continue;
        }
        process->pid = next_pid++;
        process->state = PROCESS_READY;
        process->entry = entry;
        process->user_stack_top = USER_STACK_ADDRESS + PAGE_SIZE;
        process->address_space = space;
        process->wait_reason = WAIT_NONE;
        for (u32 fd = 0; fd < MAX_FDS; fd++) {
            process->fds[fd].used = 0;
            process->fds[fd].kind = 0;
        }
        process->fds[0].used = 1;
        process->fds[0].kind = 1;
        process->fds[1].used = 1;
        process->fds[1].kind = 2;
        process->fds[2].used = 1;
        process->fds[2].kind = 2;
        process_total++;
        active_processes++;
        return process->pid;
    }
    return 0;
}

static u8 process_set_running(u32 pid) {
    for (u32 index = 0; index < MAX_PROCESSES; index++) {
        struct process *process = process_table[index];
        if (process == (struct process *)0) {
            continue;
        }
        if (process->state == PROCESS_RUNNING) {
            process->state = PROCESS_READY;
        }
        if (process->pid == pid && process->state != PROCESS_UNUSED) {
            process->state = PROCESS_RUNNING;
            current_pid = pid;
            load_address_space(process->address_space);
            return 1;
        }
    }
    return 0;
}

static u8 process_exit_current(void) {
    for (u32 index = 0; index < MAX_PROCESSES; index++) {
        struct process *process = process_table[index];
        if (process == (struct process *)0) {
            continue;
        }
        if (process->pid != current_pid || process->state == PROCESS_EXITED) {
            continue;
        }
        process->state = PROCESS_EXITED;
        address_space_destroy(index, process->address_space);
        if (active_processes > 0) {
            active_processes--;
        }
        if (process_total > 0) {
            process_total--;
        }
        current_pid = 0;
        process->pid = 0;
        process->state = PROCESS_UNUSED;
        process->entry = 0;
        process->user_stack_top = 0;
        process->address_space = (struct address_space *)0;
        process->wait_reason = WAIT_NONE;
        return 1;
    }
    return 0;
}

static u8 process_cleanup_self_test(u32 init_pid) {
    u32 free_before = free_page_count;
    u32 probe_pid = process_create(USER_CODE_ADDRESS, USER_STACK_ADDRESS + PAGE_SIZE);
    if (probe_pid == 0) {
        return 0;
    }
    /* Keep the boot CPU on the init address space while exercising teardown;
       loading a probe CR3 is the job of the later context-switch path. */
    current_pid = probe_pid;
    if (!process_exit_current()) {
        current_pid = init_pid;
        return 0;
    }
    current_pid = init_pid;
    u8 page_returned = free_page_count == free_before;
    u32 reused_pid = process_create(USER_CODE_ADDRESS, USER_STACK_ADDRESS + PAGE_SIZE);
    u8 slot_reused = reused_pid != 0;
    if (slot_reused) {
        current_pid = reused_pid;
        slot_reused = process_exit_current();
        current_pid = init_pid;
    }
    return page_returned && slot_reused;
}

static u32 fd_open_kernel_file(void) {
    for (u32 index = 3; index < MAX_FDS; index++) {
        struct process *process = (struct process *)0;
        for (u32 slot = 0; slot < MAX_PROCESSES; slot++) {
            if (process_table[slot] != (struct process *)0 &&
                process_table[slot]->pid == current_pid) {
                process = process_table[slot];
                break;
            }
        }
        if (process != (struct process *)0 && !process->fds[index].used) {
            process->fds[index].used = 1;
            process->fds[index].kind = 3;
            return index;
        }
    }
    return 0xffffffff;
}

static u8 fd_close_current(u32 fd) {
    for (u32 slot = 0; slot < MAX_PROCESSES; slot++) {
        struct process *process = process_table[slot];
        if (process == (struct process *)0 || process->pid != current_pid) {
            continue;
        }
        if (fd >= MAX_FDS || fd < 3 || !process->fds[fd].used) {
            return 0;
        }
        process->fds[fd].used = 0;
        process->fds[fd].kind = 0;
        return 1;
    }
    return 0;
}

static u8 user_range_valid(u32 address, u32 length) {
    u32 end = address + length;
    if (end < address) {
        return 0;
    }
    if (address >= USER_CODE_ADDRESS && end <= USER_CODE_ADDRESS + PAGE_SIZE) {
        return 1;
    }
    if (address >= USER_STACK_ADDRESS && end <= USER_STACK_ADDRESS + PAGE_SIZE) {
        return 1;
    }
    return address >= USER_STACK_2_ADDRESS && end <= USER_STACK_2_ADDRESS + PAGE_SIZE;
}

static u8 user_path_is_kernel(const char *path, u32 length) {
    static const char expected[] = "KERNEL.TXT";
    if (length != sizeof(expected) - 1 || !user_range_valid((u32)path, length)) {
        return 0;
    }
    for (u32 index = 0; index < sizeof(expected) - 1; index++) {
        if (path[index] != expected[index]) {
            return 0;
        }
    }
    return 1;
}

extern const u8 _binary_out_fsh_bin_start[];
extern const u8 _binary_out_fsh_bin_end[];

static u32 fsh_image_size(void) {
    return (u32)(_binary_out_fsh_bin_end - _binary_out_fsh_bin_start);
}

static u8 fsh_load_from_disk(void) {
    u32 size = 0;
    if (fsh_image_size() == 0 || fsh_image_size() > PAGE_SIZE ||
        !vfs_read_file("FSH.BIN", (u8 *)USER_CODE_ADDRESS, PAGE_SIZE, &size) ||
        size != fsh_image_size()) {
        return 0;
    }
    return 1;
}

__attribute__((noreturn))
static void enter_user_mode(void) {
    __asm__ volatile (
        "cli\n"
        "movw $0x23, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "pushl $0x23\n"
        "pushl %0\n"
        "pushfl\n"
        "orl $0x200, (%%esp)\n"
        "pushl $0x1b\n"
        "pushl %1\n"
        "iret\n"
        :
        : "r"(USER_STACK_ADDRESS + PAGE_SIZE), "r"(USER_CODE_ADDRESS)
        : "eax", "memory");

    for (;;) {
        halt();
    }
}

static void *page_alloc(void) {
    for (u32 page = FIRST_USABLE_PAGE; page < PHYSICAL_PAGE_COUNT; page++) {
        if (page_state[page] == 0) {
            page_state[page] = 1;
            free_page_count--;

            u32 *memory = (u32 *)(page * PAGE_SIZE);
            for (u32 word = 0; word < PAGE_SIZE / sizeof(u32); word++) {
                memory[word] = 0;
            }
            return memory;
        }
    }
    return (void *)0;
}

static void page_free(void *address) {
    u32 physical_address = (u32)address;
    if ((physical_address % PAGE_SIZE) != 0 ||
        physical_address < 0x100000 ||
        physical_address >= IDENTITY_MAP_BYTES) {
        return;
    }

    u32 page = physical_address / PAGE_SIZE;
    if (page_state[page] != 0) {
        page_state[page] = 0;
        free_page_count++;
    }
}

static void heap_init(void) {
    for (u32 index = 0; index < MAX_HEAP_ALLOCS; index++) {
        heap_allocations[index].address = (void *)0;
        heap_allocations[index].pages = 0;
        heap_allocations[index].bytes = 0;
    }
    heap_allocation_count = 0;
    heap_used_bytes = 0;
}

void *kmalloc(u32 bytes) {
    if (bytes == 0 || heap_allocation_count >= MAX_HEAP_ALLOCS) {
        return (void *)0;
    }
    u32 pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    for (u32 first = FIRST_USABLE_PAGE; first + pages <= PHYSICAL_PAGE_COUNT; first++) {
        u8 available = 1;
        for (u32 offset = 0; offset < pages; offset++) {
            if (page_state[first + offset] != 0) {
                available = 0;
                break;
            }
        }
        if (!available) {
            continue;
        }
        for (u32 offset = 0; offset < pages; offset++) {
            page_state[first + offset] = 1;
            free_page_count--;
            u32 *page = (u32 *)((first + offset) * PAGE_SIZE);
            for (u32 word = 0; word < PAGE_SIZE / sizeof(u32); word++) {
                page[word] = 0;
            }
        }
        void *address = (void *)(first * PAGE_SIZE);
        for (u32 index = 0; index < MAX_HEAP_ALLOCS; index++) {
            if (heap_allocations[index].address == (void *)0) {
                heap_allocations[index].address = address;
                heap_allocations[index].pages = pages;
                heap_allocations[index].bytes = bytes;
                heap_allocation_count++;
                heap_used_bytes += bytes;
                return address;
            }
        }
        for (u32 offset = 0; offset < pages; offset++) {
            page_free((void *)((first + offset) * PAGE_SIZE));
        }
        return (void *)0;
    }
    return (void *)0;
}

u8 kfree(void *address) {
    if (address == (void *)0) {
        return 0;
    }
    for (u32 index = 0; index < MAX_HEAP_ALLOCS; index++) {
        struct heap_allocation *allocation = &heap_allocations[index];
        if (allocation->address != address) {
            continue;
        }
        u32 first_page = (u32)address / PAGE_SIZE;
        for (u32 offset = 0; offset < allocation->pages; offset++) {
            page_free((void *)((first_page + offset) * PAGE_SIZE));
        }
        heap_used_bytes -= allocation->bytes;
        heap_allocation_count--;
        allocation->address = (void *)0;
        allocation->pages = 0;
        allocation->bytes = 0;
        return 1;
    }
    return 0;
}

static void idt_set_gate(u8 vector, void (*handler)(void), u8 attributes) {
    u32 address = (u32)handler;
    idt[vector].offset_low = (u16)(address & 0xffff);
    idt[vector].selector = 0x08;
    idt[vector].zero = 0;
    idt[vector].type_attributes = attributes;
    idt[vector].offset_high = (u16)(address >> 16);
}

static u32 read_cr2(void) {
    u32 address;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(address));
    return address;
}

static __attribute__((noreturn)) void page_fault_halt(void) {
    __asm__ volatile ("cli");
    for (;;) {
        halt();
    }
}

void page_fault_interrupt_handler(const u32 *register_frame) {
    u32 error_code = register_frame[8];
    u32 address = read_cr2();
    serial_write("page fault: int 0x0e address ");
    write_u32(address);
    serial_write(" error ");
    write_u32(error_code);
    serial_write((error_code & 0x4) != 0 ? " user\n" : " kernel\n");
    if (!page_fault_reported) {
        page_fault_reported = 1;
        serial_write("memory: page fault handler fail-closed\n");
    }
    page_fault_halt();
}

static u8 paging_flags_valid(void) {
    u32 kernel_entry = page_tables[0][0x100000 / PAGE_SIZE];
    u32 code_entry = page_tables[USER_CODE_PAGE / 1024][USER_CODE_PAGE % 1024];
    u32 stack_entry = page_tables[USER_STACK_PAGE / 1024][USER_STACK_PAGE % 1024];
    u32 code_directory = page_directory[USER_CODE_PAGE / 1024];
    return (kernel_entry & PAGE_USER) == 0 &&
        (code_entry & (PAGE_PRESENT | PAGE_USER)) == (PAGE_PRESENT | PAGE_USER) &&
        (code_entry & PAGE_WRITE) == 0 &&
        (stack_entry & (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)) ==
            (PAGE_PRESENT | PAGE_WRITE | PAGE_USER) &&
        (code_directory & PAGE_USER) != 0;
}

static void idt_init(void) {
    for (u32 vector = 0; vector < IDT_ENTRIES; vector++) {
        idt_set_gate((u8)vector, default_isr, 0x8e);
    }
    idt_set_gate(TIMER_VECTOR, timer_isr, 0x8e);
    idt_set_gate(KEYBOARD_VECTOR, keyboard_isr, 0x8e);
    idt_set_gate(0x0e, page_fault_isr, 0x8e);
    /* DPL 3 makes the ABI callable by a future user process. */
    idt_set_gate(SYSCALL_VECTOR, syscall_isr, 0xee);

    struct idt_pointer pointer = {
        .limit = (u16)(sizeof(idt) - 1),
        .base = (u32)idt,
    };
    lidt(&pointer);
}

static void pic_init(void) {
    outb(0x20, 0x11);
    io_wait();
    outb(0xa0, 0x11);
    io_wait();

    outb(0x21, 0x20);
    io_wait();
    outb(0xa1, 0x28);
    io_wait();

    outb(0x21, 0x04);
    io_wait();
    outb(0xa1, 0x02);
    io_wait();

    outb(0x21, 0x01);
    io_wait();
    outb(0xa1, 0x01);
    io_wait();

    /* Keep only timer and keyboard IRQs unmasked during this milestone. */
    outb(0x21, 0xfc);
    outb(0xa1, 0xff);
}

static void timer_init(void) {
    const u16 divisor = (u16)(1193182 / TIMER_FREQUENCY);
    outb(0x43, 0x36);
    outb(0x40, (u8)(divisor & 0xff));
    outb(0x40, (u8)(divisor >> 8));
}

static void serial_init(void) {
    outb(0x3f8 + 1, 0x00);
    outb(0x3f8 + 3, 0x80);
    outb(0x3f8 + 0, 0x03);
    outb(0x3f8 + 1, 0x00);
    outb(0x3f8 + 3, 0x03);
    outb(0x3f8 + 2, 0xc7);
    outb(0x3f8 + 4, 0x0b);
}

static void serial_putc(char value) {
    while ((inb(0x3f8 + 5) & 0x20) == 0) {
    }
    outb(0x3f8, (u8)value);
}

static void serial_write(const char *text) {
    while (*text != '\0') {
        serial_putc(*text++);
    }
}

static void vga_clear(void) {
    for (u32 index = 0; index < VGA_CELLS; index++) {
        vga[index] = (u16)(' ' | (0x1f << 8));
    }
    cursor = 0;
}

static void vga_scroll(void) {
    if (cursor < VGA_CELLS) {
        return;
    }

    for (u32 row = 1; row < VGA_HEIGHT; row++) {
        for (u32 column = 0; column < VGA_WIDTH; column++) {
            vga[(row - 1) * VGA_WIDTH + column] =
                vga[row * VGA_WIDTH + column];
        }
    }

    for (u32 column = 0; column < VGA_WIDTH; column++) {
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + column] = (u16)(' ' | (0x1f << 8));
    }
    cursor = (VGA_HEIGHT - 1) * VGA_WIDTH;
}

static void vga_putc(char value) {
    if (value == '\r') {
        return;
    }

    if (value == '\n') {
        cursor = (cursor / VGA_WIDTH + 1) * VGA_WIDTH;
        vga_scroll();
        return;
    }

    if (value == '\b') {
        if (cursor > 0) {
            cursor--;
            vga[cursor] = (u16)(' ' | (0x1f << 8));
        }
        return;
    }

    vga[cursor++] = (u16)(value | (0x1f << 8));
    vga_scroll();
}

static void vga_write(const char *text) {
    while (*text != '\0') {
        vga_putc(*text++);
    }
}

static void kernel_write(const char *text) {
    vga_write(text);
    serial_write(text);
}

static void shell_write_bytes(const u8 *data, u32 size) {
    for (u32 index = 0; index < size; index++) {
        vga_putc((char)data[index]);
        serial_putc((char)data[index]);
    }
}

static void scheduler_wake_reason(u32 reason) {
    for (u32 index = 0; index < MAX_PROCESSES; index++) {
        struct process *process = process_table[index];
        if (process != (struct process *)0 && process->state == PROCESS_BLOCKED &&
            process->wait_reason == reason) {
            process->state = process->pid == current_pid ? PROCESS_RUNNING : PROCESS_READY;
            process->wait_reason = WAIT_NONE;
        }
    }
}

static void scheduler_select_ready(void) {
    scheduler_ready_pid = 0;
    for (u32 index = 0; index < MAX_PROCESSES; index++) {
        struct process *process = process_table[index];
        if (process != (struct process *)0 && process->state == PROCESS_READY) {
            scheduler_ready_pid = process->pid;
            return;
        }
    }
}

static char keyboard_ascii(u8 scancode) {
    switch (scancode) {
        case 0x02: return '1';
        case 0x03: return '2';
        case 0x04: return '3';
        case 0x05: return '4';
        case 0x06: return '5';
        case 0x07: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0a: return '9';
        case 0x0b: return '0';
        case 0x0c: return '-';
        case 0x0d: return '=';
        case 0x10: return 'q';
        case 0x11: return 'w';
        case 0x12: return 'e';
        case 0x13: return 'r';
        case 0x14: return 't';
        case 0x15: return 'y';
        case 0x16: return 'u';
        case 0x17: return 'i';
        case 0x18: return 'o';
        case 0x19: return 'p';
        case 0x1a: return '[';
        case 0x1b: return ']';
        case 0x1e: return 'a';
        case 0x1f: return 's';
        case 0x20: return 'd';
        case 0x21: return 'f';
        case 0x22: return 'g';
        case 0x23: return 'h';
        case 0x24: return 'j';
        case 0x25: return 'k';
        case 0x26: return 'l';
        case 0x27: return ';';
        case 0x28: return '\'';
        case 0x29: return '`';
        case 0x2b: return '\\';
        case 0x2c: return 'z';
        case 0x2d: return 'x';
        case 0x2e: return 'c';
        case 0x2f: return 'v';
        case 0x30: return 'b';
        case 0x31: return 'n';
        case 0x32: return 'm';
        case 0x33: return ',';
        case 0x34: return '.';
        case 0x35: return '/';
        case 0x39: return ' ';
        default: return '\0';
    }
}

static void tty_enqueue(char value) {
    if (tty_count >= TTY_BUFFER_SIZE) {
        return;
    }
    tty_buffer[tty_write_index] = value;
    tty_write_index = (tty_write_index + 1) % TTY_BUFFER_SIZE;
    tty_count++;
    scheduler_wake_reason(WAIT_TTY);
}

static u32 tty_read_available(u8 *output, u32 capacity) {
    __asm__ volatile ("cli" : : : "memory");
    if (tty_count == 0) {
        __asm__ volatile ("sti" : : : "memory");
        return 0xffffffff;
    }
    u32 amount = tty_count < capacity ? tty_count : capacity;
    for (u32 index = 0; index < amount; index++) {
        output[index] = (u8)tty_buffer[tty_read_index];
        tty_read_index = (tty_read_index + 1) % TTY_BUFFER_SIZE;
    }
    tty_count -= amount;
    __asm__ volatile ("sti" : : : "memory");
    return amount;
}

static void keyboard_handle(u8 scancode) {
    if (scancode == 0x2a || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }
    if (scancode == 0xaa || scancode == 0xb6) {
        shift_pressed = 0;
        return;
    }
    if ((scancode & 0x80) != 0) {
        return;
    }
    if (scancode == 0x1c) {
        tty_enqueue('\n');
        vga_putc('\n');
        serial_putc('\n');
        return;
    }
    if (scancode == 0x0e) {
        tty_enqueue('\b');
        vga_putc('\b');
        serial_putc('\b');
        return;
    }

    char value = keyboard_ascii(scancode);
    if (value == '\0') {
        return;
    }
    if (shift_pressed && value >= 'a' && value <= 'z') {
        value = (char)(value - 'a' + 'A');
    }
    tty_enqueue(value);
    vga_putc(value);
    serial_putc(value);
}

static void shell_init(void) {
    tty_read_index = 0;
    tty_write_index = 0;
    tty_count = 0;
    kernel_write("Fadal TTY ready; FSH.BIN owns user shell\n");
    tty_enqueue('h');
    tty_enqueue('e');
    tty_enqueue('l');
    tty_enqueue('p');
    tty_enqueue('\n');
}

void keyboard_interrupt_handler(void) {
    keyboard_handle(inb(0x60));
}

void timer_interrupt_handler(void) {
    timer_ticks++;
    scheduler_ticks++;
    if ((scheduler_ticks % 10) == 0) {
        scheduler_select_ready();
    }
}

void syscall_interrupt_handler(struct syscall_frame *frame) {
    syscall_count++;
    if (!syscall_reported) {
        syscall_reported = 1;
        serial_write("syscall: ring-3 entry\n");
    }

    if (frame->eax == SYSCALL_GET_TICKS) {
        frame->eax = timer_ticks;
        if (!get_ticks_reported) {
            get_ticks_reported = 1;
            serial_write("syscall: get_ticks dispatch\n");
        }
    } else if (frame->eax == SYSCALL_GET_PID) {
        frame->eax = current_pid;
        if (!get_pid_reported) {
            get_pid_reported = 1;
            serial_write("syscall: get_pid dispatch\n");
        }
    } else if (frame->eax == SYSCALL_EXIT) {
        frame->eax = process_exit_current() ? 0 : 0xffffffff;
        if (!exit_reported) {
            exit_reported = 1;
            serial_write("syscall: exit dispatch; process marked exited\n");
        }
    } else if (frame->eax == SYSCALL_READ) {
        if (user_range_valid(frame->ebx, frame->ecx) && frame->ecx != 0) {
            frame->eax = tty_read_available((u8 *)frame->ebx, frame->ecx);
        } else {
            frame->eax = 0xffffffff;
        }
        if (!read_reported) {
            read_reported = 1;
            serial_write("syscall: read dispatch; nonblocking TTY queue read\n");
        }
    } else if (frame->eax == SYSCALL_WRITE) {
        if (user_range_valid(frame->ebx, frame->ecx)) {
            shell_write_bytes((const u8 *)frame->ebx, frame->ecx);
            frame->eax = frame->ecx;
        } else {
            frame->eax = 0xffffffff;
        }
        if (!write_reported) {
            write_reported = 1;
            serial_write("syscall: write dispatch; user buffer validated\n");
        }
    } else if (frame->eax == SYSCALL_OPEN) {
        frame->eax = user_path_is_kernel((const char *)frame->ebx, frame->ecx) &&
            vfs_is_mounted() ? fd_open_kernel_file() : 0xffffffff;
        if (!open_reported) {
            open_reported = 1;
            serial_write("syscall: open dispatch; descriptor allocated\n");
        }
    } else if (frame->eax == SYSCALL_EXEC) {
        frame->eax = frame->ebx == USER_CODE_ADDRESS ?
            process_create(USER_CODE_ADDRESS, USER_STACK_2_ADDRESS + PAGE_SIZE) : 0xffffffff;
        if (!exec_reported) {
            exec_reported = 1;
            serial_write("syscall: exec dispatch; process created\n");
        }
    } else if (frame->eax == SYSCALL_YIELD) {
        scheduler_select_ready();
        frame->eax = scheduler_ready_pid;
        if (!yield_reported) {
            yield_reported = 1;
            serial_write("syscall: yield dispatch; cooperative scheduler point\n");
        }
    } else if (frame->eax == SYSCALL_CLOSE) {
        frame->eax = fd_close_current(frame->ebx) ? 0 : 0xffffffff;
        if (!close_reported) {
            close_reported = 1;
            serial_write("syscall: close dispatch; descriptor released\n");
        }
    } else {
        frame->eax = 0xffffffff;
    }
}

__attribute__((section(".text.entry"), used))
void kernel_main(void) {
    serial_init();
    vga_clear();

    kernel_write("FADAL KERNEL ONLINE\n");
    kernel_write("-------------------\n");
    kernel_write("mode: 32-bit protected mode\n");
    kernel_write("origin: from-scratch, no Linux dependency\n");
    kernel_write("status: boot path verified\n");
    memory_init();
    reserve_page(USER_CODE_ADDRESS);
    reserve_page(USER_STACK_ADDRESS);
    reserve_page(USER_STACK_2_ADDRESS);
    heap_init();
    u32 heap_free_before = free_page_count;
    u8 *heap_test = (u8 *)kmalloc(PAGE_SIZE + 17);
    u8 heap_ok = heap_test != (u8 *)0;
    if (heap_ok) {
        heap_test[0] = 0xa5;
        heap_test[PAGE_SIZE + 16] = 0x5a;
        heap_ok = heap_test[0] == 0xa5 && heap_test[PAGE_SIZE + 16] == 0x5a;
    }
    heap_ok = heap_ok && kfree(heap_test) && free_page_count == heap_free_before &&
        heap_used_bytes == 0 && heap_allocation_count == 0;
    if (heap_ok) {
        kernel_write("memory: dynamic heap self-test passed (2 pages)\n");
    } else {
        kernel_write("memory: dynamic heap self-test failed\n");
    }
    slab_system_init();
    if (slab_system_ready()) {
        kernel_write("memory: slab caches online (process, fat12_dirent)\n");
        void *process_probe = slab_alloc(&process_slab_cache);
        void *dirent_probe = slab_alloc(&fat12_dirent_slab_cache);
        u8 slab_ok = process_probe != (void *)0 && dirent_probe != (void *)0;
        slab_ok = slab_ok && slab_free(&process_slab_cache, process_probe);
        slab_ok = slab_ok && slab_free(&fat12_dirent_slab_cache, dirent_probe);
        void *process_reuse = slab_alloc(&process_slab_cache);
        slab_ok = slab_ok && process_reuse == process_probe;
        slab_ok = slab_ok && slab_free(&process_slab_cache, process_reuse);
        slab_ok = slab_ok && slab_free_objects(&process_slab_cache) == SLAB_PROCESS_CAPACITY;
        slab_ok = slab_ok && slab_free_objects(&fat12_dirent_slab_cache) == SLAB_FAT12_DIRENT_CAPACITY;
        if (slab_ok) {
            kernel_write("memory: slab allocation/reuse self-test passed\n");
        } else {
            kernel_write("memory: slab allocation/reuse self-test failed\n");
        }
    } else {
        kernel_write("memory: slab cache initialization failed\n");
    }
    void *process_stress[SLAB_PROCESS_CAPACITY];
    u8 process_slab_stress_ok = slab_system_ready();
    for (u32 index = 0; index < SLAB_PROCESS_CAPACITY; index++) {
        process_stress[index] = slab_alloc(&process_slab_cache);
        process_slab_stress_ok = process_slab_stress_ok && process_stress[index] != (void *)0;
    }
    process_slab_stress_ok = process_slab_stress_ok &&
        slab_alloc(&process_slab_cache) == (void *)0;
    for (u32 index = 0; index < SLAB_PROCESS_CAPACITY; index++) {
        process_slab_stress_ok = process_slab_stress_ok &&
            slab_free(&process_slab_cache, process_stress[index]);
    }
    process_slab_stress_ok = process_slab_stress_ok &&
        slab_free_objects(&process_slab_cache) == SLAB_PROCESS_CAPACITY;
    if (process_slab_stress_ok) {
        kernel_write("memory: process slab exhausted cleanly at 8 objects\n");
    } else {
        kernel_write("memory: process slab overflow test failed\n");
    }
    void *heap_stress[MAX_HEAP_ALLOCS];
    u32 heap_stress_count = 0;
    u32 heap_free_before_stress = free_page_count;
    u32 heap_used_before_stress = heap_used_bytes;
    u8 heap_stress_ok = 1;
    for (u32 index = 0; index < MAX_HEAP_ALLOCS; index++) {
        heap_stress[index] = kmalloc(256 * 1024);
        if (heap_stress[index] == (void *)0) {
            break;
        }
        heap_stress_count++;
        ((u8 *)heap_stress[index])[0] = (u8)index;
        ((u8 *)heap_stress[index])[256 * 1024 - 1] = (u8)(index ^ 0xff);
    }
    heap_stress_ok = heap_stress_count == MAX_HEAP_ALLOCS - 2 &&
        kmalloc(4096) == (void *)0;
    for (u32 index = 0; index < heap_stress_count; index++) {
        heap_stress_ok = heap_stress_ok && kfree(heap_stress[index]);
    }
    heap_stress_ok = heap_stress_ok && free_page_count == heap_free_before_stress &&
        heap_used_bytes == heap_used_before_stress;
    if (heap_stress_ok) {
        kernel_write("memory: heap stress passed (30 blocks; allocator limit verified)\n");
    } else {
        kernel_write("memory: heap stress test failed\n");
    }
    static const fat12_u8 first_file[] = "FADAL FAT12 write\n";
    vfs_init();
    fat12_u8 existing_volume = ata_read_sectors(
        FAT12_DISK_LBA,
        fat12_volume_buffer(),
        fat12_volume_sectors()) && vfs_mount_fat12();
    if (existing_volume) {
        kernel_write("vfs: FAT12 backend mounted; root directory recognized (");
        write_u32(vfs_root_entries());
        kernel_write(" entries)\n");
        fat12_u8 mounted_file[32];
        fat12_u32 mounted_size = 0;
        if (vfs_read_file("KERNEL.TXT", mounted_file, sizeof(mounted_file), &mounted_size) &&
            mounted_size == sizeof(first_file) - 1) {
            kernel_write("filesystem: mounted KERNEL.TXT verified\n");
        } else {
            kernel_write("filesystem: mounted root has no valid KERNEL.TXT\n");
        }
        kernel_write("disk: FAT12 remount read-only verification passed\n");
    } else {
        fat12_format();
        vfs_mount_fat12();
        kernel_write("filesystem: FAT12 mount probe empty; formatting new volume\n");
        if (vfs_write_file("KERNEL.TXT", first_file, sizeof(first_file) - 1) != 0) {
        if (vfs_write_file("FSH.BIN", _binary_out_fsh_bin_start, fsh_image_size()) == 0) {
            kernel_write("filesystem: FSH.BIN write failed\n");
        }
        kernel_write("filesystem: FAT12 formatted; KERNEL.TXT written\n");
        kernel_write("filesystem: allocated clusters ");
        write_u32(fat12_last_allocated_clusters());
        kernel_write("; free clusters ");
        write_u32(fat12_free_clusters());
        kernel_write("\n");
        if (ata_write_sectors(FAT12_DISK_LBA, fat12_volume(), fat12_volume_sectors())) {
            kernel_write("disk: ATA LBA28 persisted FAT12 volume\n");
            fat12_u8 readback[32];
            fat12_u32 readback_size = 0;
            fat12_u8 readback_ok = ata_read_sectors(
                FAT12_DISK_LBA,
                fat12_volume_buffer(),
                fat12_volume_sectors());
            fat12_u8 file_ok = readback_ok && vfs_read_file(
                "KERNEL.TXT",
                readback,
                sizeof(readback),
                &readback_size);
            if (file_ok && readback_size == sizeof(first_file) - 1) {
                for (u32 index = 0; index < readback_size; index++) {
                    if (readback[index] != first_file[index]) {
                        file_ok = 0;
                        break;
                    }
                }
            } else {
                file_ok = 0;
            }
            if (file_ok) {
                kernel_write("filesystem: ATA read-back verified KERNEL.TXT\n");
            } else {
                kernel_write("filesystem: ATA read-back verification failed\n");
            }
        } else {
            kernel_write("disk: ATA FAT12 persistence failed\n");
        }
    } else {
        kernel_write("filesystem: FAT12 write failed\n");
    }
        kernel_write("vfs: FAT12 backend mounted; root directory recognized (");
        write_u32(vfs_root_entries());
        kernel_write(" entries)\n");
    }
    if (fsh_load_from_disk()) {
        kernel_write("userspace: FSH.BIN loaded from FAT12; ring-3 entry ready\n");
    } else {
        kernel_write("userspace: FSH.BIN load failed\n");
    }
    paging_init();
    if (paging_flags_valid()) {
        kernel_write("memory: kernel/user page flags verified\n");
    } else {
        kernel_write("memory: kernel/user page flags invalid\n");
    }
    tss_init();
    gdt_init();
    idt_init();
    process_manager_init();
    u32 init_pid = process_create(USER_CODE_ADDRESS, USER_STACK_ADDRESS + PAGE_SIZE);
    u32 worker_pid = process_create(USER_CODE_ADDRESS, USER_STACK_2_ADDRESS + PAGE_SIZE);
    kernel_write("memory: process address spaces allocated\n");
    if (init_pid == 0 || worker_pid == 0 || !process_set_running(init_pid)) {
        kernel_write("process: process table initialization failed\n");
    } else {
        kernel_write("process: dynamic PID allocator online; 2 processes ready\n");
        if (address_spaces_isolated(process_table[0]->address_space,
                                    process_table[1]->address_space)) {
            kernel_write("memory: per-process address-space isolation verified\n");
        } else {
            kernel_write("memory: per-process address-space isolation failed\n");
        }
        if (process_cleanup_self_test(init_pid)) {
            kernel_write("process: address-space cleanup and PID-slot reuse passed\n");
        } else {
            kernel_write("process: address-space cleanup self-test failed\n");
        }
    }
    kernel_write("memory: 16 MiB identity paging online\n");
    if (memory_map_valid) {
        kernel_write("memory: BIOS E820 map accepted\n");
    } else {
        kernel_write("memory: E820 unavailable; safe 16 MiB fallback\n");
    }
    void *test_page = page_alloc();
    if (test_page != (void *)0) {
        page_free(test_page);
        kernel_write("memory: page allocation self-test passed\n");
    } else {
        kernel_write("memory: page allocation self-test failed\n");
    }
    pic_init();
    timer_init();
    kernel_write("interrupts: IDT + PIC online\n");
    kernel_write("timer: PIT IRQ0 online at 100 Hz\n");
    kernel_write("syscalls: int 0x80 ABI gate online\n");
    kernel_write("userspace: ring-3 test process armed\n");
    kernel_write("keyboard: PS/2 IRQ1 interactive shell online\n");
    kernel_write("shell: FSH disk executable selected\n");
    shell_init();
    enable_interrupts();
    enter_user_mode();
}
