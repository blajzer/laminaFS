#pragma once
// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

enum lfs_file_mode_t {
	LFS_FM_READ,
	LFS_FM_WRITE,
	LFS_FM_READ_WRITE,
	LFS_FM_APPEND
};

enum lfs_error_code_t {
	LFS_OK = 0,
	LFS_NOT_FOUND,
	LFS_UNSUPPORTED,
};
