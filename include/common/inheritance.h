#pragma once

#define INHERITS(x) x super
#define typeid(x) _Generic(x, default: static_assert(0, "No typeid implemented for `" #x "`."), struct device : 0, struct block_device : 1, struct virtio_block_device : 2)

#define IS_SUBCLASS(x, y) static_assert((TEST((x).super)) == (TEST(y)), "`" #x "` is not a subclass of `" #y "` ((" #x ").super != " #y ")!")
#define SUPER(x) (typeof((x).super)*)(&(x).super)
