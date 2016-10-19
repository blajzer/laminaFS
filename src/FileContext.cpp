// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include "FileContext.h"

#include "device/Directory.h"

#include <atomic>
#include <cstring>
#include <malloc.h>

using namespace laminaFS;

#define LOG(MSG,...) if (_log) { _log(MSG, ##__VA_ARGS__); }

enum lfs_file_operation_t {
	LFS_OP_EXISTS,
	LFS_OP_SIZE,
	LFS_OP_READ,
	LFS_OP_WRITE,
	LFS_OP_APPEND,
	LFS_OP_DELETE,
};

struct lfs_work_item_t {
	lfs_file_operation_t _operation;
	lfs_work_item_callback_t _callback = nullptr;
	lfs_allocator_t *_allocator = nullptr;

	const char *_filename = nullptr;

	void *_buffer = nullptr;
	uint64_t _bufferBytes = 0;

	lfs_error_code_t _resultCode = LFS_OK;
	std::atomic<bool> _completed;
};

void *default_alloc_func(void *, size_t bytes, size_t alignment) {
#if defined _WIN32
	return _aligned_malloc(bytes, alignment);
#else
	return memalign(alignment, bytes);
#endif
}

void default_free_func(void *, void *ptr) {
	free(ptr);
}

lfs_allocator_t lfs_default_allocator = { default_alloc_func, default_free_func, nullptr };

namespace laminaFS {
Allocator DefaultAllocator = lfs_default_allocator;
	
ErrorCode WorkItemGetResult(WorkItem *workItem) {
	return workItem->_resultCode;
}

void *WorkItemGetBuffer(WorkItem *workItem) {
	return workItem->_buffer;
}

void WorkItemFreeBuffer(WorkItem *workItem) {
	workItem->_allocator->free(workItem->_allocator, workItem->_buffer);
}

uint64_t WorkItemGetBytes(WorkItem *workItem) {
	return workItem->_bufferBytes;
}

bool WorkItemCompleted(WorkItem *workItem) {
	return workItem->_completed;
}

void WaitForWorkItem(WorkItem *workItem) {
	while (!workItem->_completed)
		std::this_thread::yield();
}
}

FileContext::FileContext(Allocator &alloc, uint64_t maxQueuedWorkItems, uint64_t workItemPoolSize)
: _workItemPool(alloc, workItemPoolSize)
, _workItemQueue(alloc, maxQueuedWorkItems)
, _alloc(alloc)
{
	DeviceInterface i;
	i._create = &DirectoryDevice::create;
	i._destroy = &DirectoryDevice::destroy;
	i._fileExists = &DirectoryDevice::fileExists;
	i._fileSize = &DirectoryDevice::fileSize;
	i._readFile = &DirectoryDevice::readFile;
	i._writeFile = &DirectoryDevice::writeFile;
	i._deleteFile = &DirectoryDevice::deleteFile;

	registerDeviceInterface(i);

	// create processing thread
	_processing = true;
	_processingThread = std::thread(&FileContext::processingFunc, this);
}

FileContext::~FileContext() {
	_processing = false;
	_processingThread.join();

	for (Mount &m : _mounts) {
		_alloc.free(_alloc.allocator, m._prefix);
		m._interface->_destroy(m._device);
	}
}

int32_t FileContext::registerDeviceInterface(DeviceInterface &interface) {
	int32_t result = -1;

	if (interface._create != nullptr &&
		interface._destroy != nullptr &&
		interface._fileExists != nullptr &&
		interface._fileSize != nullptr &&
		interface._readFile != nullptr)
	{
		result = _interfaces.size();
		_interfaces.push_back(interface);	
	}

	return result;
}

ErrorCode FileContext::createMount(uint32_t deviceType, const char *mountPoint, const char *devicePath) {
	ErrorCode result = LFS_OK;
	Mount m;

	DeviceInterface *interface = &(_interfaces[deviceType]);
	result = interface->_create(&_alloc, devicePath, &m._device);
	m._interface = interface;

	if (result == LFS_OK && m._device) {
		size_t mountLen = strlen(mountPoint);
		m._prefix = reinterpret_cast<char*>(_alloc.alloc(_alloc.allocator, sizeof(char) * (mountLen + 1), alignof(char)));
		m._prefixLen = mountLen;
		strcpy(m._prefix, mountPoint);

		_mounts.push_back(m);
		LOG("mounted device %u:%s on %s\n", deviceType, devicePath, mountPoint);
	} else {
		LOG("unable to mount device %u:%s on %s\n", deviceType, devicePath, mountPoint);
	}
	
	return result;
}

FileContext::Mount* FileContext::findMountAndPath(const char *path, const char **devicePath) {
	LOG("searching for file %s\n", path);

	*devicePath = nullptr;

	// search mounts from the end
	for (auto mount = _mounts.rbegin(); mount != _mounts.rend(); ++mount) {
		const char *foundMount = strstr(path, mount->_prefix);
		if (foundMount && foundMount == path && (path[mount->_prefixLen] == '/' || mount->_prefixLen == 1)) {
			LOG("  found matching mount %s\n", mount->_prefix);

			*devicePath = mount->_prefixLen == 1 ? path : path + mount->_prefixLen;
			bool exists = mount->_interface->_fileExists(mount->_device, *devicePath);

			if (!exists) {
				*devicePath = nullptr;
				continue;
			} else {
				return &(*mount);
				break;
			}
		}
	}
	
	return nullptr;
}

