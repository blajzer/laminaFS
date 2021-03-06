// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "FileContext.h"

#if !defined(LAMINAFS_DISABLE_DIRECTORY_DEVICE)
#include "device/Directory.h"
#endif

#include <algorithm>
#include <atomic>
#include <string.h>
#include <stdlib.h>

#ifdef __linux__
#include <malloc.h>
#endif

using namespace laminaFS;

#define LOG(MSG,...) if (_log) { _log(MSG, ##__VA_ARGS__); }

enum lfs_file_operation_t {
	LFS_OP_EXISTS,
	LFS_OP_SIZE,
	LFS_OP_READ,
	LFS_OP_WRITE,
	LFS_OP_APPEND,
	LFS_OP_WRITE_SEGMENT,
	LFS_OP_DELETE,
	LFS_OP_CREATE_DIR,
	LFS_OP_DELETE_DIR,
};

struct lfs_work_item_t {
	lfs_file_operation_t _operation;
	lfs_work_item_callback_t _callback = nullptr;
	void *_callbackUserData = nullptr;
	lfs_allocator_t _allocator;
	FileContext* _context = nullptr;

	char *_filename = nullptr;

	void *_buffer = nullptr;
	uint64_t _bufferBytes = 0;
	uint64_t _offset = 0;

	lfs_callback_buffer_action_t _callbackBufferAction;

	lfs_error_code_t _resultCode = LFS_OK;
	bool _nullTerminate = false;
	bool _completed = false;
};

void *default_alloc_func(void *, size_t bytes, size_t alignment) {
#ifdef _WIN32
	return _aligned_malloc(bytes, alignment);
#elif __APPLE__
	void *ptr = nullptr;
	// XXX: macOS requires sizeof(void*)  multiples for alignment
	size_t aligned_alignment = (alignment + sizeof(void*) - 1) & (~(sizeof(void*) - 1));
	posix_memalign(&ptr, aligned_alignment, bytes);
	return ptr;
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

ErrorCode WorkItemGetResult(const WorkItem *workItem) {
	if (workItem) {
		return workItem->_resultCode;
	} else {
		// No work item? Probably a failure to allocate.
		return LFS_OUT_OF_WORK_ITEMS;
	}
}

void *WorkItemGetBuffer(const WorkItem *workItem) {
	if (workItem) {
		std::atomic_thread_fence(std::memory_order_acquire);
		return workItem->_buffer;
	} else {
		return nullptr;
	}
}

void WorkItemFreeBuffer(WorkItem *workItem) {
	if (workItem && workItem->_buffer) {
		workItem->_allocator.free(workItem->_allocator.allocator, workItem->_buffer);
		workItem->_buffer = nullptr;
		std::atomic_thread_fence(std::memory_order_release);
	}
}

uint64_t WorkItemGetBytes(const WorkItem *workItem) {
	if (workItem) {
		return workItem->_bufferBytes;
	} else {
		return 0;
	}
}

bool WorkItemCompleted(const WorkItem *workItem) {
	if (workItem && !workItem->_callback) {
		std::unique_lock<std::mutex> lock(workItem->_context->getCompletionMutex());
		return workItem->_completed;
	} else {
		return true;
	}
}

void WaitForWorkItem(const WorkItem *workItem) {
	if (workItem && !workItem->_callback) {
		std::unique_lock<std::mutex> lock(workItem->_context->getCompletionMutex());
		while (!workItem->_completed) {
			workItem->_context->getCompletionConditionVariable().wait(lock, [workItem]{ return workItem->_completed; });
		}
	}
}

}

FileContext::FileContext(Allocator &alloc, uint64_t maxQueuedWorkItems, uint64_t workItemPoolSize)
: _interfaces(AllocatorAdapter<DeviceInterface*>(alloc))
, _mounts(AllocatorAdapter<MountInfo*>(alloc))
, _workItemPool(alloc, workItemPoolSize)
, _workItemQueueSemaphore()
, _workItemQueue(alloc, maxQueuedWorkItems, &_workItemQueueSemaphore)
, _alloc(alloc)
{
#if !defined(LAMINAFS_DISABLE_DIRECTORY_DEVICE)
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
#endif

	_processing = false;
	startProcessingThread();
}

