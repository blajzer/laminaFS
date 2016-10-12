#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "shared_types.h"

struct lfs_file_t;

namespace laminaFS {

typedef void* FileHandle;
typedef lfs_file_t File;
typedef lfs_file_mode_t FileMode;
typedef lfs_error_code_t ErrorCode;

//! FileContext is the "main" object in LaminaFS. It handles management of files,
//! mounts, and the backend processing that occurs.
class FileContext {
public:
	FileContext();
	~FileContext();

	typedef int (*LogFunc)(const char *, ...);

	//! DeviceInterface is the extensibility mechanism for adding new types of
	//! devices. Fill out one of these and then call registerDeviceInterface().
	struct DeviceInterface {
		typedef void *(*CreateFunc)(const char *, bool);
		typedef void (*DestroyFunc)(void*);

		typedef ErrorCode (*OpenFileFunc)(void *, const char *, FileMode *, FileHandle *);
		typedef void (*CloseFileFunc)(void *, FileHandle);
		typedef bool (*FileExistsFunc)(void *, const char *);
		typedef size_t (*FileSizeFunc)(void *, FileHandle);
		typedef size_t (*ReadFileFunc)(void *, FileHandle, size_t, uint8_t *, size_t);
		
		typedef size_t (*WriteFileFunc)(void *, FileHandle, uint8_t *, size_t);
		typedef ErrorCode (*DeleteFileFunc)(void *, const char *);

		// required
		CreateFunc _create = nullptr;
		DestroyFunc _destroy = nullptr;

		OpenFileFunc _openFile = nullptr;
		CloseFileFunc _closeFile = nullptr;
		FileExistsFunc _fileExists = nullptr;
		FileSizeFunc _fileSize = nullptr;
		ReadFileFunc _readFile = nullptr;

		// optional
		WriteFileFunc _writeFile = nullptr;
		DeleteFileFunc _deleteFile = nullptr;
	};

	//! Registers a new device interface.
	//! @param interface the interface specification
	//! @return the device type index, used for passing to createMount() or -1 on error
	int32_t registerDeviceInterface(DeviceInterface &interface);

	//! Creates a new mount.
	//! @param deviceType the device type, as returned by registerDeviceInterface()
	//! @param mountPoint the virtual path to mount this device to
	//! @param devicePath the path to pass into the device
	//! @param virtualPath whether or not the devicePath refers to a virtual path or "real" path
	void createMount(uint32_t deviceType, const char *mountPoint, const char *devicePath, bool virtualPath);

	//! Open a file
	//! @param path the virtual path to the file
	//! @param fileMode the mode to open the file in, will be adjusted based on returned file mount capabilities
	//! @param file outval which will contain the file
	//! return the return code, 0 on success
	ErrorCode openFile(const char *path, FileMode &fileMode, File **file);

	//! Close a file.
	//! @param file the handle to the file
	//! @return the return code, 0 on success
	ErrorCode closeFile(File *file);

	//! Sets the log function.
	//! @param func the logging function
	void setLogFunc(LogFunc func) { _log = func; }
	
	//! Gets the log function.
	//! @return the logging function
	LogFunc getLogFunc() { return _log; }

	//! The type index of the Directory device. It will always be the first interface.
	static const uint32_t kDirectoryDeviceIndex = 0;
private:
	friend struct ::lfs_file_t;

	struct Mount {
		char *_prefix;
		void *_device;
		DeviceInterface *_interface;
		uint32_t _prefixLen;
	};

	std::vector<DeviceInterface> _interfaces;
	std::vector<Mount> _mounts;
	std::vector<File*> _files;
	LogFunc _log = nullptr;
};

}
