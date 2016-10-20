#include "platform.h"

#include <cstdio>
#include <cstring>

#if __linux__
int strcpy_s(char *dest, size_t, const char *src) {
	dest = strcpy(dest, src);
	return 0;
}

int fopen_s(FILE **streamptr, const char *filename, const char *mode) {
	*streamptr = fopen(filename, mode);
	return *streamptr == nullptr;
}

#endif

