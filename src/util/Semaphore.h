#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <condition_variable>
#include <mutex>

#include <cstdio>

namespace laminaFS {
namespace util {

class Semaphore {
public:
	Semaphore(uint32_t value = 0) : _value(value) {}
	~Semaphore() {}

	void notify() {
		{
			std::unique_lock<std::mutex> lock(_mutex);
			++_value;
		}
		_cond.notify_one();
	}

	void wait() {
		std::unique_lock<std::mutex> lock(_mutex);
		while (_value == 0) {
			_cond.wait(lock);
		}

		--_value;
	}

private:
	std::condition_variable _cond;
	std::mutex _mutex;
	volatile uint32_t _value;
};

}
}
