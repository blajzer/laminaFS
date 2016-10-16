// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.
#include "shared_types.h"

void lfs_work_item_t::configExists(const char *filename, lfs_work_item_callback_t callback) {
	_operation = LFS_OP_EXISTS;
	_file = nullptr;
	_callback = callback;
	_data.filename = filename;
	_bufferBytes = 0;
	_offset = 0;
	_operationBytes = 0;
	_result.exists = false;
}

void lfs_work_item_t::configSize(lfs_file_t *file, lfs_work_item_callback_t callback) {
	_operation = LFS_OP_SIZE;
	_file = file;
	_callback = callback;
	_data.buffer = nullptr;
	_bufferBytes = 0;
	_offset = 0;
	_operationBytes = 0;
	_result.bytes = 0;
}

void lfs_work_item_t::configRead(lfs_file_t *file, void *buffer, uint64_t bufferBytes, uint64_t readOffset, uint64_t readBytes, lfs_work_item_callback_t callback) {
	_operation = LFS_OP_READ;
	_file = file;
	_callback = callback;
	_data.buffer = buffer;
	_bufferBytes = bufferBytes;
	_offset = readOffset;
	_operationBytes = readBytes;
	_result.bytes = 0;
}

void lfs_work_item_t::configWrite(lfs_file_t *file, void*buffer, uint64_t bufferBytes, uint64_t writeOffset, uint64_t writeBytes, lfs_work_item_callback_t callback) {
	_operation = LFS_OP_WRITE;
	_file = file;
	_callback = callback;
	_data.buffer = buffer;
	_bufferBytes = bufferBytes;
	_offset = writeOffset;
	_operationBytes = writeBytes;
	_result.bytes = 0;
}

void lfs_work_item_t::configDelete(const char *filename, lfs_work_item_callback_t callback) {
	_operation = LFS_OP_DELETE;
	_file = nullptr;
	_callback = callback;
	_data.filename = filename;
	_bufferBytes = 0;
	_offset = 0;
	_operationBytes = 0;
	_result.code = LFS_NOT_FOUND;
}

