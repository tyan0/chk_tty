all: chk_pty chk_console

chk_pty: chk_tty.h chk_tty.cc chk_pty.cc
	$(CXX) -O2 chk_pty.cc chk_tty.cc -o $@

chk_console: chk_tty.h chk_tty.cc chk_console.cc
	$(CXX) -O2 chk_console.cc chk_tty.cc -o $@

clean:
	rm -f chk_pty chk_console
