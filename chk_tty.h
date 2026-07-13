#include <unistd.h>
#include <windows.h>

class chk_tty {
protected:
	int delay;
	char tty[64];
	pid_t child_pid;
	virtual bool is_console() { return false; };
	virtual bool is_new_pty() { return false; };
	virtual size_t emit_input(const char *p, size_t len) { return 0; };
	virtual void set_raw (int m) {};
	virtual bool do_chk(FILE *script, FILE *log);
public:
	chk_tty() : delay(0) {};
	virtual int run(char *filename, char *logfile) { return 0; };
};

class chk_pty : chk_tty {
private:
	int master;
	bool new_pty;
	bool is_new_pty() { return new_pty; };
	static size_t write_all(int fd, const char *ptr, size_t len, int delay = 0);
	size_t emit_input(const char *p, size_t len);
	void winsize_set();
	void set_raw (int m);
public:
	void sig_handler(int sig);
	void *reader(void *param);
	int run(char *filename, char *logfile);
};

class chk_console : chk_tty {
private:
	DWORD console_pid;
	bool is_console() { return true; }
	size_t emit_input(const char *p, size_t len);
public:
	int run(int pid, char *filename, char *logfile);
};
