// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.
#if !defined(LAMINAFS_DISABLE_DIRECTORY_DEVICE)

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "Directory.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define UNICODE 1
#define _UNICODE 1
#include <windows.h>
#include <shellapi.h>
#else
#include <errno.h>
#include <fts.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace laminaFS;

namespace {
#ifdef _WIN32
constexpr uint32_t MAX_PATH_LEN = 1024;

int widen(const char *inStr, WCHAR *outStr, size_t outStrLen) {
	return MultiByteToWideChar(CP_UTF8, 0, inStr, -1, outStr, static_cast<int>(outStrLen));
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
	case ERROR_ALREADY_EXISTS:
		result = LFS_ALREADY_EXISTS;
		break;
	case ERROR_DISK_FULL:
		result = LFS_OUT_OF_SPACE;
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
	case ENOSPC:
		result = LFS_OUT_OF_SPACE;
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
int DirectoryDevice::openFile(const char *filePath, int openFlags) {
	char *diskPath = getDevicePath(filePath);

	// XXX: open() can fail with EINTR, which means we're just going to retry until it works
	int file = -1;
	do {
		file = open(diskPath, openFlags, 0644);
	} while (file == -1 && errno == EINTR);

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

size_t DirectoryDevice::readFile(void *device, const char *filePath, uint64_t offset, uint64_t maxBytes, Allocator *alloc, void **buffer, bool nullTerminate, ErrorCode *outError) {
	size_t bytesRead = 0;
	DirectoryDevice *dir = static_cast<DirectoryDevice*>(device);
#ifdef _WIN32
	HANDLE file = dir->openFile(filePath, GENERIC_READ, OPEN_EXISTING);

	if (file) {
		LARGE_INTEGER result;
		GetFileSizeEx(file, &result);
		size_t fileSize = result.QuadPart;

		if (fileSize) {
			LARGE_INTEGER offsetStruct;
			offsetStruct.QuadPart = offset;
			if (SetFilePointer(file, offsetStruct.LowPart, &offsetStruct.HighPart, FILE_BEGIN) == INVALID_SET_FILE_POINTER){
				CloseHandle(file);
				return bytesRead;
			}

			fileSize = std::min(fileSize - offset, maxBytes);
			*buffer = alloc->alloc(alloc->allocator, fileSize + (nullTerminate ? 1 : 0), 1);

			DWORD bytesReadTemp = 0;
			if (!ReadFile(file, *buffer, static_cast<DWORD>(fileSize), &bytesReadTemp, nullptr)) {
				*outError = convertError(GetLastError());
			} else if (nullTerminate) {
				(*reinterpret_cast<char**>(buffer))[bytesReadTemp] = 0;
			}

			bytesRead = bytesReadTemp;
		}

		CloseHandle(file);
	} else {
		*outError = LFS_NOT_FOUND;
	}
#else
	int file = dir->openFile(filePath, O_RDONLY);

	if (file != -1) {
		uint64_t fileSize = static_cast<uint64_t>(lseek(file, 0, SEEK_END));
		if (fileSize == static_cast<uint64_t>(-1)) {
			*outError = convertError(errno);
			close(file);
			return 0;
		}

		if (fileSize && lseek(file, offset, SEEK_SET) == static_cast<off_t>(offset)) {
			fileSize = std::min(fileSize - offset, maxBytes);
			*buffer = alloc->alloc(alloc->allocator, fileSize + (nullTerminate ? 1 : 0), 1);

			if (*buffer) {
				// XXX: attempt read and retry if necessary
				ssize_t bytes = -1;
				do {
					bytes = read(file, *buffer, fileSize);
				} while (bytes == -1 && errno == EINTR);

				if (bytes == -1) {
					alloc->free(alloc->allocator, *buffer);
					*buffer = nullptr;

					*outError = convertError(errno);
					close(file);
					return 0;
				}

				bytesRead = static_cast<uint64_t>(bytes);

				if (nullTerminate) {
					(*reinterpret_cast<char**>(buffer))[bytesRead] = 0;
				}
			}
		} else {
			*outError = convertError(errno);
		}

		close(file);
	} else {
		*outError = convertError(errno);
	}
#endif
	return bytesRead;
}

size_t DirectoryDevice::writeFile(void *device, const char *filePath, uint64_t offset, void *buffer, size_t bytesToWrite, lfs_write_mode_t writeMode, ErrorCode *outError) {
	size_t bytesWritten = 0;
	DirectoryDevice *dev = static_cast<DirectoryDevice*>(device);

#ifdef _WIN32
	DWORD createFlags = 0;
	switch(writeMode) {
	case LFS_WRITE_TRUNCATE:
		createFlags = CREATE_ALWAYS;
		break;
	case LFS_WRITE_APPEND:
		createFlags = OPEN_ALWAYS;
		break;
	case LFS_WRITE_SEGMENT:
		createFlags = OPEN_EXISTING;
		break;
	}

	HANDLE file = dev->openFile(filePath, GENERIC_WRITE, createFlags);

	if (file) {
		if (writeMode == LFS_WRITE_APPEND) {
			LARGE_INTEGER offsetStruct;
			offsetStruct.QuadPart = 0;

			if (SetFilePointer(file, offsetStruct.LowPart, &offsetStruct.HighPart, FILE_END) == INVALID_SET_FILE_POINTER) {
				*outError = LFS_GENERIC_ERROR;
				CloseHandle(file);
				return bytesWritten;
			}
		} else if (writeMode == LFS_WRITE_SEGMENT){
			LARGE_INTEGER offsetStruct;
			offsetStruct.QuadPart = offset;

			if (SetFilePointer(file, offsetStruct.LowPart, &offsetStruct.HighPart, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
				*outError = LFS_GENERIC_ERROR;
				CloseHandle(file);
				return bytesWritten;
			}
		}

		DWORD temp = 0;
		if (!WriteFile(file, buffer, static_cast<DWORD>(bytesToWrite), &temp, nullptr)) {
				*outError = LFS_GENERIC_ERROR;
		} else {
			bytesWritten = temp;
		}
	} else {
		*outError = convertError(GetLastError());
	}

	CloseHandle(file);
#else
	int openFlags = 0;
	switch(writeMode) {
	case LFS_WRITE_TRUNCATE:
		openFlags = O_TRUNC;
		break;
	case LFS_WRITE_APPEND:
		openFlags = O_APPEND;
		break;
	case LFS_WRITE_SEGMENT:
		openFlags = 0;
		break;
	}

	int file = dev->openFile(filePath, O_WRONLY | O_CREAT | openFlags);

	if (file != -1 && lseek(file, static_cast<off_t>(offset), SEEK_SET) == static_cast<off_t>(offset)) {
		// XXX: attempt write and retry if necessary
		ssize_t bytes = -1;
		do {
			bytes = write(file, buffer, bytesToWrite);
		} while(bytes == -1 && errno == EINTR);

		if (bytes == -1) {
			*outError = convertError(errno);
		} else {
			bytesWritten = static_cast<uint64_t>(bytes);
		}

		close(file);
	} else {
		*outError = convertError(errno);
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
	if (mkdir(diskPath, DEFFILEMODE | S_IXUSR | S_IXGRP | S_IRWXO) != 0) {
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

#endif // LAMINAFS_DISABLE_DIRECTORY_DEVICE
