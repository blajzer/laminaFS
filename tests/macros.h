#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <stdio.h>

#define TEST_INIT() int testCount = 0;\
int testsPassed = 0;

#ifdef _WIN32
#define PASS_STRING "\033[32;1m+\033[0m"
#define FAIL_STRING "\033[31;1m-\033[0m"
#else
#define PASS_STRING "\033[32;1m✓\033[0m"
#define FAIL_STRING "\033[31;1m✘\033[0m"
#endif

#define TEST(E, X, NAME) {\
++testCount;\
bool passed = E == (X);\
testsPassed += passed ? 1 : 0;\
printf("[%s]: %s - (line %d)\n", passed ? PASS_STRING : FAIL_STRING, NAME, __LINE__);\
}

#define TEST_RESULTS() printf("\n%s: %d/%d tests passed\n", testCount == testsPassed ? "SUCCESS" : "FAILURE", testsPassed, testCount);
#define TEST_RETURN() return testCount == testsPassed ? 0 : 1;
