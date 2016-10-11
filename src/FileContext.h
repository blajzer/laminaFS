#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace laminaFS {

typedef void* FileHandle;
struct File;

//! FileContext is the "main" object in LaminaFS. It handles management of files,
//! mounts, and the backend processing that occurs.
class FileContext {
public:
	FileContext();
	~FileContext();

	//! DeviceInterface is the extensibility mechanism for adding new types of
	//! devices. Fill out one of these and then call registerDeviceInterface().
	struct DeviceInterface {
		typedef void *(*CreateFunc)();
		typedef void (*DestroyFunc)(void*);

		typedef FileHandle (*OpenFileFunc)(void *, const char *, uint32_t);
		typedef void (*CloseFileFunc)(void *, FileHandle);
		typedef bool (*FileExistsFunc)(void *, const char *);
		typedef size_t (*FileSizeFunc)(void *, FileHandle);
		typedef size_t (*ReadFileFunc)(void *, FileHandle, size_t, uint8_t *, size_t);
		
		typedef size_t (*WriteFileFunc)(void *, FileHandle, uint8_t *, size_t);
		typedef int (*DeleteFileFunc)(void *, const char *);

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
	void createMount(uint32_t deviceType, const char *mountPoint, const char *devicePath, bool virtualPath);

	static uint32_t kDirectoryDeviceIndex;
private:
	struct Mount {
		const char *_prefix;
		void *_device;
		DeviceInterface *_interface;
	};

	std::vector<DeviceInterface> _interfaces;
};

}
