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
	enum lfs_error_code_t resultCode;
	lfs_create_mount(ctx, 0, "/", "testData/testroot", &resultCode);
	TEST(LFS_OK, resultCode, "Mount testData/testroot -> /");

	lfs_mount_t mount2 = lfs_create_mount(ctx, 0, "/four", "testData/testroot2", &resultCode);
	TEST(LFS_OK, resultCode, "Mount testData/testroot2 -> /four");

	lfs_mount_t mount3 = lfs_create_mount(ctx, 0, "/five", "testData/nonexistentdir", &resultCode);
	TEST(LFS_NOT_FOUND, resultCode, "Mount testData/nonexistentdir -> /five (expected fail)");

	// test reading
	{
		struct lfs_work_item_t *readTest = lfs_read_file_ctx_alloc(ctx, "/one/random.txt", false, NULL, NULL);
		lfs_wait_for_work_item(readTest);

		TEST(LFS_OK, lfs_work_item_get_result(readTest), "Read file /one/random.txt");

		lfs_work_item_free_buffer(readTest);

		lfs_release_work_item(ctx, readTest);
	}

	// test writing
	{
		struct lfs_work_item_t *writeTest = lfs_write_file(ctx, "/two/test.txt", (char *)(testString), strlen(testString), NULL, NULL);
		lfs_wait_for_work_item(writeTest);
		TEST(LFS_OK, lfs_work_item_get_result(writeTest), "Write file /two/test.txt");
		TEST(strlen(testString), lfs_work_item_get_bytes(writeTest), "Check bytes written");

		lfs_release_work_item(ctx, writeTest);
	}

	// test file existence
	{
		struct lfs_work_item_t *existsTest = lfs_file_exists(ctx, "/four/four.txt", NULL, NULL);
		lfs_wait_for_work_item(existsTest);
		TEST(LFS_OK, lfs_work_item_get_result(existsTest), "Check file existence /four/four.txt");

		lfs_release_work_item(ctx, existsTest);
	}

	// test appending
	{
		struct lfs_work_item_t *appendTest = lfs_append_file(ctx, "/two/test.txt", (char *)(testString), strlen(testString), NULL, NULL);
		lfs_wait_for_work_item(appendTest);
		TEST(LFS_OK, lfs_work_item_get_result(appendTest), "Append file /two/test.txt");
		TEST(strlen(testString), lfs_work_item_get_bytes(appendTest), "Check bytes appended");

		lfs_release_work_item(ctx, appendTest);
	}

	// test file size
	{
		struct lfs_work_item_t *sizeTest = lfs_file_size(ctx, "/two/test.txt", NULL, NULL);
		lfs_wait_for_work_item(sizeTest);
		TEST(LFS_OK, lfs_work_item_get_result(sizeTest), "Get file size /two/test.txt");
		TEST(strlen(testString) * 2, lfs_work_item_get_bytes(sizeTest), "Check file size /two/test.txt");

		lfs_release_work_item(ctx, sizeTest);
	}

	// test deleting
	{
		struct lfs_work_item_t *deleteTest = lfs_delete_file(ctx, "/two/test.txt", NULL, NULL);
		lfs_wait_for_work_item(deleteTest);
		TEST(LFS_OK, lfs_work_item_get_result(deleteTest), "Delete file /two/test.txt");

		lfs_release_work_item(ctx, deleteTest);
	}

	// test directory creation
	{
		struct lfs_work_item_t *dirCreateTest = lfs_create_dir(ctx, "/two/testDir", NULL, NULL);
		struct lfs_work_item_t *dirCreateTest1 = lfs_create_dir(ctx, "/two/testDir/nested", NULL, NULL);
		struct lfs_work_item_t *dirCreateTest2 = lfs_create_dir(ctx, "/two/testDir/nested/even_more", NULL, NULL);
		struct lfs_work_item_t *dirCreateTest3 = lfs_create_dir(ctx, "/two/testDir/nested/even_more/so_deep", NULL, NULL);

		// Work items are sequential, only need to wait on the last one
		lfs_wait_for_work_item(dirCreateTest3);

		TEST(LFS_OK, lfs_work_item_get_result(dirCreateTest), "Create dir /two/testDir");
		TEST(LFS_OK, lfs_work_item_get_result(dirCreateTest1), "Create dir /two/testDir/nested");
		TEST(LFS_OK, lfs_work_item_get_result(dirCreateTest2), "Create dir /two/testDir/nested/even_more");
		TEST(LFS_OK, lfs_work_item_get_result(dirCreateTest3), "Create dir /two/testDir/nested/even_more/so_deep");

		lfs_release_work_item(ctx, dirCreateTest);
		lfs_release_work_item(ctx, dirCreateTest1);
		lfs_release_work_item(ctx, dirCreateTest2);
		lfs_release_work_item(ctx, dirCreateTest3);
	}

	// test directory deletion
	{
		// write a file in the test directory first
		struct lfs_work_item_t *writeTest = lfs_write_file(ctx, "/two/testDir/nested/even_more/test.txt", (char *)(testString), strlen(testString), NULL, NULL);
		lfs_wait_for_work_item(writeTest);
		TEST(LFS_OK, lfs_work_item_get_result(writeTest), "Write file /two/testDir/nested/even_more/test.txt");

		struct lfs_work_item_t *dirDeleteTest = lfs_delete_dir(ctx, "/two/testDir", NULL, NULL);
		lfs_wait_for_work_item(dirDeleteTest);
		TEST(LFS_OK, lfs_work_item_get_result(dirDeleteTest), "Delete dir /two/testDir");

		lfs_release_work_item(ctx, writeTest);
		lfs_release_work_item(ctx, dirDeleteTest);
	}

	TEST(true, lfs_release_mount(ctx, mount2), "Unmount testData/testroot2 -> /four");
	TEST(false, lfs_release_mount(ctx, mount3), "Unmount testData/nonexistentdir -> /five (expected fail)");

	lfs_context_destroy(ctx);

	TEST_RESULTS();
	TEST_RETURN();
}
