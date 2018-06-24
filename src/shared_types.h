#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <stddef.h>

// opaque types
struct lfs_work_item_t;

enum lfs_callback_result_t {
	LFS_DO_NOTHING,
	LFS_FREE_WORK_ITEM,
	LFS_FREE_WORK_ITEM_AND_BUFFER
};

// typedefs
typedef enum lfs_callback_result_t (*lfs_work_item_callback_t)(const struct lfs_work_item_t *, void *);

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
	LFS_GENERIC_ERROR,
	LFS_NOT_FOUND,
	LFS_UNSUPPORTED,
	LFS_ALREADY_EXISTS,
	LFS_PERMISSIONS_ERROR,
	LFS_OUT_OF_SPACE,
	LFS_INVALID_DEVICE
};

enum lfs_write_mode_t {
	LFS_WRITE_TRUNCATE,
	LFS_WRITE_APPEND,
	LFS_WRITE_SEGMENT
};

enum lfs_mount_permissions_t {
	LFS_MOUNT_DEFAULT = 0,
	LFS_MOUNT_READ = 1 << 0,
	LFS_MOUNT_WRITE_FILE = 1 << 1,
	LFS_MOUNT_DELETE_FILE = 1 << 2,
	LFS_MOUNT_CREATE_DIR = 1 << 3,
	LFS_MOUNT_DELETE_DIR = 1 << 4,
	LFS_MOUNT_WRITE = LFS_MOUNT_WRITE_FILE | LFS_MOUNT_DELETE_FILE | LFS_MOUNT_CREATE_DIR | LFS_MOUNT_DELETE_DIR,
	LFS_MOUNT_ALL_PERMISSIONS = LFS_MOUNT_READ | LFS_MOUNT_WRITE
};

// default allocator
#if __cplusplus
extern "C" {
	extern struct lfs_allocator_t lfs_default_allocator;
}
#else
extern struct lfs_allocator_t lfs_default_allocator;
#endif
