// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include "FileContext.h"

#include "device/Directory.h"

#include "platform.h"

#include <algorithm>
#include <atomic>
#include <string.h>
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
	LFS_OP_CREATE_DIR,
	LFS_OP_DELETE_DIR,
};

struct lfs_work_item_t {
	lfs_file_operation_t _operation;
	lfs_work_item_callback_t _callback = nullptr;
	lfs_allocator_t _allocator;

	char *_filename = nullptr;

	void *_buffer = nullptr;
	uint64_t _bufferBytes = 0;

	lfs_error_code_t _resultCode = LFS_OK;
	std::atomic<bool> _completed;
};

void *default_alloc_func(void *, size_t bytes, size_t alignment) {
#ifdef _WIN32
	return _aligned_malloc(bytes, alignment);
#else
	return memalign(alignment, bytes);
#endif
}

void default_free_func(void *, void *ptr) {
#ifdef _WIN32
	_aligned_free(ptr);
#else
	free(ptr);
#endif
}

extern "C" {
	lfs_allocator_t lfs_default_allocator = { default_alloc_func, default_free_func, nullptr };
}

namespace laminaFS {
Allocator DefaultAllocator = lfs_default_allocator;
	
ErrorCode WorkItemGetResult(WorkItem *workItem) {
	return workItem->_resultCode;
}

void *WorkItemGetBuffer(WorkItem *workItem) {
	return workItem->_buffer;
}

void WorkItemFreeBuffer(WorkItem *workItem) {
	workItem->_allocator.free(workItem->_allocator.allocator, workItem->_buffer);
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
: _interfaces(AllocatorAdapter<DeviceInterface*>(alloc))
, _mounts(AllocatorAdapter<MountInfo*>(alloc))
, _workItemPool(alloc, workItemPoolSize)
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
	i._createDir = &DirectoryDevice::createDir;
	i._deleteDir = &DirectoryDevice::deleteDir;

	registerDeviceInterface(i);
	
	_processing = false;
	startProcessingThread();
}

FileContext::~FileContext() {
	stopProcessingThread();

	for (MountInfo *m : _mounts) {
		_alloc.free(_alloc.allocator, m->_prefix);
		m->_interface->_destroy(m->_device);

		m->~MountInfo();
		_alloc.free(_alloc.allocator, m);
	}
	
	for (DeviceInterface *i : _interfaces) {
		i->~DeviceInterface();
		_alloc.free(_alloc.allocator, i);
	}
}

void FileContext::startProcessingThread() {
	if (!_processing) {
		_processing = true;
		_processingThread = std::thread(&FileContext::processingFunc, this);
	}
}

void FileContext::stopProcessingThread() {
	if (_processing) {
		_processing = false;
		_processingThread.join();
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
		result = static_cast<int32_t>(_interfaces.size());
		DeviceInterface *newInterface = new(_alloc.alloc(_alloc.allocator, sizeof(DeviceInterface), alignof(DeviceInterface))) DeviceInterface(interface);
		_interfaces.push_back(newInterface);	
	}

	return result;
}

Mount FileContext::createMount(uint32_t deviceType, const char *mountPoint, const char *devicePath, ErrorCode &resultCode) {
	ErrorCode result = LFS_OK;
	MountInfo *m = new(_alloc.alloc(_alloc.allocator, sizeof(MountInfo), alignof(MountInfo))) MountInfo();

	DeviceInterface *interface = _interfaces[deviceType];
	result = interface->_create(&_alloc, devicePath, &m->_device);
	m->_interface = interface;

	if (result == LFS_OK && m->_device) {
		size_t mountLen = strlen(mountPoint);
		m->_prefix = reinterpret_cast<char*>(_alloc.alloc(_alloc.allocator, sizeof(char) * (mountLen + 1), alignof(char)));
		m->_prefixLen = static_cast<uint32_t>(mountLen);
		strcpy_s(m->_prefix, mountLen + 1, mountPoint);

		_mounts.push_back(m);
		LOG("mounted device %u:%s on %s\n", deviceType, devicePath, mountPoint);
	} else {
		m->~MountInfo();
		_alloc.free(_alloc.allocator, m);
		m = nullptr;
		LOG("unable to mount device %u:%s on %s\n", deviceType, devicePath, mountPoint);
	}
	
	resultCode = result;
	return m;
}

bool FileContext::releaseMount(Mount mount) {
	bool result = false;
	stopProcessingThread();
	
	auto it = std::find(_mounts.begin(), _mounts.end(), mount);
	if (it != _mounts.end()) {
		(*it)->~MountInfo();
		_alloc.free(_alloc.allocator, *it);
		_mounts.erase(it);
		result = true;
	}

	startProcessingThread();
	return result;
}

FileContext::MountInfo* FileContext::findMountAndPath(const char *path, const char **devicePath) {
	LOG("searching for file %s\n", path);

	*devicePath = nullptr;

	// search mounts from the end
	for (auto mount = _mounts.rbegin(); mount != _mounts.rend(); ++mount) {
		const char *foundMount = strstr(path, (*mount)->_prefix);
		if (foundMount && foundMount == path && (path[(*mount)->_prefixLen] == '/' || (*mount)->_prefixLen == 1)) {
			LOG("  found matching mount %s\n", (*mount)->_prefix);

			*devicePath = (*mount)->_prefixLen == 1 ? path : path + (*mount)->_prefixLen;
			bool exists = (*mount)->_interface->_fileExists((*mount)->_device, *devicePath);

			if (!exists) {
				*devicePath = nullptr;
				continue;
			} else {
				return *mount;
				break;
			}
		}
	}
	
	return nullptr;
}

FileContext::MountInfo* FileContext::findMutableMountAndPath(const char *path, const char **devicePath, uint32_t op) {
	LOG("searching for writable mount for %s\n", path);

	*devicePath = nullptr;

	// search mounts from the end
	for (auto mount = _mounts.rbegin(); mount != _mounts.rend(); ++mount) {
		const char *foundMount = strstr(path, (*mount)->_prefix);
		if (foundMount && foundMount == path && (path[(*mount)->_prefixLen] == '/' || (*mount)->_prefixLen == 1)) {
			LOG("  found matching mount %s\n", (*mount)->_prefix);

			*devicePath = (*mount)->_prefixLen == 1 ? path : path + (*mount)->_prefixLen;

			if ( ((op == LFS_OP_WRITE || op == LFS_OP_APPEND) && (*mount)->_interface->_writeFile == nullptr)
			|| (op == LFS_OP_DELETE && (*mount)->_interface->_deleteFile == nullptr)
			|| (op == LFS_OP_CREATE_DIR && (*mount)->_interface->_createDir == nullptr)
			|| (op == LFS_OP_DELETE_DIR && (*mount)->_interface->_deleteDir == nullptr)) {
				*devicePath = nullptr;
				continue;
			} else {
				return *mount;
				break;
			}
		}
	}

	return nullptr;
}

void FileContext::normalizePath(char *path) {
	uint32_t writePos = 0;
	uint32_t readPos = 0;
	uint32_t inputLen = static_cast<uint32_t>(strlen(path));

	while (writePos < inputLen) {
		bool found = false;
		do {
			found = false;
			// handle multiple slashes "//", "///", etc...
			while (path[readPos] == '/' && path[readPos + 1] == '/') {
				++readPos;
				found = true;
			}
		
			// handle parent directory "/.."	
			while (path[readPos] == '/' && path[readPos + 1] == '.' && path[readPos + 2] == '.') {
				readPos += 3;
				while (writePos > 0 && path[writePos - 1] != '/') {
					--writePos;
				}

				if (writePos != 0) {
					--writePos;
				}
				found = true;
			}

			// handle "this" directory "/."	
			while (path[readPos] == '/' && path[readPos + 1] == '.' && path[readPos + 2] != '.') {
				readPos += 2;
				found = true;
			}
		} while (found);

		path[writePos] = path[readPos];

		if (path[writePos] == 0) {
			break;
		}

		++writePos;
		++readPos;
	}
	
	// remove trailing slash
	if (writePos > 1 && path[writePos - 1] == '/') {
		path[writePos - 1] = 0;
	}
	
	// fixup root slash
	if (path[0] == 0 && inputLen >= 1) {
		path[0] = '/';
		path[1] = 0;
	}

}

void FileContext::releaseWorkItem(WorkItem *workItem) {
	_alloc.free(_alloc.allocator, workItem->_filename);
	_workItemPool.free(workItem);
}

WorkItem *FileContext::allocWorkItemCommon(const char *path, uint32_t op, WorkItemCallback callback) {
	WorkItem *item = _workItemPool.alloc();

	if (item) {
		item->_operation = static_cast<lfs_file_operation_t>(op);
		
		size_t pathLen = strlen(path) + 1;
		char *normalizedPath = reinterpret_cast<char*>(_alloc.alloc(_alloc.allocator, sizeof(char) * pathLen, alignof(char)));
		strcpy_s(normalizedPath, pathLen, path);
		normalizePath(normalizedPath);

		item->_filename = normalizedPath;
		
		item->_callback = callback;
		item->_completed = false;
	}
	
	return item;
}

WorkItem *FileContext::readFile(const char *filepath, Allocator *alloc, WorkItemCallback callback) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_READ, callback);

	if (item) {
		item->_allocator = alloc ? *alloc : _alloc;

		_workItemQueue.push(item);
	}

	return item;
}

