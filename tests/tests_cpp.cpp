// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <laminaFS.h>
#include "macros.h"

#include <cstring>

using namespace laminaFS;

#ifdef _WIN32
#define strdup _strdup
#endif

#ifndef _countof
#define _countof(X) (sizeof(X) / sizeof(*(X)))
#endif

namespace {
const char *testString = "this is the C++ test string.";
const char *testString2 = "this is our C++ test string.";
constexpr uint64_t testStringOffset = 8;

const char *normalizationTestStrings[] = {
	"//path//with/a/////lot/of/slashes///",
	"//path//with/a/////lot/of/slashes///file.txt",
	"///path//with/a/////../lot/of/../../slashes///file.txt",
	"/..",
	"/////../..",
	"/////./././../boop/../some_other_dir",
	"/////",
	"/./../../../././///./bringing/everything/..//it///.///././././all/./to/./pieces/..//.///../together/",
	"/.thing",
	"/.",
	"///..first/second"
};

const char *normalizationTestResults[] = {
	"/path/with/a/lot/of/slashes",
	"/path/with/a/lot/of/slashes/file.txt",
	"/path/with/slashes/file.txt",
	"/",
	"/",
	"/some_other_dir",
	"/",
	"/bringing/it/all/together",
	"/.thing",
	"/",
	"/..first/second"
};

}

