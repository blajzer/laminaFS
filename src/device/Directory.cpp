// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "Directory.h"

#include <cstdio>
#include <cstring>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#define UNICODE 1
#define _UNICODE 1
#include <windows.h>
#include <shellapi.h>
#else
#include <fts.h>
#include <unistd.h>
#endif

using namespace laminaFS;

namespace {
#ifdef _WIN32
constexpr uint32_t MAX_PATH_LEN = 1024;

int widen(const char *inStr, WCHAR *outStr, size_t outStrLen) {
	return MultiByteToWideChar(CP_UTF8, 0, inStr, -1, outStr, (int)outStrLen);
}

ErrorCode convertError(DWORD error) {
	ErrorCode result = LFS_GENERIC_ERROR;

	switch (error) {
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		result = LFS_NOT_FOUND;
		break;
	case ERROR_ACCESS_DENIED:
		result = LFS_PERMISSIONS_ERROR;
		break;
	};

	return result;
}

#else
ErrorCode convertError(int error) {
	ErrorCode result = LFS_GENERIC_ERROR;

	switch (error) {
	case EEXIST:
		result = LFS_ALREADY_EXISTS;
		break;
	case EPERM:
	case EACCES:
		result = LFS_PERMISSIONS_ERROR;
		break;
	case EROFS:
		result = LFS_UNSUPPORTED;
		break;
	case ENOENT:
		result = LFS_NOT_FOUND;
		break;
	}

	return result;
}
#endif
}

DirectoryDevice::DirectoryDevice(Allocator *allocator, const char *path) {
	_alloc = allocator;

	// TODO: realpath()
	_pathLen = static_cast<uint32_t>(strlen(path));
	_devicePath = reinterpret_cast<char*>(_alloc->alloc(_alloc->allocator, sizeof(char) * (_pathLen + 1), alignof(char)));
	strcpy(_devicePath, path);
}

DirectoryDevice::~DirectoryDevice() {
	_alloc->free(_alloc->allocator, _devicePath);
}

ErrorCode DirectoryDevice::create(Allocator *alloc, const char *path, void **device) {
	ErrorCode returnCode = LFS_OK;
#ifdef _WIN32
	WCHAR windowsPath[MAX_PATH_LEN];
	widen(path, &windowsPath[0], MAX_PATH_LEN);

	DWORD result = GetFileAttributesW(&windowsPath[0]);

	if (result != INVALID_FILE_ATTRIBUTES && (result & FILE_ATTRIBUTE_DIRECTORY)) {
		*device = new(alloc->alloc(alloc->allocator, sizeof(DirectoryDevice), alignof(DirectoryDevice))) DirectoryDevice(alloc, path);
	} else {
		returnCode = LFS_NOT_FOUND;
	}
#else
	struct stat statInfo;
	int result = stat(path, &statInfo);

	if (result == 0 && S_ISDIR(statInfo.st_mode)) {
		*device = new(alloc->alloc(alloc->allocator, sizeof(DirectoryDevice), alignof(DirectoryDevice))) DirectoryDevice(alloc, path);
	} else {
		*device = nullptr;
		returnCode = LFS_NOT_FOUND;
	}
#endif

	return returnCode;
}

void DirectoryDevice::destroy(void *device) {
	DirectoryDevice *dir = static_cast<DirectoryDevice *>(device);
	dir->~DirectoryDevice();
	dir->_alloc->free(dir->_alloc->allocator, device);
}

