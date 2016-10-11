// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include "laminaFS.h"
#include "laminaFS_c.h"

using namespace laminaFS;

lfs_file_context_t lfs_file_context_create() {
	return lfs_file_context_t{ new FileContext() };
}

void lfs_file_context_destroy(lfs_file_context_t ctx) {
	delete static_cast<FileContext*>(ctx._value);
}

int32_t lfs_register_device_interface(lfs_file_context_t ctx, lfs_device_interface_t *interface) {
	return static_cast<FileContext*>(ctx._value)->registerDeviceInterface(*reinterpret_cast<FileContext::DeviceInterface*>(interface));
}
