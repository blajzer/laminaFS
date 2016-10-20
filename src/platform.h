#pragma once

#include <cstdio>
#include <cstddef>

#if __linux__
int strcpy_s(char *dest, size_t, const char *src);
int fopen_s(FILE **streamptr, const char *filename, const char *mode);
#endif
