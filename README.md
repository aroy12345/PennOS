Source Files
---
`pennos.c`: Represents the main function for an operating system-like environment. It initializes a logging system, mounts a file system, and spawns a shell process as the top-level process with the lowest priority. The shell is responsible for handling user commands and managing other processes. After setting up the shell, the scheduler is started to manage process execution and scheduling. The program expects the file system filename as a command-line argument and logs system activities to a file.


**Source Files in src/kernel:**\
`PCB.c`: primarily deals with managing process control blocks (PCBs) in a circular linked list, which is essential for process scheduling in operating systems. Functions include creating and deleting PCBs, adding or removing them from the list, and utilities for finding specific PCBs based on process ID or context. Additional functions compute the length of the PCB list and count the number of running processes, aiding in process management.

`kernel-functions.c`: This code is for kernel-land, featuring functions to create new process threads, send termination signals (like stop, continue, terminate), and handle process cleanup. It includes adding child processes to a list, changing their states based on signals, and performing both deep and general cleanups, involving removal from the process list and state changes.

`puser-functions.c`: This code is for user-land,  including functions for creating, managing, and terminating processes. It defines operations such as spawning a new process (p_spawn), waiting for a process to change state (p_waitpid), sending signals to terminate or modify a process's state (p_kill), changing a process's priority (p_nice), and handling process exit (p_exit). Additionally, it includes utility functions to determine the status of a process based on wait status (W_WIFEXITED, W_WIFSTOPPED, W_WIFCONTINUED, W_WIFSIGNALED). The code integrates process control block (PCB) handling, context switching, and file descriptor management.

`scheduler.c`:  implements our scheduler. It defines functions for scheduling processes, handling timer alarms, and cleaning up resources. The scheduler function selects a process to run based on a priority-driven, randomized approach. An alarm handler function is used to periodically interrupt the running process, facilitating context switching. The code sets up these functions with their own contexts and stacks, and uses signals and a timer to manage process execution and scheduling. Additionally, there's a cleanup function to free allocated stacks, and the scheduler is initiated with necessary signal handlers and timer settings.


**Source Files in src/pennfat:**\
`safe.c`:  provides a set of wrapper functions for various system calls, adding robust error handling. The functions safe_open, safe_close, safe_read, safe_write, safe_lseek, safe_msync, safe_mmap, and safe_munmap each perform their respective standard system calls (like opening files, reading, writing, seeking within files, memory mapping, and unmapping). If any of these system calls fail, the corresponding function prints an error message to stderr and then exits the program. This approach ensures that the program handles system call failures gracefully and provides clear feedback about what went wrong.

`fat.c`:  provides functions for a FAT based file system, handling tasks like creating and deleting files, reading and writing file data, and managing file metadata. It includes utilities for locating and managing file blocks, updating directory entries, and manipulating file chains in the FAT structure. The code also includes functions for copying files, listing directory contents, and changing file permissions.

`pennfat.c`: command-line interface for managing  FAT (File Allocation Table) file system. It includes commands for creating a file system (mkfs), mounting (mount), unmounting (unmount), creating files (touch), moving/renaming files (mv), deleting files (rm), concatenating file contents (cat), copying files (cp), listing directory contents (ls), changing file permissions (chmod), and displaying file system data in hex format (hd). The program performs checks for correct argument counts and whether a file system is mounted before executing commands. It also ensures that specified files exist before performing operations on them. The program uses safe wrapper functions to handle system calls reliably.


**Source Files in src/logger:**\
`logger.c`: Part of a logging system for an operating system or process management environment. It defines functions to log various process-related events to a file, including process creation, scheduling, signaling, exiting, transitioning to zombie or orphan state, and waiting. Each logging function takes the process ID (pid), priority (prio), and process name as arguments, and writes a log entry with a timestamp (ticks), an event type (like SCHEDULE, CREATE, SIGNALED, etc.), and the process details. The file pointer logfile is used to write these log entries.


**Souce Files in src/shell:**\
`job-list.c`: Manages a job list for process control, featuring functions to find, add, insert, remove, and print job details. It uses a linked list structure (job_t) to handle jobs, each identified by a unique job ID and associated with a process ID (PID). Functions include finding a job by ID, retrieving the last job ID in the list, adding new jobs to the end of the list, inserting jobs in order, removing a specific job by ID, and printing job status based on its current state in the operating system.

`pennos-shell.c`: Implements a shell for managing processes in a custom operating system. It handles various shell commands like cat, sleep, echo, ls, and process control commands (bg, fg, jobs, kill). The shell supports both foreground and background process execution, job control (including stopping, continuing, and listing jobs), and handling of terminal signals like SIGINT and SIGTSTP. It uses a job list to track and manage background processes, and integrates with a file system and process management API. The shell also includes functionality to execute script files and exit the shell environment.


**Source Files in src/util:**\
`globals.c`: global values of the filesystem `fat` and the its corresponding file descriptor `fs_fd` to access it

`p-errno.c`: provides `ERRNO` which is set by `f_`... and `p_`... functions upon returning `-1`, and `p_perror` which can print an error message

`parser.h`: interface for the `parser.o` in `bin`

`safe-user.c`: error-handled versions of `f_`... and `p_`... functions which may have an error, as specified in `p-errno.c`

`util.c`: utility functions that don't fit well into another file


Compilation
---
To compile `pennOS`, `make` in the root directory & run `./bin/pennos [ filesystem ]` (making sure that `pennfat` was compiled)

To compile `pennfat`, `make` in the `root/src/pennfat` folder & run `./pennfat` from `bin`

Make sure `parser.o` is in `root/bin`
