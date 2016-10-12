#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

struct lfs_file_mode_t {
	unsigned int read : 1;
	unsigned int write : 1;
	unsigned int append : 1;
};

enum lfs_error_code_t {
	LFS_OK = 0,
	LFS_NOT_FOUND,
	
};
