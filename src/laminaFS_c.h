#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <stddef.h>
#include <stdint.h>

// typedefs
struct lfs_file_context_t { void *_value; };
typedef void* lfs_file_handle_t;

// structs
struct lfs_device_interface_t {
	typedef void *(*create_func_t)();
	typedef void (*destroy_func_t)(void*);

	typedef lfs_file_handle_t (*open_file_func_t)(void *, const char *, uint32_t);
	typedef void (*close_file_func_t)(void *, lfs_file_handle_t);
	typedef bool (*file_exists_func_t)(void *, const char *);
	typedef size_t (*file_size_func_t)(void *, lfs_file_handle_t);
	typedef size_t (*read_file_func_t)(void *, lfs_file_handle_t, size_t, uint8_t *, size_t);
	typedef size_t (*write_file_func_t)(void *, lfs_file_handle_t, uint8_t *, size_t);
	typedef int (*delete_file_func_t)(void *, const char *);

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

// 