FileContext::~FileContext() {
	stopProcessingThread();

	_mountLock.lock();
	for (MountInfo *m : _mounts) {
		_alloc.free(_alloc.allocator, m->_prefix);
		m->_interface->_destroy(m->_device);

		m->~MountInfo();
		_alloc.free(_alloc.allocator, m);
	}
	_mountLock.unlock();

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
		_workItemQueueSemaphore.notify();
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

Mount FileContext::createMount(uint32_t deviceType, const char *mountPoint, const char *devicePath, ErrorCode &resultCode, uint32_t mountPermissions) {
	if (deviceType >= _interfaces.size()) {
		resultCode = LFS_INVALID_DEVICE;
		return nullptr;
	}

	DeviceInterface *interface = _interfaces[deviceType];

	// check permissions against device interface's supported permissions
	uint32_t supportedPermissions = LFS_MOUNT_READ |
		(interface->_writeFile != nullptr ? LFS_MOUNT_WRITE_FILE : 0) |
		(interface->_deleteFile != nullptr ? LFS_MOUNT_DELETE_FILE : 0) |
		(interface->_createDir != nullptr ? LFS_MOUNT_CREATE_DIR : 0) |
		(interface->_deleteDir != nullptr ? LFS_MOUNT_DELETE_DIR : 0);

	uint32_t calculatedPermissions = mountPermissions == LFS_MOUNT_DEFAULT ? supportedPermissions : (mountPermissions & supportedPermissions);
	if (calculatedPermissions == 0) {
		resultCode = LFS_PERMISSIONS_ERROR;
		return nullptr;
	}

	ErrorCode result = LFS_OK;
	MountInfo *m = new(_alloc.alloc(_alloc.allocator, sizeof(MountInfo), alignof(MountInfo))) MountInfo();

	result = interface->_create(&_alloc, devicePath, &m->_device);
	m->_interface = interface;

	if (result == LFS_OK && m->_device) {
		size_t mountLen = strlen(mountPoint);
		m->_prefix = reinterpret_cast<char*>(_alloc.alloc(_alloc.allocator, sizeof(char) * (mountLen + 1), alignof(char)));
		m->_prefixLen = static_cast<uint32_t>(mountLen);
		strcpy(m->_prefix, mountPoint);
		m->_permissions = calculatedPermissions;

		_mountLock.lock();
		_mounts.push_back(m);
		_mountLock.unlock();
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

	_mountLock.lock();
	auto it = std::find(_mounts.begin(), _mounts.end(), mount);
	if (it != _mounts.end()) {
		(*it)->_interface->_destroy((*it)->_device);
		_alloc.free(_alloc.allocator, (*it)->_prefix);

		(*it)->~MountInfo();
		_alloc.free(_alloc.allocator, *it);

		_mounts.erase(it);
		result = true;
	}
	_mountLock.unlock();

	startProcessingThread();
	return result;
}

FileContext::MountInfo* FileContext::findNextMountAndPath(const char *path, const char **devicePath, const MountInfo* searchStart) {
	*devicePath = nullptr;
	const MountInfo* onlyAfter = searchStart;

	// search mounts from the end
	for (auto mount = _mounts.rbegin(); mount != _mounts.rend(); ++mount) {
		if (((*mount)->_permissions & LFS_MOUNT_READ) == 0)
			continue;
		if (onlyAfter) {
			if (*mount == onlyAfter)
				onlyAfter = nullptr;
			continue;
		}

		const char *foundMount = strstr(path, (*mount)->_prefix);
		if (foundMount && foundMount == path && (path[(*mount)->_prefixLen] == '/' || (*mount)->_prefixLen == 1)) {
			*devicePath = (*mount)->_prefixLen == 1 ? path : path + (*mount)->_prefixLen;
			return *mount;
		}
	}

	*devicePath = nullptr;
	return nullptr;
}

FileContext::MountInfo* FileContext::findMutableMountAndPath(const char *path, const char **devicePath, uint32_t op) {
	LOG("searching for writable mount for %s\n", path);

	*devicePath = nullptr;

	std::shared_lock<std::shared_mutex> lock(_mountLock);

	// search mounts from the end
	for (auto mount = _mounts.rbegin(); mount != _mounts.rend(); ++mount) {
		const char *foundMount = strstr(path, (*mount)->_prefix);
		if (foundMount && foundMount == path && (path[(*mount)->_prefixLen] == '/' || (*mount)->_prefixLen == 1)) {
			LOG("  found matching mount %s\n", (*mount)->_prefix);

			*devicePath = (*mount)->_prefixLen == 1 ? path : path + (*mount)->_prefixLen;

			if ( ((op == LFS_OP_WRITE || op == LFS_OP_APPEND) && (((*mount)->_permissions & LFS_MOUNT_WRITE_FILE) == 0))
			|| (op == LFS_OP_DELETE && (((*mount)->_permissions & LFS_MOUNT_DELETE_FILE) == 0))
			|| (op == LFS_OP_CREATE_DIR && (((*mount)->_permissions & LFS_MOUNT_CREATE_DIR) == 0))
			|| (op == LFS_OP_DELETE_DIR && (((*mount)->_permissions & LFS_MOUNT_DELETE_DIR) == 0))) {
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

			if (path[readPos] == '/') {
				if (path[readPos + 1] == '/') {
					// handle multiple slashes "//", "///", etc...
					++readPos;
					found = true;
				} else if (path[readPos + 1] == '.') {
					if (path[readPos + 2] == '.' && (path[readPos + 3] == 0 || path[readPos + 3] == '/')) {
						// handle parent directory "/.."
						readPos += 3;
						while (writePos > 0 && path[writePos - 1] != '/') {
							--writePos;
						}

						if (writePos != 0) {
							--writePos;
						}
						found = true;
					} else if (path[readPos + 2] == '/' || path[readPos + 2] == 0) {
						// handle "this" directory "/."
						readPos += 2;
						found = true;
					}
				}
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
	if (workItem && !workItem->_callback) {
		releaseWorkItemInternal(workItem);
	}
}

void FileContext::releaseWorkItemInternal(WorkItem *workItem) {
	if (workItem) {
		_alloc.free(_alloc.allocator, workItem->_filename);
		_workItemPool.free(workItem);
	}
}

void FileContext::initWorkItem(WorkItem *item, const char *path, uint32_t op, WorkItemCallback callback, void *callbackUserData, CallbackBufferAction bufferAction) {
	item->_operation = static_cast<lfs_file_operation_t>(op);

	size_t pathLen = strlen(path) + 1;
	char *normalizedPath = reinterpret_cast<char*>(_alloc.alloc(_alloc.allocator, sizeof(char) * pathLen, alignof(char)));
	strcpy(normalizedPath, path);
	normalizePath(normalizedPath);

	item->_filename = normalizedPath;

	item->_callback = callback;
	item->_callbackUserData = callbackUserData;
	item->_callbackBufferAction = bufferAction;
	item->_completed = false;
	item->_context = this;
}

WorkItem *FileContext::allocWorkItemCommon(const char *path, uint32_t op, WorkItemCallback callback, void *callbackUserData, CallbackBufferAction bufferAction) {
	WorkItem *item = _workItemPool.alloc();

	if (item) {
		initWorkItem(item, path, op, callback, callbackUserData, bufferAction);
	} else {
		LOG("error: unable to allocate work item, work item pool capacity was %u", static_cast<uint32_t>(_workItemPool.getCapacity()));

		// We'll assume we want a callback with an error here.
		if (callback) {
			WorkItem errorItem;
			initWorkItem(&errorItem, path, op, callback, callbackUserData, bufferAction);
			errorItem._completed = true;
			errorItem._resultCode = LFS_OUT_OF_WORK_ITEMS;

			callback(&errorItem, callbackUserData);

			_alloc.free(_alloc.allocator, errorItem._filename);
		}
	}

	return item;
}

WorkItem *FileContext::readFile(const char *filepath, bool nullTerminate, Allocator *alloc) {
	return readFileSegment(filepath, 0, static_cast<uint64_t>(-1), nullTerminate, alloc);
}

WorkItem *FileContext::readFileSegment(const char *filepath, uint64_t offset, uint64_t maxBytes, bool nullTerminate, Allocator *alloc) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_READ, nullptr, nullptr, LFS_DO_NOT_FREE_BUFFER);

	if (item) {
		item->_allocator = alloc ? *alloc : _alloc;
		item->_nullTerminate = nullTerminate;
		item->_bufferBytes = maxBytes;
		item->_offset = offset;

		_workItemQueue.push(item);
	}

	return item;
}

void FileContext::readFileWithCallback(const char *filepath, bool nullTerminate, WorkItemCallback callback, CallbackBufferAction bufferAction, void *callbackUserData, Allocator *alloc) {
	readFileSegmentWithCallback(filepath, 0, static_cast<uint64_t>(-1), nullTerminate, callback, bufferAction, callbackUserData, alloc);
}

void FileContext::readFileSegmentWithCallback(const char *filepath, uint64_t offset, uint64_t maxBytes, bool nullTerminate, WorkItemCallback callback, CallbackBufferAction bufferAction, void *callbackUserData, Allocator *alloc) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_READ, callback, callbackUserData, bufferAction);

	if (item) {
		item->_allocator = alloc ? *alloc : _alloc;
		item->_nullTerminate = nullTerminate;
		item->_bufferBytes = maxBytes;
		item->_offset = offset;

		_workItemQueue.push(item);
	}
}

WorkItem *FileContext::writeFile(const char *filepath, const void *buffer, uint64_t bufferBytes) {
	WorkItem *item = writeFileSegment(filepath, 0, buffer, bufferBytes);

	if (item) {
		item->_operation = LFS_OP_WRITE;
	}

	return item;
}

WorkItem *FileContext::writeFileSegment(const char *filepath, uint64_t offset, const void *buffer, uint64_t bufferBytes) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_WRITE_SEGMENT, nullptr, nullptr, LFS_DO_NOT_FREE_BUFFER);

	if (item) {
		item->_buffer = const_cast<void*>(buffer);
		item->_bufferBytes = bufferBytes;
		item->_offset = offset;

		_workItemQueue.push(item);
	}

	return item;
}

