#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <thread>

#include "shared_types.h"
#include "util/PoolAllocator.h"
#include "util/RingBuffer.h"
#include "util/Semaphore.h"

namespace laminaFS {

typedef void* FileHandle;
typedef lfs_error_code_t ErrorCode;
typedef lfs_work_item_t WorkItem;
typedef lfs_allocator_t Allocator;
typedef lfs_work_item_callback_t WorkItemCallback;
typedef lfs_callback_buffer_action_t CallbackBufferAction;
typedef void* Mount;

extern Allocator DefaultAllocator;

// C++ Allocator Adapter
template <class T>
struct AllocatorAdapter {
	typedef T value_type;
	AllocatorAdapter() noexcept : _alloc(DefaultAllocator) {}
	AllocatorAdapter(Allocator &alloc) noexcept : _alloc(alloc) {}

	template <class U>
	AllocatorAdapter(const AllocatorAdapter<U>& other) noexcept {
		_alloc = other._alloc;
	}

	T* allocate(std::size_t n) {
		return static_cast<T*>(_alloc.alloc(_alloc.allocator, sizeof(T) * n, alignof(T)));
	}

	void deallocate(T* p, std::size_t) {
		_alloc.free(_alloc.allocator, p);
	}

	Allocator _alloc;
};

template <class T, class U>
bool operator==(const AllocatorAdapter<T> &a, const AllocatorAdapter<U> &b) {
	return a._alloc == b._alloc;
}

template <class T, class U>
bool operator!=(const AllocatorAdapter<T> &a, const AllocatorAdapter<U> &b) {
	return a._alloc != b._alloc;
}

//! Gets the result code from a WorkItem
//! @param workItem the WorkItem
//! @return the result code; nullptr is assumed to mean the work item failed allocation
extern ErrorCode WorkItemGetResult(const WorkItem *workItem);

//! Gets the output buffer from a WorkItem.
//! @param workItem the WorkItem
//! @return the output buffer, or nullptr if no WorkItem
extern void *WorkItemGetBuffer(const WorkItem *workItem);

//! Gets the bytes read/written from a WorkItem.
//! @param workItem the WorkItem
//! @return the bytes read/written, or 0 if no WorkItem
extern uint64_t WorkItemGetBytes(const WorkItem *workItem);

//! Frees the output buffer that was allocated by the work item.
//! @param workItem the WorkItem
extern void WorkItemFreeBuffer(WorkItem *workItem);

//! Whether or not a work item has completed processing.
//! @param workItem the WorkItem to query
//! @eturn true if finished process (or for nullptr WorkItem), false otherwise
extern bool WorkItemCompleted(const WorkItem *workItem);

//! Waits for a WorkItem to finish processing.
//! @param workItem the WorkItem to wait for
extern void WaitForWorkItem(const WorkItem *workItem);


//! FileContext is the "main" object in LaminaFS. It handles management of files,
//! mounts, and the backend processing that occurs. There is an internal thread
//! that does the processing for the work items.
//!
//! There are two methods of work item ownership:
//! 1. No callback is provided. In this case the client thread is responsible for calling FileContext::releaseWorkItem().
//!    The work item must be complete before FileContext::releaseWorkItem() can be called. Use WaitForWorkItem() to ensure completion.
//! 2. Callback is provided. The caller pre-requests that the procesing thread free any internal buffer, or have client code
//!    free it sometime later. The processing thread owns and will free the work item.
//!
class FileContext {
public:
	FileContext(Allocator &alloc, uint64_t maxQueuedWorkItems = 128, uint64_t workItemPoolSize = 1024);
	~FileContext();

	typedef int (*LogFunc)(const char *, ...);

	//! DeviceInterface is the extensibility mechanism for adding new types of
	//! devices. Fill out one of these and then call registerDeviceInterface().
	struct DeviceInterface {
		typedef ErrorCode (*CreateFunc)(lfs_allocator_t *, const char *, void **);
		typedef void (*DestroyFunc)(void*);

