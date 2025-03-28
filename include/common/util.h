#pragma once

#define ASSERT_STMT(x, m, y) (sizeof(struct { _Static_assert(x, m); }), y)
