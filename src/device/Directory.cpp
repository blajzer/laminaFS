// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include "Directory.h"

#include "platform.h"

#include <cstdio>
#include <cstring>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif

#else
#include <fts.h>
#include <unistd.h>
#endif

using namespace laminaFS;

namespace {
#ifdef _WIN32
typedef struct _stat StatType;
#define PlatformStat _stat
#else
typedef struct stat StatType;
#define PlatformStat stat
#endif
}

DirectoryDevice::DirectoryDevice(Allocator *allocator, const char *path) {
	_alloc = allocator;

	// TODO: realpath()
	_pathLen = strlen(path);
	_devicePath = reinterpret_cast<char*>(_alloc->alloc(_alloc->allocator, sizeof(char) * (_pathLen + 1), alignof(char)));
	strcpy_s(_devicePath, _pathLen + 1, path);
}

DirectoryDevice::~DirectoryDevice() {
	_alloc->free(_alloc->allocator, _devicePath);
}

ErrorCode DirectoryDevice::create(Allocator *alloc, const char *path, void **device) {
	StatType statInfo;
	int result = PlatformStat(path, &statInfo);

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
	FILE *file = nullptr;
	fopen_s(&file, diskPath, modeString);
	freeDevicePath(diskPath);
	return file;
}

char *DirectoryDevice::getDevicePath(const char *filePath, bool extraNull) {
	uint32_t diskPathLen = _pathLen + strlen(filePath) + (extraNull ? 2 : 1);
	char *diskPath = reinterpret_cast<char*>(_alloc->alloc(_alloc->allocator, sizeof(char) * diskPathLen, alignof(char)));
	strcpy_s(diskPath, diskPathLen, _devicePath);
	strcpy_s(diskPath + _pathLen, diskPathLen - _pathLen, filePath);

	if (extraNull) {
		diskPath[diskPathLen - 1] = 0;
	}

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

	StatType statInfo;
	int result = PlatformStat(diskPath, &statInfo);

	dev->freeDevicePath(diskPath);
	return result == 0 && S_ISREG(statInfo.st_mode);
}

size_t DirectoryDevice::fileSize(void *device, const char *filePath) {
	DirectoryDevice *dev = static_cast<DirectoryDevice*>(device);
	char *diskPath = dev->getDevicePath(filePath);

	StatType statInfo;
	int result = PlatformStat(diskPath, &statInfo);

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

	ErrorCode resultCode = LFS_OK;
#ifdef _WIN32
	int result = _unlink(diskPath);
#else
	int result = unlink(diskPath);
#endif

	if (result != 0) {
		switch (errno) {
		case EPERM:
		case EACCES:
			resultCode = LFS_PERMISSIONS_ERROR;
			break;
		case EROFS:
			resultCode = LFS_UNSUPPORTED;
			break;
		default:
			resultCode = LFS_GENERIC_ERROR;
			break;
		};
	}
	
	dev->freeDevicePath(diskPath);
	return resultCode;
}

ErrorCode DirectoryDevice::createDir(void *device, const char *path) {
	DirectoryDevice *dev = static_cast<DirectoryDevice*>(device);
	char *diskPath = dev->getDevicePath(path);
	
	ErrorCode resultCode = LFS_OK;
#ifdef _WIN32
	if (!CreateDirectory(diskPath, nullptr)) {
		switch (GetLastError()) {
		case ERROR_ALREADY_EXISTS:
			resultCode = LFS_ALREADY_EXISTS;
			break;
		case ERROR_PATH_NOT_FOUND:
			resultCode = LFS_NOT_FOUND;
			break;
		}
	}
#else
	int result = mkdir(diskPath, DEFFILEMODE | S_IXUSR | S_IXGRP | S_IRWXO);

	if (result != 0) {
		switch (errno) {
		case EEXIST:
			resultCode = LFS_ALREADY_EXISTS;
			break;
		case EACCES:
			resultCode = LFS_PERMISSIONS_ERROR;
			break;
		case EROFS:
			resultCode = LFS_UNSUPPORTED;
			break;
		default:
			resultCode = LFS_GENERIC_ERROR;
			break;
		};
	}
#endif

	dev->freeDevicePath(diskPath);
	return resultCode;
}

ErrorCode DirectoryDevice::deleteDir(void *device, const char *path) {
	DirectoryDevice *dev = static_cast<DirectoryDevice*>(device);
	char *diskPath = dev->getDevicePath(path, true);

	ErrorCode resultCode = LFS_OK;

#ifdef _WIN32
	// just use the shell API to delete the directory.
	// requires path to be double null-terminated.
	// https://msdn.microsoft.com/en-us/library/windows/desktop/bb762164(v=vs.85).aspx
	SHFILEOPSTRUCT shOp;
	shOp.hwnd = nullptr;
	shOp.wFunc = FO_DELETE;
	shOp.pFrom = diskPath;
	shOp.pTo = nullptr;
	shOp.fFlags = FOF_NO_UI;
	shOp.fAnyOperationsAborted = FALSE;
	shOp.hNameMappings = nullptr;
	shOp.lpszProgressTitle = nullptr;
	
	int result = SHFileOperation(&shOp);
	
	switch (result) {
	case 0:
		break;
	case 0x7C: // DE_INVALIDFILES
		resultCode = LFS_NOT_FOUND;
		break;
	case 0x78: // DE_ACCESSDENIEDSRC
	case 0x86: // DE_SRC_IS_CDROM
	case 0x87: // DE_SRC_IS_DVD
		resultCode = LFS_PERMISSIONS_ERROR;
		break;
	default:
		resultCode = LFS_GENERIC_ERROR;
		break;
	};
#else
	FTS *fts = nullptr;
	char *fileList[] = {diskPath, nullptr};
	bool error = false;

	if ((fts = fts_open(fileList, FTS_PHYSICAL | FTS_NOCHDIR | FTS_XDEV, nullptr))) {
		FTSENT *ent = fts_read(fts);
		while (!error && ent) {
			switch (ent->fts_info) {
			// ignore directories until post-traversal (FTS_DP)
			case FTS_D:
				break;

			// normal stuff we want to delete
			case FTS_DEFAULT:
			case FTS_DP:
			case FTS_F:
			case FTS_SL:
			case FTS_SLNONE:
				if (remove(ent->fts_accpath) == -1) {
					error = true;
				}
				break;

			// shouldn't ever hit these
			case FTS_DC:
			case FTS_DOT:
			case FTS_NSOK:
				break;

			// errors
			case FTS_NS:
			case FTS_DNR:
			case FTS_ERR:
				error = true;
				errno = ent->fts_errno;
				break;
			};

			if (!error)
				ent = fts_read(fts);
		}

		error = error || errno != 0;
		fts_close(fts);
	}
	
	if (error) {
		switch (errno) {
		case EACCES:
		case EROFS:
			resultCode = LFS_PERMISSIONS_ERROR;
			break;
		case ENOENT:
			resultCode = LFS_NOT_FOUND;
			break;
		default:
			resultCode = LFS_GENERIC_ERROR;
			break;
		};
		
	}
#endif

	dev->freeDevicePath(diskPath);
	return resultCode;
}

