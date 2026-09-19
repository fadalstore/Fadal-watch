#ifndef FADAL_FSH_H
#define FADAL_FSH_H

typedef unsigned char fsh_u8;
typedef unsigned int fsh_u32;
typedef void (*fsh_handler)(void);
typedef void (*fsh_write_fn)(const char *text);

struct fsh_context {
    fsh_write_fn write;
    fsh_handler info;
    fsh_handler mem;
    fsh_handler uptime;
    fsh_handler status;
    fsh_handler mount;
    fsh_handler list;
    fsh_handler cat_kernel;
    fsh_handler clear;
};

void fsh_run_line(const struct fsh_context *context, const char *line, fsh_u32 length);

#endif
