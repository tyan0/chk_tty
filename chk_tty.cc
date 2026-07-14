#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <pty.h>
#include <errno.h>
#include <wait.h>
#include <string.h>
#include <ctype.h>
#include "chk_tty.h"

bool chk_tty::do_chk(FILE *script, FILE *log)
{
	bool ok = true;;
	for (int l=1; ok; l++) {
		char scr[1024], buf[128], cmd[1024], *ptr;
		int i;
		char st1, st2;
		int n, m;
		unsigned int u;
		FILE *p;
		const char *tty1 = tty;
		fgets(scr, sizeof(scr), script);
		if (feof(script)) break;
		if (scr[strlen(scr)-1] == '\n') scr[strlen(scr)-1] = '\0';
		switch (scr[0]) {
			case 'E': /* Emit key input */
				i = 1;
				while (scr[i] == ' ') i++;
				ptr = buf;
				for (; i<strlen(scr); i++) {
					if (scr[i] == '\\') {
						if (strlen(scr+i) < 2) break;
						switch (scr[i+1]) {
							case 'n':
								*ptr++ = '\n';
								break;
							case 'r':
								*ptr++ = '\r';
								break;
							case 't':
								*ptr++ = '\t';
								break;
							case 'a':
								*ptr++ = '\a';
								break;
							case 'b':
								*ptr++ = '\b';
								break;
							case 'f':
								*ptr++ = '\f';
								break;
							case 'x':
								if (strlen(scr+i) < 4) break;
								sscanf(&scr[i+2], "%02x", &u);
								*ptr++ = u;
								i+=2;
								break;
							case '0':
								if (strlen(scr+i) >= 2
										&& !isdigit((int)scr[i+2])) {
									*ptr++ = '\0';
									break;
								}
								/* fallthrough */
							case '1':
							case '2':
							case '3':
							case '4':
							case '5':
							case '6':
							case '7':
							case '8':
							case '9':
								if (strlen(scr+i) >= 4
										&& isdigit((int)scr[i+2])
										&& isdigit((int)scr[i+3])) {
									sscanf(&scr[i+1], "%03o", &u);
									*ptr++ = u;
									i+=2;
								}
								break;
							default:
								*ptr++ = scr[i+1];
								break;
						}
						i++;
					} else *ptr++ = scr[i];
				}
				*ptr = '\0';
				emit_input(buf, strlen(buf));
				break;
			case 'W': /* Wait specified time period [ms] */
				sscanf(scr+1, "%d", &n);
				usleep(n*1000);
				break;
			case 'T': /* Check process existence on console originating from pseudo console */
				if (!is_console() && !is_new_pty()) {
					tty1 = "cons[0-9]*";
				}
				/* Fallthrough */
			case 'C': /* Check process existence on current tty */
				n = 1;
				st1 = ' ';
				sscanf(scr+1, "%127s %d %c", buf, &n, &st1);
				sprintf(cmd, "ps | grep '%s ' | grep %s | wc -l", tty1, buf);
				p = popen(cmd, "r");
				fscanf(p, "%d", &m);
				pclose(p);
				set_raw(1);
				if (m != n) {
					fprintf(log, "%d: NG\n", l);
					ok = false;
					break;
				}
				if (n == 0) {
					fprintf(log, "%d: OK\n", l);
					break;
				}
				sprintf(cmd, "ps | grep '%s ' | grep %s ", tty1, buf);
				p = popen(cmd, "r");
				st2 = fgetc(p);
				pclose(p);
				set_raw(1);
				if (st1 == st2) fprintf(log, "%d: OK\n", l);
				else {
					fprintf(log, "%d: NG\n", l);
					ok = false;
				}
				break;
			case 'N': /* Check process existence not on any ttys */
				n = 1;
				sscanf(scr+1, "%127s %d", buf, &n);
				sprintf(cmd, "ps -W | grep %s | grep -v 'pty[0-9]*' | grep -v 'cons[0-9]*' | wc -l", buf);
				p = popen(cmd, "r");
				fscanf(p, "%d", &m);
				pclose(p);
				set_raw(1);
				if (m != n) {
					fprintf(log, "%d: NG\n", l);
					ok = false;
					break;
				}
				fprintf(log, "%d: OK\n", l);
				break;
			case 'D': /* Set delay between keys [ms] */
				sscanf(scr+1, "%d", &delay);
				break;
			case 'G': /* Goto specified line on script file */
				sscanf(scr+1, "%d", &n);
				fseek(script, 0, SEEK_SET);
				for (i = 1; i < n; i++) {
					fgets(scr, sizeof(scr), script);
				}
				l = n - 1;
				break;
			case '#': /* Comment */
				break;
			default:
				fprintf(stderr, "Script Error: Line %d: %s\n", l, scr);
				ok = false;
		}
	}
	return ok;
}

