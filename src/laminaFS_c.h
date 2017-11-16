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
typedef void* lfs_mount_t;

// device function pointer types
typedef enum lfs_error_code_t (*lfs_device_create_func_t)(struct lfs_allocator_t *, const char *, void **);
typedef void (*lfs_device_destroy_func_t)(void*);
typedef bool (*lfs_device_file_exists_func_t)(void *, const char *);
typedef size_t (*lfs_device_file_size_func_t)(void *, const char *, enum lfs_error_code_t *);
typedef size_t (*lfs_device_read_file_func_t)(void *, const char *, struct lfs_allocator_t *, void **, bool, enum lfs_error_code_t *);
typedef size_t (*lfs_device_write_file_func_t)(void *, const char *, void *, size_t, bool, enum lfs_error_code_t *);
typedef enum lfs_error_code_t (*lfs_device_delete_file_func_t)(void *, const char *);
typedef enum lfs_error_code_t (*lfs_device_create_dir_func_t)(void *, const char *);
typedef enum lfs_error_code_t (*lfs_device_delete_dir_func_t)(void *, const char *);

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
	lfs_device_create_dir_func_t _createDir;
	lfs_device_delete_dir_func_t _deleteDir;
};

// FileContext functions

//! Creates a file context.
//! @param allocator the allocator interface to use
//! @return the context
LFS_C_API lfs_context_t lfs_context_create(struct lfs_allocator_t *allocator);

//! Creates a file context and specify ringbuffer and pool capacities.
//! @param allocator the allocator interface to use
//! @param maxQueuedWorkItems the size of the queue ringbuffer
//! @param workItemPoolSize the maximum number of work items
//! @return the context
LFS_C_API lfs_context_t lfs_context_create_capacity(struct lfs_allocator_t *allocator, uint64_t maxQueuedWorkItems, uint64_t workItemPoolSize);

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
//! @param returnCode the return code
//! @return the mount
LFS_C_API lfs_mount_t lfs_create_mount(lfs_context_t ctx, uint32_t deviceType, const char *mountPoint, const char *devicePath, enum lfs_error_code_t *returnCode);

//! Removes a mount from a context.
//! Finishes all processing work items and suspends processing while it runs.
//! @param ctx the context
//! @param mount the mount to remove
//! @return whether or not the mount was found and removed
LFS_C_API bool lfs_release_mount(lfs_context_t ctx, lfs_mount_t mount);

//! Reads the entirety of a file.
//! @param ctx the context
//! @param filepath the path to the file to read
//! @param nullTerminate whether or not to null-terminate the input so it can be directly used as a C-string
//! @param alloc the allocator to use.
//! @param callback optional callback
//! @return a lfs_work_item_t representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_read_file(lfs_context_t ctx, const char *filepath, bool nullTerminate, struct lfs_allocator_t *alloc, lfs_work_item_callback_t callback);

//! Reads the entirety of a file and uses the context's allocator.
//! @param ctx the context
//! @param filepath the path to the file to read
//! @param nullTerminate whether or not to null-terminate the input so it can be directly used as a C-string
//! @param callback optional callback
//! @return a lfs_work_item_t representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_read_file_ctx_alloc(lfs_context_t ctx, const char *filepath, bool nullTerminate, lfs_work_item_callback_t callback);

//! Writes a buffer to a file.
//! @param ctx the context
//! @param filepath the path to the file to write
//! @param buffer the buffer to write
//! @param bufferBytes the number of bytes to write to the buffer
//! @param callback optional callback
//! @return a lfs_work_item_t representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_write_file(lfs_context_t ctx, const char *filepath, void *buffer, uint64_t bufferBytes, lfs_work_item_callback_t callback);

//! Appends a buffer to a file.
//! @param ctx the context
//! @param filepath the path to the file to append
//! @param buffer the buffer to write
//! @param bufferBytes the number of bytes to write to the buffer
//! @param callback optional callback
//! @return a WorkItem representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_append_file(lfs_context_t ctx, const char *filepath, void *buffer, uint64_t bufferBytes, lfs_work_item_callback_t callback);

//! Determines if a file exists.
//! @param ctx the context
//! @param filepath the path to the file to delete
//! @return a WorkItem representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_file_exists(lfs_context_t ctx, const char *filepath, lfs_work_item_callback_t callback);

//! Gets the size of a file.
//! @param ctx the context
//! @param filepath the path to the file to delete
//! @param callback optional callback
//! @return a WorkItem representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_file_size(lfs_context_t ctx, const char *filepath, lfs_work_item_callback_t callback);

//! Deletes a file.
//! @param ctx the context
//! @param filepath the path to the file to delete
//! @param callback optional callback
//! @return a WorkItem representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_delete_file(lfs_context_t ctx, const char *filepath, lfs_work_item_callback_t callback);

//! Creates a directory.
//! @param ctx the context
//! @param path the path to the directory to create
//! @param callback optional callback
//! @return a WorkItem representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_create_dir(lfs_context_t ctx, const char *path, lfs_work_item_callback_t callback);

//! Deletes a directory and all contained files/directories.
//! @param ctx the context
//! @param path the path to the file to delete
//! @param callback optional callback
//! @return a WorkItem representing the work to be done
LFS_C_API struct lfs_work_item_t *lfs_delete_dir(lfs_context_t ctx, const char *path, lfs_work_item_callback_t callback);

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

//! Whether or not a work item has completed processing.
//! @param workItem the WorkItem to query
LFS_C_API bool lfs_work_item_completed(struct lfs_work_item_t *workItem);

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
