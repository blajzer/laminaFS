// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include "Directory.h"

using namespace laminaFS;

DirectoryDevice::DirectoryDevice() {
}

DirectoryDevice::~DirectoryDevice() {
}

void *DirectoryDevice::create(const char *path, bool virtualPath) {
	return new DirectoryDevice();
}

void DirectoryDevice::destroy(void *device) {
	delete static_cast<DirectoryDevice *>(device);
}

ErrorCode DirectoryDevice::openFile(void *device, const char *filePath, FileMode *fileMode, FileHandle *file) {
	return LFS_NOT_FOUND;
}

void DirectoryDevice::closeFile(void *device, FileHandle file) {
}

bool DirectoryDevice::fileExists(void *device, const char *filePath) {
	return false;
}

size_t DirectoryDevice::fileSize(void *device, FileHandle file) {
	return 0;
}

size_t DirectoryDevice::readFile(void *device, FileHandle file, size_t offset, uint8_t *buffer, size_t bytesToRead) {
	return 0;
}

size_t DirectoryDevice::writeFile(void *device, FileHandle file, uint8_t *buffer, size_t bytesToRead) {
	return 0;
}

ErrorCode DirectoryDevice::deleteFile(void *device, const char *filePath) {
	return LFS_NOT_FOUND;
}

