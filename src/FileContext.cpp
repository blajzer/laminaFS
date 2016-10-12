// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include "FileContext.h"

#include "device/Directory.h"

#include <cstring>

using namespace laminaFS;

#define LOG(MSG,...) if (_log) { _log(MSG, ##__VA_ARGS__); }

// TODO: think about this some more
struct lfs_file_t {
	FileHandle _file;
	FileContext::Mount *_mount;
};

FileContext::FileContext() {
	DeviceInterface i;
	i._create = &DirectoryDevice::create;
	i._destroy = &DirectoryDevice::destroy;
	i._openFile = &DirectoryDevice::openFile;
	i._closeFile = &DirectoryDevice::closeFile;
	i._fileExists = &DirectoryDevice::fileExists;
	i._fileSize = &DirectoryDevice::fileSize;
	i._readFile = &DirectoryDevice::readFile;
	i._writeFile = &DirectoryDevice::writeFile;
	i._deleteFile = &DirectoryDevice::deleteFile;

	registerDeviceInterface(i);
}

FileContext::~FileContext() {
	for (File *f : _files) {
		Mount *m = f->_mount;
		m->_interface->_closeFile(m->_device, f->_file);
		delete f;
	}

	for (Mount &m : _mounts) {
		delete [] m._prefix;
		m._interface->_destroy(m._device);
	}
}

int32_t FileContext::registerDeviceInterface(DeviceInterface &interface) {
	int32_t result = -1;

	if (interface._create != nullptr &&
		interface._destroy != nullptr &&
		interface._openFile != nullptr &&
		interface._closeFile != nullptr &&
		interface._fileExists != nullptr &&
		interface._fileSize != nullptr &&
		interface._readFile != nullptr)
	{
		result = _interfaces.size();
		_interfaces.push_back(interface);	
	}

	return result;
}

ErrorCode FileContext::createMount(uint32_t deviceType, const char *mountPoint, const char *devicePath, bool virtualPath) {
	ErrorCode result = LFS_OK;
	Mount m;

	DeviceInterface *interface = &(_interfaces[deviceType]);
	result = interface->_create(devicePath, virtualPath, &m._device);
	m._interface = interface;

	if (result == LFS_OK && m._device) {
		size_t mountLen = strlen(mountPoint);
		m._prefix = new char[mountLen + 1];
		m._prefixLen = mountLen;
		strcpy(m._prefix, mountPoint);

		_mounts.push_back(m);
		LOG("mounted device %u:%s on %s\n", deviceType, devicePath, mountPoint);
	} else {
		LOG("unable to mount device %u:%s on %s\n", deviceType, devicePath, mountPoint);
	}
	
	return result;
}

ErrorCode FileContext::openFile(const char *path, FileMode &fileMode, File **file) {
	ErrorCode result = LFS_NOT_FOUND;

	LOG("searching for file %s\n", path);

	File *f = new File();

	// search mounts from the end
	for (auto it = _mounts.rbegin(); it != _mounts.rend(); ++it) {
		const char *foundMount = strstr(path, it->_prefix);
		if (foundMount && foundMount == path && (path[it->_prefixLen] == '/' || it->_prefixLen == 1)) {
			LOG("  found matching mount %s\n", it->_prefix);
			
			FileHandle fileHandle = nullptr;
			ErrorCode res = it->_interface->_openFile(it->_device, it->_prefixLen == 1 ? path : path + it->_prefixLen, &fileMode, &fileHandle);
			result = res;
			if (res == LFS_NOT_FOUND) {
				continue;
			} else {
				LOG("  found file\n");
				f->_file = fileHandle;
				f->_mount = &(*it);
				break;
			}
		}
	}
	
	if (result == LFS_OK) {
		*file = f;
	} else {
		*file = nullptr;
	}
	
	return result;
}

ErrorCode FileContext::closeFile(File *file) {
	delete file;
	return LFS_NOT_FOUND;
}