void FileContext::writeFileWithCallback(const char *filepath, const void *buffer, uint64_t bufferBytes, WorkItemCallback callback, CallbackBufferAction bufferAction, void *callbackUserData) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_WRITE, callback, callbackUserData, bufferAction);

	if (item) {
		item->_buffer = const_cast<void*>(buffer);
		item->_bufferBytes = bufferBytes;
		item->_offset = 0;

		_workItemQueue.push(item);
	}
}

void FileContext::writeFileSegmentWithCallback(const char *filepath, uint64_t offset, const void *buffer, uint64_t bufferBytes, WorkItemCallback callback, CallbackBufferAction bufferAction, void *callbackUserData) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_WRITE_SEGMENT, callback, callbackUserData, bufferAction);

	if (item) {
		item->_buffer = const_cast<void*>(buffer);
		item->_bufferBytes = bufferBytes;
		item->_offset = offset;

		_workItemQueue.push(item);
	}
}

WorkItem *FileContext::appendFile(const char *filepath, const void *buffer, uint64_t bufferBytes) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_APPEND, nullptr, nullptr, LFS_DO_NOT_FREE_BUFFER);

	if (item) {
		item->_buffer = const_cast<void*>(buffer);
		item->_bufferBytes = bufferBytes;

		_workItemQueue.push(item);
	}

	return item;
}

