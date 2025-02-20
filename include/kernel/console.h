#pragma once

#include <console.h>
#include <stddef.h>

extern struct io_config_t {
    void (*volatile putc)(char);
    int (*volatile getc)(void);
} kernel_io_config;

enum StreamDirection {
    STREAM_OUT,
    STREAM_IN
};

struct stream {
    enum StreamDirection direction;
    // uint16_t no;
    bool auto_flush;
    struct stream *target;
    uint16_t buffer_r, buffer_w;
    char *buffer;
};

extern struct stream stdout;

void sbi_putc(char);
void init_streams(void);
struct stream *create_stream(enum StreamDirection dir, struct stream *target, bool buffered, bool auto_flush);
