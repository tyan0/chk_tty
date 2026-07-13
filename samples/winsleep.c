#include <windows.h>

int main(int argc, char *argv[])
{
	double slp = 0;
	if (argc >= 2) slp = atof(argv[1]);
	Sleep((int)(slp*1000));
	return 0;
}
