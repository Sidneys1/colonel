/*
 * CRC.H - header file for SNIPPETS CRC and checksum functions
 * Copyright (C) 1986 Gary S. Brown.  You may use this program, or
 * code or tables extracted from it, as desired without restriction.
 *
 * Originally from `https://web.archive.org/web/20080303093609/http://c.snippets.org/snip_lister.php?fname=crc.h`
 * via `https://stackoverflow.com/a/303020`.
 */

#pragma once

#ifndef CRC__H
#define CRC__H

#include <stddef.h>

uint32_t updateCRC32(unsigned char ch, uint32_t crc);
uint32_t crc32buf(char *buf, size_t len);

#endif /* CRC__H */