		typedef bool (*FileExistsFunc)(void *, const char *);
		typedef size_t (*FileSizeFunc)(void *, const char *, ErrorCode *);
		typedef size_t (*ReadFileFunc)(void *, const char *, uint64_t, uint64_t, lfs_allocator_t *, void **, bool, ErrorCode *);

		typedef size_t (*WriteFileFunc)(void *, const char *, uint64_t, void *, size_t, lfs_write_mode_t, ErrorCode *);
		typedef ErrorCode (*DeleteFileFunc)(void *, const char *);

		typedef ErrorCode (*CreateDirFunc)(void *, const char *);
		typedef ErrorCode (*DeleteDirFunc)(void *, const char *);

		// required
		CreateFunc _create = nullptr;
		DestroyFunc _destroy = nullptr;

		FileExistsFunc _fileExists = nullptr;
		FileSizeFunc _fileSize = nullptr;
		ReadFileFunc _readFile = nullptr;

		// optional
		WriteFileFunc _writeFile = nullptr;
		DeleteFileFunc _deleteFile = nullptr;
		CreateDirFunc _createDir = nullptr;
		DeleteDirFunc _deleteDir = nullptr;
	};

	//! Registers a new device interface.
	//! @param interface the interface specification
	//! @return the device type index, used for passing to createMount() or -1 on error
	int32_t registerDeviceInterface(DeviceInterface &interface);

	//! Creates a new mount.
	//! @param deviceType the device type, as returned by registerDeviceInterface()
	//! @param mountPoint the virtual path to mount this device to
	//! @param devicePath the path to pass into the device
	//! @param returnCode the return code
	//! @param mountPermissions the permissions to create the mount with
	//! @return the mount
	Mount createMount(uint32_t deviceType, const char *mountPoint, const char *devicePath, ErrorCode &returnCode, uint32_t mountPermissions = LFS_MOUNT_DEFAULT);

	//! Releases a mount.
	//! Finishes all processing work items and suspends processing while it runs.
	//! @param mount the mount to remove
	//! @return whether or not the mount was found and removed
	bool releaseMount(Mount mount);

	//! Reads the entirety of a file.
	//! @param filepath the path to the file to read
	//! @param nullTerminate whether or not to add a NULL to the end of the buffer so it can be directly used as a C-string.
	//! @param alloc the allocator to use. If NULL will use the context's allocator.
	//! @return a WorkItem representing the work to be done
	WorkItem *readFile(const char *filepath, bool nullTerminate, Allocator *alloc = nullptr);

	//! Reads the entirety of a file.
	//! @param filepath the path to the file to read
	//! @param nullTerminate whether or not to add a NULL to the end of the buffer so it can be directly used as a C-string.
	//! @param callback callback
	//! @param bufferAction what to do with the buffer after the callback completes execution
	//! @param callbackUserData user data pointer for callback
	//! @param alloc the allocator to use. If NULL will use the context's allocator.
	void readFileWithCallback(const char *filepath, bool nullTerminate, WorkItemCallback callback, CallbackBufferAction bufferAction, void *callbackUserData = nullptr, Allocator *alloc = nullptr);

	//! Reads a portion of a file.
	//! @param filepath the path to the file to read
	//! @param offset the offset to start reading from
	//! @param maxBytes the maximum number of bytes to read
	//! @param nullTerminate whether or not to add a NULL to the end of the buffer so it can be directly used as a C-string.
	//! @param alloc the allocator to use. If NULL will use the context's allocator.
	//! @return a WorkItem representing the work to be done
	WorkItem *readFileSegment(const char *filepath, uint64_t offset, uint64_t maxBytes, bool nullTerminate, Allocator *alloc = nullptr);

