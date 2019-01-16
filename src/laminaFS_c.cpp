// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include "laminaFS.h"
#include "laminaFS_c.h"

using namespace laminaFS;

#define CTX(x) static_cast<FileContext*>(x._value)

lfs_context_t lfs_context_create(lfs_allocator_t *allocator) {
	void *mem = allocator->alloc(allocator->allocator, sizeof(FileContext), alignof(FileContext));
	return lfs_context_t{ new(mem) FileContext(*allocator) };
}

lfs_context_t lfs_context_create_capacity(lfs_allocator_t *allocator, uint64_t maxQueuedWorkItems, uint64_t workItemPoolSize) {
	void *mem = allocator->alloc(allocator->allocator, sizeof(FileContext), alignof(FileContext));
	return lfs_context_t{ new(mem) FileContext(*allocator, maxQueuedWorkItems, workItemPoolSize) };
}

void lfs_context_destroy(lfs_context_t ctx) {
	lfs_allocator_t alloc = CTX(ctx)->getAllocator();
	CTX(ctx)->~FileContext();
	alloc.free(alloc.allocator, CTX(ctx));
}

int32_t lfs_register_device_interface(lfs_context_t ctx, lfs_device_interface_t *interface) {
	return CTX(ctx)->registerDeviceInterface(*reinterpret_cast<FileContext::DeviceInterface*>(interface));
}

lfs_mount_t lfs_create_mount(lfs_context_t ctx, uint32_t deviceType, const char *mountPoint, const char *devicePath, lfs_error_code_t *returnCode) {
	return CTX(ctx)->createMount(deviceType, mountPoint, devicePath, *returnCode);
}

lfs_mount_t lfs_create_mount_with_permissions(lfs_context_t ctx, uint32_t deviceType, const char *mountPoint, const char *devicePath, enum lfs_error_code_t *returnCode, uint32_t permissions) {
	return CTX(ctx)->createMount(deviceType, mountPoint, devicePath, *returnCode, permissions);
}

bool lfs_release_mount(lfs_context_t ctx, lfs_mount_t mount) {
	return CTX(ctx)->releaseMount(mount);
}

lfs_work_item_t *lfs_read_file(lfs_context_t ctx, const char *filepath, bool nullTerminate, lfs_allocator_t *alloc, lfs_work_item_callback_t callback, void *callbackUserData) {
	return CTX(ctx)->readFile(filepath, nullTerminate, alloc, callback, callbackUserData);
}

lfs_work_item_t *lfs_read_file_ctx_alloc(lfs_context_t ctx, const char *filepath, bool nullTerminate, lfs_work_item_callback_t callback, void *callbackUserData) {
	return CTX(ctx)->readFile(filepath, nullTerminate, nullptr, callback, callbackUserData);
}

lfs_work_item_t *lfs_read_file_segment(lfs_context_t ctx, const char *filepath, uint64_t offset, uint64_t maxBytes, bool nullTerminate, struct lfs_allocator_t *alloc, lfs_work_item_callback_t callback, void *callbackUserData) {
	return CTX(ctx)->readFileSegment(filepath, offset, maxBytes, nullTerminate, alloc, callback, callbackUserData);
}

lfs_work_item_t *lfs_read_file_segment_ctx_alloc(lfs_context_t ctx, const char *filepath, uint64_t offset, uint64_t maxBytes, bool nullTerminate, lfs_work_item_callback_t callback, void *callbackUserData) {
	return CTX(ctx)->readFileSegment(filepath, offset, maxBytes, nullTerminate, nullptr, callback, callbackUserData);
}

lfs_work_item_t *lfs_write_file(lfs_context_t ctx, const char *filepath, const void *buffer, uint64_t bufferBytes, lfs_work_item_callback_t callback, void *callbackUserData) {
	return CTX(ctx)->writeFile(filepath, buffer, bufferBytes, callback, callbackUserData);
}

lfs_work_item_t *lfs_write_file_segment(lfs_context_t ctx, const char *filepath, uint64_t offset, const void *buffer, uint64_t bufferBytes, lfs_work_item_callback_t callback, void *callbackUserData) {
	return CTX(ctx)->writeFileSegment(filepath, offset, buffer, bufferBytes, callback, callbackUserData);
}

lfs_work_item_t *lfs_append_file(lfs_context_t ctx, const char *filepath, const void *buffer, uint64_t bufferBytes, lfs_work_item_callback_t callback, void *callbackUserData) {
	return CTX(ctx)->appendFile(filepath, buffer, bufferBytes, callback, callbackUserData);
}

lfs_work_item_t *lfs_file_exists(lfs_context_t ctx, const char *filepath, lfs_work_item_callback_t callback, void *callbackUserData) {
	return CTX(ctx)->fileExists(filepath, callback, callbackUserData);
}

lfs_work_item_t *lfs_file_size(lfs_context_t ctx, const char *filepath, lfs_work_item_callback_t callback, void *callbackUserData) {
	return CTX(ctx)->fileSize(filepath, callback, callbackUserData);
}

lfs_work_item_t *lfs_delete_file(lfs_context_t ctx, const char *filepath, lfs_work_item_callback_t callback, void *callbackUserData) {
	return CTX(ctx)->deleteFile(filepath, callback, callbackUserData);
}

lfs_work_item_t *lfs_create_dir(lfs_context_t ctx, const char *path, lfs_work_item_callback_t callback, void *callbackUserData) {
	return CTX(ctx)->createDir(path, callback, callbackUserData);
}

lfs_work_item_t *lfs_delete_dir(lfs_context_t ctx, const char *path, lfs_work_item_callback_t callback, void *callbackUserData) {
	return CTX(ctx)->deleteDir(path, callback, callbackUserData);
}

lfs_error_code_t lfs_work_item_get_result(const lfs_work_item_t *workItem) {
	return WorkItemGetResult(workItem);
}

void *lfs_work_item_get_buffer(const lfs_work_item_t *workItem) {
	return WorkItemGetBuffer(workItem);
}

uint64_t lfs_work_item_get_bytes(const lfs_work_item_t *workItem) {
	return WorkItemGetBytes(workItem);
}

void lfs_work_item_free_buffer(lfs_work_item_t *workItem) {
	WorkItemFreeBuffer(workItem);
}

bool lfs_work_item_completed(const lfs_work_item_t *workItem) {
	return WorkItemCompleted(workItem);
}

void lfs_wait_for_work_item(const lfs_work_item_t *workItem) {
	WaitForWorkItem(workItem);
}

void lfs_release_work_item(lfs_context_t ctx, lfs_work_item_t *workItem) {
	CTX(ctx)->releaseWorkItem(workItem);
}

void lfs_set_log_func(lfs_context_t ctx, lfs_log_func_t func) {
	CTX(ctx)->setLogFunc(func);
}

lfs_log_func_t lfs_get_log_func(lfs_context_t ctx) {
	return CTX(ctx)->getLogFunc();
}
