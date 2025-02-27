#include <common.h>
#include <harts.h>
#include <kernel.h>
#include <memory/page_allocator.h>
#include <sbi/sbi.h>
#include <spinlock.h>
#include <stdio.h>
#include <devices/uart.h>
#include <console.h>
#include <memory/slab_allocator.h>

struct spinlock lock = {.name = "CONSOLE", .hart = 0, .locked = 0};

void sbi_putc(char c) {
    sbi_call(c, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}

int sbi_getc() {
    // printf("sbi getc\n");
    // TODO: make this non-blocking.
    return sbi_call(0, 0, 0, 0, 0, 0, 0, 2).error;
    // int ret;
    // while ((ret = sbi_call(0, 0, 0, 0, 0, 0, 0, 2).error) == -1);
    // return ret;
}

struct io_config_t kernel_io_config = {.putc = &sbi_putc, .getc = &sbi_getc };

int getchar(void) {
    acquire(&lock);
    int ret = kernel_io_config.getc();
    release(&lock);
    return ret;
}

void s_putchar(struct stream *stream, char ch);

void s_flush(struct stream *stream) {
    if (stream == &stdout) {
        // kprintf("Flushing stdout\n");
        acquire(&lock);
        // for (int i = 0; i < 10; i++)
        //     sbi_putc('!');
        while (stdout.buffer_r < stdout.buffer_w)
            kernel_io_config.putc(stdout.buffer[stdout.buffer_r++ % PAGE_SIZE]);
        release(&lock);
        return;
    }
    acquire(&stream->target->lock);
    while (stream->buffer_r < stream->buffer_w)
        s_putchar(stream->target, stream->buffer[stream->buffer_r++ % PAGE_SIZE]);
    release(&stream->target->lock);
    s_flush(stream->target);
}

void s_putchar(struct stream *stream, char ch) {
    if (stream->buffer != NULL) {
        stream->buffer[stream->buffer_w++ % PAGE_SIZE] = ch;
        if (stream->buffer_w == (stream->buffer_r + PAGE_SIZE) || (stream->auto_flush && ch == '\n'))
            s_flush(stream);
        return;
    }

    if (stream == &stdout) {
        kernel_io_config.putc(ch);
        return;
    }

    s_putchar(stream->target, ch);
}

void putchar(char ch) {
    // s_putchar(&stdout, ch);
    s_putchar(get_hart_local()->stdout, ch);
    // hart->buffer[hart->buffer_idx++] = ch;
    // if ((kernel_io_config.auto_flush && ch == '\n')) {
    //     s_flush(hart->stdout);
    //     // flush();
    // }
}
struct stream stdout = {.direction = STREAM_OUT,  /*.no = 0,*/ .target = NULL, .lock={.locked=0, .name=NULL, .hart=0}, .buffer = NULL};

// Flush console stdout
void flush(void) {
    // for (char* i = "flush"; *i != '\0'; i++)
    //     sbi_putc(*i);
    s_flush(get_hart_local()->stdout);
    // acquire(&lock);
    // // for (int i = 0; i < 5; i++)
    // //     kernel_io_config.putc('X');
    // // struct hart_local *hart = get_hart_local();
    // while (stdout.buffer_r < stdout.buffer_w)
    //     kernel_io_config.putc(stdout.buffer[stdout.buffer_r++ % PAGE_SIZE]);
    // // hart->buffer[hart->buffer_idx] = '\0';
    // // hart->buffer_idx = 0;
    // release(&lock);
}

// _Atomic uint16_t streamno = 0;
// static struct slab_16 streams;

void init_streams(void) {
    // create_slab(&streams);

    stdout.buffer = (char*)alloc_pages(1);
}

struct stream *create_stream(enum StreamDirection dir, struct stream *target, bool buffered, bool auto_flush) {
    struct stream *stream = (struct stream*)slab_alloc(&root_slab32);
    // TODO: validate target (exists, direction, etc);
    stream->target = target;
    // stream->no = ++streamno;
    stream->direction = dir;
    stream->auto_flush = auto_flush;
    stream->lock = (struct spinlock){.locked=0, .name=NULL, .hart=0};

    if (buffered) {
        // TODO: buffers don't need to be 4k each...
        stream->buffer = (char*)alloc_pages(1);
    } else {
        stream->buffer = NULL;
    }

    return stream;
}
