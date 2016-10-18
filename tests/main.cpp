// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <laminaFS.h>
#include <cstdio>

#include <malloc.h>

#include "macros.h"

void *test_alloc(void *, size_t bytes, size_t alignment) {
#if defined _WIN32
	return _aligned_malloc(bytes, alignment);
#else
	return memalign(alignment, bytes);
#endif
}

void test_free(void *, void *ptr) {
	free(ptr);
}

const char *testString = "this is a test string.";

using namespace laminaFS;

int main(int argc, char *argv[]) {
	TEST_INIT();

	Allocator allocator;
	allocator.alloc = test_alloc;
	allocator.free = test_free;
	allocator.allocator = nullptr;

	FileContext ctx(allocator);
	//ctx.setLogFunc(printf);

	// test creating mounts
	TEST(LFS_OK, ctx.createMount(0, "/", "testData/testroot"), "Mount testData/testroot -> /");
	TEST(LFS_OK, ctx.createMount(0, "/four", "testData/testroot2"), "Mount testData/testroot2 -> /four");
	TEST(LFS_NOT_FOUND, ctx.createMount(0, "/five", "testData/nonexistentdir"), "Mount testData/nonexistentdir -> /five (expected fail)");

	// test reading
	{
		WorkItem *readTest = ctx.readFile("/one/random.txt");
		ctx.waitForWorkItem(readTest);

		TEST(LFS_OK, ctx.workItemGetResult(readTest), "Read file /one/random.txt");

		ctx.releaseWorkItem(readTest);
	}

	// test writing
	{
		WorkItem *writeTest = ctx.writeFile("/two/test.txt", const_cast<char *>(testString), strlen(testString));
		ctx.waitForWorkItem(writeTest);
		TEST(LFS_OK, ctx.workItemGetResult(writeTest), "Write file /two/test.txt");
		TEST(strlen(testString), ctx.workItemGetBytes(writeTest), "Check bytes written");

		ctx.releaseWorkItem(writeTest);
	}
	
	// test file existence
	{
		WorkItem *existsTest = ctx.fileExists("/four/four.txt");
		ctx.waitForWorkItem(existsTest);
		TEST(LFS_OK, ctx.workItemGetResult(existsTest), "Check file existence /four/four.txt");
		
		ctx.releaseWorkItem(existsTest);
	}
	
	// test appending
	{
		WorkItem *appendTest = ctx.appendFile("/two/test.txt", const_cast<char *>(testString), strlen(testString));
		ctx.waitForWorkItem(appendTest);
		TEST(LFS_OK, ctx.workItemGetResult(appendTest), "Append file /two/test.txt");
		TEST(strlen(testString), ctx.workItemGetBytes(appendTest), "Check bytes appended");

		ctx.releaseWorkItem(appendTest);
	}
	
	// test file size
	{
		WorkItem *sizeTest = ctx.fileSize("/two/test.txt");
		ctx.waitForWorkItem(sizeTest);
		TEST(LFS_OK, ctx.workItemGetResult(sizeTest), "Get file size /two/test.txt");
		TEST(strlen(testString) * 2, ctx.workItemGetBytes(sizeTest), "Check file size /two/test.txt");

		ctx.releaseWorkItem(sizeTest);
	}
	
	// test deleting
	{
		WorkItem *deleteTest = ctx.deleteFile("/two/test.txt");
		ctx.waitForWorkItem(deleteTest);
		TEST(LFS_OK, ctx.workItemGetResult(deleteTest), "Delete file /two/test.txt");

		ctx.releaseWorkItem(deleteTest);
	}
	
	TEST_RESULTS();
	TEST_RETURN();
}
