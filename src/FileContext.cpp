// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include "FileContext.h"

using namespace laminaFS;

uint32_t FileContext::kDirectoryDeviceIndex = 0;

struct File {
	FileHandle _file;
	void *_interface;
	uint32_t _openCount;
};

FileContext::FileContext() {
}

FileContext::~FileContext() {
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