#ifdef _WIN32
void *DirectoryDevice::openFile(const char *filePath, uint32_t accessMode, uint32_t createMode) {
	char *diskPath = getDevicePath(filePath);
	WCHAR windowsPath[MAX_PATH_LEN];
	widen(diskPath, &windowsPath[0], MAX_PATH_LEN);
	freeDevicePath(diskPath);

	HANDLE file = CreateFileW(
		&windowsPath[0],
		accessMode,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		createMode,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	return file;
}
#else
FILE *DirectoryDevice::openFile(const char *filePath, const char *modeString) {
	char *diskPath = getDevicePath(filePath);
	FILE *file = fopen(diskPath, modeString);
	freeDevicePath(diskPath);
	return file;
}
#endif

char *DirectoryDevice::getDevicePath(const char *filePath) {
	uint32_t diskPathLen = static_cast<uint32_t>(_pathLen + strlen(filePath) + 1);
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
	WCHAR windowsPath[MAX_PATH_LEN];
	widen(diskPath, &windowsPath[0], MAX_PATH_LEN);
	dev->freeDevicePath(diskPath);

	DWORD result = GetFileAttributesW(&windowsPath[0]);

	return result != INVALID_FILE_ATTRIBUTES && !(result & FILE_ATTRIBUTE_DIRECTORY);
#else
	struct stat statInfo;
	int result = stat(diskPath, &statInfo);

	dev->freeDevicePath(diskPath);
	return result == 0 && S_ISREG(statInfo.st_mode);
#endif
}

size_t DirectoryDevice::fileSize(void *device, const char *filePath, ErrorCode *outError) {
	DirectoryDevice *dev = static_cast<DirectoryDevice*>(device);
	size_t size = 0;

#ifdef _WIN32
	HANDLE file = dev->openFile(filePath, GENERIC_READ, OPEN_EXISTING);

	if (file) {
		LARGE_INTEGER result;
		if(!GetFileSizeEx(file, &result)) {
			printf("error code %lu\n", GetLastError());
		} else {
			size = result.QuadPart;
		}
		CloseHandle(file);
	} else {
		*outError = convertError(GetLastError());
	}
#else
	char *diskPath = dev->getDevicePath(filePath);
	struct stat statInfo;
	int result = stat(diskPath, &statInfo);
	dev->freeDevicePath(diskPath);

	if (result == 0 && S_ISREG(statInfo.st_mode)) {
		*outError = LFS_OK;
		size = statInfo.st_size;
	} else if (result != 0) {
		*outError = convertError(errno);
	} else {
	   *outError = LFS_UNSUPPORTED;
	}
#endif
	return size;
}

size_t DirectoryDevice::readFile(void *device, const char *filePath, Allocator *alloc, void **buffer, bool nullTerminate, ErrorCode *outError) {
	size_t bytesRead = 0;
	DirectoryDevice *dir = static_cast<DirectoryDevice*>(device);
#ifdef _WIN32
	HANDLE file = dir->openFile(filePath, GENERIC_READ, OPEN_EXISTING);

	if (file) {
		LARGE_INTEGER result;
		GetFileSizeEx(file, &result);
		size_t fileSize = result.QuadPart;

		if (fileSize) {
			*buffer = alloc->alloc(alloc->allocator, fileSize + (nullTerminate ? 1 : 0), 1);

			DWORD bytesReadTemp = 0;
			if (!ReadFile(file, *buffer, (DWORD)fileSize, &bytesReadTemp, nullptr)) {
				*outError = convertError(GetLastError());
			} else if (nullTerminate) {
				(*(char**)buffer)[bytesRead] = 0;
			}

			bytesRead = bytesReadTemp;
		}

		CloseHandle(file);
	} else {
		*outError = LFS_NOT_FOUND;
	}
#else
	FILE *file = dir->openFile(filePath, "rb");

	if (file) {
		size_t fileSize = 0;
		if (fseek(file, 0L, SEEK_END) == 0) {
			fileSize = ftell(file);
		}

		if (fileSize && fseek(file, 0L, SEEK_SET) == 0) {
			*buffer = alloc->alloc(alloc->allocator, fileSize + (nullTerminate ? 1 : 0), 1);
			if (*buffer) {
				bytesRead = fread(*buffer, 1, fileSize, file);

				if (nullTerminate) {
					(*(char**)buffer)[bytesRead] = 0;
				}
			}
		}

		*outError = ferror(file) ? LFS_GENERIC_ERROR : LFS_OK;

		fclose(file);
	} else {
		*outError = LFS_NOT_FOUND;
	}
#endif
	return bytesRead;
}

size_t DirectoryDevice::writeFile(void *device, const char *filePath, void *buffer, size_t bytesToWrite, bool append, ErrorCode *outError) {
	size_t bytesWritten = 0;
	DirectoryDevice *dev = static_cast<DirectoryDevice*>(device);

#ifdef _WIN32
	HANDLE file = dev->openFile(filePath, GENERIC_WRITE, append ? OPEN_ALWAYS : CREATE_ALWAYS);

	if (file) {
		if (append) {
			if (SetFilePointer(file, 0, nullptr, FILE_END) == INVALID_SET_FILE_POINTER) {
				*outError = LFS_GENERIC_ERROR;
				CloseHandle(file);
				return bytesWritten;
			}
		}

		DWORD temp = 0;
		if (!WriteFile(file, buffer, (DWORD)bytesToWrite, &temp, nullptr)) {
				*outError = LFS_GENERIC_ERROR;
		} else {
			bytesWritten = temp;
		}
	} else {
		*outError = convertError(GetLastError());
	}

	CloseHandle(file);
#else
	FILE *file = dev->openFile(filePath, append ? "ab" : "wb");

	if (file) {
		bytesWritten = fwrite(buffer, 1, bytesToWrite, file);
		*outError = ferror(file) ? LFS_GENERIC_ERROR : LFS_OK;
		fclose(file);
	} else {
		*outError = LFS_PERMISSIONS_ERROR;
	}
#endif
	return bytesWritten;
}

ErrorCode DirectoryDevice::deleteFile(void *device, const char *filePath) {
	DirectoryDevice *dev = static_cast<DirectoryDevice*>(device);
	char *diskPath = dev->getDevicePath(filePath);

	ErrorCode resultCode = LFS_OK;
#ifdef _WIN32
	WCHAR windowsPath[MAX_PATH_LEN];
	widen(diskPath, &windowsPath[0], MAX_PATH_LEN);

	if (!DeleteFileW(&windowsPath[0])) {
		resultCode = convertError(GetLastError());
	}
#else
	if (unlink(diskPath) != 0) {
		resultCode = convertError(errno);
	}
#endif

	dev->freeDevicePath(diskPath);
	return resultCode;
}

ErrorCode DirectoryDevice::createDir(void *device, const char *path) {
	DirectoryDevice *dev = static_cast<DirectoryDevice*>(device);
	char *diskPath = dev->getDevicePath(path);

	ErrorCode resultCode = LFS_OK;
#ifdef _WIN32
	WCHAR windowsPath[MAX_PATH_LEN];
	widen(diskPath, &windowsPath[0], MAX_PATH_LEN);

	if (!CreateDirectoryW(&windowsPath[0], nullptr)) {
		resultCode = convertError(GetLastError());
	}
#else
	int result = mkdir(diskPath, DEFFILEMODE | S_IXUSR | S_IXGRP | S_IRWXO);

	if (result != 0) {
		resultCode = convertError(errno);
	}
#endif

	dev->freeDevicePath(diskPath);
	return resultCode;
}

ErrorCode DirectoryDevice::deleteDir(void *device, const char *path) {
	DirectoryDevice *dev = static_cast<DirectoryDevice*>(device);
	char *diskPath = dev->getDevicePath(path);

	ErrorCode resultCode = LFS_OK;

#ifdef _WIN32
	WCHAR windowsPath[MAX_PATH_LEN];
	int len = widen(diskPath, &windowsPath[0], MAX_PATH_LEN - 1); // XXX: ensure one space for double-null
	windowsPath[len] = 0;

	// just use the shell API to delete the directory.
	// requires path to be double null-terminated.
	// https://msdn.microsoft.com/en-us/library/windows/desktop/bb762164(v=vs.85).aspx
	SHFILEOPSTRUCTW shOp;
	shOp.hwnd = nullptr;
	shOp.wFunc = FO_DELETE;
	shOp.pFrom = &windowsPath[0];
	shOp.pTo = nullptr;
	shOp.fFlags = FOF_NO_UI;
	shOp.fAnyOperationsAborted = FALSE;
	shOp.hNameMappings = nullptr;
	shOp.lpszProgressTitle = nullptr;

	int result = SHFileOperationW(&shOp);

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
		resultCode = convertError(errno);
	}
#endif

	dev->freeDevicePath(diskPath);
	return resultCode;
}
