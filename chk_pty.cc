#include <stdio.h>
#include "chk_tty.h"

chk_pty *the_pty_checker;

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <script file name> [result file name]\n", argv[0]);
		return -1;
	}
	chk_pty checker;
	return checker.run(argv[1], argc >= 3 ? argv[2] : NULL);
}
