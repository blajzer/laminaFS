// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include "Directory.h"

#include <cstdio>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace laminaFS;

DirectoryDevice::DirectoryDevice(Allocator *allocator, const char *path) {
	_alloc = allocator;

	// TODO: realpath()
	_pathLen = strlen(path);
	_devicePath = reinterpret_cast<char*>(_alloc->alloc(_alloc->allocator, sizeof(char) * (_pathLen + 1), alignof(char)));
	strcpy(_devicePath, path);
}

DirectoryDevice::~DirectoryDevice() {
	_alloc->free(_alloc->allocator, _devicePath);
}

ErrorCode DirectoryDevice::create(Allocator *alloc, const char *path, void **device) {
#ifdef _WIN32
	struct _stat statInfo;
	int result = _stat(path, &statInfo);
#else
	struct stat statInfo;
	int result = stat(path, &statInfo);
#endif

	ErrorCode returnCode = LFS_OK;

	if (result == 0 && S_ISDIR(statInfo.st_mode)) {
		*device = new(alloc->alloc(alloc->allocator, sizeof(DirectoryDevice), alignof(DirectoryDevice))) DirectoryDevice(alloc, path);
	} else {
		*device = nullptr;
		returnCode = LFS_NOT_FOUND;
	}

	return returnCode;
}

void DirectoryDevice::destroy(void *device) {
	DirectoryDevice *dir = static_cast<DirectoryDevice *>(device);
	dir->~DirectoryDevice();
	dir->_alloc->free(dir->_alloc->allocator, device);
}

FILE *DirectoryDevice::openFile(const char *filePath, const char *modeString) {
	char *diskPath = getDevicePath(filePath);
	FILE *file = fopen(diskPath, modeString);
	freeDevicePath(diskPath);
	return file;
}

char *DirectoryDevice::getDevicePath(const char *filePath) {
	uint32_t diskPathLen = _pathLen + strlen(filePath) + 1;
	char *diskPath = reinterpret_cast<char*>(_alloc->alloc(_alloc->allocator, sizeof(char) * diskPathLen, alignof(char)));
	strcpy(diskPath, _devicePath);
	strcpy(diskPath + _pathLen, filePath);

	// replace forward slashes with backslashes on windows
#ifdef _WIN32
	for (uint32_t i = 0; i < diskPathLen; ++i) {
		if (diskPath[i] == '/')
			diskPath[i] = '\\';
	}
#endif

	return diskPath;
}

void DirectoryDevice::freeDevicePath(char *path) {
	_alloc->free(_alloc->allocator, path);
}

bool DirectoryDevice::fileExists(void *device, const char *filePath) {
	DirectoryDevice *dev = static_cast<DirectoryDevice*>(device);
	char *diskPath = dev->getDevicePath(filePath);

#ifdef _WIN32
	struct _stat statInfo;
	int result = _stat(diskPath, &statInfo);
#else
	struct stat statInfo;
	int result = stat(diskPath, &statInfo);
#endif

	dev->freeDevicePath(diskPath);
	return result == 0 && S_ISREG(statInfo.st_mode);
}

size_t DirectoryDevice::fileSize(void *device, const char *filePath) {
	DirectoryDevice *dev = static_cast<DirectoryDevice*>(device);
	char *diskPath = dev->getDevicePath(filePath);

#ifdef _WIN32
	struct _stat statInfo;
	int result = _stat(diskPath, &statInfo);
#else
	struct stat statInfo;
	int result = stat(diskPath, &statInfo);
#endif

	dev->freeDevicePath(diskPath);

	if (result == 0 && S_ISREG(statInfo.st_mode)) {
		return statInfo.st_size;
	} else {
		return 0;
	}
}

size_t DirectoryDevice::readFile(void *device, const char *filePath, Allocator *alloc, void **buffer) {
	size_t bytesRead = 0;

	DirectoryDevice *dir = static_cast<DirectoryDevice*>(device);
	FILE *file = dir->openFile(filePath, "rb");

	if (file) {
		size_t fileSize = 0;
		if (fseek(file, 0L, SEEK_END) == 0) {
			fileSize = ftell(file);
		}

		if (fileSize && fseek(file, 0L, SEEK_SET) == 0) {
			*buffer = dir->_alloc->alloc(dir->_alloc->allocator, fileSize, 1);
			if (*buffer) {
				bytesRead = fread(*buffer, 1, fileSize, file);
			}
		}

		fclose(file);
	}
	return bytesRead;
}

size_t DirectoryDevice::writeFile(void *device, const char *filePath, void *buffer, size_t bytesToWrite, bool append) {
	size_t bytesWritten = 0;

	DirectoryDevice *dev = static_cast<DirectoryDevice*>(device);
	FILE *file = dev->openFile(filePath, append ? "ab" : "wb");

	if (file) {
		bytesWritten = fwrite(buffer, 1, bytesToWrite, file);
		fclose(file);
	}

	return bytesWritten;
}

ErrorCode DirectoryDevice::deleteFile(void *device, const char *filePath) {
	DirectoryDevice *dev = static_cast<DirectoryDevice*>(device);
	char *diskPath = dev->getDevicePath(filePath);

	// TODO: handle other possible errors?
	ErrorCode result = unlink(diskPath) == 0 ? LFS_OK : LFS_NOT_FOUND;

	dev->freeDevicePath(diskPath);
	return result;
}

