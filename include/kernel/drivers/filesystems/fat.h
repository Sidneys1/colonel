#pragma once

#include <io.h>

bool fat_init(const struct block_device *dev, struct block *base);
