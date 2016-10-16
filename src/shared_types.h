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
	LFS_OP_DELETE,
};

//! Struct that represents a file request.
//! It is recommended to use the configuration functions instead of attempting
//! to manually configure the various options as they're guaranteed to produce
//! a valid work item.
//! 
//! The result value is in the _result union. Which member to use is determined
//! by which operation one is doing.
//! TODO: insert a table here.
//!
//! The _completed member can be used to check whether or not a work item has
//! completed yet without explicitly blocking until it finishes.
struct lfs_work_item_t {
#ifdef __cplusplus
	void configExists(const char *filename, lfs_work_item_callback_t callback = nullptr);
	void configSize(lfs_file_t *file, lfs_work_item_callback_t callback = nullptr);
	void configRead(lfs_file_t *file, void *buffer, uint64_t bufferBytes, uint64_t readOffset, uint64_t readBytes, lfs_work_item_callback_t callback = nullptr);
	void configWrite(lfs_file_t *file, void*buffer, uint64_t bufferBytes, uint64_t writeOffset, uint64_t writeBytes, lfs_work_item_callback_t callback = nullptr);
	void configDelete(const char *filename, lfs_work_item_callback_t callback = nullptr);
#endif

	lfs_file_operation_t _operation;
	lfs_file_t *_file;
	lfs_work_item_callback_t _callback;
	
	union {
		const char *filename;
		void *buffer;
	} _data;
	
	uint64_t _bufferBytes;
	uint64_t _offset;
	uint64_t _operationBytes;
	
	union {
		uint64_t bytes;
		lfs_error_code_t code;
		bool exists;
	} _result;

	// TODO: sanity check this
#ifdef __cplusplus
	std::atomic<bool> _completed;
#else
	atomic_bool _completed;
#endif
};

