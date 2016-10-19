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

extern Allocator DefaultAllocator;


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

		// required
		CreateFunc _create = nullptr;
		DestroyFunc _destroy = nullptr;

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
	//! @return the return code, 0 on success
	ErrorCode createMount(uint32_t deviceType, const char *mountPoint, const char *devicePath);

	// TODO: releaseMount

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
	struct Mount {
		char *_prefix;
		void *_device;
		DeviceInterface *_interface;
		uint32_t _prefixLen;
	};

	Mount* findMountAndPath(const char *path, const char **devicePath);
	Mount* findMutableMountAndPath(const char *path, const char **devicePath, uint32_t op);
	
	WorkItem *allocWorkItemCommon(const char *path, uint32_t op);

	static void processingFunc(FileContext *ctx);

	// TODO: replace with flat arrays allocated through the allocator?
	std::vector<DeviceInterface> _interfaces;
	std::vector<Mount> _mounts;

	util::PoolAllocator<WorkItem> _workItemPool;
	util::RingBuffer<WorkItem*> _workItemQueue;
	std::thread _processingThread;

	Allocator _alloc;
	LogFunc _log = nullptr;
	std::atomic<bool> _processing;
};

}