/* PTY stuff */
static chk_pty *the_pty_checker;
static void sig_handler(int sig)
{
	the_pty_checker->sig_handler(sig);
}

static void *reader (void *param)
{
	return the_pty_checker->reader(param);
}

void chk_pty::winsize_set(void)
{
	struct winsize win;
	ioctl(fileno(stdin), TIOCGWINSZ, &win);
	ioctl(master, TIOCSWINSZ, &win);
}

void chk_pty::sig_handler(int sig)
{
	switch (sig) {
	case SIGWINCH:
		winsize_set();
		break;
	case SIGCHLD:
		{
			struct termios tt;
			tcgetattr(fileno(stdout), &tt);
			tt.c_oflag |= OPOST;
			tcsetattr(fileno(stdout), TCSANOW, &tt);

			tcgetattr(fileno(stdin), &tt);
			tt.c_lflag |= ECHO|ICANON|ISIG;
			tt.c_iflag |= IXON;
			tt.c_iflag |= ICRNL;
			tcsetattr(fileno(stdin), TCSANOW, &tt);
		}
		break;
	}
}

size_t chk_pty::write_all(int fd, const char *buf, size_t len, int delay1)
{
	size_t cnt = 0;
	const char *p = buf;
	while (len > cnt) {
		int n = delay1 ? 1 : len - cnt;
		int ret = write(fd, p+cnt, n);
		if (ret < 0) return ret;
		cnt += ret;
		usleep(delay1 * 1000);
	}
	return cnt;
}

void *chk_pty::reader (void *param)
{
	for (;;) {
		fd_set rdfds;
		int nfd;
		int r;

		FD_ZERO(&rdfds);
		FD_SET(master, &rdfds);
		FD_SET(fileno(stdin), &rdfds);

		nfd = ((master > fileno(stdin)) ? master : fileno(stdin) ) + 1;
		r = select(nfd, &rdfds, NULL, NULL, NULL);

		if (r<0 && errno == EINTR) {
			continue;
		}

		if (r<=0) {
			perror("select()");
			close(master);
			exit(-1);
		}

		if (r) {
			char buf[BUFSIZ];
			int len;
			if (FD_ISSET(master, &rdfds)) {
				len = read(master, buf, BUFSIZ);
				if (len<=0) break;
				if (len > 0) {
					len = write_all(fileno(stdout), buf, len);
					if (len<=0) break;
				}
			}
			if (FD_ISSET(fileno(stdin), &rdfds)) {
				len = read(fileno(stdin), buf, BUFSIZ);
				if (len<=0) break;
				if (len==1 && buf[0] == '\030' /* Ctrl-X */) {
					kill(child_pid, SIGHUP);
					wait(NULL);
					exit(1);
				}
				len = write_all(master, buf, len);
				if (len<=0) break;
			}
		}
	}
	return NULL;
}

size_t chk_pty::emit_input(const char *ptr, size_t len)
{
	return write_all(master, ptr, len, delay);
}

void chk_pty::set_raw (int m)
{ /* Setup STDIN and STDOUT */
	struct termios tt;

	if (m & 2) {
		tcgetattr(fileno(stdout), &tt);
		tt.c_oflag &= ~OPOST;
		tcsetattr(fileno(stdout), TCSANOW, &tt);
	}

	if (m & 1) {
		tcgetattr(fileno(stdin), &tt);
		tt.c_lflag &= ~(ECHO|ICANON|ISIG);
		tt.c_iflag &= ~IXON;
		tt.c_iflag &= ~ICRNL;
		tcsetattr(fileno(stdin), TCSANOW, &tt);
	}
}

