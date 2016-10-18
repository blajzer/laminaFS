// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <laminaFS_c.h>
#include "macros.h"

#include <string.h>

const char *testString = "this is the C test string.";

int test_c_api() {
	TEST_INIT();

	lfs_context_t ctx = lfs_context_create(&lfs_default_allocator);

	// test creating mounts
	TEST(LFS_OK, lfs_create_mount(ctx, 0, "/", "testData/testroot"), "Mount testData/testroot -> /");
	TEST(LFS_OK, lfs_create_mount(ctx, 0, "/four", "testData/testroot2"), "Mount testData/testroot2 -> /four");
	TEST(LFS_NOT_FOUND, lfs_create_mount(ctx, 0, "/five", "testData/nonexistentdir"), "Mount testData/nonexistentdir -> /five (expected fail)");

	// test reading
	{
		struct lfs_work_item_t *readTest = lfs_read_file_ctx_alloc(ctx, "/one/random.txt");
		lfs_wait_for_work_item(readTest);

		TEST(LFS_OK, lfs_work_item_get_result(readTest), "Read file /one/random.txt");
		
		lfs_work_item_free_buffer(readTest);
		
		lfs_release_work_item(ctx, readTest);
	}

	// test writing
	{
		struct lfs_work_item_t *writeTest = lfs_write_file(ctx, "/two/test.txt", (char *)(testString), strlen(testString));
		lfs_wait_for_work_item(writeTest);
		TEST(LFS_OK, lfs_work_item_get_result(writeTest), "Write file /two/test.txt");
		TEST(strlen(testString), lfs_work_item_get_bytes(writeTest), "Check bytes written");

		lfs_release_work_item(ctx, writeTest);
	}
	
	// test file existence
	{
		struct lfs_work_item_t *existsTest = lfs_file_exists(ctx, "/four/four.txt");
		lfs_wait_for_work_item(existsTest);
		TEST(LFS_OK, lfs_work_item_get_result(existsTest), "Check file existence /four/four.txt");
		
		lfs_release_work_item(ctx, existsTest);
	}
	
	// test appending
	{
		struct lfs_work_item_t *appendTest = lfs_append_file(ctx, "/two/test.txt", (char *)(testString), strlen(testString));
		lfs_wait_for_work_item(appendTest);
		TEST(LFS_OK, lfs_work_item_get_result(appendTest), "Append file /two/test.txt");
		TEST(strlen(testString), lfs_work_item_get_bytes(appendTest), "Check bytes appended");

		lfs_release_work_item(ctx, appendTest);
	}
	
	// test file size
	{
		struct lfs_work_item_t *sizeTest = lfs_file_size(ctx, "/two/test.txt");
		lfs_wait_for_work_item(sizeTest);
		TEST(LFS_OK, lfs_work_item_get_result(sizeTest), "Get file size /two/test.txt");
		TEST(strlen(testString) * 2, lfs_work_item_get_bytes(sizeTest), "Check file size /two/test.txt");

		lfs_release_work_item(ctx, sizeTest);
	}
	
	// test deleting
	{
		struct lfs_work_item_t *deleteTest = lfs_delete_file(ctx, "/two/test.txt");
		lfs_wait_for_work_item(deleteTest);
		TEST(LFS_OK, lfs_work_item_get_result(deleteTest), "Delete file /two/test.txt");

		lfs_release_work_item(ctx, deleteTest);
	}
	
	lfs_context_destroy(ctx);
	
	TEST_RESULTS();
	TEST_RETURN();
}
