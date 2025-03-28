#pragma once

#include <io.h>

struct ustar_filesystem {
    INHERITS(struct filesystem);
};

struct ustar_file {
    INHERITS(struct file);
};

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
    char data[]; // Array pointing to the data area following the header
                 // (flexible array member)
} __attribute__((packed));

bool ustar_init(struct block_device *dev, struct block *block);
