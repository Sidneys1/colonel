#pragma once

#include <console.h>
#include <spinlock.h>
#include <stddef.h>

extern struct io_config_t {
    void (*volatile putc)(char);
    int (*volatile getc)(void);
} kernel_io_config;

enum StreamDirection : uint8_t {
    STREAM_OUT,
    STREAM_IN
};

struct stream {
    struct stream *target;
    char *buffer;
    uint16_t buffer_r, buffer_w;
    enum StreamDirection direction;
    struct spinlock lock;
    bool auto_flush;
};

extern struct stream stdout, stdin;

void s_putchar(struct stream *stream, char ch);
void sbi_putc(char);
void init_streams(void);
struct stream *create_stream(enum StreamDirection dir, struct stream *target, bool buffered, bool auto_flush);
