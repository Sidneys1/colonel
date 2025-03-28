#pragma once

#define INHERITS(x) x super

#define IS_SUBCLASS(x, y)                                                                                              \
    static_assert(__builtin_types_compatible_p(typeof(((x *)0)->super), y),                                            \
                  "`" #x "` is not a subclass of `" #y "` ((" #x ").super != " #y ")!")

#define SUPER(x) (typeof((x).super) *)(&(x).super)

#define SUB(type, x) ((void)sizeof(struct { IS_SUBCLASS(type, typeof(x)); }), (type *)(&x))