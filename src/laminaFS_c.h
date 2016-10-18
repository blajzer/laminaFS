#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "shared_types.h"

#ifdef __cplusplus
#define LFS_C_API extern "C"
#else
#define LFS_C_API 
#endif

// typedefs
typedef struct lfs_file_context_s { void *_value; } lfs_context_t;
typedef void* lfs_file_handle_t;
typedef int (*lfs_log_func_t)(const char *, ...);

// device function pointer types
typedef enum lfs_error_code_t (*lfs_device_create_func_t)(struct lfs_allocator_t *, const char *, void **);
typedef void (*lfs_device_destroy_func_t)(void*);
typedef bool (*lfs_device_file_exists_func_t)(void *, const char *);
typedef size_t (*lfs_device_file_size_func_t)(void *, const char *);
typedef size_t (*lfs_device_read_file_func_t)(void *, const char *, struct lfs_allocator_t *, void **);
typedef size_t (*lfs_device_write_file_func_t)(void *, const char *, void *, size_t, bool);
typedef enum lfs_error_code_t (*lfs_device_delete_file_func_t)(void *, const char *);

// structs
struct lfs_device_interface_t {
	// required
	lfs_device_create_func_t _create;
	lfs_device_destroy_func_t _destroy;

	lfs_device_file_exists_func_t _fileExists;
	lfs_device_file_size_func_t _fileSize;
	lfs_device_read_file_func_t _readFile;

	// optional
	lfs_device_write_file_func_t _writeFile;
	lfs_device_delete_file_func_t _deleteFile;
};

// FileContext functions

//! Creates a file context.
//! @return the context
LFS_C_API lfs_context_t lfs_context_create(struct lfs_allocator_t *allocator);

//! Destroys a file context
//! @param ctx the context to destroy
LFS_C_API void lfs_context_destroy(lfs_context_t ctx);

//! Registers a device interface with a context
//! @param ctx the context
//! @param interface the device interface to register
LFS_C_API int32_t lfs_register_device_interface(lfs_context_t ctx, struct lfs_device_interface_t *interface);

//! Creates a mount on a context
//! @param ctx the context
//! @param deviceType the type index of the device to create the mount with
//! @param mountPoint the virtual path to mount this device to
//! @param devicePath the path to pass into the device
LFS_C_API enum lfs_error_code_t lfs_create_mount(lfs_context_t ctx, uint32_t deviceType, const char *mountPoint, const char *devicePath);

//! Reads the entirety of a file.
//! @param ctx the context
//! @param filepath the path to the file to read
//! @param alloc the allocator to use.
//! @return a lfs_work_item_t representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_read_file(lfs_context_t ctx, const char *filepath, struct lfs_allocator_t *alloc);

//! Reads the entirety of a file and uses the context's allocator.
//! @param ctx the context
//! @param filepath the path to the file to read
//! @return a lfs_work_item_t representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_read_file_ctx_alloc(lfs_context_t ctx, const char *filepath);

//! Writes a buffer to a file.
//! @param ctx the context
//! @param filepath the path to the file to write
//! @param buffer the buffer to write
//! @param bufferBytes the number of bytes to write to the buffer
//! @return a lfs_work_item_t representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_write_file(lfs_context_t ctx, const char *filepath, void *buffer, uint64_t bufferBytes);

//! Appends a buffer to a file.
//! @param ctx the context
//! @param filepath the path to the file to append
//! @param buffer the buffer to write
//! @param bufferBytes the number of bytes to write to the buffer
//! @return a WorkItem representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_append_file(lfs_context_t ctx, const char *filepath, void *buffer, uint64_t bufferBytes);
	
//! Determines if a file exists.
//! @param ctx the context
//! @param filepath the path to the file to delete
//! @return a WorkItem representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_file_exists(lfs_context_t ctx, const char *filepath);

//! Gets the size of a file.
//! @param ctx the context
//! @param filepath the path to the file to delete
//! @return a WorkItem representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_file_size(lfs_context_t ctx, const char *filepath);

//! Deletes a file.
//! @param ctx the context
//! @param filepath the path to the file to delete
//! @return a WorkItem representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_delete_file(lfs_context_t ctx, const char *filepath);

//! Gets the result code from a WorkItem
//! @param workItem the WorkItem
//! @return the result code
LFS_C_API enum lfs_error_code_t lfs_work_item_get_result(struct lfs_work_item_t *workItem);

//! Gets the output buffer from a WorkItem.
//! @param workItem the WorkItem
//! @return the output buffer
LFS_C_API void *lfs_work_item_get_buffer(struct lfs_work_item_t *workItem);

//! Gets the bytes read/written from a WorkItem.
//! @param workItem the WorkItem
//! @return the bytes read/written
LFS_C_API uint64_t lfs_work_item_get_bytes(struct lfs_work_item_t *workItem);

//! Frees the output buffer that was allocated by the work item.
//! @param workItem the WorkItem
LFS_C_API void lfs_work_item_free_buffer(struct lfs_work_item_t *workItem);

//! Waits for a WorkItem to finish processing.
//! @param workItem the WorkItem to wait for
LFS_C_API void lfs_wait_for_work_item(struct lfs_work_item_t *workItem);

//! Releases a WorkItem.
//! @param ctx the context
//! @param workItem the WorkItem to release.
LFS_C_API void lfs_release_work_item(lfs_context_t ctx, struct lfs_work_item_t *workItem);

//! Sets the log function.
//! @param ctx the context
//! @param func the logging function
LFS_C_API void lfs_set_log_func(lfs_context_t ctx, lfs_log_func_t func);

//! Gets the log function.
//! @param ctx the context
//! @return the logging function
LFS_C_API lfs_log_func_t lfs_get_log_func(lfs_context_t ctx);
