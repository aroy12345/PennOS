#include <stdio.h>
#include <stdlib.h>

#include "util/globals.h" // fs_fd, fat
#include "shell/pennos-shell.h"
#include "pennfat/fat.h"
#include "kernel/PCB.h"
#include "kernel/scheduler.h"
#include "logger/logger.h"

/**
 * Entry point for PennOS.
 * Initializes the logger, filesystem, and spawns the shell process.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return Returns 1 if the number of command-line arguments is less than 2.
 */
int main(int argc, char* argv[]) {
    if (argc < 2) return 1;

    // initialize the logger
    logfile = fopen("./log/log", "w+");
    if (logfile == NULL) return 1;

    // initialize filesystem
    char* fs_filename = argv[1];
    fs_fd = fs_mount(fs_filename, &fat);

    // add the shell as a top-level process, and set it to -1 priority
    char* pennos_args[] = { "shell", NULL };
    int shell_pid = p_spawn(pennos_shell, pennos_args, 0, 1);
    p_nice(shell_pid, -1);

    // start the scheduler
    start_scheduler();
}