WorkItem *FileContext::writeFile(const char *filepath, void *buffer, uint64_t bufferBytes, WorkItemCallback callback) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_WRITE, callback);

	if (item) {
		item->_buffer = buffer;
		item->_bufferBytes = bufferBytes;

		_workItemQueue.push(item);
	}

	return item;
}

WorkItem *FileContext::appendFile(const char *filepath, void *buffer, uint64_t bufferBytes, WorkItemCallback callback) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_APPEND, callback);

	if (item) {
		item->_buffer = buffer;
		item->_bufferBytes = bufferBytes;

		_workItemQueue.push(item);
	}

	return item;
}

WorkItem *FileContext::fileExists(const char *filepath, WorkItemCallback callback) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_EXISTS, callback);

	if (item) {
		_workItemQueue.push(item);
	}

	return item;
}

WorkItem *FileContext::fileSize(const char *filepath, WorkItemCallback callback) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_SIZE, callback);

	if (item) {
		_workItemQueue.push(item);
	}

	return item;
}

WorkItem *FileContext::deleteFile(const char *filepath, WorkItemCallback callback) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_DELETE, callback);

	if (item) {
		_workItemQueue.push(item);
	}

	return item;
}

WorkItem *FileContext::createDir(const char *path, WorkItemCallback callback) {
	WorkItem *item = allocWorkItemCommon(path, LFS_OP_CREATE_DIR, callback);

	if (item) {
		_workItemQueue.push(item);
	}

	return item;
}

