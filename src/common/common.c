#include <common.h>
#include <stddef.h>

__attribute__((always_inline)) uint32_t be_to_le(uint32_t value) {
    return ((value >> 24) & 0xff) |      // move byte 3 to byte 0
           ((value << 8) & 0xff0000) |   // move byte 1 to byte 2
           ((value >> 8) & 0xff00) |     // move byte 2 to byte 1
           ((value << 24) & 0xff000000); // byte 0 to byte 3
}
