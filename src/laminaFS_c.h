#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <stddef.h>
#include <stdint.h>

#include "shared_types.h"

// typedefs
struct lfs_file_context_t { void *_value; };
typedef void* lfs_file_handle_t;
typedef int (*lfs_log_func_t)(const char *, ...);

// structs
struct lfs_file_t;

struct lfs_device_interface_t {
	typedef lfs_error_code_t (*create_func_t)(const char *, bool, void **);
	typedef void (*destroy_func_t)(void*);

	typedef lfs_error_code_t (*open_file_func_t)(void *, const char *, lfs_file_mode_t *, lfs_file_handle_t *);
	typedef void (*close_file_func_t)(void *, lfs_file_handle_t);
	typedef bool (*file_exists_func_t)(void *, const char *);
	typedef size_t (*file_size_func_t)(void *, lfs_file_handle_t);
	typedef size_t (*read_file_func_t)(void *, lfs_file_handle_t, size_t, uint8_t *, size_t);
	typedef size_t (*write_file_func_t)(void *, lfs_file_handle_t, uint8_t *, size_t);
	typedef lfs_error_code_t (*delete_file_func_t)(void *, const char *);

	// required
	create_func_t _create;
	destroy_func_t _destroy;
	
	open_file_func_t _openFile;
	close_file_func_t _closeFile;
	file_exists_func_t _fileExists;
	file_size_func_t _fileSize;
	read_file_func_t _readFile;
	
	// optional
	write_file_func_t _writeFile;
	delete_file_func_t _deleteFile;
};

// FileContext functions

//! Creates a file context.
//! @return the context
extern "C" lfs_file_context_t lfs_file_context_create();

//! Destroys a file context
//! @param ctx the context to destroy
extern "C" void lfs_file_context_destroy(lfs_file_context_t ctx);

//! Registers a device interface with a context
//! @param ctx the context
//! @param interface the device interface to register
extern "C" int32_t lfs_register_device_interface(lfs_file_context_t ctx, lfs_device_interface_t *interface);

//! Creates a mount on a context
//! @param ctx the context
//! @param deviceType the type index of the device to create the mount with
//! @param mountPoint the virtual path to mount this device to
//! @param devicePath the path to pass into the device
//! @param virtualPath whether or not the devicePath refers to a virtual path or "real" path
extern "C" lfs_error_code_t lfs_create_mount(lfs_file_context_t ctx, uint32_t deviceType, const char *mountPoint, const char *devicePath, bool virtualPath);

//! Sets the log function.
//! @param ctx the context
//! @param func the logging function
extern "C" void lfs_set_log_func(lfs_file_context_t ctx, lfs_log_func_t func);

//! Gets the log function.
//! @param ctx the context
//! @return the logging function
extern "C" lfs_log_func_t lfs_get_log_func(lfs_file_context_t ctx);

