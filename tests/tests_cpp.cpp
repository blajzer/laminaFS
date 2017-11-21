// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <laminaFS.h>
#include "macros.h"

#include <cstring>

using namespace laminaFS;

#ifdef _WIN32
#define strdup _strdup
#endif

namespace {
const char *testString = "this is the C++ test string.";
}

int test_cpp_api() {
	TEST_INIT();

	// test normalization
	{
		char *str1 = strdup("//path//with/a/////lot/of/slashes///");
		FileContext::normalizePath(str1);
		TEST(0, strcmp(str1, "/path/with/a/lot/of/slashes"), "Normalize \"//path//with/a/////lot/of/slashes///\"");
		free(str1);

		char *str2 = strdup("//path//with/a/////lot/of/slashes///file.txt");
		FileContext::normalizePath(str2);
		TEST(0, strcmp(str2, "/path/with/a/lot/of/slashes/file.txt"), "Normalize \"//path//with/a/////lot/of/slashes///file.txt\"");
		free(str2);

		char *str3 = strdup("///path//with/a/////../lot/of/../../slashes///file.txt");
		FileContext::normalizePath(str3);
		TEST(0, strcmp(str3, "/path/with/slashes/file.txt"), "Normalize \"///path//with/a/////../lot/of/../../slashes///file.txt\"");
		free(str3);

		char *str4 = strdup("/..");
		FileContext::normalizePath(str4);
		TEST(0, strcmp(str4, "/"), "Normalize \"/..\"");
		free(str4);

		char *str5 = strdup("/////../..");
		FileContext::normalizePath(str5);
		TEST(0, strcmp(str5, "/"), "Normalize \"/////../..\"");
		free(str5);

		char *str6 = strdup("/////./././../boop/../some_other_dir");
		FileContext::normalizePath(str6);
		TEST(0, strcmp(str6, "/some_other_dir"), "Normalize \"/////./././../boop/../some_other_dir\"");
		free(str6);

		char *str7 = strdup("/////");
		FileContext::normalizePath(str7);
		TEST(0, strcmp(str7, "/"), "Normalize \"/////\"");
		free(str7);

		char *str8 = strdup("/./../../../././///./bringing/everything/..//it///.///././././all/./to/./pieces/..//.///../together/");
		FileContext::normalizePath(str8);
		TEST(0, strcmp(str8, "/bringing/it/all/together"), "Normalize \"/./../../../././///./bringing/everything/..//it///.///././././all/./to/./pieces/..//.///../together/\"");
		free(str8);

		char *str9 = strdup("/.thing");
		FileContext::normalizePath(str9);
		TEST(0, strcmp(str9, "/.thing"), "Normalize \"/.thing\"");
		free(str9);

		char *str10 = strdup("/.");
		FileContext::normalizePath(str10);
		TEST(0, strcmp(str10, "/"), "Normalize \"/.\"");
		free(str10);

		char *str11 = strdup("///..first/second");
		FileContext::normalizePath(str11);
		TEST(0, strcmp(str11, "/..first/second"), "Normalize \"////..first/second\"");
		free(str11);
	}

	FileContext ctx(laminaFS::DefaultAllocator);

	// test creating mounts
	ErrorCode resultCode;

	Mount mount1 = ctx.createMount(0, "/", "testData/testroot", resultCode);
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

	// remove mount
	TEST(true, ctx.releaseMount(mount2), "Unmount testData/testroot2 -> /four");
	TEST(false, ctx.releaseMount(mount3), "Unmount testData/nonexistentdir -> /five (expected fail)");

	TEST_RESULTS();
	TEST_RETURN();
}
