// LaminaFS is Copyright (c) 2016 Brett Lajzer
// See LICENSE for license information.

#include <laminaFS.h>
#include <cstdio>

using namespace laminaFS;

int main(int argc, char *argv[]) {
	FileContext ctx;
	ctx.setLogFunc(printf);
	ctx.createMount(0, "/", ".", false);
	ctx.createMount(0, "/bleh", "/bin", false);
	
	File *f;
	FileMode fileMode{true, false, false};
	if (ctx.openFile("/foo/bar", fileMode, &f) != 0) {
		printf("Couldn't open file\n");
	}
	ctx.closeFile(f);

	if (ctx.openFile("/bleh/bar", fileMode, &f) != 0) {
		printf("Couldn't open file\n");
	}

	ctx.closeFile(f);

	return 0;
}
