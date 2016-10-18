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

void lfs_context_destroy(lfs_context_t ctx) {
	lfs_allocator_t alloc = CTX(ctx)->getAllocator();
	CTX(ctx)->~FileContext();
	alloc.free(alloc.allocator, CTX(ctx));
}

int32_t lfs_register_device_interface(lfs_context_t ctx, lfs_device_interface_t *interface) {
	return CTX(ctx)->registerDeviceInterface(*reinterpret_cast<FileContext::DeviceInterface*>(interface));
}

lfs_error_code_t lfs_create_mount(lfs_context_t ctx, uint32_t deviceType, const char *mountPoint, const char *devicePath) {
	return CTX(ctx)->createMount(deviceType, mountPoint, devicePath);
}

lfs_work_item_t *lfs_read_file(lfs_context_t ctx, const char *filepath, lfs_allocator_t *alloc) {
	return CTX(ctx)->readFile(filepath, alloc);
}

lfs_work_item_t *lfs_read_file_ctx_alloc(lfs_context_t ctx, const char *filepath) {
	return CTX(ctx)->readFile(filepath);
}

lfs_work_item_t *lfs_write_file(lfs_context_t ctx, const char *filepath, void *buffer, uint64_t bufferBytes) {
	return CTX(ctx)->writeFile(filepath, buffer, bufferBytes);
}

lfs_work_item_t *lfs_append_file(lfs_context_t ctx, const char *filepath, void *buffer, uint64_t bufferBytes) {
	return CTX(ctx)->appendFile(filepath, buffer, bufferBytes);
}

lfs_work_item_t *lfs_file_exists(lfs_context_t ctx, const char *filepath) {
	return CTX(ctx)->fileExists(filepath);
}

lfs_work_item_t *lfs_file_size(lfs_context_t ctx, const char *filepath) {
	return CTX(ctx)->fileSize(filepath);
}

lfs_work_item_t *lfs_delete_file(lfs_context_t ctx, const char *filepath) {
	return CTX(ctx)->deleteFile(filepath);
}

lfs_error_code_t lfs_work_item_get_result(lfs_work_item_t *workItem) {
	return WorkItemGetResult(workItem);
}

void *lfs_work_item_get_buffer(lfs_work_item_t *workItem) {
	return WorkItemGetBuffer(workItem);
}

uint64_t lfs_work_item_get_bytes(lfs_work_item_t *workItem) {
	return WorkItemGetBytes(workItem);
}

void lfs_work_item_free_buffer(lfs_work_item_t *workItem) {
	WorkItemFreeBuffer(workItem);
}

void lfs_wait_for_work_item(lfs_work_item_t *workItem) {
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
