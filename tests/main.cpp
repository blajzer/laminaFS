// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <laminaFS.h>
#include <cstdio>

using namespace laminaFS;

int main(int argc, char *argv[]) {
	FileContext ctx;
	ctx.setLogFunc(printf);

	ctx.createMount(0, "/", "testData/testroot");
	ctx.createMount(0, "/four", "testData/testroot2");

	File *f;
	FileMode fileMode = LFS_FM_READ;
	if (ctx.openFile("/one/random.txt", fileMode, &f) != 0) {
		printf("Couldn't open file\n");
	} else {
		WorkItem *wi = ctx.createWorkItem();
		wi->configSize(f);

		ctx.submitWorkItem(wi);
		ctx.waitForWorkItem(wi);

		printf("file size is: %llu bytes\n", wi->_result.bytes);
		
		ctx.releaseWorkItem(wi);
	}
	ctx.closeFile(f);

	if (ctx.openFile("/four/four.txt", fileMode, &f) != 0) {
		printf("Couldn't open file\n");
	}

	ctx.closeFile(f);

	return 0;
}