void FileContext::appendFileWithCallback(const char *filepath, const void *buffer, uint64_t bufferBytes, WorkItemCallback callback, CallbackBufferAction bufferAction, void *callbackUserData) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_APPEND, callback, callbackUserData, bufferAction);

	if (item) {
		item->_buffer = const_cast<void*>(buffer);
		item->_bufferBytes = bufferBytes;

		_workItemQueue.push(item);
	}
}

WorkItem *FileContext::fileExists(const char *filepath) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_EXISTS, nullptr, nullptr, LFS_DO_NOT_FREE_BUFFER);

	if (item) {
		_workItemQueue.push(item);
	}

	return item;
}

void FileContext::fileExistsWithCallback(const char *filepath, WorkItemCallback callback, void *callbackUserData) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_EXISTS, callback, callbackUserData, LFS_DO_NOT_FREE_BUFFER);

	if (item) {
		_workItemQueue.push(item);
	}
}

WorkItem *FileContext::fileSize(const char *filepath) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_SIZE, nullptr, nullptr, LFS_DO_NOT_FREE_BUFFER);

	if (item) {
		_workItemQueue.push(item);
	}

	return item;
}

void FileContext::fileSizeWithCallback(const char *filepath, WorkItemCallback callback, void *callbackUserData) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_SIZE, callback, callbackUserData, LFS_DO_NOT_FREE_BUFFER);

	if (item) {
		_workItemQueue.push(item);
	}
}

WorkItem *FileContext::deleteFile(const char *filepath) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_DELETE, nullptr, nullptr, LFS_DO_NOT_FREE_BUFFER);

	if (item) {
		_workItemQueue.push(item);
	}

	return item;
}

void FileContext::deleteFileWithCallback(const char *filepath, WorkItemCallback callback, void *callbackUserData) {
	WorkItem *item = allocWorkItemCommon(filepath, LFS_OP_DELETE, callback, callbackUserData, LFS_DO_NOT_FREE_BUFFER);

	if (item) {
		_workItemQueue.push(item);
	}
}

WorkItem *FileContext::createDir(const char *path) {
	WorkItem *item = allocWorkItemCommon(path, LFS_OP_CREATE_DIR, nullptr, nullptr, LFS_DO_NOT_FREE_BUFFER);

	if (item) {
		_workItemQueue.push(item);
	}

	return item;
}