	//! Reads a portion of a file.
	//! @param filepath the path to the file to read
	//! @param offset the offset to start reading from
	//! @param maxBytes the maximum number of bytes to read
	//! @param nullTerminate whether or not to add a NULL to the end of the buffer so it can be directly used as a C-string.
	//! @param callback callback
	//! @param bufferAction what to do with the buffer after the callback completes execution
	//! @param callbackUserData optional user data pointer for callback
	//! @param alloc the allocator to use. If NULL will use the context's allocator.
	void readFileSegmentWithCallback(const char *filepath, uint64_t offset, uint64_t maxBytes, bool nullTerminate, WorkItemCallback callback, CallbackBufferAction bufferAction, void *callbackUserData = nullptr, Allocator *alloc = nullptr);

	//! Writes a buffer to a file.
	//! @param filepath the path to the file to write
	//! @param buffer the buffer to write
	//! @param bufferBytes the number of bytes to write to the buffer
	//! @return a WorkItem representing the work to be done
	WorkItem *writeFile(const char *filepath, const void *buffer, uint64_t bufferBytes);

	//! Writes a buffer to a file.
	//! @param filepath the path to the file to write
	//! @param buffer the buffer to write
	//! @param bufferBytes the number of bytes to write to the buffer
	//! @param callback callback
	//! @param bufferAction what to do with the buffer after the callback completes execution
	//! @param callbackUserData optional user data pointer for callback
	void writeFileWithCallback(const char *filepath, const void *buffer, uint64_t bufferBytes, WorkItemCallback callback, CallbackBufferAction bufferAction, void *callbackUserData = nullptr);

	//! Writes a buffer to a given offset in a file.
	//! @param filepath the path to the file to write
	//! @param offset the offset to write to
	//! @param buffer the buffer to write
	//! @param bufferBytes the number of bytes to write to the buffer
	//! @return a WorkItem representing the work to be done
	WorkItem *writeFileSegment(const char *filepath, uint64_t offset, const void *buffer, uint64_t bufferBytes);

	//! Writes a buffer to a given offset in a file.
	//! @param filepath the path to the file to write
	//! @param offset the offset to write to
	//! @param buffer the buffer to write
	//! @param bufferBytes the number of bytes to write to the buffer
	//! @param callback callback
	//! @param bufferAction what to do with the buffer after the callback completes execution
	//! @param callbackUserData optional user data pointer for callback
	void writeFileSegmentWithCallback(const char *filepath, uint64_t offset, const void *buffer, uint64_t bufferBytes, WorkItemCallback callback, CallbackBufferAction bufferAction, void *callbackUserData = nullptr);

	//! Appends a buffer to a file.
	//! @param filepath the path to the file to append
	//! @param buffer the buffer to write
	//! @param bufferBytes the number of bytes to write to the buffer
	//! @return a WorkItem representing the work to be done
	WorkItem *appendFile(const char *filepath, const void *buffer, uint64_t bufferBytes);

	//! Appends a buffer to a file.
	//! @param filepath the path to the file to append
	//! @param buffer the buffer to write
	//! @param bufferBytes the number of bytes to write to the buffer
	//! @param callback callback
	//! @param bufferAction what to do with the buffer after the callback completes execution
	//! @param callbackUserData optional user data pointer for callback
	void appendFileWithCallback(const char *filepath, const void *buffer, uint64_t bufferBytes, WorkItemCallback callback, CallbackBufferAction bufferAction, void *callbackUserData = nullptr);

	//! Determines if a file exists.
	//! @param filepath the path to the file to delete
	//! @return a WorkItem representing the work to be done
	WorkItem *fileExists(const char *filepath);

	//! Determines if a file exists.
	//! @param filepath the path to the file to delete
	//! @param callback callback
	//! @param callbackUserData optional user data pointer for callback
	void fileExistsWithCallback(const char *filepath, WorkItemCallback callback, void *callbackUserData = nullptr);

	//! Gets the size of a file.
	//! @param filepath the path to the file to delete
	//! @return a WorkItem representing the work to be done
	WorkItem *fileSize(const char *filepath);

	//! Gets the size of a file.
	//! @param filepath the path to the file to delete
	//! @param callback callback
	//! @param callbackUserData optional user data pointer for callback
	void fileSizeWithCallback(const char *filepath, WorkItemCallback callback, void *callbackUserData = nullptr);