int chk_pty::run(char *filename, char *logfile)
{
	int pm, ps;
	pid_t pid;

	if (openpty(&pm, &ps, NULL, NULL, NULL) < 0) {
		perror("openpty()");
		exit(-1);
	}

	master = pm; /* for signal handler */ 

	strncpy(tty, ttyname(ps) + 5, sizeof(tty) - 1);
	tty[sizeof(tty) - 1] = '\0';

	char buf1[64], buf2[64];
	FILE *p = popen("tty", "r");
	fscanf(p, "%63s", buf1);
	pclose(p);
	p = popen("cmd /c tty", "r");
	fscanf(p, "%63s", buf2);
	pclose(p);
	if (strcmp(buf1, buf2) == 0) {
		new_pty = true;
	} else {
		new_pty = false;
	}

#if 0
	{ /* Disable echo */
		struct termios tt;
		tcgetattr(ps, &tt);
		tt.c_lflag &= ~ECHO;
		tcsetattr(ps, TCSANOW, &tt);
	}
#endif

	pid = fork();
	if (pid < 0) {
		perror("fork()");
		close(pm);
		exit(-1);
	}

	if (pid != 0) { /* Parent */
		pthread_t th;
		FILE *script, *log = stdout;
		the_pty_checker = this;
		child_pid = pid;

		set_raw (1);

		signal(SIGWINCH, ::sig_handler);
		signal(SIGCHLD, ::sig_handler);

		winsize_set();

		/* Process Input and Output */
		pthread_create(&th, NULL, ::reader, NULL); /* Process output */

		/* Process input */
		script = fopen(filename, "r");
		if (logfile) log = fopen(logfile, "w");

		bool ok = do_chk(script, log);

		if (log != stdout) fclose(log);
		if (script) fclose(script);

		close(ps);
		usleep(1000000);
		emit_input("exit\n", 5);

		wait(NULL);
		pthread_join(th, NULL);
		close(pm);
		return ok ? 0 : 1;
	} else { /* Child */
		close(pm);
		setsid();
		ioctl(ps, TIOCSCTTY, 1);
		dup2(ps, fileno(stdin));
		dup2(ps, fileno(stdout));
		dup2(ps, fileno(stderr));
		close(ps);
		execl("/usr/bin/bash", "bash", NULL);
		perror("execl()");
		exit(-1);
	}
}

/* Console stuff */
size_t chk_console::emit_input(const char *buf, size_t len)
{
	size_t total = 0;
	DWORD mode;
	FreeConsole();
	AttachConsole(console_pid);
	HANDLE h = CreateFile("CONIN$",
						GENERIC_READ | GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL, OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL, NULL);
	GetConsoleMode(h, &mode);
	for (int i=0; i<len; i++) {
		INPUT_RECORD r[2] = {0,};
		DWORD n;
		if ((mode & ENABLE_PROCESSED_INPUT) && buf[i] == '\003') {
			GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
			total += 1;
		} else {
			r[0].EventType = KEY_EVENT;
			r[0].Event.KeyEvent.bKeyDown = TRUE;
			r[0].Event.KeyEvent.uChar.AsciiChar = buf[i];
			r[1].EventType = KEY_EVENT;
			r[1].Event.KeyEvent.bKeyDown = FALSE;
			r[1].Event.KeyEvent.uChar.AsciiChar = buf[i];
			WriteConsoleInputA(h, r, 2, &n);
			total += n/2;
		}
		usleep (delay * 1000);
	}
	CloseHandle(h);
	return total;
}

int chk_console::run(int pid, char *filename, char *logfile)
{
	char cmd[1024];
	FILE *p;
	int ppid, pgid, winpid;
	FILE *script, *log = stdout;

	sprintf(cmd, "ps -p %d | grep %d", pid, pid);
	p = popen(cmd, "r");
	fgetc(p); /* Skip status char */
	fscanf(p, "%d %d %d %d %63s", &pid, &ppid, &pgid, &winpid, tty);
	pclose(p);
	console_pid = winpid;

	script = fopen(filename, "r");
	if (logfile) log = fopen(logfile, "w");

	return do_chk(script, log) ? 0 : 1;
}
