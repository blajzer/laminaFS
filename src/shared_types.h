#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <stddef.h>

// opaque types
struct lfs_work_item_t;

// typedefs
typedef void (*lfs_work_item_callback_t)(lfs_work_item_t *);

//! Memory allocation function. Params are userdata pointer, size in bytes, and alignment in bytes.
typedef void *(*lfs_mem_alloc_t)(void *, size_t, size_t);
//! Memory free function. Params are userdata pointer and pointer to memory to free
typedef void (*lfs_mem_free_t)(void *, void *);

struct lfs_allocator_t {
	lfs_mem_alloc_t alloc;
	lfs_mem_free_t free;
	void *allocator;
};

enum lfs_error_code_t {
	LFS_OK = 0,
	LFS_NOT_FOUND,
	LFS_UNSUPPORTED,
};

