#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#ifdef __cplusplus
#include <atomic>
#else
#include <atomic.h>
#endif

// opaque types
struct lfs_file_t;

// forward declarations
struct lfs_work_item_t;

// typedefs
typedef void (*lfs_work_item_callback_t)(lfs_work_item_t *);

enum lfs_file_mode_t {
	LFS_FM_READ,
	LFS_FM_WRITE,
	LFS_FM_READ_WRITE,
	LFS_FM_APPEND
};

enum lfs_error_code_t {
	LFS_OK = 0,
	LFS_NOT_FOUND,
	LFS_UNSUPPORTED,
};

enum lfs_file_operation_t {
	LFS_OP_EXISTS,
	LFS_OP_SIZE,
	LFS_OP_READ,
	LFS_OP_WRITE,
};

struct lfs_work_item_t {
	lfs_file_operation_t _operation;
	lfs_file_t *_file;
	lfs_work_item_callback_t _callback;
	void *_buffer;
	uint64_t _bufferBytes;
	uint64_t _offset;
	uint64_t _operationBytes;

	// TODO: sanity check this
#ifdef __cplusplus
	std::atomic<bool> _completed;
#else
	atomic_bool _completed;
#endif
};