	//! Deletes a file.
	//! @param filepath the path to the file to delete
	//! @return a WorkItem representing the work to be done
	WorkItem *deleteFile(const char *filepath);

	//! Deletes a file.
	//! @param filepath the path to the file to delete
	//! @param callback callback
	//! @param callbackUserData optional user data pointer for callback
	void deleteFileWithCallback(const char *filepath, WorkItemCallback callback, void *callbackUserData = nullptr);

	//! Creates a directory.
	//! @param path the path to the directory to create
	//! @return a WorkItem representing the work to be done
	WorkItem *createDir(const char *path);

	//! Creates a directory.
	//! @param path the path to the directory to create
	//! @param callback callback
	//! @param callbackUserData optional user data pointer for callback
	void createDirWithCallback(const char *path, WorkItemCallback callback, void *callbackUserData = nullptr);

	//! Deletes a directory and all contained files/directories.
	//! @param path the path to the directory to delete
	//! @return a WorkItem representing the work to be done
	WorkItem *deleteDir(const char *path);

	//! Deletes a directory and all contained files/directories.
	//! @param path the path to the directory to delete
	//! @param callback callback
	//! @param callbackUserData optional user data pointer for callback
	void deleteDirWithCallback(const char *path, WorkItemCallback callback, void *callbackUserData = nullptr);

	//! Releases a WorkItem.
	//! @param workItem the WorkItem to release.
	void releaseWorkItem(WorkItem *workItem);

	//! Sets the log function.
	//! @param func the logging function
	void setLogFunc(LogFunc func) { _log = func; }

	//! Gets the log function.
	//! @return the logging function
	LogFunc getLogFunc() { return _log; }

	//! Returns the Allocator interface used to initialize this context.
	//! @return the Allocator
	Allocator &getAllocator() { return _alloc; }

	//! Gets a mutex guarding the work item completion condition variable.
	//! @return the mutex
	std::mutex &getCompletionMutex() { return _completionMutex; }

	//! Gets a condition variable signalled when a work item completes.
	//! @return the condition variable
	std::condition_variable &getCompletionConditionVariable() { return _completionConditionVariable; }

	//! Destructively normalizes a path.
	//! @param path the path to normalize
	static void normalizePath(char *path);

	//! The type index of the Directory device. It will always be the first interface.
	static const uint32_t kDirectoryDeviceIndex = 0;
private:
	struct MountInfo {
		char *_prefix;
		void *_device;
		DeviceInterface *_interface;
		uint32_t _prefixLen;
		uint32_t _permissions;
	};

	MountInfo* findNextMountAndPath(const char *path, const char **devicePath, const MountInfo* searchStart);
	MountInfo* findMutableMountAndPath(const char *path, const char **devicePath, uint32_t op);

	WorkItem *allocWorkItemCommon(const char *path, uint32_t op, WorkItemCallback callback, void *callbackUserData, CallbackBufferAction bufferAction);
	void initWorkItem(WorkItem *item, const char *path, uint32_t op, WorkItemCallback callback, void *callbackUserData, CallbackBufferAction bufferAction);
	void releaseWorkItemInternal(WorkItem *workItem);

	void startProcessingThread();
	void stopProcessingThread();
	static void processingFunc(FileContext *ctx);

	std::vector<DeviceInterface*, AllocatorAdapter<DeviceInterface*>> _interfaces;
	std::vector<MountInfo*, AllocatorAdapter<MountInfo*>> _mounts;
	std::shared_mutex _mountLock;

	util::PoolAllocator<WorkItem> _workItemPool;
	util::Semaphore _workItemQueueSemaphore;
	util::RingBuffer<WorkItem*> _workItemQueue;

	std::thread _processingThread;

	Allocator _alloc;
	LogFunc _log = nullptr;
	std::atomic<bool> _processing;
	std::mutex _completionMutex;
	std::condition_variable _completionConditionVariable;
};

}
