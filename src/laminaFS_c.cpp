// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include "laminaFS.h"
#include "laminaFS_c.h"

using namespace laminaFS;

#define CTX(x) static_cast<FileContext*>(x._value)

lfs_file_context_t lfs_file_context_create(lfs_allocator_t *allocator) {
	void *mem = allocator->alloc(allocator->allocator, sizeof(FileContext), alignof(FileContext));
	return lfs_file_context_t{ new(mem) FileContext(*allocator) };
}

void lfs_file_context_destroy(lfs_file_context_t ctx) {
	lfs_allocator_t alloc = CTX(ctx)->getAllocator();
	CTX(ctx)->~FileContext();
	alloc.free(alloc.allocator, CTX(ctx));
}

int32_t lfs_register_device_interface(lfs_file_context_t ctx, lfs_device_interface_t *interface) {
	return CTX(ctx)->registerDeviceInterface(*reinterpret_cast<FileContext::DeviceInterface*>(interface));
}

lfs_error_code_t lfs_create_mount(lfs_file_context_t ctx, uint32_t deviceType, const char *mountPoint, const char *devicePath) {
	return CTX(ctx)->createMount(deviceType, mountPoint, devicePath);
}

void lfs_set_log_func(lfs_file_context_t ctx, lfs_log_func_t func) {
	CTX(ctx)->setLogFunc(func);
}

lfs_log_func_t lfs_get_log_func(lfs_file_context_t ctx) {
	return CTX(ctx)->getLogFunc();
}
