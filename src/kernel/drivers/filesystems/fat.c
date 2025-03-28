#include <inheritance.h>
#include <stddef.h>
#include <stdio.h>

#include <common.h>

#include <io.h>
#include <kernel.h>
#include <memory/slab_allocator.h>

#include <drivers/filesystems/fat.h>
#include <string.h>

struct __attribute__((__packed__)) fat {
    uint8_t magic[3];
    char version[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t number_of_fats;
    uint16_t number_of_root_directory_entries, logical_sector_count;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat, sectors_per_track, number_of_heads_or_sides;
    uint32_t number_of_hidden_sectors, large_sector_count;
};

struct __attribute__((__packed__)) fat_12_16 {
    struct fat fat;
    uint8_t drive_number, flags, signature;
    uint32_t serial;
    char label[11], system_identifier[8];
    uint8_t boot_code[448];
    uint16_t bootable_signature;
};

struct cluster {
} __attribute__((__packed__));

struct FATable {
    uint16_t media_type, partition_state;
    struct cluster clusters[];
} __attribute__((__packed__));

struct fat16_time {
    union {
        struct {
            uint8_t hours : 5, minutes : 6, seconds : 5;
        } __attribute__((__packed__));
        uint16_t raw;
    };
} __attribute__((__packed__));

struct fat16_date {
    union {
        struct {
            uint8_t day : 5, month : 4, year : 7;
        } __attribute__((__packed__));
        uint16_t raw;
    };
} __attribute__((__packed__));

struct fat_directory {
    union {
        uint8_t marker;
        struct {
            char name[8], ext[3];
        };
        char name8_3[11];
    };
    enum directory_attribute : uint8_t {
        NONE = 0,
        READ_ONLY,
        HIDDEN,
        SYSTEM = 0x04,
        VOLUME_ID = 0x08,
        DIRECTORY = 0x10,
        ARCHIVE = 0x20,
        LFN = READ_ONLY | HIDDEN | SYSTEM | VOLUME_ID
    } attributes;
    uint8_t reserved, creation_time_microseconds;
    struct fat16_time creation_time;
    struct fat16_date creation_date;
    struct fat16_date last_accessed_date;
    uint16_t cluster_high;
    struct fat16_time last_mod_time;
    struct fat16_date last_mod_date;
    uint16_t cluster_low;
    uint32_t file_size;
} __attribute__((__packed__));

typedef uint16_t wchar_t;

struct long_filename {
    uint8_t marker;
    wchar_t name[5];
    enum directory_attribute attribute;
    uint8_t reserved, chk_short_name;
    wchar_t name2[6];
    uint16_t cluster;
    wchar_t name3[2];
} __attribute__((__packed__));

#define FAT12_OFFSET(active_cluster) (active_cluster + (active_cluster / 2))
#define FAT12_SECTOR(x)              (1 + (FAT12_OFFSET(x) / SECTOR_SIZE))
#define FAT12_ENT_OFFSET(x)          (FAT12_OFFSET(x) % SECTOR_SIZE)
#define FAT12_DISC_OFF(x)            (FAT12_SECTOR(x) * SECTOR_SIZE + FAT12_ENT_OFFSET(x))
#define _FAT12_TABLE_VALUE(x)        (*(unsigned short *)(data + FAT12_ENT_OFFSET(x)))
#define FAT12_TABLE_VALUE(x)         ((x & 1) ? _FAT12_TABLE_VALUE(x) >> 4 : _FAT12_TABLE_VALUE(x) & 0xfff)

size_t fat12_read_file(struct filesystem *fs, void *restrict buffer, char (*name)[MAX_FILENAME_LENGTH]) {
    // Let's be lazy and find the `struct file` for this entry...
    const struct fs_entry *file = files_head;
    for (; file != NULL && strncmp(*file->name, *name, MAX_FILENAME_LENGTH) != 0; file = file->next)
        ;
    if (file == NULL)
        PANIC("Could not find fs_entry by that name!");
    FAT12_DBG("Found fs_entry: `%S` == `%S`?\n", *file->name, *name);
    const struct fat_file *fat_file = SUB(struct fat_file, *SUB(struct file, *file));
    FAT12_DBG("File should be at cluster #%u!\n", fat_file->start_cluster);

    const struct fat_filesystem *fatfs = SUB(struct fat_filesystem, *fs);

    size_t clusters_to_read = fat_file->super.size / fatfs->bytes_per_cluster;
    uint32_t start_cluster = fat_file->start_cluster, end_cluster = fat_file->start_cluster + 1;

    uint32_t active_cluster = start_cluster;
    uint32_t data_sector = FAT12_DISC_OFF(active_cluster) / SECTOR_SIZE;
    uint32_t current_sector = data_sector;
    uint8_t data[SECTOR_SIZE * 2];
    fs->device->read_block(fs->device, data, data_sector + fs->base_sector, 2);

    size_t bytes_read = 0;
    do {
        uint16_t tv = FAT12_TABLE_VALUE(active_cluster);
        while (tv == active_cluster + 1) {
            end_cluster++;
            active_cluster++;
            data_sector = FAT12_DISC_OFF(active_cluster) / SECTOR_SIZE;
            if (data_sector != current_sector) {
                current_sector = data_sector;
                fs->device->read_block(fs->device, data, data_sector + fs->base_sector, 2);
            }
            tv = FAT12_TABLE_VALUE(active_cluster);
        }
        FAT12_DBG("Will read these %u sequential clusters: %u-%u. That's %zu bytes!\n", end_cluster - start_cluster,
                  start_cluster, end_cluster - 1, fatfs->bytes_per_cluster * (end_cluster - start_cluster));

        const uint32_t first_sector = ((fatfs->relative_first_data_sector * fatfs->bytes_per_sector) +
                                       ((start_cluster - 2) * fatfs->bytes_per_cluster)) /
                                      SECTOR_SIZE;
        const uint32_t sectors_to_read =
            (((end_cluster - start_cluster) * fatfs->bytes_per_cluster) + SECTOR_SIZE - 1) / SECTOR_SIZE;
        FAT12_DBG("That translates to reading %u sectors (as there are %zu sectors per cluster) starting at sector "
                  "number %u.\n",
                  sectors_to_read, fatfs->bytes_per_sector, first_sector + fs->base_sector);
        // const size_t read = fs->device->read_block(fs->device, buffer, first_sector + fs->base_sector,
        // sectors_to_read) * SECTOR_SIZE; kprintf("Read %zu bytes (%zu sectors * %zu = %zu)...\n", read,
        // sectors_to_read, SECTOR_SIZE, sectors_to_read * SECTOR_SIZE); buffer += read; bytes_read += read;

        if (tv == 0xfff) {
            FAT12_DBG("Last sector! So far we've read %zu, meaning we have %zu left... Modulo is %u...\n", bytes_read,
                      fat_file->super.size - bytes_read, fat_file->super.size % fatfs->bytes_per_cluster);
            // We're including the last cluster!
            if ((end_cluster - start_cluster) > 1 && fat_file->super.size % fatfs->bytes_per_cluster) {
                // If we're reading more than one cluster, isolate the last one
                end_cluster--;
                FAT12_DBG("We were going to read %u, but now we're reading one less (%u-%u)...\n",
                          end_cluster - start_cluster + 1, start_cluster, end_cluster - 1);
                const size_t read =
                    fs->device->read_block(fs->device, buffer, first_sector + fs->base_sector,
                                           ((end_cluster - start_cluster) * fatfs->bytes_per_cluster) / SECTOR_SIZE) *
                    SECTOR_SIZE;
                FAT12_DBG("Read %zu bytes (%zu sectors * %zu = %zu)...\n", read,
                          ((end_cluster - start_cluster) * fatfs->bytes_per_cluster + SECTOR_SIZE - 1) / SECTOR_SIZE,
                          SECTOR_SIZE, ((end_cluster - start_cluster) * fatfs->bytes_per_cluster));
                buffer += read;
                bytes_read += read;
            }
            start_cluster = end_cluster++;
            FAT12_DBG("Now to read the final cluster (%u)...\n", start_cluster);
            const size_t bytes_to_read = fat_file->super.size % fatfs->bytes_per_cluster;

            // We're relying on a GCC extension here (dynamically sized arrays)...

            char temp[align_up(bytes_to_read, SECTOR_SIZE)] = {};
            const size_t read = fs->device->read_block(fs->device, temp,
                                                       ((fatfs->relative_first_data_sector * fatfs->bytes_per_sector) +
                                                        ((start_cluster - 2) * fatfs->bytes_per_cluster)) /
                                                               SECTOR_SIZE +
                                                           fs->base_sector,
                                                       (bytes_to_read + SECTOR_SIZE - 1) / SECTOR_SIZE) *
                                SECTOR_SIZE;
            if (read < bytes_to_read) {
                PANIC("Failed to read (%zu < %zu)???\n", read, bytes_to_read);
            }
            FAT12_DBG("Read %zu bytes (%zu sectors * %zu = %zu)...\n", read,
                      (bytes_to_read + SECTOR_SIZE - 1) / SECTOR_SIZE, SECTOR_SIZE,
                      align_up(bytes_to_read, SECTOR_SIZE));
            memcpy_s(buffer, bytes_to_read, temp, bytes_to_read);
            return bytes_read + bytes_to_read;
        } else {
            const size_t read =
                fs->device->read_block(fs->device, buffer, first_sector + fs->base_sector, sectors_to_read) *
                SECTOR_SIZE;
            FAT12_DBG("Read %zu bytes (%zu sectors * %zu = %zu)...\n", read, sectors_to_read, SECTOR_SIZE,
                      sectors_to_read * SECTOR_SIZE);
            buffer += read;
            bytes_read += read;
        }

        FAT12_DBG(ANSI_GREEN "\tCluster[%u]=%#hx\n", active_cluster, tv);
        clusters_to_read -= end_cluster - start_cluster;
        end_cluster = 1 + (start_cluster = active_cluster = tv);

        if (tv == 0xfff && clusters_to_read) {
            PANIC("Somehow got to the end of the file without having read enough data!! (clusters_to_read=%zu)\n",
                  clusters_to_read);
        }
    } while (clusters_to_read);
    PANIC("Unreachable!\n");
}

static size_t fat_no = 0;

bool fat12_init(const struct block_device *dev, uint32_t base_sector, const struct fat_12_16 *fat2) {
    const unsigned int num_root_dir_sectors =
        ((fat2->fat.number_of_root_directory_entries * 32) + (fat2->fat.bytes_per_sector - 1)) /
        fat2->fat.bytes_per_sector;
    const unsigned int relative_first_data_sector =
        fat2->fat.reserved_sectors + (fat2->fat.sectors_per_fat * fat2->fat.number_of_fats) + num_root_dir_sectors;

    // const unsigned int num_data_sectors = fat2->fat.logical_sector_count - (fat2->fat.reserved_sectors +
    // (fat2->fat.number_of_fats * fat2->fat.sectors_per_fat) + num_root_dir_sectors); const unsigned int num_clusters =
    // num_data_sectors / fat2->fat.sectors_per_cluster; unsigned int first_sector_of_cluster = ((cluster - 2) *
    // fat->sectors_per_cluster) + first_data_sector;

    const unsigned int relative_first_root_dir_sector = relative_first_data_sector - num_root_dir_sectors;
    FAT_DBG(ANSI_GREEN "\tFirst root dir sector: %u (absolute: %u)\n" ANSI_RESET, relative_first_root_dir_sector,
            base_sector + relative_first_root_dir_sector);

    uint32_t active_cluster = 0;
    uint32_t offset = FAT12_DISC_OFF(active_cluster);
    FAT12_DBG("Offset: %u (sector #%u)\n", offset, offset / SECTOR_SIZE + base_sector);

    uint8_t data[SECTOR_SIZE * 2];
    uint32_t data_sector = offset / SECTOR_SIZE;
    dev->read_block(dev, data, data_sector + base_sector, 2);

    uint16_t tv = FAT12_TABLE_VALUE(active_cluster);
    if ((tv & 0xf00) != 0xf00 || (tv & 0x0ff) != fat2->fat.media_descriptor) {
        kprintf(ANSI_RED "Cluster[0] was not 0xf%02x (it was %#06hx)!\n", fat2->fat.media_descriptor, tv);
        return false;
    }
    FAT12_DBG(ANSI_GREEN "\tCluster[0]=%#hx\n" ANSI_RESET, tv);

    active_cluster = 1;
    offset = FAT12_DISC_OFF(active_cluster);
    FAT12_DBG("Offset: %u (sector #%u)\n", offset, offset / SECTOR_SIZE);
    if ((offset / SECTOR_SIZE) != data_sector)
        dev->read_block(dev, data, data_sector + base_sector, 2);

    tv = FAT12_TABLE_VALUE(active_cluster);
    if (tv != 0xfff) {
        kprintf(ANSI_RED "Cluster[1] was not 0xfff (it was %#hx)!\n", tv);
        return false;
    }
    FAT12_DBG(ANSI_GREEN "\tCluster[1]=%#hx\n", tv);

    const uint32_t bytes_per_cluster = (fat2->fat.sectors_per_cluster * fat2->fat.bytes_per_sector);

    struct fat_filesystem *fs = slab_malloc(struct fat_filesystem);
    fs->bytes_per_cluster = bytes_per_cluster;
    fs->bytes_per_sector = fat2->fat.bytes_per_sector;
    fs->relative_first_data_sector = relative_first_data_sector;
    fs->super.type_name = "FAT12";
    fs->super.base_sector = base_sector;
    fs->super.device = dev;
    fs->super.read_file = fat12_read_file;
    add_filesystem(SUPER(*fs));

    FAT12_DBG("Each cluster is %u bytes. Data starts at %#p\n", bytes_per_cluster,
              relative_first_data_sector * fat2->fat.bytes_per_sector);

    FAT12_DBG("Root directory sectors: %u\n",
              fat2->fat.number_of_root_directory_entries * sizeof(struct directory) / SECTOR_SIZE);

    size_t this_drive = fat_no++;
    const struct fat_directory *d = (struct fat_directory *)data;
    char buffer[MAX_FILENAME_LENGTH] = {};
    // memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));

    // buffer[0] = '\0';
    for (int i = 0; i < fat2->fat.number_of_root_directory_entries; i++) {
        uint32_t sector_i = i * sizeof(struct directory) / SECTOR_SIZE;
        if (i % (sizeof(data) / sizeof(struct directory)) == 0) {
            IO_DBG("Copying in sector %zu-%zu...\n", relative_first_root_dir_sector + sector_i + base_sector,
                   relative_first_root_dir_sector + sector_i + 1 + base_sector);
            dev->read_block(dev, data, relative_first_root_dir_sector + sector_i + base_sector, 2);
        }
        const struct fat_directory *this_entry = &d[i % (sizeof(data) / sizeof(struct fat_directory))];
        if (this_entry->marker == 0xe5) {
            // printf("-unused entry-\n");
            continue;
        }
        if (this_entry->marker == 0x00)
            break;
        if (this_entry->attributes == LFN) {
            const struct long_filename *lfname = (struct long_filename *)this_entry;

#ifdef FAT12_DEBUG
            FAT12_DBG(ANSI_MAGENTA "Before: [");
            printf(ANSI_MAGENTA);
            for (size_t i = 0; i < sizeof(buffer); i++) {
                if (buffer[i] == '\0')
                    printf("␀");
                else if (buffer[i] < ' ' || buffer[i] == 0x7f)
                    printf("\\%hhu", buffer[i]);
                else
                    putchar(buffer[i]);
            }
            printf("]\n" ANSI_RESET);
#endif

            if (buffer[0] != '\0') {
                for (size_t i = 0; i < (sizeof(buffer) - 13); i++)
                    buffer[(sizeof(buffer) - 1) - i] = buffer[(sizeof(buffer) - 1) - (i + 13)];
                buffer[sizeof(buffer) - 1] = '\0';
            }

#ifdef FAT12_DEBUG
            FAT12_DBG(ANSI_MAGENTA " Shift: [");
            printf(ANSI_MAGENTA);
            for (size_t i = 0; i < sizeof(buffer); i++) {
                if (buffer[i] == '\0')
                    printf("␀");
                else if (buffer[i] < ' ' || buffer[i] == 0x7f)
                    printf("\\%hhu", buffer[i]);
                else
                    putchar(buffer[i]);
            }
            printf("]\n" ANSI_RESET);
#endif

            char *c2 = buffer;
            for (char *c = (char *)&lfname->name;
                 c < (((char *)&lfname->name) + sizeof(lfname->name)) && *c != '\0' && *c != 0xff; c += 2, c2++)
                *c2 = *c;
            for (char *c = (char *)&lfname->name2;
                 c < (((char *)&lfname->name2) + sizeof(lfname->name2)) && *c != '\0' && *c != 0xff; c += 2, c2++)
                *c2 = *c;
            for (char *c = (char *)&lfname->name3;
                 c < (((char *)&lfname->name3) + sizeof(lfname->name3)) && *c != '\0' && *c != 0xff; c += 2, c2++)
                *c2 = *c;

#ifdef FAT12_DEBUG
            FAT12_DBG(ANSI_MAGENTA " After: [");
            printf(ANSI_MAGENTA);
            for (size_t i = 0; i < sizeof(buffer); i++) {
                if (buffer[i] == '\0')
                    printf("␀");
                else if (buffer[i] < ' ' || buffer[i] == 0x7f)
                    printf("\\x%hhu", buffer[i]);
                else
                    putchar(buffer[i]);
            }
            printf("]\n" ANSI_RESET);

            FAT12_DBG(ANSI_MAGENTA "Long filename: `%S`\n" ANSI_RESET, buffer);
#endif
            continue;
        }

        const_string fname = {.head = this_entry->name, .tail = this_entry->ext};
        const_string fext = {.head = this_entry->ext, .tail = this_entry->ext + sizeof(this_entry->ext)};
        char *end = strchr(fname, ' ');
        if (end != NULL)
            fname.tail = end;
        if (this_entry->attributes == VOLUME_ID) {
            const_string find = strstr(fname, CSTR(" "));
            const_string sname = {.head = fname.head,
                                  .tail = find.tail == NULL ? fname.tail : (fname.head + (find.tail - find.head))};

            find = strstr(fext, CSTR(" "));
            const_string sext = {.head = fext.head,
                                 .tail = find.tail == NULL ? fext.tail : (fext.head + (find.tail - find.head))};
            kprintf(ANSI_ORANGE "\nLABEL: `%s.%s` %c%S%c\n", sname, sext, buffer[0] == '\0' ? ' ' : '(', buffer,
                    buffer[0] == '\0' ? ' ' : ')');
            buffer[0] = '\0';
            continue;
        }

        FAT_DBG(ANSI_CYAN "\nFile: `%s.%s` %c%S%c\n"
                          "\tAttributes: %#hhx\n"
                          "\tCreated: %u-%02hhu-%02hhu %02hhu:%02hhu:%02hhu.%03hhu\n"
                          "\tModified: %u-%02hhu-%02hhu %02hhu:%02hhu:%02hhu\n"
                          "\tAccessed: %u-%02hhu-%02hhu\n"
                          "\tCluster: H:%hu L:%hu\n"
                          "\tSize: %u\n" ANSI_RESET,
                fname, (const_string){.head = this_entry->ext, .tail = this_entry->ext + sizeof(this_entry->ext)},
                buffer[0] == '\0' ? ' ' : '(', buffer, buffer[0] == '\0' ? ' ' : ')', this_entry->attributes,
                1980 + this_entry->creation_date.year, this_entry->creation_date.month, this_entry->creation_date.day,
                this_entry->creation_time.hours, this_entry->creation_time.minutes, this_entry->creation_time.seconds,
                this_entry->creation_time_microseconds, 1980 + this_entry->last_mod_date.year,
                this_entry->last_mod_date.month, this_entry->last_mod_date.day, this_entry->last_mod_time.hours,
                this_entry->last_mod_time.minutes, this_entry->last_mod_time.seconds,
                1980 + this_entry->last_accessed_date.year, this_entry->last_accessed_date.month,
                this_entry->last_accessed_date.day, this_entry->cluster_high, this_entry->cluster_low,
                this_entry->file_size);

        FAT_DBG(ANSI_CYAN "\tFile should be %u clusters (last cluster is only %u bytes).\n" ANSI_RESET,
                this_entry->file_size / bytes_per_cluster + ((this_entry->file_size % bytes_per_cluster) ? 1 : 0), rem);

        struct fat_file *file = slab_malloc(struct fat_file);
        file->super.super.filesystem = SUPER(*fs);
        file->start_cluster = ((uint32_t)this_entry->cluster_high << 16) | this_entry->cluster_low;
        char (*fnameBuffer)[MAX_FILENAME_LENGTH] = (char (*)[MAX_FILENAME_LENGTH])_slab_malloc(MAX_FILENAME_LENGTH);

        if (buffer[0] == '\0') {
            const_string find = strstr(fname, CSTR(" "));
            const_string sname = {.head = fname.head,
                                  .tail = find.tail == NULL ? fname.tail : (fname.head + (find.tail - find.head))};

            find = strstr(fext, CSTR(" "));
            const_string sext = {.head = fext.head,
                                 .tail = find.tail == NULL ? fext.tail : (fext.head + (find.tail - find.head))};
            // snprintf(buffer, MAX_FILENAME_LENGTH, "%s.%s", this_drive, sname, sext);
            char *c = buffer;
            for (size_t i = 0; sname.head + i != sname.tail; i++, c++)
                *c = (sname.head[i] >= 'A' && sname.head[i] <= 'Z') ? sname.head[i] + ('a' - 'A') : sname.head[i];
            *(c++) = '.';
            for (size_t i = 0; sext.head + i != sext.tail; i++, c++)
                *c = (sext.head[i] >= 'A' && sext.head[i] <= 'Z') ? sext.head[i] + ('a' - 'A') : sext.head[i];
        }

        snprintf(*fnameBuffer, sizeof(*file->super.super.name), "fat%zu:/%S", this_drive, buffer);
        file->super.super.name = (char (*)[MAX_FILENAME_LENGTH])fnameBuffer;

        file->super.size = this_entry->file_size;
        add_fs_entry(SUPER(*SUPER(*file)));

        buffer[0] = '\0';
        // memset(buffer, 0, MAX_FILENAME_LENGTH);
    }

    return true;
}

#define FAT16_OFFSET(active_cluster)            (active_cluster * 2)
#define FAT16_SECTOR(active_cluster)            (1 + (FAT16_OFFSET(active_cluster) / SECTOR_SIZE))
#define FAT16_ENT_OFFSET(active_cluster)        (FAT16_OFFSET(active_cluster) % SECTOR_SIZE)
#define FAT16_DISC_OFF(active_cluster)          (FAT16_SECTOR(active_cluster) * SECTOR_SIZE + FAT16_ENT_OFFSET(active_cluster))
#define FAT16_TABLE_VALUE(active_cluster, data) (*(unsigned short *)&data[FAT16_ENT_OFFSET(active_cluster)])

size_t fat16_read_file(struct filesystem *fs, void *restrict buffer, char (*name)[MAX_FILENAME_LENGTH]) {
    // Let's be lazy and find the `struct file` for this entry...
    const struct fs_entry *file = files_head;
    for (; file != NULL && strncmp(*file->name, *name, MAX_FILENAME_LENGTH) != 0; file = file->next)
        ;
    if (file == NULL)
        PANIC("Could not find fs_entry by that name!");
    FAT12_DBG("Found fs_entry: `%S` == `%S`?\n", *file->name, *name);
    const struct fat_file *fat_file = SUB(struct fat_file, *SUB(struct file, *file));
    FAT12_DBG("File should be at cluster #%u!\n", fat_file->start_cluster);

    const struct fat_filesystem *fatfs = SUB(struct fat_filesystem, *fs);

    size_t clusters_to_read = fat_file->super.size / fatfs->bytes_per_cluster;
    uint32_t start_cluster = fat_file->start_cluster, end_cluster = fat_file->start_cluster + 1;

    uint32_t active_cluster = start_cluster;
    struct block data = {.block_number = FAT16_SECTOR(active_cluster) + fs->base_sector};
    // uint32_t current_sector = data.block_number;
    fs->device->read_block(fs->device, data.data, data.block_number, 1);

    size_t bytes_read = 0;
    do {
        uint16_t tv = FAT16_TABLE_VALUE(active_cluster, data.data);
        FAT16_DBG("Value of cluster %u is %hu...\n", active_cluster, tv);
        while (tv == active_cluster + 1) {
            end_cluster++;
            active_cluster++;
            const uint32_t data_sector = FAT16_SECTOR(active_cluster) + fs->base_sector;
            if (data_sector != data.block_number) {
                data.block_number = data_sector;
                fs->device->read_block(fs->device, data.data, data.block_number, 1);
            }
            tv = FAT16_TABLE_VALUE(active_cluster, data.data);
        }
        FAT16_DBG("Will read these %u sequential clusters: %u-%u. That's %zu bytes!\n", end_cluster - start_cluster,
                  start_cluster, end_cluster - 1, fatfs->bytes_per_cluster * (end_cluster - start_cluster));

        const uint32_t first_sector = ((fatfs->relative_first_data_sector * fatfs->bytes_per_sector) +
                                       ((start_cluster - 2) * fatfs->bytes_per_cluster)) /
                                      SECTOR_SIZE;
        const uint32_t sectors_to_read =
            (((end_cluster - start_cluster) * fatfs->bytes_per_cluster) + SECTOR_SIZE - 1) / SECTOR_SIZE;
        FAT16_DBG("That translates to reading %u sectors (as there are %zu sectors per cluster) starting at sector "
                  "number %u.\n",
                  sectors_to_read, fatfs->bytes_per_sector, first_sector + fs->base_sector);

        if (tv == 0xffff) {
            FAT16_DBG("Last sector! So far we've read %zu, meaning we have %zu left... Modulo is %u...\n", bytes_read,
                      fat_file->super.size - bytes_read, fat_file->super.size % fatfs->bytes_per_cluster);
            // We're including the last cluster!
            if ((end_cluster - start_cluster) > 1 && fat_file->super.size % fatfs->bytes_per_cluster) {
                // If we're reading more than one cluster, isolate the last one
                end_cluster--;
                FAT16_DBG("We were going to read %u, but now we're reading one less (%u-%u)...\n",
                          end_cluster - start_cluster + 1, start_cluster, end_cluster - 1);
                const size_t read =
                    fs->device->read_block(fs->device, buffer, first_sector + fs->base_sector,
                                           ((end_cluster - start_cluster) * fatfs->bytes_per_cluster) / SECTOR_SIZE) *
                    SECTOR_SIZE;
                FAT16_DBG("Read %zu bytes (%zu sectors * %zu = %zu)...\n", read,
                          ((end_cluster - start_cluster) * fatfs->bytes_per_cluster + SECTOR_SIZE - 1) / SECTOR_SIZE,
                          SECTOR_SIZE, ((end_cluster - start_cluster) * fatfs->bytes_per_cluster));
                buffer += read;
                bytes_read += read;
            }
            start_cluster = end_cluster++;
            FAT16_DBG("Now to read the final cluster (%u)...\n", start_cluster);
            const size_t bytes_to_read = fat_file->super.size % fatfs->bytes_per_cluster;

            // We're relying on a GCC extension here (dynamically sized arrays)...

            char temp[align_up(bytes_to_read, SECTOR_SIZE)] = {};
            const size_t read = fs->device->read_block(fs->device, temp,
                                                       ((fatfs->relative_first_data_sector * fatfs->bytes_per_sector) +
                                                        ((start_cluster - 2) * fatfs->bytes_per_cluster)) /
                                                               SECTOR_SIZE +
                                                           fs->base_sector,
                                                       (bytes_to_read + SECTOR_SIZE - 1) / SECTOR_SIZE) *
                                SECTOR_SIZE;
            if (read < bytes_to_read) {
                PANIC("Failed to read (%zu < %zu)???\n", read, bytes_to_read);
            }
            FAT16_DBG("Read %zu bytes (%zu sectors * %zu = %zu)...\n", read,
                      (bytes_to_read + SECTOR_SIZE - 1) / SECTOR_SIZE, SECTOR_SIZE,
                      align_up(bytes_to_read, SECTOR_SIZE));
            memcpy_s(buffer, bytes_to_read, temp, bytes_to_read);
            return bytes_read + bytes_to_read;
        } else {
            const size_t read =
                fs->device->read_block(fs->device, buffer, first_sector + fs->base_sector, sectors_to_read) *
                SECTOR_SIZE;
            FAT16_DBG("Read %zu bytes (%zu sectors * %zu = %zu)...\n", read, sectors_to_read, SECTOR_SIZE,
                      sectors_to_read * SECTOR_SIZE);
            buffer += read;
            bytes_read += read;
        }

        FAT16_DBG(ANSI_GREEN "\tCluster[%u]=%#hx\n", active_cluster, tv);
        clusters_to_read -= end_cluster - start_cluster;
        end_cluster = 1 + (start_cluster = active_cluster = tv);

        if (tv == 0xffff && clusters_to_read) {
            PANIC("Somehow got to the end of the file without having read enough data!! (clusters_to_read=%zu)\n",
                  clusters_to_read);
        }
    } while (clusters_to_read);
    PANIC("Unreachable!\n");
}

bool fat16_init(const struct block_device *dev, uint32_t base_sector, const struct fat_12_16 *fat2) {
    const unsigned int num_root_dir_sectors =
        ((fat2->fat.number_of_root_directory_entries * 32) + (fat2->fat.bytes_per_sector - 1)) /
        fat2->fat.bytes_per_sector;
    const unsigned int relative_first_data_sector =
        fat2->fat.reserved_sectors + (fat2->fat.sectors_per_fat * fat2->fat.number_of_fats) + num_root_dir_sectors;

    const unsigned int relative_first_root_dir_sector = relative_first_data_sector - num_root_dir_sectors;
    FAT_DBG(ANSI_GREEN "\tFirst root dir sector: %u (absolute: %u)\n", relative_first_root_dir_sector,
            base_sector + relative_first_root_dir_sector);

    uint32_t active_cluster = 0;
    FAT16_DBG("Offset: %u (sector #%u)\n", FAT16_OFFSET(active_cluster), FAT16_SECTOR(active_cluster) + base_sector);

    // uint8_t data[SECTOR_SIZE];
    struct block data = {.block_number = FAT16_SECTOR(active_cluster) + base_sector};
    dev->read_block(dev, data.data, data.block_number, 1);

    uint16_t tv = FAT16_TABLE_VALUE(active_cluster, data.data);
    if ((tv & 0xf00) != 0xf00 || (tv & 0x0ff) != fat2->fat.media_descriptor) {
        kprintf(ANSI_RED "Cluster[0] was not 0xf%02x (it was %#06hx)!\n", fat2->fat.media_descriptor, tv);
        return false;
    }
    FAT16_DBG(ANSI_GREEN "\tCluster[0]=%#hx\n", tv);

    active_cluster = 1;
    FAT16_DBG("Offset: %u (sector #%u)\n", FAT16_OFFSET(active_cluster), FAT16_SECTOR(active_cluster) + base_sector);
    if (FAT16_SECTOR(active_cluster) != (data.block_number - base_sector)) {
        data.block_number = FAT16_SECTOR(active_cluster) + base_sector;
        dev->read_block(dev, data.data, data.block_number, 2);
    }

    tv = FAT16_TABLE_VALUE(active_cluster, data.data);
    if (tv != 0xffff) {
        kprintf(ANSI_RED "Cluster[1] was not 0xffff (it was %#hx)!\n", tv);
        return false;
    }
    FAT16_DBG(ANSI_GREEN "\tCluster[1]=%#hx\n", tv);

    const uint32_t bytes_per_cluster = (fat2->fat.sectors_per_cluster * fat2->fat.bytes_per_sector);

    struct fat_filesystem *fs = slab_malloc(struct fat_filesystem);
    fs->bytes_per_cluster = bytes_per_cluster;
    fs->bytes_per_sector = fat2->fat.bytes_per_sector;
    fs->relative_first_data_sector = relative_first_data_sector;
    fs->super.type_name = "FAT16";
    fs->super.base_sector = base_sector;
    fs->super.device = dev;
    fs->super.read_file = fat16_read_file;
    add_filesystem(SUPER(*fs));

    FAT16_DBG("Each cluster is %u bytes. Data starts at %#p\n", bytes_per_cluster,
              (base_sector * SECTOR_SIZE) + relative_first_data_sector * fat2->fat.bytes_per_sector);

    FAT16_DBG("Root directory sectors: %u\n",
              fat2->fat.number_of_root_directory_entries * sizeof(struct directory) / SECTOR_SIZE);

    size_t this_drive = fat_no++;
    const struct fat_directory *d = (struct fat_directory *)data.data;
    char buffer[MAX_FILENAME_LENGTH] = {};
    // memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));

    // buffer[0] = '\0';
    for (int i = 0; i < fat2->fat.number_of_root_directory_entries; i++) {
        const uint32_t sector_i = i * sizeof(struct directory) / SECTOR_SIZE;
        const uint32_t physical_sector =
            (((relative_first_root_dir_sector + sector_i) * fat2->fat.bytes_per_sector) / SECTOR_SIZE + base_sector);
        if (data.block_number != physical_sector) {
            IO_DBG("Copying in sector %zu...\n", physical_sector);
            dev->read_block(dev, data.data, physical_sector, 1);
        }
        const struct fat_directory *this_entry = &d[i % (sizeof(data) / sizeof(struct fat_directory))];
        // printf("Marker: %#hhx\n", this_entry->marker);
        if (this_entry->marker == 0xe5) {
            // printf("-unused entry-\n");
            continue;
        }
        if (this_entry->marker == 0x00)
            break;
        if (this_entry->attributes == LFN) {
            const struct long_filename *lfname = (struct long_filename *)this_entry;

#ifdef FAT16_DEBUG
            FAT16_DBG(ANSI_MAGENTA "Before: [");
            printf(ANSI_MAGENTA);
            for (size_t i = 0; i < sizeof(buffer); i++) {
                if (buffer[i] == '\0')
                    printf("␀");
                else if (buffer[i] < ' ' || buffer[i] == 0x7f)
                    printf("\\%hhu", buffer[i]);
                else
                    putchar(buffer[i]);
            }
            printf("]\n" ANSI_RESET);
#endif

            if (buffer[0] != '\0') {
                for (size_t i = 0; i < (sizeof(buffer) - 13); i++)
                    buffer[(sizeof(buffer) - 1) - i] = buffer[(sizeof(buffer) - 1) - (i + 13)];
                buffer[sizeof(buffer) - 1] = '\0';
            }

#ifdef FAT16_DEBUG
            FAT16_DBG(ANSI_MAGENTA " Shift: [");
            printf(ANSI_MAGENTA);
            for (size_t i = 0; i < sizeof(buffer); i++) {
                if (buffer[i] == '\0')
                    printf("␀");
                else if (buffer[i] < ' ' || buffer[i] == 0x7f)
                    printf("\\%hhu", buffer[i]);
                else
                    putchar(buffer[i]);
            }
            printf("]\n" ANSI_RESET);
#endif

            char *c2 = buffer;
            for (char *c = (char *)&lfname->name;
                 c < (((char *)&lfname->name) + sizeof(lfname->name)) && *c != '\0' && *c != 0xff; c += 2, c2++)
                *c2 = *c;
            for (char *c = (char *)&lfname->name2;
                 c < (((char *)&lfname->name2) + sizeof(lfname->name2)) && *c != '\0' && *c != 0xff; c += 2, c2++)
                *c2 = *c;
            for (char *c = (char *)&lfname->name3;
                 c < (((char *)&lfname->name3) + sizeof(lfname->name3)) && *c != '\0' && *c != 0xff; c += 2, c2++)
                *c2 = *c;

#ifdef FAT16_DEBUG
            FAT16_DBG(ANSI_MAGENTA " After: [");
            printf(ANSI_MAGENTA);
            for (size_t i = 0; i < sizeof(buffer); i++) {
                if (buffer[i] == '\0')
                    printf("␀");
                else if (buffer[i] < ' ' || buffer[i] == 0x7f)
                    printf("\\x%hhu", buffer[i]);
                else
                    putchar(buffer[i]);
            }
            printf("]\n" ANSI_RESET);

            FAT16_DBG(ANSI_MAGENTA "Long filename: `%S`\n" ANSI_RESET, buffer);
#endif
            continue;
        }

        const_string fname = {.head = this_entry->name, .tail = this_entry->ext};
        const_string fext = {.head = this_entry->ext, .tail = this_entry->ext + sizeof(this_entry->ext)};
        char *end = strchr(fname, ' ');
        if (end != NULL)
            fname.tail = end;
        if (this_entry->attributes == VOLUME_ID) {
            const_string find = strstr(fname, CSTR(" "));
            const_string sname = {.head = fname.head,
                                  .tail = find.tail == NULL ? fname.tail : (fname.head + (find.tail - find.head))};

            find = strstr(fext, CSTR(" "));
            const_string sext = {.head = fext.head,
                                 .tail = find.tail == NULL ? fext.tail : (fext.head + (find.tail - find.head))};
            FAT16_DBG(ANSI_ORANGE "\nLABEL: `%s.%s` %c%S%c\n", sname, sext, buffer[0] == '\0' ? ' ' : '(', buffer,
                      buffer[0] == '\0' ? ' ' : ')');
            buffer[0] = '\0';
            continue;
        }

        FAT16_DBG(ANSI_CYAN "\nFile: `%s.%s` %c%S%c\n"
                            "\tAttributes: %#hhx\n"
                            "\tCreated: %u-%02hhu-%02hhu %02hhu:%02hhu:%02hhu.%03hhu\n"
                            "\tModified: %u-%02hhu-%02hhu %02hhu:%02hhu:%02hhu\n"
                            "\tAccessed: %u-%02hhu-%02hhu\n"
                            "\tCluster: H:%hu L:%hu\n"
                            "\tSize: %u\n",
                  fname, (const_string){.head = this_entry->ext, .tail = this_entry->ext + sizeof(this_entry->ext)},
                  buffer[0] == '\0' ? ' ' : '(', buffer, buffer[0] == '\0' ? ' ' : ')', this_entry->attributes,
                  1980 + this_entry->creation_date.year, this_entry->creation_date.month, this_entry->creation_date.day,
                  this_entry->creation_time.hours, this_entry->creation_time.minutes, this_entry->creation_time.seconds,
                  this_entry->creation_time_microseconds, 1980 + this_entry->last_mod_date.year,
                  this_entry->last_mod_date.month, this_entry->last_mod_date.day, this_entry->last_mod_time.hours,
                  this_entry->last_mod_time.minutes, this_entry->last_mod_time.seconds,
                  1980 + this_entry->last_accessed_date.year, this_entry->last_accessed_date.month,
                  this_entry->last_accessed_date.day, this_entry->cluster_high, this_entry->cluster_low,
                  this_entry->file_size);

        FAT16_DBG(ANSI_CYAN "\tFile should be %u clusters (last cluster is only %u bytes).\n",
                  this_entry->file_size / bytes_per_cluster + ((this_entry->file_size % bytes_per_cluster) ? 1 : 0),
                  rem);

        // const uint32_t start_cluster = ((uint32_t)this_entry->cluster_high << 16) | this_entry->cluster_low;
        // uint32_t last_cluster = start_cluster;
        // active_cluster = FAT12_TABLE_VALUE(start_cluster);
        // bool sequential = active_cluster == (last_cluster + 1);
        // size_t n = 1;
        // for (; active_cluster != file_end; n++, last_cluster = active_cluster, active_cluster =
        // FAT12_TABLE_VALUE(active_cluster))// {
        //     sequential &= active_cluster == (last_cluster + 1);
        //     // printf("%u ", active_cluster);
        // // }
        // printf(n == expected_clusters ? (ANSI_GREEN "Verified. ") : (ANSI_RED "Following the cluster chain, we found
        // %zu clusters intstead!"), n); printf(sequential ? (ANSI_CYAN "Clusters were sequential.\n") : (ANSI_ORANGE
        // "Clusters weren't sequential.\n"));

        // uint8_t *data = (uint8_t*)fat + (first_data_sector * fat->bytes_per_sector) + ((start_cluster - 2) *
        // bytes_per_cluster); kprintf("Data: %hhx %hhx %hhx %hhx\n", data[0], data[1], data[2], data[3]); if
        // (sequential)
        //     kprintf("Data runs from %#010x to %#010x.\n", (uint32_t)(first_data_sector * fat->bytes_per_sector) +
        //     ((start_cluster - 2) * bytes_per_cluster), (uint32_t)(first_data_sector * fat->bytes_per_sector) +
        //     ((start_cluster - 2) * bytes_per_cluster) + this_entry->file_size);

        struct fat_file *file = slab_malloc(struct fat_file);
        file->super.super.filesystem = SUPER(*fs);
        file->start_cluster = ((uint32_t)this_entry->cluster_high << 16) | this_entry->cluster_low;
        char (*fnameBuffer)[MAX_FILENAME_LENGTH] = (char (*)[MAX_FILENAME_LENGTH])_slab_malloc(MAX_FILENAME_LENGTH);

        if (buffer[0] == '\0') {
            const_string find = strstr(fname, CSTR(" "));
            const_string sname = {.head = fname.head,
                                  .tail = find.tail == NULL ? fname.tail : (fname.head + (find.tail - find.head))};

            find = strstr(fext, CSTR(" "));
            const_string sext = {.head = fext.head,
                                 .tail = find.tail == NULL ? fext.tail : (fext.head + (find.tail - find.head))};
            // snprintf(buffer, MAX_FILENAME_LENGTH, "%s.%s", this_drive, sname, sext);
            char *c = buffer;
            for (size_t i = 0; sname.head + i != sname.tail; i++, c++)
                *c = (sname.head[i] >= 'A' && sname.head[i] <= 'Z') ? sname.head[i] + ('a' - 'A') : sname.head[i];
            *(c++) = '.';
            for (size_t i = 0; sext.head + i != sext.tail; i++, c++)
                *c = (sext.head[i] >= 'A' && sext.head[i] <= 'Z') ? sext.head[i] + ('a' - 'A') : sext.head[i];
        }

        snprintf(*fnameBuffer, sizeof(*file->super.super.name), "fat%zu:/%S", this_drive, buffer);
        file->super.super.name = (char (*)[MAX_FILENAME_LENGTH])fnameBuffer;

        file->super.size = this_entry->file_size;
        add_fs_entry(SUPER(*SUPER(*file)));

        buffer[0] = '\0';
        // memset(buffer, 0, MAX_FILENAME_LENGTH);
    }

    return true;
}

bool fat_init(const struct block_device *dev, struct block *base) {
    const struct fat *fat = (struct fat *)base->data;
    FAT_DBG(ANSI_GREEN "\n[FAT] Found FAT-formatted disk at sector #%u.\n"
                       "\tVersion: `%s`.\n"
                       "\tBytes per sector: %hu\n"
                       "\tSectors per cluster: %hhu\n"
                       "\tReserved sectors: %hu\n"
                       "\tFAT count: %hhu\n"
                       "\tRoot directory entries: %hu\n"
                       "\tLogical sectors: %hu\n"
                       "\tMedia descriptor: %#hhx\n"
                       "\tSectors per FAT: %hu\n"
                       "\tSectors per track: %hu\n"
                       "\tHeads/sides: %hu\n"
                       "\tHidden sectors: %u\n"
                       "\tLarge sectors: %u\n" ANSI_RESET,
            base->block_number, (const_string){.head = fat->version, .tail = fat->version + sizeof(fat->version)},
            fat->bytes_per_sector, fat->sectors_per_cluster, fat->reserved_sectors, fat->number_of_fats,
            fat->number_of_root_directory_entries, fat->logical_sector_count, fat->media_descriptor,
            fat->sectors_per_fat, fat->sectors_per_track, fat->number_of_heads_or_sides, fat->number_of_hidden_sectors,
            fat->large_sector_count);

    const unsigned int num_root_dir_sectors =
        ((fat->number_of_root_directory_entries * 32) + (fat->bytes_per_sector - 1)) / fat->bytes_per_sector;

    const uint32_t logical_sector_count =
        fat->logical_sector_count == 0 ? fat->large_sector_count : fat->logical_sector_count;
    const unsigned int num_data_sectors =
        logical_sector_count -
        (fat->reserved_sectors + (fat->number_of_fats * fat->sectors_per_fat) + num_root_dir_sectors);

    const unsigned int num_clusters = num_data_sectors / fat->sectors_per_cluster;

    if (fat->bytes_per_sector == 0) {
        printf(ANSI_RED "Disk is exFAT, not FAT12!\n" ANSI_RESET);
        return true;
    } else if (num_clusters >= 65525) {
        printf(ANSI_RED "Disk is FAT32, not FAT12! (num_clusters=%u)\n" ANSI_RESET, num_clusters);
        return true;
    }

    const struct fat_12_16 *fat2 = (struct fat_12_16 *)fat;

#ifdef FAT_DEBUG
    const unsigned int relative_first_data_sector =
        fat->reserved_sectors + (fat->sectors_per_fat * fat->number_of_fats) + num_root_dir_sectors;
    FAT_DBG(ANSI_GREEN "\n[FAT12/16] FAT12/16-formatted disk.\n"
                       "\tRoot directory sectors: %u\n"
                       "\tFirst data sector: %u (relative), %u (absolute)\n"
                       "\tData sectors: %u\n"
                       "\tTotal clusters: %u\n"
                       "\tDrive number: %hhu\n"
                       "\tFlags: %#hhx (%#hhb)\n"
                       "\tSignature: %#hhx\n"
                       "\tSerial: `%lX`\n"
                       "\tLabel: `%s`\n"
                       "\tSystem identifier: `%s`\n"
                       "\tBootable signature: %#hx\n" ANSI_RESET,
            num_root_dir_sectors, relative_first_data_sector, relative_first_data_sector + base->block_number,
            num_data_sectors, num_clusters, fat2->drive_number, fat2->flags, fat2->flags, fat2->signature, fat2->serial,
            (const_string){.head = fat2->label, .tail = fat2->label + sizeof(fat2->label)},
            (const_string){.head = fat2->system_identifier,
                           .tail = fat2->system_identifier + sizeof(fat2->system_identifier)},
            fat2->bootable_signature);
#endif

    if (num_clusters < 4085) {
        printf(ANSI_GREEN "Disk is probably FAT12 (num_clusters=%u)\n" ANSI_RESET, num_clusters);
        return fat12_init(dev, base->block_number, fat2);
    } else if (num_clusters < 65525) {
        printf(ANSI_GREEN "Disk is FAT16! (num_clusters=%u)\n" ANSI_RESET, num_clusters);
        return fat16_init(dev, base->block_number, fat2);
    }

    // PANIC("notdone\n");

    // return true;
}
