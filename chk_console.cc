#include <stdio.h>
#include <stdlib.h>
#include "chk_tty.h"

int main(int argc, char *argv[])
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <PID> <script file name> [result file name]\n", argv[0]);
		return -1;
	}

	int pid = atoi(argv[1]);
	chk_console checker;

	return checker.run(pid, argv[2], argc >= 4 ? argv[3] : NULL);
}
