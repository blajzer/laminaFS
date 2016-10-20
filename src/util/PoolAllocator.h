#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <cstdint>
#include <cstdlib>
#include <mutex>

#include <cstdio>

#ifdef _WIN32
#include <intrin.h>
#else
#include <string.h>
#endif

#include "shared_types.h"

namespace laminaFS {
namespace util {

//! Simple thread-safe pool allocator
template <typename T>
class PoolAllocator {
public:
	PoolAllocator() {
	}
	
	PoolAllocator(lfs_allocator_t &alloc, uint64_t capacity) {
		_alloc = alloc;
		_storage = reinterpret_cast<T*>(_alloc.alloc(_alloc.allocator, sizeof(T) * capacity, alignof(T)));
		_capacity = capacity;
		
		// create bitmask and initialize all bits to available
		lldiv_t d = lldiv(capacity, 32);
		_bitmaskCount = d.quot + (d.rem != 0 ? 1 : 0);
		_bitmask = reinterpret_cast<uint32_t*>(_alloc.alloc(_alloc.allocator, sizeof(uint32_t) * _bitmaskCount, alignof(uint32_t)));
		for (uint64_t i = 0 ; i < static_cast<uint64_t>(d.quot); ++i) {
			_bitmask[i] = 0xFFFFFFFF;
		}
		
		if (d.rem != 0) {
			uint32_t newBitmask = 0;
			for (uint32_t i = 0; i < d.rem; ++i) {
				newBitmask |= 1 << i;
			}
			_bitmask[d.quot] = newBitmask;
		}
	}
	
	~PoolAllocator() {
		_alloc.free(_alloc.allocator, _storage);
		_alloc.free(_alloc.allocator, _bitmask);
	}

	T *alloc() {
		T *result = nullptr;

		std::lock_guard<std::mutex> lock(_mutex);
		for (uint64_t i = 0; i < _bitmaskCount; ++i) {
			if (_bitmask[i] != 0) {
#ifdef _WIN32
				unsigned long bit;
				_BitScanForward(&bit, _bitmask[i]);
#else
				uint64_t bit = static_cast<uint64_t>(ffs(_bitmask[i]) - 1);
#endif
				uint64_t index = i * 32 + bit;
				result = new(&_storage[index]) T();
				_bitmask[i] = _bitmask[i] & ~(1 << bit);
				break;
			}
		}

		return result;
	}

	void free(T *v) {
		if (v && v >= _storage && v < _storage + _capacity) {
			v->~T();
			uintptr_t index = (reinterpret_cast<uintptr_t>(v) - reinterpret_cast<uintptr_t>(_storage)) / sizeof(T);
			lldiv_t d = lldiv(index, 32);
			_bitmask[d.quot] |= 1 << d.rem;
		}
	}

private:
	std::mutex _mutex;
	T *_storage = nullptr;
	uint32_t *_bitmask = nullptr;
	lfs_allocator_t _alloc;
	uint64_t _capacity = 0;
	uint64_t _bitmaskCount = 0;
};

}
}