FileContext::Mount* FileContext::findMutableMountAndPath(const char *path, const char **devicePath, uint32_t op) {
	LOG("searching for writable mount for %s\n", path);

	*devicePath = nullptr;

	// search mounts from the end
	for (auto mount = _mounts.rbegin(); mount != _mounts.rend(); ++mount) {
		const char *foundMount = strstr(path, mount->_prefix);
		if (foundMount && foundMount == path && (path[mount->_prefixLen] == '/' || mount->_prefixLen == 1)) {
			LOG("  found matching mount %s\n", mount->_prefix);

			*devicePath = mount->_prefixLen == 1 ? path : path + mount->_prefixLen;

			if ( ((op == LFS_OP_WRITE || op == LFS_OP_APPEND) && mount->_interface->_writeFile == nullptr)
			|| (op == LFS_OP_DELETE && mount->_interface->_deleteFile == nullptr)) {
				*devicePath = nullptr;
				continue;
			} else {
				return &(*mount);
				break;
			}
		}
	}

	return nullptr;
}

void FileContext::releaseWorkItem(WorkItem *workItem) {
	_workItemPool.free(workItem);
}


WorkItem *FileContext::allocWorkItemCommon(const char *path, uint32_t op) {
	WorkItem *item = _workItemPool.alloc();
	
	if (item) {
		item->_operation = static_cast<lfs_file_operation_t>(op);
		item->_filename = path;
		item->_completed = false;
	}
	
	return item;
}

WorkItem *FileContext::readFile(const char *filepath, Allocator *alloc) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_READ);
	
	if (item) {
		item->_allocator = alloc ? alloc : &_alloc;

		_workItemQueue.push(item);
	}

	return item;
}

WorkItem *FileContext::writeFile(const char *filepath, void *buffer, uint64_t bufferBytes) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_WRITE);
	
	if (item) {
		item->_buffer = buffer;
		item->_bufferBytes = bufferBytes;

		_workItemQueue.push(item);
	}

	return item;
}

WorkItem *FileContext::appendFile(const char *filepath, void *buffer, uint64_t bufferBytes) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_APPEND);
	
	if (item) {
		item->_buffer = buffer;
		item->_bufferBytes = bufferBytes;

		_workItemQueue.push(item);
	}

	return item;
}

WorkItem *FileContext::fileExists(const char *filepath) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_EXISTS);
	
	if (item) {
		_workItemQueue.push(item);
	}

	return item;
}

WorkItem *FileContext::fileSize(const char *filepath) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_SIZE);
	
	if (item) {
		_workItemQueue.push(item);
	}

	return item;
}

WorkItem *FileContext::deleteFile(const char *filepath) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_DELETE);
	
	if (item) {
		_workItemQueue.push(item);
	}

	return item;
}


void FileContext::processingFunc(FileContext *ctx) {
	while(ctx->_processing) {
		WorkItem *item = ctx->_workItemQueue.pop(nullptr);
		if (item) {
			switch (item->_operation) {
			case LFS_OP_EXISTS:
			{
				const char *devicePath;
				Mount *mount = ctx->findMountAndPath(item->_filename, &devicePath);
				item->_resultCode = mount ? LFS_OK : LFS_NOT_FOUND;
				break;
			}
			case LFS_OP_SIZE:
			{
				const char *devicePath;
				Mount *mount = ctx->findMountAndPath(item->_filename, &devicePath);
				if (mount) {
					item->_bufferBytes = mount->_interface->_fileSize(mount->_device, devicePath);
					item->_resultCode = LFS_OK;
				} else {
					item->_resultCode = LFS_NOT_FOUND;
				}
				break;
			}
			case LFS_OP_READ:
			{
				const char *devicePath;
				Mount *mount = ctx->findMountAndPath(item->_filename, &devicePath);
				if (mount) {
					item->_bufferBytes = mount->_interface->_readFile(mount->_device, devicePath, item->_allocator, &item->_buffer);
					item->_resultCode = LFS_OK;
				} else {
					item->_resultCode = LFS_NOT_FOUND;
				}
				break;
			}
			case LFS_OP_WRITE:
			case LFS_OP_APPEND:
			{
				const char *devicePath;
				Mount *mount = ctx->findMutableMountAndPath(item->_filename, &devicePath, item->_operation);
				if (mount) {
					item->_bufferBytes = mount->_interface->_writeFile(mount->_device, devicePath, item->_buffer, item->_bufferBytes, item->_operation == LFS_OP_APPEND);
					item->_resultCode = LFS_OK;
				} else {
					item->_resultCode = LFS_UNSUPPORTED;
				}
				break;
			}
			case LFS_OP_DELETE:
			{
				const char *devicePath;
				Mount *mount = ctx->findMutableMountAndPath(item->_filename, &devicePath, item->_operation);
				if (mount) {
					item->_resultCode = mount->_interface->_deleteFile(mount->_device, devicePath);
				} else {
					item->_resultCode = LFS_UNSUPPORTED;
				}
				break;
			}
			};

			item->_completed = true;
			if (item->_callback) {
				item->_callback(item);
			}
		} else {
			// wait for condition variable to notify because queue is empty
		}
	}
}

