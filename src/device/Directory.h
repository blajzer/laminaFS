#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <cstddef>
#include <cstdint>

#include "FileContext.h"

namespace laminaFS {

class DirectoryDevice {
public:
	DirectoryDevice() = delete;
	DirectoryDevice(Allocator *allocator, const char *path);
	~DirectoryDevice();

	static ErrorCode create(Allocator *allocator, const char *path, void **device);
	static void destroy(void *device);
	
	static bool fileExists(void *device, const char *filePath);
	static size_t fileSize(void *device, const char *filePath);
	static size_t readFile(void *device, const char *filePath, lfs_allocator_t *, void **buffer);

	static size_t writeFile(void *device, const char *filePath, void *buffer, size_t bytesToWrite, bool append);
	static ErrorCode deleteFile(void *device, const char *filePath);

private:
	FILE *openFile(const char *filePath, const char *modeString);
	char *getDevicePath(const char *filePath);
	void freeDevicePath(char *path);

	Allocator *_alloc;
	char *_devicePath = nullptr;
	uint32_t _pathLen = 0;
};

}
