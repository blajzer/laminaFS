// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include "laminaFS.h"
#include "laminaFS_c.h"

using namespace laminaFS;

#define CTX(x) static_cast<FileContext*>(x._value)

lfs_file_context_t lfs_file_context_create() {
	return lfs_file_context_t{ new FileContext() };
}

void lfs_file_context_destroy(lfs_file_context_t ctx) {
	delete CTX(ctx);
}

int32_t lfs_register_device_interface(lfs_file_context_t ctx, lfs_device_interface_t *interface) {
	return CTX(ctx)->registerDeviceInterface(*reinterpret_cast<FileContext::DeviceInterface*>(interface));
}

void lfs_create_mount(lfs_file_context_t ctx, uint32_t deviceType, const char *mountPoint, const char *devicePath, bool virtualPath) {
	CTX(ctx)->createMount(deviceType, mountPoint, devicePath, virtualPath);
}

void lfs_set_log_func(lfs_file_context_t ctx, lfs_log_func_t func) {
	CTX(ctx)->setLogFunc(func);
}

lfs_log_func_t lfs_get_log_func(lfs_file_context_t ctx) {
	return CTX(ctx)->getLogFunc();
}
