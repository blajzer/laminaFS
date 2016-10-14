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
	DirectoryDevice(const char *path);
	~DirectoryDevice();

	static ErrorCode create(const char *path, void **device);
	static void destroy(void *device);

	static ErrorCode openFile(void *device, const char *filePath, FileMode *fileMode, FileHandle *file);
	static void closeFile(void *device, FileHandle file);
	static bool fileExists(void *device, const char *filePath);
	static size_t fileSize(void *device, FileHandle file);
	static size_t readFile(void *device, FileHandle file, size_t offset, uint8_t *buffer, size_t bytesToRead);

	static size_t writeFile(void *device, FileHandle file, uint8_t *buffer, size_t bytesToRead);
	static ErrorCode deleteFile(void *device, const char *filePath);

private:
	char *_devicePath = nullptr;
	uint32_t _pathLen = 0;
};

}
