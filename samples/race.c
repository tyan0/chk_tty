#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	int n = 1;
	if (argc > 1)
		n = atoi(argv[1]);
	if (fork()) {
		execlp("cmd.exe", "cmd", NULL);
		perror("execlp(\"cmd\"): ");
	}
	for (int i=0; i<n; i++) {
		if (fork() == 0) {
			execlp("./winsleep.exe", "winsleep", "0", NULL);
			perror("execlp(\"winsleep\"): ");
		}
	}
	return 0;
}
