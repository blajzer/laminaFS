// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <cstdio>

#include "macros.h"

extern "C" int test_c_api();
extern int test_cpp_api();

int main(int argc, char *argv[]) {
	int result = 0;
	
	printf("Testing C++ API:\n");
	printf("----------------\n");
	result += test_cpp_api();
	printf("\n");
	
	printf("Testing C API:\n");
	printf("--------------\n");
	result += test_c_api();
	
	return result;
}
