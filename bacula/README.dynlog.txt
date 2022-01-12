Bacula's Dynamic logging with gdb's dprintf

It is possible to add some logging with the help of GDB and without
modifying the binary.
The logging goes into the trace file with other logging from Dmsg()
functions

Be sure that gdb can attach to any process

$ cat /proc/sys/kernel/yama/ptrace_scope
0

If not zero, run the following command

$ echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope

or modify /etc/sysctl.d/10-ptrace.conf to get the change persistent 

The you must tweak the following script to add your own logging

-------------------------------8<-------------------------------
#!/bin/sh
# DAEMON can be "fd", "sd" or "dir" depending the Bacula's
# component you want to trace
DAEMON=fd

PID=`pgrep -x bacula-$DAEMON`
filename=`mktemp  --suffix $PID gdb.XXX`
trap "rm $filename" EXIT

cat >> $filename <<EOF
set target-async on
set non-stop on
attach $PID
handle SIGUSR2 nostop
set overload-resolution off
set dprintf-style call
set dprintf-function gdb_dprintf
dprintf restore.c:487, "restore.c:487-%u file_index=%ld\n", get_jobid_from_tsd(), file_index
cont
EOF

gdb -x $filename
------------------------------->8-------------------------------

See the line dprintf at the end, you can modify it and add some more.
The arguments can be split in 3 groups
- the location where the logging must be done in the source code
- the message format
- a list of variables that you want to print and that match your
  format string

"restore.c:487" is the location in the sources where the
message must be printed

"restore.c:487-%u file_index=%ld\n" is the format string
surrounded with double quote. To match the Bacula's messages
we repeat the location and add %u to print the jobid.
Then come the useful information that you want to display.
The conversion specifier are the same as the one used inside
Bacula in Dmsg(), Mmsg() or bsnprintf(), they are a little bit
different of the one used standard "printf" function.

Unfortunately when using the compiler optimization (gcc -O2)
like we do for our production binaries some variables are
"optimized out". This means that some variables share the
same location and the value depend where you are in the
function. If a variable "A" is only used a the beginning
of a function and a variable "B" at the end only, there is
a chance that they will share the same location and if
you add a "dprintf" at the beginning you can probably
print the "A" value but not the "B" one and this is
the opposite at the end of the function.
The optimization can vary from one compiler version to
another, then if you test your "dprintf" on a debian
for a customer that run a redhat with a different gcc,
there is a possibility for the variable to be optimized
in a different way :-(.
The best way to know which variables can be printed
at one place is to use the same binaries as the target,
setup a breakpoint where you need the information, wait for
the program to stop there and use the command
"info locals" to show witch variables are available.
Real Dmsg() don't suffer of this problem as the
compiler will optimize the variables around the
requirements of the program and the Dmsg themselves. 

Some information about the instructions in the script:
  set target-async on
  set non-stop on
The two line above are used to not stop gdb when it reach the
location that we want to monitor.
  attach $PID
The two first line must be executed before to attach to a process,
that why we attach the process after.
  handle SIGUSR2 nostop
Bacula use USR2 for internal use and we don't want to interrupt
gdb when they are used
  set overload-resolution off
Our "gdb_dprintf()" below expect a variable number of arguments
and this is the only way for gdb to accept the function without
complaining
  set dprintf-style call
  set dprintf-function gdb_dprintf
This is where we tell GDB to use our function "gdb_dprintf()" 
instead of standard "printf()"
  dprintf restore.c:487, "restore.c:487-%u file_index=%ld\n", get_jobid_from_tsd(), file_index
We can add as many dprintf as we want...
  cont
Then tel gdb to continue to run the program, after the setup

To enable the extra logging :
1 - edit the script above
  a - choose the component you want to monitor,
      the "fd", "sd" or "dir"
  b - edit or add more dprintf lines
2 - enable logging on the component using the setdebug command
    for exemple :
	* setdebug level=100 trace=1 client
3 - run the script above
    # sh gdbdynlog.sh
4 - when our are done you must interrupt gdb with CTRL-C,
	then quit gdb with the "quit" command and answer yes to the last question
5 - stop the extra logging using the setdebug command
    * setdebug level=0 trace=0

Before to send a script and these instructions to the customer
verify that your dprintf works on the binaries used by the customer!


For the example above, you should see something like this in your trace file:

-------------------------------8<-------------------------------
...
bac-fd: authenticatebase.cc:561-32 TLSPSK Start PSK
bac-fd: restore.c:487-32 file_index=1
...
bac-fd: restore.c:487-32 file_index=3013
bac-fd: restore.c:1147-32 End Do Restore. Files=3013 Bytes=195396973
bac-fd: job.c:3484-32 end_restore_cmd
------------------------------->8-------------------------------

When you are done you must interrupt gdb with CTRL-C
then quit gdb with the quit command and answer yes to the last question

CTRL-C
(gdb) quit
A debugging session is active.

	Inferior 1 [process 19286] will be detached.

Quit anyway? (y or n) y
Detaching from program: /home/bac/workspace2/bee/regress/bin/bacula-fd, process 19286

