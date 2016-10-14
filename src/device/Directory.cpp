// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include "Directory.h"

#include <cstdio>
#include <cstring>

using namespace laminaFS;

DirectoryDevice::DirectoryDevice(const char *path) {
	// TODO: realpath()
	_pathLen = strlen(path);
	_devicePath = new char[_pathLen + 1];
	strcpy(_devicePath, path);
}

DirectoryDevice::~DirectoryDevice() {
	delete [] _devicePath;
}

ErrorCode DirectoryDevice::create(const char *path, void **device) {
	// TODO: verify directory exists, set return code accordingly
	ErrorCode returnCode = LFS_OK;
	*device = new DirectoryDevice(path);

	return returnCode;
}

void DirectoryDevice::destroy(void *device) {
	delete static_cast<DirectoryDevice *>(device);
}

ErrorCode DirectoryDevice::openFile(void *device, const char *filePath, FileMode *fileMode, FileHandle *file) {
	DirectoryDevice *dev = static_cast<DirectoryDevice*>(device);
	
	// TODO: Windows paths
	uint32_t diskPathLen = dev->_pathLen + strlen(filePath) + 1;
	char *diskPath = new char[diskPathLen];
	strcpy(diskPath, dev->_devicePath);
	strcpy(diskPath + dev->_pathLen, filePath);
	
	const char *modeString = 0;
	switch (*fileMode) {
	case LFS_FM_READ:
		modeString = "r";
		break;
	case LFS_FM_WRITE:
		modeString = "w";
		break;
	case LFS_FM_READ_WRITE:
		modeString = "r+";
		break;
	case LFS_FM_APPEND:
		modeString = "a";
		break;
	};

	ErrorCode returnVal = LFS_OK;
	
	FILE *realFile = fopen(diskPath, modeString);
	if (realFile) {
		*file = realFile;
	} else {
		returnVal = LFS_NOT_FOUND;
		*file = nullptr;
	}

	delete [] diskPath;

	return returnVal;
}

void DirectoryDevice::closeFile(void *device, FileHandle file) {
	if (file) {
		fclose(static_cast<FILE*>(file));
	}
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

