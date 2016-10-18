// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <laminaFS.h>
#include "macros.h"

using namespace laminaFS;

namespace {
const char *testString = "this is the C++ test string.";
}

int test_cpp_api() {
	TEST_INIT();

	FileContext ctx(laminaFS::DefaultAllocator);

	// test creating mounts
	TEST(LFS_OK, ctx.createMount(0, "/", "testData/testroot"), "Mount testData/testroot -> /");
	TEST(LFS_OK, ctx.createMount(0, "/four", "testData/testroot2"), "Mount testData/testroot2 -> /four");
	TEST(LFS_NOT_FOUND, ctx.createMount(0, "/five", "testData/nonexistentdir"), "Mount testData/nonexistentdir -> /five (expected fail)");

	// test reading
	{
		WorkItem *readTest = ctx.readFile("/one/random.txt");
		WaitForWorkItem(readTest);

		TEST(LFS_OK, WorkItemGetResult(readTest), "Read file /one/random.txt");

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
	
	TEST_RESULTS();
	TEST_RETURN();
}
