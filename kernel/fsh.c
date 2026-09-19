#include "fsh.h"

static fsh_u8 line_equal(const char *line, fsh_u32 length, const char *command) {
    fsh_u32 command_length = 0;
    while (command[command_length] != '\0') {
        command_length++;
    }
    if (length != command_length) {
        return 0;
    }
    for (fsh_u32 index = 0; index < length; index++) {
        if (line[index] != command[index]) {
            return 0;
        }
    }
    return 1;
}

void fsh_run_line(const struct fsh_context *context, const char *line, fsh_u32 length) {
    if (length == 0) {
        return;
    }
    if (line_equal(line, length, "help")) {
        context->write("commands: help info mem uptime status mount ls cat KERNEL.TXT clear\n");
    } else if (line_equal(line, length, "info")) {
        context->info();
    } else if (line_equal(line, length, "mem")) {
        context->mem();
    } else if (line_equal(line, length, "uptime")) {
        context->uptime();
    } else if (line_equal(line, length, "status")) {
        context->status();
    } else if (line_equal(line, length, "mount")) {
        context->mount();
    } else if (line_equal(line, length, "ls")) {
        context->list();
    } else if (line_equal(line, length, "cat KERNEL.TXT")) {
        context->cat_kernel();
    } else if (line_equal(line, length, "clear")) {
        context->clear();
    } else {
        context->write("unknown command; try help\n");
    }
}