void FileContext::createDirWithCallback(const char *path, WorkItemCallback callback, void *callbackUserData) {
	WorkItem *item = allocWorkItemCommon(path, LFS_OP_CREATE_DIR, callback, callbackUserData, LFS_DO_NOT_FREE_BUFFER);

	if (item) {
		_workItemQueue.push(item);
	}
}

WorkItem *FileContext::deleteDir(const char *path) {
	WorkItem *item = allocWorkItemCommon(path, LFS_OP_DELETE_DIR, nullptr, nullptr, LFS_DO_NOT_FREE_BUFFER);

	if (item) {
		_workItemQueue.push(item);
	}

	return item;
}

void FileContext::deleteDirWithCallback(const char *path, WorkItemCallback callback, void *callbackUserData) {
	WorkItem *item = allocWorkItemCommon(path, LFS_OP_DELETE_DIR, callback, callbackUserData, LFS_DO_NOT_FREE_BUFFER);

	if (item) {
		_workItemQueue.push(item);
	}
}

void FileContext::processingFunc(FileContext *ctx) {
	while(ctx->_processing) {
		WorkItem *item = ctx->_workItemQueue.pop(nullptr);
		if (item) {
			switch (item->_operation) {
			case LFS_OP_EXISTS:
			{
				const char *devicePath;
				MountInfo *mount = nullptr;
				item->_resultCode = LFS_NOT_FOUND;
				while ((mount = ctx->findNextMountAndPath(item->_filename, &devicePath, mount)) != nullptr) {
					bool exists = mount->_interface->_fileExists(mount->_device, devicePath);
					if (exists) {
						item->_resultCode = LFS_OK;
						break;
					}
				}
				break;
			}
			case LFS_OP_SIZE:
			{
				const char *devicePath;
				MountInfo *mount = nullptr;
				item->_resultCode = LFS_NOT_FOUND;
				item->_bufferBytes = 0;
				while ((mount = ctx->findNextMountAndPath(item->_filename, &devicePath, mount)) != nullptr) {
					item->_bufferBytes = mount->_interface->_fileSize(mount->_device, devicePath, &item->_resultCode);
					if (item->_resultCode != LFS_NOT_FOUND) {
						break;
					}
				}
				break;
			}
			case LFS_OP_READ:
			{
				const char *devicePath;
				MountInfo *mount = nullptr;
				item->_resultCode = LFS_NOT_FOUND;
				size_t maxBytes = item->_bufferBytes;
				item->_bufferBytes = 0;
				while ((mount = ctx->findNextMountAndPath(item->_filename, &devicePath, mount)) != nullptr) {
					item->_bufferBytes = mount->_interface->_readFile(mount->_device, devicePath, item->_offset, maxBytes, &item->_allocator, &item->_buffer, item->_nullTerminate, &item->_resultCode);
					if (item->_resultCode != LFS_NOT_FOUND) {
						break;
					}
				}
				break;
			}
			case LFS_OP_WRITE:
			case LFS_OP_WRITE_SEGMENT:
			case LFS_OP_APPEND:
			{
				const char *devicePath;
				MountInfo *mount = ctx->findMutableMountAndPath(item->_filename, &devicePath, item->_operation);
				if (mount) {
					lfs_write_mode_t writeMode = LFS_WRITE_TRUNCATE;
					switch (item->_operation) {
					case LFS_OP_WRITE:
						writeMode = LFS_WRITE_TRUNCATE;
						break;
					case LFS_OP_WRITE_SEGMENT:
						writeMode = LFS_WRITE_SEGMENT;
						break;
					case LFS_OP_APPEND:
						writeMode = LFS_WRITE_APPEND;
						break;
					default:
						break;
					}

					item->_bufferBytes = mount->_interface->_writeFile(mount->_device, devicePath, item->_offset, item->_buffer, item->_bufferBytes, writeMode, &item->_resultCode);
				} else {
					item->_bufferBytes = 0;
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

			{
				std::unique_lock<std::mutex> lock(ctx->getCompletionMutex());
				item->_completed = true;
			}

			if (item->_callback) {
				item->_callback(item, item->_callbackUserData);

				if (item->_callbackBufferAction == LFS_FREE_BUFFER) {
					WorkItemFreeBuffer(item);
				}

				ctx->releaseWorkItemInternal(item);
			} else {
				ctx->getCompletionConditionVariable().notify_all();
			}

		} else {
			ctx->_workItemQueueSemaphore.wait();
		}
	}
}