int test_cpp_api() {
	TEST_INIT();

	// test normalization
	for (uint32_t i = 0; i < _countof(normalizationTestStrings); ++i)
	{
		char *testStr = strdup(normalizationTestStrings[i]);
		FileContext::normalizePath(testStr);

		++testCount;
		bool passed = strcmp(testStr, normalizationTestResults[i]) == 0;
		testsPassed += passed ? 1 : 0;
		printf("[%s]: Normalize: %s\n", passed ? PASS_STRING : FAIL_STRING, normalizationTestStrings[i]);

		free(testStr);
	}

	FileContext ctx(laminaFS::DefaultAllocator);

	// test creating mounts
	ErrorCode resultCode;

	ctx.createMount(0, "/", "testData/testroot", resultCode);
	TEST(LFS_OK, resultCode, "Mount testData/testroot -> /");

	Mount mount2 = ctx.createMount(0, "/four", "testData/testroot2", resultCode);
	TEST(LFS_OK, resultCode, "Mount testData/testroot2 -> /four");

	Mount mount3 = ctx.createMount(0, "/five", "testData/nonexistentdir", resultCode);
	TEST(LFS_NOT_FOUND, resultCode, "Mount testData/nonexistentdir -> /five (expected fail)");

	// test reading
	{
		WorkItem *readTest = ctx.readFile("/one/random.txt", false);
		WaitForWorkItem(readTest);

		TEST(LFS_OK, WorkItemGetResult(readTest), "Read file /one/random.txt");

		WorkItemFreeBuffer(readTest);

		ctx.releaseWorkItem(readTest);
	}

	// test writing
	{
		WorkItem *writeTest = ctx.writeFile("/two/test.txt", const_cast<char *>(testString), strlen(testString));
		WaitForWorkItem(writeTest);
		TEST(LFS_OK, WorkItemGetResult(writeTest), "Write file /two/test.txt");
		TEST(strlen(testString), WorkItemGetBytes(writeTest), "Check bytes written");

		ctx.releaseWorkItem(writeTest);
	}

	// test segment writing and reading
	{
		WorkItem *writeTest = ctx.writeFileSegment("/two/test.txt", testStringOffset, "our", 3);
		WaitForWorkItem(writeTest);
		TEST(LFS_OK, WorkItemGetResult(writeTest), "Write file segment to /two/test.txt");
		TEST(3, WorkItemGetBytes(writeTest), "Check bytes written");

		ctx.releaseWorkItem(writeTest);

		WorkItem *readTest = ctx.readFile("/two/test.txt", true);
		WaitForWorkItem(readTest);
		TEST(LFS_OK, WorkItemGetResult(readTest), "Read file /two/test.txt");
		TEST(0, strcmp(static_cast<char*>(WorkItemGetBuffer(readTest)), testString2), "Compare string.");
		WorkItemFreeBuffer(readTest);

		ctx.releaseWorkItem(readTest);

		WorkItem *readTest2 = ctx.readFileSegment("/two/test.txt", testStringOffset, 3, true);
		WaitForWorkItem(readTest2);
		TEST(LFS_OK, WorkItemGetResult(readTest2), "Read file segment /two/test.txt");
		TEST(0, strcmp(static_cast<char*>(WorkItemGetBuffer(readTest2)), "our"), "Compare string segment.");
		WorkItemFreeBuffer(readTest2);

		ctx.releaseWorkItem(readTest2);
	}

	// test segment reading
	{
		WorkItem *writeTest = ctx.writeFileSegment("/two/test.txt", testStringOffset, "our", 3);
		WaitForWorkItem(writeTest);
		TEST(LFS_OK, WorkItemGetResult(writeTest), "Write file segment to /two/test.txt");
		TEST(3, WorkItemGetBytes(writeTest), "Check bytes written");

		ctx.releaseWorkItem(writeTest);
	}

	// test file existence
	{
		WorkItem *existsTest = ctx.fileExists("/four/four.txt");
		WaitForWorkItem(existsTest);
		TEST(LFS_OK, WorkItemGetResult(existsTest), "Check file existence /four/four.txt");

		ctx.releaseWorkItem(existsTest);
	}

	// test appending
	{
		WorkItem *appendTest = ctx.appendFile("/two/test.txt", const_cast<char *>(testString), strlen(testString));
		WaitForWorkItem(appendTest);
		TEST(LFS_OK, WorkItemGetResult(appendTest), "Append file /two/test.txt");
		TEST(strlen(testString), WorkItemGetBytes(appendTest), "Check bytes appended");

		ctx.releaseWorkItem(appendTest);
	}

	// test file size
	{
		WorkItem *sizeTest = ctx.fileSize("/two/test.txt");
		WaitForWorkItem(sizeTest);
		TEST(LFS_OK, WorkItemGetResult(sizeTest), "Get file size /two/test.txt");
		TEST(strlen(testString) * 2, WorkItemGetBytes(sizeTest), "Check file size /two/test.txt");

		ctx.releaseWorkItem(sizeTest);
	}

	// test deleting
	{
		WorkItem *deleteTest = ctx.deleteFile("/two/test.txt");
		WaitForWorkItem(deleteTest);
		TEST(LFS_OK, WorkItemGetResult(deleteTest), "Delete file /two/test.txt");

		ctx.releaseWorkItem(deleteTest);
	}

	// test directory creation
	{
		WorkItem *dirCreateTest = ctx.createDir("/two/testDir");
		WorkItem *dirCreateTest1 = ctx.createDir("/two/testDir/nested");
		WorkItem *dirCreateTest2 = ctx.createDir("/two/testDir/nested/even_more");
		WorkItem *dirCreateTest3 = ctx.createDir("/two/testDir/nested/even_more/so_deep");

		// Work items are sequential, only need to wait on the last one
		WaitForWorkItem(dirCreateTest3);

		TEST(LFS_OK, WorkItemGetResult(dirCreateTest), "Create dir /two/testDir");
		TEST(LFS_OK, WorkItemGetResult(dirCreateTest1), "Create dir /two/testDir/nested");
		TEST(LFS_OK, WorkItemGetResult(dirCreateTest2), "Create dir /two/testDir/nested/even_more");
		TEST(LFS_OK, WorkItemGetResult(dirCreateTest3), "Create dir /two/testDir/nested/even_more/so_deep");

		ctx.releaseWorkItem(dirCreateTest);
		ctx.releaseWorkItem(dirCreateTest1);
		ctx.releaseWorkItem(dirCreateTest2);
		ctx.releaseWorkItem(dirCreateTest3);
	}

	// test directory deletion
	{
		// write a file in the test directory first
		WorkItem *writeTest = ctx.writeFile("/two/testDir/nested/even_more/test.txt", const_cast<char *>(testString), strlen(testString));
		WaitForWorkItem(writeTest);
		TEST(LFS_OK, WorkItemGetResult(writeTest), "Write file /two/testDir/nested/even_more/test.txt");

		WorkItem *dirDeleteTest = ctx.deleteDir("/two/testDir");
		WaitForWorkItem(dirDeleteTest);
		TEST(LFS_OK, WorkItemGetResult(dirDeleteTest), "Delete dir /two/testDir");

		ctx.releaseWorkItem(writeTest);
		ctx.releaseWorkItem(dirDeleteTest);
	}

	// test Unicode support
	{
		WorkItem *unicodeTest = ctx.writeFile("/two/koala🐨.txt", const_cast<char *>(testString), strlen(testString));
		WaitForWorkItem(unicodeTest);
		TEST(LFS_OK, WorkItemGetResult(unicodeTest), "Write file /two/koala🐨.txt");

		WorkItem *deleteTest = ctx.deleteFile("/two/koala🐨.txt");
		WaitForWorkItem(deleteTest);
		TEST(LFS_OK, WorkItemGetResult(deleteTest), "Delete file /two/koala🐨.txt");

		ctx.releaseWorkItem(unicodeTest);
		ctx.releaseWorkItem(deleteTest);
	}

	// remove mount
	TEST(true, ctx.releaseMount(mount2), "Unmount testData/testroot2 -> /four");
	TEST(false, ctx.releaseMount(mount3), "Unmount testData/nonexistentdir -> /five (expected fail)");

	TEST_RESULTS();
	TEST_RETURN();
}
