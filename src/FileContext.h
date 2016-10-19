#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <thread>

#include "shared_types.h"
#include "util/PoolAllocator.h"
#include "util/RingBuffer.h"

namespace laminaFS {

typedef void* FileHandle;
typedef lfs_error_code_t ErrorCode;
typedef lfs_work_item_t WorkItem;
typedef lfs_allocator_t Allocator;
typedef lfs_work_item_callback_t WorkItemCallback;
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

	void deallocate(T* p, std::size_t n) {
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
//! @return the result code
extern ErrorCode WorkItemGetResult(WorkItem *workItem);

//! Gets the output buffer from a WorkItem.
//! @param workItem the WorkItem
//! @return the output buffer
extern void *WorkItemGetBuffer(WorkItem *workItem);

//! Gets the bytes read/written from a WorkItem.
//! @param workItem the WorkItem
//! @return the bytes read/written
extern uint64_t WorkItemGetBytes(WorkItem *workItem);

//! Frees the output buffer that was allocated by the work item.
//! @param workItem the WorkItem
extern void WorkItemFreeBuffer(WorkItem *workItem);

//! Whether or not a work item has completed processing.
//! @param workItem the WorkItem to query
extern bool WorkItemCompleted(WorkItem *workItem);

//! Waits for a WorkItem to finish processing.
//! @param workItem the WorkItem to wait for
extern void WaitForWorkItem(WorkItem *workItem);


//! FileContext is the "main" object in LaminaFS. It handles management of files,
//! mounts, and the backend processing that occurs.
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
		typedef size_t (*FileSizeFunc)(void *, const char *);
		typedef size_t (*ReadFileFunc)(void *, const char *, lfs_allocator_t *, void **);

		typedef size_t (*WriteFileFunc)(void *, const char *, void *, size_t, bool);
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
	//! @return the mount
	Mount createMount(uint32_t deviceType, const char *mountPoint, const char *devicePath, ErrorCode &returnCode);

	//! Releases a mount. 
	//! Finishes all processing work items and suspends processing while it runs.
	//! @param mount the mount to remove
	//! @return whether or not the mount was found and removed
	bool releaseMount(Mount mount);

	//! Reads the entirety of a file.
	//! @param filepath the path to the file to read
	//! @param alloc the allocator to use. If NULL will use the context's allocator.
	//! @return a WorkItem representing the work to be done
	WorkItem *readFile(const char *filepath, Allocator *alloc = nullptr);
	
	//! Writes a buffer to a file.
	//! @param filepath the path to the file to write
	//! @param buffer the buffer to write
	//! @param bufferBytes the number of bytes to write to the buffer
	//! @return a WorkItem representing the work to be done
	WorkItem *writeFile(const char *filepath, void *buffer, uint64_t bufferBytes);

	//! Appends a buffer to a file.
	//! @param filepath the path to the file to append
	//! @param buffer the buffer to write
	//! @param bufferBytes the number of bytes to write to the buffer
	//! @return a WorkItem representing the work to be done
	WorkItem *appendFile(const char *filepath, void *buffer, uint64_t bufferBytes);
	
	//! Determines if a file exists.
	//! @param filepath the path to the file to delete
	//! @return a WorkItem representing the work to be done
	WorkItem *fileExists(const char *filepath);
	
	//! Gets the size of a file.
	//! @param filepath the path to the file to delete
	//! @return a WorkItem representing the work to be done
	WorkItem *fileSize(const char *filepath);
	
	//! Deletes a file.
	//! @param filepath the path to the file to delete
	//! @return a WorkItem representing the work to be done
	WorkItem *deleteFile(const char *filepath);
	
	//! Creates a directory.
	//! @param path the path to the directory to create
	//! @return a WorkItem representing the work to be done
	WorkItem *createDir(const char *path);
	
	//! Deletes a directory and all contained files/directories.
	//! @param path the path to the directory to delete
	//! @return a WorkItem representing the work to be done
	WorkItem *deleteDir(const char *path);

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

	//! The type index of the Directory device. It will always be the first interface.
	static const uint32_t kDirectoryDeviceIndex = 0;
private:
	struct MountInfo {
		char *_prefix;
		void *_device;
		DeviceInterface *_interface;
		uint32_t _prefixLen;
	};

	MountInfo* findMountAndPath(const char *path, const char **devicePath);
	MountInfo* findMutableMountAndPath(const char *path, const char **devicePath, uint32_t op);

	WorkItem *allocWorkItemCommon(const char *path, uint32_t op);

	void startProcessingThread();
	void stopProcessingThread();
	static void processingFunc(FileContext *ctx);

	std::vector<DeviceInterface*, AllocatorAdapter<DeviceInterface*>> _interfaces;
	std::vector<MountInfo*, AllocatorAdapter<MountInfo*>> _mounts;

	util::PoolAllocator<WorkItem> _workItemPool;
	util::RingBuffer<WorkItem*> _workItemQueue;
	std::thread _processingThread;

	Allocator _alloc;
	LogFunc _log = nullptr;
	std::atomic<bool> _processing;
};

}
