#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

#include "shared_types.h"

namespace laminaFS {
namespace util {

//! Simple ring buffer of a fixed capacity.
template <typename T>
class RingBuffer {
public:
	RingBuffer() {
		_capacity = 0;
		_full = true;
	}

	RingBuffer(lfs_allocator_t &alloc, uint64_t capacity) {
		_alloc = alloc;
		_capacity = capacity;
		_buffer = reinterpret_cast<T*>(_alloc.alloc(_alloc.allocator, sizeof(T) * capacity, alignof(T)));
		_full = false;
	}

	~RingBuffer() {
		_alloc.free(_alloc.allocator, _buffer);
	}

	//! Push an item into the buffer. Blocks if buffer is full.
	//! @param v the item to push.
	void push(T v) {
		bool done = false;

		while (!done) {
			while (_full)
				std::this_thread::yield();

			std::lock_guard<std::mutex> lock(_lock);
			if (!_full) {
				_buffer[_writePos++] = v;

				if (_writePos == _capacity)
					_writePos = 0;
			
				if (_writePos == _readPos)
					_full = true;
		
				done = true;
			}
		}
	}
	
	//! Attemps to pop an item. Nonblocking.
	//! @param defaultValue the value to return if the buffer is empty
	//! @return an item or the default value
	T pop(T defaultValue) {
		std::lock_guard<std::mutex> lock(_lock);
		if (_full || _readPos != _writePos) {
			uint64_t oldReadPos = _readPos++;

			_full = false;
			if (_readPos == _capacity)
				_readPos = 0;

			return _buffer[oldReadPos];
		} else {
			return defaultValue;
		}
	}

	//! Gets the capacity of the buffer.
	//! @return the capacity
	uint64_t getCapacity() const { return _capacity; }

	//! Gets the current number of items in the buffer.
	//! @return the current number of items
	uint64_t getCount() const {
		std::lock_guard<std::mutex>(_lock);
		uint64_t result = 0;
		
		if (_full) {
			result = _capacity;
		} else if (_readPos <= _writePos) {
			result = _writePos - _readPos;
		} else {
			result = _capacity - _readPos + _writePos; 
		}
		
		return result;
	}

private:
	T *_buffer;
	lfs_allocator_t _alloc;
	uint64_t _capacity;
	uint64_t _readPos = 0;
	uint64_t _writePos = 0;
	std::mutex _lock;
	std::atomic<bool> _full;
};
}
}
