# Cygwin TTY behaviour tester
## Summary
This tool provides automated test against key input in TTY. With your test script, test would be done. The sample test script is as follows.
```
# Sample Test script
E cat\n
W 100
C /cat$ 1 I
E \003
C /cat$ 0
```
This script confirm that `cat` can be stoppped by Ctrl-C.

The first line started with `#` is a comment. The sencond line emits key input `cat\n`. The thrid line waits 100ms.
The 4th line checks process existence for `cat`. The command `C` runs cygwin's `ps` command and `grep` to exclude lines other than the target rocess.
This line searches `/cat`at the line end. `1` menas the number of lines matched is `1`. The 'I' at the end of the line meas process is waiting for input.
The 5th line emits Ctrl-C.
The last line checks if the `cat` has been terminated.

This tool can check only the exsistence of processes, so you need to find a clever way to check what you want to test.
For example, you want to check check if the file 'testfile.txt` is exist, you can use the script below.
```
# Checking existence of a file
E if [ -f testfile.txt ]; then sleep 10; fi\n
W 100
C /sleep$ 1
E \003
W 100
C /sleep$ 0
```
If this test succeeed (returns 0), the file `testfile.txt` exists. Otherwise, the file does not exit.

## Usage
```
chk_pty <script> [<logfile>]
chk_console <PID> <script> [<logfile>]
```
`PID` is the Process ID of a cygwin process which is running in the target console.
## All command list
All the command must be placed at the beggining of the line.
### `E` \<string\>
Emit the string to the tty. You can use, e.g. '\033', '\x1b' like C language.
### `W` \<value\>
Wait for `value` msec.
### `C` \<pattern\> \<value\> \<state\>
Check process state in the current TTY. Search `pattern` from the output of cygwin `ps` command, and checks if the number of output lines matches to `value`.
You need to specify `state` if the process state is not `running`. `I`: waiting for input, `O`: waiting for output, `S`: being suspended, etc.
### `T` \<pattern\> \<value\> \<state\>
Same as `C` except that `T` searches for processes in the TTY which derived from pseudo console.
### `N` \<pattern\> \<value\>
Same as `C` except that `N` searches for processes whose CTTY is not a cygwin TTY.
This is for the process of non-cygwin app started via `CreateProcess()`.
### `D` \<value\>
Specify the time period value which is used by `E` command. For example, if you specify 100, 'E' emits each char in every `value` msec.
### `G` \<value\>
Goto specified line in the script file.
### `#` \<string\>
Comment. `string` is just ignored.

## TODO
Implement conditional jump.