WorkItem *FileContext::deleteDir(const char *path, WorkItemCallback callback) {
	WorkItem *item = allocWorkItemCommon(path, LFS_OP_DELETE_DIR, callback);

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
				MountInfo *mount = ctx->findMountAndPath(item->_filename, &devicePath);
				item->_resultCode = mount ? LFS_OK : LFS_NOT_FOUND;
				break;
			}
			case LFS_OP_SIZE:
			{
				const char *devicePath;
				MountInfo *mount = ctx->findMountAndPath(item->_filename, &devicePath);
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
				MountInfo *mount = ctx->findMountAndPath(item->_filename, &devicePath);
				if (mount) {
					item->_bufferBytes = mount->_interface->_readFile(mount->_device, devicePath, &item->_allocator, &item->_buffer);
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
				MountInfo *mount = ctx->findMutableMountAndPath(item->_filename, &devicePath, item->_operation);
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
				MountInfo *mount = ctx->findMutableMountAndPath(item->_filename, &devicePath, item->_operation);
				if (mount) {
					item->_resultCode = mount->_interface->_deleteFile(mount->_device, devicePath);
				} else {
					item->_resultCode = LFS_UNSUPPORTED;
				}
				break;
			}
			case LFS_OP_CREATE_DIR:
			{
				const char *devicePath;
				MountInfo *mount = ctx->findMutableMountAndPath(item->_filename, &devicePath, item->_operation);
				if (mount) {
					item->_resultCode = mount->_interface->_createDir(mount->_device, devicePath);
				} else {
					item->_resultCode = LFS_UNSUPPORTED;
				}
				break;
			}
			case LFS_OP_DELETE_DIR:
			{
				const char *devicePath;
				MountInfo *mount = ctx->findMutableMountAndPath(item->_filename, &devicePath, item->_operation);
				if (mount) {
					item->_resultCode = mount->_interface->_deleteDir(mount->_device, devicePath);
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

